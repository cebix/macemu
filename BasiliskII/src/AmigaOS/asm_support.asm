*
* asm_support.asm - AmigaOS utility functions in assembly language
*
* Basilisk II (C) 1997-2001 Christian Bauer
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*

DEBUG_DETAIL	SET 1

		INCLUDE	"exec/types.i"
		INCLUDE	"exec/macros.i"
		INCLUDE	"exec/memory.i"
		INCLUDE	"exec/tasks.i"
		INCLUDE	"dos/dos.i"
		INCLUDE	"devices/timer.i"

;		INCLUDE	"asmsupp.i"

		XDEF	_AtomicAnd
		XDEF	_AtomicOr
		XDEF	_MoveVBR
		XDEF	_DisableSuperBypass
		XDEF	_Execute68k
		XDEF	_Execute68kTrap
		XDEF	_TrapHandlerAsm
		XDEF	_ExceptionHandlerAsm
		XDEF	_AsmTriggerNMI

		XREF	_OldTrapHandler
		XREF	_OldExceptionHandler
		XREF	_IllInstrHandler
		XREF	_PrivViolHandler
		XREF	_EmulatedSR
		XREF	_IRQSigMask
		XREF	_InterruptFlags
		XREF	_MainTask
		XREF	_SysBase
		XREF	_quit_emulator

INFO_LEVEL	equ	0

		SECTION	text,CODE

		MACHINE	68020

		IFGE	INFO_LEVEL
subSysName:	dc.b	'+',0
		ENDIF

*
* Atomic bit operations (don't trust the compiler)
*

_AtomicAnd	move.l	4(sp),a0
		move.l	8(sp),d0
		and.l	d0,(a0)
		rts

_AtomicOr	move.l	4(sp),a0
		move.l	8(sp),d0
		or.l	d0,(a0)
		rts

*
* Move VBR away from 0 if neccessary
*

_MoveVBR	movem.l	d0-d1/a0-a1/a5-a6,-(sp)
		move.l	_SysBase,a6

		lea	getvbr,a5		;VBR at 0?
		JSRLIB	Supervisor
		tst.l	d0
		bne.s	1$

		move.l	#$400,d0		;Yes, allocate memory for new table
		move.l	#MEMF_PUBLIC,d1
		JSRLIB	AllocMem
		tst.l	d0
		beq.s	1$

		JSRLIB	Disable

		move.l	d0,a5			;Copy old table
		move.l	d0,a1
		sub.l	a0,a0
		move.l	#$400,d0
		JSRLIB	CopyMem
		JSRLIB	CacheClearU

		move.l	a5,d0			;Set VBR
		lea	setvbr,a5
		JSRLIB	Supervisor

		JSRLIB	Enable

1$		movem.l	(sp)+,d0-d1/a0-a1/a5-a6
		rts

getvbr		movec	vbr,d0
		rte

setvbr		movec	d0,vbr
		rte

*
* Disable 68060 Super Bypass mode
*

_DisableSuperBypass
		movem.l	d0-d1/a0-a1/a5-a6,-(sp)
		move.l	_SysBase,a6

		lea	dissb,a5
		JSRLIB	Supervisor

		movem.l	(sp)+,d0-d1/a0-a1/a5-a6
		rts

		MACHINE	68060

dissb		movec	pcr,d0
		bset	#5,d0
		movec	d0,pcr
		rte

		MACHINE	68020

*
* Execute 68k subroutine (must be ended with rts)
* r->a[7] and r->sr are unused!
*

; void Execute68k(uint32 addr, M68kRegisters *r);
_Execute68k
		move.l	4(sp),d0		;Get arguments
		move.l	8(sp),a0

		movem.l	d2-d7/a2-a6,-(sp)	;Save registers

		move.l	a0,-(sp)		;Push pointer to M68kRegisters on stack
		pea	1$			;Push return address on stack
		move.l	d0,-(sp)		;Push pointer to 68k routine on stack
		movem.l	(a0),d0-d7/a0-a6	;Load registers from M68kRegisters

		rts				;Jump into 68k routine

1$		move.l	a6,-(sp)		;Save a6
		move.l	4(sp),a6		;Get pointer to M68kRegisters
		movem.l	d0-d7/a0-a5,(a6)	;Save d0-d7/a0-a5 to M68kRegisters
		move.l	(sp)+,56(a6)		;Save a6 to M68kRegisters
		addq.l	#4,sp			;Remove pointer from stack

		movem.l	(sp)+,d2-d7/a2-a6	;Restore registers
		rts

*
* Execute MacOS 68k trap
* r->a[7] and r->sr are unused!
*

; void Execute68kTrap(uint16 trap, M68kRegisters *r);
_Execute68kTrap
		move.l	4(sp),d0		;Get arguments
		move.l	8(sp),a0

		movem.l	d2-d7/a2-a6,-(sp)	;Save registers

		move.l	a0,-(sp)		;Push pointer to M68kRegisters on stack
		move.w	d0,-(sp)		;Push trap word on stack
		subq.l	#8,sp			;Create fake A-Line exception frame
		movem.l	(a0),d0-d7/a0-a6	;Load registers from M68kRegisters

		move.l	a2,-(sp)		;Save a2 and d2
		move.l	d2,-(sp)
		lea	1$,a2			;a2 points to return address
		move.w	16(sp),d2		;Load trap word into d2

		jmp	([$28.w],10)		;Jump into MacOS A-Line handler

1$		move.l	a6,-(sp)		;Save a6
		move.l	6(sp),a6		;Get pointer to M68kRegisters
		movem.l	d0-d7/a0-a5,(a6)	;Save d0-d7/a0-a5 to M68kRegisters
		move.l	(sp)+,56(a6)		;Save a6 to M68kRegisters
		addq.l	#6,sp			;Remove pointer and trap word from stack

		movem.l	(sp)+,d2-d7/a2-a6	;Restore registers
		rts

*
* Exception handler of main task (for interrupts)
*

_ExceptionHandlerAsm
		move.l	d0,-(sp)		;Save d0

		and.l	#SIGBREAKF_CTRL_C,d0	;CTRL-C?
		bne.s	2$

		move.w	_EmulatedSR,d0		;Interrupts enabled in emulated SR?
		and.w	#$0700,d0
		bne	1$
		move.w	#$0064,-(sp)		;Yes, fake interrupt stack frame
		pea	1$
		move.w	_EmulatedSR,d0
		move.w	d0,-(sp)
		or.w	#$2100,d0		;Set interrupt level in SR, enter (virtual) supervisor mode
		move.w	d0,_EmulatedSR
		move.l	$64.w,-(sp)		;Jump to MacOS interrupt handler
		rts

1$		move.l	(sp)+,d0		;Restore d0
		rts

2$		JSRLIB	Forbid			;Waiting for Dos signal?
		sub.l	a1,a1
		JSRLIB	FindTask
		move.l	d0,a0
		move.l	TC_SIGWAIT(a0),d0
		move.l	TC_SIGRECVD(a0),d1
		JSRLIB	Permit
		btst	#SIGB_DOS,d0
		beq	3$
		btst	#SIGB_DOS,d1
		bne	4$

3$		lea	TC_SIZE(a0),a0		;No, remove pending Dos packets
		JSRLIB	GetMsg

		move.w	_EmulatedSR,d0
		or.w	#$0700,d0		;Disable all interrupts
		move.w	d0,_EmulatedSR
		moveq	#0,d0			;Disable all exception signals
		moveq	#-1,d1
		JSRLIB	SetExcept
		jsr	_quit_emulator		;CTRL-C, quit emulator
4$		move.l	(sp)+,d0
		rts

*
* Trap handler of main task
*

_TrapHandlerAsm:
	IFEQ	INFO_LEVEL-1002
	move.w	([6,a0]),-(sp)
	move.w	#0,-(sp)
	move.l	(4+6,a0),-(sp)
	PUTMSG	0,'%s/TrapHandlerAsm:  addr=%08lx  opcode=%04lx'
	lea	(2*4,sp),sp
	ENDC

		cmp.l	#4,(sp)			;Illegal instruction?
		beq.s	doillinstr
		cmp.l	#10,(sp)		;A-Line exception?
		beq.s	doaline
		cmp.l	#8,(sp)			;Privilege violation?
		beq.s	doprivviol

		cmp.l	#9,(sp)			;Trace?
		beq	dotrace
		cmp.l	#3,(sp)			;Illegal Address?
		beq.s	doilladdr
		cmp.l	#11,(sp)		;F-Line exception
		beq.s	dofline

		cmp.l	#32,(sp)
		blt	1$
		cmp.l	#47,(sp)
		ble	doTrapXX		; Vector 32-47 : TRAP #0 - 15 Instruction Vectors

1$:
		cmp.l	#48,(sp)
		blt	2$
		cmp.l	#55,(sp)
		ble	doTrapFPU
2$:
	IFEQ	INFO_LEVEL-1009
	PUTMSG	0,'%s/TrapHandlerAsm:  stack=%08lx %08lx %08lx %08lx'
	ENDC

		move.l	_OldTrapHandler,-(sp)	;No, jump to old trap handler
		rts

*
* TRAP #0 - 15 Instruction Vectors
*

doTrapXX:
	IFEQ	INFO_LEVEL-1009
	PUTMSG	0,'%s/doTrapXX:  stack=%08lx %08lx %08lx %08lx'
	ENDC

		movem.l	a0/d0,-(sp)		;Save a0,d0
		move.l	(2*4,sp),d0		;vector number 32-47

		move.l	usp,a0			;Get user stack pointer
		move.l	(4*4,sp),-(a0)		;Copy 4-word stack frame to user stack
		move.l	(3*4,sp),-(a0)
		move.l	a0,usp			;Update USP
		or.w	#$2000,(a0)		;set Supervisor bit in SR

		lsl.l	#2,d0			;convert vector number to vector offset
		move.l	d0,a0
		move.l	(a0),d0			;get mac trap vector

		move.l	usp,a0			;Get user stack pointer
		move.l	d0,-(a0)		;store vector offset as return address
		move.l	a0,usp			;Update USP

		movem.l	(sp)+,a0/d0		;Restore a0,d0
		addq.l	#4*2,sp			;Remove exception frame from supervisor stack

		andi	#$d8ff,sr		;Switch to user mode, enable interrupts
		rts


*
* FPU Exception Instruction Vectors
*

doTrapFPU:
		move.l	d0,(sp)
		fmove.l	fpcr,d0
		and.w	#$00ff,d0		;disable FPU exceptions
		fmove.l	d0,fpcr
		move.l	(sp)+,d0		;Restore d0
		rte


*
* trace Vector
*

dotrace
	IFEQ	INFO_LEVEL-1009
	PUTMSG	0,'%s/dotrace:  stack=%08lx %08lx %08lx %08lx'
	ENDC

		move.l	a0,(sp)			;Save a0
		move.l	usp,a0			;Get user stack pointer

	IFEQ	INFO_LEVEL-1009
	move.l	(12,a0),-(sp)
	move.l	(8,a0),-(sp)
	move.l	(4,a0),-(sp)
	move.l	(0,a0),-(sp)
	move.l	a0,-(sp)
	move.l	a7,-(sp)
	PUTMSG	0,'%s/dotrace:  sp=%08lx  usp=%08lx (%08lx %08lx %08lx %08lx)'
	lea	(6*4,sp),sp
	ENDC

		move.l	3*4(sp),-(a0)		;Copy 6-word stack frame to user stack
		move.l	2*4(sp),-(a0)
		move.l	1*4(sp),-(a0)
		move.l	a0,usp			;Update USP
		or.w	#$2000,(a0)		;set Supervisor bit in SR
		move.l	(sp)+,a0		;Restore a0

		lea	6*2(sp),sp		;Remove exception frame from supervisor stack
		andi	#$18ff,sr		;Switch to user mode, enable interrupts, disable trace

		move.l	$24.w,-(sp)		;Jump to MacOS exception handler
		rts


*
* A-Line handler: call MacOS A-Line handler
*

doaline		move.l	a0,(sp)			;Save a0
		move.l	usp,a0			;Get user stack pointer
		move.l	8(sp),-(a0)		;Copy stack frame to user stack
		move.l	4(sp),-(a0)
		move.l	a0,usp			;Update USP

		or.w	#$2000,(a0)		;set Supervisor bit in SR
		move.l	(sp)+,a0		;Restore a0

		addq.l	#8,sp			;Remove exception frame from supervisor stack
		andi	#$d8ff,sr		;Switch to user mode, enable interrupts

;		and.w	#$f8ff,_EmulatedSR	;enable interrupts in EmulatedSR

		move.l	$28.w,-(sp)		;Jump to MacOS exception handler
		rts

*
* F-Line handler: call F-Line exception handler
*

dofline		move.l	a0,(sp)			;Save a0
		move.l	usp,a0			;Get user stack pointer
		move.l	8(sp),-(a0)		;Copy stack frame to user stack
		move.l	4(sp),-(a0)
		move.l	a0,usp			;Update USP
		or.w	#$2000,(a0)		;set Supervisor bit in SR
		move.l	(sp)+,a0		;Restore a0

		addq.l	#8,sp			;Remove exception frame from supervisor stack
		andi	#$d8ff,sr		;Switch to user mode, enable interrupts

		and.w	#$f8ff,_EmulatedSR	;enable interrupts in EmulatedSR

		move.l	$2c.w,-(sp)		;Jump to MacOS exception handler
		rts

*
* Illegal address handler
*

doilladdr:
	IFEQ	INFO_LEVEL-1009
	PUTMSG	0,'%s/doilladdr:  stack=%08lx %08lx %08lx %08lx'
	ENDC

		move.l	a0,(sp)			;Save a0

		move.l	usp,a0			;Get user stack pointer
		move.l	3*4(sp),-(a0)		;Copy 6-word stack frame to user stack
		move.l	2*4(sp),-(a0)
		move.l	1*4(sp),-(a0)
		move.l	a0,usp			;Update USP
		or.w	#$2000,(a0)		;set Supervisor bit in SR
		move.l	(sp)+,a0		;Restore a0

		lea	6*2(sp),sp		;Remove exception frame from supervisor stack
		andi	#$d8ff,sr		;Switch to user mode, enable interrupts

		move.l	$0c.w,-(sp)		;Jump to MacOS exception handler
		rts


*
* Illegal instruction handler: call IllInstrHandler() (which calls EmulOp())
*   to execute extended opcodes (see emul_op.h)
*

doillinstr	movem.l	a0/d0,-(sp)
		move.w	([6+2*4,sp]),d0
		and.w	#$ff00,d0
		cmp.w	#$7100,d0

	IFEQ	INFO_LEVEL-1009
	move.l	d0,-(sp)
	PUTMSG	0,'%s/doillinst:  d0=%08lx stack=%08lx %08lx %08lx %08lx'
	lea	(1*4,sp),sp
	ENDC
		movem.l	(sp)+,a0/d0
		beq	1$

		move.l	a0,(sp)			;Save a0
		move.l	usp,a0			;Get user stack pointer
		move.l	8(sp),-(a0)		;Copy stack frame to user stack
		move.l	4(sp),-(a0)
		move.l	a0,usp			;Update USP
		or.w	#$2000,(a0)		;set Supervisor bit in SR
		move.l	(sp)+,a0		;Restore a0

		add.w	#3*4,sp			;Remove exception frame from supervisor stack
		andi	#$d8ff,sr		;Switch to user mode, enable interrupts

		move.l	$10.w,-(sp)		;Jump to MacOS exception handler
		rts

1$:
		move.l	a6,(sp)			;Save a6
		move.l	usp,a6			;Get user stack pointer

		move.l	a6,-10(a6)		;Push USP (a7)
		move.l	6(sp),-(a6)		;Push PC
		move.w	4(sp),-(a6)		;Push SR
		subq.l	#4,a6			;Skip saved USP
		move.l	(sp),-(a6)		;Push old a6
		movem.l	d0-d7/a0-a5,-(a6)	;Push remaining registers
		move.l	a6,usp			;Update USP

		add.w	#12,sp			;Remove exception frame from supervisor stack
		andi	#$d8ff,sr		;Switch to user mode, enable interrupts

		move.l	a6,-(sp)		;Jump to IllInstrHandler() in main.cpp
		jsr	_IllInstrHandler
		addq.l	#4,sp

		movem.l	(sp)+,d0-d7/a0-a6	;Restore registers
		addq.l	#4,sp			;Skip saved USP (!!)
		rtr				;Return from exception

*
* Privilege violation handler: MacOS runs in supervisor mode,
*   so we have to emulate certain privileged instructions
*

doprivviol	move.l	d0,(sp)			;Save d0
		move.w	([6,sp]),d0		;Get instruction word

	IFEQ	INFO_LEVEL-1001
	move.w	([6,a0]),-(sp)
	move.w	#0,-(sp)
	PUTMSG	0,'%s/doprivviol:  opcode=%04lx'
	lea	(1*4,sp),sp
	ENDC

		cmp.w	#$40e7,d0		;move sr,-(sp)?
		beq	pushsr
		cmp.w	#$46df,d0		;move (sp)+,sr?
		beq	popsr

		cmp.w	#$007c,d0		;ori #xxxx,sr?
		beq	orisr
		cmp.w	#$027c,d0		;andi #xxxx,sr?
		beq	andisr

		cmp.w	#$46fc,d0		;move #xxxx,sr?
		beq	movetosrimm

		cmp.w	#$46ef,d0		;move (xxxx,sp),sr?
		beq	movetosrsprel
		cmp.w	#$46d8,d0		;move (a0)+,sr?
		beq	movetosra0p
		cmp.w	#$46d9,d0		;move (a1)+,sr?
		beq	movetosra1p

		cmp.w	#$40f8,d0		;move sr,xxxx.w?
		beq	movefromsrabs
		cmp.w	#$40d0,d0		;move sr,(a0)?
		beq	movefromsra0
		cmp.w	#$40d7,d0		;move sr,(sp)?
		beq	movefromsrsp

		cmp.w	#$f327,d0		;fsave -(sp)?
		beq	fsavepush
		cmp.w	#$f35f,d0		;frestore (sp)+?
		beq	frestorepop
		cmp.w	#$f32d,d0		;fsave xxx(a5) ?
		beq	fsavea5
		cmp.w	#$f36d,d0		;frestore xxx(a5) ?
		beq	frestorea5

		cmp.w	#$4e73,d0		;rte?
		beq	pvrte

		cmp.w	#$40c0,d0		;move sr,d0?
		beq	movefromsrd0
		cmp.w	#$40c1,d0		;move sr,d1?
		beq	movefromsrd1
		cmp.w	#$40c2,d0		;move sr,d2?
		beq	movefromsrd2
		cmp.w	#$40c3,d0		;move sr,d3?
		beq	movefromsrd3
		cmp.w	#$40c4,d0		;move sr,d4?
		beq	movefromsrd4
		cmp.w	#$40c5,d0		;move sr,d5?
		beq	movefromsrd5
		cmp.w	#$40c6,d0		;move sr,d6?
		beq	movefromsrd6
		cmp.w	#$40c7,d0		;move sr,d7?
		beq	movefromsrd7

		cmp.w	#$46c0,d0		;move d0,sr?
		beq	movetosrd0
		cmp.w	#$46c1,d0		;move d1,sr?
		beq	movetosrd1
		cmp.w	#$46c2,d0		;move d2,sr?
		beq	movetosrd2
		cmp.w	#$46c3,d0		;move d3,sr?
		beq	movetosrd3
		cmp.w	#$46c4,d0		;move d4,sr?
		beq	movetosrd4
		cmp.w	#$46c5,d0		;move d5,sr?
		beq	movetosrd5
		cmp.w	#$46c6,d0		;move d6,sr?
		beq	movetosrd6
		cmp.w	#$46c7,d0		;move d7,sr?
		beq	movetosrd7

		cmp.w	#$4e7a,d0		;movec cr,x?
		beq	movecfromcr
		cmp.w	#$4e7b,d0		;movec x,cr?
		beq	movectocr

		cmp.w	#$f478,d0		;cpusha dc?
		beq	cpushadc
		cmp.w	#$f4f8,d0		;cpusha dc/ic?
		beq	cpushadcic

		cmp.w	#$4e69,d0		;move usp,a1
		beq	moveuspa1
		cmp.w	#$4e68,d0		;move usp,a0
		beq	moveuspa0

		cmp.w	#$4e61,d0		;move a1,usp
		beq	moved1usp

pv_unhandled	move.l	(sp),d0			;Unhandled instruction, jump to handler in main.cpp
		move.l	a6,(sp)			;Save a6
		move.l	usp,a6			;Get user stack pointer

		move.l	a6,-10(a6)		;Push USP (a7)
		move.l	6(sp),-(a6)		;Push PC
		move.w	4(sp),-(a6)		;Push SR
		subq.l	#4,a6			;Skip saved USP
		move.l	(sp),-(a6)		;Push old a6
		movem.l	d0-d7/a0-a5,-(a6)	;Push remaining registers
		move.l	a6,usp			;Update USP

		add.w	#12,sp			;Remove exception frame from supervisor stack
		andi	#$d8ff,sr		;Switch to user mode, enable interrupts

		move.l	a6,-(sp)		;Jump to PrivViolHandler() in main.cpp
		jsr	_PrivViolHandler
		addq.l	#4,sp

		movem.l	(sp)+,d0-d7/a0-a6	;Restore registers
		addq.l	#4,sp			;Skip saved USP
		rtr				;Return from exception

; move sr,-(sp)
pushsr		move.l	a0,-(sp)		;Save a0
		move.l	usp,a0			;Get user stack pointer
		move.w	8(sp),d0		;Get CCR from exception stack frame
		or.w	_EmulatedSR,d0		;Add emulated supervisor bits
		move.w	d0,-(a0)		;Store SR on user stack
		move.l	a0,usp			;Update USP
		move.l	(sp)+,a0		;Restore a0
		move.l	(sp)+,d0		;Restore d0
		addq.l	#2,2(sp)		;Skip instruction

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; move (sp)+,sr
popsr		move.l	a0,-(sp)		;Save a0
		move.l	usp,a0			;Get user stack pointer
		move.w	(a0)+,d0		;Get SR from user stack
		move.w	d0,8(sp)		;Store into CCR on exception stack frame
		and.w	#$00ff,8(sp)
		and.w	#$e700,d0		;Extract supervisor bits
		move.w	d0,_EmulatedSR		;And save them

		and.w	#$0700,d0		;Rethrow exception if interrupts are pending and reenabled
		bne	1$
		tst.l	_InterruptFlags
		beq	1$
		movem.l	d0-d1/a0-a1/a6,-(sp)
		move.l	_SysBase,a6
		move.l	_MainTask,a1
		move.l	_IRQSigMask,d0
		JSRLIB	Signal
		movem.l	(sp)+,d0-d1/a0-a1/a6
1$
		move.l	a0,usp			;Update USP
		move.l	(sp)+,a0		;Restore a0
		move.l	(sp)+,d0		;Restore d0
		addq.l	#2,2(sp)		;Skip instruction

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; ori #xxxx,sr
orisr		move.w	4(sp),d0		;Get CCR from stack
		or.w	_EmulatedSR,d0		;Add emulated supervisor bits
		or.w	([6,sp],2),d0		;Or with immediate value
		move.w	d0,4(sp)		;Store into CCR on stack
		and.w	#$00ff,4(sp)
		and.w	#$e700,d0		;Extract supervisor bits
		move.w	d0,_EmulatedSR		;And save them
		move.l	(sp)+,d0		;Restore d0
		addq.l	#4,2(sp)		;Skip instruction

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; andi #xxxx,sr
andisr		move.w	4(sp),d0		;Get CCR from stack
		or.w	_EmulatedSR,d0		;Add emulated supervisor bits
		and.w	([6,sp],2),d0		;And with immediate value
storesr4	move.w	d0,4(sp)		;Store into CCR on stack
		and.w	#$00ff,4(sp)
		and.w	#$e700,d0		;Extract supervisor bits
		move.w	d0,_EmulatedSR		;And save them

		and.w	#$0700,d0		;Rethrow exception if interrupts are pending and reenabled
		bne.s	1$
		tst.l	_InterruptFlags
		beq.s	1$
		movem.l	d0-d1/a0-a1/a6,-(sp)
		move.l	_SysBase,a6
		move.l	_MainTask,a1
		move.l	_IRQSigMask,d0
		JSRLIB	Signal
		movem.l	(sp)+,d0-d1/a0-a1/a6
1$		move.l	(sp)+,d0		;Restore d0
		addq.l	#4,2(sp)		;Skip instruction

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; move #xxxx,sr
movetosrimm	move.w	([6,sp],2),d0		;Get immediate value
		bra.s	storesr4

; move (xxxx,sp),sr
movetosrsprel	move.l	a0,-(sp)		;Save a0
		move.l	usp,a0			;Get user stack pointer
		move.w	([10,sp],2),d0		;Get offset
		move.w	(a0,d0.w),d0		;Read word
		move.l	(sp)+,a0		;Restore a0
		bra.s	storesr4

; move (a0)+,sr
movetosra0p	move.w	(a0)+,d0		;Read word
		bra	storesr2

; move (a1)+,sr
movetosra1p	move.w	(a1)+,d0		;Read word
		bra	storesr2

; move sr,xxxx.w
movefromsrabs	move.l	a0,-(sp)		;Save a0
		move.w	([10,sp],2),a0		;Get address
		move.w	8(sp),d0		;Get CCR
		or.w	_EmulatedSR,d0		;Add emulated supervisor bits
		move.w	d0,(a0)			;Store SR
		move.l	(sp)+,a0		;Restore a0
		move.l	(sp)+,d0		;Restore d0
		addq.l	#4,2(sp)		;Skip instruction

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; move sr,(a0)
movefromsra0	move.w	4(sp),d0		;Get CCR
		or.w	_EmulatedSR,d0		;Add emulated supervisor bits
		move.w	d0,(a0)			;Store SR
		move.l	(sp)+,d0		;Restore d0
		addq.l	#2,2(sp)		;Skip instruction

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; move sr,(sp)
movefromsrsp	move.l	a0,-(sp)		;Save a0
		move.l	usp,a0			;Get user stack pointer
		move.w	8(sp),d0		;Get CCR
		or.w	_EmulatedSR,d0		;Add emulated supervisor bits
		move.w	d0,(a0)			;Store SR
		move.l	(sp)+,a0		;Restore a0
		move.l	(sp)+,d0		;Restore d0
		addq.l	#2,2(sp)		;Skip instruction

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; fsave -(sp)
fsavepush	move.l	(sp),d0			;Restore d0
		move.l	a0,(sp)			;Save a0
		move.l	usp,a0			;Get user stack pointer
		move.l	#$41000000,-(a0)	;Push idle frame
		move.l	a0,usp			;Update USP
		move.l	(sp)+,a0		;Restore a0
		addq.l	#2,2(sp)		;Skip instruction

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; fsave xxx(a5)
fsavea5		move.l	(sp),d0			;Restore d0
		move.l	a0,(sp)			;Save a0
		move.l	a5,a0			;Get base register
		add.w	([6,sp],2),a0		;Add offset to base register
		move.l	#$41000000,(a0)		;Push idle frame
		move.l	(sp)+,a0		;Restore a0
		addq.l	#4,2(sp)		;Skip instruction

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; frestore (sp)+
frestorepop	move.l	(sp),d0			;Restore d0
		move.l	a0,(sp)			;Save a0
		move.l	usp,a0			;Get user stack pointer
		addq.l	#4,a0			;Nothing to do...
		move.l	a0,usp			;Update USP
		move.l	(sp)+,a0		;Restore a0
		addq.l	#2,2(sp)		;Skip instruction

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; frestore xxx(a5)
frestorea5	move.l	(sp),d0			;Restore d0
		move.l	a0,(sp)			;Save a0
		move.l	(sp)+,a0		;Restore a0
		addq.l	#4,2(sp)		;Skip instruction

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; rte
pvrte		movem.l	a0/a1,-(sp)		;Save a0 and a1
		move.l	usp,a0			;Get user stack pointer

		move.w	(a0)+,d0		;Get SR from user stack
		move.w	d0,8+4(sp)		;Store into CCR on exception stack frame
		and.w	#$c0ff,8+4(sp)
		and.w	#$e700,d0		;Extract supervisor bits
		move.w	d0,_EmulatedSR		;And save them
		move.l	(a0)+,10+4(sp)		;Store return address in exception stack frame

		move.w	(a0)+,d0		;get format word
		lsr.w	#7,d0			;get stack frame Id 
		lsr.w	#4,d0
		and.w	#$001e,d0
		move.w	(StackFormatTable,pc,d0.w),d0	; get total stack frame length
		subq.w	#4,d0			; count only extra words
		lea	16+4(sp),a1		; destination address (in supervisor stack)
		bra	1$

2$		move.w	(a0)+,(a1)+		; copy additional stack words back to supervisor stack
1$		dbf	d0,2$

		move.l	a0,usp			;Update USP
		movem.l	(sp)+,a0/a1		;Restore a0 and a1
		move.l	(sp)+,d0		;Restore d0

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; sizes of exceptions stack frames
StackFormatTable:
		dc.w	4			; Four-word stack frame, format $0
		dc.w	4			; Throwaway four-word stack frame, format $1
		dc.w	6			; Six-word stack frame, format $2
		dc.w	6			; MC68040 floating-point post-instruction stack frame, format $3
		dc.w	8			; MC68EC040 and MC68LC040 floating-point unimplemented stack frame, format $4
		dc.w	4			; Format $5
		dc.w	4			; Format $6
		dc.w	30			; MC68040 access error stack frame, Format $7
		dc.w	29			; MC68010 bus and address error stack frame, format $8
		dc.w	10			; MC68020 and MC68030 coprocessor mid-instruction stack frame, format $9
		dc.w	16			; MC68020 and MC68030 short bus cycle stack frame, format $a
		dc.w	46			; MC68020 and MC68030 long bus cycle stack frame, format $b
		dc.w	12			; CPU32 bus error for prefetches and operands stack frame, format $c
		dc.w	4			; Format $d
		dc.w	4			; Format $e
		dc.w	4			; Format $f

; move sr,dx
movefromsrd0	addq.l	#4,sp			;Skip saved d0
		moveq	#0,d0
		move.w	(sp),d0			;Get CCR
		or.w	_EmulatedSR,d0		;Add emulated supervisor bits
		addq.l	#2,2(sp)		;Skip instruction

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

movefromsrd1	move.l	(sp)+,d0
		moveq	#0,d1
		move.w	(sp),d1
		or.w	_EmulatedSR,d1
		addq.l	#2,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

movefromsrd2	move.l	(sp)+,d0
		moveq	#0,d2
		move.w	(sp),d2
		or.w	_EmulatedSR,d2
		addq.l	#2,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

movefromsrd3	move.l	(sp)+,d0
		moveq	#0,d3
		move.w	(sp),d3
		or.w	_EmulatedSR,d3
		addq.l	#2,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

movefromsrd4	move.l	(sp)+,d0
		moveq	#0,d4
		move.w	(sp),d4
		or.w	_EmulatedSR,d4
		addq.l	#2,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

movefromsrd5	move.l	(sp)+,d0
		moveq	#0,d5
		move.w	(sp),d5
		or.w	_EmulatedSR,d5
		addq.l	#2,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

movefromsrd6	move.l	(sp)+,d0
		moveq	#0,d6
		move.w	(sp),d6
		or.w	_EmulatedSR,d6
		addq.l	#2,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

movefromsrd7	move.l	(sp)+,d0
		moveq	#0,d7
		move.w	(sp),d7
		or.w	_EmulatedSR,d7
		addq.l	#2,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; move dx,sr
movetosrd0	move.l	(sp),d0
storesr2	move.w	d0,4(sp)
		and.w	#$00ff,4(sp)
		and.w	#$e700,d0
		move.w	d0,_EmulatedSR

		and.w	#$0700,d0		;Rethrow exception if interrupts are pending and reenabled
		bne.s	1$
		tst.l	_InterruptFlags
		beq.s	1$
		movem.l	d0-d1/a0-a1/a6,-(sp)
		move.l	_SysBase,a6
		move.l	_MainTask,a1
		move.l	_IRQSigMask,d0
		JSRLIB	Signal
		movem.l	(sp)+,d0-d1/a0-a1/a6
1$		move.l	(sp)+,d0
		addq.l	#2,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

movetosrd1	move.l	d1,d0
		bra.s	storesr2

movetosrd2	move.l	d2,d0
		bra.s	storesr2

movetosrd3	move.l	d3,d0
		bra.s	storesr2

movetosrd4	move.l	d4,d0
		bra.s	storesr2

movetosrd5	move.l	d5,d0
		bra.s	storesr2

movetosrd6	move.l	d6,d0
		bra.s	storesr2

movetosrd7	move.l	d7,d0
		bra.s	storesr2

; movec cr,x
movecfromcr	move.w	([6,sp],2),d0		;Get next instruction word

		cmp.w	#$8801,d0		;movec vbr,a0?
		beq.s	movecvbra0
		cmp.w	#$9801,d0		;movec vbr,a1?
		beq.s	movecvbra1
		cmp.w	#$A801,d0		;movec vbr,a2?
		beq.s	movecvbra2
		cmp.w	#$1801,d0		;movec vbr,d1?
		beq	movecvbrd1
		cmp.w	#$0002,d0		;movec cacr,d0?
		beq.s	moveccacrd0
		cmp.w	#$1002,d0		;movec cacr,d1?
		beq.s	moveccacrd1
		cmp.w	#$0003,d0		;movec tc,d0?
		beq.s	movectcd0
		cmp.w	#$1003,d0		;movec tc,d1?
		beq.s	movectcd1
		cmp.w	#$1000,d0		;movec sfc,d1?
		beq	movecsfcd1
		cmp.w	#$1001,d0		;movec dfc,d1?
		beq	movecdfcd1
		cmp.w	#$0806,d0		;movec urp,d0?
		beq	movecurpd0
		cmp.w	#$0807,d0		;movec srp,d0?
		beq.s	movecsrpd0
		cmp.w	#$0004,d0		;movec itt0,d0
		beq.s	movecitt0d0
		cmp.w	#$0005,d0		;movec itt1,d0
		beq.s	movecitt1d0
		cmp.w	#$0006,d0		;movec dtt0,d0
		beq.s	movecdtt0d0
		cmp.w	#$0007,d0		;movec dtt1,d0
		beq.s	movecdtt1d0

		bra	pv_unhandled

; movec cacr,d0
moveccacrd0	move.l	(sp)+,d0
		move.l	#$3111,d0		;All caches and bursts on
		addq.l	#4,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; movec cacr,d1
moveccacrd1	move.l	(sp)+,d0
		move.l	#$3111,d1		;All caches and bursts on
		addq.l	#4,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; movec vbr,a0
movecvbra0	move.l	(sp)+,d0
		sub.l	a0,a0			;VBR always appears to be at 0
		addq.l	#4,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; movec vbr,a1
movecvbra1	move.l	(sp)+,d0
		sub.l	a1,a1			;VBR always appears to be at 0
		addq.l	#4,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; movec vbr,a2
movecvbra2	move.l	(sp)+,d0
		sub.l	a2,a2			;VBR always appears to be at 0
		addq.l	#4,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; movec vbr,d1
movecvbrd1	move.l	(sp)+,d0
		moveq.l	#0,d1			;VBR always appears to be at 0
		addq.l	#4,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; movec tc,d0
movectcd0	addq.l	#4,sp
		moveq	#0,d0			;MMU is always off
		addq.l	#4,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; movec tc,d1	+jl+
movectcd1	move.l	(sp)+,d0		;Restore d0
		moveq	#0,d1			;MMU is always off
		addq.l	#4,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; movec sfc,d1	+jl+
movecsfcd1	move.l	(sp)+,d0		;Restore d0
		moveq	#0,d1
		addq.l	#4,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; movec dfc,d1	+jl+
movecdfcd1	move.l	(sp)+,d0		;Restore d0
		moveq	#0,d1
		addq.l	#4,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

movecurpd0		; movec urp,d0	+jl+
movecsrpd0		; movec srp,d0
movecitt0d0		; movec itt0,d0
movecitt1d0		; movec itt1,d0
movecdtt0d0		; movec dtt0,d0
movecdtt1d0		; movec dtt1,d0
		addq.l	#4,sp
		moveq.l	#0,d0			;MMU is always off
		addq.l	#4,2(sp)		;skip instruction

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; movec x,cr
movectocr	move.w	([6,sp],2),d0		;Get next instruction word

		cmp.w	#$0801,d0		;movec d0,vbr?
		beq.s	movectovbr
		cmp.w	#$1801,d0		;movec d1,vbr?
		beq.s	movectovbr
		cmp.w	#$A801,d0		;movec a2,vbr?
		beq.s	movectovbr
		cmp.w	#$0002,d0		;movec d0,cacr?
		beq.s	movectocacr
		cmp.w	#$1002,d0		;movec d1,cacr?
		beq.s	movectocacr
		cmp.w	#$1000,d0		;movec d1,sfc?
		beq.s	movectoxfc
		cmp.w	#$1001,d0		;movec d1,dfc?
		beq.s	movectoxfc

		bra	pv_unhandled

; movec x,vbr
movectovbr	move.l	(sp)+,d0		;Ignore moves to VBR
		addq.l	#4,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; movec dx,cacr
movectocacr	movem.l	d1/a0-a1/a6,-(sp)	;Move to CACR, clear caches
		move.l	_SysBase,a6
		JSRLIB	CacheClearU
		movem.l	(sp)+,d1/a0-a1/a6
		move.l	(sp)+,d0
		addq.l	#4,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; movec x,sfc
; movec x,dfc
movectoxfc	move.l	(sp)+,d0		;Ignore moves to SFC, DFC
		addq.l	#4,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

; cpusha
cpushadc
cpushadcic
	IFEQ	INFO_LEVEL-1003
	move.l	(4),-(sp)
	move.l	d0,-(sp)
	PUTMSG	0,'%s/cpushadc:  opcode=%04lx  Execbase=%08lx'
	lea	(2*4,sp),sp
	ENDC
		movem.l	d1/a0-a1/a6,-(sp)	;Clear caches
		move.l	_SysBase,a6
		JSRLIB	CacheClearU
		movem.l	(sp)+,d1/a0-a1/a6
		move.l	(sp)+,d0
		addq.l	#2,2(sp)
		rte

; move usp,a1	+jl+
moveuspa1	move.l	(sp)+,d0
		move	usp,a1
		addq.l	#2,2(sp)

	IFEQ	INFO_LEVEL-1009
	move.l	a1,-(sp)
	move.l	a7,-(sp)
	PUTMSG	0,'%s/moveuspa1:  a7=%08lx  a1=%08lx'
	lea	(2*4,sp),sp
	ENDC

		rte

; move usp,a0	+jl+
moveuspa0	move.l	(sp)+,d0
		move	usp,a0
		addq.l	#2,2(sp)

	IFEQ	INFO_LEVEL-1009
	move.l	a0,-(sp)
	move.l	a7,-(sp)
	PUTMSG	0,'%s/moveuspa0:  a7=%08lx  a0=%08lx'
	lea	(2*4,sp),sp
	ENDC

		rte

; move a1,usp	+jl+
moved1usp	move.l	(sp)+,d0
		move	a1,usp
		addq.l	#2,2(sp)

	IFEQ	INFO_LEVEL-1001
	move.l	(4),-(sp)
	PUTMSG	0,'%s/doprivviol END:  Execbase=%08lx'
	lea	(1*4,sp),sp
	ENDC
		rte

;
; Trigger NMI (Pop up debugger)
;

_AsmTriggerNMI	move.l	d0,-(sp)		;Save d0
		move.w	#$007c,-(sp)		;Yes, fake NMI stack frame
		pea	1$
		move.w	_EmulatedSR,d0
		and.w	#$f8ff,d0		;Set interrupt level in SR
		move.w	d0,-(sp)
		move.w	d0,_EmulatedSR

		move.l	$7c.w,-(sp)		;Jump to MacOS NMI handler
		rts

1$		move.l	(sp)+,d0		;Restore d0
		rts


CopyTrapStack:
		movem.l	d0/a0/a1,-(sp)

		move.w	(5*4+6,sp),d0		;get format word
		lsr.w	#7,d0			;get stack frame Id 
		lsr.w	#4,d0
		and.w	#$001e,d0
		move.w	(StackFormatTable,pc,d0.w),d0	; get total stack frame length

		lea	(5*4,sp),a0		;get start of exception stack frame
		move.l	usp,a1			;Get user stack pointer
		bra	1$

2$		move.w	(a0)+,(a1)+		; copy additional stack words back to supervisor stack
1$		dbf	d0,2$

		move.l	(3*4,sp),-(a0)		;copy return address to new top of stack 
		move.l	a0,sp
		rts

		END
