/*
 *  cpu_emulation.h - Definitions for Basilisk II CPU emulation module (Apple PowerMac ROM 680x0 emulator version (BeOS/PPC))
 *
 *  Basilisk II (C) 1997-2000 Christian Bauer
 */

#ifndef CPU_EMULATION_H
#define CPU_EMULATION_H


/*
 *  Memory system
 */

// RAM and ROM pointers (allocated and set by main_*.cpp)
extern uint32 RAMBaseMac;		// RAM base (Mac address space), does not include Low Mem when != 0
extern uint8 *RAMBaseHost;		// RAM base (host address space)
extern uint32 RAMSize;			// Size of RAM

extern uint32 ROMBaseMac;		// ROM base (Mac address space)
extern uint8 *ROMBaseHost;		// ROM base (host address space)
extern uint32 ROMSize;			// Size of ROM

// Mac memory access functions
static inline uint32 ReadMacInt32(uint32 addr) {return ntohl(*(uint32 *)addr);}
static inline uint32 ReadMacInt16(uint32 addr) {return ntohs(*(uint16 *)addr);}
static inline uint32 ReadMacInt8(uint32 addr) {return *(uint8 *)addr;}
static inline void WriteMacInt32(uint32 addr, uint32 l) {*(uint32 *)addr = htonl(l);}
static inline void WriteMacInt16(uint32 addr, uint32 w) {*(uint16 *)addr = htons(w);}
static inline void WriteMacInt8(uint32 addr, uint32 b) {*(uint8 *)addr = b;}
static inline uint8 *Mac2HostAddr(uint32 addr) {return (uint8 *)addr;}


/*
 *  680x0 emulation
 */

// Initialization
extern bool Init680x0(void);	// This routine may want to look at CPUType/FPUType to set up the apropriate emulation
extern void Exit680x0(void);

// 680x0 emulation functions
struct M68kRegisters;
extern void Start680x0(void);									// Reset and start 680x0
extern "C" void Execute68k(uint32 addr, M68kRegisters *r);		// Execute 68k code from EMUL_OP routine
extern "C" void Execute68kTrap(uint16 trap, M68kRegisters *r);	// Execute MacOS 68k trap from EMUL_OP routine

// Interrupt functions
extern void TriggerInterrupt(void);								// Trigger interrupt (InterruptFlag must be set first)

#endif
