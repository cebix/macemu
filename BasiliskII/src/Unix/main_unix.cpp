/*
 *  main_unix.cpp - Startup code for Unix
 *
 *  Basilisk II (C) 1997-2000 Christian Bauer
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
#include <X11/Xlib.h>

#ifdef HAVE_PTHREADS
# include <pthread.h>
#endif

#if defined(USE_MAPPED_MEMORY) || REAL_ADDRESSING || DIRECT_ADDRESSING
# include <sys/mman.h>
#endif

#if !EMULATED_68K && defined(__NetBSD__)
# include <m68k/sync_icache.h> 
# include <m68k/frame.h>
# include <sys/param.h>
# include <sys/sysctl.h>
struct sigstate {
	int ss_flags;
	struct frame ss_frame;
	struct fpframe ss_fpstate;
};
# define SS_FPSTATE  0x02
# define SS_USERREGS 0x04
#endif

#ifdef ENABLE_GTK
# include <gtk/gtk.h>
#endif

#ifdef ENABLE_XF86_DGA
# include <X11/Xutil.h>
# include <X11/extensions/xf86dga.h>
#endif

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

#ifdef ENABLE_MON
# include "mon.h"
#endif

#define DEBUG 0
#include "debug.h"


// Constants
const char ROM_FILE_NAME[] = "ROM";
const int SIG_STACK_SIZE = SIGSTKSZ;	// Size of signal stack
const int SCRATCH_MEM_SIZE = 0x10000;	// Size of scratch memory area


#if !EMULATED_68K
// RAM and ROM pointers
uint32 RAMBaseMac;		// RAM base (Mac address space)
uint8 *RAMBaseHost;		// RAM base (host address space)
uint32 RAMSize;			// Size of RAM
uint32 ROMBaseMac;		// ROM base (Mac address space)
uint8 *ROMBaseHost;		// ROM base (host address space)
uint32 ROMSize;			// Size of ROM
#endif


// CPU and FPU type, addressing mode
int CPUType;
bool CPUIs68060;
int FPUType;
bool TwentyFourBitAddressing;


// Global variables
char *x_display_name = NULL;						// X11 display name
Display *x_display = NULL;							// X11 display handle

static int zero_fd = -1;							// FD of /dev/zero
static uint8 last_xpram[256];						// Buffer for monitoring XPRAM changes

#ifdef HAVE_PTHREADS
static pthread_t emul_thread;						// Handle of MacOS emulation thread (main thread)

static bool xpram_thread_active = false;			// Flag: XPRAM watchdog installed
static volatile bool xpram_thread_cancel = false;	// Flag: Cancel XPRAM thread
static pthread_t xpram_thread;						// XPRAM watchdog

static bool tick_thread_active = false;				// Flag: 60Hz thread installed
static volatile bool tick_thread_cancel = false;	// Flag: Cancel 60Hz thread
static pthread_t tick_thread;						// 60Hz thread
static pthread_attr_t tick_thread_attr;				// 60Hz thread attributes

#if EMULATED_68K
static pthread_mutex_t intflag_lock = PTHREAD_MUTEX_INITIALIZER;	// Mutex to protect InterruptFlags
#endif
#endif

#if !EMULATED_68K
#define SIG_IRQ SIGUSR1
static struct sigaction sigirq_sa;	// Virtual 68k interrupt signal
static struct sigaction sigill_sa;	// Illegal instruction
static void *sig_stack = NULL;		// Stack for signal handlers
uint16 EmulatedSR;					// Emulated bits of SR (supervisor bit and interrupt mask)
#endif

#if USE_SCRATCHMEM_SUBTERFUGE
uint8 *ScratchMem = NULL;			// Scratch memory for Mac ROM writes
#endif

static struct sigaction timer_sa;	// sigaction used for timer

#if defined(HAVE_TIMER_CREATE) && defined(_POSIX_REALTIME_SIGNALS)
#define SIG_TIMER SIGRTMIN
static timer_t timer;				// 60Hz timer
#endif

#ifdef ENABLE_MON
static struct sigaction sigint_sa;	// sigaction for SIGINT handler
static void sigint_handler(...);
#endif

#if REAL_ADDRESSING
static bool lm_area_mapped = false;	// Flag: Low Memory area mmap()ped
static bool memory_mapped_from_zero = false; // Flag: Could allocate RAM area from 0
#endif

#if REAL_ADDRESSING || DIRECT_ADDRESSING
static uint32 mapped_ram_rom_size;		// Total size of mmap()ed RAM/ROM area
#endif

#ifdef USE_MAPPED_MEMORY
extern char *address_space, *good_address_map;
#endif


// Prototypes
static void *xpram_func(void *arg);
static void *tick_func(void *arg);
static void one_tick(...);
#if !EMULATED_68K
static void sigirq_handler(int sig, int code, struct sigcontext *scp);
static void sigill_handler(int sig, int code, struct sigcontext *scp);
extern "C" void EmulOpTrampoline(void);
#endif


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
 *  Main program
 */

int main(int argc, char **argv)
{
	char str[256];

	// Initialize variables
	RAMBaseHost = NULL;
	ROMBaseHost = NULL;
	srand(time(NULL));
	tzset();

	// Print some info
	printf(GetString(STR_ABOUT_TEXT1), VERSION_MAJOR, VERSION_MINOR);
	printf(" %s\n", GetString(STR_ABOUT_TEXT2));

	// Parse arguments
	for (int i=1; i<argc; i++) {
		if (strcmp(argv[i], "-display") == 0 && ++i < argc)
			x_display_name = argv[i];
		else if (strcmp(argv[i], "-break") == 0 && ++i < argc)
			ROMBreakpoint = strtol(argv[i], NULL, 0);
		else if (strcmp(argv[i], "-rominfo") == 0)
			PrintROMInfo = true;
	}

	// Open display
	x_display = XOpenDisplay(x_display_name);
	if (x_display == NULL) {
		char str[256];
		sprintf(str, GetString(STR_NO_XSERVER_ERR), XDisplayName(x_display_name));
		ErrorAlert(str);
		QuitEmulator();
	}

#if defined(ENABLE_XF86_DGA) && !defined(ENABLE_MON)
	// Fork out, so we can return from fullscreen mode when things get ugly
	XF86DGAForkApp(DefaultScreen(x_display));
#endif

#ifdef ENABLE_GTK
	// Init GTK
	gtk_set_locale();
	gtk_init(&argc, &argv);
#endif

	// Read preferences
	PrefsInit();

	// Init system routines
	SysInit();

	// Show preferences editor
	if (!PrefsFindBool("nogui"))
		if (!PrefsEditor())
			QuitEmulator();

	// Open /dev/zero
	zero_fd = open("/dev/zero", O_RDWR);
	if (zero_fd < 0) {
		sprintf(str, GetString(STR_NO_DEV_ZERO_ERR), strerror(errno));
		ErrorAlert(str);
		QuitEmulator();
	}

	// Read RAM size
	RAMSize = PrefsFindInt32("ramsize") & 0xfff00000;	// Round down to 1MB boundary
	if (RAMSize < 1024*1024) {
		WarningAlert(GetString(STR_SMALL_RAM_WARN));
		RAMSize = 1024*1024;
	}

#if REAL_ADDRESSING || DIRECT_ADDRESSING
	const uint32 page_size = getpagesize();
	const uint32 page_mask = page_size - 1;
	const uint32 aligned_ram_size = (RAMSize + page_mask) & ~page_mask;
	mapped_ram_rom_size = aligned_ram_size + 0x100000;
#endif

#if REAL_ADDRESSING
	// Try to allocate the complete address space from zero
	// gb-- the Solaris manpage about mmap(2) states that using MAP_FIXED
	// implies undefined behaviour for further use of sbrk(), malloc(), etc.
	// cebix-- on NetBSD/m68k, this causes a segfault
#if defined(OS_solaris) || defined(OS_netbsd)
	// Anyway, it doesn't work...
	if (0) {
#else
	if (mmap((caddr_t)0x0000, mapped_ram_rom_size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE, zero_fd, 0) != MAP_FAILED) {
#endif
		D(bug("Could allocate RAM and ROM from 0x0000\n"));
		memory_mapped_from_zero = true;
	}
	// Create Low Memory area (0x0000..0x2000)
	else if (mmap((char *)0x0000, 0x2000, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE, zero_fd, 0) != MAP_FAILED) {
		D(bug("Could allocate the Low Memory globals\n"));
		lm_area_mapped = true;
	}
	// Exit on error
	else {
		sprintf(str, GetString(STR_LOW_MEM_MMAP_ERR), strerror(errno));
		ErrorAlert(str);
		QuitEmulator();
	}
#endif

#if USE_SCRATCHMEM_SUBTERFUGE
	// Allocate scratch memory
	ScratchMem = (uint8 *)malloc(SCRATCH_MEM_SIZE);
	if (ScratchMem == NULL) {
		ErrorAlert(GetString(STR_NO_MEM_ERR));
		QuitEmulator();
	}
	ScratchMem += SCRATCH_MEM_SIZE/2;	// ScratchMem points to middle of block
#endif

	// Create areas for Mac RAM and ROM
#if defined(USE_MAPPED_MEMORY)
    good_address_map = (char *)mmap(NULL, 1<<24, PROT_READ, MAP_PRIVATE, zero_fd, 0);
    address_space = (char *)mmap(NULL, 1<<24, PROT_READ | PROT_WRITE, MAP_PRIVATE, zero_fd, 0);
    if ((int)address_space < 0 || (int)good_address_map < 0) {
		ErrorAlert(GetString(STR_NOT_ENOUGH_MEMORY_ERR));
		QuitEmulator();
    }
    RAMBaseHost = (uint8 *)mmap(address_space, RAMSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, zero_fd, 0);
    ROMBaseHost = (uint8 *)mmap(address_space + 0x00400000, 0x80000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED, zero_fd, 0);
	char *nam = tmpnam(NULL);
    int good_address_fd = open(nam, O_CREAT | O_RDWR, 0600);
	char buffer[4096];
    memset(buffer, 1, sizeof(buffer));
    write(good_address_fd, buffer, sizeof(buffer));
    unlink(nam);
    for (int i=0; i<RAMSize; i+=4096)
        mmap(good_address_map + i, 4096, PROT_READ, MAP_FIXED | MAP_PRIVATE, good_address_fd, 0);
    for (int i=0; i<0x80000; i+=4096)
        mmap(good_address_map + i + 0x00400000, 4096, PROT_READ, MAP_FIXED | MAP_PRIVATE, good_address_fd, 0);
#elif REAL_ADDRESSING || DIRECT_ADDRESSING
	// gb-- Overkill, needs to be cleaned up. Probably explode it for either
	// real or direct addressing mode.
#if REAL_ADDRESSING
	if (memory_mapped_from_zero) {
		RAMBaseHost = (uint8 *)0;
		ROMBaseHost = RAMBaseHost + aligned_ram_size;
	}
	else
#endif
	{
		RAMBaseHost = (uint8 *)mmap(0, mapped_ram_rom_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, zero_fd, 0);
		if (RAMBaseHost == (uint8 *)MAP_FAILED) {
			ErrorAlert(GetString(STR_NO_MEM_ERR));
			QuitEmulator();
		}
		ROMBaseHost = RAMBaseHost + aligned_ram_size;
	}
#else
	RAMBaseHost = (uint8 *)malloc(RAMSize);
	ROMBaseHost = (uint8 *)malloc(0x100000);
	if (RAMBaseHost == NULL || ROMBaseHost == NULL) {
		ErrorAlert(GetString(STR_NO_MEM_ERR));
		QuitEmulator();
	}
#endif

#if DIRECT_ADDRESSING
	// Initialize MEMBaseDiff now so that Host2MacAddr in the Video module
	// will return correct results
	RAMBaseMac = 0;
	ROMBaseMac = RAMBaseMac + aligned_ram_size;
	InitMEMBaseDiff(RAMBaseHost, RAMBaseMac);
#endif
#if REAL_ADDRESSING // && !EMULATED_68K
	RAMBaseMac = (uint32)RAMBaseHost;
	ROMBaseMac = (uint32)ROMBaseHost;
#endif
	D(bug("Mac RAM starts at %p (%08x)\n", RAMBaseHost, RAMBaseMac));
	D(bug("Mac ROM starts at %p (%08x)\n", ROMBaseHost, ROMBaseMac));
	
	// Get rom file path from preferences
	const char *rom_path = PrefsFindString("rom");

	// Load Mac ROM
	int rom_fd = open(rom_path ? rom_path : ROM_FILE_NAME, O_RDONLY);
	if (rom_fd < 0) {
		ErrorAlert(GetString(STR_NO_ROM_FILE_ERR));
		QuitEmulator();
	}
	printf(GetString(STR_READING_ROM_FILE));
	ROMSize = lseek(rom_fd, 0, SEEK_END);
	if (ROMSize != 64*1024 && ROMSize != 128*1024 && ROMSize != 256*1024 && ROMSize != 512*1024 && ROMSize != 1024*1024) {
		ErrorAlert(GetString(STR_ROM_SIZE_ERR));
		close(rom_fd);
		QuitEmulator();
	}
	lseek(rom_fd, 0, SEEK_SET);
	if (read(rom_fd, ROMBaseHost, ROMSize) != (ssize_t)ROMSize) {
		ErrorAlert(GetString(STR_ROM_FILE_READ_ERR));
		close(rom_fd);
		QuitEmulator();
	}

#if !EMULATED_68K
	// Get CPU model
	int mib[2] = {CTL_HW, HW_MODEL};
	char *model;
	size_t model_len;
	sysctl(mib, 2, NULL, &model_len, NULL, 0);
	model = (char *)malloc(model_len);
	sysctl(mib, 2, model, &model_len, NULL, 0);
	D(bug("Model: %s\n", model));

	// Set CPU and FPU type
	CPUIs68060 = false;
	if (strstr(model, "020"))
		CPUType = 2;
	else if (strstr(model, "030"))
		CPUType = 3;
	else if (strstr(model, "040"))
		CPUType = 4;
	else if (strstr(model, "060")) {
		CPUType = 4;
		CPUIs68060 = true;
	} else {
		printf("WARNING: Cannot detect CPU type, assuming 68020\n");
		CPUType = 2;
	}
	FPUType = 1;	// NetBSD has an FPU emulation, so the FPU ought to be available at all times
	TwentyFourBitAddressing = false;
#endif

	// Initialize everything
	if (!InitAll())
		QuitEmulator();
	D(bug("Initialization complete\n"));

#ifdef HAVE_PTHREADS
	// Get handle of main thread
	emul_thread = pthread_self();
#endif

#if !EMULATED_68K
	// (Virtual) supervisor mode, disable interrupts
	EmulatedSR = 0x2700;

	// Create and install stack for signal handlers
	sig_stack = malloc(SIG_STACK_SIZE);
	D(bug("Signal stack at %p\n", sig_stack));
	if (sig_stack == NULL) {
		ErrorAlert(GetString(STR_NOT_ENOUGH_MEMORY_ERR));
		QuitEmulator();
	}
	stack_t new_stack;
	new_stack.ss_sp = sig_stack;
	new_stack.ss_flags = 0;
	new_stack.ss_size = SIG_STACK_SIZE;
	if (sigaltstack(&new_stack, NULL) < 0) {
		sprintf(str, GetString(STR_SIGALTSTACK_ERR), strerror(errno));
		ErrorAlert(str);
		QuitEmulator();
	}

	// Install SIGILL handler for emulating privileged instructions and
	// executing A-Trap and EMUL_OP opcodes
	sigemptyset(&sigill_sa.sa_mask);	// Block virtual 68k interrupts during SIGILL handling
	sigaddset(&sigill_sa.sa_mask, SIG_IRQ);
	sigaddset(&sigill_sa.sa_mask, SIGALRM);
	sigill_sa.sa_handler = (void (*)(int))sigill_handler;
	sigill_sa.sa_flags = SA_ONSTACK;
	if (sigaction(SIGILL, &sigill_sa, NULL) < 0) {
		sprintf(str, GetString(STR_SIG_INSTALL_ERR), "SIGILL", strerror(errno));
		ErrorAlert(str);
		QuitEmulator();
	}

	// Install virtual 68k interrupt signal handler
	sigemptyset(&sigirq_sa.sa_mask);
	sigaddset(&sigirq_sa.sa_mask, SIGALRM);
	sigirq_sa.sa_handler = (void (*)(int))sigirq_handler;
	sigirq_sa.sa_flags = SA_ONSTACK | SA_RESTART;
	if (sigaction(SIG_IRQ, &sigirq_sa, NULL) < 0) {
		sprintf(str, GetString(STR_SIG_INSTALL_ERR), "SIG_IRQ", strerror(errno));
		ErrorAlert(str);
		QuitEmulator();
	}
#endif

#ifdef ENABLE_MON
	// Setup SIGINT handler to enter mon
	sigemptyset(&sigint_sa.sa_mask);
	sigint_sa.sa_handler = (void (*)(int))sigint_handler;
	sigint_sa.sa_flags = 0;
	sigaction(SIGINT, &sigint_sa, NULL);
#endif

#if defined(HAVE_TIMER_CREATE) && defined(_POSIX_REALTIME_SIGNALS)

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

#elif defined(HAVE_PTHREADS)

	// POSIX threads available, start 60Hz thread
	pthread_attr_init(&tick_thread_attr);
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING)
	if (geteuid() == 0) {
		pthread_attr_setinheritsched(&tick_thread_attr, PTHREAD_EXPLICIT_SCHED);
		pthread_attr_setschedpolicy(&tick_thread_attr, SCHED_FIFO);
		struct sched_param fifo_param;
		fifo_param.sched_priority = (sched_get_priority_min(SCHED_FIFO) + sched_get_priority_max(SCHED_FIFO)) / 2;
		pthread_attr_setschedparam(&tick_thread_attr, &fifo_param);
	}
#endif
	tick_thread_active = (pthread_create(&tick_thread, &tick_thread_attr, tick_func, NULL) == 0);
	if (!tick_thread_active) {
		sprintf(str, GetString(STR_TICK_THREAD_ERR), strerror(errno));
		ErrorAlert(str);
		QuitEmulator();
	}
	D(bug("60Hz thread started\n"));

#else

	// Start 60Hz timer
	sigemptyset(&timer_sa.sa_mask);		// Block virtual 68k interrupts during SIGARLM handling
	sigaddset(&timer_sa.sa_mask, SIG_IRQ);
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

#ifdef HAVE_PTHREADS
	// Start XPRAM watchdog thread
	memcpy(last_xpram, XPRAM, 256);
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

#if EMULATED_68K
	// Exit 680x0 emulation
	Exit680x0();
#endif

#if defined(HAVE_TIMER_CREATE) && defined(_POSIX_REALTIME_SIGNALS)
	// Stop 60Hz timer
	timer_delete(timer);
#elif defined(HAVE_PTHREADS)
	// Stop 60Hz thread
	if (tick_thread_active) {
		tick_thread_cancel = true;
#ifdef HAVE_PTHREAD_CANCEL
		pthread_cancel(tick_thread);
#endif
		pthread_join(tick_thread, NULL);
	}
#else
	struct itimerval req;
	req.it_interval.tv_sec = req.it_value.tv_sec = 0;
	req.it_interval.tv_usec = req.it_value.tv_usec = 0;
	setitimer(ITIMER_REAL, &req, NULL);
#endif

#ifdef HAVE_PTHREADS
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
#if REAL_ADDRESSING
	if (memory_mapped_from_zero)
		munmap((caddr_t)0x0000, mapped_ram_rom_size);
	else
#endif
#if REAL_ADDRESSING || DIRECT_ADDRESSING
	if (RAMBaseHost != (uint8 *)MAP_FAILED) {
		munmap((caddr_t)RAMBaseHost, mapped_ram_rom_size);
		RAMBaseHost = NULL;
	}
#else
	if (ROMBaseHost) {
		free(ROMBaseHost);
		ROMBaseHost = NULL;
	}
	if (RAMBaseHost) {
		free(RAMBaseHost);
		RAMBaseHost = NULL;
	}
#endif

#if USE_SCRATCHMEM_SUBTERFUGE
	// Delete scratch memory area
	if (ScratchMem) {
		free((void *)(ScratchMem - SCRATCH_MEM_SIZE/2));
		ScratchMem = NULL;
	}
#endif

#if REAL_ADDRESSING
	// Delete Low Memory area
	if (lm_area_mapped)
		munmap((char *)0x0000, 0x2000);
#endif

	// Close /dev/zero
	if (zero_fd > 0)
		close(zero_fd);

	// Exit system routines
	SysExit();

	// Exit preferences
	PrefsExit();

	// Close X11 server connection
	if (x_display)
		XCloseDisplay(x_display);

	exit(0);
}


/*
 *  Code was patched, flush caches if neccessary (i.e. when using a real 680x0
 *  or a dynamically recompiling emulator)
 */

void FlushCodeCache(void *start, uint32 size)
{
#if !EMULATED_68K && defined(__NetBSD__)
	m68k_sync_icache(start, size);
#endif
}


/*
 *  SIGINT handler, enters mon
 */

#ifdef ENABLE_MON
static void sigint_handler(...)
{
#if EMULATED_68K
	uaecptr nextpc;
	extern void m68k_dumpstate(uaecptr *nextpc);
	m68k_dumpstate(&nextpc);
#else
	char *arg[4] = {"mon", "-m", "-r", NULL};
	mon(3, arg);
	QuitEmulator();
#endif
}
#endif


/*
 *  Interrupt flags (must be handled atomically!)
 */

uint32 InterruptFlags = 0;

#if EMULATED_68K
void SetInterruptFlag(uint32 flag)
{
#ifdef HAVE_PTHREADS
	pthread_mutex_lock(&intflag_lock);
	InterruptFlags |= flag;
	pthread_mutex_unlock(&intflag_lock);
#else
	InterruptFlags |= flag;		// Pray that this is an atomic operation...
#endif
}

void ClearInterruptFlag(uint32 flag)
{
#ifdef HAVE_PTHREADS
	pthread_mutex_lock(&intflag_lock);
	InterruptFlags &= ~flag;
	pthread_mutex_unlock(&intflag_lock);
#else
	InterruptFlags &= ~flag;
#endif
}
#endif

#if !EMULATED_68K
void TriggerInterrupt(void)
{
#if defined(HAVE_PTHREADS)
	pthread_kill(emul_thread, SIG_IRQ);
#else
	raise(SIG_IRQ);
#endif
}

void TriggerNMI(void)
{
	// not yet supported
}
#endif


/*
 *  XPRAM watchdog thread (saves XPRAM every minute)
 */

static void xpram_watchdog(void)
{
	if (memcmp(last_xpram, XPRAM, 256)) {
		memcpy(last_xpram, XPRAM, 256);
		SaveXPRAM();
	}
}

#ifdef HAVE_PTHREADS
static void *xpram_func(void *arg)
{
	while (!xpram_thread_cancel) {
		for (int i=0; i<60 && !xpram_thread_cancel; i++)
			Delay_usec(999999);
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

#ifndef HAVE_PTHREADS
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

#ifndef HAVE_PTHREADS
	// No threads available, perform video refresh from here
	VideoRefresh();
#endif

	// Trigger 60Hz interrupt
	if (ROMVersion != ROM_VERSION_CLASSIC || HasMacStarted()) {
		SetInterruptFlag(INTFLAG_60HZ);
		TriggerInterrupt();
	}
}

#ifdef HAVE_PTHREADS
static void *tick_func(void *arg)
{
	uint64 next = GetTicks_usec();
	while (!tick_thread_cancel) {
		one_tick();
		next += 16625;
		int64 delay = next - GetTicks_usec();
		if (delay > 0)
			Delay_usec(delay);
		else if (delay < -16625)
			next = GetTicks_usec();
	}
	return NULL;
}
#endif


/*
 *  Get current value of microsecond timer
 */

uint64 GetTicks_usec(void)
{
#ifdef HAVE_CLOCK_GETTIME
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	return (uint64)t.tv_sec * 1000000 + t.tv_nsec / 1000;
#else
	struct timeval t;
	gettimeofday(&t, NULL);
	return (uint64)t.tv_sec * 1000000 + t.tv_usec;
#endif
}


/*
 *  Delay by specified number of microseconds (<1 second)
 *  (adapted from SDL_Delay() source; this function is designed to provide
 *  the highest accuracy possible)
 */

void Delay_usec(uint32 usec)
{
	int was_error;

#if defined(linux)
	struct timeval tv;
#elif defined(__FreeBSD__) || defined(sgi)
	struct timespec elapsed, tv;
#else	// Non-Linux implementations need to calculate time left
	uint64 then, now, elapsed;
#endif

	// Set the timeout interval - Linux only needs to do this once
#if defined(linux)
	tv.tv_sec = 0;
	tv.tv_usec = usec;
#elif defined(__FreeBSD__) || defined(sgi)
	elapsed.tv_sec = 0;
	elapsed.tv_nsec = usec * 1000;
#else
	then = GetTicks_usec();
#endif
	do {
		errno = 0;
#if !defined(linux) && !defined(__FreeBSD__) && !defined(sgi)
		/* Calculate the time interval left (in case of interrupt) */
		now = GetTicks_usec();
		elapsed = now - then;
		then = now;
		if (elapsed >= usec)
			break;
		usec -= elapsed;
		tv.tv_sec = 0;
		tv.tv_usec = usec;
#endif
#if defined(__FreeBSD__) || defined(sgi)
		tv.tv_sec = elapsed.tv_sec;
		tv.tv_nsec = elapsed.tv_nsec;
		was_error = nanosleep(&tv, &elapsed);
#else
		was_error = select(0, NULL, NULL, NULL, &tv);
#endif
	} while (was_error && (errno == EINTR));
}


#if !EMULATED_68K
/*
 *  Virtual 68k interrupt handler
 */

static void sigirq_handler(int sig, int code, struct sigcontext *scp)
{
	// Interrupts disabled? Then do nothing
	if (EmulatedSR & 0x0700)
		return;

	struct sigstate *state = (struct sigstate *)scp->sc_ap;
	M68kRegisters *regs = (M68kRegisters *)&state->ss_frame;

	// Set up interrupt frame on stack
	uint32 a7 = regs->a[7];
	a7 -= 2;
	WriteMacInt16(a7, 0x64);
	a7 -= 4;
	WriteMacInt32(a7, scp->sc_pc);
	a7 -= 2;
	WriteMacInt16(a7, scp->sc_ps | EmulatedSR);
	scp->sc_sp = regs->a[7] = a7;

	// Set interrupt level
	EmulatedSR |= 0x2100;

	// Jump to MacOS interrupt handler on return
	scp->sc_pc = ReadMacInt32(0x64);
}


/*
 *  SIGILL handler, for emulation of privileged instructions and executing
 *  A-Trap and EMUL_OP opcodes
 */

static void sigill_handler(int sig, int code, struct sigcontext *scp)
{
	struct sigstate *state = (struct sigstate *)scp->sc_ap;
	uint16 *pc = (uint16 *)scp->sc_pc;
	uint16 opcode = *pc;
	M68kRegisters *regs = (M68kRegisters *)&state->ss_frame;

#define INC_PC(n) scp->sc_pc += (n)

#define GET_SR (scp->sc_ps | EmulatedSR)

#define STORE_SR(v) \
	scp->sc_ps = (v) & 0xff; \
	EmulatedSR = (v) & 0xe700; \
	if (((v) & 0x0700) == 0 && InterruptFlags) \
		TriggerInterrupt();

//printf("opcode %04x at %p, sr %04x, emul_sr %04x\n", opcode, pc, scp->sc_ps, EmulatedSR);

	if ((opcode & 0xf000) == 0xa000) {

		// A-Line instruction, set up A-Line trap frame on stack
		uint32 a7 = regs->a[7];
		a7 -= 2;
		WriteMacInt16(a7, 0x28);
		a7 -= 4;
		WriteMacInt32(a7, (uint32)pc);
		a7 -= 2;
		WriteMacInt16(a7, GET_SR);
		scp->sc_sp = regs->a[7] = a7;

		// Jump to MacOS A-Line handler on return
		scp->sc_pc = ReadMacInt32(0x28);

	} else if ((opcode & 0xff00) == 0x7100) {

		// Extended opcode, push registers on user stack
		uint32 a7 = regs->a[7];
		a7 -= 4;
		WriteMacInt32(a7, (uint32)pc);
		a7 -= 2;
		WriteMacInt16(a7, scp->sc_ps);
		for (int i=7; i>=0; i--) {
			a7 -= 4;
			WriteMacInt32(a7, regs->a[i]);
		}
		for (int i=7; i>=0; i--) {
			a7 -= 4;
			WriteMacInt32(a7, regs->d[i]);
		}
		scp->sc_sp = regs->a[7] = a7;

		// Jump to EmulOp trampoline code on return
		scp->sc_pc = (uint32)EmulOpTrampoline;
		
	} else switch (opcode) {	// Emulate privileged instructions

		case 0x40e7:	// move sr,-(sp)
			regs->a[7] -= 2;
			WriteMacInt16(regs->a[7], GET_SR);
			scp->sc_sp = regs->a[7];
			INC_PC(2);
			break;

		case 0x46df: {	// move (sp)+,sr
			uint16 sr = ReadMacInt16(regs->a[7]);
			STORE_SR(sr);
			regs->a[7] += 2;
			scp->sc_sp = regs->a[7];
			INC_PC(2);
			break;
		}

		case 0x007c: {	// ori #xxxx,sr
			uint16 sr = GET_SR | pc[1];
			scp->sc_ps = sr & 0xff;		// oring bits into the sr can't enable interrupts, so we don't need to call STORE_SR
			EmulatedSR = sr & 0xe700;
			INC_PC(4);
			break;
		}

		case 0x027c: {	// andi #xxxx,sr
			uint16 sr = GET_SR & pc[1];
			STORE_SR(sr);
			INC_PC(4);
			break;
		}

		case 0x46fc:	// move #xxxx,sr
			STORE_SR(pc[1]);
			INC_PC(4);
			break;

		case 0x46ef: {	// move (xxxx,sp),sr
			uint16 sr = ReadMacInt16(regs->a[7] + (int32)(int16)pc[1]);
			STORE_SR(sr);
			INC_PC(4);
			break;
		}

		case 0x46d8:	// move (a0)+,sr
		case 0x46d9: {	// move (a1)+,sr
			uint16 sr = ReadMacInt16(regs->a[opcode & 7]);
			STORE_SR(sr);
			regs->a[opcode & 7] += 2;
			INC_PC(2);
			break;
		}

		case 0x40f8:	// move sr,xxxx.w
			WriteMacInt16(pc[1], GET_SR);
			INC_PC(4);
			break;

		case 0x40d0:	// move sr,(a0)
		case 0x40d1:	// move sr,(a1)
		case 0x40d2:	// move sr,(a2)
		case 0x40d3:	// move sr,(a3)
		case 0x40d4:	// move sr,(a4)
		case 0x40d5:	// move sr,(a5)
		case 0x40d6:	// move sr,(a6)
		case 0x40d7:	// move sr,(sp)
			WriteMacInt16(regs->a[opcode & 7], GET_SR);
			INC_PC(2);
			break;

		case 0x40c0:	// move sr,d0
		case 0x40c1:	// move sr,d1
		case 0x40c2:	// move sr,d2
		case 0x40c3:	// move sr,d3
		case 0x40c4:	// move sr,d4
		case 0x40c5:	// move sr,d5
		case 0x40c6:	// move sr,d6
		case 0x40c7:	// move sr,d7
			regs->d[opcode & 7] = GET_SR;
			INC_PC(2);
			break;

		case 0x46c0:	// move d0,sr
		case 0x46c1:	// move d1,sr
		case 0x46c2:	// move d2,sr
		case 0x46c3:	// move d3,sr
		case 0x46c4:	// move d4,sr
		case 0x46c5:	// move d5,sr
		case 0x46c6:	// move d6,sr
		case 0x46c7: {	// move d7,sr
			uint16 sr = regs->d[opcode & 7];
			STORE_SR(sr);
			INC_PC(2);
			break;
		}

		case 0xf327:	// fsave -(sp)
			if (CPUIs68060) {
				regs->a[7] -= 4;
				WriteMacInt32(regs->a[7], 0x60000000);	// Idle frame
				regs->a[7] -= 4;
				WriteMacInt32(regs->a[7], 0);
				regs->a[7] -= 4;
				WriteMacInt32(regs->a[7], 0);
			} else {
				regs->a[7] -= 4;
				WriteMacInt32(regs->a[7], 0x41000000);	// Idle frame
			}
			scp->sc_sp = regs->a[7];
			INC_PC(2);
			break;

		case 0xf35f:	// frestore (sp)+
			if (CPUIs68060)
				regs->a[7] += 12;
			else
				regs->a[7] += 4;
			scp->sc_sp = regs->a[7];
			INC_PC(2);
			break;

		case 0x4e73: {	// rte
			uint32 a7 = regs->a[7];
			uint16 sr = ReadMacInt16(a7);
			a7 += 2;
			scp->sc_ps = sr & 0xff;
			EmulatedSR = sr & 0xe700;
			scp->sc_pc = ReadMacInt32(a7);
			a7 += 4;
			uint16 format = ReadMacInt16(a7) >> 12;
			a7 += 2;
			static const int frame_adj[16] = {
				0, 0, 4, 4, 8, 0, 0, 52, 50, 12, 24, 84, 16, 0, 0, 0
			};
			scp->sc_sp = regs->a[7] = a7 + frame_adj[format];
			break;
		}

		case 0x4e7a:	// movec cr,x
			switch (pc[1]) {
				case 0x0002:	// movec cacr,d0
					regs->d[0] = 0x3111;
					break;
				case 0x1002:	// movec cacr,d1
					regs->d[1] = 0x3111;
					break;
				case 0x0003:	// movec tc,d0
				case 0x0004:	// movec itt0,d0
				case 0x0005:	// movec itt1,d0
				case 0x0006:	// movec dtt0,d0
				case 0x0007:	// movec dtt1,d0
				case 0x0806:	// movec urp,d0
				case 0x0807:	// movec srp,d0
					regs->d[0] = 0;
					break;
				case 0x1000:	// movec sfc,d1
				case 0x1001:	// movec dfc,d1
				case 0x1003:	// movec tc,d1
				case 0x1801:	// movec vbr,d1
					regs->d[1] = 0;
					break;
				case 0x8801:	// movec vbr,a0
					regs->a[0] = 0;
					break;
				case 0x9801:	// movec vbr,a1
					regs->a[1] = 0;
					break;
				default:
					goto ill;
			}
			INC_PC(4);
			break;

		case 0x4e7b:	// movec x,cr
			switch (pc[1]) {
				case 0x1000:	// movec d1,sfc
				case 0x1001:	// movec d1,dfc
				case 0x0801:	// movec d0,vbr
				case 0x1801:	// movec d1,vbr
					break;
				case 0x0002:	// movec d0,cacr
				case 0x1002:	// movec d1,cacr
					FlushCodeCache(NULL, 0);
					break;
				default:
					goto ill;
			}
			INC_PC(4);
			break;

		case 0xf478:	// cpusha dc
		case 0xf4f8:	// cpusha dc/ic
			FlushCodeCache(NULL, 0);
			INC_PC(2);
			break;

		default:
ill:		printf("SIGILL num %d, code %d\n", sig, code);
			printf(" context %p:\n", scp);
			printf("  onstack %08x\n", scp->sc_onstack);
			printf("  sp %08x\n", scp->sc_sp);
			printf("  fp %08x\n", scp->sc_fp);
			printf("  pc %08x\n", scp->sc_pc);
			printf("   opcode %04x\n", opcode);
			printf("  sr %08x\n", scp->sc_ps);
			printf(" state %p:\n", state);
			printf("  flags %d\n", state->ss_flags);
			for (int i=0; i<8; i++)
				printf("  d%d %08x\n", i, state->ss_frame.f_regs[i]);
			for (int i=0; i<8; i++)
				printf("  a%d %08x\n", i, state->ss_frame.f_regs[i+8]);

#ifdef ENABLE_MON
			char *arg[4] = {"mon", "-m", "-r", NULL};
			mon(3, arg);
#endif
			QuitEmulator();
			break;
	}
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
#ifdef ENABLE_GTK
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
#ifdef ENABLE_GTK
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
