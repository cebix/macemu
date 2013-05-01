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

FILE * runTool(const char *ifName);

FILE * runTool(const char *ifName) {
	OSStatus authStatus;
	FILE *fp;
	char *args[] = {"ethsheeptool", NULL, NULL};
	int ret;
	const char *path;

	path = [[[NSBundle mainBundle]
			pathForResource:@"etherslavetool" ofType: nil] UTF8String];

	if(path == NULL) {
		return NULL;
	}

	AuthorizationFlags authFlags;
	AuthorizationRef authRef;
	AuthorizationItem authItems[1];
	AuthorizationRights authRights;

	args[1] = (char *)ifName;
  
	authFlags = kAuthorizationFlagExtendRights |
		kAuthorizationFlagInteractionAllowed |
		kAuthorizationFlagPreAuthorize;
 
	authItems[0].name = "system.privilege.admin";
	authItems[0].valueLength = 0;
	authItems[0].value = NULL;
	authItems[0].flags = 0;

	authRights.count = sizeof (authItems) / sizeof (authItems[0]);
	authRights.items = authItems;
  
	authStatus = AuthorizationCreate(&authRights,
					 kAuthorizationEmptyEnvironment,
					 authFlags,
					 &authRef);
  
	if(authStatus != errAuthorizationSuccess) {
		fprintf(stderr, "%s: AuthorizationCreate() failed.\n", __func__);
		return NULL;
	}

	authStatus = AuthorizationExecuteWithPrivileges(authRef,
							path,
							kAuthorizationFlagDefaults,
							args + 1,
							&fp);

	if(authStatus != errAuthorizationSuccess) {
		fprintf(stderr, "%s: AuthorizationExecWithPrivileges() failed.\n", __func__);
		return NULL;
	}

	return fp;
}
