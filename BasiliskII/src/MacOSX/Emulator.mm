/*
 *	Emulator.mm - Class whose actions are attached to GUI widgets in a window,
 *				  used to control a single Basilisk II emulated Macintosh. 
 *
 *	$Id$
 *
 *  Basilisk II (C) 1997-2002 Christian Bauer
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

#import "Emulator.h"
#import "EmulatorView.h"

@implementation Emulator

#import "sysdeps.h"			// Types used in Basilisk C++ code

#import "main_macosx.h"		// Prototypes for QuitEmuNoExit() and InitEmulator()
#import "misc_macosx.h"		// Some other prototypes
#import "video_macosx.h"	// Some window/view globals

#import <adb.h>
#import <main.h>
#import <prefs.h>
#import <timer.h>

#undef check()				// memory.h defines a check macro, which clashes with an OS X one?
#import <cpu_emulation.h>

#define DEBUG 1
#import <debug.h>

// NSWindow method, which is invoked via delegation

- (BOOL) windowShouldClose: (id)sender
{
	if ( uaeCreated )
	{
		NSLog(@"windowShouldClose returning NO");
		return NO;	// Should initiate poweroff and return NSTerminateLater ?
	}

	NSLog(@"windowShouldClose returning YES");
	return YES;
}

// Default methods

- (Emulator *) init
{
	int frameSkip;

	self = [super init];

	running = NO;			// Save churn when application loads
//	running = YES;
	uaeCreated = NO;

	frameSkip = PrefsFindInt32("frameskip");
	if ( frameSkip )
		redrawDelay = frameSkip / 60.0;
	else
		redrawDelay = 0.0;

	// We do this so that we can work out if we are in full screen mode:
	parse_screen_prefs(PrefsFindString("screen"));

	[self createThreads];

	return self;
}

- (void) awakeFromNib
{
	the_win = win;					// Set global for access by Basilisk C++ code


	[win setDelegate: self];		// Enable windowShouldClose calling

	// Try to speed up everything
	//[win setHasShadow: NO];		// This causes view & window to now be drawn correctly
	[win useOptimizedDrawing: YES];			

//	[win center];
	[win makeKeyAndOrderFront:self];

//	[self resizeWinToWidth:x Height:y];

	if ( redrawDelay )
		[speed setFloatValue: 1.0 / redrawDelay];
	else
		[speed setFloatValue: 60.0];


	if ( runOrPause == nil )
		NSLog(@"%s - runOrPause button pointer is nil!", __PRETTY_FUNCTION__);

	[self runUpdate];
}


// Helpers which other classes use to access our private stuff

- (BOOL)			isRunning	{	return running;		}
- (BOOL)			uaeCreated	{	return uaeCreated;	}
- (EmulatorView *)	screen		{	return screen;		}
- (NSSlider *)		speed		{	return speed;		}
- (NSWindow *)		window		{	return win;			}

//#define DEBUG 1
//#include <debug.h>

// Update some UI elements

- (void) runUpdate
{
	if ( running )
		[runOrPause setState: NSOnState];	// Running. Change button label to 'Pause'
	else
		[runOrPause setState: NSOffState];	// Paused.  Change button label to 'Run'
	
	[win setDocumentEdited: uaeCreated];	// Set the little dimple in the close button
}


// Methods invoked by buttons & menu items

- (IBAction) Benchmark:	(id)sender;
{
	BOOL	wasRunning = running;

	if ( running )
		[self Suspend: self];
	[screen benchmark];
	if ( wasRunning )
		[self Resume: self];
}

- (IBAction) Interrupt:	(id)sender;
{
	WarningSheet (@"Interrupt action not yet supported", @"", @"OK", win);
}

- (IBAction) PowerKey:	(id)sender;
{
	if ( uaeCreated )		// If Mac has started
	{
		ADBKeyDown(0x7f);	// Send power key, which is also
		ADBKeyUp(0x7f);		// called ADB_RESET or ADB_POWER
	}
	else
	{
		running = YES;						// Start emulator
		[self runUpdate];
		[self Resume: nil];
	}
}

- (IBAction) Restart: (id)sender
{
	if ( running )
//		reset680x0();
	{
		uaeCreated = NO;
		[redraw suspend];
		NSLog (@"%s - uae_cpu reset not yet supported, will try to fake it",
				__PRETTY_FUNCTION__);

//		[screen blacken];
		[screen setNeedsDisplay: YES];

		[emul terminate]; QuitEmuNoExit();

		emul = [[NNThread alloc] init];
		[emul perform:@selector(emulThread) of:self];
		[emul start];

		if ( display_type != DISPLAY_SCREEN )
			[redraw resume];
		uaeCreated = YES;
	}
}

- (IBAction) Resume: (id)sender
{
	[RTC	resume];
	[emul	resume];
	if ( display_type != DISPLAY_SCREEN )
		[redraw	resume];
	[tick	resume];
	[xPRAM	resume];
}

- (IBAction) ScreenHideShow: (NSButton *)sender;
{
	WarningSheet(@"Nigel doesn't know how to shrink or grow this window",
				 @"Maybe you can grab the source code and have a go yourself?",
				 nil, win);
}

- (IBAction) Snapshot: (id) sender
{
	if ( screen == nil || uaeCreated == NO  )
		WarningSheet(@"The emulator has not yet started.",
					 @"There is no screen output to snapshot",
					 @"OK", win);
	else
	{
		NSData	*TIFFdata;

		[self Suspend: self];

		TIFFdata = [screen TIFFrep];
		if ( TIFFdata == nil )
			NSLog(@"%s - Unable to convert Basilisk screen to a TIFF representation",
					__PRETTY_FUNCTION__);
		else
		{
			NSSavePanel *sp = [NSSavePanel savePanel];

			[sp setRequiredFileType:@"tiff"];

			if ( [sp runModalForDirectory:NSHomeDirectory()
									 file:@"B2-screen-snapshot.tiff"] == NSOKButton )
				if ( ! [TIFFdata writeToFile:[sp filename] atomically:YES] )
					NSLog(@"%s - Could not write TIFF data to file @%",
							__PRETTY_FUNCTION__, [sp filename]);

		}
		if ( running )
			[self Resume: self];
	}
}

- (IBAction) SpeedChange: (NSSlider *)sender
{
	float frequency = [sender floatValue];
	
	[redraw suspend];

	if ( frequency == 0.0 )
		redrawDelay = 0.0;
	else
	{
		frequencyToTickDelay(frequency);

		redrawDelay = 1.0 / frequency;

		[redraw changeIntervalTo: (int)(redrawDelay * 1e6)
						   units: NNmicroSeconds];
		if ( running && display_type != DISPLAY_SCREEN )
			[redraw resume];
	}
}

- (IBAction) Suspend: (id)sender
{
	[RTC	suspend];
	[emul	suspend];
	[redraw	suspend];
	[tick	suspend];
	[xPRAM	suspend];
}

- (IBAction) ToggleState: (NSButton *)sender
{
	running = [sender state];		// State of the toggled NSButton
	if ( running )
		[self Resume: nil];
	else
		[self Suspend: nil];
}

- (IBAction) Terminate: (id)sender;
{
	[self exitThreads];
	[win performClose: self];
}

#include <xpram.h>

#define XPRAM_SIZE	256

uint8 lastXPRAM[XPRAM_SIZE];		// Copy of PRAM

- (IBAction) ZapPRAM: (id)sender;
{
	memset(XPRAM,     0, XPRAM_SIZE);
	memset(lastXPRAM, 0, XPRAM_SIZE);
	ZapPRAM();
}

//
// Threads, Timers and stuff to manage them:
//

- (void) createThreads
{
#ifdef USE_PTHREADS
	[NSThread detachNewThreadSelector:(SEL)"" toTarget:nil withObject:nil]; // Make UI threadsafe
	//emul   = [[NNThread	alloc] initWithAutoReleasePool];
#endif
	emul   = [[NNThread	alloc] init];
	RTC    = [[NNTimer	alloc] init];
	redraw = [[NNTimer  alloc] init];
	tick   = [[NNTimer	alloc] init];
	xPRAM  = [[NNTimer	alloc] init];

	[emul  perform:@selector(emulThread)	of:self];
	[RTC    repeat:@selector(RTCinterrupt)	of:self
			 every:1
			 units:NNseconds];
	[redraw	repeat:@selector(redrawScreen)	of:self
			 every:(int)(1000*redrawDelay)
			 units:NNmilliSeconds];
	[tick   repeat:@selector(tickInterrupt)	of:self
			 every:16625
			 units:NNmicroSeconds];
	[xPRAM  repeat:@selector(xPRAMbackup)	of:self
			 every:60
			 units:NNseconds];

	if ( running )		// Start emulator, then threads in most economical order
	{
		[emul	start];
		[xPRAM	start];
		[RTC	start];
		if ( display_type != DISPLAY_SCREEN )
			[redraw	start];
		[tick	start];
	}
}

- (void) exitThreads
{
	running = NO;
	[emul	terminate];  [emul	 release]; emul   = nil;
	[tick	invalidate]; [tick	 release]; tick   = nil;
	[redraw	invalidate]; [redraw release]; redraw = nil;
	[RTC	invalidate]; [RTC	 release]; RTC    = nil;
	[xPRAM	invalidate]; [xPRAM	 release]; xPRAM  = nil;
	if ( uaeCreated )
		QuitEmuNoExit();
}

- (void) emulThread
{
	extern uint8		*RAMBaseHost, *ROMBaseHost;
	NSAutoreleasePool	*pool = [[NSAutoreleasePool alloc] init];

//	[screen allocBitmap];	// Do this first, because InitEmulator() calls VideoInit(), which needs the_bitmap.

	InitEmulator();

	if ( RAMBaseHost == NULL || ROMBaseHost == NULL )
		ErrorSheet(@"Cannot start Emulator.",
				   @"Emulator memory not allocated", @"OK", win);
	else
	{
		memcpy(lastXPRAM, XPRAM, XPRAM_SIZE);

		uaeCreated = YES;		// Enable timers to access emulated Mac's memory

		while ( screen == nil )	// If init sets running, but we are still loading from Nib?
			[NSThread sleepUntilDate:[NSDate dateWithTimeIntervalSinceNow: 1.0]];

//		[screen readyToDraw];
		[self   runUpdate];

		Start680x0();			// Start 68k and jump to ROM boot routine

		puts ("Emulator exited normally");
	}

	running = NO;
	uaeCreated = NO;
	[self runUpdate];			// Update button & dimple
	[pool release];
	[self exitThreads];
}

- (void) RTCinterrupt
{
	if ( uaeCreated )
		WriteMacInt32 (0x20c, TimerDateTime() );	// Update MacOS time
}

- (void) redrawScreen
{
	if ( display_type == DISPLAY_SCREEN )
	{
		NSLog(@"Why was redrawScreen() called?");
		return;
	}
	[barberPole animate:self];			// wobble the pole
	[screen setNeedsDisplay: YES];		// redisplay next time through runLoop
	// Or, use a direct method. e.g.
	//	[screen cgDrawInto: ...];
}

#include <main.h>				// For #define INTFLAG_60HZ
#include <rom_patches.h>		// For ROMVersion
#include "macos_util_macosx.h"	// For HasMacStarted()

- (void) tickInterrupt
{
	if ( ROMVersion != ROM_VERSION_CLASSIC || HasMacStarted() )
	{
		SetInterruptFlag (INTFLAG_60HZ);
		TriggerInterrupt  ();
	}
}

- (void) xPRAMbackup
{
	if ( uaeCreated &&
		memcmp(lastXPRAM, XPRAM, XPRAM_SIZE) )	// if PRAM changed from copy
	{
		memcpy (lastXPRAM, XPRAM, XPRAM_SIZE);	// re-copy
		SaveXPRAM ();							// and save to disk
	}
}

@end
