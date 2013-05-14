/*
 *  runtool.m - Run an external program as root for networking
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

#include <unistd.h>

#include <net/if.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <net/bpf.h>
#include <fcntl.h>

#include <strings.h>

#include <Carbon/Carbon.h>

FILE * run_tool(const char *ifName);

FILE * run_tool(const char *ifName)
{
	OSStatus auth_status;
	FILE *fp;
	char *args[] = {"etherslavetool", NULL, NULL};
	int ret;
	const char *path;
	AuthorizationFlags auth_flags;
	AuthorizationRef auth_ref;
	AuthorizationItem auth_items[1];
	AuthorizationRights auth_rights;

	path = [[[NSBundle mainBundle]
			pathForResource:@"etherslavetool" ofType: nil] UTF8String];

	if (path == NULL) {
		return NULL;
	}

	args[1] = (char *)ifName;
  
	auth_flags = kAuthorizationFlagExtendRights |
		kAuthorizationFlagInteractionAllowed |
		kAuthorizationFlagPreAuthorize;
 
	auth_items[0].name = "system.privilege.admin";
	auth_items[0].valueLength = 0;
	auth_items[0].value = NULL;
	auth_items[0].flags = 0;

	auth_rights.count = sizeof (auth_items) / sizeof (auth_items[0]);
	auth_rights.items = auth_items;
  
	auth_status = AuthorizationCreate(&auth_rights,
					  kAuthorizationEmptyEnvironment,
					  auth_flags,
					  &auth_ref);
  
	if (auth_status != errAuthorizationSuccess) {
		fprintf(stderr, "%s: AuthorizationCreate() failed.\n", __func__);
		return NULL;
	}

	auth_status = AuthorizationExecuteWithPrivileges(auth_ref,
							 path,
							 kAuthorizationFlagDefaults,
							 args + 1,
							 &fp);

	if (auth_status != errAuthorizationSuccess) {
		fprintf(stderr, "%s: AuthorizationExecWithPrivileges() failed.\n", __func__);
		return NULL;
	}

	return fp;
}
