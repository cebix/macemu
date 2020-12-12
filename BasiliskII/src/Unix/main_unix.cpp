/*
 *  main_unix.cpp - Startup code for Unix
 *
 *  Basilisk II (C) Christian Bauer
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
#include <sstream>

#ifdef USE_SDL
# include <SDL.h>
# include <SDL_main.h>
#endif

#ifdef HAVE_PTHREADS
# include <pthread.h>
#endif

#if DIRECT_ADDRESSING
# include <sys/mman.h>
#endif

#ifdef ENABLE_GTK
# include <gtk/gtk.h>
# include <gdk/gdk.h>
# ifdef HAVE_GNOMEUI
#  include <gnome.h>
# endif
# if !defined(GDK_WINDOWING_QUARTZ) && !defined(GDK_WINDOWING_WAYLAND)
#  include <X11/Xlib.h>
# endif
#endif

#include <string>
using std::string;

#include "cpu_emulation.h"
#include "sys.h"
#include "rom_patches.h"
#include "xpram.h"
#include "timer.h"
#include "video.h"
#include "emul_op.h"
#include "prefs.h"
#include "prefs_editor.h"
#include "macos_util.h"
#include "user_strings.h"
#include "version.h"
#include "main.h"
#include "vm_alloc.h"
#include "sigsegv.h"
#include "rpc.h"

#if USE_JIT
#ifdef UPDATE_UAE
extern void (*flush_icache)(void); // from compemu_support.cpp
extern bool UseJIT;
#else
extern void flush_icache_range(uint8 *start, uint32 size); // from compemu_support.cpp
#endif
#endif

#ifdef ENABLE_MON
# include "mon.h"
#endif

#define DEBUG 0
#include "debug.h"

// Constants
const char ROM_FILE_NAME[] = "ROM";
const int SCRATCH_MEM_SIZE = 0x10000;	// Size of scratch memory area

// CPU and FPU type, addressing mode
int CPUType;
bool CPUIs68060;
int FPUType;
bool TwentyFourBitAddressing;

static uint8 last_xpram[XPRAM_SIZE];				// Buffer for monitoring XPRAM changes

#ifdef HAVE_PTHREADS
static bool xpram_thread_active = false;			// Flag: XPRAM watchdog installed
static volatile bool xpram_thread_cancel = false;	// Flag: Cancel XPRAM thread
static pthread_t xpram_thread;						// XPRAM watchdog

static bool tick_thread_active = false;				// Flag: 60Hz thread installed
static volatile bool tick_thread_cancel = false;	// Flag: Cancel 60Hz thread
static pthread_t tick_thread;						// 60Hz thread
static pthread_attr_t tick_thread_attr;				// 60Hz thread attributes

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

#if !defined(HAVE_PTHREADS)
static struct sigaction timer_sa;	// sigaction used for timer

#if defined(HAVE_TIMER_CREATE) && defined(_POSIX_REALTIME_SIGNALS)
#define SIG_TIMER SIGRTMIN
static timer_t timer;				// 60Hz timer
#endif
#endif // !HAVE_PTHREADS

#ifdef ENABLE_MON
static struct sigaction sigint_sa;	// sigaction for SIGINT handler
static void sigint_handler(...);
#endif

static rpc_connection_t *gui_connection = NULL;	// RPC connection to the GUI
static const char *gui_connection_path = NULL;	// GUI connection identifier


// Prototypes
static void *xpram_func(void *arg);
static void *tick_func(void *arg);
static void one_tick(...);

// vde switch variable
char* vde_sock;

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
 *  Helpers to map memory that can be accessed from the Mac side
 */

// NOTE: VM_MAP_32BIT is only used when compiling a 64-bit JIT on specific platforms
void *vm_acquire_mac(size_t size)
{
	return vm_acquire(size, VM_MAP_DEFAULT | VM_MAP_32BIT);
}

static int vm_acquire_mac_fixed(void *addr, size_t size)
{
	return vm_acquire_fixed(addr, size, VM_MAP_DEFAULT | VM_MAP_32BIT);
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
#ifdef UPDATE_UAE
	extern void m68k_dumpstate(FILE *, uaecptr *nextpc);
	m68k_dumpstate(stderr, &nextpc);
#else
	extern void m68k_dumpstate(uaecptr *nextpc);
	m68k_dumpstate(&nextpc);
#endif
#if USE_JIT && JIT_DEBUG
	extern void compiler_dumpstate(void);
	compiler_dumpstate();
#endif
	VideoQuitFullScreen();
#ifdef ENABLE_MON
	const char *arg[4] = {"mon", "-m", "-r", NULL};
	mon(3, arg);
#endif
	QuitEmulator();
}


/*
 *  Update virtual clock and trigger interrupts if necessary
 */

#ifdef USE_CPU_EMUL_SERVICES
static uint64 n_check_ticks = 0;
static uint64 emulated_ticks_start = 0;
static uint64 emulated_ticks_count = 0;
static int64 emulated_ticks_current = 0;
static int32 emulated_ticks_quantum = 1000;
int32 emulated_ticks = emulated_ticks_quantum;

void cpu_do_check_ticks(void)
{
#if DEBUG
	n_check_ticks++;
#endif

	uint64 now;
	static uint64 next = 0;
	if (next == 0)
		next = emulated_ticks_start = GetTicks_usec();

	// Update total instructions count
	if (emulated_ticks <= 0) {
		emulated_ticks_current += (emulated_ticks_quantum - emulated_ticks);
		// XXX: can you really have a machine fast enough to overflow
		// a 63-bit m68k instruction counter within 16 ms?
		if (emulated_ticks_current < 0) {
			printf("WARNING: Overflowed 63-bit m68k instruction counter in less than 16 ms!\n");
			goto recalibrate_quantum;
		}
	}

	// Check for interrupt opportunity
	now = GetTicks_usec();
	if (next < now) {
		one_tick();
		do {
			next += 16625;
		} while (next < now);
		emulated_ticks_count++;

		// Recalibrate 1000 Hz quantum every 10 ticks
		static uint64 last = 0;
		if (last == 0)
			last = now;
		else if (now - last > 166250) {
		  recalibrate_quantum:
			emulated_ticks_quantum = ((uint64)emulated_ticks_current * 1000) / (now - last);
			emulated_ticks_current = 0;
			last = now;
		}
	}

	// Update countdown
	if (emulated_ticks <= 0)
		emulated_ticks += emulated_ticks_quantum;
}
#endif


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
		"  --break ADDRESS\n    set ROM breakpoint in hexadecimal\n"
		"  --loadbreak FILE\n    load breakpoint from FILE\n"
		"  --rominfo\n    dump ROM information\n"
		"  --switch SWITCH_PATH\n    vde_switch address\n", prg_name
	);
	LoadPrefs(NULL); // read the prefs file so PrefsPrintUsage() will print the correct default values
	PrefsPrintUsage();
	exit(0);
}

int main(int argc, char **argv)
{
#if defined(ENABLE_GTK) && !defined(GDK_WINDOWING_QUARTZ) && !defined(GDK_WINDOWING_WAYLAND)
	XInitThreads();
#endif
	const char *vmdir = NULL;
	char str[256];

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
		} else if (strcmp(argv[i], "--gui-connection") == 0) {
			argv[i++] = NULL;
			if (i < argc) {
				gui_connection_path = argv[i];
				argv[i] = NULL;
			}
		} else if (strcmp(argv[i], "--break") == 0) {
			argv[i++] = NULL;
			if (i < argc) {
				std::stringstream ss;
				ss << std::hex << argv[i];
				ss >> ROMBreakpoint;
				argv[i] = NULL;
			}
#ifdef ENABLE_MON
		} else if (strcmp(argv[i], "--loadbreak") == 0) {
			argv[i++] = NULL;
			if (i < argc)
				mon_load_break_point(argv[i]);
#endif
		} else if (strcmp(argv[i], "--config") == 0) {
			argv[i++] = NULL;
			if (i < argc) {
				extern string UserPrefsPath; // from prefs_unix.cpp
				UserPrefsPath = argv[i];
				argv[i] = NULL;
			}
		} else if (strcmp(argv[i], "--rominfo") == 0) {
			argv[i] = NULL;
			PrintROMInfo = true;
		} else if (strcmp(argv[i], "--switch") == 0) {
			argv[i] = NULL;
			if (argv[++i] == NULL) {
				printf("switch address not defined\n");
				usage(argv[0]);
			}
			vde_sock = argv[i];
			argv[i] = NULL;
		}
		
#if defined(__APPLE__) && defined(__MACH__)
		// Mac OS X likes to pass in various options of its own, when launching an app.
		// Attempt to ignore these.
		if (argv[i]) {
			const char * mac_psn_prefix = "-psn_";
			if (strcmp(argv[i], "-NSDocumentRevisionsDebugMode") == 0) {
				argv[i] = NULL;
			} else if (strncmp(mac_psn_prefix, argv[i], strlen(mac_psn_prefix)) == 0) {
				argv[i] = NULL;
			}
		}
#endif
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

	// Connect to the external GUI
	if (gui_connection_path) {
		if ((gui_connection = rpc_init_client(gui_connection_path)) == NULL) {
			fprintf(stderr, "Failed to initialize RPC client connection to the GUI\n");
			return 1;
		}
	}

#ifdef ENABLE_GTK
	if (!gui_connection) {
#ifdef HAVE_GNOMEUI
		// Init GNOME/GTK
		char version[16];
		sprintf(version, "%d.%d", VERSION_MAJOR, VERSION_MINOR);
		gnome_init("Basilisk II", version, argc, argv);
#else
		// Init GTK
		gtk_set_locale();
		gtk_init(&argc, &argv);
#endif
	}
#endif

	// Read preferences
	PrefsInit(vmdir, argc, argv);

	// Any command line arguments left?
	for (int i=1; i<argc; i++) {
		if (argv[i][0] == '-') {
			fprintf(stderr, "Unrecognized option '%s'\n", argv[i]);
			usage(argv[0]);
		}
	}

#ifdef USE_SDL
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

#if __MACOSX__ && SDL_VERSION_ATLEAST(2,0,0)
	// On Mac OS X hosts, SDL2 will create its own menu bar.  This is mostly OK,
	// except that it will also install keyboard shortcuts, such as Command + Q,
	// which can interfere with keyboard shortcuts in the guest OS.
	//
	// HACK: disable these shortcuts, while leaving all other pieces of SDL2's
	// menu bar in-place.
	extern void disable_SDL2_macosx_menu_bar_keyboard_shortcuts();
	disable_SDL2_macosx_menu_bar_keyboard_shortcuts();
#endif
	
#endif

	// Init system routines
	SysInit();

	// Show preferences editor
	if (!gui_connection && !PrefsFindBool("nogui"))
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
	if (RAMSize > 1023*1024*1024)						// Cap to 1023MB (APD crashes at 1GB)
		RAMSize = 1023*1024*1024;

#if DIRECT_ADDRESSING
	RAMSize = RAMSize & -getpagesize();					// Round down to page boundary
#endif
	
	// Initialize VM system
	vm_init();

	// Create areas for Mac RAM and ROM
	{
		uint8 *ram_rom_area = (uint8 *)vm_acquire_mac(RAMSize + 0x100000);
		if (ram_rom_area == VM_MAP_FAILED) {	
			ErrorAlert(STR_NO_MEM_ERR);
			QuitEmulator();
		}
		RAMBaseHost = ram_rom_area;
		ROMBaseHost = RAMBaseHost + RAMSize;
	}

#if USE_SCRATCHMEM_SUBTERFUGE
	// Allocate scratch memory
	ScratchMem = (uint8 *)vm_acquire_mac(SCRATCH_MEM_SIZE);
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

#if __MACOSX__
	extern void set_current_directory();
	set_current_directory();
#endif

	// Get rom file path from preferences
	const char *rom_path = PrefsFindString("rom");

	// Load Mac ROM
	int rom_fd = open(rom_path ? rom_path : ROM_FILE_NAME, O_RDONLY);
	if (rom_fd < 0) {
		ErrorAlert(STR_NO_ROM_FILE_ERR);
		QuitEmulator();
	}
	printf("%s", GetString(STR_READING_ROM_FILE));
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
	if (!InitAll(vmdir))
		QuitEmulator();
	D(bug("Initialization complete\n"));

	D(bug("Mac RAM starts at %p (%08x)\n", RAMBaseHost, RAMBaseMac));
	D(bug("Mac ROM starts at %p (%08x)\n", ROMBaseHost, ROMBaseMac));

#ifdef ENABLE_MON
	// Setup SIGINT handler to enter mon
	sigemptyset(&sigint_sa.sa_mask);
	sigint_sa.sa_handler = (void (*)(int))sigint_handler;
	sigint_sa.sa_flags = 0;
	sigaction(SIGINT, &sigint_sa, NULL);
#endif

#ifndef USE_CPU_EMUL_SERVICES
#if defined(HAVE_PTHREADS)

	// POSIX threads available, start 60Hz thread
	Set_pthread_attr(&tick_thread_attr, 0);
	tick_thread_active = (pthread_create(&tick_thread, &tick_thread_attr, tick_func, NULL) == 0);
	if (!tick_thread_active) {
		sprintf(str, GetString(STR_TICK_THREAD_ERR), strerror(errno));
		ErrorAlert(str);
		QuitEmulator();
	}
	D(bug("60Hz thread started\n"));

#elif defined(HAVE_TIMER_CREATE) && defined(_POSIX_REALTIME_SIGNALS)

	// POSIX.4 timers and real-time signals available, start 60Hz timer
	sigemptyset(&timer_sa.sa_mask);
	timer_sa.sa_sigaction = (void (*)(int, siginfo_t *, void *))one_tick;
	timer_sa.sa_flags = SA_SIGINFO | SA_RESTART;
	if (sigaction(SIG_TIMER, &timer_sa, NULL) < 0) {
		sprintf(str, GetString(STR_SIG_INSTALL_ERR), "SIG_TIMER", strerror(errno));
		ErrorAlert(str);
		QuitEmulator();
	}
	struct sigevent timer_event;
	timer_event.sigev_notify = SIGEV_SIGNAL;
	timer_event.sigev_signo = SIG_TIMER;
	if (timer_create(CLOCK_REALTIME, &timer_event, &timer) < 0) {
		sprintf(str, GetString(STR_TIMER_CREATE_ERR), strerror(errno));
		ErrorAlert(str);
		QuitEmulator();
	}
	struct itimerspec req;
	req.it_value.tv_sec = 0;
	req.it_value.tv_nsec = 16625000;
	req.it_interval.tv_sec = 0;
	req.it_interval.tv_nsec = 16625000;
	if (timer_settime(timer, 0, &req, NULL) < 0) {
		sprintf(str, GetString(STR_TIMER_SETTIME_ERR), strerror(errno));
		ErrorAlert(str);
		QuitEmulator();
	}
	D(bug("60Hz timer started\n"));

#else

	// Start 60Hz timer
	sigemptyset(&timer_sa.sa_mask);		// Block virtual 68k interrupts during SIGARLM handling
	timer_sa.sa_handler = one_tick;
	timer_sa.sa_flags = SA_ONSTACK | SA_RESTART;
	if (sigaction(SIGALRM, &timer_sa, NULL) < 0) {
		sprintf(str, GetString(STR_SIG_INSTALL_ERR), "SIGALRM", strerror(errno));
		ErrorAlert(str);
		QuitEmulator();
	}
	struct itimerval req;
	req.it_interval.tv_sec = req.it_value.tv_sec = 0;
	req.it_interval.tv_usec = req.it_value.tv_usec = 16625;
	setitimer(ITIMER_REAL, &req, NULL);

#endif
#endif

#ifdef USE_PTHREADS_SERVICES
	// Start XPRAM watchdog thread
	memcpy(last_xpram, XPRAM, XPRAM_SIZE);
	xpram_thread_active = (pthread_create(&xpram_thread, NULL, xpram_func, NULL) == 0);
	D(bug("XPRAM thread started\n"));
#endif

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

#if defined(USE_CPU_EMUL_SERVICES)
	// Show statistics
	uint64 emulated_ticks_end = GetTicks_usec();
	D(bug("%ld ticks in %ld usec = %f ticks/sec [%ld tick checks]\n",
		  (long)emulated_ticks_count, (long)(emulated_ticks_end - emulated_ticks_start),
		  emulated_ticks_count * 1000000.0 / (emulated_ticks_end - emulated_ticks_start), (long)n_check_ticks));
#elif defined(USE_PTHREADS_SERVICES)
	// Stop 60Hz thread
	if (tick_thread_active) {
		tick_thread_cancel = true;
#ifdef HAVE_PTHREAD_CANCEL
		pthread_cancel(tick_thread);
#endif
		pthread_join(tick_thread, NULL);
	}
#elif defined(HAVE_TIMER_CREATE) && defined(_POSIX_REALTIME_SIGNALS)
	// Stop 60Hz timer
	timer_delete(timer);
#else
	struct itimerval req;
	req.it_interval.tv_sec = req.it_value.tv_sec = 0;
	req.it_interval.tv_usec = req.it_value.tv_usec = 0;
	setitimer(ITIMER_REAL, &req, NULL);
#endif

#ifdef USE_PTHREADS_SERVICES
	// Stop XPRAM watchdog thread
	if (xpram_thread_active) {
		xpram_thread_cancel = true;
#ifdef HAVE_PTHREAD_CANCEL
		pthread_cancel(xpram_thread);
#endif
		pthread_join(xpram_thread, NULL);
	}
#endif

	// Deinitialize everything
	ExitAll();

	// Free ROM/RAM areas
	if (RAMBaseHost != VM_MAP_FAILED) {
		vm_release(RAMBaseHost, RAMSize + 0x100000);
		RAMBaseHost = NULL;
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

	// Notify GUI we are about to leave
	if (gui_connection) {
		if (rpc_method_invoke(gui_connection, RPC_METHOD_EXIT, RPC_TYPE_INVALID) == RPC_ERROR_NO_ERROR)
			rpc_method_wait_for_reply(gui_connection, RPC_TYPE_INVALID);
	}

	exit(0);
}


/*
 *  Code was patched, flush caches if neccessary (i.e. when using a real 680x0
 *  or a dynamically recompiling emulator)
 */

void FlushCodeCache(void *start, uint32 size){
#if USE_JIT
    if (UseJIT)
#ifdef UPDATE_UAE
		flush_icache();
#else
		flush_icache_range((uint8 *)start, size);
#endif
#endif
}


/*
 *  SIGINT handler, enters mon
 */

#ifdef ENABLE_MON
static void sigint_handler(...){
	uaecptr nextpc;
#ifdef UPDATE_UAE
	extern void m68k_dumpstate(FILE *, uaecptr *nextpc);
	m68k_dumpstate(stderr, &nextpc);
#else
	extern void m68k_dumpstate(uaecptr *nextpc);
	m68k_dumpstate(&nextpc);
#endif
	VideoQuitFullScreen();
	const char *arg[4] = {"mon", "-m", "-r", NULL};
	mon(3, arg);
	QuitEmulator();
}
#endif


#ifdef HAVE_PTHREADS
/*
 *  Pthread configuration
 */

void Set_pthread_attr(pthread_attr_t *attr, int priority)
{
	pthread_attr_init(attr);
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING)
	// Some of these only work for superuser
	if (geteuid() == 0) {
		pthread_attr_setinheritsched(attr, PTHREAD_EXPLICIT_SCHED);
		pthread_attr_setschedpolicy(attr, SCHED_FIFO);
		struct sched_param fifo_param;
		fifo_param.sched_priority = ((sched_get_priority_min(SCHED_FIFO) + 
					      sched_get_priority_max(SCHED_FIFO)) / 2 +
					     priority);
		pthread_attr_setschedparam(attr, &fifo_param);
	}
	if (pthread_attr_setscope(attr, PTHREAD_SCOPE_SYSTEM) != 0) {
#ifdef PTHREAD_SCOPE_BOUND_NP
	    // If system scope is not available (eg. we're not running
	    // with CAP_SCHED_MGT capability on an SGI box), try bound
	    // scope.  It exposes pthread scheduling to the kernel,
	    // without setting realtime priority.
	    pthread_attr_setscope(attr, PTHREAD_SCOPE_BOUND_NP);
#endif
	}
#endif
}
#endif // HAVE_PTHREADS


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
#if defined(HAVE_PTHREAD_MUTEXATTR_SETPROTOCOL) && !defined(__CYGWIN__)
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
	    pthread_mutex_trylock(&m); // Make sure it's locked before
	    pthread_mutex_unlock(&m);  // unlocking it.
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

void SetInterruptFlag(uint32 flag){
	LOCK_INTFLAGS;
	InterruptFlags |= flag;
	UNLOCK_INTFLAGS;
}

void ClearInterruptFlag(uint32 flag){
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

#ifdef USE_PTHREADS_SERVICES
static void *xpram_func(void *arg)
{
	while (!xpram_thread_cancel) {
		for (int i=0; i<60 && !xpram_thread_cancel; i++)
			Delay_usec(999999);		// Only wait 1 second so we quit promptly when xpram_thread_cancel becomes true
		xpram_watchdog();
	}
	return NULL;
}
#endif


/*
 *  60Hz thread (really 60.15Hz)
 */

static void one_second(void)
{
	// Pseudo Mac 1Hz interrupt, update local time
	WriteMacInt32(0x20c, TimerDateTime());

	SetInterruptFlag(INTFLAG_1HZ);
	TriggerInterrupt();

#ifndef USE_PTHREADS_SERVICES
	static int second_counter = 0;
	if (++second_counter > 60) {
		second_counter = 0;
		xpram_watchdog();
	}
#endif
}

static void one_tick(...)
{
	static int tick_counter = 0;
	if (++tick_counter > 60) {
		tick_counter = 0;
		one_second();
	}

#ifndef USE_PTHREADS_SERVICES
	// Threads not used to trigger interrupts, perform video refresh from here
	VideoRefresh();
#endif

#ifndef HAVE_PTHREADS
	// No threads available, perform networking from here
	SetInterruptFlag(INTFLAG_ETHER);
#endif

	// Trigger 60Hz interrupt
	if (ROMVersion != ROM_VERSION_CLASSIC || HasMacStarted()) {
		SetInterruptFlag(INTFLAG_60HZ);
		TriggerInterrupt();
	}
}

#ifdef USE_PTHREADS_SERVICES
static void *tick_func(void *arg)
{
	uint64 start = GetTicks_usec();
	int64 ticks = 0;
	uint64 next = GetTicks_usec();
	while (!tick_thread_cancel) {
		one_tick();
		next += 16625;
		int64 delay = next - GetTicks_usec();
		if (delay > 0)
			Delay_usec(delay);
		else if (delay < -16625)
			next = GetTicks_usec();
		ticks++;
	}
	uint64 end = GetTicks_usec();
	D(bug("%lld ticks in %lld usec = %f ticks/sec\n", ticks, end - start, ticks * 1000000.0 / (end - start)));
	return NULL;
}
#endif

/*
 *  Display alert
 */

#ifdef ENABLE_GTK
static void dl_destroyed(void)
{
	gtk_main_quit();
}

static void dl_quit(GtkWidget *dialog)
{
	gtk_widget_destroy(dialog);
}

void display_alert(int title_id, int prefix_id, int button_id, const char *text)
{
	char str[256];
	sprintf(str, GetString(prefix_id), text);

	GtkWidget *dialog = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dialog), GetString(title_id));
	gtk_container_border_width(GTK_CONTAINER(dialog), 5);
	gtk_widget_set_uposition(GTK_WIDGET(dialog), 100, 150);
	gtk_signal_connect(GTK_OBJECT(dialog), "destroy", GTK_SIGNAL_FUNC(dl_destroyed), NULL);

	GtkWidget *label = gtk_label_new(str);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label, TRUE, TRUE, 0);

	GtkWidget *button = gtk_button_new_with_label(GetString(button_id));
	gtk_widget_show(button);
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(dl_quit), GTK_OBJECT(dialog));
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area), button, FALSE, FALSE, 0);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_grab_default(button);
	gtk_widget_show(dialog);

	gtk_main();
}
#endif


/*
 *  Display error alert
 */

void ErrorAlert(const char *text)
{
	if (gui_connection) {
		if (rpc_method_invoke(gui_connection, RPC_METHOD_ERROR_ALERT, RPC_TYPE_STRING, text, RPC_TYPE_INVALID) == RPC_ERROR_NO_ERROR &&
			rpc_method_wait_for_reply(gui_connection, RPC_TYPE_INVALID) == RPC_ERROR_NO_ERROR)
			return;
	}
#if defined(ENABLE_GTK) && !defined(USE_SDL_VIDEO)
	if (PrefsFindBool("nogui") || x_display == NULL) {
		printf(GetString(STR_SHELL_ERROR_PREFIX), text);
		return;
	}
	VideoQuitFullScreen();
	display_alert(STR_ERROR_ALERT_TITLE, STR_GUI_ERROR_PREFIX, STR_QUIT_BUTTON, text);
#else
	printf(GetString(STR_SHELL_ERROR_PREFIX), text);
#endif
}


/*
 *  Display warning alert
 */

void WarningAlert(const char *text)
{
	if (gui_connection) {
		if (rpc_method_invoke(gui_connection, RPC_METHOD_WARNING_ALERT, RPC_TYPE_STRING, text, RPC_TYPE_INVALID) == RPC_ERROR_NO_ERROR &&
			rpc_method_wait_for_reply(gui_connection, RPC_TYPE_INVALID) == RPC_ERROR_NO_ERROR)
			return;
	}
#if defined(ENABLE_GTK) && !defined(USE_SDL_VIDEO)
	if (PrefsFindBool("nogui") || x_display == NULL) {
		printf(GetString(STR_SHELL_WARNING_PREFIX), text);
		return;
	}
	display_alert(STR_WARNING_ALERT_TITLE, STR_GUI_WARNING_PREFIX, STR_OK_BUTTON, text);
#else
	printf(GetString(STR_SHELL_WARNING_PREFIX), text);
#endif
}


/*
 *  Display choice alert
 */

bool ChoiceAlert(const char *text, const char *pos, const char *neg)
{
	printf(GetString(STR_SHELL_WARNING_PREFIX), text);
	return false;	//!!
}
