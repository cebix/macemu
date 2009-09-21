/*
 *  main_beos.cpp - Startup code for BeOS
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
#include <KernelKit.h>
#include <StorageKit.h>

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "xpram.h"
#include "timer.h"
#include "video.h"
#include "rom_patches.h"
#include "prefs.h"
#include "prefs_editor.h"
#include "sys.h"
#include "user_strings.h"
#include "version.h"
#include "main.h"

#include "sheep_driver.h"

#define DEBUG 0
#include "debug.h"


// Constants
const char APP_SIGNATURE[] = "application/x-vnd.cebix-BasiliskII";
const char ROM_FILE_NAME[] = "ROM";
const char RAM_AREA_NAME[] = "Macintosh RAM";
const char ROM_AREA_NAME[] = "Macintosh ROM";
const uint32 MSG_START = 'strt';			// Emulator start message
const uint32 ROM_AREA_SIZE = 0x500000;		// Enough to hold PowerMac ROM (for powerrom_cpu)

// Prototypes
#if __POWERPC__
static void sigsegv_handler(vregs *r);
#endif


// Application object
class BasiliskII : public BApplication {
public:
	BasiliskII() : BApplication(APP_SIGNATURE)
	{
		// Find application directory and cwd to it
		app_info the_info;
		GetAppInfo(&the_info);
		BEntry the_file(&the_info.ref);
		BEntry the_dir;
		the_file.GetParent(&the_dir);
		BPath the_path;
		the_dir.GetPath(&the_path);
		chdir(the_path.Path());

		// Initialize other variables
		rom_area = ram_area = -1;
		xpram_thread = tick_thread = -1;
		xpram_thread_active = true;
		tick_thread_active = true;
		AllowQuitting = true;
	}
	virtual void ReadyToRun(void);
	virtual void MessageReceived(BMessage *msg);
	void StartEmulator(void);
	virtual bool QuitRequested(void);
	virtual void Quit(void);

	thread_id xpram_thread;		// XPRAM watchdog
	thread_id tick_thread;		// 60Hz thread

	volatile bool xpram_thread_active;	// Flag for quitting the XPRAM thread
	volatile bool tick_thread_active;	// Flag for quitting the 60Hz thread

	bool AllowQuitting;			// Flag: Alt-Q quitting allowed

private:
	static status_t emul_func(void *arg);
	static status_t tick_func(void *arg);
	static status_t xpram_func(void *arg);
	static void sigsegv_invoc(int sig, void *arg, vregs *r);

	void init_rom(void);
	void load_rom(void);

	area_id rom_area;		// ROM area ID
	area_id ram_area;		// RAM area ID

	struct sigaction sigsegv_action;	// Data access exception signal (of emulator thread)

	// Exceptions
	class area_error {};
	class file_open_error {};
	class file_read_error {};
	class rom_size_error {};
};

static BasiliskII *the_app;


// CPU and FPU type, addressing mode
int CPUType;
bool CPUIs68060;
int FPUType;
bool TwentyFourBitAddressing;


// Global variables for PowerROM CPU
thread_id emul_thread = -1;			// Emulator thread

#if __POWERPC__
int sheep_fd = -1;					// fd of sheep driver
#endif


/*
 *  Create application object and start it
 */

int main(int argc, char **argv)
{	
	the_app = new BasiliskII();
	the_app->Run();
	delete the_app;
	return 0;
}


/*
 *  Run application
 */

void BasiliskII::ReadyToRun(void)
{
	// Initialize variables
	RAMBaseHost = NULL;
	ROMBaseHost = NULL;
	srand(real_time_clock());
	tzset();

	// Print some info
	printf(GetString(STR_ABOUT_TEXT1), VERSION_MAJOR, VERSION_MINOR);
	printf(" %s\n", GetString(STR_ABOUT_TEXT2));

	// Delete old areas
	area_id old_ram_area = find_area(RAM_AREA_NAME);
	if (old_ram_area > 0)
		delete_area(old_ram_area);
	area_id old_rom_area = find_area(ROM_AREA_NAME);
	if (old_rom_area > 0)
		delete_area(old_rom_area);

	// Read preferences
	int argc = 0;
	char **argv = NULL;
	PrefsInit(argc, argv);

	// Init system routines
	SysInit();

	// Show preferences editor (or start emulator directly)
	if (!PrefsFindBool("nogui"))
		PrefsEditor();
	else
		PostMessage(MSG_START);
}


/*
 *  Message received
 */

void BasiliskII::MessageReceived(BMessage *msg)
{
	switch (msg->what) {
		case MSG_START:
			StartEmulator();
			break;
		default:
			BApplication::MessageReceived(msg);
	}
}


/*
 *  Start emulator
 */

void BasiliskII::StartEmulator(void)
{
	char str[256];

#if REAL_ADDRESSING
	// Open sheep driver and remap low memory
	sheep_fd = open("/dev/sheep", 0);
	if (sheep_fd < 0) {
		sprintf(str, GetString(STR_NO_SHEEP_DRIVER_ERR), strerror(sheep_fd), sheep_fd);
		ErrorAlert(str);
		PostMessage(B_QUIT_REQUESTED);
		return;
	}
	status_t res = ioctl(sheep_fd, SHEEP_UP);
	if (res < 0) {
		sprintf(str, GetString(STR_SHEEP_UP_ERR), strerror(res), res);
		ErrorAlert(str);
		PostMessage(B_QUIT_REQUESTED);
		return;
	}
#endif

	// Create area for Mac RAM
	RAMSize = PrefsFindInt32("ramsize") & 0xfff00000;	// Round down to 1MB boundary
	if (RAMSize < 1024*1024) {
		WarningAlert(GetString(STR_SMALL_RAM_WARN));
		RAMSize = 1024*1024;
	}

	RAMBaseHost = (uint8 *)0x10000000;
	ram_area = create_area(RAM_AREA_NAME, (void **)&RAMBaseHost, B_BASE_ADDRESS, RAMSize, B_NO_LOCK, B_READ_AREA | B_WRITE_AREA);
	if (ram_area < 0) {
		ErrorAlert(STR_NO_RAM_AREA_ERR);
		PostMessage(B_QUIT_REQUESTED);
		return;
	}
	D(bug("RAM area %ld at %p\n", ram_area, RAMBaseHost));

	// Create area and load Mac ROM
	try {
		init_rom();
	} catch (area_error) {
		ErrorAlert(STR_NO_ROM_AREA_ERR);
		PostMessage(B_QUIT_REQUESTED);
		return;
	} catch (file_open_error) {
		ErrorAlert(STR_NO_ROM_FILE_ERR);
		PostMessage(B_QUIT_REQUESTED);
		return;
	} catch (file_read_error) {
		ErrorAlert(STR_ROM_FILE_READ_ERR);
		PostMessage(B_QUIT_REQUESTED);
		return;
	} catch (rom_size_error) {
		ErrorAlert(STR_ROM_SIZE_ERR);
		PostMessage(B_QUIT_REQUESTED);
		return;
	}

	// Initialize everything
	if (!InitAll(NULL)) {
		PostMessage(B_QUIT_REQUESTED);
		return;
	}

	// Write protect ROM
	set_area_protection(rom_area, B_READ_AREA);

	// Disallow quitting with Alt-Q from now on
	AllowQuitting = false;

	// Start XPRAM watchdog thread
	xpram_thread = spawn_thread(xpram_func, "XPRAM Watchdog", B_LOW_PRIORITY, this);
	resume_thread(xpram_thread);

	// Start 60Hz interrupt
	tick_thread = spawn_thread(tick_func, "60Hz", B_REAL_TIME_PRIORITY, this);
	resume_thread(tick_thread);

	// Start emulator thread
	emul_thread = spawn_thread(emul_func, "MacOS", B_NORMAL_PRIORITY, this);
	resume_thread(emul_thread);
}


/*
 *  Quit emulator
 */

void QuitEmulator(void)
{
	the_app->AllowQuitting = true;
	be_app->PostMessage(B_QUIT_REQUESTED);
	exit_thread(0);
}

bool BasiliskII::QuitRequested(void)
{
	if (AllowQuitting)
		return BApplication::QuitRequested();
	else
		return false;
}

void BasiliskII::Quit(void)
{
	status_t l;

	// Stop 60Hz interrupt
	if (tick_thread > 0) {
		tick_thread_active = false;
		wait_for_thread(tick_thread, &l);
	}

	// Wait for emulator thread to finish
	if (emul_thread > 0)
		wait_for_thread(emul_thread, &l);

	// Exit 680x0 emulation
	Exit680x0();

	// Stop XPRAM watchdog thread
	if (xpram_thread > 0) {
		xpram_thread_active = false;
		suspend_thread(xpram_thread);	// Wake thread up from snooze()
		snooze(1000);
		resume_thread(xpram_thread);
		wait_for_thread(xpram_thread, &l);
	}

	// Deinitialize everything
	ExitAll();

	// Delete ROM area
	if (rom_area >= 0)
		delete_area(rom_area);

	// Delete RAM area
	if (ram_area >= 0)
		delete_area(ram_area);

#if REAL_ADDRESSING
	// Unmap low memory and close memory mess driver
	if (sheep_fd >= 0) {
		ioctl(sheep_fd, SHEEP_DOWN);
		close(sheep_fd);
	}
#endif

	// Exit system routines
	SysExit();

	// Exit preferences
	PrefsExit();

	BApplication::Quit();
}


/*
 *  Create area for ROM (sets rom_area) and load ROM file
 *
 *  area_error     : Cannot create area
 *  file_open_error: Cannot open ROM file
 *  file_read_error: Cannot read ROM file
 */

void BasiliskII::init_rom(void)
{
	// Create area for ROM
	ROMBaseHost = (uint8 *)0x40800000;
	rom_area = create_area(ROM_AREA_NAME, (void **)&ROMBaseHost, B_BASE_ADDRESS, ROM_AREA_SIZE, B_NO_LOCK, B_READ_AREA | B_WRITE_AREA);
	if (rom_area < 0)
		throw area_error();
	D(bug("ROM area %ld at %p\n", rom_area, ROMBaseHost));

	// Load ROM
	load_rom();
}


/*
 *  Load ROM file
 *
 *  file_open_error: Cannot open ROM file
 *  file_read_error: Cannot read ROM file
 */

void BasiliskII::load_rom(void)
{
	// Get rom file path from preferences
	const char *rom_path = PrefsFindString("rom");

	// Try to open ROM file
	BFile file(rom_path ? rom_path : ROM_FILE_NAME, B_READ_ONLY);
	if (file.InitCheck() != B_NO_ERROR)
		throw file_open_error();

	printf(GetString(STR_READING_ROM_FILE));

	// Is the ROM size correct?
	off_t rom_size = 0;
	file.GetSize(&rom_size);
	if (rom_size != 64*1024 && rom_size != 128*1024 && rom_size != 256*1024 && rom_size != 512*1024 && rom_size != 1024*1024)
		throw rom_size_error();

	uint8 *rom = new uint8[rom_size];	// Reading directly into the area doesn't work
	ssize_t actual = file.Read((void *)rom, rom_size);
	if (actual == rom_size)
		memcpy(ROMBaseHost, rom, rom_size);
	delete[] rom;
	if (actual != rom_size)
		throw file_read_error();
	ROMSize = rom_size;
}


/*
 *  Emulator thread function
 */

status_t BasiliskII::emul_func(void *arg)
{
	BasiliskII *obj = (BasiliskII *)arg;

#if __POWERPC__
	// Install data access signal handler
	sigemptyset(&obj->sigsegv_action.sa_mask);
	obj->sigsegv_action.sa_handler = (__signal_func_ptr)(obj->sigsegv_invoc);
	obj->sigsegv_action.sa_flags = 0;
	obj->sigsegv_action.sa_userdata = arg;
	sigaction(SIGSEGV, &obj->sigsegv_action, NULL);
#endif

	// Exceptions will send signals
	disable_debugger(true);

	// Start 68k and jump to ROM boot routine
	Start680x0();

	// Quit program
	obj->AllowQuitting = true;
	be_app->PostMessage(B_QUIT_REQUESTED);
	return 0;
}


/*
 *  Code was patched, flush caches if neccessary (i.e. when using a real 680x0
 *  or a dynamically recompiling emulator)
 */

void FlushCodeCache(void *start, uint32 size)
{
}


/*
 *  Mutexes
 */

struct B2_mutex {
	int dummy;	//!!
};

B2_mutex *B2_create_mutex(void)
{
	return new B2_mutex;
}

void B2_lock_mutex(B2_mutex *mutex)
{
}

void B2_unlock_mutex(B2_mutex *mutex)
{
}

void B2_delete_mutex(B2_mutex *mutex)
{
	delete mutex;
}


/*
 *  Interrupt flags (must be handled atomically!)
 */

uint32 InterruptFlags = 0;

void SetInterruptFlag(uint32 flag)
{
	atomic_or((int32 *)&InterruptFlags, flag);
}

void ClearInterruptFlag(uint32 flag)
{
	atomic_and((int32 *)&InterruptFlags, ~flag);
}


/*
 *  60Hz thread (really 60.15Hz)
 */

status_t BasiliskII::tick_func(void *arg)
{
	BasiliskII *obj = (BasiliskII *)arg;
	int tick_counter = 0;
	bigtime_t current = system_time();

	while (obj->tick_thread_active) {

		// Wait
		current += 16625;
		snooze_until(current, B_SYSTEM_TIMEBASE);

		// Pseudo Mac 1Hz interrupt, update local time
		if (++tick_counter > 60) {
			tick_counter = 0;
			WriteMacInt32(0x20c, TimerDateTime());
			SetInterruptFlag(INTFLAG_1HZ);
			TriggerInterrupt();
		}

		// Trigger 60Hz interrupt
		SetInterruptFlag(INTFLAG_60HZ);
		TriggerInterrupt();
	}
	return 0;
}


/*
 *  XPRAM watchdog thread (saves XPRAM every minute)
 */

status_t BasiliskII::xpram_func(void *arg)
{
	uint8 last_xpram[XPRAM_SIZE];
	memcpy(last_xpram, XPRAM, XPRAM_SIZE);

	while (((BasiliskII *)arg)->xpram_thread_active) {
		snooze(60*1000000);
		if (memcmp(last_xpram, XPRAM, XPRAM_SIZE)) {
			memcpy(last_xpram, XPRAM, XPRAM_SIZE);
			SaveXPRAM();
		}
	}
	return 0;
}


/*
 *  Display error alert
 */

void ErrorAlert(const char *text)
{
	if (PrefsFindBool("nogui")) {
		printf(GetString(STR_SHELL_ERROR_PREFIX), text);
		return;
	}
	VideoQuitFullScreen();
	char str[256];
	sprintf(str, GetString(STR_GUI_ERROR_PREFIX), text);
	BAlert *alert = new BAlert(GetString(STR_ERROR_ALERT_TITLE), str, GetString(STR_QUIT_BUTTON), NULL, NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT);
	alert->Go();
}


/*
 *  Display warning alert
 */

void WarningAlert(const char *text)
{
	if (PrefsFindBool("nogui")) {
		printf(GetString(STR_SHELL_WARNING_PREFIX), text);
		return;
	}
	char str[256];
	sprintf(str, GetString(STR_GUI_WARNING_PREFIX), text);
	BAlert *alert = new BAlert(GetString(STR_WARNING_ALERT_TITLE), str, GetString(STR_OK_BUTTON), NULL, NULL, B_WIDTH_AS_USUAL, B_INFO_ALERT);
	alert->Go();
}


/*
 *  Display choice alert
 */

bool ChoiceAlert(const char *text, const char *pos, const char *neg)
{
	char str[256];
	sprintf(str, GetString(STR_GUI_WARNING_PREFIX), text);
	BAlert *alert = new BAlert(GetString(STR_WARNING_ALERT_TITLE), str, pos, neg, NULL, B_WIDTH_AS_USUAL, B_INFO_ALERT);
	return alert->Go() == 0;
}


/*
 *  SEGV handler
 */

#if __POWERPC__
static uint32 segv_r[32];

asm void BasiliskII::sigsegv_invoc(register int sig, register void *arg, register vregs *r)
{
	mflr	r0
	stw		r0,8(r1)
	stwu	r1,-56(r1)

	lwz		r3,segv_r(r2)
	stmw	r13,13*4(r3)

	mr		r3,r5
	bl		sigsegv_handler

	lwz		r3,segv_r(r2)
	lmw		r13,13*4(r3)

	lwz		r0,56+8(r1)
	mtlr	r0
	addi	r1,r1,56
	blr
}

static void sigsegv_handler(vregs *r)
{
	// Fetch volatile registers
	segv_r[0] = r->r0;
	segv_r[1] = r->r1;
	segv_r[2] = r->r2;
	segv_r[3] = r->r3;
	segv_r[4] = r->r4;
	segv_r[5] = r->r5;
	segv_r[6] = r->r6;
	segv_r[7] = r->r7;
	segv_r[8] = r->r8;
	segv_r[9] = r->r9;
	segv_r[10] = r->r10;
	segv_r[11] = r->r11;
	segv_r[12] = r->r12;

	// Get opcode and divide into fields
	uint32 opcode = *(uint32 *)r->pc;
	uint32 primop = opcode >> 26;
	uint32 exop = (opcode >> 1) & 0x3ff;
	uint32 ra = (opcode >> 16) & 0x1f;
	uint32 rb = (opcode >> 11) & 0x1f;
	uint32 rd = (opcode >> 21) & 0x1f;
	uint32 imm = opcode & 0xffff;

	// Analyze opcode
	enum {
		TYPE_UNKNOWN,
		TYPE_LOAD,
		TYPE_STORE
	} transfer_type = TYPE_UNKNOWN;
	enum {
		SIZE_UNKNOWN,
		SIZE_BYTE,
		SIZE_HALFWORD,
		SIZE_WORD
	} transfer_size = SIZE_UNKNOWN;
	enum {
		MODE_UNKNOWN,
		MODE_NORM,
		MODE_U,
		MODE_X,
		MODE_UX
	} addr_mode = MODE_UNKNOWN;
	switch (primop) {
		case 31:
			switch (exop) {
				case 23:	// lwzx
					transfer_type = TYPE_LOAD; transfer_size = SIZE_WORD; addr_mode = MODE_X; break;
				case 55:	// lwzux
					transfer_type = TYPE_LOAD; transfer_size = SIZE_WORD; addr_mode = MODE_UX; break;
				case 87:	// lbzx
					transfer_type = TYPE_LOAD; transfer_size = SIZE_BYTE; addr_mode = MODE_X; break;
				case 119:	// lbzux
					transfer_type = TYPE_LOAD; transfer_size = SIZE_BYTE; addr_mode = MODE_UX; break;
				case 151:	// stwx
					transfer_type = TYPE_STORE; transfer_size = SIZE_WORD; addr_mode = MODE_X; break;
				case 183:	// stwux
					transfer_type = TYPE_STORE; transfer_size = SIZE_WORD; addr_mode = MODE_UX; break;
				case 215:	// stbx
					transfer_type = TYPE_STORE; transfer_size = SIZE_BYTE; addr_mode = MODE_X; break;
				case 247:	// stbux
					transfer_type = TYPE_STORE; transfer_size = SIZE_BYTE; addr_mode = MODE_UX; break;
				case 279:	// lhzx
					transfer_type = TYPE_LOAD; transfer_size = SIZE_HALFWORD; addr_mode = MODE_X; break;
				case 311:	// lhzux
					transfer_type = TYPE_LOAD; transfer_size = SIZE_HALFWORD; addr_mode = MODE_UX; break;
				case 343:	// lhax
					transfer_type = TYPE_LOAD; transfer_size = SIZE_HALFWORD; addr_mode = MODE_X; break;
				case 375:	// lhaux
					transfer_type = TYPE_LOAD; transfer_size = SIZE_HALFWORD; addr_mode = MODE_UX; break;
				case 407:	// sthx
					transfer_type = TYPE_STORE; transfer_size = SIZE_HALFWORD; addr_mode = MODE_X; break;
				case 439:	// sthux
					transfer_type = TYPE_STORE; transfer_size = SIZE_HALFWORD; addr_mode = MODE_UX; break;
			}
			break;

		case 32:	// lwz
			transfer_type = TYPE_LOAD; transfer_size = SIZE_WORD; addr_mode = MODE_NORM; break;
		case 33:	// lwzu
			transfer_type = TYPE_LOAD; transfer_size = SIZE_WORD; addr_mode = MODE_U; break;
		case 34:	// lbz
			transfer_type = TYPE_LOAD; transfer_size = SIZE_BYTE; addr_mode = MODE_NORM; break;
		case 35:	// lbzu
			transfer_type = TYPE_LOAD; transfer_size = SIZE_BYTE; addr_mode = MODE_U; break;
		case 36:	// stw
			transfer_type = TYPE_STORE; transfer_size = SIZE_WORD; addr_mode = MODE_NORM; break;
		case 37:	// stwu
			transfer_type = TYPE_STORE; transfer_size = SIZE_WORD; addr_mode = MODE_U; break;
		case 38:	// stb
			transfer_type = TYPE_STORE; transfer_size = SIZE_BYTE; addr_mode = MODE_NORM; break;
		case 39:	// stbu
			transfer_type = TYPE_STORE; transfer_size = SIZE_BYTE; addr_mode = MODE_U; break;
		case 40:	// lhz
			transfer_type = TYPE_LOAD; transfer_size = SIZE_HALFWORD; addr_mode = MODE_NORM; break;
		case 41:	// lhzu
			transfer_type = TYPE_LOAD; transfer_size = SIZE_HALFWORD; addr_mode = MODE_U; break;
		case 42:	// lha
			transfer_type = TYPE_LOAD; transfer_size = SIZE_HALFWORD; addr_mode = MODE_NORM; break;
		case 43:	// lhau
			transfer_type = TYPE_LOAD; transfer_size = SIZE_HALFWORD; addr_mode = MODE_U; break;
		case 44:	// sth
			transfer_type = TYPE_STORE; transfer_size = SIZE_HALFWORD; addr_mode = MODE_NORM; break;
		case 45:	// sthu
			transfer_type = TYPE_STORE; transfer_size = SIZE_HALFWORD; addr_mode = MODE_U; break;
	}

	// Calculate effective address
	uint32 addr = 0;
	switch (addr_mode) {
		case MODE_X:
		case MODE_UX:
			if (ra == 0)
				addr = segv_r[rb];
			else
				addr = segv_r[ra] + segv_r[rb];
			break;
		case MODE_NORM:
		case MODE_U:
			if (ra == 0)
				addr = (int32)(int16)imm;
			else
				addr = segv_r[ra] + (int32)(int16)imm;
			break;
	}

	// Ignore ROM writes
	if (transfer_type == TYPE_STORE && addr >= (uint32)ROMBaseHost && addr < (uint32)ROMBaseHost + ROMSize) {
//		D(bug("WARNING: %s write access to ROM at %p, 68k pc %p\n", transfer_size == SIZE_BYTE ? "Byte" : transfer_size == SIZE_HALFWORD ? "Halfword" : "Word", addr, r->pc));
		if (addr_mode == MODE_U || addr_mode == MODE_UX)
			segv_r[ra] = addr;
		r->pc += 4;
		goto rti;
	}

	// For all other errors, jump into debugger
	char str[256];
	sprintf(str, "SIGSEGV\n"
		"   pc %08lx     lr %08lx    ctr %08lx    msr %08lx\n"
		"  xer %08lx     cr %08lx  fpscr %08lx\n"
		"   r0 %08lx     r1 %08lx     r2 %08lx     r3 %08lx\n"
		"   r4 %08lx     r5 %08lx     r6 %08lx     r7 %08lx\n"
		"   r8 %08lx     r9 %08lx    r10 %08lx    r11 %08lx\n"
		"  r12 %08lx    r13 %08lx    r14 %08lx    r15 %08lx\n"
		"  r16 %08lx    r17 %08lx    r18 %08lx    r19 %08lx\n"
		"  r20 %08lx    r21 %08lx    r22 %08lx    r23 %08lx\n"
		"  r24 %08lx    r25 %08lx    r26 %08lx    r27 %08lx\n"
		"  r28 %08lx    r29 %08lx    r30 %08lx    r31 %08lx\n",
		r->pc, r->lr, r->ctr, r->msr,
		r->xer, r->cr, r->fpscr,
		r->r0, r->r1, r->r2, r->r3,
		r->r4, r->r5, r->r6, r->r7,
		r->r8, r->r9, r->r10, r->r11,
		r->r12, segv_r[13], segv_r[14], segv_r[15],
		segv_r[16], segv_r[17], segv_r[18], segv_r[19],
		segv_r[20], segv_r[21], segv_r[22], segv_r[23],
		segv_r[24], segv_r[25], segv_r[26], segv_r[27],
		segv_r[28], segv_r[29], segv_r[30], segv_r[31]);
	disable_debugger(false);
	debugger(str);
	QuitEmulator();
	return;

rti:
	// Restore volatile registers
	r->r0 = segv_r[0];
	r->r1 = segv_r[1];
	r->r2 = segv_r[2];
	r->r3 = segv_r[3];
	r->r4 = segv_r[4];
	r->r5 = segv_r[5];
	r->r6 = segv_r[6];
	r->r7 = segv_r[7];
	r->r8 = segv_r[8];
	r->r9 = segv_r[9];
	r->r10 = segv_r[10];
	r->r11 = segv_r[11];
	r->r12 = segv_r[12];
}
#endif
