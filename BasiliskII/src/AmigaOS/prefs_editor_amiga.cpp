/*
 *  prefs_editor_amiga.cpp - Preferences editor, AmigaOS implementation (using gtlayout.library)
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

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <intuition/intuition.h>
#include <libraries/asl.h>
#include <libraries/gtlayout.h>
#include <libraries/Picasso96.h>
#include <cybergraphics/cybergraphics.h>
#include <graphics/displayinfo.h>
#include <devices/ahi.h>
#define __USE_SYSBASE
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/gtlayout.h>
#include <proto/graphics.h>
#include <proto/asl.h>
#include <proto/Picasso96.h>
#include <proto/cybergraphics.h>
#include <proto/ahi.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/intuition.h>
#include <inline/gadtools.h>
#include <inline/gtlayout.h>
#include <inline/graphics.h>
#include <inline/asl.h>
#include <inline/Picasso96.h>
#include <inline/cybergraphics.h>
#include <inline/cybergraphics.h>
#include <inline/ahi.h>
#include <clib/alib_protos.h>

#include "sysdeps.h"
#include "main.h"
#include "xpram.h"
#include "cdrom.h"
#include "user_strings.h"
#include "version.h"
#include "prefs.h"
#include "prefs_editor.h"


// Gadget/menu IDs
const int MSG_OK = 0x0100;					// "Start" button
const int MSG_CANCEL = 0x0101;				// "Quit" button
const int MSG_ABOUT = 0x0102;				// "About..." menu item
const int MSG_ZAP_PRAM = 0x0103;			// "Zap PRAM" menu item

const int GAD_PAGEGROUP = 0x0200;

const int GAD_DISK_LIST = 0x0300;			// "Volumes" pane
const int GAD_ADD_VOLUME = 0x0301;
const int GAD_EDIT_VOLUME = 0x0302;
const int GAD_REMOVE_VOLUME = 0x0303;
const int GAD_CDROM_DEVICE = 0x0304;
const int GAD_CDROM_UNIT = 0x0305;
const int GAD_BOOTDRIVER = 0x0306;
const int GAD_NOCDROM = 0x0307;
const int GAD_EXTFS = 0x0308;

const int GAD_VOLUME_READONLY = 0x0310;		// "Add/Edit Volume" window
const int GAD_VOLUME_TYPE = 0x0311;
const int GAD_VOLUME_FILE = 0x0312;
const int GAD_VOLUME_DEVICE = 0x0313;
const int GAD_VOLUME_UNIT = 0x0314;
const int GAD_VOLUME_OPENFLAGS = 0x0315;
const int GAD_VOLUME_STARTBLOCK = 0x0316;
const int GAD_VOLUME_SIZE = 0x0317;
const int GAD_VOLUME_BLOCKSIZE = 0x0318;
const int GAD_VOLUME_PAGEGROUP = 0x0319;

const int GAD_SCSI0_DEVICE = 0x0400;		// "SCSI" pane
const int GAD_SCSI1_DEVICE = 0x0401;
const int GAD_SCSI2_DEVICE = 0x0402;
const int GAD_SCSI3_DEVICE = 0x0403;
const int GAD_SCSI4_DEVICE = 0x0404;
const int GAD_SCSI5_DEVICE = 0x0405;
const int GAD_SCSI6_DEVICE = 0x0406;
const int GAD_SCSI0_UNIT = 0x0410;
const int GAD_SCSI1_UNIT = 0x0411;
const int GAD_SCSI2_UNIT = 0x0412;
const int GAD_SCSI3_UNIT = 0x0413;
const int GAD_SCSI4_UNIT = 0x0414;
const int GAD_SCSI5_UNIT = 0x0415;
const int GAD_SCSI6_UNIT = 0x0416;
const int GAD_SCSI_MEMTYPE = 0x0420;

const int GAD_VIDEO_TYPE = 0x0500;			// "Graphics/Sound" pane
const int GAD_DISPLAY_X = 0x0501;
const int GAD_DISPLAY_Y = 0x0502;
const int GAD_FRAMESKIP = 0x0503;
const int GAD_SCREEN_MODE = 0x0504;
const int GAD_AHI_MODE = 0x0505;
const int GAD_NOSOUND = 0x0506;

const int GAD_SERIALA_DEVICE = 0x0600;		// "Serial/Network" pane
const int GAD_SERIALA_UNIT = 0x0601;
const int GAD_SERIALA_ISPAR = 0x0602;
const int GAD_SERIALB_DEVICE = 0x0603;
const int GAD_SERIALB_UNIT = 0x0604;
const int GAD_SERIALB_ISPAR = 0x0605;
const int GAD_ETHER_DEVICE = 0x0606;
const int GAD_ETHER_UNIT = 0x00607;

const int GAD_RAMSIZE = 0x0700;				// "Memory/Misc" pane
const int GAD_MODELID = 0x0701;
const int GAD_ROM_FILE = 0x0702;


// Global variables
struct Library *GTLayoutBase = NULL;
static struct FileRequester *dev_request = NULL, *file_request = NULL;

// gtlayout.library macros
#define VGROUP LT_New(h, LA_Type, VERTICAL_KIND, TAG_END)
#define HGROUP LT_New(h, LA_Type, HORIZONTAL_KIND, TAG_END)
#define ENDGROUP LT_EndGroup(h)

// Prototypes
static void create_volumes_pane(struct LayoutHandle *h);
static void create_scsi_pane(struct LayoutHandle *h);
static void create_graphics_pane(struct LayoutHandle *h);
static void create_serial_pane(struct LayoutHandle *h);
static void create_memory_pane(struct LayoutHandle *h);
static void add_edit_volume(struct LayoutHandle *h, bool adding);
static void remove_volume(struct LayoutHandle *h);
static void ghost_volumes_gadgets(struct LayoutHandle *h);
static void ghost_graphics_gadgets(struct LayoutHandle *h);
static void screen_mode_req(struct Window *win, struct LayoutHandle *h);
static void ahi_mode_req(struct Window *win, struct LayoutHandle *h);
static void read_settings(struct LayoutHandle *h);


/*
 *  Locale hook - returns string for given ID
 */

static __saveds __attribute__((regparm(3))) const char *locale_hook_func(struct Hook *hook /*a0*/, void *id /*a1*/, struct LayoutHandle *h /*a2*/)
{
	return GetString((uint32)id);
}

struct Hook locale_hook = {{NULL, NULL}, (HOOKFUNC)locale_hook_func, NULL, NULL};


/*
 *  Show preferences editor
 *  Returns true when user clicked on "Start", false otherwise
 */

bool PrefsEditor(void)
{
	bool retval = true, done = false;
	struct LayoutHandle *h = NULL;
	struct Window *win = NULL;
	struct Menu *menu = NULL;

	// Pane tabs
	static const LONG labels[] = {
		STR_VOLUMES_PANE_TITLE,
		STR_SCSI_PANE_TITLE,
		STR_GRAPHICS_SOUND_PANE_TITLE,
		STR_SERIAL_NETWORK_PANE_TITLE,
		STR_MEMORY_MISC_PANE_TITLE,
		-1
	};

	// Open gtlayout.library
	GTLayoutBase = (struct Library *)OpenLibrary("gtlayout.library", 39);
	if (GTLayoutBase == NULL) {
		WarningAlert(GetString(STR_NO_GTLAYOUT_LIB_WARN));
		return true;
	}

	// Create layout handle
	h = LT_CreateHandleTags(NULL,
		LAHN_AutoActivate, FALSE,
		LAHN_LocaleHook, (ULONG)&locale_hook,
		TAG_END
	);
	if (h == NULL)
		goto quit;

	// Create menus
	menu = LT_NewMenuTags(
		LAMN_LayoutHandle, (ULONG)h,
		LAMN_TitleID, STR_PREFS_MENU,
		LAMN_ItemID, STR_PREFS_ITEM_ABOUT,
		LAMN_UserData, MSG_ABOUT,
		LAMN_ItemText, (ULONG)NM_BARLABEL,
		LAMN_ItemID, STR_PREFS_ITEM_START,
		LAMN_UserData, MSG_OK,
		LAMN_ItemID, STR_PREFS_ITEM_ZAP_PRAM,
		LAMN_UserData, MSG_ZAP_PRAM,
		LAMN_ItemText, (ULONG)NM_BARLABEL,
		LAMN_ItemID, STR_PREFS_ITEM_QUIT,
		LAMN_UserData, MSG_CANCEL,
		LAMN_KeyText, (ULONG)"Q",
		TAG_END
	);

	// Create window contents
	VGROUP;
		VGROUP;
			LT_New(h, LA_Type, TAB_KIND,
				LATB_LabelTable, (ULONG)labels,
				LATB_AutoPageID, GAD_PAGEGROUP,
				LATB_FullWidth, TRUE,
				TAG_END
			);
		ENDGROUP;

		// Panes
		LT_New(h, LA_Type, VERTICAL_KIND,
			LA_ID, GAD_PAGEGROUP,
			LAGR_ActivePage, 0,
			TAG_END
		);
			create_volumes_pane(h);
			create_scsi_pane(h);
			create_graphics_pane(h);
			create_serial_pane(h);
			create_memory_pane(h);
		ENDGROUP;

		// Separator between tabs and buttons
		VGROUP;
			LT_New(h, LA_Type, XBAR_KIND,
				LAXB_FullSize, TRUE,
				TAG_END
			);
		ENDGROUP;

		// "Start" and "Quit" buttons
		LT_New(h, LA_Type, HORIZONTAL_KIND,
			LAGR_SameSize, TRUE,
			LAGR_Spread, TRUE,
			TAG_END
		);
			LT_New(h, LA_Type, BUTTON_KIND,
				LA_LabelID, STR_START_BUTTON,
				LA_ID, MSG_OK,
				LABT_ReturnKey, TRUE,
				TAG_END
			);
			LT_New(h, LA_Type, BUTTON_KIND,
				LA_LabelID, STR_QUIT_BUTTON,
				LA_ID, MSG_CANCEL,
				LABT_EscKey, TRUE,
				TAG_END
			);
		ENDGROUP;
	ENDGROUP;

	// Open window
	win = LT_Build(h,
		LAWN_TitleID, STR_PREFS_TITLE,
		LAWN_Menu, (ULONG)menu,
		LAWN_IDCMP, IDCMP_CLOSEWINDOW,
		LAWN_BelowMouse, TRUE,
		LAWN_SmartZoom, TRUE,
		WA_SimpleRefresh, TRUE,
		WA_Activate, TRUE,
		WA_CloseGadget, TRUE,
		WA_DepthGadget, TRUE,
		WA_DragBar, TRUE,
		TAG_END
	);
	if (win == NULL)
		goto quit;

	// Create file requesters
	dev_request = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
		ASLFR_DoPatterns, TRUE,
		ASLFR_RejectIcons, TRUE,
		ASLFR_InitialDrawer, (ULONG)"DEVS:",
		ASLFR_InitialPattern, (ULONG)"#?.device",
		TAG_END
	);
	file_request = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest,
		ASLFR_DoPatterns, TRUE,
		ASLFR_RejectIcons, TRUE,
		ASLFR_InitialPattern, (ULONG)"#?",
		TAG_END
	);

	// Event loop
	do {
		struct IntuiMessage *msg;

		// Wait for message
		WaitPort(win->UserPort);

		// Get pending messages
		while (msg = LT_GetIMsg(h)) {

			// Get data from message and reply
			ULONG cl = msg->Class;
			UWORD code = msg->Code;
			struct Gadget *gad = (struct Gadget *)msg->IAddress;
			LT_ReplyIMsg(msg);

			// Handle message according to class
			switch (cl) {
				case IDCMP_CLOSEWINDOW:
					retval = false;
					done = true;
					break;

				case IDCMP_GADGETUP:
					switch (gad->GadgetID) {
						case MSG_OK:
							read_settings(h);
							SavePrefs();
							retval = true;
							done = true;
							break;

						case MSG_CANCEL:
							retval = false;
							done = true;
							break;

						case GAD_DISK_LIST:
							ghost_volumes_gadgets(h);
							break;

						case GAD_ADD_VOLUME:
							LT_LockWindow(win);
							add_edit_volume(h, true);
							LT_UnlockWindow(win);
							break;

						case GAD_EDIT_VOLUME:
							LT_LockWindow(win);
							add_edit_volume(h, false);
							LT_UnlockWindow(win);
							break;

						case GAD_REMOVE_VOLUME:
							remove_volume(h);
							break;

						case GAD_BOOTDRIVER:
							switch (code) {
								case 0:
									PrefsReplaceInt32("bootdriver", 0);
									break;
								case 1:
									PrefsReplaceInt32("bootdriver", CDROMRefNum);
									break;
							}
							break;

						case GAD_SCSI_MEMTYPE:
							PrefsReplaceInt32("scsimemtype", code);
							break;

						case GAD_VIDEO_TYPE:
							ghost_graphics_gadgets(h);
							break;

						case GAD_FRAMESKIP:
							switch (code) {
								case 0:
									PrefsReplaceInt32("frameskip", 12);
									break;
								case 1:
									PrefsReplaceInt32("frameskip", 8);
									break;
								case 2:
									PrefsReplaceInt32("frameskip", 6);
									break;
								case 3:
									PrefsReplaceInt32("frameskip", 4);
									break;
								case 4:
									PrefsReplaceInt32("frameskip", 2);
									break;
								case 5:
									PrefsReplaceInt32("frameskip", 1);
									break;
							}
							break;

						case GAD_MODELID:
							switch (code) {
								case 0:
									PrefsReplaceInt32("modelid", 5);
									break;
								case 1:
									PrefsReplaceInt32("modelid", 14);
									break;
							}
							break;
					}
					break;

				case IDCMP_IDCMPUPDATE:
					switch (gad->GadgetID) {
						case GAD_DISK_LIST:	// Double-click on volumes list = edit volume
							LT_LockWindow(win);
							add_edit_volume(h, false);
							LT_UnlockWindow(win);
							break;

						case GAD_SCREEN_MODE:
							screen_mode_req(win, h);
							break;

						case GAD_AHI_MODE:
							ahi_mode_req(win, h);
							break;

						case GAD_CDROM_DEVICE:
						case GAD_SCSI0_DEVICE:
						case GAD_SCSI1_DEVICE:
						case GAD_SCSI2_DEVICE:
						case GAD_SCSI3_DEVICE:
						case GAD_SCSI4_DEVICE:
						case GAD_SCSI5_DEVICE:
						case GAD_SCSI6_DEVICE:
						case GAD_SERIALA_DEVICE:
						case GAD_SERIALB_DEVICE:
							if (dev_request) {
								LT_LockWindow(win);
								BOOL result = AslRequestTags(dev_request,
									ASLFR_Window, (ULONG)win, 
									ASLFR_InitialDrawer, (ULONG) "Devs:",
									TAG_END);
								LT_UnlockWindow(win);
								if (result) {
									char *str;
									GT_GetGadgetAttrs(gad, win, NULL, GTST_String, (ULONG)&str, TAG_END);
									strncpy(str, dev_request->rf_File, 255);	// Don't copy the directory part. This is usually "DEVS:" and we don't need that.
									str[255] = 0;
									LT_SetAttributes(h, gad->GadgetID, GTST_String, (ULONG)str, TAG_END);
								}
							}
							break;

						case GAD_ETHER_DEVICE:
							if (dev_request) {
								LT_LockWindow(win);
								BOOL result = AslRequestTags(dev_request,
									ASLFR_Window, (ULONG)win, 
									ASLFR_InitialDrawer, (ULONG) "Devs:Networks",
									TAG_END);
								LT_UnlockWindow(win);
								if (result) {
									char *str;
									GT_GetGadgetAttrs(gad, win, NULL, GTST_String, (ULONG)&str, TAG_END);
									strncpy(str, dev_request->rf_File, 255);	// Don't copy the directory part. This is usually "DEVS:" and we don't need that.
									str[255] = 0;
									LT_SetAttributes(h, gad->GadgetID, GTST_String, (ULONG)str, TAG_END);
								}
							}
							break;

						case GAD_ROM_FILE:
							if (file_request) {
								LT_LockWindow(win);
								BOOL result = AslRequestTags(file_request, ASLFR_Window, (ULONG)win, TAG_END);
								LT_UnlockWindow(win);
								if (result) {
									char *str;
									GT_GetGadgetAttrs(gad, win, NULL, GTST_String, (ULONG)&str, TAG_END);
									strncpy(str, file_request->rf_Dir, 255);
									str[255] = 0;
									AddPart(str, file_request->rf_File, 255);
									LT_SetAttributes(h, gad->GadgetID, GTST_String, (ULONG)str, TAG_END);
								}
							}
							break;
					}
					break;

				case IDCMP_MENUPICK:
					while (code != MENUNULL) {
						struct MenuItem *item = ItemAddress(menu, code);
						if (item == NULL)
							break;
						switch ((ULONG)GTMENUITEM_USERDATA(item)) {
							case MSG_OK:
								read_settings(h);
								SavePrefs();
								retval = true;
								done = true;
								break;

							case MSG_CANCEL:
								retval = false;
								done = true;
								break;

							case MSG_ABOUT: {
								char str[256];
								sprintf(str, GetString(STR_ABOUT_TEXT1), VERSION_MAJOR, VERSION_MINOR);
								strncat(str, "\n", 255);
								strncat(str, GetString(STR_ABOUT_TEXT2), 255);

								EasyStruct req;
								req.es_StructSize = sizeof(EasyStruct);
								req.es_Flags = 0;
								req.es_Title = (UBYTE *)GetString(STR_ABOUT_TITLE);
								req.es_TextFormat = (UBYTE *)str;
								req.es_GadgetFormat = (UBYTE *)GetString(STR_OK_BUTTON);
								LT_LockWindow(win);
								EasyRequest(win, &req, NULL);
								LT_UnlockWindow(win);
								break;
							}

							case MSG_ZAP_PRAM:
								ZapPRAM();
								break;
						}
						code = item->NextSelect;
					}
					break;
			}
		}
	} while (!done);

quit:
	// Free requesters
	FreeAslRequest(dev_request);
	FreeAslRequest(file_request);

	// Delete Menus
	LT_DisposeMenu(menu);

	// Delete handle
	LT_DeleteHandle(h);

	// Close gtlayout.library
	CloseLibrary(GTLayoutBase);
	return retval;
}


/*
 *  "Volumes" pane
 */

static struct List disk_list;
static char cdrom_name[256], extfs_name[256];
static ULONG cdrom_unit, cdrom_flags, cdrom_start, cdrom_size, cdrom_bsize;
static BYTE bootdriver_num, nocdrom;

// Read volumes preferences
static void parse_volumes_prefs(void)
{
	NewList(&disk_list);
	const char *str;
	for (int i=0; (str = PrefsFindString("disk", i)) != NULL; i++) {
		struct Node *item = (struct Node *)AllocMem(sizeof(struct Node), MEMF_CLEAR);
		item->ln_Name = (char *)str;
		AddTail(&disk_list, item);
	}

	cdrom_name[0] = 0;
	cdrom_unit = 0; cdrom_flags = 0; cdrom_start = 0; cdrom_size = 0; cdrom_bsize = 2048;

	str = PrefsFindString("cdrom");
	if (str)
		sscanf(str, "/dev/%[^/]/%ld/%ld/%ld/%ld/%ld", cdrom_name, &cdrom_unit, &cdrom_flags, &cdrom_start, &cdrom_size, &cdrom_bsize);

	bootdriver_num = 0;

	int bootdriver = PrefsFindInt32("bootdriver");
	switch (bootdriver) {
		case 0:
			bootdriver_num = 0;
			break;
		case CDROMRefNum:
			bootdriver_num = 1;
			break;
	}

	nocdrom = PrefsFindBool("nocdrom");

	extfs_name[0] = 0;
	str = PrefsFindString("extfs");
	if (str)
		strncpy(extfs_name, str, sizeof(extfs_name) - 1);
}

// Ghost/unghost "Edit" and "Remove" buttons
static void ghost_volumes_gadgets(struct LayoutHandle *h)
{
	UWORD sel = LT_GetAttributes(h, GAD_DISK_LIST, TAG_END);
	if (sel == 0xffff) {
		LT_SetAttributes(h, GAD_EDIT_VOLUME, GA_Disabled, TRUE, TAG_END);
		LT_SetAttributes(h, GAD_REMOVE_VOLUME, GA_Disabled, TRUE, TAG_END);
	} else {
		LT_SetAttributes(h, GAD_EDIT_VOLUME, GA_Disabled, FALSE, TAG_END);
		LT_SetAttributes(h, GAD_REMOVE_VOLUME, GA_Disabled, FALSE, TAG_END);
	}
}

// Get device data from partition name
static void analyze_partition(const char *part, char *dev_name, ULONG &dev_unit, ULONG &dev_flags, ULONG &dev_start, ULONG &dev_size, ULONG &dev_bsize)
{
	// Remove everything after and including the ':'
	char str[256];
	strncpy(str, part, sizeof(str) - 1);
	str[sizeof(str) - 1] = 0;
	char *colon = strchr(str, ':');
	if (colon)
		*colon = 0;

	// Look for partition
	struct DosList *dl = LockDosList(LDF_DEVICES | LDF_READ);
	dl = FindDosEntry(dl, str, LDF_DEVICES);
	if (dl) {
		// Get File System Startup Message
		struct FileSysStartupMsg *fssm = (struct FileSysStartupMsg *)(dl->dol_misc.dol_handler.dol_Startup << 2);
		if (fssm) {
			// Get DOS environment vector
			struct DosEnvec *de = (struct DosEnvec *)(fssm->fssm_Environ << 2);
			if (de && de->de_TableSize >= DE_UPPERCYL) {
				// Read settings from FSSM and Envec
				strncpy(dev_name, (char *)(fssm->fssm_Device << 2) + 1, 255);
				dev_name[255] = 0;
				dev_unit = fssm->fssm_Unit;
				dev_flags = fssm->fssm_Flags;
				dev_start = de->de_BlocksPerTrack * de->de_Surfaces * de->de_LowCyl;
				dev_size = de->de_BlocksPerTrack * de->de_Surfaces * (de->de_HighCyl - de->de_LowCyl + 1);
				dev_bsize = de->de_SizeBlock << 2;
			}
		}
	}
	UnLockDosList(LDF_DEVICES | LDF_READ);
}

// Display and handle "Add/Edit Volume" window
static void add_edit_volume(struct LayoutHandle *h2, bool adding)
{
	bool ok_clicked = false;

	UWORD sel = LT_GetAttributes(h2, GAD_DISK_LIST, TAG_END);
	if ((sel == 0xffff) && !adding)
		return;

	char dev_name[256] = "";
	char file_name[256] = "";
	ULONG dev_unit = 0, dev_flags = 0, dev_start = 0, dev_size = 0, dev_bsize = 512;
	BYTE read_only = false, is_device = false;

	if (!adding) {
		const char *str = PrefsFindString("disk", sel);
		if (str == NULL)
			return;
		if (str[0] == '*') {
			read_only = true;
			str++;
		}
		if (strstr(str, "/dev/") == str) {
			is_device = true;
			sscanf(str, "/dev/%[^/]/%ld/%ld/%ld/%ld/%ld", dev_name, &dev_unit, &dev_flags, &dev_start, &dev_size, &dev_bsize);
		} else {
			strncpy(file_name, str, sizeof(file_name) - 1);
			file_name[sizeof(file_name) - 1] = 0;
		}
	}

	// Create layout handle
	struct LayoutHandle *h = NULL;
	struct Window *win = NULL;
	h = LT_CreateHandleTags(NULL,
		LAHN_AutoActivate, FALSE,
		LAHN_LocaleHook, (ULONG)&locale_hook,
		TAG_END
	);
	if (h == NULL)
		return;

	// Create window contents
	VGROUP;
		// Volume gadgets
		VGROUP;
			LT_New(h, LA_Type, CHECKBOX_KIND,
				LA_LabelID, STR_VOL_READONLY_CTRL,
				LA_ID, GAD_VOLUME_READONLY,
				LA_BYTE, (ULONG)&read_only,
				TAG_END
			);
			LT_New(h, LA_Type, CYCLE_KIND,
				LA_LabelID, STR_VOL_TYPE_CTRL,
				LA_ID, GAD_VOLUME_TYPE,
				LACY_AutoPageID, GAD_VOLUME_PAGEGROUP,
				LACY_FirstLabel, STR_VOL_FILE_LAB,
				LACY_LastLabel, STR_VOL_DEVICE_LAB,
				LA_BYTE, (ULONG)&is_device,
				TAG_END
			);
		ENDGROUP;
		LT_New(h, LA_Type, VERTICAL_KIND,
			LA_ID, GAD_VOLUME_PAGEGROUP,
			LAGR_ActivePage, is_device,
			TAG_END
		);
			VGROUP;
				LT_New(h, LA_Type, STRING_KIND,
					LA_LabelID, STR_VOL_FILE_CTRL,
					LA_ID, GAD_VOLUME_FILE,
					LA_Chars, 20,
					LA_STRPTR, (ULONG)file_name,
					GTST_MaxChars, sizeof(file_name) - 1,
					LAST_Picker, TRUE,
					TAG_END
				);
			ENDGROUP;
			VGROUP;
				LT_New(h, LA_Type, STRING_KIND,
					LA_LabelID, STR_DEVICE_CTRL,
					LA_ID, GAD_VOLUME_DEVICE,
					LA_Chars, 20,
					LA_STRPTR, (ULONG)dev_name,
					GTST_MaxChars, sizeof(dev_name) - 1,
					LAST_Picker, TRUE,
					TAG_END
				);
				LT_New(h, LA_Type, INTEGER_KIND,
					LA_LabelID, STR_UNIT_CTRL,
					LA_ID, GAD_VOLUME_UNIT,
					LA_LONG, (ULONG)&dev_unit,
					LAIN_UseIncrementers, TRUE,
					GTIN_MaxChars, 8,
					TAG_END
				);
				LT_New(h, LA_Type, INTEGER_KIND,
					LA_LabelID, STR_VOL_OPENFLAGS_CTRL,
					LA_ID, GAD_VOLUME_OPENFLAGS,
					LA_LONG, (ULONG)&dev_flags,
					LAIN_UseIncrementers, TRUE,
					GTIN_MaxChars, 8,
					TAG_END
				);
				LT_New(h, LA_Type, INTEGER_KIND,
					LA_LabelID, STR_VOL_STARTBLOCK_CTRL,
					LA_ID, GAD_VOLUME_STARTBLOCK,
					LA_LONG, (ULONG)&dev_start,
					LAIN_UseIncrementers, TRUE,
					GTIN_MaxChars, 8,
					TAG_END
				);
				LT_New(h, LA_Type, INTEGER_KIND,
					LA_LabelID, STR_VOL_SIZE_CTRL,
					LA_ID, GAD_VOLUME_SIZE,
					LA_LONG, (ULONG)&dev_size,
					LAIN_UseIncrementers, TRUE,
					GTIN_MaxChars, 8,
					TAG_END
				);
				LT_New(h, LA_Type, INTEGER_KIND,
					LA_LabelID, STR_VOL_BLOCKSIZE_CTRL,
					LA_ID, GAD_VOLUME_BLOCKSIZE,
					LA_LONG, (ULONG)&dev_bsize,
					LAIN_UseIncrementers, TRUE,
					GTIN_MaxChars, 8,
					TAG_END
				);
			ENDGROUP;
		ENDGROUP;

		// Separator between gadgets and buttons
		VGROUP;
			LT_New(h, LA_Type, XBAR_KIND,
				LAXB_FullSize, TRUE,
				TAG_END
			);
		ENDGROUP;

		// "OK" and "Cancel" buttons
		LT_New(h, LA_Type, HORIZONTAL_KIND,
			LAGR_SameSize, TRUE,
			LAGR_Spread, TRUE,
			TAG_END
		);
			LT_New(h, LA_Type, BUTTON_KIND,
				LA_LabelID, STR_OK_BUTTON,
				LA_ID, MSG_OK,
				LABT_ReturnKey, TRUE,
				TAG_END
			);
			LT_New(h, LA_Type, BUTTON_KIND,
				LA_LabelID, STR_CANCEL_BUTTON,
				LA_ID, MSG_CANCEL,
				LABT_EscKey, TRUE,
				TAG_END
			);
		ENDGROUP;
	ENDGROUP;

	// Open window
	win = LT_Build(h,
		LAWN_TitleID, adding ? STR_ADD_VOLUME_TITLE : STR_EDIT_VOLUME_TITLE,
		LAWN_IDCMP, IDCMP_CLOSEWINDOW,
		LAWN_BelowMouse, TRUE,
		LAWN_SmartZoom, TRUE,
		WA_SimpleRefresh, TRUE,
		WA_Activate, TRUE,
		WA_CloseGadget, TRUE,
		WA_DepthGadget, TRUE,
		WA_DragBar, TRUE,
		TAG_END
	);
	if (win == NULL) {
		LT_DeleteHandle(h);
		return;
	}

	// Event loop
	bool done = false;
	do {
		struct IntuiMessage *msg;

		// Wait for message
		WaitPort(win->UserPort);

		// Get pending messages
		while (msg = LT_GetIMsg(h)) {

			// Get data from message and reply
			ULONG cl = msg->Class;
			UWORD code = msg->Code;
			struct Gadget *gad = (struct Gadget *)msg->IAddress;
			LT_ReplyIMsg(msg);

			// Handle message according to class
			switch (cl) {
				case IDCMP_CLOSEWINDOW:
					done = true;
					break;

				case IDCMP_GADGETUP:
					switch (gad->GadgetID) {
						case MSG_OK:
							ok_clicked = true;
							done = true;
							break;
						case MSG_CANCEL:
							done = true;
							break;
					}
					break;

				case IDCMP_IDCMPUPDATE: {
					struct FileRequester *req = NULL;
					switch (gad->GadgetID) {
						case GAD_VOLUME_FILE:
							req = file_request;
							goto do_req;
						case GAD_VOLUME_DEVICE:
							req = dev_request;
do_req:						if (req) {
								LT_LockWindow(win);
								BOOL result = AslRequestTags(req, ASLFR_Window, (ULONG)win, TAG_END);
								LT_UnlockWindow(win);
								if (result) {
									char *str;
									GT_GetGadgetAttrs(gad, win, NULL, GTST_String, (ULONG)&str, TAG_END);
									if (gad->GadgetID == GAD_VOLUME_FILE) {
										strncpy(str, req->rf_Dir, 255);
										str[255] = 0;
										AddPart(str, req->rf_File, 255);
									} else {
										if (strlen(req->rf_File)) {
											strncpy(str, req->rf_File, 255);	// Don't copy the directory part. This is usually "DEVS:" and we don't need that.
											str[255] = 0;
										} else if (strlen(req->rf_Dir) && req->rf_Dir[strlen(req->rf_Dir) - 1] == ':') {
											analyze_partition(req->rf_Dir, str, dev_unit, dev_flags, dev_start, dev_size, dev_bsize);
											LT_SetAttributes(h, GAD_VOLUME_UNIT, GTIN_Number, dev_unit, TAG_END);
											LT_SetAttributes(h, GAD_VOLUME_OPENFLAGS, GTIN_Number, dev_flags, TAG_END);
											LT_SetAttributes(h, GAD_VOLUME_STARTBLOCK, GTIN_Number, dev_start, TAG_END);
											LT_SetAttributes(h, GAD_VOLUME_SIZE, GTIN_Number, dev_size, TAG_END);
											LT_SetAttributes(h, GAD_VOLUME_BLOCKSIZE, GTIN_Number, dev_bsize, TAG_END);
										}
									}
									LT_SetAttributes(h, gad->GadgetID, GTST_String, (ULONG)str, TAG_END);
								}
							}
							break;
					}
					break;
				}
			}
		}
	} while (!done);

	// Update preferences and list view
	if (ok_clicked) {
		char str[256];
		LT_UpdateStrings(h);

		if (is_device)
			sprintf(str, "%s/dev/%s/%ld/%ld/%ld/%ld/%ld", read_only ? "*" : "", dev_name, dev_unit, dev_flags, dev_start, dev_size, dev_bsize);
		else
			sprintf(str, "%s%s", read_only ? "*" : "", file_name);
		LT_SetAttributes(h2, GAD_DISK_LIST, GTLV_Labels, ~0, TAG_END);

		if (adding) {

			// Add new item
			int i;
			PrefsAddString("disk", str);
			struct Node *item = (struct Node *)AllocMem(sizeof(struct Node), MEMF_CLEAR);
			for (i=0; PrefsFindString("disk", i); i++) ;
			item->ln_Name = (char *)PrefsFindString("disk", i - 1);
			AddTail(&disk_list, item);

		} else {

			// Replace existing item
			PrefsReplaceString("disk", str, sel);
			struct Node *item = disk_list.lh_Head;
			for (int i=0; item->ln_Succ; i++) {
				if (i == sel) {
					item->ln_Name = (char *)PrefsFindString("disk", sel);
					break;
				}
				item = item->ln_Succ;
			}
		}
		LT_SetAttributes(h2, GAD_DISK_LIST, GTLV_Labels, (ULONG)&disk_list, TAG_END);
		ghost_volumes_gadgets(h2);
	}

	// Delete handle
	LT_DeleteHandle(h);
}

// Remove volume from list
static void remove_volume(struct LayoutHandle *h)
{
	UWORD sel = LT_GetAttributes(h, GAD_DISK_LIST, TAG_END);
	if (sel != 0xffff) {

		// Remove item from preferences and list view
		LT_SetAttributes(h, GAD_DISK_LIST, GTLV_Labels, ~0, TAG_END);
		PrefsRemoveItem("disk", sel);
		struct Node *item = disk_list.lh_Head;
		for (int i=0; item->ln_Succ; i++) {
			struct Node *next = item->ln_Succ;
			if (i == sel) {
				Remove(item);
				FreeMem(item, sizeof(struct Node));
				break;
			}
			item = next;
		}
		LT_SetAttributes(h, GAD_DISK_LIST, GTLV_Labels, (ULONG)&disk_list, GTLV_Selected, 0xffff, TAG_END);
		ghost_volumes_gadgets(h);
	}
}

// Read settings from gadgets and set preferences
static void read_volumes_settings(void)
{
	struct Node *item = disk_list.lh_Head;
	while (item->ln_Succ) {
		struct Node *next = item->ln_Succ;
		Remove(item);
		FreeMem(item, sizeof(struct Node));
		item = next;
	}

	if (strlen(cdrom_name)) {
		char str[256];
		sprintf(str, "/dev/%s/%ld/%ld/%ld/%ld/%ld", cdrom_name, cdrom_unit, cdrom_flags, cdrom_start, cdrom_size, cdrom_bsize);
		PrefsReplaceString("cdrom", str);
	} else
		PrefsRemoveItem("cdrom");

	PrefsReplaceBool("nocdrom", nocdrom);

	if (strlen(extfs_name))
		PrefsReplaceString("extfs", extfs_name);
}

// Create "Volumes" pane
static void create_volumes_pane(struct LayoutHandle *h)
{
	parse_volumes_prefs();

	VGROUP;
		LT_New(h, LA_Type, VERTICAL_KIND,
			LA_LabelID, STR_VOLUMES_CTRL,
			TAG_END
		);
			VGROUP;
				LT_New(h, LA_Type, LISTVIEW_KIND,
					LA_ID, GAD_DISK_LIST,
					LA_Chars, 20,
					GTLV_Labels, (ULONG)&disk_list,
					LALV_Lines, 6,
					LALV_Link, (ULONG)NIL_LINK,
					LALV_ResizeX, TRUE,
					LALV_ResizeY, TRUE,
					LALV_Selected, 0,
					TAG_END
				);
			ENDGROUP;
			LT_New(h, LA_Type, HORIZONTAL_KIND,
				LAGR_SameSize, TRUE,
				LAGR_Spread, TRUE,
				TAG_END
			);
				LT_New(h, LA_Type, BUTTON_KIND,
					LA_LabelID, STR_ADD_VOLUME_BUTTON,
					LA_ID, GAD_ADD_VOLUME,
					TAG_END
				);
				LT_New(h, LA_Type, BUTTON_KIND,
					LA_LabelID, STR_EDIT_VOLUME_BUTTON,
					LA_ID, GAD_EDIT_VOLUME,
					TAG_END
				);
				LT_New(h, LA_Type, BUTTON_KIND,
					LA_LabelID, STR_REMOVE_VOLUME_BUTTON,
					LA_ID, GAD_REMOVE_VOLUME,
					TAG_END
				);
			ENDGROUP;
		ENDGROUP;
		LT_New(h, LA_Type, VERTICAL_KIND,
			LA_LabelID, STR_CDROM_DRIVE_CTRL,
			TAG_END
		);
			LT_New(h, LA_Type, STRING_KIND,
				LA_LabelID, STR_DEVICE_CTRL,
				LA_ID, GAD_CDROM_DEVICE,
				LA_Chars, 20,
				LA_STRPTR, (ULONG)cdrom_name,
				GTST_MaxChars, sizeof(cdrom_name) - 1,
				LAST_Picker, TRUE,
				TAG_END
			);
			LT_New(h, LA_Type, INTEGER_KIND,
				LA_LabelID, STR_UNIT_CTRL,
				LA_ID, GAD_CDROM_UNIT,
				LA_LONG, (ULONG)&cdrom_unit,
				LAIN_UseIncrementers, TRUE,
				GTIN_MaxChars, 8,
				TAG_END
			);
			LT_New(h, LA_Type, CYCLE_KIND,
				LA_LabelID, STR_BOOTDRIVER_CTRL,
				LA_ID, GAD_BOOTDRIVER,
				LACY_FirstLabel, STR_BOOT_ANY_LAB,
				LACY_LastLabel, STR_BOOT_CDROM_LAB,
				LA_BYTE, (ULONG)&bootdriver_num,
				TAG_END
			);
			LT_New(h, LA_Type, CHECKBOX_KIND,
				LA_LabelID, STR_NOCDROM_CTRL,
				LA_ID, GAD_NOCDROM,
				LA_BYTE, (ULONG)&nocdrom,
				TAG_END
			);
		ENDGROUP;
		VGROUP;
			LT_New(h, LA_Type, STRING_KIND,
				LA_LabelID, STR_EXTFS_CTRL,
				LA_ID, GAD_EXTFS,
				LA_Chars, 20,
				LA_STRPTR, (ULONG)extfs_name,
				GTST_MaxChars, sizeof(extfs_name) - 1,
				TAG_END
			);
		ENDGROUP;
	ENDGROUP;
}


/*
 *  "SCSI" pane
 */

static char scsi_dev[6][256];
static LONG scsi_unit[6];
static LONG scsi_memtype;

// Read SCSI preferences
static void parse_scsi_prefs(void)
{
	for (int i=0; i<7; i++) {
		scsi_dev[i][0] = 0;
		scsi_unit[i] = 0;

		char prefs_name[16];
		sprintf(prefs_name, "scsi%d", i);
		const char *str = PrefsFindString(prefs_name);
		if (str)
			sscanf(str, "%[^/]/%ld", scsi_dev[i], &scsi_unit[i]);
	}

	scsi_memtype = PrefsFindInt32("scsimemtype");
}

// Read settings from gadgets and set preferences
static void read_scsi_settings(void)
{
	for (int i=0; i<7; i++) {
		char prefs_name[16];
		sprintf(prefs_name, "scsi%d", i);

		if (strlen(scsi_dev[i])) {
			char str[256];
			sprintf(str, "%s/%ld", scsi_dev[i], scsi_unit[i]);
			PrefsReplaceString(prefs_name, str);
		} else
			PrefsRemoveItem(prefs_name);
	}
}

// Create "SCSI" pane
static void create_scsi_pane(struct LayoutHandle *h)
{
	parse_scsi_prefs();

	VGROUP;
		LT_New(h, LA_Type, VERTICAL_KIND,
			LA_LabelID, STR_SCSI_DEVICES_CTRL,
			TAG_END
		);
			for (int i=0; i<7; i++) {
				HGROUP;
					LT_New(h, LA_Type, TEXT_KIND,
						LA_LabelID, STR_SCSI_ID_0 + i,
						TAG_END
					);
					LT_New(h, LA_Type, STRING_KIND,
						LA_LabelID, STR_DEVICE_CTRL,
						LA_ID, GAD_SCSI0_DEVICE + i,
						LA_Chars, 20,
						LA_STRPTR, (ULONG)scsi_dev[i],
						GTST_MaxChars, sizeof(scsi_dev[i]) - 1,
						LAST_Picker, TRUE,
						TAG_END
					);
					LT_New(h, LA_Type, INTEGER_KIND,
						LA_LabelID, STR_UNIT_CTRL,
						LA_ID, GAD_SCSI0_UNIT + i,
						LA_Chars, 4,
						LA_LONG, (ULONG)&scsi_unit[i],
						LAIN_UseIncrementers, TRUE,
						GTIN_MaxChars, 8,
						TAG_END
					);
				ENDGROUP;
			}
		ENDGROUP;
		VGROUP;
			LT_New(h, LA_Type, CYCLE_KIND,
				LA_LabelID, STR_SCSI_MEMTYPE_CTRL,
				LA_ID, GAD_SCSI_MEMTYPE,
				LACY_FirstLabel, STR_MEMTYPE_CHIP_LAB,
				LACY_LastLabel, STR_MEMTYPE_ANY_LAB,
				LA_LONG, (ULONG)&scsi_memtype,
				TAG_END
			);
		ENDGROUP;
	ENDGROUP;
}


/*
 *  "Graphics/Sound" pane
 */

// Display types
enum {
	DISPLAY_WINDOW,
	DISPLAY_PIP,
	DISPLAY_SCREEN
};

static LONG display_type;
static LONG dis_width, dis_height;
static ULONG mode_id;
static BYTE frameskip_num;
static struct NameInfo mode_name;
static ULONG ahi_id;
static char ahi_mode_name[256];
static BYTE nosound;

// Read graphics preferences
static void parse_graphics_prefs(void)
{
	display_type = DISPLAY_WINDOW;
	dis_width = 512;
	dis_height = 384;
	mode_id = 0;
	ahi_id = AHI_DEFAULT_ID;
	ahi_mode_name[0] = 0;

	frameskip_num = 0;
	int frameskip = PrefsFindInt32("frameskip");
	switch (frameskip) {
		case 12:
			frameskip_num = 0;
			break;
		case 8:
			frameskip_num = 1;
			break;
		case 6:
			frameskip_num = 2;
			break;
		case 4:
			frameskip_num = 3;
			break;
		case 2:
			frameskip_num = 4;
			break;
		case 1:
			frameskip_num = 5;
			break;
	}

	const char *str = PrefsFindString("screen");
	if (str) {
		if (sscanf(str, "win/%ld/%ld", &dis_width, &dis_height) == 2)
			display_type = DISPLAY_WINDOW;
		else if (sscanf(str, "pip/%ld/%ld", &dis_width, &dis_height) == 2)
			display_type = DISPLAY_PIP;
		else if (sscanf(str, "scr/%08lx", &mode_id) == 1)
			display_type = DISPLAY_SCREEN;
	}

	GetDisplayInfoData(NULL, (UBYTE *)&mode_name, sizeof(mode_name), DTAG_NAME, mode_id);

	str = PrefsFindString("sound");
	if (str) {
		if (sscanf(str, "ahi/%08lx", &ahi_id) == 1 && AHIBase) {
			AHI_GetAudioAttrs(ahi_id, NULL,
				AHIDB_Name, (ULONG)ahi_mode_name,
				AHIDB_BufferLen, sizeof(ahi_mode_name) - 1,
				TAG_END
			);
		}
	}
	nosound = PrefsFindBool("nosound");
}

// Ghost/unghost graphics gadgets, depending on display type
static void ghost_graphics_gadgets(struct LayoutHandle *h)
{
	bool dis_xy, dis_skip, dis_mode;
	switch (display_type) {
		case DISPLAY_WINDOW:
			dis_xy = false;
			dis_skip = false;
			dis_mode = true;
			break;
		case DISPLAY_PIP:
			dis_xy = false;
			dis_skip = true;
			dis_mode = true;
			break;
		case DISPLAY_SCREEN:
			dis_xy = true;
			dis_skip = true;
			dis_mode = false;
			break;
	}
	LT_SetAttributes(h, GAD_DISPLAY_X, GA_Disabled, dis_xy, TAG_END);
	LT_SetAttributes(h, GAD_DISPLAY_Y, GA_Disabled, dis_xy, TAG_END);
	LT_SetAttributes(h, GAD_FRAMESKIP, GA_Disabled, dis_skip, TAG_END);
	LT_SetAttributes(h, GAD_SCREEN_MODE, GA_Disabled, dis_mode, TAG_END);
	LT_SetAttributes(h, GAD_AHI_MODE, GA_Disabled, AHIBase == NULL, TAG_END);
}

// Show screen mode requester
static void screen_mode_req(struct Window *win, struct LayoutHandle *h)
{
	if (P96Base == NULL && CyberGfxBase == NULL)
		return;

	LT_LockWindow(win);

	ULONG id;

	// Try P96 first, because it also provides a (fake) cybergraphics.library
	if (P96Base) {
		id = p96RequestModeIDTags(
			P96MA_MinDepth, 8,
			P96MA_FormatsAllowed, RGBFF_CLUT | RGBFF_R5G5B5 | RGBFF_A8R8G8B8,
			TAG_END
		);
	} else {
		UWORD ModelArray[] = { PIXFMT_LUT8, PIXFMT_RGB15, PIXFMT_ARGB32, 0, ~0 };
		id = (ULONG) CModeRequestTags(NULL,
			CYBRMREQ_MinDepth, 8,
			CYBRMREQ_CModelArray, (ULONG) ModelArray,
			TAG_END
		);
	}
	LT_UnlockWindow(win);

	if (id != INVALID_ID) {
		mode_id = id;
		GetDisplayInfoData(NULL, (UBYTE *)&mode_name, sizeof(mode_name), DTAG_NAME, mode_id);
		LT_SetAttributes(h, GAD_SCREEN_MODE, GTTX_Text, (ULONG)mode_name.Name, TAG_END);
	}
}

// Show AHI mode requester
static void ahi_mode_req(struct Window *win, struct LayoutHandle *h)
{
	if (AHIBase == NULL)
		return;

	struct AHIAudioModeRequester *req = AHI_AllocAudioRequest(
		AHIR_Window, (ULONG)win,
		TAG_END
	);
	if (req == NULL)
		return;

	LT_LockWindow(win);
	BOOL ok = AHI_AudioRequest(req,
		AHIR_InitialAudioID, ahi_id,
		TAG_END
	);
	LT_UnlockWindow(win);

	if (ok) {
		ahi_id = req->ahiam_AudioID;
		AHI_GetAudioAttrs(ahi_id, NULL,
			AHIDB_Name, (ULONG)ahi_mode_name,
			AHIDB_BufferLen, sizeof(ahi_mode_name) - 1,
			TAG_END
		);
		LT_SetAttributes(h, GAD_AHI_MODE, GTTX_Text, (ULONG)ahi_mode_name, TAG_END);
	}
	AHI_FreeAudioRequest(req);
}

// Read settings from gadgets and set preferences
static void read_graphics_settings(void)
{
	char str[256];
	switch (display_type) {
		case DISPLAY_WINDOW:
			sprintf(str, "win/%ld/%ld", dis_width, dis_height);
			break;
		case DISPLAY_PIP:
			sprintf(str, "pip/%ld/%ld", dis_width, dis_height);
			break;
		case DISPLAY_SCREEN:
			sprintf(str, "scr/%08lx", mode_id);
			break;
		default:
			PrefsRemoveItem("screen");
			return;
	}
	PrefsReplaceString("screen", str);

	sprintf(str, "ahi/%08lx", ahi_id);
	PrefsReplaceString("sound", str);

	PrefsReplaceBool("nosound", nosound);
}

// Create "Graphics/Sound" pane
static void create_graphics_pane(struct LayoutHandle *h)
{
	parse_graphics_prefs();

	VGROUP;
		LT_New(h, LA_Type, VERTICAL_KIND,
			LA_LabelID, STR_GRAPHICS_CTRL,
			TAG_END
		);
			static const LONG labels[] = {STR_WINDOW_LAB, STR_PIP_LAB, STR_FULLSCREEN_LAB, -1};
			LT_New(h, LA_Type, CYCLE_KIND,
				LA_LabelID, STR_VIDEO_TYPE_CTRL,
				LA_ID, GAD_VIDEO_TYPE,
				LACY_LabelTable, (ULONG)labels,
				LA_LONG, (ULONG)&display_type,
				TAG_END
			);
			LT_New(h, LA_Type, INTEGER_KIND,
				LA_LabelID, STR_DISPLAY_X_CTRL,
				LA_ID, GAD_DISPLAY_X,
				LA_LONG, (ULONG)&dis_width,
				GTIN_MaxChars, 8,
				TAG_END
			);
			LT_New(h, LA_Type, INTEGER_KIND,
				LA_LabelID, STR_DISPLAY_Y_CTRL,
				LA_ID, GAD_DISPLAY_Y,
				LA_LONG, (ULONG)&dis_height,
				GTIN_MaxChars, 8,
				TAG_END
			);
			LT_New(h, LA_Type, POPUP_KIND,
				LA_LabelID, STR_FRAMESKIP_CTRL,
				LA_ID, GAD_FRAMESKIP,
				LAPU_FirstLabel, STR_REF_5HZ_LAB,
				LAPU_LastLabel, STR_REF_60HZ_LAB,
				LA_BYTE, (ULONG)&frameskip_num,
				TAG_END
			);
			LT_New(h, LA_Type, TEXT_KIND,
				LA_LabelID, STR_SCREEN_MODE_CTRL,
				LA_ID, GAD_SCREEN_MODE,
				LA_Chars, DISPLAYNAMELEN,
				LATX_Picker, TRUE,
				GTTX_Text, (ULONG)mode_name.Name,
				GTTX_Border, TRUE,
				TAG_END
			);
		ENDGROUP;
		LT_New(h, LA_Type, VERTICAL_KIND,
			LA_LabelID, STR_SOUND_CTRL,
			TAG_END
		);
			LT_New(h, LA_Type, TEXT_KIND,
				LA_LabelID, STR_AHI_MODE_CTRL,
				LA_ID, GAD_AHI_MODE,
				LA_Chars, DISPLAYNAMELEN,
				LATX_Picker, TRUE,
				GTTX_Text, (ULONG)ahi_mode_name,
				GTTX_Border, TRUE,
				TAG_END
			);
			LT_New(h, LA_Type, CHECKBOX_KIND,
				LA_LabelID, STR_NOSOUND_CTRL,
				LA_ID, GAD_NOSOUND,
				LA_BYTE, (ULONG)&nosound,
				TAG_END
			);
		ENDGROUP;
	ENDGROUP;

	ghost_graphics_gadgets(h);
}


/*
 *  "Serial/Network" pane
 */

static char seriala_dev[256], serialb_dev[256];
static LONG seriala_unit, serialb_unit;
static BYTE seriala_ispar, serialb_ispar;

static char ether_dev[256];
static ULONG ether_unit;

// Read serial/network preferences
static void parse_ser_prefs(const char *prefs, char *dev, LONG &unit, BYTE &ispar)
{
	dev[0] = 0;
	unit = 0;
	ispar = false;

	const char *str = PrefsFindString(prefs);
	if (str) {
		if (str[0] == '*') {
			ispar = true;
			str++;
		}
		sscanf(str, "%[^/]/%ld", dev, &unit);
	}
}

static void parse_serial_prefs(void)
{
	parse_ser_prefs("seriala", seriala_dev, seriala_unit, seriala_ispar);
	parse_ser_prefs("serialb", serialb_dev, serialb_unit, serialb_ispar);

	ether_dev[0] = 0;
	ether_unit = 0;

	const char *str = PrefsFindString("ether");
	if (str) {
		const char *FirstSlash = strchr(str, '/');
		const char *LastSlash = strrchr(str, '/');

		if (FirstSlash && FirstSlash && FirstSlash != LastSlash) {
			// Device name contains path, i.e. "Networks/xyzzy.device"
			const char *lp = str;
			char *dp = ether_dev;

			while (lp != LastSlash)
				*dp++ = *lp++;
			*dp = '\0';

			sscanf(LastSlash, "/%ld", &ether_unit);

//			printf("dev=<%s> unit=%d\n", ether_dev, ether_unit);
		} else {
			sscanf(str, "%[^/]/%ld", ether_dev, &ether_unit);
		}
	}
}

// Set serial preference item
static void make_serial_prefs(const char *prefs, const char *dev, LONG unit, BYTE ispar)
{
	if (strlen(dev)) {
		char str[256];
		sprintf(str, "%s%s/%ld", ispar ? "*" : "", dev, unit);
		PrefsReplaceString(prefs, str);
	} else
		PrefsRemoveItem(prefs);
}

// Read settings from gadgets and set preferences
static void read_serial_settings(void)
{
	make_serial_prefs("seriala", seriala_dev, seriala_unit, seriala_ispar);
	make_serial_prefs("serialb", serialb_dev, serialb_unit, serialb_ispar);

	if (strlen(ether_dev)) {
		char str[256];

		sprintf(str, "%s/%ld", ether_dev, ether_unit);
		PrefsReplaceString("ether", str);
	} else
		PrefsRemoveItem("ether");
}

// Create "Serial/Network" pane
static void create_serial_pane(struct LayoutHandle *h)
{
	parse_serial_prefs();

	VGROUP;
		LT_New(h, LA_Type, VERTICAL_KIND,
			LA_LabelID, STR_SERIALA_CTRL,
			TAG_END
		);
			LT_New(h, LA_Type, STRING_KIND,
				LA_LabelID, STR_DEVICE_CTRL,
				LA_ID, GAD_SERIALA_DEVICE,
				LA_Chars, 20,
				LA_STRPTR, (ULONG)seriala_dev,
				GTST_MaxChars, sizeof(seriala_dev) - 1,
				LAST_Picker, TRUE,
				TAG_END
			);
			LT_New(h, LA_Type, INTEGER_KIND,
				LA_LabelID, STR_UNIT_CTRL,
				LA_ID, GAD_SERIALA_UNIT,
				LA_LONG, (ULONG)&seriala_unit,
				LAIN_UseIncrementers, TRUE,
				GTIN_MaxChars, 8,
				TAG_END
			);
			LT_New(h, LA_Type, CHECKBOX_KIND,
				LA_LabelID, STR_ISPAR_CTRL,
				LA_ID, GAD_SERIALA_ISPAR,
				LA_BYTE, (ULONG)&seriala_ispar,
				TAG_END
			);
		ENDGROUP;

		LT_New(h, LA_Type, VERTICAL_KIND,
			LA_LabelID, STR_SERIALB_CTRL,
			TAG_END
		);
			LT_New(h, LA_Type, STRING_KIND,
				LA_LabelID, STR_DEVICE_CTRL,
				LA_ID, GAD_SERIALB_DEVICE,
				LA_Chars, 20,
				LA_STRPTR, (ULONG)serialb_dev,
				GTST_MaxChars, sizeof(serialb_dev) - 1,
				LAST_Picker, TRUE,
				TAG_END
			);
			LT_New(h, LA_Type, INTEGER_KIND,
				LA_LabelID, STR_UNIT_CTRL,
				LA_ID, GAD_SERIALB_UNIT,
				LA_LONG, (ULONG)&serialb_unit,
				LAIN_UseIncrementers, TRUE,
				GTIN_MaxChars, 8,
				TAG_END
			);
			LT_New(h, LA_Type, CHECKBOX_KIND,
				LA_LabelID, STR_ISPAR_CTRL,
				LA_ID, GAD_SERIALB_ISPAR,
				LA_BYTE, (ULONG)&serialb_ispar,
				TAG_END
			);
		ENDGROUP;

		LT_New(h, LA_Type, VERTICAL_KIND,
			LA_LabelID, STR_ETHERNET_IF_CTRL,
			TAG_END
		);
			LT_New(h, LA_Type, STRING_KIND,
				LA_LabelID, STR_DEVICE_CTRL,
				LA_ID, GAD_ETHER_DEVICE,
				LA_Chars, 20,
				LA_STRPTR, (ULONG)ether_dev,
				GTST_MaxChars, sizeof(ether_dev) - 1,
				LAST_Picker, TRUE,
				TAG_END
			);
			LT_New(h, LA_Type, INTEGER_KIND,
				LA_LabelID, STR_UNIT_CTRL,
				LA_ID, GAD_ETHER_UNIT,
				LA_LONG, (ULONG)&ether_unit,
				LAIN_UseIncrementers, TRUE,
				GTIN_MaxChars, 8,
				TAG_END
			);
		ENDGROUP;
	ENDGROUP;
}


/*
 *  "Memory/Misc" pane
 */

static ULONG ramsize_mb;
static BYTE model_num;
static char rom_file[256];

// Read memory/misc preferences
static void parse_memory_prefs(void)
{
	ramsize_mb = PrefsFindInt32("ramsize") >> 20;

	model_num = 0;
	int id = PrefsFindInt32("modelid");
	switch (id) {
		case 5:
			model_num = 0;
			break;
		case 14:
			model_num = 1;
			break;
	}

	rom_file[0] = 0;
	const char *str = PrefsFindString("rom");
	if (str) {
		strncpy(rom_file, str, sizeof(rom_file) - 1);
		rom_file[sizeof(rom_file) - 1] = 0;
	}
}

// Read settings from gadgets and set preferences
static void read_memory_settings(void)
{
	PrefsReplaceInt32("ramsize", ramsize_mb << 20);

	if (strlen(rom_file))
		PrefsReplaceString("rom", rom_file);
	else
		PrefsRemoveItem("rom");
}

// Create "Memory/Misc" pane
static void create_memory_pane(struct LayoutHandle *h)
{
	parse_memory_prefs();

	VGROUP;
		LT_New(h, LA_Type, LEVEL_KIND,
			LA_LabelID, STR_RAMSIZE_SLIDER,
			LA_ID, GAD_RAMSIZE,
			LA_Chars, 20,
			LA_LONG, (ULONG)&ramsize_mb,
			GTSL_LevelFormat, (ULONG)GetString(STR_RAMSIZE_FMT),
			GTSL_Min, 1,
			GTSL_Max, AvailMem(MEMF_LARGEST) >> 20,
			TAG_END
		);
		LT_New(h, LA_Type, CYCLE_KIND,
			LA_LabelID, STR_MODELID_CTRL,
			LA_ID, GAD_MODELID,
			LACY_FirstLabel, STR_MODELID_5_LAB,
			LACY_LastLabel, STR_MODELID_14_LAB,
			LA_BYTE, (ULONG)&model_num,
			TAG_END
		);
		LT_New(h, LA_Type, STRING_KIND,
			LA_LabelID, STR_ROM_FILE_CTRL,
			LA_ID, GAD_ROM_FILE,
			LA_Chars, 20,
			LA_STRPTR, (ULONG)rom_file,
			GTST_MaxChars, sizeof(rom_file) - 1,
			LAST_Picker, TRUE,
			TAG_END
		);
	ENDGROUP;
}


/*
 *  Read settings from gadgets and set preferences
 */

static void read_settings(struct LayoutHandle *h)
{
	LT_UpdateStrings(h);
	read_volumes_settings();
	read_scsi_settings();
	read_graphics_settings();
	read_serial_settings();
	read_memory_settings();
}
