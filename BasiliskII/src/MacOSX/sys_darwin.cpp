/*
 *	$Id$
 *
 *	sys_darwin.cpp - Extra Darwin system dependant routines. Called by:
 *
 *  sys_unix.cpp - System dependent routines, Unix implementation
 *
 *	Based on Apple's CDROMSample.c and Evan Jones' cd-discid.c patches
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

#import <errno.h>
#import <sys/param.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/IOBSD.h>
#import <IOKit/serial/IOSerialKeys.h>
#import <IOKit/storage/IOMedia.h>
#import <IOKit/storage/IOMediaBSDClient.h>
#ifdef HAVE_IOKIT_STORAGE_IOBLOCKSTORAGEDEVICE_H
	#import <IOKit/storage/IOBlockStorageDevice.h>
#endif
#import <IOKit/storage/IOCDMedia.h>
#import <IOKit/storage/IOCDMediaBSDClient.h>
#import <CoreFoundation/CoreFoundation.h>

#include "sysdeps.h"

#include "sys.h"
#include "prefs.h"

#define DEBUG 0
#import "debug.h"


// Global variables
static volatile CFRunLoopRef media_poll_loop = NULL;
static bool media_thread_active = false;
static pthread_t media_thread;

// Prototypes
static void *media_poll_func(void *);

// From sys_unix.cpp
extern void SysMediaArrived(const char *path, int type);
extern void SysMediaRemoved(const char *path, int type);


/*
 *  Initialization
 */

void DarwinSysInit(void)
{
	if (!PrefsFindBool("nocdrom")) {
		media_thread_active = (pthread_create(&media_thread, NULL, media_poll_func, NULL) == 0);
		D(bug("Media poll thread installed (%ld)\n", media_thread));
	}
}


/*
 *  Deinitialization
 */

void DarwinSysExit(void)
{
	// Stop media poll thread
	if (media_thread_active) {
		while (media_poll_loop == NULL || !CFRunLoopIsWaiting(media_poll_loop))
			usleep(0);
		CFRunLoopStop(media_poll_loop);
		pthread_join(media_thread, NULL);
		media_poll_loop = NULL;
		media_thread_active = false;
	}
}


/*
 *  Get the BSD-style path of specified object
 */

static kern_return_t get_device_path(io_object_t obj, char *path, size_t maxPathLength)
{
	kern_return_t kernResult = KERN_FAILURE;
	CFTypeRef pathAsCFString = IORegistryEntryCreateCFProperty(obj, CFSTR(kIOBSDNameKey),
															   kCFAllocatorDefault, 0);
	if (pathAsCFString) {
		strcpy(path, "/dev/");
		size_t pathLength = strlen(path);
		if (CFStringGetCString((const __CFString *)pathAsCFString,
							   path + pathLength,
							   maxPathLength - pathLength,
							   kCFStringEncodingASCII))
			kernResult = KERN_SUCCESS;
		CFRelease(pathAsCFString);
	}
	return kernResult;
}


/*
 *  kIOMatchedNotification handler
 */

static void media_arrived(int type, io_iterator_t iterator)
{
	io_object_t obj;
	while ((obj = IOIteratorNext(iterator))) {
		char path[MAXPATHLEN];
		kern_return_t kernResult = get_device_path(obj, path, sizeof(path));
		if (kernResult == KERN_SUCCESS) {
			D(bug("Media Arrived: %s\n", path));
			SysMediaArrived(path, type);
		}
		kernResult = IOObjectRelease(obj);
		if (kernResult != KERN_SUCCESS) {
			fprintf(stderr, "IOObjectRelease() returned %d\n", kernResult);
		}
	}
}


/*
 *  kIOTerminatedNotification handler
 */

static void media_removed(int type, io_iterator_t iterator)
{
	io_object_t obj;
	while ((obj = IOIteratorNext(iterator))) {
		char path[MAXPATHLEN];
		kern_return_t kernResult = get_device_path(obj, path, sizeof(path));
		if (kernResult == KERN_SUCCESS) {
			D(bug("Media Removed: %s\n", path));
			SysMediaRemoved(path, type);
		}
		kernResult = IOObjectRelease(obj);
		if (kernResult != KERN_SUCCESS) {
			fprintf(stderr, "IOObjectRelease() returned %d\n", kernResult);
		}
	}
}


/*
 *  Media poll function
 *
 *  NOTE: to facilitate orderly thread termination, media_poll_func MUST end up waiting in CFRunLoopRun.
 *  Early returns must be avoided, even if there is nothing useful to be done here. See DarwinSysExit.
 */

static void dummy(void *) { };	// stub for dummy runloop source

static void *media_poll_func(void *)
{
	media_poll_loop = CFRunLoopGetCurrent();

	mach_port_t masterPort;
	kern_return_t kernResult;
	CFMutableDictionaryRef matchingDictionary;
	CFRunLoopSourceRef loopSource = NULL;
	CFRunLoopSourceRef dummySource = NULL;

	if ((kernResult = IOMasterPort(bootstrap_port, &masterPort)) != KERN_SUCCESS)
		fprintf(stderr, "IOMasterPort() returned %d\n", kernResult);
	else if ((matchingDictionary = IOServiceMatching(kIOCDMediaClass)) == NULL)
		fprintf(stderr, "IOServiceMatching() returned a NULL dictionary\n");
	else {
		matchingDictionary = (CFMutableDictionaryRef)CFRetain(matchingDictionary);
		IONotificationPortRef notificationPort = IONotificationPortCreate(kIOMasterPortDefault);
		loopSource = IONotificationPortGetRunLoopSource(notificationPort);
		CFRunLoopAddSource(media_poll_loop, loopSource, kCFRunLoopDefaultMode);

		io_iterator_t mediaArrivedIterator;
		kernResult = IOServiceAddMatchingNotification(notificationPort,
			kIOMatchedNotification,
			matchingDictionary,
			(IOServiceMatchingCallback)media_arrived,
			(void *)MEDIA_CD, &mediaArrivedIterator);
		if (kernResult != KERN_SUCCESS)
			fprintf(stderr, "IOServiceAddMatchingNotification() returned %d\n", kernResult);
		media_arrived(MEDIA_CD, mediaArrivedIterator);

		io_iterator_t mediaRemovedIterator;
		kernResult = IOServiceAddMatchingNotification(notificationPort,
			kIOTerminatedNotification,
			matchingDictionary,
			(IOServiceMatchingCallback)media_removed,
			(void *)MEDIA_CD, &mediaRemovedIterator);
		if (kernResult != KERN_SUCCESS)
			fprintf(stderr, "IOServiceAddMatchingNotification() returned %d\n", kernResult);
		media_removed(MEDIA_CD, mediaRemovedIterator);
	}

	if (loopSource == NULL) {
		// add a dummy runloop source to prevent premature return from CFRunLoopRun
		CFRunLoopSourceContext context = { 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, dummy };
		dummySource = CFRunLoopSourceCreate(NULL, 0, &context);
		CFRunLoopAddSource(media_poll_loop, dummySource, kCFRunLoopDefaultMode);
	}

	CFRunLoopRun();

	if (dummySource != NULL)
		CFRelease(dummySource);
	return NULL;
}


void DarwinAddFloppyPrefs(void)
{
	mach_port_t				masterPort;		// The way to talk to the kernel
	io_iterator_t			allFloppies;	// List of possible floppys
	CFMutableDictionaryRef	classesToMatch;
	io_object_t				nextFloppy;


	if ( IOMasterPort(MACH_PORT_NULL, &masterPort) != KERN_SUCCESS )
		bug("IOMasterPort failed. Won't be able to do anything with floppy drives\n");


	// This selects all partitions of all disks
	classesToMatch = IOServiceMatching(kIOMediaClass); 
	if ( classesToMatch )
	{
		// Skip drivers and partitions
		CFDictionarySetValue(classesToMatch,
							 CFSTR(kIOMediaWholeKey), kCFBooleanTrue); 
	
		// Skip fixed drives (hard disks?)
		CFDictionarySetValue(classesToMatch,
							 CFSTR(kIOMediaEjectableKey), kCFBooleanTrue); 
	}

	if ( IOServiceGetMatchingServices(masterPort,
									  classesToMatch, &allFloppies) != KERN_SUCCESS )
	{
		D(bug("IOServiceGetMatchingServices failed. No removable drives found?\n"));
		return;
	}

	// Iterate through each floppy
	while ((nextFloppy = IOIteratorNext(allFloppies)))
	{
		char		bsdPath[MAXPATHLEN];
		long		size = 0;
		Boolean gotSize = FALSE;
		CFTypeRef	sizeAsCFNumber =
						IORegistryEntryCreateCFProperty(nextFloppy,
														CFSTR(kIOMediaSizeKey),
														kCFAllocatorDefault, 0);

		if (sizeAsCFNumber)
		{
			gotSize = CFNumberGetValue((CFNumberRef)sizeAsCFNumber, kCFNumberSInt32Type, &size);
			CFRelease(sizeAsCFNumber);
		}

		if (gotSize)
		{
			D(bug("Got size of %ld\n", size));
			if ( size < 800 * 1024 || size > 1440 * 1024 )
			{
				D(puts("Device does not appear to be 800k or 1440k"));
				continue;
			}
		}
		else {
			D(puts("Couldn't get kIOMediaSizeKey of device"));
			continue; // if kIOMediaSizeKey is unavailable, we shouldn't use it anyway
		}

		if (get_device_path(nextFloppy, bsdPath, sizeof(bsdPath)) == KERN_SUCCESS) {
			PrefsAddString("floppy", bsdPath);
		} else {
			D(bug("Could not get BSD device path for floppy\n"));
		}
	}

	IOObjectRelease(nextFloppy);
	IOObjectRelease(allFloppies);
}


void DarwinAddSerialPrefs(void)
{
	mach_port_t				masterPort;		// The way to talk to the kernel
	io_iterator_t			allModems;		// List of modems on the system
	CFMutableDictionaryRef	classesToMatch;
	io_object_t				nextModem;


	if ( IOMasterPort(MACH_PORT_NULL, &masterPort) != KERN_SUCCESS )
		bug("IOMasterPort failed. Won't be able to do anything with modems\n");


    // Serial devices are instances of class IOSerialBSDClient
    classesToMatch = IOServiceMatching(kIOSerialBSDServiceValue);
	if ( classesToMatch )
	{
		// Narrow the search a little further. Each serial device object has
		// a property with key kIOSerialBSDTypeKey and a value that is one of
		// kIOSerialBSDAllTypes, kIOSerialBSDModemType, or kIOSerialBSDRS232Type.

        CFDictionarySetValue(classesToMatch,
                             CFSTR(kIOSerialBSDTypeKey),
                             CFSTR(kIOSerialBSDModemType));

        // This will find built-in and USB modems, but not serial modems.
	}

	if ( IOServiceGetMatchingServices(masterPort,
									  classesToMatch, &allModems) != KERN_SUCCESS )
	{
		D(bug("IOServiceGetMatchingServices failed. No modems found?\n"));
		return;
	}

	// Iterate through each modem
	while ((nextModem = IOIteratorNext(allModems)))
	{
		char		bsdPath[MAXPATHLEN];
		CFTypeRef	bsdPathAsCFString =
						IORegistryEntryCreateCFProperty(nextModem,
														CFSTR(kIOCalloutDeviceKey),
														// kIODialinDeviceKey?
														kCFAllocatorDefault, 0);
		*bsdPath = '\0';
		if ( bsdPathAsCFString )
		{
			if ( CFStringGetCString((const __CFString *)bsdPathAsCFString,
									 bsdPath, MAXPATHLEN,
									 kCFStringEncodingASCII) )
			{
				D(bug("Modem BSD path: %s\n", bsdPath));

				// Note that if there are multiple modems, we only get the last
				PrefsAddString("seriala", bsdPath);
			}
			else
				D(bug("Could not get BSD device path for modem\n"));

			CFRelease(bsdPathAsCFString);
		}
		else
			D(puts("Cannot determine bsdPath for modem\n"));
	}

	IOObjectRelease(nextModem);
	IOObjectRelease(allModems);


	// Getting a printer device is a bit harder. Creating a fake device
	// that emulates a simple printer (e.g. a HP DeskJet) is one possibility,
	// but for now I will just create a fake, safe, device entry:

	PrefsAddString("serialb", "/dev/null");
}


#ifdef MAC_OS_X_VERSION_10_2
/*
 *  Read CD-ROM TOC (binary MSF format, 804 bytes max.)
 */

bool DarwinCDReadTOC(char *name, uint8 *toc)
{
	char				*c, *devname;
	int					fd;


	// The open filehandle is something like /dev/disk5s1
	// The DKIOCCDREADTOC ioctl needs the original cd file,
	// so we strip the s1 suffix off it, and open the file just for this ioctl

	devname = strdup(name);
	if ( ! devname )
		return false;

	for ( c = devname; *c; ++c ) ;	// Go to the end of the name,
	--c, --c;						// point to the 's1' on the end,
	*c = '\0';						// and truncate the string

	fd = open(devname, O_RDONLY);
	if ( ! fd )
	{
		printf("Failed to open CD device %s for ioctl\n", devname);
		free(devname);
		return false;
	}

	D(bug("Opened %s for ioctl()\n", devname));

	dk_cd_read_toc_t	TOCrequest;

	// Setup the ioctl request structure:

	memset(&TOCrequest, 0, sizeof(TOCrequest));
	TOCrequest.buffer = toc;
	TOCrequest.bufferLength = 804;
	TOCrequest.formatAsTime = kCDTrackInfoAddressTypeTrackNumber;

	if ( ioctl(fd, DKIOCCDREADTOC, &TOCrequest) < 0 )
	{
		printf("ioctl(DKIOCCDREADTOC) failed: %s\n", strerror(errno));
		close(fd);
		free(devname);
		return false;
	}
	if ( TOCrequest.bufferLength < sizeof(CDTOC) )
	{
		printf("ioctl(DKIOCCDREADTOC): only read %d bytes (a CDTOC is at least %d)\n",
											TOCrequest.bufferLength, (int)sizeof(CDTOC));
		close(fd);
		free(devname);
		return false;
	}
	D(bug("ioctl(DKIOCCDREADTOC) read %d bytes\n", TOCrequest.bufferLength));

	close(fd);
	free(devname);
	return true;
}
#endif
