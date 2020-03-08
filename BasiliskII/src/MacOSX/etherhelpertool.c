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
#include <net/if_dl.h>
#include <ifaddrs.h>
#include <errno.h>
#include <sys/select.h>

#include <unistd.h>

#include <net/if.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <net/bpf.h>
#include <fcntl.h>

#include <strings.h>

#include <Carbon/Carbon.h>

#define STR_MAX 256
#define MAX_ARGV 10

static int open_bpf(char *ifname);
static int open_tap(char *ifname);
static int retreive_auth_info(void);
static int main_loop(int sd, int use_bpf);
static int run_cmd(const char *cmd);
static void handler(int signum);
static int install_signal_handlers();
static void do_exit();

static char remove_bridge[STR_MAX];
static const char *exec_name = "etherhelpertool";

int main(int argc, char **argv)
{
	char *if_name;
	int ret = 255;
	int sd;
        int tapNum;
        int use_bpf;

	if (argc != 2) {
		return 255;
	}
	
	if_name = argv[1];

        do {
                ret = retreive_auth_info();
                if (ret != 0) {
			fprintf(stderr, "%s: authorization failed.\n",
				exec_name);
			ret = 254;
			break;
                }

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
	struct bpf_hdr *hdr;
	int pkt_len;
	int frame_len;
	int pad;
        char c = 0;

	if (use_bpf) {
                if (ioctl(sd, BIOCGBLEN, &blen) < 0) {
			fprintf(stderr, 
				"%s: ioctl() failed.\n",
				exec_name);
                        return -1;
                }
        } else {
                blen = 4096;
        }

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
        write(0, &c, 1);

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
					ret = write(sd, out_len + 1, *out_len);
					if (ret != *out_len) {
						fprintf(stderr, 
							"%s: write() failed.\n",
							exec_name);
						fret = -7;
						break;
					}
					out_index = 0;
				}
			}

		}

		if (use_bpf && FD_ISSET(sd, &readSet)) {
			int i;

			ret = read(sd, incoming, blen);
			if (ret < 1) {
				if(ret < 0) {
					fprintf(stderr, 
						"%s: read() failed %d.\n",
						exec_name, errno);
				}
				fret = -8;
				break;
			}

			hdr = (struct bpf_hdr *)incoming;
			in_len = (unsigned short *)(incoming + 16);

			do {
				pkt_len = hdr->bh_caplen;
				frame_len = pkt_len + 18;

				if ((pkt_len < 0) || (frame_len > ret) || (frame_len < 0)) {
					fret = -9;
					break;
				}
				*in_len = pkt_len;

				if (write(0, in_len, pkt_len + 2) < (pkt_len + 2)) {
                                        fret = -10;
                                        break;
                                }

				if ((frame_len & 0x03) == 0) {
					pad = 0;
				} else {
					pad = 4 - (frame_len & 0x03);
				}

				ret -= (frame_len + pad);
				hdr = (struct bpf_hdr *)((unsigned char *)hdr + frame_len + pad);
				in_len = (unsigned short *)((unsigned char *)hdr + 16);
			} while (ret > 0);

			if (fret != 0) {
				fprintf(stderr, 
					"%s: fret == %d.\n",
					exec_name, fret);
				break;
			}
		}

		if (!use_bpf && FD_ISSET(sd, &readSet)) {
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

static int retreive_auth_info(void)
{
	AuthorizationRef aRef;
	OSStatus status;
	AuthorizationRights myRights;
	AuthorizationRights *newRights;
	AuthorizationItem *myItem;
	AuthorizationItem myItems[1];
	AuthorizationItemSet *mySet;
	int i;

	status = AuthorizationCopyPrivilegedReference(&aRef, kAuthorizationFlagDefaults);
	if (status != errAuthorizationSuccess) {
		return -1;
	}

	status = AuthorizationCopyInfo(aRef, NULL, &mySet);
	if (status != errAuthorizationSuccess) {
		AuthorizationFree(aRef, kAuthorizationFlagDestroyRights);
		return -1;
	}

	myItems[0].name = "system.privilege.admin";
	myItems[0].valueLength = 0;
	myItems[0].value = NULL;
	myItems[0].flags = 0;

	myRights.count = sizeof (myItems) / sizeof (myItems[0]);
	myRights.items = myItems;

	status = AuthorizationCopyRights(aRef, &myRights, NULL,
					 kAuthorizationFlagExtendRights,
					 &newRights);
	if (status != errAuthorizationSuccess) {
		AuthorizationFreeItemSet(mySet);
		AuthorizationFree(aRef, kAuthorizationFlagDestroyRights);
		return -2;
	}

	AuthorizationFreeItemSet(newRights);
	AuthorizationFreeItemSet(mySet);
	AuthorizationFree(aRef, kAuthorizationFlagDestroyRights);  

	return 0;
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

        snprintf(str, STR_MAX, "/dev/%s", interface);

        sd = open(str, O_RDWR);
        if (sd < 0) {
		fprintf(stderr, "%s: Failed to open %s\n",
			exec_name, interface);
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
			snprintf(str, STR_MAX, "/sbin/ifconfig %s create", bridge);
			if (run_cmd(str) != 0) {
				fprintf(stderr, "%s: Failed to create %s\n",
					exec_name, bridge);
				close(sd);
				return -1;
			}
			strlcpy(remove_bridge, bridge, STR_MAX);

			snprintf(str, STR_MAX, "/sbin/ifconfig %s up", bridge);
			if (run_cmd(str) != 0) {
				fprintf(stderr, "%s: Failed to open %s\n",
					exec_name, bridge);
				close(sd);
				return -1;
			}

			if (bridged_if != NULL) {
				snprintf(str, STR_MAX, "/sbin/ifconfig %s addm %s", 
					 bridge, bridged_if);
				if (run_cmd(str) != 0) {
					fprintf(stderr, "%s: Failed to add %s to %s\n",
						exec_name, bridged_if, bridge);
					close(sd);
					return -1;
				}
			}
        }
		
        snprintf(str, STR_MAX, "/sbin/ifconfig %s addm %s",
             bridge, interface);
        if (run_cmd(str) != 0) {
            fprintf(stderr, "%s: Failed to add %s to %s\n",
                exec_name, interface, bridge);
            close(sd);
            return -1;
        }
	}

        return sd;
}
 
static int open_bpf(char *ifname)
{
	u_int blen = 0;
	struct ifreq ifreq;
	u_int arg;

	int sd = open("/dev/bpf2", O_RDWR);

	if (sd < 0) {
		return -1;
	}

	if (ioctl(sd, BIOCGBLEN, &blen) < 0) {
		close(sd);
		return -2;
	}

	bzero(&ifreq, sizeof(ifreq));
	strncpy(ifreq.ifr_name, ifname, IFNAMSIZ);

	arg = 0;
	if (ioctl(sd, BIOCSETIF, &ifreq) < 0) {
		close(sd);
		return -3;
	}

	arg = 0;
	if (ioctl(sd, BIOCSSEESENT, &arg) < 0) {
		close(sd);
		return -4;
	}

	arg = 1;
	if (ioctl(sd, BIOCPROMISC, &arg) < 0) {
		close(sd);
		return -5;
	}

	arg = 1;
	if (ioctl(sd, BIOCIMMEDIATE, &arg) < 0) {
		close(sd);
		return -6;
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
        if (*remove_bridge) {
				char str[STR_MAX];
				snprintf(str, STR_MAX, "/sbin/ifconfig %s destroy", remove_bridge);
				run_cmd(str);
        }
}
