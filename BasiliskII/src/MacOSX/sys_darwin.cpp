/*
 *	$Id$
 *
 *	sys_darwin.cpp - Extra Darwin system dependant routines. Called by:
 *
 *  sys_unix.cpp - System dependent routines, Unix implementation
 *
 *	Based on Apple's CDROMSample.c and Evan Jones' cd-discid.c patches
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
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

#import "sysdeps.h"

#import "prefs.h"

#define DEBUG 0
#import "debug.h"



/*
 *  This gets called when no "cdrom" prefs items are found
 *  It scans for available CD-ROM drives and adds appropriate prefs items
 */

void DarwinAddCDROMPrefs(void)
{
	mach_port_t				masterPort;	// The way to talk to the kernel
	io_iterator_t			allCDs;		// List of CD drives on the system
	CFMutableDictionaryRef	classesToMatch;
	io_object_t				nextCD;


	// Don't scan for drives if nocdrom option given
	if ( PrefsFindBool("nocdrom") )
		return;


	// Let this task talk to the guts of the kernel:
	if ( IOMasterPort(MACH_PORT_NULL, &masterPort) != KERN_SUCCESS )
		bug("IOMasterPort failed. Won't be able to do anything with CD drives\n");


	// CD media are instances of class kIOCDMediaClass
	classesToMatch = IOServiceMatching(kIOCDMediaClass); 
	if ( classesToMatch )
	{
		// Narrow the search a little further. Each IOMedia object
		// has a property with key kIOMediaEjectable.  We limit
		// the match only to those CDs that are actually ejectable
		CFDictionarySetValue(classesToMatch,
							 CFSTR(kIOMediaEjectableKey), kCFBooleanTrue); 
	}

	if ( IOServiceGetMatchingServices(masterPort,
									  classesToMatch, &allCDs) != KERN_SUCCESS )
	{
		D(bug("IOServiceGetMatchingServices failed. No CD media drives found?\n"));
		return;
	}


	// Iterate through each CD drive
	while ( nextCD = IOIteratorNext(allCDs))
	{
		char		bsdPath[MAXPATHLEN];
		CFTypeRef	bsdPathAsCFString =
						IORegistryEntryCreateCFProperty(nextCD,
														CFSTR(kIOBSDNameKey),
														kCFAllocatorDefault, 0);
		*bsdPath = '\0';
		if ( bsdPathAsCFString )
		{
			size_t devPathLength;

			strcpy(bsdPath, "/dev/");
			devPathLength = strlen(bsdPath);

			if ( CFStringGetCString((const __CFString *)bsdPathAsCFString,
									 bsdPath + devPathLength,
									 MAXPATHLEN - devPathLength,
									 kCFStringEncodingASCII) )
			{
				D(bug("CDROM BSD path: %s\n", bsdPath));
				PrefsAddString("cdrom", bsdPath);
			}
			else
				D(bug("Could not get BSD device path for CD\n"));

			CFRelease(bsdPathAsCFString);
		}
		else
			D(bug("Cannot determine bsdPath for CD\n"));
	}

	IOObjectRelease(nextCD);
	IOObjectRelease(allCDs);
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
	while ( nextFloppy = IOIteratorNext(allFloppies))
	{
		char		bsdPath[MAXPATHLEN];
		CFTypeRef	bsdPathAsCFString =
						IORegistryEntryCreateCFProperty(nextFloppy,
														CFSTR(kIOBSDNameKey),
														kCFAllocatorDefault, 0);
		long		size;
		CFTypeRef	sizeAsCFNumber =
						IORegistryEntryCreateCFProperty(nextFloppy,
														CFSTR(kIOMediaSizeKey),
														kCFAllocatorDefault, 0);

		if ( CFNumberGetValue((CFNumberRef)sizeAsCFNumber,
								kCFNumberSInt32Type, &size) )
		{
			D(bug("Got size of %ld\n", size));
			if ( size < 800 * 1024 || size > 1440 * 1024 )
			{
				D(puts("Device does not appear to be 800k or 1440k"));
				continue;
			}
		}
		else
			bug("Couldn't get kIOMediaSizeKey of device");
		
		
		*bsdPath = '\0';
		if ( bsdPathAsCFString )
		{
			size_t devPathLength;

			strcpy(bsdPath, "/dev/");
			devPathLength = strlen(bsdPath);

			if ( CFStringGetCString((const __CFString *)bsdPathAsCFString,
									 bsdPath + devPathLength,
									 MAXPATHLEN - devPathLength,
									 kCFStringEncodingASCII) )
			{
				D(bug("Floppy BSD path: %s\n", bsdPath));
				PrefsAddString("floppy", bsdPath);
			}
			else
				D(bug("Could not get BSD device path for floppy\n"));

			CFRelease(bsdPathAsCFString);
		}
		else
			D(bug("Cannot determine bsdPath for floppy\n"));
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
	while ( nextModem = IOIteratorNext(allModems))
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
