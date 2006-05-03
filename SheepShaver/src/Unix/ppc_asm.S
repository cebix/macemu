/*
 *  asm_linux.S - Assembly routines
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

#include <ppc_asm.tmpl>
#include <xlowmem.h>

#define SAVE_FP_EXEC_68K 1


/*
 *  void *get_sp(void) - Get stack pointer
 */

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(get_sp)
C_SYMBOL_NAME(get_sp):
	mr	r3,r1
	blr


/*
 *  void *get_r2(void) - Get r2
 */

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(get_r2)
C_SYMBOL_NAME(get_r2):
	mr	r3,r2
	blr


/*
 *  void set_r2(void *val {r3}) - Set r2
 */

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(set_r2)
C_SYMBOL_NAME(set_r2):
	mr	r2,r3
	blr


/*
 *  void *get_r13(void) - Get r13 (small data pointer under Linux)
 */

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(get_r13)
C_SYMBOL_NAME(get_r13):
	mr	r3,r13
	blr

/*
 *  void set_r13(void *val {r3}) - Set r13 (small data pointer under Linux)
 */

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(set_r13)
C_SYMBOL_NAME(set_r13):
	mr	r13,r3
	blr


/*
 *  void flush_icache_range(void *start {r3}, void *end {r3}) - Flush D and I cache
 */

CACHE_LINE_SIZE = 32
LG_CACHE_LINE_SIZE = 5

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(flush_icache_range)
C_SYMBOL_NAME(flush_icache_range):
	li	r5,CACHE_LINE_SIZE-1
	andc	r3,r3,r5
	subf	r4,r3,r4
	add	r4,r4,r5
	srwi.	r4,r4,LG_CACHE_LINE_SIZE
	beqlr
	mtctr	r4
	mr	r6,r3
1:	dcbst	0,r3
	addi	r3,r3,CACHE_LINE_SIZE
	bdnz	1b
	sync				/* wait for dcbst's to get to ram */
	mtctr	r4
2:	icbi	0,r6
	addi	r6,r6,CACHE_LINE_SIZE
	bdnz	2b
	sync
	isync
	blr


/*
 *  long atomic_add(long *var{r3}, long add{r4}) - Atomic add operation
 *  long atomic_and(long *var{r3}, long and{r4}) - Atomic and operation
 *  long atomic_or(long *var{r3}, long or{r4}) - Atomic or operation
 *  int test_and_set(int *var{r3}, int val{r4}) - Atomic test-and-set
 */

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(atomic_add)
C_SYMBOL_NAME(atomic_add):
0:	dcbf	0,r3
	sync
	ori	r0,r0,1
	ori	r0,r0,1
	ori	r0,r0,1
	ori	r0,r0,1
	ori	r0,r0,1
	ori	r0,r0,1
	isync
	lwarx	r5,0,r3
	add	r0,r4,r5
	stwcx.	r0,0,r3
	bne-	0b
	mr	r3,r5
	isync
	blr

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(atomic_and)
C_SYMBOL_NAME(atomic_and):
0:	dcbf	0,r3
	sync
	ori	r0,r0,1
	ori	r0,r0,1
	ori	r0,r0,1
	ori	r0,r0,1
	ori	r0,r0,1
	ori	r0,r0,1
	isync
	lwarx	r5,0,r3
	and	r0,r4,r5
	stwcx.	r0,0,r3
	bne-	0b
	mr	r3,r5
	isync
	blr

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(atomic_or)
C_SYMBOL_NAME(atomic_or):
0:	dcbf	0,r3
	sync
	ori	r0,r0,1
	ori	r0,r0,1
	ori	r0,r0,1
	ori	r0,r0,1
	ori	r0,r0,1
	ori	r0,r0,1
	isync
	lwarx	r5,0,r3
	or	r0,r4,r5
	stwcx.	r0,0,r3
	bne-	0b
	mr	r3,r5
	isync
	blr

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(test_and_set)
C_SYMBOL_NAME(test_and_set):
0:	dcbf	0,r3
	sync
	ori	r0,r0,1
	ori	r0,r0,1
	ori	r0,r0,1
	ori	r0,r0,1
	ori	r0,r0,1
	ori	r0,r0,1
	isync
	lwarx	r5,0,r3
	cmpi	0,r5,0x0000
	bne	1f
	stwcx.	r4,0,r3
	bne-	0b
1:	isync     
	mr	r3,r5
	blr


/*
 *  void quit_emulator(void) - Jump to XLM_EMUL_RETURN_PROC
 */

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(quit_emulator)
C_SYMBOL_NAME(quit_emulator):
	lwz	r0,XLM_EMUL_RETURN_PROC(0)
	mtlr	r0
	blr


/*
 *  void jump_to_rom(uint32 entry {r3}, uint32 emulator_data {r4}) - Jump to Mac ROM
 */

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(jump_to_rom)
C_SYMBOL_NAME(jump_to_rom):
	// Create stack frame
	mflr	r0
	stw	r0,4(r1)
	stwu	r1,-(20+19*4+18*8)(r1)	// maintain 16 byte alignment

	// Save PowerPC registers
	stmw	r13,20(r1)
	stfd	f14,20+19*4+0*8(r1)
	stfd	f15,20+19*4+1*8(r1)
	stfd	f16,20+19*4+2*8(r1)
	stfd	f17,20+19*4+3*8(r1)
	stfd	f18,20+19*4+4*8(r1)
	stfd	f19,20+19*4+5*8(r1)
	stfd	f20,20+19*4+6*8(r1)
	stfd	f21,20+19*4+7*8(r1)
	stfd	f22,20+19*4+8*8(r1)
	stfd	f23,20+19*4+9*8(r1)
	stfd	f24,20+19*4+10*8(r1)
	stfd	f25,20+19*4+11*8(r1)
	stfd	f26,20+19*4+12*8(r1)
	stfd	f27,20+19*4+13*8(r1)
	stfd	f28,20+19*4+14*8(r1)
	stfd	f29,20+19*4+15*8(r1)
	stfd	f30,20+19*4+16*8(r1)
	stfd	f31,20+19*4+17*8(r1)

	// Move entry address to ctr
	mtctr	r3

	// Skip over EMUL_RETURN routine and get its address
	bl	1f


	/*
	 *  EMUL_RETURN: Returned from emulator
	 */

	// Restore PowerPC registers
	lwz	r1,XLM_EMUL_RETURN_STACK(0)
	RESTORE_SYSTEM_R2
	lmw	r13,20(r1)
	lfd	f14,20+19*4+0*8(r1)
	lfd	f15,20+19*4+1*8(r1)
	lfd	f16,20+19*4+2*8(r1)
	lfd	f17,20+19*4+3*8(r1)
	lfd	f18,20+19*4+4*8(r1)
	lfd	f19,20+19*4+5*8(r1)
	lfd	f20,20+19*4+6*8(r1)
	lfd	f21,20+19*4+7*8(r1)
	lfd	f22,20+19*4+8*8(r1)
	lfd	f23,20+19*4+9*8(r1)
	lfd	f24,20+19*4+10*8(r1)
	lfd	f25,20+19*4+11*8(r1)
	lfd	f26,20+19*4+12*8(r1)
	lfd	f27,20+19*4+13*8(r1)
	lfd	f28,20+19*4+14*8(r1)
	lfd	f29,20+19*4+15*8(r1)
	lfd	f30,20+19*4+16*8(r1)
	lfd	f31,20+19*4+17*8(r1)

	// Exiting from 68k emulator
	li	r0,1
	stw	r0,XLM_IRQ_NEST(0)
	li	r0,MODE_NATIVE
	stw	r0,XLM_RUN_MODE(0)

	// Return to caller of jump_to_rom()
	lwz	r0,20+19*4+18*8+4(r1)
	mtlr	r0
	addi	r1,r1,20+19*4+18*8
	blr


	// Save address of EMUL_RETURN routine for 68k emulator patch
1:	mflr	r0
	stw	r0,XLM_EMUL_RETURN_PROC(0)

	// Skip over EXEC_RETURN routine and get its address
	bl	2f


	/*
	 *  EXEC_RETURN: Returned from 68k routine executed with Execute68k()
	 */

	// Save r25 (contains current 68k interrupt level)
	stw	r25,XLM_68K_R25(0)

	// Reentering EMUL_OP mode
	li	r0,MODE_EMUL_OP
	stw	r0,XLM_RUN_MODE(0)

	// Save 68k registers
	lwz	r4,48(r1)	// Pointer to M68kRegisters
	stw	r8,0*4(r4)	// d[0]...d[7]
	stw	r9,1*4(r4)
	stw	r10,2*4(r4)
	stw	r11,3*4(r4)
	stw	r12,4*4(r4)
	stw	r13,5*4(r4)
	stw	r14,6*4(r4)
	stw	r15,7*4(r4)
	stw	r16,8*4(r4)	// a[0]..a[6]
	stw	r17,9*4(r4)
	stw	r18,10*4(r4)
	stw	r19,11*4(r4)
	stw	r20,12*4(r4)
	stw	r21,13*4(r4)
	stw	r22,14*4(r4)

	// Restore PowerPC registers
	lmw	r13,56(r1)
#if SAVE_FP_EXEC_68K
	lfd	f14,56+19*4+0*8(r1)
	lfd	f15,56+19*4+1*8(r1)
	lfd	f16,56+19*4+2*8(r1)
	lfd	f17,56+19*4+3*8(r1)
	lfd	f18,56+19*4+4*8(r1)
	lfd	f19,56+19*4+5*8(r1)
	lfd	f20,56+19*4+6*8(r1)
	lfd	f21,56+19*4+7*8(r1)
	lfd	f22,56+19*4+8*8(r1)
	lfd	f23,56+19*4+9*8(r1)
	lfd	f24,56+19*4+10*8(r1)
	lfd	f25,56+19*4+11*8(r1)
	lfd	f26,56+19*4+12*8(r1)
	lfd	f27,56+19*4+13*8(r1)
	lfd	f28,56+19*4+14*8(r1)
	lfd	f29,56+19*4+15*8(r1)
	lfd	f30,56+19*4+16*8(r1)
	lfd	f31,56+19*4+17*8(r1)
#endif

	// Return to caller
	lwz	r0,52(r1)
	mtcrf	0xff,r0
	lwz	r0,56+19*4+18*8+4(r1)
	mtlr	r0
	addi	r1,r1,56+19*4+18*8
	RESTORE_SYSTEM_R2
	RESTORE_SYSTEM_R13
	blr


	// Save address of EXEC_RETURN routine for 68k emulator patch
2:	mflr	r0
	stw	r0,XLM_EXEC_RETURN_PROC(0)

	// Skip over EMUL_BREAK/EMUL_OP routine and get its address
	bl	3f


	/*
	 *  EMUL_BREAK/EMUL_OP: Execute native routine, selector in r5 (my own private mode switch)
	 *
	 *  68k registers are stored in a M68kRegisters struct on the stack
	 *  which the native routine may read and modify
	 */

	// Save r25 (contains current 68k interrupt level)
	stw	r25,XLM_68K_R25(0)

	// Entering EMUL_OP mode within 68k emulator
	li	r0,MODE_EMUL_OP
	stw	r0,XLM_RUN_MODE(0)

	// Create PowerPC stack frame, reserve space for M68kRegisters
	mr	r3,r1
	subi	r1,r1,64	// Fake "caller" frame
	rlwinm	r1,r1,0,0,27	// Align stack

	mfcr	r0
	rlwinm	r0,r0,0,11,8
	stw	r0,4(r1)
	mfxer	r0
	stw	r0,16(r1)
	stw	r2,12(r1)
	stwu	r1,-(56+16*4+15*8)(r1)

	// Save 68k registers (M68kRegisters)
	stw	r8,56+0*4(r1)	// d[0]..d[7]
	stw	r9,56+1*4(r1)
	stw	r10,56+2*4(r1)
	stw	r11,56+3*4(r1)
	stw	r12,56+4*4(r1)
	stw	r13,56+5*4(r1)
	stw	r14,56+6*4(r1)
	stw	r15,56+7*4(r1)
	stw	r16,56+8*4(r1)	// a[0]..a[7]
	stw	r17,56+9*4(r1)
	stw	r18,56+10*4(r1)
	stw	r19,56+11*4(r1)
	stw	r20,56+12*4(r1)
	stw	r21,56+13*4(r1)
	stw	r22,56+14*4(r1)
	stw	r3,56+15*4(r1)
	stfd	f0,56+16*4+0*8(r1)
	stfd	f1,56+16*4+1*8(r1)
	stfd	f2,56+16*4+2*8(r1)
	stfd	f3,56+16*4+3*8(r1)
	stfd	f4,56+16*4+4*8(r1)
	stfd	f5,56+16*4+5*8(r1)
	stfd	f6,56+16*4+6*8(r1)
	stfd	f7,56+16*4+7*8(r1)
	mffs	f0
	stfd	f8,56+16*4+8*8(r1)
	stfd	f9,56+16*4+9*8(r1)
	stfd	f10,56+16*4+10*8(r1)
	stfd	f11,56+16*4+11*8(r1)
	stfd	f12,56+16*4+12*8(r1)
	stfd	f13,56+16*4+13*8(r1)
	stfd	f0,56+16*4+14*8(r1)

	// Execute native routine
	RESTORE_SYSTEM_R2
	RESTORE_SYSTEM_R13
	addi	r3,r1,56
	mr	r4,r24
	bl	C_SYMBOL_NAME(EmulOp)

	// Restore 68k registers (M68kRegisters)
	lwz	r8,56+0*4(r1)	// d[0]..d[7]
	lwz	r9,56+1*4(r1)
	lwz	r10,56+2*4(r1)
	lwz	r11,56+3*4(r1)
	lwz	r12,56+4*4(r1)
	lwz	r13,56+5*4(r1)
	lwz	r14,56+6*4(r1)
	lwz	r15,56+7*4(r1)
	lwz	r16,56+8*4(r1)	// a[0]..a[7]
	lwz	r17,56+9*4(r1)
	lwz	r18,56+10*4(r1)
	lwz	r19,56+11*4(r1)
	lwz	r20,56+12*4(r1)
	lwz	r21,56+13*4(r1)
	lwz	r22,56+14*4(r1)
	lwz	r3,56+15*4(r1)
	lfd	f13,56+16*4+14*8(r1)
	lfd	f0,56+16*4+0*8(r1)
	lfd	f1,56+16*4+1*8(r1)
	lfd	f2,56+16*4+2*8(r1)
	lfd	f3,56+16*4+3*8(r1)
	lfd	f4,56+16*4+4*8(r1)
	lfd	f5,56+16*4+5*8(r1)
	lfd	f6,56+16*4+6*8(r1)
	lfd	f7,56+16*4+7*8(r1)
	mtfsf	0xff,f13
	lfd	f8,56+16*4+8*8(r1)
	lfd	f9,56+16*4+9*8(r1)
	lfd	f10,56+16*4+10*8(r1)
	lfd	f11,56+16*4+11*8(r1)
	lfd	f12,56+16*4+12*8(r1)
	lfd	f13,56+16*4+13*8(r1)

	// Delete PowerPC stack frame
	lwz	r2,56+16*4+15*8+12(r1)
	lwz	r0,56+16*4+15*8+16(r1)
	mtxer	r0
	lwz	r0,56+16*4+15*8+4(r1)
	mtcrf	0xff,r0
	mr	r1,r3

	// Reentering 68k emulator
	li	r0,MODE_68K
	stw	r0,XLM_RUN_MODE(0)

	// Set r0 to 0 for 68k emulator
	li	r0,0

	// Execute next 68k opcode
	rlwimi	r29,r27,3,13,28
	lhau	r27,2(r24)
	mtlr	r29
	blr


	// Save address of EMUL_BREAK/EMUL_OP routine for 68k emulator patch
3:	mflr	r0
	stw	r0,XLM_EMUL_OP_PROC(0)

	// Save stack pointer for EMUL_RETURN
	stw	r1,XLM_EMUL_RETURN_STACK(0)

	// Preset registers for ROM boot routine
	lis	r3,0x40b0		// Pointer to ROM boot structure
	ori	r3,r3,0xd000

	// 68k emulator is now active
	li	r0,MODE_68K
	stw	r0,XLM_RUN_MODE(0)

	// Jump to ROM
	bctr


/*
 *  void execute_68k(uint32 pc {r3}, M68kRegisters *r {r4}) - Execute 68k routine
 */

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(execute_68k)
C_SYMBOL_NAME(execute_68k):
	// Create MacOS stack frame
	mflr	r0
	stw	r0,4(r1)
	stwu	r1,-(56+19*4+18*8)(r1)
	mfcr	r0
	stw	r4,48(r1)	// save pointer to M68kRegisters for EXEC_RETURN
	stw	r0,52(r1)	// save CR

	// Save PowerPC registers
	stmw	r13,56(r1)
#if SAVE_FP_EXEC_68K
	stfd	f14,56+19*4+0*8(r1)
	stfd	f15,56+19*4+1*8(r1)
	stfd	f16,56+19*4+2*8(r1)
	stfd	f17,56+19*4+3*8(r1)
	stfd	f18,56+19*4+4*8(r1)
	stfd	f19,56+19*4+5*8(r1)
	stfd	f20,56+19*4+6*8(r1)
	stfd	f21,56+19*4+7*8(r1)
	stfd	f22,56+19*4+8*8(r1)
	stfd	f23,56+19*4+9*8(r1)
	stfd	f24,56+19*4+10*8(r1)
	stfd	f25,56+19*4+11*8(r1)
	stfd	f26,56+19*4+12*8(r1)
	stfd	f27,56+19*4+13*8(r1)
	stfd	f28,56+19*4+14*8(r1)
	stfd	f29,56+19*4+15*8(r1)
	stfd	f30,56+19*4+16*8(r1)
	stfd	f31,56+19*4+17*8(r1)
#endif

	// Set up registers for 68k emulator
	lwz	r31,XLM_KERNEL_DATA(0)	// Pointer to Kernel Data
	addi	r31,r31,0x1000
	li	r0,0
	mtcrf	0xff,r0
	creqv	11,11,11			// Supervisor mode
	lwz	r8,0*4(r4)	// d[0]..d[7]
	lwz	r9,1*4(r4)
	lwz	r10,2*4(r4)
	lwz	r11,3*4(r4)
	lwz	r12,4*4(r4)
	lwz	r13,5*4(r4)
	lwz	r14,6*4(r4)
	lwz	r15,7*4(r4)
	lwz	r16,8*4(r4)	// a[0]..a[6]
	lwz	r17,9*4(r4)
	lwz	r18,10*4(r4)
	lwz	r19,11*4(r4)
	lwz	r20,12*4(r4)
	lwz	r21,13*4(r4)
	lwz	r22,14*4(r4)
	li	r23,0
	mr	r24,r3
	lwz	r25,XLM_68K_R25(0)		// MSB of SR
	li	r26,0
	li	r28,0				// VBR
	lwz	r29,0x74(r31)		// Pointer to opcode table
	lwz	r30,0x78(r31)		// Address of emulator

	// Push return address (points to EXEC_RETURN opcode) on stack
	li	r0,XLM_EXEC_RETURN_OPCODE
	stwu	r0,-4(r1)

	// Reentering 68k emulator
	li	r0,MODE_68K
	stw	r0,XLM_RUN_MODE(0)

	// Set r0 to 0 for 68k emulator
	li	r0,0

	// Execute 68k opcode
	lha	r27,0(r24)
	rlwimi	r29,r27,3,13,28
	lhau	r27,2(r24)
	mtlr	r29
	blr


/*
 *  uint32 call_macos1(uint32 tvect{r3}, uint32 arg1{r4}) ... - Call MacOS routines
 */

ASM_MACRO_START	prolog
	mflr	r0
	stw	r0,4(r1)
	stwu	r1,-64(r1)
ASM_MACRO_END

ASM_MACRO_START	epilog
	lwz	r0,64+4(r1)
	mtlr	r0
	addi	r1,r1,64
	RESTORE_SYSTEM_R2
	RESTORE_SYSTEM_R13
	blr
ASM_MACRO_END

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(call_macos)
C_SYMBOL_NAME(call_macos):
	prolog
	lwz	r0,0(r3)	// Get routine address
	lwz	r2,4(r3)	// Load TOC pointer
	mtctr	r0
	bctrl
	epilog

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(call_macos1)
C_SYMBOL_NAME(call_macos1):
	prolog
	lwz	r0,0(r3)	// Get routine address
	lwz	r2,4(r3)	// Load TOC pointer
	mtctr	r0
	mr	r3,r4
	bctrl
	epilog

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(call_macos2)
C_SYMBOL_NAME(call_macos2):
	prolog
	lwz	r0,0(r3)	// Get routine address
	lwz	r2,4(r3)	// Load TOC pointer
	mtctr	r0
	mr	r3,r4
	mr	r4,r5
	bctrl
	epilog

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(call_macos3)
C_SYMBOL_NAME(call_macos3):
	prolog
	lwz	r0,0(r3)	// Get routine address
	lwz	r2,4(r3)	// Load TOC pointer
	mtctr	r0
	mr	r3,r4
	mr	r4,r5
	mr	r5,r6
	bctrl
	epilog

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(call_macos4)
C_SYMBOL_NAME(call_macos4):
	prolog
	lwz	r0,0(r3)	// Get routine address
	lwz	r2,4(r3)	// Load TOC pointer
	mtctr	r0
	mr	r3,r4
	mr	r4,r5
	mr	r5,r6
	mr	r6,r7
	bctrl
	epilog

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(call_macos5)
C_SYMBOL_NAME(call_macos5):
	prolog
	lwz	r0,0(r3)	// Get routine address
	lwz	r2,4(r3)	// Load TOC pointer
	mtctr	r0
	mr	r3,r4
	mr	r4,r5
	mr	r5,r6
	mr	r6,r7
	mr	r7,r8
	bctrl
	epilog

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(call_macos6)
C_SYMBOL_NAME(call_macos6):
	prolog
	lwz	r0,0(r3)	// Get routine address
	lwz	r2,4(r3)	// Load TOC pointer
	mtctr	r0
	mr	r3,r4
	mr	r4,r5
	mr	r5,r6
	mr	r6,r7
	mr	r7,r8
	mr	r8,r9
	bctrl
	epilog

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(call_macos7)
C_SYMBOL_NAME(call_macos7):
	prolog
	lwz	r0,0(r3)	// Get routine address
	lwz	r2,4(r3)	// Load TOC pointer
	mtctr	r0
	mr	r3,r4
	mr	r4,r5
	mr	r5,r6
	mr	r6,r7
	mr	r7,r8
	mr	r8,r9
	mr	r9,r10
	bctrl
	epilog


/*
 *  Native resource manager patches
 */

ASM_MACRO_START	do_get_resource ASM_MACRO_ARG0_DEF
	// Create stack frame
	mflr	r0
	stw	r0,8(r1)
	stwu	r1,-(56+12)(r1)

	// Save type/ID
	stw	r3,56(r1)
	stw	r4,56+4(r1)

	// Call old routine
	lwz	r0,ASM_MACRO_ARG0(0)
	lwz	r2,XLM_RES_LIB_TOC(0)
	mtctr	r0
	bctrl
	stw	r3,56+8(r1)		// Save handle

	// Call CheckLoad
	RESTORE_SYSTEM_R2
	RESTORE_SYSTEM_R13
	lwz	r3,56(r1)
	lha	r4,56+6(r1)
	lwz	r5,56+8(r1)
	bl	C_SYMBOL_NAME(check_load_invoc)
	lwz	r3,56+8(r1)		// Restore handle

	// Return to caller
	lwz	r0,56+12+8(r1)
	mtlr	r0
	addi	r1,r1,56+12
	blr
ASM_MACRO_END

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(get_resource)
C_SYMBOL_NAME(get_resource):
	do_get_resource XLM_GET_RESOURCE

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(get_1_resource)
C_SYMBOL_NAME(get_1_resource):
	do_get_resource XLM_GET_1_RESOURCE

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(get_ind_resource)
C_SYMBOL_NAME(get_ind_resource):
	do_get_resource XLM_GET_IND_RESOURCE

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(get_1_ind_resource)
C_SYMBOL_NAME(get_1_ind_resource):
	do_get_resource XLM_GET_1_IND_RESOURCE

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(r_get_resource)
C_SYMBOL_NAME(r_get_resource):
	do_get_resource XLM_R_GET_RESOURCE

ASM_MACRO_START	do_get_named_resource ASM_MACRO_ARG0_DEF
	// Create stack frame
	mflr	r0
	stw	r0,8(r1)
	stwu	r1,-(56+12)(r1)

	// Save type/ID
	stw	r3,56(r1)
	stw	r4,56+4(r1)

	// Call old routine
	lwz	r0,ASM_MACRO_ARG0(0)
	lwz	r2,XLM_RES_LIB_TOC(0)
	mtctr	r0
	bctrl
	stw	r3,56+8(r1)		// Save handle

	// Call CheckLoad
	RESTORE_SYSTEM_R2
	RESTORE_SYSTEM_R13
	lwz	r3,56(r1)
	lwz	r4,56+4(r1)
	lwz	r5,56+8(r1)
	bl	C_SYMBOL_NAME(named_check_load_invoc)
	lwz	r3,56+8(r1)		// Restore handle

	// Return to caller
	lwz	r0,56+12+8(r1)
	mtlr	r0
	addi	r1,r1,56+12
	blr
ASM_MACRO_END

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(get_named_resource)
C_SYMBOL_NAME(get_named_resource):
	do_get_named_resource XLM_GET_NAMED_RESOURCE

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(get_1_named_resource)
C_SYMBOL_NAME(get_1_named_resource):
	do_get_named_resource XLM_GET_1_NAMED_RESOURCE


/*
 *  void ppc_interrupt(uint32 entry{r3}, uint32 kernel_data{r4}) - Execute PPC interrupt
 */

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(ppc_interrupt)
C_SYMBOL_NAME(ppc_interrupt):
	mflr	r0
	stw	r0,4(r1)
	stwu	r1,-64(r1)

	// Get address of return routine
	bl	1f

	// Return routine
	lwz	r0,64+4(r1)
	mtlr	r0
	addi	r1,r1,64
	blr

	// Prepare registers for nanokernel interrupt routine
1:	mtctr	r1
	mr	r1,r4
	stw	r6,0x018(r1)
	mfctr	r6
	stw	r6,0x004(r1)
	lwz	r6,0x65c(r1)
	stw	r7,0x13c(r6)
	stw	r8,0x144(r6)
	stw	r9,0x14c(r6)
	stw	r10,0x154(r6)
	stw	r11,0x15c(r6)
	stw	r12,0x164(r6)
	stw	r13,0x16c(r6)

	mflr	r10
	mfcr	r13
	lwz	r7,0x660(r1)
	mflr	r12
	rlwimi.	r7,r7,8,0,0
	li	r11,0
	ori	r11,r11,0xf072	// MSR (SRR1)
	mtcrf	0x70,r11
	li	r8,0

	// Enter nanokernel
	mtlr	r3
	blr


/*
 *  Define signal handlers with alternate stack initialization magic
 */

#define SIG_STACK_SIZE 0x10000

ASM_MACRO_START do_define_signal_handler	\
	ASM_MACRO_ARG0_DEF /* name */		\
	ASM_MACRO_ARG1_DEF /* stack */		\
	ASM_MACRO_ARG2_DEF /* stack id */	\
	ASM_MACRO_ARG3_DEF /* signal handler */

	// Alternate stack lower base for this signal handler
	.lcomm ASM_MACRO_ARG1,SIG_STACK_SIZE,ASM_ALIGN_2(4)
	ASM_TYPE(ASM_MACRO_ARG1,@object)

	// Represents the current nest level for this signal handler
	// Note that in SheepShaver, SIGUSR2 signals are blocked while
	// handling other signals so, it's unlikely we ever get a nest
	// level greater than 1
	.lcomm ASM_MACRO_ARG2,4,ASM_ALIGN_2(2)
	ASM_TYPE(ASM_MACRO_ARG2,@object)

	ASM_GLOBAL_DIRECTIVE C_SYMBOL_NAME(ASM_MACRO_ARG0)
C_SYMBOL_NAME(ASM_MACRO_ARG0):
	// Preserve args in scratch registers
	mflr	r14
	mr	r15,r3
	mr	r16,r4
	mr	r17,r5
	mr	r18,r1

	// Atomically increase stack_id
	lis	r19,ASM_HA16(ASM_MACRO_ARG2)
	la	r19,ASM_LO16(ASM_MACRO_ARG2,r19)
	li	r4,1
	mr	r3,r19
	bl	C_SYMBOL_NAME(atomic_add)
	cmpwi	r3,0
	bne-	1f

	// ID was 0, we can use the local stack
	lis	r9,ASM_HA16(ASM_MACRO_ARG1)
	lis	r3,(SIG_STACK_SIZE>>16)
	la	r9,ASM_LO16(ASM_MACRO_ARG1,r9)
	addi	r3,r3,((SIG_STACK_SIZE&0xffff)-64)
	add	r1,r9,r3

1:	// Invoke signal handler
	stwu	r1,-16(r1)
	mr	r3,r15
	mr	r4,r16
	mr	r5,r17
	bl	C_SYMBOL_NAME(ASM_MACRO_ARG3)
	addi	r1,r1,16

	// Atomically decrease stack id
	mr	r3,r19
	li	r4,-1
	bl	C_SYMBOL_NAME(atomic_add)

	// Restore kernel stack and return
	mtlr	r14
	mr	r1,r18
	blr
ASM_MACRO_END

#define DEFINE_SIGNAL_HANDLER(NAME)		  \
	do_define_signal_handler		  \
	NAME##_handler_init	ASM_MACRO_ARG_SEP \
	NAME##_stack		ASM_MACRO_ARG_SEP \
	NAME##_stack_id		ASM_MACRO_ARG_SEP \
	NAME##_handler

DEFINE_SIGNAL_HANDLER(sigusr2)
