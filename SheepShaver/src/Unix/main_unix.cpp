/*
 *  main_unix.cpp - Emulation core, Unix implementation
 *
 *  SheepShaver (C) Christian Bauer and Marc Hellwig
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
 *  See main_beos.cpp for a description of the three operating modes.
 *
 *  In addition to that, we have to handle the fact that the MacOS ABI
 *  is slightly different from the SysV ABI used by Linux:
 *    - Stack frames are different (e.g. LR is stored in 8(r1) under
 *      MacOS, but in 4(r1) under Linux)
 *    - There is a pointer to Thread Local Storage (TLS) under Linux with
 *      recent enough glibc. This is r2 in 32-bit mode and r13 in
 *      64-bit mode (PowerOpen/AIX ABI)
 *    - r13 is used as a small data pointer under Linux (but appearently
 *      it is not used this way? To be sure, we specify -msdata=none
 *      in the Makefile)
 *    - There are no TVECTs under Linux; function pointers point
 *      directly to the function code
 *  The Execute*() functions have to account for this. Additionally, we
 *  cannot simply call MacOS functions by getting their TVECT and jumping
 *  to it. Such calls are done via the call_macos*() functions in
 *  asm_linux.S that create a MacOS stack frame, load the TOC pointer
 *  and put the arguments into the right registers.
 *
 *  As on the BeOS, we have to specify an alternate signal stack because
 *  interrupts (and, under Linux, Low Memory accesses) may occur when r1
 *  is pointing to the Kernel Data or to Low Memory. There is one
 *  problem, however, due to the alternate signal stack being global to
 *  all signal handlers. Consider the following scenario:
 *    - The main thread is executing some native PPC MacOS code in
 *      MODE_NATIVE, running on the MacOS stack (somewhere in the Mac RAM).
 *    - A SIGUSR2 interrupt occurs. The kernel switches to the signal
 *      stack and starts executing the SIGUSR2 signal handler.
 *    - The signal handler sees the MODE_NATIVE and calls ppc_interrupt()
 *      to handle a native interrupt.
 *    - ppc_interrupt() sets r1 to point to the Kernel Data and jumps to
 *      the nanokernel.
 *    - The nanokernel accesses a Low Memory global (most likely one of
 *      the XLMs), a SIGSEGV occurs.
 *    - The kernel sees that r1 does not point to the signal stack and
 *      switches to the signal stack again, thus overwriting the data that
 *      the SIGUSR2 handler put there.
 *  The same problem arises when calling ExecutePPC() inside the MODE_EMUL_OP
 *  interrupt handler.
 *
 *  The solution is to set the signal stack to a second, "extra" stack
 *  inside the SIGUSR2 handler before entering the Nanokernel or calling
 *  ExecutePPC (or any function that might cause a mode switch). The signal
 *  stack is restored before exiting the SIGUSR2 handler.
 *
 *  Note that POSIX standard says you can't modify the alternate
 *  signal stack while the process is executing on it. There is a
 *  hackaround though: we install a trampoline SIGUSR2 handler that
 *  sets up an alternate stack itself and calls the real handler.
 *  Then, when we call sigaltstack() there, we no longer get an EPERM,
 *  i.e. it now works.
 *
 *  TODO:
 *    check if SIGSEGV handler works for all registers (including FP!)
 */

#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <signal.h>

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
#include "vm_alloc.h"
#include "sigsegv.h"
#include "sigregs.h"
#include "rpc.h"

#define DEBUG 0
#include "debug.h"


#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef USE_SDL
#include <SDL.h>
#endif

#ifndef USE_SDL_VIDEO
#include <X11/Xlib.h>
#endif

#ifdef ENABLE_GTK
#include <gtk/gtk.h>
#endif

#ifdef ENABLE_XF86_DGA
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xxf86dga.h>
#endif

#ifdef ENABLE_MON
#include "mon.h"
#endif


// Enable emulation of unaligned lmw/stmw?
#define EMULATE_UNALIGNED_LOADSTORE_MULTIPLE 1

// Enable Execute68k() safety checks?
#define SAFE_EXEC_68K 0

// Interrupts in EMUL_OP mode?
#define INTERRUPTS_IN_EMUL_OP_MODE 1

// Interrupts in native mode?
#define INTERRUPTS_IN_NATIVE_MODE 1


// Constants
const char ROM_FILE_NAME[] = "ROM";
const char ROM_FILE_NAME2[] = "Mac OS ROM";

#if !REAL_ADDRESSING
// FIXME: needs to be >= 0x04000000
const uintptr RAM_BASE = 0x10000000;		// Base address of RAM
#endif
const uintptr ROM_BASE = 0x40800000;		// Base address of ROM
#if REAL_ADDRESSING
const uint32 ROM_ALIGNMENT = 0x100000;		// ROM must be aligned to a 1MB boundary
#endif
const uint32 SIG_STACK_SIZE = 0x10000;		// Size of signal stack


// Global variables (exported)
#if !EMULATED_PPC
void *TOC = NULL;		// Pointer to Thread Local Storage (r2)
void *R13 = NULL;		// Pointer to .sdata section (r13 under Linux)
#endif
uint32 RAMBase;			// Base address of Mac RAM
uint32 RAMSize;			// Size of Mac RAM
uint32 ROMBase;			// Base address of Mac ROM
uint32 KernelDataAddr;	// Address of Kernel Data
uint32 BootGlobsAddr;	// Address of BootGlobs structure at top of Mac RAM
uint32 DRCacheAddr;		// Address of DR Cache
uint32 PVR;				// Theoretical PVR
int64 CPUClockSpeed;	// Processor clock speed (Hz)
int64 BusClockSpeed;	// Bus clock speed (Hz)
int64 TimebaseSpeed;	// Timebase clock speed (Hz)
uint8 *RAMBaseHost;		// Base address of Mac RAM (host address space)
uint8 *ROMBaseHost;		// Base address of Mac ROM (host address space)


// Global variables
#ifndef USE_SDL_VIDEO
char *x_display_name = NULL;				// X11 display name
Display *x_display = NULL;					// X11 display handle
#ifdef X11_LOCK_TYPE
X11_LOCK_TYPE x_display_lock = X11_LOCK_INIT; // X11 display lock
#endif
#endif

static int zero_fd = 0;						// FD of /dev/zero
static bool lm_area_mapped = false;			// Flag: Low Memory area mmap()ped
static bool rom_area_mapped = false;		// Flag: Mac ROM mmap()ped
static bool ram_area_mapped = false;		// Flag: Mac RAM mmap()ped
static bool dr_cache_area_mapped = false;	// Flag: Mac DR Cache mmap()ped
static bool dr_emulator_area_mapped = false;// Flag: Mac DR Emulator mmap()ped
static KernelData *kernel_data;				// Pointer to Kernel Data
static EmulatorData *emulator_data;

static uint8 last_xpram[XPRAM_SIZE];		// Buffer for monitoring XPRAM changes

static bool nvram_thread_active = false;	// Flag: NVRAM watchdog installed
static volatile bool nvram_thread_cancel;	// Flag: Cancel NVRAM thread
static pthread_t nvram_thread;				// NVRAM watchdog
static bool tick_thread_active = false;		// Flag: MacOS thread installed
static volatile bool tick_thread_cancel;	// Flag: Cancel 60Hz thread
static pthread_t tick_thread;				// 60Hz thread
static pthread_t emul_thread;				// MacOS thread

static bool ready_for_signals = false;		// Handler installed, signals can be sent

#if EMULATED_PPC
static uintptr sig_stack = 0;				// Stack for PowerPC interrupt routine
#else
static struct sigaction sigusr2_action;		// Interrupt signal (of emulator thread)
static struct sigaction sigsegv_action;		// Data access exception signal (of emulator thread)
static struct sigaction sigill_action;		// Illegal instruction signal (of emulator thread)
static stack_t sig_stack;					// Stack for signal handlers
static stack_t extra_stack;					// Stack for SIGSEGV inside interrupt handler
static bool emul_thread_fatal = false;		// Flag: MacOS thread crashed, tick thread shall dump debug output
static sigregs sigsegv_regs;				// Register dump when crashed
static const char *crash_reason = NULL;		// Reason of the crash (SIGSEGV, SIGBUS, SIGILL)
#endif

static rpc_connection_t *gui_connection = NULL;	// RPC connection to the GUI
static const char *gui_connection_path = NULL;	// GUI connection identifier

uint32  SheepMem::page_size;				// Size of a native page
uintptr SheepMem::zero_page = 0;			// Address of ro page filled in with zeros
uintptr SheepMem::base = 0x60000000;		// Address of SheepShaver data
uintptr SheepMem::proc;						// Bottom address of SheepShave procedures
uintptr SheepMem::data;						// Top of SheepShaver data (stack like storage)


// Prototypes
static bool kernel_data_init(void);
static bool shm_map_address(int kernel_area, uint32 addr);
static void Quit(void);
static void *emul_func(void *arg);
static void *nvram_func(void *arg);
static void *tick_func(void *arg);
#if EMULATED_PPC
extern void emul_ppc(uint32 start);
extern void init_emul_ppc(void);
extern void exit_emul_ppc(void);
sigsegv_return_t sigsegv_handler(sigsegv_info_t *sip);
#else
extern "C" void sigusr2_handler_init(int sig, siginfo_t *sip, void *scp);
extern "C" void sigusr2_handler(int sig, siginfo_t *sip, void *scp);
static void sigsegv_handler(int sig, siginfo_t *sip, void *scp);
static void sigill_handler(int sig, siginfo_t *sip, void *scp);
#endif


// From asm_linux.S
#if !EMULATED_PPC
extern "C" void *get_sp(void);
extern "C" void *get_r2(void);
extern "C" void set_r2(void *);
extern "C" void *get_r13(void);
extern "C" void set_r13(void *);
extern "C" void flush_icache_range(uint32 start, uint32 end);
extern "C" void jump_to_rom(uint32 entry, uint32 context);
extern "C" void quit_emulator(void);
extern "C" void execute_68k(uint32 pc, M68kRegisters *r);
extern "C" void ppc_interrupt(uint32 entry, uint32 kernel_data);
extern "C" int atomic_add(int *var, int v);
extern "C" int atomic_and(int *var, int v);
extern "C" int atomic_or(int *var, int v);
extern void paranoia_check(void);
#endif


#if EMULATED_PPC
/*
 *  Return signal stack base
 */

uintptr SignalStackBase(void)
{
	return sig_stack + SIG_STACK_SIZE;
}


/*
 *  Atomic operations
 */

#if HAVE_SPINLOCKS
static spinlock_t atomic_ops_lock = SPIN_LOCK_UNLOCKED;
#else
#define spin_lock(LOCK)
#define spin_unlock(LOCK)
#endif

int atomic_add(int *var, int v)
{
	spin_lock(&atomic_ops_lock);
	int ret = *var;
	*var += v;
	spin_unlock(&atomic_ops_lock);
	return ret;
}

int atomic_and(int *var, int v)
{
	spin_lock(&atomic_ops_lock);
	int ret = *var;
	*var &= v;
	spin_unlock(&atomic_ops_lock);
	return ret;
}

int atomic_or(int *var, int v)
{
	spin_lock(&atomic_ops_lock);
	int ret = *var;
	*var |= v;
	spin_unlock(&atomic_ops_lock);
	return ret;
}
#endif


/*
 *  Memory management helpers
 */

static inline uint8 *vm_mac_acquire(uint32 size)
{
	return (uint8 *)vm_acquire(size);
}

static inline int vm_mac_acquire_fixed(uint32 addr, uint32 size)
{
	return vm_acquire_fixed(Mac2HostAddr(addr), size);
}

static inline int vm_mac_release(uint32 addr, uint32 size)
{
	return vm_release(Mac2HostAddr(addr), size);
}


/*
 *  Main program
 */

static void usage(const char *prg_name)
{
	printf("Usage: %s [OPTION...]\n", prg_name);
	printf("\nUnix options:\n");
	printf("  --display STRING\n    X display to use\n");
	PrefsPrintUsage();
	exit(0);
}

static bool valid_vmdir(const char *path)
{
	const int suffix_len = sizeof(".sheepvm") - 1;
	int len = strlen(path);
	if (len && path[len - 1] == '/') // to support both ".sheepvm" and ".sheepvm/"
		len--;
	if (len > suffix_len && !strncmp(path + len - suffix_len, ".sheepvm", suffix_len)) {
		struct stat d;
		if (!stat(path, &d) && S_ISDIR(d.st_mode)) {
			return true;
		}
	}
	return false;
}

static void get_system_info(void)
{
#if !EMULATED_PPC
	FILE *proc_file;
#endif

	PVR = 0x00040000;			// Default: 604
	CPUClockSpeed = 100000000;	// Default: 100MHz
	BusClockSpeed = 100000000;	// Default: 100MHz
	TimebaseSpeed =  25000000;	// Default:  25MHz

#if EMULATED_PPC
	PVR = 0x000c0000;			// Default: 7400 (with AltiVec)
#elif defined(__APPLE__) && defined(__MACH__)
	proc_file = popen("ioreg -c IOPlatformDevice", "r");
	if (proc_file) {
		char line[256];
		bool powerpc_node = false;
		while (fgets(line, sizeof(line) - 1, proc_file)) {
			// Read line
			int len = strlen(line);
			if (len == 0)
				continue;
			line[len - 1] = 0;

			// Parse line
			if (strstr(line, "o PowerPC,"))
				powerpc_node = true;
			else if (powerpc_node) {
				uint32 value;
				char head[256];
				if (sscanf(line, "%[ |]\"cpu-version\" = <%x>", head, &value) == 2)
					PVR = value;
				else if (sscanf(line, "%[ |]\"clock-frequency\" = <%x>", head, &value) == 2)
					CPUClockSpeed = value;
				else if (sscanf(line, "%[ |]\"bus-frequency\" = <%x>", head, &value) == 2)
					BusClockSpeed = value;
				else if (sscanf(line, "%[ |]\"timebase-frequency\" = <%x>", head, &value) == 2)
					TimebaseSpeed = value;
				else if (strchr(line, '}'))
					powerpc_node = false;
			}
		}
		fclose(proc_file);
	} else {
		char str[256];
		sprintf(str, GetString(STR_PROC_CPUINFO_WARN), strerror(errno));
		WarningAlert(str);
	}
#else
	proc_file = fopen("/proc/cpuinfo", "r");
	if (proc_file) {
		// CPU specs from Linux kernel
		// TODO: make it more generic with features (e.g. AltiVec) and
		// cache information and friends for NameRegistry
		static const struct {
			uint32 pvr_mask;
			uint32 pvr_value;
			const char *cpu_name;
		}
		cpu_specs[] = {
			{ 0xffff0000, 0x00010000, "601" },
			{ 0xffff0000, 0x00030000, "603" },
			{ 0xffff0000, 0x00060000, "603e" },
			{ 0xffff0000, 0x00070000, "603ev" },
			{ 0xffff0000, 0x00040000, "604" },
			{ 0xfffff000, 0x00090000, "604e" },
			{ 0xffff0000, 0x00090000, "604r" },
			{ 0xffff0000, 0x000a0000, "604ev" },
			{ 0xffffffff, 0x00084202, "740/750" },
			{ 0xfffff000, 0x00083000, "745/755" },
			{ 0xfffffff0, 0x00080100, "750CX" },
			{ 0xfffffff0, 0x00082200, "750CX" },
			{ 0xfffffff0, 0x00082210, "750CXe" },
			{ 0xffffff00, 0x70000100, "750FX" },
			{ 0xffffffff, 0x70000200, "750FX" },
			{ 0xffff0000, 0x70000000, "750FX" },
			{ 0xffff0000, 0x70020000, "750GX" },
			{ 0xffff0000, 0x00080000, "740/750" },
			{ 0xffffffff, 0x000c1101, "7400 (1.1)" },
			{ 0xffff0000, 0x000c0000, "7400" },
			{ 0xffff0000, 0x800c0000, "7410" },
			{ 0xffffffff, 0x80000200, "7450" },
			{ 0xffffffff, 0x80000201, "7450" },
			{ 0xffff0000, 0x80000000, "7450" },
			{ 0xffffff00, 0x80010100, "7455" },
			{ 0xffffffff, 0x80010200, "7455" },
			{ 0xffff0000, 0x80010000, "7455" },
			{ 0xffff0000, 0x80020000, "7457" },
			{ 0xffff0000, 0x80030000, "7447A" },
			{ 0xffff0000, 0x80040000, "7448" },
			{ 0x7fff0000, 0x00810000, "82xx" },
			{ 0x7fff0000, 0x00820000, "8280" },
			{ 0xffff0000, 0x00400000, "Power3 (630)" },
			{ 0xffff0000, 0x00410000, "Power3 (630+)" },
			{ 0xffff0000, 0x00360000, "I-star" },
			{ 0xffff0000, 0x00370000, "S-star" },
			{ 0xffff0000, 0x00350000, "Power4" },
			{ 0xffff0000, 0x00390000, "PPC970" },
			{ 0xffff0000, 0x003c0000, "PPC970FX" },
			{ 0xffff0000, 0x00440000, "PPC970MP" },
			{ 0xffff0000, 0x003a0000, "POWER5 (gr)" },
			{ 0xffff0000, 0x003b0000, "POWER5+ (gs)" },
			{ 0xffff0000, 0x003e0000, "POWER6" },
			{ 0xffff0000, 0x00700000, "Cell Broadband Engine" },
			{ 0x7fff0000, 0x00900000, "PA6T" },
			{ 0, 0, 0 }
		};

		char line[256];
		while(fgets(line, 255, proc_file)) {
			// Read line
			int len = strlen(line);
			if (len == 0)
				continue;
			line[len-1] = 0;

			// Parse line
			int i;
			float f;
			char value[256];
			if (sscanf(line, "cpu : %[^,]", value) == 1) {
				// Search by name
				const char *cpu_name = NULL;
				for (int i = 0; cpu_specs[i].pvr_mask != 0; i++) {
					if (strcmp(cpu_specs[i].cpu_name, value) == 0) {
						cpu_name = cpu_specs[i].cpu_name;
						PVR = cpu_specs[i].pvr_value;
						break;
					}
				}
				if (cpu_name == NULL)
					printf("WARNING: Unknown CPU type '%s', assuming 604\n", value);
				else
					printf("Found a PowerPC %s processor\n", cpu_name);
			}
			if (sscanf(line, "clock : %fMHz", &f) == 1)
				CPUClockSpeed = BusClockSpeed = ((int64)f) * 1000000;
			else if (sscanf(line, "clock : %dMHz", &i) == 1)
				CPUClockSpeed = BusClockSpeed = i * 1000000;
		}
		fclose(proc_file);
	} else {
		char str[256];
		sprintf(str, GetString(STR_PROC_CPUINFO_WARN), strerror(errno));
		WarningAlert(str);
	}

	// Get actual bus frequency
	proc_file = fopen("/proc/device-tree/clock-frequency", "r");
	if (proc_file) {
		union { uint8 b[4]; uint32 l; } value;
		if (fread(value.b, sizeof(value), 1, proc_file) == 1)
			BusClockSpeed = value.l;
		fclose(proc_file);
	}

	// Get actual timebase frequency
	TimebaseSpeed = BusClockSpeed / 4;
	DIR *cpus_dir;
	if ((cpus_dir = opendir("/proc/device-tree/cpus")) != NULL) {
		struct dirent *cpu_entry;
		while ((cpu_entry = readdir(cpus_dir)) != NULL) {
			if (strstr(cpu_entry->d_name, "PowerPC,") == cpu_entry->d_name) {
				char timebase_freq_node[256];
				sprintf(timebase_freq_node, "/proc/device-tree/cpus/%s/timebase-frequency", cpu_entry->d_name);
				proc_file = fopen(timebase_freq_node, "r");
				if (proc_file) {
					union { uint8 b[4]; uint32 l; } value;
					if (fread(value.b, sizeof(value), 1, proc_file) == 1)
						TimebaseSpeed = value.l;
					fclose(proc_file);
				}
			}
		}
		closedir(cpus_dir);
	}
#endif

	// Remap any newer G4/G5 processor to plain G4 for compatibility
	switch (PVR >> 16) {
	case 0x8000:				// 7450
	case 0x8001:				// 7455
	case 0x8002:				// 7457
	case 0x8003:				// 7447A
	case 0x8004:				// 7448
	case 0x0039:				//  970
	case 0x003c:				//  970FX
	case 0x0044:				//  970MP
		PVR = 0x000c0000;		// 7400
		break;
	}
	D(bug("PVR: %08x (assumed)\n", PVR));
}

static bool load_mac_rom(void)
{
	uint32 rom_size, actual;
	uint8 *rom_tmp;
	const char *rom_path = PrefsFindString("rom");
	int rom_fd = open(rom_path && *rom_path ? rom_path : ROM_FILE_NAME, O_RDONLY);
	if (rom_fd < 0) {
		rom_fd = open(ROM_FILE_NAME2, O_RDONLY);
		if (rom_fd < 0) {
			ErrorAlert(GetString(STR_NO_ROM_FILE_ERR));
			return false;
		}
	}
	printf("%s", GetString(STR_READING_ROM_FILE));
	rom_size = lseek(rom_fd, 0, SEEK_END);
	lseek(rom_fd, 0, SEEK_SET);
	rom_tmp = new uint8[ROM_SIZE];
	actual = read(rom_fd, (void *)rom_tmp, ROM_SIZE);
	close(rom_fd);
	
	// Decode Mac ROM
	if (!DecodeROM(rom_tmp, actual)) {
		if (rom_size != 4*1024*1024) {
			ErrorAlert(GetString(STR_ROM_SIZE_ERR));
			return false;
		} else {
			ErrorAlert(GetString(STR_ROM_FILE_READ_ERR));
			return false;
		}
	}
	delete[] rom_tmp;
	return true;
}

static bool install_signal_handlers(void)
{
	char str[256];
#if !EMULATED_PPC
	// Create and install stacks for signal handlers
	sig_stack.ss_sp = malloc(SIG_STACK_SIZE);
	D(bug("Signal stack at %p\n", sig_stack.ss_sp));
	if (sig_stack.ss_sp == NULL) {
		ErrorAlert(GetString(STR_NOT_ENOUGH_MEMORY_ERR));
		return false;
	}
	sig_stack.ss_flags = 0;
	sig_stack.ss_size = SIG_STACK_SIZE;
	if (sigaltstack(&sig_stack, NULL) < 0) {
		sprintf(str, GetString(STR_SIGALTSTACK_ERR), strerror(errno));
		ErrorAlert(str);
		return false;
	}
	extra_stack.ss_sp = malloc(SIG_STACK_SIZE);
	D(bug("Extra stack at %p\n", extra_stack.ss_sp));
	if (extra_stack.ss_sp == NULL) {
		ErrorAlert(GetString(STR_NOT_ENOUGH_MEMORY_ERR));
		return false;
	}
	extra_stack.ss_flags = 0;
	extra_stack.ss_size = SIG_STACK_SIZE;

	// Install SIGSEGV and SIGBUS handlers
	sigemptyset(&sigsegv_action.sa_mask);	// Block interrupts during SEGV handling
	sigaddset(&sigsegv_action.sa_mask, SIGUSR2);
	sigsegv_action.sa_sigaction = sigsegv_handler;
	sigsegv_action.sa_flags = SA_ONSTACK | SA_SIGINFO;
#ifdef HAVE_SIGNAL_SA_RESTORER
	sigsegv_action.sa_restorer = NULL;
#endif
	if (sigaction(SIGSEGV, &sigsegv_action, NULL) < 0) {
		sprintf(str, GetString(STR_SIG_INSTALL_ERR), "SIGSEGV", strerror(errno));
		ErrorAlert(str);
		return false;
	}
	if (sigaction(SIGBUS, &sigsegv_action, NULL) < 0) {
		sprintf(str, GetString(STR_SIG_INSTALL_ERR), "SIGBUS", strerror(errno));
		ErrorAlert(str);
		return false;
	}
#else
	// Install SIGSEGV handler for CPU emulator
	if (!sigsegv_install_handler(sigsegv_handler)) {
		sprintf(str, GetString(STR_SIG_INSTALL_ERR), "SIGSEGV", strerror(errno));
		ErrorAlert(str);
		return false;
	}
#endif
	return true;
}

#ifdef USE_SDL
static bool init_sdl()
{
	int sdl_flags = 0;
#ifdef USE_SDL_VIDEO
	sdl_flags |= SDL_INIT_VIDEO;
#endif
#ifdef USE_SDL_AUDIO
	sdl_flags |= SDL_INIT_AUDIO;
#endif
	assert(sdl_flags != 0);

#ifdef USE_SDL_VIDEO
	// Don't let SDL block the screensaver
	setenv("SDL_VIDEO_ALLOW_SCREENSAVER", "1", true);

	// Make SDL pass through command-clicks and option-clicks unaltered
	setenv("SDL_HAS3BUTTONMOUSE", "1", true);
#endif

	if (SDL_Init(sdl_flags) == -1) {
		char str[256];
		sprintf(str, "Could not initialize SDL: %s.\n", SDL_GetError());
		ErrorAlert(str);
		return false;
	}
	atexit(SDL_Quit);

	// Don't let SDL catch SIGINT and SIGTERM signals
	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	return true;
}
#endif

int main(int argc, char **argv)
{
	char str[256];
	bool memory_mapped_from_zero, ram_rom_areas_contiguous;
	const char *vmdir = NULL;

	// Initialize variables
	RAMBase = 0;
	tzset();

	// Print some info
	printf(GetString(STR_ABOUT_TEXT1), VERSION_MAJOR, VERSION_MINOR);
	printf(" %s\n", GetString(STR_ABOUT_TEXT2));

#if !EMULATED_PPC
#ifdef SYSTEM_CLOBBERS_R2
	// Get TOC pointer
	TOC = get_r2();
#endif
#ifdef SYSTEM_CLOBBERS_R13
	// Get r13 register
	R13 = get_r13();
#endif
#endif

	// Parse command line arguments
	for (int i=1; i<argc; i++) {
		if (strcmp(argv[i], "--help") == 0) {
			usage(argv[0]);
#ifndef USE_SDL_VIDEO
		} else if (strcmp(argv[i], "--display") == 0) {
			i++;
			if (i < argc)
				x_display_name = strdup(argv[i]);
#endif
		} else if (strcmp(argv[i], "--gui-connection") == 0) {
			argv[i++] = NULL;
			if (i < argc) {
				gui_connection_path = argv[i];
				argv[i] = NULL;
			}
		} else if (valid_vmdir(argv[i])) {
			vmdir = argv[i];
			argv[i] = NULL;
			printf("Using %s as vmdir.\n", vmdir);
			if (chdir(vmdir)) {
				printf("Failed to chdir to %s. Good bye.", vmdir);
				exit(1);
			}
			break;
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

	// Connect to the external GUI
	if (gui_connection_path) {
		if ((gui_connection = rpc_init_client(gui_connection_path)) == NULL) {
			fprintf(stderr, "Failed to initialize RPC client connection to the GUI\n");
			return 1;
		}
	}

#ifdef ENABLE_GTK
	if (!gui_connection) {
		// Init GTK
		gtk_set_locale();
		gtk_init(&argc, &argv);
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
	if (!init_sdl())
		goto quit;
#endif

#ifndef USE_SDL_VIDEO
	// Open display
	x_display = XOpenDisplay(x_display_name);
	if (x_display == NULL) {
		char str[256];
		sprintf(str, GetString(STR_NO_XSERVER_ERR), XDisplayName(x_display_name));
		ErrorAlert(str);
		goto quit;
	}

#if defined(ENABLE_XF86_DGA) && !defined(ENABLE_MON)
	// Fork out, so we can return from fullscreen mode when things get ugly
	XF86DGAForkApp(DefaultScreen(x_display));
#endif
#endif

#ifdef ENABLE_MON
	// Initialize mon
	mon_init();
#endif

  // Install signal handlers
	if (!install_signal_handlers())
		goto quit;

	// Initialize VM system
	vm_init();

	// Get system info
	get_system_info();

	// Init system routines
	SysInit();

	// Show preferences editor
	if (!PrefsFindBool("nogui"))
		if (!PrefsEditor())
			goto quit;

#if !EMULATED_PPC
	// Check some things
	paranoia_check();
#endif

	// Open /dev/zero
	zero_fd = open("/dev/zero", O_RDWR);
	if (zero_fd < 0) {
		sprintf(str, GetString(STR_NO_DEV_ZERO_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}

	// Create areas for Kernel Data
	if (!kernel_data_init())
		goto quit;
	kernel_data = (KernelData *)Mac2HostAddr(KERNEL_DATA_BASE);
	emulator_data = &kernel_data->ed;
	KernelDataAddr = KERNEL_DATA_BASE;
	D(bug("Kernel Data at %p (%08x)\n", kernel_data, KERNEL_DATA_BASE));
	D(bug("Emulator Data at %p (%08x)\n", emulator_data, KERNEL_DATA_BASE + offsetof(KernelData, ed)));

	// Create area for DR Cache
	if (vm_mac_acquire_fixed(DR_EMULATOR_BASE, DR_EMULATOR_SIZE) < 0) {
		sprintf(str, GetString(STR_DR_EMULATOR_MMAP_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
	dr_emulator_area_mapped = true;
	if (vm_mac_acquire_fixed(DR_CACHE_BASE, DR_CACHE_SIZE) < 0) {
		sprintf(str, GetString(STR_DR_CACHE_MMAP_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
	dr_cache_area_mapped = true;
#if !EMULATED_PPC
	if (vm_protect((char *)DR_CACHE_BASE, DR_CACHE_SIZE, VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_EXECUTE) < 0) {
		sprintf(str, GetString(STR_DR_CACHE_MMAP_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
#endif
	DRCacheAddr = DR_CACHE_BASE;
	D(bug("DR Cache at %p\n", DRCacheAddr));

	// Create area for SheepShaver data
	if (!SheepMem::Init()) {
		sprintf(str, GetString(STR_SHEEP_MEM_MMAP_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
	
	// Create area for Mac RAM
	RAMSize = PrefsFindInt32("ramsize");
	if (RAMSize < 8*1024*1024) {
		WarningAlert(GetString(STR_SMALL_RAM_WARN));
		RAMSize = 8*1024*1024;
	}
	memory_mapped_from_zero = false;
	ram_rom_areas_contiguous = false;
#if REAL_ADDRESSING && HAVE_LINKER_SCRIPT
	if (vm_mac_acquire_fixed(0, RAMSize) == 0) {
		D(bug("Could allocate RAM from 0x0000\n"));
		RAMBase = 0;
		RAMBaseHost = Mac2HostAddr(RAMBase);
		memory_mapped_from_zero = true;
	}
#endif
	if (!memory_mapped_from_zero) {
#ifndef PAGEZERO_HACK
		// Create Low Memory area (0x0000..0x3000)
		if (vm_mac_acquire_fixed(0, 0x3000) < 0) {
			sprintf(str, GetString(STR_LOW_MEM_MMAP_ERR), strerror(errno));
			ErrorAlert(str);
			goto quit;
		}
		lm_area_mapped = true;
#endif
#if REAL_ADDRESSING
		// Allocate RAM at any address. Since ROM must be higher than RAM, allocate the RAM
		// and ROM areas contiguously, plus a little extra to allow for ROM address alignment.
		RAMBaseHost = vm_mac_acquire(RAMSize + ROM_AREA_SIZE + ROM_ALIGNMENT);
		if (RAMBaseHost == VM_MAP_FAILED) {
			sprintf(str, GetString(STR_RAM_ROM_MMAP_ERR), strerror(errno));
			ErrorAlert(str);
			goto quit;
		}
		RAMBase = Host2MacAddr(RAMBaseHost);
		ROMBase = (RAMBase + RAMSize + ROM_ALIGNMENT -1) & -ROM_ALIGNMENT;
		ROMBaseHost = Mac2HostAddr(ROMBase);
		ram_rom_areas_contiguous = true;
#else
		if (vm_mac_acquire_fixed(RAM_BASE, RAMSize) < 0) {
			sprintf(str, GetString(STR_RAM_MMAP_ERR), strerror(errno));
			ErrorAlert(str);
			goto quit;
		}
		RAMBase = RAM_BASE;
		RAMBaseHost = Mac2HostAddr(RAMBase);
#endif
	}
#if !EMULATED_PPC
	if (vm_protect(RAMBaseHost, RAMSize, VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_EXECUTE) < 0) {
		sprintf(str, GetString(STR_RAM_MMAP_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
#endif
	ram_area_mapped = true;
	D(bug("RAM area at %p (%08x)\n", RAMBaseHost, RAMBase));

	if (RAMBase > KernelDataAddr) {
		ErrorAlert(GetString(STR_RAM_AREA_TOO_HIGH_ERR));
		goto quit;
	}
	
	// Create area for Mac ROM
	if (!ram_rom_areas_contiguous) {
		if (vm_mac_acquire_fixed(ROM_BASE, ROM_AREA_SIZE) < 0) {
			sprintf(str, GetString(STR_ROM_MMAP_ERR), strerror(errno));
			ErrorAlert(str);
			goto quit;
		}
		ROMBase = ROM_BASE;
		ROMBaseHost = Mac2HostAddr(ROMBase);
	}
#if !EMULATED_PPC
	if (vm_protect(ROMBaseHost, ROM_AREA_SIZE, VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_EXECUTE) < 0) {
		sprintf(str, GetString(STR_ROM_MMAP_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
#endif
	rom_area_mapped = true;
	D(bug("ROM area at %p (%08x)\n", ROMBaseHost, ROMBase));

	if (RAMBase > ROMBase) {
		ErrorAlert(GetString(STR_RAM_HIGHER_THAN_ROM_ERR));
		goto quit;
	}

	// Load Mac ROM
	if (!load_mac_rom())
		goto quit;

	// Initialize everything
	if (!InitAll(vmdir))
		goto quit;
	D(bug("Initialization complete\n"));

	// Clear caches (as we loaded and patched code) and write protect ROM
#if !EMULATED_PPC
	flush_icache_range(ROMBase, ROMBase + ROM_AREA_SIZE);
#endif
	vm_protect(ROMBaseHost, ROM_AREA_SIZE, VM_PAGE_READ | VM_PAGE_EXECUTE);

	// Start 60Hz thread
	tick_thread_cancel = false;
	tick_thread_active = (pthread_create(&tick_thread, NULL, tick_func, NULL) == 0);
	D(bug("Tick thread installed (%ld)\n", tick_thread));

	// Start NVRAM watchdog thread
	memcpy(last_xpram, XPRAM, XPRAM_SIZE);
	nvram_thread_cancel = false;
	nvram_thread_active = (pthread_create(&nvram_thread, NULL, nvram_func, NULL) == 0);
	D(bug("NVRAM thread installed (%ld)\n", nvram_thread));

#if !EMULATED_PPC
	// Install SIGILL handler
	sigemptyset(&sigill_action.sa_mask);	// Block interrupts during ILL handling
	sigaddset(&sigill_action.sa_mask, SIGUSR2);
	sigill_action.sa_sigaction = sigill_handler;
	sigill_action.sa_flags = SA_ONSTACK | SA_SIGINFO;
#ifdef HAVE_SIGNAL_SA_RESTORER
	sigill_action.sa_restorer = NULL;
#endif
	if (sigaction(SIGILL, &sigill_action, NULL) < 0) {
		sprintf(str, GetString(STR_SIG_INSTALL_ERR), "SIGILL", strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
#endif

#if !EMULATED_PPC
	// Install interrupt signal handler
	sigemptyset(&sigusr2_action.sa_mask);
	sigusr2_action.sa_sigaction = sigusr2_handler_init;
	sigusr2_action.sa_flags = SA_ONSTACK | SA_RESTART | SA_SIGINFO;
#ifdef HAVE_SIGNAL_SA_RESTORER
	sigusr2_action.sa_restorer = NULL;
#endif
	if (sigaction(SIGUSR2, &sigusr2_action, NULL) < 0) {
		sprintf(str, GetString(STR_SIG_INSTALL_ERR), "SIGUSR2", strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
#endif

	// Get my thread ID and execute MacOS thread function
	emul_thread = pthread_self();
	D(bug("MacOS thread is %ld\n", emul_thread));
	emul_func(NULL);

quit:
	Quit();
	return 0;
}


/*
 *  Cleanup and quit
 */

static void Quit(void)
{
#if EMULATED_PPC
	// Exit PowerPC emulation
	exit_emul_ppc();
#endif

	// Stop 60Hz thread
	if (tick_thread_active) {
		tick_thread_cancel = true;
		pthread_cancel(tick_thread);
		pthread_join(tick_thread, NULL);
	}

	// Stop NVRAM watchdog thread
	if (nvram_thread_active) {
		nvram_thread_cancel = true;
		pthread_cancel(nvram_thread);
		pthread_join(nvram_thread, NULL);
	}

#if !EMULATED_PPC
	// Uninstall SIGSEGV and SIGBUS handlers
	sigemptyset(&sigsegv_action.sa_mask);
	sigsegv_action.sa_handler = SIG_DFL;
	sigsegv_action.sa_flags = 0;
	sigaction(SIGSEGV, &sigsegv_action, NULL);
	sigaction(SIGBUS, &sigsegv_action, NULL);

	// Uninstall SIGILL handler
	sigemptyset(&sigill_action.sa_mask);
	sigill_action.sa_handler = SIG_DFL;
	sigill_action.sa_flags = 0;
	sigaction(SIGILL, &sigill_action, NULL);

	// Delete stacks for signal handlers
	if (sig_stack.ss_sp)
		free(sig_stack.ss_sp);
	if (extra_stack.ss_sp)
		free(extra_stack.ss_sp);
#endif

	// Deinitialize everything
	ExitAll();

	// Delete SheepShaver globals
	SheepMem::Exit();

	// Delete RAM area
	if (ram_area_mapped)
		vm_mac_release(RAMBase, RAMSize);

	// Delete ROM area
	if (rom_area_mapped)
		vm_mac_release(ROMBase, ROM_AREA_SIZE);

	// Delete DR cache areas
	if (dr_emulator_area_mapped)
		vm_mac_release(DR_EMULATOR_BASE, DR_EMULATOR_SIZE);
	if (dr_cache_area_mapped)
		vm_mac_release(DR_CACHE_BASE, DR_CACHE_SIZE);

	// Delete Low Memory area
	if (lm_area_mapped)
		vm_mac_release(0, 0x3000);

	// Close /dev/zero
	if (zero_fd > 0)
		close(zero_fd);

	// Exit system routines
	SysExit();

	// Exit preferences
	PrefsExit();

#ifdef ENABLE_MON
	// Exit mon
	mon_exit();
#endif

	// Close X11 server connection
#ifndef USE_SDL_VIDEO
	if (x_display)
		XCloseDisplay(x_display);
#endif

	// Notify GUI we are about to leave
	if (gui_connection) {
		if (rpc_method_invoke(gui_connection, RPC_METHOD_EXIT, RPC_TYPE_INVALID) == RPC_ERROR_NO_ERROR)
			rpc_method_wait_for_reply(gui_connection, RPC_TYPE_INVALID);
	}

	exit(0);
}


/*
 *  Initialize Kernel Data segments
 */

static bool kernel_data_init(void)
{
	int error_string = STR_KD_SHMGET_ERR;
	uint32 kernel_area_size = (KERNEL_AREA_SIZE + SHMLBA - 1) & -SHMLBA;
	int kernel_area = shmget(IPC_PRIVATE, kernel_area_size, 0600);
	if (kernel_area != -1) {
		bool mapped =
			shm_map_address(kernel_area, KERNEL_DATA_BASE & -SHMLBA) &&
			shm_map_address(kernel_area, KERNEL_DATA2_BASE & -SHMLBA);

		// Mark the shared memory segment for removal. This is safe to do
		// because the deletion is not performed while the memory is still
		// mapped and so will only be done once the process exits.
		shmctl(kernel_area, IPC_RMID, NULL);
		if (mapped)
			return true;

		error_string = STR_KD_SHMAT_ERR;
	}

	char str[256];
	sprintf(str, GetString(error_string), strerror(errno));
	ErrorAlert(str);
	return false;
}


/*
 *  Maps the memory identified by kernel_area at the specified addr
 */

static bool shm_map_address(int kernel_area, uint32 addr)
{
	void *kernel_addr = Mac2HostAddr(addr);
	return shmat(kernel_area, kernel_addr, 0) == kernel_addr;
}


/*
 *  Jump into Mac ROM, start 680x0 emulator
 */

#if EMULATED_PPC
void jump_to_rom(uint32 entry)
{
	init_emul_ppc();
	emul_ppc(entry);
}
#endif


/*
 *  Emulator thread function
 */

static void *emul_func(void *arg)
{
	// We're now ready to receive signals
	ready_for_signals = true;

	// Decrease priority, so more time-critical things like audio will work better
	nice(1);

	// Jump to ROM boot routine
	D(bug("Jumping to ROM\n"));
#if EMULATED_PPC
	jump_to_rom(ROMBase + 0x310000);
#else
	jump_to_rom(ROMBase + 0x310000, (uint32)emulator_data);
#endif
	D(bug("Returned from ROM\n"));

	// We're no longer ready to receive signals
	ready_for_signals = false;
	return NULL;
}


#if !EMULATED_PPC
/*
 *  Execute 68k subroutine (must be ended with RTS)
 *  This must only be called by the emul_thread when in EMUL_OP mode
 *  r->a[7] is unused, the routine runs on the caller's stack
 */

void Execute68k(uint32 pc, M68kRegisters *r)
{
#if SAFE_EXEC_68K
	if (ReadMacInt32(XLM_RUN_MODE) != MODE_EMUL_OP)
		printf("FATAL: Execute68k() not called from EMUL_OP mode\n");
	if (!pthread_equal(pthread_self(), emul_thread))
		printf("FATAL: Execute68k() not called from emul_thread\n");
#endif
	execute_68k(pc, r);
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
#endif


/*
 *  Quit emulator (cause return from jump_to_rom)
 */

void QuitEmulator(void)
{
#if EMULATED_PPC
	Quit();
#else
	quit_emulator();
#endif
}


/*
 *  Dump 68k registers
 */

void Dump68kRegs(M68kRegisters *r)
{
	// Display 68k registers
	for (int i=0; i<8; i++) {
		printf("d%d: %08x", i, r->d[i]);
		if (i == 3 || i == 7)
			printf("\n");
		else
			printf(", ");
	}
	for (int i=0; i<8; i++) {
		printf("a%d: %08x", i, r->a[i]);
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
#if EMULATED_PPC
	FlushCodeCache(start, start + length);
#else
	flush_icache_range(start, start + length);
#endif
}


/*
 *  NVRAM watchdog thread (saves NVRAM every minute)
 */

static void nvram_watchdog(void)
{
	if (memcmp(last_xpram, XPRAM, XPRAM_SIZE)) {
		memcpy(last_xpram, XPRAM, XPRAM_SIZE);
		SaveXPRAM();
	}
}

static void *nvram_func(void *arg)
{
	while (!nvram_thread_cancel) {
		for (int i=0; i<60 && !nvram_thread_cancel; i++)
			Delay_usec(999999);		// Only wait 1 second so we quit promptly when nvram_thread_cancel becomes true
		nvram_watchdog();
	}
	return NULL;
}


/*
 *  60Hz thread (really 60.15Hz)
 */

static void *tick_func(void *arg)
{
	int tick_counter = 0;
	uint64 start = GetTicks_usec();
	int64 ticks = 0;
	uint64 next = GetTicks_usec();

	while (!tick_thread_cancel) {

		// Wait
		next += 16625;
		int64 delay = next - GetTicks_usec();
		if (delay > 0)
			Delay_usec(delay);
		else if (delay < -16625)
			next = GetTicks_usec();
		ticks++;

#if !EMULATED_PPC
		// Did we crash?
		if (emul_thread_fatal) {

			// Yes, dump registers
			sigregs *r = &sigsegv_regs;
			char str[256];
			if (crash_reason == NULL)
				crash_reason = "SIGSEGV";
			sprintf(str, "%s\n"
				"   pc %08lx     lr %08lx    ctr %08lx    msr %08lx\n"
				"  xer %08lx     cr %08lx  \n"
				"   r0 %08lx     r1 %08lx     r2 %08lx     r3 %08lx\n"
				"   r4 %08lx     r5 %08lx     r6 %08lx     r7 %08lx\n"
				"   r8 %08lx     r9 %08lx    r10 %08lx    r11 %08lx\n"
				"  r12 %08lx    r13 %08lx    r14 %08lx    r15 %08lx\n"
				"  r16 %08lx    r17 %08lx    r18 %08lx    r19 %08lx\n"
				"  r20 %08lx    r21 %08lx    r22 %08lx    r23 %08lx\n"
				"  r24 %08lx    r25 %08lx    r26 %08lx    r27 %08lx\n"
				"  r28 %08lx    r29 %08lx    r30 %08lx    r31 %08lx\n",
				crash_reason,
				r->nip, r->link, r->ctr, r->msr,
				r->xer, r->ccr,
				r->gpr[0], r->gpr[1], r->gpr[2], r->gpr[3],
				r->gpr[4], r->gpr[5], r->gpr[6], r->gpr[7],
				r->gpr[8], r->gpr[9], r->gpr[10], r->gpr[11],
				r->gpr[12], r->gpr[13], r->gpr[14], r->gpr[15],
				r->gpr[16], r->gpr[17], r->gpr[18], r->gpr[19],
				r->gpr[20], r->gpr[21], r->gpr[22], r->gpr[23],
				r->gpr[24], r->gpr[25], r->gpr[26], r->gpr[27],
				r->gpr[28], r->gpr[29], r->gpr[30], r->gpr[31]);
			printf(str);
			VideoQuitFullScreen();

#ifdef ENABLE_MON
			// Start up mon in real-mode
			printf("Welcome to the sheep factory.\n");
			const char *arg[4] = {"mon", "-m", "-r", NULL};
			mon(3, arg);
#endif
			return NULL;
		}
#endif

		// Pseudo Mac 1Hz interrupt, update local time
		if (++tick_counter > 60) {
			tick_counter = 0;
			WriteMacInt32(0x20c, TimerDateTime());
		}

		// Trigger 60Hz interrupt
		if (ReadMacInt32(XLM_IRQ_NEST) == 0) {
			SetInterruptFlag(INTFLAG_VIA);
			TriggerInterrupt();
		}
	}

	uint64 end = GetTicks_usec();
	D(bug("%lld ticks in %lld usec = %f ticks/sec\n", ticks, end - start, ticks * 1000000.0 / (end - start)));
	return NULL;
}


/*
 *  Pthread configuration
 */

void Set_pthread_attr(pthread_attr_t *attr, int priority)
{
#ifdef HAVE_PTHREADS
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
#endif
}


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
 *  Trigger signal USR2 from another thread
 */

#if !EMULATED_PPC
void TriggerInterrupt(void)
{
	if (ready_for_signals) {
		idle_resume();
		pthread_kill(emul_thread, SIGUSR2);
	}
}
#endif


/*
 *  Interrupt flags (must be handled atomically!)
 */

volatile uint32 InterruptFlags = 0;

void SetInterruptFlag(uint32 flag)
{
	atomic_or((int *)&InterruptFlags, flag);
}

void ClearInterruptFlag(uint32 flag)
{
	atomic_and((int *)&InterruptFlags, ~flag);
}


/*
 *  Disable interrupts
 */

void DisableInterrupt(void)
{
#if EMULATED_PPC
	WriteMacInt32(XLM_IRQ_NEST, int32(ReadMacInt32(XLM_IRQ_NEST)) + 1);
#else
	atomic_add((int *)XLM_IRQ_NEST, 1);
#endif
}


/*
 *  Enable interrupts
 */

void EnableInterrupt(void)
{
#if EMULATED_PPC
	WriteMacInt32(XLM_IRQ_NEST, int32(ReadMacInt32(XLM_IRQ_NEST)) - 1);
#else
	atomic_add((int *)XLM_IRQ_NEST, -1);
#endif
}


/*
 *  USR2 handler
 */

#if !EMULATED_PPC
void sigusr2_handler(int sig, siginfo_t *sip, void *scp)
{
	machine_regs *r = MACHINE_REGISTERS(scp);

#ifdef SYSTEM_CLOBBERS_R2
	// Restore pointer to Thread Local Storage
	set_r2(TOC);
#endif
#ifdef SYSTEM_CLOBBERS_R13
	// Restore pointer to .sdata section
	set_r13(R13);
#endif

#ifdef USE_SDL_VIDEO
	// We must fill in the events queue in the same thread that did call SDL_SetVideoMode()
	SDL_PumpEvents();
#endif

	// Do nothing if interrupts are disabled
	if (*(int32 *)XLM_IRQ_NEST > 0)
		return;

	// Disable MacOS stack sniffer
	WriteMacInt32(0x110, 0);

	// Interrupt action depends on current run mode
	switch (ReadMacInt32(XLM_RUN_MODE)) {
		case MODE_68K:
			// 68k emulator active, trigger 68k interrupt level 1
			WriteMacInt16(ntohl(kernel_data->v[0x67c >> 2]), 1);
			r->cr() |= ntohl(kernel_data->v[0x674 >> 2]);
			break;

#if INTERRUPTS_IN_NATIVE_MODE
		case MODE_NATIVE:
			// 68k emulator inactive, in nanokernel?
			if (r->gpr(1) != KernelDataAddr) {

				// Set extra stack for SIGSEGV handler
				sigaltstack(&extra_stack, NULL);
				
				// Prepare for 68k interrupt level 1
				WriteMacInt16(ntohl(kernel_data->v[0x67c >> 2]), 1);
				WriteMacInt32(ntohl(kernel_data->v[0x658 >> 2]) + 0xdc, ReadMacInt32(ntohl(kernel_data->v[0x658 >> 2]) + 0xdc) | ntohl(kernel_data->v[0x674 >> 2]));

				// Execute nanokernel interrupt routine (this will activate the 68k emulator)
				DisableInterrupt();
				if (ROMType == ROMTYPE_NEWWORLD)
					ppc_interrupt(ROMBase + 0x312b1c, KernelDataAddr);
				else
					ppc_interrupt(ROMBase + 0x312a3c, KernelDataAddr);

				// Reset normal stack
				sigaltstack(&sig_stack, NULL);
			}
			break;
#endif

#if INTERRUPTS_IN_EMUL_OP_MODE
		case MODE_EMUL_OP:
			// 68k emulator active, within EMUL_OP routine, execute 68k interrupt routine directly when interrupt level is 0
			if ((ReadMacInt32(XLM_68K_R25) & 7) == 0) {

				// Set extra stack for SIGSEGV handler
				sigaltstack(&extra_stack, NULL);
#if 1
				// Execute full 68k interrupt routine
				M68kRegisters r;
				uint32 old_r25 = ReadMacInt32(XLM_68K_R25);	// Save interrupt level
				WriteMacInt32(XLM_68K_R25, 0x21);			// Execute with interrupt level 1
				static const uint16 proc[] = {
					0x3f3c, 0x0000,		// move.w	#$0000,-(sp)	(fake format word)
					0x487a, 0x000a,		// pea		@1(pc)			(return address)
					0x40e7,				// move		sr,-(sp)		(saved SR)
					0x2078, 0x0064,		// move.l	$64,a0
					0x4ed0,				// jmp		(a0)
					M68K_RTS			// @1
				};
				Execute68k((uint32)proc, &r);
				WriteMacInt32(XLM_68K_R25, old_r25);		// Restore interrupt level
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
				// Reset normal stack
				sigaltstack(&sig_stack, NULL);
			}
			break;
#endif
	}
}
#endif


/*
 *  SIGSEGV handler
 */

#if !EMULATED_PPC
static void sigsegv_handler(int sig, siginfo_t *sip, void *scp)
{
	machine_regs *r = MACHINE_REGISTERS(scp);

	// Get effective address
	uint32 addr = r->dar();
	
#ifdef SYSTEM_CLOBBERS_R2
	// Restore pointer to Thread Local Storage
	set_r2(TOC);
#endif
#ifdef SYSTEM_CLOBBERS_R13
	// Restore pointer to .sdata section
	set_r13(R13);
#endif

#if ENABLE_VOSF
	// Handle screen fault
#if SIGSEGV_CHECK_VERSION(1,0,0)
	sigsegv_info_t si;
	si.addr = (sigsegv_address_t)addr;
	si.pc = (sigsegv_address_t)r->pc();
#endif
	extern bool Screen_fault_handler(sigsegv_info_t *sip);
	if (Screen_fault_handler(&si))
		return;
#endif

	// Fault in Mac ROM or RAM or DR Cache?
	bool mac_fault = (r->pc() >= ROMBase) && (r->pc() < (ROMBase + ROM_AREA_SIZE)) || (r->pc() >= RAMBase) && (r->pc() < (RAMBase + RAMSize)) || (r->pc() >= DR_CACHE_BASE && r->pc() < (DR_CACHE_BASE + DR_CACHE_SIZE));
	if (mac_fault) {

		// "VM settings" during MacOS 8 installation
		if (r->pc() == ROMBase + 0x488160 && r->gpr(20) == 0xf8000000) {
			r->pc() += 4;
			r->gpr(8) = 0;
			return;
	
		// MacOS 8.5 installation
		} else if (r->pc() == ROMBase + 0x488140 && r->gpr(16) == 0xf8000000) {
			r->pc() += 4;
			r->gpr(8) = 0;
			return;
	
		// MacOS 8 serial drivers on startup
		} else if (r->pc() == ROMBase + 0x48e080 && (r->gpr(8) == 0xf3012002 || r->gpr(8) == 0xf3012000)) {
			r->pc() += 4;
			r->gpr(8) = 0;
			return;
	
		// MacOS 8.1 serial drivers on startup
		} else if (r->pc() == ROMBase + 0x48c5e0 && (r->gpr(20) == 0xf3012002 || r->gpr(20) == 0xf3012000)) {
			r->pc() += 4;
			return;
		} else if (r->pc() == ROMBase + 0x4a10a0 && (r->gpr(20) == 0xf3012002 || r->gpr(20) == 0xf3012000)) {
			r->pc() += 4;
			return;
	
		// MacOS 8.6 serial drivers on startup (with DR Cache and OldWorld ROM)
		} else if ((r->pc() - DR_CACHE_BASE) < DR_CACHE_SIZE && (r->gpr(16) == 0xf3012002 || r->gpr(16) == 0xf3012000)) {
			r->pc() += 4;
			return;
		} else if ((r->pc() - DR_CACHE_BASE) < DR_CACHE_SIZE && (r->gpr(20) == 0xf3012002 || r->gpr(20) == 0xf3012000)) {
			r->pc() += 4;
			return;
		}

		// Get opcode and divide into fields
		uint32 opcode = *((uint32 *)r->pc());
		uint32 primop = opcode >> 26;
		uint32 exop = (opcode >> 1) & 0x3ff;
		uint32 ra = (opcode >> 16) & 0x1f;
		uint32 rb = (opcode >> 11) & 0x1f;
		uint32 rd = (opcode >> 21) & 0x1f;
		int32 imm = (int16)(opcode & 0xffff);

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
#if EMULATE_UNALIGNED_LOADSTORE_MULTIPLE
			case 46:	// lmw
				if ((addr % 4) != 0) {
					uint32 ea = addr;
					D(bug("WARNING: unaligned lmw to EA=%08x from IP=%08x\n", ea, r->pc()));
					for (int i = rd; i <= 31; i++) {
						r->gpr(i) = ReadMacInt32(ea);
						ea += 4;
					}
					r->pc() += 4;
					goto rti;
				}
				break;
			case 47:	// stmw
				if ((addr % 4) != 0) {
					uint32 ea = addr;
					D(bug("WARNING: unaligned stmw to EA=%08x from IP=%08x\n", ea, r->pc()));
					for (int i = rd; i <= 31; i++) {
						WriteMacInt32(ea, r->gpr(i));
						ea += 4;
					}
					r->pc() += 4;
					goto rti;
				}
				break;
#endif
		}
	
		// Ignore ROM writes (including to the zero page, which is read-only)
		if (transfer_type == TYPE_STORE &&
			((addr >= ROMBase && addr < ROMBase + ROM_SIZE) ||
			 (addr >= SheepMem::ZeroPage() && addr < SheepMem::ZeroPage() + SheepMem::PageSize()))) {
//			D(bug("WARNING: %s write access to ROM at %08lx, pc %08lx\n", transfer_size == SIZE_BYTE ? "Byte" : transfer_size == SIZE_HALFWORD ? "Halfword" : "Word", addr, r->pc()));
			if (addr_mode == MODE_U || addr_mode == MODE_UX)
				r->gpr(ra) = addr;
			r->pc() += 4;
			goto rti;
		}

		// Ignore illegal memory accesses?
		if (PrefsFindBool("ignoresegv")) {
			if (addr_mode == MODE_U || addr_mode == MODE_UX)
				r->gpr(ra) = addr;
			if (transfer_type == TYPE_LOAD)
				r->gpr(rd) = 0;
			r->pc() += 4;
			goto rti;
		}

		// In GUI mode, show error alert
		if (!PrefsFindBool("nogui")) {
			char str[256];
			if (transfer_type == TYPE_LOAD || transfer_type == TYPE_STORE)
				sprintf(str, GetString(STR_MEM_ACCESS_ERR), transfer_size == SIZE_BYTE ? "byte" : transfer_size == SIZE_HALFWORD ? "halfword" : "word", transfer_type == TYPE_LOAD ? GetString(STR_MEM_ACCESS_READ) : GetString(STR_MEM_ACCESS_WRITE), addr, r->pc(), r->gpr(24), r->gpr(1));
			else
				sprintf(str, GetString(STR_UNKNOWN_SEGV_ERR), r->pc(), r->gpr(24), r->gpr(1), opcode);
			ErrorAlert(str);
			QuitEmulator();
			return;
		}
	}

	// For all other errors, jump into debugger (sort of...)
	crash_reason = (sig == SIGBUS) ? "SIGBUS" : "SIGSEGV";
	if (!ready_for_signals) {
		printf("%s\n");
		printf(" sigcontext %p, machine_regs %p\n", scp, r);
		printf(
			"   pc %08lx     lr %08lx    ctr %08lx    msr %08lx\n"
			"  xer %08lx     cr %08lx  \n"
			"   r0 %08lx     r1 %08lx     r2 %08lx     r3 %08lx\n"
			"   r4 %08lx     r5 %08lx     r6 %08lx     r7 %08lx\n"
			"   r8 %08lx     r9 %08lx    r10 %08lx    r11 %08lx\n"
			"  r12 %08lx    r13 %08lx    r14 %08lx    r15 %08lx\n"
			"  r16 %08lx    r17 %08lx    r18 %08lx    r19 %08lx\n"
			"  r20 %08lx    r21 %08lx    r22 %08lx    r23 %08lx\n"
			"  r24 %08lx    r25 %08lx    r26 %08lx    r27 %08lx\n"
			"  r28 %08lx    r29 %08lx    r30 %08lx    r31 %08lx\n",
			crash_reason,
			r->pc(), r->lr(), r->ctr(), r->msr(),
			r->xer(), r->cr(),
			r->gpr(0), r->gpr(1), r->gpr(2), r->gpr(3),
			r->gpr(4), r->gpr(5), r->gpr(6), r->gpr(7),
			r->gpr(8), r->gpr(9), r->gpr(10), r->gpr(11),
			r->gpr(12), r->gpr(13), r->gpr(14), r->gpr(15),
			r->gpr(16), r->gpr(17), r->gpr(18), r->gpr(19),
			r->gpr(20), r->gpr(21), r->gpr(22), r->gpr(23),
			r->gpr(24), r->gpr(25), r->gpr(26), r->gpr(27),
			r->gpr(28), r->gpr(29), r->gpr(30), r->gpr(31));
		exit(1);
		QuitEmulator();
		return;
	} else {
		// We crashed. Save registers, tell tick thread and loop forever
		build_sigregs(&sigsegv_regs, r);
		emul_thread_fatal = true;
		for (;;) ;
	}
rti:;
}


/*
 *  SIGILL handler
 */

static void sigill_handler(int sig, siginfo_t *sip, void *scp)
{
	machine_regs *r = MACHINE_REGISTERS(scp);
	char str[256];

#ifdef SYSTEM_CLOBBERS_R2
	// Restore pointer to Thread Local Storage
	set_r2(TOC);
#endif
#ifdef SYSTEM_CLOBBERS_R13
	// Restore pointer to .sdata section
	set_r13(R13);
#endif

	// Fault in Mac ROM or RAM?
	bool mac_fault = (r->pc() >= ROMBase) && (r->pc() < (ROMBase + ROM_AREA_SIZE)) || (r->pc() >= RAMBase) && (r->pc() < (RAMBase + RAMSize));
	if (mac_fault) {

		// Get opcode and divide into fields
		uint32 opcode = *((uint32 *)r->pc());
		uint32 primop = opcode >> 26;
		uint32 exop = (opcode >> 1) & 0x3ff;
		uint32 ra = (opcode >> 16) & 0x1f;
		uint32 rb = (opcode >> 11) & 0x1f;
		uint32 rd = (opcode >> 21) & 0x1f;
		int32 imm = (int16)(opcode & 0xffff);

		switch (primop) {
			case 9:		// POWER instructions
			case 22:
power_inst:		sprintf(str, GetString(STR_POWER_INSTRUCTION_ERR), r->pc(), r->gpr(1), opcode);
				ErrorAlert(str);
				QuitEmulator();
				return;

			case 31:
				switch (exop) {
					case 83:	// mfmsr
						r->gpr(rd) = 0xf072;
						r->pc() += 4;
						goto rti;

					case 210:	// mtsr
					case 242:	// mtsrin
					case 306:	// tlbie
						r->pc() += 4;
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
								r->pc() += 4;
								goto rti;
							case 25:	// SDR1
								r->gpr(rd) = 0xdead001f;
								r->pc() += 4;
								goto rti;
							case 287:	// PVR
								r->gpr(rd) = PVR;
								r->pc() += 4;
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
								r->pc() += 4;
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
			sprintf(str, GetString(STR_UNKNOWN_SEGV_ERR), r->pc(), r->gpr(24), r->gpr(1), opcode);
			ErrorAlert(str);
			QuitEmulator();
			return;
		}
	}

	// For all other errors, jump into debugger (sort of...)
	crash_reason = "SIGILL";
	if (!ready_for_signals) {
		printf("%s\n");
		printf(" sigcontext %p, machine_regs %p\n", scp, r);
		printf(
			"   pc %08lx     lr %08lx    ctr %08lx    msr %08lx\n"
			"  xer %08lx     cr %08lx  \n"
			"   r0 %08lx     r1 %08lx     r2 %08lx     r3 %08lx\n"
			"   r4 %08lx     r5 %08lx     r6 %08lx     r7 %08lx\n"
			"   r8 %08lx     r9 %08lx    r10 %08lx    r11 %08lx\n"
			"  r12 %08lx    r13 %08lx    r14 %08lx    r15 %08lx\n"
			"  r16 %08lx    r17 %08lx    r18 %08lx    r19 %08lx\n"
			"  r20 %08lx    r21 %08lx    r22 %08lx    r23 %08lx\n"
			"  r24 %08lx    r25 %08lx    r26 %08lx    r27 %08lx\n"
			"  r28 %08lx    r29 %08lx    r30 %08lx    r31 %08lx\n",
			crash_reason,
			r->pc(), r->lr(), r->ctr(), r->msr(),
			r->xer(), r->cr(),
			r->gpr(0), r->gpr(1), r->gpr(2), r->gpr(3),
			r->gpr(4), r->gpr(5), r->gpr(6), r->gpr(7),
			r->gpr(8), r->gpr(9), r->gpr(10), r->gpr(11),
			r->gpr(12), r->gpr(13), r->gpr(14), r->gpr(15),
			r->gpr(16), r->gpr(17), r->gpr(18), r->gpr(19),
			r->gpr(20), r->gpr(21), r->gpr(22), r->gpr(23),
			r->gpr(24), r->gpr(25), r->gpr(26), r->gpr(27),
			r->gpr(28), r->gpr(29), r->gpr(30), r->gpr(31));
		exit(1);
		QuitEmulator();
		return;
	} else {
		// We crashed. Save registers, tell tick thread and loop forever
		build_sigregs(&sigsegv_regs, r);
		emul_thread_fatal = true;
		for (;;) ;
	}
rti:;
}
#endif


/*
 *  Helpers to share 32-bit addressable data with MacOS
 */

bool SheepMem::Init(void)
{
	// Size of a native page
	page_size = getpagesize();

	// Allocate SheepShaver globals
	proc = base;
	if (vm_mac_acquire_fixed(base, size) < 0)
		return false;

	// Allocate page with all bits set to 0, right in the middle
	// This is also used to catch undesired overlaps between proc and data areas
	zero_page = proc + (size / 2);
	Mac_memset(zero_page, 0, page_size);
	if (vm_protect(Mac2HostAddr(zero_page), page_size, VM_PAGE_READ) < 0)
		return false;

#if EMULATED_PPC
	// Allocate alternate stack for PowerPC interrupt routine
	sig_stack = base + size;
	if (vm_mac_acquire_fixed(sig_stack, SIG_STACK_SIZE) < 0)
		return false;
#endif

	data = base + size;
	return true;
}

void SheepMem::Exit(void)
{
	if (data) {
		// Delete SheepShaver globals
		vm_mac_release(base, size);

#if EMULATED_PPC
		// Delete alternate stack for PowerPC interrupt routine
		vm_mac_release(sig_stack, SIG_STACK_SIZE);
#endif
	}
}


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
