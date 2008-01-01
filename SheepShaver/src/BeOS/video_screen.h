/*
 *  video_screen.h - Full screen video modes
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


static bool drawing_enable = false;	// This flag indicated if the access to the screen is allowed
static int page_num;				// Index of the currently displayed buffer


// Blitter functions
typedef void (*bitblt_ptr)(int32, int32, int32, int32, int32, int32);
static bitblt_ptr bitblt_hook;
typedef void (*fillrect8_ptr)(int32, int32, int32, int32, uint8);
static fillrect8_ptr fillrect8_hook;
typedef void (*fillrect32_ptr)(int32, int32, int32, int32, uint32);
static fillrect32_ptr fillrect32_hook;
typedef void (*invrect_ptr)(int32, int32, int32, int32);
static invrect_ptr invrect_hook;
typedef void (*sync_ptr)(void);
static sync_ptr sync_hook;


class MacScreen : public BWindowScreen {
public:
	MacScreen(const char *name, uint32 space);
	virtual ~MacScreen();
	virtual void Quit(void);
	virtual	void ScreenConnected(bool active);

	bool palette_changed;

private:
	static status_t tick_func(void *arg);

	BView *view;			// Main view for GetMouse()

	uint8 *frame_backup;	// Frame buffer backup when switching from/to different workspace
	bool quitting;			// Flag for ScreenConnected: We are quitting, don't pause emulator thread
	bool first;				// Flag for ScreenConnected: This is the first time we become active

	thread_id tick_thread;
	bool tick_thread_active;
};


// Pointer to our screen
static MacScreen *the_screen = NULL;

// Error code from BWindowScreen constructor
static status_t screen_error;

// to enable debugger mode.
#define SCREEN_DEBUG false


/*
 *  Screen constructor
 */

MacScreen::MacScreen(const char *name, uint32 space) : BWindowScreen(name, space, &screen_error, SCREEN_DEBUG), tick_thread(-1)
{
	D(bug("Screen constructor\n"));

	// Set all variables
	frame_backup = NULL;
	palette_changed = false;
	quitting = false;
	first = true;
	drawing_enable = false;
	ADBSetRelMouseMode(true);

	// Create view to poll the mouse
	view = new BView (BRect(0,0,VModes[cur_mode].viXsize-1,VModes[cur_mode].viYsize-1),NULL,B_FOLLOW_NONE,0);
	AddChild(view);

	// Start 60Hz interrupt
	tick_thread_active = true;
	tick_thread = spawn_thread(tick_func, "Polling sucks...", B_DISPLAY_PRIORITY, this);
	RegisterThread(tick_thread);
	resume_thread(tick_thread);

	// Add filter for keyboard and mouse events
	BMessageFilter *filter = new BMessageFilter(B_ANY_DELIVERY, B_ANY_SOURCE, filter_func);
	AddCommonFilter(filter);
	D(bug("Screen constructor done\n"));
}


/*
 *  Screen destructor
 */

MacScreen::~MacScreen()
{
	D(bug("Screen destructor, quitting tick thread\n"));

	// Stop 60Hz interrupt
	if (tick_thread > 0) {
		status_t l;
		tick_thread_active = false;
		while (wait_for_thread(tick_thread, &l) == B_INTERRUPTED) ;
	}
	D(bug("tick thread quit\n"));

	// Tell the emulator that we're done
	the_screen = NULL;
	D(bug("Screen destructor done\n"));
}


/*
 *  Screen closed
 */

void MacScreen::Quit(void)
{
	// Tell ScreenConnected() that we are quitting
	quitting = true;
	D(bug("MacScreen::Quit(), disconnecting\n"));
	Disconnect();
	D(bug("disconnected\n"));
	BWindowScreen::Quit();
}


/*
 *  Screen connected/disconnected
 */

void MacScreen::ScreenConnected(bool active)
{
	D(bug("ScreenConnected(%d)\n", active));
	graphics_card_info *info = CardInfo();
	D(bug(" card_info %p\n", info));

	if (active) {

		// Read graphics parameters
		D(bug(" active\n"));
		screen_base = (uint32)info->frame_buffer;
		D(bug(" screen_base %p\n", screen_base));
		VModes[cur_mode].viRowBytes = info->bytes_per_row;
		D(bug(" xmod %d\n", info->bytes_per_row));

		// Get acceleration functions
		if (PrefsFindBool("gfxaccel")) {
			bitblt_hook = (bitblt_ptr)CardHookAt(7);
			D(bug(" bitblt_hook %p\n", bitblt_hook));
			fillrect8_hook = (fillrect8_ptr)CardHookAt(5);
			D(bug(" fillrect8_hook %p\n", fillrect8_hook));
			fillrect32_hook = (fillrect32_ptr)CardHookAt(6);
			D(bug(" fillrect32_hook %p\n", fillrect32_hook));
			invrect_hook = (invrect_ptr)CardHookAt(11);
			D(bug(" invrect_hook %p\n", invrect_hook));
			sync_hook = (sync_ptr)CardHookAt(10);
			D(bug(" sync_hook %p\n", sync_hook));
		} else {
			bitblt_hook = NULL;
			fillrect8_hook = NULL;
			fillrect32_hook = NULL;
			invrect_hook = NULL;
			sync_hook = NULL;
		}

		// The first time we got the screen, we need to init the Window
		if (first) {
			D(bug(" first time\n"));
			first = false;
			page_num = 0;  								// current display : page 0
		} else { 										// we get our screen back
			D(bug(" not first time\n"));
			// copy from backup bitmap to framebuffer
			memcpy((void *)screen_base, frame_backup, VModes[cur_mode].viRowBytes * VModes[cur_mode].viYsize);
			// delete backup bitmap
			delete[] frame_backup;			
			frame_backup = NULL;
			// restore palette
			if (info->bits_per_pixel == 8)
				SetColorList(mac_pal);
			// restart emul thread
			release_sem(mac_os_lock);
		}

		// allow the drawing in the frame buffer
		D(bug(" enabling frame buffer access\n"));
		drawing_enable = true;
		video_activated = true;

	} else {

		drawing_enable = false;							// stop drawing.
		video_activated = false;
		if (!quitting) {
			// stop emul thread
			acquire_sem(mac_os_lock);
			// create bitmap and store frame buffer into
			frame_backup = new uint8[VModes[cur_mode].viRowBytes * VModes[cur_mode].viYsize];
			memcpy(frame_backup, (void *)screen_base, VModes[cur_mode].viRowBytes * VModes[cur_mode].viYsize);
		}
	}
	D(bug("ScreenConnected() done\n"));
}


/*
 *  60Hz interrupt routine
 */

status_t MacScreen::tick_func(void *arg)
{
	MacScreen *obj = (MacScreen *)arg;
	while (obj->tick_thread_active) {

		// Wait
		snooze(16667);

		// Workspace activated? Then poll the mouse and change the palette if needed
		if (video_activated) {
			BPoint pt;
			uint32 button = 0;
			if (obj->LockWithTimeout(200000) == B_OK) {
				if (obj->palette_changed) {
					obj->palette_changed = false;
					obj->SetColorList(mac_pal);
				}
				obj->view->GetMouse(&pt, &button);
				obj->Unlock();
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
		}
	}
	return 0;
}
