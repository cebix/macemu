/*
 *  video_amiga.cpp - Video/graphics emulation, AmigaOS specific stuff
 *
 *  Basilisk II (C) 1997-1999 Christian Bauer
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
#include <intuition/intuition.h>
#include <graphics/rastport.h>
#include <graphics/gfx.h>
#include <dos/dostags.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/Picasso96.h>

#include "sysdeps.h"
#include "main.h"
#include "adb.h"
#include "prefs.h"
#include "user_strings.h"
#include "video.h"

#define DEBUG 1
#include "debug.h"


// Display types
enum {
	DISPLAY_WINDOW,
	DISPLAY_PIP,
	DISPLAY_SCREEN
};

// Global variables
static int32 frame_skip;
static int display_type = DISPLAY_WINDOW;		// See enum above
static struct Screen *the_screen = NULL;
static struct Window *the_win = NULL;
static struct BitMap *the_bitmap = NULL;
static LONG black_pen = -1, white_pen = -1;
static struct Process *periodic_proc = NULL;	// Periodic process

extern struct Task *MainTask;					// Pointer to main task (from main_amiga.cpp)


// Amiga -> Mac raw keycode translation table
static const uint8 keycode2mac[0x80] = {
	0x0a, 0x12, 0x13, 0x14, 0x15, 0x17, 0x16, 0x1a,	//   `   1   2   3   4   5   6   7
	0x1c, 0x19, 0x1d, 0x1b, 0x18, 0x2a, 0xff, 0x52,	//   8   9   0   -   =   \ inv   0
	0x0c, 0x0d, 0x0e, 0x0f, 0x11, 0x10, 0x20, 0x22,	//   Q   W   E   R   T   Y   U   I
	0x1f, 0x23, 0x21, 0x1e, 0xff, 0x53, 0x54, 0x55,	//   O   P   [   ] inv   1   2   3
	0x00, 0x01, 0x02, 0x03, 0x05, 0x04, 0x26, 0x28,	//   A   S   D   F   G   H   J   K
	0x25, 0x29, 0x27, 0x2a, 0xff, 0x56, 0x57, 0x58,	//   L   ;   '   # inv   4   5   6
	0x32, 0x06, 0x07, 0x08, 0x09, 0x0b, 0x2d, 0x2e,	//   <   Z   X   C   V   B   N   M
	0x2b, 0x2f, 0x2c, 0xff, 0x41, 0x59, 0x5b, 0x5c,	//   ,   .   / inv   .   7   8   9
	0x31, 0x33, 0x30, 0x4c, 0x24, 0x35, 0x75, 0xff,	// SPC BSP TAB ENT RET ESC DEL inv
	0xff, 0xff, 0x4e, 0xff, 0x3e, 0x3d, 0x3c, 0x3b,	// inv inv   - inv CUP CDN CRT CLF
	0x7a, 0x78, 0x63, 0x76, 0x60, 0x61, 0x62, 0x64,	//  F1  F2  F3  F4  F5  F6  F7  F8
	0x65, 0x6d, 0x47, 0x51, 0x4b, 0x43, 0x45, 0x72,	//  F9 F10   (   )   /   *   + HLP
	0x38, 0x38, 0x39, 0x36, 0x3a, 0x3a, 0x37, 0x37,	// SHL SHR CAP CTL ALL ALR AML AMR
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	// inv inv inv inv inv inv inv inv
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,	// inv inv inv inv inv inv inv inv
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff	// inv inv inv inv inv inv inv inv
};


// Prototypes
static void periodic_func(void);


/*
 *  Initialization
 */

// Open window
static bool init_window(int width, int height)
{
	// Set absolute mouse mode
	ADBSetRelMouseMode(false);

	// Open window
	the_win = OpenWindowTags(NULL,
		WA_Left, 0, WA_Top, 0,
		WA_InnerWidth, width, WA_InnerHeight, height,
		WA_SimpleRefresh, TRUE,
		WA_NoCareRefresh, TRUE,
		WA_Activate, TRUE,
		WA_RMBTrap, TRUE,
		WA_ReportMouse, TRUE,
		WA_DragBar, TRUE,
		WA_DepthGadget, TRUE,
		WA_SizeGadget, FALSE,
		WA_Title, (ULONG)GetString(STR_WINDOW_TITLE),
		TAG_END
	);
	if (the_win == NULL) {
		ErrorAlert(GetString(STR_OPEN_WINDOW_ERR));
		return false;
	}

	// Create bitmap ("height + 2" for safety)
	the_bitmap = AllocBitMap(width, height + 2, 1, BMF_CLEAR, NULL);
	if (the_bitmap == NULL) {
		ErrorAlert(GetString(STR_NO_MEM_ERR));
		return false;
	}

	// Set VideoMonitor
	VideoMonitor.mac_frame_base = (uint32)the_bitmap->Planes[0];
	VideoMonitor.bytes_per_row = the_bitmap->BytesPerRow;
	VideoMonitor.x = width;
	VideoMonitor.y = height;
	VideoMonitor.mode = VMODE_1BIT;

	// Set FgPen and BgPen
	black_pen = ObtainBestPenA(the_win->WScreen->ViewPort.ColorMap, 0, 0, 0, NULL);
	white_pen = ObtainBestPenA(the_win->WScreen->ViewPort.ColorMap, 0xffffffff, 0xffffffff, 0xffffffff, NULL);
	SetAPen(the_win->RPort, black_pen);
	SetBPen(the_win->RPort, white_pen);
	SetDrMd(the_win->RPort, JAM2);
	return true;
}

// Open PIP (requires Picasso96)
static bool init_pip(int width, int height)
{
	// Set absolute mouse mode
	ADBSetRelMouseMode(false);

	// Open window
	ULONG error = 0;
	the_win = p96PIP_OpenTags(
		P96PIP_SourceFormat, RGBFB_R5G5B5,
		P96PIP_SourceWidth, width,
		P96PIP_SourceHeight, height,
		P96PIP_ErrorCode, (ULONG)&error,
		WA_Left, 0, WA_Top, 0,
		WA_InnerWidth, width, WA_InnerHeight, height,
		WA_SimpleRefresh, TRUE,
		WA_NoCareRefresh, TRUE,
		WA_Activate, TRUE,
		WA_RMBTrap, TRUE,
		WA_ReportMouse, TRUE,
		WA_DragBar, TRUE,
		WA_DepthGadget, TRUE,
		WA_SizeGadget, FALSE,
		WA_Title, (ULONG)GetString(STR_WINDOW_TITLE),
		WA_PubScreenName, (ULONG)"Workbench",
		TAG_END
	);
	if (the_win == NULL || error) {
		ErrorAlert(GetString(STR_OPEN_WINDOW_ERR));
		return false;
	}

	// Find bitmap
	p96PIP_GetTags(the_win, P96PIP_SourceBitMap, (ULONG)&the_bitmap, TAG_END);

	// Set VideoMonitor
	VideoMonitor.mac_frame_base = p96GetBitMapAttr(the_bitmap, P96BMA_MEMORY);
	VideoMonitor.bytes_per_row = p96GetBitMapAttr(the_bitmap, P96BMA_BYTESPERROW);
	VideoMonitor.x = width;
	VideoMonitor.y = height;
	VideoMonitor.mode = VMODE_16BIT;
	return true;
}

// Open screen (requires Picasso96 as we need chunky modes)
static bool init_screen(ULONG mode_id)
{
	// Set relative mouse mode
	ADBSetRelMouseMode(true);

	// Check if the mode is a Picasso96 mode
	if (!p96GetModeIDAttr(mode_id, P96IDA_ISP96)) {
		ErrorAlert(GetString(STR_NO_P96_MODE_ERR));
		return false;
	}

	// Check if the mode is one we can handle
	uint32 depth = p96GetModeIDAttr(mode_id, P96IDA_DEPTH);
	uint32 format = p96GetModeIDAttr(mode_id, P96IDA_RGBFORMAT);
	switch (depth) {
		case 8:
			VideoMonitor.mode = VMODE_8BIT;
			break;
		case 15:
		case 16:
			if (format != RGBFB_R5G5B5) {
				ErrorAlert(GetString(STR_WRONG_SCREEN_FORMAT_ERR));
				return false;
			}
			VideoMonitor.mode = VMODE_16BIT;
			break;
		case 24:
		case 32:
			if (format != RGBFB_A8R8G8B8) {
				ErrorAlert(GetString(STR_WRONG_SCREEN_FORMAT_ERR));
				return false;
			}
			VideoMonitor.mode = VMODE_32BIT;
			break;
		default:
			ErrorAlert(GetString(STR_WRONG_SCREEN_DEPTH_ERR));
			return false;
	}

	// Yes, get width and height
	uint32 width = p96GetModeIDAttr(mode_id, P96IDA_WIDTH);
	uint32 height = p96GetModeIDAttr(mode_id, P96IDA_HEIGHT);
	VideoMonitor.x = width;
	VideoMonitor.y = height;

	// Open screen
	the_screen = p96OpenScreenTags(
		P96SA_DisplayID, mode_id,
		P96SA_Title, (ULONG)GetString(STR_WINDOW_TITLE),
		P96SA_Quiet, TRUE,
		P96SA_NoMemory, TRUE,
		P96SA_NoSprite, TRUE,
		P96SA_Exclusive, TRUE,
		TAG_END
	);
	if (the_screen == NULL) {
		ErrorAlert(GetString(STR_OPEN_SCREEN_ERR));
		return false;
	}

	// Open window
	the_win = OpenWindowTags(NULL,
		WA_Left, 0, WA_Top, 0,
		WA_Width, width, WA_Height, height,
		WA_NoCareRefresh, TRUE,
		WA_Borderless, TRUE,
		WA_Activate, TRUE,
		WA_RMBTrap, TRUE,
		WA_ReportMouse, TRUE,
		WA_CustomScreen, (ULONG)the_screen,
		TAG_END
	);
	if (the_win == NULL) {
		ErrorAlert(GetString(STR_OPEN_WINDOW_ERR));
		return false;
	}

	// Set VideoMonitor
	ScreenToFront(the_screen);
	VideoMonitor.mac_frame_base = p96GetBitMapAttr(the_screen->RastPort.BitMap, P96BMA_MEMORY);
	VideoMonitor.bytes_per_row = p96GetBitMapAttr(the_screen->RastPort.BitMap, P96BMA_BYTESPERROW);
	return true;
}

bool VideoInit(bool classic)
{
	// Read frame skip prefs
	frame_skip = PrefsFindInt32("frameskip");
	if (frame_skip == 0)
		frame_skip = 1;

	// Get screen mode from preferences
	const char *mode_str;
	if (classic)
		mode_str = "win/512/342";
	else
		mode_str = PrefsFindString("screen");

	// Determine type and mode
	display_type = DISPLAY_WINDOW;
	int width = 512, height = 384;
	ULONG mode_id = 0;
	if (mode_str) {
		if (sscanf(mode_str, "win/%d/%d", &width, &height) == 2)
			display_type = DISPLAY_WINDOW;
		else if (sscanf(mode_str, "pip/%d/%d", &width, &height) == 2 && P96Base)
			display_type = DISPLAY_PIP;
		else if (sscanf(mode_str, "scr/%08lx", &mode_id) == 1 && P96Base)
			display_type = DISPLAY_SCREEN;
	}

	// Open display
	switch (display_type) {
		case DISPLAY_WINDOW:
			if (!init_window(width, height))
				return false;
			break;

		case DISPLAY_PIP:
			if (!init_pip(width, height))
				return false;
			break;

		case DISPLAY_SCREEN:
			if (!init_screen(mode_id))
				return false;
			break;
	}

	// Start periodic process
	periodic_proc = CreateNewProcTags(
		NP_Entry, (ULONG)periodic_func,
		NP_Name, (ULONG)"Basilisk II IDCMP Handler",
		NP_Priority, 0,
		TAG_END
	);
	if (periodic_proc == NULL) {
		ErrorAlert(GetString(STR_NO_MEM_ERR));
		return false;
	}
	return true;
}


/*
 *  Deinitialization
 */

void VideoExit(void)
{
	// Stop periodic process
	if (periodic_proc) {
		SetSignal(0, SIGF_SINGLE);
		Signal(&periodic_proc->pr_Task, SIGBREAKF_CTRL_C);
		Wait(SIGF_SINGLE);
	}

	switch (display_type) {

		case DISPLAY_WINDOW:

			// Window mode, free bitmap
			if (the_bitmap) {
				WaitBlit();
				FreeBitMap(the_bitmap);
			}

			// Free pens and close window
			if (the_win) {
				ReleasePen(the_win->WScreen->ViewPort.ColorMap, black_pen);
				ReleasePen(the_win->WScreen->ViewPort.ColorMap, white_pen);

				CloseWindow(the_win);
			}
			break;

		case DISPLAY_PIP:

			// Close PIP
			if (the_win)
				p96PIP_Close(the_win);
			break;

		case DISPLAY_SCREEN:

			// Close window
			if (the_win)
				CloseWindow(the_win);

			// Close screen
			if (the_screen)
				p96CloseScreen(the_screen);
			break;
	}
}


/*
 *  Set palette
 */

void video_set_palette(uint8 *pal)
{
	if (display_type == DISPLAY_SCREEN) {

		// Convert palette to 32 bits
		ULONG table[2 + 256 * 3];
		table[0] = 256 << 16;
		table[256 * 3 + 1] = 0;
		for (int i=0; i<256; i++) {
			table[i*3+1] = pal[i*3] * 0x01010101;
			table[i*3+2] = pal[i*3+1] * 0x01010101;
			table[i*3+3] = pal[i*3+2] * 0x01010101;
		}

		// And load it
		LoadRGB32(&the_screen->ViewPort, table);
	}
}


/*
 *  Video message handling (not neccessary under AmigaOS, handled by periodic_func())
 */

void VideoInterrupt(void)
{
}


/*
 *  Process for window refresh and message handling
 */

static __saveds void periodic_func(void)
{
	struct MsgPort *timer_port = NULL;
	struct timerequest *timer_io = NULL;
	struct IntuiMessage *msg;
	ULONG win_mask = 0, timer_mask = 0;

	// Create message port for window and attach it
	struct MsgPort *win_port = CreateMsgPort();
	if (win_port) {
		win_mask = 1 << win_port->mp_SigBit;
		the_win->UserPort = win_port;
		ModifyIDCMP(the_win, IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_RAWKEY | (display_type == DISPLAY_SCREEN ? IDCMP_DELTAMOVE : 0));
	}

	// Start 60Hz timer for window refresh
	if (display_type == DISPLAY_WINDOW) {
		timer_port = CreateMsgPort();
		if (timer_port) {
			timer_io = (struct timerequest *)CreateIORequest(timer_port, sizeof(struct timerequest));
			if (timer_io) {
				if (!OpenDevice((UBYTE *)TIMERNAME, UNIT_MICROHZ, (struct IORequest *)timer_io, 0)) {
					timer_mask = 1 << timer_port->mp_SigBit;
					timer_io->tr_node.io_Command = TR_ADDREQUEST;
					timer_io->tr_time.tv_secs = 0;
					timer_io->tr_time.tv_micro = 16667 * frame_skip;
					SendIO((struct IORequest *)timer_io);
				}
			}
		}
	}

	// Main loop
	for (;;) {

		// Wait for timer and/or window (CTRL_C is used for quitting the task)
		ULONG sig = Wait(win_mask | timer_mask | SIGBREAKF_CTRL_C);

		if (sig & SIGBREAKF_CTRL_C)
			break;

		if (sig & timer_mask) {

			// Timer tick, update display
			BltTemplate(the_bitmap->Planes[0], 0, the_bitmap->BytesPerRow, the_win->RPort,
				the_win->BorderLeft, the_win->BorderTop, VideoMonitor.x, VideoMonitor.y);

			// Restart timer
			timer_io->tr_node.io_Command = TR_ADDREQUEST;
			timer_io->tr_time.tv_secs = 0;
			timer_io->tr_time.tv_micro = 16667 * frame_skip;
			SendIO((struct IORequest *)timer_io);
		}

		if (sig & win_mask) {

			// Handle window messages
			while (msg = (struct IntuiMessage *)GetMsg(win_port)) {

				// Get data from message and reply
				ULONG cl = msg->Class;
				UWORD code = msg->Code;
				UWORD qualifier = msg->Qualifier;
				WORD mx = msg->MouseX;
				WORD my = msg->MouseY;
				ReplyMsg((struct Message *)msg);

				// Handle message according to class
				switch (cl) {
					case IDCMP_MOUSEMOVE:
						if (display_type == DISPLAY_SCREEN)
							ADBMouseMoved(mx, my);
						else
							ADBMouseMoved(mx - the_win->BorderLeft, my - the_win->BorderTop);
						break;

					case IDCMP_MOUSEBUTTONS:
						if (code == SELECTDOWN)
							ADBMouseDown(0);
						else if (code == SELECTUP)
							ADBMouseUp(0);
						else if (code == MENUDOWN)
							ADBMouseDown(1);
						else if (code == MENUUP)
							ADBMouseUp(1);
						else if (code == MIDDLEDOWN)
							ADBMouseDown(2);
						else if (code == MIDDLEUP)
							ADBMouseUp(2);
						break;

					case IDCMP_RAWKEY:
						if (qualifier & IEQUALIFIER_REPEAT)	// Keyboard repeat is done by MacOS
							break;
						if (code & IECODE_UP_PREFIX)
							ADBKeyUp(keycode2mac[code & 0x7f]);
						else
							ADBKeyDown(keycode2mac[code & 0x7f]);
						break;
				}
			}
		}
	}

	// Stop timer
	if (timer_io) {
		if (!CheckIO((struct IORequest *)timer_io))
			AbortIO((struct IORequest *)timer_io);
		WaitIO((struct IORequest *)timer_io);
		CloseDevice((struct IORequest *)timer_io);
		DeleteIORequest(timer_io);
	}
	if (timer_port)
		DeleteMsgPort(timer_port);

	// Remove port from window and delete it
	Forbid();
	msg = (struct IntuiMessage *)win_port->mp_MsgList.lh_Head;
	struct Node *succ;
	while (succ = msg->ExecMessage.mn_Node.ln_Succ) {
		if (msg->IDCMPWindow == the_win) {
			Remove((struct Node *)msg);
			ReplyMsg((struct Message *)msg);
		}
		msg = (struct IntuiMessage *)succ;
	}
	the_win->UserPort = NULL;
	ModifyIDCMP(the_win, 0);
	Permit();
	DeleteMsgPort(win_port);

	// Main task asked for termination, send signal
	Forbid();
	Signal(MainTask, SIGF_SINGLE);
}
