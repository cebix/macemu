/*
 *  basilisk_glue.cpp - Glue UAE CPU to Basilisk II CPU engine interface
 *
 *  Basilisk II (C) 1997-1999 Christian Bauer
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
#include "emul_op.h"
#include "rom_patches.h"
#include "m68k.h"
#include "memory.h"
#include "readcpu.h"
#include "newcpu.h"
#include "compiler.h"


// RAM and ROM pointers
uint32 RAMBaseMac;			// RAM base (Mac address space)
uint8 *RAMBaseHost;			// RAM base (host address space)
uint32 RAMSize;				// Size of RAM
uint32 ROMBaseMac;			// ROM base (Mac address space)
uint8 *ROMBaseHost;			// ROM base (host address space)
uint32 ROMSize;				// Size of ROM

#if !REAL_ADDRESSING
// Mac frame buffer
uint8 *MacFrameBaseHost;	// Frame buffer base (host address space)
uint32 MacFrameSize;		// Size of frame buffer
int MacFrameLayout;			// Frame buffer layout
#endif

// From newcpu.cpp
extern int quit_program;


/*
 *  Initialize 680x0 emulation, CheckROM() must have been called first
 */

bool Init680x0(void)
{
#if REAL_ADDRESSING
	// Mac address space = host address space
	RAMBaseMac = (uint32)RAMBaseHost;
	ROMBaseMac = (uint32)ROMBaseHost;
#else
	// Initialize UAE memory banks
	RAMBaseMac = 0;
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

	init_m68k();
	compiler_init();
	return true;
}


/*
 *  Deinitialize 680x0 emulation
 */

void Exit680x0(void)
{
}


/*
 *  Reset and start 680x0 emulation (doesn't return)
 */

void Start680x0(void)
{
	m68k_reset();
	m68k_go(true);
}


/*
 *  Trigger interrupt
 */

void TriggerInterrupt(void)
{
	regs.spcflags |= SPCFLAG_INT;
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
	quit_program = 0;
	m68k_go(true);

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
	quit_program = 0;
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
	quit_program = 0;
	m68k_go(true);

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
	quit_program = 0;
}
