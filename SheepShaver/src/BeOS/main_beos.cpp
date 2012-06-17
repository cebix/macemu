/*
 *  main_beos.cpp - Emulation core, BeOS implementation
 *
 *  SheepShaver (C) 1997-2008 Christian Bauer and Marc Hellwig
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
 *
 *  SheepShaver uses three run-time environments, reflected by the value of XLM_RUN_MODE.
 *  The two modes which are also present in the original MacOS, are:
 *    MODE_68K - 68k emulator is active
 *    MODE_NATIVE - 68k emulator is inactive
 *  In the original MacOS, these two modes have different memory mappings and exception
 *  tables. Under SheepShaver, the only difference is the handling of interrupts (see below).
 *  SheepShaver extends the 68k emulator with special opcodes (EMUL_OP) to perform faster
 *  mode switches when patching 68k routines with PowerPC code and adds a third run mode:
 *    MODE_EMUL_OP - 68k emulator active, but native register usage
 *
 *  Switches between MODE_68K and MODE_NATIVE are only done with the Mixed Mode Manager
 *  (via nanokernel patches). The switch from MODE_68K to MODE_EMUL_OP occurs when executin
 *  one of the EMUL_OP 68k opcodes. When the opcode routine is done, it returns to MODE_68K.
 *
 *  The Execute68k() routine allows EMUL_OP routines to execute 68k subroutines. It switches
 *  from MODE_EMUL_OP back to MODE_68K, so it must not be used by native routines (executing
 *  in MODE_NATIVE) nor by any other thread than the emul_thread (because the 68k emulator
 *  is not reentrant). When the 68k subroutine returns, it switches back to MODE_EMUL_OP.
 *  It is OK for a 68k routine called with Execute68k() to contain an EMUL_OP opcode.
 *
 *  The handling of interrupts depends on the current run mode:
 *    MODE_68K - The USR1 signal handler sets one bit in the processor's CR. The 68k emulator
 *      will then execute the 68k interrupt routine when fetching the next instruction.
 *    MODE_NATIVE - The USR1 signal handler switches back to the original stack (signals run
 *      on a separate signal stack) and enters the External Interrupt routine in the
 *      nanokernel.
 *    MODE_EMUL_OP - The USR1 signal handler directly executes the 68k interrupt routine
 *      with Execute68k(). Before doing this, it must first check the current 68k interrupt
 *      level which is stored in XLM_68K_R25. This variable is set to the current level
 *      when entering EMUL_OP mode. Execute68k() also uses it to restore the level so that
 *      Execute68k()'d routines will run at the same interrupt level as the EMUL_OP routine
 *      it was called from.
 */

#include <Path.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sysdeps.h"
#include "main.h"
#include "version.h"
#include "prefs.h"
#include "prefs_editor.h"
#include "cpu_emulation.h"
#include "emul_op.h"
#include "xlowmem.h"
#include "xpram.h"
#include "timer.h"
#include "adb.h"
#include "video.h"
#include "sys.h"
#include "macos_util.h"
#include "rom_patches.h"
#include "user_strings.h"

#include "sheep_driver.h"

#define DEBUG 0
#include "debug.h"

// Enable Execute68k() safety checks?
#define SAFE_EXEC_68K 0

// Save FP regs in Execute68k()?
#define SAVE_FP_EXEC_68K 0

// Interrupts in EMUL_OP mode?
#define INTERRUPTS_IN_EMUL_OP_MODE 1

// Interrupts in native mode?
#define INTERRUPTS_IN_NATIVE_MODE 1


// Constants
const char APP_SIGNATURE[] = "application/x-vnd.cebix-SheepShaver";
const char ROM_FILE_NAME[] = "ROM";
const char ROM_FILE_NAME2[] = "Mac OS ROM";
const char KERNEL_AREA_NAME[] = "Macintosh Kernel Data";
const char KERNEL_AREA2_NAME[] = "Macintosh Kernel Data 2";
const char RAM_AREA_NAME[] = "Macintosh RAM";
const char ROM_AREA_NAME[] = "Macintosh ROM";
const char DR_CACHE_AREA_NAME[] = "Macintosh DR Cache";
const char DR_EMULATOR_AREA_NAME[] = "Macintosh DR Emulator";
const char SHEEP_AREA_NAME[] = "SheepShaver Virtual Stack";

const uintptr ROM_BASE = 0x40800000;		// Base address of ROM

const uint32 SIG_STACK_SIZE = 8192;			// Size of signal stack

const uint32 MSG_START = 'strt';			// Emulator start message


// Application object
class SheepShaver : public BApplication {
public:
	SheepShaver() : BApplication(APP_SIGNATURE)
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
		sheep_fd = -1;
		emulator_data = NULL;
		kernel_area = kernel_area2 = rom_area = ram_area = dr_cache_area = dr_emulator_area = -1;
		emul_thread = nvram_thread = tick_thread = -1;
		ReadyForSignals = false;
		AllowQuitting = true;
		NVRAMThreadActive = true;
		TickThreadActive = true;
		memset(last_xpram, 0, XPRAM_SIZE);
	}
	virtual void ReadyToRun(void);
	virtual void MessageReceived(BMessage *msg);
	void StartEmulator(void);
	virtual bool QuitRequested(void);
	virtual void Quit(void);

	thread_id emul_thread;		// Emulator thread
	thread_id nvram_thread;		// NVRAM watchdog thread
	thread_id tick_thread;		// 60Hz thread

	KernelData *kernel_data;	// Pointer to Kernel Data
	EmulatorData *emulator_data;

	bool ReadyForSignals;		// Flag: emul_thread ready to receive signals
	bool AllowQuitting;			// Flag: Alt-Q quitting allowed
	bool NVRAMThreadActive;		// nvram_thread will exit when this is false
	bool TickThreadActive;		// tick_thread will exit when this is false

	uint8 last_xpram[XPRAM_SIZE]; // Buffer for monitoring XPRAM changes

private:
	static status_t emul_func(void *arg);
	static status_t nvram_func(void *arg);
	static status_t tick_func(void *arg);
	static void sigusr1_invoc(int sig, void *arg, vregs *r);
	void sigusr1_handler(vregs *r);
	static void sigsegv_invoc(int sig, void *arg, vregs *r);
	static void sigill_invoc(int sig, void *arg, vregs *r);
	void jump_to_rom(uint32 entry);

	void init_rom(void);
	void load_rom(void);

	int sheep_fd;			// FD of sheep driver

	area_id kernel_area;	// Kernel Data area ID
	area_id kernel_area2;	// Alternate Kernel Data area ID
	area_id rom_area;		// ROM area ID
	area_id ram_area;		// RAM area ID
	area_id dr_cache_area;	// DR Cache area ID
	area_id dr_emulator_area;	// DR Emulator area ID

	struct sigaction sigusr1_action;	// Interrupt signal (of emulator thread)
	struct sigaction sigsegv_action;	// Data access exception signal (of emulator thread)
	struct sigaction sigill_action;		// Illegal instruction exception signal (of emulator thread)

	// Exceptions
	class area_error {};
	class file_open_error {};
	class file_read_error {};
	class rom_size_error {};
};


// Global variables
SheepShaver *the_app;	// Pointer to application object
#if !EMULATED_PPC
void *TOC;				// TOC pointer
#endif
uint32 RAMBase;			// Base address of Mac RAM
uint32 RAMSize;			// Size of Mac RAM
uint32 ROMBase;			// Base address of Mac ROM
uint32 KernelDataAddr;	// Address of Kernel Data
uint32 BootGlobsAddr;	// Address of BootGlobs structure at top of Mac RAM
uint32 DRCacheAddr;		// Address of DR Cache
uint32 DREmulatorAddr;	// Address of DR Emulator
uint32 PVR;				// Theoretical PVR
int64 CPUClockSpeed;	// Processor clock speed (Hz)
int64 BusClockSpeed;	// Bus clock speed (Hz)
int64 TimebaseSpeed;	// Timebase clock speed (Hz)
system_info SysInfo;	// System information
uint8 *RAMBaseHost;		// Base address of Mac RAM (host address space)
uint8 *ROMBaseHost;		// Base address of Mac ROM (host address space)

static void *sig_stack = NULL;		// Stack for signal handlers
static void *extra_stack = NULL;	// Stack for SIGSEGV inside interrupt handler
uint32  SheepMem::page_size;		// Size of a native page
uintptr SheepMem::zero_page = 0;	// Address of ro page filled in with zeros
uintptr SheepMem::base;				// Address of SheepShaver data
uintptr SheepMem::proc;				// Bottom address of SheepShave procedures
uintptr SheepMem::data;				// Top of SheepShaver data (stack like storage)
static area_id SheepMemArea;		// SheepShaver data area ID


// Prototypes
static void sigsegv_handler(vregs *r);
static void sigill_handler(vregs *r);


/*
 *  Create application object and start it
 */

int main(int argc, char **argv)
{	
	tzset();
	the_app = new SheepShaver();
	the_app->Run();
	delete the_app;
	return 0;
}


/*
 *  Run application
 */

#if !EMULATED_PPC
static asm void *get_toc(void)
{
	mr	r3,r2
	blr
}
#endif

void SheepShaver::ReadyToRun(void)
{
	// Print some info
	printf(GetString(STR_ABOUT_TEXT1), VERSION_MAJOR, VERSION_MINOR);
	printf(" %s\n", GetString(STR_ABOUT_TEXT2));

#if !EMULATED_PPC
	// Get TOC pointer
	TOC = get_toc();
#endif

	// Get system info
	get_system_info(&SysInfo);
	switch (SysInfo.cpu_type) {
		case B_CPU_PPC_601:
			PVR = 0x00010000;
			break;
		case B_CPU_PPC_603:
			PVR = 0x00030000;
			break;
		case B_CPU_PPC_603e:
			PVR = 0x00060000;
			break;
		case B_CPU_PPC_604:
			PVR = 0x00040000;
			break;
		case B_CPU_PPC_604e:
			PVR = 0x00090000;
			break;
		case B_CPU_PPC_750:
			PVR = 0x00080000;
			break;
		default:
			PVR = 0x00040000;
			break;
	}
	CPUClockSpeed = SysInfo.cpu_clock_speed;
	BusClockSpeed = SysInfo.bus_clock_speed;
	TimebaseSpeed = BusClockSpeed / 4;

	// Delete old areas
	area_id old_kernel_area = find_area(KERNEL_AREA_NAME);
	if (old_kernel_area > 0)
		delete_area(old_kernel_area);
	area_id old_kernel2_area = find_area(KERNEL_AREA2_NAME);
	if (old_kernel2_area > 0)
		delete_area(old_kernel2_area);
	area_id old_ram_area = find_area(RAM_AREA_NAME);
	if (old_ram_area > 0)
		delete_area(old_ram_area);
	area_id old_rom_area = find_area(ROM_AREA_NAME);
	if (old_rom_area > 0)
		delete_area(old_rom_area);
	area_id old_dr_cache_area = find_area(DR_CACHE_AREA_NAME);
	if (old_dr_cache_area > 0)
		delete_area(old_dr_cache_area);
	area_id old_dr_emulator_area = find_area(DR_EMULATOR_AREA_NAME);
	if (old_dr_emulator_area > 0)
		delete_area(old_dr_emulator_area);

	// Read preferences
	int argc = 0;
	char **argv = NULL;
	PrefsInit(NULL, argc, argv);

	// Init system routines
	SysInit();

	// Test amount of RAM available for areas
	if (SysInfo.max_pages * B_PAGE_SIZE < 16 * 1024 * 1024) {
		ErrorAlert(GetString(STR_NOT_ENOUGH_MEMORY_ERR));
		PostMessage(B_QUIT_REQUESTED);
		return;
	}

	// Show preferences editor (or start emulator directly)
	if (!PrefsFindBool("nogui"))
		PrefsEditor(MSG_START);
	else
		PostMessage(MSG_START);
}


/*
 *  Message received
 */

void SheepShaver::MessageReceived(BMessage *msg)
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

void SheepShaver::StartEmulator(void)
{
	char str[256];

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

	// Create areas for Kernel Data
	kernel_data = (KernelData *)KERNEL_DATA_BASE;
	kernel_area = create_area(KERNEL_AREA_NAME, &kernel_data, B_EXACT_ADDRESS, KERNEL_AREA_SIZE, B_NO_LOCK, B_READ_AREA | B_WRITE_AREA);
	if (kernel_area < 0) {
		sprintf(str, GetString(STR_NO_KERNEL_DATA_ERR), strerror(kernel_area), kernel_area);
		ErrorAlert(str);
		PostMessage(B_QUIT_REQUESTED);
		return;
	}
	emulator_data = &kernel_data->ed;
	KernelDataAddr = (uint32)kernel_data;
	D(bug("Kernel Data area %ld at %p, Emulator Data at %p\n", kernel_area, kernel_data, emulator_data));

	void *kernel_data2 = (void *)KERNEL_DATA2_BASE;
	kernel_area2 = clone_area(KERNEL_AREA2_NAME, &kernel_data2, B_EXACT_ADDRESS, B_READ_AREA | B_WRITE_AREA, kernel_area);
	if (kernel_area2 < 0) {
		sprintf(str, GetString(STR_NO_KERNEL_DATA_ERR), strerror(kernel_area2), kernel_area2);
		ErrorAlert(str);
		PostMessage(B_QUIT_REQUESTED);
		return;
	}
	D(bug("Kernel Data 2 area %ld at %p\n", kernel_area2, kernel_data2));

	// Create area for SheepShaver data
	if (!SheepMem::Init()) {
		sprintf(str, GetString(STR_NO_SHEEP_MEM_AREA_ERR), strerror(SheepMemArea), SheepMemArea);
		ErrorAlert(str);
		PostMessage(B_QUIT_REQUESTED);
		return;
	}
	
	// Create area for Mac RAM
	RAMSize = PrefsFindInt32("ramsize") & 0xfff00000;	// Round down to 1MB boundary
	if (RAMSize < 8*1024*1024) {
		WarningAlert(GetString(STR_SMALL_RAM_WARN));
		RAMSize = 8*1024*1024;
	}

	RAMBase = 0x10000000;
	ram_area = create_area(RAM_AREA_NAME, (void **)&RAMBase, B_BASE_ADDRESS, RAMSize, B_NO_LOCK, B_READ_AREA | B_WRITE_AREA);
	if (ram_area < 0) {
		sprintf(str, GetString(STR_NO_RAM_AREA_ERR), strerror(ram_area), ram_area);
		ErrorAlert(str);
		PostMessage(B_QUIT_REQUESTED);
		return;
	}
	RAMBaseHost = (uint8 *)RAMBase;
	D(bug("RAM area %ld at %p\n", ram_area, RAMBaseHost));

	// Create area and load Mac ROM
	try {
		init_rom();
	} catch (area_error) {
		ErrorAlert(GetString(STR_NO_ROM_AREA_ERR));
		PostMessage(B_QUIT_REQUESTED);
		return;
	} catch (file_open_error) {
		ErrorAlert(GetString(STR_NO_ROM_FILE_ERR));
		PostMessage(B_QUIT_REQUESTED);
		return;
	} catch (file_read_error) {
		ErrorAlert(GetString(STR_ROM_FILE_READ_ERR));
		PostMessage(B_QUIT_REQUESTED);
		return;
	} catch (rom_size_error) {
		ErrorAlert(GetString(STR_ROM_SIZE_ERR));
		PostMessage(B_QUIT_REQUESTED);
		return;
	}

	// Create area for DR Cache
	DRCacheAddr = DR_CACHE_BASE;
	dr_cache_area = create_area(DR_CACHE_AREA_NAME, (void **)&DRCacheAddr, B_EXACT_ADDRESS, DR_CACHE_SIZE, B_NO_LOCK, B_READ_AREA | B_WRITE_AREA);
	if (dr_cache_area < 0) {
		sprintf(str, GetString(STR_NO_KERNEL_DATA_ERR), strerror(dr_cache_area), dr_cache_area);
		ErrorAlert(str);
		PostMessage(B_QUIT_REQUESTED);
		return;
	}
	D(bug("DR Cache area %ld at %p\n", dr_cache_area, DRCacheAddr));

	// Create area for DR Emulator
	DREmulatorAddr = DR_EMULATOR_BASE;
	dr_emulator_area = create_area(DR_EMULATOR_AREA_NAME, (void **)&DREmulatorAddr, B_EXACT_ADDRESS, DR_EMULATOR_SIZE, B_NO_LOCK, B_READ_AREA | B_WRITE_AREA);
	if (dr_emulator_area < 0) {
		sprintf(str, GetString(STR_NO_KERNEL_DATA_ERR), strerror(dr_emulator_area), dr_emulator_area);
		ErrorAlert(str);
		PostMessage(B_QUIT_REQUESTED);
		return;
	}
	D(bug("DR Emulator area %ld at %p\n", dr_emulator_area, DREmulatorAddr));

	// Initialize everything
	if (!InitAll(NULL)) {
		PostMessage(B_QUIT_REQUESTED);
		return;
	}
	D(bug("Initialization complete\n"));

	// Clear caches (as we loaded and patched code) and write protect ROM
#if !EMULATED_PPC
	clear_caches(ROMBaseHost, ROM_AREA_SIZE, B_INVALIDATE_ICACHE | B_FLUSH_DCACHE);
#endif
	set_area_protection(rom_area, B_READ_AREA);

	// Initialize extra low memory
	D(bug("Initializing extra Low Memory...\n"));
	WriteMacInt32(XLM_SHEEP_OBJ, (uint32)this);						// Pointer to SheepShaver object
	D(bug("Extra Low Memory initialized\n"));

	// Disallow quitting with Alt-Q from now on
	AllowQuitting = false;

	// Start 60Hz interrupt
	tick_thread = spawn_thread(tick_func, "60Hz", B_URGENT_DISPLAY_PRIORITY, this);
	resume_thread(tick_thread);

	// Start NVRAM watchdog thread
	memcpy(last_xpram, XPRAM, XPRAM_SIZE);
	nvram_thread = spawn_thread(nvram_func, "NVRAM Watchdog", B_LOW_PRIORITY, this);
	resume_thread(nvram_thread);

	// Start emulator thread
	emul_thread = spawn_thread(emul_func, "MacOS", B_NORMAL_PRIORITY, this);
	resume_thread(emul_thread);
}


/*
 *  Quit requested
 */

bool SheepShaver::QuitRequested(void)
{
	if (AllowQuitting)
		return BApplication::QuitRequested();
	else
		return false;
}

void SheepShaver::Quit(void)
{
	status_t l;

	// Stop 60Hz interrupt
	if (tick_thread > 0) {
		TickThreadActive = false;
		wait_for_thread(tick_thread, &l);
	}

	// Stop NVRAM watchdog
	if (nvram_thread > 0) {
		status_t l;
		NVRAMThreadActive = false;
		suspend_thread(nvram_thread);	// Wake thread up from snooze()
		snooze(1000);
		resume_thread(nvram_thread);
		while (wait_for_thread(nvram_thread, &l) == B_INTERRUPTED) ;
	}

	// Wait for emulator thread to finish
	if (emul_thread > 0)
		wait_for_thread(emul_thread, &l);

	// Deinitialize everything
	ExitAll();

	// Delete SheepShaver globals
	SheepMem::Exit();

	// Delete DR Emulator area
	if (dr_emulator_area >= 0)
		delete_area(dr_emulator_area);

	// Delete DR Cache area
	if (dr_cache_area >= 0)
		delete_area(dr_cache_area);

	// Delete ROM area
	if (rom_area >= 0)
		delete_area(rom_area);

	// Delete RAM area
	if (ram_area >= 0)
		delete_area(ram_area);

	// Delete Kernel Data area2
	if (kernel_area2 >= 0)
		delete_area(kernel_area2);
	if (kernel_area >= 0)
		delete_area(kernel_area);

	// Unmap low memory and close sheep driver
	if (sheep_fd >= 0) {
		ioctl(sheep_fd, SHEEP_DOWN);
		close(sheep_fd);
	}

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

void SheepShaver::init_rom(void)
{
	// Size of a native page
	page_size = B_PAGE_SIZE;

	// Create area for ROM
	ROMBase = ROM_BASE;
	rom_area = create_area(ROM_AREA_NAME, (void **)&ROMBase, B_EXACT_ADDRESS, ROM_AREA_SIZE, B_NO_LOCK, B_READ_AREA | B_WRITE_AREA);
	if (rom_area < 0)
		throw area_error();
	ROMBaseHost = (uint8 *)ROMBase;
	D(bug("ROM area %ld at %p\n", rom_area, rom_addr));

	// Load ROM
	load_rom();
}


/*
 *  Load ROM file
 *
 *  file_open_error: Cannot open ROM file (nor use built-in ROM)
 *  file_read_error: Cannot read ROM file
 */

void SheepShaver::load_rom(void)
{
	// Get rom file path from preferences
	const char *rom_path = PrefsFindString("rom");

	// Try to open ROM file
	BFile file(rom_path && *rom_path ? rom_path : ROM_FILE_NAME, B_READ_ONLY);
	if (file.InitCheck() != B_NO_ERROR) {

		// Failed, then ask memory_mess driver for ROM
		uint8 *rom = new uint8[ROM_SIZE];	// Reading directly into the area doesn't work
		ssize_t actual = read(sheep_fd, (void *)rom, ROM_SIZE);
		if (actual == ROM_SIZE) {
			memcpy(ROMBaseHost, rom, ROM_SIZE);
			delete[] rom;
			return;
		} else
			throw file_open_error();
	}

	printf(GetString(STR_READING_ROM_FILE));

	// Get file size
	off_t rom_size = 0;
	file.GetSize(&rom_size);

	uint8 *rom = new uint8[ROM_SIZE];	// Reading directly into the area doesn't work
	ssize_t actual = file.Read((void *)rom, ROM_SIZE);
	
	// Decode Mac ROM
	if (!DecodeROM(rom, actual)) {
		if (rom_size != 4*1024*1024)
			throw rom_size_error();
		else
			throw file_read_error();
	}
	delete[] rom;
}


/*
 *  Emulator thread function
 */

status_t SheepShaver::emul_func(void *arg)
{
	SheepShaver *obj = (SheepShaver *)arg;

	// Install interrupt signal handler
	sigemptyset(&obj->sigusr1_action.sa_mask);
	obj->sigusr1_action.sa_handler = (__signal_func_ptr)(obj->sigusr1_invoc);
	obj->sigusr1_action.sa_flags = 0;
	obj->sigusr1_action.sa_userdata = arg;
	sigaction(SIGUSR1, &obj->sigusr1_action, NULL);

	// Install data access signal handler
	sigemptyset(&obj->sigsegv_action.sa_mask);
	obj->sigsegv_action.sa_handler = (__signal_func_ptr)(obj->sigsegv_invoc);
	obj->sigsegv_action.sa_flags = 0;
	obj->sigsegv_action.sa_userdata = arg;
	sigaction(SIGSEGV, &obj->sigsegv_action, NULL);

#if !EMULATED_PPC
	// Install illegal instruction signal handler
	sigemptyset(&obj->sigill_action.sa_mask);
	obj->sigill_action.sa_handler = (__signal_func_ptr)(obj->sigill_invoc);
	obj->sigill_action.sa_flags = 0;
	obj->sigill_action.sa_userdata = arg;
	sigaction(SIGILL, &obj->sigill_action, NULL);
#endif

	// Exceptions will send signals
	disable_debugger(true);

	// Install signal stack
	sig_stack = malloc(SIG_STACK_SIZE);
	extra_stack = malloc(SIG_STACK_SIZE);
	set_signal_stack(sig_stack, SIG_STACK_SIZE);

	// We're now ready to receive signals
	obj->ReadyForSignals = true;

	// Jump to ROM boot routine
	D(bug("Jumping to ROM\n"));
	obj->jump_to_rom(ROMBase + 0x310000);
	D(bug("Returned from ROM\n"));

	// We're no longer ready to receive signals
	obj->ReadyForSignals = false;
	obj->AllowQuitting = true;

	// Quit program
	be_app->PostMessage(B_QUIT_REQUESTED);
	return 0;
}


/*
 *  Jump into Mac ROM, start 680x0 emulator
 *  (also contains other EMUL_RETURN and EMUL_OP routines)
 */

#if EMULATED_PPC
extern void emul_ppc(uint32 start);
extern void init_emul_ppc(void);
void SheepShaver::jump_to_rom(uint32 entry)
{
	init_emul_ppc();
	emul_ppc(entry);
}
#else
asm void SheepShaver::jump_to_rom(register uint32 entry)
{
	// Create stack frame
	mflr	r0
	stw		r0,8(r1)
	mfcr	r0
	stw		r0,4(r1)
	stwu	r1,-(56+19*4+18*8)(r1)

	// Save PowerPC registers
	stmw	r13,56(r1)
	stfd	f14,56+19*4+0*8(r1)
	stfd	f15,56+19*4+1*8(r1)
	stfd	f16,56+19*4+2*8(r1)
	stfd	f17,56+19*4+3*8(r1)
	stfd	f18,56+19*4+4*8(r1)
	stfd	f19,56+19*4+5*8(r1)
	stfd	f20,56+19*4+6*8(r1)
	stfd	f21,56+19*4+7*8(r1)
	stfd	f22,56+19*4+8*8(r1)
	stfd	f23,56+19*4+9*8(r1)
	stfd	f24,56+19*4+10*8(r1)
	stfd	f25,56+19*4+11*8(r1)
	stfd	f26,56+19*4+12*8(r1)
	stfd	f27,56+19*4+13*8(r1)
	stfd	f28,56+19*4+14*8(r1)
	stfd	f29,56+19*4+15*8(r1)
	stfd	f30,56+19*4+16*8(r1)
	stfd	f31,56+19*4+17*8(r1)

	// Move entry address to ctr, get pointer to Emulator Data
	mtctr	r4
	lwz		r4,SheepShaver.emulator_data(r3)

	// Skip over EMUL_RETURN routine and get its address
	bl		@1


	/*
	 *  EMUL_RETURN: Returned from emulator
	 */

	// Restore PowerPC registers
	lwz		r1,XLM_EMUL_RETURN_STACK
	lwz		r2,XLM_TOC
	lmw		r13,56(r1)
	lfd		f14,56+19*4+0*8(r1)
	lfd		f15,56+19*4+1*8(r1)
	lfd		f16,56+19*4+2*8(r1)
	lfd		f17,56+19*4+3*8(r1)
	lfd		f18,56+19*4+4*8(r1)
	lfd		f19,56+19*4+5*8(r1)
	lfd		f20,56+19*4+6*8(r1)
	lfd		f21,56+19*4+7*8(r1)
	lfd		f22,56+19*4+8*8(r1)
	lfd		f23,56+19*4+9*8(r1)
	lfd		f24,56+19*4+10*8(r1)
	lfd		f25,56+19*4+11*8(r1)
	lfd		f26,56+19*4+12*8(r1)
	lfd		f27,56+19*4+13*8(r1)
	lfd		f28,56+19*4+14*8(r1)
	lfd		f29,56+19*4+15*8(r1)
	lfd		f30,56+19*4+16*8(r1)
	lfd		f31,56+19*4+17*8(r1)

	// Exiting from 68k emulator
	li		r0,1
	stw		r0,XLM_IRQ_NEST
	li		r0,MODE_NATIVE
	stw		r0,XLM_RUN_MODE

	// Return to caller of jump_to_rom()
	lwz		r0,56+19*4+18*8+8(r1)
	mtlr	r0
	lwz		r0,56+19*4+18*8+4(r1)
	mtcrf	0xff,r0
	addi	r1,r1,56+19*4+18*8
	blr


	// Save address of EMUL_RETURN routine for 68k emulator patch
@1	mflr	r0
	stw		r0,XLM_EMUL_RETURN_PROC

	// Skip over EXEC_RETURN routine and get its address
	bl		@2


	/*
	 *  EXEC_RETURN: Returned from 68k routine executed with Execute68k()
	 */

	// Save r25 (contains current 68k interrupt level)
	stw		r25,XLM_68K_R25

	// Reentering EMUL_OP mode
	li		r0,MODE_EMUL_OP
	stw		r0,XLM_RUN_MODE

	// Save 68k registers
	lwz		r4,56+19*4+18*8+12(r1)
	stw		r8,M68kRegisters.d[0](r4)
	stw		r9,M68kRegisters.d[1](r4)
	stw		r10,M68kRegisters.d[2](r4)
	stw		r11,M68kRegisters.d[3](r4)
	stw		r12,M68kRegisters.d[4](r4)
	stw		r13,M68kRegisters.d[5](r4)
	stw		r14,M68kRegisters.d[6](r4)
	stw		r15,M68kRegisters.d[7](r4)
	stw		r16,M68kRegisters.a[0](r4)
	stw		r17,M68kRegisters.a[1](r4)
	stw		r18,M68kRegisters.a[2](r4)
	stw		r19,M68kRegisters.a[3](r4)
	stw		r20,M68kRegisters.a[4](r4)
	stw		r21,M68kRegisters.a[5](r4)
	stw		r22,M68kRegisters.a[6](r4)

	// Restore PowerPC registers
	lmw		r13,56(r1)
#if SAVE_FP_EXEC_68K
	lfd		f14,56+19*4+0*8(r1)
	lfd		f15,56+19*4+1*8(r1)
	lfd		f16,56+19*4+2*8(r1)
	lfd		f17,56+19*4+3*8(r1)
	lfd		f18,56+19*4+4*8(r1)
	lfd		f19,56+19*4+5*8(r1)
	lfd		f20,56+19*4+6*8(r1)
	lfd		f21,56+19*4+7*8(r1)
	lfd		f22,56+19*4+8*8(r1)
	lfd		f23,56+19*4+9*8(r1)
	lfd		f24,56+19*4+10*8(r1)
	lfd		f25,56+19*4+11*8(r1)
	lfd		f26,56+19*4+12*8(r1)
	lfd		f27,56+19*4+13*8(r1)
	lfd		f28,56+19*4+14*8(r1)
	lfd		f29,56+19*4+15*8(r1)
	lfd		f30,56+19*4+16*8(r1)
	lfd		f31,56+19*4+17*8(r1)
#endif

	// Return to caller
	lwz		r0,56+19*4+18*8+8(r1)
	mtlr	r0
	addi	r1,r1,56+19*4+18*8
	blr


	// Stave address of EXEC_RETURN routine for 68k emulator patch
@2	mflr	r0
	stw		r0,XLM_EXEC_RETURN_PROC

	// Skip over EMUL_BREAK/EMUL_OP routine and get its address
	bl		@3


	/*
	 *  EMUL_BREAK/EMUL_OP: Execute native routine, selector in r5 (my own private mode switch)
	 *
	 *  68k registers are stored in a M68kRegisters struct on the stack
	 *  which the native routine may read and modify
	 */

	// Save r25 (contains current 68k interrupt level)
	stw		r25,XLM_68K_R25

	// Entering EMUL_OP mode within 68k emulator
	li		r0,MODE_EMUL_OP
	stw		r0,XLM_RUN_MODE

	// Create PowerPC stack frame, reserve space for M68kRegisters
	mr		r3,r1
	subi	r1,r1,56		// Fake "caller" frame
	rlwinm	r1,r1,0,0,29	// Align stack

	mfcr	r0
	rlwinm	r0,r0,0,11,8
	stw		r0,4(r1)
	mfxer	r0
	stw		r0,16(r1)
	stw		r2,12(r1)
	stwu	r1,-(56+16*4+15*8)(r1)
	lwz		r2,XLM_TOC

	// Save 68k registers
	stw		r8,56+M68kRegisters.d[0](r1)
	stw		r9,56+M68kRegisters.d[1](r1)
	stw		r10,56+M68kRegisters.d[2](r1)
	stw		r11,56+M68kRegisters.d[3](r1)
	stw		r12,56+M68kRegisters.d[4](r1)
	stw		r13,56+M68kRegisters.d[5](r1)
	stw		r14,56+M68kRegisters.d[6](r1)
	stw		r15,56+M68kRegisters.d[7](r1)
	stw		r16,56+M68kRegisters.a[0](r1)
	stw		r17,56+M68kRegisters.a[1](r1)
	stw		r18,56+M68kRegisters.a[2](r1)
	stw		r19,56+M68kRegisters.a[3](r1)
	stw		r20,56+M68kRegisters.a[4](r1)
	stw		r21,56+M68kRegisters.a[5](r1)
	stw		r22,56+M68kRegisters.a[6](r1)
	stw		r3,56+M68kRegisters.a[7](r1)
	stfd	f0,56+16*4+0*8(r1)
	stfd	f1,56+16*4+1*8(r1)
	stfd	f2,56+16*4+2*8(r1)
	stfd	f3,56+16*4+3*8(r1)
	stfd	f4,56+16*4+4*8(r1)
	stfd	f5,56+16*4+5*8(r1)
	stfd	f6,56+16*4+6*8(r1)
	stfd	f7,56+16*4+7*8(r1)
	mffs	f0
	stfd	f8,56+16*4+8*8(r1)
	stfd	f9,56+16*4+9*8(r1)
	stfd	f10,56+16*4+10*8(r1)
	stfd	f11,56+16*4+11*8(r1)
	stfd	f12,56+16*4+12*8(r1)
	stfd	f13,56+16*4+13*8(r1)
	stfd	f0,56+16*4+14*8(r1)

	// Execute native routine
	addi	r3,r1,56
	mr		r4,r24
	bl		EmulOp

	// Restore 68k registers
	lwz		r8,56+M68kRegisters.d[0](r1)
	lwz		r9,56+M68kRegisters.d[1](r1)
	lwz		r10,56+M68kRegisters.d[2](r1)
	lwz		r11,56+M68kRegisters.d[3](r1)
	lwz		r12,56+M68kRegisters.d[4](r1)
	lwz		r13,56+M68kRegisters.d[5](r1)
	lwz		r14,56+M68kRegisters.d[6](r1)
	lwz		r15,56+M68kRegisters.d[7](r1)
	lwz		r16,56+M68kRegisters.a[0](r1)
	lwz		r17,56+M68kRegisters.a[1](r1)
	lwz		r18,56+M68kRegisters.a[2](r1)
	lwz		r19,56+M68kRegisters.a[3](r1)
	lwz		r20,56+M68kRegisters.a[4](r1)
	lwz		r21,56+M68kRegisters.a[5](r1)
	lwz		r22,56+M68kRegisters.a[6](r1)
	lwz		r3,56+M68kRegisters.a[7](r1)
	lfd		f13,56+16*4+14*8(r1)
	lfd		f0,56+16*4+0*8(r1)
	lfd		f1,56+16*4+1*8(r1)
	lfd		f2,56+16*4+2*8(r1)
	lfd		f3,56+16*4+3*8(r1)
	lfd		f4,56+16*4+4*8(r1)
	lfd		f5,56+16*4+5*8(r1)
	lfd		f6,56+16*4+6*8(r1)
	lfd		f7,56+16*4+7*8(r1)
	mtfsf	0xff,f13
	lfd		f8,56+16*4+8*8(r1)
	lfd		f9,56+16*4+9*8(r1)
	lfd		f10,56+16*4+10*8(r1)
	lfd		f11,56+16*4+11*8(r1)
	lfd		f12,56+16*4+12*8(r1)
	lfd		f13,56+16*4+13*8(r1)

	// Delete PowerPC stack frame
	lwz		r2,56+16*4+15*8+12(r1)
	lwz		r0,56+16*4+15*8+16(r1)
	mtxer	r0
	lwz		r0,56+16*4+15*8+4(r1)
	mtcrf	0xff,r0
	mr		r1,r3

	// Reeintering 68k emulator
	li		r0,MODE_68K
	stw		r0,XLM_RUN_MODE

	// Set r0 to 0 for 68k emulator
	li		r0,0

	// Execute next 68k opcode
	rlwimi	r29,r27,3,13,28
	lhau	r27,2(r24)
	mtlr	r29
	blr


	// Save address of EMUL_BREAK/EMUL_OP routine for 68k emulator patch
@3	mflr	r0
	stw		r0,XLM_EMUL_OP_PROC

	// Save stack pointer for EMUL_RETURN
	stw		r1,XLM_EMUL_RETURN_STACK

	// Preset registers for ROM boot routine
	lis		r3,0x40b0		// Pointer to ROM boot structure
	ori		r3,r3,0xd000

	// 68k emulator is now active
	li		r0,MODE_68K
	stw		r0,XLM_RUN_MODE

	// Jump to ROM
	bctr
}
#endif


#if !EMULATED_PPC
/*
 *  Execute 68k subroutine (must be ended with RTS)
 *  This must only be called by the emul_thread when in EMUL_OP mode
 *  r->a[7] is unused, the routine runs on the caller's stack
 */

#if SAFE_EXEC_68K
void execute_68k(uint32 pc, M68kRegisters *r);

void Execute68k(uint32 pc, M68kRegisters *r)
{
	if (*(uint32 *)XLM_RUN_MODE != MODE_EMUL_OP)
		printf("FATAL: Execute68k() not called from EMUL_OP mode\n");
	if (find_thread(NULL) != the_app->emul_thread)
		printf("FATAL: Execute68k() not called from emul_thread\n");
	execute_68k(pc, r);
}

asm void execute_68k(register uint32 pc, register M68kRegisters *r)
#else
asm void Execute68k(register uint32 pc, register M68kRegisters *r)
#endif
{
	// Create stack frame
	mflr	r0
	stw		r0,8(r1)
	stw		r4,12(r1)
	stwu	r1,-(56+19*4+18*8)(r1)

	// Save PowerPC registers
	stmw	r13,56(r1)
#if SAVE_FP_EXEC_68K
	stfd	f14,56+19*4+0*8(r1)
	stfd	f15,56+19*4+1*8(r1)
	stfd	f16,56+19*4+2*8(r1)
	stfd	f17,56+19*4+3*8(r1)
	stfd	f18,56+19*4+4*8(r1)
	stfd	f19,56+19*4+5*8(r1)
	stfd	f20,56+19*4+6*8(r1)
	stfd	f21,56+19*4+7*8(r1)
	stfd	f22,56+19*4+8*8(r1)
	stfd	f23,56+19*4+9*8(r1)
	stfd	f24,56+19*4+10*8(r1)
	stfd	f25,56+19*4+11*8(r1)
	stfd	f26,56+19*4+12*8(r1)
	stfd	f27,56+19*4+13*8(r1)
	stfd	f28,56+19*4+14*8(r1)
	stfd	f29,56+19*4+15*8(r1)
	stfd	f30,56+19*4+16*8(r1)
	stfd	f31,56+19*4+17*8(r1)
#endif

	// Set up registers for 68k emulator
	lwz		r31,XLM_KERNEL_DATA	// Pointer to Kernel Data
	addi	r31,r31,0x1000
	li		r0,0
	mtcrf	0xff,r0
	creqv	11,11,11			// Supervisor mode
	lwz		r8,M68kRegisters.d[0](r4)
	lwz		r9,M68kRegisters.d[1](r4)
	lwz		r10,M68kRegisters.d[2](r4)
	lwz		r11,M68kRegisters.d[3](r4)
	lwz		r12,M68kRegisters.d[4](r4)
	lwz		r13,M68kRegisters.d[5](r4)
	lwz		r14,M68kRegisters.d[6](r4)
	lwz		r15,M68kRegisters.d[7](r4)
	lwz		r16,M68kRegisters.a[0](r4)
	lwz		r17,M68kRegisters.a[1](r4)
	lwz		r18,M68kRegisters.a[2](r4)
	lwz		r19,M68kRegisters.a[3](r4)
	lwz		r20,M68kRegisters.a[4](r4)
	lwz		r21,M68kRegisters.a[5](r4)
	lwz		r22,M68kRegisters.a[6](r4)
	li		r23,0
	mr		r24,r3
	lwz		r25,XLM_68K_R25		// MSB of SR
	li		r26,0
	li		r28,0				// VBR
	lwz		r29,0x74(r31)		// Pointer to opcode table
	lwz		r30,0x78(r31)		// Address of emulator

	// Push return address (points to EXEC_RETURN opcode) on stack
	li		r0,XLM_EXEC_RETURN_OPCODE
	stwu	r0,-4(r1)

	// Reentering 68k emulator
	li		r0,MODE_68K
	stw		r0,XLM_RUN_MODE

	// Set r0 to 0 for 68k emulator
	li		r0,0

	// Execute 68k opcode
	lha		r27,0(r24)
	rlwimi	r29,r27,3,13,28
	lhau	r27,2(r24)
	mtlr	r29
	blr
}


/*
 *  Execute 68k A-Trap from EMUL_OP routine
 *  r->a[7] is unused, the routine runs on the caller's stack
 */

void Execute68kTrap(uint16 trap, M68kRegisters *r)
{
	uint16 proc[2] = {trap, M68K_RTS};
	Execute68k((uint32)proc, r);
}


/*
 *  Quit emulator (must only be called from main thread)
 */

asm void QuitEmulator(void)
{
	lwz		r0,XLM_EMUL_RETURN_PROC
	mtlr	r0
	blr
}
#endif


/*
 *  Dump 68k registers
 */

void Dump68kRegs(M68kRegisters *r)
{
	// Display 68k registers
	for (int i=0; i<8; i++) {
		printf("d%d: %08lx", i, r->d[i]);
		if (i == 3 || i == 7)
			printf("\n");
		else
			printf(", ");
	}
	for (int i=0; i<8; i++) {
		printf("a%d: %08lx", i, r->a[i]);
		if (i == 3 || i == 7)
			printf("\n");
		else
			printf(", ");
	}
}


/*
 *  Make code executable
 */

void MakeExecutable(int dummy, uint32 start, uint32 length)
{
	if ((start >= ROMBase) && (start < (ROMBase + ROM_SIZE)))
		return;
	clear_caches((void *)start, length, B_INVALIDATE_ICACHE | B_FLUSH_DCACHE);
}


/*
 *  NVRAM watchdog thread (saves NVRAM every minute)
 */

status_t SheepShaver::nvram_func(void *arg)
{
	SheepShaver *obj = (SheepShaver *)arg;

	while (obj->NVRAMThreadActive) {
		snooze(60*1000000);
		if (memcmp(obj->last_xpram, XPRAM, XPRAM_SIZE)) {
			memcpy(obj->last_xpram, XPRAM, XPRAM_SIZE);
			SaveXPRAM();
		}
	}
	return 0;
}


/*
 *  60Hz thread (really 60.15Hz)
 */

status_t SheepShaver::tick_func(void *arg)
{
	SheepShaver *obj = (SheepShaver *)arg;
	int tick_counter = 0;
	bigtime_t current = system_time();

	while (obj->TickThreadActive) {

		// Wait
		current += 16625;
		snooze_until(current, B_SYSTEM_TIMEBASE);

		// Pseudo Mac 1Hz interrupt, update local time
		if (++tick_counter > 60) {
			tick_counter = 0;
			WriteMacInt32(0x20c, TimerDateTime());
		}

		// 60Hz interrupt
		if (ReadMacInt32(XLM_IRQ_NEST) == 0) {
			SetInterruptFlag(INTFLAG_VIA);
			TriggerInterrupt();
		}
	}
	return 0;
}


/*
 *  Trigger signal USR1 from another thread
 */

void TriggerInterrupt(void)
{
	idle_resume();
#if 0
	WriteMacInt32(0x16a, ReadMacInt32(0x16a) + 1);
#else
	if (the_app->emul_thread > 0 && the_app->ReadyForSignals)
		send_signal(the_app->emul_thread, SIGUSR1);
#endif
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
 *  Set/clear interrupt flags (must be done atomically!)
 */

volatile uint32 InterruptFlags = 0;

void SetInterruptFlag(uint32 flag)
{
	atomic_or((int32 *)&InterruptFlags, flag);
}

void ClearInterruptFlag(uint32 flag)
{
	atomic_and((int32 *)&InterruptFlags, ~flag);
}


/*
 *  Disable interrupts
 */

void DisableInterrupt(void)
{
	atomic_add((int32 *)XLM_IRQ_NEST, 1);
}


/*
 *  Enable interrupts
 */

void EnableInterrupt(void)
{
	atomic_add((int32 *)XLM_IRQ_NEST, -1);
}


/*
 *  USR1 handler
 */

void SheepShaver::sigusr1_invoc(int sig, void *arg, vregs *r)
{
	((SheepShaver *)arg)->sigusr1_handler(r);
}

#if !EMULATED_PPC
static asm void ppc_interrupt(register uint32 entry)
{
	fralloc

	// Get address of return routine
	bl		@1

	// Return routine
	frfree
	blr

@1
	// Prepare registers for nanokernel interrupt routine
	mtctr	r1
	lwz		r1,XLM_KERNEL_DATA
	stw		r6,0x018(r1)
	mfctr	r6
	stw		r6,0x004(r1)
	lwz		r6,0x65c(r1)
	stw		r7,0x13c(r6)
	stw		r8,0x144(r6)
	stw		r9,0x14c(r6)
	stw		r10,0x154(r6)
	stw		r11,0x15c(r6)
	stw		r12,0x164(r6)
	stw		r13,0x16c(r6)

	mflr	r10
	mfcr	r13
	lwz		r7,0x660(r1)
	mflr	r12
	rlwimi.	r7,r7,8,0,0
	li		r11,0
	ori		r11,r11,0xf072	// MSR (SRR1)
	mtcrf	0x70,r11
	li		r8,0

	// Enter nanokernel
	mtlr	r3
	blr
}
#endif

void SheepShaver::sigusr1_handler(vregs *r)
{
	// Do nothing if interrupts are disabled
	if ((*(int32 *)XLM_IRQ_NEST) > 0)
		return;

	// Interrupt action depends on current run mode
	switch (*(uint32 *)XLM_RUN_MODE) {
		case MODE_68K:
			// 68k emulator active, trigger 68k interrupt level 1
			*(uint16 *)(kernel_data->v[0x67c >> 2]) = 1;
			r->cr |= kernel_data->v[0x674 >> 2];
			break;

#if INTERRUPTS_IN_NATIVE_MODE
		case MODE_NATIVE:
			// 68k emulator inactive, in nanokernel?
			if (r->r1 != KernelDataAddr) {
				// No, prepare for 68k interrupt level 1
				*(uint16 *)(kernel_data->v[0x67c >> 2]) = 1;
				*(uint32 *)(kernel_data->v[0x658 >> 2] + 0xdc) |= kernel_data->v[0x674 >> 2];

				// Execute nanokernel interrupt routine (this will activate the 68k emulator)
				atomic_add((int32 *)XLM_IRQ_NEST, 1);
				if (ROMType == ROMTYPE_NEWWORLD)
					ppc_interrupt(ROMBase + 0x312b1c);
				else
					ppc_interrupt(ROMBase + 0x312a3c);
			}
			break;
#endif

#if INTERRUPTS_IN_EMUL_OP_MODE
		case MODE_EMUL_OP:
			// 68k emulator active, within EMUL_OP routine, execute 68k interrupt routine directly when interrupt level is 0
			if ((*(uint32 *)XLM_68K_R25 & 7) == 0) {

				// Set extra stack for SIGSEGV handler
				set_signal_stack(extra_stack, SIG_STACK_SIZE);
#if 1
				// Execute full 68k interrupt routine
				M68kRegisters r;
				uint32 old_r25 = *(uint32 *)XLM_68K_R25;	// Save interrupt level
				*(uint32 *)XLM_68K_R25 = 0x21;				// Execute with interrupt level 1
				static const uint16 proc[] = {
					0x3f3c, 0x0000,		// move.w	#$0000,-(sp)	(fake format word)
					0x487a, 0x000a,		// pea		@1(pc)			(return address)
					0x40e7,				// move		sr,-(sp)		(saved SR)
					0x2078, 0x0064,		// move.l	$64,a0
					0x4ed0,				// jmp		(a0)
					M68K_RTS			// @1
				};
				Execute68k((uint32)proc, &r);
				*(uint32 *)XLM_68K_R25 = old_r25;			// Restore interrupt level
#else
				// Only update cursor
				if (HasMacStarted()) {
					if (InterruptFlags & INTFLAG_VIA) {
						ClearInterruptFlag(INTFLAG_VIA);
						ADBInterrupt();
						ExecuteNative(NATIVE_VIDEO_VBL);
					}
				}
#endif
				// Reset normal signal stack
				set_signal_stack(sig_stack, SIG_STACK_SIZE);
			}
			break;
#endif
	}
}


/*
 *  SIGSEGV handler
 */

static uint32 segv_r[32];

#if !EMULATED_PPC
asm void SheepShaver::sigsegv_invoc(register int sig, register void *arg, register vregs *r)
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
#endif

static void sigsegv_handler(vregs *r)
{
	char str[256];

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

	// Fault in Mac ROM or RAM?
	bool mac_fault = (r->pc >= ROMBase) && (r->pc < (ROMBase + ROM_AREA_SIZE)) || (r->pc >= RAMBase) && (r->pc < (RAMBase + RAMSize));
	if (mac_fault) {

		// "VM settings" during MacOS 8 installation
		if (r->pc == ROMBase + 0x488160 && segv_r[20] == 0xf8000000) {
			r->pc += 4;
			segv_r[8] = 0;
			goto rti;

		// MacOS 8.5 installation
		} else if (r->pc == ROMBase + 0x488140 && segv_r[16] == 0xf8000000) {
			r->pc += 4;
			segv_r[8] = 0;
			goto rti;

		// MacOS 8 serial drivers on startup
		} else if (r->pc == ROMBase + 0x48e080 && (segv_r[8] == 0xf3012002 || segv_r[8] == 0xf3012000)) {
			r->pc += 4;
			segv_r[8] = 0;
			goto rti;

		// MacOS 8.1 serial drivers on startup
		} else if (r->pc == ROMBase + 0x48c5e0 && (segv_r[20] == 0xf3012002 || segv_r[20] == 0xf3012000)) {
			r->pc += 4;
			goto rti;
		} else if (r->pc == ROMBase + 0x4a10a0 && (segv_r[20] == 0xf3012002 || segv_r[20] == 0xf3012000)) {
			r->pc += 4;
			goto rti;
		}
	}

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
		default:
			break;
	}

	// Ignore ROM writes
	if (transfer_type == TYPE_STORE && addr >= ROMBase && addr < ROMBase + ROM_SIZE) {
		D(bug("WARNING: %s write access to ROM at %p, pc %p\n", transfer_size == SIZE_BYTE ? "Byte" : transfer_size == SIZE_HALFWORD ? "Halfword" : "Word", addr, r->pc));
		if (addr_mode == MODE_U || addr_mode == MODE_UX)
			segv_r[ra] = addr;
		r->pc += 4;
		goto rti;
	}

	// Fault in Mac ROM or RAM?
	if (mac_fault) {

		// Ignore illegal memory accesses?
		if (PrefsFindBool("ignoresegv")) {
			if (addr_mode == MODE_U || addr_mode == MODE_UX)
				segv_r[ra] = addr;
			if (transfer_type == TYPE_LOAD)
				segv_r[rd] = 0;
			r->pc += 4;
			goto rti;
		}

		// In GUI mode, show error alert
		if (!PrefsFindBool("nogui")) {
			if (transfer_type == TYPE_LOAD || transfer_type == TYPE_STORE)
				sprintf(str, GetString(STR_MEM_ACCESS_ERR), transfer_size == SIZE_BYTE ? "byte" : transfer_size == SIZE_HALFWORD ? "halfword" : "word", transfer_type == TYPE_LOAD ? GetString(STR_MEM_ACCESS_READ) : GetString(STR_MEM_ACCESS_WRITE), addr, r->pc, segv_r[24], segv_r[1]);
			else
				sprintf(str, GetString(STR_UNKNOWN_SEGV_ERR), r->pc, segv_r[24], segv_r[1], opcode);
			ErrorAlert(str);
			QuitEmulator();
			return;
		}
	}

	// For all other errors, jump into debugger
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
	VideoQuitFullScreen();
	disable_debugger(false);
	debugger(str);
	exit(1);
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


/*
 *  SIGILL handler
 */

#if !EMULATED_PPC
asm void SheepShaver::sigill_invoc(register int sig, register void *arg, register vregs *r)
{
	mflr	r0
	stw		r0,8(r1)
	stwu	r1,-56(r1)

	lwz		r3,segv_r(r2)
	stmw	r13,13*4(r3)

	mr		r3,r5
	bl		sigill_handler

	lwz		r3,segv_r(r2)
	lmw		r13,13*4(r3)

	lwz		r0,56+8(r1)
	mtlr	r0
	addi	r1,r1,56
	blr
}
#endif

static void sigill_handler(vregs *r)
{
	char str[256];

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

	// Fault in Mac ROM or RAM?
	bool mac_fault = (r->pc >= ROMBase) && (r->pc < (ROMBase + ROM_AREA_SIZE)) || (r->pc >= RAMBase) && (r->pc < (RAMBase + RAMSize));
	if (mac_fault) {

		switch (primop) {
			case 9:		// POWER instructions
			case 22:
power_inst:		sprintf(str, GetString(STR_POWER_INSTRUCTION_ERR), r->pc, segv_r[1], opcode);
				ErrorAlert(str);
				QuitEmulator();
				return;

			case 31:
				switch (exop) {
					case 83:	// mfmsr
						segv_r[rd] = 0xf072;
						r->pc += 4;
						goto rti;

					case 210:	// mtsr
					case 242:	// mtsrin
					case 306:	// tlbie
						r->pc += 4;
						goto rti;

					case 339: {	// mfspr
						int spr = ra | (rb << 5);
						switch (spr) {
							case 0:		// MQ
							case 22:	// DEC
							case 952:	// MMCR0
							case 953:	// PMC1
							case 954:	// PMC2
							case 955:	// SIA
							case 956:	// MMCR1
							case 957:	// PMC3
							case 958:	// PMC4
							case 959:	// SDA
								r->pc += 4;
								goto rti;
							case 25:	// SDR1
								segv_r[rd] = 0xdead001f;
								r->pc += 4;
								goto rti;
							case 287:	// PVR
								segv_r[rd] = PVR;
								r->pc += 4;
								goto rti;
						}
						break;
					}

					case 467: {	// mtspr
						int spr = ra | (rb << 5);
						switch (spr) {
							case 0:		// MQ
							case 22:	// DEC
							case 275:	// SPRG3
							case 528:	// IBAT0U
							case 529:	// IBAT0L
							case 530:	// IBAT1U
							case 531:	// IBAT1L
							case 532:	// IBAT2U
							case 533:	// IBAT2L
							case 534:	// IBAT3U
							case 535:	// IBAT3L
							case 536:	// DBAT0U
							case 537:	// DBAT0L
							case 538:	// DBAT1U
							case 539:	// DBAT1L
							case 540:	// DBAT2U
							case 541:	// DBAT2L
							case 542:	// DBAT3U
							case 543:	// DBAT3L
							case 952:	// MMCR0
							case 953:	// PMC1
							case 954:	// PMC2
							case 955:	// SIA
							case 956:	// MMCR1
							case 957:	// PMC3
							case 958:	// PMC4
							case 959:	// SDA
								r->pc += 4;
								goto rti;
						}
						break;
					}

					case 29: case 107: case 152: case 153:	// POWER instructions
					case 184: case 216: case 217: case 248:
					case 264: case 277: case 331: case 360:
					case 363: case 488: case 531: case 537:
					case 541: case 664: case 665: case 696:
					case 728: case 729: case 760: case 920:
					case 921: case 952:
						goto power_inst;
				}
		}

		// In GUI mode, show error alert
		if (!PrefsFindBool("nogui")) {
			sprintf(str, GetString(STR_UNKNOWN_SEGV_ERR), r->pc, segv_r[24], segv_r[1], opcode);
			ErrorAlert(str);
			QuitEmulator();
			return;
		}
	}

	// For all other errors, jump into debugger
	sprintf(str, "SIGILL\n"
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
	VideoQuitFullScreen();
	disable_debugger(false);
	debugger(str);
	exit(1);
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


/*
 *  Helpers to share 32-bit addressable data with MacOS
 */

bool SheepMem::Init(void)
{
	// Delete old area
	area_id old_sheep_area = find_area(SHEEP_AREA_NAME);
	if (old_sheep_area > 0)
		delete_area(old_sheep_area);

	// Create area for SheepShaver data
	proc = base = 0x60000000;
	SheepMemArea = create_area(SHEEP_AREA_NAME, (void **)&base, B_BASE_ADDRESS, size, B_NO_LOCK, B_READ_AREA | B_WRITE_AREA);
	if (SheepMemArea < 0)
		return false;

	// Create read-only area with all bits set to 0
	static const uint8 const_zero_page[4096] = {0,};
	zero_page = const_zero_page;

	D(bug("SheepShaver area %ld at %p\n", SheepMemArea, base));
	data = base + size;
	return true;
}

void SheepMem::Exit(void)
{
	if (SheepMemArea >= 0)
		delete_area(SheepMemArea);
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
	char str[256];
	sprintf(str, GetString(STR_GUI_ERROR_PREFIX), text);
	VideoQuitFullScreen();
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
