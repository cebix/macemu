/*
 *	Controller.h - Simple application window management. 
 *
 *	$Id$
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

#import <Cocoa/Cocoa.h>
#import "PrefsEditor.h"

// If the application supports multiple windows,
// ENABLE_MULTIPLE can be defined in config.h

@interface Controller : NSApplication
{
#ifdef ENABLE_MULTIPLE
	NSMutableArray			*emulators;		// Array of created Emulators
#endif
	IBOutlet Emulator		*theEmulator;
	IBOutlet PrefsEditor	*thePrefsEditor;
}

- (void) dispatchKeyEvent:	(NSEvent *)event
					 type:	(NSEventType)type;
- (void) dispatchEvent:		(NSEvent *)event
					 type:	(NSEventType)type;


- (IBAction) HelpHowTo:		(id)sender;
- (IBAction) HelpToDo:		(id)sender;
- (IBAction) HelpVersions:	(id)sender;

#ifdef ENABLE_MULTIPLE
- (IBAction) NewEmulator:	(id)sender;
- (IBAction) PauseAll:		(id)sender;
- (IBAction) RunAll:		(id)sender;
- (IBAction) TerminateAll:	(id)sender;
#endif

- (BOOL)  isAnyEmulatorDisplayingSheets;
- (BOOL)  isAnyEmulatorRunning;
- (short) emulatorCreatedCount;		// If any emulator environments have been setup, count how many

@end
