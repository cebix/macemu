/*
 *  video_sdl.cpp - Video/graphics emulation, SDL specific stuff
 *
 *  Basilisk II (C) 1997-2004 Christian Bauer
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
 *
 *  FIXMEs and TODOs:
 *  - Ctrl-Fn doesn't generate SDL_KEYDOWN events (SDL bug?)
 *  - Mouse acceleration, there is no API in SDL yet for that
 *  - Force relative mode in Grab mode even if SDL provides absolute coordinates?
 *  - Fullscreen mode
 *  - Gamma tables support is likely to be broken here
 *  - Events processing is bound to the general emulation thread as SDL requires
 *    to PumpEvents() within the same thread as the one that called SetVideoMode().
 *    Besides, there can't seem to be a way to call SetVideoMode() from a child thread.
 *  - Refresh performance is still slow. Use SDL_CreateRGBSurface()?
 *  - Backport hw cursor acceleration to Basilisk II?
 *  - Move generic Native QuickDraw acceleration routines to gfxaccel.cpp
 */

#include "sysdeps.h"

#include <SDL.h>
#include <SDL_mutex.h>
#include <SDL_thread.h>
#include <errno.h>
#include <vector>

#include "cpu_emulation.h"
#include "main.h"
#include "adb.h"
#include "macos_util.h"
#include "prefs.h"
#include "user_strings.h"
#include "video.h"
#include "video_defs.h"
#include "video_blit.h"

#define DEBUG 0
#include "debug.h"


// Supported video modes
using std::vector;
static vector<VIDEO_MODE> VideoModes;

// Display types
#ifdef SHEEPSHAVER
enum {
	DISPLAY_WINDOW = DIS_WINDOW,					// windowed display
	DISPLAY_SCREEN = DIS_SCREEN						// fullscreen display
};
extern int display_type;							// See enum above
#else
enum {
	DISPLAY_WINDOW,									// windowed display
	DISPLAY_SCREEN									// fullscreen display
};
static int display_type = DISPLAY_WINDOW;			// See enum above
#endif

// Constants
const char KEYCODE_FILE_NAME[] = DATADIR "/keycodes";


// Global variables
static int32 frame_skip;							// Prefs items
static int16 mouse_wheel_mode;
static int16 mouse_wheel_lines;

static uint8 *the_buffer = NULL;					// Mac frame buffer (where MacOS draws into)
static uint8 *the_buffer_copy = NULL;				// Copy of Mac frame buffer (for refreshed modes)
static uint32 the_buffer_size;						// Size of allocated the_buffer

static bool redraw_thread_active = false;			// Flag: Redraw thread installed
static volatile bool redraw_thread_cancel;			// Flag: Cancel Redraw thread
static SDL_Thread *redraw_thread = NULL;			// Redraw thread

#ifdef ENABLE_VOSF
static bool use_vosf = false;						// Flag: VOSF enabled
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

// SDL variables
static int screen_depth;							// Depth of current screen
static SDL_Cursor *sdl_cursor;						// Copy of Mac cursor
static volatile bool cursor_changed = false;		// Flag: cursor changed, redraw_func must update the cursor
static SDL_Color sdl_palette[256];					// Color palette to be used as CLUT and gamma table
static bool sdl_palette_changed = false;			// Flag: Palette changed, redraw thread must set new colors
static const int sdl_eventmask = SDL_MOUSEBUTTONDOWNMASK | SDL_MOUSEBUTTONUPMASK | SDL_MOUSEMOTIONMASK | SDL_KEYUPMASK | SDL_KEYDOWNMASK | SDL_VIDEOEXPOSEMASK | SDL_QUITMASK;

// Mutex to protect palette
static SDL_mutex *sdl_palette_lock = NULL;
#define LOCK_PALETTE SDL_LockMutex(sdl_palette_lock)
#define UNLOCK_PALETTE SDL_UnlockMutex(sdl_palette_lock)

// Mutex to protect frame buffer
static SDL_mutex *frame_buffer_lock = NULL;
#define LOCK_FRAME_BUFFER SDL_LockMutex(frame_buffer_lock)
#define UNLOCK_FRAME_BUFFER SDL_UnlockMutex(frame_buffer_lock)

// Video refresh function
static void VideoRefreshInit(void);
static void (*video_refresh)(void);


// Prototypes
static int redraw_func(void *arg);

// From sys_unix.cpp
extern void SysMountFirstFloppy(void);


/*
 *  SheepShaver glue
 */

#ifdef SHEEPSHAVER
// Color depth modes type
typedef int video_depth;

// 1, 2, 4 and 8 bit depths use a color palette
static inline bool IsDirectMode(VIDEO_MODE const & mode)
{
	return IsDirectMode(mode.viAppleMode);
}

// Abstract base class representing one (possibly virtual) monitor
// ("monitor" = rectangular display with a contiguous frame buffer)
class monitor_desc {
public:
	monitor_desc(const vector<VIDEO_MODE> &available_modes, video_depth default_depth, uint32 default_id) {}
	virtual ~monitor_desc() {}

	// Get current Mac frame buffer base address
	uint32 get_mac_frame_base(void) const {return screen_base;}

	// Set Mac frame buffer base address (called from switch_to_mode())
	void set_mac_frame_base(uint32 base) {screen_base = base;}

	// Get current video mode
	const VIDEO_MODE &get_current_mode(void) const {return VModes[cur_mode];}

	// Called by the video driver to switch the video mode on this display
	// (must call set_mac_frame_base())
	virtual void switch_to_current_mode(void) = 0;

	// Called by the video driver to set the color palette (in indexed modes)
	// or the gamma table (in direct modes)
	virtual void set_palette(uint8 *pal, int num) = 0;
};

// Vector of pointers to available monitor descriptions, filled by VideoInit()
static vector<monitor_desc *> VideoMonitors;

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

// Set parameters to specified Apple mode
static void set_apple_resolution(int apple_id, int &xsize, int &ysize)
{
	switch (apple_id) {
	case APPLE_640x480:
		xsize = 640;
		ysize = 480;
		break;
	case APPLE_800x600:
		xsize = 800;
		ysize = 600;
		break;
	case APPLE_1024x768:
		xsize = 1024;
		ysize = 768;
		break;
	case APPLE_1152x768:
		xsize = 1152;
		ysize = 768;
		break;
	case APPLE_1152x900:
		xsize = 1152;
		ysize = 900;
		break;
	case APPLE_1280x1024:
		xsize = 1280;
		ysize = 1024;
		break;
	case APPLE_1600x1200:
		xsize = 1600;
		ysize = 1200;
		break;
	default:
		abort();
	}
}

// Match Apple mode matching best specified dimensions
static int match_apple_resolution(int &xsize, int &ysize)
{
	int apple_id = find_apple_resolution(xsize, ysize);
	set_apple_resolution(apple_id, xsize, ysize);
	return apple_id;
}

// Display error alert
static void ErrorAlert(int error)
{
	ErrorAlert(GetString(error));
}

// Display warning alert
static void WarningAlert(int warning)
{
	WarningAlert(GetString(warning));
}
#endif


/*
 *  monitor_desc subclass for SDL display
 */

class SDL_monitor_desc : public monitor_desc {
public:
	SDL_monitor_desc(const vector<VIDEO_MODE> &available_modes, video_depth default_depth, uint32 default_id) : monitor_desc(available_modes, default_depth, default_id) {}
	~SDL_monitor_desc() {}

	virtual void switch_to_current_mode(void);
	virtual void set_palette(uint8 *pal, int num);

	bool video_open(void);
	void video_close(void);
};


/*
 *  Utility functions
 */

// Find palette size for given color depth
static int palette_size(int mode)
{
	switch (mode) {
	case VIDEO_DEPTH_1BIT: return 2;
	case VIDEO_DEPTH_2BIT: return 4;
	case VIDEO_DEPTH_4BIT: return 16;
	case VIDEO_DEPTH_8BIT: return 256;
	case VIDEO_DEPTH_16BIT: return 32;
	case VIDEO_DEPTH_32BIT: return 256;
	default: return 0;
	}
}

// Return bytes per pixel for requested depth
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
static int sdl_depth_of_video_depth(int video_depth)
{
	int depth = -1;
	switch (video_depth) {
	case VIDEO_DEPTH_1BIT:
		depth = 1;
		break;
	case VIDEO_DEPTH_2BIT:
		depth = 2;
		break;
	case VIDEO_DEPTH_4BIT:
		depth = 4;
		break;
	case VIDEO_DEPTH_8BIT:
		depth = 8;
		break;
	case VIDEO_DEPTH_16BIT:
		depth = 16;
		break;
	case VIDEO_DEPTH_32BIT:
		depth = 32;
		break;
	default:
		abort();
	}
	return depth;
}

// Check wether specified mode is available
static bool has_mode(int type, int width, int height)
{
	// FIXME: no fullscreen support yet
	if (type == DISPLAY_SCREEN)
		return false;

#ifdef SHEEPSHAVER
	// Filter out Classic resolutiosn
	if (width == 512 && height == 384)
		return false;

	// Read window modes prefs
	static uint32 window_modes = 0;
	static uint32 screen_modes = 0;
	if (window_modes == 0 || screen_modes == 0) {
		window_modes = PrefsFindInt32("windowmodes");
		screen_modes = PrefsFindInt32("screenmodes");
		if (window_modes == 0 || screen_modes == 0)
			window_modes |= 3;			// Allow at least 640x480 and 800x600 window modes
	}

	if (type == DISPLAY_WINDOW) {
		int apple_mask, apple_id = find_apple_resolution(width, height);
		switch (apple_id) {
		case APPLE_640x480:		apple_mask = 0x01; break;
		case APPLE_800x600:		apple_mask = 0x02; break;
		case APPLE_1024x768:	apple_mask = 0x04; break;
		case APPLE_1152x768:	apple_mask = 0x40; break;
		case APPLE_1152x900:	apple_mask = 0x08; break;
		case APPLE_1280x1024:	apple_mask = 0x10; break;
		case APPLE_1600x1200:	apple_mask = 0x20; break;
		default:				apple_mask = 0x00; break;
		}
		return (window_modes & apple_mask);
	}
#else
	return true;
#endif
	return false;
}

// Add mode to list of supported modes
static void add_mode(int type, int width, int height, int resolution_id, int bytes_per_row, int depth)
{
	// Filter out unsupported modes
	if (!has_mode(type, width, height))
		return;

	// Fill in VideoMode entry
	VIDEO_MODE mode;
#ifdef SHEEPSHAVER
	// Recalculate dimensions to fit Apple modes
	resolution_id = match_apple_resolution(width, height);
	mode.viType = type;
#endif
	VIDEO_MODE_X = width;
	VIDEO_MODE_Y = height;
	VIDEO_MODE_RESOLUTION = resolution_id;
	VIDEO_MODE_ROW_BYTES = bytes_per_row;
	VIDEO_MODE_DEPTH = (video_depth)depth;
	VideoModes.push_back(mode);
}

// Add standard list of windowed modes for given color depth
static void add_window_modes(int depth)
{
	video_depth vdepth = (video_depth)depth;
	add_mode(DISPLAY_WINDOW, 512, 384, 0x80, TrivialBytesPerRow(512, vdepth), depth);
	add_mode(DISPLAY_WINDOW, 640, 480, 0x81, TrivialBytesPerRow(640, vdepth), depth);
	add_mode(DISPLAY_WINDOW, 800, 600, 0x82, TrivialBytesPerRow(800, vdepth), depth);
	add_mode(DISPLAY_WINDOW, 1024, 768, 0x83, TrivialBytesPerRow(1024, vdepth), depth);
	add_mode(DISPLAY_WINDOW, 1152, 870, 0x84, TrivialBytesPerRow(1152, vdepth), depth);
	add_mode(DISPLAY_WINDOW, 1280, 1024, 0x85, TrivialBytesPerRow(1280, vdepth), depth);
	add_mode(DISPLAY_WINDOW, 1600, 1200, 0x86, TrivialBytesPerRow(1600, vdepth), depth);
}

// Set Mac frame layout and base address (uses the_buffer/MacFrameBaseMac)
static void set_mac_frame_buffer(SDL_monitor_desc &monitor, int depth, bool native_byte_order)
{
#if !REAL_ADDRESSING && !DIRECT_ADDRESSING
	int layout = FLAYOUT_DIRECT;
	if (depth == VIDEO_DEPTH_16BIT)
		layout = (screen_depth == 15) ? FLAYOUT_HOST_555 : FLAYOUT_HOST_565;
	else if (depth == VIDEO_DEPTH_32BIT)
		layout = (screen_depth == 24) ? FLAYOUT_HOST_888 : FLAYOUT_DIRECT;
	if (native_byte_order)
		MacFrameLayout = layout;
	else
		MacFrameLayout = FLAYOUT_DIRECT;
	monitor.set_mac_frame_base(MacFrameBaseMac);

	// Set variables used by UAE memory banking
	const VIDEO_MODE &mode = monitor.get_current_mode();
	MacFrameBaseHost = the_buffer;
	MacFrameSize = VIDEO_MODE_ROW_BYTES * VIDEO_MODE_Y;
	InitFrameBufferMapping();
#else
	monitor.set_mac_frame_base(Host2MacAddr(the_buffer));
#endif
	D(bug("monitor.mac_frame_base = %08x\n", monitor.get_mac_frame_base()));
}

// Set window name and class
static void set_window_name(int name)
{
	const SDL_VideoInfo *vi = SDL_GetVideoInfo();
	if (vi && vi->wm_available) {
		const char *str = GetString(name);
		SDL_WM_SetCaption(str, str);
	}
}

// Set mouse grab mode
static SDL_GrabMode set_grab_mode(SDL_GrabMode mode)
{
	const SDL_VideoInfo *vi =SDL_GetVideoInfo();
	return (vi && vi->wm_available ? SDL_WM_GrabInput(mode) : SDL_GRAB_OFF);
}


/*
 *  Display "driver" classes
 */

class driver_base {
public:
	driver_base(SDL_monitor_desc &m);
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
	SDL_monitor_desc &monitor; // Associated video monitor
	const VIDEO_MODE &mode;    // Video mode handled by the driver

	bool init_ok;	// Initialization succeeded (we can't use exceptions because of -fomit-frame-pointer)
	SDL_Surface *s;	// The surface we draw into
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
	driver_window(SDL_monitor_desc &monitor);
	~driver_window();

	void toggle_mouse_grab(void);
	void mouse_moved(int x, int y);

	void grab_mouse(void);
	void ungrab_mouse(void);

private:
	bool mouse_grabbed;				// Flag: mouse pointer grabbed, using relative mouse mode
	int mouse_last_x, mouse_last_y;	// Last mouse position (for relative mode)
};

static driver_base *drv = NULL;	// Pointer to currently used driver object

#ifdef ENABLE_VOSF
# include "video_vosf.h"
#endif

driver_base::driver_base(SDL_monitor_desc &m)
	: monitor(m), mode(m.get_current_mode()), init_ok(false), s(NULL)
{
	the_buffer = NULL;
	the_buffer_copy = NULL;
}

driver_base::~driver_base()
{
	ungrab_mouse();
	restore_mouse_accel();

	if (s)
		SDL_FreeSurface(s);

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

// Palette has changed
void driver_base::update_palette(void)
{
	const VIDEO_MODE &mode = monitor.get_current_mode();

	if ((int)VIDEO_MODE_DEPTH <= VIDEO_DEPTH_8BIT)
		SDL_SetPalette(s, SDL_PHYSPAL, sdl_palette, 0, 256);
}

// Disable mouse acceleration
void driver_base::disable_mouse_accel(void)
{
}

// Restore mouse acceleration to original value
void driver_base::restore_mouse_accel(void)
{
}


/*
 *  Windowed display driver
 */

// Open display
driver_window::driver_window(SDL_monitor_desc &m)
	: driver_base(m), mouse_grabbed(false)
{
	int width = VIDEO_MODE_X, height = VIDEO_MODE_Y;
	int aligned_width = (width + 15) & ~15;
	int aligned_height = (height + 15) & ~15;

	// Set absolute mouse mode
	ADBSetRelMouseMode(mouse_grabbed);

	// Create surface
	int depth = ((int)VIDEO_MODE_DEPTH <= VIDEO_DEPTH_8BIT ? 8 : screen_depth);
	if ((s = SDL_SetVideoMode(width, height, depth, SDL_HWSURFACE)) == NULL)
		return;

#ifdef ENABLE_VOSF
	use_vosf = true;
	// Allocate memory for frame buffer (SIZE is extended to page-boundary)
	the_host_buffer = (uint8 *)s->pixels;
	the_buffer_size = page_extend((aligned_height + 2) * s->pitch);
	the_buffer = (uint8 *)vm_acquire(the_buffer_size);
	the_buffer_copy = (uint8 *)malloc(the_buffer_size);
	D(bug("the_buffer = %p, the_buffer_copy = %p, the_host_buffer = %p\n", the_buffer, the_buffer_copy, the_host_buffer));

	// Check whether we can initialize the VOSF subsystem and it's profitable
	if (!video_vosf_init(m)) {
		WarningAlert(STR_VOSF_INIT_ERR);
		use_vosf = false;
	}
	else if (!video_vosf_profitable()) {
		video_vosf_exit();
		printf("VOSF acceleration is not profitable on this platform, disabling it\n");
		use_vosf = false;
	}
	if (!use_vosf) {
		free(the_buffer_copy);
		vm_release(the_buffer, the_buffer_size);
		the_host_buffer = NULL;
	}
#endif
	if (!use_vosf) {
		// Allocate memory for frame buffer
		the_buffer_size = (aligned_height + 2) * s->pitch;
		the_buffer_copy = (uint8 *)calloc(1, the_buffer_size);
		the_buffer = (uint8 *)calloc(1, the_buffer_size);
		D(bug("the_buffer = %p, the_buffer_copy = %p\n", the_buffer, the_buffer_copy));
	}
	
#ifdef SHEEPSHAVER
	// Create cursor
	if ((sdl_cursor = SDL_CreateCursor(MacCursor + 4, MacCursor + 36, 16, 16, 0, 0)) != NULL) {
		SDL_SetCursor(sdl_cursor);
		cursor_changed = false;
	}
#else
	// Hide cursor
	SDL_ShowCursor(0);
#endif

	// Set window name/class
	set_window_name(STR_WINDOW_TITLE);

	// Init blitting routines
	SDL_PixelFormat *f = s->format;
	VisualFormat visualFormat;
	visualFormat.depth = depth;
	visualFormat.Rmask = f->Rmask;
	visualFormat.Gmask = f->Gmask;
	visualFormat.Bmask = f->Bmask;
	Screen_blitter_init(visualFormat, true, sdl_depth_of_video_depth(VIDEO_MODE_DEPTH));

	// Load gray ramp to 8->16/32 expand map
	if (!IsDirectMode(mode))
		for (int i=0; i<256; i++)
			ExpandMap[i] = SDL_MapRGB(f, i, i, i);

	// Set frame buffer base
	set_mac_frame_buffer(monitor, VIDEO_MODE_DEPTH, true);

	// Everything went well
	init_ok = true;
}

// Close display
driver_window::~driver_window()
{
#ifdef ENABLE_VOSF
	if (use_vosf)
		the_host_buffer = NULL;	// don't free() in driver_base dtor
#endif
	if (s)
		SDL_FreeSurface(s);
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
	if (!mouse_grabbed) {
		SDL_GrabMode new_mode = set_grab_mode(SDL_GRAB_ON);
		if (new_mode == SDL_GRAB_ON) {
			set_window_name(STR_WINDOW_TITLE_GRABBED);
			disable_mouse_accel();
			mouse_grabbed = true;
		}
	}
}

// Ungrab mouse, switch to absolute mouse mode
void driver_window::ungrab_mouse(void)
{
	if (mouse_grabbed) {
		SDL_GrabMode new_mode = set_grab_mode(SDL_GRAB_OFF);
		if (new_mode == SDL_GRAB_OFF) {
			set_window_name(STR_WINDOW_TITLE);
			restore_mouse_accel();
			mouse_grabbed = false;
		}
	}
}

// Mouse moved
void driver_window::mouse_moved(int x, int y)
{
	mouse_last_x = x; mouse_last_y = y;
	ADBMouseMoved(x, y);
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
		char video_driver[256];
		SDL_VideoDriverName(video_driver, sizeof(video_driver));
		bool video_driver_found = false;
		char line[256];
		while (fgets(line, sizeof(line) - 1, f)) {
			// Read line
			int len = strlen(line);
			if (len == 0)
				continue;
			line[len-1] = 0;

			// Comments begin with "#" or ";"
			if (line[0] == '#' || line[0] == ';' || line[0] == 0)
				continue;

			if (video_driver_found) {
				// Skip aliases
				static const char sdl_str[] = "sdl";
				if (strncmp(line, sdl_str, sizeof(sdl_str) - 1) == 0)
					continue;

				// Read keycode
				int x_code, mac_code;
				if (sscanf(line, "%d %d", &x_code, &mac_code) == 2)
					keycode_table[x_code & 0xff] = mac_code;
				else
					break;
			} else {
				// Search for SDL video driver string
				static const char sdl_str[] = "sdl";
				if (strncmp(line, sdl_str, sizeof(sdl_str) - 1) == 0) {
					char *p = line + sizeof(sdl_str);
					if (strstr(video_driver, p) == video_driver)
						video_driver_found = true;
				}
			}
		}

		// Keycode file completely read
		fclose(f);
		use_keycodes = video_driver_found;

		// Vendor not found? Then display warning
		if (!video_driver_found) {
			char str[256];
			sprintf(str, GetString(STR_KEYCODE_VENDOR_WARN), video_driver, kc_path ? kc_path : KEYCODE_FILE_NAME);
			WarningAlert(str);
			return;
		}
	}
}

// Open display for current mode
bool SDL_monitor_desc::video_open(void)
{
	D(bug("video_open()\n"));
	const VIDEO_MODE &mode = get_current_mode();
#if DEBUG
	D(bug("Current video mode:\n"));
	D(bug(" %dx%d (ID %02x), %d bpp\n", VIDEO_MODE_X, VIDEO_MODE_Y, VIDEO_MODE_RESOLUTION, 1 << (VIDEO_MODE_DEPTH & 0x0f)));
#endif

	// Create display driver object of requested type
	switch (display_type) {
	case DISPLAY_WINDOW:
		drv = new(std::nothrow) driver_window(*this);
		break;
	}
	if (drv == NULL)
		return false;
	if (!drv->init_ok) {
		delete drv;
		drv = NULL;
		return false;
	}

	// Initialize VideoRefresh function
	VideoRefreshInit();

	// Lock down frame buffer
	LOCK_FRAME_BUFFER;

	// Start redraw/input thread
	redraw_thread_cancel = false;
	redraw_thread_active = ((redraw_thread = SDL_CreateThread(redraw_func, NULL)) != NULL);
	if (!redraw_thread_active) {
		printf("FATAL: cannot create redraw thread\n");
		return false;
	}
	return true;
}

#ifdef SHEEPSHAVER
bool VideoInit(void)
{
	const bool classic = false;
#else
bool VideoInit(bool classic)
{
#endif
	classic_mode = classic;

#ifdef ENABLE_VOSF
	// Zero the mainBuffer structure
	mainBuffer.dirtyPages = NULL;
	mainBuffer.pageInfo = NULL;
#endif

	// Create Mutexes
	if ((sdl_palette_lock = SDL_CreateMutex()) == NULL)
		return false;
	if ((frame_buffer_lock = SDL_CreateMutex()) == NULL)
		return false;

	// Init keycode translation
	keycode_init();

	// Read prefs
	frame_skip = PrefsFindInt32("frameskip");
	mouse_wheel_mode = PrefsFindInt32("mousewheelmode");
	mouse_wheel_lines = PrefsFindInt32("mousewheellines");

	// Get screen mode from preferences
	const char *mode_str = NULL;
#ifndef SHEEPSHAVER
	if (classic_mode)
		mode_str = "win/512/342";
	else
		mode_str = PrefsFindString("screen");
#endif

	// Determine display type and default dimensions
	int default_width, default_height;
	if (classic) {
		default_width = 512;
		default_height = 384;
	}
	else {
		default_width = 640;
		default_height = 480;
	}
	display_type = DISPLAY_WINDOW;
	if (mode_str) {
		if (sscanf(mode_str, "win/%d/%d", &default_width, &default_height) == 2)
			display_type = DISPLAY_WINDOW;
	}
	int max_width = 640, max_height = 480;
	SDL_Rect **modes = SDL_ListModes(NULL, SDL_FULLSCREEN | SDL_HWSURFACE);
	if (modes && modes != (SDL_Rect **)-1) {
		max_width = modes[0]->w;
		max_height = modes[0]->h;
		if (default_width > max_width)
			default_width = max_width;
		if (default_height > max_height)
			default_height = max_height;
	}
	if (default_width <= 0)
		default_width = max_width;
	if (default_height <= 0)
		default_height = max_height;

	// Mac screen depth follows X depth
	screen_depth = SDL_GetVideoInfo()->vfmt->BitsPerPixel;
	int default_depth;
	switch (screen_depth) {
	case 8:
		default_depth = VIDEO_DEPTH_8BIT;
		break;
	case 15: case 16:
		default_depth = VIDEO_DEPTH_16BIT;
		break;
	case 24: case 32:
		default_depth = VIDEO_DEPTH_32BIT;
		break;
	default:
		default_depth =  VIDEO_DEPTH_1BIT;
		break;
	}

	// Construct list of supported modes
	if (display_type == DISPLAY_WINDOW) {
		if (classic)
			add_mode(display_type, 512, 342, 0x80, 64, VIDEO_DEPTH_1BIT);
		else {
			for (int d = VIDEO_DEPTH_1BIT; d <= default_depth; d++) {
				int bpp = (d <= VIDEO_DEPTH_8BIT ? 8 : sdl_depth_of_video_depth(d));
				if (SDL_VideoModeOK(max_width, max_height, bpp, SDL_HWSURFACE))
					add_window_modes(video_depth(d));
			}
		}
	} else
		add_mode(display_type, default_width, default_height, 0x80, TrivialBytesPerRow(default_width, (video_depth)default_depth), default_depth);
	if (VideoModes.empty()) {
		ErrorAlert(STR_NO_XVISUAL_ERR);
		return false;
	}

	// Find requested default mode with specified dimensions
	uint32 default_id;
	std::vector<VIDEO_MODE>::const_iterator i, end = VideoModes.end();
	for (i = VideoModes.begin(); i != end; ++i) {
		const VIDEO_MODE & mode = (*i);
		if (VIDEO_MODE_X == default_width && VIDEO_MODE_Y == default_height && VIDEO_MODE_DEPTH == default_depth) {
			default_id = VIDEO_MODE_RESOLUTION;
#ifdef SHEEPSHAVER
			std::vector<VIDEO_MODE>::const_iterator begin = VideoModes.begin();
			cur_mode = distance(begin, i);
#endif
			break;
		}
	}
	if (i == end) { // not found, use first available mode
		const VIDEO_MODE & mode = VideoModes[0];
		default_depth = VIDEO_MODE_DEPTH;
		default_id = VIDEO_MODE_RESOLUTION;
#ifdef SHEEPSHAVER
		cur_mode = 0;
#endif
	}

#ifdef SHEEPSHAVER
	for (int i = 0; i < VideoModes.size(); i++)
		VModes[i] = VideoModes[i];
	VideoInfo *p = &VModes[VideoModes.size()];
	p->viType = DIS_INVALID;        // End marker
	p->viRowBytes = 0;
	p->viXsize = p->viYsize = 0;
	p->viAppleMode = 0;
	p->viAppleID = 0;
#endif

#if DEBUG
	D(bug("Available video modes:\n"));
	for (i = VideoModes.begin(); i != end; ++i) {
		const VIDEO_MODE & mode = (*i);
		int bits = 1 << VIDEO_MODE_DEPTH;
		if (bits == 16)
			bits = 15;
		else if (bits == 32)
			bits = 24;
		D(bug(" %dx%d (ID %02x), %d colors\n", VIDEO_MODE_X, VIDEO_MODE_Y, VIDEO_MODE_RESOLUTION, 1 << bits));
	}
#endif

	// Create SDL_monitor_desc for this (the only) display
	SDL_monitor_desc *monitor = new SDL_monitor_desc(VideoModes, (video_depth)default_depth, default_id);
	VideoMonitors.push_back(monitor);

	// Open display
	return monitor->video_open();
}


/*
 *  Deinitialization
 */

// Close display
void SDL_monitor_desc::video_close(void)
{
	D(bug("video_close()\n"));

	// Stop redraw thread
	if (redraw_thread_active) {
		redraw_thread_cancel = true;
		SDL_WaitThread(redraw_thread, NULL);
	}
	redraw_thread_active = false;

	// Unlock frame buffer
	UNLOCK_FRAME_BUFFER;
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
}

void VideoExit(void)
{
	// Close displays
	vector<monitor_desc *>::iterator i, end = VideoMonitors.end();
	for (i = VideoMonitors.begin(); i != end; ++i)
		dynamic_cast<SDL_monitor_desc *>(*i)->video_close();

	// Destroy locks
	if (frame_buffer_lock)
		SDL_DestroyMutex(frame_buffer_lock);
	if (sdl_palette_lock)
		SDL_DestroyMutex(sdl_palette_lock);
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

/*
 *  Execute video VBL routine
 */

#ifdef SHEEPSHAVER
void VideoVBL(void)
{
	// Emergency quit requested? Then quit
	if (emerg_quit)
		QuitEmulator();

	// Temporarily give up frame buffer lock (this is the point where
	// we are suspended when the user presses Ctrl-Tab)
	UNLOCK_FRAME_BUFFER;
	LOCK_FRAME_BUFFER;

	// Execute video VBL
	if (private_data != NULL && private_data->interruptsEnabled)
		VSLDoInterruptService(private_data->vslServiceID);
}
#else
void VideoInterrupt(void)
{
	// We must fill in the events queue in the same thread that did call SDL_SetVideoMode()
	SDL_PumpEvents();

	// Emergency quit requested? Then quit
	if (emerg_quit)
		QuitEmulator();

	// Temporarily give up frame buffer lock (this is the point where
	// we are suspended when the user presses Ctrl-Tab)
	UNLOCK_FRAME_BUFFER;
	LOCK_FRAME_BUFFER;
}
#endif


/*
 *  Set palette
 */

#ifdef SHEEPSHAVER
void video_set_palette(void)
{
	monitor_desc * monitor = VideoMonitors[0];
	int n_colors = palette_size(monitor->get_current_mode().viAppleMode);
	uint8 pal[256 * 3];
	for (int c = 0; c < n_colors; c++) {
		pal[c*3 + 0] = mac_pal[c].red;
		pal[c*3 + 1] = mac_pal[c].green;
		pal[c*3 + 2] = mac_pal[c].blue;
	}
	monitor->set_palette(pal, n_colors);
}
#endif

void SDL_monitor_desc::set_palette(uint8 *pal, int num_in)
{
	const VIDEO_MODE &mode = get_current_mode();

	// FIXME: how can we handle the gamma ramp?
	if ((int)VIDEO_MODE_DEPTH > VIDEO_DEPTH_8BIT)
		return;

	LOCK_PALETTE;

	// Convert colors to XColor array
	int num_out = 256;
	bool stretch = false;
	SDL_Color *p = sdl_palette;
	for (int i=0; i<num_out; i++) {
		int c = (stretch ? (i * num_in) / num_out : i);
		p->r = pal[c*3 + 0] * 0x0101;
		p->g = pal[c*3 + 1] * 0x0101;
		p->b = pal[c*3 + 2] * 0x0101;
		p++;
	}

	// Recalculate pixel color expansion map
	if (!IsDirectMode(mode)) {
		for (int i=0; i<256; i++) {
			int c = i & (num_in-1); // If there are less than 256 colors, we repeat the first entries (this makes color expansion easier)
			ExpandMap[i] = SDL_MapRGB(drv->s->format, pal[c*3+0], pal[c*3+1], pal[c*3+2]);
		}

#ifdef ENABLE_VOSF
		if (use_vosf) {
			// We have to redraw everything because the interpretation of pixel values changed
			LOCK_VOSF;
			PFLAG_SET_ALL;
			UNLOCK_VOSF;
			memset(the_buffer_copy, 0, VIDEO_MODE_ROW_BYTES * VIDEO_MODE_Y);
		}
#endif
	}

	// Tell redraw thread to change palette
	sdl_palette_changed = true;

	UNLOCK_PALETTE;
}


/*
 *  Switch video mode
 */

#ifdef SHEEPSHAVER
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

			// Disable interrupts
			DisableInterrupt();

			cur_mode = i;
			monitor_desc *monitor = VideoMonitors[0];
			monitor->switch_to_current_mode();

			WriteMacInt32(ParamPtr + csBaseAddr, screen_base);
			csSave->saveBaseAddr=screen_base;
			csSave->saveData=VModes[cur_mode].viAppleID;/* First mode ... */
			csSave->saveMode=VModes[cur_mode].viAppleMode;

			// Enable interrupts
			EnableInterrupt();
			return noErr;
		}
	}
	return paramErr;
}
#endif

void SDL_monitor_desc::switch_to_current_mode(void)
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
 *  Can we set the MacOS cursor image into the window?
 */

#ifdef SHEEPSHAVER
bool video_can_change_cursor(void)
{
	return (display_type == DISPLAY_WINDOW);
}
#endif


/*
 *  Set cursor image for window
 */

#ifdef SHEEPSHAVER
void video_set_cursor(void)
{
	cursor_changed = true;
}
#endif


/*
 *  Install graphics acceleration
 */

#ifdef SHEEPSHAVER
// Rectangle inversion
template< int bpp >
static inline void do_invrect(uint8 *dest, uint32 length)
{
#define INVERT_1(PTR, OFS) ((uint8  *)(PTR))[OFS] = ~((uint8  *)(PTR))[OFS]
#define INVERT_2(PTR, OFS) ((uint16 *)(PTR))[OFS] = ~((uint16 *)(PTR))[OFS]
#define INVERT_4(PTR, OFS) ((uint32 *)(PTR))[OFS] = ~((uint32 *)(PTR))[OFS]
#define INVERT_8(PTR, OFS) ((uint64 *)(PTR))[OFS] = ~((uint64 *)(PTR))[OFS]

#ifndef UNALIGNED_PROFITABLE
	// Align on 16-bit boundaries
	if (bpp < 16 && (((uintptr)dest) & 1)) {
		INVERT_1(dest, 0);
		dest += 1; length -= 1;
	}

	// Align on 32-bit boundaries
	if (bpp < 32 && (((uintptr)dest) & 2)) {
		INVERT_2(dest, 0);
		dest += 2; length -= 2;
	}
#endif

	// Invert 8-byte words
	if (length >= 8) {
		const int r = (length / 8) % 8;
		dest += r * 8;

		int n = ((length / 8) + 7) / 8;
		switch (r) {
		case 0: do {
				dest += 64;
				INVERT_8(dest, -8);
		case 7: INVERT_8(dest, -7);
		case 6: INVERT_8(dest, -6);
		case 5: INVERT_8(dest, -5);
		case 4: INVERT_8(dest, -4);
		case 3: INVERT_8(dest, -3);
		case 2: INVERT_8(dest, -2);
		case 1: INVERT_8(dest, -1);
				} while (--n > 0);
		}
	}

	// 32-bit cell to invert?
	if (length & 4) {
		INVERT_4(dest, 0);
		if (bpp <= 16)
			dest += 4;
	}

	// 16-bit cell to invert?
	if (bpp <= 16 && (length & 2)) {
		INVERT_2(dest, 0);
		if (bpp <= 8)
			dest += 2;
	}

	// 8-bit cell to invert?
	if (bpp <= 8 && (length & 1))
		INVERT_1(dest, 0);

#undef INVERT_1
#undef INVERT_2
#undef INVERT_4
#undef INVERT_8
}

void NQD_invrect(uint32 p)
{
	D(bug("accl_invrect %08x\n", p));

	// Get inversion parameters
	int16 dest_X = (int16)ReadMacInt16(p + acclDestRect + 2) - (int16)ReadMacInt16(p + acclDestBoundsRect + 2);
	int16 dest_Y = (int16)ReadMacInt16(p + acclDestRect + 0) - (int16)ReadMacInt16(p + acclDestBoundsRect + 0);
	int16 width  = (int16)ReadMacInt16(p + acclDestRect + 6) - (int16)ReadMacInt16(p + acclDestRect + 2);
	int16 height = (int16)ReadMacInt16(p + acclDestRect + 4) - (int16)ReadMacInt16(p + acclDestRect + 0);
	D(bug(" dest X %d, dest Y %d\n", dest_X, dest_Y));
	D(bug(" width %d, height %d, bytes_per_row %d\n", width, height, (int32)ReadMacInt32(p + acclDestRowBytes)));

	//!!?? pen_mode == 14

	// And perform the inversion
	const int bpp = bytes_per_pixel(ReadMacInt32(p + acclDestPixelSize));
	const int dest_row_bytes = (int32)ReadMacInt32(p + acclDestRowBytes);
	uint8 *dest = Mac2HostAddr(ReadMacInt32(p + acclDestBaseAddr) + (dest_Y * dest_row_bytes) + (dest_X * bpp));
	width *= bpp;
	switch (bpp) {
	case 1:
		for (int i = 0; i < height; i++) {
			do_invrect<8>(dest, width);
			dest += dest_row_bytes;
		}
		break;
	case 2:
		for (int i = 0; i < height; i++) {
			do_invrect<16>(dest, width);
			dest += dest_row_bytes;
		}
		break;
	case 4:
		for (int i = 0; i < height; i++) {
			do_invrect<32>(dest, width);
			dest += dest_row_bytes;
		}
		break;
	}
}

// Rectangle filling
template< int bpp >
static inline void do_fillrect(uint8 *dest, uint32 color, uint32 length)
{
#define FILL_1(PTR, OFS, VAL) ((uint8  *)(PTR))[OFS] = (VAL)
#define FILL_2(PTR, OFS, VAL) ((uint16 *)(PTR))[OFS] = (VAL)
#define FILL_4(PTR, OFS, VAL) ((uint32 *)(PTR))[OFS] = (VAL)
#define FILL_8(PTR, OFS, VAL) ((uint64 *)(PTR))[OFS] = (VAL)

#ifndef UNALIGNED_PROFITABLE
	// Align on 16-bit boundaries
	if (bpp < 16 && (((uintptr)dest) & 1)) {
		FILL_1(dest, 0, color);
		dest += 1; length -= 1;
	}

	// Align on 32-bit boundaries
	if (bpp < 32 && (((uintptr)dest) & 2)) {
		FILL_2(dest, 0, color);
		dest += 2; length -= 2;
	}
#endif

	// Fill 8-byte words
	if (length >= 8) {
		const uint64 c = (((uint64)color) << 32) | color;
		const int r = (length / 8) % 8;
		dest += r * 8;

		int n = ((length / 8) + 7) / 8;
		switch (r) {
		case 0: do {
				dest += 64;
				FILL_8(dest, -8, c);
		case 7: FILL_8(dest, -7, c);
		case 6: FILL_8(dest, -6, c);
		case 5: FILL_8(dest, -5, c);
		case 4: FILL_8(dest, -4, c);
		case 3: FILL_8(dest, -3, c);
		case 2: FILL_8(dest, -2, c);
		case 1: FILL_8(dest, -1, c);
				} while (--n > 0);
		}
	}

	// 32-bit cell to fill?
	if (length & 4) {
		FILL_4(dest, 0, color);
		if (bpp <= 16)
			dest += 4;
	}

	// 16-bit cell to fill?
	if (bpp <= 16 && (length & 2)) {
		FILL_2(dest, 0, color);
		if (bpp <= 8)
			dest += 2;
	}

	// 8-bit cell to fill?
	if (bpp <= 8 && (length & 1))
		FILL_1(dest, 0, color);

#undef FILL_1
#undef FILL_2
#undef FILL_4
#undef FILL_8
}

void NQD_fillrect(uint32 p)
{
	D(bug("accl_fillrect %08x\n", p));

	// Get filling parameters
	int16 dest_X = (int16)ReadMacInt16(p + acclDestRect + 2) - (int16)ReadMacInt16(p + acclDestBoundsRect + 2);
	int16 dest_Y = (int16)ReadMacInt16(p + acclDestRect + 0) - (int16)ReadMacInt16(p + acclDestBoundsRect + 0);
	int16 width  = (int16)ReadMacInt16(p + acclDestRect + 6) - (int16)ReadMacInt16(p + acclDestRect + 2);
	int16 height = (int16)ReadMacInt16(p + acclDestRect + 4) - (int16)ReadMacInt16(p + acclDestRect + 0);
	uint32 color = htonl(ReadMacInt32(p + acclPenMode) == 8 ? ReadMacInt32(p + acclForePen) : ReadMacInt32(p + acclBackPen));
	D(bug(" dest X %d, dest Y %d\n", dest_X, dest_Y));
	D(bug(" width %d, height %d\n", width, height));
	D(bug(" bytes_per_row %d color %08x\n", (int32)ReadMacInt32(p + acclDestRowBytes), color));

	// And perform the fill
	const int bpp = bytes_per_pixel(ReadMacInt32(p + acclDestPixelSize));
	const int dest_row_bytes = (int32)ReadMacInt32(p + acclDestRowBytes);
	uint8 *dest = Mac2HostAddr(ReadMacInt32(p + acclDestBaseAddr) + (dest_Y * dest_row_bytes) + (dest_X * bpp));
	width *= bpp;
	switch (bpp) {
	case 1:
		for (int i = 0; i < height; i++) {
			memset(dest, color, width);
			dest += dest_row_bytes;
		}
		break;
	case 2:
		for (int i = 0; i < height; i++) {
			do_fillrect<16>(dest, color, width);
			dest += dest_row_bytes;
		}
		break;
	case 4:
		for (int i = 0; i < height; i++) {
			do_fillrect<32>(dest, color, width);
			dest += dest_row_bytes;
		}
		break;
	}
}

bool NQD_fillrect_hook(uint32 p)
{
	D(bug("accl_fillrect_hook %08x\n", p));

	// Check if we can accelerate this fillrect
	if (ReadMacInt32(p + 0x284) != 0 && ReadMacInt32(p + acclDestPixelSize) >= 8) {
		const int transfer_mode = ReadMacInt32(p + acclTransferMode);
		if (transfer_mode == 8) {
			// Fill
			WriteMacInt32(p + acclDrawProc, NativeTVECT(NATIVE_FILLRECT));
			return true;
		}
		else if (transfer_mode == 10) {
			// Invert
			WriteMacInt32(p + acclDrawProc, NativeTVECT(NATIVE_INVRECT));
			return true;
		}
	}
	return false;
}

// Rectangle blitting
// TODO: optimize for VOSF and target pixmap == screen
void NQD_bitblt(uint32 p)
{
	D(bug("accl_bitblt %08x\n", p));

	// Get blitting parameters
	int16 src_X  = (int16)ReadMacInt16(p + acclSrcRect + 2) - (int16)ReadMacInt16(p + acclSrcBoundsRect + 2);
	int16 src_Y  = (int16)ReadMacInt16(p + acclSrcRect + 0) - (int16)ReadMacInt16(p + acclSrcBoundsRect + 0);
	int16 dest_X = (int16)ReadMacInt16(p + acclDestRect + 2) - (int16)ReadMacInt16(p + acclDestBoundsRect + 2);
	int16 dest_Y = (int16)ReadMacInt16(p + acclDestRect + 0) - (int16)ReadMacInt16(p + acclDestBoundsRect + 0);
	int16 width  = (int16)ReadMacInt16(p + acclDestRect + 6) - (int16)ReadMacInt16(p + acclDestRect + 2);
	int16 height = (int16)ReadMacInt16(p + acclDestRect + 4) - (int16)ReadMacInt16(p + acclDestRect + 0);
	D(bug(" src addr %08x, dest addr %08x\n", ReadMacInt32(p + acclSrcBaseAddr), ReadMacInt32(p + acclDestBaseAddr)));
	D(bug(" src X %d, src Y %d, dest X %d, dest Y %d\n", src_X, src_Y, dest_X, dest_Y));
	D(bug(" width %d, height %d\n", width, height));

	// And perform the blit
	const int bpp = bytes_per_pixel(ReadMacInt32(p + acclSrcPixelSize));
	width *= bpp;
	if ((int32)ReadMacInt32(p + acclSrcRowBytes) > 0) {
		const int src_row_bytes = (int32)ReadMacInt32(p + acclSrcRowBytes);
		const int dst_row_bytes = (int32)ReadMacInt32(p + acclDestRowBytes);
		uint8 *src = Mac2HostAddr(ReadMacInt32(p + acclSrcBaseAddr) + (src_Y * src_row_bytes) + (src_X * bpp));
		uint8 *dst = Mac2HostAddr(ReadMacInt32(p + acclDestBaseAddr) + (dest_Y * dst_row_bytes) + (dest_X * bpp));
		for (int i = 0; i < height; i++) {
			memmove(dst, src, width);
			src += src_row_bytes;
			dst += dst_row_bytes;
		}
	}
	else {
		const int src_row_bytes = -(int32)ReadMacInt32(p + acclSrcRowBytes);
		const int dst_row_bytes = -(int32)ReadMacInt32(p + acclDestRowBytes);
		uint8 *src = Mac2HostAddr(ReadMacInt32(p + acclSrcBaseAddr) + ((src_Y + height - 1) * src_row_bytes) + (src_X * bpp));
		uint8 *dst = Mac2HostAddr(ReadMacInt32(p + acclDestBaseAddr) + ((dest_Y + height - 1) * dst_row_bytes) + (dest_X * bpp));
		for (int i = height - 1; i >= 0; i--) {
			memmove(dst, src, width);
			src -= src_row_bytes;
			dst -= dst_row_bytes;
		}
	}
}

/*
  BitBlt transfer modes:
  0 : srcCopy
  1 : srcOr
  2 : srcXor
  3 : srcBic
  4 : notSrcCopy
  5 : notSrcOr
  6 : notSrcXor
  7 : notSrcBic
  32 : blend
  33 : addPin
  34 : addOver
  35 : subPin
  36 : transparent
  37 : adMax
  38 : subOver
  39 : adMin
  50 : hilite
*/

bool NQD_bitblt_hook(uint32 p)
{
	D(bug("accl_draw_hook %08x\n", p));

	// Check if we can accelerate this bitblt
	if (ReadMacInt32(p + 0x018) + ReadMacInt32(p + 0x128) == 0 &&
		ReadMacInt32(p + 0x130) == 0 &&
		ReadMacInt32(p + acclSrcPixelSize) >= 8 &&
		ReadMacInt32(p + acclSrcPixelSize) == ReadMacInt32(p + acclDestPixelSize) &&
		(ReadMacInt32(p + acclSrcRowBytes) ^ ReadMacInt32(p + acclDestRowBytes)) >= 0 && // same sign?
		ReadMacInt32(p + acclTransferMode) == 0 &&										 // srcCopy?
		ReadMacInt32(p + 0x15c) > 0) {

		// Yes, set function pointer
		WriteMacInt32(p + acclDrawProc, NativeTVECT(NATIVE_BITBLT));
		return true;
	}
	return false;
}

// Wait for graphics operation to finish
bool NQD_sync_hook(uint32 arg)
{
	D(bug("accl_sync_hook %08x\n", arg));
	return true;
}

void VideoInstallAccel(void)
{
	// Install acceleration hooks
	if (PrefsFindBool("gfxaccel")) {
		D(bug("Video: Installing acceleration hooks\n"));
		uint32 base;

		SheepVar bitblt_hook_info(sizeof(accl_hook_info));
		base = bitblt_hook_info.addr();
		WriteMacInt32(base + 0, NativeTVECT(NATIVE_BITBLT_HOOK));
		WriteMacInt32(base + 4, NativeTVECT(NATIVE_SYNC_HOOK));
		WriteMacInt32(base + 8, ACCL_BITBLT);
		NQDMisc(6, bitblt_hook_info.ptr());

		SheepVar fillrect_hook_info(sizeof(accl_hook_info));
		base = fillrect_hook_info.addr();
		WriteMacInt32(base + 0, NativeTVECT(NATIVE_FILLRECT_HOOK));
		WriteMacInt32(base + 4, NativeTVECT(NATIVE_SYNC_HOOK));
		WriteMacInt32(base + 8, ACCL_FILLRECT);
		NQDMisc(6, fillrect_hook_info.ptr());
	}
}
#endif


/*
 *  Keyboard-related utilify functions
 */

static bool is_modifier_key(SDL_KeyboardEvent const & e)
{
	switch (e.keysym.sym) {
	case SDLK_NUMLOCK:
	case SDLK_CAPSLOCK:
	case SDLK_SCROLLOCK:
	case SDLK_RSHIFT:
	case SDLK_LSHIFT:
	case SDLK_RCTRL:
	case SDLK_LCTRL:
	case SDLK_RALT:
	case SDLK_LALT:
	case SDLK_RMETA:
	case SDLK_LMETA:
	case SDLK_LSUPER:
	case SDLK_RSUPER:
	case SDLK_MODE:
	case SDLK_COMPOSE:
		return true;
	}
	return false;
}

static bool is_ctrl_down(SDL_keysym const & ks)
{
	return ctrl_down || (ks.mod & KMOD_CTRL);
}


/*
 *  Translate key event to Mac keycode, returns -1 if no keycode was found
 *  and -2 if the key was recognized as a hotkey
 */

static int kc_decode(SDL_keysym const & ks, bool key_down)
{
	switch (ks.sym) {
	case SDLK_a: return 0x00;
	case SDLK_b: return 0x0b;
	case SDLK_c: return 0x08;
	case SDLK_d: return 0x02;
	case SDLK_e: return 0x0e;
	case SDLK_f: return 0x03;
	case SDLK_g: return 0x05;
	case SDLK_h: return 0x04;
	case SDLK_i: return 0x22;
	case SDLK_j: return 0x26;
	case SDLK_k: return 0x28;
	case SDLK_l: return 0x25;
	case SDLK_m: return 0x2e;
	case SDLK_n: return 0x2d;
	case SDLK_o: return 0x1f;
	case SDLK_p: return 0x23;
	case SDLK_q: return 0x0c;
	case SDLK_r: return 0x0f;
	case SDLK_s: return 0x01;
	case SDLK_t: return 0x11;
	case SDLK_u: return 0x20;
	case SDLK_v: return 0x09;
	case SDLK_w: return 0x0d;
	case SDLK_x: return 0x07;
	case SDLK_y: return 0x10;
	case SDLK_z: return 0x06;

	case SDLK_1: case SDLK_EXCLAIM: return 0x12;
	case SDLK_2: case SDLK_AT: return 0x13;
//	case SDLK_3: case SDLK_numbersign: return 0x14;
	case SDLK_4: case SDLK_DOLLAR: return 0x15;
//	case SDLK_5: case SDLK_percent: return 0x17;
	case SDLK_6: return 0x16;
	case SDLK_7: return 0x1a;
	case SDLK_8: return 0x1c;
	case SDLK_9: return 0x19;
	case SDLK_0: return 0x1d;

//	case SDLK_BACKQUOTE: case SDLK_asciitilde: return 0x0a;
	case SDLK_MINUS: case SDLK_UNDERSCORE: return 0x1b;
	case SDLK_EQUALS: case SDLK_PLUS: return 0x18;
//	case SDLK_bracketleft: case SDLK_braceleft: return 0x21;
//	case SDLK_bracketright: case SDLK_braceright: return 0x1e;
//	case SDLK_BACKSLASH: case SDLK_bar: return 0x2a;
	case SDLK_SEMICOLON: case SDLK_COLON: return 0x29;
//	case SDLK_apostrophe: case SDLK_QUOTEDBL: return 0x27;
	case SDLK_COMMA: case SDLK_LESS: return 0x2b;
	case SDLK_PERIOD: case SDLK_GREATER: return 0x2f;
	case SDLK_SLASH: case SDLK_QUESTION: return 0x2c;

	case SDLK_TAB: if (is_ctrl_down(ks)) {if (!key_down) drv->suspend(); return -2;} else return 0x30;
	case SDLK_RETURN: return 0x24;
	case SDLK_SPACE: return 0x31;
	case SDLK_BACKSPACE: return 0x33;

	case SDLK_DELETE: return 0x75;
	case SDLK_INSERT: return 0x72;
	case SDLK_HOME: case SDLK_HELP: return 0x73;
	case SDLK_END: return 0x77;
	case SDLK_PAGEUP: return 0x74;
	case SDLK_PAGEDOWN: return 0x79;

	case SDLK_LCTRL: return 0x36;
	case SDLK_RCTRL: return 0x36;
	case SDLK_LSHIFT: return 0x38;
	case SDLK_RSHIFT: return 0x38;
#if (defined(__APPLE__) && defined(__MACH__))
	case SDLK_LALT: return 0x3a;
	case SDLK_RALT: return 0x3a;
	case SDLK_LMETA: return 0x37;
	case SDLK_RMETA: return 0x37;
#else
	case SDLK_LALT: return 0x37;
	case SDLK_RALT: return 0x37;
	case SDLK_LMETA: return 0x3a;
	case SDLK_RMETA: return 0x3a;
#endif
	case SDLK_MENU: return 0x32;
	case SDLK_CAPSLOCK: return 0x39;
	case SDLK_NUMLOCK: return 0x47;

	case SDLK_UP: return 0x3e;
	case SDLK_DOWN: return 0x3d;
	case SDLK_LEFT: return 0x3b;
	case SDLK_RIGHT: return 0x3c;

	case SDLK_ESCAPE: if (is_ctrl_down(ks)) {if (!key_down) { quit_full_screen = true; emerg_quit = true; } return -2;} else return 0x35;

	case SDLK_F1: if (is_ctrl_down(ks)) {if (!key_down) SysMountFirstFloppy(); return -2;} else return 0x7a;
	case SDLK_F2: return 0x78;
	case SDLK_F3: return 0x63;
	case SDLK_F4: return 0x76;
	case SDLK_F5: if (is_ctrl_down(ks)) {if (!key_down) drv->toggle_mouse_grab(); return -2;} else return 0x60;
	case SDLK_F6: return 0x61;
	case SDLK_F7: return 0x62;
	case SDLK_F8: return 0x64;
	case SDLK_F9: return 0x65;
	case SDLK_F10: return 0x6d;
	case SDLK_F11: return 0x67;
	case SDLK_F12: return 0x6f;

	case SDLK_PRINT: return 0x69;
	case SDLK_SCROLLOCK: return 0x6b;
	case SDLK_PAUSE: return 0x71;

	case SDLK_KP0: return 0x52;
	case SDLK_KP1: return 0x53;
	case SDLK_KP2: return 0x54;
	case SDLK_KP3: return 0x55;
	case SDLK_KP4: return 0x56;
	case SDLK_KP5: return 0x57;
	case SDLK_KP6: return 0x58;
	case SDLK_KP7: return 0x59;
	case SDLK_KP8: return 0x5b;
	case SDLK_KP9: return 0x5c;
	case SDLK_KP_PERIOD: return 0x41;
	case SDLK_KP_PLUS: return 0x45;
	case SDLK_KP_MINUS: return 0x4e;
	case SDLK_KP_MULTIPLY: return 0x43;
	case SDLK_KP_DIVIDE: return 0x4b;
	case SDLK_KP_ENTER: return 0x4c;
	case SDLK_KP_EQUALS: return 0x51;
	}
	D(bug("Unhandled SDL keysym: %d\n", ks.sym));
	return -1;
}

static int event2keycode(SDL_KeyboardEvent const &ev, bool key_down)
{
	return kc_decode(ev.keysym, key_down);
}


/*
 *  SDL event handling
 */

static void handle_events(void)
{
	SDL_Event events[10];
	const int n_max_events = sizeof(events) / sizeof(events[0]);
	int n_events;

	while ((n_events = SDL_PeepEvents(events, n_max_events, SDL_GETEVENT, sdl_eventmask)) > 0) {
		for (int i = 0; i < n_events; i++) {
			SDL_Event const & event = events[i];
			switch (event.type) {

			// Mouse button
			case SDL_MOUSEBUTTONDOWN: {
				unsigned int button = event.button.button;
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
			case SDL_MOUSEBUTTONUP: {
				unsigned int button = event.button.button;
				if (button < 4)
					ADBMouseUp(button - 1);
				break;
			}

			// Mouse moved
			case SDL_MOUSEMOTION:
				drv->mouse_moved(event.motion.x, event.motion.y);
				break;

			// Keyboard
			case SDL_KEYDOWN: {
				int code = -1;
				if (use_keycodes && !is_modifier_key(event.key)) {
					if (event2keycode(event.key, true) != -2)	// This is called to process the hotkeys
						code = keycode_table[event.key.keysym.scancode & 0xff];
				} else
					code = event2keycode(event.key, true);
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
			case SDL_KEYUP: {
				int code = -1;
				if (use_keycodes && !is_modifier_key(event.key)) {
					if (event2keycode(event.key, false) != -2)	// This is called to process the hotkeys
						code = keycode_table[event.key.keysym.scancode & 0xff];
				} else
					code = event2keycode(event.key, false);
				if (code >= 0) {
					if (code == 0x39) {	// Caps Lock released
						if (caps_on) {
							ADBKeyUp(code);
							caps_on = false;
						} else {
							ADBKeyDown(code);
							caps_on = true;
						}
					} else
						ADBKeyUp(code);
					if (code == 0x36)
						ctrl_down = false;
				}
				break;
			}

			// Hidden parts exposed, force complete refresh of window
			case SDL_VIDEOEXPOSE:
				if (display_type == DISPLAY_WINDOW) {
					const VIDEO_MODE &mode = VideoMonitors[0]->get_current_mode();
#ifdef ENABLE_VOSF
					if (use_vosf) {			// VOSF refresh
						LOCK_VOSF;
						PFLAG_SET_ALL;
						UNLOCK_VOSF;
						memset(the_buffer_copy, 0, VIDEO_MODE_ROW_BYTES * VIDEO_MODE_Y);
					}
					else
#endif
						memset(the_buffer_copy, 0, VIDEO_MODE_ROW_BYTES * VIDEO_MODE_Y);
				}
				break;

			// Window "close" widget clicked
			case SDL_QUIT:
				ADBKeyDown(0x7f);	// Power key
				ADBKeyUp(0x7f);
				break;
			}
		}
	}
}


/*
 *  Window display update
 */

// Static display update (fixed frame rate, but incremental)
static void update_display_static(driver_window *drv)
{
	// Incremental update code
	int wide = 0, high = 0, x1, x2, y1, y2, i, j;
	const VIDEO_MODE &mode = drv->mode;
	int bytes_per_row = VIDEO_MODE_ROW_BYTES;
	uint8 *p, *p2;

	// Check for first line from top and first line from bottom that have changed
	y1 = 0;
	for (j=0; j<VIDEO_MODE_Y; j++) {
		if (memcmp(&the_buffer[j * bytes_per_row], &the_buffer_copy[j * bytes_per_row], bytes_per_row)) {
			y1 = j;
			break;
		}
	}
	y2 = y1 - 1;
	for (j=VIDEO_MODE_Y-1; j>=y1; j--) {
		if (memcmp(&the_buffer[j * bytes_per_row], &the_buffer_copy[j * bytes_per_row], bytes_per_row)) {
			y2 = j;
			break;
		}
	}
	high = y2 - y1 + 1;

	// Check for first column from left and first column from right that have changed
	if (high) {
		if ((int)VIDEO_MODE_DEPTH < VIDEO_DEPTH_8BIT) {
			const int src_bytes_per_row = bytes_per_row;
			const int dst_bytes_per_row = drv->s->pitch;
			const int pixels_per_byte = VIDEO_MODE_X / src_bytes_per_row;

			x1 = VIDEO_MODE_X / pixels_per_byte;
			for (j = y1; j <= y2; j++) {
				p = &the_buffer[j * bytes_per_row];
				p2 = &the_buffer_copy[j * bytes_per_row];
				for (i = 0; i < x1; i++) {
					if (*p != *p2) {
						x1 = i;
						break;
					}
					p++; p2++;
				}
			}
			x2 = x1;
			for (j = y1; j <= y2; j++) {
				p = &the_buffer[j * bytes_per_row];
				p2 = &the_buffer_copy[j * bytes_per_row];
				p += bytes_per_row;
				p2 += bytes_per_row;
				for (i = (VIDEO_MODE_X / pixels_per_byte); i > x2; i--) {
					p--; p2--;
					if (*p != *p2) {
						x2 = i;
						break;
					}
				}
			}
			x1 *= pixels_per_byte;
			x2 *= pixels_per_byte;
			wide = (x2 - x1 + pixels_per_byte - 1) & -pixels_per_byte;

			// Update copy of the_buffer
			if (high && wide) {

				// Lock surface, if required
				if (SDL_MUSTLOCK(drv->s))
					SDL_LockSurface(drv->s);

				// Blit to screen surface
				int si = y1 * src_bytes_per_row + (x1 / pixels_per_byte);
				int di = y1 * dst_bytes_per_row + x1;
				for (j = y1; j <= y2; j++) {
					memcpy(the_buffer_copy + si, the_buffer + si, wide / pixels_per_byte);
					Screen_blit((uint8 *)drv->s->pixels + di, the_buffer + si, wide / pixels_per_byte);
					si += src_bytes_per_row;
					di += dst_bytes_per_row;
				}

				// Unlock surface, if required
				if (SDL_MUSTLOCK(drv->s))
					SDL_UnlockSurface(drv->s);

				// Refresh display
				SDL_UpdateRect(drv->s, x1, y1, wide, high);
			}

		} else {
			const int bytes_per_pixel = VIDEO_MODE_ROW_BYTES / VIDEO_MODE_X;

			x1 = VIDEO_MODE_X;
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
				for (i=VIDEO_MODE_X*bytes_per_pixel; i>x2*bytes_per_pixel; i--) {
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

				// Lock surface, if required
				if (SDL_MUSTLOCK(drv->s))
					SDL_LockSurface(drv->s);

				// Blit to screen surface
				for (j=y1; j<=y2; j++) {
					i = j * bytes_per_row + x1 * bytes_per_pixel;
					memcpy(the_buffer_copy + i, the_buffer + i, bytes_per_pixel * wide);
					Screen_blit((uint8 *)drv->s->pixels + i, the_buffer + i, bytes_per_pixel * wide);
				}

				// Unlock surface, if required
				if (SDL_MUSTLOCK(drv->s))
					SDL_UnlockSurface(drv->s);

				// Refresh display
				SDL_UpdateRect(drv->s, x1, y1, wide, high);
			}
		}
	}
}


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

	if (sdl_palette_changed) {
		sdl_palette_changed = false;
		drv->update_palette();
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
			update_display_dga_vosf();
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
			LOCK_VOSF;
			update_display_window_vosf(static_cast<driver_window *>(drv));
			UNLOCK_VOSF;
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


/*
 *  Thread for screen refresh, input handling etc.
 */

static void VideoRefreshInit(void)
{
	// TODO: set up specialised 8bpp VideoRefresh handlers ?
	if (display_type == DISPLAY_SCREEN) {
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
			video_refresh = video_refresh_window_static;
	}
}

const int VIDEO_REFRESH_HZ = 60;
const int VIDEO_REFRESH_DELAY = 1000000 / VIDEO_REFRESH_HZ;

static int redraw_func(void *arg)
{
	uint64 start = GetTicks_usec();
	int64 ticks = 0;
	uint64 next = GetTicks_usec() + VIDEO_REFRESH_DELAY;

	while (!redraw_thread_cancel) {

		// Wait
		next += VIDEO_REFRESH_DELAY;
		int64 delay = next - GetTicks_usec();
		if (delay > 0)
			Delay_usec(delay);
		else if (delay < -VIDEO_REFRESH_DELAY)
			next = GetTicks_usec();
		ticks++;

		// Handle SDL events
		handle_events();

		// Refresh display
		video_refresh();

#ifdef SHEEPSHAVER
		// Set new cursor image if it was changed
		if (cursor_changed && sdl_cursor) {
			cursor_changed = false;
			SDL_FreeCursor(sdl_cursor);
			sdl_cursor = SDL_CreateCursor(MacCursor + 4, MacCursor + 36, 16, 16, MacCursor[2], MacCursor[3]);
			if (sdl_cursor)
				SDL_SetCursor(sdl_cursor);
		}
#endif

		// Set new palette if it was changed
		handle_palette_changes();
	}

	uint64 end = GetTicks_usec();
	D(bug("%lld refreshes in %lld usec = %f refreshes/sec\n", ticks, end - start, ticks * 1000000.0 / (end - start)));
	return 0;
}
