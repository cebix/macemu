/*
 *  main.h - Emulation core
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

#ifndef MAIN_H
#define MAIN_H

// Global variables
extern void *TOC;				// TOC pointer
extern void *R13;				// r13 register
extern uint32 KernelDataAddr;	// Address of Kernel Data
extern uint32 BootGlobsAddr;	// Address of BootGlobs structure at top of Mac RAM
extern uint32 PVR;				// Theoretical PVR
extern int64 CPUClockSpeed;		// Processor clock speed (Hz)
extern int64 BusClockSpeed;		// Bus clock speed (Hz)
extern int64 TimebaseSpeed;		// Timebase clock speed (Hz)

#ifdef __BEOS__
extern system_info SysInfo;		// System information
#endif

// 68k register structure (for Execute68k())
struct M68kRegisters {
	uint32 d[8];
	uint32 a[8];
};


// Functions
extern bool InitAll(void);
extern void ExitAll(void);
extern void Dump68kRegs(M68kRegisters *r);					// Dump 68k registers
extern void MakeExecutable(int dummy, uint32 start, uint32 length);	// Make code executable
extern void PatchAfterStartup(void);						// Patches after system startup
extern void QuitEmulator(void);								// Quit emulator (must only be called from main thread)
extern void ErrorAlert(const char *text);					// Display error alert
extern void WarningAlert(const char *text);					// Display warning alert
extern bool ChoiceAlert(const char *text, const char *pos, const char *neg);	// Display choice alert

// Mutexes (non-recursive)
struct B2_mutex;
extern B2_mutex *B2_create_mutex(void);
extern void B2_lock_mutex(B2_mutex *mutex);
extern void B2_unlock_mutex(B2_mutex *mutex);
extern void B2_delete_mutex(B2_mutex *mutex);

// Interrupt flags
enum {
	INTFLAG_VIA = 1,	// 60.15Hz VBL
	INTFLAG_SERIAL = 2,	// Serial driver
	INTFLAG_ETHER = 4,	// Ethernet driver
	INTFLAG_AUDIO = 16,	// Audio block read
	INTFLAG_TIMER = 32,	// Time Manager
	INTFLAG_ADB = 64	// ADB
};

extern volatile uint32 InterruptFlags;						// Currently pending interrupts
extern void SetInterruptFlag(uint32);
extern void ClearInterruptFlag(uint32);
extern void TriggerInterrupt(void);							// Trigger SIGUSR1 interrupt in emulator thread
extern void DisableInterrupt(void);							// Disable SIGUSR1 interrupt (can be nested)
extern void EnableInterrupt(void);							// Enable SIGUSR1 interrupt (can be nested)

#endif
