/*
 *  fpu_x86.h - 68881/68040 fpu code for x86/Windows and Linux/x86.
 *
 *  Basilisk II (C) 1997-2001 Christian Bauer
 *
 *  MC68881 emulation
 *
 *  Based on UAE FPU, original copyright 1996 Herman ten Brugge,
 *  rewritten by Lauri Pesonen 1999-2000,
 *  accomodated to GCC's Extended Asm syntax by Gwenole Beauchesne 2000.
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

/* gb-- defined in newcpu.h
// is_integral: true == 68040, false == 68881
void fpu_set_integral_fpu( bool is_integral );

// MUST be called before the cpu starts up.
void fpu_init( void );

// Finalize.
void fpu_exit( void );

// Soft reset.
void fpu_reset( void );
*/

// The same as original. "ftrapcc_opp" is bound to change soon.
/* gb-- defined in newcpu.h
void REGPARAM2 fpp_opp (uae_u32, uae_u16);
void REGPARAM2 fdbcc_opp (uae_u32, uae_u16);
void REGPARAM2 fscc_opp (uae_u32, uae_u16);
void REGPARAM2 ftrapcc_opp (uae_u32,uaecptr);
void REGPARAM2 fbcc_opp (uae_u32, uaecptr, uae_u32);
void REGPARAM2 fsave_opp (uae_u32);
void REGPARAM2 frestore_opp (uae_u32);
*/

/* ---------------------------- Motorola ---------------------------- */

// Exception byte
#define BSUN		0x00008000
#define SNAN		0x00004000
#define OPERR		0x00002000
#define OVFL		0x00001000
#define UNFL		0x00000800
#define DZ			0x00000400
#define INEX2		0x00000200
#define INEX1		0x00000100

// Accrued exception byte
#define ACCR_IOP	0x80
#define ACCR_OVFL	0x40
#define ACCR_UNFL	0x20
#define ACCR_DZ		0x10
#define ACCR_INEX	0x08

// fpcr rounding modes
#define ROUND_CONTROL_MASK				0x30
#define ROUND_TO_NEAREST				0
#define ROUND_TO_ZERO					0x10
#define ROUND_TO_NEGATIVE_INFINITY		0x20
#define ROUND_TO_POSITIVE_INFINITY		0x30

// fpcr precision control
#define PRECISION_CONTROL_MASK			0xC0
#define PRECISION_CONTROL_EXTENDED		0
#define PRECISION_CONTROL_DOUBLE		0x80
#define PRECISION_CONTROL_SINGLE		0x40
#define PRECISION_CONTROL_UNDEFINED		0xC0


/* ---------------------------- Intel ---------------------------- */

#define CW_RESET                0x0040           // initial CW value after RESET
#define CW_FINIT                0x037F           // initial CW value after FINIT
#define SW_RESET                0x0000           // initial SW value after RESET
#define SW_FINIT                0x0000           // initial SW value after FINIT
#define TW_RESET                0x5555           // initial TW value after RESET
#define TW_FINIT                0x0FFF           // initial TW value after FINIT

#define CW_X                    0x1000           // infinity control
#define CW_RC_ZERO              0x0C00           // rounding control toward zero
#define CW_RC_UP                0x0800           // rounding control toward +
#define CW_RC_DOWN              0x0400           // rounding control toward -
#define CW_RC_NEAR              0x0000           // rounding control toward even
#define CW_PC_EXTENDED          0x0300           // precision control 64bit
#define CW_PC_DOUBLE            0x0200           // precision control 53bit
#define CW_PC_RESERVED          0x0100           // precision control reserved
#define CW_PC_SINGLE            0x0000           // precision control 24bit
#define CW_PM                   0x0020           // precision exception mask
#define CW_UM                   0x0010           // underflow exception mask
#define CW_OM                   0x0008           // overflow exception mask
#define CW_ZM                   0x0004           // zero divide exception mask
#define CW_DM                   0x0002           // denormalized operand exception mask
#define CW_IM                   0x0001           // invalid operation exception mask

#define SW_B                    0x8000           // busy flag
#define SW_C3                   0x4000           // condition code flag 3
#define SW_TOP_7                0x3800           // top of stack = ST(7)
#define SW_TOP_6                0x3000           // top of stack = ST(6)
#define SW_TOP_5                0x2800           // top of stack = ST(5)
#define SW_TOP_4                0x2000           // top of stack = ST(4)
#define SW_TOP_3                0x1800           // top of stack = ST(3)
#define SW_TOP_2                0x1000           // top of stack = ST(2)
#define SW_TOP_1                0x0800           // top of stack = ST(1)
#define SW_TOP_0                0x0000           // top of stack = ST(0)
#define SW_C2                   0x0400           // condition code flag 2
#define SW_C1                   0x0200           // condition code flag 1
#define SW_C0                   0x0100           // condition code flag 0
#define SW_ES                   0x0080           // error summary status flag
#define SW_SF                   0x0040           // stack fault flag
#define SW_PE                   0x0020           // precision exception flag
#define SW_UE                   0x0010           // underflow exception flag
#define SW_OE                   0x0008           // overflow exception flag
#define SW_ZE                   0x0004           // zero divide exception flag
#define SW_DE                   0x0002           // denormalized operand exception flag
#define SW_IE                   0x0001           // invalid operation exception flag

#define X86_ROUND_CONTROL_MASK			0x0C00
#define X86_PRECISION_CONTROL_MASK		0x0300
