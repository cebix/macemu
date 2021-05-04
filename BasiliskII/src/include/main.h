/*
 *  main.h - General definitions
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

#ifndef MAIN_H
#define MAIN_H

// CPU type (0 = 68000, 1 = 68010, 2 = 68020, 3 = 68030, 4 = 68040/060)
extern int CPUType;
extern bool CPUIs68060;		// Flag to distinguish 68040 and 68060

// FPU type (0 = no FPU, 1 = 68881, 2 = 68882)
extern int FPUType;

// Flag: 24-bit-addressing?
extern bool TwentyFourBitAddressing;

// 68k register structure (for Execute68k())
struct M68kRegisters {
	uint32 d[8];
#ifdef UPDATE_UAE
	memptr a[8];
	uint16 sr;
	memptr usp, isp, msp;
	memptr pc;
#else
	uint32 a[8];
	uint16 sr;
#endif
};

// General functions
extern bool InitAll(const char *vmdir);
extern void ExitAll(void);

// Platform-specific functions
extern void FlushCodeCache(void *start, uint32 size);	// Code was patched, flush caches if neccessary
extern void QuitEmulator(void);							// Quit emulator
extern void ErrorAlert(const char *text);				// Display error alert
extern void ErrorAlert(int string_id);
extern void WarningAlert(const char *text);				// Display warning alert
extern void WarningAlert(int string_id);
extern bool ChoiceAlert(const char *text, const char *pos, const char *neg);	// Display choice alert

// Mutexes (non-recursive)
struct B2_mutex;
extern B2_mutex *B2_create_mutex(void);
extern void B2_lock_mutex(B2_mutex *mutex);
extern void B2_unlock_mutex(B2_mutex *mutex);
extern void B2_delete_mutex(B2_mutex *mutex);

// Interrupt flags
enum {
	INTFLAG_60HZ = 1,	// 60.15Hz VBL
	INTFLAG_1HZ = 2,	// ~1Hz interrupt
	INTFLAG_SERIAL = 4,	// Serial driver
	INTFLAG_ETHER = 8,	// Ethernet driver
	INTFLAG_AUDIO = 16,	// Audio block read
	INTFLAG_TIMER = 32,	// Time Manager
	INTFLAG_ADB = 64,	// ADB
	INTFLAG_NMI = 128	// NMI
};

extern uint32 InterruptFlags;									// Currently pending interrupts
extern void SetInterruptFlag(uint32 flag);						// Set/clear interrupt flags
extern void ClearInterruptFlag(uint32 flag);

// vde switch variable
extern char* vde_sock;

// Array length
#if __cplusplus >= 201103L || (_MSC_VER >= 1900 && defined __cplusplus)
template <typename T, size_t size>
constexpr size_t lengthof(T (& a)[size])
{
	return size;
}
#else
#define lengthof(a) (sizeof(a) / sizeof(a[0]))
#endif

#endif
