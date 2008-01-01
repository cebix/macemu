/*
 *	EmulatorView.h - Custom NSView for Basilisk II window input & output
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

#ifdef NSBITMAP
#import <AppKit/NSBitmapImageRep.h>
#endif

#import <AppKit/NSView.h>


@interface EmulatorView : NSView
{
#ifdef CGIMAGEREF
	CGImageRef			cgImgRep;
#endif
#ifdef NSBITMAP
	NSBitmapImageRep	*bitmap;
#else
	void				*bitmap;
#endif
#ifdef CGDRAWBITMAP
	short				bps, spp, bpp;
	int					bytesPerRow;
	BOOL				isPlanar, hasAlpha;
#endif
	float				numBytes;

	short				x, y;

	BOOL				drawView,	// Set when the bitmap is all set up
									// and ready to display
						fullScreen;	// Is this Emulator using the whole screen?

	NSRect				displayBox;	// Cached dimensions of the screen

	int					screen_height; // Height of the screen with the key window
}

- (void) benchmark;
- (NSData *) TIFFrep;				// Used for snapshot function

// Enable display of, and drawing into, the view
#ifdef NSBITMAP
- (void) readyToDraw: (NSBitmapImageRep *) theBitmap
		  imageWidth: (short) width
		 imageHeight: (short) height;
#endif
#ifdef CGIMAGEREF
- (void) readyToDraw: (CGImageRef) image
			  bitmap: (void *) theBitmap
		  imageWidth: (short) width
		 imageHeight: (short) height;
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
			hasAlpha: (BOOL)  alpha;
#endif

- (void) disableDrawing;
- (void) startedFullScreen: (CGDirectDisplayID) theDisplay;

- (void) blacken;
- (void) clear;

- (short) width;
- (short) height;

- (BOOL) isFullScreen;
- (BOOL) mouseInView: (NSEvent *) event;
- (BOOL) mouseInView;
- (void) fullscreenMouseMove;
- (BOOL) processMouseMove: (NSEvent *) event;

#ifdef CGDRAWBITMAP
- (void) CGDrawBitmap;
#endif

#ifdef CGIMAGEREF
void cgDrawInto(NSRect rect, CGImageRef bitmap);
#endif

@end
