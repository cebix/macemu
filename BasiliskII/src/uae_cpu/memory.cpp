/*
 * memory.cpp - memory management
 *
 * Copyright (c) 2001-2004 Milan Jurik of ARAnyM dev team (see AUTHORS)
 * 
 * Inspired by Christian Bauer's Basilisk II
 *
 * This file is part of the ARAnyM project which builds a new and powerful
 * TOS/FreeMiNT compatible virtual machine running on almost any hardware.
 *
 * ARAnyM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ARAnyM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ARAnyM; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
 /*
  * UAE - The Un*x Amiga Emulator
  *
  * Memory management
  *
  * (c) 1995 Bernd Schmidt
  */

#include "sysdeps.h"

#include "memory.h"
#define DEBUG 0
#include "debug.h"

#ifdef ARAM_PAGE_CHECK
uaecptr pc_page = 0xeeeeeeee;
uintptr pc_offset = 0;
uaecptr read_page = 0xeeeeeeee;
uintptr read_offset = 0;
uaecptr write_page = 0xeeeeeeee;
uintptr write_offset = 0;
#endif

extern "C" void breakpt(void)
{
	// bug("bus err: pc=%08x, sp=%08x, addr=%08x", m68k_getpc(), regs.regs[15], regs.mmu_fault_addr);
}

#if !KNOWN_ALLOC && !NORMAL_ADDRESSING
// This part need rewrite for ARAnyM !!
// It can be taken from hatari.

#error Not prepared for your platform, maybe you need memory banks from hatari 

#endif /* !KNOWN_ALLOC && !NORMAL_ADDRESSING */
