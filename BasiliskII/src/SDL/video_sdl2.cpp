/*
 *  video_sdl2.cpp - Video/graphics emulation, SDL 2.x specific stuff
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

/*
 *  NOTES:
 *    The Ctrl key works like a qualifier for special actions:
 *      Ctrl-Tab = suspend DGA mode (TODO)
 *      Ctrl-Esc = emergency quit
 *      Ctrl-F1 = mount floppy
 *      Ctrl-F5 = grab mouse (in windowed mode)
 *
 *  FIXMEs and TODOs:
 *  - Windows requires an extra mouse event to update the actual cursor image?
 *  - Ctr-Tab for suspend/resume but how? SDL does not support that for non-Linux
 *  - Ctrl-Fn doesn't generate SDL_KEYDOWN events (SDL bug?)
 *  - Mouse acceleration, there is no API in SDL yet for that
 *  - Gamma tables support is likely to be broken here
 *  - Events processing is bound to the general emulation thread as SDL requires
 *    to PumpEvents() within the same thread as the one that called SetVideoMode().
 *    Besides, there can't seem to be a way to call SetVideoMode() from a child thread.
 *  - Backport hw cursor acceleration to Basilisk II?
 *  - Factor out code
 */

#include "sysdeps.h"

#include <SDL.h>
#if SDL_VERSION_ATLEAST(2,0,0)

#include <SDL_mutex.h>
#include <SDL_thread.h>
#include <errno.h>
#include <vector>

#ifdef WIN32
#include <malloc.h> /* alloca() */
#endif

#include <cpu_emulation.h>
#include "main.h"
#include "adb.h"
#include "macos_util.h"
#include "prefs.h"
#include "user_strings.h"
#include "video.h"
#include "video_defs.h"
#include "video_blit.h"
#include "vm_alloc.h"

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
#ifdef WIN32
const char KEYCODE_FILE_NAME[] = "BasiliskII_keycodes";
#elif __MACOSX__
const char KEYCODE_FILE_NAME[] = "BasiliskII_keycodes";
#else
const char KEYCODE_FILE_NAME[] = DATADIR "/keycodes";
#endif


// Global variables
static uint32 frame_skip;							// Prefs items
static int16 mouse_wheel_mode;
static int16 mouse_wheel_lines;

static uint8 *the_buffer = NULL;					// Mac frame buffer (where MacOS draws into)
static uint8 *the_buffer_copy = NULL;				// Copy of Mac frame buffer (for refreshed modes)
static uint32 the_buffer_size;						// Size of allocated the_buffer

static bool redraw_thread_active = false;			// Flag: Redraw thread installed
#ifndef USE_CPU_EMUL_SERVICES
static volatile bool redraw_thread_cancel;			// Flag: Cancel Redraw thread
static SDL_Thread *redraw_thread = NULL;			// Redraw thread
static volatile bool thread_stop_req = false;
static volatile bool thread_stop_ack = false;		// Acknowledge for thread_stop_req
#endif

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
SDL_Window * sdl_window = NULL;				        // Wraps an OS-native window
static SDL_Surface * host_surface = NULL;			// Surface in host-OS display format
static SDL_Surface * guest_surface = NULL;			// Surface in guest-OS display format
static SDL_Renderer * sdl_renderer = NULL;			// Handle to SDL2 renderer
static SDL_threadID sdl_renderer_thread_id = 0;		// Thread ID where the SDL_renderer was created, and SDL_renderer ops should run (for compatibility w/ d3d9)
static SDL_Texture * sdl_texture = NULL;			// Handle to a GPU texture, with which to draw guest_surface to
static SDL_Rect sdl_update_video_rect = {0,0,0,0};  // Union of all rects to update, when updating sdl_texture
static SDL_mutex * sdl_update_video_mutex = NULL;   // Mutex to protect sdl_update_video_rect
static int screen_depth;							// Depth of current screen
static SDL_Cursor *sdl_cursor = NULL;				// Copy of Mac cursor
static SDL_Palette *sdl_palette = NULL;				// Color palette to be used as CLUT and gamma table
static bool sdl_palette_changed = false;			// Flag: Palette changed, redraw thread must set new colors
static bool toggle_fullscreen = false;
static bool did_add_event_watch = false;

static bool mouse_grabbed = false;

// Mutex to protect SDL events
static SDL_mutex *sdl_events_lock = NULL;
#define LOCK_EVENTS SDL_LockMutex(sdl_events_lock)
#define UNLOCK_EVENTS SDL_UnlockMutex(sdl_events_lock)

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
static int present_sdl_video();
static int SDLCALL on_sdl_event_generated(void *userdata, SDL_Event * event);
static bool is_fullscreen(SDL_Window *);

// From sys_unix.cpp
extern void SysMountFirstFloppy(void);


/*
 *  SDL surface locking glue
 */

#ifdef ENABLE_VOSF
#define SDL_VIDEO_LOCK_VOSF_SURFACE(SURFACE) do {				\
	if (sdl_window && SDL_GetWindowFlags(sdl_window) & (SDL_WINDOW_FULLSCREEN))	\
		the_host_buffer = (uint8 *)(SURFACE)->pixels;			\
} while (0)
#else
#define SDL_VIDEO_LOCK_VOSF_SURFACE(SURFACE)
#endif

#define SDL_VIDEO_LOCK_SURFACE(SURFACE) do {	\
	if (SDL_MUSTLOCK(SURFACE)) {				\
		SDL_LockSurface(SURFACE);				\
		SDL_VIDEO_LOCK_VOSF_SURFACE(SURFACE);	\
	}											\
} while (0)

#define SDL_VIDEO_UNLOCK_SURFACE(SURFACE) do {	\
	if (SDL_MUSTLOCK(SURFACE))					\
		SDL_UnlockSurface(SURFACE);				\
} while (0)


/*
 *  Framebuffer allocation routines
 */

static void *vm_acquire_framebuffer(uint32 size)
{
	// always try to reallocate framebuffer at the same address
	static void *fb = VM_MAP_FAILED;
	if (fb != VM_MAP_FAILED) {
		if (vm_acquire_fixed(fb, size) < 0) {
#ifndef SHEEPSHAVER
			printf("FATAL: Could not reallocate framebuffer at previous address\n");
#endif
			fb = VM_MAP_FAILED;
		}
	}
	if (fb == VM_MAP_FAILED)
		fb = vm_acquire(size, VM_MAP_DEFAULT | VM_MAP_32BIT);
	return fb;
}

static inline void vm_release_framebuffer(void *fb, uint32 size)
{
	vm_release(fb, size);
}

static inline int get_customized_color_depth(int default_depth)
{
	int display_color_depth = PrefsFindInt32("displaycolordepth");

	D(bug("Get displaycolordepth %d\n", display_color_depth));

	if(0 == display_color_depth)
		return default_depth;
	else{
		switch (display_color_depth) {
		case 8:
			return VIDEO_DEPTH_8BIT;
		case 15: case 16:
			return VIDEO_DEPTH_16BIT;
		case 24: case 32:
			return VIDEO_DEPTH_32BIT;
		default:
			return default_depth;
		}
	}
}

/*
 *  Windows message handler
 */

#ifdef WIN32
#include <dbt.h>
static WNDPROC sdl_window_proc = NULL;				// Window proc used by SDL

extern void SysMediaArrived(void);
extern void SysMediaRemoved(void);
extern HWND GetMainWindowHandle(void);

static LRESULT CALLBACK windows_message_handler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_DEVICECHANGE:
		if (wParam == DBT_DEVICEREMOVECOMPLETE) {
			DEV_BROADCAST_HDR *p = (DEV_BROADCAST_HDR *)lParam;
			if (p->dbch_devicetype == DBT_DEVTYP_VOLUME)
				SysMediaRemoved();
		}
		else if (wParam == DBT_DEVICEARRIVAL) {
			DEV_BROADCAST_HDR *p = (DEV_BROADCAST_HDR *)lParam;
			if (p->dbch_devicetype == DBT_DEVTYP_VOLUME)
				SysMediaArrived();
		}
		return 0;

	default:
		if (sdl_window_proc)
			return CallWindowProc(sdl_window_proc, hwnd, msg, wParam, lParam);
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}
#endif


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
	if (xsize == 640 && ysize == 480)
		return APPLE_640x480;
	if (xsize == 800 && ysize == 600)
		return APPLE_800x600;
	if (xsize == 1024 && ysize == 768)
		return APPLE_1024x768;
	if (xsize == 1152 && ysize == 768)
		return APPLE_1152x768;
	if (xsize == 1152 && ysize == 900)
		return APPLE_1152x900;
	if (xsize == 1280 && ysize == 1024)
		return APPLE_1280x1024;
	if (xsize == 1600 && ysize == 1200)
		return APPLE_1600x1200;
	return APPLE_CUSTOM;
}

// Display error alert
static void ErrorAlert(int error)
{
	ErrorAlert(GetString(error));
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

// Map video_mode depth ID to numerical depth value
static int mac_depth_of_video_depth(int video_depth)
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

// Map video_mode depth ID to SDL screen depth
static int sdl_depth_of_video_depth(int video_depth)
{
	return (video_depth <= VIDEO_DEPTH_8BIT) ? 8 : mac_depth_of_video_depth(video_depth);
}

// Get screen dimensions
static void sdl_display_dimensions(int &width, int &height)
{
	SDL_DisplayMode desktop_mode;
	const int display_index = 0;	// TODO: try supporting multiple displays
	if (SDL_GetDesktopDisplayMode(display_index, &desktop_mode) != 0) {
		// TODO: report a warning, here?
		width = height = 0;
		return;
	}
	width = desktop_mode.w;
	height = desktop_mode.h;
}

static inline int sdl_display_width(void)
{
	int width, height;
	sdl_display_dimensions(width, height);
	return width;
}

static inline int sdl_display_height(void)
{
	int width, height;
	sdl_display_dimensions(width, height);
	return height;
}

// Check wether specified mode is available
static bool has_mode(int type, int width, int height, int depth)
{
#ifdef SHEEPSHAVER
	// Filter out Classic resolutions
	if (width == 512 && height == 384)
		return false;
#endif

	// Filter out out-of-bounds resolutions
	if (width > sdl_display_width() || height > sdl_display_height())
		return false;

	// Whatever size it is, beyond what we've checked, we'll scale to/from as appropriate.
	return true;
}

// Add mode to list of supported modes
static void add_mode(int type, int width, int height, int resolution_id, int bytes_per_row, int depth)
{
	// Filter out unsupported modes
	if (!has_mode(type, width, height, depth))
		return;

	// Fill in VideoMode entry
	VIDEO_MODE mode;
#ifdef SHEEPSHAVER
	resolution_id = find_apple_resolution(width, height);
	mode.viType = type;
#endif
	VIDEO_MODE_X = width;
	VIDEO_MODE_Y = height;
	VIDEO_MODE_RESOLUTION = resolution_id;
	VIDEO_MODE_ROW_BYTES = bytes_per_row;
	VIDEO_MODE_DEPTH = (video_depth)depth;
	VideoModes.push_back(mode);
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
	if (!sdl_window) {
		return;
	}
	const char *str = GetString(name);
	SDL_SetWindowTitle(sdl_window, str);
}

// Set mouse grab mode
static void set_grab_mode(bool grab)
{
	if (!sdl_window) {
		return;
	}
	SDL_SetWindowGrab(sdl_window, grab ? SDL_TRUE : SDL_FALSE);
}

// Migrate preferences items (XXX to be handled in MigratePrefs())
static void migrate_screen_prefs(void)
{
#ifdef SHEEPSHAVER
	// Look-up priorities are: "screen", "screenmodes", "windowmodes".
	if (PrefsFindString("screen"))
		return;

	uint32 window_modes = PrefsFindInt32("windowmodes");
	uint32 screen_modes = PrefsFindInt32("screenmodes");
	int width = 0, height = 0;
	if (screen_modes) {
		static const struct {
			int id;
			int width;
			int height;
		}
		modes[] = {
			{  1,	 640,	 480 },
			{  2,	 800,	 600 },
			{  4,	1024,	 768 },
			{ 64,	1152,	 768 },
			{  8,	1152,	 900 },
			{ 16,	1280,	1024 },
			{ 32,	1600,	1200 },
			{ 0, }
		};
		for (int i = 0; modes[i].id != 0; i++) {
			if (screen_modes & modes[i].id) {
				if (width < modes[i].width && height < modes[i].height) {
					width = modes[i].width;
					height = modes[i].height;
				}
			}
		}
	} else {
		if (window_modes & 1)
			width = 640, height = 480;
		if (window_modes & 2)
			width = 800, height = 600;
	}
	if (width && height) {
		char str[32];
		sprintf(str, "%s/%d/%d", screen_modes ? "dga" : "win", width, height);
		PrefsReplaceString("screen", str);
	}
#endif
}


/*
 *  Display "driver" classes
 */

class driver_base {
public:
	driver_base(SDL_monitor_desc &m);
	~driver_base();

	void init(); // One-time init
	void set_video_mode(int flags);
	void adapt_to_video_mode();

	void update_palette(void);
	void suspend(void) {}
	void resume(void) {}
	void toggle_mouse_grab(void);
	void mouse_moved(int x, int y) { ADBMouseMoved(x, y); }

	void disable_mouse_accel(void);
	void restore_mouse_accel(void);

	void grab_mouse(void);
	void ungrab_mouse(void);

public:
	SDL_monitor_desc &monitor; // Associated video monitor
	const VIDEO_MODE &mode;    // Video mode handled by the driver

	bool init_ok;	// Initialization succeeded (we can't use exceptions because of -fomit-frame-pointer)
	SDL_Surface *s;	// The surface we draw into
};

#ifdef ENABLE_VOSF
static void update_display_window_vosf(driver_base *drv);
#endif
static void update_display_static(driver_base *drv);

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

static void delete_sdl_video_surfaces()
{
	if (sdl_texture) {
		SDL_DestroyTexture(sdl_texture);
		sdl_texture = NULL;
	}
	
	if (host_surface) {
		if (host_surface == guest_surface) {
			guest_surface = NULL;
		}
		
		SDL_FreeSurface(host_surface);
		host_surface = NULL;
	}
	
	if (guest_surface) {
		SDL_FreeSurface(guest_surface);
		guest_surface = NULL;
	}
}

static void delete_sdl_video_window()
{
	if (sdl_renderer) {
		SDL_DestroyRenderer(sdl_renderer);
		sdl_renderer = NULL;
	}
	
	if (sdl_window) {
		SDL_DestroyWindow(sdl_window);
		sdl_window = NULL;
	}
}

static void shutdown_sdl_video()
{
	delete_sdl_video_surfaces();
	delete_sdl_video_window();
}

static SDL_Surface * init_sdl_video(int width, int height, int bpp, Uint32 flags)
{
    if (guest_surface) {
        delete_sdl_video_surfaces();
    }
    
	int window_width = width;
	int window_height = height;
    Uint32 window_flags = 0;
	const int window_flags_to_monitor = SDL_WINDOW_FULLSCREEN;
	
	if (flags & SDL_WINDOW_FULLSCREEN) {
		SDL_DisplayMode desktop_mode;
		if (SDL_GetDesktopDisplayMode(0, &desktop_mode) != 0) {
			shutdown_sdl_video();
			return NULL;
		}
		window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		window_width = desktop_mode.w;
		window_height = desktop_mode.h;
	}
	
	if (sdl_window) {
		int old_window_width, old_window_height, old_window_flags;
		SDL_GetWindowSize(sdl_window, &old_window_width, &old_window_height);
		old_window_flags = SDL_GetWindowFlags(sdl_window);
		if (old_window_width != window_width ||
			old_window_height != window_height ||
			(old_window_flags & window_flags_to_monitor) != (window_flags & window_flags_to_monitor))
		{
			delete_sdl_video_window();
		}
	}
	
	// Apply anti-aliasing, if and when appropriate (usually in fullscreen)
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

	// Always use a resize-able window.  This helps allow SDL to manage
	// transitions involving fullscreen to or from windowed-mode.
	window_flags |= SDL_WINDOW_RESIZABLE;
	
	if (!sdl_window) {
		sdl_window = SDL_CreateWindow(
			"Basilisk II",
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			window_width,
			window_height,
			window_flags);
		if (!sdl_window) {
			shutdown_sdl_video();
			return NULL;
		}
	}
	
	// Some SDL events (regarding some native-window events), need processing
	// as they are generated.  SDL2 has a facility, SDL_AddEventWatch(), which
	// allows events to be processed as they are generated.
	if (!did_add_event_watch) {
		SDL_AddEventWatch(&on_sdl_event_generated, NULL);
		did_add_event_watch = true;
	}

	if (!sdl_renderer) {
		const char *render_driver = PrefsFindString("sdlrender");
		if (render_driver) {
			if (SDL_strcmp(render_driver, "auto") == 0) {
				SDL_SetHint(SDL_HINT_RENDER_DRIVER, "");
			} else {
				SDL_SetHint(SDL_HINT_RENDER_DRIVER, render_driver);
			}
		}

		sdl_renderer = SDL_CreateRenderer(sdl_window, -1, 0);
		if (!sdl_renderer) {
			shutdown_sdl_video();
			return NULL;
		}
		sdl_renderer_thread_id = SDL_ThreadID();

		SDL_RendererInfo info;
		memset(&info, 0, sizeof(info));
		SDL_GetRendererInfo(sdl_renderer, &info);
		printf("Using SDL_Renderer driver: %s\n", (info.name ? info.name : "(null)"));
	}
    
    if (!sdl_update_video_mutex) {
        sdl_update_video_mutex = SDL_CreateMutex();
    }

	SDL_assert(sdl_texture == NULL);
    sdl_texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!sdl_texture) {
        shutdown_sdl_video();
        return NULL;
    }
    sdl_update_video_rect.x = 0;
    sdl_update_video_rect.y = 0;
    sdl_update_video_rect.w = 0;
    sdl_update_video_rect.h = 0;

	SDL_assert(guest_surface == NULL);
	SDL_assert(host_surface == NULL);
    switch (bpp) {
        case 8:
            guest_surface = SDL_CreateRGBSurface(0, width, height, 8, 0, 0, 0, 0);
            break;
		case 16:
			guest_surface = SDL_CreateRGBSurface(0, width, height, 16, 0x0000F800, 0x000007E0, 0x0000001F, 0x00000000);
			break;
        case 32:
            guest_surface = SDL_CreateRGBSurface(0, width, height, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
            host_surface = guest_surface;
            break;
        default:
            printf("WARNING: An unsupported bpp of %d was used\n", bpp);
            break;
    }
    if (!guest_surface) {
        shutdown_sdl_video();
        return NULL;
    }

    if (!host_surface) {
    	Uint32 texture_format;
    	if (SDL_QueryTexture(sdl_texture, &texture_format, NULL, NULL, NULL) != 0) {
    		printf("ERROR: Unable to get the SDL texture's pixel format: %s\n", SDL_GetError());
    		shutdown_sdl_video();
    		return NULL;
    	}

    	int bpp;
    	Uint32 Rmask, Gmask, Bmask, Amask;
    	if (!SDL_PixelFormatEnumToMasks(texture_format, &bpp, &Rmask, &Gmask, &Bmask, &Amask)) {
    		printf("ERROR: Unable to determine format for host SDL_surface: %s\n", SDL_GetError());
    		shutdown_sdl_video();
    		return NULL;
    	}

        host_surface = SDL_CreateRGBSurface(0, width, height, bpp, Rmask, Gmask, Bmask, Amask);
        if (!host_surface) {
        	printf("ERROR: Unable to create host SDL_surface: %s\n", SDL_GetError());
            shutdown_sdl_video();
            return NULL;
        }
    }

	if (SDL_RenderSetLogicalSize(sdl_renderer, width, height) != 0) {
		printf("ERROR: Unable to set SDL rendeer's logical size (to %dx%d): %s\n",
			   width, height, SDL_GetError());
		shutdown_sdl_video();
		return NULL;
	}

    return guest_surface;
}

static int present_sdl_video()
{
	if (!sdl_renderer || !sdl_texture || !guest_surface) {
		printf("WARNING: A video mode does not appear to have been set.\n");
		return -1;
	}

	// Some systems, such as D3D9, can fail if and when they are used across
	// certain operations.  To address this, only utilize SDL_Renderer in a
	// single thread, preferably the main thread.
	//
	// This was added as part of a fix for https://github.com/DavidLudwig/macemu/issues/21
	// "BasiliskII, Win32: resizing a window does not stretch "
	SDL_assert(SDL_ThreadID() == sdl_renderer_thread_id);

	// Make sure the display's internal (to SDL, possibly the OS) buffer gets
	// cleared.  Not doing so can, if and when letterboxing is applied (whereby
	// colored bars are drawn on the screen's sides to help with aspect-ratio
	// correction), the colored bars can be an unknown color.
	SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0, 0);	// Use black
	SDL_RenderClear(sdl_renderer);						// Clear the display
	
    // We're about to work with sdl_update_video_rect, so stop other threads from
    // modifying it!
    SDL_LockMutex(sdl_update_video_mutex);

    // Convert from the guest OS' pixel format, to the host OS' texture, if necessary.
    if (host_surface != guest_surface &&
		host_surface != NULL &&
		guest_surface != NULL)
	{
		SDL_Rect destRect = sdl_update_video_rect;
		if (SDL_BlitSurface(guest_surface, &sdl_update_video_rect, host_surface, &destRect) != 0) {
            SDL_UnlockMutex(sdl_update_video_mutex);
			return -1;
		}
	}

    // Update the host OS' texture
    void * srcPixels = (void *)((uint8_t *)host_surface->pixels +
        sdl_update_video_rect.y * host_surface->pitch +
        sdl_update_video_rect.x * host_surface->format->BytesPerPixel);

    if (SDL_UpdateTexture(sdl_texture, &sdl_update_video_rect, srcPixels, host_surface->pitch) != 0) {
        SDL_UnlockMutex(sdl_update_video_mutex);
		return -1;
	}

    // We are done working with pixels in host_surface.  Reset sdl_update_video_rect, then let
    // other threads modify it, as-needed.
    sdl_update_video_rect.x = 0;
    sdl_update_video_rect.y = 0;
    sdl_update_video_rect.w = 0;
    sdl_update_video_rect.h = 0;
    SDL_UnlockMutex(sdl_update_video_mutex);

    // Copy the texture to the display
    if (SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL) != 0) {
		return -1;
	}
	
    // Update the display
	SDL_RenderPresent(sdl_renderer);
    
    // Indicate success to the caller!
    return 0;
}

void update_sdl_video(SDL_Surface *s, int numrects, SDL_Rect *rects)
{
    // TODO: make sure SDL_Renderer resources get displayed, if and when
    // MacsBug is running (and VideoInterrupt() might not get called)
    
    SDL_LockMutex(sdl_update_video_mutex);
    for (int i = 0; i < numrects; ++i) {
        SDL_UnionRect(&sdl_update_video_rect, &rects[i], &sdl_update_video_rect);
    }
    SDL_UnlockMutex(sdl_update_video_mutex);
}

void update_sdl_video(SDL_Surface *s, Sint32 x, Sint32 y, Sint32 w, Sint32 h)
{
    SDL_Rect temp = {x, y, w, h};
    update_sdl_video(s, 1, &temp);
}

void driver_base::set_video_mode(int flags)
{
	int depth = sdl_depth_of_video_depth(VIDEO_MODE_DEPTH);
	if ((s = init_sdl_video(VIDEO_MODE_X, VIDEO_MODE_Y, depth, flags)) == NULL)
		return;
#ifdef ENABLE_VOSF
	the_host_buffer = (uint8 *)s->pixels;
#endif
}

void driver_base::init()
{
	set_video_mode(display_type == DISPLAY_SCREEN ? SDL_WINDOW_FULLSCREEN : 0);
	int aligned_height = (VIDEO_MODE_Y + 15) & ~15;

#ifdef ENABLE_VOSF
	use_vosf = true;
	// Allocate memory for frame buffer (SIZE is extended to page-boundary)
	the_buffer_size = page_extend((aligned_height + 2) * s->pitch);
	the_buffer = (uint8 *)vm_acquire_framebuffer(the_buffer_size);
	the_buffer_copy = (uint8 *)malloc(the_buffer_size);
	D(bug("the_buffer = %p, the_buffer_copy = %p, the_host_buffer = %p\n", the_buffer, the_buffer_copy, the_host_buffer));

	// Check whether we can initialize the VOSF subsystem and it's profitable
	if (!video_vosf_init(monitor)) {
		WarningAlert(GetString(STR_VOSF_INIT_ERR));
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
		the_buffer = (uint8 *)vm_acquire_framebuffer(the_buffer_size);
		D(bug("the_buffer = %p, the_buffer_copy = %p\n", the_buffer, the_buffer_copy));
	}

	// Set frame buffer base
	set_mac_frame_buffer(monitor, VIDEO_MODE_DEPTH, true);

	adapt_to_video_mode();
}

void driver_base::adapt_to_video_mode() {
	ADBSetRelMouseMode(mouse_grabbed);

	// Init blitting routines
	SDL_PixelFormat *f = s->format;
	VisualFormat visualFormat;
	visualFormat.depth = sdl_depth_of_video_depth(VIDEO_MODE_DEPTH);
	visualFormat.Rmask = f->Rmask;
	visualFormat.Gmask = f->Gmask;
	visualFormat.Bmask = f->Bmask;
	Screen_blitter_init(visualFormat, true, mac_depth_of_video_depth(VIDEO_MODE_DEPTH));

	// Load gray ramp to 8->16/32 expand map
	if (!IsDirectMode(mode))
		for (int i=0; i<256; i++)
			ExpandMap[i] = SDL_MapRGB(f, i, i, i);


	bool hardware_cursor = false;
#ifdef SHEEPSHAVER
	hardware_cursor = video_can_change_cursor();
	if (hardware_cursor) {
		// Create cursor
		if ((sdl_cursor = SDL_CreateCursor(MacCursor + 4, MacCursor + 36, 16, 16, 0, 0)) != NULL) {
			SDL_SetCursor(sdl_cursor);
		}
	}
	// Tell the video driver there's a change in cursor type
	if (private_data)
		private_data->cursorHardware = hardware_cursor;
#endif
	// Hide cursor
	SDL_ShowCursor(hardware_cursor);

	// Set window name/class
	set_window_name(mouse_grabbed ? (int)STR_WINDOW_TITLE_GRABBED : (int)STR_WINDOW_TITLE);

	// Everything went well
	init_ok = true;
}

driver_base::~driver_base()
{
	ungrab_mouse();
	restore_mouse_accel();

	// HACK: Just delete instances of SDL_Surface and SDL_Texture, rather
	// than also the SDL_Window and SDL_Renderer.  This fixes a bug whereby
	// OSX hosts, when in fullscreen, will, on a guest OS resolution change,
	// do a series of switches (using OSX's "Spaces" feature) to and from
	// the Basilisk II desktop,
	delete_sdl_video_surfaces();	// This deletes instances of SDL_Surface and SDL_Texture
	//shutdown_sdl_video();			// This deletes SDL_Window, SDL_Renderer, in addition to
									// instances of SDL_Surface and SDL_Texture.

	// the_buffer shall always be mapped through vm_acquire_framebuffer()
	if (the_buffer != VM_MAP_FAILED) {
		D(bug(" releasing the_buffer at %p (%d bytes)\n", the_buffer, the_buffer_size));
		vm_release_framebuffer(the_buffer, the_buffer_size);
		the_buffer = NULL;
	}

	// Free frame buffer(s)
	if (!use_vosf) {
		if (the_buffer_copy) {
			free(the_buffer_copy);
			the_buffer_copy = NULL;
		}
	}
#ifdef ENABLE_VOSF
	else {
		if (the_buffer_copy) {
			D(bug(" freeing the_buffer_copy at %p\n", the_buffer_copy));
			free(the_buffer_copy);
			the_buffer_copy = NULL;
		}

		// Deinitialize VOSF
		video_vosf_exit();
	}
#endif

	SDL_ShowCursor(1);
}

// Palette has changed
void driver_base::update_palette(void)
{
	const VIDEO_MODE &mode = monitor.get_current_mode();

	if ((int)VIDEO_MODE_DEPTH <= VIDEO_DEPTH_8BIT) {
		SDL_SetSurfacePalette(s, sdl_palette);
	}
}

// Disable mouse acceleration
void driver_base::disable_mouse_accel(void)
{
}

// Restore mouse acceleration to original value
void driver_base::restore_mouse_accel(void)
{
}

// Toggle mouse grab
void driver_base::toggle_mouse_grab(void)
{
	if (mouse_grabbed)
		ungrab_mouse();
	else
		grab_mouse();
}

static void update_mouse_grab()
{
	if (mouse_grabbed) {
		SDL_SetRelativeMouseMode(SDL_TRUE);
	} else {
		SDL_SetRelativeMouseMode(SDL_FALSE);
	}
}

// Grab mouse, switch to relative mouse mode
void driver_base::grab_mouse(void)
{
	if (!mouse_grabbed) {
		mouse_grabbed = true;
		update_mouse_grab();
		set_window_name(STR_WINDOW_TITLE_GRABBED);
		disable_mouse_accel();
		ADBSetRelMouseMode(true);
	}
}

// Ungrab mouse, switch to absolute mouse mode
void driver_base::ungrab_mouse(void)
{
	if (mouse_grabbed) {
		mouse_grabbed = false;
		update_mouse_grab();
		set_window_name(STR_WINDOW_TITLE);
		restore_mouse_accel();
		ADBSetRelMouseMode(false);
	}
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
			snprintf(str, sizeof(str), GetString(STR_KEYCODE_FILE_WARN), kc_path ? kc_path : KEYCODE_FILE_NAME, strerror(errno));
			WarningAlert(str);
			return;
		}

		// Default translation table
		for (int i=0; i<256; i++)
			keycode_table[i] = -1;

		// Search for server vendor string, then read keycodes
		const char * video_driver = SDL_GetCurrentVideoDriver();
		bool video_driver_found = false;
		char line[256];
		int n_keys = 0;
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
				// Skip aliases as long as we have read keycodes yet
				// Otherwise, it's another mapping and we have to stop
				static const char sdl_str[] = "sdl";
				if (strncmp(line, sdl_str, sizeof(sdl_str) - 1) == 0 && n_keys == 0)
					continue;

				// Read keycode
				int x_code, mac_code;
				if (sscanf(line, "%d %d", &x_code, &mac_code) == 2)
					keycode_table[x_code & 0xff] = mac_code, n_keys++;
				else
					break;
			} else {
				// Search for SDL video driver string
				static const char sdl_str[] = "sdl";
				if (strncmp(line, sdl_str, sizeof(sdl_str) - 1) == 0) {
					char *p = line + sizeof(sdl_str);
					if (video_driver && strstr(video_driver, p) == video_driver)
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
			snprintf(str, sizeof(str), GetString(STR_KEYCODE_VENDOR_WARN), video_driver ? video_driver : "", kc_path ? kc_path : KEYCODE_FILE_NAME);
			WarningAlert(str);
			return;
		}

		D(bug("Using SDL/%s keycodes table, %d key mappings\n", video_driver ? video_driver : "", n_keys));
	}
}

// Open display for current mode
bool SDL_monitor_desc::video_open(void)
{
	D(bug("video_open()\n"));
#if DEBUG
	const VIDEO_MODE &mode = get_current_mode();
	D(bug("Current video mode:\n"));
	D(bug(" %dx%d (ID %02x), %d bpp\n", VIDEO_MODE_X, VIDEO_MODE_Y, VIDEO_MODE_RESOLUTION, 1 << (VIDEO_MODE_DEPTH & 0x0f)));
#endif

	// Create display driver object of requested type
	drv = new(std::nothrow) driver_base(*this);
	if (drv == NULL)
		return false;
	drv->init();
	if (!drv->init_ok) {
		delete drv;
		drv = NULL;
		return false;
	}

#ifdef WIN32
	// Chain in a new message handler for WM_DEVICECHANGE
	HWND the_window = GetMainWindowHandle();
	sdl_window_proc = (WNDPROC)GetWindowLongPtr(the_window, GWLP_WNDPROC);
	SetWindowLongPtr(the_window, GWLP_WNDPROC, (LONG_PTR)windows_message_handler);
#endif

	// Initialize VideoRefresh function
	VideoRefreshInit();

	// Lock down frame buffer
	LOCK_FRAME_BUFFER;

	// Start redraw/input thread
#ifndef USE_CPU_EMUL_SERVICES
	redraw_thread_cancel = false;
	redraw_thread_active = ((redraw_thread = SDL_CreateThread(redraw_func, "Redraw Thread", NULL)) != NULL);
	if (!redraw_thread_active) {
		printf("FATAL: cannot create redraw thread\n");
		return false;
	}
#else
	redraw_thread_active = true;
#endif
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
	if ((sdl_events_lock = SDL_CreateMutex()) == NULL)
		return false;
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
	migrate_screen_prefs();
	const char *mode_str = NULL;
	if (classic_mode)
		mode_str = "win/512/342";
	else
		mode_str = PrefsFindString("screen");

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
		else if (sscanf(mode_str, "dga/%d/%d", &default_width, &default_height) == 2)
			display_type = DISPLAY_SCREEN;
	}
	if (default_width <= 0)
		default_width = sdl_display_width();
	else if (default_width > sdl_display_width())
		default_width = sdl_display_width();
	if (default_height <= 0)
		default_height = sdl_display_height();
	else if (default_height > sdl_display_height())
		default_height = sdl_display_height();

	// Mac screen depth follows X depth
	screen_depth = 32;
	SDL_DisplayMode desktop_mode;
	if (SDL_GetDesktopDisplayMode(0, &desktop_mode) == 0) {
		screen_depth = SDL_BITSPERPIXEL(desktop_mode.format);
	}
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

	// Initialize list of video modes to try
	struct {
		int w;
		int h;
		int resolution_id;
	}
	video_modes[] = {
		{   -1,   -1, 0x80 },
		{  512,  384, 0x80 },
		{  640,  480, 0x81 },
		{  800,  600, 0x82 },
		{ 1024,  768, 0x83 },
		{ 1152,  870, 0x84 },
		{ 1280, 1024, 0x85 },
		{ 1600, 1200, 0x86 },
		{ 0, }
	};
	video_modes[0].w = default_width;
	video_modes[0].h = default_height;

	// Construct list of supported modes
	if (display_type == DISPLAY_WINDOW) {
		if (classic)
			add_mode(display_type, 512, 342, 0x80, 64, VIDEO_DEPTH_1BIT);
		else {
			for (int i = 0; video_modes[i].w != 0; i++) {
				const int w = video_modes[i].w;
				const int h = video_modes[i].h;
				if (i > 0 && (w >= default_width || h >= default_height))
					continue;
				for (int d = VIDEO_DEPTH_1BIT; d <= default_depth; d++)
					add_mode(display_type, w, h, video_modes[i].resolution_id, TrivialBytesPerRow(w, (video_depth)d), d);
			}
		}
	} else if (display_type == DISPLAY_SCREEN) {
		for (int i = 0; video_modes[i].w != 0; i++) {
			const int w = video_modes[i].w;
			const int h = video_modes[i].h;
			if (i > 0 && (w >= default_width || h >= default_height))
				continue;
			for (int d = VIDEO_DEPTH_1BIT; d <= default_depth; d++)
				add_mode(display_type, w, h, video_modes[i].resolution_id, TrivialBytesPerRow(w, (video_depth)d), d);
		}
	}

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

	int color_depth = get_customized_color_depth(default_depth);

	D(bug("Return get_customized_color_depth %d\n", color_depth));

	// Create SDL_monitor_desc for this (the only) display
	SDL_monitor_desc *monitor = new SDL_monitor_desc(VideoModes, (video_depth)color_depth, default_id);
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

#ifdef WIN32
	// Remove message handler for WM_DEVICECHANGE
	HWND the_window = GetMainWindowHandle();
	SetWindowLongPtr(the_window, GWLP_WNDPROC, (LONG_PTR)sdl_window_proc);
#endif

	// Stop redraw thread
#ifndef USE_CPU_EMUL_SERVICES
	if (redraw_thread_active) {
		redraw_thread_cancel = true;
		SDL_WaitThread(redraw_thread, NULL);
	}
#endif
	redraw_thread_active = false;

	// Unlock frame buffer
	UNLOCK_FRAME_BUFFER;
	D(bug(" frame buffer unlocked\n"));

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
	if (sdl_events_lock)
		SDL_DestroyMutex(sdl_events_lock);
}


/*
 *  Close down full-screen mode (if bringing up error alerts is unsafe while in full-screen mode)
 */

void VideoQuitFullScreen(void)
{
	D(bug("VideoQuitFullScreen()\n"));
	quit_full_screen = true;
}

static void do_toggle_fullscreen(void)
{
#ifndef USE_CPU_EMUL_SERVICES
	// pause redraw thread
	thread_stop_ack = false;
	thread_stop_req = true;
	while (!thread_stop_ack) ;
#endif

	// Apply fullscreen
	if (sdl_window) {
		if (display_type == DISPLAY_SCREEN) {
			display_type = DISPLAY_WINDOW;
			SDL_SetWindowFullscreen(sdl_window, 0);
		} else {
			display_type = DISPLAY_SCREEN;
			SDL_SetWindowFullscreen(sdl_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
		}
	}

	// switch modes
	drv->adapt_to_video_mode();

	// reset the palette
#ifdef SHEEPSHAVER
	video_set_palette();
#endif
	drv->update_palette();

	// reset the video refresh handler
	VideoRefreshInit();

	// while SetVideoMode is happening, control key up may be missed
	ADBKeyUp(0x36);
	
	// resume redraw thread
	toggle_fullscreen = false;
#ifndef USE_CPU_EMUL_SERVICES
	thread_stop_req = false;
#endif
}

/*
 *  Mac VBL interrupt
 */

/*
 *  Execute video VBL routine
 */

static bool is_fullscreen(SDL_Window * window)
{
#ifdef __MACOSX__
	// On OSX, SDL, at least as of 2.0.5 (and possibly beyond), does not always
	// report changes to fullscreen via the SDL_WINDOW_FULLSCREEN flag.
	// (Example: https://bugzilla.libsdl.org/show_bug.cgi?id=3766 , which
	// involves fullscreen/windowed toggles via window-manager UI controls).
	// Until it does, or adds a facility to do so, we'll use a platform-specific
	// code path to detect fullscreen changes.
	extern bool is_fullscreen_osx(SDL_Window * window);
	return is_fullscreen_osx(sdl_window);
#else
	if (!window) {
		return false;
	}
	const Uint32 sdl_window_flags = SDL_GetWindowFlags(sdl_window);
	return (sdl_window_flags & SDL_WINDOW_FULLSCREEN) != 0;
#endif
}

#ifdef SHEEPSHAVER
void VideoVBL(void)
{
	// Emergency quit requested? Then quit
	if (emerg_quit)
		QuitEmulator();

	if (toggle_fullscreen)
		do_toggle_fullscreen();
	
	present_sdl_video();

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

	if (toggle_fullscreen)
		do_toggle_fullscreen();

	present_sdl_video();

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
	
	if (!sdl_palette) {
		sdl_palette = SDL_AllocPalette(num_out);
	}
	
	SDL_Color *p = sdl_palette->colors;
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

			// Disable interrupts and pause redraw thread
			DisableInterrupt();
			thread_stop_ack = false;
			thread_stop_req = true;
			while (!thread_stop_ack) ;

			cur_mode = i;
			monitor_desc *monitor = VideoMonitors[0];
			monitor->switch_to_current_mode();

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
#endif

void SDL_monitor_desc::switch_to_current_mode(void)
{
	// Close and reopen display
	LOCK_EVENTS;
	video_close();
	video_open();
	UNLOCK_EVENTS;

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
	if (display_type != DISPLAY_WINDOW)
		return false;

	return true;
}
#endif


/*
 *  Set cursor image for window
 */

#ifdef SHEEPSHAVER
void video_set_cursor(void)
{
	// Set new cursor image if it was changed
	if (sdl_cursor) {
		SDL_FreeCursor(sdl_cursor);
		sdl_cursor = SDL_CreateCursor(MacCursor + 4, MacCursor + 36, 16, 16, MacCursor[2], MacCursor[3]);
		if (sdl_cursor) {
			SDL_ShowCursor(private_data == NULL || private_data->cursorVisible);
			SDL_SetCursor(sdl_cursor);

			// XXX Windows apparently needs an extra mouse event to
			// make the new cursor image visible.
			// On Mac, if mouse is grabbed, SDL_ShowCursor() recenters the
			// mouse, we have to put it back.
			bool move = false;
#ifdef WIN32
			move = true;
#elif defined(__APPLE__)
			move = mouse_grabbed;
#endif
			if (move) {
				int visible = SDL_ShowCursor(-1);
				if (visible) {
					int x, y;
					SDL_GetMouseState(&x, &y);
					printf("WarpMouse to {%d,%d} via video_set_cursor\n", x, y);
					SDL_WarpMouseGlobal(x, y);
				}
			}
		}
	}
}
#endif


/*
 *  Keyboard-related utilify functions
 */

static bool is_modifier_key(SDL_KeyboardEvent const & e)
{
	switch (e.keysym.sym) {
	case SDLK_NUMLOCKCLEAR:
	case SDLK_CAPSLOCK:
	case SDLK_SCROLLLOCK:
	case SDLK_RSHIFT:
	case SDLK_LSHIFT:
	case SDLK_RCTRL:
	case SDLK_LCTRL:
	case SDLK_RALT:
	case SDLK_LALT:
	case SDLK_RGUI:
	case SDLK_LGUI:
	case SDLK_MODE:
	case SDLK_APPLICATION:
		return true;
	}
	return false;
}

static bool is_ctrl_down(SDL_Keysym const & ks)
{
	return ctrl_down || (ks.mod & KMOD_CTRL);
}


/*
 *  Translate key event to Mac keycode, returns -1 if no keycode was found
 *  and -2 if the key was recognized as a hotkey
 */

static int kc_decode(SDL_Keysym const & ks, bool key_down)
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
	case SDLK_3: case SDLK_HASH: return 0x14;
	case SDLK_4: case SDLK_DOLLAR: return 0x15;
	case SDLK_5: return 0x17;
	case SDLK_6: return 0x16;
	case SDLK_7: return 0x1a;
	case SDLK_8: return 0x1c;
	case SDLK_9: return 0x19;
	case SDLK_0: return 0x1d;

	case SDLK_BACKQUOTE: return 0x0a;
	case SDLK_MINUS: case SDLK_UNDERSCORE: return 0x1b;
	case SDLK_EQUALS: case SDLK_PLUS: return 0x18;
	case SDLK_LEFTBRACKET: return 0x21;
	case SDLK_RIGHTBRACKET: return 0x1e;
	case SDLK_BACKSLASH: return 0x2a;
	case SDLK_SEMICOLON: case SDLK_COLON: return 0x29;
	case SDLK_QUOTE: case SDLK_QUOTEDBL: return 0x27;
	case SDLK_COMMA: case SDLK_LESS: return 0x2b;
	case SDLK_PERIOD: case SDLK_GREATER: return 0x2f;
	case SDLK_SLASH: case SDLK_QUESTION: return 0x2c;

	case SDLK_TAB: if (is_ctrl_down(ks)) {if (!key_down) drv->suspend(); return -2;} else return 0x30;
	case SDLK_RETURN: if (is_ctrl_down(ks)) {if (!key_down) toggle_fullscreen = true; return -2;} else return 0x24;
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
	case SDLK_LGUI: return 0x37;
	case SDLK_RGUI: return 0x37;
#else
	case SDLK_LALT: return 0x37;
	case SDLK_RALT: return 0x37;
	case SDLK_LGUI: return 0x3a;
	case SDLK_RGUI: return 0x3a;
#endif
	case SDLK_MENU: return 0x32;
	case SDLK_CAPSLOCK: return 0x39;
	case SDLK_NUMLOCKCLEAR: return 0x47;

	case SDLK_UP: return 0x3e;
	case SDLK_DOWN: return 0x3d;
	case SDLK_LEFT: return 0x3b;
	case SDLK_RIGHT: return 0x3c;

	case SDLK_ESCAPE: if (is_ctrl_down(ks)) {if (!key_down) { quit_full_screen = true; emerg_quit = true; } return -2;} else return 0x35;

	case SDLK_F1: if (is_ctrl_down(ks)) {if (!key_down) SysMountFirstFloppy(); return -2;} else return 0x7a;
	case SDLK_F2: return 0x78;
	case SDLK_F3: return 0x63;
	case SDLK_F4: return 0x76;
	case SDLK_F5: return 0x60;
	case SDLK_F6: return 0x61;
	case SDLK_F7: return 0x62;
	case SDLK_F8: return 0x64;
	case SDLK_F9: return 0x65;
	case SDLK_F10: return 0x6d;
	case SDLK_F11: return 0x67;
	case SDLK_F12: return 0x6f;

	case SDLK_PRINTSCREEN: return 0x69;
	case SDLK_SCROLLLOCK: return 0x6b;
	case SDLK_PAUSE: return 0x71;

	case SDLK_KP_0: return 0x52;
	case SDLK_KP_1: return 0x53;
	case SDLK_KP_2: return 0x54;
	case SDLK_KP_3: return 0x55;
	case SDLK_KP_4: return 0x56;
	case SDLK_KP_5: return 0x57;
	case SDLK_KP_6: return 0x58;
	case SDLK_KP_7: return 0x59;
	case SDLK_KP_8: return 0x5b;
	case SDLK_KP_9: return 0x5c;
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

static void force_complete_window_refresh()
{
	if (display_type == DISPLAY_WINDOW) {
#ifdef ENABLE_VOSF
		if (use_vosf) {	// VOSF refresh
			LOCK_VOSF;
			PFLAG_SET_ALL;
			UNLOCK_VOSF;
		}
#endif
		// Ensure each byte of the_buffer_copy differs from the_buffer to force a full update.
		const VIDEO_MODE &mode = VideoMonitors[0]->get_current_mode();
		const int len = VIDEO_MODE_ROW_BYTES * VIDEO_MODE_Y;
		for (int i = 0; i < len; i++)
			the_buffer_copy[i] = !the_buffer[i];
	}
}

/*
 *  SDL event handling
 */

// possible return codes for SDL-registered event watches
enum {
	EVENT_DROP_FROM_QUEUE = 0,
	EVENT_ADD_TO_QUEUE    = 1
};

// Some events need to be processed in the host-app's main thread, due to
// host-OS requirements.
//
// This function is called by SDL, whenever it generates an SDL_Event.  It has
// the ability to process events, and optionally, to prevent them from being
// added to SDL's event queue (and retrieve-able via SDL_PeepEvents(), etc.)
static int SDLCALL on_sdl_event_generated(void *userdata, SDL_Event * event)
{
	switch (event->type) {
		case SDL_KEYUP: {
			SDL_Keysym const & ks = event->key.keysym;
			switch (ks.sym) {
				case SDLK_F5: {
					if (is_ctrl_down(ks)) {
						drv->toggle_mouse_grab();
						return EVENT_DROP_FROM_QUEUE;
					}
				} break;
			}
		} break;
			
		case SDL_WINDOWEVENT: {
			switch (event->window.event) {
				case SDL_WINDOWEVENT_RESIZED: {
					// Handle changes of fullscreen.  This is done here, in
					// on_sdl_event_generated() and not the main SDL_Event-processing
					// loop, in order to perform this change on the main thread.
					// (Some os'es UI APIs, such as OSX's NSWindow, are not
					// thread-safe.)
					const bool is_full = is_fullscreen(sdl_window);
					const bool adjust_fullscreen = \
						(display_type == DISPLAY_WINDOW && is_full) ||
						(display_type == DISPLAY_SCREEN && !is_full);
					if (adjust_fullscreen) {
						do_toggle_fullscreen();
						
#if __MACOSX__
						// HACK-FIX: on OSX hosts, make sure that the OSX menu
						// bar does not show up in fullscreen mode, when the
						// cursor is near the top of the screen, lest the
						// guest OS' menu bar be obscured.
						if (is_full) {
							extern void set_menu_bar_visible_osx(bool);
							set_menu_bar_visible_osx(false);
						}
#endif
					}
				} break;
			}
		} break;
	}
	
	return EVENT_ADD_TO_QUEUE;
}


static void handle_events(void)
{
	SDL_Event events[10];
	const int n_max_events = sizeof(events) / sizeof(events[0]);
	int n_events;

	while ((n_events = SDL_PeepEvents(events, n_max_events, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT)) > 0) {
		for (int i = 0; i < n_events; i++) {
			SDL_Event & event = events[i];
			
			switch (event.type) {

			// Mouse button
			case SDL_MOUSEBUTTONDOWN: {
				unsigned int button = event.button.button;
				if (button == SDL_BUTTON_LEFT)
					ADBMouseDown(0);
				else if (button == SDL_BUTTON_RIGHT)
					ADBMouseDown(1);
				else if (button == SDL_BUTTON_MIDDLE)
					ADBMouseDown(2);
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
				if (button == SDL_BUTTON_LEFT)
					ADBMouseUp(0);
				else if (button == SDL_BUTTON_RIGHT)
					ADBMouseUp(1);
				else if (button == SDL_BUTTON_MIDDLE)
					ADBMouseUp(2);
				break;
			}

			// Mouse moved
			case SDL_MOUSEMOTION:
				if (mouse_grabbed) {
					drv->mouse_moved(event.motion.xrel, event.motion.yrel);
				} else {
					drv->mouse_moved(event.motion.x, event.motion.y);
				}
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
			
			case SDL_WINDOWEVENT: {
				switch (event.window.event) {
					// Hidden parts exposed, force complete refresh of window
					case SDL_WINDOWEVENT_EXPOSED:
						force_complete_window_refresh();
						break;
					
					// Force a complete window refresh when activating, to avoid redraw artifacts otherwise.
					case SDL_WINDOWEVENT_RESTORED:
						force_complete_window_refresh();
						break;
					
				}
				break;
			}

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
static void update_display_static(driver_base *drv)
{
	// Incremental update code
	int wide = 0, high = 0;
	uint32 x1, x2, y1, y2;
	const VIDEO_MODE &mode = drv->mode;
	int bytes_per_row = VIDEO_MODE_ROW_BYTES;
	uint8 *p, *p2;

	// Check for first line from top and first line from bottom that have changed
	y1 = 0;
	for (uint32 j = 0; j < VIDEO_MODE_Y; j++) {
		if (memcmp(&the_buffer[j * bytes_per_row], &the_buffer_copy[j * bytes_per_row], bytes_per_row)) {
			y1 = j;
			break;
		}
	}
	y2 = y1 - 1;
	for (uint32 j = VIDEO_MODE_Y; j-- > y1; ) {
		if (memcmp(&the_buffer[j * bytes_per_row], &the_buffer_copy[j * bytes_per_row], bytes_per_row)) {
			y2 = j;
			break;
		}
	}
	high = y2 - y1 + 1;

	// Check for first column from left and first column from right that have changed
	if (high) {
		if ((int)VIDEO_MODE_DEPTH < (int)VIDEO_DEPTH_8BIT) {
			const int src_bytes_per_row = bytes_per_row;
			const int dst_bytes_per_row = drv->s->pitch;
			const int pixels_per_byte = VIDEO_MODE_X / src_bytes_per_row;

			x1 = VIDEO_MODE_X / pixels_per_byte;
			for (uint32 j = y1; j <= y2; j++) {
				p = &the_buffer[j * bytes_per_row];
				p2 = &the_buffer_copy[j * bytes_per_row];
				for (uint32 i = 0; i < x1; i++) {
					if (*p != *p2) {
						x1 = i;
						break;
					}
					p++; p2++;
				}
			}
			x2 = x1;
			for (uint32 j = y1; j <= y2; j++) {
				p = &the_buffer[j * bytes_per_row];
				p2 = &the_buffer_copy[j * bytes_per_row];
				p += bytes_per_row;
				p2 += bytes_per_row;
				for (uint32 i = (VIDEO_MODE_X / pixels_per_byte); i > x2; i--) {
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
				for (uint32 j = y1; j <= y2; j++) {
					memcpy(the_buffer_copy + si, the_buffer + si, wide / pixels_per_byte);
					Screen_blit((uint8 *)drv->s->pixels + di, the_buffer + si, wide / pixels_per_byte);
					si += src_bytes_per_row;
					di += dst_bytes_per_row;
				}

				// Unlock surface, if required
				if (SDL_MUSTLOCK(drv->s))
					SDL_UnlockSurface(drv->s);

				// Refresh display
				update_sdl_video(drv->s, x1, y1, wide, high);
			}

		} else {
			const int bytes_per_pixel = VIDEO_MODE_ROW_BYTES / VIDEO_MODE_X;
			const int dst_bytes_per_row = drv->s->pitch;

			x1 = VIDEO_MODE_X;
			for (uint32 j = y1; j <= y2; j++) {
				p = &the_buffer[j * bytes_per_row];
				p2 = &the_buffer_copy[j * bytes_per_row];
				for (uint32 i = 0; i < x1 * bytes_per_pixel; i++) {
					if (*p != *p2) {
						x1 = i / bytes_per_pixel;
						break;
					}
					p++; p2++;
				}
			}
			x2 = x1;
			for (uint32 j = y1; j <= y2; j++) {
				p = &the_buffer[j * bytes_per_row];
				p2 = &the_buffer_copy[j * bytes_per_row];
				p += bytes_per_row;
				p2 += bytes_per_row;
				for (uint32 i = VIDEO_MODE_X * bytes_per_pixel; i > x2 * bytes_per_pixel; i--) {
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
				for (uint32 j = y1; j <= y2; j++) {
					uint32 i = j * bytes_per_row + x1 * bytes_per_pixel;
					int dst_i = j * dst_bytes_per_row + x1 * bytes_per_pixel;
					memcpy(the_buffer_copy + i, the_buffer + i, bytes_per_pixel * wide);
					Screen_blit((uint8 *)drv->s->pixels + dst_i, the_buffer + i, bytes_per_pixel * wide);
				}

				// Unlock surface, if required
				if (SDL_MUSTLOCK(drv->s))
					SDL_UnlockSurface(drv->s);

				// Refresh display
				update_sdl_video(drv->s, x1, y1, wide, high);
			}
		}
	}
}

// Static display update (fixed frame rate, bounding boxes based)
// XXX use NQD bounding boxes to help detect dirty areas?
static void update_display_static_bbox(driver_base *drv)
{
	const VIDEO_MODE &mode = drv->mode;

	// Allocate bounding boxes for SDL_UpdateRects()
	const uint32 N_PIXELS = 64;
	const uint32 n_x_boxes = (VIDEO_MODE_X + N_PIXELS - 1) / N_PIXELS;
	const uint32 n_y_boxes = (VIDEO_MODE_Y + N_PIXELS - 1) / N_PIXELS;
	SDL_Rect *boxes = (SDL_Rect *)alloca(sizeof(SDL_Rect) * n_x_boxes * n_y_boxes);
	uint32 nr_boxes = 0;

	// Lock surface, if required
	if (SDL_MUSTLOCK(drv->s))
		SDL_LockSurface(drv->s);

	// Update the surface from Mac screen
	const uint32 bytes_per_row = VIDEO_MODE_ROW_BYTES;
	const uint32 bytes_per_pixel = bytes_per_row / VIDEO_MODE_X;
	const uint32 dst_bytes_per_row = drv->s->pitch;
	for (uint32 y = 0; y < VIDEO_MODE_Y; y += N_PIXELS) {
		uint32 h = N_PIXELS;
		if (h > VIDEO_MODE_Y - y)
			h = VIDEO_MODE_Y - y;
		for (uint32 x = 0; x < VIDEO_MODE_X; x += N_PIXELS) {
			uint32 w = N_PIXELS;
			if (w > VIDEO_MODE_X - x)
				w = VIDEO_MODE_X - x;
			const int xs = w * bytes_per_pixel;
			const int xb = x * bytes_per_pixel;
			bool dirty = false;
			for (uint32 j = y; j < (y + h); j++) {
				const uint32 yb = j * bytes_per_row;
				const uint32 dst_yb = j * dst_bytes_per_row;
				if (memcmp(&the_buffer[yb + xb], &the_buffer_copy[yb + xb], xs) != 0) {
					memcpy(&the_buffer_copy[yb + xb], &the_buffer[yb + xb], xs);
					Screen_blit((uint8 *)drv->s->pixels + dst_yb + xb, the_buffer + yb + xb, xs);
					dirty = true;
				}
			}
			if (dirty) {
				boxes[nr_boxes].x = x;
				boxes[nr_boxes].y = y;
				boxes[nr_boxes].w = w;
				boxes[nr_boxes].h = h;
				nr_boxes++;
			}
		}
	}

	// Unlock surface, if required
	if (SDL_MUSTLOCK(drv->s))
		SDL_UnlockSurface(drv->s);

	// Refresh display
	if (nr_boxes)
		update_sdl_video(drv->s, nr_boxes, boxes);
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

static void video_refresh_window_static(void);

static void video_refresh_dga(void)
{
	// Quit DGA mode if requested
	possibly_quit_dga_mode();
	video_refresh_window_static();
}

#ifdef ENABLE_VOSF
#if REAL_ADDRESSING || DIRECT_ADDRESSING
static void video_refresh_dga_vosf(void)
{
	// Quit DGA mode if requested
	possibly_quit_dga_mode();
	
	// Update display (VOSF variant)
	static uint32 tick_counter = 0;
	if (++tick_counter >= frame_skip) {
		tick_counter = 0;
		if (mainBuffer.dirty) {
			LOCK_VOSF;
			update_display_dga_vosf(drv);
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
	static uint32 tick_counter = 0;
	if (++tick_counter >= frame_skip) {
		tick_counter = 0;
		if (mainBuffer.dirty) {
			LOCK_VOSF;
			update_display_window_vosf(drv);
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
	static uint32 tick_counter = 0;
	if (++tick_counter >= frame_skip) {
		tick_counter = 0;
		const VIDEO_MODE &mode = drv->mode;
		if ((int)VIDEO_MODE_DEPTH >= VIDEO_DEPTH_8BIT)
			update_display_static_bbox(drv);
		else
			update_display_static(drv);
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

static inline void do_video_refresh(void)
{
	// Handle SDL events
	handle_events();

	// Update display
	video_refresh();


	// Set new palette if it was changed
	handle_palette_changes();
}

// This function is called on non-threaded platforms from a timer interrupt
void VideoRefresh(void)
{
	// We need to check redraw_thread_active to inhibit refreshed during
	// mode changes on non-threaded platforms
	if (!redraw_thread_active)
		return;

	// Process pending events and update display
	do_video_refresh();
}

const int VIDEO_REFRESH_HZ = 60;
const int VIDEO_REFRESH_DELAY = 1000000 / VIDEO_REFRESH_HZ;

#ifndef USE_CPU_EMUL_SERVICES
static int redraw_func(void *arg)
{
	uint64 start = GetTicks_usec();
	int64 ticks = 0;
	uint64 next = GetTicks_usec() + VIDEO_REFRESH_DELAY;

	while (!redraw_thread_cancel) {

		// Wait
		next += VIDEO_REFRESH_DELAY;
		uint64 delay = int32(next - GetTicks_usec());
		if (delay > 0)
			Delay_usec(delay);
		else if (delay < -VIDEO_REFRESH_DELAY)
			next = GetTicks_usec();
		ticks++;

		// Pause if requested (during video mode switches)
		if (thread_stop_req) {
			thread_stop_ack = true;
			continue;
		}

		// Process pending events and update display
		do_video_refresh();
	}

	uint64 end = GetTicks_usec();
	D(bug("%lld refreshes in %lld usec = %f refreshes/sec\n", ticks, end - start, ticks * 1000000.0 / (end - start)));
	return 0;
}
#endif


/*
 *  Record dirty area from NQD
 */

#ifdef SHEEPSHAVER
void video_set_dirty_area(int x, int y, int w, int h)
{
#ifdef ENABLE_VOSF
	const VIDEO_MODE &mode = drv->mode;
	const unsigned screen_width = VIDEO_MODE_X;
	const unsigned screen_height = VIDEO_MODE_Y;
	const unsigned bytes_per_row = VIDEO_MODE_ROW_BYTES;

	if (use_vosf) {
		vosf_set_dirty_area(x, y, w, h, screen_width, screen_height, bytes_per_row);
		return;
	}
#endif

	// XXX handle dirty bounding boxes for non-VOSF modes
}
#endif

#endif	// ends: SDL version check
