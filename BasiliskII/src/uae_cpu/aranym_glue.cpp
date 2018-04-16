/*
 * aranym_glue.cpp - CPU interface
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

#include "sysdeps.h"

#include "cpu_emulation.h"
#include "newcpu.h"
#include "hardware.h"
#include "input.h"
#ifdef USE_JIT
# include "compiler/compemu.h"
#endif
#include "nf_objs.h"

#include "debug.h"

// RAM and ROM pointers
memptr RAMBase = 0;	// RAM base (Atari address space) gb-- init is important
uint8 *RAMBaseHost;	// RAM base (host address space)
uint32 RAMSize = 0x00e00000;	// Size of RAM

memptr ROMBase = 0x00e00000;	// ROM base (Atari address space)
uint8 *ROMBaseHost;	// ROM base (host address space)
uint32 ROMSize = 0x00100000;	// Size of ROM

uint32 RealROMSize;	// Real size of ROM

memptr HWBase = 0x00f00000;	// HW base (Atari address space)
uint8 *HWBaseHost;	// HW base (host address space)
uint32 HWSize = 0x00100000;    // Size of HW space

memptr FastRAMBase = 0x01000000;		// Fast-RAM base (Atari address space)
uint8 *FastRAMBaseHost;	// Fast-RAM base (host address space)

#ifdef HW_SIGSEGV
uint8 *FakeIOBaseHost;
#endif

#ifdef FIXED_VIDEORAM
memptr VideoRAMBase = ARANYMVRAMSTART;  // VideoRAM base (Atari address space)
#else
memptr VideoRAMBase;                    // VideoRAM base (Atari address space)
#endif
uint8 *VideoRAMBaseHost;// VideoRAM base (host address space)
//uint32 VideoRAMSize;	// Size of VideoRAM

#ifndef NOT_MALLOC
uintptr MEMBaseDiff;	// Global offset between a Atari address and its Host equivalent
uintptr ROMBaseDiff;
uintptr FastRAMBaseDiff;
#endif

uintptr VMEMBaseDiff;	// Global offset between a Atari VideoRAM address and /dev/fb0 mmap

// From newcpu.cpp
extern int quit_program;

#if defined(ENABLE_EXCLUSIVE_SPCFLAGS) && !defined(HAVE_HARDWARE_LOCKS)
SDL_mutex *spcflags_lock;
#endif
#if defined(ENABLE_REALSTOP)
SDL_cond *stop_condition;
#endif


/*
 *  Initialize 680x0 emulation
 */

bool InitMEM() {
	InitMEMBaseDiff(RAMBaseHost, RAMBase);
	InitROMBaseDiff(ROMBaseHost, ROMBase);
	InitFastRAMBaseDiff(FastRAMBaseHost, FastRAMBase);
	InitVMEMBaseDiff(VideoRAMBaseHost, VideoRAMBase);
	return true;
}

bool Init680x0(void)
{
	init_m68k();

#if defined(ENABLE_EXCLUSIVE_SPCFLAGS) && !defined(HAVE_HARDWARE_LOCKS)
    if ((spcflags_lock = SDL_CreateMutex()) ==  NULL) {
		panicbug("Error by SDL_CreateMutex()");
		exit(EXIT_FAILURE);
    }
#endif

#if ENABLE_REALSTOP
    if ((stop_condition = SDL_CreateCond()) ==  NULL) {
		panicbug("Error by SDL_CreateCond()");
		exit(EXIT_FAILURE);
    }
#endif

#ifdef USE_JIT
	if (bx_options.jit.jit) compiler_init();
#endif
	return true;
}

/*
 * Instr. RESET
 */

void AtariReset(void)
{
	// reset Atari hardware here
	HWReset();
	// reset NatFeats here
	NFReset();
	// reset the input devices (input.cpp)
	InputReset();

}

/*
 * Reset CPU
 */

void Reset680x0(void)
{
	m68k_reset();
}

/*
 *  Deinitialize 680x0 emulation
 */

void Exit680x0(void)
{
#ifdef USE_JIT
	if (bx_options.jit.jit) compiler_exit();
#endif
	exit_m68k();
}


/*
 *  Reset and start 680x0 emulation
 */

void Start680x0(void)
{
	m68k_reset();
#ifdef USE_JIT
	if (bx_options.jit.jit) {
		m68k_compile_execute();
	}
	else
#endif
		m68k_execute();
}

/*
 * Restart running 680x0 emulation safely from different thread
 */
void Restart680x0(void)
{
	quit_program = 2;
	TriggerNMI();
}

/*
 * Quit 680x0 emulation safely from different thread
 */
void Quit680x0(void)
{
	quit_program = 1;
	TriggerNMI();
}


int MFPdoInterrupt(void)
{
	return getMFP()->doInterrupt();
}

int SCCdoInterrupt(void)
{
	return getSCC()->doInterrupt();
}

/*
 *  Trigger interrupts
 */
void TriggerInternalIRQ(void)
{
	SPCFLAGS_SET( SPCFLAG_INTERNAL_IRQ );
}

void TriggerInt3(void)
{
	SPCFLAGS_SET( SPCFLAG_INT3 );
}

void TriggerVBL(void)
{
	SPCFLAGS_SET( SPCFLAG_VBL );
}

void TriggerInt5(void)
{
	SPCFLAGS_SET( SPCFLAG_INT5 );
}

void TriggerSCC(bool enable)
{
	if (enable)
		SPCFLAGS_SET( SPCFLAG_SCC );
	else
		SPCFLAGS_CLEAR( SPCFLAG_SCC );
}

void TriggerMFP(bool enable)
{
	if (enable)
		SPCFLAGS_SET( SPCFLAG_MFP );
	else
		SPCFLAGS_CLEAR( SPCFLAG_MFP );
}

void TriggerNMI(void)
{
	SPCFLAGS_SET( SPCFLAG_BRK ); // use _BRK for NMI
}

#ifndef REBOOT_OR_HALT
#define REBOOT_OR_HALT	0	// halt by default
#endif

#if REBOOT_OR_HALT == 1
#  define CPU_MSG		"CPU: Rebooting"
#  define CPU_ACTION	Restart680x0()
#else
#  define CPU_MSG		"CPU: Halting"
#  define CPU_ACTION	Quit680x0()
#endif

#ifdef ENABLE_EPSLIMITER

#ifndef EPS_LIMIT
#  define EPS_LIMIT		10000	/* this might be too high if ARAnyM is slowed down by printing the bus errors on console */
#endif

void check_eps_limit(uaecptr pc)
{
	static long last_exception_time=-1;
	static long exception_per_sec=0;
	static long exception_per_sec_pc=0;
	static uaecptr prevpc = 0;

	if (bx_options.cpu.eps_enabled) {
		if (last_exception_time == -1) {
			last_exception_time = SDL_GetTicks();
		}

		exception_per_sec++;

		if (pc == prevpc) {
			/* BUS ERRORs occur at the same PC - watch out! */
			exception_per_sec_pc++;
		}
		else {
			exception_per_sec_pc = 0;
			prevpc = pc;
		}

		if (SDL_GetTicks() - last_exception_time > 1000) {
			last_exception_time = SDL_GetTicks();
			if (exception_per_sec_pc > bx_options.cpu.eps_max ||
				exception_per_sec > EPS_LIMIT /* make it configurable */) {
				panicbug("CPU: Exception per second limit reached: %ld/%ld",
					exception_per_sec_pc, exception_per_sec);
				/* would be cool to open SDL dialog here: */
				/* [Exception per seconds limit reached. XXXXX exception
				    occured in the last second. The limit is set to YYYYY
				    in your config file. Do you want to continue emulation,
				    reset ARAnyM or quit ?][Continue] [Reset] [Quit]
				*/
				panicbug(CPU_MSG);
				CPU_ACTION;
			}
			exception_per_sec = 0;
			exception_per_sec_pc = 0;
		}
	}
}
#endif

void report_double_bus_error()
{
	panicbug("CPU: Double bus fault detected !");
	/* would be cool to open SDL dialog here: */
	/* [Double bus fault detected. The emulated system crashed badly.
	    Do you want to reset ARAnyM or quit ?] [Reset] [Quit]"
	*/
	panicbug(CPU_MSG);
	CPU_ACTION;
}

#ifdef FLIGHT_RECORDER
extern bool cpu_flight_recorder_active;
void cpu_flight_recorder(int activate) { cpu_flight_recorder_active = activate; }
#endif
