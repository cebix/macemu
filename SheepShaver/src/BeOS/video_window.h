/*
 *  video_window.h - Window video modes
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

#include <DirectWindow.h>


// Messages
static const uint32 MSG_REDRAW = 'draw';
static const uint32 MSG_ABOUT_REQUESTED = B_ABOUT_REQUESTED;
static const uint32 MSG_REF_5HZ = ' 5Hz';
static const uint32 MSG_REF_7_5HZ = ' 7Hz';
static const uint32 MSG_REF_10HZ = '10Hz';
static const uint32 MSG_REF_15HZ = '15Hz';
static const uint32 MSG_REF_30HZ = '30Hz';
static const uint32 MSG_REF_60HZ = '60Hz';
static const uint32 MSG_MOUNT = 'moun';

static bool mouse_in_view;	// Flag: Mouse pointer within bitmap view

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
		if (the_bitmap)
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
	bool cursor_changed;	// Flag: set new cursor image in tick function

private:
	static status_t tick_func(void *arg);

	BitmapView *main_view;
	BBitmap *the_bitmap;
	uint8 *the_buffer;

	uint32 old_scroll_lock_state;

	thread_id tick_thread;
	bool tick_thread_active;

	bool supports_direct_mode;
	bool bit_bang;
	sem_id drawing_sem;

	color_space mode;
	void *bits;
	int32 bytes_per_row;
	color_space pixel_format;
	bool unclipped;
};


// Pointer to our window
static MacWindow *the_window = NULL;


/*
 *  Window constructor
 */

MacWindow::MacWindow(BRect frame) : BDirectWindow(frame, GetString(STR_WINDOW_TITLE), B_TITLED_WINDOW, B_NOT_RESIZABLE | B_NOT_CLOSABLE | B_NOT_ZOOMABLE)
{
	D(bug("Window constructor\n"));
	supports_direct_mode = SupportsWindowMode();
	cursor_changed = false;
	bit_bang = supports_direct_mode && PrefsFindBool("bitbang");

	// Move window to right position
	Lock();
	MoveTo(80, 60);

	// Allocate bitmap
	{
		BScreen scr(this);
		mode = B_COLOR_8_BIT;
		switch (VModes[cur_mode].viAppleMode) {
			case APPLE_8_BIT:
				mode = B_COLOR_8_BIT;
				bit_bang = false;
				break;
			case APPLE_16_BIT:
				mode = B_RGB_16_BIT;
				if (scr.ColorSpace() != B_RGB15_BIG && scr.ColorSpace() != B_RGBA15_BIG)
					bit_bang = false;
				break;
			case APPLE_32_BIT:
				mode = B_RGB_32_BIT;
				if (scr.ColorSpace() != B_RGB32_BIG && scr.ColorSpace() != B_RGBA32_BIG)
					bit_bang = false;
				break;
		}
	}
	if (bit_bang) {
		the_bitmap = NULL;
		the_buffer = NULL;
	} else {
		the_bitmap = new BBitmap(frame, mode);
		the_buffer = new uint8[VModes[cur_mode].viRowBytes * (VModes[cur_mode].viYsize + 2)];	// ("height + 2" for safety)
		screen_base = (uint32)the_buffer;
	}

	// Create bitmap view
	main_view = new BitmapView(frame, the_bitmap);
	AddChild(main_view);
	main_view->MakeFocus();

	// Read frame skip prefs
	frame_skip = PrefsFindInt32("frameskip");

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
	bar->AddItem(menu);
	AddChild(bar);
	SetKeyMenuBar(bar);
	int mbar_height = bar->Frame().IntegerHeight() + 1;

	// Resize window to fit menu bar
	ResizeBy(0, mbar_height);

	// Set mouse mode and scroll lock state
	ADBSetRelMouseMode(false);
	mouse_in_view = true;
	old_scroll_lock_state = modifiers() & B_SCROLL_LOCK;
	if (old_scroll_lock_state)
		SetTitle(GetString(STR_WINDOW_TITLE_FROZEN));
	else
		SetTitle(GetString(STR_WINDOW_TITLE));

	// Clear Mac cursor image
	memset(MacCursor + 4, 0, 64);

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
	D(bug("Window constructor done\n"));
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
	D(bug("Window destructor, hiding window\n"));
	Hide();
	Sync();

	// Stop 60Hz interrupt
	D(bug("Quitting tick thread\n"));
	status_t l;
	tick_thread_active = false;
	delete_sem(drawing_sem);
	while (wait_for_thread(tick_thread, &l) == B_INTERRUPTED) ;
	D(bug("tick thread quit\n"));

	// dispose allocated memory
	delete the_bitmap;
	delete[] the_buffer;

	// Tell emulator that we're done
	the_window = NULL;
	D(bug("Window destructor done\n"));
}


/*
 *  Window connected/disconnected
 */

void MacWindow::DirectConnected(direct_buffer_info *info)
{
	D(bug("DirectConnected, state %d\n", info->buffer_state));
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
			if (bit_bang) {
				screen_base = (uint32)bits;
				VModes[cur_mode].viRowBytes = bytes_per_row;
			}
			release_sem(drawing_sem);
			break;
	}
	D(bug("DirectConnected done\n"));
}


/*
 *  Handles redraw messages
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
			uint32 length = VModes[cur_mode].viRowBytes * VModes[cur_mode].viYsize;
			if (mode == B_COLOR_8_BIT) {
				// Palette conversion
				uint8 *source = the_buffer - 1;
				uint8 *dest = (uint8 *)the_bitmap->Bits() - 1;
				for (int i=0; i<length; i++)
					*++dest = remap_mac_be[*++source];
			} else if (mode == B_RGB_16_BIT) {
				// Endianess conversion
				uint16 *source = (uint16 *)the_buffer;
				uint16 *dest = (uint16 *)the_bitmap->Bits() - 1;
				for (int i=0; i<length/2; i++)
					*++dest = __lhbrx(source++, 0);
			} else if (mode == B_RGB_32_BIT) {
				// Endianess conversion
				uint32 *source = (uint32 *)the_buffer;
				uint32 *dest = (uint32 *)the_bitmap->Bits() - 1;
				for (int i=0; i<length/4; i++)
					*++dest = __lwbrx(source++, 0);
			}
			BRect update_rect = BRect(0, 0, VModes[cur_mode].viXsize-1, VModes[cur_mode].viYsize-1);
			main_view->DrawBitmapAsync(the_bitmap, update_rect, update_rect);
			break;
		}

		case MSG_ABOUT_REQUESTED:
			OpenAboutWindow();
			break;

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

		default:
			BDirectWindow::MessageReceived(msg);
	}
}


/*
 *  Window activated/deactivated
 */

void MacWindow::WindowActivated(bool active)
{
	video_activated = active;
	if (active)
		frame_skip = PrefsFindInt32("frameskip");
	else
		frame_skip = 12;	// 5Hz in background
	BDirectWindow::WindowActivated(active);
}


/*
 *  60Hz interrupt routine
 */

status_t MacWindow::tick_func(void *arg)
{
	MacWindow *obj = (MacWindow *)arg;
	static int tick_counter = 0;
	while (obj->tick_thread_active) {

		// Wait
		snooze(16667);

		// Refresh window
		if (!obj->bit_bang)
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

			// Refresh display unless Scroll Lock is down
			if (!scroll_lock_state) {

				// If direct frame buffer access is supported and the content area is completely visible,
				// convert the Mac screen buffer directly. Otherwise, send a message to the window to do
				// it into a bitmap
				if (obj->supports_direct_mode) {
					if (acquire_sem_etc(obj->drawing_sem, 1, B_TIMEOUT, 200000) == B_NO_ERROR) {
						if (obj->unclipped && obj->mode == B_COLOR_8_BIT && obj->pixel_format == B_CMAP8) {
							uint8 *source = obj->the_buffer - 1;
							uint8 *dest = (uint8 *)obj->bits;
							uint32 bytes_per_row = obj->bytes_per_row;
							int xsize = VModes[cur_mode].viXsize;
							int ysize = VModes[cur_mode].viYsize;
							for (int y=0; y<ysize; y++) {
								uint32 *p = (uint32 *)dest - 1;
								for (int x=0; x<xsize/4; x++) {
									uint32 c = remap_mac_be[*++source] << 24;
									c |= remap_mac_be[*++source] << 16;
									c |= remap_mac_be[*++source] << 8;
									c |= remap_mac_be[*++source];
									*++p = c;
								}
								dest += bytes_per_row;
							}
						} else if (obj->unclipped && obj->mode == B_RGB_16_BIT && (obj->pixel_format == B_RGB15_BIG || obj->pixel_format == B_RGBA15_BIG)) {
							uint8 *source = obj->the_buffer;
							uint8 *dest = (uint8 *)obj->bits;
							uint32 sbpr = VModes[cur_mode].viRowBytes;
							uint32 dbpr = obj->bytes_per_row;
							int xsize = VModes[cur_mode].viXsize;
							int ysize = VModes[cur_mode].viYsize;
							for (int y=0; y<ysize; y++) {
								memcpy(dest, source, xsize * 2);
								source += sbpr;
								dest += dbpr;
							}
						} else if (obj->unclipped && obj->mode == B_RGB_32_BIT && (obj->pixel_format == B_RGB32_BIG || obj->pixel_format == B_RGBA32_BIG)) {
							uint8 *source = obj->the_buffer;
							uint8 *dest = (uint8 *)obj->bits;
							uint32 sbpr = VModes[cur_mode].viRowBytes;
							uint32 dbpr = obj->bytes_per_row;
							int xsize = VModes[cur_mode].viXsize;
							int ysize = VModes[cur_mode].viYsize;
							for (int y=0; y<ysize; y++) {
								memcpy(dest, source, xsize * 4);
								source += sbpr;
								dest += dbpr;
							}
						} else
							obj->PostMessage(MSG_REDRAW);
						release_sem(obj->drawing_sem);
					}
				} else
					obj->PostMessage(MSG_REDRAW);
			}
		}

		// Set new cursor image if desired
		if (obj->cursor_changed) {
			if (mouse_in_view)
				be_app->SetCursor(MacCursor);
			obj->cursor_changed = false;
		}
	}
	return 0;
}


/*
 *  Mouse moved
 */

void BitmapView::MouseMoved(BPoint point, uint32 transit, const BMessage *message)
{
	switch (transit) {
		case B_ENTERED_VIEW:
			mouse_in_view = true;
			be_app->SetCursor(MacCursor);
			break;
		case B_EXITED_VIEW:
			mouse_in_view = false;
			be_app->SetCursor(B_HAND_CURSOR);
			break;
	}
}
