/*
 *  cpu_emulation.h - Definitions for CPU emulation and Mac memory access
 *
 *  SheepShaver (C) 1997-2005 Christian Bauer and Marc Hellwig
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

#ifndef CPU_EMULATION_H
#define CPU_EMULATION_H


/*
 *  Memory system
 */

// Constants
const uintptr ROM_BASE = 0x40800000;			// Base address of ROM
const uint32  ROM_SIZE = 0x400000;				// Size of ROM file
const uint32  ROM_AREA_SIZE = 0x500000;			// Size of ROM area
const uintptr ROM_END = ROM_BASE + ROM_SIZE;	// End of ROM
const uintptr DR_EMULATOR_BASE = 0x68070000;	// Address of DR emulator code
const uint32  DR_EMULATOR_SIZE = 0x10000;		// Size of DR emulator code
const uintptr DR_CACHE_BASE = 0x69000000;		// Address of DR cache
const uint32  DR_CACHE_SIZE = 0x80000;			// Size of DR Cache

const uintptr KERNEL_DATA_BASE = 0x68ffe000;	// Address of Kernel Data
const uintptr KERNEL_DATA2_BASE = 0x5fffe000;	// Alternate address of Kernel Data
const uint32  KERNEL_AREA_SIZE = 0x2000;		// Size of Kernel Data area

// MacOS 68k Emulator Data
struct EmulatorData {
	uint32	v[0x400];
};

// MacOS Kernel Data
struct KernelData {
	uint32	v[0x400];
	EmulatorData ed;
};

// RAM and ROM pointers (allocated and set by main_*.cpp)
extern uint32 RAMBase;			// Base address of Mac RAM
extern uint32 RAMSize;			// Size address of Mac RAM
extern uint8 *RAMBaseHost;		// Base address of Mac RAM (host address space)
extern uint8 *ROMBaseHost;		// Base address of Mac ROM (host address space)

// Mac memory access functions
#if EMULATED_PPC
#include "cpu/vm.hpp"
static inline uint32 ReadMacInt8(uint32 addr) {return vm_read_memory_1(addr);}
static inline void WriteMacInt8(uint32 addr, uint32 v) {vm_write_memory_1(addr, v);}
static inline uint32 ReadMacInt16(uint32 addr) {return vm_read_memory_2(addr);}
static inline void WriteMacInt16(uint32 addr, uint32 v) {vm_write_memory_2(addr, v);}
static inline uint32 ReadMacInt32(uint32 addr) {return vm_read_memory_4(addr);}
static inline void WriteMacInt32(uint32 addr, uint32 v) {vm_write_memory_4(addr, v);}
static inline uint64 ReadMacInt64(uint32 addr) {return vm_read_memory_8(addr);}
static inline void WriteMacInt64(uint32 addr, uint64 v) {vm_write_memory_8(addr, v);}
static inline uint32 Host2MacAddr(uint8 *addr) {return vm_do_get_virtual_address(addr);}
static inline uint8 *Mac2HostAddr(uint32 addr) {return vm_do_get_real_address(addr);}
static inline void *Mac_memset(uint32 addr, int c, size_t n) {return vm_memset(addr, c, n);}
static inline void *Mac2Host_memcpy(void *dest, uint32 src, size_t n) {return vm_memcpy(dest, src, n);}
static inline void *Host2Mac_memcpy(uint32 dest, const void *src, size_t n) {return vm_memcpy(dest, src, n);}
static inline void *Mac2Mac_memcpy(uint32 dest, uint32 src, size_t n) {return vm_memcpy(dest, src, n);}
#else
static inline uint32 ReadMacInt8(uint32 addr) {return *(uint8 *)addr;}
static inline void WriteMacInt8(uint32 addr, uint32 b) {*(uint8 *)addr = b;}
static inline uint32 ReadMacInt16(uint32 addr) {return *(uint16 *)addr;}
static inline uint32 ReadMacInt32(uint32 addr) {return *(uint32 *)addr;}
static inline uint64 ReadMacInt64(uint32 addr) {return *(uint64 *)addr;}
static inline void WriteMacInt16(uint32 addr, uint32 w) {*(uint16 *)addr = w;}
static inline void WriteMacInt32(uint32 addr, uint32 l) {*(uint32 *)addr = l;}
static inline void WriteMacInt64(uint32 addr, uint64 ll) {*(uint64 *)addr = ll;}
static inline uint32 Host2MacAddr(uint8 *addr) {return (uint32)addr;}
static inline uint8 *Mac2HostAddr(uint32 addr) {return (uint8 *)addr;}
static inline void *Mac_memset(uint32 addr, int c, size_t n) {return memset(Mac2HostAddr(addr), c, n);}
static inline void *Mac2Host_memcpy(void *dest, uint32 src, size_t n) {return memcpy(dest, Mac2HostAddr(src), n);}
static inline void *Host2Mac_memcpy(uint32 dest, const void *src, size_t n) {return memcpy(Mac2HostAddr(dest), src, n);}
static inline void *Mac2Mac_memcpy(uint32 dest, uint32 src, size_t n) {return memcpy(Mac2HostAddr(dest), Mac2HostAddr(src), n);}
#endif


/*
 *  680x0 and PPC emulation
 */

// 68k procedure helper to write a big endian 16-bit word
#ifdef WORDS_BIGENDIAN
#define PW(W) W
#else
#define PW(X) ((((X) >> 8) & 0xff) | (((X) & 0xff) << 8))
#endif

// PowerPC procedure helper to write a big-endian 32-bit word
#ifdef WORDS_BIGENDIAN
#define PL(X) X
#else
#define PL(X)													\
     ((((X) & 0xff000000) >> 24) | (((X) & 0x00ff0000) >>  8) |	\
      (((X) & 0x0000ff00) <<  8) | (((X) & 0x000000ff) << 24))
#endif

struct M68kRegisters;
extern void Execute68k(uint32, M68kRegisters *r);			// Execute 68k subroutine from EMUL_OP routine, must be ended with RTS
extern void Execute68kTrap(uint16 trap, M68kRegisters *r);	// Execute 68k A-Trap from EMUL_OP routine
#if EMULATED_PPC
extern void FlushCodeCache(uintptr start, uintptr end);		// Invalidate emulator caches
#endif
extern void ExecuteNative(int selector);					// Execute native code from EMUL_OP routine (real mode switch)

#endif
