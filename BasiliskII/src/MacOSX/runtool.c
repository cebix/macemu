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

FILE * run_tool(const char *if_name, const char *tool_name);

FILE * run_tool(const char *if_name, const char *tool_name)
{
	OSStatus auth_status;
	FILE *fp = NULL;
	char *args[] = {NULL, NULL, NULL};
	int ret;
	char path_buffer[256];
	AuthorizationFlags auth_flags;
	AuthorizationRef auth_ref;
	AuthorizationItem auth_items[1];
	AuthorizationRights auth_rights;
	CFBundleRef bundle_ref;
	CFURLRef url_ref;
	CFStringRef path_str;
	CFStringRef tool_name_str;
	char c;

	bundle_ref = CFBundleGetMainBundle();
	if(bundle_ref == NULL) {
		return NULL;
	}

	tool_name_str = CFStringCreateWithCString(NULL, tool_name,
						  kCFStringEncodingUTF8);

	url_ref = CFBundleCopyResourceURL(bundle_ref, tool_name_str,
					 NULL, NULL);
	CFRelease(tool_name_str);

	if(url_ref == NULL) {
		return NULL;
	}

	path_str = CFURLCopyFileSystemPath(url_ref, kCFURLPOSIXPathStyle);
	CFRelease(url_ref);

	if(path_str == NULL) {
		return NULL;
	}

	if(!CFStringGetCString(path_str, path_buffer, sizeof(path_buffer),
			       kCFStringEncodingUTF8)) {
		CFRelease(path_str);
		return NULL;
	}
	CFRelease(path_str);

	args[0] = (char *)tool_name;
	args[1] = (char *)if_name;
  
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
		fprintf(stderr, "%s: AuthorizationCreate() failed.\n",
			__func__);
		return NULL;
	}

	auth_status = AuthorizationExecuteWithPrivileges(auth_ref,
							 path_buffer,
							 kAuthorizationFlagDefaults,
							 args + 1,
							 &fp);

	if (auth_status != errAuthorizationSuccess) {
		fprintf(stderr, "%s: AuthorizationExecWithPrivileges() failed.\n", 
			__func__);
		return NULL;
	}

	if(fread(&c, 1, 1, fp) != 1) {
	  fclose(fp);
	  return NULL;
	}

	return fp;
}
