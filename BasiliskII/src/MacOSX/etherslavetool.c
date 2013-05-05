/*
 *  etherslavetool.c - Reads and writes raw ethernet packets usng bpf
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

static int open_bpf(char *ifname);
static int retreive_auth_info(void);
static int main_loop(int sd);

int main(int argc, char **argv)
{
	char *if_name;
	int ret;
	int sd;

	if (argc != 2) {
		return 255;
	}

	if_name = argv[1];

	ret = retreive_auth_info();
	if (ret != 0) {
		return 254;
	}

	fflush(stdout);

	sd = open_bpf(if_name);
	if (sd < 0) {
		return 253;
	}

	fflush(stdout);

	ret = main_loop(sd);

	close(sd);

	if (ret < 0) {
		return 252;
	}

	return 0;
}

static int main_loop(int sd)
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

	if (ioctl(sd, BIOCGBLEN, &blen) < 0) {
		return -1;
	}

	incoming = malloc(blen);
	if (incoming == NULL) {
		return -2;
	}

	outgoing = malloc(blen);
	if (outgoing == NULL) {
		free(outgoing);
		return -3;
	}

	in_index = 0;
	out_index = 0;

	out_len = (unsigned short *)outgoing;

	while (1) {
		int i;
		FD_ZERO(&readSet);
		FD_SET(0, &readSet);
		FD_SET(sd, &readSet);

		ret = select(sd + 1, &readSet, NULL, NULL, NULL);
		if (ret < 0) {
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
				fret = -5;
				break;
			}

			out_index += ret;
			if (out_index > 1) {
				fflush(stdout);

				if ((*out_len + 2) > blen) {
					fret = -6;
					break;
				}

				if (out_index == (*out_len + 2)) {
					ret = write(sd, out_len + 1, *out_len);
					if (ret != *out_len) {
						fret = -7;
						break;
					}
					out_index = 0;
				}
			}

		}

		if (FD_ISSET(sd, &readSet)) {
			int i;

			ret = read(sd, incoming, blen);
			if (ret < 1) {
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

				write(0, in_len, pkt_len + 2);
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
