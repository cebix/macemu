/*
 *  video_amiga.cpp - Video/graphics emulation, AmigaOS specific stuff
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
#include <intuition/intuition.h>
#include <graphics/rastport.h>
#include <graphics/gfx.h>
#include <cybergraphics/cybergraphics.h>
#include <dos/dostags.h>
#include <devices/timer.h>
#define __USE_SYSBASE
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/Picasso96.h>
#include <proto/cybergraphics.h>
#include <inline/exec.h>
#include <inline/dos.h>
#include <inline/intuition.h>
#include <inline/Picasso96.h>
#include <inline/cybergraphics.h>

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "main.h"
#include "adb.h"
#include "prefs.h"
#include "user_strings.h"
#include "video.h"

#define DEBUG 0
#include "debug.h"


// Supported video modes
static vector<video_mode> VideoModes;

// Display types
enum {
	DISPLAY_WINDOW,
	DISPLAY_PIP,
	DISPLAY_SCREEN_P96,
	DISPLAY_SCREEN_CGFX
};

// Global variables
static int32 frame_skip;
static UWORD *null_pointer = NULL;			// Blank mouse pointer data
static UWORD *current_pointer = (UWORD *)-1;		// Currently visible mouse pointer data
static struct Process *periodic_proc = NULL;		// Periodic process

extern struct Task *MainTask;				// Pointer to main task (from main_amiga.cpp)


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


class Amiga_monitor_desc : public monitor_desc {
public:
	Amiga_monitor_desc(const vector<video_mode> &available_modes, video_depth default_depth, uint32 default_id, int default_display_type) 
		:  monitor_desc(available_modes, default_depth, default_id), display_type(default_display_type) {};
	~Amiga_monitor_desc() {};

	virtual void switch_to_current_mode(void);
	virtual void set_palette(uint8 *pal, int num);

	bool video_open(void);
	void video_close(void);
public:
	int display_type;		// See enum above
};


/*
 *  Display "driver" classes
 */

class driver_base {
public:
	driver_base(Amiga_monitor_desc &m);
	virtual ~driver_base();

	virtual void set_palette(uint8 *pal, int num) {};
	virtual struct BitMap *get_bitmap() { return NULL; };
public:
	Amiga_monitor_desc &monitor;	// Associated video monitor
	const video_mode &mode;		// Video mode handled by the driver
	BOOL init_ok;			// Initialization succeeded (we can't use exceptions because of -fomit-frame-pointer)
	struct Window *the_win;
};


class driver_window : public driver_base {
public:
	driver_window(Amiga_monitor_desc &m, int width, int height);
	~driver_window();

	struct BitMap *get_bitmap() { return the_bitmap; };

private:
	LONG black_pen, white_pen;
	struct BitMap *the_bitmap;
};

class driver_pip : public driver_base {
public:
	driver_pip(Amiga_monitor_desc &m, int width, int height);
	~driver_pip();

	struct BitMap *get_bitmap() { return the_bitmap; };

private:
	struct BitMap *the_bitmap;
};

class driver_screen_p96 : public driver_base {
public:
	driver_screen_p96(Amiga_monitor_desc &m, ULONG mode_id);
	~driver_screen_p96();

	void set_palette(uint8 *pal, int num);

private:
	struct Screen *the_screen;
};

class driver_screen_cgfx : public driver_base {
public:
	driver_screen_cgfx(Amiga_monitor_desc &m, ULONG mode_id);
	~driver_screen_cgfx();

	void set_palette(uint8 *pal, int num);

private:
	struct Screen *the_screen;
};


static driver_base *drv = NULL;	// Pointer to currently used driver object



// Prototypes
static void periodic_func(void);
static void add_mode(uint32 width, uint32 height, uint32 resolution_id, uint32 bytes_per_row, video_depth depth);
static void add_modes(uint32 width, uint32 height, video_depth depth);
static ULONG find_mode_for_depth(uint32 width, uint32 height, uint32 depth);
static ULONG bits_from_depth(video_depth depth);
static bool is_valid_modeid(int display_type, ULONG mode_id);
static bool check_modeid_p96(ULONG mode_id);
static bool check_modeid_cgfx(ULONG mode_id);


/*
 *  Initialization
 */


bool VideoInit(bool classic)
{
	video_depth default_depth = VDEPTH_1BIT;
	int default_width, default_height;
	int default_display_type = DISPLAY_WINDOW;
	int window_width, window_height;			// width and height for window display
	ULONG screen_mode_id;				// mode ID for screen display

	// Allocate blank mouse pointer data
	null_pointer = (UWORD *)AllocMem(12, MEMF_PUBLIC | MEMF_CHIP | MEMF_CLEAR);
	if (null_pointer == NULL) {
		ErrorAlert(STR_NO_MEM_ERR);
		return false;
	}

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

	default_width = window_width = 512;
	default_height = window_height = 384;

	if (mode_str) {
		if (sscanf(mode_str, "win/%d/%d", &window_width, &window_height) == 2)
			default_display_type = DISPLAY_WINDOW;
		else if (sscanf(mode_str, "pip/%d/%d", &window_width, &window_height) == 2 && P96Base)
			default_display_type = DISPLAY_PIP;
		else if (sscanf(mode_str, "scr/%08lx", &screen_mode_id) == 1 && (CyberGfxBase || P96Base)) {
			if (P96Base && p96GetModeIDAttr(screen_mode_id, P96IDA_ISP96))
				default_display_type = DISPLAY_SCREEN_P96;
			else if (CyberGfxBase && IsCyberModeID(screen_mode_id))
				default_display_type = DISPLAY_SCREEN_CGFX;
			else {
				ErrorAlert(STR_NO_P96_MODE_ERR);
				return false;
			}
		}
	}

	D(bug("default_display_type %d, window_width %d, window_height %d\n", default_display_type, window_width, window_height));

	// Construct list of supported modes
	switch (default_display_type) {
		case DISPLAY_WINDOW:
			default_width = window_width;
			default_height = window_height;
			default_depth = VDEPTH_1BIT;
			add_modes(window_width, window_height, VDEPTH_1BIT);
			break;

		case DISPLAY_PIP:
			default_width = window_width;
			default_height = window_height;
			default_depth = VDEPTH_16BIT;
			add_modes(window_width, window_height, VDEPTH_16BIT);
			break;

		case DISPLAY_SCREEN_P96:
		case DISPLAY_SCREEN_CGFX:
			struct DimensionInfo dimInfo;
			DisplayInfoHandle handle = FindDisplayInfo(screen_mode_id);

			if (handle == NULL)
				return false;

			if (GetDisplayInfoData(handle, (UBYTE *) &dimInfo, sizeof(dimInfo), DTAG_DIMS, 0) <= 0)
				return false;

			default_width = 1 + dimInfo.Nominal.MaxX - dimInfo.Nominal.MinX;
			default_height = 1 + dimInfo.Nominal.MaxY - dimInfo.Nominal.MinY;

			switch (dimInfo.MaxDepth) {
				case 1:
					default_depth = VDEPTH_1BIT;
					break;
				case 8:
					default_depth = VDEPTH_8BIT;
					break;
				case 15:
				case 16:
					default_depth = VDEPTH_16BIT;
					break;
				case 24:
				case 32:
					default_depth = VDEPTH_32BIT;
					break;
			}

			for (unsigned d=VDEPTH_8BIT; d<=VDEPTH_32BIT; d++) {
				ULONG mode_id = find_mode_for_depth(default_width, default_height, bits_from_depth(video_depth(d)));

				if (is_valid_modeid(default_display_type, mode_id))
					add_modes(default_width, default_height, video_depth(d));
			}
			break;
	}

#if DEBUG
	bug("Available video modes:\n");
	vector<video_mode>::const_iterator i = VideoModes.begin(), end = VideoModes.end();
	while (i != end) {
		bug(" %ld x %ld (ID %02lx), %ld colors\n", i->x, i->y, i->resolution_id, 1 << bits_from_depth(i->depth));
		++i;
	}
#endif

	D(bug("VideoInit/%ld: def_width=%ld  def_height=%ld  def_depth=%ld\n", \
		__LINE__, default_width, default_height, default_depth));

	// Find requested default mode and open display
	if (VideoModes.size() == 1) {
		uint32 default_id ;

		// Create Amiga_monitor_desc for this (the only) display
		default_id = VideoModes[0].resolution_id;
		D(bug("VideoInit/%ld: default_id=%ld\n", __LINE__, default_id));
		Amiga_monitor_desc *monitor = new Amiga_monitor_desc(VideoModes, default_depth, default_id, default_display_type);
		VideoMonitors.push_back(monitor);

		// Open display
		return monitor->video_open();

	} else {

		// Find mode with specified dimensions
		std::vector<video_mode>::const_iterator i, end = VideoModes.end();
		for (i = VideoModes.begin(); i != end; ++i) {
			D(bug("VideoInit/%ld: w=%ld  h=%ld  d=%ld\n", __LINE__, i->x, i->y, bits_from_depth(i->depth)));
			if (i->x == default_width && i->y == default_height && i->depth == default_depth) {
				// Create Amiga_monitor_desc for this (the only) display
				uint32 default_id = i->resolution_id;
				D(bug("VideoInit/%ld: default_id=%ld\n", __LINE__, default_id));
				Amiga_monitor_desc *monitor = new Amiga_monitor_desc(VideoModes, default_depth, default_id, default_display_type);
				VideoMonitors.push_back(monitor);

				// Open display
				return monitor->video_open();
			}
		}

		// Create Amiga_monitor_desc for this (the only) display
		uint32 default_id = VideoModes[0].resolution_id;
		D(bug("VideoInit/%ld: default_id=%ld\n", __LINE__, default_id));
		Amiga_monitor_desc *monitor = new Amiga_monitor_desc(VideoModes, default_depth, default_id, default_display_type);
		VideoMonitors.push_back(monitor);

		// Open display
		return monitor->video_open();
	}

	return true;
}


bool Amiga_monitor_desc::video_open()
{
	const video_mode &mode = get_current_mode();
	ULONG depth_bits = bits_from_depth(mode.depth);
	ULONG ID = find_mode_for_depth(mode.x, mode.y, depth_bits);

	D(bug("video_open/%ld: width=%ld  height=%ld  depth=%ld  ID=%08lx\n", __LINE__, mode.x, mode.y, depth_bits, ID));

	if (ID == INVALID_ID) {
		ErrorAlert(STR_NO_VIDEO_MODE_ERR);
		return false;
	}

	D(bug("video_open/%ld: display_type=%ld\n", __LINE__, display_type));

	// Open display
	switch (display_type) {
		case DISPLAY_WINDOW:
			drv = new driver_window(*this, mode.x, mode.y);
			break;

		case DISPLAY_PIP:
			drv = new driver_pip(*this, mode.x, mode.y);
			break;

		case DISPLAY_SCREEN_P96:
			drv = new driver_screen_p96(*this, ID);
			break;

		case DISPLAY_SCREEN_CGFX:
			drv = new driver_screen_cgfx(*this, ID);
			break;
	}

	D(bug("video_open/%ld: drv=%08lx\n", __LINE__, drv));

	if (drv == NULL)
		return false;

	D(bug("video_open/%ld: init_ok=%ld\n", __LINE__, drv->init_ok));
	if (!drv->init_ok) {
		delete drv;
		drv = NULL;
		return false;
	}

	// Start periodic process
	periodic_proc = CreateNewProcTags(
		NP_Entry, (ULONG)periodic_func,
		NP_Name, (ULONG)"Basilisk II IDCMP Handler",
		NP_Priority, 0,
		TAG_END
	);

	D(bug("video_open/%ld: periodic_proc=%08lx\n", __LINE__, periodic_proc));

	if (periodic_proc == NULL) {
		ErrorAlert(STR_NO_MEM_ERR);
		return false;
	}

	return true;
}


void Amiga_monitor_desc::video_close()
{
	// Stop periodic process
	if (periodic_proc) {
		SetSignal(0, SIGF_SINGLE);
		Signal(&periodic_proc->pr_Task, SIGBREAKF_CTRL_C);
		Wait(SIGF_SINGLE);
	}

	delete drv;
	drv = NULL;

	// Free mouse pointer
	if (null_pointer) {
		FreeMem(null_pointer, 12);
		null_pointer = NULL;
	}
}


/*
 *  Deinitialization
 */

void VideoExit(void)
{
	// Close displays
	vector<monitor_desc *>::iterator i, end = VideoMonitors.end();
	for (i = VideoMonitors.begin(); i != end; ++i)
		dynamic_cast<Amiga_monitor_desc *>(*i)->video_close();
}


/*
 *  Set palette
 */

void Amiga_monitor_desc::set_palette(uint8 *pal, int num)
{
	drv->set_palette(pal, num);
}


/*
 *  Switch video mode
 */

void Amiga_monitor_desc::switch_to_current_mode()
{
	// Close and reopen display
	video_close();
	if (!video_open()) {
		ErrorAlert(STR_OPEN_WINDOW_ERR);
		QuitEmulator();
	}
}


/*
 *  Close down full-screen mode (if bringing up error alerts is unsafe while in full-screen mode)
 */

void VideoQuitFullScreen(void)
{
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

	D(bug("periodic_func/%ld: \n", __LINE__));

	// Create message port for window and attach it
	struct MsgPort *win_port = CreateMsgPort();
	if (win_port) {
		win_mask = 1 << win_port->mp_SigBit;
		drv->the_win->UserPort = win_port;
		ModifyIDCMP(drv->the_win, IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_RAWKEY | 
			((drv->monitor.display_type == DISPLAY_SCREEN_P96 || drv->monitor.display_type == DISPLAY_SCREEN_CGFX) ? IDCMP_DELTAMOVE : 0));
	}

	D(bug("periodic_func/%ld: \n", __LINE__));

	// Start 60Hz timer for window refresh
	if (drv->monitor.display_type == DISPLAY_WINDOW) {
		timer_port = CreateMsgPort();
		if (timer_port) {
			timer_io = (struct timerequest *)CreateIORequest(timer_port, sizeof(struct timerequest));
			if (timer_io) {
				if (!OpenDevice((UBYTE *) TIMERNAME, UNIT_MICROHZ, (struct IORequest *)timer_io, 0)) {
					timer_mask = 1 << timer_port->mp_SigBit;
					timer_io->tr_node.io_Command = TR_ADDREQUEST;
					timer_io->tr_time.tv_secs = 0;
					timer_io->tr_time.tv_micro = 16667 * frame_skip;
					SendIO((struct IORequest *)timer_io);
				}
			}
		}
	}

	D(bug("periodic_func/%ld: \n", __LINE__));

	// Main loop
	for (;;) {
		const video_mode &mode = drv->monitor.get_current_mode();

		// Wait for timer and/or window (CTRL_C is used for quitting the task)
		ULONG sig = Wait(win_mask | timer_mask | SIGBREAKF_CTRL_C);

		if (sig & SIGBREAKF_CTRL_C)
			break;

//		D(bug("periodic_func/%ld: display_type=%ld  the_win=%08lx\n", __LINE__, drv->monitor.display_type, drv->the_win));

		if (sig & timer_mask) {
			if (drv->get_bitmap()) {
				// Timer tick, update display
				BltTemplate(drv->get_bitmap()->Planes[0], 0,
					drv->get_bitmap()->BytesPerRow, drv->the_win->RPort,
					drv->the_win->BorderLeft, drv->the_win->BorderTop,
					mode.x, mode.y);
			}

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
						switch (drv->monitor.display_type) {
							case DISPLAY_SCREEN_P96:
							case DISPLAY_SCREEN_CGFX:
//								D(bug("periodic_func/%ld: IDCMP_MOUSEMOVE mx=%ld  my=%ld\n", __LINE__, mx, my));
								ADBMouseMoved(mx, my);
								break;
							default:
//								D(bug("periodic_func/%ld: IDCMP_MOUSEMOVE mx=%ld  my=%ld\n", __LINE__, mx - drv->the_win->BorderLeft, my - drv->the_win->BorderTop));
								ADBMouseMoved(mx - drv->the_win->BorderLeft, my - drv->the_win->BorderTop);
								if (mx < drv->the_win->BorderLeft
								 || my < drv->the_win->BorderTop
								 || mx >= drv->the_win->BorderLeft + mode.x
								 || my >= drv->the_win->BorderTop + mode.y) {
									if (current_pointer) {
										ClearPointer(drv->the_win);
										current_pointer = NULL;
									}
								} else {
									if (current_pointer != null_pointer) {
										// Hide mouse pointer inside window
										SetPointer(drv->the_win, null_pointer, 1, 16, 0, 0);
										current_pointer = null_pointer;
									}
								}
								break;
						}
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
						if ((qualifier & (IEQUALIFIER_LALT | IEQUALIFIER_LSHIFT | IEQUALIFIER_CONTROL)) ==
						    (IEQUALIFIER_LALT | IEQUALIFIER_LSHIFT | IEQUALIFIER_CONTROL) && code == 0x5f) {
							SetInterruptFlag(INTFLAG_NMI);
							TriggerInterrupt();
							break;
						}

						if (code & IECODE_UP_PREFIX)
							ADBKeyUp(keycode2mac[code & 0x7f]);
						else
							ADBKeyDown(keycode2mac[code & 0x7f]);
						break;
				}
			}
		}
	}

	D(bug("periodic_func/%ld: \n", __LINE__));

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
		if (msg->IDCMPWindow == drv->the_win) {
			Remove((struct Node *)msg);
			ReplyMsg((struct Message *)msg);
		}
		msg = (struct IntuiMessage *)succ;
	}
	drv->the_win->UserPort = NULL;
	ModifyIDCMP(drv->the_win, 0);
	Permit();
	DeleteMsgPort(win_port);

	// Main task asked for termination, send signal
	Forbid();
	Signal(MainTask, SIGF_SINGLE);
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

	D(bug("Added video mode: w=%ld  h=%ld  d=%ld\n", width, height, depth));

	VideoModes.push_back(mode);
}

// Add standard list of modes for given color depth
static void add_modes(uint32 width, uint32 height, video_depth depth)
{
	D(bug("add_modes: w=%ld  h=%ld  d=%ld\n", width, height, depth));

	if (width >= 512 && height >= 384)
		add_mode(512, 384, 0x80, TrivialBytesPerRow(512, depth), depth);
	if (width >= 640 && height >= 480)
		add_mode(640, 480, 0x81, TrivialBytesPerRow(640, depth), depth);
	if (width >= 800 && height >= 600)
		add_mode(800, 600, 0x82, TrivialBytesPerRow(800, depth), depth);
	if (width >= 1024 && height >= 768)
		add_mode(1024, 768, 0x83, TrivialBytesPerRow(1024, depth), depth);
	if (width >= 1152 && height >= 870)
		add_mode(1152, 870, 0x84, TrivialBytesPerRow(1152, depth), depth);
	if (width >= 1280 && height >= 1024)
		add_mode(1280, 1024, 0x85, TrivialBytesPerRow(1280, depth), depth);
	if (width >= 1600 && height >= 1200)
		add_mode(1600, 1200, 0x86, TrivialBytesPerRow(1600, depth), depth);
}


static ULONG find_mode_for_depth(uint32 width, uint32 height, uint32 depth)
{
	ULONG ID = BestModeID(BIDTAG_NominalWidth, width,
		BIDTAG_NominalHeight, height,
		BIDTAG_Depth, depth,
		BIDTAG_DIPFMustNotHave, DIPF_IS_ECS | DIPF_IS_HAM | DIPF_IS_AA,
		TAG_END);

	return ID;
}


static ULONG bits_from_depth(video_depth depth)
{
	int bits = 1 << depth;
	if (bits == 16)
		bits = 15;
	else if (bits == 32)
		bits = 24;

	return bits;
}


static bool is_valid_modeid(int display_type, ULONG mode_id)
{
	if (INVALID_ID == mode_id)
		return false;

	switch (display_type) {
		case DISPLAY_SCREEN_P96:
			return check_modeid_p96(mode_id);
			break;
		case DISPLAY_SCREEN_CGFX:
			return check_modeid_cgfx(mode_id);
			break;
		default:
			return false;
			break;
	}
}


static bool check_modeid_p96(ULONG mode_id)
{
	// Check if the mode is one we can handle
	uint32 depth = p96GetModeIDAttr(mode_id, P96IDA_DEPTH);
	uint32 format = p96GetModeIDAttr(mode_id, P96IDA_RGBFORMAT);

	D(bug("check_modeid_p96: mode_id=%08lx  depth=%ld  format=%ld\n", mode_id, depth, format));

	if (!p96GetModeIDAttr(mode_id, P96IDA_ISP96))
		return false;

	switch (depth) {
		case 8:
			break;
		case 15:
		case 16:
			if (format != RGBFB_R5G5B5)
				return false;
			break;
		case 24:
		case 32:
			if (format != RGBFB_A8R8G8B8)
				return false;
			break;
		default:
			return false;
	}

	return true;
}


static bool check_modeid_cgfx(ULONG mode_id)
{
	uint32 depth = GetCyberIDAttr(CYBRIDATTR_DEPTH, mode_id);
	uint32 format = GetCyberIDAttr(CYBRIDATTR_PIXFMT, mode_id);

	D(bug("check_modeid_cgfx: mode_id=%08lx  depth=%ld  format=%ld\n", mode_id, depth, format));

	if (!IsCyberModeID(mode_id))
		return false;

	switch (depth) {
		case 8:
			break;
		case 15:
		case 16:
			if (format != PIXFMT_RGB15)
				return false;
			break;
		case 24:
		case 32:
			if (format != PIXFMT_ARGB32)
				return false;
			break;
		default:
			return false;
	}

	return true;
}


driver_base::driver_base(Amiga_monitor_desc &m)
 : monitor(m), mode(m.get_current_mode()), init_ok(false)
{
}

driver_base::~driver_base()
{
}


// Open window
driver_window::driver_window(Amiga_monitor_desc &m, int width, int height)
	: black_pen(-1), white_pen(-1), driver_base(m)
{
	// Set absolute mouse mode
	ADBSetRelMouseMode(false);

	// Open window
	the_win = OpenWindowTags(NULL,
		WA_Left, 0, WA_Top, 0,
		WA_InnerWidth, width, WA_InnerHeight, height,
		WA_SimpleRefresh, true,
		WA_NoCareRefresh, true,
		WA_Activate, true,
		WA_RMBTrap, true,
		WA_ReportMouse, true,
		WA_DragBar, true,
		WA_DepthGadget, true,
		WA_SizeGadget, false,
		WA_Title, (ULONG)GetString(STR_WINDOW_TITLE),
		TAG_END
	);
	if (the_win == NULL) {
		init_ok = false;
		ErrorAlert(STR_OPEN_WINDOW_ERR);
		return;
	}

	// Create bitmap ("height + 2" for safety)
	the_bitmap = AllocBitMap(width, height + 2, 1, BMF_CLEAR, NULL);
	if (the_bitmap == NULL) {
		init_ok = false;
		ErrorAlert(STR_NO_MEM_ERR);
		return;
	}

	// Add resolution and set VideoMonitor
	monitor.set_mac_frame_base((uint32)the_bitmap->Planes[0]);

	// Set FgPen and BgPen
	black_pen = ObtainBestPenA(the_win->WScreen->ViewPort.ColorMap, 0, 0, 0, NULL);
	white_pen = ObtainBestPenA(the_win->WScreen->ViewPort.ColorMap, 0xffffffff, 0xffffffff, 0xffffffff, NULL);
	SetAPen(the_win->RPort, black_pen);
	SetBPen(the_win->RPort, white_pen);
	SetDrMd(the_win->RPort, JAM2);

	init_ok = true;
}


driver_window::~driver_window()
{
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
		the_win = NULL;
	}
}


// Open PIP (requires Picasso96)
driver_pip::driver_pip(Amiga_monitor_desc &m, int width, int height)
	: driver_base(m)
{
	// Set absolute mouse mode
	ADBSetRelMouseMode(false);

	D(bug("driver_pip(%d,%d)\n", width, height));

	// Open window
	ULONG error = 0;
	the_win = p96PIP_OpenTags(
		P96PIP_SourceFormat, RGBFB_R5G5B5,
		P96PIP_SourceWidth, width,
		P96PIP_SourceHeight, height,
		P96PIP_ErrorCode, (ULONG)&error,
		P96PIP_AllowCropping, true,
		WA_Left, 0, WA_Top, 0,
		WA_InnerWidth, width, WA_InnerHeight, height,
		WA_SimpleRefresh, true,
		WA_NoCareRefresh, true,
		WA_Activate, true,
		WA_RMBTrap, true,
		WA_ReportMouse, true,
		WA_DragBar, true,
		WA_DepthGadget, true,
		WA_SizeGadget, false,
		WA_Title, (ULONG)GetString(STR_WINDOW_TITLE),
		WA_PubScreenName, (ULONG)"Workbench",
		TAG_END
	);
	if (the_win == NULL || error) {
		init_ok = false;
		ErrorAlert(STR_OPEN_WINDOW_ERR);
		return;
	}

	// Find bitmap
	p96PIP_GetTags(the_win, P96PIP_SourceBitMap, (ULONG)&the_bitmap, TAG_END);

	// Add resolution and set VideoMonitor
	monitor.set_mac_frame_base(p96GetBitMapAttr(the_bitmap, P96BMA_MEMORY));

	init_ok = true;
}

driver_pip::~driver_pip()
{
	// Close PIP
	if (the_win)
		p96PIP_Close(the_win);
}


// Open Picasso96 screen
driver_screen_p96::driver_screen_p96(Amiga_monitor_desc &m, ULONG mode_id)
	: driver_base(m)
{
	// Set relative mouse mode
	ADBSetRelMouseMode(true);

	// Check if the mode is one we can handle
	if (!check_modeid_p96(mode_id))
		{
		init_ok = false;
		ErrorAlert(STR_WRONG_SCREEN_FORMAT_ERR);
		return;
		}

	// Yes, get width and height
	uint32 depth = p96GetModeIDAttr(mode_id, P96IDA_DEPTH);
	uint32 width = p96GetModeIDAttr(mode_id, P96IDA_WIDTH);
	uint32 height = p96GetModeIDAttr(mode_id, P96IDA_HEIGHT);

	// Open screen
	the_screen = p96OpenScreenTags(
		P96SA_DisplayID, mode_id,
		P96SA_Title, (ULONG)GetString(STR_WINDOW_TITLE),
		P96SA_Quiet, true,
		P96SA_NoMemory, true,
		P96SA_NoSprite, true,
		P96SA_Exclusive, true,
		TAG_END
	);
	if (the_screen == NULL) {
		ErrorAlert(STR_OPEN_SCREEN_ERR);
		init_ok = false;
		return;
	}

	// Open window
	the_win = OpenWindowTags(NULL,
		WA_Left, 0, WA_Top, 0,
		WA_Width, width, WA_Height, height,
		WA_SimpleRefresh, true,
		WA_NoCareRefresh, true,
		WA_Borderless, true,
		WA_Activate, true,
		WA_RMBTrap, true,
		WA_ReportMouse, true,
		WA_CustomScreen, (ULONG)the_screen,
		TAG_END
	);
	if (the_win == NULL) {
		ErrorAlert(STR_OPEN_WINDOW_ERR);
		init_ok = false;
		return;
	}

	ScreenToFront(the_screen);

	// Add resolution and set VideoMonitor
	monitor.set_mac_frame_base(p96GetBitMapAttr(the_screen->RastPort.BitMap, P96BMA_MEMORY));

	init_ok = true;
}


driver_screen_p96::~driver_screen_p96()
{
	// Close window
	if (the_win)
		{
		CloseWindow(the_win);
		the_win = NULL;
		}

	// Close screen
	if (the_screen) {
		p96CloseScreen(the_screen);
		the_screen = NULL;
	}
}


void driver_screen_p96::set_palette(uint8 *pal, int num)
{
	// Convert palette to 32 bits
	ULONG table[2 + 256 * 3];
	table[0] = num << 16;
	table[num * 3 + 1] = 0;
	for (int i=0; i<num; i++) {
		table[i*3+1] = pal[i*3] * 0x01010101;
		table[i*3+2] = pal[i*3+1] * 0x01010101;
		table[i*3+3] = pal[i*3+2] * 0x01010101;
	}

	// And load it
	LoadRGB32(&the_screen->ViewPort, table);
}


// Open CyberGraphX screen
driver_screen_cgfx::driver_screen_cgfx(Amiga_monitor_desc &m, ULONG mode_id)
	: driver_base(m)
{
	D(bug("driver_screen_cgfx/%ld: mode_id=%08lx\n", __LINE__, mode_id));

	// Set absolute mouse mode
	ADBSetRelMouseMode(true);

	// Check if the mode is one we can handle
	if (!check_modeid_cgfx(mode_id))
		{
		ErrorAlert(STR_WRONG_SCREEN_FORMAT_ERR);
		init_ok = false;
		return;
		}

	// Yes, get width and height
	uint32 depth = GetCyberIDAttr(CYBRIDATTR_DEPTH, mode_id);
	uint32 width = GetCyberIDAttr(CYBRIDATTR_WIDTH, mode_id);
	uint32 height = GetCyberIDAttr(CYBRIDATTR_HEIGHT, mode_id);

	// Open screen
	the_screen = OpenScreenTags(NULL,
		SA_DisplayID, mode_id,
		SA_Title, (ULONG)GetString(STR_WINDOW_TITLE),
		SA_Quiet, true,
		SA_Exclusive, true,
		TAG_END
	);
	if (the_screen == NULL) {
		ErrorAlert(STR_OPEN_SCREEN_ERR);
		init_ok = false;
		return;
	}

	// Open window
	the_win = OpenWindowTags(NULL,
		WA_Left, 0, WA_Top, 0,
		WA_Width, width, WA_Height, height,
		WA_SimpleRefresh, true,
		WA_NoCareRefresh, true,
		WA_Borderless, true,
		WA_Activate, true,
		WA_RMBTrap, true,
		WA_ReportMouse, true,
		WA_CustomScreen, (ULONG)the_screen,
		TAG_END
	);
	if (the_win == NULL) {
		ErrorAlert(STR_OPEN_WINDOW_ERR);
		init_ok = false;
		return;
	}

	ScreenToFront(the_screen);
	static UWORD ptr[] = { 0, 0, 0, 0 };
	SetPointer(the_win, ptr, 0, 0, 0, 0);	// Hide mouse pointer

	// Set VideoMonitor
	ULONG frame_base;
	APTR handle = LockBitMapTags(the_screen->RastPort.BitMap,
		LBMI_BASEADDRESS, (ULONG)&frame_base,
		TAG_END
	);
	UnLockBitMap(handle);

	D(bug("driver_screen_cgfx/%ld: frame_base=%08lx\n", __LINE__, frame_base));

	monitor.set_mac_frame_base(frame_base);

	init_ok = true;
}


driver_screen_cgfx::~driver_screen_cgfx()
{
	D(bug("~driver_screen_cgfx/%ld: \n", __LINE__));

	// Close window
	if (the_win)
		{
		CloseWindow(the_win);
		the_win = NULL;
		}

	// Close screen
	if (the_screen) {
		CloseScreen(the_screen);
		the_screen = NULL;
	}
}


void driver_screen_cgfx::set_palette(uint8 *pal, int num)
{
	// Convert palette to 32 bits
	ULONG table[2 + 256 * 3];
	table[0] = num << 16;
	table[num * 3 + 1] = 0;
	for (int i=0; i<num; i++) {
		table[i*3+1] = pal[i*3] * 0x01010101;
		table[i*3+2] = pal[i*3+1] * 0x01010101;
		table[i*3+3] = pal[i*3+2] * 0x01010101;
	}

	// And load it
	LoadRGB32(&the_screen->ViewPort, table);
}
