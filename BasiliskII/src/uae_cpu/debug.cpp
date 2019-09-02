/*
 * debug.cpp - CPU debugger
 *
 * Copyright (c) 2001-2010 Milan Jurik of ARAnyM dev team (see AUTHORS)
 * 
 * Inspired by Bernd Schmidt's UAE
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
 * Debugger
 *
 * (c) 1995 Bernd Schmidt
 *
 */

#include "sysdeps.h"

#include "memory.h"
#include "newcpu.h"
#include "debug.h"

#include "input.h"
#include "cpu_emulation.h"

#include "main.h"

static int debugger_active = 0;
int debugging = 0;
int irqindebug = 0;

int ignore_irq = 0;


void activate_debugger (void)
{
#ifdef DEBUGGER
	ndebug::do_skip = false;
#endif
	debugger_active = 1;
	SPCFLAGS_SET( SPCFLAG_BRK );
	debugging = 1;
	/* use_debugger = 1; */
}

void deactivate_debugger(void)
{
	debugging = 0;
	debugger_active = 0;
}

void debug (void)
{
	if (ignore_irq && regs.s && !regs.m ) {
		SPCFLAGS_SET( SPCFLAG_BRK );
		return;
	}
#ifdef DEBUGGER
	ndebug::run();
#endif
}

/*
vim:ts=4:sw=4:
*/
