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

static int openBpf(char *ifname);
static int retreiveAuthInfo(void);
static int mainLoop(int sd);

int main(int argc, char **argv) {
	char *ifName;
	int ret;
	int sd;

	if(argc != 2) {
		return 255;
	}

	ifName = argv[1];

	ret = retreiveAuthInfo();
	if(ret != 0) {
		return 254;
	}

	fflush(stdout);

	sd = openBpf(ifName);
	if(sd < 0) {
		return 253;
	}

	fflush(stdout);

	ret = mainLoop(sd);

	close(sd);

	if(ret < 0) {
		return 252;
	}

	return 0;
}

static int mainLoop(int sd) {
	fd_set readSet;
	char *outgoing, *incoming;
	unsigned short *outLen;
	unsigned short *inLen;
	int inIndex, outIndex;
	u_int blen = 0;
	int ret;
	int fret = 0;
	struct bpf_hdr *hdr;
	int pktLen;
	int frameLen;
	int pad;

	if(ioctl(sd, BIOCGBLEN, &blen) < 0) {
		return -1;
	}

	incoming = malloc(blen);
	if(incoming == NULL) {
		return -2;
	}

	outgoing = malloc(blen);
	if(outgoing == NULL) {
		free(outgoing);
		return -3;
	}

	inIndex = 0;
	outIndex = 0;

	outLen = (unsigned short *)outgoing;

	while(1) {
		int i;
		FD_ZERO(&readSet);
		FD_SET(0, &readSet);
		FD_SET(sd, &readSet);

		ret = select(sd + 1, &readSet, NULL, NULL, NULL);
		if(ret < 0) {
			fret = -4;
			break;
		}

		if(FD_ISSET(0, &readSet)) {
			if(outIndex < 2) {
				ret = read(0, outgoing + outIndex, 2-outIndex);
			} else {
				ret = read(0, outgoing + outIndex, *outLen - outIndex + 2);
			}

			if(ret < 1) {
				fret = -5;
				break;
			}

			outIndex += ret;
			if(outIndex > 1) {
				fflush(stdout);

				if((*outLen + 2) > blen) {
					fret = -6;
					break;
				}

				if(outIndex == (*outLen + 2)) {
					ret = write(sd, outLen + 1, *outLen);
					if(ret != *outLen) {
						fret = -7;
						break;
					}
					outIndex = 0;
				}
			}

		}

		if(FD_ISSET(sd, &readSet)) {
			int i;

			ret = read(sd, incoming, blen);
			if(ret < 1) {
				fret = -8;
				break;
			}

			hdr = (struct bpf_hdr *)incoming;
			inLen = (unsigned short *)(incoming + 16);

			do {
				pktLen = hdr->bh_caplen;
				frameLen = pktLen + 18;

				if((pktLen < 0) || (frameLen > ret) || (frameLen < 0)) {
					fret = -9;
					break;
				}
				*inLen = pktLen;

				write(0, inLen, pktLen + 2);
				if((frameLen & 0x03) == 0) {
					pad = 0;
				} else {
					pad = 4 - (frameLen & 0x03);
				}

				ret -= (frameLen + pad);
				hdr = (struct bpf_hdr *)((unsigned char *)hdr + frameLen + pad);
				inLen = (unsigned short *)((unsigned char *)hdr + 16);
			} while (ret > 0);

			if(fret != 0) {
				break;
			}
		}
	}

	free(incoming);
	free(outgoing);

	return fret;
}

static int retreiveAuthInfo(void) {
	AuthorizationRef aRef;
	OSStatus status;
	AuthorizationRights myRights;
	AuthorizationRights *newRights;
	AuthorizationItem *myItem;
	AuthorizationItem myItems[1];
	AuthorizationItemSet *mySet;
	int i;

	status = AuthorizationCopyPrivilegedReference(&aRef, kAuthorizationFlagDefaults);
	if(status != errAuthorizationSuccess) {
		return -1;
	}

	status = AuthorizationCopyInfo(aRef, NULL, &mySet);
	if(status != errAuthorizationSuccess) {
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
	if(status != errAuthorizationSuccess) {
		AuthorizationFreeItemSet(mySet);
		AuthorizationFree(aRef, kAuthorizationFlagDestroyRights);
		return -2;
	}

	AuthorizationFreeItemSet(newRights);
	AuthorizationFreeItemSet(mySet);
	AuthorizationFree(aRef, kAuthorizationFlagDestroyRights);  

	return 0;
}
 
static int openBpf(char *ifname) {
	u_int blen = 0;
	struct ifreq ifreq;
	u_int arg;

	int sd = open("/dev/bpf2", O_RDWR);

	if(sd < 0) {
		return -1;
	}

	if(ioctl(sd, BIOCGBLEN, &blen) < 0) {
		close(sd);
		return -2;
	}

	bzero(&ifreq, sizeof(ifreq));
	strncpy(ifreq.ifr_name, ifname, IFNAMSIZ);

	arg = 0;
	if(ioctl(sd, BIOCSETIF, &ifreq) < 0) {
		close(sd);
		return -3;
	}

	arg = 0;
	if(ioctl(sd, BIOCSSEESENT, &arg) < 0) {
		close(sd);
		return -4;
	}

	arg = 1;
	if(ioctl(sd, BIOCPROMISC, &arg) < 0) {
		close(sd);
		return -5;
	}

	arg = 1;
	if(ioctl(sd, BIOCIMMEDIATE, &arg) < 0) {
		close(sd);
		return -6;
	}

	return sd;
}
