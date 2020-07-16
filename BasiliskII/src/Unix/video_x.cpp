/*
 *  video_x.cpp - Video/graphics emulation, X11 specific stuff
 *
 *  Basilisk II (C) Christian Bauer
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

/*
 *  NOTES:
 *    The Ctrl key works like a qualifier for special actions:
 *      Ctrl-Tab = suspend DGA mode
 *      Ctrl-Esc = emergency quit
 *      Ctrl-F1 = mount floppy
 *      Ctrl-F5 = grab mouse (in windowed mode)
 */

#include "sysdeps.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>

#include <algorithm>

#ifdef HAVE_PTHREADS
# include <pthread.h>
#endif

#ifdef ENABLE_XF86_DGA
# include <X11/extensions/Xxf86dga.h>
#endif

#ifdef ENABLE_XF86_VIDMODE
# include <X11/extensions/xf86vmode.h>
#endif

#ifdef ENABLE_FBDEV_DGA
# include <sys/mman.h>
#endif

#include "cpu_emulation.h"
#include "main.h"
#include "adb.h"
#include "macos_util.h"
#include "prefs.h"
#include "user_strings.h"
#include "video.h"
#include "video_blit.h"

#define DEBUG 0
#include "debug.h"


// Supported video modes
static vector<video_mode> VideoModes;

// Display types
enum {
	DISPLAY_WINDOW,	// X11 window, using MIT SHM extensions if possible
	DISPLAY_DGA		// DGA fullscreen display
};

// Constants
const char KEYCODE_FILE_NAME[] = DATADIR "/keycodes";

static const int win_eventmask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | EnterWindowMask | ExposureMask | StructureNotifyMask;
static const int dga_eventmask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask;

// Mac Screen Width and Height
uint32 MacScreenWidth;
uint32 MacScreenHeight;

// Global variables
static int32 frame_skip;							// Prefs items
static int16 mouse_wheel_mode;
static int16 mouse_wheel_lines;

static int display_type = DISPLAY_WINDOW;			// See enum above
static bool local_X11;								// Flag: X server running on local machine?
static uint8 *the_buffer = NULL;					// Mac frame buffer (where MacOS draws into)
static uint8 *the_buffer_copy = NULL;				// Copy of Mac frame buffer (for refreshed modes)
static uint32 the_buffer_size;						// Size of allocated the_buffer

static bool redraw_thread_active = false;			// Flag: Redraw thread installed
#ifdef HAVE_PTHREADS
static pthread_attr_t redraw_thread_attr;			// Redraw thread attributes
static volatile bool redraw_thread_cancel;			// Flag: Cancel Redraw thread
static volatile bool redraw_thread_cancel_ack;		// Flag: Acknowledge for redraw thread cancellation
static pthread_t redraw_thread;						// Redraw thread
#endif

static bool has_dga = false;						// Flag: Video DGA capable
static bool has_vidmode = false;					// Flag: VidMode extension available

#ifdef ENABLE_VOSF
static bool use_vosf = true;						// Flag: VOSF enabled
#else
static const bool use_vosf = false;					// VOSF not possible
#endif

static bool ctrl_down = false;						// Flag: Ctrl key pressed
static bool caps_on = false;						// Flag: Caps Lock on
static bool quit_full_screen = false;				// Flag: DGA close requested from redraw thread
static bool emerg_quit = false;						// Flag: Ctrl-Esc pressed, emergency quit requested from MacOS thread
static bool emul_suspended = false;					// Flag: Emulator suspended

static bool classic_mode = false;					// Flag: Classic Mac video mode

static bool use_keycodes = false;					// Flag: Use keycodes rather than keysyms
static int keycode_table[256];						// X keycode -> Mac keycode translation table

// X11 variables
char *x_display_name = NULL;						// X11 display name
Display *x_display = NULL;							// X11 display handle
static int screen;									// Screen number
static Window rootwin;								// Root window and our window
static int num_depths = 0;							// Number of available X depths
static int *avail_depths = NULL;					// List of available X depths
static XColor black, white;
static unsigned long black_pixel, white_pixel;
static int eventmask;

static int xdepth;									// Depth of X screen
static VisualFormat visualFormat;
static XVisualInfo visualInfo;
static Visual *vis;
static int color_class;

static bool x_native_byte_order;						// XImage has native byte order?
static int rshift, rloss, gshift, gloss, bshift, bloss;	// Pixel format of DirectColor/TrueColor modes

static Colormap cmap[2] = {0, 0};					// Colormaps for indexed modes (DGA needs two of them)

static XColor x_palette[256];						// Color palette to be used as CLUT and gamma table
static bool x_palette_changed = false;				// Flag: Palette changed, redraw thread must set new colors

#ifdef ENABLE_FBDEV_DGA
static int fbdev_fd = -1;
#endif

#ifdef ENABLE_XF86_VIDMODE
static XF86VidModeModeInfo **x_video_modes = NULL;	// Array of all available modes
static int num_x_video_modes;
#endif

// Mutex to protect palette
#ifdef HAVE_PTHREADS
static pthread_mutex_t x_palette_lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK_PALETTE pthread_mutex_lock(&x_palette_lock)
#define UNLOCK_PALETTE pthread_mutex_unlock(&x_palette_lock)
#else
#define LOCK_PALETTE
#define UNLOCK_PALETTE
#endif

// Mutex to protect frame buffer
#ifdef HAVE_PTHREADS
static pthread_mutex_t frame_buffer_lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK_FRAME_BUFFER pthread_mutex_lock(&frame_buffer_lock);
#define UNLOCK_FRAME_BUFFER pthread_mutex_unlock(&frame_buffer_lock);
#else
#define LOCK_FRAME_BUFFER
#define UNLOCK_FRAME_BUFFER
#endif

// Variables for non-VOSF incremental refresh
static const int sm_uptd[] = {4,1,6,3,0,5,2,7};
static int sm_no_boxes[] = {1,8,32,64,128,300};
static bool updt_box[17][17];
static int nr_boxes;

// Video refresh function
static void VideoRefreshInit(void);
static void (*video_refresh)(void);


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


/*
 *  monitor_desc subclass for X11 display
 */

class X11_monitor_desc : public monitor_desc {
public:
	X11_monitor_desc(const vector<video_mode> &available_modes, video_depth default_depth, uint32 default_id) : monitor_desc(available_modes, default_depth, default_id) {}
	~X11_monitor_desc() {}

	virtual void switch_to_current_mode(void);
	virtual void set_palette(uint8 *pal, int num);

	bool video_open(void);
	void video_close(void);
};


/*
 *  Utility functions
 */

// Map video_mode depth ID to numerical depth value
static inline int depth_of_video_mode(video_mode const & mode)
{
	int depth = -1;
	switch (mode.depth) {
	case VDEPTH_1BIT:
		depth = 1;
		break;
	case VDEPTH_2BIT:
		depth = 2;
		break;
	case VDEPTH_4BIT:
		depth = 4;
		break;
	case VDEPTH_8BIT:
		depth = 8;
		break;
	case VDEPTH_16BIT:
		depth = 16;
		break;
	case VDEPTH_32BIT:
		depth = 32;
		break;
	default:
		abort();
	}
	return depth;
}

// Map RGB color to pixel value (this only works in TrueColor/DirectColor visuals)
static inline uint32 map_rgb(uint8 red, uint8 green, uint8 blue, bool fix_byte_order = false)
{
	uint32 val = ((red >> rloss) << rshift) | ((green >> gloss) << gshift) | ((blue >> bloss) << bshift);
	if (fix_byte_order && !x_native_byte_order) {
		// We have to fix byte order in the ExpandMap[]
		// NOTE: this is only an optimization since Screen_blitter_init()
		// could be arranged to choose an NBO or OBO (with
		// byteswapping) Blit_Expand_X_To_Y() function
		switch (visualFormat.depth) {
		case 15: case 16:
			val = do_byteswap_16(val);
			break;
		case 24: case 32:
			val = do_byteswap_32(val);
			break;
		}
	}
	return val;
}

// Do we have a visual for handling the specified Mac depth? If so, set the
// global variables "xdepth", "visualInfo", "vis" and "color_class".
static bool find_visual_for_depth(video_depth depth)
{
	D(bug("have_visual_for_depth(%d)\n", 1 << depth));

	// 1-bit works always and uses default visual
	if (depth == VDEPTH_1BIT) {
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
		case VDEPTH_2BIT:
		case VDEPTH_4BIT:	// VOSF blitters can convert 2/4/8-bit -> 8/16/32-bit
		case VDEPTH_8BIT:
			min_depth = 8;
			max_depth = 32;
			break;
#else
		case VDEPTH_2BIT:
		case VDEPTH_4BIT:	// 2/4-bit requires VOSF blitters
			return false;
		case VDEPTH_8BIT:	// 8-bit without VOSF requires an 8-bit visual
			min_depth = 8;
			max_depth = 8;
			break;
#endif
		case VDEPTH_16BIT:	// 16-bit requires a 15/16-bit visual
			min_depth = 15;
			max_depth = 16;
			break;
		case VDEPTH_32BIT:	// 32-bit requires a 24/32-bit visual
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

// Add mode to list of supported modes
static void add_mode(uint32 width, uint32 height, uint32 resolution_id, uint32 bytes_per_row, video_depth depth)
{
	video_mode mode;
	mode.x = width;
	mode.y = height;
	mode.resolution_id = resolution_id;
	mode.bytes_per_row = bytes_per_row;
	mode.depth = depth;
	mode.user_data = 0;
	VideoModes.push_back(mode);
}

// Add standard list of windowed modes for given color depth
static void add_window_modes(video_depth depth)
{
	add_mode(512, 384, 0x80, TrivialBytesPerRow(512, depth), depth);
	add_mode(640, 480, 0x81, TrivialBytesPerRow(640, depth), depth);
	add_mode(800, 600, 0x82, TrivialBytesPerRow(800, depth), depth);
	add_mode(1024, 768, 0x83, TrivialBytesPerRow(1024, depth), depth);
	add_mode(1152, 870, 0x84, TrivialBytesPerRow(1152, depth), depth);
	add_mode(1280, 1024, 0x85, TrivialBytesPerRow(1280, depth), depth);
	add_mode(1600, 1200, 0x86, TrivialBytesPerRow(1600, depth), depth);
}

// Set Mac frame layout and base address (uses the_buffer/MacFrameBaseMac)
static void set_mac_frame_buffer(X11_monitor_desc &monitor, video_depth depth, bool native_byte_order)
{
#if !REAL_ADDRESSING && !DIRECT_ADDRESSING
	int layout = FLAYOUT_DIRECT;
	if (depth == VDEPTH_16BIT)
		layout = (xdepth == 15) ? FLAYOUT_HOST_555 : FLAYOUT_HOST_565;
	else if (depth == VDEPTH_32BIT)
		layout = (xdepth == 24) ? FLAYOUT_HOST_888 : FLAYOUT_DIRECT;
	if (native_byte_order)
		MacFrameLayout = layout;
	else
		MacFrameLayout = FLAYOUT_DIRECT;
	if (TwentyFourBitAddressing)
		monitor.set_mac_frame_base(MacFrameBaseMac24Bit);
	else
		monitor.set_mac_frame_base(MacFrameBaseMac);

	// Set variables used by UAE memory banking
	const video_mode &mode = monitor.get_current_mode();
	MacFrameBaseHost = the_buffer;
	MacFrameSize = mode.bytes_per_row * mode.y;
	InitFrameBufferMapping();
#else
	monitor.set_mac_frame_base(Host2MacAddr(the_buffer));
#endif
	D(bug("monitor.mac_frame_base = %08x\n", monitor.get_mac_frame_base()));
}

// Set window name and class
static void set_window_name(Window w, int name)
{
	const char *str = GetString(name);
	XStoreName(x_display, w, str);
	XSetIconName(x_display, w, str);

	XClassHint *hints;
	hints = XAllocClassHint();
	if (hints) {
		hints->res_name = "BasiliskII";
		hints->res_class = "BasiliskII";
		XSetClassHint(x_display, w, hints);
		XFree(hints);
	}
}

// This struct is designed to match the ones generated by GIMP in
// BasiliskII_*_icon.c
struct gimp_image {
	unsigned int 	width;
	unsigned int 	height;
	unsigned int 	bytes_per_pixel;
	unsigned char	pixel_data[0]; // Variable-length
};

// These were generated by using 'icns2png -x
// ../MacOSX/BasiliskII.icns', then using GIMP to convert the
// resulting .png files into "C source code (*.c)".  GIMP doesn't
// generate corresponding .h files with extern declarations, so just
// #include the .c files here.
#include "BasiliskII_32x32x32_icon.c"
#include "BasiliskII_128x128x32_icon.c"

// Set window icons
static void set_window_icons(Window w)
{
	// As per the _NET_WM_ICON documentation at
	// https://standards.freedesktop.org/wm-spec/wm-spec-latest.html#idm140200472568384,
	// "The first two cardinals are width, height."
	const unsigned int HEADER_SIZE = 2;
	// We will pass 32-bit values to XChangeProperty()
	const unsigned int FORMAT = 32;

	// Icon data from GIMP to be converted and passed to the
	// Window Manager
	const struct gimp_image* const icons[] =
		{(struct gimp_image *) &icon_32x32x32,
		 (struct gimp_image *) &icon_128x128x32};
	const unsigned int num_icons = sizeof(icons) / sizeof(icons[0]);
	unsigned int icon;

	// Work out how big the buffer needs to be to store all of our icons
	unsigned int buffer_size = 0;
	for (icon = 0; icon < num_icons; icon++) {
		buffer_size += HEADER_SIZE +
			       icons[icon]->width * icons[icon]->height;
	}

	// As per the XChangeProperty() man page, "If the specified
	// format is 32, the property data must be a long array."
	unsigned long buffer[buffer_size];
	// This points to the start of the current icon within buffer
	unsigned long *buffer_icon = buffer;

	// Copy the icons into the buffer
	for (icon = 0; icon < num_icons; icon++) {
		const unsigned int pixel_count = icons[icon]->width *
						 icons[icon]->height;
		assert(icons[icon]->bytes_per_pixel == 4);
		buffer_icon[0] = icons[icon]->width;
		buffer_icon[1] = icons[icon]->height;
		unsigned long *const buffer_pixels = buffer_icon + HEADER_SIZE;

		unsigned int i;
		for (i = 0; i < pixel_count; i++) {
			const unsigned char *src =
				&icons[icon]->pixel_data[i * icons[icon]->bytes_per_pixel];
			buffer_pixels[i] = (src[3] << 24 |
					    src[0] << 16 |
					    src[1] << 8  |
					    src[2]);
		}

		buffer_icon += HEADER_SIZE + pixel_count;
	}

	Atom net_wm_icon = XInternAtom(x_display, "_NET_WM_ICON", False);
	if (net_wm_icon == None) {
		ErrorAlert(STR_X_ICON_ATOM_ALLOC_ERR);
		// We can still continue running, just without an icon
		return;
	}
	XChangeProperty(x_display, w, net_wm_icon, XA_CARDINAL, FORMAT,
			PropModeReplace, (const unsigned char *) buffer,
			buffer_size);
}

// Set window input focus flag
static void set_window_focus(Window w)
{
	XWMHints *hints = XAllocWMHints();
	if (hints) {
		hints->input = True;
		hints->initial_state = NormalState;
		hints->flags = InputHint | StateHint;
		XSetWMHints(x_display, w, hints);
		XFree(hints);
	}
}

// Set WM_DELETE_WINDOW protocol on window (preventing it from being destroyed by the WM when clicking on the "close" widget)
static Atom WM_DELETE_WINDOW = (Atom)0;
static void set_window_delete_protocol(Window w)
{
	WM_DELETE_WINDOW = XInternAtom(x_display, "WM_DELETE_WINDOW", false);
	XSetWMProtocols(x_display, w, &WM_DELETE_WINDOW, 1);
}

// Wait until window is mapped/unmapped
void wait_mapped(Window w)
{
	XEvent e;
	do {
		XMaskEvent(x_display, StructureNotifyMask, &e);
	} while ((e.type != MapNotify) || (e.xmap.event != w));
}

void wait_unmapped(Window w)
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


/*
 *  Framebuffer allocation routines
 */

#ifdef ENABLE_VOSF
#include "vm_alloc.h"

static void *vm_acquire_framebuffer(uint32 size)
{
	// always try to allocate framebuffer at the same address
	static void *fb = VM_MAP_FAILED;
	if (fb != VM_MAP_FAILED) {
		if (vm_acquire_fixed(fb, size) < 0)
			fb = VM_MAP_FAILED;
	}
	if (fb == VM_MAP_FAILED)
		fb = vm_acquire(size, VM_MAP_DEFAULT | VM_MAP_32BIT);
	return fb;
}

static inline void vm_release_framebuffer(void *fb, uint32 size)
{
	vm_release(fb, size);
}
#endif


/*
 *  Display "driver" classes
 */

class driver_base {
public:
	driver_base(X11_monitor_desc &m);
	virtual ~driver_base();

	virtual void update_palette(void);
	virtual void suspend(void) {}
	virtual void resume(void) {}
	virtual void toggle_mouse_grab(void) {}
	virtual void mouse_moved(int x, int y) { ADBMouseMoved(x, y); }

	void disable_mouse_accel(void);
	void restore_mouse_accel(void);

	virtual void grab_mouse(void) {}
	virtual void ungrab_mouse(void) {}

public:
	X11_monitor_desc &monitor; // Associated video monitor
	const video_mode &mode;    // Video mode handled by the driver

	bool init_ok;	// Initialization succeeded (we can't use exceptions because of -fomit-frame-pointer)
	Window w;		// The window we draw into

	int orig_accel_numer, orig_accel_denom, orig_threshold;	// Original mouse acceleration
};

class driver_window;
static void update_display_window_vosf(driver_window *drv);
static void update_display_dynamic(int ticker, driver_window *drv);
static void update_display_static(driver_window *drv);

class driver_window : public driver_base {
	friend void update_display_window_vosf(driver_window *drv);
	friend void update_display_dynamic(int ticker, driver_window *drv);
	friend void update_display_static(driver_window *drv);

public:
	driver_window(X11_monitor_desc &monitor);
	~driver_window();

	void toggle_mouse_grab(void);
	void mouse_moved(int x, int y);

	void grab_mouse(void);
	void ungrab_mouse(void);

private:
	GC gc;
	XImage *img;
	bool have_shm;					// Flag: SHM extensions available
	XShmSegmentInfo shminfo;
	Cursor mac_cursor;
	bool mouse_grabbed;				// Flag: mouse pointer grabbed, using relative mouse mode
	int mouse_last_x, mouse_last_y;	// Last mouse position (for relative mode)
};

class driver_dga;
static void update_display_dga_vosf(driver_dga *drv);

class driver_dga : public driver_base {
	friend void update_display_dga_vosf(driver_dga *drv);

public:
	driver_dga(X11_monitor_desc &monitor);
	~driver_dga();

	void suspend(void);
	void resume(void);

protected:
	struct FakeXImage {
		int width, height;		// size of image
		int depth;				// depth of image
		int bytes_per_line;		// accelerator to next line

		FakeXImage(int w, int h, int d)
			: width(w), height(h), depth(d)
			{ bytes_per_line = TrivialBytesPerRow(width, DepthModeForPixelDepth(depth)); }
	};
	FakeXImage *img;

private:
	Window suspend_win;		// "Suspend" information window
	void *fb_save;			// Saved frame buffer for suspend/resume
};

static driver_base *drv = NULL;	// Pointer to currently used driver object

#ifdef ENABLE_VOSF
# include "video_vosf.h"
#endif

driver_base::driver_base(X11_monitor_desc &m)
 : monitor(m), mode(m.get_current_mode()), init_ok(false), w(0)
{
	the_buffer = NULL;
	the_buffer_copy = NULL;
	XGetPointerControl(x_display, &orig_accel_numer, &orig_accel_denom, &orig_threshold);
}

driver_base::~driver_base()
{
	ungrab_mouse();
	restore_mouse_accel();

	if (w) {
		XUnmapWindow(x_display, w);
		wait_unmapped(w);
		XDestroyWindow(x_display, w);
	}

	XFlush(x_display);
	XSync(x_display, false);

	// Free frame buffer(s)
	if (!use_vosf) {
		if (the_buffer) {
			free(the_buffer);
			the_buffer = NULL;
		}
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
			vm_release_framebuffer(the_buffer, the_buffer_size);
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

// Palette has changed
void driver_base::update_palette(void)
{
	if (color_class == PseudoColor || color_class == DirectColor) {
		int num = vis->map_entries;
		if (!IsDirectMode(monitor.get_current_mode()) && color_class == DirectColor)
			return; // Indexed mode on true color screen, don't set CLUT
		XStoreColors(x_display, cmap[0], x_palette, num);
		XStoreColors(x_display, cmap[1], x_palette, num);
	}
	XSync(x_display, false);
}

// Disable mouse acceleration
void driver_base::disable_mouse_accel(void)
{
	XChangePointerControl(x_display, True, False, 1, 1, 0);
}

// Restore mouse acceleration to original value
void driver_base::restore_mouse_accel(void)
{
	XChangePointerControl(x_display, True, True, orig_accel_numer, orig_accel_denom, orig_threshold);
}


/*
 *  Windowed display driver
 */

// Open display
driver_window::driver_window(X11_monitor_desc &m)
 : driver_base(m), gc(0), img(NULL), have_shm(false), mac_cursor(0),
   mouse_grabbed(false), mouse_last_x(0), mouse_last_y(0)
{
	int width = mode.x, height = mode.y;
	int aligned_width = (width + 15) & ~15;
	int aligned_height = (height + 15) & ~15;

	// Set absolute mouse mode
	ADBSetRelMouseMode(mouse_grabbed);

	// Create window (setting background_pixel, border_pixel and colormap is
	// mandatory when using a non-default visual; in 1-bit mode we use the
	// default visual, so we can also use the default colormap)
	XSetWindowAttributes wattr;
	wattr.event_mask = eventmask = win_eventmask;
	wattr.background_pixel = (vis == DefaultVisual(x_display, screen) ? black_pixel : 0);
	wattr.border_pixel = 0;
	wattr.colormap = (mode.depth == VDEPTH_1BIT ? DefaultColormap(x_display, screen) : cmap[0]);
	w = XCreateWindow(x_display, rootwin, 0, 0, width, height, 0, xdepth,
		InputOutput, vis, CWEventMask | CWBackPixel | CWBorderPixel | CWColormap, &wattr);
	D(bug(" window created\n"));

	// Set window name/class
	set_window_name(w, STR_WINDOW_TITLE);

	// Set window icons
	set_window_icons(w);

	// Indicate that we want keyboard input
	set_window_focus(w);

	// Set delete protocol property
	set_window_delete_protocol(w);

	// Make window unresizable
	{
		XSizeHints *hints = XAllocSizeHints();
		if (hints) {
			hints->min_width = width;
			hints->max_width = width;
			hints->min_height = height;
			hints->max_height = height;
			hints->flags = PMinSize | PMaxSize;
			XSetWMNormalHints(x_display, w, hints);
			XFree(hints);
		}
	}
	D(bug(" window attributes set\n"));
	
	// Show window
	XMapWindow(x_display, w);
	wait_mapped(w);
	D(bug(" window mapped\n"));

	// 1-bit mode is big-endian; if the X server is little-endian, we can't
	// use SHM because that doesn't allow changing the image byte order
	bool need_msb_image = (mode.depth == VDEPTH_1BIT && XImageByteOrder(x_display) == LSBFirst);

	// Try to create and attach SHM image
	if (local_X11 && !need_msb_image && XShmQueryExtension(x_display)) {

		// Create SHM image ("height + 2" for safety)
		img = XShmCreateImage(x_display, vis, mode.depth == VDEPTH_1BIT ? 1 : xdepth, mode.depth == VDEPTH_1BIT ? XYBitmap : ZPixmap, 0, &shminfo, width, height);
		D(bug(" shm image created\n"));
		shminfo.shmid = shmget(IPC_PRIVATE, (aligned_height + 2) * img->bytes_per_line, IPC_CREAT | 0777);
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
			img = NULL;
			shminfo.shmid = -1;
		} else {
			have_shm = true;
			shmctl(shminfo.shmid, IPC_RMID, 0);
		}
		D(bug(" shm image attached\n"));
	}
	
	// Create normal X image if SHM doesn't work ("height + 2" for safety)
	if (!have_shm) {
		int bytes_per_row = (mode.depth == VDEPTH_1BIT ? aligned_width/8 : TrivialBytesPerRow(aligned_width, DepthModeForPixelDepth(xdepth)));
		the_buffer_copy = (uint8 *)malloc((aligned_height + 2) * bytes_per_row);
		img = XCreateImage(x_display, vis, mode.depth == VDEPTH_1BIT ? 1 : xdepth, mode.depth == VDEPTH_1BIT ? XYBitmap : ZPixmap, 0, (char *)the_buffer_copy, aligned_width, aligned_height, 32, bytes_per_row);
		D(bug(" X image created\n"));
	}

	if (need_msb_image) {
		img->byte_order = MSBFirst;
		img->bitmap_bit_order = MSBFirst;
	}

#ifdef ENABLE_VOSF
	use_vosf = true;
	// Allocate memory for frame buffer (SIZE is extended to page-boundary)
	the_host_buffer = the_buffer_copy;
	the_buffer_size = page_extend((aligned_height + 2) * img->bytes_per_line);
	the_buffer = (uint8 *)vm_acquire_framebuffer(the_buffer_size);
	the_buffer_copy = (uint8 *)malloc(the_buffer_size);
	D(bug("the_buffer = %p, the_buffer_copy = %p, the_host_buffer = %p\n", the_buffer, the_buffer_copy, the_host_buffer));
#else
	// Allocate memory for frame buffer
	the_buffer = (uint8 *)malloc((aligned_height + 2) * img->bytes_per_line);
	D(bug("the_buffer = %p, the_buffer_copy = %p\n", the_buffer, the_buffer_copy));
#endif

	// Create GC
	gc = XCreateGC(x_display, w, 0, 0);
	XSetState(x_display, gc, black_pixel, white_pixel, GXcopy, AllPlanes);

	// Create no_cursor
	mac_cursor = XCreatePixmapCursor(x_display,
	   XCreatePixmap(x_display, w, 1, 1, 1),
	   XCreatePixmap(x_display, w, 1, 1, 1),
	   &black, &white, 0, 0);
	XDefineCursor(x_display, w, mac_cursor);

	// Init blitting routines
#ifdef ENABLE_VOSF
	Screen_blitter_init(visualFormat, x_native_byte_order, depth_of_video_mode(mode));
#endif

	// Set frame buffer base
	set_mac_frame_buffer(monitor, mode.depth, x_native_byte_order);

	// Everything went well
	init_ok = true;
}

// Close display
driver_window::~driver_window()
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
	if (gc)
		XFreeGC(x_display, gc);
}

// Toggle mouse grab
void driver_window::toggle_mouse_grab(void)
{
	if (mouse_grabbed)
		ungrab_mouse();
	else
		grab_mouse();
}

// Grab mouse, switch to relative mouse mode
void driver_window::grab_mouse(void)
{
	int result;
	for (int i=0; i<10; i++) {
		result = XGrabPointer(x_display, w, True, 0,
			GrabModeAsync, GrabModeAsync, w, None, CurrentTime);
		if (result != AlreadyGrabbed)
			break;
		Delay_usec(100000);
	}
	if (result == GrabSuccess) {
		XStoreName(x_display, w, GetString(STR_WINDOW_TITLE_GRABBED));
		ADBSetRelMouseMode(mouse_grabbed = true);
		disable_mouse_accel();
	}
}

// Ungrab mouse, switch to absolute mouse mode
void driver_window::ungrab_mouse(void)
{
	if (mouse_grabbed) {
		XUngrabPointer(x_display, CurrentTime);
		XStoreName(x_display, w, GetString(STR_WINDOW_TITLE));
		ADBSetRelMouseMode(mouse_grabbed = false);
		restore_mouse_accel();
	}
}

// Mouse moved
void driver_window::mouse_moved(int x, int y)
{
	if (!mouse_grabbed) {
		mouse_last_x = x; mouse_last_y = y;
		ADBMouseMoved(x, y);
		return;
	}

	// Warped mouse motion (this code is taken from SDL)

	// Post first mouse event
	int width = monitor.get_current_mode().x, height = monitor.get_current_mode().y;
	int delta_x = x - mouse_last_x, delta_y = y - mouse_last_y;
	mouse_last_x = x; mouse_last_y = y;
	ADBMouseMoved(delta_x, delta_y);

	// Only warp the pointer when it has reached the edge
	const int MOUSE_FUDGE_FACTOR = 8;
	if (x < MOUSE_FUDGE_FACTOR || x > (width - MOUSE_FUDGE_FACTOR)
	 || y < MOUSE_FUDGE_FACTOR || y > (height - MOUSE_FUDGE_FACTOR)) {
		XEvent event;
		while (XCheckTypedEvent(x_display, MotionNotify, &event)) {
			delta_x = x - mouse_last_x; delta_y = y - mouse_last_y;
			mouse_last_x = x; mouse_last_y = y;
			ADBMouseMoved(delta_x, delta_y);
		}
		mouse_last_x = width/2;
		mouse_last_y = height/2;
		XWarpPointer(x_display, None, w, 0, 0, 0, 0, mouse_last_x, mouse_last_y);
		for (int i=0; i<10; i++) {
			XMaskEvent(x_display, PointerMotionMask, &event);
			if (event.xmotion.x > (mouse_last_x - MOUSE_FUDGE_FACTOR)
			 && event.xmotion.x < (mouse_last_x + MOUSE_FUDGE_FACTOR)
			 && event.xmotion.y > (mouse_last_y - MOUSE_FUDGE_FACTOR)
			 && event.xmotion.y < (mouse_last_y + MOUSE_FUDGE_FACTOR))
				break;
		}
	}
}


#if defined(ENABLE_XF86_DGA) || defined(ENABLE_FBDEV_DGA)
/*
 *  DGA display driver base class
 */

driver_dga::driver_dga(X11_monitor_desc &m)
	: driver_base(m), suspend_win(0), fb_save(NULL), img(NULL)
{
}

driver_dga::~driver_dga()
{
	XUngrabPointer(x_display, CurrentTime);
	XUngrabKeyboard(x_display, CurrentTime);

	if (img)
		delete img;
}

// Suspend emulation
void driver_dga::suspend(void)
{
	// Release ctrl key
	ADBKeyUp(0x36);
	ctrl_down = false;

	// Lock frame buffer (this will stop the MacOS thread)
	LOCK_FRAME_BUFFER;

	// Save frame buffer
	fb_save = malloc(mode.y * mode.bytes_per_row);
	if (fb_save)
		memcpy(fb_save, the_buffer, mode.y * mode.bytes_per_row);

	// Close full screen display
#ifdef ENABLE_XF86_DGA
	XF86DGADirectVideo(x_display, screen, 0);
#endif
	XUngrabPointer(x_display, CurrentTime);
	XUngrabKeyboard(x_display, CurrentTime);
	restore_mouse_accel();
	XUnmapWindow(x_display, w);
	wait_unmapped(w);

	// Open "suspend" window
	XSetWindowAttributes wattr;
	wattr.event_mask = KeyPressMask;
	wattr.background_pixel = black_pixel;
		
	suspend_win = XCreateWindow(x_display, rootwin, 0, 0, 512, 1, 0, xdepth,
		InputOutput, vis, CWEventMask | CWBackPixel, &wattr);
	set_window_name(suspend_win, STR_SUSPEND_WINDOW_TITLE);
	set_window_focus(suspend_win);
	XMapWindow(x_display, suspend_win);
	emul_suspended = true;
}

// Resume emulation
void driver_dga::resume(void)
{
	// Close "suspend" window
	XDestroyWindow(x_display, suspend_win);
	XSync(x_display, false);

	// Reopen full screen display
	XMapRaised(x_display, w);
	wait_mapped(w);
	XWarpPointer(x_display, None, rootwin, 0, 0, 0, 0, 0, 0);
	XGrabKeyboard(x_display, rootwin, True, GrabModeAsync, GrabModeAsync, CurrentTime);
	XGrabPointer(x_display, rootwin, True, PointerMotionMask | ButtonPressMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
	disable_mouse_accel();
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
		memset(the_buffer_copy, 0, mode.bytes_per_row * mode.y);
	}
#endif
	
	// Restore frame buffer
	if (fb_save) {
#ifdef ENABLE_VOSF
		// Don't copy fb_save to the temporary frame buffer in VOSF mode
		if (!use_vosf)
#endif
		memcpy(the_buffer, fb_save, mode.y * mode.bytes_per_row);
		free(fb_save);
		fb_save = NULL;
	}
	
	// Unlock frame buffer (and continue MacOS thread)
	UNLOCK_FRAME_BUFFER;
	emul_suspended = false;
}
#endif


#ifdef ENABLE_FBDEV_DGA
/*
 *  fbdev DGA display driver
 */

const char FBDEVICES_FILE_NAME[] = DATADIR "/fbdevices";
const char FBDEVICE_FILE_NAME[] = "/dev/fb";

class driver_fbdev : public driver_dga {
public:
	driver_fbdev(X11_monitor_desc &monitor);
	~driver_fbdev();
};

// Open display
driver_fbdev::driver_fbdev(X11_monitor_desc &m) : driver_dga(m)
{
	int width = mode.x, height = mode.y;

	// Set absolute mouse mode
	ADBSetRelMouseMode(false);
	
	// Find the maximum depth available
	int ndepths, max_depth(0);
	int *depths = XListDepths(x_display, screen, &ndepths);
	if (depths == NULL) {
		printf("FATAL: Could not determine the maximal depth available\n");
		return;
	} else {
		while (ndepths-- > 0) {
			if (depths[ndepths] > max_depth)
				max_depth = depths[ndepths];
		}
	}
	
	// Get fbdevices file path from preferences
	const char *fbd_path = PrefsFindString("fbdevicefile");
	
	// Open fbdevices file
	FILE *fp = fopen(fbd_path ? fbd_path : FBDEVICES_FILE_NAME, "r");
	if (fp == NULL) {
		char str[256];
		sprintf(str, GetString(STR_NO_FBDEVICE_FILE_ERR), fbd_path ? fbd_path : FBDEVICES_FILE_NAME, strerror(errno));
		ErrorAlert(str);
		return;
	}
	
	int fb_depth;		// supported depth
	uint32 fb_offset;	// offset used for mmap(2)
	char fb_name[20];
	char line[256];
	bool device_found = false;
	while (fgets(line, 255, fp)) {
		// Read line
		int len = strlen(line);
		if (len == 0)
			continue;
		line[len - 1] = '\0';
		
		// Comments begin with "#" or ";"
		if ((line[0] == '#') || (line[0] == ';') || (line[0] == '\0'))
			continue;
		
		if ((sscanf(line, "%19s %d %x", fb_name, &fb_depth, &fb_offset) == 3)
		 && (strcmp(fb_name, fb_name) == 0) && (fb_depth == max_depth)) {
			device_found = true;
			break;
		}
	}
	
	// fbdevices file completely read
	fclose(fp);
	
	// Frame buffer name not found ? Then, display warning
	if (!device_found) {
		char str[256];
		sprintf(str, GetString(STR_FBDEV_NAME_ERR), fb_name, max_depth);
		ErrorAlert(str);
		return;
	}
	
	// Create window
	XSetWindowAttributes wattr;
	wattr.event_mask = eventmask = dga_eventmask;
	wattr.background_pixel = white_pixel;
	wattr.override_redirect = True;
	wattr.colormap = cmap[0];
	
	w = XCreateWindow(x_display, rootwin,
		0, 0, width, height,
		0, xdepth, InputOutput, vis,
		CWEventMask | CWBackPixel | CWOverrideRedirect | (fb_depth <= 8 ? CWColormap : 0),
		&wattr);

	// Set window name/class
	set_window_name(w, STR_WINDOW_TITLE);

	// Indicate that we want keyboard input
	set_window_focus(w);

	// Show window
	XMapRaised(x_display, w);
	wait_mapped(w);
	
	// Grab mouse and keyboard
	XGrabKeyboard(x_display, w, True,
		GrabModeAsync, GrabModeAsync, CurrentTime);
	XGrabPointer(x_display, w, True,
		PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
		GrabModeAsync, GrabModeAsync, w, None, CurrentTime);
	disable_mouse_accel();
	
	// Calculate bytes per row
	int bytes_per_row = TrivialBytesPerRow(mode.x, mode.depth);
	
	// Map frame buffer
	the_buffer_size = height * bytes_per_row;
	if ((the_buffer = (uint8 *) mmap(NULL, the_buffer_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fbdev_fd, fb_offset)) == MAP_FAILED) {
		if ((the_buffer = (uint8 *) mmap(NULL, the_buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fbdev_fd, fb_offset)) == MAP_FAILED) {
			char str[256];
			sprintf(str, GetString(STR_FBDEV_MMAP_ERR), strerror(errno));
			ErrorAlert(str);
			return;
		}
	}
	
#if ENABLE_VOSF
#if REAL_ADDRESSING || DIRECT_ADDRESSING
	// Screen_blitter_init() returns TRUE if VOSF is mandatory
	// i.e. the framebuffer update function is not Blit_Copy_Raw
	use_vosf = Screen_blitter_init(visualFormat, true, mode.depth);
	
	if (use_vosf) {
	  // Allocate memory for frame buffer (SIZE is extended to page-boundary)
	  the_host_buffer = the_buffer;
	  the_buffer_size = page_extend((height + 2) * bytes_per_row);
	  the_buffer_copy = (uint8 *)malloc(the_buffer_size);
	  the_buffer = (uint8 *)vm_acquire_framebuffer(the_buffer_size);

	  // Fake image for DGA/VOSF mode to know about display bounds
	  img = new FakeXImage(width, height, depth_of_video_mode(mode));
	}
#else
	use_vosf = false;
#endif
#endif
	
	// Set frame buffer base
	const_cast<video_mode *>(&mode)->bytes_per_row = bytes_per_row;
	const_cast<video_mode *>(&mode)->depth = DepthModeForPixelDepth(fb_depth);
	set_mac_frame_buffer(monitor, mode.depth, true);

	// Everything went well
	init_ok = true;
}

// Close display
driver_fbdev::~driver_fbdev()
{
	if (!use_vosf) {
		if (the_buffer != MAP_FAILED) {
			// don't free() the screen buffer in driver_base dtor
			munmap(the_buffer, the_buffer_size);
			the_buffer = NULL;
		}
	}
#ifdef ENABLE_VOSF
	else {
		if (the_host_buffer != MAP_FAILED) {
			// don't free() the screen buffer in driver_base dtor
			munmap(the_host_buffer, the_buffer_size);
			the_host_buffer = NULL;
		}
	}
#endif
}
#endif


#ifdef ENABLE_XF86_DGA
/*
 *  XFree86 DGA display driver
 */

class driver_xf86dga : public driver_dga {
public:
	driver_xf86dga(X11_monitor_desc &monitor);
	~driver_xf86dga();

	void update_palette(void);
	void resume(void);

private:
	int current_dga_cmap;					// Number (0 or 1) of currently installed DGA colormap
};

// Open display
driver_xf86dga::driver_xf86dga(X11_monitor_desc &m)
 : driver_dga(m), current_dga_cmap(0)
{
	int width = mode.x, height = mode.y;

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
		XSync(x_display, false);
	}
#endif

	// Create window
	XSetWindowAttributes wattr;
	wattr.event_mask = eventmask = dga_eventmask;
	wattr.override_redirect = True;
	wattr.colormap = (mode.depth == VDEPTH_1BIT ? DefaultColormap(x_display, screen) : cmap[0]);

	w = XCreateWindow(x_display, rootwin, 0, 0, width, height, 0, xdepth,
		InputOutput, vis, CWEventMask | CWOverrideRedirect |
		(color_class == DirectColor ? CWColormap : 0), &wattr);

	// Set window name/class
	set_window_name(w, STR_WINDOW_TITLE);

	// Indicate that we want keyboard input
	set_window_focus(w);

	// Show window
	XMapRaised(x_display, w);
	wait_mapped(w);

	// Establish direct screen connection
	XMoveResizeWindow(x_display, w, 0, 0, width, height);
	XWarpPointer(x_display, None, rootwin, 0, 0, 0, 0, 0, 0);
	XGrabKeyboard(x_display, rootwin, True, GrabModeAsync, GrabModeAsync, CurrentTime);
	XGrabPointer(x_display, rootwin, True, PointerMotionMask | ButtonPressMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
	disable_mouse_accel();

	int v_width, v_bank, v_size;
	XF86DGAGetVideo(x_display, screen, (char **)&the_buffer, &v_width, &v_bank, &v_size);
	XF86DGADirectVideo(x_display, screen, XF86DGADirectGraphics | XF86DGADirectKeyb | XF86DGADirectMouse);
	XF86DGASetViewPort(x_display, screen, 0, 0);
	XF86DGASetVidPage(x_display, screen, 0);

	// Set colormap
	if (!IsDirectMode(mode)) {
		XSetWindowColormap(x_display, w, cmap[current_dga_cmap = 0]);
		XF86DGAInstallColormap(x_display, screen, cmap[current_dga_cmap]);
	}
	XSync(x_display, false);

	// Init blitting routines
	int bytes_per_row = TrivialBytesPerRow((v_width + 7) & ~7, mode.depth);
#if ENABLE_VOSF
#if REAL_ADDRESSING || DIRECT_ADDRESSING
	// Screen_blitter_init() returns TRUE if VOSF is mandatory
	// i.e. the framebuffer update function is not Blit_Copy_Raw
	use_vosf = Screen_blitter_init(visualFormat, x_native_byte_order, depth_of_video_mode(mode));
	
	if (use_vosf) {
	  // Allocate memory for frame buffer (SIZE is extended to page-boundary)
	  the_host_buffer = the_buffer;
	  the_buffer_size = page_extend((height + 2) * bytes_per_row);
	  the_buffer_copy = (uint8 *)malloc(the_buffer_size);
	  the_buffer = (uint8 *)vm_acquire_framebuffer(the_buffer_size);

	  // Fake image for DGA/VOSF mode to know about display bounds
	  img = new FakeXImage((v_width + 7) & ~7, height, depth_of_video_mode(mode));
	}
#else
	use_vosf = false;
#endif
#endif
	
	// Set frame buffer base
	const_cast<video_mode *>(&mode)->bytes_per_row = bytes_per_row;
	set_mac_frame_buffer(monitor, mode.depth, true);

	// Everything went well
	init_ok = true;
}

// Close display
driver_xf86dga::~driver_xf86dga()
{
	XF86DGADirectVideo(x_display, screen, 0);
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
#ifdef ENABLE_XF86_VIDMODE
	if (has_vidmode)
		XF86VidModeSwitchToMode(x_display, screen, x_video_modes[0]);
#endif
}

// Palette has changed
void driver_xf86dga::update_palette(void)
{
	driver_dga::update_palette();
	current_dga_cmap ^= 1;
	if (!IsDirectMode(monitor.get_current_mode()) && cmap[current_dga_cmap])
		XF86DGAInstallColormap(x_display, screen, cmap[current_dga_cmap]);
}

// Resume emulation
void driver_xf86dga::resume(void)
{
	driver_dga::resume();
	if (!IsDirectMode(monitor.get_current_mode()))
		XF86DGAInstallColormap(x_display, screen, cmap[current_dga_cmap]);
}
#endif


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
		// Force use of MacX mappings on MacOS X with Apple's X server
		int dummy;
		if (XQueryExtension(x_display, "Apple-DRI", &dummy, &dummy, &dummy))
			vendor = "MacX";
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

// Open display for current mode
bool X11_monitor_desc::video_open(void)
{
	D(bug("video_open()\n"));
	const video_mode &mode = get_current_mode();
	// set Mac screen global variabls
	MacScreenWidth = VIDEO_MODE_X;
	MacScreenHeight = VIDEO_MODE_Y;
	D(bug("Set Mac Screen Width: %d, Mac Screen Height: %d\n", MacScreenWidth, MacScreenHeight));

	// Find best available X visual
	if (!find_visual_for_depth(mode.depth)) {
		ErrorAlert(STR_NO_XVISUAL_ERR);
		return false;
	}

	// Determine the byte order of an XImage content
#ifdef WORDS_BIGENDIAN
	x_native_byte_order = (XImageByteOrder(x_display) == MSBFirst);
#else
	x_native_byte_order = (XImageByteOrder(x_display) == LSBFirst);
#endif

	// Build up visualFormat structure
	visualFormat.fullscreen = (display_type == DISPLAY_DGA);
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
	if (!IsDirectMode(mode) && xdepth > 8)
		for (int i=0; i<256; i++)
			ExpandMap[i] = map_rgb(i, i, i, true);
#endif

	// Create display driver object of requested type
	switch (display_type) {
		case DISPLAY_WINDOW:
			drv = new driver_window(*this);
			break;
#ifdef ENABLE_FBDEV_DGA
		case DISPLAY_DGA:
			drv = new driver_fbdev(*this);
			break;
#endif
#ifdef ENABLE_XF86_DGA
		case DISPLAY_DGA:
			drv = new driver_xf86dga(*this);
			break;
#endif
	}
	if (drv == NULL)
		return false;
	if (!drv->init_ok) {
		delete drv;
		drv = NULL;
		return false;
	}

#ifdef ENABLE_VOSF
	if (use_vosf) {
		// Initialize the VOSF system
		if (!video_vosf_init(*this)) {
			ErrorAlert(STR_VOSF_INIT_ERR);
        	return false;
		}
	}
#endif
	
	// Initialize VideoRefresh function
	VideoRefreshInit();

	// Lock down frame buffer
	XSync(x_display, false);
	LOCK_FRAME_BUFFER;

	// Start redraw/input thread
#ifdef USE_PTHREADS_SERVICES
	redraw_thread_cancel = false;
	Set_pthread_attr(&redraw_thread_attr, 0);
	redraw_thread_active = (pthread_create(&redraw_thread, &redraw_thread_attr, redraw_func, NULL) == 0);
	if (!redraw_thread_active) {
		printf("FATAL: cannot create redraw thread\n");
		return false;
	}
#else
	redraw_thread_active = true;
#endif

	return true;
}

bool VideoInit(bool classic)
{
	classic_mode = classic;

#ifdef ENABLE_VOSF
	// Zero the mainBuffer structure
	mainBuffer.dirtyPages = NULL;
	mainBuffer.pageInfo = NULL;
#endif
	
	// Check if X server runs on local machine
	local_X11 = (strncmp(XDisplayName(x_display_name), ":", 1) == 0)
 	         || (strncmp(XDisplayName(x_display_name), "/", 1) == 0)
	         || (strncmp(XDisplayName(x_display_name), "unix:", 5) == 0);
    
	// Init keycode translation
	keycode_init();

	// Read prefs
	frame_skip = PrefsFindInt32("frameskip");
	mouse_wheel_mode = PrefsFindInt32("mousewheelmode");
	mouse_wheel_lines = PrefsFindInt32("mousewheellines");

	// Find screen and root window
	screen = XDefaultScreen(x_display);
	rootwin = XRootWindow(x_display, screen);

	// Get sorted list of available depths
	avail_depths = XListDepths(x_display, screen, &num_depths);
	if (avail_depths == NULL) {
		ErrorAlert(STR_UNSUPP_DEPTH_ERR);
		return false;
	}
	std::sort(avail_depths, avail_depths + num_depths);
	
#ifdef ENABLE_FBDEV_DGA
	// Frame buffer name
	char fb_name[20];
	
	// Could do fbdev DGA?
	if ((fbdev_fd = open(FBDEVICE_FILE_NAME, O_RDWR)) != -1)
		has_dga = true;
	else
		has_dga = false;
#endif

#ifdef ENABLE_XF86_DGA
	// DGA available?
	int dga_event_base, dga_error_base;
	if (local_X11 && XF86DGAQueryExtension(x_display, &dga_event_base, &dga_error_base)) {
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

	// Get screen mode from preferences
	const char *mode_str;
	mode_str = PrefsFindString("screen");

	// Determine display type and default dimensions
	int default_width, default_height;
	if (classic) {
		default_width = 512;
		default_height = 342;
	} else {
		default_width = 640;
		default_height = 480;
	}
	display_type = DISPLAY_WINDOW;
	if (mode_str) {
		if (sscanf(mode_str, "win/%d/%d", &default_width, &default_height) == 2) {
			display_type = DISPLAY_WINDOW;
#ifdef ENABLE_FBDEV_DGA
		} else if (has_dga && sscanf(mode_str, "dga/%19s", fb_name) == 1) {
			display_type = DISPLAY_DGA;
			default_width = -1; default_height = -1; // use entire screen
#endif
#ifdef ENABLE_XF86_DGA
		} else if (has_dga && sscanf(mode_str, "dga/%d/%d", &default_width, &default_height) == 2) {
			display_type = DISPLAY_DGA;
#endif
		}
	}
	if (default_width <= 0)
		default_width = DisplayWidth(x_display, screen);
	else if (default_width > DisplayWidth(x_display, screen))
		default_width = DisplayWidth(x_display, screen);
	if (default_height <= 0)
		default_height = DisplayHeight(x_display, screen);
	else if (default_height > DisplayHeight(x_display, screen))
		default_height = DisplayHeight(x_display, screen);

	// for classic Mac, make sure the display width is divisible by 8
	if (classic) {
		default_width = (default_width / 8) * 8;
	}

	// Mac screen depth follows X depth
	video_depth default_depth = VDEPTH_1BIT;
	switch (DefaultDepth(x_display, screen)) {
		case 8:
			default_depth = VDEPTH_8BIT;
			break;
		case 15: case 16:
			default_depth = VDEPTH_16BIT;
			break;
		case 24: case 32:
			default_depth = VDEPTH_32BIT;
			break;
	}

	// Construct list of supported modes
	if (display_type == DISPLAY_WINDOW) {
		if (classic)
			add_mode(default_width, default_height, 0x80, default_width/8, VDEPTH_1BIT);
		else {
			for (unsigned d=VDEPTH_1BIT; d<=VDEPTH_32BIT; d++) {
				if (find_visual_for_depth(video_depth(d)))
					add_window_modes(video_depth(d));
			}
		}
	} else
		add_mode(default_width, default_height, 0x80, TrivialBytesPerRow(default_width, default_depth), default_depth);
	if (VideoModes.empty()) {
		ErrorAlert(STR_NO_XVISUAL_ERR);
		return false;
	}

	// Find requested default mode with specified dimensions
	uint32 default_id;
	std::vector<video_mode>::const_iterator i, end = VideoModes.end();
	for (i = VideoModes.begin(); i != end; ++i) {
		if (i->x == default_width && i->y == default_height && i->depth == default_depth) {
			default_id = i->resolution_id;
			break;
		}
	}
	if (i == end) { // not found, use first available mode
		default_depth = VideoModes[0].depth;
		default_id = VideoModes[0].resolution_id;
	}

#if DEBUG
	D(bug("Available video modes:\n"));
	for (i = VideoModes.begin(); i != end; ++i) {
		int bits = 1 << i->depth;
		if (bits == 16)
			bits = 15;
		else if (bits == 32)
			bits = 24;
		D(bug(" %dx%d (ID %02x), %d colors\n", i->x, i->y, i->resolution_id, 1 << bits));
	}
#endif

	// Create X11_monitor_desc for this (the only) display
	X11_monitor_desc *monitor = new X11_monitor_desc(VideoModes, default_depth, default_id);
	VideoMonitors.push_back(monitor);

	// Open display
	return monitor->video_open();
}


/*
 *  Deinitialization
 */

// Close display
void X11_monitor_desc::video_close(void)
{
	D(bug("video_close()\n"));

	// Stop redraw thread
#ifdef USE_PTHREADS_SERVICES
	if (redraw_thread_active) {
		redraw_thread_cancel = true;
		redraw_thread_cancel_ack = false;
		pthread_join(redraw_thread, NULL);
		while (!redraw_thread_cancel_ack) ;
	}
#endif
	redraw_thread_active = false;

	// Unlock frame buffer
	UNLOCK_FRAME_BUFFER;
	XSync(x_display, false);
	D(bug(" frame buffer unlocked\n"));

#ifdef ENABLE_VOSF
	if (use_vosf) {
		// Deinitialize VOSF
		video_vosf_exit();
	}
#endif

	// Close display
	delete drv;
	drv = NULL;

	// Free colormaps
	if (cmap[0]) {
		XFreeColormap(x_display, cmap[0]);
		cmap[0] = 0;
	}
	if (cmap[1]) {
		XFreeColormap(x_display, cmap[1]);
		cmap[1] = 0;
	}
}

void VideoExit(void)
{
	// Close displays
	vector<monitor_desc *>::iterator i, end = VideoMonitors.end();
	for (i = VideoMonitors.begin(); i != end; ++i)
		dynamic_cast<X11_monitor_desc *>(*i)->video_close();

#ifdef ENABLE_XF86_VIDMODE
	// Free video mode list
	if (x_video_modes) {
		XFree(x_video_modes);
		x_video_modes = NULL;
	}
#endif

#ifdef ENABLE_FBDEV_DGA
	// Close framebuffer device
	if (fbdev_fd >= 0) {
		close(fbdev_fd);
		fbdev_fd = -1;
	}
#endif

	// Free depth list
	if (avail_depths) {
		XFree(avail_depths);
		avail_depths = NULL;
	}
}


/*
 *  Close down full-screen mode (if bringing up error alerts is unsafe while in full-screen mode)
 */

void VideoQuitFullScreen(void)
{
	D(bug("VideoQuitFullScreen()\n"));
	quit_full_screen = true;
}


/*
 *  Mac VBL interrupt
 */

void VideoInterrupt(void)
{
	// Emergency quit requested? Then quit
	if (emerg_quit)
		QuitEmulator();

	// Temporarily give up frame buffer lock (this is the point where
	// we are suspended when the user presses Ctrl-Tab)
	UNLOCK_FRAME_BUFFER;
	LOCK_FRAME_BUFFER;
}


/*
 *  Set palette
 */

void X11_monitor_desc::set_palette(uint8 *pal, int num_in)
{
	const video_mode &mode = get_current_mode();

	LOCK_PALETTE;

	// Convert colors to XColor array
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
		p->red = pal[c*3 + 0] * 0x0101;
		p->green = pal[c*3 + 1] * 0x0101;
		p->blue = pal[c*3 + 2] * 0x0101;
		p++;
	}

#ifdef ENABLE_VOSF
	// Recalculate pixel color expansion map
	if (!IsDirectMode(mode) && xdepth > 8) {
		for (int i=0; i<256; i++) {
			int c = i & (num_in-1); // If there are less than 256 colors, we repeat the first entries (this makes color expansion easier)
			ExpandMap[i] = map_rgb(pal[c*3+0], pal[c*3+1], pal[c*3+2], true);
		}

		// We have to redraw everything because the interpretation of pixel values changed
		LOCK_VOSF;
		PFLAG_SET_ALL;
		UNLOCK_VOSF;
		memset(the_buffer_copy, 0, mode.bytes_per_row * mode.y);
	}
#endif

	// Tell redraw thread to change palette
	x_palette_changed = true;

	UNLOCK_PALETTE;
}


/*
 *  Switch video mode
 */

void X11_monitor_desc::switch_to_current_mode(void)
{
	// Close and reopen display
	video_close();
	video_open();

	if (drv == NULL) {
		ErrorAlert(STR_OPEN_WINDOW_ERR);
		QuitEmulator();
	}
}


/*
 *  Translate key event to Mac keycode, returns -1 if no keycode was found
 *  and -2 if the key was recognized as a hotkey
 */

static int kc_decode(KeySym ks, bool key_down)
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

		case XK_Tab: if (ctrl_down) {if (key_down) drv->suspend(); return -2;} else return 0x30;
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

		case XK_Escape: if (ctrl_down) {if (key_down) { quit_full_screen = true; emerg_quit = true; } return -2;} else return 0x35;

		case XK_F1: if (ctrl_down) {if (key_down) SysMountFirstFloppy(); return -2;} else return 0x7a;
		case XK_F2: return 0x78;
		case XK_F3: return 0x63;
		case XK_F4: return 0x76;
		case XK_F5: if (ctrl_down) {if (key_down) drv->toggle_mouse_grab(); return -2;} else return 0x60;
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
		int as = kc_decode(ks, key_down);
		if (as >= 0)
			return as;
		if (as == -2)
			return as;
	} while (ks != NoSymbol);

	return -1;
}


/*
 *  X event handling
 */

static void handle_events(void)
{
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
		
		switch (event.type) {

			// Mouse button
			case ButtonPress: {
				unsigned int button = event.xbutton.button;
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
				unsigned int button = event.xbutton.button;
				if (button < 4)
					ADBMouseUp(button - 1);
				break;
			}

			// Mouse moved
			case MotionNotify:
				drv->mouse_moved(event.xmotion.x, event.xmotion.y);
				break;

			// Mouse entered window
			case EnterNotify:
				if (event.xcrossing.mode != NotifyGrab && event.xcrossing.mode != NotifyUngrab)
					drv->mouse_moved(event.xmotion.x, event.xmotion.y);
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
							drv->resume();	// Space wakes us up
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

			// Hidden parts exposed, force complete refresh of window
			case Expose:
				if (display_type == DISPLAY_WINDOW) {
					const video_mode &mode = VideoMonitors[0]->get_current_mode();
#ifdef ENABLE_VOSF
					if (use_vosf) {			// VOSF refresh
						LOCK_VOSF;
						PFLAG_SET_ALL;
						UNLOCK_VOSF;
						memset(the_buffer_copy, 0, mode.bytes_per_row * mode.y);
					}
					else
#endif
					if (frame_skip == 0) {	// Dynamic refresh
						int x1, y1;
						for (y1=0; y1<16; y1++)
						for (x1=0; x1<16; x1++)
							updt_box[x1][y1] = true;
						nr_boxes = 16 * 16;
					} else					// Static refresh
						memset(the_buffer_copy, 0, mode.bytes_per_row * mode.y);
				}
				break;
		}

		XDisplayUnlock();
	}
}


/*
 *  Window display update
 */

// Dynamic display update (variable frame rate for each box)
static void update_display_dynamic(int ticker, driver_window *drv)
{
	int y1, y2, y2s, y2a, i, x1, xm, xmo, ymo, yo, yi, yil, xi;
	int xil = 0;
	int rxm = 0, rxmo = 0;
	const video_mode &mode = drv->monitor.get_current_mode();
	int bytes_per_row = mode.bytes_per_row;
	int bytes_per_pixel = mode.bytes_per_row / mode.x;
	int rx = mode.bytes_per_row / 16;
	int ry = mode.y / 16;
	int max_box;

	y2s = sm_uptd[ticker % 8];
	y2a = 8;
	for (i = 0; i < 6; i++) {
		if (ticker % (2 << i))
			break;
	}
	max_box = sm_no_boxes[i];

	if (y2a) {
		for (y1=0; y1<16; y1++) {
			for (y2=y2s; y2 < ry; y2 += y2a) {
				i = ((y1 * ry) + y2) * bytes_per_row; 
				for (x1=0; x1<16; x1++, i += rx) {
					if (updt_box[x1][y1] == false) {
						if (memcmp(&the_buffer_copy[i], &the_buffer[i], rx)) {
							updt_box[x1][y1] = true;
							nr_boxes++;
						}
					}
				}
			}
		}
	}

	XDisplayLock();
	if ((nr_boxes <= max_box) && (nr_boxes)) {
		for (y1=0; y1<16; y1++) {
			for (x1=0; x1<16; x1++) {
				if (updt_box[x1][y1] == true) {
					if (rxm == 0)
						xm = x1;
					rxm += rx; 
					updt_box[x1][y1] = false;
				}
				if (((updt_box[x1+1][y1] == false) || (x1 == 15)) && (rxm)) {
					if ((rxmo != rxm) || (xmo != xm) || (yo != y1 - 1)) {
						if (rxmo) {
							xi = xmo * rx;
							yi = ymo * ry;
							xil = rxmo;
							yil = (yo - ymo +1) * ry;
						}
						rxmo = rxm;
						xmo = xm;
						ymo = y1;
					}
					rxm = 0;
					yo = y1;
				}	
				if (xil) {
					i = (yi * bytes_per_row) + xi;
					for (y2=0; y2 < yil; y2++, i += bytes_per_row)
						memcpy(&the_buffer_copy[i], &the_buffer[i], xil);
					if (mode.depth == VDEPTH_1BIT) {
						if (drv->have_shm)
							XShmPutImage(x_display, drv->w, drv->gc, drv->img, xi * 8, yi, xi * 8, yi, xil * 8, yil, 0);
						else
							XPutImage(x_display, drv->w, drv->gc, drv->img, xi * 8, yi, xi * 8, yi, xil * 8, yil);
					} else {
						if (drv->have_shm)
							XShmPutImage(x_display, drv->w, drv->gc, drv->img, xi / bytes_per_pixel, yi, xi / bytes_per_pixel, yi, xil / bytes_per_pixel, yil, 0);
						else
							XPutImage(x_display, drv->w, drv->gc, drv->img, xi / bytes_per_pixel, yi, xi / bytes_per_pixel, yi, xil / bytes_per_pixel, yil);
					}
					xil = 0;
				}
				if ((x1 == 15) && (y1 == 15) && (rxmo)) {
					x1--;
					xi = xmo * rx;
					yi = ymo * ry;
					xil = rxmo;
					yil = (yo - ymo +1) * ry;
					rxmo = 0;
				}
			}
		}
		nr_boxes = 0;
	}
	XDisplayUnlock();
}

// Static display update (fixed frame rate, but incremental)
static void update_display_static(driver_window *drv)
{
	// Incremental update code
	unsigned wide = 0, high = 0, x1, x2, y1, y2, i, j;
	const video_mode &mode = drv->monitor.get_current_mode();
	int bytes_per_row = mode.bytes_per_row;
	int bytes_per_pixel = mode.bytes_per_row / mode.x;
	uint8 *p, *p2;

	// Check for first line from top and first line from bottom that have changed
	y1 = 0;
	for (j=0; j<mode.y; j++) {
		if (memcmp(&the_buffer[j * bytes_per_row], &the_buffer_copy[j * bytes_per_row], bytes_per_row)) {
			y1 = j;
			break;
		}
	}
	y2 = y1 - 1;
	for (j=mode.y-1; j>=y1; j--) {
		if (memcmp(&the_buffer[j * bytes_per_row], &the_buffer_copy[j * bytes_per_row], bytes_per_row)) {
			y2 = j;
			break;
		}
	}
	high = y2 - y1 + 1;

	// Check for first column from left and first column from right that have changed
	if (high) {
		if (mode.depth == VDEPTH_1BIT) {
			x1 = mode.x - 1;
			for (j=y1; j<=y2; j++) {
				p = &the_buffer[j * bytes_per_row];
				p2 = &the_buffer_copy[j * bytes_per_row];
				for (i=0; i<(x1>>3); i++) {
					if (*p != *p2) {
						x1 = i << 3;
						break;
					}
					p++; p2++;
				}
			}
			x2 = x1;
			for (j=y1; j<=y2; j++) {
				p = &the_buffer[j * bytes_per_row];
				p2 = &the_buffer_copy[j * bytes_per_row];
				p += bytes_per_row;
				p2 += bytes_per_row;
				for (i=(mode.x>>3); i>(x2>>3); i--) {
					p--; p2--;
					if (*p != *p2) {
						x2 = (i << 3) + 7;
						break;
					}
				}
			}
			wide = x2 - x1 + 1;

			// Update copy of the_buffer
			if (high && wide) {
				for (j=y1; j<=y2; j++) {
					i = j * bytes_per_row + (x1 >> 3);
					memcpy(the_buffer_copy + i, the_buffer + i, wide >> 3);
				}
			}

		} else {
			x1 = mode.x;
			for (j=y1; j<=y2; j++) {
				p = &the_buffer[j * bytes_per_row];
				p2 = &the_buffer_copy[j * bytes_per_row];
				for (i=0; i<x1*bytes_per_pixel; i++) {
					if (*p != *p2) {
						x1 = i / bytes_per_pixel;
						break;
					}
					p++; p2++;
				}
			}
			x2 = x1;
			for (j=y1; j<=y2; j++) {
				p = &the_buffer[j * bytes_per_row];
				p2 = &the_buffer_copy[j * bytes_per_row];
				p += bytes_per_row;
				p2 += bytes_per_row;
				for (i=mode.x*bytes_per_pixel; i>x2*bytes_per_pixel; i--) {
					p--;
					p2--;
					if (*p != *p2) {
						x2 = i / bytes_per_pixel;
						break;
					}
				}
			}
			wide = x2 - x1;

			// Update copy of the_buffer
			if (high && wide) {
				for (j=y1; j<=y2; j++) {
					i = j * bytes_per_row + x1 * bytes_per_pixel;
					memcpy(the_buffer_copy + i, the_buffer + i, bytes_per_pixel * wide);
				}
			}
		}
	}

	// Refresh display
	XDisplayLock();
	if (high && wide) {
		if (drv->have_shm)
			XShmPutImage(x_display, drv->w, drv->gc, drv->img, x1, y1, x1, y1, wide, high, 0);
		else
			XPutImage(x_display, drv->w, drv->gc, drv->img, x1, y1, x1, y1, wide, high);
	}
	XDisplayUnlock();
}


/*
 *	Screen refresh functions
 */

// We suggest the compiler to inline the next two functions so that it
// may specialise the code according to the current screen depth and
// display type. A clever compiler would do that job by itself though...

// NOTE: update_display_vosf is inlined too

static inline void possibly_quit_dga_mode()
{
	// Quit DGA mode if requested (something terrible has happened and we
	// want to give control back to the user)
	if (quit_full_screen) {
		quit_full_screen = false;
		delete drv;
		drv = NULL;
	}
}

static inline void possibly_ungrab_mouse()
{
	// Ungrab mouse if requested (something terrible has happened and we
	// want to give control back to the user)
	if (quit_full_screen) {
		quit_full_screen = false;
		if (drv)
			drv->ungrab_mouse();
	}
}

static inline void handle_palette_changes(void)
{
	LOCK_PALETTE;

	if (x_palette_changed) {
		x_palette_changed = false;
		XDisplayLock();
		drv->update_palette();
		XDisplayUnlock();
	}

	UNLOCK_PALETTE;
}

static void video_refresh_dga(void)
{
	// Quit DGA mode if requested
	possibly_quit_dga_mode();
}

#ifdef ENABLE_VOSF
#if REAL_ADDRESSING || DIRECT_ADDRESSING
static void video_refresh_dga_vosf(void)
{
	// Quit DGA mode if requested
	possibly_quit_dga_mode();
	
	// Update display (VOSF variant)
	static int tick_counter = 0;
	if (++tick_counter >= frame_skip) {
		tick_counter = 0;
		if (mainBuffer.dirty) {
			LOCK_VOSF;
			update_display_dga_vosf(static_cast<driver_dga *>(drv));
			UNLOCK_VOSF;
		}
	}
}
#endif

static void video_refresh_window_vosf(void)
{
	// Ungrab mouse if requested
	possibly_ungrab_mouse();
	
	// Update display (VOSF variant)
	static int tick_counter = 0;
	if (++tick_counter >= frame_skip) {
		tick_counter = 0;
		if (mainBuffer.dirty) {
			XDisplayLock();
			LOCK_VOSF;
			update_display_window_vosf(static_cast<driver_window *>(drv));
			UNLOCK_VOSF;
			XSync(x_display, false); // Let the server catch up
			XDisplayUnlock();
		}
	}
}
#endif // def ENABLE_VOSF

static void video_refresh_window_static(void)
{
	// Ungrab mouse if requested
	possibly_ungrab_mouse();

	// Update display (static variant)
	static int tick_counter = 0;
	if (++tick_counter >= frame_skip) {
		tick_counter = 0;
		update_display_static(static_cast<driver_window *>(drv));
	}
}

static void video_refresh_window_dynamic(void)
{
	// Ungrab mouse if requested
	possibly_ungrab_mouse();

	// Update display (dynamic variant)
	static int tick_counter = 0;
	tick_counter++;
	update_display_dynamic(tick_counter, static_cast<driver_window *>(drv));
}


/*
 *  Thread for screen refresh, input handling etc.
 */

static void VideoRefreshInit(void)
{
	// TODO: set up specialised 8bpp VideoRefresh handlers ?
	if (display_type == DISPLAY_DGA) {
#if ENABLE_VOSF && (REAL_ADDRESSING || DIRECT_ADDRESSING)
		if (use_vosf)
			video_refresh = video_refresh_dga_vosf;
		else
#endif
			video_refresh = video_refresh_dga;
	}
	else {
#ifdef ENABLE_VOSF
		if (use_vosf)
			video_refresh = video_refresh_window_vosf;
		else
#endif
		if (frame_skip == 0)
			video_refresh = video_refresh_window_dynamic;
		else
			video_refresh = video_refresh_window_static;
	}
}

// This function is called on non-threaded platforms from a timer interrupt
void VideoRefresh(void)
{
	// We need to check redraw_thread_active to inhibit refreshed during
	// mode changes on non-threaded platforms
	if (!redraw_thread_active)
		return;

	// Handle X events
	handle_events();

	// Handle palette changes
	handle_palette_changes();

	// Update display
	video_refresh();
}

const int VIDEO_REFRESH_HZ = 60;
const int VIDEO_REFRESH_DELAY = 1000000 / VIDEO_REFRESH_HZ;

#ifdef USE_PTHREADS_SERVICES
static void *redraw_func(void *arg)
{
	int fd = ConnectionNumber(x_display);

	uint64 start = GetTicks_usec();
	int64 ticks = 0;
	uint64 next = GetTicks_usec() + VIDEO_REFRESH_DELAY;

	while (!redraw_thread_cancel) {

		int64 delay = next - GetTicks_usec();
		if (delay < -VIDEO_REFRESH_DELAY) {

			// We are lagging far behind, so we reset the delay mechanism
			next = GetTicks_usec();

		} else if (delay <= 0) {

			// Delay expired, refresh display
			handle_events();
			handle_palette_changes();
			video_refresh();
			next += VIDEO_REFRESH_DELAY;
			ticks++;

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

	uint64 end = GetTicks_usec();
	D(bug("%lld refreshes in %lld usec = %f refreshes/sec\n", ticks, end - start, ticks * 1000000.0 / (end - start)));

	redraw_thread_cancel_ack = true;
	return NULL;
}
#endif
