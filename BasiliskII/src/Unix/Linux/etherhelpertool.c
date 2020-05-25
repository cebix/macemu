/*
 *  etherhelpertool.c - Reads and writes raw ethernet packets usng bpf
 *  interface.
 *
 *  Copyright (C) 2010, Daniel Sumorok
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <errno.h>
#include <sys/select.h>

#include <unistd.h>
#include <stdlib.h>

#include <net/if.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <fcntl.h>
#include <signal.h>

#include <linux/if_tun.h>

#include <strings.h>

#define STR_MAX 256
#define MAX_ARGV 10

static int remove_bridge = 0;
static char bridge_name[STR_MAX];
static const char *exec_name = "etherhelpertool";

static int main_loop(int sd, int use_bpf);
static int open_tap(char *ifname);
static int run_cmd(const char *cmd);
static void handler(int signum);
static int install_signal_handlers(void);
static void do_exit(void);
static int open_bpf(char *ifname);

int main(int argc, char **argv)
{
	char *if_name;
	int ret = 255;
	int sd = -1;
        int tapNum;
        int use_bpf;

	if (argc != 2) {
		return 255;
	}
	
	if_name = argv[1];

        do {
		if (strncmp(if_name, "tap", 3) == 0) {
                        sd = open_tap(if_name);
                        use_bpf = 0;
                } else {
                        sd = open_bpf(if_name);
                        use_bpf = 1;
                }

                if (sd < 0) {
			fprintf(stderr, "%s: open device failed.\n",
				exec_name);
                        ret = 253;
                        break;
                }

                if (install_signal_handlers() != 0) {
			fprintf(stderr, 
				"%s: failed to install signal handers.\n",
				exec_name);
                        ret = 252;
                        break;
                }

                ret = main_loop(sd, use_bpf);
                close(sd);
        } while (0);

        do_exit();

	return ret;
}

static int main_loop(int sd, int use_bpf)
{
	fd_set readSet;
	char *outgoing, *incoming;
	unsigned short *out_len;
	unsigned short *in_len;
	int in_index, out_index;
	u_int blen = 0;
	int ret;
	int fret = 0;
	int pkt_len;
	int frame_len;
	int pad;
        char c = 0;

	blen = 4096;

	incoming = malloc(blen);
	if (incoming == NULL) {
		fprintf(stderr, 
			"%s: malloc() failed.\n",
			exec_name);
		return -2;
	}

	outgoing = malloc(blen);
	if (outgoing == NULL) {
		free(outgoing);
		fprintf(stderr, 
			"%s: malloc() failed.\n",
			exec_name);
		return -3;
	}

	in_index = 0;
	out_index = 0;

	out_len = (unsigned short *)outgoing;

        /* Let our parent know we are ready for business. */
        if(write(0, &c, 1) != 1) {
	  fprintf(stderr, "%s: Failed to notify main application: %s\n",
		  __func__, strerror(errno));
	}

	while (1) {
		int i;
		FD_ZERO(&readSet);
		FD_SET(0, &readSet);
		FD_SET(sd, &readSet);

		ret = select(sd + 1, &readSet, NULL, NULL, NULL);
		if (ret < 0) {
			fprintf(stderr, 
				"%s: select() failed.\n",
				exec_name);
			fret = -4;
			break;
		}

		if (FD_ISSET(0, &readSet)) {
			if (out_index < 2) {
				ret = read(0, outgoing + out_index, 2-out_index);
			} else {
				ret = read(0, outgoing + out_index, *out_len - out_index + 2);
			}

			if (ret < 1) {
				if(ret < 0) {
					fprintf(stderr, 
						"%s: read() failed.\n",
						exec_name);
				}
				fret = -5;
				break;
			}

			out_index += ret;

			if (out_index > 1) {
				if ((*out_len + 2) > blen) {
					fret = -6;
					break;
				}

				if (out_index == (*out_len + 2)) {
					if(use_bpf) {
						ret = write(sd, out_len + 1, *out_len);
						if (ret != *out_len) {
							fprintf(stderr,
								"%s: write() failed.\n",
								exec_name);
							fret = -7;
							break;
						}
					} else {
						ret = write(sd, out_len + 1, *out_len);
						if (ret != *out_len) {
							fprintf(stderr,
								"%s: write() failed.\n",
								exec_name);
							fret = -7;
							break;
						}
					}

					out_index = 0;
				}
			}

		}

		if (FD_ISSET(sd, &readSet)) {
			in_len = (unsigned short *)incoming;

			pkt_len = read(sd, in_len + 1, blen-2);
			if (pkt_len < 14) {
				fprintf(stderr,
					"%s: read() returned %d.\n",
					exec_name, pkt_len);
				fret = -8;
				break;
			}
                        *in_len = pkt_len;

                        if (write(0, in_len, pkt_len + 2) < (pkt_len + 2)) {
				fprintf(stderr,
					"%s: write() failed\n",
					exec_name);
                                fret = -10;
                                break;
                        }
                }
	}

	free(incoming);
	free(outgoing);

	return fret;
}


static int open_tap(char *ifname)
{
        char str[STR_MAX] = {0};
        char ifstr[STR_MAX] = {0};
	char *interface;
	char *address = NULL;
	char *netmask = NULL;
	char *bridge = NULL;
	char *bridged_if = NULL;
        int sd;
	struct ifreq ifr = {0};

	snprintf(ifstr, STR_MAX, "%s", ifname);
	interface = strtok(ifstr, "/");
	bridge = strtok(NULL, "/");
	if (bridge != NULL) {
		bridged_if = strtok(NULL, "/");
	}
	interface = strtok(ifstr, ":");

	address = strtok(NULL, ":");

	if (address != NULL) {
		netmask = strtok(NULL, ":");
	}

        sd = open("/dev/net/tun", O_RDWR);
        if (sd < 0) {
		fprintf(stderr, "%s: Failed to open %s\n",
			exec_name, interface);
                return -1;
        }

        snprintf(str, STR_MAX, "/dev/%s", interface);
        ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
        strncpy(ifr.ifr_name, interface, IFNAMSIZ);

        if(ioctl(sd, TUNSETIFF, (void *)&ifr) != 0) {
                fprintf(stderr, "%s: ioctl(TUNSETIFF): %s\n",
                        __func__, strerror(errno));
                close(sd);
                return -1;
        }

        if (address == NULL) {
                snprintf(str, STR_MAX, "/sbin/ifconfig %s up", interface);
        } else if (netmask == NULL) {
                snprintf(str, STR_MAX, "/sbin/ifconfig %s %s",
                         interface, address);
        } else {
                snprintf(str, STR_MAX, "/sbin/ifconfig %s %s netmask %s",
                         interface, address, netmask);
        }

        if (run_cmd(str) != 0) {
		fprintf(stderr, "%s: Failed to configure %s\n",
			exec_name, interface);
                close(sd);
                return -1;
        }

	if (bridge != NULL) {
		/* Check to see if bridge is alread up */
		snprintf(str, STR_MAX, "/sbin/ifconfig %s", bridge);
		if (run_cmd(str) == 0) {
			/* bridge is already up */
			if (bridged_if != NULL) {
				fprintf(stderr, "%s: Warning: %s already exists, so %s was not added.\n",
					exec_name, bridge, bridged_if);
			}
		} else {
			snprintf(str, STR_MAX, "/sbin/brctl addbr %s", bridge);
			if (run_cmd(str) != 0) {
				fprintf(stderr, "%s: Failed to create %s\n",
					exec_name, bridge);
				close(sd);
				return -1;
			}
			remove_bridge = 1;

			strncpy(bridge_name, bridge, STR_MAX);

			snprintf(str, STR_MAX, "/sbin/ifconfig %s up", bridge);
			if (run_cmd(str) != 0) {
				fprintf(stderr, "%s: Failed to open %s\n",
					exec_name, bridge);
				close(sd);
				return -1;
			}

			if (bridged_if != NULL) {
				snprintf(str, STR_MAX, "/sbin/brctl addif %s %s",
					 bridge, bridged_if);
				if (run_cmd(str) != 0) {
					fprintf(stderr, "%s: Failed to add %s to %s\n",
						exec_name, bridged_if, bridge);
					close(sd);
					return -1;
				}
			}

			snprintf(str, STR_MAX, "/sbin/brctl addif %s %s",
				 bridge, interface);
			if (run_cmd(str) != 0) {
				fprintf(stderr, "%s: Failed to add %s to %s\n",
					exec_name, interface, bridge);
				close(sd);
				return -1;
			}
		}
	}

        return sd;
}

static int run_cmd(const char *cmd) {
	char cmd_buffer[STR_MAX] = {0};
	char *argv[MAX_ARGV + 1] = {0};
	int i;
	pid_t pid, waitpid;
	int status = 0;

	/* Collect arguments */
	strncpy(cmd_buffer, cmd, STR_MAX-1);

	argv[0] = strtok(cmd_buffer, " ");
	for (i=1; i<MAX_ARGV; ++i) {
		argv[i] = strtok(NULL, " ");
		if (argv[i] == NULL) {
			break;
		}
	}

	/* Run sub process */
	pid = fork();
	if (pid == 0) {

		/* Child process */
		fclose(stdout);
		fclose(stderr);

		if (execve(argv[0], argv, NULL) < 0) {
			perror("execve");
			return -1;
		}
	} else {
		/* Wait for child to exit */
		waitpid = wait(&status);
		if (waitpid < 0) {
			perror("wait");
			return -1;
		}

		if (status != 0) {
			return -1;
		}
	}

	return 0;
}

static void handler(int signum) {
        do_exit();
        exit(1);
}

static int install_signal_handlers() {
        struct sigaction act = {0};

        act.sa_handler = handler;
        sigemptyset(&act.sa_mask);

        if (sigaction(SIGINT, &act, NULL) != 0) {
                return -1;
        }


        if (sigaction(SIGHUP, &act, NULL) != 0) {
                return -1;
        }


        if (sigaction(SIGTERM, &act, NULL) != 0) {
                return -1;
        }

        return 0;
}

static void do_exit() {
	char cmd[STR_MAX];

        if (remove_bridge) {
		snprintf(cmd, STR_MAX, "/sbin/ifconfig %s down",
			 bridge_name);

                if(run_cmd(cmd) != 0) {
			fprintf(stderr, "Failed to bring bridge down\n");
		}

		snprintf(cmd, STR_MAX, "/sbin/brctl delbr %s",
			 bridge_name);

                if(run_cmd(cmd) != 0) {
			fprintf(stderr, "Failed to destroy bridge\n");
		}
        }
}

static int open_bpf(char *ifname)
{
	int sd;
	struct sockaddr_ll sockaddr = {0};
	struct ifreq ifreq = {0};
	struct packet_mreq pmreq = {0};
	socklen_t socklen = sizeof(struct packet_mreq);

	sd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if(sd < 0) {
		perror("socket");
		return -1;
	}

	strncpy(ifreq.ifr_name, ifname, IFNAMSIZ);
	if(ioctl(sd, SIOCGIFINDEX, &ifreq) != 0) {
		fprintf(stderr, "%s: ioctl(SIOCGIFINDEX): %s\n",
			__func__, strerror(errno));
		close(sd);
		return -1;
	}

	pmreq.mr_ifindex =  ifreq.ifr_ifindex;
	pmreq.mr_type = PACKET_MR_PROMISC;
	if(setsockopt(sd, SOL_PACKET, PACKET_ADD_MEMBERSHIP,
		      &pmreq, socklen) != 0) {
		fprintf(stderr, "%s: setsockopt() failed: %s\n",
			__func__, strerror(errno));
		close(sd);
		return -1;
	}

	sockaddr.sll_family = AF_PACKET;
	sockaddr.sll_ifindex = ifreq.ifr_ifindex;
	sockaddr.sll_protocol = htons(ETH_P_ALL);
	if(bind(sd, (struct sockaddr *)&sockaddr,
		sizeof(struct sockaddr_ll)) != 0) {
		fprintf(stderr, "%s: bind failed: %s\n",
			__func__, strerror(errno));
		close(sd);
		return -1;
	}

	return sd;
}
