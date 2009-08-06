/*
 *  AppController.mm - Cocoa SheepShaver launcher for Mac OS X
 *
 *  Copyright (C) 2009 Alexei Svitkine
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

#import "AppController.h"
#import "VMListController.h"

@implementation AppController

- (void) awakeFromNib
{
	[self openVirtualMachinesList:self];
	[NSApp setDelegate:self];
}

- (IBAction) openVirtualMachinesList: (id) sender
{
	[[VMListController sharedInstance] showWindow:sender];
}

- (BOOL) applicationShouldHandleReopen: (NSApplication *) app hasVisibleWindows: (BOOL) hasVisible
{
	if (!hasVisible)
		[self openVirtualMachinesList:self];
	return YES;
}

@end
