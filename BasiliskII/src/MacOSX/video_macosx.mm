/*
 *  $Id$
 *
 *  video_macosx.mm - Interface between Basilisk II and Cocoa windowing.
 *                    Based on video_amiga.cpp and video_x.cpp
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
#include "debug.h"



// Global variables
uint8		display_type = DISPLAY_WINDOW,	// These are used by PrefsEditor
			frame_skip;	
uint16		init_width  = MIN_WIDTH,		// as well as this code
			init_height = MIN_HEIGHT,
			init_depth  = 32,
			screen_height = MIN_HEIGHT;		// Used by processMouseMove

		EmulatorView	*output = nil;		// Set by [EmulatorView init]
		NSWindow		*the_win = nil;		// Set by [Emulator awakeFromNib]
static	void			*the_buffer = NULL;


#import <Foundation/NSString.h>				// Needed for NSLog(@"")

#ifdef CGIMAGEREF
static CGImageRef		imageRef = nil;
#endif

#ifdef NSBITMAP
#import <AppKit/NSBitmapImageRep.h>

static NSBitmapImageRep	*bitmap = nil;
#endif

// These record changes we made in setting full screen mode
CGDirectDisplayID	theDisplay   = NULL;
CFDictionaryRef		originalMode = NULL,
					newMode = NULL;



// Prototypes

static void add_mode			(const uint16 width, const uint16 height,
								 const uint32 resolution_id,
								 const uint32 bytes_per_row,
								 const video_depth depth);
static void add_standard_modes	(const video_depth depth);

static bool video_open	(const video_mode &mode);
static void video_close	(void);



/*
 *  Utility functions
 */

uint8 bits_from_depth(const video_depth depth)
{
	int bits = 1 << depth;
//	if (bits == 16)
//		bits = 15;
//	else if (bits == 32)
//		bits = 24;
	return bits;
}

char *
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

char *
colours_from_depth(const uint16 depth)
{
	return colours_from_depth(DepthModeForPixelDepth(depth) );
}

bool
parse_screen_prefs(const char *mode_str)
{
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


// Add mode to list of supported modes
static void
add_mode(const uint16 width, const uint16 height,
		 const uint32 resolution_id, const uint32 bytes_per_row,
		 const video_depth depth)
{
	video_mode mode;
	mode.x = width;
	mode.y = height;
	mode.resolution_id = resolution_id;
	mode.bytes_per_row = bytes_per_row;
	mode.depth = depth;

	D(bug("Added video mode: w=%ld  h=%ld  d=%ld(%d bits)\n",
				width, height, depth, bits_from_depth(depth) ));

	VideoModes.push_back(mode);
}

// Add standard list of windowed modes for given color depth
static void add_standard_modes(const video_depth depth)
{
	D(bug("add_standard_modes: depth=%ld(%d bits)\n",
						depth, bits_from_depth(depth) ));

	add_mode(512,  384,  0x80, TrivialBytesPerRow(512,  depth), depth);
	add_mode(640,  480,  0x81, TrivialBytesPerRow(640,  depth), depth);
	add_mode(800,  600,  0x82, TrivialBytesPerRow(800,  depth), depth);
	add_mode(832,  624,  0x83, TrivialBytesPerRow(832,  depth), depth);
	add_mode(1024, 768,  0x84, TrivialBytesPerRow(1024, depth), depth);
	add_mode(1152, 768,  0x85, TrivialBytesPerRow(1152, depth), depth);
	add_mode(1152, 870,  0x86, TrivialBytesPerRow(1152, depth), depth);
	add_mode(1280, 1024, 0x87, TrivialBytesPerRow(1280, depth), depth);
	add_mode(1600, 1200, 0x88, TrivialBytesPerRow(1600, depth), depth);
}

// Helper function to get a 32bit int from a dictionary
static int32 getCFint32 (CFDictionaryRef dict, CFStringRef key)
{
	int32		val = 0;
	CFNumberRef	ref = CFDictionaryGetValue(dict, key);

	if ( ref && CFNumberGetValue(ref, kCFNumberSInt32Type, &val) )
		if ( ! val )
			NSLog(@"getCFint32() - Logic error - Got a value of 0");

	return val;
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
		return false;

	for ( CGDisplayCount dc = 0; dc < n; ++dc )
	{
		CGDirectDisplayID	d = displays[dc];
		CFArrayRef			m = CGDisplayAvailableModes(d);

		if ( m == NULL )					// Store the current display mode
			add_mode(CGDisplayPixelsWide(d),
					 CGDisplayPixelsHigh(d),
					 res_id++, CGDisplayBytesPerRow(d),
					 DepthModeForPixelDepth(CGDisplayBitsPerPixel(d)));
		else
		{
			CFIndex  nModes = CFArrayGetCount(m);

			for ( CFIndex mc = 0; mc < nModes; ++mc )
			{
				CFDictionaryRef modeSpec = CFArrayGetValueAtIndex(m, mc);

				int32	bpp    = getCFint32(modeSpec, kCGDisplayBitsPerPixel);
				int32	height = getCFint32(modeSpec, kCGDisplayHeight);
				int32	mode   = getCFint32(modeSpec, kCGDisplayMode);
				int32	width  = getCFint32(modeSpec, kCGDisplayWidth);
				video_depth	depth = DepthModeForPixelDepth(bpp);

				if ( ! bpp || ! height || ! width )
				{
					NSLog(@"Could not get details of mode %d, display %d",
							mc, dc);
					return false;
				}
#if DEBUG
				else
					NSLog(@"Display %ld, spec = %@", d, modeSpec);
#endif

				add_mode(width, height, res_id,
						 TrivialBytesPerRow(width, depth), depth);

				if ( ! oldRes )
					oldRes = width * height;
				else
					if ( oldRes != width * height )
					{
						oldRes = width * height;
						++res_id;
					}
			}
		}
	}

	return true;
}

// Set Mac frame layout and base address (uses the_buffer/MacFrameBaseMac)
static void set_mac_frame_buffer(video_depth depth)
{
#if !REAL_ADDRESSING && !DIRECT_ADDRESSING
	switch ( depth )
	{
	//	case VDEPTH_15BIT:
		case VDEPTH_16BIT: MacFrameLayout = FLAYOUT_HOST_555; break;
	//	case VDEPTH_24BIT:
		case VDEPTH_32BIT: MacFrameLayout = FLAYOUT_HOST_888; break;
		default			 : MacFrameLayout = FLAYOUT_DIRECT;
	}
	VideoMonitor.mac_frame_base = MacFrameBaseMac;

	// Set variables used by UAE memory banking
	MacFrameBaseHost = the_buffer;
	MacFrameSize = VideoMonitor.mode.bytes_per_row * VideoMonitor.mode.y;
	InitFrameBufferMapping();
#else
	VideoMonitor.mac_frame_base = Host2MacAddr(the_buffer);
#endif
	D(bug("VideoMonitor.mac_frame_base = %08x\n", VideoMonitor.mac_frame_base));
}

void resizeWinBy(const short deltaX, const short deltaY)
{
	NSRect	rect = [the_win frame];

	D(bug("resizeWinBy(%d,%d) - ", deltaX, deltaY));
	D(bug("old x=%g, y=%g", rect.size.width, rect.size.height));

	rect.size.width  += deltaX;
	rect.size.height += deltaY;

	D(bug(", new x=%g, y=%g\n", rect.size.width, rect.size.height));

	[the_win setFrame: rect display: YES];
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
static bool init_window(const video_mode &mode)
{
#ifdef CGIMAGEREF
	CGColorSpaceRef		colourSpace;
	CGDataProviderRef	provider;
#endif
	short				bitsPer, samplesPer;	// How big is each Pixel?
	int					the_buffer_size;

	D(bug("init_window: depth=%ld(%d bits)\n",
			mode.depth, bits_from_depth(mode.depth) ));


	// Set absolute mouse mode
	ADBSetRelMouseMode(false);


	// Open window
	if (the_win == NULL)
	{
		ErrorAlert(STR_OPEN_WINDOW_ERR);
		return false;
	}
	resizeWinTo(mode.x, mode.y);


	// Create frame buffer ("height + 2" for safety)
	the_buffer_size = mode.bytes_per_row * (mode.y + 2);
	the_buffer = calloc(the_buffer_size, 1);
	if (the_buffer == NULL)
	{
		NSLog(@"calloc(%d) failed", the_buffer_size);
		ErrorAlert(STR_NO_MEM_ERR);
		return false;
	}
	D(bug("the_buffer = %p\n", the_buffer));


	if ( mode.depth == VDEPTH_1BIT )
		bitsPer = 1;
	else
		bitsPer = 8;

	if ( mode.depth == VDEPTH_32BIT )
		samplesPer = 3;
	else
		samplesPer = 1;

#ifdef CGIMAGEREF
	switch ( mode.depth )
	{
		//case VDEPTH_1BIT:	colourSpace = CGColorSpaceCreateDeviceMono(); break
		case VDEPTH_8BIT:	colourSpace = CGColorSpaceCreateDeviceGray(); break;
		case VDEPTH_32BIT:	colourSpace = CGColorSpaceCreateDeviceRGB();  break;
		default:			colourSpace = NULL;
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
	imageRef = CGImageCreate(mode.x,
							 mode.y,
							 bitsPer,
							 bits_from_depth(mode.depth),
							 mode.bytes_per_row,
							 colourSpace,
							 kCGImageAlphaNoneSkipFirst,
							 provider,
							 NULL, 	// colourMap
							 NO,	// shouldInterpolate
							 kCGRenderingIntentDefault);
	if ( ! imageRef )
	{
		ErrorAlert("Could not create CGImage from CGDataProvider");
		return false;
	}
	CGDataProviderRelease(provider);
	CGColorSpaceRelease(colourSpace);

	[output readyToDraw: imageRef
			 imageWidth: mode.x
			imageHeight: mode.y];
#else
	unsigned char *offsetBuffer = the_buffer;
	offsetBuffer += 1;	// OS X NSBitmaps are RGBA, but Basilisk generates ARGB
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

    if ( bitmap == nil )
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

	// Set VideoMonitor
	VideoMonitor.mode = mode;
	set_mac_frame_buffer(mode.depth);

	return true;
}

#import <AppKit/NSEvent.h>

// How do I include this file?
// #import <Carbon/HIToolbox/Menus.h>
extern "C" void HideMenuBar(),
				ShowMenuBar();

static bool init_screen(const video_mode &mode)
{
	// Set absolute mouse mode
	ADBSetRelMouseMode(false);


	theDisplay = kCGDirectMainDisplay;	// For now
	originalMode = CGDisplayCurrentMode(theDisplay);

	newMode = CGDisplayBestModeForParameters(theDisplay,
											 bits_from_depth(mode.depth),
													mode.x, mode.y, NULL);
	if ( NULL == newMode )
	{
		ErrorAlert("Could not find a matching screen mode");
		return false;
	}

	[the_win miniaturize: nil];
//	[the_win setLevel: CGShieldingWindowLevel()];
	the_win = nil;

	HideMenuBar();

	CGDisplayCapture(theDisplay);

	if ( CGDisplaySwitchToMode(theDisplay, newMode) != CGDisplayNoErr )
	{
		ErrorAlert("Could not switch to matching screen mode");
		return false;
	}

	CGDisplayHideCursor(theDisplay);

	the_buffer = CGDisplayBaseAddress(theDisplay);
	if ( the_buffer == NULL )
	{
		ErrorAlert("Could not get base address of screen");
		return false;
	}

	screen_height = mode.y;		// For mouse co-ordinate flipping

	// Send emulated mouse to current location

	NSPoint mouse = [NSEvent mouseLocation];
	ADBMouseMoved((int)mouse.x, screen_height - (int)mouse.y);

	// Set VideoMonitor
	VideoMonitor.mode = mode;
	set_mac_frame_buffer(mode.depth);

	return true;
}


static bool init_opengl(const video_mode &mode)
{
	ErrorAlert("Sorry. OpenGL mode is not implemented yet");
	return false;
}

/*
 *  Initialization
 */

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
		add_mode(512, 342, 0x80, 64, VDEPTH_1BIT);
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
				//add_standard_modes(VDEPTH_1BIT);
				//add_standard_modes(VDEPTH_8BIT);
				//add_standard_modes(VDEPTH_16BIT);
				add_standard_modes(VDEPTH_32BIT);
				break;
		}

	video_init_depth_list();

#if DEBUG
	bug("Available video modes:\n");
	vector<video_mode>::const_iterator i, end = VideoModes.end();
	for (i = VideoModes.begin(); i != end; ++i)
		bug(" %dx%d (ID %02x), %s\n", i->x, i->y, i->resolution_id,
										colours_from_depth(i->depth));
#endif

	D(bug("VideoInit: width=%hd height=%hd depth=%ld\n",
						init_width, init_height, init_depth));

	// Find requested default mode and open display
	if (VideoModes.size() > 0)
	{
		// Find mode with specified dimensions
		std::vector<video_mode>::const_iterator i, end = VideoModes.end();
		for (i = VideoModes.begin(); i != end; ++i)
		{
			D(bug("VideoInit: w=%ld h=%ld d=%ld\n",
					i->x, i->y, bits_from_depth(i->depth)));
			if (i->x == init_width && i->y == init_height
					&& bits_from_depth(i->depth) == init_depth)
				return video_open(*i);
		}
	}

	char str[150];
	sprintf(str, "Cannot open selected video mode\r(%hd x %hd, %s).\r%s",
			init_width, init_height,
			colours_from_depth(init_depth), "Using lowest resolution");
	WarningAlert(str);

	return video_open(VideoModes[0]);
}


// Open display for specified mode
static bool video_open(const video_mode &mode)
{
	D(bug("video_open: width=%ld  height=%ld  depth=%ld  bytes_per_row=%ld\n",
			mode.x, mode.y, bits_from_depth(mode.depth), mode.bytes_per_row));

	// Open display
	switch ( display_type ) {
		case DISPLAY_WINDOW:
			if ( ! init_window(mode) )
				return false;
			break;

		case DISPLAY_SCREEN:
			if ( ! init_screen(mode) )
				return false;
			break;

		case DISPLAY_OPENGL:
			if ( ! init_opengl(mode) )
				return false;
			break;
	}

	return true;
}


static void video_close()
{
	D(bug("video_close()\n"));

	switch ( display_type ) {
		case DISPLAY_WINDOW:
			// Stop redraw thread
			[output disableDrawing];

			// Free frame buffer stuff
#ifdef CGIMAGEREF
			CGImageRelease(imageRef);
#endif
#ifdef NSBITMAP
			[bitmap release];
#endif
			free(the_buffer);

			break;

		case DISPLAY_SCREEN:
			if ( theDisplay && originalMode )
			{
				//CGDisplayShowCursor(theDisplay);
				CGDisplaySwitchToMode(theDisplay, originalMode);
				CGDisplayRelease(theDisplay);
				ShowMenuBar();
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
	video_close();
}


/*
 *  Set palette
 */

void video_set_palette(uint8 *pal, int num)
{
	if ( FULLSCREEN && CGDisplayCanSetPalette(theDisplay)
					&& ! IsDirectMode(VideoMonitor.mode) )
	{
		CGDirectPaletteRef	CGpal;
		CGDisplayErr		err;


		CGpal = CGPaletteCreateWithByteSamples((CGDeviceByteColor *)pal, num);
		err   = CGDisplaySetPalette(theDisplay, CGpal);
		if ( err != noErr )
			NSLog(@"Failed to set palette, error = %d", err);
		CGPaletteRelease(CGpal);
	}
}


/*
 *  Switch video mode
 */

void video_switch_to_mode(const video_mode &mode)
{
	// Close and reopen display
	video_close();
	if (!video_open(mode))
	{
		if ( display_type == DISPLAY_SCREEN )
			ErrorAlert("Cannot switch screen to selected video mode");
		else
			ErrorAlert(STR_OPEN_WINDOW_ERR);
		QuitEmulator();
	}
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
