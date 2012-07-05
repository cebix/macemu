/*
 *	EmulatorView.mm - Custom NSView for Basilisk II windowed graphics output
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

#import "sysdeps.h"			// Types used in Basilisk C++ code,

#define DEBUG 0
#import <debug.h>

#import <Cocoa/Cocoa.h>

#import	"main_macosx.h"		// For WarningAlert() et al prototypes
#import	"misc_macosx.h"		// For InfoSheet() prototype
#import	"video_macosx.h"	// For init_* globals, and bitmap drawing strategy

#import "EmulatorView.h"

@implementation EmulatorView


//
// Standard NSView methods that we override
//

- (id) initWithFrame: (NSRect) frameRect
{
	self = [super initWithFrame: frameRect];

	output = self;		// Set global for access by Basilisk C++ code
//	bitmap = nil;		// Set by readyToDraw:
	drawView = NO;		// Disable drawing until later
	fullScreen = NO;

	return self;
}

- (void) awakeFromNib
{
	// Here we store the height of the screen which the app was opened on.
	// NSApplication's sendEvent: always uses that screen for its mouse co-ords
	screen_height = (int) [[NSScreen mainScreen] frame].size.height;
}


// Mouse click in this window. If window is not active,
// should the click be passed to this view?
- (BOOL) acceptsFirstMouse: (NSEvent *) event
{
	return [self mouseInView];
}


//
// Key event processing.
// OS X doesn't send us separate events for the modifier keys
// (shift/control/command), so we need to monitor them separately
//

#include <adb.h>

static int prevFlags;

- (void) flagsChanged: (NSEvent *) event
{
	int	flags = [event modifierFlags];

	if ( (flags & NSAlphaShiftKeyMask) != (prevFlags & NSAlphaShiftKeyMask) )
		if ( flags & NSAlphaShiftKeyMask )
			ADBKeyDown(0x39);	// CAPS_LOCK
		else
			ADBKeyUp(0x39);

	if ( (flags & NSShiftKeyMask) != (prevFlags & NSShiftKeyMask) )
		if ( flags & NSShiftKeyMask )
			ADBKeyDown(0x38);	// SHIFT_LEFT
		else
			ADBKeyUp(0x38);

	if ( (flags & NSControlKeyMask) != (prevFlags & NSControlKeyMask) )
		if ( flags & NSControlKeyMask )
			ADBKeyDown(0x36);	// CTL_LEFT
		else
			ADBKeyUp(0x36);

	if ( (flags & NSAlternateKeyMask) != (prevFlags & NSAlternateKeyMask) )
		if ( flags & NSAlternateKeyMask )
			ADBKeyDown(0x3a);	// OPTION_LEFT
		else
			ADBKeyUp(0x3a);

	if ( (flags & NSCommandKeyMask) != (prevFlags & NSCommandKeyMask) )
		if ( flags & NSCommandKeyMask )
			ADBKeyDown(0x37);	// APPLE_LEFT
		else
			ADBKeyUp(0x37);

	prevFlags = flags;
}

//
// Windowed mode. We only send mouse/key events
// if the OS X mouse is within the little screen
//
- (BOOL) mouseInView: (NSEvent *) event
{
	NSRect	box;
	NSPoint loc;

	if ( fullScreen )
	{
		box = displayBox;
		loc = [NSEvent mouseLocation];
	}
	else
	{
		box = [self frame];
		loc = [event locationInWindow];
	}

	D(NSLog (@"%s - loc.x=%f, loc.y=%f, box.origin.x=%f, box.origin.y=%f, box.size.width=%f, box.size.height=%f", __PRETTY_FUNCTION__, loc.x, loc.y, box.origin.x, box.origin.y, box.size.width, box.size.height));
	return [self mouse: loc inRect: box];
}

- (BOOL) mouseInView
{
	NSPoint loc = [[self window] mouseLocationOutsideOfEventStream];
	NSRect	box = [self frame];
	D(NSLog (@"%s - loc.x=%f, loc.y=%f, box.origin.x=%f, box.origin.y=%f",
				__PRETTY_FUNCTION__, loc.x, loc.y, box.origin.x, box.origin.y));
	return [self mouse: loc inRect: box];
}

//
// Custom methods
//

- (void) benchmark
{
	int			i;
	float		seconds;
	NSDate		*startDate;
	const char *method;

	if ( ! drawView )
	{
		WarningSheet (@"The emulator has not been setup yet.",
					  @"Try to run, then pause the emulator, first.", nil, [self window]);
		return;
	}

	drawView = NO;
	[self lockFocus];
	startDate = [NSDate date];
	for (i = 1; i < 300; ++i )
#ifdef NSBITMAP
		[bitmap draw];
#endif
#ifdef CGIMAGEREF
		cgDrawInto([self bounds], cgImgRep);
#endif
#ifdef CGDRAWBITMAP
		[self CGDrawBitmap];
#endif
	seconds = -[startDate timeIntervalSinceNow];
	[self unlockFocus];
	drawView = YES;

#ifdef NSBITMAP
	method = "NSBITMAP";
#endif
#ifdef CGIMAGEREF
	method = "CGIMAGEREF";
#endif
#ifdef CGDRAWBITMAP
	method = "CGDRAWBITMAP";
#endif

	InfoSheet(@"Ran benchmark (300 screen redraws)",
			  [NSString stringWithFormat:
				@"%.2f seconds, %.3f frames per second (using %s implementation)",
				seconds, i/seconds, method],
			  @"Thanks", [self window]);
}

// Return a TIFF for a snapshot of the screen image
- (NSData *) TIFFrep
{
#ifdef NSBITMAP
	return [bitmap TIFFRepresentation];
#else
	NSBitmapImageRep	*b = [NSBitmapImageRep alloc];

	b = [b initWithBitmapDataPlanes: (unsigned char **) &bitmap
						 pixelsWide: x
						 pixelsHigh: y
  #ifdef CGIMAGEREF
					  bitsPerSample: CGImageGetBitsPerComponent(cgImgRep)
					samplesPerPixel: 3
						   hasAlpha: NO
						   isPlanar: NO
					 colorSpaceName: NSCalibratedRGBColorSpace
						bytesPerRow: CGImageGetBytesPerRow(cgImgRep)
					   bitsPerPixel: CGImageGetBitsPerPixel(cgImgRep)];
  #endif
  #ifdef CGDRAWBITMAP
					  bitsPerSample: bps
					samplesPerPixel: spp
						   hasAlpha: hasAlpha
						   isPlanar: isPlanar
					 colorSpaceName: NSCalibratedRGBColorSpace
						bytesPerRow: bytesPerRow
					   bitsPerPixel: bpp];
  #endif

    if ( ! b )
	{
		ErrorAlert("Could not allocate an NSBitmapImageRep for the TIFF\nTry setting the emulation to millions of colours?");
		return nil;
	}

	return [b TIFFRepresentation];
#endif
}

// Enable display of, and drawing into, the view
#ifdef NSBITMAP
- (void) readyToDraw: (NSBitmapImageRep *) theBitmap
		  imageWidth: (short) width
		 imageHeight: (short) height
{
	numBytes = [theBitmap bytesPerRow] * height;
#endif
#ifdef CGIMAGEREF
- (void) readyToDraw: (CGImageRef) image
			  bitmap: (void *) theBitmap
		  imageWidth: (short) width
		 imageHeight: (short) height
{
	cgImgRep = image;
	numBytes = CGImageGetBytesPerRow(image) * height;
#endif
#ifdef CGDRAWBITMAP
- (void) readyToDraw: (void *) theBitmap
			   width: (short) width
			  height: (short) height
				 bps: (short) bitsPerSample
				 spp: (short) samplesPerPixel
				 bpp: (short) bitsPerPixel
				 bpr: (int)   bpr
			isPlanar: (BOOL)  planar
			hasAlpha: (BOOL)  alpha
{
	bps = bitsPerSample;
	spp = samplesPerPixel;
	bpp = bitsPerPixel;
	bytesPerRow = bpr;
	isPlanar = planar;
	hasAlpha = alpha;
	numBytes = bpr * height;
#endif
	D(NSLog(@"readyToDraw: theBitmap=%lx\n", theBitmap));

	bitmap = theBitmap;
	x = width, y = height;
	drawView = YES;
	[[self window] setAcceptsMouseMovedEvents:	YES];
//	[[self window] setInitialFirstResponder:	self];
	[[self window] makeFirstResponder:			self];
}

- (void) disableDrawing
{
	drawView = NO;
}

- (void) startedFullScreen: (CGDirectDisplayID) display
{
	CGRect	displayBounds = CGDisplayBounds(display);

	fullScreen = YES;
	memcpy(&displayBox, &displayBounds, sizeof(displayBox));
}

- (short) width
{
	return (short)[self bounds].size.width;
}

- (short) height
{
	return (short)[self bounds].size.height;
}

- (BOOL) isFullScreen
{
	return fullScreen;
}

- (BOOL) isOpaque
{
	return drawView;
}

- (BOOL) processKeyEvent: (NSEvent *) event
{
	if ( fullScreen || [self acceptsFirstMouse: event] )
		if ( [event isARepeat] )
			return NO;
		else
			return YES;

	[self interpretKeyEvents:[NSArray arrayWithObject:event]];
	return NO;
}

- (void) keyDown: (NSEvent *) event
{
	if ( [self processKeyEvent: event] )
	{
		int	code = [event keyCode];

		if ( code == 126 )	code = 0x3e;	// CURS_UP
		if ( code == 125 )	code = 0x3d;	// CURS_DOWN
		if ( code == 124 )	code = 0x3c;	// CURS_RIGHT
		if ( code == 123 )	code = 0x3b;	// CURS_LEFT

		ADBKeyDown(code);
	}
}

- (void) keyUp: (NSEvent *) event
{
	if ( [self processKeyEvent: event] )
	{
		int	code = [event keyCode];

		if ( code == 126 )	code = 0x3e;	// CURS_UP
		if ( code == 125 )	code = 0x3d;	// CURS_DOWN
		if ( code == 124 )	code = 0x3c;	// CURS_RIGHT
		if ( code == 123 )	code = 0x3b;	// CURS_LEFT

		ADBKeyUp(code);
	}
}


- (void) fullscreenMouseMove
{
	NSPoint location = [NSEvent mouseLocation];

	D(NSLog (@"%s - loc.x=%f, loc.y=%f",
				__PRETTY_FUNCTION__, location.x, location.y));
	D(NSLog (@"%s - Sending ADBMouseMoved(%d,%d). (%d-%d)",
					__PRETTY_FUNCTION__, (int)location.x,
					screen_height - (int)location.y, screen_height, (int)location.y));
	ADBMouseMoved((int)location.x, screen_height - (int)location.y);
}

static NSPoint	mouse;			// Previous/current mouse location

- (BOOL) processMouseMove: (NSEvent *) event
{
	if ( ! drawView )
	{
		D(NSLog(@"Unable to process event - Emulator has not started yet"));
		return NO;
	}

	if ( fullScreen )
	{
		[self fullscreenMouseMove];
		return YES;
	}

	NSPoint location = [self convertPoint: [event locationInWindow] fromView:nil];

	D(NSLog (@"%s - loc.x=%f, loc.y=%f",
				__PRETTY_FUNCTION__, location.x, location.y));

	if ( NSEqualPoints(location, mouse) )
		return NO;

	mouse = location;

	int	mouseY = y - (int) (y * mouse.y / [self height]);
	int	mouseX =	 (int) (x * mouse.x / [self width]);
	// If the view was not resizable, then this would be simpler:
	// int	mouseY = y - (int) mouse.y;
	// int	mouseX =	 (int) mouse.x;

	ADBMouseMoved(mouseX, mouseY);
	return YES;
}

- (void) mouseDown: (NSEvent *) event
{
	[self processMouseMove: event];
	ADBMouseDown(0);
}

- (void) mouseDragged: (NSEvent *) event
{
	[self processMouseMove: event];
}

- (void) mouseMoved: (NSEvent *) event
{
#if DEBUG
	if ( ! [self mouseInView] )
	{
		NSLog (@"%s - Received event while outside of view", __PRETTY_FUNCTION__);
		return;
	}
#endif
	[self processMouseMove: event];
}

- (void) mouseUp: (NSEvent *) event
{
	[self processMouseMove: event];
	ADBMouseUp(0);
}

#if DEBUG
- (void) randomise		// Draw some coloured snow in the bitmap
{
	unsigned char	*data,
					*pixel;

  #ifdef NSBITMAP
	data = [bitmap bitmapData];
  #else
	data = bitmap;
  #endif

	for ( int i = 0; i < 1000; ++i )
	{
		pixel  = data + (int) (numBytes * rand() / RAND_MAX);
		*pixel = (unsigned char) (256.0 * rand() / RAND_MAX);
	}
}
#endif

- (void) drawRect: (NSRect) rect
{
	if ( ! drawView )		// If the emulator is still being setup,
		return;				// we do not want to draw

#if DEBUG
	NSLog(@"In drawRect");
	[self randomise];
#endif

#ifdef NSBITMAP
	NSRectClip(rect);
	[bitmap draw];
#endif
#ifdef CGIMAGEREF
	cgDrawInto(rect, cgImgRep);
#endif
#ifdef CGDRAWBITMAP
	[self CGDrawBitmap];
#endif
}

- (void) setTo: (int) val		// Set all of bitmap to val
{
	unsigned char	*data
  #ifdef NSBITMAP
							= [bitmap bitmapData];
  #else
							= (unsigned char *) bitmap;
  #endif

	memset(data, val, (long unsigned)numBytes);
}

- (void) blacken	// Set bitmap black
{
	[self setTo: 0];
}

- (void) clear		// Set bitmap white
{
	[self setTo: 0xFF];
}

//
// Extra drawing stuff
//

#ifdef CGDRAWBITMAP
extern "C" void CGDrawBitmap(...);

- (void) CGDrawBitmap
{
	CGContextRef	cgContext = (CGContextRef) [[NSGraphicsContext currentContext]
												graphicsPort];
	NSRect			rect = [self bounds];
	CGRect			cgRect = {
								{rect.origin.x, rect.origin.y},
								{rect.size.width, rect.size.height}
							 };

	CGColorSpaceRef	colourSpace = CGColorSpaceCreateDeviceRGB();


//	CGContextSetShouldAntialias(cgContext, NO);		// Seems to have no effect?

	CGDrawBitmap(cgContext, cgRect, x, y, bps, spp, bpp,
					bytesPerRow, isPlanar, hasAlpha, colourSpace, &bitmap);
}
#endif

#ifdef CGIMAGEREF
void
cgDrawInto(NSRect rect, CGImageRef cgImgRep)
{
	CGContextRef	cgContext = (CGContextRef) [[NSGraphicsContext currentContext]
												graphicsPort];
	CGRect			cgRect = {
								{rect.origin.x, rect.origin.y},
								{rect.size.width, rect.size.height}
							 };

//	CGContextSetShouldAntialias(cgContext, NO);		// Seems to have no effect?

	CGContextDrawImage(cgContext, cgRect, cgImgRep);
}
#endif

@end
