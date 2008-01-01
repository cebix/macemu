/*
 *	$Id$
 *
 *	misc_macosx.m - Miscellaneous Mac OS X routines.
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

#import <AppKit/AppKit.h>

#import "sysdeps.h"			// Types used in Basilisk C++ code

#import <prefs.h>

#define DEBUG 0
#import <debug.h>



/************************************************************************/
/* Display Errors and Warnings in a sliding thingy attached to a		*/
/* particular window, instead of as a separate window (Panel or Dialog)	*/
/************************************************************************/

void ErrorSheet (NSString * message, NSWindow * window)
{
	NSLog(message);
	NSBeginCriticalAlertSheet(message, nil, nil, nil, window,
									   nil, nil, nil, NULL, @"");
	while ( [window attachedSheet] )
		sleep(1);
		//[NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow: 1.0]];
}

void ErrorSheet (NSString * message1, NSString * message2,
				 NSString * button,   NSWindow * window)
{
	NSLog(message1);
	NSLog(message2);
	NSBeginCriticalAlertSheet(message1, button, nil, nil, window,
									nil, nil, nil, NULL, message2);
	while ( [window attachedSheet] )
		sleep(1);
		//[NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow: 1.0]];
}


void WarningSheet (NSString * message, NSWindow * window)
{
	NSLog(message);
	NSBeginAlertSheet(message, nil, nil, nil, window,
							   nil, nil, nil, NULL, @"");
}

void WarningSheet (NSString * message1, NSString * message2,
				   NSString * button,   NSWindow * window)
{
	NSLog(message1);
	NSLog(message2);
	NSBeginAlertSheet(message1, button, nil, nil, window,
							nil, nil, nil, NULL, message2);
}


void InfoSheet (NSString * message, NSWindow * window)
{
	NSLog(message);
	NSBeginInformationalAlertSheet(message, nil, nil, nil, window,
											nil, nil, nil, NULL, @"");
}

void InfoSheet (NSString * message1, NSString * message2,
				NSString * button,   NSWindow * window)
{
	NSLog(message1);
	NSLog(message2);
	NSBeginInformationalAlertSheet(message1, nil, nil, nil, window,
										nil, nil, nil, NULL, message2);
}

void EndSheet (NSWindow * window)
{
	[[window attachedSheet] close];
}

// Convert a frequency (i.e. updates per second) to a 60hz tick delay, and update prefs
int	frequencyToTickDelay (float freq)
{
	if ( freq == 0.0 )
		return 0;
	else
	{
		int delay = (int) (60.0 / freq);

		PrefsReplaceInt32("frameskip", delay);
		return delay;
	}
}
