/*
 *  basilisk_glue.cpp - Glue UAE CPU to Basilisk II CPU engine interface
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

#include "cpu_emulation.h"
#include "main.h"
#include "prefs.h"
#include "emul_op.h"
#include "rom_patches.h"
#include "timer.h"
#include "m68k.h"
#include "memory.h"
#include "readcpu.h"
#include "newcpu.h"
#include "compiler/compemu.h"
#include "vm_alloc.h"
#include "user_strings.h"

#include "debug.h"

#if DIRECT_ADDRESSING
uintptr MEMBaseDiff = 0;	// Global offset between a Mac address and its Host equivalent
#endif

// RAM and ROM pointers
uint8* RAMBaseHost = 0;		// RAM base (host address space)
uint8* ROMBaseHost = 0;		// ROM base (host address space)
uint32 RAMBaseMac = 0;		// RAM base (Mac address space)
uint32 ROMBaseMac = 0;		// ROM base (Mac address space)
uint32 RAMSize = 0;			// Size of RAM
uint32 ROMSize = 0;			// Size of ROM

// Mac frame buffer
uint8* MacFrameBaseHost;	// Frame buffer base (host address space)
uint32 MacFrameSize;			// Size of current frame buffer
int MacFrameLayout;			// Frame buffer layout
uint32 VRAMSize;				// Size of VRAM

uint32 JITCacheSize=0;

const char ROM_FILE_NAME[] = "ROM";
const int MAX_ROM_SIZE = 1024*1024;	// 1mb

#if USE_SCRATCHMEM_SUBTERFUGE
uint8* ScratchMem = NULL;	// Scratch memory for Mac ROM writes
int ScratchMemSize = 64*1024; // 64k
#else
int ScratchMemSize = 0;
#endif

#if USE_JIT
bool UseJIT = false;
#endif

// From newcpu.cpp
extern bool quit_program;

// Create our virtual Macintosh memory map and load ROM
bool InitMacMem(void){
	assert(RAMBaseHost==0); // don't call us twice

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
	if (RAMSize > 1023*1024*1024) // Cap to 1023MB (APD crashes at 1GB)
		RAMSize = 1023*1024*1024;

	VRAMSize = 16*1024*1024; // 16mb, more than enough for 1920x1440x32

#if USE_JIT
	JITCacheSize = 1024*PrefsFindInt32("jitcachesize") + 1024;
#endif

	// Initialize VM system
	vm_init();

	// Create our virtual Macintosh memory map
	RAMBaseHost = (uint8*)vm_acquire(
			RAMSize + ScratchMemSize + MAX_ROM_SIZE + VRAMSize + JITCacheSize,
			VM_MAP_DEFAULT | VM_MAP_32BIT);
	if (RAMBaseHost == VM_MAP_FAILED) {
		ErrorAlert(STR_NO_MEM_ERR);
		return false;
	}
	printf("RAMBaseHost=%p\n",RAMBaseHost);
	ROMBaseHost = RAMBaseHost + RAMSize + ScratchMemSize;
	printf("ROMBaseHost=%p\n",ROMBaseHost);
	MacFrameBaseHost = ROMBaseHost + MAX_ROM_SIZE;
	printf("MacFrameBaseHost=%p\n",MacFrameBaseHost);

#if USE_SCRATCHMEM_SUBTERFUGE
	// points to middle of scratch memory
	ScratchMem = RAMBaseHost + RAMSize + ScratchMemSize/2;
	printf("ScratchMem=%p\n",ScratchMem);
#endif

	// Get rom file path from preferences
	const char *rom_path = PrefsFindString("rom");

	// Load Mac ROM
#ifdef WIN32
	HANDLE rom_fh = CreateFile(
		rom_path ? rom_path : ROM_FILE_NAME,
		GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);

	if (rom_fh == INVALID_HANDLE_VALUE) {
		ErrorAlert(STR_NO_ROM_FILE_ERR);
		return false;
	}
#else
	int rom_fd = open(rom_path ? rom_path : ROM_FILE_NAME, O_RDONLY);
	if (rom_fd < 0) {
		ErrorAlert(STR_NO_ROM_FILE_ERR);
		return false;
	}
#endif
	printf("%s", GetString(STR_READING_ROM_FILE));
#ifdef WIN32
	ROMSize = GetFileSize(rom_fh, NULL);
#else
	ROMSize = lseek(rom_fd, 0, SEEK_END);
#endif
	switch(ROMSize){
		case  64*1024:
		case 128*1024:
		case 256*1024:
		case 512*1024:
		case MAX_ROM_SIZE:
			break;
		default:
			ErrorAlert(STR_ROM_SIZE_ERR);
#ifdef WIN32
			CloseHandle(rom_fh);
#else
			close(rom_fd);
#endif
			return false;
	}
#ifdef WIN32
	DWORD bytes_read;
	if (ReadFile(rom_fh, ROMBaseHost, ROMSize, &bytes_read, NULL) == 0 || bytes_read != ROMSize) {
#else
	lseek(rom_fd, 0, SEEK_SET);
	if (read(rom_fd, ROMBaseHost, ROMSize) != (ssize_t)ROMSize) {
#endif
		ErrorAlert(STR_ROM_FILE_READ_ERR);
#ifdef WIN32
		CloseHandle(rom_fh);
#else
		close(rom_fd);
#endif
		return false;
	}

	if (!CheckROM()) {
		ErrorAlert(STR_UNSUPPORTED_ROM_TYPE_ERR);
		return false;
	}

#if DIRECT_ADDRESSING
	// Mac address space = host address space minus constant offset (MEMBaseDiff)
	MEMBaseDiff = (uintptr)RAMBaseHost;
	ROMBaseMac = Host2MacAddr(ROMBaseHost);
#else
	// Initialize UAE memory banks
	switch (ROMVersion) {
		case ROM_VERSION_64K:
		case ROM_VERSION_PLUS:
		case ROM_VERSION_CLASSIC:
			ROMBaseMac = 0x00400000;
			break;
		case ROM_VERSION_II:
			ROMBaseMac = 0x00a00000;
			break;
		case ROM_VERSION_32:
			ROMBaseMac = 0x40800000;
			break;
		default:
			return false;
	}
	memory_init();
#endif
	D(bug("Mac RAM starts at %p (%08x)\n", RAMBaseHost, RAMBaseMac));
	D(bug("Mac ROM starts at %p (%08x)\n", ROMBaseHost, ROMBaseMac));

	return true;
}

void MacMemExit(void){
	assert(RAMBaseHost);
	vm_release(RAMBaseHost,
			RAMSize + ScratchMemSize + MAX_ROM_SIZE + VRAMSize + JITCacheSize);
	RAMBaseHost = NULL;
	ROMBaseHost = NULL;
	//Exit VM wrappers
	vm_exit();
}

/*
 *  Initialize 680x0 emulation, CheckROM() must have been called first
 */

bool Init680x0(void){
	init_m68k();
#if USE_JIT
	UseJIT = compiler_use_jit();
	if (UseJIT)
	    compiler_init(MacFrameBaseHost + VRAMSize);
#endif
	return true;
}


/*
 *  Deinitialize 680x0 emulation
 */

void Exit680x0(void)
{
#if USE_JIT
    if (UseJIT)
	compiler_exit();
#endif
	exit_m68k();
}

/*
 *  Reset and start 680x0 emulation (doesn't return)
 */

void Start680x0(void)
{
	m68k_reset();
#if USE_JIT
    if (UseJIT)
	m68k_compile_execute();
    else
#endif
	m68k_execute();
}


/*
 *  Trigger interrupt
 */

void TriggerInterrupt(void)
{
	idle_resume();
	SPCFLAGS_SET( SPCFLAG_INT );
}

void TriggerNMI(void)
{
	//!! not implemented yet
}


/*
 *  Get 68k interrupt level
 */

int intlev(void)
{
	return InterruptFlags ? 1 : 0;
}


/*
 *  Execute MacOS 68k trap
 *  r->a[7] and r->sr are unused!
 */

void Execute68kTrap(uint16 trap, struct M68kRegisters *r)
{
	int i;

	// Save old PC
	uaecptr oldpc = m68k_getpc();

	// Set registers
	for (i=0; i<8; i++)
		m68k_dreg(regs, i) = r->d[i];
	for (i=0; i<7; i++)
		m68k_areg(regs, i) = r->a[i];

	// Push trap and EXEC_RETURN on stack
	m68k_areg(regs, 7) -= 2;
	put_word(m68k_areg(regs, 7), M68K_EXEC_RETURN);
	m68k_areg(regs, 7) -= 2;
	put_word(m68k_areg(regs, 7), trap);

	// Execute trap
	m68k_setpc(m68k_areg(regs, 7));
	fill_prefetch_0();
	quit_program = false;
	m68k_execute();

	// Clean up stack
	m68k_areg(regs, 7) += 4;

	// Restore old PC
	m68k_setpc(oldpc);
	fill_prefetch_0();

	// Get registers
	for (i=0; i<8; i++)
		r->d[i] = m68k_dreg(regs, i);
	for (i=0; i<7; i++)
		r->a[i] = m68k_areg(regs, i);
	quit_program = false;
}


/*
 *  Execute 68k subroutine
 *  The executed routine must reside in UAE memory!
 *  r->a[7] and r->sr are unused!
 */

void Execute68k(uint32 addr, struct M68kRegisters *r)
{
	int i;

	// Save old PC
	uaecptr oldpc = m68k_getpc();

	// Set registers
	for (i=0; i<8; i++)
		m68k_dreg(regs, i) = r->d[i];
	for (i=0; i<7; i++)
		m68k_areg(regs, i) = r->a[i];

	// Push EXEC_RETURN and faked return address (points to EXEC_RETURN) on stack
	m68k_areg(regs, 7) -= 2;
	put_word(m68k_areg(regs, 7), M68K_EXEC_RETURN);
	m68k_areg(regs, 7) -= 4;
	put_long(m68k_areg(regs, 7), m68k_areg(regs, 7) + 4);

	// Execute routine
	m68k_setpc(addr);
	fill_prefetch_0();
	quit_program = false;
	m68k_execute();

	// Clean up stack
	m68k_areg(regs, 7) += 2;

	// Restore old PC
	m68k_setpc(oldpc);
	fill_prefetch_0();

	// Get registers
	for (i=0; i<8; i++)
		r->d[i] = m68k_dreg(regs, i);
	for (i=0; i<7; i++)
		r->a[i] = m68k_areg(regs, i);
	quit_program = false;
}
