/*
 *  asm_support.s - Utility functions in assemmbly language
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

		.file	"asm_support.s"
		.text

		.globl	_m68k_sync_icache
		.globl	_Start680x0__Fv
		.globl	_SetInterruptFlag__FUi
		.globl	_ClearInterruptFlag__FUi
		.globl	_Execute68k
		.globl	_Execute68kTrap
		.globl	_Scod060Patch1
		.globl	_Scod060Patch2
		.globl	_ThInitFPUPatch
		.globl	_EmulOpTrampoline

		.globl	_RAMBaseHost
		.globl	_ROMBaseHost
		.globl	_EmulOp__FUsP13M68kRegisters
		.globl	_EmulatedSR
		.globl	_InterruptFlags
		.globl	_TriggerInterrupt__Fv


/*
 *  Call m68k_sync_icache() (NetBSD, the version in libm68k is broken)
 */

		.type	_m68k_sync_icache,@function
_m68k_sync_icache:
		movl	sp@(8),d1
		movl	sp@(4),a1
		movl	#0x80000004,d0
		trap	#12
		rts


/*
 *  Jump to Mac ROM, start emulation
 */

		.type	_Start680x0__Fv,@function
_Start680x0__Fv:
		movl	_RAMBaseHost,a0
		addl	#0x8000,a0
		movl	a0,sp
		movl	_ROMBaseHost,a0
		lea	a0@(0x2a),a0
		jmp	a0@


/*
 *  Set/clear interrupt flag (atomically)
 */

		.type	_SetInterruptFlag__FUi,@function
_SetInterruptFlag__FUi:
		movl	sp@(4),d0
		orl	d0,_InterruptFlags
		rts

		.type	_ClearInterruptFlag__FUi,@function
_ClearInterruptFlag__FUi:
		movl	sp@(4),d0
		notl	d0
		andl	d0,_InterruptFlags
		rts


/*
 *  Execute 68k subroutine (must be ended with rts)
 *  r->a[7] and r->sr are unused!
 */

/* void Execute68k(uint32 addr, M68kRegisters *r); */
		.type	_Execute68k,@function
_Execute68k:	movl	sp@(4),d0		|Get arguments
		movl	sp@(8),a0

		movml	d2-d7/a2-a6,sp@-	|Save registers

		movl	a0,sp@-			|Push pointer to M68kRegisters on stack
		pea	exec68kret		|Push return address on stack
		movl	d0,sp@-			|Push pointer to 68k routine on stack
		movml	a0@,d0-d7/a0-a6		|Load registers from M68kRegisters

		rts				|Jump into 68k routine

exec68kret:	movl	a6,sp@-			|Save a6
		movl	sp@(4),a6		|Get pointer to M68kRegisters
		movml	d0-d7/a0-a5,a6@		|Save d0-d7/a0-a5 to M68kRegisters
		movl	sp@+,a6@(56)		|Save a6 to M68kRegisters
		addql	#4,sp			|Remove pointer from stack

		movml	sp@+,d2-d7/a2-a6	|Restore registers
		rts


/*
 *  Execute MacOS 68k trap
 *  r->a[7] and r->sr are unused!
 */

/* void Execute68kTrap(uint16 trap, M68kRegisters *r); */
		.type	_Execute68kTrap,@function
_Execute68kTrap:
		movl	sp@(4),d0		|Get arguments
		movl	sp@(8),a0

		movml	d2-d7/a2-a6,sp@-	|Save registers

		movl	a0,sp@-			|Push pointer to M68kRegisters on stack
		movw	d0,sp@-			|Push trap word on stack
		subql	#8,sp			|Create fake A-Line exception frame
		movml	a0@,d0-d7/a0-a6		|Load registers from M68kRegisters

		movl	a2,sp@-			|Save a2 and d2
		movl	d2,sp@-
		lea	exectrapret,a2		|a2 points to return address
		movw	sp@(16),d2		|Load trap word into d2

		jmp	zpc@(0x28:w)@(10)	|Jump into MacOS A-Line handler

exectrapret:	movl	a6,sp@-			|Save a6
		movl	sp@(6),a6		|Get pointer to M68kRegisters
		movml	d0-d7/a0-a5,a6@		|Save d0-d7/a0-a5 to M68kRegisters
		movl	sp@+,a6@(56)		|Save a6 to M68kRegisters
		addql	#6,sp			|Remove pointer and trap word from stack

		movml	sp@+,d2-d7/a2-a6	|Restore registers
		rts


/*
 *  Call EmulOp() after return from SIGILL handler, registers are pushed on stack
 */

		.type	_EmulOpTrampoline,@function
_EmulOpTrampoline:
		movl	sp,a0			|Get pointer to registers

		movw	_EmulatedSR,d0		|Save EmulatedSR, disable interrupts
		movw	d0,sp@-
		oriw	#0x0700,d0
		movw	d0,_EmulatedSR

		movl	a0,sp@-			|Push pointer to registers
		movl	a0@(66),a1		|Get saved PC
		addql	#2,a0@(66)		|Skip EMUL_OP opcode
		movw	a1@,sp@-		|Push opcode word
		clrw	sp@-
		jbsr	_EmulOp__FUsP13M68kRegisters
		addql	#8,sp

		movw	sp@+,d0			|Restore interrupts, trigger pending interrupt
		movw	d0,_EmulatedSR
		andiw	#0x0700,d0
		bne	eot1
		tstl	_InterruptFlags
		beq	eot1
		jbsr	_TriggerInterrupt__Fv

eot1:		moveml	sp@+,d0-d7/a0-a6	|Restore registers
		addql	#4,sp			|Skip saved SP
		rtr


/*
 *  Process Manager 68060 FPU patches
 */

		.type	_Scod060Patch1,@function
_Scod060Patch1:	fsave	sp@-		|Save FPU state
		tstb	sp@(2)		|Null?
		beqs	sc1
		fmovemx	fp0-fp7,sp@-	|No, save FPU registers
		fmovel	fpi,sp@-
		fmovel	fpsr,sp@-
		fmovel	fpcr,sp@-
		pea	-1		|Push "FPU state saved" flag
sc1:		movl	d1,sp@-
		movl	d0,sp@-
		bsrs	sc3		|Switch integer registers and stack
		addql	#8,sp
		tstb	sp@(2)		|New FPU state null or "FPU state saved" flag set?
		beqs	sc2
		addql	#4,sp		|Flag set, skip it
		fmovel	sp@+,fpcr	|Restore FPU registers and state
		fmovel	sp@+,fpsr
		fmovel	sp@+,fpi
		fmovemx	sp@+,fp0-fp7
sc2:		frestore sp@+
		movml	sp@+,d0-d1
		rts

sc3:		movl	sp@(4),a0	|Switch integer registers and stack
		movw	sr,sp@-
		movml	d2-d7/a2-a6,sp@-
		cmpw	#0,a0
		beqs	sc4
		movl	sp,a0@
sc4:		movl	sp@(0x36),a0
		movml	a0@+,d2-d7/a2-a6
		movw	a0@+,sr
		movl	a0,sp
		rts

		.type	_Scod060Patch2,@function
_Scod060Patch2:	movl	d0,sp@-		|Create 68060 null frame on stack
		movl	d0,sp@-
		movl	d0,sp@-
		frestore sp@+		|and load it
		rts


/*
 *  Thread Manager 68060 FPU patches
 */

		.type	_ThInitFPUPatch,@function
_ThInitFPUPatch:
		tstb	a4@(0x40)
		bnes	th1
		moveq	#0,d0		|Create 68060 null frame on stack
		movl	d0,a3@-
		movl	d0,a3@-
		movl	d0,a3@-
th1:		rts
