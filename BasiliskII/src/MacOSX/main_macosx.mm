/*
 *  $Id$
 *
 *  main_macosx.mm -	Startup code for MacOS X
 *						Based (in a small way) on the default main.m,
						and on Basilisk's main_unix.cpp
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
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
#define PTHREADS	// Why is this here?
#include "sysdeps.h"

#ifdef HAVE_PTHREADS
# include <pthread.h>
#endif

#if REAL_ADDRESSING || DIRECT_ADDRESSING
# include <sys/mman.h>
#endif

#include <string>
using std::string;

#include "cpu_emulation.h"
#include "macos_util_macosx.h"
#include "main.h"
#include "prefs.h"
#include "prefs_editor.h"
#include "rom_patches.h"
#include "sigsegv.h"
#include "sys.h"
#include "user_strings.h"
#include "version.h"
#include "video.h"
#include "vm_alloc.h"
#include "xpram.h"

#if USE_JIT
extern void flush_icache_range(uint32 start, uint32 size);  // from compemu_support.cpp
#endif

#ifdef ENABLE_MON
# include "mon.h"
#endif

#define DEBUG 0
#include "debug.h"


#import <AppKit/AppKit.h>

#include "main_macosx.h"		// To bridge between main() and misc. classes


// Constants
const char ROM_FILE_NAME[] = "ROM";
const int SCRATCH_MEM_SIZE = 0x10000;	// Size of scratch memory area


// CPU and FPU type, addressing mode
int CPUType;
bool CPUIs68060;
int FPUType;
bool TwentyFourBitAddressing;


// Global variables

#ifdef HAVE_PTHREADS

static pthread_mutex_t intflag_lock = PTHREAD_MUTEX_INITIALIZER;	// Mutex to protect InterruptFlags
#define LOCK_INTFLAGS pthread_mutex_lock(&intflag_lock)
#define UNLOCK_INTFLAGS pthread_mutex_unlock(&intflag_lock)

#else

#define LOCK_INTFLAGS
#define UNLOCK_INTFLAGS

#endif

#if USE_SCRATCHMEM_SUBTERFUGE
uint8 *ScratchMem = NULL;			// Scratch memory for Mac ROM writes
#endif

#ifdef ENABLE_MON
static struct sigaction sigint_sa;	// sigaction for SIGINT handler
static void sigint_handler(...);
#endif

#if REAL_ADDRESSING
static bool lm_area_mapped = false;	// Flag: Low Memory area mmap()ped
#endif


/*
 *  SIGSEGV handler
 */

static sigsegv_return_t sigsegv_handler(sigsegv_address_t fault_address,
										sigsegv_address_t fault_instruction)
{
#if ENABLE_VOSF
	// Handle screen fault
	extern bool Screen_fault_handler(sigsegv_address_t, sigsegv_address_t);
	if (Screen_fault_handler(fault_address, fault_instruction))
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

static void sigsegv_dump_state(sigsegv_address_t fault_address, sigsegv_address_t fault_instruction)
{
	fprintf(stderr, "Caught SIGSEGV at address %p", fault_address);
	if (fault_instruction != SIGSEGV_INVALID_PC)
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
	char *arg[4] = {"mon", "-m", "-r", NULL};
	mon(3, arg);
	QuitEmulator();
#endif
}


/*
 *  Main program
 */

static void usage(const char *prg_name)
{
	printf("Usage: %s [OPTION...]\n", prg_name);
	printf("\nUnix options:\n");
	printf("  --help\n    display this usage message\n");
	printf("  --config FILE\n    read/write configuration from/to FILE\n");
	printf("  --break ADDRESS\n    set ROM breakpoint\n");
	printf("  --rominfo\n    dump ROM information\n");
	LoadPrefs(); // read the prefs file so PrefsPrintUsage() will print the correct default values
	PrefsPrintUsage();
	exit(0);
}

int main(int argc, char **argv)
{
	// Initialize variables
	RAMBaseHost = NULL;
	ROMBaseHost = NULL;
	srand(time(NULL));
	tzset();

	// Print some info
	printf(GetString(STR_ABOUT_TEXT1), VERSION_MAJOR, VERSION_MINOR);
	printf(" %s\n", GetString(STR_ABOUT_TEXT2));

	// Parse command line arguments
	for (int i=1; i<argc; i++) {
		if (strcmp(argv[i], "--help") == 0) {
			usage(argv[0]);
		} else if (strncmp(argv[i], "-psn_", 5) == 0) {// OS X process identifier
			i++;
		} else if (strcmp(argv[i], "--break") == 0) {
			i++;
			if (i < argc)
				ROMBreakpoint = strtol(argv[i], NULL, 0);
		} else if (strcmp(argv[i], "--config") == 0) {
			argv[i++] = NULL;
			if (i < argc) {
				extern string UserPrefsPath; // from prefs_unix.cpp
				UserPrefsPath = argv[i];
				argv[i] = NULL;
			}
		} else if (strcmp(argv[i], "--rominfo") == 0) {
			PrintROMInfo = true;
		} else if (argv[i][0] == '-') {
			fprintf(stderr, "Unrecognized option '%s'\n", argv[i]);
			usage(argv[0]);
		}
	}

	// Read preferences
	PrefsInit(argc, argv);

	// Init system routines
	SysInit();

	// Open display, attach to window server,
	// load pre-instantiated classes from MainMenu.nib, start run loop
	int i = NSApplicationMain(argc, (const char **)argv);
	// We currently never get past here, because QuitEmulator() does an exit()

	// Exit system routines
	SysExit();

	// Exit preferences
	PrefsExit();

	return i;
}

#define QuitEmulator()	{ QuitEmuNoExit() ; return NO; }

bool InitEmulator (void)
{
	char str[256];


	// Install the handler for SIGSEGV
	if (!sigsegv_install_handler(sigsegv_handler)) {
		sprintf(str, GetString(STR_SIG_INSTALL_ERR), "SIGSEGV", strerror(errno));
		ErrorAlert(str);
		QuitEmulator();
	}

	// Register dump state function when we got mad after a segfault
	sigsegv_set_dump_state(sigsegv_dump_state);

	// Read RAM size
	RAMSize = PrefsFindInt32("ramsize") & 0xfff00000;	// Round down to 1MB boundary
	if (RAMSize < 1024*1024) {
		WarningAlert(GetString(STR_SMALL_RAM_WARN));
		RAMSize = 1024*1024;
	}

#if REAL_ADDRESSING || DIRECT_ADDRESSING
	RAMSize = RAMSize & -getpagesize();					// Round down to page boundary
#endif
	
	// Initialize VM system
	vm_init();

#if REAL_ADDRESSING
	// Flag: RAM and ROM are contigously allocated from address 0
	bool memory_mapped_from_zero = false;
	
	// Under Solaris/SPARC and NetBSD/m68k, Basilisk II is known to crash
	// when trying to map a too big chunk of memory starting at address 0
#if defined(OS_solaris) || defined(OS_netbsd) || defined(PAGEZERO_HACK)
	const bool can_map_all_memory = false;
#else
	const bool can_map_all_memory = true;
#endif
	
	// Try to allocate all memory from 0x0000, if it is not known to crash
	if (can_map_all_memory && (vm_acquire_fixed(0, RAMSize + 0x100000) == 0)) {
		D(bug("Could allocate RAM and ROM from 0x0000\n"));
		memory_mapped_from_zero = true;
	}

#ifndef PAGEZERO_HACK
	// Otherwise, just create the Low Memory area (0x0000..0x2000)
	else if (vm_acquire_fixed(0, 0x2000) == 0) {
		D(bug("Could allocate the Low Memory globals\n"));
		lm_area_mapped = true;
	}
	
	// Exit on failure
	else {
		sprintf(str, GetString(STR_LOW_MEM_MMAP_ERR), strerror(errno));
		ErrorAlert(str);
		QuitEmulator();
	}
#endif
#else
	*str = 0;		// Eliminate unused variable warning
#endif /* REAL_ADDRESSING */

	// Create areas for Mac RAM and ROM
#if REAL_ADDRESSING
	if (memory_mapped_from_zero) {
		RAMBaseHost = (uint8 *)0;
		ROMBaseHost = RAMBaseHost + RAMSize;
	}
	else
#endif
	{
		RAMBaseHost = (uint8 *)vm_acquire(RAMSize);
		ROMBaseHost = (uint8 *)vm_acquire(0x100000);
		if (RAMBaseHost == VM_MAP_FAILED || ROMBaseHost == VM_MAP_FAILED) {
			ErrorAlert(STR_NO_MEM_ERR);
			QuitEmulator();
		}
	}

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
#if REAL_ADDRESSING
	RAMBaseMac = (uint32)RAMBaseHost;
	ROMBaseMac = (uint32)ROMBaseHost;
#endif
	D(bug("Mac RAM starts at %p (%08x)\n", RAMBaseHost, RAMBaseMac));
	D(bug("Mac ROM starts at %p (%08x)\n", ROMBaseHost, ROMBaseMac));
	
	// Get rom file path from preferences
	const char *rom_path = PrefsFindString("rom");
	if ( ! rom_path )
		WarningAlert("No rom pathname set. Trying ./ROM");

	// Load Mac ROM
	int rom_fd = open(rom_path ? rom_path : ROM_FILE_NAME, O_RDONLY);
	if (rom_fd < 0) {
		ErrorAlert(STR_NO_ROM_FILE_ERR);
		QuitEmulator();
	}
	printf(GetString(STR_READING_ROM_FILE));
	ROMSize = lseek(rom_fd, 0, SEEK_END);
	if (ROMSize != 64*1024 && ROMSize != 128*1024 && ROMSize != 256*1024 && ROMSize != 512*1024 && ROMSize != 1024*1024) {
		ErrorAlert(STR_ROM_SIZE_ERR);
		close(rom_fd);
		QuitEmulator();
	}
	lseek(rom_fd, 0, SEEK_SET);
	if (read(rom_fd, ROMBaseHost, ROMSize) != (ssize_t)ROMSize) {
		ErrorAlert(STR_ROM_FILE_READ_ERR);
		close(rom_fd);
		QuitEmulator();
	}


	// Initialize everything
	if (!InitAll())
		QuitEmulator();
	D(bug("Initialization complete\n"));


#ifdef ENABLE_MON
	// Setup SIGINT handler to enter mon
	sigemptyset(&sigint_sa.sa_mask);
	sigint_sa.sa_handler = (void (*)(int))sigint_handler;
	sigint_sa.sa_flags = 0;
	sigaction(SIGINT, &sigint_sa, NULL);
#endif


	return YES;
}

#undef QuitEmulator()


/*
 *  Quit emulator
 */

void QuitEmuNoExit()
{
	D(bug("QuitEmulator\n"));

	// Exit 680x0 emulation
	Exit680x0();

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

#if REAL_ADDRESSING
	// Delete Low Memory area
	if (lm_area_mapped)
		vm_release(0, 0x2000);
#endif
	
	// Exit VM wrappers
	vm_exit();

	// Exit system routines
	SysExit();

	// Exit preferences
	PrefsExit();
}

void QuitEmulator(void)
{
	extern	NSApplication *NSApp;


	QuitEmuNoExit();

	// Stop run loop?
	[NSApp terminate: nil];

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
		flush_icache_range((uintptr)start, size);
#endif
}


/*
 *  SIGINT handler, enters mon
 */

#ifdef ENABLE_MON
static void sigint_handler(...)
{
	uaecptr nextpc;
	extern void m68k_dumpstate(uaecptr *nextpc);
	m68k_dumpstate(&nextpc);
	VideoQuitFullScreen();
	char *arg[4] = {"mon", "-m", "-r", NULL};
	mon(3, arg);
	QuitEmulator();
}
#endif


/*
 *  Mutexes
 */

#ifdef HAVE_PTHREADS

struct B2_mutex {
	B2_mutex() {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		// Initialize the mutex for priority inheritance --
		// required for accurate timing.
#ifdef HAVE_PTHREAD_MUTEXATTR_SETPROTOCOL
		pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
#endif
#if defined(HAVE_PTHREAD_MUTEXATTR_SETTYPE) && defined(PTHREAD_MUTEX_NORMAL)
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
#endif
#ifdef HAVE_PTHREAD_MUTEXATTR_SETPSHARED
		pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_PRIVATE);
#endif
		pthread_mutex_init(&m, &attr);
		pthread_mutexattr_destroy(&attr);
	}
	~B2_mutex() {
		pthread_mutex_trylock(&m);	// Make sure it's locked before
		pthread_mutex_unlock(&m);	// unlocking it.
		pthread_mutex_destroy(&m);
	}
	pthread_mutex_t m;
};

B2_mutex *B2_create_mutex(void)
{
	return new B2_mutex;
}

void B2_lock_mutex(B2_mutex *mutex)
{
	pthread_mutex_lock(&mutex->m);
}

void B2_unlock_mutex(B2_mutex *mutex)
{
	pthread_mutex_unlock(&mutex->m);
}

void B2_delete_mutex(B2_mutex *mutex)
{
	delete mutex;
}

#else

struct B2_mutex {
	int dummy;
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

#endif


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
 *  Display error alert
 */

void ErrorAlert(const char *text)
{
	NSString *title  = [NSString stringWithCString:
						GetString(STR_ERROR_ALERT_TITLE) ];
	NSString *error  = [NSString stringWithCString: text];
	NSString *button = [NSString stringWithCString: GetString(STR_QUIT_BUTTON) ];

	NSLog(error);
	if ( PrefsFindBool("nogui") )
		return;
	VideoQuitFullScreen();
	NSRunCriticalAlertPanel(title, error, button, nil, nil);
}


/*
 *  Display warning alert
 */

void WarningAlert(const char *text)
{
	NSString *title   = [NSString stringWithCString:
							GetString(STR_WARNING_ALERT_TITLE) ];
	NSString *warning = [NSString stringWithCString: text];
	NSString *button  = [NSString stringWithCString: GetString(STR_OK_BUTTON) ];

	NSLog(warning);
	if ( PrefsFindBool("nogui") )
		return;
	VideoQuitFullScreen();
	NSRunAlertPanel(title, warning, button, nil, nil);
}


/*
 *  Display choice alert
 */

bool ChoiceAlert(const char *text, const char *pos, const char *neg)
{
	NSString *title   = [NSString stringWithCString:
							GetString(STR_WARNING_ALERT_TITLE) ];
	NSString *warning = [NSString stringWithCString: text];
	NSString *yes	  = [NSString stringWithCString: pos];
	NSString *no	  = [NSString stringWithCString: neg];

	return NSRunInformationalAlertPanel(title, warning, yes, no, nil);
}
