/*
 *  main_windows.cpp - Emulation core, Windows implementation
 *
 *  SheepShaver (C) 1997-2004 Christian Bauer and Marc Hellwig
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

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
#include "thunks.h"
#include "util_windows.h"

#define DEBUG 0
#include "debug.h"

#ifdef ENABLE_MON
#include "mon.h"
#endif


// Constants
const char ROM_FILE_NAME[] = "ROM";
const char ROM_FILE_NAME2[] = "Mac OS ROM";

const uintptr RAM_BASE = 0x10000000;		// Base address of RAM
const uint32 SIG_STACK_SIZE = 0x10000;		// Size of signal stack


// Global variables (exported)
uint32 RAMBase;			// Base address of Mac RAM
uint32 RAMSize;			// Size of Mac RAM
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
static bool lm_area_mapped = false;			// Flag: Low Memory area mmap()ped
static int kernel_area = -1;				// SHM ID of Kernel Data area
static bool rom_area_mapped = false;		// Flag: Mac ROM mmap()ped
static bool ram_area_mapped = false;		// Flag: Mac RAM mmap()ped
static bool dr_cache_area_mapped = false;	// Flag: Mac DR Cache mmap()ped
static bool dr_emulator_area_mapped = false;// Flag: Mac DR Emulator mmap()ped
static KernelData *kernel_data;				// Pointer to Kernel Data
static EmulatorData *emulator_data;

static uint8 last_xpram[XPRAM_SIZE];		// Buffer for monitoring XPRAM changes
static bool nvram_thread_active = false;	// Flag: NVRAM watchdog installed
static volatile bool nvram_thread_cancel;	// Flag: Cancel NVRAM thread
static HANDLE nvram_thread = NULL;			// NVRAM watchdog
static bool tick_thread_active = false;		// Flag: MacOS thread installed
static volatile bool tick_thread_cancel;	// Flag: Cancel 60Hz thread
static HANDLE tick_thread = NULL;			// 60Hz thread
static HANDLE emul_thread = NULL;			// MacOS thread
static uintptr sig_stack = 0;				// Stack for PowerPC interrupt routine

uint32  SheepMem::page_size;				// Size of a native page
uintptr SheepMem::zero_page = 0;			// Address of ro page filled in with zeros
uintptr SheepMem::base = 0x60000000;		// Address of SheepShaver data
uintptr SheepMem::proc;						// Bottom address of SheepShave procedures
uintptr SheepMem::data;						// Top of SheepShaver data (stack like storage)


// Prototypes
static bool kernel_data_init(void);
static void kernel_data_exit(void);
static void Quit(void);
static DWORD WINAPI nvram_func(void *arg);
static DWORD WINAPI tick_func(void *arg);

static void jump_to_rom(uint32 entry);
extern void emul_ppc(uint32 start);
extern void init_emul_ppc(void);
extern void exit_emul_ppc(void);
sigsegv_return_t sigsegv_handler(sigsegv_address_t, sigsegv_address_t);


/*
 *  Return signal stack base
 */

uintptr SignalStackBase(void)
{
	return sig_stack + SIG_STACK_SIZE;
}


/*
 *  Memory management helpers
 */

static inline int vm_mac_acquire(uint32 addr, uint32 size)
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

int main(int argc, char **argv)
{
	char str[256];
	int16 i16;
	HANDLE rom_fh;
	const char *rom_path;
	uint32 rom_size;
	DWORD actual;
	uint8 *rom_tmp;

	// Initialize variables
	RAMBase = 0;
	tzset();

	// Print some info
	printf(GetString(STR_ABOUT_TEXT1), VERSION_MAJOR, VERSION_MINOR);
	printf(" %s\n", GetString(STR_ABOUT_TEXT2));

	// Read preferences
	PrefsInit(argc, argv);

	// Parse command line arguments
	for (int i=1; i<argc; i++) {
		if (strcmp(argv[i], "--help") == 0) {
			usage(argv[0]);
		} else if (argv[i][0] == '-') {
			fprintf(stderr, "Unrecognized option '%s'\n", argv[i]);
			usage(argv[0]);
		}
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
		goto quit;
	}
	atexit(SDL_Quit);

#ifdef ENABLE_MON
	// Initialize mon
	mon_init();
#endif

	// Install SIGSEGV handler for CPU emulator
	if (!sigsegv_install_handler(sigsegv_handler)) {
		sprintf(str, GetString(STR_SIGSEGV_INSTALL_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}

	// Initialize VM system
	vm_init();

	// Get system info
	PVR = 0x00040000;			// Default: 604
	CPUClockSpeed = 100000000;	// Default: 100MHz
	BusClockSpeed = 100000000;	// Default: 100MHz
	TimebaseSpeed =  25000000;	// Default:  25MHz
	PVR = 0x000c0000;			// Default: 7400 (with AltiVec)
	D(bug("PVR: %08x (assumed)\n", PVR));

	// Init system routines
	SysInit();

	// Show preferences editor
	if (!PrefsFindBool("nogui"))
		if (!PrefsEditor())
			goto quit;

	// Create Low Memory area (0x0000..0x3000)
	if (vm_mac_acquire(0, 0x3000) < 0) {
		sprintf(str, GetString(STR_LOW_MEM_MMAP_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
	lm_area_mapped = true;

	// Create areas for Kernel Data
	if (!kernel_data_init())
		goto quit;
	kernel_data = (KernelData *)Mac2HostAddr(KERNEL_DATA_BASE);
	emulator_data = &kernel_data->ed;
	KernelDataAddr = KERNEL_DATA_BASE;
	D(bug("Kernel Data at %p (%08x)\n", kernel_data, KERNEL_DATA_BASE));
	D(bug("Emulator Data at %p (%08x)\n", emulator_data, KERNEL_DATA_BASE + offsetof(KernelData, ed)));

	// Create area for DR Cache
	if (vm_mac_acquire(DR_EMULATOR_BASE, DR_EMULATOR_SIZE) < 0) {
		sprintf(str, GetString(STR_DR_EMULATOR_MMAP_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
	dr_emulator_area_mapped = true;
	if (vm_mac_acquire(DR_CACHE_BASE, DR_CACHE_SIZE) < 0) {
		sprintf(str, GetString(STR_DR_CACHE_MMAP_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
	dr_cache_area_mapped = true;
	DRCacheAddr = (uint32)Mac2HostAddr(DR_CACHE_BASE);
	D(bug("DR Cache at %p (%08x)\n", DRCacheAddr, DR_CACHE_BASE));

	// Create area for SheepShaver data
	if (!SheepMem::Init()) {
		sprintf(str, GetString(STR_SHEEP_MEM_MMAP_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}

	// Create area for Mac ROM
	if (vm_mac_acquire(ROM_BASE, ROM_AREA_SIZE) < 0) {
		sprintf(str, GetString(STR_ROM_MMAP_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
	ROMBaseHost = Mac2HostAddr(ROM_BASE);
	rom_area_mapped = true;
	D(bug("ROM area at %p (%08x)\n", ROMBaseHost, ROM_BASE));

	// Create area for Mac RAM
	RAMSize = PrefsFindInt32("ramsize");
	if (RAMSize < 8*1024*1024) {
		WarningAlert(GetString(STR_SMALL_RAM_WARN));
		RAMSize = 8*1024*1024;
	}

	if (vm_mac_acquire(RAM_BASE, RAMSize) < 0) {
		sprintf(str, GetString(STR_RAM_MMAP_ERR), strerror(errno));
		ErrorAlert(str);
		goto quit;
	}
	RAMBaseHost = Mac2HostAddr(RAM_BASE);
	RAMBase = RAM_BASE;
	ram_area_mapped = true;
	D(bug("RAM area at %p (%08x)\n", RAMBaseHost, RAMBase));

	if (RAMBase > ROM_BASE) {
		ErrorAlert(GetString(STR_RAM_HIGHER_THAN_ROM_ERR));
		goto quit;
	}

	// Load Mac ROM
	rom_path = PrefsFindString("rom");
	rom_fh = CreateFile(rom_path ? rom_path : ROM_FILE_NAME,
						GENERIC_READ, 0, NULL, OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL, NULL);

	if (rom_fh == INVALID_HANDLE_VALUE) {
		rom_fh = CreateFile(rom_path ? rom_path : ROM_FILE_NAME2,
							GENERIC_READ, 0, NULL, OPEN_EXISTING,
							FILE_ATTRIBUTE_NORMAL, NULL);

		if (rom_fh == INVALID_HANDLE_VALUE) {
			ErrorAlert(GetString(STR_NO_ROM_FILE_ERR));
			goto quit;
		}
	}
	printf(GetString(STR_READING_ROM_FILE));
	rom_size = GetFileSize(rom_fh, NULL);
	rom_tmp = new uint8[ROM_SIZE];
	ReadFile(rom_fh, (void *)rom_tmp, ROM_SIZE, &actual, NULL);
	CloseHandle(rom_fh);
	
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

	// Load XPRAM default values if signature not found
	if (XPRAM[0x130c] != 0x4e || XPRAM[0x130d] != 0x75
	 || XPRAM[0x130e] != 0x4d || XPRAM[0x130f] != 0x63) {
		D(bug("Loading XPRAM default values\n"));
		memset(XPRAM + 0x1300, 0, 0x100);
		XPRAM[0x130c] = 0x4e;	// "NuMc" signature
		XPRAM[0x130d] = 0x75;
		XPRAM[0x130e] = 0x4d;
		XPRAM[0x130f] = 0x63;
		XPRAM[0x1301] = 0x80;	// InternalWaitFlags = DynWait (don't wait for SCSI devices upon bootup)
		XPRAM[0x1310] = 0xa8;	// Standard PRAM values
		XPRAM[0x1311] = 0x00;
		XPRAM[0x1312] = 0x00;
		XPRAM[0x1313] = 0x22;
		XPRAM[0x1314] = 0xcc;
		XPRAM[0x1315] = 0x0a;
		XPRAM[0x1316] = 0xcc;
		XPRAM[0x1317] = 0x0a;
		XPRAM[0x131c] = 0x00;
		XPRAM[0x131d] = 0x02;
		XPRAM[0x131e] = 0x63;
		XPRAM[0x131f] = 0x00;
		XPRAM[0x1308] = 0x13;
		XPRAM[0x1309] = 0x88;
		XPRAM[0x130a] = 0x00;
		XPRAM[0x130b] = 0xcc;
		XPRAM[0x1376] = 0x00;	// OSDefault = MacOS
		XPRAM[0x1377] = 0x01;
	}

	// Set boot volume
	i16 = PrefsFindInt32("bootdrive");
	XPRAM[0x1378] = i16 >> 8;
	XPRAM[0x1379] = i16 & 0xff;
	i16 = PrefsFindInt32("bootdriver");
	XPRAM[0x137a] = i16 >> 8;
	XPRAM[0x137b] = i16 & 0xff;

	// Create BootGlobs at top of Mac memory
	memset(RAMBaseHost + RAMSize - 4096, 0, 4096);
	BootGlobsAddr = RAMBase + RAMSize - 0x1c;
	WriteMacInt32(BootGlobsAddr - 5 * 4, RAMBase + RAMSize);	// MemTop
	WriteMacInt32(BootGlobsAddr + 0 * 4, RAMBase);				// First RAM bank
	WriteMacInt32(BootGlobsAddr + 1 * 4, RAMSize);
	WriteMacInt32(BootGlobsAddr + 2 * 4, (uint32)-1);			// End of bank table

	// Init thunks
	if (!ThunksInit())
		goto quit;

	// Init drivers
	SonyInit();
	DiskInit();
	CDROMInit();
	SCSIInit();

	// Init external file system
	ExtFSInit(); 

	// Init ADB
	ADBInit();

	// Init audio
	AudioInit();

	// Init network
	EtherInit();

	// Init serial ports
	SerialInit();

	// Init Time Manager
	timer_init();

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

	// Write protect ROM
	vm_protect(ROMBaseHost, ROM_AREA_SIZE, VM_PAGE_READ | VM_PAGE_EXECUTE);

	// Initialize Kernel Data
	memset(kernel_data, 0, sizeof(KernelData));
	if (ROMType == ROMTYPE_NEWWORLD) {
		uint32 of_dev_tree = SheepMem::Reserve(4 * sizeof(uint32));
		Mac_memset(of_dev_tree, 0, 4 * sizeof(uint32));
		uint32 vector_lookup_tbl = SheepMem::Reserve(128);
		uint32 vector_mask_tbl = SheepMem::Reserve(64);
		memset((uint8 *)kernel_data + 0xb80, 0x3d, 0x80);
		Mac_memset(vector_lookup_tbl, 0, 128);
		Mac_memset(vector_mask_tbl, 0, 64);
		kernel_data->v[0xb80 >> 2] = htonl(ROM_BASE);
		kernel_data->v[0xb84 >> 2] = htonl(of_dev_tree);			// OF device tree base
		kernel_data->v[0xb90 >> 2] = htonl(vector_lookup_tbl);
		kernel_data->v[0xb94 >> 2] = htonl(vector_mask_tbl);
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
		kernel_data->v[0xf64 >> 2] = htonl(CPUClockSpeed);			// clock-frequency
		kernel_data->v[0xf68 >> 2] = htonl(BusClockSpeed);			// bus-frequency
		kernel_data->v[0xf6c >> 2] = htonl(TimebaseSpeed);			// timebase-frequency
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
		kernel_data->v[0xf84 >> 2] = htonl(CPUClockSpeed);			// clock-frequency
		kernel_data->v[0xf88 >> 2] = htonl(BusClockSpeed);			// bus-frequency
		kernel_data->v[0xf8c >> 2] = htonl(TimebaseSpeed);			// timebase-frequency
	}

	// Initialize extra low memory
	D(bug("Initializing Low Memory...\n"));
	Mac_memset(0, 0, 0x3000);
	WriteMacInt32(XLM_SIGNATURE, FOURCC('B','a','a','h'));			// Signature to detect SheepShaver
	WriteMacInt32(XLM_KERNEL_DATA, KernelDataAddr);					// For trap replacement routines
	WriteMacInt32(XLM_PVR, PVR);									// Theoretical PVR
	WriteMacInt32(XLM_BUS_CLOCK, BusClockSpeed);					// For DriverServicesLib patch
	WriteMacInt16(XLM_EXEC_RETURN_OPCODE, M68K_EXEC_RETURN);		// For Execute68k() (RTS from the executed 68k code will jump here and end 68k mode)
	WriteMacInt32(XLM_ZERO_PAGE, SheepMem::ZeroPage());				// Pointer to read-only page with all bits set to 0
	WriteMacInt32(XLM_ETHER_INIT, NativeFunction(NATIVE_ETHER_INIT));	// DLPI ethernet driver functions
	WriteMacInt32(XLM_ETHER_TERM, NativeFunction(NATIVE_ETHER_TERM));
	WriteMacInt32(XLM_ETHER_OPEN, NativeFunction(NATIVE_ETHER_OPEN));
	WriteMacInt32(XLM_ETHER_CLOSE, NativeFunction(NATIVE_ETHER_CLOSE));
	WriteMacInt32(XLM_ETHER_WPUT, NativeFunction(NATIVE_ETHER_WPUT));
	WriteMacInt32(XLM_ETHER_RSRV, NativeFunction(NATIVE_ETHER_RSRV));
	WriteMacInt32(XLM_VIDEO_DOIO, NativeFunction(NATIVE_VIDEO_DO_DRIVER_IO));
	D(bug("Low Memory initialized\n"));

	// Start 60Hz thread
	tick_thread_cancel = false;
	tick_thread_active = ((tick_thread = create_thread(tick_func)) != NULL);
	SetThreadPriority(tick_thread, THREAD_PRIORITY_ABOVE_NORMAL);
	D(bug("Tick thread installed (%ld)\n", tick_thread));

	// Start NVRAM watchdog thread
	memcpy(last_xpram, XPRAM, XPRAM_SIZE);
	nvram_thread_cancel = false;
	nvram_thread_active = ((nvram_thread = create_thread(nvram_func, NULL)) != NULL);
	SetThreadPriority(nvram_thread, THREAD_PRIORITY_BELOW_NORMAL);
	D(bug("NVRAM thread installed (%ld)\n", nvram_thread));

	// Get my thread ID and jump to ROM boot routine
	emul_thread = GetCurrentThread();
	D(bug("Jumping to ROM\n"));
	jump_to_rom(ROM_BASE + 0x310000);
	D(bug("Returned from ROM\n"));

quit:
	Quit();
	return 0;
}


/*
 *  Cleanup and quit
 */

static void Quit(void)
{
	// Exit PowerPC emulation
	exit_emul_ppc();

	// Stop 60Hz thread
	if (tick_thread_active) {
		tick_thread_cancel = true;
		wait_thread(tick_thread);
	}

	// Stop NVRAM watchdog thread
	if (nvram_thread_active) {
		nvram_thread_cancel = true;
		wait_thread(nvram_thread);
	}

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

	// Exit ADB
	ADBExit();

	// Exit video
	VideoExit();

	// Exit external file system
	ExtFSExit();

	// Exit drivers
	SCSIExit();
	CDROMExit();
	DiskExit();
	SonyExit();

	// Delete thunks
	ThunksExit();

	// Delete SheepShaver globals
	SheepMem::Exit();

	// Delete RAM area
	if (ram_area_mapped)
		vm_mac_release(RAM_BASE, RAMSize);

	// Delete ROM area
	if (rom_area_mapped)
		vm_mac_release(ROM_BASE, ROM_AREA_SIZE);

	// Delete DR cache areas
	if (dr_emulator_area_mapped)
		vm_mac_release(DR_EMULATOR_BASE, DR_EMULATOR_SIZE);
	if (dr_cache_area_mapped)
		vm_mac_release(DR_CACHE_BASE, DR_CACHE_SIZE);

	// Delete Kernel Data area
	kernel_data_exit();

	// Delete Low Memory area
	if (lm_area_mapped)
		vm_mac_release(0, 0x3000);

	// Exit system routines
	SysExit();

	// Exit preferences
	PrefsExit();

#ifdef ENABLE_MON
	// Exit mon
	mon_exit();
#endif

	exit(0);
}


/*
 *  Initialize Kernel Data segments
 */

static HANDLE kernel_handle;				// Shared memory handle for Kernel Data
static DWORD allocation_granule;			// Minimum size of allocateable are (64K)
static DWORD kernel_area_size;				// Size of Kernel Data area

static bool kernel_data_init(void)
{
	char str[256];
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	allocation_granule = si.dwAllocationGranularity;
	kernel_area_size = (KERNEL_AREA_SIZE + allocation_granule - 1) & -allocation_granule;

	char rcs[10];
	LPVOID kernel_addr;
	kernel_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, kernel_area_size, NULL);
	if (kernel_handle == NULL) {
		sprintf(rcs, "%d", GetLastError());
		sprintf(str, GetString(STR_KD_SHMGET_ERR), rcs);
		ErrorAlert(str);
		return false;
	}
	kernel_addr = (LPVOID)Mac2HostAddr(KERNEL_DATA_BASE & -allocation_granule);
	if (MapViewOfFileEx(kernel_handle, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, kernel_area_size, kernel_addr) != kernel_addr) {
		sprintf(rcs, "%d", GetLastError());
		sprintf(str, GetString(STR_KD_SHMAT_ERR), rcs);
		ErrorAlert(str);
		return false;
	}
	kernel_addr = (LPVOID)Mac2HostAddr(KERNEL_DATA2_BASE & -allocation_granule);
	if (MapViewOfFileEx(kernel_handle, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, kernel_area_size, kernel_addr) != kernel_addr) {
		sprintf(rcs, "%d", GetLastError());
		sprintf(str, GetString(STR_KD2_SHMAT_ERR), rcs);
		ErrorAlert(str);
		return false;
	}
	return true;
}


/*
 *  Deallocate Kernel Data segments
 */

static void kernel_data_exit(void)
{
	if (kernel_handle) {
		UnmapViewOfFile(Mac2HostAddr(KERNEL_DATA_BASE & -allocation_granule));
		UnmapViewOfFile(Mac2HostAddr(KERNEL_DATA2_BASE & -allocation_granule));
		CloseHandle(kernel_handle);
	}
}


/*
 *  Jump into Mac ROM, start 680x0 emulator
 */

void jump_to_rom(uint32 entry)
{
	init_emul_ppc();
	emul_ppc(entry);
}


/*
 *  Quit emulator (cause return from jump_to_rom)
 */

void QuitEmulator(void)
{
	Quit();
}


/*
 *  Pause/resume emulator
 */

void PauseEmulator(void)
{
	SuspendThread(emul_thread);
}

void ResumeEmulator(void)
{
	ResumeThread(emul_thread);
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
	if ((start >= ROM_BASE) && (start < (ROM_BASE + ROM_SIZE)))
		return;
	FlushCodeCache(start, start + length);
}


/*
 *  Patch things after system startup (gets called by disk driver accRun routine)
 */

void PatchAfterStartup(void)
{
	ExecuteNative(NATIVE_VIDEO_INSTALL_ACCEL);
	InstallExtFS();
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

static DWORD nvram_func(void *arg)
{
	while (!nvram_thread_cancel) {
		for (int i=0; i<60 && !nvram_thread_cancel; i++)
			Delay_usec(999999);		// Only wait 1 second so we quit promptly when nvram_thread_cancel becomes true
		nvram_watchdog();
	}
	return 0;
}


/*
 *  60Hz thread (really 60.15Hz)
 */

static DWORD tick_func(void *arg)
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
	D(bug("%Ld ticks in %Ld usec = %f ticks/sec\n", ticks, end - start, ticks * 1000000.0 / (end - start)));
	return 0;
}


/*
 *  Mutexes
 */

struct B2_mutex {
	mutex_t m;
};

B2_mutex *B2_create_mutex(void)
{
	return new B2_mutex;
}

void B2_lock_mutex(B2_mutex *mutex)
{
	mutex->m.lock();
}

void B2_unlock_mutex(B2_mutex *mutex)
{
	mutex->m.unlock();
}

void B2_delete_mutex(B2_mutex *mutex)
{
	delete mutex;
}


/*
 *  Interrupt flags (must be handled atomically!)
 */

volatile uint32 InterruptFlags = 0;
static mutex_t intflags_mutex;

void SetInterruptFlag(uint32 flag)
{
	intflags_mutex.lock();
	InterruptFlags |= flag;
	intflags_mutex.unlock();
}

void ClearInterruptFlag(uint32 flag)
{
	intflags_mutex.lock();
	InterruptFlags &= ~flag;
	intflags_mutex.unlock();
}


/*
 *  Disable interrupts
 */

void DisableInterrupt(void)
{
	WriteMacInt32(XLM_IRQ_NEST, int32(ReadMacInt32(XLM_IRQ_NEST)) + 1);
}


/*
 *  Enable interrupts
 */

void EnableInterrupt(void)
{
	WriteMacInt32(XLM_IRQ_NEST, int32(ReadMacInt32(XLM_IRQ_NEST)) - 1);
}


/*
 *  Helpers to share 32-bit addressable data with MacOS
 */

bool SheepMem::Init(void)
{
	// Size of a native page
	page_size = vm_page_size();

	// Allocate SheepShaver globals
	proc = base;
	if (vm_mac_acquire(base, size) < 0)
		return false;

	// Allocate page with all bits set to 0, right in the middle
	// This is also used to catch undesired overlaps between proc and data areas
	zero_page = proc + (size / 2);
	Mac_memset(zero_page, 0, page_size);
	if (vm_protect(Mac2HostAddr(zero_page), page_size, VM_PAGE_READ) < 0)
		return false;

	// Allocate alternate stack for PowerPC interrupt routine
	sig_stack = base + size;
	if (vm_mac_acquire(sig_stack, SIG_STACK_SIZE) < 0)
		return false;

	data = base + size;
	return true;
}

void SheepMem::Exit(void)
{
	if (data) {
		// Delete SheepShaver globals
		vm_mac_release(base, size);

		// Delete alternate stack for PowerPC interrupt routine
		vm_mac_release(sig_stack, SIG_STACK_SIZE);
	}
}


/*
 *  Get the main window handle
 */

#ifdef USE_SDL_VIDEO
#include <SDL_syswm.h>
static HWND GetMainWindowHandle(void)
{
	SDL_SysWMinfo wmInfo;
	wmInfo.version.major = SDL_MAJOR_VERSION;
	wmInfo.version.minor = SDL_MINOR_VERSION;
	wmInfo.version.patch = SDL_PATCHLEVEL;
	return SDL_GetWMInfo(&wmInfo) ? wmInfo.window : NULL;
}
#endif


/*
 *  Display alert
 */

static void display_alert(int title_id, const char *text, int flags)
{
	HWND hMainWnd = GetMainWindowHandle();
	MessageBox(hMainWnd, text, GetString(title_id), MB_OK | flags);
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


/*
 *  Display warning alert
 */

void WarningAlert(const char *text)
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
