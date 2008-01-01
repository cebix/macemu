/*
 *  powerrom_cpu.cpp - Using the 680x0 emulator in PowerMac ROMs for Basilisk II
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

#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

#include <AppKit.h>
#include <KernelKit.h>
#include <StorageKit.h>

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "main.h"
#include "emul_op.h"
#include "prefs.h"
#include "timer.h"
#include "user_strings.h"

#include "sheep_driver.h"

#define DEBUG 0
#include "debug.h"

// Save FP regs in Execute68k()?
#define SAVE_FP_EXEC_68K 0


// Constants
const char ROM_FILE_NAME[] = "PowerROM";
const char KERNEL_AREA_NAME[] = "Macintosh Kernel Data";
const char DR_CACHE_AREA_NAME[] = "Macintosh DR Cache";

const uint32 ROM_BASE = 0x40800000;			// Base address of ROM
const uint32 ROM_SIZE = 0x00400000;			// Size of ROM file
const uint32 ROM_AREA_SIZE = 0x00500000;	// Size of ROM area

const uint32 DR_CACHE_BASE = 0x69000000;	// Address of DR cache
const uint32 DR_CACHE_SIZE = 0x80000;		// Size of DR Cache

const uint32 SIG_STACK_SIZE = 8192;			// Size of signal stack

// PowerPC opcodes
const uint32 POWERPC_NOP = 0x60000000;
const uint32 POWERPC_ILLEGAL = 0x00000000;
const uint32 POWERPC_BLR = 0x4e800020;
const uint32 POWERPC_BCTR = 0x4e800420;

// Extra Low Memory Globals
#define MODE_68K 0		// 68k emulator active
#define MODE_EMUL_OP 1	// Within EMUL_OP routine

#define XLM_RESET_STACK 0x2800			// Reset stack pointer
#define XLM_KERNEL_DATA 0x2804			// Pointer to Kernel Data
#define XLM_TOC 0x2808					// TOC pointer of emulator
#define XLM_RUN_MODE 0x2810				// Current run mode, see enum above
#define XLM_68K_R25 0x2814				// Contents of the 68k emulator's r25 (which contains the interrupt level), saved upon entering EMUL_OP mode, used by Execute68k() and the USR1 signal handler
#define XLM_IRQ_NEST 0x2818				// Interrupt disable nesting counter (>0: disabled)
#define XLM_PVR 0x281c					// Theoretical PVR
#define XLM_EMUL_RETURN_PROC 0x2824		// Pointer to EMUL_RETURN routine
#define XLM_EXEC_RETURN_PROC 0x2828		// Pointer to EXEC_RETURN routine
#define XLM_EMUL_OP_PROC 0x282c			// Pointer to EMUL_OP routine
#define XLM_EMUL_RETURN_STACK 0x2830	// Stack pointer for EMUL_RETURN


// RAM and ROM pointers
uint32 RAMBaseMac;			// RAM base (Mac address space)
uint8 *RAMBaseHost;			// RAM base (host address space)
uint32 RAMSize;				// Size of RAM
uint32 ROMBaseMac;			// ROM base (Mac address space)
uint8 *ROMBaseHost;			// ROM base (host address space)
uint32 ROMSize;				// Size of ROM


// Emulator Data
struct EmulatorData {
	uint32	v[0x400];	
};


// Kernel Data
struct KernelData {
	uint32	v[0x400];
	EmulatorData ed;
};


// Exceptions
class file_open_error {};
class file_read_error {};
class rom_size_error {};


// Global variables
static void *TOC;						// TOC pointer
static uint32 PVR;						// Theoretical PVR
static int64 CPUClockSpeed;				// Processor clock speed (Hz)
static int64 BusClockSpeed;				// Bus clock speed (Hz)
static system_info SysInfo;				// System information

static area_id kernel_area = -1;		// Kernel Data area ID
static KernelData *kernel_data = NULL;	// Pointer to Kernel Data
static uint32 KernelDataAddr;			// Address of Kernel Data
static EmulatorData *emulator_data = NULL;
static area_id dr_cache_area;			// DR Cache area ID
static uint32 DRCacheAddr;				// Address of DR Cache

static struct sigaction sigusr1_action;	// Interrupt signal (of emulator thread)
static bool ReadyForSignals = false;	// Flag: emul_thread ready to receive signals


// Prototypes
static void sigusr1_handler(int sig, void *arg, vregs *r);

// From main_beos.cpp
extern int sheep_fd;					// fd of sheep driver
extern thread_id emul_thread;			// Emulator thread


/*
 *  Load ROM file (upper 3MB)
 *
 *  file_open_error: Cannot open ROM file (nor use built-in ROM)
 *  file_read_error: Cannot read ROM file
 */

// Decode LZSS data
static void decode_lzss(const uint8 *src, uint8 *dest, int size)
{
	char dict[0x1000];
	int run_mask = 0, dict_idx = 0xfee;
	for (;;) {
		if (run_mask < 0x100) {
			// Start new run
			if (--size < 0)
				break;
			run_mask = *src++ | 0xff00;
		}
		bool bit = run_mask & 1;
		run_mask >>= 1;
		if (bit) {
			// Verbatim copy
			if (--size < 0)
				break;
			int c = *src++;
			dict[dict_idx++] = c;
			*dest++ = c;
			dict_idx &= 0xfff;
		} else {
			// Copy from dictionary
			if (--size < 0)
				break;
			int idx = *src++;
			if (--size < 0)
				break;
			int cnt = *src++;
			idx |= (cnt << 4) & 0xf00;
			cnt = (cnt & 0x0f) + 3;
			while (cnt--) {
				char c = dict[idx++];
				dict[dict_idx++] = c;
				*dest++ = c;
				idx &= 0xfff;
				dict_idx &= 0xfff;
			}
		}
	}
}

static void load_rom(void)
{
	// Get rom file path from preferences
	const char *rom_path = PrefsFindString("powerrom");

	// Try to open ROM file
	BFile file(rom_path ? rom_path : ROM_FILE_NAME, B_READ_ONLY);
	if (file.InitCheck() != B_NO_ERROR) {

		// Failed, then ask sheep driver for ROM
		uint8 *rom = new uint8[ROM_SIZE];	// Reading directly into the area doesn't work
		ssize_t actual = read(sheep_fd, (void *)rom, ROM_SIZE);
		if (actual == ROM_SIZE) {
			// Copy upper 3MB
			memcpy((void *)(ROM_BASE + 0x100000), rom + 0x100000, ROM_SIZE - 0x100000);
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
	if (actual == ROM_SIZE) {
		// Plain ROM image, copy upper 3MB
		memcpy((void *)(ROM_BASE + 0x100000), rom + 0x100000, ROM_SIZE - 0x100000);
		delete[] rom;
	} else {
		if (strncmp((char *)rom, "<CHRP-BOOT>", 11) == 0) {
			// CHRP compressed ROM image
			D(bug("CHRP ROM image\n"));
			uint32 lzss_offset, lzss_size;

			char *s = strstr((char *)rom, "constant lzss-offset"); 
			if (s == NULL)
				throw rom_size_error();
			s -= 7;
			if (sscanf(s, "%06lx", &lzss_offset) != 1)
				throw rom_size_error();
			s = strstr((char *)rom, "constant lzss-size"); 
			if (s == NULL)
				throw rom_size_error();
			s -= 7;
			if (sscanf(s, "%06lx", &lzss_size) != 1)
				throw rom_size_error();
			D(bug("Offset of compressed data: %08lx\n", lzss_offset));
			D(bug("Size of compressed data: %08lx\n", lzss_size));

			D(bug("Uncompressing ROM...\n"));
			uint8 *decoded = new uint8[ROM_SIZE];
			decode_lzss(rom + lzss_offset, decoded, lzss_size);
			memcpy((void *)(ROM_BASE + 0x100000), decoded + 0x100000, ROM_SIZE - 0x100000);
			delete[] decoded;
			delete[] rom;
		} else if (rom_size != 4*1024*1024)
			throw rom_size_error();
		else
			throw file_read_error();
	}
}


/*
 *  Patch PowerMac ROM
 */

// ROM type
enum {
	ROMTYPE_TNT,
	ROMTYPE_ALCHEMY,
	ROMTYPE_ZANZIBAR,
	ROMTYPE_GAZELLE,
	ROMTYPE_NEWWORLD
};
static int ROMType;

// Nanokernel boot routine patches
static bool patch_nanokernel_boot(void)
{
	uint32 *lp;
	int i;

	// Patch ConfigInfo
	lp = (uint32 *)(ROM_BASE + 0x30d000);
	lp[0x9c >> 2] = KernelDataAddr;			// LA_InfoRecord
	lp[0xa0 >> 2] = KernelDataAddr;			// LA_KernelData
	lp[0xa4 >> 2] = KernelDataAddr + 0x1000;// LA_EmulatorData
	lp[0xa8 >> 2] = ROM_BASE + 0x480000;	// LA_DispatchTable
	lp[0xac >> 2] = ROM_BASE + 0x460000;	// LA_EmulatorCode
	lp[0x360 >> 2] = 0;						// Physical RAM base (? on NewWorld ROM, this contains -1)
	lp[0xfd8 >> 2] = ROM_BASE + 0x2a;		// 68k reset vector

	// Skip SR/BAT/SDR init
	if (ROMType == ROMTYPE_GAZELLE || ROMType == ROMTYPE_NEWWORLD) {
		lp = (uint32 *)(ROM_BASE + 0x310000);
		*lp++ = POWERPC_NOP;
		*lp = 0x38000000;
	}
	static const uint32 sr_init_loc[] = {0x3101b0, 0x3101b0, 0x3101b0, 0x3101ec, 0x310200};
	lp = (uint32 *)(ROM_BASE + 0x310008);
	*lp = 0x48000000 | (sr_init_loc[ROMType] - 8) & 0xffff;	// b		ROM_BASE+0x3101b0
	lp = (uint32 *)(ROM_BASE + sr_init_loc[ROMType]);
	*lp++ = 0x80200000 + XLM_KERNEL_DATA;	// lwz	r1,(pointer to Kernel Data)
	*lp++ = 0x3da0dead;						// lis	r13,0xdead	(start of kernel memory)
	*lp++ = 0x3dc00010;						// lis	r14,0x0010	(size of page table)
	*lp = 0x3de00010;						// lis	r15,0x0010	(size of kernel memory)

	// Don't read PVR
	static const uint32 pvr_loc[] = {0x3103b0, 0x3103b4, 0x3103b4, 0x310400, 0x310438};
	lp = (uint32 *)(ROM_BASE + pvr_loc[ROMType]);
	*lp = 0x81800000 + XLM_PVR;	// lwz	r12,(theoretical PVR)

	// Set CPU specific data (even if ROM doesn't have support for that CPU)
	lp = (uint32 *)(ROM_BASE + pvr_loc[ROMType]);
	if (ntohl(lp[6]) != 0x2c0c0001)
		return false;
	uint32 ofs = lp[7] & 0xffff;
	lp[8] = (lp[8] & 0xffff) | 0x48000000;	// beq -> b
	uint32 loc = (lp[8] & 0xffff) + (uint32)(lp+8) - ROM_BASE;
	lp = (uint32 *)(ROM_BASE + ofs + 0x310000);
	switch (PVR >> 16) {
		case 1:		// 601
			lp[0] = 0x1000;		// Page size
			lp[1] = 0x8000;		// Data cache size
			lp[2] = 0x8000;		// Inst cache size
			lp[3] = 0x00200020;	// Coherency block size/Reservation granule size
			lp[4] = 0x00010040;	// Unified caches/Inst cache line size
			lp[5] = 0x00400020;	// Data cache line size/Data cache block size touch
			lp[6] = 0x00200020;	// Inst cache block size/Data cache block size
			lp[7] = 0x00080008;	// Inst cache assoc/Data cache assoc
			lp[8] = 0x01000002;	// TLB total size/TLB assoc
			break;
		case 3:		// 603
			lp[0] = 0x1000;		// Page size
			lp[1] = 0x2000;		// Data cache size
			lp[2] = 0x2000;		// Inst cache size
			lp[3] = 0x00200020;	// Coherency block size/Reservation granule size
			lp[4] = 0x00000020;	// Unified caches/Inst cache line size
			lp[5] = 0x00200020;	// Data cache line size/Data cache block size touch
			lp[6] = 0x00200020;	// Inst cache block size/Data cache block size
			lp[7] = 0x00020002;	// Inst cache assoc/Data cache assoc
			lp[8] = 0x00400002;	// TLB total size/TLB assoc
			break;
		case 4:		// 604
			lp[0] = 0x1000;		// Page size
			lp[1] = 0x4000;		// Data cache size
			lp[2] = 0x4000;		// Inst cache size
			lp[3] = 0x00200020;	// Coherency block size/Reservation granule size
			lp[4] = 0x00000020;	// Unified caches/Inst cache line size
			lp[5] = 0x00200020;	// Data cache line size/Data cache block size touch
			lp[6] = 0x00200020;	// Inst cache block size/Data cache block size
			lp[7] = 0x00040004;	// Inst cache assoc/Data cache assoc
			lp[8] = 0x00800002;	// TLB total size/TLB assoc
			break;
//		case 5:		// 740?
		case 6:		// 603e
		case 7:		// 603ev
			lp[0] = 0x1000;		// Page size
			lp[1] = 0x4000;		// Data cache size
			lp[2] = 0x4000;		// Inst cache size
			lp[3] = 0x00200020;	// Coherency block size/Reservation granule size
			lp[4] = 0x00000020;	// Unified caches/Inst cache line size
			lp[5] = 0x00200020;	// Data cache line size/Data cache block size touch
			lp[6] = 0x00200020;	// Inst cache block size/Data cache block size
			lp[7] = 0x00040004;	// Inst cache assoc/Data cache assoc
			lp[8] = 0x00400002;	// TLB total size/TLB assoc
			break;
		case 8:		// 750
			lp[0] = 0x1000;		// Page size
			lp[1] = 0x8000;		// Data cache size
			lp[2] = 0x8000;		// Inst cache size
			lp[3] = 0x00200020;	// Coherency block size/Reservation granule size
			lp[4] = 0x00000020;	// Unified caches/Inst cache line size
			lp[5] = 0x00200020;	// Data cache line size/Data cache block size touch
			lp[6] = 0x00200020;	// Inst cache block size/Data cache block size
			lp[7] = 0x00080008;	// Inst cache assoc/Data cache assoc
			lp[8] = 0x00800002;	// TLB total size/TLB assoc
			break;
		case 9:		// 604e
		case 10:	// 604ev5
			lp[0] = 0x1000;		// Page size
			lp[1] = 0x8000;		// Data cache size
			lp[2] = 0x8000;		// Inst cache size
			lp[3] = 0x00200020;	// Coherency block size/Reservation granule size
			lp[4] = 0x00000020;	// Unified caches/Inst cache line size
			lp[5] = 0x00200020;	// Data cache line size/Data cache block size touch
			lp[6] = 0x00200020;	// Inst cache block size/Data cache block size
			lp[7] = 0x00040004;	// Inst cache assoc/Data cache assoc
			lp[8] = 0x00800002;	// TLB total size/TLB assoc
			break;
//		case 11:	// X704?
		case 12:	// ???
			lp[0] = 0x1000;		// Page size
			lp[1] = 0x8000;		// Data cache size
			lp[2] = 0x8000;		// Inst cache size
			lp[3] = 0x00200020;	// Coherency block size/Reservation granule size
			lp[4] = 0x00000020;	// Unified caches/Inst cache line size
			lp[5] = 0x00200020;	// Data cache line size/Data cache block size touch
			lp[6] = 0x00200020;	// Inst cache block size/Data cache block size
			lp[7] = 0x00080008;	// Inst cache assoc/Data cache assoc
			lp[8] = 0x00800002;	// TLB total size/TLB assoc
			break;
		case 13:	// ???
			lp[0] = 0x1000;		// Page size
			lp[1] = 0x8000;		// Data cache size
			lp[2] = 0x8000;		// Inst cache size
			lp[3] = 0x00200020;	// Coherency block size/Reservation granule size
			lp[4] = 0x00000020;	// Unified caches/Inst cache line size
			lp[5] = 0x00200020;	// Data cache line size/Data cache block size touch
			lp[6] = 0x00200020;	// Inst cache block size/Data cache block size
			lp[7] = 0x00080008;	// Inst cache assoc/Data cache assoc
			lp[8] = 0x01000004;	// TLB total size/TLB assoc
			break;
//		case 50:	// 821
//		case 80:	// 860
		case 96:	// ???
			lp[0] = 0x1000;		// Page size
			lp[1] = 0x8000;		// Data cache size
			lp[2] = 0x8000;		// Inst cache size
			lp[3] = 0x00200020;	// Coherency block size/Reservation granule size
			lp[4] = 0x00010020;	// Unified caches/Inst cache line size
			lp[5] = 0x00200020;	// Data cache line size/Data cache block size touch
			lp[6] = 0x00200020;	// Inst cache block size/Data cache block size
			lp[7] = 0x00080008;	// Inst cache assoc/Data cache assoc
			lp[8] = 0x00800004;	// TLB total size/TLB assoc
			break;
		default:
			printf("WARNING: Unknown CPU type\n");
			break;
	}

	// Don't set SPRG3, don't test MQ
	lp = (uint32 *)(ROM_BASE + loc + 0x20);
	*lp++ = POWERPC_NOP;
	lp++;
	*lp++ = POWERPC_NOP;
	lp++;
	*lp = POWERPC_NOP;

	// Don't read MSR
	lp = (uint32 *)(ROM_BASE + loc + 0x40);
	*lp = 0x39c00000;		// li	r14,0

	// Don't write to DEC
	lp = (uint32 *)(ROM_BASE + loc + 0x70);
	*lp++ = POWERPC_NOP;
	loc = (lp[0] & 0xffff) + (uint32)lp - ROM_BASE;

	// Don't set SPRG3
	lp = (uint32 *)(ROM_BASE + loc + 0x2c);
	*lp = POWERPC_NOP;

	// Don't read PVR
	static const uint32 pvr_ofs[] = {0x138, 0x138, 0x138, 0x140, 0x148};
	lp = (uint32 *)(ROM_BASE + loc + pvr_ofs[ROMType]);
	*lp = 0x82e00000 + XLM_PVR;		// lwz	r23,(theoretical PVR)
	lp = (uint32 *)(ROM_BASE + loc + 0x170);
	if (*lp == 0x7eff42a6)		// NewWorld ROM
		*lp = 0x82e00000 + XLM_PVR;	// lwz	r23,(theoretical PVR)
	lp = (uint32 *)(ROM_BASE + 0x313134);
	if (*lp == 0x7e5f42a6)
		*lp = 0x82400000 + XLM_PVR;	// lwz	r18,(theoretical PVR)
	lp = (uint32 *)(ROM_BASE + 0x3131f4);
	if (*lp == 0x7e5f42a6)		// NewWorld ROM
		*lp = 0x82400000 + XLM_PVR;	// lwz	r18,(theoretical PVR)

	// Don't read SDR1
	static const uint32 sdr1_ofs[] = {0x174, 0x174, 0x174, 0x17c, 0x19c};
	lp = (uint32 *)(ROM_BASE + loc + sdr1_ofs[ROMType]);
	*lp++ = 0x3d00dead;		// lis	r8,0xdead		(pointer to page table)
	*lp++ = 0x3ec0001f;		// lis	r22,0x001f	(size of page table)
	*lp = POWERPC_NOP;

	// Don't clear page table
	static const uint32 pgtb_ofs[] = {0x198, 0x198, 0x198, 0x1a0, 0x1c4};
	lp = (uint32 *)(ROM_BASE + loc + pgtb_ofs[ROMType]);
	*lp = POWERPC_NOP;

	// Don't invalidate TLB
	static const uint32 tlb_ofs[] = {0x1a0, 0x1a0, 0x1a0, 0x1a8, 0x1cc};
	lp = (uint32 *)(ROM_BASE + loc + tlb_ofs[ROMType]);
	*lp = POWERPC_NOP;

	// Don't create RAM descriptor table
	static const uint32 desc_ofs[] = {0x350, 0x350, 0x350, 0x358, 0x37c};
	lp = (uint32 *)(ROM_BASE + loc + desc_ofs[ROMType]);
	*lp = POWERPC_NOP;

	// Don't load SRs and BATs
	static const uint32 sr_ofs[] = {0x3d8, 0x3d8, 0x3d8, 0x3e0, 0x404};
	lp = (uint32 *)(ROM_BASE + loc + sr_ofs[ROMType]);
	*lp = POWERPC_NOP;

	// Don't mess with SRs
	static const uint32 sr2_ofs[] = {0x312118, 0x312118, 0x312118, 0x312118, 0x3121b4};
	lp = (uint32 *)(ROM_BASE + sr2_ofs[ROMType]);
	*lp = POWERPC_BLR;

	// Don't check performance monitor
	static const uint32 pm_ofs[] = {0x313148, 0x313148, 0x313148, 0x313148, 0x313218};
	lp = (uint32 *)(ROM_BASE + pm_ofs[ROMType]);
	while (*lp != 0x7e58eba6) lp++;
	*lp++ = POWERPC_NOP;
	while (*lp != 0x7e78eaa6) lp++;
	*lp++ = POWERPC_NOP;
	while (*lp != 0x7e59eba6) lp++;
	*lp++ = POWERPC_NOP;
	while (*lp != 0x7e79eaa6) lp++;
	*lp++ = POWERPC_NOP;
	while (*lp != 0x7e5aeba6) lp++;
	*lp++ = POWERPC_NOP;
	while (*lp != 0x7e7aeaa6) lp++;
	*lp++ = POWERPC_NOP;
	while (*lp != 0x7e5beba6) lp++;
	*lp++ = POWERPC_NOP;
	while (*lp != 0x7e7beaa6) lp++;
	*lp++ = POWERPC_NOP;
	while (*lp != 0x7e5feba6) lp++;
	*lp++ = POWERPC_NOP;
	while (*lp != 0x7e7feaa6) lp++;
	*lp++ = POWERPC_NOP;
	while (*lp != 0x7e5ceba6) lp++;
	*lp++ = POWERPC_NOP;
	while (*lp != 0x7e7ceaa6) lp++;
	*lp++ = POWERPC_NOP;
	while (*lp != 0x7e5deba6) lp++;
	*lp++ = POWERPC_NOP;
	while (*lp != 0x7e7deaa6) lp++;
	*lp++ = POWERPC_NOP;
	while (*lp != 0x7e5eeba6) lp++;
	*lp++ = POWERPC_NOP;
	while (*lp != 0x7e7eeaa6) lp++;
	*lp++ = POWERPC_NOP;

	// Jump to 68k emulator
	static const uint32 jump68k_ofs[] = {0x40c, 0x40c, 0x40c, 0x414, 0x438};
	lp = (uint32 *)(ROM_BASE + loc + jump68k_ofs[ROMType]);
	*lp++ = 0x80610634;		// lwz	r3,0x0634(r1)	(pointer to Emulator Data)
	*lp++ = 0x8081119c;		// lwz	r4,0x119c(r1)	(pointer to opcode table)
	*lp++ = 0x80011184;		// lwz	r0,0x1184(r1)	(pointer to emulator entry)
	*lp++ = 0x7c0903a6;		// mtctr	r0
	*lp = POWERPC_BCTR;
	return true;
}

// 68k emulator patches
static bool patch_68k_emul(void)
{
	uint32 *lp;
	uint32 base;

	// Overwrite twi instructions
	static const uint32 twi_loc[] = {0x36e680, 0x36e6c0, 0x36e6c0, 0x36e6c0, 0x36e740};
	base = twi_loc[ROMType];
	lp = (uint32 *)(ROM_BASE + base);
	*lp++ = 0x48000000 + 0x36f900 - base;		// b 0x36f900 (Emulator start)
	*lp++ = POWERPC_ILLEGAL;
	*lp++ = 0x48000000 + 0x36fb00 - base - 8;	// b 0x36fb00 (Reset opcode)
	*lp++ = POWERPC_ILLEGAL;
	*lp++ = POWERPC_ILLEGAL;
	*lp++ = POWERPC_ILLEGAL;
	*lp++ = POWERPC_ILLEGAL;
	*lp++ = POWERPC_ILLEGAL;
	*lp++ = POWERPC_ILLEGAL;
	*lp++ = POWERPC_ILLEGAL;
	*lp++ = POWERPC_ILLEGAL;
	*lp++ = POWERPC_ILLEGAL;
	*lp++ = POWERPC_ILLEGAL;
	*lp++ = POWERPC_ILLEGAL;
	*lp++ = POWERPC_ILLEGAL;
	*lp++ = POWERPC_ILLEGAL;

	// Set reset stack pointer
	lp = (uint32 *)(ROM_BASE + base + 0xf0);
	*lp++ = 0x80200000 + XLM_RESET_STACK;			// lwz		r1,XLM_RESET_STACK

	// Install EXEC_RETURN and EMUL_OP opcodes
	lp = (uint32 *)(ROM_BASE + 0x380000 + (M68K_EXEC_RETURN << 3));
	*lp++ = 0x80000000 + XLM_EXEC_RETURN_PROC;		// lwz	r0,XLM_EXEC_RETURN_PROC
	*lp++ = 0x4bfb6ffc;								// b	0x36f800
	for (int i=0; i<M68K_EMUL_OP_MAX-M68K_EMUL_BREAK; i++) {
		*lp++ = 0x38a00000 + i + M68K_EMUL_BREAK;	// li	r5,M68K_EMUL_OP_*
		*lp++ = 0x4bfb6ffc - i*8;					// b	0x36f808
	}

	// Special handling for M68K_EMUL_OP_SHUTDOWN because Basilisk II is running
	// on the 68k stack and simply quitting would delete the RAM area leaving
	// the stack pointer in unaccessible memory
	lp = (uint32 *)(ROM_BASE + 0x380000 + (M68K_EMUL_OP_SHUTDOWN << 3));
	*lp++ = 0x80000000 + XLM_EMUL_RETURN_PROC;		// lwz	r0,XLM_EMUL_RETURN_PROC
	*lp++ = 0x4bfb6ffc - (M68K_EMUL_OP_SHUTDOWN - M68K_EXEC_RETURN) * 8;	// b	0x36f800

	// Extra routines for EMUL_RETURN/EXEC_RETURN/EMUL_OP
	lp = (uint32 *)(ROM_BASE + 0x36f800);
	*lp++ = 0x7c0803a6;					// mtlr	r0
	*lp++ = 0x4e800020;					// blr

	*lp++ = 0x80000000 + XLM_EMUL_OP_PROC;	// lwz	r0,XLM_EMUL_OP_PROC
	*lp++ = 0x7c0803a6;					// mtlr	r0
	*lp++ = 0x4e800020;					// blr

	// Extra routine for 68k emulator start
	lp = (uint32 *)(ROM_BASE + 0x36f900);
	*lp++ = 0x7c2903a6;					// mtctr	r1
	*lp++ = 0x80200000 + XLM_IRQ_NEST;	// lwz		r1,XLM_IRQ_NEST
	*lp++ = 0x38210001;					// addi		r1,r1,1
	*lp++ = 0x90200000 + XLM_IRQ_NEST;	// stw		r1,XLM_IRQ_NEST
	*lp++ = 0x80200000 + XLM_KERNEL_DATA;// lwz		r1,XLM_KERNEL_DATA
	*lp++ = 0x90c10018;					// stw		r6,0x18(r1)
	*lp++ = 0x7cc902a6;					// mfctr	r6
	*lp++ = 0x90c10004;					// stw		r6,$0004(r1)
	*lp++ = 0x80c1065c;					// lwz		r6,$065c(r1)
	*lp++ = 0x90e6013c;					// stw		r7,$013c(r6)
	*lp++ = 0x91060144;					// stw		r8,$0144(r6)
	*lp++ = 0x9126014c;					// stw		r9,$014c(r6)
	*lp++ = 0x91460154;					// stw		r10,$0154(r6)
	*lp++ = 0x9166015c;					// stw		r11,$015c(r6)
	*lp++ = 0x91860164;					// stw		r12,$0164(r6)
	*lp++ = 0x91a6016c;					// stw		r13,$016c(r6)
	*lp++ = 0x7da00026;					// mfcr		r13
	*lp++ = 0x80e10660;					// lwz		r7,$0660(r1)
	*lp++ = 0x7d8802a6;					// mflr		r12
	*lp++ = 0x50e74001;					// rlwimi.	r7,r7,8,$80000000
	*lp++ = 0x814105f0;					// lwz		r10,0x05f0(r1)
	*lp++ = 0x7d4803a6;					// mtlr		r10
	*lp++ = 0x7d8a6378;					// mr		r10,r12
	*lp++ = 0x3d600002;					// lis		r11,0x0002
	*lp++ = 0x616bf072;					// ori		r11,r11,0xf072 (MSR)
	*lp++ = 0x50e7deb4;					// rlwimi	r7,r7,27,$00000020
	*lp++ = 0x4e800020;					// blr

	// Extra routine for Reset opcode
	lp = (uint32 *)(ROM_BASE + 0x36fc00);
	*lp++ = 0x7c2903a6;					// mtctr	r1
	*lp++ = 0x80200000 + XLM_IRQ_NEST;	// lwz		r1,XLM_IRQ_NEST
	*lp++ = 0x38210001;					// addi		r1,r1,1
	*lp++ = 0x90200000 + XLM_IRQ_NEST;	// stw		r1,XLM_IRQ_NEST
	*lp++ = 0x80200000 + XLM_KERNEL_DATA;// lwz		r1,XLM_KERNEL_DATA
	*lp++ = 0x90c10018;					// stw		r6,0x18(r1)
	*lp++ = 0x7cc902a6;					// mfctr	r6
	*lp++ = 0x90c10004;					// stw		r6,$0004(r1)
	*lp++ = 0x80c1065c;					// lwz		r6,$065c(r1)
	*lp++ = 0x90e6013c;					// stw		r7,$013c(r6)
	*lp++ = 0x91060144;					// stw		r8,$0144(r6)
	*lp++ = 0x9126014c;					// stw		r9,$014c(r6)
	*lp++ = 0x91460154;					// stw		r10,$0154(r6)
	*lp++ = 0x9166015c;					// stw		r11,$015c(r6)
	*lp++ = 0x91860164;					// stw		r12,$0164(r6)
	*lp++ = 0x91a6016c;					// stw		r13,$016c(r6)
	*lp++ = 0x7da00026;					// mfcr		r13
	*lp++ = 0x80e10660;					// lwz		r7,$0660(r1)
	*lp++ = 0x7d8802a6;					// mflr		r12
	*lp++ = 0x50e74001;					// rlwimi.	r7,r7,8,$80000000
	*lp++ = 0x814105f4;					// lwz		r10,0x05f8(r1)
	*lp++ = 0x7d4803a6;					// mtlr		r10
	*lp++ = 0x7d8a6378;					// mr		r10,r12
	*lp++ = 0x3d600002;					// lis		r11,0x0002
	*lp++ = 0x616bf072;					// ori		r11,r11,0xf072 (MSR)
	*lp++ = 0x50e7deb4;					// rlwimi	r7,r7,27,$00000020
	*lp++ = 0x4e800020;					// blr

	// Patch DR emulator to jump to right address when an interrupt occurs
	lp = (uint32 *)(ROM_BASE + 0x370000);
	while (lp < (uint32 *)(ROM_BASE + 0x380000)) {
		if (*lp == 0x4ca80020)		// bclr		5,8
			goto dr_found;
		lp++;
	}
	D(bug("DR emulator patch location not found\n"));
	return false;
dr_found:
	lp++;
	*lp = 0x48000000 + 0xf000 - ((uint32)lp & 0xffff);		// b	DR_CACHE_BASE+0x1f000
	lp = (uint32 *)(ROM_BASE + 0x37f000);
	*lp++ = 0x3c000000 + ((ROM_BASE + 0x46d0a4) >> 16);		// lis	r0,xxx
	*lp++ = 0x60000000 + ((ROM_BASE + 0x46d0a4) & 0xffff);	// ori	r0,r0,xxx
	*lp++ = 0x7c0903a6;										// mtctr	r0
	*lp = POWERPC_BCTR;										// bctr
	return true;
}

// Nanokernel patches
static bool patch_nanokernel(void)
{
	uint32 *lp;

	// Patch 68k emulator trap routine
	lp = (uint32 *)(ROM_BASE + 0x312994);	// Always restore FPU state
	while (*lp != 0x39260040) lp++;
	lp--;
	*lp = 0x48000441;					// bl	0x00312dd4
	lp = (uint32 *)(ROM_BASE + 0x312dd8);	// Don't modify MSR to turn on FPU
	while (*lp != 0x810600e4) lp++;
	lp--;
	*lp++ = POWERPC_NOP;
	lp += 2;
	*lp++ = POWERPC_NOP;
	lp++;
	*lp++ = POWERPC_NOP;
	*lp++ = POWERPC_NOP;
	*lp = POWERPC_NOP;

	// Patch trap return routine
	lp = (uint32 *)(ROM_BASE + 0x312c20);
	while (*lp != 0x7d5a03a6) lp++;
	*lp++ = 0x7d4903a6;					// mtctr	r10
	*lp++ = 0x7daff120;					// mtcr	r13
	*lp++ = 0x48000000 + 0x8000 - ((uint32)lp & 0xffff);	// b		ROM_BASE+0x318000
	uint32 xlp = (uint32)lp & 0xffff;

	lp = (uint32 *)(ROM_BASE + 0x312c50);	// Replace rfi
	while (*lp != 0x4c000064) lp++;
	*lp = POWERPC_BCTR;

	lp = (uint32 *)(ROM_BASE + 0x318000);
	*lp++ = 0x81400000 + XLM_IRQ_NEST;	// lwz	r10,XLM_IRQ_NEST
	*lp++ = 0x394affff;					// subi	r10,r10,1
	*lp++ = 0x91400000 + XLM_IRQ_NEST;	// stw	r10,XLM_IRQ_NEST
	*lp = 0x48000000 + ((xlp - 0x800c) & 0x03fffffc);	// b		ROM_BASE+0x312c2c
	return true;
}

static bool patch_rom(void)
{
	// Detect ROM type
	if (!memcmp((void *)(ROM_BASE + 0x30d064), "Boot TNT", 8))
		ROMType = ROMTYPE_TNT;
	else if (!memcmp((void *)(ROM_BASE + 0x30d064), "Boot Alchemy", 12))
		ROMType = ROMTYPE_ALCHEMY;
	else if (!memcmp((void *)(ROM_BASE + 0x30d064), "Boot Zanzibar", 13))
		ROMType = ROMTYPE_ZANZIBAR;
	else if (!memcmp((void *)(ROM_BASE + 0x30d064), "Boot Gazelle", 12))
		ROMType = ROMTYPE_GAZELLE;
	else if (!memcmp((void *)(ROM_BASE + 0x30d064), "NewWorld", 8))
		ROMType = ROMTYPE_NEWWORLD;
	else
		return false;

	// Apply patches
	if (!patch_nanokernel_boot()) return false;
	if (!patch_68k_emul()) return false;
	if (!patch_nanokernel()) return false;

	// Copy 68k emulator to 2MB boundary
	memcpy((void *)(ROM_BASE + ROM_SIZE), (void *)(ROM_BASE + ROM_SIZE - 0x100000), 0x100000);
	return true;
}


/*
 *  Initialize 680x0 emulation
 */

static asm void *get_toc(void)
{
	mr	r3,r2
	blr
}

bool Init680x0(void)
{
	char str[256];

	// Mac address space = host address space
	RAMBaseMac = (uint32)RAMBaseHost;
	ROMBaseMac = (uint32)ROMBaseHost;

	// Get TOC pointer
	TOC = get_toc();

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
		default:
			PVR = 0x00040000;
			break;
	}
	CPUClockSpeed = SysInfo.cpu_clock_speed;
	BusClockSpeed = SysInfo.bus_clock_speed;

	// Delete old areas
	area_id old_kernel_area = find_area(KERNEL_AREA_NAME);
	if (old_kernel_area > 0)
		delete_area(old_kernel_area);
	area_id old_dr_cache_area = find_area(DR_CACHE_AREA_NAME);
	if (old_dr_cache_area > 0)
		delete_area(old_dr_cache_area);

	// Create area for Kernel Data
	kernel_data = (KernelData *)0x68ffe000;
	kernel_area = create_area(KERNEL_AREA_NAME, &kernel_data, B_EXACT_ADDRESS, 0x2000, B_NO_LOCK, B_READ_AREA | B_WRITE_AREA);
	if (kernel_area < 0) {
		sprintf(str, GetString(STR_NO_KERNEL_DATA_ERR), strerror(kernel_area), kernel_area);
		ErrorAlert(str);
		return false;
	}
	emulator_data = &kernel_data->ed;
	KernelDataAddr = (uint32)kernel_data;
	D(bug("Kernel Data area %ld at %p, Emulator Data at %p\n", kernel_area, kernel_data, emulator_data));

	// Load PowerMac ROM (upper 3MB)
	try {
		load_rom();
	} catch (file_open_error) {
		ErrorAlert(STR_NO_ROM_FILE_ERR);
		return false;
	} catch (file_read_error) {
		ErrorAlert(STR_ROM_FILE_READ_ERR);
		return false;
	} catch (rom_size_error) {
		ErrorAlert(STR_ROM_SIZE_ERR);
		return false;
	}

	// Install ROM patches
	if (!patch_rom()) {
		ErrorAlert("Unsupported PowerMac ROM version");
		return false;
	}

	// Create area for DR Cache
	DRCacheAddr = DR_CACHE_BASE;
	dr_cache_area = create_area(DR_CACHE_AREA_NAME, (void **)&DRCacheAddr, B_EXACT_ADDRESS, DR_CACHE_SIZE, B_NO_LOCK, B_READ_AREA | B_WRITE_AREA);
	if (dr_cache_area < 0) {
		sprintf(str, GetString(STR_NO_KERNEL_DATA_ERR), strerror(dr_cache_area), dr_cache_area);
		ErrorAlert(str);
		return false;
	}
	D(bug("DR Cache area %ld at %p\n", dr_cache_area, DRCacheAddr));

	// Initialize Kernel Data
	memset(kernel_data, 0, sizeof(KernelData));
	if (ROMType == ROMTYPE_NEWWORLD) {
		kernel_data->v[0xc20 >> 2] = RAMSize;
		kernel_data->v[0xc24 >> 2] = RAMSize;
		kernel_data->v[0xc30 >> 2] = RAMSize;
		kernel_data->v[0xc34 >> 2] = RAMSize;
		kernel_data->v[0xc38 >> 2] = 0x00010020;
		kernel_data->v[0xc3c >> 2] = 0x00200001;
		kernel_data->v[0xc40 >> 2] = 0x00010000;
		kernel_data->v[0xc50 >> 2] = RAMBaseMac;
		kernel_data->v[0xc54 >> 2] = RAMSize;
		kernel_data->v[0xf60 >> 2] = PVR;
		kernel_data->v[0xf64 >> 2] = CPUClockSpeed;
		kernel_data->v[0xf68 >> 2] = BusClockSpeed;
		kernel_data->v[0xf6c >> 2] = CPUClockSpeed;
	} else {
		kernel_data->v[0xc80 >> 2] = RAMSize;
		kernel_data->v[0xc84 >> 2] = RAMSize;
		kernel_data->v[0xc90 >> 2] = RAMSize;
		kernel_data->v[0xc94 >> 2] = RAMSize;
		kernel_data->v[0xc98 >> 2] = 0x00010020;
		kernel_data->v[0xc9c >> 2] = 0x00200001;
		kernel_data->v[0xca0 >> 2] = 0x00010000;
		kernel_data->v[0xcb0 >> 2] = RAMBaseMac;
		kernel_data->v[0xcb4 >> 2] = RAMSize;
		kernel_data->v[0xf80 >> 2] = PVR;
		kernel_data->v[0xf84 >> 2] = CPUClockSpeed;
		kernel_data->v[0xf88 >> 2] = BusClockSpeed;
		kernel_data->v[0xf8c >> 2] = CPUClockSpeed;
	}

	// Initialize extra low memory
	memset((void *)0x2000, 0, 0x1000);
	*(uint32 *)XLM_RESET_STACK = 0x2000;		// Reset stack pointer
	*(KernelData **)XLM_KERNEL_DATA = kernel_data;// For trap replacement routines
	*(void **)XLM_TOC = TOC;					// TOC pointer of emulator
	*(uint32 *)XLM_PVR = PVR;					// Theoretical PVR

	// Clear caches (as we loaded and patched code)
	clear_caches((void *)ROM_BASE, ROM_AREA_SIZE, B_INVALIDATE_ICACHE | B_FLUSH_DCACHE);
	return true;
}


/*
 *  Deinitialize 680x0 emulation
 */

void Exit680x0(void)
{
	// Delete DR Cache area
	if (dr_cache_area >= 0)
		delete_area(dr_cache_area);

	// Delete Kernel Data area
	if (kernel_area >= 0)
		delete_area(kernel_area);
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


/*
 *  Reset and start 680x0 emulation
 */

static asm void jump_to_rom(register uint32 entry)
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
	mtctr	r3
	lwz		r3,emulator_data(r2)

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
	li		r0,MODE_EMUL_OP
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

	// Skip over EMUL_OP routine and get its address
	bl		@3


	/*
	 *  EMUL_OP: Execute native routine, selector in r5 (my own private mode switch)
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
	mr		r3,r5
	addi	r4,r1,56
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


	// Save address of EMUL_OP routine for 68k emulator patch
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

void Start680x0(void)
{
	// Install interrupt signal handler
	sigemptyset(&sigusr1_action.sa_mask);
	sigusr1_action.sa_handler = (__signal_func_ptr)(sigusr1_handler);
	sigusr1_action.sa_flags = 0;
	sigusr1_action.sa_userdata = NULL;
	sigaction(SIGUSR1, &sigusr1_action, NULL);

	// Install signal stack
	set_signal_stack(malloc(SIG_STACK_SIZE), SIG_STACK_SIZE);

	// We're now ready to receive signals
	ReadyForSignals = true;

	D(bug("Jumping to ROM\n"));
	jump_to_rom(ROM_BASE + 0x310000);
	D(bug("Returned from ROM\n"));

	// We're no longer ready to receive signals
	ReadyForSignals = false;
}


/*
 *  Trigger interrupt
 */

void TriggerInterrupt(void)
{
	idle_resume();
	if (emul_thread > 0 && ReadyForSignals)
		send_signal(emul_thread, SIGUSR1);
}

void TriggerNMI(void)
{
	//!! not implemented yet
}


/*
 *  Execute 68k subroutine
 *  r->a[7] and r->sr are unused!
 */

static asm void execute_68k(register uint32 addr, register M68kRegisters *r)
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
	addi	r31,r31,0x1000		// points to Emulator Data
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

void Execute68k(uint32 addr, M68kRegisters *r)
{
	uint16 proc[4] = {M68K_JSR, addr >> 16, addr & 0xffff, M68K_EXEC_RETURN};
	execute_68k((uint32)proc, r);
}


/*
 *  Execute MacOS 68k trap
 *  r->a[7] and r->sr are unused!
 */

void Execute68kTrap(uint16 trap, struct M68kRegisters *r)
{
	uint16 proc[2] = {trap, M68K_EXEC_RETURN};
	execute_68k((uint32)proc, r);
}


/*
 *  USR1 handler
 */

static void sigusr1_handler(int sig, void *arg, vregs *r)
{
	// Do nothing if interrupts are disabled
	if ((*(int32 *)XLM_IRQ_NEST) > 0)
		return;

	// 68k emulator active? Then trigger 68k interrupt level 1
	if (*(uint32 *)XLM_RUN_MODE == MODE_68K) {
		*(uint16 *)(kernel_data->v[0x67c >> 2]) = 1;
		r->cr |= kernel_data->v[0x674 >> 2];
	}
}
