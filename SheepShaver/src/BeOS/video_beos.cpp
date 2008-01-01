/*
 *  video_beos.cpp - Video/graphics emulation, BeOS specific things
 *
 *  SheepShaver (C) 1997-2008 Marc Hellwig and Christian Bauer
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

#include "video.h"
#include "video_defs.h"
#include "main.h"
#include "adb.h"
#include "prefs.h"
#include "user_strings.h"
#include "about_window.h"
#include "version.h"

#define DEBUG 0
#include "debug.h"


// Global variables
static sem_id video_lock = -1;		// Protection during mode changes
static sem_id mac_os_lock = -1;		// This is used to stop the MacOS thread when the SheepShaver workspace is switched out

// Prototypes
static filter_result filter_func(BMessage *msg, BHandler **target, BMessageFilter *filter);

// From sys_beos.cpp
extern void SysCreateVolumeMenu(BMenu *menu, uint32 msg);
extern void SysMountVolume(const char *name);


#include "video_window.h"
#include "video_screen.h"


/*
 *  Display manager thread (for opening and closing windows and screens;
 *  this is not safe under R4 when running on the MacOS stack in kernel
 *  space)
 */

// Message constants
const uint32 MSG_OPEN_WINDOW = 'owin';
const uint32 MSG_CLOSE_WINDOW = 'cwin';
const uint32 MSG_OPEN_SCREEN = 'oscr';
const uint32 MSG_CLOSE_SCREEN = 'cscr';
const uint32 MSG_QUIT_DISPLAY_MANAGER = 'quit';

static thread_id dm_thread = -1;
static sem_id dm_done_sem = -1;

static status_t display_manager(void *arg)
{
	for (;;) {

		// Receive message
		thread_id sender;
		uint32 code = receive_data(&sender, NULL, 0);
		D(bug("Display manager received %08lx\n", code));
		switch (code) {
			case MSG_QUIT_DISPLAY_MANAGER:
				return 0;

			case MSG_OPEN_WINDOW:
				D(bug("Opening window\n"));
				the_window = new MacWindow(BRect(0, 0, VModes[cur_mode].viXsize-1, VModes[cur_mode].viYsize-1));
				D(bug("Opened\n"));
				break;

			case MSG_CLOSE_WINDOW:
				if (the_window != NULL) {
					D(bug("Posting quit to window\n"));
					the_window->PostMessage(B_QUIT_REQUESTED);
					D(bug("Posted, waiting\n"));
					while (the_window)
						snooze(200000);
					D(bug("Window closed\n"));
				}
				break;

			case MSG_OPEN_SCREEN: {
				D(bug("Opening screen\n"));
				long scr_mode = 0;
				switch (VModes[cur_mode].viAppleMode) {
					case APPLE_8_BIT:
						switch (VModes[cur_mode].viAppleID) {
							case APPLE_640x480:
								scr_mode = B_8_BIT_640x480;
								break;
							case APPLE_800x600:
								scr_mode = B_8_BIT_800x600;
								break;
							case APPLE_1024x768:
								scr_mode = B_8_BIT_1024x768;
								break;
							case APPLE_1152x900:
								scr_mode = B_8_BIT_1152x900;
								break;
							case APPLE_1280x1024:
								scr_mode = B_8_BIT_1280x1024;
								break;
							case APPLE_1600x1200:
								scr_mode = B_8_BIT_1600x1200;
								break;
						}
						break;
					case APPLE_16_BIT:
						switch (VModes[cur_mode].viAppleID) {
							case APPLE_640x480:
								scr_mode = B_15_BIT_640x480;
								break;
							case APPLE_800x600:
								scr_mode = B_15_BIT_800x600;
								break;
							case APPLE_1024x768:
								scr_mode = B_15_BIT_1024x768;
								break;
							case APPLE_1152x900:
								scr_mode = B_15_BIT_1152x900;
								break;
							case APPLE_1280x1024:
								scr_mode = B_15_BIT_1280x1024;
								break;
							case APPLE_1600x1200:
								scr_mode = B_15_BIT_1600x1200;
								break;
						}
						break;
					case APPLE_32_BIT:
						switch (VModes[cur_mode].viAppleID) {
							case APPLE_640x480:
								scr_mode = B_32_BIT_640x480;
								break;
							case APPLE_800x600:
								scr_mode = B_32_BIT_800x600;
								break;
							case APPLE_1024x768:
								scr_mode = B_32_BIT_1024x768;
								break;
							case APPLE_1152x900:
								scr_mode = B_32_BIT_1152x900;
								break;
							case APPLE_1280x1024:
								scr_mode = B_32_BIT_1280x1024;
								break;
							case APPLE_1600x1200:
								scr_mode = B_32_BIT_1600x1200;
								break;
						}
						break;
				}
				the_screen = new MacScreen(GetString(STR_WINDOW_TITLE), scr_mode);
				D(bug("Opened, error %08lx\n", screen_error));
				if (screen_error != B_NO_ERROR) {
					D(bug("Error, posting quit to screen\n"));
					the_screen->PostMessage(B_QUIT_REQUESTED);
					D(bug("Posted, waiting\n"));
					while (the_screen)
						snooze(200000);
					D(bug("Screen closed\n"));
					break;
				}
		
				// Wait for video mem access
				D(bug("Showing screen\n"));
				the_screen->Show();
				D(bug("Shown, waiting for frame buffer access\n"));
				while (!drawing_enable)
					snooze(200000);
				D(bug("Access granted\n"));
				break;
			}

			case MSG_CLOSE_SCREEN:
				if (the_screen != NULL) {
					D(bug("Posting quit to screen\n"));
					the_screen->PostMessage(B_QUIT_REQUESTED);
					D(bug("Posted, waiting\n"));
					while (the_screen)
						snooze(200000);
					D(bug("Screen closed\n"));
				}
				break;
		}

		// Acknowledge
		release_sem(dm_done_sem);
	}
}


/*
 *  Open display (window or screen)
 */

static void open_display(void)
{
	D(bug("entering open_display()\n"));
	display_type = VModes[cur_mode].viType;
	if (display_type == DIS_SCREEN) {
		while (send_data(dm_thread, MSG_OPEN_SCREEN, NULL, 0) == B_INTERRUPTED) ;
		while (acquire_sem(dm_done_sem) == B_INTERRUPTED) ;
	} else if (display_type == DIS_WINDOW) {
		while (send_data(dm_thread, MSG_OPEN_WINDOW, NULL, 0) == B_INTERRUPTED) ;
		while (acquire_sem(dm_done_sem) == B_INTERRUPTED) ;
	}
	D(bug("exiting open_display()\n"));
}


/*
 *  Close display
 */

static void close_display(void)
{
	D(bug("entering close_display()\n"));
	if (display_type == DIS_SCREEN)  {
		while (send_data(dm_thread, MSG_CLOSE_SCREEN, NULL, 0) == B_INTERRUPTED) ;
		while (acquire_sem(dm_done_sem) == B_INTERRUPTED) ;
	} else if (display_type == DIS_WINDOW) {
		while (send_data(dm_thread, MSG_CLOSE_WINDOW, NULL, 0) == B_INTERRUPTED) ;
		while (acquire_sem(dm_done_sem) == B_INTERRUPTED) ;
	}
	D(bug("exiting close_display()\n"));
}


/*
 *  Initialization
 */

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

bool VideoInit(void)
{
	// Init variables, create semaphores
	private_data = NULL;
	cur_mode = 0;	// Window 640x480
	video_lock = create_sem(1, "Video Lock");
	mac_os_lock = create_sem(0, "MacOS Frame Buffer Lock");
	dm_done_sem = create_sem(0, "Display Manager Done");

	// Construct video mode table
	VideoInfo *p = VModes;
	uint32 window_modes = PrefsFindInt32("windowmodes");
	uint32 screen_modes = PrefsFindInt32("screenmodes");
	if (window_modes == 0 && screen_modes == 0)
		window_modes |= B_8_BIT_640x480 | B_8_BIT_800x600;	// Allow at least 640x480 and 800x600 window modes
	add_mode(p, window_modes, B_8_BIT_640x480, APPLE_8_BIT, APPLE_W_640x480, DIS_WINDOW);
	add_mode(p, window_modes, B_8_BIT_800x600, APPLE_8_BIT, APPLE_W_800x600, DIS_WINDOW);
	add_mode(p, window_modes, B_15_BIT_640x480, APPLE_16_BIT, APPLE_W_640x480, DIS_WINDOW);
	add_mode(p, window_modes, B_15_BIT_800x600, APPLE_16_BIT, APPLE_W_800x600, DIS_WINDOW);
	add_mode(p, window_modes, B_32_BIT_640x480, APPLE_32_BIT, APPLE_W_640x480, DIS_WINDOW);
	add_mode(p, window_modes, B_32_BIT_800x600, APPLE_32_BIT, APPLE_W_800x600, DIS_WINDOW);
	add_mode(p, screen_modes, B_8_BIT_640x480, APPLE_8_BIT, APPLE_640x480, DIS_SCREEN);
	add_mode(p, screen_modes, B_8_BIT_800x600, APPLE_8_BIT, APPLE_800x600, DIS_SCREEN);
	add_mode(p, screen_modes, B_8_BIT_1024x768, APPLE_8_BIT, APPLE_1024x768, DIS_SCREEN);
	add_mode(p, screen_modes, B_8_BIT_1152x900, APPLE_8_BIT, APPLE_1152x900, DIS_SCREEN);
	add_mode(p, screen_modes, B_8_BIT_1280x1024, APPLE_8_BIT, APPLE_1280x1024, DIS_SCREEN);
	add_mode(p, screen_modes, B_8_BIT_1600x1200, APPLE_8_BIT, APPLE_1600x1200, DIS_SCREEN);
	add_mode(p, screen_modes, B_15_BIT_640x480, APPLE_16_BIT, APPLE_640x480, DIS_SCREEN);
	add_mode(p, screen_modes, B_15_BIT_800x600, APPLE_16_BIT, APPLE_800x600, DIS_SCREEN);
	add_mode(p, screen_modes, B_15_BIT_1024x768, APPLE_16_BIT, APPLE_1024x768, DIS_SCREEN);
	add_mode(p, screen_modes, B_15_BIT_1152x900, APPLE_16_BIT, APPLE_1152x900, DIS_SCREEN);
	add_mode(p, screen_modes, B_15_BIT_1280x1024, APPLE_16_BIT, APPLE_1280x1024, DIS_SCREEN);
	add_mode(p, screen_modes, B_15_BIT_1600x1200, APPLE_16_BIT, APPLE_1600x1200, DIS_SCREEN);
	add_mode(p, screen_modes, B_32_BIT_640x480, APPLE_32_BIT, APPLE_640x480, DIS_SCREEN);
	add_mode(p, screen_modes, B_32_BIT_800x600, APPLE_32_BIT, APPLE_800x600, DIS_SCREEN);
	add_mode(p, screen_modes, B_32_BIT_1024x768, APPLE_32_BIT, APPLE_1024x768, DIS_SCREEN);
	add_mode(p, screen_modes, B_32_BIT_1152x900, APPLE_32_BIT, APPLE_1152x900, DIS_SCREEN);
	add_mode(p, screen_modes, B_32_BIT_1280x1024, APPLE_32_BIT, APPLE_1280x1024, DIS_SCREEN);
	add_mode(p, screen_modes, B_32_BIT_1600x1200, APPLE_32_BIT, APPLE_1600x1200, DIS_SCREEN);
	p->viType = DIS_INVALID;	// End marker
	p->viRowBytes = 0;
	p->viXsize = p->viYsize = 0;
	p->viAppleMode = 0;
	p->viAppleID = 0;

	// Start display manager thread
	dm_thread = spawn_thread(display_manager, "Display Manager", B_NORMAL_PRIORITY, NULL);
	resume_thread(dm_thread);

	// Open window/screen
	open_display();
	if (display_type == DIS_SCREEN && the_screen == NULL) {
		char str[256];
		sprintf(str, GetString(STR_FULL_SCREEN_ERR), strerror(screen_error), screen_error);
		ErrorAlert(str);
		return false;
	}
	return true;
}


/*
 *  Deinitialization
 */

void VideoExit(void)
{
	if (dm_thread >= 0) {

		// Close display
		acquire_sem(video_lock);
		close_display();
		if (private_data != NULL) {
			delete private_data->gammaTable;
			delete private_data;
		}

		// Stop display manager
		status_t l;
		send_data(dm_thread, MSG_QUIT_DISPLAY_MANAGER, NULL, 0);
		while (wait_for_thread(dm_thread, &l) == B_INTERRUPTED) ;
	}

	// Delete semaphores
	delete_sem(video_lock);
	delete_sem(mac_os_lock);
	delete_sem(dm_done_sem);
}


/*
 *  Close screen in full-screen mode
 */

void VideoQuitFullScreen(void)
{
	D(bug("VideoQuitFullScreen()\n"));
	if (display_type == DIS_SCREEN) {
		acquire_sem(video_lock);
		close_display();
		release_sem(video_lock);
	}
}


/*
 *  Execute video VBL routine
 */

void VideoVBL(void)
{
	release_sem(mac_os_lock);
	if (private_data != NULL && private_data->interruptsEnabled)
		VSLDoInterruptService(private_data->vslServiceID);
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
//	msg->PrintToStream();
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
 *  Install graphics acceleration
 */

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
		p->draw_proc = (uint32)accl_bitblt;
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
				p->draw_proc = (uint32)accl_fillrect8;
				return true;
			} else if (p->dest_pixel_size == 32 && fillrect32_hook != NULL) {
				p->draw_proc = (uint32)accl_fillrect32;
				return true;
			}
		} else if (p->transfer_mode == 10 && invrect_hook != NULL) {
			// Invert
			p->draw_proc = (uint32)accl_invrect;
			return true;
		}
	}
	return false;
}

// Dummy for testing
/*
static void do_nothing(accl_params *p) {}
static bool accl_foobar_hook(accl_params *p)
{
	printf("accl_foobar_hook %p\n", p);
	printf(" src_base_addr %p, dest_base_addr %p\n", p->src_base_addr, p->dest_base_addr);
	printf(" src_row_bytes %d, dest_row_bytes %d\n", p->src_row_bytes, p->dest_row_bytes);
	printf(" src_pixel_size %d, dest_pixel_size %d\n", p->src_pixel_size, p->dest_pixel_size);
	printf(" src_bounds (%d,%d,%d,%d), dest_bounds (%d,%d,%d,%d)\n", p->src_bounds[0], p->src_bounds[1], p->src_bounds[2], p->src_bounds[3], p->dest_bounds[0], p->dest_bounds[1], p->dest_bounds[2], p->dest_bounds[3]);
	printf(" src_rect (%d,%d,%d,%d), dest_rect (%d,%d,%d,%d)\n", p->src_rect[0], p->src_rect[1], p->src_rect[2], p->src_rect[3], p->dest_rect[0], p->dest_rect[1], p->dest_rect[2], p->dest_rect[3]);
	printf(" transfer mode %d\n", p->transfer_mode);
	printf(" pen mode %d\n", p->pen_mode);
	printf(" fore_pen %08x, back_pen %08x\n", p->fore_pen, p->back_pen);
	printf(" val1 %08x, val2 %08x\n", ((uint32 *)p)[0x18 >> 2], ((uint32 *)p)[0x128 >> 2]);
	printf(" val3 %08x\n", ((uint32 *)p)[0x130 >> 2]);
	printf(" val4 %08x\n", ((uint32 *)p)[0x15c >> 2]);
	printf(" val5 %08x\n", ((uint32 *)p)[0x160 >> 2]);
	printf(" val6 %08x\n", ((uint32 *)p)[0x1b4 >> 2]);
	printf(" val7 %08x\n", ((uint32 *)p)[0x284 >> 2]);
	p->draw_proc = (uint32)do_nothing;
	return true;
}
static struct accl_hook_info foobar_hook_info = {(uint32)accl_foobar_hook, (uint32)accl_sync_hook, 6};
*/

// Wait for graphics operation to finish
static bool accl_sync_hook(void *arg)
{
	D(bug("accl_sync_hook %p\n", arg));
	if (sync_hook != NULL)
		sync_hook();
	return true;
}

static struct accl_hook_info bitblt_hook_info = {(uint32)accl_bitblt_hook, (uint32)accl_sync_hook, ACCL_BITBLT};
static struct accl_hook_info fillrect_hook_info = {(uint32)accl_fillrect_hook, (uint32)accl_sync_hook, ACCL_FILLRECT};

void VideoInstallAccel(void)
{
	// Install acceleration hooks
	if (PrefsFindBool("gfxaccel")) {
		D(bug("Video: Installing acceleration hooks\n"));
		NQDMisc(6, (uintptr)&bitblt_hook_info);
		NQDMisc(6, (uintptr)&fillrect_hook_info);
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

			while (acquire_sem(video_lock) == B_INTERRUPTED) ;
			DisableInterrupt();

			/* close old display */
			close_display();

			/* open new display */
			cur_mode = i;
			open_display();

			/* opening the screen failed? Then bail out */
			if (display_type == DIS_SCREEN && the_screen == NULL) {
				release_sem(video_lock);
				ErrorAlert(GetString(STR_FULL_SCREEN_ERR));
				QuitEmulator();
			}

			WriteMacInt32(ParamPtr + csBaseAddr, screen_base);
			csSave->saveBaseAddr=screen_base;
			csSave->saveData=VModes[cur_mode].viAppleID;/* First mode ... */
			csSave->saveMode=VModes[cur_mode].viAppleMode;

			EnableInterrupt();
			release_sem(video_lock);
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
	if (display_type == DIS_SCREEN && the_screen != NULL)
		the_screen->palette_changed = true;
	else {									// remap colors to BeOS-Palette
		BScreen screen;
		for (int i=0;i<256;i++)
			remap_mac_be[i]=screen.IndexForColor(mac_pal[i].red,mac_pal[i].green,mac_pal[i].blue);
	}
}


/*
 *  Can we set the MacOS cursor image into the window?
 */

bool video_can_change_cursor(void)
{
	return (display_type != DIS_SCREEN);
}


/*
 *  Set cursor image for window
 */

void video_set_cursor(void)
{
	the_window->cursor_changed = true;	// Inform window (don't set cursor directly because this may run at interrupt (i.e. signal handler) time)
}


/*
 *  Record dirty area from NQD
 */

void video_set_dirty_area(int x, int y, int w, int h)
{
}
