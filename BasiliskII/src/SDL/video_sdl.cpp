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
 */

#include "sysdeps.h"

#include <SDL.h>
#include <SDL_mutex.h>
#include <SDL_thread.h>
#include <errno.h>

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
	DISPLAY_WINDOW,	// windowed display
	DISPLAY_SCREEN	// fullscreen display
};

// Constants
const char KEYCODE_FILE_NAME[] = DATADIR "/keycodes";


// Global variables
static int32 frame_skip;							// Prefs items
static int16 mouse_wheel_mode;
static int16 mouse_wheel_lines;

static int display_type = DISPLAY_WINDOW;			// See enum above
static uint8 *the_buffer = NULL;					// Mac frame buffer (where MacOS draws into)
static uint8 *the_buffer_copy = NULL;				// Copy of Mac frame buffer (for refreshed modes)
static uint32 the_buffer_size;						// Size of allocated the_buffer

static bool redraw_thread_active = false;			// Flag: Redraw thread installed
static volatile bool redraw_thread_cancel;			// Flag: Cancel Redraw thread
static SDL_Thread *redraw_thread = NULL;			// Redraw thread

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

// SDL variables
static int screen_depth;							// Depth of current screen
static SDL_Color sdl_palette[256];					// Color palette to be used as CLUT and gamma table
static bool sdl_palette_changed = false;			// Flag: Palette changed, redraw thread must set new colors

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
 *  monitor_desc subclass for SDL display
 */

class SDL_monitor_desc : public monitor_desc {
public:
	SDL_monitor_desc(const vector<video_mode> &available_modes, video_depth default_depth, uint32 default_id) : monitor_desc(available_modes, default_depth, default_id) {}
	~SDL_monitor_desc() {}

	virtual void switch_to_current_mode(void);
	virtual void set_palette(uint8 *pal, int num);

	bool video_open(void);
	void video_close(void);
};


/*
 *  Utility functions
 */

// Map video_mode depth ID to numerical depth value
static int sdl_depth_of_video_depth(int video_depth)
{
	int depth = -1;
	switch (video_depth) {
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
static void set_mac_frame_buffer(SDL_monitor_desc &monitor, video_depth depth, bool native_byte_order)
{
#if !REAL_ADDRESSING && !DIRECT_ADDRESSING
	int layout = FLAYOUT_DIRECT;
	if (depth == VDEPTH_16BIT)
		layout = (screen_depth == 15) ? FLAYOUT_HOST_555 : FLAYOUT_HOST_565;
	else if (depth == VDEPTH_32BIT)
		layout = (screen_depth == 24) ? FLAYOUT_HOST_888 : FLAYOUT_DIRECT;
	if (native_byte_order)
		MacFrameLayout = layout;
	else
		MacFrameLayout = FLAYOUT_DIRECT;
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
	const video_mode &mode;    // Video mode handled by the driver

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
	const video_mode &mode = monitor.get_current_mode();

	if (mode.depth <= VDEPTH_8BIT)
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
	int width = mode.x, height = mode.y;
	int aligned_width = (width + 15) & ~15;
	int aligned_height = (height + 15) & ~15;

	// Set absolute mouse mode
	ADBSetRelMouseMode(mouse_grabbed);

	// Create surface
	int depth = (mode.depth <= VDEPTH_8BIT ? 8 : screen_depth);
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
#else
	// Allocate memory for frame buffer
	the_buffer_size = (aligned_height + 2) * s->pitch;
	the_buffer_copy = (uint8 *)calloc(1, the_buffer_size);
	the_buffer = (uint8 *)calloc(1, the_buffer_size);
	D(bug("the_buffer = %p, the_buffer_copy = %p\n", the_buffer, the_buffer_copy));
#endif

	// Set window name/class
	set_window_name(STR_WINDOW_TITLE);

	// Hide cursor
	SDL_ShowCursor(0);

	// Init blitting routines
	SDL_PixelFormat *f = s->format;
	VisualFormat visualFormat;
	visualFormat.depth = depth;
	visualFormat.Rmask = f->Rmask;
	visualFormat.Gmask = f->Gmask;
	visualFormat.Bmask = f->Bmask;
	Screen_blitter_init(visualFormat, true, sdl_depth_of_video_depth(mode.depth));

	// Load gray ramp to 8->16/32 expand map
	if (!IsDirectMode(mode))
		for (int i=0; i<256; i++)
			ExpandMap[i] = SDL_MapRGB(f, i, i, i);

	// Set frame buffer base
	set_mac_frame_buffer(monitor, mode.depth, true);

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
				static const char alias_str[] = "alias";
				if (strncmp(line, alias_str, sizeof(alias_str) - 1) == 0)
					continue;

				// Read keycode
				int x_code, mac_code;
				if (sscanf(line, "%d %d", &x_code, &mac_code) == 2)
					keycode_table[x_code & 0xff] = mac_code;
				else
					break;
			} else {
				// Search for SDL video driver string
				static const char alias_sdl_str[] = "alias SDL";
				if (strncmp(line, alias_sdl_str, sizeof(alias_sdl_str) - 1) == 0) {
					char *p = line + sizeof(alias_sdl_str);
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
	const video_mode &mode = get_current_mode();

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
	LOCK_FRAME_BUFFER;

	// Start redraw/input thread
	redraw_thread_cancel = false;
	redraw_thread_active = (SDL_CreateThread(redraw_func, NULL) != NULL);
	if (!redraw_thread_active) {
		printf("FATAL: cannot create redraw thread\n");
		return false;
	}
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
	const char *mode_str;
	if (classic_mode)
		mode_str = "win/512/342";
	else
		mode_str = PrefsFindString("screen");

	// Determine display type and default dimensions
	int default_width = 512, default_height = 384;
	display_type = DISPLAY_WINDOW;
	if (mode_str) {
		if (sscanf(mode_str, "win/%d/%d", &default_width, &default_height) == 2)
			display_type = DISPLAY_WINDOW;
	}
	int max_width = 640, max_height = 480;
	if (display_type == DISPLAY_SCREEN) {
		SDL_Rect **modes = SDL_ListModes(NULL, SDL_FULLSCREEN | SDL_HWSURFACE);
		if (modes && modes != (SDL_Rect **)-1) {
			max_width = modes[0]->w;
			max_height = modes[0]->h;
		}
	}
	if (default_width <= 0)
		default_width = max_width;
	if (default_height <= 0)
		default_height = max_height;

	// Mac screen depth follows X depth
	screen_depth = SDL_GetVideoInfo()->vfmt->BitsPerPixel;
	video_depth default_depth;
	switch (screen_depth) {
	case 8:
		default_depth = VDEPTH_8BIT;
		break;
	case 15: case 16:
		default_depth = VDEPTH_16BIT;
		break;
	case 24: case 32:
		default_depth = VDEPTH_32BIT;
		break;
	default:
		default_depth =  VDEPTH_1BIT;
		break;
	}

	// Construct list of supported modes
	if (display_type == DISPLAY_WINDOW) {
		if (classic)
			add_mode(512, 342, 0x80, 64, VDEPTH_1BIT);
		else {
			for (int d = VDEPTH_1BIT; d <= default_depth; d++) {
				int bpp = (d <= VDEPTH_8BIT ? 8 : sdl_depth_of_video_depth(d));
				if (SDL_VideoModeOK(max_width, max_height, bpp, SDL_HWSURFACE))
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

	// Create SDL_monitor_desc for this (the only) display
	SDL_monitor_desc *monitor = new SDL_monitor_desc(VideoModes, default_depth, default_id);
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
//		SDL_WaitThread(redraw_thread, NULL); doesn't work
		while (redraw_thread_cancel);
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

void SDL_monitor_desc::set_palette(uint8 *pal, int num_in)
{
	const video_mode &mode = get_current_mode();

	// FIXME: how can we handle the gamma ramp?
	if (mode.depth > VDEPTH_8BIT)
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
		// We have to redraw everything because the interpretation of pixel values changed
		LOCK_VOSF;
		PFLAG_SET_ALL;
		UNLOCK_VOSF;
		memset(the_buffer_copy, 0, mode.bytes_per_row * mode.y);
#endif
	}

	// Tell redraw thread to change palette
	sdl_palette_changed = true;

	UNLOCK_PALETTE;
}


/*
 *  Switch video mode
 */

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
 *  Translate key event to Mac keycode, returns -1 if no keycode was found
 *  and -2 if the key was recognized as a hotkey
 */

static bool is_ctrl_down(SDL_keysym const & ks)
{
	return ctrl_down || (ks.mod & KMOD_CTRL);
}

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
	case SDLK_LALT: return 0x37;
	case SDLK_RALT: return 0x37;
	case SDLK_LMETA: return 0x3a;
	case SDLK_RMETA: return 0x3a;
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
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
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
				if (use_keycodes) {
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
				if (use_keycodes) {
					if (event2keycode(event.key, false) != -2)	// This is called to process the hotkeys
						code = keycode_table[event.key.keysym.scancode & 0xff];
				} else
					code = event2keycode(event.key, false);
				if (code >= 0 && code != 0x39) {	// Don't propagate Caps Lock releases
					ADBKeyUp(code);
					if (code == 0x36)
						ctrl_down = false;
				}
				break;
			}

			// Hidden parts exposed, force complete refresh of window
			case SDL_VIDEOEXPOSE:
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
						memset(the_buffer_copy, 0, mode.bytes_per_row * mode.y);
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


/*
 *  Window display update
 */

// Static display update (fixed frame rate, but incremental)
static void update_display_static(driver_window *drv)
{
	// Incremental update code
	int wide = 0, high = 0, x1, x2, y1, y2, i, j;
	const video_mode &mode = drv->mode;
	int bytes_per_row = mode.bytes_per_row;
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
		if (mode.depth < VDEPTH_8BIT) {
			const int src_bytes_per_row = bytes_per_row;
			const int dst_bytes_per_row = drv->s->pitch;
			const int pixels_per_byte = mode.x / src_bytes_per_row;

			x1 = mode.x / pixels_per_byte;
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
				for (i = (mode.x / pixels_per_byte); i > x2; i--) {
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
			const int bytes_per_pixel = mode.bytes_per_row / mode.x;

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

static int redraw_func(void *arg)
{
	uint64 start = GetTicks_usec();
	int64 ticks = 0;

	while (!redraw_thread_cancel) {

		// Wait
		Delay_usec(16667);

		// Handle SDL events
		handle_events();

		// Refresh display
		video_refresh();
		ticks++;

		// Set new palette if it was changed
		handle_palette_changes();
	}

	uint64 end = GetTicks_usec();
	D(bug("%lld refreshes in %lld usec = %f refreshes/sec\n", ticks, end - start, ticks * 1000000.0 / (end - start)));
	redraw_thread_cancel = false;
	return 0;
}
