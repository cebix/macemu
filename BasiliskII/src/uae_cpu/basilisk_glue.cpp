/*
 *  basilisk_glue.cpp - Glue UAE CPU to Basilisk II CPU engine interface
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

#include "sysdeps.h"

#include "cpu_emulation.h"
#include "main.h"
#include "prefs.h"
#include "emul_op.h"
#include "rom_patches.h"
#include "timer.h"
#include "m68k.h"
#include "memory.h"
#include "readcpu.h"
#include "newcpu.h"
#include "compiler/compemu.h"


// RAM and ROM pointers
uint32 RAMBaseMac = 0;		// RAM base (Mac address space) gb-- initializer is important
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

#if DIRECT_ADDRESSING
uintptr MEMBaseDiff;		// Global offset between a Mac address and its Host equivalent
#endif

#if USE_JIT
bool UseJIT = false;
#endif

// From newcpu.cpp
extern bool quit_program;


/*
 *  Initialize 680x0 emulation, CheckROM() must have been called first
 */

bool Init680x0(void)
{
#if REAL_ADDRESSING
	// Mac address space = host address space
	RAMBaseMac = (uintptr)RAMBaseHost;
	ROMBaseMac = (uintptr)ROMBaseHost;
#elif DIRECT_ADDRESSING
	// Mac address space = host address space minus constant offset (MEMBaseDiff)
	// NOTE: MEMBaseDiff is set up in main_unix.cpp/main()
	RAMBaseMac = 0;
	ROMBaseMac = Host2MacAddr(ROMBaseHost);
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
#if USE_JIT
	UseJIT = compiler_use_jit();
	if (UseJIT)
	    compiler_init();
#endif
	return true;
}


/*
 *  Deinitialize 680x0 emulation
 */

void Exit680x0(void)
{
#if USE_JIT
    if (UseJIT)
	compiler_exit();
#endif
	exit_m68k();
}


/*
 *  Initialize memory mapping of frame buffer (called upon video mode change)
 */

void InitFrameBufferMapping(void)
{
#if !REAL_ADDRESSING && !DIRECT_ADDRESSING
	memory_init();
#endif
}

/*
 *  Reset and start 680x0 emulation (doesn't return)
 */

void Start680x0(void)
{
	m68k_reset();
#if USE_JIT
    if (UseJIT)
	m68k_compile_execute();
    else
#endif
	m68k_execute();
}


/*
 *  Trigger interrupt
 */

void TriggerInterrupt(void)
{
	idle_resume();
	SPCFLAGS_SET( SPCFLAG_INT );
}

void TriggerNMI(void)
{
	//!! not implemented yet
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
	quit_program = false;
	m68k_execute();

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
	quit_program = false;
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
	quit_program = false;
	m68k_execute();

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
	quit_program = false;
}

#ifdef ENABLE_ASC_EMU

#include <asc.h>

static uae_u8 ASCRegs[0x2000] = {0};
static const int fifoCapacity = 2048;
static uint32 fifoInA = 0;
static uint32 fifoWriteA = 0;
static uint32 fifoOutA = 0;
static uae_u8 fifoA[fifoCapacity];
static int underrun = 0;
static int clearFifo = 0;
static int32 ascBufferSize = -1;
static int soundRunning = 0;
static uae_u8 zeros[1024] = {0};

extern uae_u32 io_read(uaecptr addr, int width_bits) {
	if((addr & 0x00ff000) == 0x0014000) {
		// Apple Sound Chip

		uaecptr offset = addr & 0x00000fff;
		uae_u32 val;

		if(offset < 0x400) {
			return 0;
		} else if(offset < 0x800) {
			return 0;
		} else {
			if(width_bits > 8) {
				fprintf(stderr,
					"Unexpected ASC read width %d\n", width_bits);
				return 0;
			}

			switch(offset) {
			case 0x800:
				// VERSION
				return 0;

			case 0x804:
				// FIFO IRQ STATUS
				val = 0;
				if((fifoInA - fifoWriteA) >= 0x200) {
					val = 0x1;
				}
				if((fifoInA - fifoWriteA) >= 0x400) {
					val = 0x2;
				}

				val |= (val << 2);

				return val;

			default:
				return ASCRegs[offset];
				break;
			}
		}
    
	}

	return 0;
}

extern void io_write(uaecptr addr, uae_u32 b, int width_bits) {
	static int downsample = 0;

	if((addr & 0x00ff000) == 0x0014000) {
		// Apple Sound Chip
		if(width_bits > 8) {
			fprintf(stderr,
				"Unexpected ASC read width %d, addr 0x%08x\n",
				width_bits, addr);
			return;
		}

		uaecptr offset = addr & 0x00000fff;
		uae_u32 val;

		if(offset < 0x400) {
			if(ASCRegs[0x801] != 2) {
				static int counter = 0;
				static int32 depthA = fifoInA - fifoWriteA;

				// FIFO Mode
				if(depthA == fifoCapacity) {
					return;
				}

				if(ASCRegs[0x807] == 0) {
					downsample += 22050;
					if(downsample >= 22257) {
						downsample -= 22257;
						fifoA[(fifoInA++) % fifoCapacity] = b;
					}
				}
			}

		} else if(offset < 0x800) {
		} else {
			switch(offset) {
			case 0x801:
				// MODE
				// 1 = FIFO mode, 2 = wavetable mode
				ASCRegs[0x801] = b & 0x03;
				break;

			case 0x802:
				// CONTROL
				// bit 0: analog or PWM output
				// bit 1: stereo/mono
				// bit 7: processing time exceeded
				ASCRegs[0x802] = b;
				break;

			case 0x803:
				// FIFO Mode 
				if((b & 0x80) && (underrun == 0)) {
					if(fifoInA > (fifoWriteA + ascBufferSize)) {
						fifoInA = 0;
						clearFifo = 1;
					}
				}
				break;

			case 0x804:
				// fifo status
				break;

			case 0x805:
				// wavetable control
				break;

			case 0x806:
				// Volume
				break;

			case 0x807:
				// Clock rate 0 = 22257, 2 = 22050, 3 = 44100
				{
					int newRate, oldRate;

					if(ASCRegs[0x807] == 3) {
						oldRate = 44100;
					} else {
						oldRate = 22050;
					}

					if(b == 3) {
						newRate = 44100;
					} else {
						newRate = 22050;
					}

					if(newRate != oldRate) {
						asc_stop();
						soundRunning = 0;
					}

					if(soundRunning == 0) {
						int32 depthA = fifoInA - fifoWriteA;

						soundRunning = 1;
						downsample = 0;
						if(zeros[0] == 0) {
							memset(zeros, 128, sizeof(zeros));
						}

						asc_init(newRate);

						ascBufferSize = asc_get_buffer_size();

						if(depthA >= ascBufferSize) {
							asc_process_samples(&fifoA[fifoWriteA % fifoCapacity],
									    ascBufferSize);
							fifoWriteA += ascBufferSize;
							underrun = 0;
						} else {
							underrun = 1;
							asc_process_samples(zeros, ascBufferSize);
						}
						
					}
				}

				ASCRegs[0x807] = b;
				break;

			case 0x80f:
				printf("ASC Test\n");
				break;

			default:
				break;
			}

			if(soundRunning == 0) {
				int32 depthA = fifoInA - fifoWriteA;

				soundRunning = 1;
				downsample = 0;

				if(zeros[0] == 0) {
					memset(zeros, 128, sizeof(zeros));
				}

				asc_init(22050);

				ascBufferSize = asc_get_buffer_size();

				if(depthA >= ascBufferSize) {
					asc_process_samples(&fifoA[fifoWriteA % fifoCapacity],
							    ascBufferSize);
					fifoWriteA += ascBufferSize;
					underrun = 0;
				} else {
					underrun = 1;
					asc_process_samples(zeros, ascBufferSize);
				}
						
			}

		}
	}

}
	
void asc_callback() {
	if(soundRunning == 0) {
		asc_process_samples(zeros, ascBufferSize);
		return;
	}

	if(clearFifo) {
		fifoWriteA = 0;
		clearFifo = 0;
	}

	if((fifoInA > fifoWriteA) &&
	   ((fifoInA - fifoWriteA) >= ascBufferSize)) {
		asc_process_samples(&fifoA[fifoWriteA % fifoCapacity], ascBufferSize);
		fifoWriteA += ascBufferSize;
		underrun = 0;
	} else {
		underrun = 1;
		asc_process_samples(zeros, ascBufferSize);
	}
}
#endif
