/*
 *  video_x.cpp - Video/graphics emulation, X11 specific stuff
 *
 *  SheepShaver (C) 1997-2004 Marc Hellwig and Christian Bauer
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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>
#include <pthread.h>

#include <algorithm>

#ifdef ENABLE_XF86_DGA
# include <X11/extensions/xf86dga.h>
#endif

#ifdef ENABLE_XF86_VIDMODE
# include <X11/extensions/xf86vmode.h>
#endif

#include "main.h"
#include "adb.h"
#include "prefs.h"
#include "user_strings.h"
#include "about_window.h"
#include "video.h"
#include "video_defs.h"
#include "video_blit.h"

#define DEBUG 0
#include "debug.h"

#ifndef NO_STD_NAMESPACE
using std::sort;
#endif


// Constants
const char KEYCODE_FILE_NAME[] = DATADIR "/keycodes";
static const bool hw_mac_cursor_accl = true;	// Flag: Enable MacOS to X11 copy of cursor?

// Global variables
static int32 frame_skip;
static int16 mouse_wheel_mode;
static int16 mouse_wheel_lines;
static bool redraw_thread_active = false;	// Flag: Redraw thread installed
static pthread_attr_t redraw_thread_attr;	// Redraw thread attributes
static volatile bool redraw_thread_cancel;	// Flag: Cancel Redraw thread
static pthread_t redraw_thread;				// Redraw thread

static bool local_X11;						// Flag: X server running on local machine?
static volatile bool thread_stop_req = false;
static volatile bool thread_stop_ack = false;	// Acknowledge for thread_stop_req

static bool has_dga = false;				// Flag: Video DGA capable
static bool has_vidmode = false;			// Flag: VidMode extension available

#ifdef ENABLE_VOSF
static bool use_vosf = true;				// Flag: VOSF enabled
#else
static const bool use_vosf = false;			// VOSF not possible
#endif

static bool palette_changed = false;		// Flag: Palette changed, redraw thread must update palette
static bool ctrl_down = false;				// Flag: Ctrl key pressed
static bool caps_on = false;				// Flag: Caps Lock on
static bool quit_full_screen = false;		// Flag: DGA close requested from redraw thread
static volatile bool quit_full_screen_ack = false;	// Acknowledge for quit_full_screen
static bool emerg_quit = false;				// Flag: Ctrl-Esc pressed, emergency quit requested from MacOS thread

static bool emul_suspended = false;			// Flag: emulator suspended
static Window suspend_win;					// "Suspend" window
static void *fb_save = NULL;				// Saved frame buffer for suspend
static bool use_keycodes = false;			// Flag: Use keycodes rather than keysyms
static int keycode_table[256];				// X keycode -> Mac keycode translation table

// X11 variables
static int screen;							// Screen number
static int xdepth;							// Depth of X screen
static int depth;							// Depth of Mac frame buffer
static Window rootwin, the_win;				// Root window and our window
static int num_depths = 0;					// Number of available X depths
static int *avail_depths = NULL;			// List of available X depths
static VisualFormat visualFormat;
static XVisualInfo visualInfo;
static Visual *vis;
static int color_class;
static int rshift, rloss, gshift, gloss, bshift, bloss;	// Pixel format of DirectColor/TrueColor modes
static Colormap cmap[2];					// Two colormaps (DGA) for 8-bit mode
static XColor x_palette[256];				// Color palette to be used as CLUT and gamma table

static XColor black, white;
static unsigned long black_pixel, white_pixel;
static int eventmask;
static const int win_eventmask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | EnterWindowMask | ExposureMask | StructureNotifyMask;
static const int dga_eventmask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask;

// Variables for window mode
static GC the_gc;
static XImage *img = NULL;
static XShmSegmentInfo shminfo;
static XImage *cursor_image, *cursor_mask_image;
static Pixmap cursor_map, cursor_mask_map;
static Cursor mac_cursor;
static GC cursor_gc, cursor_mask_gc;
static bool cursor_changed = false;			// Flag: Cursor changed, window_func must update cursor
static bool have_shm = false;				// Flag: SHM present and usable
static uint8 *the_buffer = NULL;			// Pointer to Mac frame buffer
static uint8 *the_buffer_copy = NULL;		// Copy of Mac frame buffer
static uint32 the_buffer_size;				// Size of allocated the_buffer

// Variables for DGA mode
static int current_dga_cmap;

#ifdef ENABLE_XF86_VIDMODE
// Variables for XF86 VidMode support
static XF86VidModeModeInfo **x_video_modes;		// Array of all available modes
static int num_x_video_modes;
#endif

// Mutex to protect palette
#ifdef HAVE_SPINLOCKS
static spinlock_t x_palette_lock = SPIN_LOCK_UNLOCKED;
#define LOCK_PALETTE spin_lock(&x_palette_lock)
#define UNLOCK_PALETTE spin_unlock(&x_palette_lock)
#elif defined(HAVE_PTHREADS)
static pthread_mutex_t x_palette_lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK_PALETTE pthread_mutex_lock(&x_palette_lock)
#define UNLOCK_PALETTE pthread_mutex_unlock(&x_palette_lock)
#else
#define LOCK_PALETTE
#define UNLOCK_PALETTE
#endif


// Prototypes
static void *redraw_func(void *arg);


// From main_unix.cpp
extern char *x_display_name;
extern Display *x_display;

// From sys_unix.cpp
extern void SysMountFirstFloppy(void);

// From clip_unix.cpp
extern void ClipboardSelectionClear(XSelectionClearEvent *);
extern void ClipboardSelectionRequest(XSelectionRequestEvent *);


// Video acceleration through SIGSEGV
#ifdef ENABLE_VOSF
# include "video_vosf.h"
#endif


/*
 *  Utility functions
 */

// Get current video mode
static inline int get_current_mode(void)
{
	return VModes[cur_mode].viAppleMode;
}

// Find palette size for given color depth
static int palette_size(int mode)
{
	switch (mode) {
	case APPLE_1_BIT: return 2;
	case APPLE_2_BIT: return 4;
	case APPLE_4_BIT: return 16;
	case APPLE_8_BIT: return 256;
	case APPLE_16_BIT: return 32;
	case APPLE_32_BIT: return 256;
	default: return 0;
	}
}

// Return bits per pixel for requested depth
static inline int bytes_per_pixel(int depth)
{
	int bpp;
	switch (depth) {
	case 8:
		bpp = 1;
		break;
	case 15: case 16:
		bpp = 2;
		break;
	case 24: case 32:
		bpp = 4;
		break;
	default:
		abort();
	}
	return bpp;
}

// Map video_mode depth ID to numerical depth value
static inline int depth_of_video_mode(int mode)
{
	int depth;
	switch (mode) {
	case APPLE_1_BIT:
		depth = 1;
		break;
	case APPLE_2_BIT:
		depth = 2;
		break;
	case APPLE_4_BIT:
		depth = 4;
		break;
	case APPLE_8_BIT:
		depth = 8;
		break;
	case APPLE_16_BIT:
		depth = 16;
		break;
	case APPLE_32_BIT:
		depth = 32;
		break;
	default:
		abort();
	}
	return depth;
}

// Map RGB color to pixel value (this only works in TrueColor/DirectColor visuals)
static inline uint32 map_rgb(uint8 red, uint8 green, uint8 blue)
{
	return ((red >> rloss) << rshift) | ((green >> gloss) << gshift) | ((blue >> bloss) << bshift);
}


// Do we have a visual for handling the specified Mac depth? If so, set the
// global variables "xdepth", "visualInfo", "vis" and "color_class".
static bool find_visual_for_depth(int depth)
{
	D(bug("have_visual_for_depth(%d)\n", depth_of_video_mode(depth)));

	// 1-bit works always and uses default visual
	if (depth == APPLE_1_BIT) {
		vis = DefaultVisual(x_display, screen);
		visualInfo.visualid = XVisualIDFromVisual(vis);
		int num = 0;
		XVisualInfo *vi = XGetVisualInfo(x_display, VisualIDMask, &visualInfo, &num);
		visualInfo = vi[0];
		XFree(vi);
		xdepth = visualInfo.depth;
		color_class = visualInfo.c_class;
		D(bug(" found visual ID 0x%02x, depth %d\n", visualInfo.visualid, xdepth));
		return true;
	}

	// Calculate minimum and maximum supported X depth
	int min_depth = 1, max_depth = 32;
	switch (depth) {
#ifdef ENABLE_VOSF
		case APPLE_2_BIT:
		case APPLE_4_BIT:	// VOSF blitters can convert 2/4/8-bit -> 8/16/32-bit
		case APPLE_8_BIT:
			min_depth = 8;
			max_depth = 32;
			break;
#else
		case APPLE_2_BIT:
		case APPLE_4_BIT:	// 2/4-bit requires VOSF blitters
			return false;
		case APPLE_8_BIT:	// 8-bit without VOSF requires an 8-bit visual
			min_depth = 8;
			max_depth = 8;
			break;
#endif
		case APPLE_16_BIT:	// 16-bit requires a 15/16-bit visual
			min_depth = 15;
			max_depth = 16;
			break;
		case APPLE_32_BIT:	// 32-bit requires a 24/32-bit visual
			min_depth = 24;
			max_depth = 32;
			break;
	}
	D(bug(" minimum required X depth is %d, maximum supported X depth is %d\n", min_depth, max_depth));

	// Try to find a visual for one of the color depths
	bool visual_found = false;
	for (int i=0; i<num_depths && !visual_found; i++) {

		xdepth = avail_depths[i];
		D(bug(" trying to find visual for depth %d\n", xdepth));
		if (xdepth < min_depth || xdepth > max_depth)
			continue;

		// Determine best color class for this depth
		switch (xdepth) {
			case 1:	// Try StaticGray or StaticColor
				if (XMatchVisualInfo(x_display, screen, xdepth, StaticGray, &visualInfo)
				 || XMatchVisualInfo(x_display, screen, xdepth, StaticColor, &visualInfo))
					visual_found = true;
				break;
			case 8:	// Need PseudoColor
				if (XMatchVisualInfo(x_display, screen, xdepth, PseudoColor, &visualInfo))
					visual_found = true;
				break;
			case 15:
			case 16:
			case 24:
			case 32: // Try DirectColor first, as this will allow gamma correction
				if (XMatchVisualInfo(x_display, screen, xdepth, DirectColor, &visualInfo)
				 || XMatchVisualInfo(x_display, screen, xdepth, TrueColor, &visualInfo))
					visual_found = true;
				break;
			default:
				D(bug("  not a supported depth\n"));
				break;
		}
	}
	if (!visual_found)
		return false;

	// Visual was found
	vis = visualInfo.visual;
	color_class = visualInfo.c_class;
	D(bug(" found visual ID 0x%02x, depth %d, class ", visualInfo.visualid, xdepth));
#if DEBUG
	switch (color_class) {
		case StaticGray: D(bug("StaticGray\n")); break;
		case GrayScale: D(bug("GrayScale\n")); break;
		case StaticColor: D(bug("StaticColor\n")); break;
		case PseudoColor: D(bug("PseudoColor\n")); break;
		case TrueColor: D(bug("TrueColor\n")); break;
		case DirectColor: D(bug("DirectColor\n")); break;
	}
#endif
	return true;
}


/*
 *  Open display (window or fullscreen)
 */

// Set WM_DELETE_WINDOW protocol on window (preventing it from being destroyed by the WM when clicking on the "close" widget)
static Atom WM_DELETE_WINDOW = (Atom)0;
static void set_window_delete_protocol(Window w)
{
	WM_DELETE_WINDOW = XInternAtom(x_display, "WM_DELETE_WINDOW", false);
	XSetWMProtocols(x_display, w, &WM_DELETE_WINDOW, 1);
}

// Wait until window is mapped/unmapped
static void wait_mapped(Window w)
{
	XEvent e;
	do {
		XMaskEvent(x_display, StructureNotifyMask, &e);
	} while ((e.type != MapNotify) || (e.xmap.event != w));
}

static void wait_unmapped(Window w)
{
	XEvent e;
	do {
		XMaskEvent(x_display, StructureNotifyMask, &e);
	} while ((e.type != UnmapNotify) || (e.xmap.event != w));
}

// Trap SHM errors
static bool shm_error = false;
static int (*old_error_handler)(Display *, XErrorEvent *);

static int error_handler(Display *d, XErrorEvent *e)
{
	if (e->error_code == BadAccess) {
		shm_error = true;
		return 0;
	} else
		return old_error_handler(d, e);
}

// Open window
static bool open_window(int width, int height)
{
	int aligned_width = (width + 15) & ~15;
	int aligned_height = (height + 15) & ~15;

	// Set absolute mouse mode
	ADBSetRelMouseMode(false);

	// Create window
	XSetWindowAttributes wattr;
	wattr.event_mask = eventmask = win_eventmask;
	wattr.background_pixel = (vis == DefaultVisual(x_display, screen) ? black_pixel : 0);
	wattr.border_pixel = 0;
	wattr.backing_store = NotUseful;
	wattr.colormap = (depth == 1 ? DefaultColormap(x_display, screen) : cmap[0]);
	the_win = XCreateWindow(x_display, rootwin, 0, 0, width, height, 0, xdepth,
		InputOutput, vis, CWEventMask | CWBackPixel | CWBorderPixel | CWBackingStore | CWColormap, &wattr);

	// Set window name
	XStoreName(x_display, the_win, GetString(STR_WINDOW_TITLE));

	// Set delete protocol property
	set_window_delete_protocol(the_win);

	// Make window unresizable
	XSizeHints *hints;
	if ((hints = XAllocSizeHints()) != NULL) {
		hints->min_width = width;
		hints->max_width = width;
		hints->min_height = height;
		hints->max_height = height;
		hints->flags = PMinSize | PMaxSize;
		XSetWMNormalHints(x_display, the_win, hints);
		XFree((char *)hints);
	}

	// Show window
	XMapWindow(x_display, the_win);
	wait_mapped(the_win);

	// 1-bit mode is big-endian; if the X server is little-endian, we can't
	// use SHM because that doesn't allow changing the image byte order
	bool need_msb_image = (depth == 1 && XImageByteOrder(x_display) == LSBFirst);

	// Try to create and attach SHM image
	have_shm = false;
	if (local_X11 && !need_msb_image && XShmQueryExtension(x_display)) {

		// Create SHM image ("height + 2" for safety)
		img = XShmCreateImage(x_display, vis, depth == 1 ? 1 : xdepth, depth == 1 ? XYBitmap : ZPixmap, 0, &shminfo, width, height);
		shminfo.shmid = shmget(IPC_PRIVATE, (aligned_height + 2) * img->bytes_per_line, IPC_CREAT | 0777);
		D(bug(" shm image created\n"));
		the_buffer_copy = (uint8 *)shmat(shminfo.shmid, 0, 0);
		shminfo.shmaddr = img->data = (char *)the_buffer_copy;
		shminfo.readOnly = False;

		// Try to attach SHM image, catching errors
		shm_error = false;
		old_error_handler = XSetErrorHandler(error_handler);
		XShmAttach(x_display, &shminfo);
		XSync(x_display, false);
		XSetErrorHandler(old_error_handler);
		if (shm_error) {
			shmdt(shminfo.shmaddr);
			XDestroyImage(img);
			shminfo.shmid = -1;
		} else {
			have_shm = true;
			shmctl(shminfo.shmid, IPC_RMID, 0);
		}
		D(bug(" shm image attached\n"));
	}

	// Create normal X image if SHM doesn't work ("height + 2" for safety)
	if (!have_shm) {
		int bytes_per_row = depth == 1 ? aligned_width/8 : TrivialBytesPerRow(aligned_width, DepthModeForPixelDepth(xdepth));
		the_buffer_copy = (uint8 *)malloc((aligned_height + 2) * bytes_per_row);
		img = XCreateImage(x_display, vis, depth == 1 ? 1 : xdepth, depth == 1 ? XYBitmap : ZPixmap, 0, (char *)the_buffer_copy, aligned_width, aligned_height, 32, bytes_per_row);
		D(bug(" X image created\n"));
	}

	// 1-Bit mode is big-endian
    if (need_msb_image) {
        img->byte_order = MSBFirst;
        img->bitmap_bit_order = MSBFirst;
    }

#ifdef ENABLE_VOSF
	use_vosf = true;
	// Allocate memory for frame buffer (SIZE is extended to page-boundary)
	the_host_buffer = the_buffer_copy;
	the_buffer_size = page_extend((aligned_height + 2) * img->bytes_per_line);
	the_buffer = (uint8 *)vm_acquire(the_buffer_size);
	the_buffer_copy = (uint8 *)malloc(the_buffer_size);
	D(bug("the_buffer = %p, the_buffer_copy = %p, the_host_buffer = %p\n", the_buffer, the_buffer_copy, the_host_buffer));
#else
	// Allocate memory for frame buffer
	the_buffer = (uint8 *)malloc((aligned_height + 2) * img->bytes_per_line);
	D(bug("the_buffer = %p, the_buffer_copy = %p\n", the_buffer, the_buffer_copy));
#endif
	screen_base = (uint32)the_buffer;

	// Create GC
	the_gc = XCreateGC(x_display, the_win, 0, 0);
	XSetState(x_display, the_gc, black_pixel, white_pixel, GXcopy, AllPlanes);

	// Create cursor
	if (hw_mac_cursor_accl) {
		cursor_image = XCreateImage(x_display, vis, 1, XYPixmap, 0, (char *)MacCursor + 4, 16, 16, 16, 2);
		cursor_image->byte_order = MSBFirst;
		cursor_image->bitmap_bit_order = MSBFirst;
		cursor_mask_image = XCreateImage(x_display, vis, 1, XYPixmap, 0, (char *)MacCursor + 36, 16, 16, 16, 2);
		cursor_mask_image->byte_order = MSBFirst;
		cursor_mask_image->bitmap_bit_order = MSBFirst;
		cursor_map = XCreatePixmap(x_display, the_win, 16, 16, 1);
		cursor_mask_map = XCreatePixmap(x_display, the_win, 16, 16, 1);
		cursor_gc = XCreateGC(x_display, cursor_map, 0, 0);
		cursor_mask_gc = XCreateGC(x_display, cursor_mask_map, 0, 0);
		mac_cursor = XCreatePixmapCursor(x_display, cursor_map, cursor_mask_map, &black, &white, 0, 0);
		cursor_changed = false;
	}

	// Create no_cursor
	else {
		mac_cursor = XCreatePixmapCursor(x_display,
			XCreatePixmap(x_display, the_win, 1, 1, 1),
			XCreatePixmap(x_display, the_win, 1, 1, 1),
			&black, &white, 0, 0);
		XDefineCursor(x_display, the_win, mac_cursor);
	}

	// Init blitting routines
	bool native_byte_order;
#ifdef WORDS_BIGENDIAN
	native_byte_order = (XImageByteOrder(x_display) == MSBFirst);
#else
	native_byte_order = (XImageByteOrder(x_display) == LSBFirst);
#endif
#ifdef ENABLE_VOSF
	Screen_blitter_init(visualFormat, native_byte_order, depth);
#endif

	// Set bytes per row
	XSync(x_display, false);
	return true;
}

// Open DGA display (!! should use X11 VidMode extensions to set mode)
static bool open_dga(int width, int height)
{
#ifdef ENABLE_XF86_DGA
	// Set relative mouse mode
	ADBSetRelMouseMode(true);

	// Create window
	XSetWindowAttributes wattr;
	wattr.event_mask = eventmask = dga_eventmask;
	wattr.override_redirect = True;
	wattr.colormap = (depth == 1 ? DefaultColormap(x_display, screen) : cmap[0]);
	the_win = XCreateWindow(x_display, rootwin, 0, 0, width, height, 0, xdepth,
		InputOutput, vis, CWEventMask | CWOverrideRedirect |
		(color_class == DirectColor ? CWColormap : 0), &wattr);

	// Show window
	XMapRaised(x_display, the_win);
	wait_mapped(the_win);

#ifdef ENABLE_XF86_VIDMODE
	// Switch to best mode
	if (has_vidmode) {
		int best = 0;
		for (int i=1; i<num_x_video_modes; i++) {
			if (x_video_modes[i]->hdisplay >= width && x_video_modes[i]->vdisplay >= height &&
				x_video_modes[i]->hdisplay <= x_video_modes[best]->hdisplay && x_video_modes[i]->vdisplay <= x_video_modes[best]->vdisplay) {
				best = i;
			}
		}
		XF86VidModeSwitchToMode(x_display, screen, x_video_modes[best]);
		XF86VidModeSetViewPort(x_display, screen, 0, 0);
	}
#endif

	// Establish direct screen connection
	XMoveResizeWindow(x_display, the_win, 0, 0, width, height);
	XWarpPointer(x_display, None, rootwin, 0, 0, 0, 0, 0, 0);
	XGrabKeyboard(x_display, rootwin, True, GrabModeAsync, GrabModeAsync, CurrentTime);
	XGrabPointer(x_display, rootwin, True, PointerMotionMask | ButtonPressMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);

	int v_width, v_bank, v_size;
	XF86DGAGetVideo(x_display, screen, (char **)&the_buffer, &v_width, &v_bank, &v_size);
	XF86DGADirectVideo(x_display, screen, XF86DGADirectGraphics | XF86DGADirectKeyb | XF86DGADirectMouse);
	XF86DGASetViewPort(x_display, screen, 0, 0);
	XF86DGASetVidPage(x_display, screen, 0);

	// Set colormap
	if (!IsDirectMode(get_current_mode())) {
		XSetWindowColormap(x_display, the_win, cmap[current_dga_cmap = 0]);
		XF86DGAInstallColormap(x_display, screen, cmap[current_dga_cmap]);
	}
	XSync(x_display, false);

	// Init blitting routines
	int bytes_per_row = TrivialBytesPerRow((v_width + 7) & ~7, DepthModeForPixelDepth(depth));
#if ENABLE_VOSF
	bool native_byte_order;
#ifdef WORDS_BIGENDIAN
	native_byte_order = (XImageByteOrder(x_display) == MSBFirst);
#else
	native_byte_order = (XImageByteOrder(x_display) == LSBFirst);
#endif
#if REAL_ADDRESSING || DIRECT_ADDRESSING
	// Screen_blitter_init() returns TRUE if VOSF is mandatory
	// i.e. the framebuffer update function is not Blit_Copy_Raw
	use_vosf = Screen_blitter_init(visualFormat, native_byte_order, depth);
	
	if (use_vosf) {
	  // Allocate memory for frame buffer (SIZE is extended to page-boundary)
	  the_host_buffer = the_buffer;
	  the_buffer_size = page_extend((height + 2) * bytes_per_row);
	  the_buffer_copy = (uint8 *)malloc(the_buffer_size);
	  the_buffer = (uint8 *)vm_acquire(the_buffer_size);
	  D(bug("the_buffer = %p, the_buffer_copy = %p, the_host_buffer = %p\n", the_buffer, the_buffer_copy, the_host_buffer));
	}
#else
	use_vosf = false;
#endif
#endif

	// Set frame buffer base
	D(bug("the_buffer = %p, use_vosf = %d\n", the_buffer, use_vosf));
	screen_base = (uint32)the_buffer;
	VModes[cur_mode].viRowBytes = bytes_per_row;
	return true;
#else
	ErrorAlert("SheepShaver has been compiled with DGA support disabled.");
	return false;
#endif
}

static bool open_display(void)
{
	D(bug("open_display()\n"));
	const VideoInfo &mode = VModes[cur_mode];

	// Find best available X visual
	if (!find_visual_for_depth(mode.viAppleMode)) {
		ErrorAlert(GetString(STR_NO_XVISUAL_ERR));
		return false;
	}

	// Build up visualFormat structure
	visualFormat.depth = visualInfo.depth;
	visualFormat.Rmask = visualInfo.red_mask;
	visualFormat.Gmask = visualInfo.green_mask;
	visualFormat.Bmask = visualInfo.blue_mask;

	// Create color maps
	if (color_class == PseudoColor || color_class == DirectColor) {
		cmap[0] = XCreateColormap(x_display, rootwin, vis, AllocAll);
		cmap[1] = XCreateColormap(x_display, rootwin, vis, AllocAll);
	} else {
		cmap[0] = XCreateColormap(x_display, rootwin, vis, AllocNone);
		cmap[1] = XCreateColormap(x_display, rootwin, vis, AllocNone);
	}

	// Find pixel format of direct modes
	if (color_class == DirectColor || color_class == TrueColor) {
		rshift = gshift = bshift = 0;
		rloss = gloss = bloss = 8;
		uint32 mask;
		for (mask=vis->red_mask; !(mask&1); mask>>=1)
			++rshift;
		for (; mask&1; mask>>=1)
			--rloss;
		for (mask=vis->green_mask; !(mask&1); mask>>=1)
			++gshift;
		for (; mask&1; mask>>=1)
			--gloss;
		for (mask=vis->blue_mask; !(mask&1); mask>>=1)
			++bshift;
		for (; mask&1; mask>>=1)
			--bloss;
	}

	// Preset palette pixel values for CLUT or gamma table
	if (color_class == DirectColor) {
		int num = vis->map_entries;
		for (int i=0; i<num; i++) {
			int c = (i * 256) / num;
			x_palette[i].pixel = map_rgb(c, c, c);
			x_palette[i].flags = DoRed | DoGreen | DoBlue;
		}
	} else if (color_class == PseudoColor) {
		for (int i=0; i<256; i++) {
			x_palette[i].pixel = i;
			x_palette[i].flags = DoRed | DoGreen | DoBlue;
		}
	}

	// Load gray ramp to color map
	int num = (color_class == DirectColor ? vis->map_entries : 256);
	for (int i=0; i<num; i++) {
		int c = (i * 256) / num;
		x_palette[i].red = c * 0x0101;
		x_palette[i].green = c * 0x0101;
		x_palette[i].blue = c * 0x0101;
	}
	if (color_class == PseudoColor || color_class == DirectColor) {
		XStoreColors(x_display, cmap[0], x_palette, num);
		XStoreColors(x_display, cmap[1], x_palette, num);
	}

#ifdef ENABLE_VOSF
	// Load gray ramp to 8->16/32 expand map
	if (!IsDirectMode(get_current_mode()) && xdepth > 8)
		for (int i=0; i<256; i++)
			ExpandMap[i] = map_rgb(i, i, i);
#endif

	// Create display of requested type
	display_type = mode.viType;
	depth = depth_of_video_mode(mode.viAppleMode);

	bool display_open = false;
	if (display_type == DIS_SCREEN)
		display_open = open_dga(VModes[cur_mode].viXsize, VModes[cur_mode].viYsize);
	else if (display_type == DIS_WINDOW)
		display_open = open_window(VModes[cur_mode].viXsize, VModes[cur_mode].viYsize);

#ifdef ENABLE_VOSF
	if (use_vosf) {
		// Initialize the VOSF system
		if (!video_vosf_init()) {
			ErrorAlert(GetString(STR_VOSF_INIT_ERR));
			return false;
		}
	}
#endif
	
	return display_open;
}


/*
 *  Close display
 */

// Close window
static void close_window(void)
{
	if (have_shm) {
		XShmDetach(x_display, &shminfo);
#ifdef ENABLE_VOSF
		the_host_buffer = NULL;	// don't free() in driver_base dtor
#else
		the_buffer_copy = NULL; // don't free() in driver_base dtor
#endif
	}
	if (img) {
		if (!have_shm)
			img->data = NULL;
		XDestroyImage(img);
	}
	if (have_shm) {
		shmdt(shminfo.shmaddr);
		shmctl(shminfo.shmid, IPC_RMID, 0);
	}
	if (the_gc)
		XFreeGC(x_display, the_gc);

	XFlush(x_display);
	XSync(x_display, false);
}

// Close DGA mode
static void close_dga(void)
{
#ifdef ENABLE_XF86_DGA
	XF86DGADirectVideo(x_display, screen, 0);
	XUngrabPointer(x_display, CurrentTime);
	XUngrabKeyboard(x_display, CurrentTime);
#endif

#ifdef ENABLE_XF86_VIDMODE
	if (has_vidmode)
		XF86VidModeSwitchToMode(x_display, screen, x_video_modes[0]);
#endif

	if (!use_vosf) {
		// don't free() the screen buffer in driver_base dtor
		the_buffer = NULL;
	}
#ifdef ENABLE_VOSF
	else {
		// don't free() the screen buffer in driver_base dtor
		the_host_buffer = NULL;
	}
#endif
}

static void close_display(void)
{
	if (display_type == DIS_SCREEN)
		close_dga();
	else if (display_type == DIS_WINDOW)
		close_window();

	// Close window
	if (the_win) {
		XUnmapWindow(x_display, the_win);
		wait_unmapped(the_win);
		XDestroyWindow(x_display, the_win);
	}

	// Free colormaps
	if (cmap[0]) {
		XFreeColormap(x_display, cmap[0]);
		cmap[0] = 0;
	}
	if (cmap[1]) {
		XFreeColormap(x_display, cmap[1]);
		cmap[1] = 0;
	}

#ifdef ENABLE_VOSF
	if (use_vosf) {
		// Deinitialize VOSF
		video_vosf_exit();
	}
#endif

	// Free frame buffer(s)
	if (!use_vosf) {
		if (the_buffer_copy) {
			free(the_buffer_copy);
			the_buffer_copy = NULL;
		}
	}
#ifdef ENABLE_VOSF
	else {
		// the_buffer shall always be mapped through vm_acquire() so that we can vm_protect() it at will
		if (the_buffer != VM_MAP_FAILED) {
			D(bug(" releasing the_buffer at %p (%d bytes)\n", the_buffer, the_buffer_size));
			vm_release(the_buffer, the_buffer_size);
			the_buffer = NULL;
		}
		if (the_host_buffer) {
			D(bug(" freeing the_host_buffer at %p\n", the_host_buffer));
			free(the_host_buffer);
			the_host_buffer = NULL;
		}
		if (the_buffer_copy) {
			D(bug(" freeing the_buffer_copy at %p\n", the_buffer_copy));
			free(the_buffer_copy);
			the_buffer_copy = NULL;
		}
	}
#endif
}


/*
 *  Initialization
 */

// Init keycode translation table
static void keycode_init(void)
{
	bool use_kc = PrefsFindBool("keycodes");
	if (use_kc) {

		// Get keycode file path from preferences
		const char *kc_path = PrefsFindString("keycodefile");

		// Open keycode table
		FILE *f = fopen(kc_path ? kc_path : KEYCODE_FILE_NAME, "r");
		if (f == NULL) {
			char str[256];
			sprintf(str, GetString(STR_KEYCODE_FILE_WARN), kc_path ? kc_path : KEYCODE_FILE_NAME, strerror(errno));
			WarningAlert(str);
			return;
		}

		// Default translation table
		for (int i=0; i<256; i++)
			keycode_table[i] = -1;

		// Search for server vendor string, then read keycodes
		const char *vendor = ServerVendor(x_display);
#if (defined(__APPLE__) && defined(__MACH__))
		// Force use of MacX mappings on MacOS X with Apple's X server
		int dummy;
		if (XQueryExtension(x_display, "Apple-DRI", &dummy, &dummy, &dummy))
			vendor = "MacX";
#endif
		bool vendor_found = false;
		char line[256];
		while (fgets(line, 255, f)) {
			// Read line
			int len = strlen(line);
			if (len == 0)
				continue;
			line[len-1] = 0;

			// Comments begin with "#" or ";"
			if (line[0] == '#' || line[0] == ';' || line[0] == 0)
				continue;

			if (vendor_found) {
				// Read keycode
				int x_code, mac_code;
				if (sscanf(line, "%d %d", &x_code, &mac_code) == 2)
					keycode_table[x_code & 0xff] = mac_code;
				else
					break;
			} else {
				// Search for vendor string
				if (strstr(vendor, line) == vendor)
					vendor_found = true;
			}
		}

		// Keycode file completely read
		fclose(f);
		use_keycodes = vendor_found;

		// Vendor not found? Then display warning
		if (!vendor_found) {
			char str[256];
			sprintf(str, GetString(STR_KEYCODE_VENDOR_WARN), vendor, kc_path ? kc_path : KEYCODE_FILE_NAME);
			WarningAlert(str);
			return;
		}
	}
}

// Find Apple mode matching best specified dimensions
static int find_apple_resolution(int xsize, int ysize)
{
	int apple_id;
	if (xsize < 800)
		apple_id = APPLE_640x480;
	else if (xsize < 1024)
		apple_id = APPLE_800x600;
	else if (xsize < 1152)
		apple_id = APPLE_1024x768;
	else if (xsize < 1280) {
		if (ysize < 900)
			apple_id = APPLE_1152x768;
		else
			apple_id = APPLE_1152x900;
	}
	else if (xsize < 1600)
		apple_id = APPLE_1280x1024;
	else
		apple_id = APPLE_1600x1200;
	return apple_id;
}

// Find mode in list of supported modes
static int find_mode(int apple_mode, int apple_id, int type)
{
	for (VideoInfo *p = VModes; p->viType != DIS_INVALID; p++) {
		if (p->viType == type && p->viAppleID == apple_id && p->viAppleMode == apple_mode)
			return p - VModes;
	}
	return -1;
}

// Add mode to list of supported modes
static void add_mode(VideoInfo *&p, uint32 allow, uint32 test, int apple_mode, int apple_id, int type)
{
	if (allow & test) {
		p->viType = type;
		switch (apple_id) {
			case APPLE_W_640x480:
			case APPLE_640x480:
				p->viXsize = 640;
				p->viYsize = 480;
				break;
			case APPLE_W_800x600:
			case APPLE_800x600:
				p->viXsize = 800;
				p->viYsize = 600;
				break;
			case APPLE_1024x768:
				p->viXsize = 1024;
				p->viYsize = 768;
				break;
			case APPLE_1152x768:
				p->viXsize = 1152;
				p->viYsize = 768;
				break;
			case APPLE_1152x900:
				p->viXsize = 1152;
				p->viYsize = 900;
				break;
			case APPLE_1280x1024:
				p->viXsize = 1280;
				p->viYsize = 1024;
				break;
			case APPLE_1600x1200:
				p->viXsize = 1600;
				p->viYsize = 1200;
				break;
		}
		p->viRowBytes = TrivialBytesPerRow(p->viXsize, apple_mode);
		p->viAppleMode = apple_mode;
		p->viAppleID = apple_id;
		p++;
	}
}

// Add standard list of windowed modes for given color depth
static void add_window_modes(VideoInfo *&p, int window_modes, int mode)
{
	add_mode(p, window_modes, 1, mode, APPLE_W_640x480, DIS_WINDOW);
	add_mode(p, window_modes, 2, mode, APPLE_W_800x600, DIS_WINDOW);
}

static bool has_mode(int x, int y)
{
#ifdef ENABLE_XF86_VIDMODE
	for (int i=0; i<num_x_video_modes; i++)
		if (x_video_modes[i]->hdisplay >= x && x_video_modes[i]->vdisplay >= y)
			return true;
	return false;
#else
	return DisplayWidth(x_display, screen) >= x && DisplayHeight(x_display, screen) >= y;
#endif
}

bool VideoInit(void)
{
#ifdef ENABLE_VOSF
	// Zero the mainBuffer structure
	mainBuffer.dirtyPages = NULL;
	mainBuffer.pageInfo = NULL;
#endif
	
	// Check if X server runs on local machine
	local_X11 = (strncmp(XDisplayName(x_display_name), ":", 1) == 0)
	         || (strncmp(XDisplayName(x_display_name), "unix:", 5) == 0);
    
	// Init keycode translation
	keycode_init();

	// Read frame skip prefs
	frame_skip = PrefsFindInt32("frameskip");
	if (frame_skip == 0)
		frame_skip = 1;

	// Read mouse wheel prefs
	mouse_wheel_mode = PrefsFindInt32("mousewheelmode");
	mouse_wheel_lines = PrefsFindInt32("mousewheellines");

	// Init variables
	private_data = NULL;
	video_activated = true;

	// Find screen and root window
	screen = XDefaultScreen(x_display);
	rootwin = XRootWindow(x_display, screen);

	// Get sorted list of available depths
	avail_depths = XListDepths(x_display, screen, &num_depths);
	if (avail_depths == NULL) {
		ErrorAlert(GetString(STR_UNSUPP_DEPTH_ERR));
		return false;
	}
	sort(avail_depths, avail_depths + num_depths);
	
	// Get screen depth
	xdepth = DefaultDepth(x_display, screen);

#ifdef ENABLE_XF86_DGA
	// DGA available?
    int event_base, error_base;
    if (local_X11 && XF86DGAQueryExtension(x_display, &event_base, &error_base)) {
		int dga_flags = 0;
		XF86DGAQueryDirectVideo(x_display, screen, &dga_flags);
		has_dga = dga_flags & XF86DGADirectPresent;
	} else
		has_dga = false;
#endif

#ifdef ENABLE_XF86_VIDMODE
	// VidMode available?
	int vm_event_base, vm_error_base;
	has_vidmode = XF86VidModeQueryExtension(x_display, &vm_event_base, &vm_error_base);
	if (has_vidmode)
		XF86VidModeGetAllModeLines(x_display, screen, &num_x_video_modes, &x_video_modes);
#endif

	// Find black and white colors
	XParseColor(x_display, DefaultColormap(x_display, screen), "rgb:00/00/00", &black);
	XAllocColor(x_display, DefaultColormap(x_display, screen), &black);
	XParseColor(x_display, DefaultColormap(x_display, screen), "rgb:ff/ff/ff", &white);
	XAllocColor(x_display, DefaultColormap(x_display, screen), &white);
	black_pixel = BlackPixel(x_display, screen);
	white_pixel = WhitePixel(x_display, screen);

	// Mac screen depth follows X depth (for now)
	int default_mode = APPLE_8_BIT;
	switch (DefaultDepth(x_display, screen)) {
	case 1:
		default_mode = APPLE_1_BIT;
		break;
	case 8:
		default_mode = APPLE_8_BIT;
		break;
	case 15: case 16:
		default_mode = APPLE_16_BIT;
		break;
	case 24: case 32:
		default_mode = APPLE_32_BIT;
		break;
	}

	// Construct video mode table
	uint32 window_modes = PrefsFindInt32("windowmodes");
	uint32 screen_modes = PrefsFindInt32("screenmodes");
	if (!has_dga)
		screen_modes = 0;
	if (window_modes == 0 && screen_modes == 0)
		window_modes |= 3;	// Allow at least 640x480 and 800x600 window modes

	VideoInfo *p = VModes;
	for (unsigned int d = APPLE_1_BIT; d <= APPLE_32_BIT; d++)
		if (find_visual_for_depth(d))
			add_window_modes(p, window_modes, d);

	if (has_vidmode) {
		if (has_mode(640, 480))
			add_mode(p, screen_modes, 1, default_mode, APPLE_640x480, DIS_SCREEN);
		if (has_mode(800, 600))
			add_mode(p, screen_modes, 2, default_mode, APPLE_800x600, DIS_SCREEN);
		if (has_mode(1024, 768))
			add_mode(p, screen_modes, 4, default_mode, APPLE_1024x768, DIS_SCREEN);
		if (has_mode(1152, 768))
			add_mode(p, screen_modes, 64, default_mode, APPLE_1152x768, DIS_SCREEN);
		if (has_mode(1152, 900))
			add_mode(p, screen_modes, 8, default_mode, APPLE_1152x900, DIS_SCREEN);
		if (has_mode(1280, 1024))
			add_mode(p, screen_modes, 16, default_mode, APPLE_1280x1024, DIS_SCREEN);
		if (has_mode(1600, 1200))
			add_mode(p, screen_modes, 32, default_mode, APPLE_1600x1200, DIS_SCREEN);
	} else if (screen_modes) {
		int xsize = DisplayWidth(x_display, screen);
		int ysize = DisplayHeight(x_display, screen);
		int apple_id = find_apple_resolution(xsize, ysize);
		p->viType = DIS_SCREEN;
		p->viRowBytes = 0;
		p->viXsize = xsize;
		p->viYsize = ysize;
		p->viAppleMode = default_mode;
		p->viAppleID = apple_id;
		p++;
	}
	p->viType = DIS_INVALID;	// End marker
	p->viRowBytes = 0;
	p->viXsize = p->viYsize = 0;
	p->viAppleMode = 0;
	p->viAppleID = 0;

	// Find default mode (window 640x480)
	cur_mode = -1;
	if (has_dga && screen_modes) {
		int screen_width = DisplayWidth(x_display, screen);
		int screen_height = DisplayHeight(x_display, screen);
		int apple_id = find_apple_resolution(screen_width, screen_height);
		if (apple_id != -1)
			cur_mode = find_mode(default_mode, apple_id, DIS_SCREEN);
	}
	if (cur_mode == -1) {
		// pick up first windowed mode available
		for (VideoInfo *p = VModes; p->viType != DIS_INVALID; p++) {
			if (p->viType == DIS_WINDOW && p->viAppleMode == default_mode) {
				cur_mode = p - VModes;
				break;
			}
		}
	}
	assert(cur_mode != -1);

#if DEBUG
	D(bug("Available video modes:\n"));
	for (p = VModes; p->viType != DIS_INVALID; p++) {
		int bits = depth_of_video_mode(p->viAppleMode);
		D(bug(" %dx%d (ID %02x), %d colors\n", p->viXsize, p->viYsize, p->viAppleID, 1 << bits));
	}
#endif

	// Open window/screen
	if (!open_display())
		return false;

#if 0
	// Ignore errors from now on
	XSetErrorHandler(ignore_errors);
#endif

	// Start periodic thread
	XSync(x_display, false);
	Set_pthread_attr(&redraw_thread_attr, 0);
	redraw_thread_cancel = false;
	redraw_thread_active = (pthread_create(&redraw_thread, &redraw_thread_attr, redraw_func, NULL) == 0);
	D(bug("Redraw thread installed (%ld)\n", redraw_thread));
	return true;
}


/*
 *  Deinitialization
 */

void VideoExit(void)
{
	// Stop redraw thread
	if (redraw_thread_active) {
		redraw_thread_cancel = true;
		pthread_cancel(redraw_thread);
		pthread_join(redraw_thread, NULL);
		redraw_thread_active = false;
	}

#ifdef ENABLE_VOSF
	if (use_vosf) {
		// Deinitialize VOSF
		video_vosf_exit();
	}
#endif

	// Close window and server connection
	if (x_display != NULL) {
		XSync(x_display, false);
		close_display();
		XFlush(x_display);
		XSync(x_display, false);
	}
}


/*
 *  Suspend/resume emulator
 */

extern void PauseEmulator(void);
extern void ResumeEmulator(void);

static void suspend_emul(void)
{
	if (display_type == DIS_SCREEN) {
		// Release ctrl key
		ADBKeyUp(0x36);
		ctrl_down = false;

		// Pause MacOS thread
		PauseEmulator();
		emul_suspended = true;

		// Save frame buffer
		fb_save = malloc(VModes[cur_mode].viYsize * VModes[cur_mode].viRowBytes);
		if (fb_save)
			memcpy(fb_save, (void *)screen_base, VModes[cur_mode].viYsize * VModes[cur_mode].viRowBytes);

		// Close full screen display
#ifdef ENABLE_XF86_DGA
		XF86DGADirectVideo(x_display, screen, 0);
		XUngrabPointer(x_display, CurrentTime);
		XUngrabKeyboard(x_display, CurrentTime);
#endif
		XSync(x_display, false);

		// Open "suspend" window
		XSetWindowAttributes wattr;
		wattr.event_mask = KeyPressMask;
		wattr.background_pixel = black_pixel;
		wattr.border_pixel = black_pixel;
		wattr.backing_store = Always;
		wattr.backing_planes = xdepth;
		wattr.colormap = DefaultColormap(x_display, screen);
		XSync(x_display, false);
		suspend_win = XCreateWindow(x_display, rootwin, 0, 0, 512, 1, 0, xdepth,
			InputOutput, vis, CWEventMask | CWBackPixel | CWBorderPixel |
			CWBackingStore | CWBackingPlanes | (xdepth == 8 ? CWColormap : 0), &wattr);
		XSync(x_display, false);
		XStoreName(x_display, suspend_win, GetString(STR_SUSPEND_WINDOW_TITLE));
		XMapRaised(x_display, suspend_win);
		XSync(x_display, false);
	}
}

static void resume_emul(void)
{
	// Close "suspend" window
	XDestroyWindow(x_display, suspend_win);
	XSync(x_display, false);

	// Reopen full screen display
	XGrabKeyboard(x_display, rootwin, 1, GrabModeAsync, GrabModeAsync, CurrentTime);
	XGrabPointer(x_display, rootwin, 1, PointerMotionMask | ButtonPressMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
#ifdef ENABLE_XF86_DGA
	XF86DGADirectVideo(x_display, screen, XF86DGADirectGraphics | XF86DGADirectKeyb | XF86DGADirectMouse);
	XF86DGASetViewPort(x_display, screen, 0, 0);
#endif
	XSync(x_display, false);

	// the_buffer already contains the data to restore. i.e. since a temporary
	// frame buffer is used when VOSF is actually used, fb_save is therefore
	// not necessary.
#ifdef ENABLE_VOSF
	if (use_vosf) {
		LOCK_VOSF;
		PFLAG_SET_ALL;
		UNLOCK_VOSF;
		memset(the_buffer_copy, 0, VModes[cur_mode].viRowBytes * VModes[cur_mode].viYsize);
	}
#endif
	
	// Restore frame buffer
	if (fb_save) {
#ifdef ENABLE_VOSF
		// Don't copy fb_save to the temporary frame buffer in VOSF mode
		if (!use_vosf)
#endif
		memcpy((void *)screen_base, fb_save, VModes[cur_mode].viYsize * VModes[cur_mode].viRowBytes);
		free(fb_save);
		fb_save = NULL;
	}
	if (depth == 8)
		palette_changed = true;

	// Resume MacOS thread
	emul_suspended = false;
	ResumeEmulator();
}


/*
 *  Close screen in full-screen mode
 */

void VideoQuitFullScreen(void)
{
	D(bug("VideoQuitFullScreen()\n"));
	if (display_type == DIS_SCREEN) {
		quit_full_screen = true;
		while (!quit_full_screen_ack) ;
	}
}


/*
 *  X11 event handling
 */

// Translate key event to Mac keycode
static int kc_decode(KeySym ks)
{
	switch (ks) {
		case XK_A: case XK_a: return 0x00;
		case XK_B: case XK_b: return 0x0b;
		case XK_C: case XK_c: return 0x08;
		case XK_D: case XK_d: return 0x02;
		case XK_E: case XK_e: return 0x0e;
		case XK_F: case XK_f: return 0x03;
		case XK_G: case XK_g: return 0x05;
		case XK_H: case XK_h: return 0x04;
		case XK_I: case XK_i: return 0x22;
		case XK_J: case XK_j: return 0x26;
		case XK_K: case XK_k: return 0x28;
		case XK_L: case XK_l: return 0x25;
		case XK_M: case XK_m: return 0x2e;
		case XK_N: case XK_n: return 0x2d;
		case XK_O: case XK_o: return 0x1f;
		case XK_P: case XK_p: return 0x23;
		case XK_Q: case XK_q: return 0x0c;
		case XK_R: case XK_r: return 0x0f;
		case XK_S: case XK_s: return 0x01;
		case XK_T: case XK_t: return 0x11;
		case XK_U: case XK_u: return 0x20;
		case XK_V: case XK_v: return 0x09;
		case XK_W: case XK_w: return 0x0d;
		case XK_X: case XK_x: return 0x07;
		case XK_Y: case XK_y: return 0x10;
		case XK_Z: case XK_z: return 0x06;

		case XK_1: case XK_exclam: return 0x12;
		case XK_2: case XK_at: return 0x13;
		case XK_3: case XK_numbersign: return 0x14;
		case XK_4: case XK_dollar: return 0x15;
		case XK_5: case XK_percent: return 0x17;
		case XK_6: return 0x16;
		case XK_7: return 0x1a;
		case XK_8: return 0x1c;
		case XK_9: return 0x19;
		case XK_0: return 0x1d;

		case XK_grave: case XK_asciitilde: return 0x0a;
		case XK_minus: case XK_underscore: return 0x1b;
		case XK_equal: case XK_plus: return 0x18;
		case XK_bracketleft: case XK_braceleft: return 0x21;
		case XK_bracketright: case XK_braceright: return 0x1e;
		case XK_backslash: case XK_bar: return 0x2a;
		case XK_semicolon: case XK_colon: return 0x29;
		case XK_apostrophe: case XK_quotedbl: return 0x27;
		case XK_comma: case XK_less: return 0x2b;
		case XK_period: case XK_greater: return 0x2f;
		case XK_slash: case XK_question: return 0x2c;

		case XK_Tab: if (ctrl_down) {suspend_emul(); return -1;} else return 0x30;
		case XK_Return: return 0x24;
		case XK_space: return 0x31;
		case XK_BackSpace: return 0x33;

		case XK_Delete: return 0x75;
		case XK_Insert: return 0x72;
		case XK_Home: case XK_Help: return 0x73;
		case XK_End: return 0x77;
#ifdef __hpux
		case XK_Prior: return 0x74;
		case XK_Next: return 0x79;
#else
		case XK_Page_Up: return 0x74;
		case XK_Page_Down: return 0x79;
#endif

		case XK_Control_L: return 0x36;
		case XK_Control_R: return 0x36;
		case XK_Shift_L: return 0x38;
		case XK_Shift_R: return 0x38;
		case XK_Alt_L: return 0x37;
		case XK_Alt_R: return 0x37;
		case XK_Meta_L: return 0x3a;
		case XK_Meta_R: return 0x3a;
		case XK_Menu: return 0x32;
		case XK_Caps_Lock: return 0x39;
		case XK_Num_Lock: return 0x47;

		case XK_Up: return 0x3e;
		case XK_Down: return 0x3d;
		case XK_Left: return 0x3b;
		case XK_Right: return 0x3c;

		case XK_Escape: if (ctrl_down) {quit_full_screen = true; emerg_quit = true; return -1;} else return 0x35;

		case XK_F1: if (ctrl_down) {SysMountFirstFloppy(); return -1;} else return 0x7a;
		case XK_F2: return 0x78;
		case XK_F3: return 0x63;
		case XK_F4: return 0x76;
		case XK_F5: return 0x60;
		case XK_F6: return 0x61;
		case XK_F7: return 0x62;
		case XK_F8: return 0x64;
		case XK_F9: return 0x65;
		case XK_F10: return 0x6d;
		case XK_F11: return 0x67;
		case XK_F12: return 0x6f;

		case XK_Print: return 0x69;
		case XK_Scroll_Lock: return 0x6b;
		case XK_Pause: return 0x71;

#if defined(XK_KP_Prior) && defined(XK_KP_Left) && defined(XK_KP_Insert) && defined (XK_KP_End)
		case XK_KP_0: case XK_KP_Insert: return 0x52;
		case XK_KP_1: case XK_KP_End: return 0x53;
		case XK_KP_2: case XK_KP_Down: return 0x54;
		case XK_KP_3: case XK_KP_Next: return 0x55;
		case XK_KP_4: case XK_KP_Left: return 0x56;
		case XK_KP_5: case XK_KP_Begin: return 0x57;
		case XK_KP_6: case XK_KP_Right: return 0x58;
		case XK_KP_7: case XK_KP_Home: return 0x59;
		case XK_KP_8: case XK_KP_Up: return 0x5b;
		case XK_KP_9: case XK_KP_Prior: return 0x5c;
		case XK_KP_Decimal: case XK_KP_Delete: return 0x41;
#else
		case XK_KP_0: return 0x52;
		case XK_KP_1: return 0x53;
		case XK_KP_2: return 0x54;
		case XK_KP_3: return 0x55;
		case XK_KP_4: return 0x56;
		case XK_KP_5: return 0x57;
		case XK_KP_6: return 0x58;
		case XK_KP_7: return 0x59;
		case XK_KP_8: return 0x5b;
		case XK_KP_9: return 0x5c;
		case XK_KP_Decimal: return 0x41;
#endif
		case XK_KP_Add: return 0x45;
		case XK_KP_Subtract: return 0x4e;
		case XK_KP_Multiply: return 0x43;
		case XK_KP_Divide: return 0x4b;
		case XK_KP_Enter: return 0x4c;
		case XK_KP_Equal: return 0x51;
	}
	return -1;
}

static int event2keycode(XKeyEvent &ev, bool key_down)
{
	KeySym ks;
	int i = 0;

	do {
		ks = XLookupKeysym(&ev, i++);
		int as = kc_decode(ks);
		if (as >= 0)
			return as;
		if (as == -2)
			return as;
	} while (ks != NoSymbol);

	return -1;
}

static void handle_events(void)
{
	// Handle events
	for (;;) {
		XEvent event;

		XDisplayLock();
		if (!XCheckMaskEvent(x_display, eventmask, &event)) {
			// Handle clipboard events
			if (XCheckTypedEvent(x_display, SelectionRequest, &event))
				ClipboardSelectionRequest(&event.xselectionrequest);
			else if (XCheckTypedEvent(x_display, SelectionClear, &event))
				ClipboardSelectionClear(&event.xselectionclear);

			// Window "close" widget clicked
			else if (XCheckTypedEvent(x_display, ClientMessage, &event)) {
				if (event.xclient.format == 32 && event.xclient.data.l[0] == WM_DELETE_WINDOW) {
					ADBKeyDown(0x7f);	// Power key
					ADBKeyUp(0x7f);
				}
			}

			XDisplayUnlock();
			break;
		}
		XDisplayUnlock();

		switch (event.type) {
			// Mouse button
			case ButtonPress: {
				unsigned int button = ((XButtonEvent *)&event)->button;
				if (button < 4)
					ADBMouseDown(button - 1);
				else if (button < 6) {	// Wheel mouse
					if (mouse_wheel_mode == 0) {
						int key = (button == 5) ? 0x79 : 0x74;	// Page up/down
						ADBKeyDown(key);
						ADBKeyUp(key);
					} else {
						int key = (button == 5) ? 0x3d : 0x3e;	// Cursor up/down
						for(int i=0; i<mouse_wheel_lines; i++) {
							ADBKeyDown(key);
							ADBKeyUp(key);
						}
					}
				}
				break;
			}
			case ButtonRelease: {
				unsigned int button = ((XButtonEvent *)&event)->button;
				if (button < 4)
					ADBMouseUp(button - 1);
				break;
			}

			// Mouse entered window
			case EnterNotify:
				if (event.xcrossing.mode != NotifyGrab && event.xcrossing.mode != NotifyUngrab)
					ADBMouseMoved(event.xmotion.x, event.xmotion.y);
				break;

			// Mouse moved
			case MotionNotify:
				ADBMouseMoved(event.xmotion.x, event.xmotion.y);
				break;

			// Keyboard
			case KeyPress: {
				int code = -1;
				if (use_keycodes) {
					if (event2keycode(event.xkey, true) != -2)	// This is called to process the hotkeys
						code = keycode_table[event.xkey.keycode & 0xff];
				} else
					code = event2keycode(event.xkey, true);
				if (code >= 0) {
					if (!emul_suspended) {
						if (code == 0x39) {	// Caps Lock pressed
							if (caps_on) {
								ADBKeyUp(code);
								caps_on = false;
							} else {
								ADBKeyDown(code);
								caps_on = true;
							}
						} else
							ADBKeyDown(code);
						if (code == 0x36)
							ctrl_down = true;
					} else {
						if (code == 0x31)
							resume_emul();	// Space wakes us up
					}
				}
				break;
			}
			case KeyRelease: {
				int code = -1;
				if (use_keycodes) {
					if (event2keycode(event.xkey, false) != -2)	// This is called to process the hotkeys
						code = keycode_table[event.xkey.keycode & 0xff];
				} else
					code = event2keycode(event.xkey, false);
				if (code >= 0 && code != 0x39) {	// Don't propagate Caps Lock releases
					ADBKeyUp(code);
					if (code == 0x36)
						ctrl_down = false;
				}
				break;
			}

			// Hidden parts exposed, force complete refresh
			case Expose:
#ifdef ENABLE_VOSF
				if (use_vosf) {			// VOSF refresh
					LOCK_VOSF;
					PFLAG_SET_ALL;
					UNLOCK_VOSF;
				}
#endif
				memset(the_buffer_copy, 0, VModes[cur_mode].viRowBytes * VModes[cur_mode].viYsize);
				break;
		}
	}
}


/*
 *  Execute video VBL routine
 */

void VideoVBL(void)
{
	if (emerg_quit)
		QuitEmulator();

	// Execute video VBL
	if (private_data != NULL && private_data->interruptsEnabled)
		VSLDoInterruptService(private_data->vslServiceID);
}


/*
 *  Change video mode
 */

int16 video_mode_change(VidLocals *csSave, uint32 ParamPtr)
{
	/* return if no mode change */
	if ((csSave->saveData == ReadMacInt32(ParamPtr + csData)) &&
	    (csSave->saveMode == ReadMacInt16(ParamPtr + csMode))) return noErr;

	/* first find video mode in table */
	for (int i=0; VModes[i].viType != DIS_INVALID; i++) {
		if ((ReadMacInt16(ParamPtr + csMode) == VModes[i].viAppleMode) &&
		    (ReadMacInt32(ParamPtr + csData) == VModes[i].viAppleID)) {
			csSave->saveMode = ReadMacInt16(ParamPtr + csMode);
			csSave->saveData = ReadMacInt32(ParamPtr + csData);
			csSave->savePage = ReadMacInt16(ParamPtr + csPage);

			// Disable interrupts and pause redraw thread
			DisableInterrupt();
			thread_stop_ack = false;
			thread_stop_req = true;
			while (!thread_stop_ack) ;

			/* close old display */
			close_display();

			/* open new display */
			cur_mode = i;
			bool ok = open_display();

			/* opening the screen failed? Then bail out */
			if (!ok) {
				ErrorAlert(GetString(STR_FULL_SCREEN_ERR));
				QuitEmulator();
			}

			WriteMacInt32(ParamPtr + csBaseAddr, screen_base);
			csSave->saveBaseAddr=screen_base;
			csSave->saveData=VModes[cur_mode].viAppleID;/* First mode ... */
			csSave->saveMode=VModes[cur_mode].viAppleMode;

			// Enable interrupts and resume redraw thread
			thread_stop_req = false;
			EnableInterrupt();
			return noErr;
		}
	}
	return paramErr;
}


/*
 *  Set color palette
 */

void video_set_palette(void)
{
	LOCK_PALETTE;

	// Convert colors to XColor array
	int mode = get_current_mode();
	int num_in = palette_size(mode);
	int num_out = 256;
	bool stretch = false;
	if (IsDirectMode(mode)) {
		// If X is in 565 mode we have to stretch the gamma table from 32 to 64 entries
		num_out = vis->map_entries;
		stretch = true;
	}
	XColor *p = x_palette;
	for (int i=0; i<num_out; i++) {
		int c = (stretch ? (i * num_in) / num_out : i);
		p->red = mac_pal[c].red * 0x0101;
		p->green = mac_pal[c].green * 0x0101;
		p->blue = mac_pal[c].blue * 0x0101;
		p++;
	}

#ifdef ENABLE_VOSF
	// Recalculate pixel color expansion map
	if (!IsDirectMode(mode) && xdepth > 8) {
		for (int i=0; i<256; i++) {
			int c = i & (num_in-1); // If there are less than 256 colors, we repeat the first entries (this makes color expansion easier)
			ExpandMap[i] = map_rgb(mac_pal[c].red, mac_pal[c].green, mac_pal[c].blue);
		}

		// We have to redraw everything because the interpretation of pixel values changed
		LOCK_VOSF;
		PFLAG_SET_ALL;
		UNLOCK_VOSF;
		memset(the_buffer_copy, 0, VModes[cur_mode].viRowBytes * VModes[cur_mode].viYsize);
	}
#endif

	// Tell redraw thread to change palette
	palette_changed = true;

	UNLOCK_PALETTE;
}


/*
 *  Can we set the MacOS cursor image into the window?
 */

bool video_can_change_cursor(void)
{
	return hw_mac_cursor_accl && (display_type != DIS_SCREEN);
}


/*
 *  Set cursor image for window
 */

void video_set_cursor(void)
{
	cursor_changed = true;
}


/*
 *  Thread for window refresh, event handling and other periodic actions
 */

static void update_display(void)
{
	// Incremental update code
	int wide = 0, high = 0, x1, x2, y1, y2, i, j;
	int bytes_per_row = VModes[cur_mode].viRowBytes;
	int bytes_per_pixel = VModes[cur_mode].viRowBytes / VModes[cur_mode].viXsize;
	uint8 *p, *p2;

	// Check for first line from top and first line from bottom that have changed
	y1 = 0;
	for (j=0; j<VModes[cur_mode].viYsize; j++) {
		if (memcmp(&the_buffer[j * bytes_per_row], &the_buffer_copy[j * bytes_per_row], bytes_per_row)) {
			y1 = j;
			break;
		}
	}
	y2 = y1 - 1;
	for (j=VModes[cur_mode].viYsize-1; j>=y1; j--) {
		if (memcmp(&the_buffer[j * bytes_per_row], &the_buffer_copy[j * bytes_per_row], bytes_per_row)) {
			y2 = j;
			break;
		}
	}
	high = y2 - y1 + 1;

	// Check for first column from left and first column from right that have changed
	if (high) {
		if (depth == 1) {
			x1 = VModes[cur_mode].viXsize;
			for (j=y1; j<=y2; j++) {
				p = &the_buffer[j * bytes_per_row];
				p2 = &the_buffer_copy[j * bytes_per_row];
				for (i=0; i<(x1>>3); i++) {
					if (*p != *p2) {
						x1 = i << 3;
						break;
					}
					p++;
					p2++;
				}
			}
			x2 = x1;
			for (j=y1; j<=y2; j++) {
				p = &the_buffer[j * bytes_per_row];
				p2 = &the_buffer_copy[j * bytes_per_row];
				p += bytes_per_row;
				p2 += bytes_per_row;
				for (i=(VModes[cur_mode].viXsize>>3); i>(x2>>3); i--) {
					p--;
					p2--;
					if (*p != *p2) {
						x2 = i << 3;
						break;
					}
				}
			}
			wide = x2 - x1;

			// Update copy of the_buffer
			if (high && wide) {
				for (j=y1; j<=y2; j++) {
					i = j * bytes_per_row + (x1 >> 3);
					memcpy(&the_buffer_copy[i], &the_buffer[i], wide >> 3);
				}
			}

		} else {
			x1 = VModes[cur_mode].viXsize;
			for (j=y1; j<=y2; j++) {
				p = &the_buffer[j * bytes_per_row];
				p2 = &the_buffer_copy[j * bytes_per_row];
				for (i=0; i<x1; i++) {
					if (memcmp(p, p2, bytes_per_pixel)) {
						x1 = i;
						break;
					}
					p += bytes_per_pixel;
					p2 += bytes_per_pixel;
				}
			}
			x2 = x1;
			for (j=y1; j<=y2; j++) {
				p = &the_buffer[j * bytes_per_row];
				p2 = &the_buffer_copy[j * bytes_per_row];
				p += bytes_per_row;
				p2 += bytes_per_row;
				for (i=VModes[cur_mode].viXsize; i>x2; i--) {
					p -= bytes_per_pixel;
					p2 -= bytes_per_pixel;
					if (memcmp(p, p2, bytes_per_pixel)) {
						x2 = i;
						break;
					}
				}
			}
			wide = x2 - x1;

			// Update copy of the_buffer
			if (high && wide) {
				for (j=y1; j<=y2; j++) {
					i = j * bytes_per_row + x1 * bytes_per_pixel;
					memcpy(&the_buffer_copy[i], &the_buffer[i], bytes_per_pixel * wide);
				}
			}
		}
	}

	// Refresh display
	if (high && wide) {
		XDisplayLock();
		if (have_shm)
			XShmPutImage(x_display, the_win, the_gc, img, x1, y1, x1, y1, wide, high, 0);
		else
			XPutImage(x_display, the_win, the_gc, img, x1, y1, x1, y1, wide, high);
		XDisplayUnlock();
	}
}

const int VIDEO_REFRESH_HZ = 60;
const int VIDEO_REFRESH_DELAY = 1000000 / VIDEO_REFRESH_HZ;

static void handle_palette_changes(void)
{
	LOCK_PALETTE;

	if (palette_changed && !emul_suspended) {
		palette_changed = false;

		int mode = get_current_mode();
		if (color_class == PseudoColor || color_class == DirectColor) {
			int num = vis->map_entries;
			bool set_clut = true;
			if (!IsDirectMode(mode) && color_class == DirectColor) {
				if (display_type == DIS_WINDOW)
					set_clut = false; // Indexed mode on true color screen, don't set CLUT
			}

			if (set_clut) {
				XDisplayLock();
				XStoreColors(x_display, cmap[0], x_palette, num);
				XStoreColors(x_display, cmap[1], x_palette, num);
				XSync(x_display, false);
				XDisplayUnlock();
			}
		}

#ifdef ENABLE_XF86_DGA
		if (display_type == DIS_SCREEN) {
			current_dga_cmap ^= 1;
			if (!IsDirectMode(mode) && cmap[current_dga_cmap])
				XF86DGAInstallColormap(x_display, screen, cmap[current_dga_cmap]);
		}
#endif
	}

	UNLOCK_PALETTE;
}

static void *redraw_func(void *arg)
{
	int fd = ConnectionNumber(x_display);

	uint64 start = GetTicks_usec();
	int64 ticks = 0;
	uint64 next = GetTicks_usec() + VIDEO_REFRESH_DELAY;

	while (!redraw_thread_cancel) {

		// Pause if requested (during video mode switches)
		while (thread_stop_req)
			thread_stop_ack = true;

		int64 delay = next - GetTicks_usec();
		if (delay < -VIDEO_REFRESH_DELAY) {

			// We are lagging far behind, so we reset the delay mechanism
			next = GetTicks_usec();

		} else if (delay <= 0) {

			// Delay expired, refresh display
			next += VIDEO_REFRESH_DELAY;
			ticks++;

			// Handle X11 events
			handle_events();

			// Quit DGA mode if requested
			if (quit_full_screen) {
				quit_full_screen = false;
				if (display_type == DIS_SCREEN) {
					XDisplayLock();
#ifdef ENABLE_XF86_DGA
					XF86DGADirectVideo(x_display, screen, 0);
					XUngrabPointer(x_display, CurrentTime);
					XUngrabKeyboard(x_display, CurrentTime);
					XUnmapWindow(x_display, the_win);
					wait_unmapped(the_win);
					XDestroyWindow(x_display, the_win);
#endif
					XSync(x_display, false);
					XDisplayUnlock();
					quit_full_screen_ack = true;
					return NULL;
				}
			}

			// Refresh display and set cursor image in window mode
			static int tick_counter = 0;
			if (display_type == DIS_WINDOW) {
				tick_counter++;
				if (tick_counter >= frame_skip) {
					tick_counter = 0;

					// Update display
#ifdef ENABLE_VOSF
					if (use_vosf) {
						XDisplayLock();
						if (mainBuffer.dirty) {
							LOCK_VOSF;
							update_display_window_vosf();
							UNLOCK_VOSF;
							XSync(x_display, false); // Let the server catch up
						}
						XDisplayUnlock();
					}
					else
#endif
						update_display();

					// Set new cursor image if it was changed
					if (hw_mac_cursor_accl && cursor_changed) {
						cursor_changed = false;
						uint8 *x_data = (uint8 *)cursor_image->data;
						uint8 *x_mask = (uint8 *)cursor_mask_image->data;
						for (int i = 0; i < 32; i++) {
							x_mask[i] = MacCursor[4 + i] | MacCursor[36 + i];
							x_data[i] = MacCursor[4 + i];
						}
						XDisplayLock();
						XFreeCursor(x_display, mac_cursor);
						XPutImage(x_display, cursor_map, cursor_gc, cursor_image, 0, 0, 0, 0, 16, 16);
						XPutImage(x_display, cursor_mask_map, cursor_mask_gc, cursor_mask_image, 0, 0, 0, 0, 16, 16);
						mac_cursor = XCreatePixmapCursor(x_display, cursor_map, cursor_mask_map, &black, &white, MacCursor[2], MacCursor[3]);
						XDefineCursor(x_display, the_win, mac_cursor);
						XDisplayUnlock();
					}
				}
			}
#ifdef ENABLE_VOSF
			else if (use_vosf) {
				// Update display (VOSF variant)
				if (++tick_counter >= frame_skip) {
					tick_counter = 0;
					if (mainBuffer.dirty) {
						LOCK_VOSF;
						update_display_dga_vosf();
						UNLOCK_VOSF;
					}
				}
			}
#endif

			// Set new palette if it was changed
			handle_palette_changes();

		} else {

			// No display refresh pending, check for X events
			fd_set readfds;
			FD_ZERO(&readfds);
			FD_SET(fd, &readfds);
			struct timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = delay;
			if (select(fd+1, &readfds, NULL, NULL, &timeout) > 0)
				handle_events();
		}
	}
	return NULL;
}
