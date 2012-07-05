/*
 *	Controller.m - Simple application window management. 
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

#import "Controller.h"
#import "Emulator.h"

#import "sysdeps.h"				// Types used in Basilisk C++ code

#import <main.h>
#import <prefs.h>

#define DEBUG 0
#import <debug.h>

#import "misc_macosx.h"
#import "video_macosx.h"

@implementation Controller

//
// Standard NSApplication methods that we override
//

- (id) init
{
#ifdef ENABLE_MULTIPLE
	emulators  = [NSMutableArray new];
#endif
	return [super init];
}

- (void) dealloc
{
#ifdef ENABLE_MULTIPLE
	[emulators	dealloc];
#endif
	[super		dealloc];
}

- (void) awakeFromNib
{
#ifdef ENABLE_MULTIPLE
	[self NewEmulator: self];			// So the user gets something on screen
#endif
	[[NSApplication sharedApplication]
		setDelegate: self];				// Enable applicationShouldTerminate
}

- (void) sendEvent: (NSEvent *)event;
{
	// We can either process the event ourselves,
	// or pass it to our superclass for the other UI objects to process
	bool	passToSuper = false;

	if ( [self isAnyEmulatorDisplayingSheets] ||
			[[thePrefsEditor window] isVisible] || ! [self isAnyEmulatorRunning] )
		passToSuper = true;

	if ( [[theEmulator screen] isFullScreen] )
		passToSuper = false;

	if ( passToSuper )
		[super sendEvent: event];		// NSApplication default
	else
	{
		NSEventType	type = [event type];

		if ( type == NSKeyUp || type == NSKeyDown || type == NSFlagsChanged )
			[self dispatchKeyEvent: event
							  type: type];
		else
			[self dispatchEvent: event
						   type: type];
	}
}

// NSApplication methods which are invoked through delegation

- (BOOL) applicationShouldTerminateAfterLastWindowClosed: (NSApplication *)app
{	return YES;  }

- (NSApplicationTerminateReply) applicationShouldTerminate: (NSApplication *)app
{
	short	count;
	const char *stillRunningMessage;

	if ( [thePrefsEditor hasEdited] )
		if ( ChoiceAlert("Preferences have been edited",
									"Save changes", "Quit") )
			SavePrefs();

//	if ( edited )
//	{
//		NSString *title = [NSString stringWithCString: getString(STR_WARNING_ALERT_TITLE)],
//				 *msg   = @"Preferences have been edited",
//				 *def   = @"Save changes",
//				 *alt   = @"Quit Application",
//				 *other = @"Continue";
//
//		switch ( NSRunAlertPanel(title, msg, def, alt, other, nil) )
//		{
//			case NSAlertDefault:	savePrefs();
//			case NSAlertAlternate:	return NSTerminateNow;
//			case NSAlertOther:		return NSTerminateCancel;
//		}
//	}


	if ( [[thePrefsEditor window] isVisible] )
		[[thePrefsEditor window] performClose: self];


	count = [self emulatorCreatedCount];

	if ( count > 0 )
	{
		if ( count > 1 )
			stillRunningMessage = "Emulators are still running\nExiting Basilisk may lose data";
		else
			stillRunningMessage = "Emulator is still running\nExiting Basilisk may lose data";
		if ( ! ChoiceAlert(stillRunningMessage, "Exit", "Continue") )
			return NSTerminateCancel;				// NSTerminateLater?
	}

	return NSTerminateNow;
}


// Event dispatching, called by sendEvent

- (void) dispatchKeyEvent: (NSEvent *)event
					 type: (NSEventType)type
{
	EmulatorView	*view;

#ifdef ENABLE_MULTIPLE
	// We need to work out what window's Emulator should receive these messages


	int	tmp;

	for ( tmp = 0; tmp < [emulators count], ++tmp )
	{
		theEmulator = [emulators objectAtIndex: tmp];
		view = [theEmulator screen];

		if ( [ theEmulator isRunning ] &&
				( [[theEmulator window] isKeyWindow] || [view isFullScreen] ) )
			break;
	}
	
	if ( tmp < [emulators count] )		// i.e. if we exited the for loop
#else
	view = [theEmulator screen];

	if ( [theEmulator isRunning] &&
				( [[theEmulator window] isKeyWindow] || [view isFullScreen] ) )
#endif
	{
		D(NSLog(@"Got a key event - %d\n", [event keyCode]));
		switch ( type )
		{
			case NSKeyUp:
				[view keyUp: event];
				break;
			case NSKeyDown:
				D(NSLog(@"%s - NSKeyDown - %@", __PRETTY_FUNCTION__, event));
				[view keyDown: event];
				break;
			case NSFlagsChanged:
				[view flagsChanged: event];
				break;
			default:
				NSLog(@"%s - Sent a non-key event (logic error)",
						__PRETTY_FUNCTION__);
				[super sendEvent: event];
		}
	}
	else				// No Basilisk window is key (maybe a panel or pane).
		[super sendEvent: event];				// Call NSApplication default

}

- (void) dispatchEvent: (NSEvent *)event
				  type: (NSEventType)type				
{
	EmulatorView	*view;
	BOOL			fullScreen;

#ifdef ENABLE_MULTIPLE
	// We need to work out what window's Emulator should receive these messages


	int	tmp;

	for ( tmp = 0; tmp < [emulators count], ++tmp )
	{
		theEmulator = [emulators objectAtIndex: tmp];
		view = [theEmulator screen];
		fullScreen = [view isFullScreen];

		if ( [theEmulator isRunning] &&
				( fullScreen || [[theEmulator window] isMainWindow] ) )
			break;
	}
	
	if ( tmp < [emulators count] )		// i.e. if we exited the for loop
#else
	view = [theEmulator screen];
	fullScreen = [view isFullScreen];

	if ( [theEmulator isRunning] &&
				( fullScreen || [[theEmulator window] isMainWindow] ) )
#endif
	{
		if ( fullScreen || [view mouseInView: event] )
		{
			switch ( type )
			{
				case NSLeftMouseDown:
					[view mouseDown: event];
					break;
				case NSLeftMouseUp:
					[view mouseUp: event];
					break;
				case NSLeftMouseDragged:
				case NSMouseMoved:
					if ( fullScreen )
						[view fullscreenMouseMove];
					else
						[view processMouseMove: event];
					break;
				default:
					[super sendEvent: event];		// NSApplication default
			}
			return;
		}
	}

	// Either the pointer is not in the Emulator's screen, no Basilisk window is running,
	// or no Basilisk window is main (e.g. there might be a panel or pane up).
	//
	// We should just be calling NSApp's default sendEvent, but then for some reason
	// mouseMoved events are still passed to our EmulatorView, so we filter them out.

	if ( type != NSMouseMoved )
		[super sendEvent: event];
}


// Methods to display documentation:

- (IBAction) HelpHowTo: (id)sender
{
	NSString	*path = [[NSBundle mainBundle] pathForResource: @"HowTo"
														ofType: @"html"];

	if ( ! path )
		InfoSheet(@"Cannot find HowTo.html", [theEmulator window]);
	else
		if ( ! [[NSWorkspace sharedWorkspace] openFile: path] )
			InfoSheet(@"Cannot open HowTo.html with default app", [theEmulator window]);
}

- (IBAction) HelpToDo: (id)sender
{
	NSString	*path = [[NSBundle mainBundle] pathForResource: @"ToDo"
														ofType: @"html"];

	if ( ! path )
		InfoSheet(@"Cannot find ToDo.html", [theEmulator window]);
	else
		if ( ! [[NSWorkspace sharedWorkspace] openFile: path
									   withApplication: @"TextEdit"] )
			InfoSheet(@"Cannot open ToDo.html with TextEdit", [theEmulator window]);
}

- (IBAction) HelpVersions: (id)sender
{
	NSString	*path = [[NSBundle mainBundle] pathForResource: @"Versions"
														ofType: @"html"];

	if ( ! path )
		InfoSheet(@"Cannot find Versions.html", [theEmulator window]);
	else
		if ( ! [[NSWorkspace sharedWorkspace] openFile: path
									   withApplication: @"TextEdit"] )
			InfoSheet(@"Cannot open Versions.html with TextEdit",
												[theEmulator window]);
}


// Menu items which for managing more than one window

#ifdef ENABLE_MULTIPLE

- (IBAction) NewEmulator: (id)sender
{
	NSString	*title;

	if ( ! [NSBundle loadNibNamed:@"Win512x342" owner:self] )
	{
		NSLog(@"%s - LoadNibNamed@Win512x342 failed", __PRETTY_FUNCTION__);
		return;
	}

	if ( theEmulator == nil)
	{
		NSLog(@"%s - Newly created emulator's NIB stuff not fully linked?", __PRETTY_FUNCTION__);
		return;
	}

	[emulators addObject: theEmulator];
	title = [NSString localizedStringWithFormat:@"BasiliskII Emulator %d", [emulators count]];
	[theEmulator -> win setTitle: title];
}

- (IBAction) PauseAll: (id)sender
{
	[emulators makeObjectsPerformSelector:@selector(Suspend:)
							   withObject:self];
}

- (IBAction) RunAll: (id)sender
{
	[emulators makeObjectsPerformSelector:@selector(Resume:)
							   withObject:self];
}

- (IBAction) TerminateAll: (id)sender
{
	[emulators makeObjectsPerformSelector:@selector(Terminate:)
							   withObject:self];
}

#endif

- (BOOL) isAnyEmulatorDisplayingSheets
{
#ifdef ENABLE_MULTIPLE
	int	tmp;

	for ( tmp = 0; tmp < [emulators count], ++tmp )
		if ( [[[emulators objectAtIndex: tmp] window] attachedSheet] )
			break;
	
	if ( tmp < [emulators count] )		// i.e. if we exited the for loop
#else
	if ( [[theEmulator window] attachedSheet] )
#endif
		return TRUE;

	return FALSE;
}

- (BOOL) isAnyEmulatorRunning
{
#ifdef ENABLE_MULTIPLE
	int	tmp;

	for ( tmp = 0; tmp < [emulators count], ++tmp )
		if ( [[emulators objectAtIndex: tmp] isRunning] )
			break;
	
	if ( tmp < [emulators count] )		// i.e. if we exited the for loop
#else
	if ( [theEmulator isRunning] )
#endif
		return TRUE;

	return FALSE;
}

- (short) emulatorCreatedCount
{
	short	count = 0;
#ifdef ENABLE_MULTIPLE
	int		tmp;

	for ( tmp = 0; tmp < [emulators count], ++tmp )
		if ( [[emulators objectAtIndex: tmp] uaeCreated] )
			++count;
#else
	if ( [theEmulator uaeCreated] )
		++count;
#endif

	return count;
}

@end
