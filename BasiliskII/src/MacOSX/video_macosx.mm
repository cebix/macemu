/*
 *  $Id$
 *
 *  video_macosx.mm - Interface between Basilisk II and Cocoa windowing.
 *                    Based on video_amiga.cpp and video_x.cpp
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


#include "sysdeps.h"

#ifdef HAVE_PTHREADS
# include <pthread.h>
#endif

#include <adb.h>
#include <cpu_emulation.h>
#include <main.h>
#include "macos_util_macosx.h"
#include <prefs.h>
#include <user_strings.h>
#include "video_macosx.h"

#define DEBUG 0
#define VERBOSE 0
#include "debug.h"

#ifdef NSBITMAP
#import <AppKit/NSBitmapImageRep.h>
#endif

#import <Foundation/NSString.h>				// Needed for NSLog(@"")
#import "misc_macosx.h"						// WarningSheet() prototype



// Global variables
uint8		display_type = DISPLAY_WINDOW,	// These are used by PrefsEditor
			frame_skip;	
uint16		init_width  = MIN_WIDTH,		// as well as this code
			init_height = MIN_HEIGHT,
			init_depth  = 32;

		EmulatorView	*output = nil;		// Set by [EmulatorView init]
		NSWindow		*the_win = nil;		// Set by [Emulator awakeFromNib]

static	BOOL			singleDisplay = YES;

/*
 *  Utility functions
 */

static uint8
bits_from_depth(const video_depth depth)
{
	int bits = 1 << depth;
//	if (bits == 16)
//		bits = 15;
//	else if (bits == 32)
//		bits = 24;
	return bits;
}

static const char *
colours_from_depth(const video_depth depth)
{
	switch ( depth )
	{
		case VDEPTH_1BIT : return "Monochrome";
		case VDEPTH_2BIT : return "4 colours";
		case VDEPTH_4BIT : return "16 colours";
		case VDEPTH_8BIT : return "256 colours";
		case VDEPTH_16BIT: return "Thousands of colours";
		case VDEPTH_32BIT: return "Millions of colours";
	}

	return "illegal colour depth";
}

static const char *
colours_from_depth(const uint16 depth)
{
	return colours_from_depth(DepthModeForPixelDepth(depth) );
}

bool
parse_screen_prefs(const char *mode_str)
{
	if ( ! mode_str )
	{
		// No screen pref was found. Supply a default:
		mode_str = "win/512/384";
	}

	if (sscanf(mode_str, "win/%hd/%hd/%hd",
				&init_width, &init_height, &init_depth) == 3)
		display_type = DISPLAY_WINDOW;
	else if (sscanf(mode_str, "win/%hd/%hd", &init_width, &init_height) == 2)
		display_type  = DISPLAY_WINDOW;
	else if (strcmp(mode_str, "full") == 0)
		display_type = DISPLAY_SCREEN;
	else if (sscanf(mode_str, "full/%hd/%hd/%hd",
					&init_width, &init_height, &init_depth) == 3)
		display_type = DISPLAY_SCREEN;
	else if (sscanf(mode_str, "full/%hd/%hd", &init_width, &init_height) == 2)
		display_type = DISPLAY_SCREEN;
	else if (sscanf(mode_str, "opengl/%hd/%hd/%hd",
					&init_width, &init_height, &init_depth) == 3)
		display_type = DISPLAY_OPENGL;
	else if (sscanf(mode_str, "opengl/%hd/%hd", &init_width, &init_height) == 2)
		display_type = DISPLAY_OPENGL;
	else return false;

	return true;
}

// Supported video modes
static vector<video_mode> VideoModes;


// Add mode to list of supported modes
static void
add_mode(const uint16 width, const uint16 height,
		 const uint32 resolution_id, const uint32 bytes_per_row,
		 const uint32 user_data,
		 const video_depth depth)
{
	vector<video_mode>::const_iterator	i,
										end = VideoModes.end();

	for (i = VideoModes.begin(); i != end; ++i)
		if ( i->x == width && i->y == height &&
			 i->bytes_per_row == bytes_per_row && i->depth == depth )
		{
			D(NSLog(@"Duplicate mode (%hdx%hdx%ld, ID %02x, new ID %02x)\n",
						width, height, depth, i->resolution_id, resolution_id));
			return;
		}

	video_mode mode;

	mode.x = width;
	mode.y = height;
	mode.resolution_id = resolution_id;
	mode.bytes_per_row = bytes_per_row;
	mode.user_data = user_data;
	mode.depth = depth;

	D(bug("Added video mode: w=%d  h=%d  d=%d(%d bits)\n",
				width, height, depth, bits_from_depth(depth) ));

	VideoModes.push_back(mode);
}

// Add standard list of windowed modes for given color depth
static void add_standard_modes(const video_depth depth)
{
	D(bug("add_standard_modes: depth=%d(%d bits)\n",
						depth, bits_from_depth(depth) ));

	add_mode(512,  384,  0x80, TrivialBytesPerRow(512,  depth), 0, depth);
	add_mode(640,  480,  0x81, TrivialBytesPerRow(640,  depth), 0, depth);
	add_mode(800,  600,  0x82, TrivialBytesPerRow(800,  depth), 0, depth);
	add_mode(832,  624,  0x83, TrivialBytesPerRow(832,  depth), 0, depth);
	add_mode(1024, 768,  0x84, TrivialBytesPerRow(1024, depth), 0, depth);
	add_mode(1152, 768,  0x85, TrivialBytesPerRow(1152, depth), 0, depth);
	add_mode(1152, 870,  0x86, TrivialBytesPerRow(1152, depth), 0, depth);
	add_mode(1280, 1024, 0x87, TrivialBytesPerRow(1280, depth), 0, depth);
	add_mode(1600, 1200, 0x88, TrivialBytesPerRow(1600, depth), 0, depth);
}

// Helper function to get a 32bit int from a dictionary
static int32 getCFint32 (CFDictionaryRef dict, CFStringRef key)
{
	CFNumberRef	ref = (CFNumberRef) CFDictionaryGetValue(dict, key);

	if ( ref )
	{
		int32	val;

		if ( CFNumberGetValue(ref, kCFNumberSInt32Type, &val) )
			return val;
		else
			NSLog(@"getCFint32() - Failed to get the value %@", key);
	}
	else
		NSLog(@"getCFint32() - Failed to get a 32bit int for %@", key);

	return 0;
}

// Nasty hack. Under 10.1, CGDisplayAvailableModes() does not provide bytes per row,
// and the emulator doesn't like setting the bytes per row after the screen,
// so we use a lot of magic numbers here.
// This will probably fail on some video hardware.
// I have tested on my G4 PowerBook 400 and G3 PowerBook Series 292

static int
CGBytesPerRow(const uint16 width, const video_depth depth)
{
	if ( depth == VDEPTH_8BIT )
		switch ( width )
		{
			case 640:
			case 720:	return 768;
			case 800:
			case 896:	return 1024;
			case 1152:	return 1280;
		}

	if ( width == 720  && depth == VDEPTH_16BIT)	return 1536;
	if ( width == 720  && depth == VDEPTH_32BIT)	return 3072;
	if ( width == 800  && depth == VDEPTH_16BIT)	return 1792;
	if ( width == 800  && depth == VDEPTH_32BIT)	return 3328;

	return TrivialBytesPerRow(width, depth);
}

static bool add_CGDirectDisplay_modes()
{
#define kMaxDisplays 8
	CGDirectDisplayID	displays[kMaxDisplays];
	CGDisplayErr		err;
	CGDisplayCount		n;
	int32				oldRes = 0,
						res_id = 0x80;


	err = CGGetActiveDisplayList(kMaxDisplays, displays, &n);
	if ( err != CGDisplayNoErr )
		n = 1, displays[n] = kCGDirectMainDisplay;

	if ( n > 1 )
		singleDisplay = NO;

	for ( CGDisplayCount dc = 0; dc < n; ++dc )
	{
		CGDirectDisplayID	d = displays[dc];
		CFArrayRef			m = CGDisplayAvailableModes(d);

		if ( ! m )					// Store the current display mode
			add_mode(CGDisplayPixelsWide(d),
					 CGDisplayPixelsHigh(d),
					 res_id++, CGDisplayBytesPerRow(d),
					 (const uint32) d,
					 DepthModeForPixelDepth(CGDisplayBitsPerPixel(d)));
		else
		{
			CFIndex  nModes = CFArrayGetCount(m);

			for ( CFIndex mc = 0; mc < nModes; ++mc )
			{
				CFDictionaryRef modeSpec = (CFDictionaryRef)
											CFArrayGetValueAtIndex(m, mc);

				int32	bpp    = getCFint32(modeSpec, kCGDisplayBitsPerPixel);
				int32	height = getCFint32(modeSpec, kCGDisplayHeight);
				int32	width  = getCFint32(modeSpec, kCGDisplayWidth);
#ifdef MAC_OS_X_VERSION_10_2
				int32	bytes  = getCFint32(modeSpec, kCGDisplayBytesPerRow);
#else
				int32	bytes  = 0;
#endif
				video_depth	depth = DepthModeForPixelDepth(bpp);

				if ( ! bpp || ! height || ! width )
				{
					NSLog(@"Could not get details of mode %d, display %d",
							mc, dc);
					return false;
				}
#if VERBOSE
				else
					NSLog(@"Display %ld, spec = %@", d, modeSpec);
#endif

				if ( ! bytes )
				{
					NSLog(@"Could not get bytes per row, guessing");
					bytes = CGBytesPerRow(width, depth);
				}

				if ( ! oldRes )
					oldRes = width * height;
				else
					if ( oldRes != width * height )
					{
						oldRes = width * height;
						++res_id;
					}

				add_mode(width, height, res_id, bytes, (const uint32) d, depth);
			}
		}
	}

	return true;
}

#ifdef CG_USE_ALPHA
// memset() by long instead of byte

static void memsetl (long *buffer, long	pattern, size_t length)
{
	long	*buf = (long *) buffer,
			*end = buf + length/4;

	while ( ++buf < end )
		*buf = pattern;
}

// Sets the alpha channel in a image to full on, except for the corners

static void mask_buffer (void *buffer, size_t width, size_t size)
{
	long	*bufl = (long *) buffer;
	char	*bufc = (char *) buffer;


	memsetl(bufl, 0xFF000000, size);


	// Round upper-left corner
				   *bufl = 0, *bufc+4 = 0;					// XXXXX
	bufc += width, *bufc++ = 0, *bufc++ = 0, *bufc++ = 0;	// XXX
	bufc += width, *bufc++ = 0, *bufc = 0;					// XX
	bufc += width, *bufc = 0;								// X
	bufc += width, *bufc = 0;								// X


	NSLog(@"Masked buffer");
}
#endif

// monitor_desc subclass for Mac OS X displays

class OSX_monitor : public monitor_desc
{
	public:
		OSX_monitor(const vector<video_mode>	&available_modes,
					video_depth					default_depth,
					uint32						default_id);

		virtual void set_palette(uint8 *pal, int num);
		virtual void switch_to_current_mode(void);

				void set_mac_frame_buffer(const video_mode mode);

				void video_close(void);
				bool video_open (const video_mode &mode);


	private:
		bool init_opengl(const video_mode &mode);
		bool init_screen(      video_mode &mode);
		bool init_window(const video_mode &mode);


#ifdef CGIMAGEREF
		CGColorSpaceRef		colourSpace;
		uint8 				*colourTable;
		CGImageRef			imageRef;
		CGDataProviderRef	provider;
		short				x, y, bpp, depth, bpr;
#endif
#ifdef NSBITMAP
		NSBitmapImageRep	*bitmap;
#endif
		void				*the_buffer;


		// These record changes we made in setting full screen mode,
		// so that we can set the display back as it was again.
		CGDirectDisplayID	theDisplay;
		CFDictionaryRef		originalMode,
							newMode;
};


OSX_monitor :: OSX_monitor (const	vector<video_mode>	&available_modes,
									video_depth			default_depth,
									uint32				default_id)
			: monitor_desc (available_modes, default_depth, default_id)
{
#ifdef CGIMAGEREF
	colourSpace = nil;
	colourTable = (uint8 *) malloc(256 * 3);
	imageRef = nil;
	provider = nil;
#endif
#ifdef NSBITMAP
	bitmap = nil;
#endif
	newMode = originalMode = nil;
	the_buffer = NULL;
	theDisplay = nil;
};

// Should also have a destructor which does
//#ifdef CGIMAGEREF
//	free(colourTable);
//#endif


// Set Mac frame layout and base address (uses the_buffer/MacFrameBaseMac)
void
OSX_monitor::set_mac_frame_buffer(const video_mode mode)
{
#if !REAL_ADDRESSING && !DIRECT_ADDRESSING
	switch ( mode.depth )
	{
	//	case VDEPTH_15BIT:
		case VDEPTH_16BIT: MacFrameLayout = FLAYOUT_HOST_555; break;
	//	case VDEPTH_24BIT:
		case VDEPTH_32BIT: MacFrameLayout = FLAYOUT_HOST_888; break;
		default			 : MacFrameLayout = FLAYOUT_DIRECT;
	}
	set_mac_frame_base(MacFrameBaseMac);

	// Set variables used by UAE memory banking
	MacFrameBaseHost = (uint8 *) the_buffer;
	MacFrameSize = mode.bytes_per_row * mode.y;
	InitFrameBufferMapping();
#else
	set_mac_frame_base((unsigned int)Host2MacAddr((uint8 *)the_buffer));
#endif
	D(bug("mac_frame_base = %08x\n", get_mac_frame_base()));
}

static void
resizeWinBy(const short deltaX, const short deltaY)
{
	NSRect	rect = [the_win frame];

	D(bug("resizeWinBy(%d,%d) - ", deltaX, deltaY));
	D(bug("old x=%g, y=%g", rect.size.width, rect.size.height));

	rect.size.width  += deltaX;
	rect.size.height += deltaY;

	D(bug(", new x=%g, y=%g\n", rect.size.width, rect.size.height));

	[the_win setFrame: rect display: YES animate: YES];
	[the_win center];
	rect = [the_win frame];
}

void resizeWinTo(const uint16 newWidth, const uint16 newHeight)
{
	int		deltaX = newWidth  - [output width],
			deltaY = newHeight - [output height];

	D(bug("resizeWinTo(%d,%d)\n", newWidth, newHeight));

	if ( deltaX || deltaY )
		resizeWinBy(deltaX, deltaY);
}

// Open window
bool
OSX_monitor::init_window(const video_mode &mode)
{
	D(bug("init_window: depth=%d(%d bits)\n",
			mode.depth, bits_from_depth(mode.depth) ));


	// Set absolute mouse mode
	ADBSetRelMouseMode(false);


	// Is the window open?
	if ( ! the_win )
	{
		ErrorAlert(STR_OPEN_WINDOW_ERR);
		return false;
	}
	resizeWinTo(mode.x, mode.y);


	// Create frame buffer ("height + 2" for safety)
	int the_buffer_size = mode.bytes_per_row * (mode.y + 2);

	the_buffer = calloc(the_buffer_size, 1);
	if ( ! the_buffer )
	{
		NSLog(@"calloc(%d) failed", the_buffer_size);
		ErrorAlert(STR_NO_MEM_ERR);
		return false;
	}
	D(bug("the_buffer = %p\n", the_buffer));


	unsigned char *offsetBuffer = (unsigned char *) the_buffer;
	offsetBuffer += 1;		// OS X NSBitmaps are RGBA, but Basilisk generates ARGB

#ifdef CGIMAGEREF
	switch ( mode.depth )
	{
		case VDEPTH_1BIT:	bpp = 1; break;
		case VDEPTH_2BIT:	bpp = 2; break;
		case VDEPTH_4BIT:	bpp = 4; break;
		case VDEPTH_8BIT:	bpp = 8; break;
		case VDEPTH_16BIT:	bpp = 5; break;
		case VDEPTH_32BIT:	bpp = 8; break;
	}

	x = mode.x, y = mode.y, depth = bits_from_depth(mode.depth), bpr = mode.bytes_per_row;

	colourSpace = CGColorSpaceCreateDeviceRGB();

	if ( mode.depth < VDEPTH_16BIT )
	{
		CGColorSpaceRef	oldColourSpace = colourSpace;

		colourSpace = CGColorSpaceCreateIndexed(colourSpace, 255, colourTable);

		CGColorSpaceRelease(oldColourSpace);
	}

	if ( ! colourSpace )
	{
		ErrorAlert("No valid colour space");
		return false;
	}

	provider = CGDataProviderCreateWithData(NULL, the_buffer,
											the_buffer_size, NULL);
	if ( ! provider )
	{
		ErrorAlert("Could not create CGDataProvider from buffer data");
		return false;
	}

	imageRef = CGImageCreate(x, y, bpp, depth, bpr, colourSpace,
  #ifdef CG_USE_ALPHA
							 kCGImageAlphaPremultipliedFirst,
  #else
							 kCGImageAlphaNoneSkipFirst,
  #endif
							 provider,
							 NULL, 	// colourMap translation table
							 NO,	// shouldInterpolate colours?
							 kCGRenderingIntentDefault);
	if ( ! imageRef )
	{
		ErrorAlert("Could not create CGImage from CGDataProvider");
		return false;
	}

	[output readyToDraw: imageRef
				 bitmap: offsetBuffer
			 imageWidth: x
			imageHeight: y];


  #ifdef CG_USE_ALPHA
	mask_buffer(the_buffer, x, the_buffer_size);

/* Create an image mask with this call? */
//CG_EXTERN CGImageRef
//CGImageMaskCreate(size_t width, size_t height, size_t bitsPerComponent,
//					size_t bitsPerPixel, size_t bytesPerRow,
//					CGDataProviderRef provider, const float decode[], bool shouldInterpolate);
  #endif

	return true;
#endif


#ifndef CGIMAGEREF
	short	bitsPer, samplesPer;	// How big is each Pixel?

	if ( mode.depth == VDEPTH_1BIT )
		bitsPer = 1;
	else
		bitsPer = 8;

	if ( mode.depth == VDEPTH_32BIT )
		samplesPer = 3;
	else
		samplesPer = 1;
#endif


#ifdef NSBITMAP
	bitmap = [NSBitmapImageRep alloc];
	bitmap = [bitmap initWithBitmapDataPlanes: (unsigned char **) &offsetBuffer
								   pixelsWide: mode.x
								   pixelsHigh: mode.y
								bitsPerSample: bitsPer
                              samplesPerPixel: samplesPer
                                     hasAlpha: NO
									 isPlanar: NO
							   colorSpaceName: NSCalibratedRGBColorSpace
								  bytesPerRow: mode.bytes_per_row
								 bitsPerPixel: bits_from_depth(mode.depth)];

    if ( ! bitmap )
	{
		ErrorAlert("Could not allocate an NSBitmapImageRep");
		return false;
	}

	[output readyToDraw: bitmap
			 imageWidth: mode.x
			imageHeight: mode.y];
#endif

#ifdef CGDRAWBITMAP
	[output readyToDraw: offsetBuffer
				  width: mode.x
				 height: mode.y
					bps: bitsPer
					spp: samplesPer
					bpp: bits_from_depth(mode.depth)
					bpr: mode.bytes_per_row
			   isPlanar: NO
			   hasAlpha: NO];
#endif

	return true;
}

#import <AppKit/NSEvent.h>
#import <Carbon/Carbon.h>
#import "NNThread.h"

bool
OSX_monitor::init_screen(video_mode &mode)
{
	// Set absolute mouse mode
	ADBSetRelMouseMode(false);

	// Display stored by add_CGDirectDisplay_modes()
	theDisplay = (CGDirectDisplayID) mode.user_data;

	originalMode = CGDisplayCurrentMode(theDisplay);
	if ( ! originalMode )
	{
		ErrorSheet(@"Could not get current mode of display", the_win);
		return false;
	}

	D(NSLog(@"About to call CGDisplayBestModeForParameters()"));
	newMode = CGDisplayBestModeForParameters(theDisplay,
											 bits_from_depth(mode.depth),
													mode.x, mode.y, NULL);
	if ( ! newMode )
	{
		ErrorSheet(@"Could not find a matching screen mode", the_win);
		return false;
	}

//	This sometimes takes ages to return after the window is genied,
//	so for now we leave it onscreen
//	[the_win miniaturize: nil];

	D(NSLog(@"About to call CGDisplayCapture()"));
	if ( CGDisplayCapture(theDisplay) != CGDisplayNoErr )
	{
//		[the_win deminiaturize: nil];
		ErrorSheet(@"Could not capture display", the_win);
		return false;
	}

	D(NSLog(@"About to call CGDisplaySwitchToMode()"));
	if ( CGDisplaySwitchToMode(theDisplay, newMode) != CGDisplayNoErr )
	{
		CGDisplayRelease(theDisplay);
//		[the_win deminiaturize: nil];
		ErrorSheet(@"Could not switch to matching screen mode", the_win);
		return false;
	}

	the_buffer = CGDisplayBaseAddress(theDisplay);
	if ( ! the_buffer )
	{
		CGDisplaySwitchToMode(theDisplay, originalMode);
		CGDisplayRelease(theDisplay);
//		[the_win deminiaturize: nil];
		ErrorSheet(@"Could not get base address of screen", the_win);
		return false;
	}

	if ( mode.bytes_per_row != CGDisplayBytesPerRow(theDisplay) )
	{
		D(bug("Bytes per row (%d) doesn't match current (%ld)\n",
				mode.bytes_per_row, CGDisplayBytesPerRow(theDisplay)));
		mode.bytes_per_row = CGDisplayBytesPerRow(theDisplay);
	}

	[NSMenu setMenuBarVisible:NO];

	if ( singleDisplay )
	{
		CGDisplayHideCursor(theDisplay);

		[output startedFullScreen: theDisplay];

		// Send emulated mouse to current location
		[output fullscreenMouseMove];
	}
	else
	{
		// Should set up something to hide the cursor when it enters theDisplay?
	}

	return true;
}


bool
OSX_monitor::init_opengl(const video_mode &mode)
{
	ErrorAlert("Sorry. OpenGL mode is not implemented yet");
	return false;
}

/*
 *  Initialization
 */
static bool
monitor_init(const video_mode &init_mode)
{
	OSX_monitor	*monitor;
	BOOL		success;

	monitor = new OSX_monitor(VideoModes, init_mode.depth,
										  init_mode.resolution_id);
	success = monitor->video_open(init_mode);

	if ( success )
	{
		monitor->set_mac_frame_buffer(init_mode);
		VideoMonitors.push_back(monitor);
		return YES;
	}

	return NO;
}

bool VideoInit(bool classic)
{
	// Read frame skip prefs
	frame_skip = PrefsFindInt32("frameskip");
	if (frame_skip == 0)
		frame_skip = 1;

	// Get screen mode from preferences
	const char *mode_str;
	if (classic)
		mode_str = "win/512/342";
	else
		mode_str = PrefsFindString("screen");

	// Determine display_type and init_width, height & depth
	parse_screen_prefs(mode_str);

	// Construct list of supported modes
	if (classic)
		add_mode(512, 342, 0x80, 64, 0, VDEPTH_1BIT);
	else
		switch ( display_type )
		{
			case DISPLAY_SCREEN:
				if ( ! add_CGDirectDisplay_modes() )
				{
					ErrorAlert("Unable to get list of displays for full screen mode");
					return false;
				}
				break;
			case DISPLAY_OPENGL:
				// Same as window depths and sizes?
			case DISPLAY_WINDOW:
#ifdef CGIMAGEREF
				add_standard_modes(VDEPTH_1BIT);
				add_standard_modes(VDEPTH_2BIT);
				add_standard_modes(VDEPTH_4BIT);
				add_standard_modes(VDEPTH_8BIT);
				add_standard_modes(VDEPTH_16BIT);
#endif
				add_standard_modes(VDEPTH_32BIT);
				break;
		}

//	video_init_depth_list();		Now done in monitor_desc constructor?

#if DEBUG
	bug("Available video modes:\n");
	vector<video_mode>::const_iterator i, end = VideoModes.end();
	for (i = VideoModes.begin(); i != end; ++i)
		bug(" %dx%d (ID %02x), %s\n", i->x, i->y, i->resolution_id,
										colours_from_depth(i->depth));
#endif

	D(bug("VideoInit: width=%hd height=%hd depth=%d\n",
						init_width, init_height, init_depth));

	// Find requested default mode and open display
	if (VideoModes.size() > 0)
	{
		// Find mode with specified dimensions
		std::vector<video_mode>::const_iterator i, end = VideoModes.end();
		for (i = VideoModes.begin(); i != end; ++i)
		{
			D(bug("VideoInit: w=%d h=%d d=%d\n",
					i->x, i->y, bits_from_depth(i->depth)));
			if (i->x == init_width && i->y == init_height
					&& bits_from_depth(i->depth) == init_depth)
				return monitor_init(*i);
		}
	}

	char str[150];
	sprintf(str, "Cannot open selected video mode\r(%hd x %hd, %s).\r%s",
			init_width, init_height,
			colours_from_depth(init_depth), "Using lowest resolution");
	WarningAlert(str);

	return monitor_init(VideoModes[0]);
}


// Open display for specified mode
bool
OSX_monitor::video_open(const video_mode &mode)
{
	D(bug("video_open: width=%d  height=%d  depth=%d  bytes_per_row=%d\n",
			mode.x, mode.y, bits_from_depth(mode.depth), mode.bytes_per_row));

	// Open display
	switch ( display_type )
	{
		case DISPLAY_WINDOW:	return init_window(mode);
		case DISPLAY_SCREEN:	return init_screen((video_mode &)mode);
		case DISPLAY_OPENGL:	return init_opengl(mode);
	}

	return false;
}


void
OSX_monitor::video_close()
{
	D(bug("video_close()\n"));

	switch ( display_type ) {
		case DISPLAY_WINDOW:
			// Stop redraw thread
			[output disableDrawing];

			// Free frame buffer stuff
#ifdef CGIMAGEREF
			CGImageRelease(imageRef);
			CGColorSpaceRelease(colourSpace);
			CGDataProviderRelease(provider);
#endif
#ifdef NSBITMAP
			[bitmap release];
#endif
			free(the_buffer);

			break;

		case DISPLAY_SCREEN:
			if ( theDisplay && originalMode )
			{
				if ( singleDisplay )
					CGDisplayShowCursor(theDisplay);
				[NSMenu setMenuBarVisible:YES];
				CGDisplaySwitchToMode(theDisplay, originalMode);
				CGDisplayRelease(theDisplay);
				//[the_win deminiaturize: nil];
			}
			break;

		case DISPLAY_OPENGL:
			break;
	}
}


/*
 *  Deinitialization
 */

void VideoExit(void)
{
	// Close displays
	vector<monitor_desc *>::iterator	i, end;

	end = VideoMonitors.end();

	for (i = VideoMonitors.begin(); i != end; ++i)
		dynamic_cast<OSX_monitor *>(*i)->video_close();

	VideoMonitors.clear();
	VideoModes.clear();
}


/*
 *  Set palette
 */

void
OSX_monitor::set_palette(uint8 *pal, int num)
{
	if ( [output isFullScreen] && CGDisplayCanSetPalette(theDisplay)
								&& ! IsDirectMode(get_current_mode()) )
	{
		CGDirectPaletteRef	CGpal;
		CGDisplayErr		err;


		CGpal = CGPaletteCreateWithByteSamples((CGDeviceByteColor *)pal, num);
		err   = CGDisplaySetPalette(theDisplay, CGpal);
		if ( err != noErr )
			NSLog(@"Failed to set palette, error = %d", err);
		CGPaletteRelease(CGpal);
	}

#ifdef CGIMAGEREF
	if ( display_type != DISPLAY_WINDOW )
		return;

	// To change the palette, we have to regenerate
	// the CGImageRef with the new colour space.

	CGImageRef			oldImageRef = imageRef;
	CGColorSpaceRef		oldColourSpace = colourSpace;

	colourSpace = CGColorSpaceCreateDeviceRGB();

	if ( depth < 16 )
	{
		CGColorSpaceRef		tempColourSpace = colourSpace;

		colourSpace = CGColorSpaceCreateIndexed(colourSpace, 255, pal);
		CGColorSpaceRelease(tempColourSpace);
	}

	if ( ! colourSpace )
	{
		ErrorAlert("No valid colour space");
		return;
	}

	imageRef = CGImageCreate(x, y, bpp, depth, bpr, colourSpace,
  #ifdef CG_USE_ALPHA
							 kCGImageAlphaPremultipliedFirst,
  #else
							 kCGImageAlphaNoneSkipFirst,
  #endif
							 provider,
							 NULL, 	// colourMap translation table
							 NO,	// shouldInterpolate colours?
							 kCGRenderingIntentDefault);
	if ( ! imageRef )
	{
		ErrorAlert("Could not create CGImage from CGDataProvider");
		return;
	}

	unsigned char *offsetBuffer = (unsigned char *) the_buffer;
	offsetBuffer += 1;		// OS X NSBitmaps are RGBA, but Basilisk generates ARGB

	[output readyToDraw: imageRef
				 bitmap: offsetBuffer
			 imageWidth: x
			imageHeight: y];

	CGColorSpaceRelease(oldColourSpace);
	CGImageRelease(oldImageRef);
#endif
}


/*
 *  Switch video mode
 */

void
OSX_monitor::switch_to_current_mode(void)
{
	video_mode mode = get_current_mode();
	const char *failure = NULL;

	D(bug("switch_to_current_mode(): width=%d  height=%d  depth=%d  bytes_per_row=%d\n", mode.x, mode.y, bits_from_depth(mode.depth), mode.bytes_per_row));
	
	if ( display_type == DISPLAY_SCREEN && originalMode )
	{
		D(NSLog(@"About to call CGDisplayBestModeForParameters()"));
		newMode = CGDisplayBestModeForParameters(theDisplay,
												 bits_from_depth(mode.depth),
														mode.x, mode.y, NULL);
		if ( ! newMode )
			failure = "Could not find a matching screen mode";
		else
		{
			D(NSLog(@"About to call CGDisplaySwitchToMode()"));
			if ( CGDisplaySwitchToMode(theDisplay, newMode) != CGDisplayNoErr )
				failure = "Could not switch to matching screen mode";
		}

		// For mouse event processing: update screen height
		[output startedFullScreen: theDisplay];

		if ( ! failure &&
			mode.bytes_per_row != CGDisplayBytesPerRow(theDisplay) )
		{
			D(bug("Bytes per row (%d) doesn't match current (%ld)\n",
					mode.bytes_per_row, CGDisplayBytesPerRow(theDisplay)));
			mode.bytes_per_row = CGDisplayBytesPerRow(theDisplay);
		}

		if ( ! failure &&
			 ! ( the_buffer = CGDisplayBaseAddress(theDisplay) ) )
			failure = "Could not get base address of screen";

	}
#ifdef CGIMAGEREF
	// Clean up the old CGImageRef stuff
	else if ( display_type == DISPLAY_WINDOW && imageRef )
	{
		CGImageRef			oldImageRef		= imageRef;
		CGColorSpaceRef		oldColourSpace	= colourSpace;
		CGDataProviderRef	oldProvider		= provider;
		void				*oldBuffer		= the_buffer;
 
		if ( video_open(mode) )
		{
			CGImageRelease(oldImageRef);
			CGColorSpaceRelease(oldColourSpace);
			CGDataProviderRelease(oldProvider);
			free(oldBuffer);
		}
		else
			failure = "Could not video_open() requested mode";
	}
#endif
	else if ( ! video_open(mode) )
		failure = "Could not video_open() requested mode";

	if ( ! failure && display_type == DISPLAY_SCREEN )
	{
		// Whenever we change screen resolution, the MacOS mouse starts
		// up in the top left corner. Send real mouse to that location
//		if ( CGDisplayMoveCursorToPoint(theDisplay, CGPointMake(15,15))
//														== CGDisplayNoErr )
//		{
			// 
			[output fullscreenMouseMove];
//		}
//		else
//			failure = "Could move (jump) cursor on screen";
	}

	if ( failure )
	{
		NSLog(@"In switch_to_current_mode():");
		NSLog(@"%s.", failure);
		video_close();
		if ( display_type == DISPLAY_SCREEN )
			ErrorAlert("Cannot switch screen to selected video mode");
		else
			ErrorAlert(STR_OPEN_WINDOW_ERR);
		QuitEmulator();
	}
	else
		set_mac_frame_buffer(mode);
}

/*
 *  Close down full-screen mode
 *	(if bringing up error alerts is unsafe while in full-screen mode)
 */

void VideoQuitFullScreen(void)
{
}


/*
 *  Mac VBL interrupt
 */

void VideoInterrupt(void)
{
}


// This function is called on non-threaded platforms from a timer interrupt
void VideoRefresh(void)
{
}



// Deal with a memory access signal referring to the screen.
// For now, just ignore
bool Screen_fault_handler(char *a, char *b)
{
//	NSLog(@"Got a screen fault %lx %lx", a, b);
//	[output setNeedsDisplay: YES];
	return YES;
}
