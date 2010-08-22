/*
 *	$Id$
 *
 *	prefs_macosx.mm - Enables access to SheepShaver preferences while
 *                    SheepShaver is running (on Mac OS X).
 *
 *  Copyright (C) 2007 Alexei Svitkine
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


#include "sysdeps.h"

// The _UINT64 define is needed to guard against a typedef mismatch with Snow Leopard headers.
#define _UINT64

#include <Cocoa/Cocoa.h>
#include "VMSettingsController.h"


@interface SheepShaverMain : NSObject
{
	NSArray *nibObjects;
	NSWindow *prefsWindow;
}
@end

@implementation SheepShaverMain


- (NSArray*) loadPrefsNibFile
{
	NSNib *nib = [[NSNib alloc] initWithNibNamed:@"VMSettingsWindow" bundle:nil];
	NSArray *objects = nil;
 
	if (![nib instantiateNibWithOwner:[VMSettingsController sharedInstance] topLevelObjects:&objects]) {
		NSLog(@"Could not load Prefs NIB file!\n");
		return nil;
	}

	NSLog(@"%d objects loaded\n", [objects count]);

	// Release the raw nib data.
	[nib release];
 
	// Release the top-level objects so that they are just owned by the array.
	[objects makeObjectsPerformSelector:@selector(release)];

	prefsWindow = nil;
 	for (int i = 0; i < [objects count]; i++) {
		NSObject *object = [objects objectAtIndex:i];
		NSLog(@"Got %@", object);

		if ([object isKindOfClass:[NSWindow class]]) {
			prefsWindow = (NSWindow *) object;
			break;
		}
	}

	if (prefsWindow == nil) {
		NSLog(@"Could not find NSWindow in Prefs NIB file!\n");
		return nil;
	}

	return objects;
}


- (void) openPreferences:(id)sender
{
	NSAutoreleasePool *pool;

	if (nibObjects == nil) {
		nibObjects = [self loadPrefsNibFile];
		if (nibObjects == nil)
			return;
		[nibObjects retain];
	}

	pool = [[NSAutoreleasePool alloc] init];
	[[VMSettingsController sharedInstance] setupGUI];
	[NSApp runModalForWindow:prefsWindow];
	[pool release];
}

@end

/*
 *  Initialization
 */

void prefs_init(void)
{
	NSAutoreleasePool *pool;
	NSMenu *appMenu;
	NSMenuItem *menuItem;

	pool = [[NSAutoreleasePool alloc] init];

	appMenu = [[[NSApp mainMenu] itemAtIndex:0] submenu];
	menuItem = [[NSMenuItem alloc] initWithTitle:@"Preferences..." action:@selector(openPreferences:) keyEquivalent:@","];
	[appMenu insertItem:menuItem atIndex:2];
	[appMenu insertItem:[NSMenuItem separatorItem] atIndex:3];
	[menuItem release];
	
	[NSApp setDelegate:[[SheepShaverMain alloc] init]];

	[pool release];
}


/*
 *  Deinitialization
 */

void prefs_exit(void)
{
}
