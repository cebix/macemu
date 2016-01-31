/*
 *  cpu_emulation.h - Definitions for Basilisk II CPU emulation module (native 68k version)
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

#ifndef CPU_EMULATION_H
#define CPU_EMULATION_H


/*
 *  Memory system
 */

// RAM and ROM pointers (allocated and set by main_*.cpp)
extern uaecptr RAMBaseMac;		// RAM base (Mac address space), does not include Low Mem when != 0
extern uae_u8* RAMBaseHost;		// RAM base (host address space)
extern uint32 RAMSize;			// Size of RAM

extern uaecptr ROMBaseMac;		// ROM base (Mac address space)
extern uae_u8* ROMBaseHost;		// ROM base (host address space)
extern uint32 ROMSize;			// Size of ROM

// Mac memory access functions
static inline uint32 ReadMacInt32(uaecptr addr) {return *(uint32*)addr;}
static inline uint32 ReadMacInt16(uaecptr addr) {return *(uint16*)addr;}
static inline uint32 ReadMacInt8(uaecptr addr) {return *(uint8*)addr;}
static inline void WriteMacInt32(uaecptr addr, uint32 l) {*(uint32*)addr = l;}
static inline void WriteMacInt16(uaecptr addr, uint16 w) {*(uint16*)addr = w;}
static inline void WriteMacInt8(uaecptr addr, uint8 b) {*(uint8*)addr = b;}
static inline void* Mac2HostAddr(uaecptr addr) {return (void*)addr;}
static inline uaecptr Host2MacAddr(const void* addr) {return (uaecptr)addr;}
static inline void* Mac_memset(uint32 addr, int c, size_t n) {return memset(Mac2HostAddr(addr), c, n);}
static inline void* Mac2Host_memcpy(void* dest, uaecptr src, size_t n) {return memcpy(dest, Mac2HostAddr(src), n);}
static inline void* Host2Mac_memcpy(uaecptr dest, const void* src, size_t n) {return memcpy(Mac2HostAddr(dest), src, n);}
static inline void* Mac2Mac_memcpy(uaecptr dest, uaecptr src, size_t n) {return memcpy(Mac2HostAddr(dest), Mac2HostAddr(src), n);}


/*
 *  680x0 emulation
 */

// 680x0 emulation functions
struct M68kRegisters;
extern void Start680x0(void);									// Reset and start 680x0
extern "C" void Execute68k(uaecptr addr, M68kRegisters *r);		// Execute 68k code from EMUL_OP routine
extern "C" void Execute68kTrap(uint16 trap, M68kRegisters *r);	// Execute MacOS 68k trap from EMUL_OP routine

// Interrupt functions
extern void TriggerInterrupt(void);								// Trigger interrupt (InterruptFlag must be set first)
extern void TriggerNMI(void);									// Trigger interrupt level 7

#endif
