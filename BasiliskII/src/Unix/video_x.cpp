/*
 *  video_x.cpp - Video/graphics emulation, X11 specific stuff
 *
 *  Basilisk II (C) 1997-2001 Christian Bauer
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
 */

#include "sysdeps.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>

#ifdef HAVE_PTHREADS
# include <pthread.h>
#endif

#ifdef ENABLE_XF86_DGA
# include <X11/extensions/xf86dga.h>
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

#define DEBUG 0
#include "debug.h"


// Display types
enum {
	DISPLAY_WINDOW,	// X11 window, using MIT SHM extensions if possible
	DISPLAY_DGA		// DGA fullscreen display
};

// Constants
const char KEYCODE_FILE_NAME[] = DATADIR "/keycodes";

static const int win_eventmask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | EnterWindowMask | ExposureMask | StructureNotifyMask;
static const int dga_eventmask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | StructureNotifyMask;


// Global variables
static int32 frame_skip;							// Prefs items
static int16 mouse_wheel_mode;
static int16 mouse_wheel_lines;

static int display_type = DISPLAY_WINDOW;			// See enum above
static bool local_X11;								// Flag: X server running on local machine?
static uint8 *the_buffer = NULL;					// Mac frame buffer (where MacOS draws into)
static uint8 *the_buffer_copy = NULL;				// Copy of Mac frame buffer (for refreshed modes)

static bool redraw_thread_active = false;			// Flag: Redraw thread installed
#ifdef HAVE_PTHREADS
static volatile bool redraw_thread_cancel;			// Flag: Cancel Redraw thread
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
static int screen;									// Screen number
static int xdepth;									// Depth of X screen
static Window rootwin;								// Root window and our window
static XVisualInfo visualInfo;
static Visual *vis;
static Colormap cmap[2] = {0, 0};					// Colormaps for indexed modes (DGA needs two of them)
static XColor black, white;
static unsigned long black_pixel, white_pixel;
static int eventmask;

static int rshift, rloss, gshift, gloss, bshift, bloss;	// Pixel format of DirectColor/TrueColor modes

static XColor palette[256];							// Color palette to be used as CLUT and gamma table
static bool palette_changed = false;				// Flag: Palette changed, redraw thread must set new colors

#ifdef ENABLE_FBDEV_DGA
static int fbdev_fd = -1;
#endif

#ifdef ENABLE_XF86_VIDMODE
static XF86VidModeModeInfo **x_video_modes = NULL;	// Array of all available modes
static int num_x_video_modes;
#endif

// Mutex to protect palette
#ifdef HAVE_PTHREADS
static pthread_mutex_t palette_lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK_PALETTE pthread_mutex_lock(&palette_lock)
#define UNLOCK_PALETTE pthread_mutex_unlock(&palette_lock)
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
static int event2keycode(XKeyEvent &ev);

// From main_unix.cpp
extern char *x_display_name;
extern Display *x_display;

// From sys_unix.cpp
extern void SysMountFirstFloppy(void);


/*
 *  Utility functions
 */

// Add mode to list of supported modes
static void add_mode(uint32 width, uint32 height, uint32 resolution_id, uint32 bytes_per_row, video_depth depth)
{
	video_mode mode;
	mode.x = width;
	mode.y = height;
	mode.resolution_id = resolution_id;
	mode.bytes_per_row = bytes_per_row;
	mode.depth = depth;
	VideoModes.push_back(mode);
}

// Set Mac frame layout and base address (uses the_buffer/MacFrameBaseMac)
static void set_mac_frame_buffer(video_depth depth, bool native_byte_order)
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
	VideoMonitor.mac_frame_base = MacFrameBaseMac;

	// Set variables used by UAE memory banking
	MacFrameBaseHost = the_buffer;
	MacFrameSize = VideoMonitor.mode.bytes_per_row * VideoMonitor.mode.y;
	InitFrameBufferMapping();
#else
	VideoMonitor.mac_frame_base = Host2MacAddr(the_buffer);
	D(bug("Host frame buffer = %p, ", the_buffer));
#endif
	D(bug("VideoMonitor.mac_frame_base = %08x\n", VideoMonitor.mac_frame_base));
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
 *  Display "driver" classes
 */

class driver_base {
public:
	driver_base();
	virtual ~driver_base();

	virtual void update_palette(void);
	virtual void suspend(void) {}
	virtual void resume(void) {}

public:
	bool init_ok;	// Initialization succeeded (we can't use exceptions because of -fomit-frame-pointer)
	Window w;		// The window we draw into
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
	driver_window(const video_mode &mode);
	~driver_window();

private:
	GC gc;
	XImage *img;
	bool have_shm;				// Flag: SHM extensions available
	XShmSegmentInfo shminfo;
	Cursor mac_cursor;
};

static driver_base *drv = NULL;	// Pointer to currently used driver object

#ifdef ENABLE_VOSF
# include "video_vosf.h"
#endif

driver_base::driver_base()
 : init_ok(false), w(0)
{
	the_buffer = NULL;
	the_buffer_copy = NULL;
}

driver_base::~driver_base()
{
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
		if (the_buffer != (uint8 *)VM_MAP_FAILED) {
			vm_release(the_buffer, the_buffer_size);
			the_buffer = NULL;
		}
		if (the_buffer_copy != (uint8 *)VM_MAP_FAILED) {
			vm_release(the_buffer_copy, the_buffer_size);
			the_buffer_copy = NULL;
		}
	}
#endif
}

// Palette has changed
void driver_base::update_palette(void)
{
	if (cmap[0] && cmap[1]) {
		int num = 256;
		if (IsDirectMode(VideoMonitor.mode))
			num = vis->map_entries; // Palette is gamma table
		else if (vis->c_class == DirectColor)
			return; // Indexed mode on true color screen, don't set CLUT
		XStoreColors(x_display, cmap[0], palette, num);
		XStoreColors(x_display, cmap[1], palette, num);
	}
	XSync(x_display, false);
}


/*
 *  Windowed display driver
 */

// Open display
driver_window::driver_window(const video_mode &mode)
 : gc(0), img(NULL), have_shm(false), mac_cursor(0)
{
	int width = mode.x, height = mode.y;
	int aligned_width = (width + 15) & ~15;
	int aligned_height = (height + 15) & ~15;

	// Set absolute mouse mode
	ADBSetRelMouseMode(false);

	// Create window
	XSetWindowAttributes wattr;
	wattr.event_mask = eventmask = win_eventmask;
	wattr.background_pixel = black_pixel;
	wattr.colormap = (mode.depth == VDEPTH_1BIT && vis->c_class == PseudoColor ? DefaultColormap(x_display, screen) : cmap[0]);
	w = XCreateWindow(x_display, rootwin, 0, 0, width, height, 0, xdepth,
		InputOutput, vis, CWEventMask | CWBackPixel | (vis->c_class == PseudoColor || vis->c_class == DirectColor ? CWColormap : 0), &wattr);

	// Set window name/class
	set_window_name(w, STR_WINDOW_TITLE);

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
	
	// Show window
	XMapWindow(x_display, w);
	wait_mapped(w);

	// 1-bit mode is big-endian; if the X server is little-endian, we can't
	// use SHM because that doesn't allow changing the image byte order
	bool need_msb_image = (mode.depth == VDEPTH_1BIT && XImageByteOrder(x_display) == LSBFirst);

	// Try to create and attach SHM image
	if (local_X11 && !need_msb_image && XShmQueryExtension(x_display)) {

		// Create SHM image ("height + 2" for safety)
		img = XShmCreateImage(x_display, vis, mode.depth == VDEPTH_1BIT ? 1 : xdepth, mode.depth == VDEPTH_1BIT ? XYBitmap : ZPixmap, 0, &shminfo, width, height);
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
			shminfo.shmid = -1;
		} else {
			have_shm = true;
			shmctl(shminfo.shmid, IPC_RMID, 0);
		}
	}
	
	// Create normal X image if SHM doesn't work ("height + 2" for safety)
	if (!have_shm) {
		int bytes_per_row = TrivialBytesPerRow(aligned_width, mode.depth);
		the_buffer_copy = (uint8 *)malloc((aligned_height + 2) * bytes_per_row);
		img = XCreateImage(x_display, vis, mode.depth == VDEPTH_1BIT ? 1 : xdepth, mode.depth == VDEPTH_1BIT ? XYBitmap : ZPixmap, 0, (char *)the_buffer_copy, aligned_width, aligned_height, 32, bytes_per_row);
	}

	if (need_msb_image) {
		img->byte_order = MSBFirst;
		img->bitmap_bit_order = MSBFirst;
	}

#ifdef ENABLE_VOSF
	// Allocate memory for frame buffer (SIZE is extended to page-boundary)
	the_host_buffer = the_buffer_copy;
	the_buffer_size = page_extend((aligned_height + 2) * img->bytes_per_line);
	the_buffer_copy = (uint8 *)vm_acquire(the_buffer_size);
	the_buffer = (uint8 *)vm_acquire(the_buffer_size);
#else
	// Allocate memory for frame buffer
	the_buffer = (uint8 *)malloc((aligned_height + 2) * img->bytes_per_line);
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
	bool native_byte_order;
#ifdef WORDS_BIGENDIAN
	native_byte_order = (XImageByteOrder(x_display) == MSBFirst);
#else
	native_byte_order = (XImageByteOrder(x_display) == LSBFirst);
#endif
#ifdef ENABLE_VOSF
	Screen_blitter_init(&visualInfo, native_byte_order, mode.depth);
#endif

	// Set VideoMonitor
	VideoMonitor.mode = mode;
	set_mac_frame_buffer(mode.depth, native_byte_order);

	// Everything went well
	init_ok = true;
}

// Close display
driver_window::~driver_window()
{
	if (img)
		XDestroyImage(img);
	if (have_shm) {
		XShmDetach(x_display, &shminfo);
		the_buffer_copy = NULL; // don't free() in driver_base dtor
	}
	if (gc)
		XFreeGC(x_display, gc);
}


#if defined(ENABLE_XF86_DGA) || defined(ENABLE_FBDEV_DGA)
/*
 *  DGA display driver base class
 */

class driver_dga : public driver_base {
public:
	driver_dga();
	~driver_dga();

	void suspend(void);
	void resume(void);

private:
	Window suspend_win;		// "Suspend" information window
	void *fb_save;			// Saved frame buffer for suspend/resume
};

driver_dga::driver_dga()
 : suspend_win(0), fb_save(NULL)
{
}

driver_dga::~driver_dga()
{
	XUngrabPointer(x_display, CurrentTime);
	XUngrabKeyboard(x_display, CurrentTime);
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
	fb_save = malloc(VideoMonitor.mode.y * VideoMonitor.mode.bytes_per_row);
	if (fb_save)
		memcpy(fb_save, the_buffer, VideoMonitor.mode.y * VideoMonitor.mode.bytes_per_row);

	// Close full screen display
#ifdef ENABLE_XF86_DGA
	XF86DGADirectVideo(x_display, screen, 0);
#endif
	XUngrabPointer(x_display, CurrentTime);
	XUngrabKeyboard(x_display, CurrentTime);
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
		memset(the_buffer_copy, 0, VideoMonitor.mode.bytes_per_row * VideoMonitor.mode.y);
	}
#endif
	
	// Restore frame buffer
	if (fb_save) {
#ifdef ENABLE_VOSF
		// Don't copy fb_save to the temporary frame buffer in VOSF mode
		if (!use_vosf)
#endif
		memcpy(the_buffer, fb_save, VideoMonitor.mode.y * VideoMonitor.mode.bytes_per_row);
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
	driver_fbdev(const video_mode &mode);
	~driver_fbdev();
};

// Open display
driver_fbdev::driver_fbdev(const video_mode &mode)
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
		
		if ((sscanf(line, "%19s %d %x", &fb_name, &fb_depth, &fb_offset) == 3)
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
	
	// Calculate bytes per row
	int bytes_per_row = TrivialBytesPerRow(mode.x, mode.depth);
	
	// Map frame buffer
	if ((the_buffer = (uint8 *) mmap(NULL, height * bytes_per_row, PROT_READ | PROT_WRITE, MAP_PRIVATE, fbdev_fd, fb_offset)) == MAP_FAILED) {
		if ((the_buffer = (uint8 *) mmap(NULL, height * bytes_per_row, PROT_READ | PROT_WRITE, MAP_SHARED, fbdev_fd, fb_offset)) == MAP_FAILED) {
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
	use_vosf = Screen_blitter_init(&visualInfo, true, mode.depth);
	
	if (use_vosf) {
	  // Allocate memory for frame buffer (SIZE is extended to page-boundary)
	  the_host_buffer = the_buffer;
	  the_buffer_size = page_extend((height + 2) * bytes_per_row);
	  the_buffer_copy = (uint8 *)vm_acquire(the_buffer_size);
	  the_buffer = (uint8 *)vm_acquire(the_buffer_size);
	}
#else
	use_vosf = false;
#endif
#endif
	
	// Set VideoMonitor
	VideoModes[0].bytes_per_row = bytes_per_row;
	VideoModes[0].depth = DepthModeForPixelDepth(fb_depth);
	VideoMonitor.mode = mode;
	set_mac_frame_buffer(mode.depth, true);

	// Everything went well
	init_ok = true;
}

// Close display
driver_fbdev::~driver_fbdev()
{
}
#endif


#ifdef ENABLE_XF86_DGA
/*
 *  XFree86 DGA display driver
 */

class driver_xf86dga : public driver_dga {
public:
	driver_xf86dga(const video_mode &mode);
	~driver_xf86dga();

	void update_palette(void);
	void resume(void);

private:
	int current_dga_cmap;					// Number (0 or 1) of currently installed DGA colormap
};

// Open display
driver_xf86dga::driver_xf86dga(const video_mode &mode)
 : current_dga_cmap(0)
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

	w = XCreateWindow(x_display, rootwin, 0, 0, width, height, 0, xdepth,
		InputOutput, vis, CWEventMask | CWOverrideRedirect, &wattr);

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
#ifdef VIDEO_VOSF
#if REAL_ADDRESSING || DIRECT_ADDRESSING
	// Screen_blitter_init() returns TRUE if VOSF is mandatory
	// i.e. the framebuffer update function is not Blit_Copy_Raw
	use_vosf = Screen_blitter_init(&visualInfo, true, mode.depth);
	
	if (use_vosf) {
	  // Allocate memory for frame buffer (SIZE is extended to page-boundary)
	  the_host_buffer = the_buffer;
	  the_buffer_size = page_extend((height + 2) * bytes_per_row);
	  the_buffer_copy = (uint8 *)vm_acquire(the_buffer_size);
	  the_buffer = (uint8 *)vm_acquire(the_buffer_size);
	}
#else
	use_vosf = false;
#endif
#endif
	
	// Set VideoMonitor
	const_cast<video_mode *>(&mode)->bytes_per_row = bytes_per_row;
	VideoMonitor.mode = mode;
	set_mac_frame_buffer(mode.depth, true);

	// Everything went well
	init_ok = true;
}

// Close display
driver_xf86dga::~driver_xf86dga()
{
	XF86DGADirectVideo(x_display, screen, 0);
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
	if (!IsDirectMode(VideoMonitor.mode) && cmap[current_dga_cmap])
		XF86DGAInstallColormap(x_display, screen, cmap[current_dga_cmap]);
}

// Resume emulation
void driver_xf86dga::resume(void)
{
	driver_dga::resume();
	if (!IsDirectMode(VideoMonitor.mode))
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

// Open display for specified mode
static bool video_open(const video_mode &mode)
{
	// Load gray ramp to color map
	int num = (vis->c_class == DirectColor ? vis->map_entries : 256);
	for (int i=0; i<num; i++) {
		int c = (i * 256) / num;
		palette[i].red = c * 0x0101;
		palette[i].green = c * 0x0101;
		palette[i].blue = c * 0x0101;
		if (vis->c_class == PseudoColor)
			palette[i].pixel = i;
		palette[i].flags = DoRed | DoGreen | DoBlue;
	}
	if (cmap[0] && cmap[1]) {
		XStoreColors(x_display, cmap[0], palette, num);
		XStoreColors(x_display, cmap[1], palette, num);
	}

	// Create display driver object of requested type
	switch (display_type) {
		case DISPLAY_WINDOW:
			drv = new driver_window(mode);
			break;
#ifdef ENABLE_FBDEV_DGA
		case DISPLAY_DGA:
			drv = new driver_fbdev(mode);
			break;
#endif
#ifdef ENABLE_XF86_DGA
		case DISPLAY_DGA:
			drv = new driver_xf86dga(mode);
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
		// Initialize the mainBuffer structure
		if (!video_init_buffer()) {
			ErrorAlert(STR_VOSF_INIT_ERR);
        	return false;
		}

		// Initialize the handler for SIGSEGV
		if (!sigsegv_install_handler(screen_fault_handler)) {
			ErrorAlert("Could not initialize Video on SEGV signals");
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
#ifdef HAVE_PTHREADS
	redraw_thread_cancel = false;
	redraw_thread_active = (pthread_create(&redraw_thread, NULL, redraw_func, NULL) == 0);
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
	
	// Get screen depth
	xdepth = DefaultDepth(x_display, screen);
	
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

	// Get appropriate visual
	int color_class;
	switch (xdepth) {
		case 1:
			color_class = StaticGray;
			break;
		case 8:
			color_class = PseudoColor;
			break;
		case 15:
		case 16:
		case 24:
		case 32: // Try DirectColor first, as this will allow gamma correction
			color_class = DirectColor;
			if (!XMatchVisualInfo(x_display, screen, xdepth, color_class, &visualInfo))
				color_class = TrueColor;
			break;
		default:
			ErrorAlert(STR_UNSUPP_DEPTH_ERR);
			return false;
	}
	if (!XMatchVisualInfo(x_display, screen, xdepth, color_class, &visualInfo)) {
		ErrorAlert(STR_NO_XVISUAL_ERR);
		return false;
	}
	if (visualInfo.depth != xdepth) {
		ErrorAlert(STR_NO_XVISUAL_ERR);
		return false;
	}
	vis = visualInfo.visual;

	// Create color maps
	if (color_class == PseudoColor || color_class == DirectColor) {
		cmap[0] = XCreateColormap(x_display, rootwin, vis, AllocAll);
		cmap[1] = XCreateColormap(x_display, rootwin, vis, AllocAll);
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

	// Preset palette pixel values for gamma table
	if (color_class == DirectColor) {
		int num = vis->map_entries;
		for (int i=0; i<num; i++) {
			int c = (i * 256) / num;
			palette[i].pixel = ((c >> rloss) << rshift) | ((c >> gloss) << gshift) | ((c >> bloss) << bshift);
		}
	}

	// Get screen mode from preferences
	const char *mode_str;
	if (classic_mode)
		mode_str = "win/512/342";
	else
		mode_str = PrefsFindString("screen");

	// Determine display type and default dimensions
	int default_width = 512, default_height = 384;
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

	// Mac screen depth follows X depth
	video_depth default_depth = DepthModeForPixelDepth(xdepth);

	// Construct list of supported modes
	if (display_type == DISPLAY_WINDOW) {
		if (classic)
			add_mode(512, 342, 0x80, 64, VDEPTH_1BIT);
		else {
			if (default_depth != VDEPTH_1BIT) { // 1-bit modes are always available
				add_mode(512, 384, 0x80, TrivialBytesPerRow(512, VDEPTH_1BIT), VDEPTH_1BIT);
				add_mode(640, 480, 0x81, TrivialBytesPerRow(640, VDEPTH_1BIT), VDEPTH_1BIT);
				add_mode(800, 600, 0x82, TrivialBytesPerRow(800, VDEPTH_1BIT), VDEPTH_1BIT);
				add_mode(832, 624, 0x83, TrivialBytesPerRow(832, VDEPTH_1BIT), VDEPTH_1BIT);
				add_mode(1024, 768, 0x84, TrivialBytesPerRow(1024, VDEPTH_1BIT), VDEPTH_1BIT);
				add_mode(1152, 870, 0x85, TrivialBytesPerRow(1152, VDEPTH_1BIT), VDEPTH_1BIT);
				add_mode(1280, 1024, 0x86, TrivialBytesPerRow(1280, VDEPTH_1BIT), VDEPTH_1BIT);
				add_mode(1600, 1200, 0x87, TrivialBytesPerRow(1600, VDEPTH_1BIT), VDEPTH_1BIT);
			}
			add_mode(512, 384, 0x80, TrivialBytesPerRow(512, default_depth), default_depth);
			add_mode(640, 480, 0x81, TrivialBytesPerRow(640, default_depth), default_depth);
			add_mode(800, 600, 0x82, TrivialBytesPerRow(800, default_depth), default_depth);
			add_mode(832, 624, 0x83, TrivialBytesPerRow(832, default_depth), default_depth);
			add_mode(1024, 768, 0x84, TrivialBytesPerRow(1024, default_depth), default_depth);
			add_mode(1152, 870, 0x85, TrivialBytesPerRow(1152, default_depth), default_depth);
			add_mode(1280, 1024, 0x86, TrivialBytesPerRow(1280, default_depth), default_depth);
			add_mode(1600, 1200, 0x87, TrivialBytesPerRow(1600, default_depth), default_depth);
		}
	} else
		add_mode(default_width, default_height, 0x80, TrivialBytesPerRow(default_width, default_depth), default_depth);

	// Find requested default mode and open display
	if (VideoModes.size() == 1)
		return video_open(VideoModes[0]);
	else {
		// Find mode with specified dimensions
		std::vector<video_mode>::const_iterator i, end = VideoModes.end();
		for (i = VideoModes.begin(); i != end; ++i) {
			if (i->x == default_width && i->y == default_height && i->depth == default_depth)
				return video_open(*i);
		}
		return video_open(VideoModes[0]);
	}
}


/*
 *  Deinitialization
 */

// Close display
static void video_close(void)
{
	// Stop redraw thread
#ifdef HAVE_PTHREADS
	if (redraw_thread_active) {
		redraw_thread_cancel = true;
#ifdef HAVE_PTHREAD_CANCEL
		pthread_cancel(redraw_thread);
#endif
		pthread_join(redraw_thread, NULL);
	}
#endif
	redraw_thread_active = false;

	// Unlock frame buffer
	UNLOCK_FRAME_BUFFER;
	XSync(x_display, false);

#ifdef ENABLE_VOSF
	// Deinitialize VOSF
	if (use_vosf) {
		if (mainBuffer.pageInfo) {
			free(mainBuffer.pageInfo);
			mainBuffer.pageInfo = NULL;
		}
		if (mainBuffer.dirtyPages) {
			free(mainBuffer.dirtyPages);
			mainBuffer.dirtyPages = NULL;
		}
	}
#endif

	// Close display
	delete drv;
	drv = NULL;
}

void VideoExit(void)
{
	// Close display
	video_close();

	// Free colormaps
	if (cmap[0]) {
		XFreeColormap(x_display, cmap[0]);
		cmap[0] = 0;
	}
	if (cmap[1]) {
		XFreeColormap(x_display, cmap[1]);
		cmap[1] = 0;
	}

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

void video_set_palette(uint8 *pal)
{
	LOCK_PALETTE;

	// Convert colors to XColor array
	int num_in = 256, num_out = 256;
	if (VideoMonitor.mode.depth == VDEPTH_16BIT)
		num_in = 32;
	if (IsDirectMode(VideoMonitor.mode)) {
		// If X is in 565 mode we have to stretch the palette from 32 to 64 entries
		num_out = vis->map_entries;
	}
	XColor *p = palette;
	for (int i=0; i<num_out; i++) {
		int c = (i * num_in) / num_out;
		p->red = pal[c*3 + 0] * 0x0101;
		p->green = pal[c*3 + 1] * 0x0101;
		p->blue = pal[c*3 + 2] * 0x0101;
		if (vis->c_class == PseudoColor)
			p->pixel = i;
		p->flags = DoRed | DoGreen | DoBlue;
		p++;
	}

	// Tell redraw thread to change palette
	palette_changed = true;

	UNLOCK_PALETTE;
}


/*
 *  Switch video mode
 */

void video_switch_to_mode(const video_mode &mode)
{
	// Close and reopen display
	video_close();
	video_open(mode);

	if (drv == NULL) {
		ErrorAlert(STR_OPEN_WINDOW_ERR);
		QuitEmulator();
	}
}


/*
 *  Translate key event to Mac keycode
 */

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

#if defined(ENABLE_XF86_DGA) || defined(ENABLE_FBDEV_DGA)
		case XK_Tab: if (ctrl_down) {drv->suspend(); return -1;} else return 0x30;
#else
		case XK_Tab: return 0x30;
#endif
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


/*
 *  X event handling
 */

static void handle_events(void)
{
	while (XPending(x_display)) {
		XEvent event;
		XNextEvent(x_display, &event);

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
			case EnterNotify:
			case MotionNotify:
				ADBMouseMoved(event.xmotion.x, event.xmotion.y);
				break;

			// Keyboard
			case KeyPress: {
				int code;
				if (use_keycodes) {
					event2keycode(event.xkey);	// This is called to process the hotkeys
					code = keycode_table[event.xkey.keycode & 0xff];
				} else
					code = event2keycode(event.xkey);
				if (code != -1) {
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
				int code;
				if (use_keycodes) {
					event2keycode(event.xkey);	// This is called to process the hotkeys
					code = keycode_table[event.xkey.keycode & 0xff];
				} else
					code = event2keycode(event.xkey);
				if (code != -1 && code != 0x39) {	// Don't propagate Caps Lock releases
					ADBKeyUp(code);
					if (code == 0x36)
						ctrl_down = false;
				}
				break;
			}

			// Hidden parts exposed, force complete refresh of window
			case Expose:
				if (display_type == DISPLAY_WINDOW) {
#ifdef ENABLE_VOSF
					if (use_vosf) {			// VOSF refresh
						LOCK_VOSF;
						PFLAG_SET_ALL;
						UNLOCK_VOSF;
						memset(the_buffer_copy, 0, VideoMonitor.mode.bytes_per_row * VideoMonitor.mode.y);
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
						memset(the_buffer_copy, 0, VideoMonitor.mode.bytes_per_row * VideoMonitor.mode.y);
				}
				break;

			// Window "close" widget clicked
			case ClientMessage:
				if (event.xclient.format == 32 && event.xclient.data.l[0] == WM_DELETE_WINDOW) {
					ADBKeyDown(0x7f);	// Power key
					ADBKeyUp(0x7f);
				}
				break;
		}
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
	int bytes_per_row = VideoMonitor.mode.bytes_per_row;
	int bytes_per_pixel = VideoMonitor.mode.bytes_per_row / VideoMonitor.mode.x;
	int rx = VideoMonitor.mode.bytes_per_row / 16;
	int ry = VideoMonitor.mode.y / 16;
	int max_box;

	y2s = sm_uptd[ticker % 8];
	y2a = 8;
	for (i = 0; i < 6; i++)
		if (ticker % (2 << i))
			break;
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
					if (VideoMonitor.mode.depth == VDEPTH_1BIT) {
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
}

// Static display update (fixed frame rate, but incremental)
static void update_display_static(driver_window *drv)
{
	// Incremental update code
	int wide = 0, high = 0, x1, x2, y1, y2, i, j;
	int bytes_per_row = VideoMonitor.mode.bytes_per_row;
	int bytes_per_pixel = VideoMonitor.mode.bytes_per_row / VideoMonitor.mode.x;
	uint8 *p, *p2;

	// Check for first line from top and first line from bottom that have changed
	y1 = 0;
	for (j=0; j<VideoMonitor.mode.y; j++) {
		if (memcmp(&the_buffer[j * bytes_per_row], &the_buffer_copy[j * bytes_per_row], bytes_per_row)) {
			y1 = j;
			break;
		}
	}
	y2 = y1 - 1;
	for (j=VideoMonitor.mode.y-1; j>=y1; j--) {
		if (memcmp(&the_buffer[j * bytes_per_row], &the_buffer_copy[j * bytes_per_row], bytes_per_row)) {
			y2 = j;
			break;
		}
	}
	high = y2 - y1 + 1;

	// Check for first column from left and first column from right that have changed
	if (high) {
		if (VideoMonitor.mode.depth == VDEPTH_1BIT) {
			x1 = VideoMonitor.mode.x - 1;
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
				for (i=(VideoMonitor.mode.x>>3); i>(x2>>3); i--) {
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
			x1 = VideoMonitor.mode.x;
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
				for (i=VideoMonitor.mode.x*bytes_per_pixel; i>x2*bytes_per_pixel; i--) {
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
	if (high && wide) {
		if (drv->have_shm)
			XShmPutImage(x_display, drv->w, drv->gc, drv->img, x1, y1, x1, y1, wide, high, 0);
		else
			XPutImage(x_display, drv->w, drv->gc, drv->img, x1, y1, x1, y1, wide, high);
	}
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
	// Quit DGA mode if requested
	if (quit_full_screen) {
		quit_full_screen = false;
		delete drv;
		drv = NULL;
	}
}

static inline void handle_palette_changes(void)
{
	LOCK_PALETTE;

	if (palette_changed) {
		palette_changed = false;
		drv->update_palette();
	}

	UNLOCK_PALETTE;
}

static void video_refresh_dga(void)
{
	// Quit DGA mode if requested
	possibly_quit_dga_mode();
	
	// Handle X events
	handle_events();
	
	// Handle palette changes
	handle_palette_changes();
}

#ifdef ENABLE_VOSF
#if REAL_ADDRESSING || DIRECT_ADDRESSING
static void video_refresh_dga_vosf(void)
{
	// Quit DGA mode if requested
	possibly_quit_dga_mode();
	
	// Handle X events
	handle_events();
	
	// Handle palette changes
	handle_palette_changes();
	
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

static void video_refresh_window_vosf(void)
{
	// Quit DGA mode if requested
	possibly_quit_dga_mode();
	
	// Handle X events
	handle_events();
	
	// Handle palette changes
	handle_palette_changes();
	
	// Update display (VOSF variant)
	static int tick_counter = 0;
	if (++tick_counter >= frame_skip) {
		tick_counter = 0;
		if (mainBuffer.dirty) {
			LOCK_VOSF;
			update_display_window_vosf(static_cast<driver_window *>(drv));
			UNLOCK_VOSF;
			XSync(x_display, false); // Let the server catch up
		}
	}
}
#endif // def ENABLE_VOSF

static void video_refresh_window_static(void)
{
	// Handle X events
	handle_events();
	
	// Handle_palette changes
	handle_palette_changes();
	
	// Update display (static variant)
	static int tick_counter = 0;
	if (++tick_counter >= frame_skip) {
		tick_counter = 0;
		update_display_static(static_cast<driver_window *>(drv));
	}
}

static void video_refresh_window_dynamic(void)
{
	// Handle X events
	handle_events();
	
	// Handle_palette changes
	handle_palette_changes();
	
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

void VideoRefresh(void)
{
	// We need to check redraw_thread_active to inhibit refreshed during
	// mode changes on non-threaded platforms
	if (redraw_thread_active)
		video_refresh();
}

#ifdef HAVE_PTHREADS
static void *redraw_func(void *arg)
{
	uint64 start = GetTicks_usec();
	int64 ticks = 0;
	uint64 next = GetTicks_usec();
	while (!redraw_thread_cancel) {
		video_refresh();
		next += 16667;
		int64 delay = next - GetTicks_usec();
		if (delay > 0)
			Delay_usec(delay);
		else if (delay < -16667)
			next = GetTicks_usec();
		ticks++;
	}
	uint64 end = GetTicks_usec();
	// printf("%Ld ticks in %Ld usec = %Ld ticks/sec\n", ticks, end - start, ticks * 1000000 / (end - start));
	return NULL;
}
#endif
