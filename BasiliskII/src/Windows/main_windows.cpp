/*
 *  main_windows.cpp - Startup code for Windows
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

#include "sysdeps.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

#include <SDL.h>
#include <SDL_mutex.h>
#include <SDL_thread.h>

#include <string>
typedef std::basic_string<TCHAR> tstring;

#include "cpu_emulation.h"
#include "sys.h"
#include "rom_patches.h"
#include "xpram.h"
#include "timer.h"
#include "video.h"
#include "cdrom.h"
#include "emul_op.h"
#include "prefs.h"
#include "prefs_editor.h"
#include "macos_util.h"
#include "user_strings.h"
#include "version.h"
#include "main.h"
#include "vm_alloc.h"
#include "sigsegv.h"
#include "util_windows.h"

#if USE_JIT
extern void flush_icache_range(uint8 *start, uint32 size); // from compemu_support.cpp
#endif

#ifdef ENABLE_MON
# include "mon.h"
#endif

#define DEBUG 0
#include "debug.h"

#if !SDL_VERSION_ATLEAST(3, 0, 0)
#define SDL_Mutex		SDL_mutex
#define SDL_EVENT_KEY_UP	SDL_KEYUP
#define SDL_EVENT_KEY_DOWN	SDL_KEYDOWN
#endif

// Constants
const TCHAR ROM_FILE_NAME[] = TEXT("ROM");
const int SCRATCH_MEM_SIZE = 0x10000;	// Size of scratch memory area


// CPU and FPU type, addressing mode
int CPUType;
bool CPUIs68060;
int FPUType;
bool TwentyFourBitAddressing;


// Global variables
HANDLE emul_thread = NULL;							// Handle of MacOS emulation thread (main thread)

static uint8 last_xpram[XPRAM_SIZE];				// Buffer for monitoring XPRAM changes
static bool xpram_thread_active = false;			// Flag: XPRAM watchdog installed
static volatile bool xpram_thread_cancel = false;	// Flag: Cancel XPRAM thread
static SDL_Thread *xpram_thread = NULL;				// XPRAM watchdog

static bool tick_thread_active = false;				// Flag: 60Hz thread installed
static volatile bool tick_thread_cancel = false;	// Flag: Cancel 60Hz thread
static SDL_Thread *tick_thread;						// 60Hz thread

static SDL_Mutex *intflag_lock = NULL;				// Mutex to protect InterruptFlags
#define LOCK_INTFLAGS SDL_LockMutex(intflag_lock)
#define UNLOCK_INTFLAGS SDL_UnlockMutex(intflag_lock)

#if USE_SCRATCHMEM_SUBTERFUGE
uint8 *ScratchMem = NULL;			// Scratch memory for Mac ROM writes
#endif

#if REAL_ADDRESSING
static bool lm_area_mapped = false;	// Flag: Low Memory area mmap()ped
#endif

static HHOOK keyboard_hook; // Hook for intercepting windows key events


// Prototypes
static int xpram_func(void *arg);
static int tick_func(void *arg);
static void one_tick(...);
static LRESULT CALLBACK low_level_keyboard_hook(int nCode, WPARAM wParam, LPARAM lParam);


/*
 *  Ersatz functions
 */

extern "C" {

#ifndef HAVE_STRDUP
char *strdup(const char *s)
{
	char *n = (char *)malloc(strlen(s) + 1);
	strcpy(n, s);
	return n;
}
#endif

}


/*
 *  Map memory that can be accessed from the Mac side
 */

void *vm_acquire_mac(size_t size)
{
	return vm_acquire(size, VM_MAP_DEFAULT | VM_MAP_32BIT);
}


/*
 *  SIGSEGV handler
 */

static sigsegv_return_t sigsegv_handler(sigsegv_info_t *sip)
{
	const uintptr fault_address = (uintptr)sigsegv_get_fault_address(sip);
#if ENABLE_VOSF
	// Handle screen fault
	extern bool Screen_fault_handler(sigsegv_info_t *sip);
	if (Screen_fault_handler(sip))
		return SIGSEGV_RETURN_SUCCESS;
#endif

#ifdef HAVE_SIGSEGV_SKIP_INSTRUCTION
	// Ignore writes to ROM
	if (((uintptr)fault_address - (uintptr)ROMBaseHost) < ROMSize)
		return SIGSEGV_RETURN_SKIP_INSTRUCTION;

	// Ignore all other faults, if requested
	if (PrefsFindBool("ignoresegv"))
		return SIGSEGV_RETURN_SKIP_INSTRUCTION;
#endif

	return SIGSEGV_RETURN_FAILURE;
}

/*
 *  Dump state when everything went wrong after a SEGV
 */

static void sigsegv_dump_state(sigsegv_info_t *sip)
{
	const sigsegv_address_t fault_address = sigsegv_get_fault_address(sip);
	const sigsegv_address_t fault_instruction = sigsegv_get_fault_instruction_address(sip);
	fprintf(stderr, "Caught SIGSEGV at address %p", fault_address);
	if (fault_instruction != SIGSEGV_INVALID_ADDRESS)
		fprintf(stderr, " [IP=%p]", fault_instruction);
	fprintf(stderr, "\n");
	uaecptr nextpc;
	extern void m68k_dumpstate(uaecptr *nextpc);
	m68k_dumpstate(&nextpc);
#if USE_JIT && JIT_DEBUG
	extern void compiler_dumpstate(void);
	compiler_dumpstate();
#endif
	VideoQuitFullScreen();
#ifdef ENABLE_MON
	const char *arg[4] = {"mon", "-m", "-r", NULL};
	mon(3, arg);
	QuitEmulator();
#endif
}

uint16 emulated_ticks;
void cpu_do_check_ticks(void)
{
	static int delay = -1;
	if (delay < 0)
		delay = PrefsFindInt32("delay");
	if (delay)
		usleep(delay);
}


/*
 *  Main program
 */

static void usage(const char *prg_name)
{
	printf(
		"Usage: %s [OPTION...]\n"
		"\nUnix options:\n"
		"  --config FILE\n    read/write configuration from/to FILE\n"
		"  --display STRING\n    X display to use\n"
		"  --break ADDRESS\n    set ROM breakpoint\n"
		"  --rominfo\n    dump ROM information\n", prg_name
	);
	LoadPrefs(NULL); // read the prefs file so PrefsPrintUsage() will print the correct default values
	PrefsPrintUsage();
	printf("\nBuild Date: %s\n", __DATE__);
	exit(0);
}

int main(int argc, char **argv)
{
	char str[256];
	bool cd_boot = false;

	// Initialize variables
	RAMBaseHost = NULL;
	ROMBaseHost = NULL;
	srand(unsigned(time(NULL)));
	tzset();

	// Print some info
	printf(GetString(STR_ABOUT_TEXT1), VERSION_MAJOR, VERSION_MINOR);
	printf(" %s\n", GetString(STR_ABOUT_TEXT2));

	// Parse command line arguments
	for (int i=1; i<argc; i++) {
		if (strcmp(argv[i], "--help") == 0) {
			usage(argv[0]);
		} else if (strcmp(argv[i], "--break") == 0) {
			argv[i++] = NULL;
			if (i < argc) {
				ROMBreakpoint = strtol(argv[i], NULL, 0);
				argv[i] = NULL;
			}
		} else if (strcmp(argv[i], "--config") == 0) {
			argv[i++] = NULL;
			if (i < argc) {
				extern tstring UserPrefsPath; // from prefs_windows.cpp
				UserPrefsPath = to_tstring(argv[i]);
				argv[i] = NULL;
			}
		} else if (strcmp(argv[i], "--rominfo") == 0) {
			argv[i] = NULL;
			PrintROMInfo = true;
		} else if (strcmp(argv[i], "--cdboot") == 0) {
			argv[i] = NULL;
			cd_boot = true;
		}
	}

	// Remove processed arguments
	for (int i=1; i<argc; i++) {
		int k;
		for (k=i; k<argc; k++)
			if (argv[k] != NULL)
				break;
		if (k > i) {
			k -= i;
			for (int j=i+k; j<argc; j++)
				argv[j-k] = argv[j];
			argc -= k;
		}
	}

	// Read preferences
	PrefsInit(NULL, argc, argv);

	// Boot MacOS from CD-ROM?
	if (cd_boot)
		PrefsReplaceInt32("bootdriver", CDROMRefNum);

	// Any command line arguments left?
	for (int i=1; i<argc; i++) {
		if (argv[i][0] == '-') {
			fprintf(stderr, "Unrecognized option '%s'\n", argv[i]);
			usage(argv[0]);
		}
	}

#if 0
	// Check that drivers are installed
	if (!check_drivers())
		QuitEmulator();
#endif

	// Install keyboard hook to block Windows key if enabled in prefs
	if (PrefsFindBool("reservewindowskey"))
	{
		keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, low_level_keyboard_hook, GetModuleHandle(NULL), 0);
	}

	// Initialize SDL system
	int sdl_flags = 0;
#ifdef USE_SDL_VIDEO
	sdl_flags |= SDL_INIT_VIDEO;
#endif
#ifdef USE_SDL_AUDIO
	sdl_flags |= SDL_INIT_AUDIO;
#endif
	assert(sdl_flags != 0);
	if (SDL_Init(sdl_flags) == -1) {
		char str[256];
		sprintf(str, "Could not initialize SDL: %s.\n", SDL_GetError());
		ErrorAlert(str);
		QuitEmulator();
	}
	atexit(SDL_Quit);

	// Init system routines
	SysInit();

	// Show preferences editor
	if (!PrefsFindBool("nogui"))
		if (!PrefsEditor())
			QuitEmulator();

	// Install the handler for SIGSEGV
	if (!sigsegv_install_handler(sigsegv_handler)) {
		sprintf(str, GetString(STR_SIG_INSTALL_ERR), "SIGSEGV", strerror(errno));
		ErrorAlert(str);
		QuitEmulator();
	}
	
	// Register dump state function when we got mad after a segfault
	sigsegv_set_dump_state(sigsegv_dump_state);

	// Read RAM size
	RAMSize = PrefsFindInt32("ramsize");
	if (RAMSize <= 1000) {
		RAMSize *= 1024 * 1024;
	}
	RAMSize &= 0xfff00000;	// Round down to 1MB boundary
	if (RAMSize < 1024*1024) {
		WarningAlert(GetString(STR_SMALL_RAM_WARN));
		RAMSize = 1024*1024;
	}
	
	// Initialize VM system
	vm_init();

	// Create areas for Mac RAM and ROM
	uint8 *ram_rom_area = (uint8 *)vm_acquire_mac(RAMSize + 0x100000);
	if (ram_rom_area == VM_MAP_FAILED) {
		ErrorAlert(STR_NO_MEM_ERR);
		QuitEmulator();
	}
	RAMBaseHost = ram_rom_area;
	ROMBaseHost = RAMBaseHost + RAMSize;

#if USE_SCRATCHMEM_SUBTERFUGE
	// Allocate scratch memory
	ScratchMem = (uint8 *)vm_acquire(SCRATCH_MEM_SIZE);
	if (ScratchMem == VM_MAP_FAILED) {
		ErrorAlert(STR_NO_MEM_ERR);
		QuitEmulator();
	}
	ScratchMem += SCRATCH_MEM_SIZE/2;	// ScratchMem points to middle of block
#endif

#if DIRECT_ADDRESSING
	// RAMBaseMac shall always be zero
	MEMBaseDiff = (uintptr)RAMBaseHost;
	RAMBaseMac = 0;
	ROMBaseMac = Host2MacAddr(ROMBaseHost);
#endif
	D(bug("Mac RAM starts at %p (%08x)\n", RAMBaseHost, RAMBaseMac));
	D(bug("Mac ROM starts at %p (%08x)\n", ROMBaseHost, ROMBaseMac));
	
	// Get rom file path from preferences
	const char* rom_path = PrefsFindString("rom");

	// Load Mac ROM
	HANDLE rom_fh = CreateFile((rom_path != NULL) ? rom_path : ROM_FILE_NAME,
							   GENERIC_READ,
							   FILE_SHARE_READ, NULL,
							   OPEN_EXISTING,
							   FILE_ATTRIBUTE_NORMAL,
							   NULL);
	if (rom_fh == INVALID_HANDLE_VALUE) {
		ErrorAlert(STR_NO_ROM_FILE_ERR);
		QuitEmulator();
	}
	printf(GetString(STR_READING_ROM_FILE));
	ROMSize = GetFileSize(rom_fh, NULL);
	if (ROMSize != 64*1024 && ROMSize != 128*1024 && ROMSize != 256*1024 && ROMSize != 512*1024 && ROMSize != 1024*1024) {
		ErrorAlert(STR_ROM_SIZE_ERR);
		CloseHandle(rom_fh);
		QuitEmulator();
	}
	DWORD bytes_read;
	if (ReadFile(rom_fh, ROMBaseHost, ROMSize, &bytes_read, NULL) == 0 || bytes_read != ROMSize) {
		ErrorAlert(STR_ROM_FILE_READ_ERR);
		CloseHandle(rom_fh);
		QuitEmulator();
	}

	// Initialize native timers
	timer_init();

	// Initialize everything
	if (!InitAll(NULL))
		QuitEmulator();
	D(bug("Initialization complete\n"));

	// Get handle of main thread
	emul_thread = GetCurrentThread();

	// SDL threads available, start 60Hz thread
	tick_thread_active = ((tick_thread = SDL_CreateThread(tick_func, "Redraw Thread", NULL)) != NULL);
	if (!tick_thread_active) {
		sprintf(str, GetString(STR_TICK_THREAD_ERR), strerror(errno));
		ErrorAlert(str);
		QuitEmulator();
	}
	D(bug("60Hz thread started\n"));

	// Start XPRAM watchdog thread
	memcpy(last_xpram, XPRAM, XPRAM_SIZE);
	xpram_thread_active = ((xpram_thread = SDL_CreateThread(xpram_func, "XPRAM Thread", NULL)) != NULL);
	D(bug("XPRAM thread started\n"));

	// Start 68k and jump to ROM boot routine
	D(bug("Starting emulation...\n"));
	Start680x0();

	QuitEmulator();
	return 0;
}


/*
 *  Quit emulator
 */

void QuitEmulator(void)
{
	D(bug("QuitEmulator\n"));

	// Exit 680x0 emulation
	Exit680x0();

	// Stop 60Hz thread
	if (tick_thread_active) {
		tick_thread_cancel = true;
		SDL_WaitThread(tick_thread, NULL);
	}

	// Stop XPRAM watchdog thread
	if (xpram_thread_active) {
		xpram_thread_cancel = true;
		SDL_WaitThread(xpram_thread, NULL);
	}

	// Deinitialize everything
	ExitAll();

	// Free ROM/RAM areas
	if (RAMBaseHost != VM_MAP_FAILED) {
		vm_release(RAMBaseHost, RAMSize);
		RAMBaseHost = NULL;
	}
	if (ROMBaseHost != VM_MAP_FAILED) {
		vm_release(ROMBaseHost, 0x100000);
		ROMBaseHost = NULL;
	}

#if USE_SCRATCHMEM_SUBTERFUGE
	// Delete scratch memory area
	if (ScratchMem != (uint8 *)VM_MAP_FAILED) {
		vm_release((void *)(ScratchMem - SCRATCH_MEM_SIZE/2), SCRATCH_MEM_SIZE);
		ScratchMem = NULL;
	}
#endif

	// Exit VM wrappers
	vm_exit();

	// Exit system routines
	SysExit();

	// Exit preferences
	PrefsExit();

	exit(0);
}


/*
 *  Code was patched, flush caches if neccessary (i.e. when using a real 680x0
 *  or a dynamically recompiling emulator)
 */

void FlushCodeCache(void *start, uint32 size)
{
#if USE_JIT
    if (UseJIT)
		flush_icache_range((uint8 *)start, size);
#endif
}


/*
 *  Mutexes
 */

struct B2_mutex {
	B2_mutex() { m = SDL_CreateMutex(); }
	~B2_mutex() { if (m) SDL_DestroyMutex(m); }
	SDL_Mutex *m;
};

B2_mutex *B2_create_mutex(void)
{
	return new B2_mutex;
}

void B2_lock_mutex(B2_mutex *mutex)
{
	if (mutex)
		SDL_LockMutex(mutex->m);
}

void B2_unlock_mutex(B2_mutex *mutex)
{
	if (mutex)
		SDL_UnlockMutex(mutex->m);
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
	LOCK_INTFLAGS;
	InterruptFlags |= flag;
	UNLOCK_INTFLAGS;
}

void ClearInterruptFlag(uint32 flag)
{
	LOCK_INTFLAGS;
	InterruptFlags &= ~flag;
	UNLOCK_INTFLAGS;
}


/*
 *  XPRAM watchdog thread (saves XPRAM every minute)
 */

static void xpram_watchdog(void)
{
	if (memcmp(last_xpram, XPRAM, XPRAM_SIZE)) {
		memcpy(last_xpram, XPRAM, XPRAM_SIZE);
		SaveXPRAM();
	}
}

static int xpram_func(void *arg)
{
	while (!xpram_thread_cancel) {
		for (int i=0; i<60 && !xpram_thread_cancel; i++)
			Delay_usec(999999);		// Only wait 1 second so we quit promptly when xpram_thread_cancel becomes true
		xpram_watchdog();
	}
	return 0;
}


/*
 *  60Hz thread (really 60.15Hz)
 */

static void one_second(void)
{
	// Pseudo Mac 1Hz interrupt, update local time
	WriteMacInt32(0x20c, TimerDateTime());

	SetInterruptFlag(INTFLAG_1HZ);
	TriggerInterrupt();
}

static void one_tick(...)
{
	static int tick_counter = 0;
	if (++tick_counter > 60) {
		tick_counter = 0;
		one_second();
	}

	// Trigger 60Hz interrupt
	if (ROMVersion != ROM_VERSION_CLASSIC || HasMacStarted()) {
		SetInterruptFlag(INTFLAG_60HZ);
		TriggerInterrupt();
	}
}

bool tick_inhibit;
static int tick_func(void *arg)
{
	uint64 start = GetTicks_usec();
	int64 ticks = 0;
	uint64 next = GetTicks_usec();
	while (!tick_thread_cancel) {
		if (!tick_inhibit)
			one_tick();
		next += 16625;
		int64 delay = next - GetTicks_usec();
		if (delay > 0)
			Delay_usec(uint32(delay));
		else if (delay < -16625)
			next = GetTicks_usec();
		ticks++;
	}
	uint64 end = GetTicks_usec();
	D(bug("%Ld ticks in %Ld usec = %f ticks/sec\n", ticks, end - start, ticks * 1000000.0 / (end - start)));
	return 0;
}


/*
 *  Get the main window handle
 */

#ifdef USE_SDL_VIDEO
#if SDL_VERSION_ATLEAST(3, 0, 0)
#include <SDL_video.h>
#else
#include <SDL_syswm.h>
#endif
extern SDL_Window *sdl_window;
HWND GetMainWindowHandle(void)
{
	if (!sdl_window) {
		return NULL;
	}
#if SDL_VERSION_ATLEAST(3, 0, 0)
	SDL_PropertiesID props = SDL_GetWindowProperties(sdl_window);
	return (HWND)SDL_GetProperty(props, "SDL.window.cocoa.window", NULL);
#else
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	return SDL_GetWindowWMInfo(sdl_window, &wmInfo) ? wmInfo.info.win.window : NULL;
#endif
}
#endif


/*
 *  Display alert
 */

static void display_alert(int title_id, const char *text, int flags)
{
	HWND hMainWnd = GetMainWindowHandle();
	MessageBoxA(hMainWnd, text, GetString(title_id), MB_OK | flags);
}
static void display_alert(int title_id, const wchar_t *text, int flags)
{
	HWND hMainWnd = GetMainWindowHandle();
	MessageBoxW(hMainWnd, text, GetStringW(title_id).get(), MB_OK | flags);
}


/*
 *  Display error alert
 */

void ErrorAlert(const char *text)
{
	if (PrefsFindBool("nogui"))
		return;

	VideoQuitFullScreen();
	display_alert(STR_ERROR_ALERT_TITLE, text, MB_ICONSTOP);
}
void ErrorAlert(const wchar_t *text)
{
	if (PrefsFindBool("nogui"))
		return;

	VideoQuitFullScreen();
	display_alert(STR_ERROR_ALERT_TITLE, text, MB_ICONSTOP);
}


/*
 *  Display warning alert
 */

void WarningAlert(const char *text)
{
	if (PrefsFindBool("nogui"))
		return;

	display_alert(STR_WARNING_ALERT_TITLE, text, MB_ICONINFORMATION);
}
void WarningAlert(const wchar_t *text)
{
	if (PrefsFindBool("nogui"))
		return;

	display_alert(STR_WARNING_ALERT_TITLE, text, MB_ICONINFORMATION);
}


/*
 *  Display choice alert
 */

bool ChoiceAlert(const char *text, const char *pos, const char *neg)
{
	printf(GetString(STR_SHELL_WARNING_PREFIX), text);
	return false;	//!!
}

/*
 *  Low level keyboard hook allowing us to intercept events involving the Windows key
 */
static LRESULT CALLBACK low_level_keyboard_hook(int nCode, WPARAM wParam, LPARAM lParam)
{
	// Not a relevant event, immediately pass it on
	if (nCode != HC_ACTION)
        return CallNextHookEx(keyboard_hook, nCode, wParam, lParam);

	KBDLLHOOKSTRUCT *p = (KBDLLHOOKSTRUCT *)lParam;
	switch (wParam) {
		case WM_KEYDOWN:
		case WM_KEYUP:
			// Intercept left/right windows keys when we have keyboard focus so Windows doesn't handle them
			if (p->vkCode == VK_LWIN || p->vkCode == VK_RWIN) {
				bool intercept_event = false;
#ifdef USE_SDL_VIDEO
				if (sdl_window && (SDL_GetWindowFlags(sdl_window) & SDL_WINDOW_INPUT_FOCUS)) {
					intercept_event = true;
				}
#endif

				// If we've determined we should intercept the event, intercept it. But pass the event onto SDL so Basilisk handles it.
				if (intercept_event) {
					SDL_Event e;
					memset(&e, 0, sizeof(e));
					e.type = (wParam == WM_KEYDOWN) ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
					e.key.keysym.sym = (p->vkCode == VK_LWIN) ? SDLK_LGUI : SDLK_RGUI;
					e.key.keysym.scancode = (p->vkCode == VK_LWIN) ? SDL_SCANCODE_LGUI : SDL_SCANCODE_RGUI;
					SDL_PushEvent(&e);
					return 1;
				}
			}
			break;
	}

	// If we fall here, we weren't supposed to intercept it.
	return CallNextHookEx(keyboard_hook, nCode, wParam, lParam);
}
