/*
 *  video_beos.cpp - Video/graphics emulation, BeOS specific stuff
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
 *  Portions written by Marc Hellwig
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

#include <AppKit.h>
#include <InterfaceKit.h>
#include <GameKit.h>

#include <stdio.h>
#include <string.h>

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "main.h"
#include "macos_util.h"
#include "prefs.h"
#include "adb.h"
#include "prefs.h"
#include "user_strings.h"
#include "about_window.h"
#include "video.h"

#include "m68k.h"
#include "memory.h"
#include "readcpu.h"
#include "newcpu.h"

#define DEBUG 0
#include "debug.h"

#define DEBUGGER_AVAILABLE 0


// Messages
const uint32 MSG_REDRAW = 'draw';
const uint32 MSG_ABOUT_REQUESTED = B_ABOUT_REQUESTED;
const uint32 MSG_REF_5HZ = ' 5Hz';
const uint32 MSG_REF_7_5HZ = ' 7Hz';
const uint32 MSG_REF_10HZ = '10Hz';
const uint32 MSG_REF_15HZ = '15Hz';
const uint32 MSG_REF_30HZ = '30Hz';
const uint32 MSG_REF_60HZ = '60Hz';
const uint32 MSG_MOUNT = 'moun';
const uint32 MSG_DEBUGGER = 'dbug';

// Display types
enum {
	DISPLAY_WINDOW,
	DISPLAY_SCREEN
};

// From sys_beos.cpp
extern void SysCreateVolumeMenu(BMenu *menu, uint32 msg);
extern void SysMountVolume(const char *name);


/*
 *  A simple view class for blitting a bitmap on the screen
 */

class BitmapView : public BView {
public:
	BitmapView(BRect frame, BBitmap *bitmap) : BView(frame, "bitmap", B_FOLLOW_NONE, B_WILL_DRAW)
	{
		the_bitmap = bitmap;
	}
	virtual void Draw(BRect update)
	{
		DrawBitmap(the_bitmap, update, update);
	}
	virtual void MouseMoved(BPoint point, uint32 transit, const BMessage *message);

private:
	BBitmap *the_bitmap;
};


/*
 *  Window class
 */

class MacWindow : public BDirectWindow {
public:
	MacWindow(BRect frame);
	virtual ~MacWindow();
	virtual void MessageReceived(BMessage *msg);
	virtual void DirectConnected(direct_buffer_info *info);
	virtual void WindowActivated(bool active);

	int32 frame_skip;
	bool mouse_in_view;			// Flag: Mouse pointer within bitmap view
	uint8 remap_mac_be[256];	// For remapping of Mac colors to Be colors

private:
	static status_t tick_func(void *arg);

	thread_id tick_thread;
	bool tick_thread_active;	// Flag for quitting the tick thread

	BitmapView *main_view;		// Main view for bitmap drawing
	BBitmap *the_bitmap;		// Mac screen bitmap
	uint8 *the_buffer;			// Mac frame buffer

	uint32 old_scroll_lock_state;

	bool supports_direct_mode;	// Flag: Direct frame buffer access supported
	sem_id drawing_sem;

	void *bits;
	int32 bytes_per_row;
	color_space pixel_format;
	bool unclipped;
};


/*
 *  Screen class
 */

class MacScreen : public BWindowScreen {
public:
	MacScreen(const char *name, int mode_bit, status_t *error);
	virtual ~MacScreen();
	virtual void Quit(void);
	virtual	void ScreenConnected(bool active);

	rgb_color palette[256];		// Color palette, 256 entries
	bool palette_changed;

private:
	static status_t tick_func(void *arg);

	thread_id tick_thread;
	bool tick_thread_active;	// Flag for quitting the tick thread

	BView *main_view;			// Main view for GetMouse()
	uint8 *frame_backup;		// Frame buffer backup when switching from/to different workspace
	bool quitting;				// Flag for ScreenConnected: We are quitting, don't pause emulator thread
	bool screen_active;
	bool first_time;
};


// Global variables
static int display_type = DISPLAY_WINDOW;	// See enum above
static MacWindow *the_window = NULL;		// Pointer to the window
static MacScreen *the_screen = NULL;		// Pointer to the screen
static sem_id mac_os_lock = -1;				// This is used to stop the MacOS thread when the Basilisk workspace is switched out
static uint8 MacCursor[68] = {16, 1};		// Mac cursor image


/*
 *  Initialization
 */

// Add resolution to list of supported modes and set VideoMonitor
static void set_video_monitor(uint32 width, uint32 height, uint32 bytes_per_row, int depth)
{
	video_mode mode;

	mode.x = width;
	mode.y = height;
	mode.resolution_id = 0x80;
	mode.bytes_per_row = bytes_per_row;

	switch (depth) {
		case 1:
			mode.depth = VDEPTH_1BIT;
			break;
		case 2:
			mode.depth = VDEPTH_2BIT;
			break;
		case 4:
			mode.depth = VDEPTH_4BIT;
			break;
		case 8:
			mode.depth = VDEPTH_8BIT;
			break;
		case 15:
			mode.depth = VDEPTH_16BIT;
			break;
		case 16:
			mode.depth = VDEPTH_16BIT;
			break;
		case 24:
		case 32:
			mode.depth = VDEPTH_32BIT;
			break;
	}

	VideoModes.push_back(mode);
	video_init_depth_list();
	VideoMonitor.mode = mode;
}


bool VideoInit(bool classic)
{
	// Create semaphore
	mac_os_lock = create_sem(0, "MacOS Frame Buffer Lock");

	// Get screen mode from preferences
	const char *mode_str = PrefsFindString("screen");

	// Determine type and mode
	display_type = DISPLAY_WINDOW;
	int width = 512, height = 384;
	int scr_mode_bit = 0;
	if (mode_str) {
		if (sscanf(mode_str, "win/%d/%d", &width, &height) == 2)
			display_type = DISPLAY_WINDOW;
		else if (sscanf(mode_str, "scr/%d", &scr_mode_bit) == 1)
			display_type = DISPLAY_SCREEN;
	}

	// Open display
	switch (display_type) {
		case DISPLAY_WINDOW:
			the_window = new MacWindow(BRect(0, 0, width-1, height-1));
			break;
		case DISPLAY_SCREEN: {
			status_t screen_error;
			the_screen = new MacScreen(GetString(STR_WINDOW_TITLE), scr_mode_bit & 0x1f, &screen_error);
			if (screen_error != B_NO_ERROR) {
				the_screen->PostMessage(B_QUIT_REQUESTED);
				while (the_screen)
					snooze(200000);
				ErrorAlert(STR_OPEN_SCREEN_ERR);
				return false;
			} else {
				the_screen->Show();
				acquire_sem(mac_os_lock);
			}
			break;
		}
	}
	return true;
}


/*
 *  Deinitialization
 */

void VideoExit(void)
{
	// Close display
	switch (display_type) {
		case DISPLAY_WINDOW:
			if (the_window != NULL) {
				the_window->PostMessage(B_QUIT_REQUESTED);
				while (the_window)
					snooze(200000);
			}
			break;
		case DISPLAY_SCREEN:
			if (the_screen != NULL) {
				the_screen->PostMessage(B_QUIT_REQUESTED);
				while (the_screen)
					snooze(200000);
			}
			break;
	}

	// Delete semaphore
	delete_sem(mac_os_lock);
}


/*
 *  Set palette
 */

void video_set_palette(uint8 *pal, int num)
{
	switch (display_type) {
		case DISPLAY_WINDOW: {
			BScreen screen(the_window);
			for (int i=0; i<256; i++)
				the_window->remap_mac_be[i] = screen.IndexForColor(pal[i*3], pal[i*3+1], pal[i*3+2]);
			break;
		}
		case DISPLAY_SCREEN:
			for (int i=0; i<256; i++) {
				the_screen->palette[i].red = pal[i*3];
				the_screen->palette[i].green = pal[i*3+1];
				the_screen->palette[i].blue = pal[i*3+2];
			}
			the_screen->palette_changed = true;
			break;
	}
}


/*
 *  Switch video mode
 */

void video_switch_to_mode(const video_mode &mode)
{
}


/*
 *  Close down full-screen mode (if bringing up error alerts is unsafe while in full-screen mode)
 */

void VideoQuitFullScreen(void)
{
	D(bug("VideoQuitFullScreen()\n"));
	if (display_type == DISPLAY_SCREEN) {
		if (the_screen != NULL) {
			the_screen->PostMessage(B_QUIT_REQUESTED);
			while (the_screen)
				snooze(200000);
		}
	}
}


/*
 *  Video event handling (not neccessary under BeOS, handled by filter function)
 */

void VideoInterrupt(void)
{
	release_sem(mac_os_lock);
	while (acquire_sem(mac_os_lock) == B_INTERRUPTED) ;
}


/*
 *  Filter function for receiving mouse and keyboard events
 */

#define MENU_IS_POWER 0

// Be -> Mac raw keycode translation table
static const uint8 keycode2mac[0x80] = {
	0xff, 0x35, 0x7a, 0x78, 0x63, 0x76, 0x60, 0x61,	// inv Esc  F1  F2  F3  F4  F5  F6
	0x62, 0x64, 0x65, 0x6d, 0x67, 0x6f, 0x69, 0x6b,	//  F7  F8  F9 F10 F11 F12 F13 F14
	0x71, 0x0a, 0x12, 0x13, 0x14, 0x15, 0x17, 0x16,	// F15   `   1   2   3   4   5   6
	0x1a, 0x1c, 0x19, 0x1d, 0x1b, 0x18, 0x33, 0x72,	//   7   8   9   0   -   = BSP INS
	0x73, 0x74, 0x47, 0x4b, 0x43, 0x4e, 0x30, 0x0c,	// HOM PUP NUM   /   *   - TAB   Q
	0x0d, 0x0e, 0x0f, 0x11, 0x10, 0x20, 0x22, 0x1f,	//   W   E   R   T   Y   U   I   O
	0x23, 0x21, 0x1e, 0x2a, 0x75, 0x77, 0x79, 0x59,	//   P   [   ]   \ DEL END PDN   7
	0x5b, 0x5c, 0x45, 0x39, 0x00, 0x01, 0x02, 0x03,	//   8   9   + CAP   A   S   D   F
	0x05, 0x04, 0x26, 0x28, 0x25, 0x29, 0x27, 0x24,	//   G   H   J   K   L   ;   ' RET
	0x56, 0x57, 0x58, 0x38, 0x06, 0x07, 0x08, 0x09,	//   4   5   6 SHL   Z   X   C   V
	0x0b, 0x2d, 0x2e, 0x2b, 0x2f, 0x2c, 0x38, 0x3e,	//   B   N   M   ,   .   / SHR CUP
	0x53, 0x54, 0x55, 0x4c, 0x36, 0x37, 0x31, 0x37, //   1   2   3 ENT CTL ALT SPC ALT
	0x36, 0x3b, 0x3d, 0x3c, 0x52, 0x41, 0x3a, 0x3a,	// CTR CLF CDN CRT   0   . CMD CMD
#if MENU_IS_POWER
	0x7f, 0x32, 0x51, 0x7f, 0xff, 0xff, 0xff, 0xff,	// MNU EUR   = POW inv inv inv inv
#else
	0x32, 0x32, 0x51, 0x7f, 0xff, 0xff, 0xff, 0xff,	// MNU EUR   = POW inv inv inv inv
#endif
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	// inv inv inv inv inv inv inv inv
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff	// inv inv inv inv inv inv inv inv
};

static const uint8 modifier2mac[0x20] = {
#if MENU_IS_POWER
	0x38, 0x37, 0x36, 0x39, 0x6b, 0x47, 0x3a, 0x7f,	// SHF CMD inv CAP F14 NUM OPT MNU
#else
	0x38, 0x37, 0x36, 0x39, 0x6b, 0x47, 0x3a, 0x32,	// SHF CMD CTR CAP F14 NUM OPT MNU
#endif
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	// inv inv inv inv inv inv inv inv
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	// inv inv inv inv inv inv inv inv
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff	// inv inv inv inv inv inv inv inv
};

static filter_result filter_func(BMessage *msg, BHandler **target, BMessageFilter *filter)
{
	switch (msg->what) {
		case B_KEY_DOWN:
		case B_KEY_UP: {
			uint32 be_code = msg->FindInt32("key") & 0xff;
			uint32 mac_code = keycode2mac[be_code];

			// Intercept Ctrl-F1 (mount floppy disk shortcut)
			uint32 mods = msg->FindInt32("modifiers");
			if (be_code == 0x02 && (mods & B_CONTROL_KEY))
				SysMountVolume("/dev/disk/floppy/raw");

			if (mac_code == 0xff)
				return B_DISPATCH_MESSAGE;
			if (msg->what == B_KEY_DOWN)
				ADBKeyDown(mac_code);
			else
				ADBKeyUp(mac_code);
			return B_SKIP_MESSAGE;
		}

		case B_MODIFIERS_CHANGED: {
			uint32 mods = msg->FindInt32("modifiers");
			uint32 old_mods = msg->FindInt32("be:old_modifiers");
			uint32 changed = mods ^ old_mods;
			uint32 mask = 1;
			for (int i=0; i<32; i++, mask<<=1)
				if (changed & mask) {
					uint32 mac_code = modifier2mac[i];
					if (mac_code == 0xff)
						continue;
					if (mods & mask)
						ADBKeyDown(mac_code);
					else
						ADBKeyUp(mac_code);
				}
			return B_SKIP_MESSAGE;
		}

		case B_MOUSE_MOVED: {
			BPoint point;
			msg->FindPoint("where", &point);
			ADBMouseMoved(int(point.x), int(point.y));
			return B_DISPATCH_MESSAGE;	// Otherwise BitmapView::MouseMoved() wouldn't be called
		}

		case B_MOUSE_DOWN: {
			uint32 buttons = msg->FindInt32("buttons");
			if (buttons & B_PRIMARY_MOUSE_BUTTON)
				ADBMouseDown(0);
			if (buttons & B_SECONDARY_MOUSE_BUTTON)
				ADBMouseDown(1);
			if (buttons & B_TERTIARY_MOUSE_BUTTON)
				ADBMouseDown(2);
			return B_SKIP_MESSAGE;
		}

		case B_MOUSE_UP:	// B_MOUSE_UP means "all buttons released"
			ADBMouseUp(0);
			ADBMouseUp(1);
			ADBMouseUp(2);
			return B_SKIP_MESSAGE;

		default:
			return B_DISPATCH_MESSAGE;
	}
}


/*
 *  Window constructor
 */

MacWindow::MacWindow(BRect frame) : BDirectWindow(frame, GetString(STR_WINDOW_TITLE), B_TITLED_WINDOW, B_NOT_RESIZABLE | B_NOT_CLOSABLE | B_NOT_ZOOMABLE)
{
	supports_direct_mode = SupportsWindowMode();

	// Move window to right position
	Lock();
	MoveTo(80, 60);

	// Allocate bitmap and Mac frame buffer
	uint32 x = frame.IntegerWidth() + 1;
	uint32 y = frame.IntegerHeight() + 1;
	the_bitmap = new BBitmap(frame, B_COLOR_8_BIT);
	the_buffer = new uint8[x * (y + 2)];	// "y + 2" for safety

	// Add resolution and set VideoMonitor
	set_video_monitor(x, y, x, 8);
#if REAL_ADDRESSING
	VideoMonitor.mac_frame_base = (uint32)the_buffer;
#else
	VideoMonitor.mac_frame_base = MacFrameBaseMac;
#endif

#if !REAL_ADDRESSING
	// Set variables for UAE memory mapping
	MacFrameBaseHost = the_buffer;
	MacFrameSize = x * y;
	MacFrameLayout = FLAYOUT_DIRECT;
#endif

	// Create bitmap view
	main_view = new BitmapView(frame, the_bitmap);
	AddChild(main_view);
	main_view->MakeFocus();

	// Read frame skip prefs
	frame_skip = PrefsFindInt32("frameskip");
	if (frame_skip == 0)
		frame_skip = 1;

	// Set up menus
	BRect bounds = Bounds();
	bounds.OffsetBy(0, bounds.IntegerHeight() + 1);
	BMenuItem *item;
	BMenuBar *bar = new BMenuBar(bounds, "menu");
	BMenu *menu = new BMenu(GetString(STR_WINDOW_MENU));
	menu->AddItem(new BMenuItem(GetString(STR_WINDOW_ITEM_ABOUT), new BMessage(MSG_ABOUT_REQUESTED)));
	menu->AddItem(new BSeparatorItem);
	BMenu *submenu = new BMenu(GetString(STR_WINDOW_ITEM_REFRESH));
	submenu->AddItem(new BMenuItem(GetString(STR_REF_5HZ_LAB), new BMessage(MSG_REF_5HZ)));
	submenu->AddItem(new BMenuItem(GetString(STR_REF_7_5HZ_LAB), new BMessage(MSG_REF_7_5HZ)));
	submenu->AddItem(new BMenuItem(GetString(STR_REF_10HZ_LAB), new BMessage(MSG_REF_10HZ)));
	submenu->AddItem(new BMenuItem(GetString(STR_REF_15HZ_LAB), new BMessage(MSG_REF_15HZ)));
	submenu->AddItem(new BMenuItem(GetString(STR_REF_30HZ_LAB), new BMessage(MSG_REF_30HZ)));
	submenu->AddItem(new BMenuItem(GetString(STR_REF_60HZ_LAB), new BMessage(MSG_REF_60HZ)));
	submenu->SetRadioMode(true);
	if (frame_skip == 12) {
		if ((item = submenu->FindItem(GetString(STR_REF_5HZ_LAB))) != NULL)
			item->SetMarked(true);
	} else if (frame_skip == 8) {
		if ((item = submenu->FindItem(GetString(STR_REF_7_5HZ_LAB))) != NULL)
			item->SetMarked(true);
	} else if (frame_skip == 6) {
		if ((item = submenu->FindItem(GetString(STR_REF_10HZ_LAB))) != NULL)
			item->SetMarked(true);
	} else if (frame_skip == 4) {
		if ((item = submenu->FindItem(GetString(STR_REF_15HZ_LAB))) != NULL)
			item->SetMarked(true);
	} else if (frame_skip == 2) {
		if ((item = submenu->FindItem(GetString(STR_REF_30HZ_LAB))) != NULL)
			item->SetMarked(true);
	} else if (frame_skip == 1) {
		if ((item = submenu->FindItem(GetString(STR_REF_60HZ_LAB))) != NULL)
			item->SetMarked(true);
	}
	menu->AddItem(submenu);
	submenu = new BMenu(GetString(STR_WINDOW_ITEM_MOUNT));
	SysCreateVolumeMenu(submenu, MSG_MOUNT);
	menu->AddItem(submenu);
#if DEBUGGER_AVAILABLE
	menu->AddItem(new BMenuItem("Debugger", new BMessage(MSG_DEBUGGER)));
#endif
	bar->AddItem(menu);
	AddChild(bar);
	SetKeyMenuBar(bar);
	int mbar_height = bar->Frame().IntegerHeight() + 1;

	// Resize window to fit menu bar
	ResizeBy(0, mbar_height);

	// Set absolute mouse mode and get scroll lock state
	ADBSetRelMouseMode(false);
	mouse_in_view = true;
	old_scroll_lock_state = modifiers() & B_SCROLL_LOCK;
	if (old_scroll_lock_state)
		SetTitle(GetString(STR_WINDOW_TITLE_FROZEN));
	else
		SetTitle(GetString(STR_WINDOW_TITLE));

	// Keep window aligned to 8-byte frame buffer boundaries for faster blitting
	SetWindowAlignment(B_BYTE_ALIGNMENT, 8);

	// Create drawing semaphore (for direct mode)
	drawing_sem = create_sem(0, "direct frame buffer access");

	// Start 60Hz interrupt
	tick_thread_active = true;
	tick_thread = spawn_thread(tick_func, "Window Redraw", B_DISPLAY_PRIORITY, this);
	resume_thread(tick_thread);

	// Add filter for keyboard and mouse events
	BMessageFilter *filter = new BMessageFilter(B_ANY_DELIVERY, B_ANY_SOURCE, filter_func);
	main_view->AddFilter(filter);

	// Show window
	Unlock();
	Show();
	Sync();
}


/*
 *  Window destructor
 */

MacWindow::~MacWindow()
{
	// Restore cursor
	mouse_in_view = false;
	be_app->SetCursor(B_HAND_CURSOR);

	// Hide window
	Hide();
	Sync();

	// Stop 60Hz interrupt
	status_t l;
	tick_thread_active = false;
	delete_sem(drawing_sem);
	wait_for_thread(tick_thread, &l);

	// Free bitmap and frame buffer
	delete the_bitmap;
	delete[] the_buffer;

	// Tell emulator that we're done
	the_window = NULL;
}


/*
 *  Window connected/disconnected
 */

void MacWindow::DirectConnected(direct_buffer_info *info)
{
	switch (info->buffer_state & B_DIRECT_MODE_MASK) {
		case B_DIRECT_STOP:
			acquire_sem(drawing_sem);
			break;
		case B_DIRECT_MODIFY:
			acquire_sem(drawing_sem);
		case B_DIRECT_START:
			bits = (void *)((uint8 *)info->bits + info->window_bounds.top * info->bytes_per_row + info->window_bounds.left * info->bits_per_pixel / 8);
			bytes_per_row = info->bytes_per_row;
			pixel_format = info->pixel_format;
			unclipped = false;
			if (info->clip_list_count == 1)
				if (memcmp(&info->clip_bounds, &info->window_bounds, sizeof(clipping_rect)) == 0)
					unclipped = true;
			release_sem(drawing_sem);
			break;
	}
}


/*
 *  Handle redraw and menu messages
 */

void MacWindow::MessageReceived(BMessage *msg)
{
	BMessage *msg2;

	switch (msg->what) {
		case MSG_REDRAW: {

			// Prevent backlog of messages
			MessageQueue()->Lock();
			while ((msg2 = MessageQueue()->FindMessage(MSG_REDRAW, 0)) != NULL) {
				MessageQueue()->RemoveMessage(msg2);
				delete msg2;
			}
			MessageQueue()->Unlock();
			
			// Convert Mac screen buffer to BeOS palette and blit
			uint8 *source = the_buffer - 1;
			uint8 *dest = (uint8 *)the_bitmap->Bits() - 1;
			uint32 length = VideoMonitor.mode.bytes_per_row * VideoMonitor.mode.y;
			for (int i=0; i<length; i++)
				*++dest = remap_mac_be[*++source];
			BRect update_rect = BRect(0, 0, VideoMonitor.mode.x-1, VideoMonitor.mode.y-1);
			main_view->DrawBitmapAsync(the_bitmap, update_rect, update_rect);
			break;
		}

		case MSG_ABOUT_REQUESTED: {
			ShowAboutWindow();
			break;
		}

		case MSG_REF_5HZ:
			PrefsReplaceInt32("frameskip", frame_skip = 12);
			break;

		case MSG_REF_7_5HZ:
			PrefsReplaceInt32("frameskip", frame_skip = 8);
			break;

		case MSG_REF_10HZ:
			PrefsReplaceInt32("frameskip", frame_skip = 6);
			break;

		case MSG_REF_15HZ:
			PrefsReplaceInt32("frameskip", frame_skip = 4);
			break;

		case MSG_REF_30HZ:
			PrefsReplaceInt32("frameskip", frame_skip = 2);
			break;

		case MSG_REF_60HZ:
			PrefsReplaceInt32("frameskip", frame_skip = 1);
			break;

		case MSG_MOUNT: {
			BMenuItem *source = NULL;
			msg->FindPointer("source", (void **)&source);
			if (source)
				SysMountVolume(source->Label());
			break;
		}

#if DEBUGGER_AVAILABLE
		case MSG_DEBUGGER:
			extern int debugging;
			debugging = 1;
			regs.spcflags |= SPCFLAG_BRK;
			break;
#endif

		default:
			BDirectWindow::MessageReceived(msg);
	}
}


/*
 *  Window activated/deactivated
 */

void MacWindow::WindowActivated(bool active)
{
	if (active) {
		frame_skip = PrefsFindInt32("frameskip");
		if (frame_skip == 0)
			frame_skip = 1;
	} else
		frame_skip = 12;	// 5Hz in background
}


/*
 *  60Hz interrupt routine
 */

status_t MacWindow::tick_func(void *arg)
{
	MacWindow *obj = (MacWindow *)arg;
	static int tick_counter = 0;
	while (obj->tick_thread_active) {

		tick_counter++;
		if (tick_counter >= obj->frame_skip) {
			tick_counter = 0;

			// Window title is determined by Scroll Lock state
			uint32 scroll_lock_state = modifiers() & B_SCROLL_LOCK;
			if (scroll_lock_state != obj->old_scroll_lock_state) {
				if (scroll_lock_state)
					obj->SetTitle(GetString(STR_WINDOW_TITLE_FROZEN));
				else
					obj->SetTitle(GetString(STR_WINDOW_TITLE));
				obj->old_scroll_lock_state = scroll_lock_state;
			}

			// Has the Mac started?
			if (HasMacStarted()) {

				// Yes, set new cursor image if it was changed
				if (memcmp(MacCursor+4, Mac2HostAddr(0x844), 64)) {
					Mac2Host_memcpy(MacCursor+4, 0x844, 64);	// Cursor image
					MacCursor[2] = ReadMacInt8(0x885);			// Hotspot
					MacCursor[3] = ReadMacInt8(0x887);
					be_app->SetCursor(MacCursor);
				}
			}

			// Refresh screen unless Scroll Lock is down
			if (!scroll_lock_state) {

				// If direct frame buffer access is supported and the content area is completely visible,
				// convert the Mac screen buffer directly. Otherwise, send a message to the window to do
				// it into a bitmap
				if (obj->supports_direct_mode) {
					if (acquire_sem(obj->drawing_sem) != B_NO_ERROR)
						return 0;
					if (obj->unclipped && obj->pixel_format == B_CMAP8) {
						uint8 *source = obj->the_buffer - 1;
						uint8 *dest = (uint8 *)obj->bits;
						uint32 bytes_per_row = obj->bytes_per_row;
						int xsize = VideoMonitor.mode.x;
						int ysize = VideoMonitor.mode.y;
						for (int y=0; y<ysize; y++) {
							uint32 *p = (uint32 *)dest - 1;
							for (int x=0; x<xsize/4; x++) {
#if B_HOST_IS_BENDIAN
								uint32 c = obj->remap_mac_be[*++source] << 24;
								c |= obj->remap_mac_be[*++source] << 16;
								c |= obj->remap_mac_be[*++source] << 8;
								c |= obj->remap_mac_be[*++source];
#else
								uint32 c = obj->remap_mac_be[*++source];
								c |= obj->remap_mac_be[*++source] << 8;
								c |= obj->remap_mac_be[*++source] << 16;
								c |= obj->remap_mac_be[*++source] << 24;
#endif
								*++p = c;
							}
							dest += bytes_per_row;
						}
					} else
						obj->PostMessage(MSG_REDRAW);
					release_sem(obj->drawing_sem);
				} else
					obj->PostMessage(MSG_REDRAW);
			}
		}
		snooze(16666);
	}
	return 0;
}


/*
 *  Mouse moved in window
 */

void BitmapView::MouseMoved(BPoint point, uint32 transit, const BMessage *message)
{
	switch (transit) {
		case B_ENTERED_VIEW:
			((MacWindow *)Window())->mouse_in_view = true;
			be_app->SetCursor(MacCursor);
			break;
		case B_EXITED_VIEW:
			((MacWindow *)Window())->mouse_in_view = false;
			be_app->SetCursor(B_HAND_CURSOR);
			break;
	}
}


/*
 *  Screen constructor
 */

MacScreen::MacScreen(const char *name, int mode_bit, status_t *error) : BWindowScreen(name, 1 << mode_bit, error), tick_thread(-1)
{
	// Set all variables
	frame_backup = NULL;
	palette_changed = false;
	screen_active = false;
	first_time = true;
	quitting = false;

	// Set relative mouse mode
	ADBSetRelMouseMode(true);

	// Create view to get mouse events
	main_view = new BView(Frame(), NULL, B_FOLLOW_NONE, 0);
	AddChild(main_view);

	// Start 60Hz interrupt
	tick_thread_active = true;
	tick_thread = spawn_thread(tick_func, "Polling sucks...", B_DISPLAY_PRIORITY, this);
	resume_thread(tick_thread);

	// Add filter for keyboard and mouse events
	BMessageFilter *filter = new BMessageFilter(B_ANY_DELIVERY, B_ANY_SOURCE, filter_func);
	AddCommonFilter(filter);
}


/*
 *  Screen destructor
 */

MacScreen::~MacScreen()
{
	// Stop 60Hz interrupt
	if (tick_thread > 0) {
		status_t l;
		tick_thread_active = false;
		wait_for_thread(tick_thread, &l);
	}

	// Tell emulator that we're done
	the_screen = NULL;
}


/*
 *  Screen closed
 */

void MacScreen::Quit(void)
{
	// Tell ScreenConnected() that we are quitting
	quitting = true;
	BWindowScreen::Quit();
}


/*
 *  Screen connected/disconnected
 */

void MacScreen::ScreenConnected(bool active)
{
	graphics_card_info *info = CardInfo();
	screen_active = active;

	if (active == true) {

		// Add resolution and set VideoMonitor
		if (first_time) {
			set_video_monitor(info->width, info->height, info->bytes_per_row, info->bits_per_pixel);
			first_time = false;
		}

		// Set VideoMonitor
#if REAL_ADDRESSING
		VideoMonitor.mac_frame_base = (uint32)info->frame_buffer;
#else
		VideoMonitor.mac_frame_base = MacFrameBaseMac;
#endif

#if !REAL_ADDRESSING
		// Set variables for UAE memory mapping
		MacFrameBaseHost = (uint8 *)info->frame_buffer;
		MacFrameSize = VideoMonitor.mode.bytes_per_row * VideoMonitor.mode.y;
		switch (info->bits_per_pixel) {
			case 15:
				MacFrameLayout = FLAYOUT_HOST_555;
				break;
			case 16:
				MacFrameLayout = FLAYOUT_HOST_565;
				break;
			case 32:
				MacFrameLayout = FLAYOUT_HOST_888;
				break;
			default:
				MacFrameLayout = FLAYOUT_DIRECT;
				break;
		}
#endif

		// Copy from backup store to frame buffer
		if (frame_backup != NULL) {
			memcpy(info->frame_buffer, frame_backup, VideoMonitor.mode.bytes_per_row * VideoMonitor.mode.y);
			delete[] frame_backup;			
			frame_backup = NULL;
		}

		// Restore palette
		if (VideoMonitor.mode.depth == VDEPTH_8BIT)
			SetColorList(palette);

		// Restart/signal emulator thread
		release_sem(mac_os_lock);

	} else {

		if (!quitting) {

			// Stop emulator thread
			acquire_sem(mac_os_lock);

			// Create backup store and save frame buffer
			frame_backup = new uint8[VideoMonitor.mode.bytes_per_row * VideoMonitor.mode.y];
			memcpy(frame_backup, info->frame_buffer, VideoMonitor.mode.bytes_per_row * VideoMonitor.mode.y);
		}
	}
}


/*
 *  Screen 60Hz interrupt routine
 */

status_t MacScreen::tick_func(void *arg)
{
	MacScreen *obj = (MacScreen *)arg;
	while (obj->tick_thread_active) {

		// Wait
		snooze(16667);

		// Workspace activated? Then poll the mouse and set the palette if needed
		if (!obj->quitting && obj->LockWithTimeout(200000) == B_OK) {
			if (obj->screen_active) {
				BPoint pt;
				uint32 button = 0;
				if (obj->palette_changed) {
					obj->palette_changed = false;
					obj->SetColorList(obj->palette);
				}
				obj->main_view->GetMouse(&pt, &button);
				set_mouse_position(320, 240);
				ADBMouseMoved(int(pt.x) - 320, int(pt.y) - 240);
				if (button & B_PRIMARY_MOUSE_BUTTON)
					ADBMouseDown(0);
				if (!(button & B_PRIMARY_MOUSE_BUTTON))
					ADBMouseUp(0);
				if (button & B_SECONDARY_MOUSE_BUTTON)
					ADBMouseDown(1);
				if (!(button & B_SECONDARY_MOUSE_BUTTON))
					ADBMouseUp(1);
				if (button & B_TERTIARY_MOUSE_BUTTON)
					ADBMouseDown(2);
				if (!(button & B_TERTIARY_MOUSE_BUTTON))
					ADBMouseUp(2);
			}
			obj->Unlock();
		}
	}
	return 0;
}
