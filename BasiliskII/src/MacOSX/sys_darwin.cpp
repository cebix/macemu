/*
 *	$Id$
 *
 *	sys_darwin.cpp - Extra Darwin system dependant routines. Called by:
 *
 *  sys_unix.cpp - System dependent routines, Unix implementation
 *
 *	Based on Apple's CDROMSample.c and Evan Jones' cd-discid.c patches
 *
 *  Basilisk II (C) 1997-2003 Christian Bauer
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
#import <IOKit/storage/IOMedia.h>
#import <IOKit/storage/IOMediaBSDClient.h>
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
		bug("IOMasterPort failed. Won't be able to do anything with CD drives");


	// CD media are instances of class kIOCDMediaClass
	classesToMatch = IOServiceMatching(kIOCDMediaClass); 
	if ( classesToMatch )
	{
		// Narrow the search a little further. Each IOMedia object
		// has a property with key kIOMediaEjectable.  We limit
		// the match only to those CDs that are actually ejectable
		CFDictionarySetValue(classesToMatch,
							 CFSTR(kIOMediaEjectable), kCFBooleanTrue); 
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
						IORegistryEntryCreateCFProperty(nextCD, CFSTR(kIOBSDName),
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
				// If we try to do raw reads on the file bsdPath (e.g. /dev/disk5),
				// we get a lot of extra padding in the data. For some reason,
				// the device we need has a different name (e.g. /dev/disk5s1)
				//strcat(bsdPath, "s1");
				D(bug("CDROM BSD path: %s\n", bsdPath));
			}
			else
				D(bug("Could not get BSD device path for CD\n"));

			CFRelease(bsdPathAsCFString);
		}

		PrefsAddString("cdrom", bsdPath);
	}

	IOObjectRelease(nextCD);
	IOObjectRelease(allCDs);
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
