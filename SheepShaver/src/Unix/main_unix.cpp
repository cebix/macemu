/*
 *  main_unix.cpp - Emulation core, Unix implementation
 *
 *  SheepShaver (C) 1997-2002 Christian Bauer and Marc Hellwig
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
 *    - There is no TOC under Linux; r2 is free for the user
 *    - r13 is used as a small data pointer under Linux (but appearently
 *      it is not used this way? To be sure, we specify -msdata=none
 *      in the Makefile)
 *    - As there is no TOC, there are also no TVECTs under Linux;
 *      function pointers point directly to the function code
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
#include "sony.h"
#include "disk.h"
#include "cdrom.h"
#include "scsi.h"
#include "video.h"
#include "audio.h"
#include "ether.h"
#include "serial.h"
#include "clip.h"
#include "extfs.h"
#include "sys.h"
#include "macos_util.h"
#include "rom_patches.h"
#include "user_strings.h"
#include "vm_alloc.h"
#include "sigsegv.h"

#define DEBUG 0
#include "debug.h"


#include <X11/Xlib.h>

#ifdef ENABLE_GTK
#include <gtk/gtk.h>
#endif

#ifdef ENABLE_XF86_DGA
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/xf86dga.h>
#endif

#ifdef ENABLE_MON
#include "mon.h"
#endif


// Enable Execute68k() safety checks?
#define SAFE_EXEC_68K 0

// Interrupts in EMUL_OP mode?
#define INTERRUPTS_IN_EMUL_OP_MODE 1

// Interrupts in native mode?
#define INTERRUPTS_IN_NATIVE_MODE 1


// Constants
const char ROM_FILE_NAME[] = "ROM";
const char ROM_FILE_NAME2[] = "Mac OS ROM";

const uint32 RAM_BASE = 0x20000000;			// Base address of RAM
const uint32 SIG_STACK_SIZE = 0x10000;		// Size of signal stack


#if !EMULATED_PPC
// Structure in which registers are saved in a signal handler;
// sigcontext->regs points to it
// (see arch/ppc/kernel/signal.c)
typedef struct {
	uint32 u[4];
} __attribute((aligned(16))) vector128;
#include <linux/elf.h>

struct sigregs {
	elf_gregset_t	gp_regs;				// Identical to pt_regs
	double			fp_regs[ELF_NFPREG];	// f0..f31 and fpsrc
	//more (uninteresting) stuff following here
};
#endif


// Global variables (exported)
#if !EMULATED_PPC
void *TOC;				// Small data pointer (r13)
#endif
uint32 RAMBase;			// Base address of Mac RAM
uint32 RAMSize;			// Size of Mac RAM
uint32 SheepStack1Base;	// SheepShaver first alternate stack base
uint32 SheepStack2Base;	// SheepShaver second alternate stack base
uint32 SheepThunksBase;	// SheepShaver thunks base
uint32 KernelDataAddr;	// Address of Kernel Data
uint32 BootGlobsAddr;	// Address of BootGlobs structure at top of Mac RAM
uint32 PVR;				// Theoretical PVR
int64 CPUClockSpeed;	// Processor clock speed (Hz)
int64 BusClockSpeed;	// Bus clock speed (Hz)


// Global variables
char *x_display_name = NULL;				// X11 display name
Display *x_display = NULL;					// X11 display handle

static int zero_fd = 0;						// FD of /dev/zero
static bool sheep_area_mapped = false;		// Flag: SheepShaver data area mmap()ed
static bool lm_area_mapped = false;			// Flag: Low Memory area mmap()ped
static int kernel_area = -1;				// SHM ID of Kernel Data area
static bool rom_area_mapped = false;		// Flag: Mac ROM mmap()ped
static bool ram_area_mapped = false;		// Flag: Mac RAM mmap()ped
static KernelData *kernel_data;				// Pointer to Kernel Data
static EmulatorData *emulator_data;

static uint8 last_xpram[XPRAM_SIZE];		// Buffer for monitoring XPRAM changes

static bool nvram_thread_active = false;	// Flag: NVRAM watchdog installed
static pthread_t nvram_thread;				// NVRAM watchdog
static bool tick_thread_active = false;		// Flag: MacOS thread installed
static pthread_t tick_thread;				// 60Hz thread
static pthread_t emul_thread;				// MacOS thread

static bool ready_for_signals = false;		// Handler installed, signals can be sent
static int64 num_segv = 0;					// Number of handled SEGV signals

static struct sigaction sigusr2_action;		// Interrupt signal (of emulator thread)
#if !EMULATED_PPC
static struct sigaction sigsegv_action;		// Data access exception signal (of emulator thread)
static struct sigaction sigill_action;		// Illegal instruction signal (of emulator thread)
static void *sig_stack = NULL;				// Stack for signal handlers
static void *extra_stack = NULL;			// Stack for SIGSEGV inside interrupt handler
static bool emul_thread_fatal = false;		// Flag: MacOS thread crashed, tick thread shall dump debug output
static sigregs sigsegv_regs;				// Register dump when crashed
#endif


// Prototypes
static void Quit(void);
static void *emul_func(void *arg);
static void *nvram_func(void *arg);
static void *tick_func(void *arg);
#if EMULATED_PPC
static void sigusr2_handler(int sig);
extern void emul_ppc(uint32 start);
extern void init_emul_ppc(void);
extern void exit_emul_ppc(void);
#else
static void sigusr2_handler(int sig, sigcontext_struct *sc);
static void sigsegv_handler(int sig, sigcontext_struct *sc);
static void sigill_handler(int sig, sigcontext_struct *sc);
#endif


// From asm_linux.S
#if !EMULATED_PPC
extern "C" void *get_toc(void);
extern "C" void *get_sp(void);
extern "C" void flush_icache_range(void *start, void *end);
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

int main(int argc, char **argv)
{
	char str[256];
	uint32 *boot_globs;
	int16 i16;
	int rom_fd;
	FILE *proc_file;
	const char *rom_path;
	uint32 rom_size, actual;
	uint8 *rom_tmp;
	time_t now, expire;

	// Initialize variables
	RAMBase = 0;
	tzset();

	// Print some info
	printf(GetString(STR_ABOUT_TEXT1), VERSION_MAJOR, VERSION_MINOR);
	printf(" %s\n", GetString(STR_ABOUT_TEXT2));

#if !EMULATED_PPC
	// Get TOC pointer
	TOC = get_toc();
#endif

#ifdef ENABLE_GTK
	// Init GTK
	gtk_set_locale();
	gtk_init(&argc, &argv);
#endif

	// Read preferences
	PrefsInit(argc, argv);

	// Parse command line arguments
	for (int i=1; i<argc; i++) {
		if (strcmp(argv[i], "--help") == 0) {
			usage(argv[0]);
		} else if (strcmp(argv[i], "--display") == 0) {
			i++;
			if (i < argc)
				x_display_name = strdup(argv[i]);
		} else if (argv[i][0] == '-') {
			fprintf(stderr, "Unrecognized option '%s'\n", argv[i]);
			usage(argv[0]);
		}
	}

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

#ifdef ENABLE_MON
	// Initialize mon
	mon_init();
#endif

	// Get system info
	PVR = 0x00040000;			// Default: 604
	CPUClockSpeed = 100000000;	// Default: 100MHz
	BusClockSpeed = 100000000;	// Default: 100MHz
#if !EMULATED_PPC
	proc_file = fopen("/proc/cpuinfo", "r");
	if (proc_file) {
		char line[256];
		while(fgets(line, 255, proc_file)) {
			// Read line
			int len = strlen(line);
			if (len == 0)
				continue;
			line[len-1] = 0;

			// Parse line
			int i;
			char value[256];
			if (sscanf(line, "cpu : %s", value) == 1) {
				if (strcmp(value, "601") == 0)
					PVR = 0x00010000;
				else if (strcmp(value, "603") == 0)
					PVR = 0x00030000;
				else if (strcmp(value, "604") == 0)
					PVR = 0x00040000;
				else if (strcmp(value, "603e") == 0)
					PVR = 0x00060000;
				else if (strcmp(value, "603ev") == 0)
					PVR = 0x00070000;
				else if (strcmp(value, "604e") == 0)
					PVR = 0x00090000;
				else if (strcmp(value, "604ev5") == 0)
					PVR = 0x000a0000;
				else if (strcmp(value, "750") == 0)
					PVR = 0x00080000;
				else if (strcmp(value, "821") == 0)
					PVR = 0x00320000;
				else if (strcmp(value, "860") == 0)
					PVR = 0x00500000;
				else
					printf("WARNING: Unknown CPU type '%s', assuming 604\n", value);
			}
			if (sscanf(line, "clock : %dMHz", &i) == 1)
				CPUClockSpeed = BusClockSpeed = i * 1000000;
		}
		fclose(proc_file);
	} else {
		sprintf(str, GetString(STR_PROC_CPUINFO_WARN), strerror(errno));
		WarningAlert(str);
	}
#endif
	D(bug("PVR: %08x (assumed)\n", PVR));

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

	// Create Low Memory area (0x0000..0x3000)
	if (vm_acquire_fixed((char *)0, 0x3000) < 0) {
		sprintf(str, GetString(STR_LOW_MEM_MMAP_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
	lm_area_mapped = true;

	// Create areas for Kernel Data
	kernel_area = shmget(IPC_PRIVATE, KERNEL_AREA_SIZE, 0600);
	if (kernel_area == -1) {
		sprintf(str, GetString(STR_KD_SHMGET_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
	if (shmat(kernel_area, (void *)KERNEL_DATA_BASE, 0) < 0) {
		sprintf(str, GetString(STR_KD_SHMAT_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
	if (shmat(kernel_area, (void *)KERNEL_DATA2_BASE, 0) < 0) {
		sprintf(str, GetString(STR_KD2_SHMAT_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
	kernel_data = (KernelData *)0x68ffe000;
	emulator_data = &kernel_data->ed;
	KernelDataAddr = (uint32)kernel_data;
	D(bug("Kernel Data at %p, Emulator Data at %p\n", kernel_data, emulator_data));

	// Create area for SheepShaver data
	if (vm_acquire_fixed((char *)SHEEP_BASE, SHEEP_SIZE) < 0) {
		sprintf(str, GetString(STR_SHEEP_MEM_MMAP_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
	SheepStack1Base = SHEEP_BASE + 0x10000;
	SheepStack2Base = SheepStack1Base + 0x10000;
	SheepThunksBase = SheepStack2Base + 0x1000;
	sheep_area_mapped = true;

	// Create area for Mac ROM
	if (vm_acquire_fixed((char *)ROM_BASE, ROM_AREA_SIZE) < 0) {
		sprintf(str, GetString(STR_ROM_MMAP_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
#if !EMULATED_PPC || defined(__powerpc__)
	if (vm_protect((char *)ROM_BASE, ROM_AREA_SIZE, VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_EXECUTE) < 0) {
		sprintf(str, GetString(STR_ROM_MMAP_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
#endif
	rom_area_mapped = true;
	D(bug("ROM area at %08x\n", ROM_BASE));

	// Create area for Mac RAM
	RAMSize = PrefsFindInt32("ramsize");
	if (RAMSize < 8*1024*1024) {
		WarningAlert(GetString(STR_SMALL_RAM_WARN));
		RAMSize = 8*1024*1024;
	}

	if (vm_acquire_fixed((char *)RAM_BASE, RAMSize) < 0) {
		sprintf(str, GetString(STR_RAM_MMAP_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
#if !EMULATED_PPC
	if (vm_protect((char *)RAM_BASE, RAMSize, VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_EXECUTE) < 0) {
		sprintf(str, GetString(STR_RAM_MMAP_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
#endif
	RAMBase = RAM_BASE;
	ram_area_mapped = true;
	D(bug("RAM area at %08x\n", RAMBase));

	if (RAMBase > ROM_BASE) {
		ErrorAlert(GetString(STR_RAM_HIGHER_THAN_ROM_ERR));
		goto quit;
	}

	// Load Mac ROM
	rom_path = PrefsFindString("rom");
	rom_fd = open(rom_path ? rom_path : ROM_FILE_NAME, O_RDONLY);
	if (rom_fd < 0) {
		rom_fd = open(rom_path ? rom_path : ROM_FILE_NAME2, O_RDONLY);
		if (rom_fd < 0) {
			ErrorAlert(GetString(STR_NO_ROM_FILE_ERR));
			goto quit;
		}
	}
	printf(GetString(STR_READING_ROM_FILE));
	rom_size = lseek(rom_fd, 0, SEEK_END);
	lseek(rom_fd, 0, SEEK_SET);
	rom_tmp = new uint8[ROM_SIZE];
	actual = read(rom_fd, (void *)rom_tmp, ROM_SIZE);
	close(rom_fd);
	
	// Decode Mac ROM
	if (!DecodeROM(rom_tmp, actual)) {
		if (rom_size != 4*1024*1024) {
			ErrorAlert(GetString(STR_ROM_SIZE_ERR));
			goto quit;
		} else {
			ErrorAlert(GetString(STR_ROM_FILE_READ_ERR));
			goto quit;
		}
	}
	delete[] rom_tmp;

	// Load NVRAM
	XPRAMInit();

	// Set boot volume
	i16 = PrefsFindInt32("bootdrive");
	XPRAM[0x1378] = i16 >> 8;
	XPRAM[0x1379] = i16 & 0xff;
	i16 = PrefsFindInt32("bootdriver");
	XPRAM[0x137a] = i16 >> 8;
	XPRAM[0x137b] = i16 & 0xff;

	// Create BootGlobs at top of Mac memory
	memset((void *)(RAMBase + RAMSize - 4096), 0, 4096);
	BootGlobsAddr = RAMBase + RAMSize - 0x1c;
	boot_globs = (uint32 *)BootGlobsAddr;
	boot_globs[-5] = htonl(RAMBase + RAMSize);	// MemTop
	boot_globs[0] = htonl(RAMBase);				// First RAM bank
	boot_globs[1] = htonl(RAMSize);
	boot_globs[2] = htonl((uint32)-1);			// End of bank table

	// Init drivers
	SonyInit();
	DiskInit();
	CDROMInit();
	SCSIInit();

	// Init external file system
	ExtFSInit(); 

	// Init audio
	AudioInit();

	// Init network
	EtherInit();

	// Init serial ports
	SerialInit();

	// Init Time Manager
	TimerInit();

	// Init clipboard
	ClipInit();

	// Init video
	if (!VideoInit())
		goto quit;

	// Install ROM patches
	if (!PatchROM()) {
		ErrorAlert(GetString(STR_UNSUPPORTED_ROM_TYPE_ERR));
		goto quit;
	}

	// Clear caches (as we loaded and patched code) and write protect ROM
#if !EMULATED_PPC
	MakeExecutable(0, (void *)ROM_BASE, ROM_AREA_SIZE);
#endif
	vm_protect((char *)ROM_BASE, ROM_AREA_SIZE, VM_PAGE_READ | VM_PAGE_EXECUTE);

	// Initialize Kernel Data
	memset(kernel_data, 0, sizeof(KernelData));
	if (ROMType == ROMTYPE_NEWWORLD) {
		static uint32 of_dev_tree[4] = {0, 0, 0, 0};
		static uint8 vector_lookup_tbl[128];
		static uint8 vector_mask_tbl[64];
		memset((uint8 *)kernel_data + 0xb80, 0x3d, 0x80);
		memset(vector_lookup_tbl, 0, 128);
		memset(vector_mask_tbl, 0, 64);
		kernel_data->v[0xb80 >> 2] = htonl(ROM_BASE);
		kernel_data->v[0xb84 >> 2] = htonl((uint32)of_dev_tree);	// OF device tree base
		kernel_data->v[0xb90 >> 2] = htonl((uint32)vector_lookup_tbl);
		kernel_data->v[0xb94 >> 2] = htonl((uint32)vector_mask_tbl);
		kernel_data->v[0xb98 >> 2] = htonl(ROM_BASE);				// OpenPIC base
		kernel_data->v[0xbb0 >> 2] = htonl(0);						// ADB base
		kernel_data->v[0xc20 >> 2] = htonl(RAMSize);
		kernel_data->v[0xc24 >> 2] = htonl(RAMSize);
		kernel_data->v[0xc30 >> 2] = htonl(RAMSize);
		kernel_data->v[0xc34 >> 2] = htonl(RAMSize);
		kernel_data->v[0xc38 >> 2] = htonl(0x00010020);
		kernel_data->v[0xc3c >> 2] = htonl(0x00200001);
		kernel_data->v[0xc40 >> 2] = htonl(0x00010000);
		kernel_data->v[0xc50 >> 2] = htonl(RAMBase);
		kernel_data->v[0xc54 >> 2] = htonl(RAMSize);
		kernel_data->v[0xf60 >> 2] = htonl(PVR);
		kernel_data->v[0xf64 >> 2] = htonl(CPUClockSpeed);
		kernel_data->v[0xf68 >> 2] = htonl(BusClockSpeed);
		kernel_data->v[0xf6c >> 2] = htonl(CPUClockSpeed);
	} else {
		kernel_data->v[0xc80 >> 2] = htonl(RAMSize);
		kernel_data->v[0xc84 >> 2] = htonl(RAMSize);
		kernel_data->v[0xc90 >> 2] = htonl(RAMSize);
		kernel_data->v[0xc94 >> 2] = htonl(RAMSize);
		kernel_data->v[0xc98 >> 2] = htonl(0x00010020);
		kernel_data->v[0xc9c >> 2] = htonl(0x00200001);
		kernel_data->v[0xca0 >> 2] = htonl(0x00010000);
		kernel_data->v[0xcb0 >> 2] = htonl(RAMBase);
		kernel_data->v[0xcb4 >> 2] = htonl(RAMSize);
		kernel_data->v[0xf80 >> 2] = htonl(PVR);
		kernel_data->v[0xf84 >> 2] = htonl(CPUClockSpeed);
		kernel_data->v[0xf88 >> 2] = htonl(BusClockSpeed);
		kernel_data->v[0xf8c >> 2] = htonl(CPUClockSpeed);
	}

	// Initialize extra low memory
	D(bug("Initializing Low Memory...\n"));
	memset(NULL, 0, 0x3000);
	WriteMacInt32(XLM_SIGNATURE, FOURCC('B','a','a','h'));			// Signature to detect SheepShaver
	WriteMacInt32(XLM_KERNEL_DATA, (uint32)kernel_data);			// For trap replacement routines
	WriteMacInt32(XLM_PVR, PVR);									// Theoretical PVR
	WriteMacInt32(XLM_BUS_CLOCK, BusClockSpeed);					// For DriverServicesLib patch
	WriteMacInt16(XLM_EXEC_RETURN_OPCODE, M68K_EXEC_RETURN);		// For Execute68k() (RTS from the executed 68k code will jump here and end 68k mode)
#if EMULATED_PPC
	WriteMacInt32(XLM_ETHER_INIT, POWERPC_NATIVE_OP_FUNC(NATIVE_ETHER_INIT));
	WriteMacInt32(XLM_ETHER_TERM, POWERPC_NATIVE_OP_FUNC(NATIVE_ETHER_TERM));
	WriteMacInt32(XLM_ETHER_OPEN, POWERPC_NATIVE_OP_FUNC(NATIVE_ETHER_OPEN));
	WriteMacInt32(XLM_ETHER_CLOSE, POWERPC_NATIVE_OP_FUNC(NATIVE_ETHER_CLOSE));
	WriteMacInt32(XLM_ETHER_WPUT, POWERPC_NATIVE_OP_FUNC(NATIVE_ETHER_WPUT));
	WriteMacInt32(XLM_ETHER_RSRV, POWERPC_NATIVE_OP_FUNC(NATIVE_ETHER_RSRV));
	WriteMacInt32(XLM_VIDEO_DOIO, POWERPC_NATIVE_OP_FUNC(NATIVE_VIDEO_DO_DRIVER_IO));
#else
	WriteMacInt32(XLM_TOC, (uint32)TOC);							// TOC pointer of emulator
	WriteMacInt32(XLM_ETHER_INIT, (uint32)InitStreamModule);		// DLPI ethernet driver functions
	WriteMacInt32(XLM_ETHER_TERM, (uint32)TerminateStreamModule);
	WriteMacInt32(XLM_ETHER_OPEN, (uint32)ether_open);
	WriteMacInt32(XLM_ETHER_CLOSE, (uint32)ether_close);
	WriteMacInt32(XLM_ETHER_WPUT, (uint32)ether_wput);
	WriteMacInt32(XLM_ETHER_RSRV, (uint32)ether_rsrv);
	WriteMacInt32(XLM_VIDEO_DOIO, (uint32)VideoDoDriverIO);
#endif
	D(bug("Low Memory initialized\n"));

	// Start 60Hz thread
	tick_thread_active = (pthread_create(&tick_thread, NULL, tick_func, NULL) == 0);
	D(bug("Tick thread installed (%ld)\n", tick_thread));

	// Start NVRAM watchdog thread
	memcpy(last_xpram, XPRAM, XPRAM_SIZE);
	nvram_thread_active = (pthread_create(&nvram_thread, NULL, nvram_func, NULL) == 0);
	D(bug("NVRAM thread installed (%ld)\n", nvram_thread));

#if !EMULATED_PPC
	// Create and install stacks for signal handlers
	sig_stack = malloc(SIG_STACK_SIZE);
	D(bug("Signal stack at %p\n", sig_stack));
	if (sig_stack == NULL) {
		ErrorAlert(GetString(STR_NOT_ENOUGH_MEMORY_ERR));
		goto quit;
	}
	extra_stack = malloc(SIG_STACK_SIZE);
	D(bug("Extra stack at %p\n", extra_stack));
	if (extra_stack == NULL) {
		ErrorAlert(GetString(STR_NOT_ENOUGH_MEMORY_ERR));
		goto quit;
	}
	struct sigaltstack new_stack;
	new_stack.ss_sp = sig_stack;
	new_stack.ss_flags = 0;
	new_stack.ss_size = SIG_STACK_SIZE;
	if (sigaltstack(&new_stack, NULL) < 0) {
		sprintf(str, GetString(STR_SIGALTSTACK_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
#endif

#if !EMULATED_PPC
	// Install SIGSEGV handler
	sigemptyset(&sigsegv_action.sa_mask);	// Block interrupts during SEGV handling
	sigaddset(&sigsegv_action.sa_mask, SIGUSR2);
	sigsegv_action.sa_handler = (__sighandler_t)sigsegv_handler;
	sigsegv_action.sa_flags = SA_ONSTACK;
	sigsegv_action.sa_restorer = NULL;
	if (sigaction(SIGSEGV, &sigsegv_action, NULL) < 0) {
		sprintf(str, GetString(STR_SIGSEGV_INSTALL_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}

	// Install SIGILL handler
	sigemptyset(&sigill_action.sa_mask);	// Block interrupts during ILL handling
	sigaddset(&sigill_action.sa_mask, SIGUSR2);
	sigill_action.sa_handler = (__sighandler_t)sigill_handler;
	sigill_action.sa_flags = SA_ONSTACK;
	sigill_action.sa_restorer = NULL;
	if (sigaction(SIGILL, &sigill_action, NULL) < 0) {
		sprintf(str, GetString(STR_SIGILL_INSTALL_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
#endif

	// Install interrupt signal handler
	sigemptyset(&sigusr2_action.sa_mask);
	sigusr2_action.sa_handler = (__sighandler_t)sigusr2_handler;
	sigusr2_action.sa_flags = 0;
#if !EMULATED_PPC
	sigusr2_action.sa_flags = SA_ONSTACK | SA_RESTART;
#endif
	sigusr2_action.sa_restorer = NULL;
	if (sigaction(SIGUSR2, &sigusr2_action, NULL) < 0) {
		sprintf(str, GetString(STR_SIGUSR2_INSTALL_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}

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
		pthread_cancel(tick_thread);
		pthread_join(tick_thread, NULL);
	}

	// Stop NVRAM watchdog thread
	if (nvram_thread_active) {
		pthread_cancel(nvram_thread);
		pthread_join(nvram_thread, NULL);
	}

#if !EMULATED_PPC
	// Uninstall SIGSEGV handler
	sigemptyset(&sigsegv_action.sa_mask);
	sigsegv_action.sa_handler = SIG_DFL;
	sigsegv_action.sa_flags = 0;
	sigaction(SIGSEGV, &sigsegv_action, NULL);

	// Uninstall SIGILL handler
	sigemptyset(&sigill_action.sa_mask);
	sigill_action.sa_handler = SIG_DFL;
	sigill_action.sa_flags = 0;
	sigaction(SIGILL, &sigill_action, NULL);
#endif

	// Save NVRAM
	XPRAMExit();

	// Exit clipboard
	ClipExit();

	// Exit Time Manager
	TimerExit();

	// Exit serial
	SerialExit();

	// Exit network
	EtherExit();

	// Exit audio
	AudioExit();

	// Exit video
	VideoExit();

	// Exit external file system
	ExtFSExit();

	// Exit drivers
	SCSIExit();
	CDROMExit();
	DiskExit();
	SonyExit();

	// Delete RAM area
	if (ram_area_mapped)
		vm_release((char *)RAM_BASE, RAMSize);

	// Delete ROM area
	if (rom_area_mapped)
		vm_release((char *)ROM_BASE, ROM_AREA_SIZE);

	// Delete Kernel Data area
	if (kernel_area >= 0) {
		shmdt((void *)KERNEL_DATA_BASE);
		shmdt((void *)KERNEL_DATA2_BASE);
		shmctl(kernel_area, IPC_RMID, NULL);
	}

	// Delete Low Memory area
	if (lm_area_mapped)
		munmap((char *)0x0000, 0x3000);

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
	if (x_display)
		XCloseDisplay(x_display);

	exit(0);
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
	jump_to_rom(ROM_BASE + 0x310000);
#else
	jump_to_rom(ROM_BASE + 0x310000, (uint32)emulator_data);
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


/*
 *  Execute PPC code from EMUL_OP routine (real mode switch)
 */

void ExecutePPC(void (*func)())
{
	uint32 tvect[2] = {(uint32)func, 0};	// Fake TVECT
	RoutineDescriptor desc = BUILD_PPC_ROUTINE_DESCRIPTOR(0, tvect);
	M68kRegisters r;
	Execute68k((uint32)&desc, &r);
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
 *  Pause/resume emulator
 */

void PauseEmulator(void)
{
	pthread_kill(emul_thread, SIGSTOP);
}

void ResumeEmulator(void)
{
	pthread_kill(emul_thread, SIGCONT);
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

void MakeExecutable(int dummy, void *start, uint32 length)
{
	if (((uintptr)start >= ROM_BASE) && ((uintptr)start < (ROM_BASE + ROM_SIZE)))
		return;
#if EMULATED_PPC
	FlushCodeCache((uintptr)start, (uintptr)start + length);
#else
	flush_icache_range(start, (void *)((uintptr)start + length));
#endif
}


/*
 *  Patch things after system startup (gets called by disk driver accRun routine)
 */

void PatchAfterStartup(void)
{
#if EMULATED_PPC
	ExecuteNative(NATIVE_VIDEO_INSTALL_ACCEL);
#else
	ExecutePPC(VideoInstallAccel);
#endif
	InstallExtFS();
}


/*
 *  NVRAM watchdog thread (saves NVRAM every minute)
 */

static void *nvram_func(void *arg)
{
	struct timespec req = {60, 0};	// 1 minute

	for (;;) {
		pthread_testcancel();
		nanosleep(&req, NULL);
		pthread_testcancel();
		if (memcmp(last_xpram, XPRAM, XPRAM_SIZE)) {
			memcpy(last_xpram, XPRAM, XPRAM_SIZE);
			SaveXPRAM();
		}
	}
	return NULL;
}


/*
 *  60Hz thread (really 60.15Hz)
 */

static void *tick_func(void *arg)
{
	int tick_counter = 0;
	struct timespec req = {0, 16625000};

	for (;;) {

		// Wait
		nanosleep(&req, NULL);

#if !EMULATED_PPC
		// Did we crash?
		if (emul_thread_fatal) {

			// Yes, dump registers
			pt_regs *r = (pt_regs *)&sigsegv_regs;
			char str[256];
			sprintf(str, "SIGSEGV\n"
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
			char *arg[4] = {"mon", "-m", "-r", NULL};
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
	return NULL;
}


/*
 *  Pthread configuration
 */

void Set_pthread_attr(pthread_attr_t *attr, int priority)
{
	// nothing to do
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

#if !EMULATED_PPC || ASYNC_IRQ
void TriggerInterrupt(void)
{
	if (ready_for_signals)
		pthread_kill(emul_thread, SIGUSR2);
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
	atomic_add((int *)XLM_IRQ_NEST, 1);
}


/*
 *  Enable interrupts
 */

void EnableInterrupt(void)
{
	atomic_add((int *)XLM_IRQ_NEST, -1);
}


/*
 *  USR2 handler
 */

#if EMULATED_PPC
static void sigusr2_handler(int sig)
{
#if ASYNC_IRQ
	extern void HandleInterrupt(void);
	HandleInterrupt();
#endif
}
#else
static void sigusr2_handler(int sig, sigcontext_struct *sc)
{
	pt_regs *r = sc->regs;

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
			r->ccr |= ntohl(kernel_data->v[0x674 >> 2]);
			break;

#if INTERRUPTS_IN_NATIVE_MODE
		case MODE_NATIVE:
			// 68k emulator inactive, in nanokernel?
			if (r->gpr[1] != KernelDataAddr) {
				// Prepare for 68k interrupt level 1
				WriteMacInt16(ntohl(kernel_data->v[0x67c >> 2]), 1);
				WriteMacInt32(ntohl(kernel_data->v[0x658 >> 2]) + 0xdc, ReadMacInt32(ntohl(kernel_data->v[0x658 >> 2]) + 0xdc) | ntohl(kernel_data->v[0x674 >> 2]));

				// Execute nanokernel interrupt routine (this will activate the 68k emulator)
				atomic_add((int32 *)XLM_IRQ_NEST, 1);
				if (ROMType == ROMTYPE_NEWWORLD)
					ppc_interrupt(ROM_BASE + 0x312b1c, KernelDataAddr);
				else
					ppc_interrupt(ROM_BASE + 0x312a3c, KernelDataAddr);
			}
			break;
#endif

#if INTERRUPTS_IN_EMUL_OP_MODE
		case MODE_EMUL_OP:
			// 68k emulator active, within EMUL_OP routine, execute 68k interrupt routine directly when interrupt level is 0
			if ((ReadMacInt32(XLM_68K_R25) & 7) == 0) {

				// Set extra stack for SIGSEGV handler
				struct sigaltstack new_stack;
				new_stack.ss_sp = extra_stack;
				new_stack.ss_flags = 0;
				new_stack.ss_size = SIG_STACK_SIZE;
				sigaltstack(&new_stack, NULL);
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
						ExecutePPC(VideoVBL);
					}
				}
#endif
				// Reset normal signal stack
				new_stack.ss_sp = sig_stack;
				new_stack.ss_flags = 0;
				new_stack.ss_size = SIG_STACK_SIZE;
				sigaltstack(&new_stack, NULL);
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
static void sigsegv_handler(int sig, sigcontext_struct *sc)
{
	pt_regs *r = sc->regs;

	// Get effective address
	uint32 addr = r->dar;
	
#if ENABLE_VOSF
	// Handle screen fault.
	extern bool Screen_fault_handler(sigsegv_address_t fault_address, sigsegv_address_t fault_instruction);
	if (Screen_fault_handler((sigsegv_address_t)addr, (sigsegv_address_t)r->nip))
		return;
#endif

	num_segv++;

	// Fault in Mac ROM or RAM?
	bool mac_fault = (r->nip >= ROM_BASE) && (r->nip < (ROM_BASE + ROM_AREA_SIZE)) || (r->nip >= RAMBase) && (r->nip < (RAMBase + RAMSize));
	if (mac_fault) {

		// "VM settings" during MacOS 8 installation
		if (r->nip == ROM_BASE + 0x488160 && r->gpr[20] == 0xf8000000) {
			r->nip += 4;
			r->gpr[8] = 0;
			return;
	
		// MacOS 8.5 installation
		} else if (r->nip == ROM_BASE + 0x488140 && r->gpr[16] == 0xf8000000) {
			r->nip += 4;
			r->gpr[8] = 0;
			return;
	
		// MacOS 8 serial drivers on startup
		} else if (r->nip == ROM_BASE + 0x48e080 && (r->gpr[8] == 0xf3012002 || r->gpr[8] == 0xf3012000)) {
			r->nip += 4;
			r->gpr[8] = 0;
			return;
	
		// MacOS 8.1 serial drivers on startup
		} else if (r->nip == ROM_BASE + 0x48c5e0 && (r->gpr[20] == 0xf3012002 || r->gpr[20] == 0xf3012000)) {
			r->nip += 4;
			return;
		} else if (r->nip == ROM_BASE + 0x4a10a0 && (r->gpr[20] == 0xf3012002 || r->gpr[20] == 0xf3012000)) {
			r->nip += 4;
			return;
		}

		// Get opcode and divide into fields
		uint32 opcode = *((uint32 *)r->nip);
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
		}
	
		// Ignore ROM writes
		if (transfer_type == TYPE_STORE && addr >= ROM_BASE && addr < ROM_BASE + ROM_SIZE) {
//			D(bug("WARNING: %s write access to ROM at %08lx, pc %08lx\n", transfer_size == SIZE_BYTE ? "Byte" : transfer_size == SIZE_HALFWORD ? "Halfword" : "Word", addr, r->nip));
			if (addr_mode == MODE_U || addr_mode == MODE_UX)
				r->gpr[ra] = addr;
			r->nip += 4;
			goto rti;
		}

		// Ignore illegal memory accesses?
		if (PrefsFindBool("ignoresegv")) {
			if (addr_mode == MODE_U || addr_mode == MODE_UX)
				r->gpr[ra] = addr;
			if (transfer_type == TYPE_LOAD)
				r->gpr[rd] = 0;
			r->nip += 4;
			goto rti;
		}

		// In GUI mode, show error alert
		if (!PrefsFindBool("nogui")) {
			char str[256];
			if (transfer_type == TYPE_LOAD || transfer_type == TYPE_STORE)
				sprintf(str, GetString(STR_MEM_ACCESS_ERR), transfer_size == SIZE_BYTE ? "byte" : transfer_size == SIZE_HALFWORD ? "halfword" : "word", transfer_type == TYPE_LOAD ? GetString(STR_MEM_ACCESS_READ) : GetString(STR_MEM_ACCESS_WRITE), addr, r->nip, r->gpr[24], r->gpr[1]);
			else
				sprintf(str, GetString(STR_UNKNOWN_SEGV_ERR), r->nip, r->gpr[24], r->gpr[1], opcode);
			ErrorAlert(str);
			QuitEmulator();
			return;
		}
	}

	// For all other errors, jump into debugger (sort of...)
	if (!ready_for_signals) {
		printf("SIGSEGV\n");
		printf(" sigcontext %p, pt_regs %p\n", sc, r);
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
		exit(1);
		QuitEmulator();
		return;
	} else {
		// We crashed. Save registers, tell tick thread and loop forever
		sigsegv_regs = *(sigregs *)r;
		emul_thread_fatal = true;
		for (;;) ;
	}
rti:;
}


/*
 *  SIGILL handler
 */

static void sigill_handler(int sig, sigcontext_struct *sc)
{
	pt_regs *r = sc->regs;
	char str[256];

	// Fault in Mac ROM or RAM?
	bool mac_fault = (r->nip >= ROM_BASE) && (r->nip < (ROM_BASE + ROM_AREA_SIZE)) || (r->nip >= RAMBase) && (r->nip < (RAMBase + RAMSize));
	if (mac_fault) {

		// Get opcode and divide into fields
		uint32 opcode = *((uint32 *)r->nip);
		uint32 primop = opcode >> 26;
		uint32 exop = (opcode >> 1) & 0x3ff;
		uint32 ra = (opcode >> 16) & 0x1f;
		uint32 rb = (opcode >> 11) & 0x1f;
		uint32 rd = (opcode >> 21) & 0x1f;
		int32 imm = (int16)(opcode & 0xffff);

		switch (primop) {
			case 9:		// POWER instructions
			case 22:
power_inst:		sprintf(str, GetString(STR_POWER_INSTRUCTION_ERR), r->nip, r->gpr[1], opcode);
				ErrorAlert(str);
				QuitEmulator();
				return;

			case 31:
				switch (exop) {
					case 83:	// mfmsr
						r->gpr[rd] = 0xf072;
						r->nip += 4;
						goto rti;

					case 210:	// mtsr
					case 242:	// mtsrin
					case 306:	// tlbie
						r->nip += 4;
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
								r->nip += 4;
								goto rti;
							case 25:	// SDR1
								r->gpr[rd] = 0xdead001f;
								r->nip += 4;
								goto rti;
							case 287:	// PVR
								r->gpr[rd] = PVR;
								r->nip += 4;
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
								r->nip += 4;
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
			sprintf(str, GetString(STR_UNKNOWN_SEGV_ERR), r->nip, r->gpr[24], r->gpr[1], opcode);
			ErrorAlert(str);
			QuitEmulator();
			return;
		}
	}

	// For all other errors, jump into debugger (sort of...)
	if (!ready_for_signals) {
		printf("SIGILL\n");
		printf(" sigcontext %p, pt_regs %p\n", sc, r);
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
		exit(1);
		QuitEmulator();
		return;
	} else {
		// We crashed. Save registers, tell tick thread and loop forever
		sigsegv_regs = *(sigregs *)r;
		emul_thread_fatal = true;
		for (;;) ;
	}
rti:;
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
