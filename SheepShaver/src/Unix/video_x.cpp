/*
 *  video_x.cpp - Video/graphics emulation, X11 specific stuff
 *
 *  SheepShaver (C) 1997-2002 Marc Hellwig and Christian Bauer
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

#ifdef ENABLE_XF86_DGA
#include <X11/extensions/xf86dga.h>
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

#define DEBUG 0
#include "debug.h"


// Constants
const char KEYCODE_FILE_NAME[] = DATADIR "/keycodes";

// Global variables
static int32 frame_skip;
static bool redraw_thread_active = false;	// Flag: Redraw thread installed
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
static XVisualInfo visualInfo;
static Visual *vis;
static Colormap cmap[2];					// Two colormaps (DGA) for 8-bit mode
static XColor black, white;
static unsigned long black_pixel, white_pixel;
static int eventmask;
static const int win_eventmask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | EnterWindowMask | ExposureMask;
static const int dga_eventmask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;

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
static char *dga_screen_base;
static int dga_fb_width;
static int current_dga_cmap;

#ifdef ENABLE_XF86_VIDMODE
// Variables for XF86 VidMode support
static XF86VidModeModeInfo **x_video_modes;		// Array of all available modes
static int num_x_video_modes;
#endif


// Prototypes
static void *redraw_func(void *arg);


// From main_unix.cpp
extern char *x_display_name;
extern Display *x_display;

// From sys_unix.cpp
extern void SysMountFirstFloppy(void);


// Video acceleration through SIGSEGV
#ifdef ENABLE_VOSF
# include "video_vosf.h"
#endif


/*
 *  Open display (window or fullscreen)
 */

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

	// Read frame skip prefs
	frame_skip = PrefsFindInt32("frameskip");
	if (frame_skip == 0)
		frame_skip = 1;

	// Create window
	XSetWindowAttributes wattr;
	wattr.event_mask = eventmask = win_eventmask;
	wattr.background_pixel = black_pixel;
	wattr.border_pixel = black_pixel;
	wattr.backing_store = NotUseful;

	XSync(x_display, false);
	the_win = XCreateWindow(x_display, rootwin, 0, 0, width, height, 0, xdepth,
		InputOutput, vis, CWEventMask | CWBackPixel | CWBorderPixel | CWBackingStore, &wattr);
	XSync(x_display, false);
	XStoreName(x_display, the_win, GetString(STR_WINDOW_TITLE));
	XMapRaised(x_display, the_win);
	XSync(x_display, false);

	// Set colormap
	if (depth == 8) {
		XSetWindowColormap(x_display, the_win, cmap[0]);
		XSetWMColormapWindows(x_display, the_win, &the_win, 1);
	}

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

	// 1-bit mode is big-endian; if the X server is little-endian, we can't
	// use SHM because that doesn't allow changing the image byte order
	bool need_msb_image = (depth == 1 && XImageByteOrder(x_display) == LSBFirst);

	// Try to create and attach SHM image
	have_shm = false;
	if (local_X11 && !need_msb_image && XShmQueryExtension(x_display)) {

		// Create SHM image ("height + 2" for safety)
		img = XShmCreateImage(x_display, vis, depth == 1 ? 1 : xdepth, depth == 1 ? XYBitmap : ZPixmap, 0, &shminfo, width, height);
		shminfo.shmid = shmget(IPC_PRIVATE, (height + 2) * img->bytes_per_line, IPC_CREAT | 0777);
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
	}

	// Create normal X image if SHM doesn't work ("height + 2" for safety)
	if (!have_shm) {
		int bytes_per_row = aligned_width;
		switch (depth) {
			case 1:
				bytes_per_row /= 8;
				break;
			case 15:
			case 16:
				bytes_per_row *= 2;
				break;
			case 24:
			case 32:
				bytes_per_row *= 4;
				break;
		}
		the_buffer_copy = (uint8 *)malloc((aligned_height + 2) * bytes_per_row);
		img = XCreateImage(x_display, vis, depth == 1 ? 1 : xdepth, depth == 1 ? XYBitmap : ZPixmap, 0, (char *)the_buffer_copy, aligned_width, aligned_height, 32, bytes_per_row);
	}

	// 1-Bit mode is big-endian
    if (depth == 1) {
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
	XSetForeground(x_display, the_gc, black_pixel);

	// Create cursor
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

	// Init blitting routines
	bool native_byte_order;
#ifdef WORDS_BIGENDIAN
	native_byte_order = (XImageByteOrder(x_display) == MSBFirst);
#else
	native_byte_order = (XImageByteOrder(x_display) == LSBFirst);
#endif
#ifdef ENABLE_VOSF
	Screen_blitter_init(&visualInfo, native_byte_order, depth);
#endif

	// Set bytes per row
	VModes[cur_mode].viRowBytes = img->bytes_per_line;
	XSync(x_display, false);
	return true;
}

// Open DGA display (!! should use X11 VidMode extensions to set mode)
static bool open_dga(int width, int height)
{
#ifdef ENABLE_XF86_DGA
	// Set relative mouse mode
	ADBSetRelMouseMode(true);

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
	XGrabKeyboard(x_display, rootwin, True, GrabModeAsync, GrabModeAsync, CurrentTime);
	XGrabPointer(x_display, rootwin, True, PointerMotionMask | ButtonPressMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
	XF86DGADirectVideo(x_display, screen, XF86DGADirectGraphics | XF86DGADirectKeyb | XF86DGADirectMouse);
	XF86DGASetViewPort(x_display, screen, 0, 0);
	XF86DGASetVidPage(x_display, screen, 0);

	// Set colormap
	if (depth == 8)
		XF86DGAInstallColormap(x_display, screen, cmap[current_dga_cmap]);

	// Set bytes per row
	int bytes_per_row = (dga_fb_width + 7) & ~7;
	switch (depth) {
		case 15:
		case 16:
			bytes_per_row *= 2;
			break;
		case 24:
		case 32:
			bytes_per_row *= 4;
			break;
	}

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
	use_vosf = Screen_blitter_init(&visualInfo, native_byte_order, depth);
	
	if (use_vosf) {
	  // Allocate memory for frame buffer (SIZE is extended to page-boundary)
	  the_host_buffer = the_buffer;
	  the_buffer_size = page_extend((height + 2) * bytes_per_row);
	  the_buffer_copy = (uint8 *)malloc(the_buffer_size);
	  the_buffer = (uint8 *)vm_acquire(the_buffer_size);
	}
#else
	use_vosf = false;
	the_buffer = dga_screen_base;
#endif
#endif
	screen_base = (uint32)the_buffer;

	VModes[cur_mode].viRowBytes = bytes_per_row;
	XSync(x_display, false);
	return true;
#else
	ErrorAlert("SheepShaver has been compiled with DGA support disabled.");
	return false;
#endif
}

static bool open_display(void)
{
	display_type = VModes[cur_mode].viType;
	switch (VModes[cur_mode].viAppleMode) {
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
			depth = xdepth == 15 ? 15 : 16;
			break;
		case APPLE_32_BIT:
			depth = 32;
			break;
	}

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

	// Close window
	XDestroyWindow(x_display, the_win);
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

static void add_mode(VideoInfo *&p, uint32 allow, uint32 test, long apple_mode, long apple_id, int type)
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
		switch (apple_mode) {
			case APPLE_8_BIT:
				p->viRowBytes = p->viXsize;
				break;
			case APPLE_16_BIT:
				p->viRowBytes = p->viXsize * 2;
				break;
			case APPLE_32_BIT:
				p->viRowBytes = p->viXsize * 4;
				break;
		}
		p->viAppleMode = apple_mode;
		p->viAppleID = apple_id;
		p++;
	}
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

	// Init variables
	private_data = NULL;
	cur_mode = 0;	// Window 640x480
	video_activated = true;

	// Find screen and root window
	screen = XDefaultScreen(x_display);
	rootwin = XRootWindow(x_display, screen);

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

	// Get appropriate visual
	int color_class;
	switch (xdepth) {
#if 0
		case 1:
			color_class = StaticGray;
			break;
#endif
		case 8:
			color_class = PseudoColor;
			break;
		case 15:
		case 16:
		case 24:
		case 32:
			color_class = TrueColor;
			break;
		default:
			ErrorAlert(GetString(STR_UNSUPP_DEPTH_ERR));
			return false;
	}
	if (!XMatchVisualInfo(x_display, screen, xdepth, color_class, &visualInfo)) {
		ErrorAlert(GetString(STR_NO_XVISUAL_ERR));
		return false;
	}
	if (visualInfo.depth != xdepth) {
		ErrorAlert(GetString(STR_NO_XVISUAL_ERR));
		return false;
	}
	vis = visualInfo.visual;

	// Mac screen depth follows X depth (for now)
	depth = xdepth;

	// Create color maps for 8 bit mode
	if (depth == 8) {
		cmap[0] = XCreateColormap(x_display, rootwin, vis, AllocAll);
		cmap[1] = XCreateColormap(x_display, rootwin, vis, AllocAll);
		XInstallColormap(x_display, cmap[0]);
		XInstallColormap(x_display, cmap[1]);
	}

	// Construct video mode table
	int mode = APPLE_8_BIT;
	int bpr_mult = 8;
	switch (depth) {
		case 1:
			mode = APPLE_1_BIT;
			bpr_mult = 1;
			break;
		case 8:
			mode = APPLE_8_BIT;
			bpr_mult = 8;
			break;
		case 15:
		case 16:
			mode = APPLE_16_BIT;
			bpr_mult = 16;
			break;
		case 24:
		case 32:
			mode = APPLE_32_BIT;
			bpr_mult = 32;
			break;
	}

	uint32 window_modes = PrefsFindInt32("windowmodes");
	uint32 screen_modes = PrefsFindInt32("screenmodes");
	if (!has_dga)
		screen_modes = 0;
	if (window_modes == 0 && screen_modes == 0)
		window_modes |= 3;	// Allow at least 640x480 and 800x600 window modes

	VideoInfo *p = VModes;
	add_mode(p, window_modes, 1, mode, APPLE_W_640x480, DIS_WINDOW);
	add_mode(p, window_modes, 2, mode, APPLE_W_800x600, DIS_WINDOW);
	if (has_vidmode) {
		if (has_mode(640, 480))
			add_mode(p, screen_modes, 1, mode, APPLE_640x480, DIS_SCREEN);
		if (has_mode(800, 600))
			add_mode(p, screen_modes, 2, mode, APPLE_800x600, DIS_SCREEN);
		if (has_mode(1024, 768))
			add_mode(p, screen_modes, 4, mode, APPLE_1024x768, DIS_SCREEN);
		if (has_mode(1152, 900))
			add_mode(p, screen_modes, 8, mode, APPLE_1152x900, DIS_SCREEN);
		if (has_mode(1280, 1024))
			add_mode(p, screen_modes, 16, mode, APPLE_1280x1024, DIS_SCREEN);
		if (has_mode(1600, 1200))
			add_mode(p, screen_modes, 32, mode, APPLE_1600x1200, DIS_SCREEN);
	} else if (screen_modes) {
		int xsize = DisplayWidth(x_display, screen);
		int ysize = DisplayHeight(x_display, screen);
		int apple_id;
		if (xsize < 800)
			apple_id = APPLE_640x480;
		else if (xsize < 1024)
			apple_id = APPLE_800x600;
		else if (xsize < 1152)
			apple_id = APPLE_1024x768;
		else if (xsize < 1280)
			apple_id = APPLE_1152x900;
		else if (xsize < 1600)
			apple_id = APPLE_1280x1024;
		else
			apple_id = APPLE_1600x1200;
		p->viType = DIS_SCREEN;
		p->viRowBytes = 0;
		p->viXsize = xsize;
		p->viYsize = ysize;
		p->viAppleMode = mode;
		p->viAppleID = apple_id;
		p++;
	}
	p->viType = DIS_INVALID;	// End marker
	p->viRowBytes = 0;
	p->viXsize = p->viYsize = 0;
	p->viAppleMode = 0;
	p->viAppleID = 0;

#ifdef ENABLE_XF86_DGA
	if (has_dga && screen_modes) {
		int v_bank, v_size;
		XF86DGAGetVideo(x_display, screen, &dga_screen_base, &dga_fb_width, &v_bank, &v_size);
		D(bug("DGA screen_base %p, v_width %d\n", dga_screen_base, dga_fb_width));
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
	redraw_thread_active = (pthread_create(&redraw_thread, NULL, redraw_func, NULL) == 0);
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
		if (depth == 8) {
			XFreeColormap(x_display, cmap[0]);
			XFreeColormap(x_display, cmap[1]);
		}
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
	XF86DGADirectVideo(x_display, screen, XF86DGADirectGraphics | XF86DGADirectKeyb | XF86DGADirectMouse);
	XF86DGASetViewPort(x_display, screen, 0, 0);
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

static int event2keycode(XKeyEvent &ev)
{
	KeySym ks;
	int as;
	int i = 0;

	do {
		ks = XLookupKeysym(&ev, i++);
		as = kc_decode(ks);
		if (as != -1)
			return as;
	} while (ks != NoSymbol);

	return -1;
}

static void handle_events(void)
{
	// Handle events
	for (;;) {
		XEvent event;

		if (!XCheckMaskEvent(x_display, eventmask, &event))
			break;

		switch (event.type) {
			// Mouse button
			case ButtonPress: {
				unsigned int button = ((XButtonEvent *)&event)->button;
				if (button < 4)
					ADBMouseDown(button - 1);
				break;
			}
			case ButtonRelease: {
				unsigned int button = ((XButtonEvent *)&event)->button;
				if (button < 4)
					ADBMouseUp(button - 1);
				break;
			}

			// Mouse moved
			case EnterNotify:
				ADBMouseMoved(((XMotionEvent *)&event)->x, ((XMotionEvent *)&event)->y);
				break;
			case MotionNotify:
				ADBMouseMoved(((XMotionEvent *)&event)->x, ((XMotionEvent *)&event)->y);
				break;

			// Keyboard
			case KeyPress: {
				int code = event2keycode(event.xkey);
				if (use_keycodes && code != -1)
					code = keycode_table[event.xkey.keycode & 0xff];
				if (code != -1) {
					if (!emul_suspended) {
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
				int code = event2keycode(event.xkey);
				if (use_keycodes && code != 1)
					code = keycode_table[event.xkey.keycode & 0xff];
				if (code != -1) {
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
 *  Install graphics acceleration
 */

#if 0
// Rectangle blitting
static void accl_bitblt(accl_params *p)
{
	D(bug("accl_bitblt\n"));

	// Get blitting parameters
	int16 src_X = p->src_rect[1] - p->src_bounds[1];
	int16 src_Y = p->src_rect[0] - p->src_bounds[0];
	int16 dest_X = p->dest_rect[1] - p->dest_bounds[1];
	int16 dest_Y = p->dest_rect[0] - p->dest_bounds[0];
	int16 width = p->dest_rect[3] - p->dest_rect[1] - 1;
	int16 height = p->dest_rect[2] - p->dest_rect[0] - 1;
	D(bug(" src X %d, src Y %d, dest X %d, dest Y %d\n", src_X, src_Y, dest_X, dest_Y));
	D(bug(" width %d, height %d\n", width, height));

	// And perform the blit
	bitblt_hook(src_X, src_Y, dest_X, dest_Y, width, height);
}

static bool accl_bitblt_hook(accl_params *p)
{
	D(bug("accl_draw_hook %p\n", p));

	// Check if we can accelerate this bitblt
	if (p->src_base_addr == screen_base && p->dest_base_addr == screen_base &&
		display_type == DIS_SCREEN && bitblt_hook != NULL &&
		((uint32 *)p)[0x18 >> 2] + ((uint32 *)p)[0x128 >> 2] == 0 &&
		((uint32 *)p)[0x130 >> 2] == 0 &&
		p->transfer_mode == 0 &&
		p->src_row_bytes > 0 && ((uint32 *)p)[0x15c >> 2] > 0) {

		// Yes, set function pointer
		p->draw_proc = accl_bitblt;
		return true;
	}
	return false;
}

// Rectangle filling/inversion
static void accl_fillrect8(accl_params *p)
{
	D(bug("accl_fillrect8\n"));

	// Get filling parameters
	int16 dest_X = p->dest_rect[1] - p->dest_bounds[1];
	int16 dest_Y = p->dest_rect[0] - p->dest_bounds[0];
	int16 dest_X_max = p->dest_rect[3] - p->dest_bounds[1] - 1;
	int16 dest_Y_max = p->dest_rect[2] - p->dest_bounds[0] - 1;
	uint8 color = p->pen_mode == 8 ? p->fore_pen : p->back_pen;
	D(bug(" dest X %d, dest Y %d\n", dest_X, dest_Y));
	D(bug(" dest X max %d, dest Y max %d\n", dest_X_max, dest_Y_max));

	// And perform the fill
	fillrect8_hook(dest_X, dest_Y, dest_X_max, dest_Y_max, color);
}

static void accl_fillrect32(accl_params *p)
{
	D(bug("accl_fillrect32\n"));

	// Get filling parameters
	int16 dest_X = p->dest_rect[1] - p->dest_bounds[1];
	int16 dest_Y = p->dest_rect[0] - p->dest_bounds[0];
	int16 dest_X_max = p->dest_rect[3] - p->dest_bounds[1] - 1;
	int16 dest_Y_max = p->dest_rect[2] - p->dest_bounds[0] - 1;
	uint32 color = p->pen_mode == 8 ? p->fore_pen : p->back_pen;
	D(bug(" dest X %d, dest Y %d\n", dest_X, dest_Y));
	D(bug(" dest X max %d, dest Y max %d\n", dest_X_max, dest_Y_max));

	// And perform the fill
	fillrect32_hook(dest_X, dest_Y, dest_X_max, dest_Y_max, color);
}

static void accl_invrect(accl_params *p)
{
	D(bug("accl_invrect\n"));

	// Get inversion parameters
	int16 dest_X = p->dest_rect[1] - p->dest_bounds[1];
	int16 dest_Y = p->dest_rect[0] - p->dest_bounds[0];
	int16 dest_X_max = p->dest_rect[3] - p->dest_bounds[1] - 1;
	int16 dest_Y_max = p->dest_rect[2] - p->dest_bounds[0] - 1;
	D(bug(" dest X %d, dest Y %d\n", dest_X, dest_Y));
	D(bug(" dest X max %d, dest Y max %d\n", dest_X_max, dest_Y_max));

	//!!?? pen_mode == 14

	// And perform the inversion
	invrect_hook(dest_X, dest_Y, dest_X_max, dest_Y_max);
}

static bool accl_fillrect_hook(accl_params *p)
{
	D(bug("accl_fillrect_hook %p\n", p));

	// Check if we can accelerate this fillrect
	if (p->dest_base_addr == screen_base && ((uint32 *)p)[0x284 >> 2] != 0 && display_type == DIS_SCREEN) {
		if (p->transfer_mode == 8) {
			// Fill
			if (p->dest_pixel_size == 8 && fillrect8_hook != NULL) {
				p->draw_proc = accl_fillrect8;
				return true;
			} else if (p->dest_pixel_size == 32 && fillrect32_hook != NULL) {
				p->draw_proc = accl_fillrect32;
				return true;
			}
		} else if (p->transfer_mode == 10 && invrect_hook != NULL) {
			// Invert
			p->draw_proc = accl_invrect;
			return true;
		}
	}
	return false;
}

// Wait for graphics operation to finish
static bool accl_sync_hook(void *arg)
{
	D(bug("accl_sync_hook %p\n", arg));
	if (sync_hook != NULL)
		sync_hook();
	return true;
}

static struct accl_hook_info bitblt_hook_info = {accl_bitblt_hook, accl_sync_hook, ACCL_BITBLT};
static struct accl_hook_info fillrect_hook_info = {accl_fillrect_hook, accl_sync_hook, ACCL_FILLRECT};
#endif

void VideoInstallAccel(void)
{
	// Install acceleration hooks
	if (PrefsFindBool("gfxaccel")) {
		D(bug("Video: Installing acceleration hooks\n"));
//!!	NQDMisc(6, &bitblt_hook_info);
//		NQDMisc(6, &fillrect_hook_info);
	}
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
	palette_changed = true;
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
		if (have_shm)
			XShmPutImage(x_display, the_win, the_gc, img, x1, y1, x1, y1, wide, high, 0);
		else
			XPutImage(x_display, the_win, the_gc, img, x1, y1, x1, y1, wide, high);
	}
}

static void *redraw_func(void *arg)
{
	int tick_counter = 0;
	struct timespec req = {0, 16666667};

	for (;;) {

		// Wait
		nanosleep(&req, NULL);

		// Pause if requested (during video mode switches)
		while (thread_stop_req)
			thread_stop_ack = true;

		// Handle X11 events
		handle_events();

		// Quit DGA mode if requested
		if (quit_full_screen) {
			quit_full_screen = false;
			if (display_type == DIS_SCREEN) {
#ifdef ENABLE_XF86_DGA
				XF86DGADirectVideo(x_display, screen, 0);
				XUngrabPointer(x_display, CurrentTime);
				XUngrabKeyboard(x_display, CurrentTime);
#endif
				XSync(x_display, false);
				quit_full_screen_ack = true;
				return NULL;
			}
		}

		// Refresh display and set cursor image in window mode
		if (display_type == DIS_WINDOW) {
			tick_counter++;
			if (tick_counter >= frame_skip) {
				tick_counter = 0;

				// Update display
#ifdef ENABLE_VOSF
				if (use_vosf) {
					if (mainBuffer.dirty) {
						LOCK_VOSF;
						update_display_window_vosf();
						UNLOCK_VOSF;
						XSync(x_display, false); // Let the server catch up
					}
				}
				else
#endif
					update_display();

				// Set new cursor image if it was changed
				if (cursor_changed) {
					cursor_changed = false;
					memcpy(cursor_image->data, MacCursor + 4, 32);
					memcpy(cursor_mask_image->data, MacCursor + 36, 32);
					XFreeCursor(x_display, mac_cursor);
					XPutImage(x_display, cursor_map, cursor_gc, cursor_image, 0, 0, 0, 0, 16, 16);
					XPutImage(x_display, cursor_mask_map, cursor_mask_gc, cursor_mask_image, 0, 0, 0, 0, 16, 16);
					mac_cursor = XCreatePixmapCursor(x_display, cursor_map, cursor_mask_map, &black, &white, MacCursor[2], MacCursor[3]);
					XDefineCursor(x_display, the_win, mac_cursor);
				}
			}
		}
#ifdef ENABLE_VOSF
		else if (use_vosf) {
			// Update display (VOSF variant)
			static int tick_counter = 0;
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
		if (palette_changed && !emul_suspended) {
			palette_changed = false;
			XColor c[256];
			for (int i=0; i<256; i++) {
				c[i].pixel = i;
				c[i].red = mac_pal[i].red * 0x0101;
				c[i].green = mac_pal[i].green * 0x0101;
				c[i].blue = mac_pal[i].blue * 0x0101;
				c[i].flags = DoRed | DoGreen | DoBlue;
			}
			if (depth == 8) {
				XStoreColors(x_display, cmap[0], c, 256);
				XStoreColors(x_display, cmap[1], c, 256);
#ifdef ENABLE_XF86_DGA
				if (display_type == DIS_SCREEN) {
					current_dga_cmap ^= 1;
					XF86DGAInstallColormap(x_display, screen, cmap[current_dga_cmap]);
				}
#endif
			}
		}
	}
	return NULL;
}
