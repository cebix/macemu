/*
 *  cpu_emulation.h - Definitions for CPU emulation and Mac memory access
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

#ifndef CPU_EMULATION_H
#define CPU_EMULATION_H


/*
 *  Memory system
 */

// Constants
const uint32 ROM_BASE = 0x40800000;			// Base address of ROM
const uint32 ROM_SIZE = 0x00400000;			// Size of ROM file
const uint32 ROM_AREA_SIZE = 0x500000;		// Size of ROM area
const uint32 ROM_END = ROM_BASE + ROM_SIZE;	// End of ROM
const uint32 DR_CACHE_BASE = 0x69000000;	// Address of DR cache
const uint32 DR_CACHE_SIZE = 0x80000;		// Size of DR Cache
const uint32 SHEEP_BASE = 0x60000000;		// Address of SheepShaver data
const uint32 SHEEP_SIZE = 0x40000;			// Size of SheepShaver data

const uint32 KERNEL_DATA_BASE = 0x68ffe000;	// Address of Kernel Data
const uint32 KERNEL_DATA2_BASE = 0x5fffe000;// Alternate address of Kernel Data
const uint32 KERNEL_AREA_SIZE = 0x2000;		// Size of Kernel Data area

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
extern uint32 SheepStack1Base;	// SheepShaver first alternate stack base
extern uint32 SheepStack2Base;	// SheepShaver second alternate stack base
extern uint32 SheepThunksBase;	// SheepShaver thunks base

// Mac memory access functions
static inline uint32 ReadMacInt8(uint32 addr) {return *(uint8 *)addr;}
static inline void WriteMacInt8(uint32 addr, uint32 b) {*(uint8 *)addr = b;}
#ifdef __i386__
static inline uint32 ReadMacInt16(uint32 addr) {uint32 retval; __asm__ ("movzwl %w1,%k0\n\tshll $16,%k0\n\tbswapl %k0\n" : "=&r" (retval) : "m" (*(uint16 *)addr) : "cc"); return retval;}
static inline uint32 ReadMacInt32(uint32 addr) {uint32 retval; __asm__ ("bswap %0" : "=r" (retval) : "0" (*(uint32 *)addr) : "cc"); return retval;}
static inline uint64 ReadMacInt64(uint32 addr) {return ((uint64)ReadMacInt32(addr) << 32) | ReadMacInt32(addr + 4);}
static inline void WriteMacInt16(uint32 addr, uint32 w) {__asm__ ("bswapl %0" : "=&r" (w) : "0" (w << 16) : "cc"); *(uint16 *)addr = w;}
static inline void WriteMacInt32(uint32 addr, uint32 l) {__asm__ ("bswap %0" : "=r" (l) : "0" (l) : "cc"); *(uint32 *)addr = l;}
static inline void WriteMacInt64(uint32 addr, uint64 ll) {WriteMacInt32(addr, ll >> 32); WriteMacInt32(addr, ll);}
#else
static inline uint32 ReadMacInt16(uint32 addr) {return *(uint16 *)addr;}
static inline uint32 ReadMacInt32(uint32 addr) {return *(uint32 *)addr;}
static inline uint64 ReadMacInt64(uint32 addr) {return *(uint64 *)addr;}
static inline void WriteMacInt16(uint32 addr, uint32 w) {*(uint16 *)addr = w;}
static inline void WriteMacInt32(uint32 addr, uint32 l) {*(uint32 *)addr = l;}
static inline void WriteMacInt64(uint32 addr, uint64 ll) {*(uint64 *)addr = ll;}
#endif
static inline uint8 *Mac2HostAddr(uint32 addr) {return (uint8 *)addr;}
static inline void *Mac_memset(uint32 addr, int c, size_t n) {return memset(Mac2HostAddr(addr), c, n);}
static inline void *Mac2Host_memcpy(void *dest, uint32 src, size_t n) {return memcpy(dest, Mac2HostAddr(src), n);}
static inline void *Host2Mac_memcpy(uint32 dest, const void *src, size_t n) {return memcpy(Mac2HostAddr(dest), src, n);}
static inline void *Mac2Mac_memcpy(uint32 dest, uint32 src, size_t n) {return memcpy(Mac2HostAddr(dest), Mac2HostAddr(src), n);}


/*
 *  680x0 and PPC emulation
 */

struct M68kRegisters;
extern void Execute68k(uint32, M68kRegisters *r);			// Execute 68k subroutine from EMUL_OP routine, must be ended with RTS
extern void Execute68kTrap(uint16 trap, M68kRegisters *r);	// Execute 68k A-Trap from EMUL_OP routine
#if EMULATED_PPC
extern void ExecuteNative(int selector);					// Execute native code from EMUL_OP routine (real mode switch)
#else
extern void ExecutePPC(void (*func)(void));					// Execute PPC code from EMUL_OP routine (real mode switch)
#endif

#endif
