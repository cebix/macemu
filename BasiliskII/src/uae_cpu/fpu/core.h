/*
 *  fpu/core.h - base fpu context definition
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
 *
 *  MC68881/68040 fpu emulation
 *  
 *  Original UAE FPU, copyright 1996 Herman ten Brugge
 *  Rewrite for x86, copyright 1999-2000 Lauri Pesonen
 *  New framework, copyright 2000 Gwenole Beauchesne
 *  Adapted for JIT compilation (c) Bernd Meyer, 2000
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

#ifndef FPU_CORE_H
#define FPU_CORE_H

#include "sysdeps.h"
#include "fpu/types.h"

/* Always use x87 FPU stack on IA-32.  */
#if defined(X86_ASSEMBLY)
#define USE_X87_ASSEMBLY 1
#endif

/* Only use x87 FPU on x86-64 if long double precision is requested.  */
#if defined(X86_64_ASSEMBLY) && USE_LONG_DOUBLE
#define USE_X87_ASSEMBLY 1
#endif

/* ========================================================================== */
/* ========================= FPU CONTEXT DEFINITION ========================= */
/* ========================================================================== */

/*	We don't use all features of the C++ language so that we may still
 *	easily backport that code to C.
 */

struct fpu_t {

	/* ---------------------------------------------------------------------- */
	/* --- Floating-Point Data Registers                                  --- */
	/* ---------------------------------------------------------------------- */
	
	/* The eight %fp0 .. %fp7 registers */
	fpu_register	registers[8];
	
	/* Used for lazy evalualation of FPU flags */
	fpu_register	result;
	
	/* ---------------------------------------------------------------------- */
	/* --- Floating-Point Control Register                                --- */
	/* ---------------------------------------------------------------------- */
	
	struct		{
	
	/* Exception Enable Byte */
	uae_u32		exception_enable;
	#define		FPCR_EXCEPTION_ENABLE	0x0000ff00
	#define		FPCR_EXCEPTION_BSUN		0x00008000
	#define		FPCR_EXCEPTION_SNAN		0x00004000
	#define		FPCR_EXCEPTION_OPERR	0x00002000
	#define		FPCR_EXCEPTION_OVFL		0x00001000
	#define		FPCR_EXCEPTION_UNFL		0x00000800
	#define		FPCR_EXCEPTION_DZ		0x00000400
	#define		FPCR_EXCEPTION_INEX2	0x00000200
	#define		FPCR_EXCEPTION_INEX1	0x00000100
	
	/* Mode Control Byte Mask */
	#define		FPCR_MODE_CONTROL		0x000000ff
	
	/* Rounding precision */
	uae_u32		rounding_precision;
	#define		FPCR_ROUNDING_PRECISION	0x000000c0
	#define		FPCR_PRECISION_SINGLE	0x00000040
	#define		FPCR_PRECISION_DOUBLE	0x00000080
	#define		FPCR_PRECISION_EXTENDED	0x00000000
	
	/* Rounding mode */
	uae_u32		rounding_mode;
	#define		FPCR_ROUNDING_MODE		0x00000030
	#define		FPCR_ROUND_NEAR			0x00000000
	#define		FPCR_ROUND_ZERO			0x00000010
	#define		FPCR_ROUND_MINF			0x00000020
	#define		FPCR_ROUND_PINF			0x00000030
	
	}			fpcr;
	
	/* ---------------------------------------------------------------------- */
	/* --- Floating-Point Status Register                                 --- */
	/* ---------------------------------------------------------------------- */
	
	struct		{
	
	/* Floating-Point Condition Code Byte */
	uae_u32		condition_codes;
	#define		FPSR_CCB				0xff000000
	#define		FPSR_CCB_NEGATIVE		0x08000000
	#define		FPSR_CCB_ZERO			0x04000000
	#define		FPSR_CCB_INFINITY		0x02000000
	#define		FPSR_CCB_NAN			0x01000000
	
	/* Quotient Byte */
	uae_u32		quotient;
	#define		FPSR_QUOTIENT			0x00ff0000
	#define		FPSR_QUOTIENT_SIGN		0x00800000
	#define		FPSR_QUOTIENT_VALUE		0x007f0000
	
	/* Exception Status Byte */
	uae_u32		exception_status;
	#define		FPSR_EXCEPTION_STATUS	FPCR_EXCEPTION_ENABLE
	#define		FPSR_EXCEPTION_BSUN		FPCR_EXCEPTION_BSUN
	#define		FPSR_EXCEPTION_SNAN		FPCR_EXCEPTION_SNAN
	#define		FPSR_EXCEPTION_OPERR	FPCR_EXCEPTION_OPERR
	#define		FPSR_EXCEPTION_OVFL		FPCR_EXCEPTION_OVFL
	#define		FPSR_EXCEPTION_UNFL		FPCR_EXCEPTION_UNFL
	#define		FPSR_EXCEPTION_DZ		FPCR_EXCEPTION_DZ
	#define		FPSR_EXCEPTION_INEX2	FPCR_EXCEPTION_INEX2
	#define		FPSR_EXCEPTION_INEX1	FPCR_EXCEPTION_INEX1
	
	/* Accrued Exception Byte */
	uae_u32		accrued_exception;
	#define		FPSR_ACCRUED_EXCEPTION	0x000000ff
	#define		FPSR_ACCR_IOP			0x00000080
	#define		FPSR_ACCR_OVFL			0x00000040
	#define		FPSR_ACCR_UNFL			0x00000020
	#define		FPSR_ACCR_DZ			0x00000010
	#define		FPSR_ACCR_INEX			0x00000008
	
	}			fpsr;
	
	/* ---------------------------------------------------------------------- */
	/* --- Floating-Point Instruction Address Register                    --- */
	/* ---------------------------------------------------------------------- */
	
	uae_u32		instruction_address;
	
	/* ---------------------------------------------------------------------- */
	/* --- Initialization / Finalization                                  --- */
	/* ---------------------------------------------------------------------- */
	
	/* Flag set if we emulate an integral 68040 FPU */
	bool		is_integral;
	
	/* ---------------------------------------------------------------------- */
	/* --- Extra FPE-dependant defines                                    --- */
	/* ---------------------------------------------------------------------- */
	
	#if			defined(FPU_X86) \
			||	(defined(FPU_UAE) && defined(USE_X87_ASSEMBLY)) \
			||	(defined(FPU_IEEE) && defined(USE_X87_ASSEMBLY))
	
	#define		CW_RESET				0x0040	// initial CW value after RESET
	#define		CW_FINIT				0x037F	// initial CW value after FINIT
	#define		SW_RESET				0x0000	// initial SW value after RESET
	#define		SW_FINIT				0x0000	// initial SW value after FINIT
	#define		TW_RESET				0x5555	// initial TW value after RESET
	#define		TW_FINIT				0x0FFF	// initial TW value after FINIT
	
	#define		CW_X					0x1000	// infinity control
	#define		CW_RC_ZERO				0x0C00	// rounding control toward zero
	#define		CW_RC_UP				0x0800	// rounding control toward +
	#define		CW_RC_DOWN				0x0400	// rounding control toward -
	#define		CW_RC_NEAR				0x0000	// rounding control toward even
	#define		CW_PC_EXTENDED			0x0300	// precision control 64bit
	#define		CW_PC_DOUBLE			0x0200	// precision control 53bit
	#define		CW_PC_RESERVED			0x0100	// precision control reserved
	#define		CW_PC_SINGLE			0x0000	// precision control 24bit
	#define		CW_PM					0x0020	// precision exception mask
	#define		CW_UM					0x0010	// underflow exception mask
	#define		CW_OM					0x0008	// overflow exception mask
	#define		CW_ZM					0x0004	// zero divide exception mask
	#define		CW_DM					0x0002	// denormalized operand exception mask
	#define		CW_IM					0x0001	// invalid operation exception mask
	
	#define		SW_B					0x8000	// busy flag
	#define		SW_C3					0x4000	// condition code flag 3
	#define		SW_TOP_7				0x3800	// top of stack = ST(7)
	#define		SW_TOP_6				0x3000	// top of stack = ST(6)
	#define		SW_TOP_5				0x2800	// top of stack = ST(5)
	#define		SW_TOP_4				0x2000	// top of stack = ST(4)
	#define		SW_TOP_3				0x1800	// top of stack = ST(3)
	#define		SW_TOP_2				0x1000	// top of stack = ST(2)
	#define		SW_TOP_1				0x0800	// top of stack = ST(1)
	#define		SW_TOP_0				0x0000	// top of stack = ST(0)
	#define		SW_C2					0x0400	// condition code flag 2
	#define		SW_C1					0x0200	// condition code flag 1
	#define		SW_C0					0x0100	// condition code flag 0
	#define		SW_ES					0x0080	// error summary status flag
	#define		SW_SF					0x0040	// stack fault flag
	#define		SW_PE					0x0020	// precision exception flag
	#define		SW_UE					0x0010	// underflow exception flag
	#define		SW_OE					0x0008	// overflow exception flag
	#define		SW_ZE					0x0004	// zero divide exception flag
	#define		SW_DE					0x0002	// denormalized operand exception flag
	#define		SW_IE					0x0001	// invalid operation exception flag
	
	#define		X86_ROUNDING_MODE		0x0C00
	#define		X86_ROUNDING_PRECISION	0x0300
	
	#endif		/* FPU_X86 */
	
};

/* We handle only one global fpu */
extern fpu_t fpu;

/* Return the address of a particular register */
inline fpu_register * const fpu_register_address(int i)
	{ return &fpu.registers[i]; }

/* Dump functions for m68k_dumpstate */
extern void fpu_dump_registers(void);
extern void fpu_dump_flags(void);

/* Accessors to FPU Control Register */
static inline uae_u32 get_fpcr(void);
static inline void set_fpcr(uae_u32 new_fpcr);

/* Accessors to FPU Status Register */
static inline uae_u32 get_fpsr(void);
static inline void set_fpsr(uae_u32 new_fpsr);
	
/* Accessors to FPU Instruction Address Register */
static inline uae_u32 get_fpiar();
static inline void set_fpiar(uae_u32 new_fpiar);

/* Initialization / Finalization */
extern void fpu_init(bool integral_68040);
extern void fpu_exit(void);
extern void fpu_reset(void);
	
/* Floating-point arithmetic instructions */
void fpuop_arithmetic(uae_u32 opcode, uae_u32 extra) REGPARAM;

/* Floating-point program control operations */
void fpuop_bcc(uae_u32 opcode, uaecptr pc, uae_u32 extra) REGPARAM;
void fpuop_dbcc(uae_u32 opcode, uae_u32 extra) REGPARAM;
void fpuop_scc(uae_u32 opcode, uae_u32 extra) REGPARAM;

/* Floating-point system control operations */
void fpuop_save(uae_u32 opcode) REGPARAM;
void fpuop_restore(uae_u32 opcode) REGPARAM;
void fpuop_trapcc(uae_u32 opcode, uaecptr oldpc) REGPARAM;

#endif /* FPU_CORE_H */
