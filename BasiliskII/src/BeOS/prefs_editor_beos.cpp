/*
 *  prefs_editor_beos.cpp - Preferences editor, BeOS implementation
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

#include <AppKit.h>
#include <InterfaceKit.h>
#include <StorageKit.h>
#include <SerialPort.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fs_info.h>

#include "sysdeps.h"
#include "main.h"
#include "cdrom.h"
#include "xpram.h"
#include "prefs.h"
#include "about_window.h"
#include "user_strings.h"
#include "prefs_editor.h"


// Special colors
const rgb_color fill_color = {216, 216, 216, 0};
const rgb_color slider_fill_color = {102, 152, 255, 0};

// Display types
enum {
	DISPLAY_WINDOW,
	DISPLAY_SCREEN
};

// Window messages
const uint32 MSG_OK = 'okok';				// "Start" clicked
const uint32 MSG_CANCEL = 'cncl';			// "Quit" clicked
const uint32 MSG_ZAP_PRAM = 'zprm';

const int NUM_PANES = 4;

const uint32 MSG_VOLUME_SELECTED = 'volu';	// "Volumes" pane
const uint32 MSG_VOLUME_INVOKED = 'voli';
const uint32 MSG_ADD_VOLUME = 'addv';
const uint32 MSG_CREATE_VOLUME = 'crev';
const uint32 MSG_REMOVE_VOLUME = 'remv';
const uint32 MSG_ADD_VOLUME_PANEL = 'advp';
const uint32 MSG_CREATE_VOLUME_PANEL = 'crvp';
const uint32 MSG_DEVICE_NAME = 'devn';
const uint32 MSG_BOOT_ANY = 'bany';
const uint32 MSG_BOOT_CDROM = 'bcdr';
const uint32 MSG_NOCDROM = 'nocd';

const uint32 MSG_REF_5HZ = ' 5Hz';			// "Graphics/Sound" pane
const uint32 MSG_REF_7_5HZ = ' 7Hz';
const uint32 MSG_REF_10HZ = '10Hz';
const uint32 MSG_REF_15HZ = '15Hz';
const uint32 MSG_REF_30HZ = '30Hz';
const uint32 MSG_VIDEO_WINDOW = 'vtyw';
const uint32 MSG_VIDEO_SCREEN = 'vtys';
const uint32 MSG_SCREEN_MODE = 'sm\0\0';
const uint32 MSG_NOSOUND = 'nosn';

const uint32 MSG_SER_A = 'sera';			// "Serial/Network" pane
const uint32 MSG_SER_B = 'serb';
const uint32 MSG_ETHER = 'ethr';
const uint32 MSG_UDPTUNNEL = 'udpt';

const uint32 MSG_RAMSIZE = 'rmsz';			// "Memory/Misc" pane
const uint32 MSG_MODELID_5 = 'mi05';
const uint32 MSG_MODELID_14 = 'mi14';
const uint32 MSG_CPU_68020 = 'cpu2';
const uint32 MSG_CPU_68020_FPU = 'cpf2';
const uint32 MSG_CPU_68030 = 'cpu3';
const uint32 MSG_CPU_68030_FPU = 'cpf3';
const uint32 MSG_CPU_68040 = 'cpu4';


// RAM size slider class
class RAMSlider : public BSlider {
public:
	RAMSlider(BRect frame, const char *name, const char *label, BMessage *message,
		int32 minValue, int32 maxValue, thumb_style thumbType = B_BLOCK_THUMB,
		uint32 resizingMode = B_FOLLOW_LEFT |
							B_FOLLOW_TOP,
		uint32 flags = B_NAVIGABLE | B_WILL_DRAW |
						B_FRAME_EVENTS) : BSlider(frame, name, label, message, minValue, maxValue, thumbType, resizingMode, flags)
	{
		update_text = (char *)malloc(256);
	}

	virtual ~RAMSlider()
	{
		if (update_text)
			free(update_text);
	}

	virtual char *UpdateText(void) const
	{
		if (update_text) {
			sprintf(update_text, GetString(STR_RAMSIZE_FMT), Value());
		} 
		return update_text;
	}

private:
	char *update_text;
};


// Volumes list view class
class VolumeListView : public BListView {
public:
	VolumeListView(BRect frame, const char *name, list_view_type type = B_SINGLE_SELECTION_LIST, uint32 resizeMask = B_FOLLOW_LEFT | B_FOLLOW_TOP, uint32 flags = B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE)
		: BListView(frame, name, type, resizeMask, flags)
	{}

	// Handle dropped files and volumes
	virtual	void MessageReceived(BMessage *msg)
	{
		if (msg->what == B_SIMPLE_DATA) {
			BMessage msg2(MSG_ADD_VOLUME_PANEL);
			entry_ref ref;
			for (int i=0; msg->FindRef("refs", i, &ref) == B_NO_ERROR; i++)
				msg2.AddRef("refs", &ref);
			Window()->PostMessage(&msg2);
		} else
			BListView::MessageReceived(msg);
	}
};


// Number-entry BTextControl
class NumberControl : public BTextControl {
public:
	NumberControl(BRect frame, float divider, const char *name, const char *label, long value, BMessage *message)
	 : BTextControl(frame, name, label, NULL, message, B_FOLLOW_LEFT | B_FOLLOW_TOP, B_WILL_DRAW | B_NAVIGABLE)
	{
		SetDivider(divider);
		for (int c=0; c<256; c++)
			if (!isdigit(c) && c != B_BACKSPACE && c != B_LEFT_ARROW && c != B_RIGHT_ARROW) 
				((BTextView *)ChildAt(0))->DisallowChar(c);
		SetValue(value);
	}

	// Set integer value
	void SetValue(long value)
	{
		char str[32];
		sprintf(str, "%ld", value);
		SetText(str);
	}

	// Get integer value
	long Value(void)
	{
		return atol(Text());
	}
};


// Path-entry BTextControl
class PathControl : public BTextControl {
public:
	PathControl(bool dir_ctrl_, BRect frame, const char *name, const char *label, const char *text, BMessage *message) : BTextControl(frame, name, label, text, message), dir_ctrl(dir_ctrl_)
	{
		for (int c=0; c<' '; c++)
			if (c != B_BACKSPACE && c != B_LEFT_ARROW && c != B_RIGHT_ARROW) 
				((BTextView *)ChildAt(0))->DisallowChar(c);
	}

	virtual void MessageReceived(BMessage *msg)
	{
		if (msg->what == B_SIMPLE_DATA) {
			entry_ref the_ref;
			BEntry the_entry;

			// Look for dropped refs
			if (msg->FindRef("refs", &the_ref) == B_NO_ERROR) {
				if (the_entry.SetTo(&the_ref) == B_NO_ERROR && (dir_ctrl&& the_entry.IsDirectory() || !dir_ctrl && the_entry.IsFile())) {
					BPath the_path;
					the_entry.GetPath(&the_path);
					SetText(the_path.Path());
				}
			} else
				BTextControl::MessageReceived(msg);

			MakeFocus();
		} else
			BTextControl::MessageReceived(msg);
	}

private:
	bool dir_ctrl;
};


// Preferences window class
class PrefsWindow : public BWindow {
public:
	PrefsWindow(uint32 msg);
	virtual ~PrefsWindow();
	virtual void MessageReceived(BMessage *msg);

private:
	void read_volumes_prefs(void);
	void hide_show_graphics_ctrls(void);
	void read_graphics_prefs(void);
	void hide_show_serial_ctrls(void);
	void read_serial_prefs(void);
	void add_serial_names(BPopUpMenu *menu, uint32 msg);
	void read_memory_prefs(void);

	BView *create_volumes_pane(void);
	BView *create_graphics_pane(void);
	BView *create_serial_pane(void);
	BView *create_memory_pane(void);

	uint32 ok_message;
	bool send_quit_on_close;

	system_info sys_info;
	BMessenger this_messenger;
	BView *top;
	BRect top_frame;
	BTabView *pane_tabs;
	BView *panes[NUM_PANES];
	int current_pane;

	VolumeListView *volume_list;
	BCheckBox *nocdrom_checkbox;
	BMenuField *frameskip_menu;
	NumberControl *display_x_ctrl;
	NumberControl *display_y_ctrl;
	BMenuField *scr_mode_menu;
	BCheckBox *nosound_checkbox;
	BCheckBox *ether_checkbox;
	BCheckBox *udptunnel_checkbox;
	NumberControl *udpport_ctrl;
	RAMSlider *ramsize_slider;
	PathControl *extfs_control;
	PathControl *rom_control;
	BCheckBox *fpu_checkbox;

	BFilePanel *add_volume_panel;
	BFilePanel *create_volume_panel;

	uint32 max_ramsize;		// In MB
	int display_type;
	int scr_mode_bit;
};


/*
 *  Show preferences editor (asynchronously)
 *  Under BeOS, the return value is ignored. Instead, a message is sent to the
 *  application when the user clicks on "Start" or "Quit"
 */

bool PrefsEditor(void)
{
	new PrefsWindow('strt');
	return true;
}


/*
 *  Preferences window constructor
 */

PrefsWindow::PrefsWindow(uint32 msg) : BWindow(BRect(0, 0, 400, 289), GetString(STR_PREFS_TITLE), B_TITLED_WINDOW, B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_ASYNCHRONOUS_CONTROLS), this_messenger(this)
{
	int i;
	ok_message = msg;
	send_quit_on_close = true;
	get_system_info(&sys_info);

	// Move window to right position
	Lock();
	MoveTo(80, 80);

	// Set up menus
	BMenuBar *bar = new BMenuBar(Bounds(), "menu");
	BMenu *menu = new BMenu(GetString(STR_PREFS_MENU));
	menu->AddItem(new BMenuItem(GetString(STR_PREFS_ITEM_ABOUT), new BMessage(B_ABOUT_REQUESTED)));
	menu->AddItem(new BSeparatorItem);
	menu->AddItem(new BMenuItem(GetString(STR_PREFS_ITEM_START), new BMessage(MSG_OK)));
	menu->AddItem(new BMenuItem(GetString(STR_PREFS_ITEM_ZAP_PRAM), new BMessage(MSG_ZAP_PRAM)));
	menu->AddItem(new BSeparatorItem);
	menu->AddItem(new BMenuItem(GetString(STR_PREFS_ITEM_QUIT), new BMessage(MSG_CANCEL), 'Q'));
	bar->AddItem(menu);
	AddChild(bar);
	SetKeyMenuBar(bar);
	int mbar_height = int(bar->Bounds().bottom) + 1;

	// Resize window to fit menu bar
	ResizeBy(0, mbar_height);

	// Light gray background
	BRect b = Bounds();
	top = new BView(BRect(0, mbar_height, b.right, b.bottom), "top", B_FOLLOW_NONE, B_WILL_DRAW);
	AddChild(top);
	top->SetViewColor(fill_color);
	top_frame = top->Bounds();

	// Create panes
	panes[0] = create_volumes_pane();
	panes[1] = create_graphics_pane();
	panes[2] = create_serial_pane();
	panes[3] = create_memory_pane();

	// Prefs item tab view
	pane_tabs = new BTabView(BRect(10, 10, top_frame.right-10, top_frame.bottom-50), "items", B_WIDTH_FROM_LABEL);
	for (i=0; i<NUM_PANES; i++)
		pane_tabs->AddTab(panes[i]);
	top->AddChild(pane_tabs);

	volume_list->Select(0);

	// Create volume file panels
	add_volume_panel = new BFilePanel(B_OPEN_PANEL, &this_messenger, NULL, B_FILE_NODE | B_DIRECTORY_NODE, false, new BMessage(MSG_ADD_VOLUME_PANEL));
	add_volume_panel->SetButtonLabel(B_DEFAULT_BUTTON, GetString(STR_ADD_VOLUME_PANEL_BUTTON));
	add_volume_panel->Window()->SetTitle(GetString(STR_ADD_VOLUME_TITLE));
	create_volume_panel = new BFilePanel(B_SAVE_PANEL, &this_messenger, NULL, B_FILE_NODE | B_DIRECTORY_NODE, false, new BMessage(MSG_CREATE_VOLUME_PANEL));
	create_volume_panel->SetButtonLabel(B_DEFAULT_BUTTON, GetString(STR_CREATE_VOLUME_PANEL_BUTTON));
	create_volume_panel->Window()->SetTitle(GetString(STR_CREATE_VOLUME_TITLE));

	create_volume_panel->Window()->Lock();
	BView *background = create_volume_panel->Window()->ChildAt(0);
	background->FindView("PoseView")->ResizeBy(0, -30);
	background->FindView("VScrollBar")->ResizeBy(0, -30);
	background->FindView("CountVw")->MoveBy(0, -30);
	BView *v = background->FindView("HScrollBar");
	if (v)
		v->MoveBy(0, -30);
	else {
		i = 0;
		while ((v = background->ChildAt(i++)) != NULL) {
			if (v->Name() == NULL || v->Name()[0] == 0) {
				v->MoveBy(0, -30);	// unnamed horizontal scroll bar
				break;
			}
		}
	}
	BView *filename = background->FindView("text view");
	BRect fnr(filename->Frame());
	fnr.OffsetBy(0, -30);
	NumberControl *nc = new NumberControl(fnr, 80, "hardfile_size", GetString(STR_HARDFILE_SIZE_CTRL), 40, NULL);
	background->AddChild(nc);
	create_volume_panel->Window()->Unlock();

	// "Start" button
	BButton *button = new BButton(BRect(20, top_frame.bottom-35, 90, top_frame.bottom-10), "start", GetString(STR_START_BUTTON), new BMessage(MSG_OK));
	top->AddChild(button);
	SetDefaultButton(button);

	// "Quit" button
	top->AddChild(new BButton(BRect(top_frame.right-90, top_frame.bottom-35, top_frame.right-20, top_frame.bottom-10), "cancel", GetString(STR_QUIT_BUTTON), new BMessage(MSG_CANCEL)));

	Unlock();
	Show();
}


/*
 *  Preferences window destructor
 */

PrefsWindow::~PrefsWindow()
{
	delete add_volume_panel;
	delete create_volume_panel;
	if (send_quit_on_close)
		be_app->PostMessage(B_QUIT_REQUESTED);
}


/*
 *  Create "Volumes" pane
 */

void PrefsWindow::read_volumes_prefs(void)
{
	PrefsReplaceString("extfs", extfs_control->Text());
}

BView *PrefsWindow::create_volumes_pane(void)
{
	BView *pane = new BView(BRect(0, 0, top_frame.right-20, top_frame.bottom-80), GetString(STR_VOLUMES_PANE_TITLE), B_FOLLOW_NONE, B_WILL_DRAW);
	pane->SetViewColor(fill_color);
	float right = pane->Bounds().right-10;

	const char *str;
	int32 index = 0;
	volume_list = new VolumeListView(BRect(15, 10, pane->Bounds().right-30, 113), "volumes");
	while ((str = PrefsFindString("disk", index++)) != NULL)
		volume_list->AddItem(new BStringItem(str));
	volume_list->SetSelectionMessage(new BMessage(MSG_VOLUME_SELECTED));
	volume_list->SetInvocationMessage(new BMessage(MSG_VOLUME_INVOKED));
	pane->AddChild(new BScrollView("volumes_border", volume_list, B_FOLLOW_LEFT | B_FOLLOW_TOP, 0, false, true));

	pane->AddChild(new BButton(BRect(10, 118, pane->Bounds().right/3, 138), "add_volume", GetString(STR_ADD_VOLUME_BUTTON), new BMessage(MSG_ADD_VOLUME)));
	pane->AddChild(new BButton(BRect(pane->Bounds().right/3, 118, pane->Bounds().right*2/3, 138), "create_volume", GetString(STR_CREATE_VOLUME_BUTTON), new BMessage(MSG_CREATE_VOLUME)));
	pane->AddChild(new BButton(BRect(pane->Bounds().right*2/3, 118, pane->Bounds().right-11, 138), "remove_volume", GetString(STR_REMOVE_VOLUME_BUTTON), new BMessage(MSG_REMOVE_VOLUME)));

	extfs_control = new PathControl(true, BRect(10, 145, right, 160), "extfs", GetString(STR_EXTFS_CTRL), PrefsFindString("extfs"), NULL);
	extfs_control->SetDivider(90);
	pane->AddChild(extfs_control);

	BMenuField *menu_field;
	BPopUpMenu *menu = new BPopUpMenu("");
	menu_field = new BMenuField(BRect(10, 165, right, 180), "bootdriver", GetString(STR_BOOTDRIVER_CTRL), menu);
	menu_field->SetDivider(90);
	menu->AddItem(new BMenuItem(GetString(STR_BOOT_ANY_LAB), new BMessage(MSG_BOOT_ANY)));
	menu->AddItem(new BMenuItem(GetString(STR_BOOT_CDROM_LAB), new BMessage(MSG_BOOT_CDROM)));
	pane->AddChild(menu_field);
	int32 i32 = PrefsFindInt32("bootdriver");
	BMenuItem *item;
	if (i32 == 0) {
		if ((item = menu->FindItem(GetString(STR_BOOT_ANY_LAB))) != NULL)
			item->SetMarked(true);
	} else if (i32 == CDROMRefNum) {
		if ((item = menu->FindItem(GetString(STR_BOOT_CDROM_LAB))) != NULL)
			item->SetMarked(true);
	}

	nocdrom_checkbox = new BCheckBox(BRect(10, 185, right, 200), "nocdrom", GetString(STR_NOCDROM_CTRL), new BMessage(MSG_NOCDROM));
	pane->AddChild(nocdrom_checkbox);
	nocdrom_checkbox->SetValue(PrefsFindBool("nocdrom") ? B_CONTROL_ON : B_CONTROL_OFF);

	return pane;
}


/*
 *  Create "Graphics/Sound" pane
 */

// Screen mode list
struct scr_mode_desc {
	int mode_mask;
	int str;
};

static const scr_mode_desc scr_mode[] = {
	{B_8_BIT_640x480, STR_8_BIT_640x480_LAB},
	{B_8_BIT_800x600, STR_8_BIT_800x600_LAB},
	{B_8_BIT_1024x768, STR_8_BIT_1024x768_LAB},
    {B_8_BIT_1152x900, STR_8_BIT_1152x900_LAB},
	{B_8_BIT_1280x1024, STR_8_BIT_1280x1024_LAB},
	{B_8_BIT_1600x1200, STR_8_BIT_1600x1200_LAB},
	{B_15_BIT_640x480, STR_15_BIT_640x480_LAB},
	{B_15_BIT_800x600, STR_15_BIT_800x600_LAB},
	{B_15_BIT_1024x768, STR_15_BIT_1024x768_LAB},
    {B_15_BIT_1152x900, STR_15_BIT_1152x900_LAB},
	{B_15_BIT_1280x1024, STR_15_BIT_1280x1024_LAB},
	{B_15_BIT_1600x1200, STR_15_BIT_1600x1200_LAB},
	{B_32_BIT_640x480, STR_24_BIT_640x480_LAB},
	{B_32_BIT_800x600, STR_24_BIT_800x600_LAB},
	{B_32_BIT_1024x768, STR_24_BIT_1024x768_LAB},
    {B_32_BIT_1152x900, STR_24_BIT_1152x900_LAB},
	{B_32_BIT_1280x1024, STR_24_BIT_1280x1024_LAB},
	{B_32_BIT_1600x1200, STR_24_BIT_1600x1200_LAB},
	{0, 0}	// End marker
};

void PrefsWindow::hide_show_graphics_ctrls(void)
{
	if (display_type == DISPLAY_WINDOW) {
		if (!scr_mode_menu->IsHidden())
			scr_mode_menu->Hide();
		if (frameskip_menu->IsHidden())
			frameskip_menu->Show();
		if (display_x_ctrl->IsHidden())
			display_x_ctrl->Show();
		if (display_y_ctrl->IsHidden())
			display_y_ctrl->Show();
	} else {
		if (!frameskip_menu->IsHidden())
			frameskip_menu->Hide();
		if (!display_x_ctrl->IsHidden())
			display_x_ctrl->Hide();
		if (!display_y_ctrl->IsHidden())
			display_y_ctrl->Hide();
		if (scr_mode_menu->IsHidden())
			scr_mode_menu->Show();
	}
}

void PrefsWindow::read_graphics_prefs(void)
{
	char str[64];
	if (display_type == DISPLAY_WINDOW) {
		sprintf(str, "win/%d/%d", display_x_ctrl->Value(), display_y_ctrl->Value());
	} else {
		sprintf(str, "scr/%d", scr_mode_bit);
	}
	PrefsReplaceString("screen", str);
}

BView *PrefsWindow::create_graphics_pane(void)
{
	BView *pane = new BView(BRect(0, 0, top_frame.right-20, top_frame.bottom-80), GetString(STR_GRAPHICS_SOUND_PANE_TITLE), B_FOLLOW_NONE, B_WILL_DRAW);
	pane->SetViewColor(fill_color);
	float right = pane->Bounds().right-10;

	const char *mode_str = PrefsFindString("screen");
	int width = 512, height = 384;
	scr_mode_bit = 0;
	display_type = DISPLAY_WINDOW;
	if (mode_str) {
		if (sscanf(mode_str, "win/%d/%d", &width, &height) == 2)
			display_type = DISPLAY_WINDOW;
		else if (sscanf(mode_str, "scr/%d", &scr_mode_bit) == 1)
			display_type = DISPLAY_SCREEN;
	}

	BMenuField *menu_field;
	BMenuItem *item;
	BPopUpMenu *menu;

	menu = new BPopUpMenu("");
	menu_field = new BMenuField(BRect(10, 5, right, 20), "videotype", GetString(STR_VIDEO_TYPE_CTRL), menu);
	menu_field->SetDivider(120);
	menu->AddItem(item = new BMenuItem(GetString(STR_WINDOW_LAB), new BMessage(MSG_VIDEO_WINDOW)));
	if (display_type == DISPLAY_WINDOW)
		item->SetMarked(true);
	menu->AddItem(item = new BMenuItem(GetString(STR_FULLSCREEN_LAB), new BMessage(MSG_VIDEO_SCREEN)));
	if (display_type == DISPLAY_SCREEN)
		item->SetMarked(true);
	pane->AddChild(menu_field);

	menu = new BPopUpMenu("");
	frameskip_menu = new BMenuField(BRect(10, 26, right, 41), "frameskip", GetString(STR_FRAMESKIP_CTRL), menu);
	frameskip_menu->SetDivider(120);
	menu->AddItem(new BMenuItem(GetString(STR_REF_5HZ_LAB), new BMessage(MSG_REF_5HZ)));
	menu->AddItem(new BMenuItem(GetString(STR_REF_7_5HZ_LAB), new BMessage(MSG_REF_7_5HZ)));
	menu->AddItem(new BMenuItem(GetString(STR_REF_10HZ_LAB), new BMessage(MSG_REF_10HZ)));
	menu->AddItem(new BMenuItem(GetString(STR_REF_15HZ_LAB), new BMessage(MSG_REF_15HZ)));
	menu->AddItem(new BMenuItem(GetString(STR_REF_30HZ_LAB), new BMessage(MSG_REF_30HZ)));
	pane->AddChild(frameskip_menu);
	int32 i32 = PrefsFindInt32("frameskip");
	if (i32 == 12) {
		if ((item = menu->FindItem(GetString(STR_REF_5HZ_LAB))) != NULL)
			item->SetMarked(true);
	} else if (i32 == 8) {
		if ((item = menu->FindItem(GetString(STR_REF_7_5HZ_LAB))) != NULL)
			item->SetMarked(true);
	} else if (i32 == 6) {
		if ((item = menu->FindItem(GetString(STR_REF_10HZ_LAB))) != NULL)
			item->SetMarked(true);
	} else if (i32 == 4) {
		if ((item = menu->FindItem(GetString(STR_REF_15HZ_LAB))) != NULL)
			item->SetMarked(true);
	} else if (i32 == 2) {
		if ((item = menu->FindItem(GetString(STR_REF_30HZ_LAB))) != NULL)
			item->SetMarked(true);
	}

	display_x_ctrl = new NumberControl(BRect(10, 48, right / 2, 66), 118, "width", GetString(STR_DISPLAY_X_CTRL), width, NULL);
	pane->AddChild(display_x_ctrl);
	display_y_ctrl = new NumberControl(BRect(10, 69, right / 2, 87), 118, "height", GetString(STR_DISPLAY_Y_CTRL), height, NULL);
	pane->AddChild(display_y_ctrl);

	menu = new BPopUpMenu("");
	scr_mode_menu = new BMenuField(BRect(10, 26, right, 41), "screenmode", GetString(STR_SCREEN_MODE_CTRL), menu);
	scr_mode_menu->SetDivider(120);
	for (int i=0; scr_mode[i].mode_mask; i++) {
		menu->AddItem(item = new BMenuItem(GetString(scr_mode[i].str), new BMessage(MSG_SCREEN_MODE + i)));
		if (scr_mode[i].mode_mask & (1 << scr_mode_bit))
			item->SetMarked(true);
	}
	pane->AddChild(scr_mode_menu);

	nosound_checkbox = new BCheckBox(BRect(10, 90, right, 105), "nosound", GetString(STR_NOSOUND_CTRL), new BMessage(MSG_NOSOUND));
	pane->AddChild(nosound_checkbox);
	nosound_checkbox->SetValue(PrefsFindBool("nosound") ? B_CONTROL_ON : B_CONTROL_OFF);

	hide_show_graphics_ctrls();
	return pane;
}


/*
 *  Create "Serial/Network" pane
 */

void PrefsWindow::hide_show_serial_ctrls(void)
{
	if (udptunnel_checkbox->Value() == B_CONTROL_ON) {
		ether_checkbox->SetEnabled(false);
		udpport_ctrl->SetEnabled(true);
	} else {
		ether_checkbox->SetEnabled(true);
		udpport_ctrl->SetEnabled(false);
	}
}

void PrefsWindow::read_serial_prefs(void)
{
	PrefsReplaceInt32("udpport", udpport_ctrl->Value());
}

void PrefsWindow::add_serial_names(BPopUpMenu *menu, uint32 msg)
{
	BSerialPort *port = new BSerialPort;
	char name[B_PATH_NAME_LENGTH];
	for (int i=0; i<port->CountDevices(); i++) {
		port->GetDeviceName(i, name);
		menu->AddItem(new BMenuItem(name, new BMessage(msg)));
	}
	if (sys_info.platform_type == B_BEBOX_PLATFORM) {
		BDirectory dir;
		BEntry entry;
		dir.SetTo("/dev/parallel");
		if (dir.InitCheck() == B_NO_ERROR) {
			dir.Rewind();
			while (dir.GetNextEntry(&entry) >= 0) {
				if (!entry.IsDirectory()) {
					entry.GetName(name);
					menu->AddItem(new BMenuItem(name, new BMessage(msg)));
				}
			}
		}
	}
	delete port;
}

static void set_serial_label(BPopUpMenu *menu, const char *prefs_name)
{
	const char *str;
	BMenuItem *item;
	if ((str = PrefsFindString(prefs_name)) != NULL)
		if ((item = menu->FindItem(str)) != NULL)
			item->SetMarked(true);
}

BView *PrefsWindow::create_serial_pane(void)
{
	BView *pane = new BView(BRect(0, 0, top_frame.right-20, top_frame.bottom-80), GetString(STR_SERIAL_NETWORK_PANE_TITLE), B_FOLLOW_NONE, B_WILL_DRAW);
	pane->SetViewColor(fill_color);
	float right = pane->Bounds().right-10;

	BMenuField *menu_field;
	BPopUpMenu *menu_a = new BPopUpMenu("");
	add_serial_names(menu_a, MSG_SER_A);
	menu_field = new BMenuField(BRect(10, 5, right, 20), "seriala", GetString(STR_SERIALA_CTRL), menu_a);
	menu_field->SetDivider(90);
	pane->AddChild(menu_field);
	set_serial_label(menu_a, "seriala");

	BPopUpMenu *menu_b = new BPopUpMenu("");
	add_serial_names(menu_b, MSG_SER_B);
	menu_field = new BMenuField(BRect(10, 26, right, 41), "serialb", GetString(STR_SERIALB_CTRL), menu_b);
	menu_field->SetDivider(90);
	pane->AddChild(menu_field);
	set_serial_label(menu_b, "serialb");

	ether_checkbox = new BCheckBox(BRect(10, 47, right, 62), "ether", GetString(STR_ETHER_ENABLE_CTRL), new BMessage(MSG_ETHER));
	pane->AddChild(ether_checkbox);
	ether_checkbox->SetValue(PrefsFindString("ether") ? B_CONTROL_ON : B_CONTROL_OFF);

	udptunnel_checkbox = new BCheckBox(BRect(10, 67, right, 72), "udptunnel", GetString(STR_UDPTUNNEL_CTRL), new BMessage(MSG_UDPTUNNEL));
	pane->AddChild(udptunnel_checkbox);
	udptunnel_checkbox->SetValue(PrefsFindBool("udptunnel") ? B_CONTROL_ON : B_CONTROL_OFF);

	udpport_ctrl = new NumberControl(BRect(10, 87, right / 2, 105), 118, "udpport", GetString(STR_UDPPORT_CTRL), PrefsFindInt32("udpport"), NULL);
	pane->AddChild(udpport_ctrl);

	hide_show_serial_ctrls();
	return pane;
}


/*
 *  Create "Memory" pane
 */

void PrefsWindow::read_memory_prefs(void)
{
	const char *str = rom_control->Text();
	if (strlen(str))
		PrefsReplaceString("rom", str);
	else
		PrefsRemoveItem("rom");
}

BView *PrefsWindow::create_memory_pane(void)
{
	char str[256], str2[256];
	BView *pane = new BView(BRect(0, 0, top_frame.right-20, top_frame.bottom-80), GetString(STR_MEMORY_MISC_PANE_TITLE), B_FOLLOW_NONE, B_WILL_DRAW);
	pane->SetViewColor(fill_color);
	float right = pane->Bounds().right-10;

	BEntry entry("/boot/var/swap");
	off_t swap_space;
	if (entry.GetSize(&swap_space) == B_NO_ERROR)
		max_ramsize = swap_space / (1024 * 1024) - 8;
	else
		max_ramsize = sys_info.max_pages * B_PAGE_SIZE / (1024 * 1024) - 8;

	int32 value = PrefsFindInt32("ramsize") / (1024 * 1024);

	ramsize_slider = new RAMSlider(BRect(10, 5, right, 55), "ramsize", GetString(STR_RAMSIZE_SLIDER), new BMessage(MSG_RAMSIZE), 1, max_ramsize, B_TRIANGLE_THUMB);
	ramsize_slider->SetValue(value);
	ramsize_slider->UseFillColor(true, &slider_fill_color);
	sprintf(str, GetString(STR_RAMSIZE_FMT), 1);
	sprintf(str2, GetString(STR_RAMSIZE_FMT), max_ramsize);
	ramsize_slider->SetLimitLabels(str, str2);
	pane->AddChild(ramsize_slider);

	BMenuField *menu_field;
	BMenuItem *item;
	BPopUpMenu *menu;

	int id = PrefsFindInt32("modelid");
	menu = new BPopUpMenu("");
	menu_field = new BMenuField(BRect(10, 60, right, 75), "modelid", GetString(STR_MODELID_CTRL), menu);
	menu_field->SetDivider(120);
	menu->AddItem(item = new BMenuItem(GetString(STR_MODELID_5_LAB), new BMessage(MSG_MODELID_5)));
	if (id == 5)
		item->SetMarked(true);
	menu->AddItem(item = new BMenuItem(GetString(STR_MODELID_14_LAB), new BMessage(MSG_MODELID_14)));
	if (id == 14)
		item->SetMarked(true);
	pane->AddChild(menu_field);

	int cpu = PrefsFindInt32("cpu");
	bool fpu = PrefsFindBool("fpu");
	menu = new BPopUpMenu("");
	menu_field = new BMenuField(BRect(10, 82, right, 97), "cpu", GetString(STR_CPU_CTRL), menu);
	menu_field->SetDivider(120);
	menu->AddItem(item = new BMenuItem(GetString(STR_CPU_68020_LAB), new BMessage(MSG_CPU_68020)));
	if (cpu == 2 && !fpu)
		item->SetMarked(true);
	menu->AddItem(item = new BMenuItem(GetString(STR_CPU_68020_FPU_LAB), new BMessage(MSG_CPU_68020_FPU)));
	if (cpu == 2 && fpu)
		item->SetMarked(true);
	menu->AddItem(item = new BMenuItem(GetString(STR_CPU_68030_LAB), new BMessage(MSG_CPU_68030)));
	if (cpu == 3 && !fpu)
		item->SetMarked(true);
	menu->AddItem(item = new BMenuItem(GetString(STR_CPU_68030_FPU_LAB), new BMessage(MSG_CPU_68030_FPU)));
	if (cpu == 3 && fpu)
		item->SetMarked(true);
	menu->AddItem(item = new BMenuItem(GetString(STR_CPU_68040_LAB), new BMessage(MSG_CPU_68040)));
	if (cpu == 4)
		item->SetMarked(true);
	pane->AddChild(menu_field);

	rom_control = new PathControl(false, BRect(10, 104, right, 119), "rom", GetString(STR_ROM_FILE_CTRL), PrefsFindString("rom"), NULL);
	rom_control->SetDivider(117);
	pane->AddChild(rom_control);

	return pane;
}


/*
 *  Message from controls/menus received
 */

void PrefsWindow::MessageReceived(BMessage *msg)
{
	switch (msg->what) {
		case MSG_OK: {				// "Start" button clicked
			read_volumes_prefs();
			read_memory_prefs();
			read_graphics_prefs();
			SavePrefs();
			send_quit_on_close = false;
			PostMessage(B_QUIT_REQUESTED);
			be_app->PostMessage(ok_message);
			break;
		}

		case MSG_CANCEL:			// "Quit" button clicked
			send_quit_on_close = false;
			PostMessage(B_QUIT_REQUESTED);
			be_app->PostMessage(B_QUIT_REQUESTED);
			break;

		case B_ABOUT_REQUESTED: {	// "About" menu item selected
			ShowAboutWindow();
			break;
		}

		case MSG_ZAP_PRAM:			// "Zap PRAM File" menu item selected
			ZapPRAM();
			break;

		case MSG_VOLUME_INVOKED: {	// Double-clicked on volume name, toggle read-only flag
			int selected = volume_list->CurrentSelection();
			if (selected >= 0) {
				const char *str = PrefsFindString("disk", selected);
				BStringItem *item = (BStringItem *)volume_list->RemoveItem(selected);
				delete item;
				char newstr[256];
				if (str[0] == '*')
					strcpy(newstr, str+1);
				else {
					strcpy(newstr, "*");
					strcat(newstr, str);
				}
				PrefsReplaceString("disk", newstr, selected);
				volume_list->AddItem(new BStringItem(newstr), selected);
				volume_list->Select(selected);
			}
			break;
		}

		case MSG_ADD_VOLUME:
			add_volume_panel->Show();
			break;

		case MSG_CREATE_VOLUME:
			create_volume_panel->Show();
			break;

		case MSG_ADD_VOLUME_PANEL: {
			entry_ref ref;
			if (msg->FindRef("refs", &ref) == B_NO_ERROR) {
				BEntry entry(&ref, true);
				BPath path;
				entry.GetPath(&path);
				if (entry.IsFile()) {
					PrefsAddString("disk", path.Path());
					volume_list->AddItem(new BStringItem(path.Path()));
				} else if (entry.IsDirectory()) {
					BVolume volume;
					if (path.Path()[0] == '/' && strchr(path.Path()+1, '/') == NULL && entry.GetVolume(&volume) == B_NO_ERROR) {
						int32 i = 0;
						dev_t d;
						fs_info info;
						while ((d = next_dev(&i)) >= 0) {
							fs_stat_dev(d, &info);
							if (volume.Device() == info.dev) {
								PrefsAddString("disk", info.device_name);
								volume_list->AddItem(new BStringItem(info.device_name));
							}
						}
					}
				}
			}
			break;
		}

		case MSG_CREATE_VOLUME_PANEL: {
			entry_ref dir;
			if (msg->FindRef("directory", &dir) == B_NO_ERROR) {
				BEntry entry(&dir, true);
				BPath path;
				entry.GetPath(&path);
				path.Append(msg->FindString("name"));

				create_volume_panel->Window()->Lock();
				BView *background = create_volume_panel->Window()->ChildAt(0);
				NumberControl *v = (NumberControl *)background->FindView("hardfile_size");
				int size = v->Value();

				char cmd[1024];
				sprintf(cmd, "dd if=/dev/zero \"of=%s\" bs=1024k count=%d", path.Path(), size);
				int ret = system(cmd);
				if (ret == 0) {
					PrefsAddString("disk", path.Path());
					volume_list->AddItem(new BStringItem(path.Path()));
				} else {
					sprintf(cmd, GetString(STR_CREATE_VOLUME_WARN), strerror(ret));
					WarningAlert(cmd);
				}
			}
			break;
		}

		case MSG_REMOVE_VOLUME: {
			int selected = volume_list->CurrentSelection();
			if (selected >= 0) {
				PrefsRemoveItem("disk", selected);
				BStringItem *item = (BStringItem *)volume_list->RemoveItem(selected);
				delete item;
				volume_list->Select(selected);
			}
			break;
		}

		case MSG_BOOT_ANY:
			PrefsReplaceInt32("bootdriver", 0);
			break;

		case MSG_BOOT_CDROM:
			PrefsReplaceInt32("bootdriver", CDROMRefNum);
			break;

		case MSG_NOCDROM:
			PrefsReplaceBool("nocdrom", nocdrom_checkbox->Value() == B_CONTROL_ON);
			break;

		case MSG_VIDEO_WINDOW:
			display_type = DISPLAY_WINDOW;
			hide_show_graphics_ctrls();
			break;

		case MSG_VIDEO_SCREEN:
			display_type = DISPLAY_SCREEN;
			hide_show_graphics_ctrls();
			break;

		case MSG_REF_5HZ:
			PrefsReplaceInt32("frameskip", 12);
			break;

		case MSG_REF_7_5HZ:
			PrefsReplaceInt32("frameskip", 8);
			break;

		case MSG_REF_10HZ:
			PrefsReplaceInt32("frameskip", 6);
			break;

		case MSG_REF_15HZ:
			PrefsReplaceInt32("frameskip", 4);
			break;

		case MSG_REF_30HZ:
			PrefsReplaceInt32("frameskip", 2);
			break;

		case MSG_NOSOUND:
			PrefsReplaceBool("nosound", nosound_checkbox->Value() == B_CONTROL_ON);
			break;

		case MSG_SER_A: {
			BMenuItem *source = NULL;
			msg->FindPointer("source", (void **)&source);
			if (source)
				PrefsReplaceString("seriala", source->Label());
			break;
		}

		case MSG_SER_B: {
			BMenuItem *source = NULL;
			msg->FindPointer("source", (void **)&source);
			if (source)
				PrefsReplaceString("serialb", source->Label());
			break;
		}

		case MSG_ETHER:
			if (ether_checkbox->Value() == B_CONTROL_ON)
				PrefsReplaceString("ether", "yes");
			else
				PrefsRemoveItem("ether");
			break;

		case MSG_UDPTUNNEL:
			PrefsReplaceBool("udptunnel", udptunnel_checkbox->Value() == B_CONTROL_ON);
			hide_show_serial_ctrls();
			break;

		case MSG_RAMSIZE:
			PrefsReplaceInt32("ramsize", ramsize_slider->Value() * 1024 * 1024);
			break;

		case MSG_MODELID_5:
			PrefsReplaceInt32("modelid", 5);
			break;

		case MSG_MODELID_14:
			PrefsReplaceInt32("modelid", 14);
			break;

		case MSG_CPU_68020:
			PrefsReplaceInt32("cpu", 2);
			PrefsReplaceBool("fpu", false);
			break;

		case MSG_CPU_68020_FPU:
			PrefsReplaceInt32("cpu", 2);
			PrefsReplaceBool("fpu", true);
			break;

		case MSG_CPU_68030:
			PrefsReplaceInt32("cpu", 3);
			PrefsReplaceBool("fpu", false);
			break;

		case MSG_CPU_68030_FPU:
			PrefsReplaceInt32("cpu", 3);
			PrefsReplaceBool("fpu", true);
			break;

		case MSG_CPU_68040:
			PrefsReplaceInt32("cpu", 4);
			PrefsReplaceBool("fpu", true);
			break;

		default: {
			// Screen mode messages
			if ((msg->what & 0xffff0000) == MSG_SCREEN_MODE) {
				int m = msg->what & 0xffff;
				uint32 mask = scr_mode[m].mode_mask;
				for (int i=0; i<32; i++)
					if (mask & (1 << i))
						scr_mode_bit = i;
			} else
				BWindow::MessageReceived(msg);
		}
	}
}
