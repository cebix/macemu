/*
 *  xlowmem.h - Definitions for extra Low Memory globals (0x2800..)
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

#ifndef XLOWMEM_H
#define XLOWMEM_H

// Modes for XLM_RUN_MODE
#define MODE_68K 0		// 68k emulator active
#define MODE_NATIVE 1	// Switched to native mode
#define MODE_EMUL_OP 2	// 68k emulator active, within EMUL_OP routine

#define XLM_SIGNATURE 0x2800			// SheepShaver signature
#define XLM_KERNEL_DATA 0x2804			// Pointer to Kernel Data
#define XLM_TOC 0x2808					// TOC pointer of emulator
#define XLM_SHEEP_OBJ 0x280c			// Pointer to SheepShaver object
#define XLM_RUN_MODE 0x2810				// Current run mode, see enum above
#define XLM_68K_R25 0x2814				// Contents of the 68k emulator's r25 (which contains the interrupt level), saved upon entering EMUL_OP mode, used by Execute68k() and the USR1 signal handler
#define XLM_IRQ_NEST 0x2818				// Interrupt disable nesting counter (>0: disabled)
#define XLM_PVR 0x281c					// Theoretical PVR
#define XLM_BUS_CLOCK 0x2820			// Bus clock speed in Hz (for DriverServicesLib patch)
#define XLM_EMUL_RETURN_PROC 0x2824		// Pointer to EMUL_RETURN routine
#define XLM_EXEC_RETURN_PROC 0x2828		// Pointer to EXEC_RETURN routine
#define XLM_EMUL_OP_PROC 0x282c			// Pointer to EMUL_OP routine
#define XLM_EMUL_RETURN_STACK 0x2830	// Stack pointer for EMUL_RETURN
#define XLM_RES_LIB_TOC 0x2834			// TOC pointer of Resources library
#define XLM_GET_RESOURCE 0x2838			// Pointer to native GetResource() routine
#define XLM_GET_1_RESOURCE 0x283c		// Pointer to native Get1Resource() routine
#define XLM_GET_IND_RESOURCE 0x2840		// Pointer to native GetIndResource() routine
#define XLM_GET_1_IND_RESOURCE 0x2844	// Pointer to native Get1IndResource() routine
#define XLM_R_GET_RESOURCE 0x2848		// Pointer to native RGetResource() routine
#define XLM_EXEC_RETURN_OPCODE 0x284c	// EXEC_RETURN opcode for Execute68k()
#define XLM_ZERO_PAGE 0x2850			// Pointer to read-only page with all bits set to 0
#define XLM_R13 0x2854					// Pointer to .sdata section (Linux)
#define XLM_GET_NAMED_RESOURCE 0x2858	// Pointer to native GetNamedResource() routine
#define XLM_GET_1_NAMED_RESOURCE 0x285c	// Pointer to native Get1NamedResource() routine

#define XLM_ETHER_AO_GET_HWADDR 0x28b0	// Pointer to ethernet A0_get_ethernet_address() function
#define XLM_ETHER_AO_ADD_MULTI 0x28b4	// Pointer to ethernet A0_enable_multicast() function
#define XLM_ETHER_AO_DEL_MULTI 0x28b8	// Pointer to ethernet A0_disable_multicast() function
#define XLM_ETHER_AO_SEND_PACKET 0x28bc	// Pointer to ethernet A0_transmit_packet() function
#define XLM_ETHER_INIT 0x28c0			// Pointer to ethernet InitStreamModule() function
#define XLM_ETHER_TERM 0x28c4			// Pointer to ethernet TerminateStreamModule() function
#define XLM_ETHER_OPEN 0x28c8			// Pointer to ethernet ether_open() function
#define XLM_ETHER_CLOSE 0x28cc			// Pointer to ethernet ether_close() function
#define XLM_ETHER_WPUT 0x28d0			// Pointer to ethernet ether_wput() function
#define XLM_ETHER_RSRV 0x28d4			// Pointer to ethernet ether_rsrv() function
#define XLM_VIDEO_DOIO 0x28d8			// Pointer to video DoDriverIO() function

#endif
