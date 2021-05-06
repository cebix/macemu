/*
 *  fpu_x86.cpp - 68881/68040 fpu code for x86/Windows an Linux/x86.
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
 *
 *  MC68881/68040 fpu emulation
 *
 *  Based on UAE FPU, original copyright 1996 Herman ten Brugge,
 *  rewritten for x86 by Lauri Pesonen 1999-2000,
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
 *
 *
 *	Interface
 *		Almost the same as original. Please see the comments in "fpu.h".
 *
 *
 *	Why assembly?
 *		The reason is not really speed, but to get infinities,
 *		NANs and flags finally working.
 *
 *
 *	How to maintain Mac and x86 FPU flags -- plan B
 *
 *	regs.piar is not updated.
 *
 *	regs.FPU fpcr always contains the real 68881/68040 control word.
 *
 *	regs.FPU fpsr is not kept up-to-date, for efficiency reasons.
 *	Most of the FPU commands update this in a way or another, but it is not
 *	read nearly that often. Therefore, three host-specific words hold the
 *	status byte and exception byte ("x86_status_word"), accrued exception
 *	byte ("x86_status_word_accrued") and the quotient byte ("FPU fpsr.quotient"),
 *	as explained below.
 *
 *	CONDITION CODE - QUOTIENT - EXCEPTION STATUS - ACCRUED EXCEPTION
 *		CONDITION CODE (N,Z,I,NAN)
 *		-	updated after each opcode, if needed.
 *		-	x86 assembly opcodes call FXAM and store the status word to
 *			"x86_status_word".
 *		-	When regs.FPU fpsr is actually used, the value of "x86_status_word"
 *			is translated.
 *		QUOTIENT BYTE
 *		-	Updated by frem, fmod, frestore(null frame)
 *		-	Stored in "FPU fpsr.quotient" in correct bit position, combined when
 *			regs.FPU fpsr is actually used.
 *		EXCEPTION STATUS (BSUN,SNAN,OPERR,OVFL,UNFL,DZ,INEX2,INEX1)
 *		-	updated after each opcode, if needed.
 *		-	Saved in x86 form in "x86_status_word".
 *		-	When regs.FPU fpsr is actually used, the value of "x86_status_word"
 *			is translated.
 *		-	Only fcc_op can set BSUN
 *		ACCRUED EXCEPTION (ACCR_IOP,ACCR_OVFL,ACCR_UNFL,ACCR_DZ,ACCR_INEX)
 *		-	updated after each opcode, if needed.
 *		-	Logically OR'ed in x86 form to "x86_status_word_accrued".
 *		-	When regs.FPU fpsr is actually used, the value of
 *			"x86_status_word_accrued" is translated.
 *		
 *		When "x86_status_word" and "x86_status_word_accrued" are stored,
 *		all pending x86 FPU exceptions are cleared, if there are any.
 *
 *		Writing to "regs.FPU fpsr" reverse-maps to x86 status/exception values and
 *		stores the values in "x86_status_word", "x86_status_word_accrued"
 *		and "FPU fpsr.quotient".
 *
 *		So, "x86_status_word" and "x86_status_word_accrued" are not in
 *		correct bit positions and have x86 values, but "FPU fpsr.quotient" is at
 *		correct position.
 *
 *		Note that it does not matter that the reverse-mapping is not exact
 *		(both SW_IE and SW_DE are mapped to ACCR_IOP, but ACCR_IOP maps to
 *		SW_IE only), the MacOS always sees the correct exception bits.
 *
 *		Also note the usage of the fake BSUN flag SW_FAKE_BSUN. If you change
 *		the x86 FPU code, you must make sure that you don't generate any FPU
 *		stack faults.
 *
 *
 *	x86 co-processor initialization:
 *
 *	Bit  Code   Use
 *	 0   IM     Invalid operation exception mask     1    Disabled
 *	 1   DM     Denormalized operand exception mask  1    Disabled
 *	 2   ZM     Zerodivide exception mask            1    Disabled
 *	 3   OM     Overflow exception mask              1    Disabled
 *	 4   UM     Underflow exception mask             1    Disabled
 *	 5   PM     Precision exception mask             1    Disabled
 *	 6   -      -                                    -    -
 *	 7   IEM    Interrupt enable mask                0    Enabled
 *	 8   PC     Precision control\                   1  - 64 bits
 *	 9   PC     Precision control/                   1  /
 *	10   RC     Rounding control\                    0  - Nearest even
 *	11   RC     Rounding control/                    0  /
 *	12   IC     Infinity control                     1    Affine
 *	13   -      -                                    -    -
 *	14   -      -                                    -    -
 *	15   -      -                                    -    -
 *
 *
 *  TODO:
 *    - Exceptions are not implemented.
 *    - All tbyte variables should be aligned to 16-byte boundaries.
 *		  (for best efficiency).
 *		- FTRAPcc code looks like broken.
 *		- If USE_3_BIT_QUOTIENT is 0, exceptions should be checked after
 *			float -> int rounding (frem,fmod).
 *		- The speed can be greatly improved. Do this only after you are sure
 *			that there are no major bugs.
 *		- Support for big-endian byte order (but all assembly code needs to
 *			be rewritten anyway)
 *			I have some non-portable code like *((uae_u16 *)&m68k_dreg(regs, reg)) = newv;
 *			Sorry about that, you need to change these. I could do it myself, but better
 *			not, I would have no way to test them out.
 *			I tried to mark all spots with a comment TODO_BIGENDIAN.
 *		- to_double() may need renormalization code. Or then again, maybe not.
 *		- Signaling NANs should be handled better. The current mapping of
 *			signaling nan exception to denormalized operand exception is only
 *			based on the idea that the (possible) handler sees that "something
 *			seriously wrong" and takes the same action. Should not really get (m)any
 *			of those since normalization is handled on to_exten()
 *			
 */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "sysdeps.h"
#include "memory.h"
#include "readcpu.h"
#include "newcpu.h"
#define FPU_IMPLEMENTATION
#include "fpu/fpu.h"
#include "fpu/fpu_x86.h"
#include "fpu/fpu_x86_asm.h"

/* Global FPU context */
fpu_t fpu;

/* -------------------------------------------------------------------------- */
/* --- Native Support                                                     --- */
/* -------------------------------------------------------------------------- */

#include "fpu/flags.h"
#include "fpu/exceptions.h"
#include "fpu/rounding.h"
#include "fpu/impl.h"

#include "fpu/flags.cpp"
#include "fpu/exceptions.cpp"
#include "fpu/rounding.cpp"

/* -------------------------------------------------------------------------- */
/* --- Scopes Definition                                                  --- */
/* -------------------------------------------------------------------------- */

#undef	PUBLIC
#define PUBLIC	/**/

#undef	PRIVATE
#define PRIVATE	static

#undef	FFPU
#define FFPU	/**/

#undef	FPU
#define	FPU		fpu.

/* ---------------------------- Compatibility ---------------------------- */

#define BYTE		uint8
#define WORD		uint16
#define DWORD		uint32
#define min(a, b)	(((a) < (b)) ? (a) : (b))

/* ---------------------------- Configuration ---------------------------- */

/*
If USE_3_BIT_QUOTIENT is set to 1, FREM and FMOD use a faster version
with only 3 quotient bits (those provided by the x86 FPU). If set to 0,
they calculate the same 7 bits that m68k does. It seems (as for now) that
3 bits suffice for all Mac programs I have tried.

If you decide that you need all 7 bits (USE_3_BIT_QUOTIENT is 0),
consider checking the host exception flags after FISTP (search for
"TODO:Quotient". The result may be too large to fit into a dword.
*/
/*
gb-- I only tested the following configurations:
		USE_3_BIT_QUOTIENT			1 -- still changes to apply if no 3-bit quotient
		FPU_DEBUG					1 or 0
		USE_CONSISTENCY_CHECKING	0
		I3_ON_ILLEGAL_FPU_OP		0 -- and this won't change
		I3_ON_FTRAPCC				0 -- and this won't change
*/
#define USE_3_BIT_QUOTIENT			1

//#define FPU_DEBUG					0 -- now defined in "fpu/fpu.h"
#define USE_CONSISTENCY_CHECKING	0

#define I3_ON_ILLEGAL_FPU_OP		0
#define I3_ON_FTRAPCC				0

/* ---------------------------- Debugging ---------------------------- */

PUBLIC void FFPU fpu_dump_registers(void)
{
	for (int i = 0; i < 8; i++){
		printf ("FP%d: %g ", i, fpu_get_register(i));
		if ((i & 3) == 3)
			printf ("\n");
	}
}

PUBLIC void FFPU fpu_dump_flags(void)
{
	printf ("N=%d Z=%d I=%d NAN=%d\n",
		(get_fpsr() & FPSR_CCB_NEGATIVE) != 0,
		(get_fpsr() & FPSR_CCB_ZERO)!= 0,
		(get_fpsr() & FPSR_CCB_INFINITY) != 0,
		(get_fpsr() & FPSR_CCB_NAN) != 0);
}

#include "debug.h"

#if FPU_DEBUG
#undef __inline__
#define __inline__

PRIVATE void FFPU dump_first_bytes_buf(char *b, uae_u8* buf, uae_s32 actual)
{
	char bb[10];
	int32 i, bytes = min(actual,100);

	*b = 0;
	for (i=0; i<bytes; i++) {
		sprintf( bb, "%02x ", (uint32)buf[i] );
		strcat( b, bb );
	}
	strcat((char*)b,"\r\n");
}

PUBLIC void FFPU dump_first_bytes(uae_u8 * buf, uae_s32 actual)
{
	char msg[256];
	dump_first_bytes_buf( msg, buf, actual );
	D(bug("%s\n", msg));
}

PRIVATE char * FFPU etos(fpu_register const & e)
{
	static char _s[10][30];
	static int _ix = 0;
	float f;

/*	_asm {
		MOV			EDI, [e]
		FLD     TBYTE PTR [EDI]
    FSTP    DWORD PTR f
	} */
	
	__asm__ __volatile__(
			"fldt	%1\n"
			"fstp	%0\n"
		:	"=m" (f)
		:	"m" (e)
		);
	
	if(++_ix >= 10) _ix = 0;

	sprintf( _s[_ix], "%.04f", (float)f );
	return( _s[_ix] );
}

PUBLIC void FFPU dump_registers(const char *s)
{
	char b[512];

	sprintf(
		b,
		"%s: %s, %s, %s, %s, %s, %s, %s, %s\r\n",
		s,
		etos(FPU registers[0]),
		etos(FPU registers[1]),
		etos(FPU registers[2]),
		etos(FPU registers[3]),
		etos(FPU registers[4]),
		etos(FPU registers[5]),
		etos(FPU registers[6]),
		etos(FPU registers[7])
	);
	D(bug((char*)b));
}

#else

PUBLIC void FFPU dump_registers(const char *)
{
}

PUBLIC void FFPU dump_first_bytes(uae_u8 *, uae_s32)
{
}

#endif


/* ---------------------------- FPU consistency ---------------------------- */

#if USE_CONSISTENCY_CHECKING
PRIVATE void FFPU FPU_CONSISTENCY_CHECK_START(void)
{
/*	_asm {
	  FNSTSW checked_sw_atstart
	} */
	__asm__ __volatile__("fnstsw %0" : "=m" (checked_sw_atstart));
}

PRIVATE void FFPU FPU_CONSISTENCY_CHECK_STOP(const char *name)
{
	uae_u16 checked_sw_atend;
//	_asm FNSTSW checked_sw_atend
	__asm__ __volatile__("fnstsw %0" : "=m" (checked_sw_attend));
	char msg[256];

	// Check for FPU stack overflows/underflows.
	if( (checked_sw_atend & 0x3800) != (checked_sw_atstart & 0x3800) ) {
		wsprintf(
			msg, 
			"FPU stack leak at %s, %X, %X\r\n", 
			name,
			(int)(checked_sw_atstart & 0x3800) >> 11,
			(int)(checked_sw_atend & 0x3800) >> 11
		);
		OutputDebugString(msg);
	}

	// Observe status mapping.
	/*
	if(checked_sw_atstart != 0x400 || checked_sw_atend != 0x400) {
		wsprintf(
			msg, "Op %s, x86_status_word before=%X, x86_status_word after=%X\r\n", 
			name, (int)checked_sw_atstart, (int)checked_sw_atend
		);
		OutputDebugString(msg);
	}
	*/
}
#else
PRIVATE void FFPU FPU_CONSISTENCY_CHECK_START(void)
{
}

PRIVATE void FFPU FPU_CONSISTENCY_CHECK_STOP(const char *)
{
}
#endif


/* ---------------------------- Status byte ---------------------------- */

// Map x86 FXAM codes -> m68k fpu status byte
#define SW_Z_I_NAN_MASK		(SW_C0|SW_C2|SW_C3)
#define SW_Z				(SW_C3)
#define SW_I				(SW_C0|SW_C2)
#define SW_NAN				(SW_C0)
#define SW_FINITE			(SW_C2)
#define SW_EMPTY_REGISTER	(SW_C0|SW_C3)
#define SW_DENORMAL			(SW_C2|SW_C3)
#define SW_UNSUPPORTED		(0)
#define SW_N				(SW_C1)

// Initial state after boot, reset and frestore(null frame)
#define SW_INITIAL			SW_FINITE


/* ---------------------------- Status functions ---------------------------- */

PRIVATE void __inline__ FFPU SET_BSUN_ON_NAN ()
{
	if( (x86_status_word & (SW_Z_I_NAN_MASK)) == SW_NAN ) {
		x86_status_word |= SW_FAKE_BSUN;
		x86_status_word_accrued |= SW_IE;
	}
}

PRIVATE void __inline__ FFPU build_ex_status ()
{
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word_accrued |= x86_status_word;
	}
}

// TODO_BIGENDIAN; all of these.
/* ---------------------------- Type functions ---------------------------- */

/*
When the FPU creates a NAN, the NAN always contains the same bit pattern
in the mantissa. All bits of the mantissa are ones for any precision.
When the user creates a NAN, any nonzero bit pattern can be stored in the mantissa.
*/
PRIVATE __inline__ void FFPU MAKE_NAN (fpu_register & f)
{
	// Make it non-signaling.
	uae_u8 * p = (uae_u8 *) &f;
	memset( p, 0xFF, sizeof(fpu_register) - 1 );
	p[9] = 0x7F;
}

/*
For single- and double-precision infinities the fraction is a zero.
For extended-precision infinities, the mantissa’s MSB, the explicit
integer bit, can be either one or zero.
*/
PRIVATE __inline__ uae_u32 FFPU IS_INFINITY (fpu_register const & f)
{
	uae_u8 * p = (uae_u8 *) &f;
	if( ((p[9] & 0x7F) == 0x7F) && p[8] == 0xFF ) {
		if ((*((uae_u32 *)&p[0]) == 0) &&
			((*((uae_u32 *)&p[4]) & 0x7FFFFFFF) == 0))
			return(1);
	}
	return(0);
}

PRIVATE __inline__ uae_u32 FFPU IS_NAN (fpu_register const & f)
{
	uae_u8 * p = (uae_u8 *) &f;
	if( ((p[9] & 0x7F) == 0x7F) && p[8] == 0xFF ) {
		if ((*((uae_u32 *)&p[0]) == 0) &&
			((*((uae_u32 *)&p[4]) & 0x7FFFFFFF) != 0))
			return(1);
	}
	return(0);
}

PRIVATE __inline__ uae_u32 FFPU IS_ZERO (fpu_register const & f)
{
	uae_u8 * p = (uae_u8 *) &f;
	return *((uae_u32 *)p) == 0 &&
				 *((uae_u32 *)&p[4]) == 0 &&
				 ( *((uae_u16 *)&p[8]) & 0x7FFF ) == 0;
}

PRIVATE __inline__ void FFPU MAKE_INF_POSITIVE (fpu_register & f)
{
	uae_u8 * p = (uae_u8 *) &f;
	memset( p, 0, sizeof(fpu_register)-2 );
	*((uae_u16 *)&p[8]) = 0x7FFF;
}

PRIVATE __inline__ void FFPU MAKE_INF_NEGATIVE (fpu_register & f)
{
	uae_u8 * p = (uae_u8 *) &f;
	memset( p, 0, sizeof(fpu_register)-2 );
	*((uae_u16 *)&p[8]) = 0xFFFF;
}

PRIVATE __inline__ void FFPU MAKE_ZERO_POSITIVE (fpu_register & f)
{
	uae_u32 * const p = (uae_u32 *) &f;
	memset( p, 0, sizeof(fpu_register) );
}

PRIVATE __inline__ void FFPU MAKE_ZERO_NEGATIVE (fpu_register & f)
{
	uae_u32 * const p = (uae_u32 *) &f;
	memset( p, 0, sizeof(fpu_register) );
	*((uae_u32 *)&p[4]) = 0x80000000;
}

PRIVATE __inline__ uae_u32 FFPU IS_NEGATIVE (fpu_register const & f)
{
	uae_u8 * p = (uae_u8 *) &f;
	return( (p[9] & 0x80) != 0 );
}


/* ---------------------------- Conversions ---------------------------- */

PRIVATE void FFPU signed_to_extended ( uae_s32 x, fpu_register & f )
{
	FPU_CONSISTENCY_CHECK_START();
	
/*	_asm {
		MOV			ESI, [f]
    FILD    DWORD PTR [x]
		FSTP    TBYTE PTR [ESI]
	} */
	
	__asm__ __volatile__("fildl %1\n\tfstpt %0" : "=m" (f) : "m" (x));
	D(bug("signed_to_extended (%X) = %s\r\n",(int)x,etos(f)));
	FPU_CONSISTENCY_CHECK_STOP("signed_to_extended");
}

PRIVATE uae_s32 FFPU extended_to_signed_32 ( fpu_register const & f )
{
	FPU_CONSISTENCY_CHECK_START();
	volatile uae_s32 tmp;
	volatile WORD sw_temp;
	
/*	_asm {
		MOV			EDI, [f]
		FLD     TBYTE PTR [EDI]
    FISTP   DWORD PTR tmp
    FNSTSW  sw_temp
	} */

	__asm__ __volatile__(
			"fldt	%2\n"
			"fistpl	%0\n"
			"fnstsw	%1\n"
		:	"=m" (tmp), "=m" (sw_temp)
		:	"m" (f)
		);
	
	if(sw_temp & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		if(sw_temp & (SW_OE|SW_UE|SW_DE|SW_IE)) { // Map SW_OE to OPERR.
			x86_status_word |= SW_IE;
			x86_status_word_accrued |= SW_IE;
			// Setting the value to zero might not be the right way to go,
			// but I'll leave it like this for now.
			tmp = 0;
		}
		if(sw_temp & SW_PE) {
			x86_status_word |= SW_PE;
			x86_status_word_accrued |= SW_PE;
		}
	}

	D(bug("extended_to_signed_32 (%s) = %X\r\n",etos(f),(int)tmp));
	FPU_CONSISTENCY_CHECK_STOP("extended_to_signed_32");
	return tmp;
}

PRIVATE uae_s16 FFPU extended_to_signed_16 ( fpu_register const & f )
{
	FPU_CONSISTENCY_CHECK_START();
	volatile uae_s16 tmp;
	volatile WORD sw_temp;

/*	_asm {
		MOV			EDI, [f]
		FLD     TBYTE PTR [EDI]
    FISTP   WORD PTR tmp
    FNSTSW  sw_temp
	} */
	
	__asm__ __volatile__(
			"fldt	%2\n"
			"fistp	%0\n"
			"fnstsw	%1\n"
		:	"=m" (tmp), "=m" (sw_temp)
		:	"m" (f)
		);

	if(sw_temp & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		if(sw_temp & (SW_OE|SW_UE|SW_DE|SW_IE)) { // Map SW_OE to OPERR.
			x86_status_word |= SW_IE;
			x86_status_word_accrued |= SW_IE;
			tmp = 0;
		}
		if(sw_temp & SW_PE) {
			x86_status_word |= SW_PE;
			x86_status_word_accrued |= SW_PE;
		}
	}

	D(bug("extended_to_signed_16 (%s) = %X\r\n",etos(f),(int)tmp));
	FPU_CONSISTENCY_CHECK_STOP("extended_to_signed_16");
	return tmp;
}

PRIVATE uae_s8 FFPU extended_to_signed_8 ( fpu_register const & f )
{
	FPU_CONSISTENCY_CHECK_START();
	volatile uae_s16 tmp;
	volatile WORD sw_temp;
	
/*	_asm {
		MOV			EDI, [f]
		FLD     TBYTE PTR [EDI]
    FISTP   WORD PTR tmp
    FNSTSW  sw_temp
	} */
	
	__asm__ __volatile__(
			"fldt	%2\n"
			"fistp	%0\n"
			"fnstsw	%1\n"
		:	"=m" (tmp), "=m" (sw_temp)
		:	"m" (f)
		);

	if(sw_temp & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		if(sw_temp & (SW_OE|SW_UE|SW_DE|SW_IE)) { // Map SW_OE to OPERR.
			x86_status_word |= SW_IE;
			x86_status_word_accrued |= SW_IE;
			tmp = 0;
		}
		if(sw_temp & SW_PE) {
			x86_status_word |= SW_PE;
			x86_status_word_accrued |= SW_PE;
		}
	}

	if(tmp > 127 || tmp < -128) { // OPERR
		x86_status_word |= SW_IE;
		x86_status_word_accrued |= SW_IE;
	}

	D(bug("extended_to_signed_8 (%s) = %X\r\n",etos(f),(int)tmp));
	FPU_CONSISTENCY_CHECK_STOP("extended_to_signed_8");
	return (uae_s8)tmp;
}

PRIVATE void FFPU double_to_extended ( double x, fpu_register & f )
{
	FPU_CONSISTENCY_CHECK_START();

/*	_asm {
		MOV			EDI, [f]
    FLD     QWORD PTR [x]
		FSTP    TBYTE PTR [EDI]
	} */
	
	__asm__ __volatile__(
			"fldl	%1\n"
			"fstpt	%0\n"
		:	"=m" (f)
		:	"m" (x)
		);
	
	FPU_CONSISTENCY_CHECK_STOP("double_to_extended");
}

PRIVATE fpu_double FFPU extended_to_double( fpu_register const & f )
{
	FPU_CONSISTENCY_CHECK_START();
	double result;

/*	_asm {
		MOV			ESI, [f]
		FLD     TBYTE PTR [ESI]
    FSTP    QWORD PTR result
	} */
	
	__asm__ __volatile__(
			"fldt	%1\n"
			"fstpl	%0\n"
		:	"=m" (result)
		:	"m" (f)
		);
	
	FPU_CONSISTENCY_CHECK_STOP("extended_to_double");
	return result;
}

PRIVATE void FFPU to_single ( uae_u32 src, fpu_register & f )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [f]
    FLD     DWORD PTR src
		FSTP    TBYTE PTR [ESI]
	} */
	
	__asm__ __volatile__(
			"flds	%1\n"
			"fstpt	%0\n"
		:	"=m" (f)
		:	"m" (src)
		);

	D(bug("to_single (%X) = %s\r\n",src,etos(f)));
	FPU_CONSISTENCY_CHECK_STOP("to_single");
}

// TODO_BIGENDIAN
PRIVATE void FFPU to_exten_no_normalize ( uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3, fpu_register & f )
{
	FPU_CONSISTENCY_CHECK_START();
	uae_u32 *p = (uae_u32 *)&f;

	uae_u32 sign =  (wrd1 & 0x80000000) >> 16;
	uae_u32 exp  = (wrd1 >> 16) & 0x7fff;
	p[0] = wrd3;
	p[1] = wrd2;
	*((uae_u16 *)&p[2]) = (uae_u16)(sign | exp);

	D(bug("to_exten_no_normalize (%X,%X,%X) = %s\r\n",wrd1,wrd2,wrd3,etos(f)));
	FPU_CONSISTENCY_CHECK_STOP("to_exten_no_normalize");
}

PRIVATE void FFPU to_exten ( uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3, fpu_register & f )
{
	FPU_CONSISTENCY_CHECK_START();
	uae_u32 *p = (uae_u32 *)&f;

	uae_u32 sign =  (wrd1 & 0x80000000) >> 16;
	uae_u32 exp  = (wrd1 >> 16) & 0x7fff;

	// The explicit integer bit is not set, must normalize.
	// Don't do it for zeroes, infinities or nans.
	if( (wrd2 & 0x80000000) == 0 && exp != 0 && exp != 0x7FFF ) {
		D(bug("to_exten denormalized mantissa (%X,%X,%X)\r\n",wrd1,wrd2,wrd3));
		if( wrd2 | wrd3 ) {
			// mantissa, not fraction.
			uae_u64 man = ((uae_u64)wrd2 << 32) | wrd3;
			while( exp > 0 && (man & UVAL64(0x8000000000000000)) == 0 ) {
				man <<= 1;
				exp--;
			}
			wrd2 = (uae_u32)( man >> 32 );
			wrd3 = (uae_u32)( man & 0xFFFFFFFF );
			if( exp == 0 || (wrd2 & 0x80000000) == 0 ) {
				// underflow
				wrd2 = wrd3 = exp = 0;
				sign = 0;
			}
		} else {
			if(exp != 0x7FFF && exp != 0) {
				// Make a non-signaling nan.
				exp = 0x7FFF;
				sign = 0;
				wrd2 = 0x80000000;
			}
		}
	}

	p[0] = wrd3;
	p[1] = wrd2;
	*((uae_u16 *)&p[2]) = (uae_u16)(sign | exp);

	D(bug("to_exten (%X,%X,%X) = %s\r\n",wrd1,wrd2,wrd3,etos(f)));
	FPU_CONSISTENCY_CHECK_STOP("to_exten");
}

PRIVATE void FFPU to_double ( uae_u32 wrd1, uae_u32 wrd2, fpu_register & f )
{
	FPU_CONSISTENCY_CHECK_START();
	
	// gb-- make GCC happy
	union {
		uae_u64 q;
		uae_u32 l[2];
	} src;
	
	// Should renormalize if needed. I'm not sure that x86 and m68k FPU's
	// do it the sama way. This should be extremely rare however.
	// to_exten() is often called with denormalized values.

	src.l[0] = wrd2;
	src.l[1] = wrd1;

/*	_asm {
    FLD     QWORD PTR src
		MOV			EDI, [f]
		FSTP    TBYTE PTR [EDI]
	} */
	
	__asm__ __volatile__(
			"fldl	%1\n"
			"fstpt	%0\n"
		:	"=m" (f)
		:	"m" (src.q)
		);

	D(bug("to_double (%X,%X) = %s\r\n",wrd1,wrd2,etos(f)));
	FPU_CONSISTENCY_CHECK_STOP("to_double");
}

PRIVATE uae_u32 FFPU from_single ( fpu_register const & f )
{
	FPU_CONSISTENCY_CHECK_START();
	volatile uae_u32 dest;
	volatile WORD sw_temp;

/*	_asm {
		MOV			EDI, [f]
    FLD     TBYTE PTR [EDI]
		FSTP    DWORD PTR dest
    FNSTSW  sw_temp
	} */
	
	__asm__ __volatile__(
			"fldt	%2\n"
			"fstps	%0\n"
			"fnstsw	%1\n"
		:	"=m" (dest), "=m" (sw_temp)
		:	"m" (f)
		);

	sw_temp &= SW_EXCEPTION_MASK;
	if(sw_temp) {
//		_asm FNCLEX
		asm("fnclex");
		x86_status_word = (x86_status_word & ~SW_EXCEPTION_MASK) | sw_temp;
		x86_status_word_accrued |= sw_temp;
	}

	D(bug("from_single (%s) = %X\r\n",etos(f),dest));
	FPU_CONSISTENCY_CHECK_STOP("from_single");
	return dest;
}

// TODO_BIGENDIAN
PRIVATE void FFPU from_exten ( fpu_register const & f, uae_u32 *wrd1, uae_u32 *wrd2, uae_u32 *wrd3 )
{
	FPU_CONSISTENCY_CHECK_START();
	uae_u32 *p = (uae_u32 *)&f;
	*wrd3 = p[0];
	*wrd2 = p[1];
	*wrd1 = ( (uae_u32)*((uae_u16 *)&p[2]) ) << 16;

	D(bug("from_exten (%s) = %X,%X,%X\r\n",etos(f),*wrd1,*wrd2,*wrd3));
	FPU_CONSISTENCY_CHECK_STOP("from_exten");
}

PRIVATE void FFPU from_double ( fpu_register const & f, uae_u32 *wrd1, uae_u32 *wrd2 )
{
	FPU_CONSISTENCY_CHECK_START();
	volatile uae_u32 dest[2];
	volatile WORD sw_temp;

/*	_asm {
		MOV			EDI, [f]
    FLD     TBYTE PTR [EDI]
		FSTP    QWORD PTR dest
    FNSTSW  sw_temp
	} */
	
	__asm__ __volatile__(
			"fldt	%2\n"
			"fstpl	%0\n"
			"fnstsw	%1\n"
		:	"=m" (dest), "=m" (sw_temp)
		:	"m" (f)
		);

	sw_temp &= SW_EXCEPTION_MASK;
	if(sw_temp) {
//		_asm FNCLEX
		asm("fnclex");
		x86_status_word = (x86_status_word & ~SW_EXCEPTION_MASK) | sw_temp;
		x86_status_word_accrued |= sw_temp;
	}

	// TODO: There is a partial memory stall, nothing happens until FSTP retires.
	// On PIII, could use MMX move w/o any penalty.
	*wrd2 = dest[0];
	*wrd1 = dest[1];

	D(bug("from_double (%s) = %X,%X\r\n",etos(f),dest[1],dest[0]));
	FPU_CONSISTENCY_CHECK_STOP("from_double");
}

PRIVATE void FFPU do_fmove ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
		FXAM
    FNSTSW  x86_status_word
		FSTP    TBYTE PTR [EDI]
	} */
	
	__asm__ __volatile__(
			"fldt	%2\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (x86_status_word), "=m" (dest)
		:	"m" (src)
		);
	FPU_CONSISTENCY_CHECK_STOP("do_fmove");
}

/*
PRIVATE void FFPU do_fmove_no_status ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
		FSTP    TBYTE PTR [EDI]
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fmove_no_status");
}
*/


/* ---------------------------- Operations ---------------------------- */

PRIVATE void FFPU do_fint ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
		FRNDINT
		FXAM
    FNSTSW  x86_status_word
		FSTP    TBYTE PTR [EDI]
	} */
	__asm__ __volatile__(
			"fldt	%2\n"
			"frndint\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (x86_status_word), "=m" (dest)
		:	"m" (src)
		);
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word &= ~(SW_EXCEPTION_MASK - SW_PE);
		x86_status_word_accrued |= x86_status_word;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fint");
}

PRIVATE void FFPU do_fintrz ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
	WORD cw_temp;

/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
		FSTCW   cw_temp
		and			cw_temp, ~X86_ROUNDING_MODE
		or			cw_temp, CW_RC_ZERO
    FLDCW   cw_temp
    FLD     TBYTE PTR [ESI]
		FRNDINT
		FXAM
    FNSTSW  x86_status_word
    FLDCW   x86_control_word
		FSTP    TBYTE PTR [EDI]
	} */
	
	__asm__ __volatile__(
			"fstcw	%0\n"
			"andl	$(~X86_ROUNDING_MODE), %0\n"
			"orl	$CW_RC_ZERO, %0\n"
			"fldcw	%0\n"
			"fldt	%3\n"
			"frndint\n"
			"fxam	\n"
			"fnstsw	%1\n"
			"fldcw	%4\n"
			"fstpt	%2\n"
		:	"+m" (cw_temp), "=m" (x86_status_word), "=m" (dest)
		:	"m" (src), "m" (x86_control_word)
		);
	
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word &= ~(SW_EXCEPTION_MASK - SW_PE);
		x86_status_word_accrued |= x86_status_word;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fintrz");
}

PRIVATE void FFPU do_fsqrt ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
		FSQRT
		FXAM
    FNSTSW  x86_status_word
		FSTP    TBYTE PTR [EDI]
	} */
	
	__asm__ __volatile__(
			"fldt	%2\n"
			"fsqrt	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (x86_status_word), "=m" (dest)
		:	"m" (src)
		);
	
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word &= ~(SW_EXCEPTION_MASK - SW_IE - SW_PE);
		x86_status_word_accrued |= x86_status_word;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fsqrt");
}

PRIVATE void FFPU do_ftst ( fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
    FLD     TBYTE PTR [ESI]
		FXAM
    FNSTSW  x86_status_word
		FSTP    ST(0)
	} */
	
	__asm__ __volatile__(
			"fldt	%1\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstp	%%st(0)\n"
		:	"=m" (x86_status_word)
		:	"m" (src)
		);
	
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word &= ~SW_EXCEPTION_MASK;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_ftst");
}

// These functions are calculated in 53 bits accuracy only.
// Exception checking is not complete.
PRIVATE void FFPU do_fsinh ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = sinh(x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_fsinh");
}

PRIVATE void FFPU do_flognp1 ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = log (x + 1.0);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_flognp1");
}

PRIVATE void FFPU do_fetoxm1 ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = exp (x) - 1.0;
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_fetoxm1");
}

PRIVATE void FFPU do_ftanh ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = tanh (x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_ftanh");
}

PRIVATE void FFPU do_fatan ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = atan (x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_fatan");
}

PRIVATE void FFPU do_fasin ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = asin (x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_fasin");
}

PRIVATE void FFPU do_fatanh ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = log ((1 + x) / (1 - x)) / 2;
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_fatanh");
}

PRIVATE void FFPU do_fetox ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = exp (x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_fetox");
}

PRIVATE void FFPU do_ftwotox ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = pow(2.0, x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_ftwotox");
}

PRIVATE void FFPU do_ftentox ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = pow(10.0, x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_ftentox");
}

PRIVATE void FFPU do_flogn ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = log (x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_flogn");
}

PRIVATE void FFPU do_flog10 ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = log10 (x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_flog10");
}

PRIVATE void FFPU do_flog2 ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = log (x) / log (2.0);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_flog2");
}

PRIVATE void FFPU do_facos ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = acos(x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_facos");
}

PRIVATE void FFPU do_fcosh ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = cosh(x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_fcosh");
}

PRIVATE void FFPU do_fsin ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
		FSIN
		FXAM
    FNSTSW  x86_status_word
		FSTP    TBYTE PTR [EDI]
	} */
	__asm__ __volatile__(
			"fldt	%2\n"
			"fsin	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (x86_status_word), "=m" (dest)
		:	"m" (src)
		);
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word &= ~(SW_EXCEPTION_MASK - SW_IE - SW_PE);
		x86_status_word_accrued |= x86_status_word;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fsin");
}

// TODO: Should check for out-of-range condition (partial tangent)
PRIVATE void FFPU do_ftan ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
		FPTAN
		FSTP    ST(0)	; pop 1.0 (the 8087/287 compatibility thing)
		FXAM
    FNSTSW  x86_status_word
		FSTP    TBYTE PTR [EDI]
	} */
	__asm__ __volatile__(
			"fldt	%2\n"
			"fptan	\n"
			"fstp	%%st(0)\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (x86_status_word), "=m" (dest)
		:	"m" (src)
		);
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word &= ~(SW_EXCEPTION_MASK - SW_IE - SW_PE - SW_UE);
		x86_status_word_accrued |= x86_status_word;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_ftan");
}

PRIVATE void FFPU do_fabs ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
		FABS
		FXAM
    FNSTSW  x86_status_word
		FSTP    TBYTE PTR [EDI]
	} */
	__asm__ __volatile__(
			"fldt	%2\n"
			"fabs	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (x86_status_word), "=m" (dest)
		:	"m" (src)
		);
	// x86 fabs should not rise any exceptions (except stack underflow)
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word &= ~SW_EXCEPTION_MASK;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fabs");
}

PRIVATE void FFPU do_fneg ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
		FCHS
		FXAM
    FNSTSW  x86_status_word
		FSTP    TBYTE PTR [EDI]
	} */
	__asm__ __volatile__(
			"fldt	%2\n"
			"fchs	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (x86_status_word), "=m" (dest)
		:	"m" (src)
		);
	// x86 fchs should not rise any exceptions (except stack underflow)
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word &= ~SW_EXCEPTION_MASK;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fneg");
}

PRIVATE void FFPU do_fcos ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
		FCOS
		FXAM
    FNSTSW  x86_status_word
		FSTP    TBYTE PTR [EDI]
	} */
	__asm__ __volatile__(
			"fldt	%2\n"
			"fcos	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (x86_status_word), "=m" (dest)
		:	"m" (src)
		);
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word &= ~(SW_EXCEPTION_MASK - SW_IE - SW_PE);
		x86_status_word_accrued |= x86_status_word;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fcos");
}

PRIVATE void FFPU do_fgetexp ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
		FXTRACT
		FSTP    ST(0)						; pop mantissa
		FXAM
    FNSTSW  x86_status_word
		FSTP    TBYTE PTR [EDI]
	} */
	__asm__ __volatile__(
			"fldt	%2\n"
			"fxtract\n"
			"fstp	%%st(0)\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (x86_status_word), "=m" (dest)
		:	"m" (src)
		);
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word &= ~SW_EXCEPTION_MASK;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fgetexp");
}

PRIVATE void FFPU do_fgetman ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
		FXTRACT
		FXAM
    FNSTSW  x86_status_word
		FSTP    TBYTE PTR [EDI]
		FSTP    ST(0)						; pop exponent
	} */
	__asm__ __volatile__(
			"fldt	%2\n"
			"fxtract\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
			"fstp	%%st(0)\n"
		:	"=m" (x86_status_word), "=m" (dest)
		:	"m" (src)
		);
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word &= ~SW_EXCEPTION_MASK;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fgetman");
}

PRIVATE void FFPU do_fdiv ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
    FLD     TBYTE PTR [EDI]
		FDIV		ST(0),ST(1)
		FXAM
    FNSTSW  x86_status_word
		FSTP    TBYTE PTR [EDI]
		FSTP    ST(0)
	} */
	__asm__ __volatile__(
			"fldt	%2\n"
			"fldt	%1\n"
			"fdiv	%%st(1), %%st(0)\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
			"fstp	%%st(0)\n"
		:	"=m" (x86_status_word), "+m" (dest)
		:	"m" (src)
		);
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word_accrued |= x86_status_word;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fdiv");
}

// The sign of the quotient is the exclusive-OR of the sign bits
// of the source and destination operands.
// Quotient Byte is loaded with the sign and least significant
// seven bits of the quotient.

PRIVATE void FFPU do_fmod ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();

	volatile uint16 status;
	uae_u32 quot;
#if !USE_3_BIT_QUOTIENT
	WORD cw_temp;
#endif
	
	uae_u8 * dest_p = (uae_u8 *)&dest;
	uae_u8 * src_p = (uae_u8 *)&src;
	uae_u32 sign = (dest_p[9] ^ src_p[9]) & 0x80;

/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]

#if !USE_3_BIT_QUOTIENT
		MOV			CX, x86_control_word
		AND			CX, ~X86_ROUNDING_MODE
		OR			CX, CW_RC_ZERO
		MOV			cw_temp, CX
    FLDCW   cw_temp

		FLD     TBYTE PTR [ESI]
		FLD     TBYTE PTR [EDI]
		FDIV		ST(0),ST(1)
		FABS
    FISTP   DWORD PTR quot
		FSTP    ST(0)
    FLDCW   x86_control_word
		// TODO:Quotient
		// Should clear any possible exceptions here
#endif

		FLD     TBYTE PTR [ESI]
		FLD     TBYTE PTR [EDI]

// loop until the remainder is not partial any more.
partial_loop:
		FPREM
		FNSTSW	status
		TEST		status, SW_C2
		JNE			partial_loop


		FXAM
    FNSTSW  x86_status_word

		FSTP    TBYTE PTR [EDI]
		FSTP    ST(0)
	} */
	
#if !USE_3_BIT_QUOTIENT
	
	__asm__ __volatile__(
			"movl	%6, %%ecx\n"	// %6: x86_control_word		(read)
			"andl	$(~X86_ROUNDING_MODE), %%ecx\n"
			"orl	$CW_RC_ZERO, %%ecx\n"
			"movl	%%ecx, %0\n"	// %0: cw_temp	(read/write)
			"fldcw	%0\n"
			"fldt	%5\n"
			"fldt	%4\n"
			"fdiv	%%st(1), %%st(0)\n"
			"fabs	\n"
			"fistpl	%1\n"			// %1: quot		(read/write)
			"fstp	%%st(0)\n"
			"fldcw	%6\n"
			"fldt	%5\n"
			"fldt	%4\n"
			"0:\n"					// partial_loop
			"fprem	\n"
			"fnstsw	%2\n"			// %2: status	(read/write)
			"testl	$SW_C2, %2\n"
			"jne	0b\n"
			"fxam	\n"
			"fnstsw	%3\n"			// %3: x86_status_word		(write)
			"fstpt	%4\n"
			"fstp	%%st(0)\n"
		:	"+m" (cw_temp), "+m" (quot), "+m" (status), "=m" (x86_status_word), "+m" (dest)
		:	"m" (src), "m" (x86_control_word)
		:	"ecx"
		);
	
#else
	
	__asm__ __volatile__(
			"fldt	%3\n"
			"fldt	%2\n"
			"0:\n"					// partial_loop
			"fprem	\n"
			"fnstsw	%0\n"			// %0: status	(read/write)
			"testl	$SW_C2, %0\n"
			"jne	0b\n"
			"fxam	\n"
			"fnstsw	%1\n"			// %1: x86_status_word		(write)
			"fstpt	%2\n"
			"fstp	%%st(0)\n"
		:	"+m" (status), "=m" (x86_status_word), "+m" (dest)
		:	"m" (src)
		);
	
#endif
	
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word &= ~(SW_EXCEPTION_MASK - SW_IE - SW_UE);
		x86_status_word_accrued |= x86_status_word;
	}

#if USE_3_BIT_QUOTIENT
	// SW_C1 Set to least significant bit of quotient (Q0).
	// SW_C3 Set to bit 1 (Q1) of the quotient.
	// SW_C0 Set to bit 2 (Q2) of the quotient.
	quot = ((status & SW_C0) >> 6) | ((status & SW_C3) >> 13) | ((status & SW_C1) >> 9);
	FPU fpsr.quotient = (sign | quot) << 16;
#else
	FPU fpsr.quotient = (sign | (quot&0x7F)) << 16;
#endif

	FPU_CONSISTENCY_CHECK_STOP("do_fmod");
}

PRIVATE void FFPU do_frem ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();

	volatile uint16 status;
	uae_u32 quot;
#if !USE_3_BIT_QUOTIENT
	WORD cw_temp;
#endif

	uae_u8 * dest_p = (uae_u8 *)&dest;
	uae_u8 * src_p = (uae_u8 *)&src;
	uae_u32 sign = (dest_p[9] ^ src_p[9]) & 0x80;

/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]

#if !USE_3_BIT_QUOTIENT
		MOV			CX, x86_control_word
		AND			CX, ~X86_ROUNDING_MODE
		OR			CX, CW_RC_NEAR
		MOV			cw_temp, CX
    FLDCW   cw_temp

		FLD     TBYTE PTR [ESI]
		FLD     TBYTE PTR [EDI]
		FDIV		ST(0),ST(1)
		FABS
    FISTP   DWORD PTR quot
		FSTP    ST(0)
    FLDCW   x86_control_word
		// TODO:Quotient
		// Should clear any possible exceptions here
#endif

		FLD     TBYTE PTR [ESI]
		FLD     TBYTE PTR [EDI]

// loop until the remainder is not partial any more.
partial_loop:
		FPREM1
		FNSTSW	status
		TEST		status, SW_C2
		JNE			partial_loop

		FXAM
    FNSTSW  x86_status_word
		FSTP    TBYTE PTR [EDI]
		FSTP    ST(0)
	} */

#if !USE_3_BIT_QUOTIENT
	
	__asm__ __volatile__(
			"movl	%6, %%ecx\n"	// %6: x86_control_word		(read)
			"andl	$(~X86_ROUNDING_MODE), %%ecx\n"
			"orl	$CW_RC_NEAR, %%ecx\n"
			"movl	%%ecx, %0\n"	// %0: cw_temp	(read/write)
			"fldcw	%0\n"
			"fldt	%5\n"
			"fldt	%4\n"
			"fdiv	%%st(1), %%st(0)\n"
			"fabs	\n"
			"fistpl	%1\n"			// %1: quot		(read/write)
			"fstp	%%st(0)\n"
			"fldcw	%6\n"
			"fldt	%5\n"
			"fldt	%4\n"
			"0:\n"					// partial_loop
			"fprem1	\n"
			"fnstsw	%2\n"			// %2: status	(read/write)
			"testl	$SW_C2, %2\n"
			"jne	0b\n"
			"fxam	\n"
			"fnstsw	%3\n"			// %3: x86_status_word		(write)
			"fstpt	%4\n"
			"fstp	%%st(0)\n"
		:	"+m" (cw_temp), "+m" (quot), "+m" (status), "=m" (x86_status_word), "+m" (dest)
		:	"m" (src), "m" (x86_control_word)
		:	"ecx"
		);
	
#else
	
	__asm__ __volatile__(
			"fldt	%3\n"
			"fldt	%2\n"
			"0:\n"					// partial_loop
			"fprem1	\n"
			"fnstsw	%0\n"			// %0: status	(read/write)
			"testl	$SW_C2, %0\n"
			"jne	0b\n"
			"fxam	\n"
			"fnstsw	%1\n"			// %1: x86_status_word		(write)
			"fstpt	%2\n"
			"fstp	%%st(0)\n"
		:	"+m" (status), "=m" (x86_status_word), "+m" (dest)
		:	"m" (src)
		);
	
#endif
	
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word &= ~(SW_EXCEPTION_MASK - SW_IE - SW_UE);
		x86_status_word_accrued |= x86_status_word;
	}

#if USE_3_BIT_QUOTIENT
	// SW_C1 Set to least significant bit of quotient (Q0).
	// SW_C3 Set to bit 1 (Q1) of the quotient.
	// SW_C0 Set to bit 2 (Q2) of the quotient.
	quot = ((status & SW_C0) >> 6) | ((status & SW_C3) >> 13) | ((status & SW_C1) >> 9);
	FPU fpsr.quotient = (sign | quot) << 16;
#else
	FPU fpsr.quotient = (sign | (quot&0x7F)) << 16;
#endif

	FPU_CONSISTENCY_CHECK_STOP("do_frem");
}

// Faster versions. The current rounding mode is already correct.
#if !USE_3_BIT_QUOTIENT
PRIVATE void FFPU do_fmod_dont_set_cw ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();

	volatile uint16 status;
	uae_u32 quot;

	uae_u8 * dest_p = (uae_u8 *)&dest;
	uae_u8 * src_p = (uae_u8 *)&src;
	uae_u32 sign = (dest_p[9] ^ src_p[9]) & 0x80;

	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]

		FLD     TBYTE PTR [ESI]
		FLD     TBYTE PTR [EDI]
		FDIV		ST(0),ST(1)
		FABS
    FISTP   DWORD PTR quot
		FSTP    ST(0)
		// TODO:Quotient
		// Should clear any possible exceptions here

		FLD     TBYTE PTR [ESI]
		FLD     TBYTE PTR [EDI]

// loop until the remainder is not partial any more.
partial_loop:
		FPREM
		FNSTSW	status
		TEST		status, SW_C2
		JNE			partial_loop

		FXAM
    FNSTSW  x86_status_word

		FSTP    TBYTE PTR [EDI]
		FSTP    ST(0)
	}
	if(x86_status_word & SW_EXCEPTION_MASK) {
		_asm FNCLEX
		x86_status_word &= ~(SW_EXCEPTION_MASK - SW_IE - SW_UE);
		x86_status_word_accrued |= x86_status_word;
	}
	FPU fpsr.quotient = (sign | (quot&0x7F)) << 16;
	FPU_CONSISTENCY_CHECK_STOP("do_fmod_dont_set_cw");
}

PRIVATE void FFPU do_frem_dont_set_cw ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();

	volatile uint16 status;
	uae_u32 quot;

	uae_u8 * dest_p = (uae_u8 *)&dest;
	uae_u8 * src_p = (uae_u8 *)&src;
	uae_u32 sign = (dest_p[9] ^ src_p[9]) & 0x80;

	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]

		FLD     TBYTE PTR [ESI]
		FLD     TBYTE PTR [EDI]
		FDIV		ST(0),ST(1)
		FABS
    FISTP   DWORD PTR quot
		FSTP    ST(0)
		// TODO:Quotient
		// Should clear any possible exceptions here

		FLD     TBYTE PTR [ESI]
		FLD     TBYTE PTR [EDI]

// loop until the remainder is not partial any more.
partial_loop:
		FPREM1
		FNSTSW	status
		TEST		status, SW_C2
		JNE			partial_loop

		FXAM
    FNSTSW  x86_status_word
		FSTP    TBYTE PTR [EDI]
		FSTP    ST(0)
	}
	if(x86_status_word & SW_EXCEPTION_MASK) {
		_asm FNCLEX
		x86_status_word &= ~(SW_EXCEPTION_MASK - SW_IE - SW_UE);
		x86_status_word_accrued |= x86_status_word;
	}
	FPU fpsr.quotient = (sign | (quot&0x7F)) << 16;
	FPU_CONSISTENCY_CHECK_STOP("do_frem_dont_set_cw");
}
#endif //USE_3_BIT_QUOTIENT

PRIVATE void FFPU do_fadd ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
    FLD     TBYTE PTR [EDI]
		FADD
		FXAM
    FNSTSW  x86_status_word
		FSTP    TBYTE PTR [EDI]
	} */
	__asm__ __volatile__(
			"fldt	%2\n"
			"fldt	%1\n"
			"fadd	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (x86_status_word), "+m" (dest)
		:	"m" (src)
		);
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word &= ~(SW_EXCEPTION_MASK - SW_IE - SW_UE - SW_OE - SW_PE);
		x86_status_word_accrued |= x86_status_word;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fadd");
}

PRIVATE void FFPU do_fmul ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
    FLD     TBYTE PTR [EDI]
		FMUL
		FXAM
    FNSTSW  x86_status_word
		FSTP    TBYTE PTR [EDI]
	} */
	__asm__ __volatile__(
			"fldt	%2\n"
			"fldt	%1\n"
			"fmul	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (x86_status_word), "+m" (dest)
		:	"m" (src)
		);
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word_accrued |= x86_status_word;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fmul");
}

PRIVATE void FFPU do_fsgldiv ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
	WORD cw_temp;
/*	_asm {
		FSTCW   cw_temp
		and			cw_temp, ~X86_ROUNDING_PRECISION
		or			cw_temp, PRECISION_CONTROL_SINGLE
    FLDCW   cw_temp

		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
    FLD     TBYTE PTR [EDI]
		FDIV		ST(0),ST(1)
		FXAM
    FNSTSW  x86_status_word
		FSTP    TBYTE PTR [EDI]
		FSTP    ST(0)
    FLDCW   x86_control_word
	} */
	__asm__ __volatile__(
			"fstcw	%0\n"
			"andl	$(~X86_ROUNDING_PRECISION), %0\n"
			"orl	$PRECISION_CONTROL_SINGLE, %0\n"
			"fldcw	%0\n"
			"fldt	%3\n"
			"fldt	%2\n"
			"fdiv	%%st(1), %%st(0)\n"
			"fxam	\n"
			"fnstsw	%1\n"
			"fstpt	%2\n"
			"fstp	%%st(0)\n"
			"fldcw	%4\n"
		:	"+m" (cw_temp), "=m" (x86_status_word), "+m" (dest)
		:	"m" (src), "m" (x86_control_word)
		);
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word_accrued |= x86_status_word;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fsgldiv");
}

PRIVATE void FFPU do_fscale ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
    FLD     TBYTE PTR [EDI]
		FSCALE
		FXAM
    FNSTSW  x86_status_word
		FSTP    TBYTE PTR [EDI]
		FSTP    ST(0)
	} */
	__asm__ __volatile__(
			"fldt	%2\n"
			"fldt	%1\n"
			"fscale	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
			"fstp	%%st(0)\n"
		:	"=m" (x86_status_word), "+m" (dest)
		:	"m" (src)
		);
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word &= ~(SW_EXCEPTION_MASK - SW_UE - SW_OE);
		x86_status_word_accrued |= x86_status_word;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fscale");
}

PRIVATE void FFPU do_fsglmul ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
	WORD cw_temp;

/*	_asm {
		FSTCW   cw_temp
		and			cw_temp, ~X86_ROUNDING_PRECISION
		or			cw_temp, PRECISION_CONTROL_SINGLE
    FLDCW   cw_temp

		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
    FLD     TBYTE PTR [EDI]
		FMUL
		FXAM
    FNSTSW  x86_status_word
		FSTP    TBYTE PTR [EDI]

    FLDCW   x86_control_word
	} */
	__asm__ __volatile__(
			"fstcw	%0\n"
			"andl	$(~X86_ROUNDING_PRECISION), %0\n"
			"orl	$PRECISION_CONTROL_SINGLE, %0\n"
			"fldcw	%0\n"
			"fldt	%3\n"
			"fldt	%2\n"
			"fmul	\n"
			"fxam	\n"
			"fnstsw	%1\n"
			"fstpt	%2\n"
			"fldcw	%4\n"
		:	"+m" (cw_temp), "=m" (x86_status_word), "+m" (dest)
		:	"m" (src), "m" (x86_status_word)
		);
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word_accrued |= x86_status_word;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fsglmul");
}

PRIVATE void FFPU do_fsub ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
    FLD     TBYTE PTR [EDI]
		FSUB		ST(0),ST(1)
		FXAM
    FNSTSW  x86_status_word
		FSTP    TBYTE PTR [EDI]
		FSTP    ST(0)
	} */
	__asm__ __volatile__(
			"fldt	%2\n"
			"fldt	%1\n"
			"fsub	%%st(1), %%st(0)\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
			"fstp	%%st(0)\n"
		:	"=m" (x86_status_word), "+m" (dest)
		:	"m" (src)
		);
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word &= ~(SW_EXCEPTION_MASK - SW_IE - SW_UE - SW_OE - SW_PE);
		x86_status_word_accrued |= x86_status_word;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fsub");
}

PRIVATE void FFPU do_fsincos ( fpu_register & dest_sin, fpu_register & dest_cos, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest_cos]
    FLD     TBYTE PTR [ESI]
		FSINCOS
		FSTP    TBYTE PTR [EDI]
		FXAM
		MOV			EDI, [dest_sin]
    FNSTSW  x86_status_word
		FSTP    TBYTE PTR [EDI]
		FSTP    ST(0)
	} */
	__asm__ __volatile__(
			"fldt	%3\n"
			"fsincos\n"
			"fstpt	%1\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%2\n"
			"fstp	%%st(0)\n"
		:	"=m" (x86_status_word), "=m" (dest_cos), "=m" (dest_sin)
		:	"m" (src)
		);
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word &= ~(SW_EXCEPTION_MASK - SW_IE - SW_UE - SW_PE);
		x86_status_word_accrued |= x86_status_word;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fsincos");
}

PRIVATE void FFPU do_fcmp ( fpu_register & dest, fpu_register const & src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
    FLD     TBYTE PTR [EDI]
		FSUB    ST(0),ST(1)
		FXAM
    FNSTSW  x86_status_word
		FSTP    ST(0)
		FSTP    ST(0)
	} */
	__asm__ __volatile__(
			"fldt	%2\n"
			"fldt	%1\n"
			"fsub	%%st(1), %%st(0)\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstp	%%st(0)\n"
			"fstp	%%st(0)\n"
		:	"=m" (x86_status_word)
		:	"m" (dest), "m" (src)
		);
	if(x86_status_word & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		x86_status_word &= ~SW_EXCEPTION_MASK;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fcmp");
}

// More or less original. Should be reviewed.
PRIVATE fpu_double FFPU to_pack(uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3)
{
	FPU_CONSISTENCY_CHECK_START();

  double d;
  char *cp;
  char str[100];

  cp = str;
  if (wrd1 & 0x80000000)
		*cp++ = '-';
  *cp++ = (char)((wrd1 & 0xf) + '0');
  *cp++ = '.';
  *cp++ = (char)(((wrd2 >> 28) & 0xf) + '0');
  *cp++ = (char)(((wrd2 >> 24) & 0xf) + '0');
  *cp++ = (char)(((wrd2 >> 20) & 0xf) + '0');
  *cp++ = (char)(((wrd2 >> 16) & 0xf) + '0');
  *cp++ = (char)(((wrd2 >> 12) & 0xf) + '0');
  *cp++ = (char)(((wrd2 >> 8) & 0xf) + '0');
  *cp++ = (char)(((wrd2 >> 4) & 0xf) + '0');
  *cp++ = (char)(((wrd2 >> 0) & 0xf) + '0');
  *cp++ = (char)(((wrd3 >> 28) & 0xf) + '0');
  *cp++ = (char)(((wrd3 >> 24) & 0xf) + '0');
  *cp++ = (char)(((wrd3 >> 20) & 0xf) + '0');
  *cp++ = (char)(((wrd3 >> 16) & 0xf) + '0');
  *cp++ = (char)(((wrd3 >> 12) & 0xf) + '0');
  *cp++ = (char)(((wrd3 >> 8) & 0xf) + '0');
  *cp++ = (char)(((wrd3 >> 4) & 0xf) + '0');
  *cp++ = (char)(((wrd3 >> 0) & 0xf) + '0');
  *cp++ = 'E';
  if (wrd1 & 0x40000000)
		*cp++ = '-';
  *cp++ = (char)(((wrd1 >> 24) & 0xf) + '0');
  *cp++ = (char)(((wrd1 >> 20) & 0xf) + '0');
  *cp++ = (char)(((wrd1 >> 16) & 0xf) + '0');
  *cp = 0;
  sscanf(str, "%le", &d);

	D(bug("to_pack str = %s\r\n",str));

	D(bug("to_pack(%X,%X,%X) = %.04f\r\n",wrd1,wrd2,wrd3,(float)d));

	FPU_CONSISTENCY_CHECK_STOP("to_pack");

  return d;
}

// More or less original. Should be reviewed.
PRIVATE void FFPU from_pack (fpu_double src, uae_u32 * wrd1, uae_u32 * wrd2, uae_u32 * wrd3)
{
	FPU_CONSISTENCY_CHECK_START();

  int i;
  int t;
  char *cp;
  char str[100];
  int exponent_digit_count = 0;

  sprintf(str, "%.16e", src);

	D(bug("from_pack(%.04f,%s)\r\n",(float)src,str));

  cp = str;
  *wrd1 = *wrd2 = *wrd3 = 0;
  if (*cp == '-') {
		cp++;
		*wrd1 = 0x80000000;
  }
  if (*cp == '+')
		cp++;
  *wrd1 |= (*cp++ - '0');
  if (*cp == '.')
		cp++;
  for (i = 0; i < 8; i++) {
		*wrd2 <<= 4;
		if (*cp >= '0' && *cp <= '9')
	    *wrd2 |= *cp++ - '0';
  }
  for (i = 0; i < 8; i++) {
		*wrd3 <<= 4;
		if (*cp >= '0' && *cp <= '9')
	    *wrd3 |= *cp++ - '0';
  }
  if (*cp == 'e' || *cp == 'E') {
		cp++;
		if (*cp == '-') {
			cp++;
			*wrd1 |= 0x40000000;
		}
		if (*cp == '+')
			cp++;
		t = 0;
		for (i = 0; i < 3; i++) {
			if (*cp >= '0' && *cp <= '9') {
				t = (t << 4) | (*cp++ - '0');
				exponent_digit_count++;
			}
		}
		*wrd1 |= t << 16;
  }

	D(bug("from_pack(%.04f) = %X,%X,%X\r\n",(float)src,*wrd1,*wrd2,*wrd3));

	WORD sw_temp;
//	_asm FNSTSW sw_temp
	__asm__ __volatile__("fnstsw %0" : "=m" (sw_temp));
	if(sw_temp & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		__asm__ __volatile__("fnclex");
		if(sw_temp & SW_PE) {
			x86_status_word |= SW_PE;
			x86_status_word_accrued |= SW_PE;
		}
	}

	/*
	OPERR is set if the k-factor > + 17 or the magnitude of
	the decimal exponent exceeds three digits;
	cleared otherwise.
	*/
	if(exponent_digit_count > 3) {
		x86_status_word |= SW_IE;
		x86_status_word_accrued |= SW_IE;
	}

	FPU_CONSISTENCY_CHECK_STOP("from_pack");
}

PRIVATE int FFPU get_fp_value (uae_u32 opcode, uae_u32 extra, fpu_register & src)
{
	static const int sz1[8] = {4, 4, 12, 12, 2, 8, 1, 0};
	static const int sz2[8] = {4, 4, 12, 12, 2, 8, 2, 0};

	// D(bug("get_fp_value(%X,%X)\r\n",(int)opcode,(int)extra));
	// dump_first_bytes( regs.pc_p-4, 16 );

  if ((extra & 0x4000) == 0) {
		memcpy( &src, &FPU registers[(extra >> 10) & 7], sizeof(fpu_register) );
//		do_fmove_no_status( src, FPU registers[(extra >> 10) & 7] );
		return 1;
  }

	int mode = (opcode >> 3) & 7;
  int reg = opcode & 7;
  int size = (extra >> 10) & 7;
  uae_u32 ad = 0;

	// D(bug("get_fp_value mode=%d, reg=%d, size=%d\r\n",(int)mode,(int)reg,(int)size));

  switch ((uae_u8)mode) {
    case 0:
			switch ((uae_u8)size) {
				case 6:
					signed_to_extended( (uae_s32)(uae_s8) m68k_dreg (regs, reg), src );
					break;
				case 4:
					signed_to_extended( (uae_s32)(uae_s16) m68k_dreg (regs, reg), src );
					break;
				case 0:
					signed_to_extended( (uae_s32) m68k_dreg (regs, reg), src );
					break;
				case 1:
					to_single( m68k_dreg (regs, reg), src );
					break;
				default:
					return 0;
			}
			return 1;
    case 1:
			return 0;
    case 2:
			ad = m68k_areg (regs, reg);
			break;
    case 3:
			ad = m68k_areg (regs, reg);
			m68k_areg (regs, reg) += reg == 7 ? sz2[size] : sz1[size];
			break;
    case 4:
			m68k_areg (regs, reg) -= reg == 7 ? sz2[size] : sz1[size];
			ad = m68k_areg (regs, reg);
			break;
    case 5:
			ad = m68k_areg (regs, reg) + (uae_s32) (uae_s16) next_iword();
			break;
    case 6:
			ad = get_disp_ea_020 (m68k_areg (regs, reg), next_iword());
			break;
    case 7:
			switch ((uae_u8)reg) {
				case 0:
			    ad = (uae_s32) (uae_s16) next_iword();
					break;
				case 1:
					ad = next_ilong();
					break;
				case 2:
					ad = m68k_getpc ();
					ad += (uae_s32) (uae_s16) next_iword();
					break;
				case 3: {
					uaecptr tmppc = m68k_getpc ();
					uae_u16 tmp = (uae_u16)next_iword();
					ad = get_disp_ea_020 (tmppc, tmp);
					}
					break;
				case 4:
					ad = m68k_getpc ();
					m68k_setpc (ad + sz2[size]);

					/*
					+0000  000004  FSCALE.B   #$01,FP2     | F23C 5926 0001
					F23C 1111001000111100
					5926 0101100100100110
					0001 0000000000000001
					mode = 7
					reg  = 4
					size = 6
					*/
					// Immediate addressing mode && Operation Length == Byte -> 
					// Use the low-order byte of the extension word.

					if(size == 6) ad++;
					
					// May be faster on a PII(I), sz2[size] is already in register
					// ad += sz2[size] - sz1[size];

					break;
				default:
					return 0;
			}
  }

  switch ((uae_u8)size) {
    case 0:
			signed_to_extended( (uae_s32) get_long (ad), src );
			break;
    case 1:
			to_single( get_long (ad), src );
			break;

    case 2:{
	    uae_u32 wrd1, wrd2, wrd3;
	    wrd1 = get_long (ad);
	    ad += 4;
	    wrd2 = get_long (ad);
	    ad += 4;
	    wrd3 = get_long (ad);
			to_exten( wrd1, wrd2, wrd3, src );
			}
			break;
    case 3:{
	    uae_u32 wrd1, wrd2, wrd3;
	    wrd1 = get_long (ad);
	    ad += 4;
	    wrd2 = get_long (ad);
	    ad += 4;
	    wrd3 = get_long (ad);
			double_to_extended( to_pack(wrd1, wrd2, wrd3), src );
			}
			break;
    case 4:
			signed_to_extended( (uae_s32)(uae_s16) get_word(ad), src );
			break;
    case 5:{
	    uae_u32 wrd1, wrd2;
	    wrd1 = get_long (ad);
	    ad += 4;
	    wrd2 = get_long (ad);
			to_double(wrd1, wrd2, src);
			}
			break;
    case 6:
			signed_to_extended( (uae_s32)(uae_s8) get_byte(ad), src );
			break;
    default:
			return 0;
  }

	// D(bug("get_fp_value result = %.04f\r\n",(float)src));

  return 1;
}

PRIVATE int FFPU put_fp_value (fpu_register const & value, uae_u32 opcode, uae_u32 extra)
{
	static const int sz1[8] = {4, 4, 12, 12, 2, 8, 1, 0};
	static const int sz2[8] = {4, 4, 12, 12, 2, 8, 2, 0};

	// D(bug("put_fp_value(%.04f,%X,%X)\r\n",(float)value,(int)opcode,(int)extra));

  if ((extra & 0x4000) == 0) {
		int dest_reg = (extra >> 10) & 7;
		do_fmove( FPU registers[dest_reg], value );
		build_ex_status();
		return 1;
  }

  int mode = (opcode >> 3) & 7;
  int reg = opcode & 7;
  int size = (extra >> 10) & 7;
  uae_u32 ad = 0xffffffff;

	// Clear exception status
	x86_status_word &= ~SW_EXCEPTION_MASK;

  switch ((uae_u8)mode) {
    case 0:
			switch ((uae_u8)size) {
				case 6:
					*((uae_u8 *)&m68k_dreg(regs, reg)) = extended_to_signed_8(value);
					break;
				case 4:
					// TODO_BIGENDIAN
					*((uae_u16 *)&m68k_dreg(regs, reg)) = extended_to_signed_16(value);
					break;
				case 0:
					m68k_dreg (regs, reg) = extended_to_signed_32(value);
					break;
				case 1:
					m68k_dreg (regs, reg) = from_single(value);
					break;
				default:
					return 0;
			}
			return 1;
		case 1:
			return 0;
    case 2:
			ad = m68k_areg (regs, reg);
			break;
    case 3:
			ad = m68k_areg (regs, reg);
			m68k_areg (regs, reg) += reg == 7 ? sz2[size] : sz1[size];
			break;
    case 4:
			m68k_areg (regs, reg) -= reg == 7 ? sz2[size] : sz1[size];
			ad = m68k_areg (regs, reg);
			break;
    case 5:
			ad = m68k_areg (regs, reg) + (uae_s32) (uae_s16) next_iword();
			break;
    case 6:
			ad = get_disp_ea_020 (m68k_areg (regs, reg), next_iword());
			break;
    case 7:
			switch ((uae_u8)reg) {
				case 0:
					ad = (uae_s32) (uae_s16) next_iword();
					break;
				case 1:
					ad = next_ilong();
					break;
				case 2:
					ad = m68k_getpc ();
					ad += (uae_s32) (uae_s16) next_iword();
					break;
				case 3: {
					uaecptr tmppc = m68k_getpc ();
					uae_u16 tmp = (uae_u16)next_iword();
					ad = get_disp_ea_020 (tmppc, tmp);
					}
					break;
				case 4:
					ad = m68k_getpc ();
					m68k_setpc (ad + sz2[size]);
					break;
				default:
					return 0;
			}
  }
  switch ((uae_u8)size) {
    case 0:
			put_long (ad, (uae_s32) extended_to_signed_32(value));
			break;
    case 1:
			put_long (ad, from_single(value));
			break;
		case 2: {
			uae_u32 wrd1, wrd2, wrd3;
			from_exten(value, &wrd1, &wrd2, &wrd3);
			
			x86_status_word &= ~SW_EXCEPTION_MASK;
			if(wrd3) { // TODO: not correct! Just a "smart" guess.
				x86_status_word |= SW_PE;
				x86_status_word_accrued |= SW_PE;
			}

			put_long (ad, wrd1);
			ad += 4;
			put_long (ad, wrd2);
			ad += 4;
			put_long (ad, wrd3);
			}
			break;
    case 3: {
			uae_u32 wrd1, wrd2, wrd3;
			from_pack(extended_to_double(value), &wrd1, &wrd2, &wrd3);
			put_long (ad, wrd1);
			ad += 4;
			put_long (ad, wrd2);
			ad += 4;
			put_long (ad, wrd3);
			}
			break;
		case 4:
			put_word(ad, extended_to_signed_16(value));
			break;
    case 5:{
	    uae_u32 wrd1, wrd2;
	    from_double(value, &wrd1, &wrd2);
	    put_long (ad, wrd1);
	    ad += 4;
	    put_long (ad, wrd2);
			}
			break;
    case 6:
			put_byte(ad, extended_to_signed_8(value));

			break;
    default:
			return 0;
  }
  return 1;
}

PRIVATE int FFPU get_fp_ad(uae_u32 opcode, uae_u32 * ad)
{
  int mode = (opcode >> 3) & 7;
  int reg = opcode & 7;
  switch ( (uae_u8)mode ) {
    case 0:
    case 1:
    	if( (opcode & 0xFF00) == 0xF300 ) {
    		// fsave, frestore
				m68k_setpc (m68k_getpc () - 2);
    	} else {
				m68k_setpc (m68k_getpc () - 4);
    	}
			op_illg (opcode);
			dump_registers( "END  ");
			return 0;
    case 2:
			*ad = m68k_areg (regs, reg);
			break;
    case 3:
			*ad = m68k_areg (regs, reg);
			break;
    case 4:
			*ad = m68k_areg (regs, reg);
			break;
    case 5:
			*ad = m68k_areg (regs, reg) + (uae_s32) (uae_s16) next_iword();
			break;
    case 6:
			*ad = get_disp_ea_020 (m68k_areg (regs, reg), next_iword());
			break;
    case 7:
			switch ( (uae_u8)reg ) {
				case 0:
					*ad = (uae_s32) (uae_s16) next_iword();
					break;
				case 1:
					*ad = next_ilong();
					break;
				case 2:
					*ad = m68k_getpc ();
					*ad += (uae_s32) (uae_s16) next_iword();
					break;
				case 3: {
					uaecptr tmppc = m68k_getpc ();
					uae_u16 tmp = (uae_u16)next_iword();
					*ad = get_disp_ea_020 (tmppc, tmp);
					}
					break;
				default:
					if( (opcode & 0xFF00) == 0xF300 ) {
						// fsave, frestore
						m68k_setpc (m68k_getpc () - 2);
					} else {
						m68k_setpc (m68k_getpc () - 4);
					}
					op_illg (opcode);
					dump_registers( "END  ");
					return 0;
			}
  }
  return 1;
}

#if FPU_DEBUG
#define CONDRET(s,x) D(bug("fpp_cond %s = %d\r\n",s,(uint32)(x))); return (x)
#else
#define CONDRET(s,x) return (x)
#endif

PRIVATE int FFPU fpp_cond(uae_u32 opcode, int condition)
{

#define N				(x86_status_word & SW_N)
#define Z				((x86_status_word & (SW_Z_I_NAN_MASK)) == SW_Z)
#define I				((x86_status_word & (SW_Z_I_NAN_MASK)) == (SW_I))
#define NotANumber		((x86_status_word & (SW_Z_I_NAN_MASK)) == SW_NAN)

  switch (condition) {
		// Common Tests, no BSUN
    case 0x01:
			CONDRET("Equal",Z);
    case 0x0e:
			CONDRET("Not Equal",!Z);

		// IEEE Nonaware Tests, BSUN
    case 0x12:
			SET_BSUN_ON_NAN();
			CONDRET("Greater Than",!(NotANumber || Z || N));
    case 0x1d:
			SET_BSUN_ON_NAN();
			CONDRET("Not Greater Than",NotANumber || Z || N);
    case 0x13:
			SET_BSUN_ON_NAN();
			CONDRET("Greater Than or Equal",Z || !(NotANumber || N));
    case 0x1c:
			SET_BSUN_ON_NAN();
			CONDRET("Not Greater Than or Equal",!Z && (NotANumber || N));
    case 0x14:
			SET_BSUN_ON_NAN();
			CONDRET("Less Than",N && !(NotANumber || Z));
    case 0x1b:
			SET_BSUN_ON_NAN();
			CONDRET("Not Less Than",NotANumber || Z || !N);
    case 0x15:
			SET_BSUN_ON_NAN();
			CONDRET("Less Than or Equal",Z || (N && !NotANumber));
    case 0x1a:
			SET_BSUN_ON_NAN();
			CONDRET("Not Less Than or Equal",NotANumber || !(N || Z));
    case 0x16:
			SET_BSUN_ON_NAN();
			CONDRET("Greater or Less Than",!(NotANumber || Z));
    case 0x19:
			SET_BSUN_ON_NAN();
			CONDRET("Not Greater or Less Than",NotANumber || Z);
    case 0x17:
			CONDRET("Greater, Less or Equal",!NotANumber);
    case 0x18:
			SET_BSUN_ON_NAN();
			CONDRET("Not Greater, Less or Equal",NotANumber);

		// IEEE Aware Tests, no BSUN
    case 0x02:
			CONDRET("Ordered Greater Than",!(NotANumber || Z || N));
    case 0x0d:
			CONDRET("Unordered or Less or Equal",NotANumber || Z || N);
    case 0x03:
			CONDRET("Ordered Greater Than or Equal",Z || !(NotANumber || N));
    case 0x0c:
			CONDRET("Unordered or Less Than",NotANumber || (N && !Z));
    case 0x04:
			CONDRET("Ordered Less Than",N && !(NotANumber || Z));
    case 0x0b:
			CONDRET("Unordered or Greater or Equal",NotANumber || Z || !N);
    case 0x05:
			CONDRET("Ordered Less Than or Equal",Z || (N && !NotANumber));
    case 0x0a:
			CONDRET("Unordered or Greater Than",NotANumber || !(N || Z));
    case 0x06:
			CONDRET("Ordered Greater or Less Than",!(NotANumber || Z));
    case 0x09:
			CONDRET("Unordered or Equal",NotANumber || Z);
    case 0x07:
			CONDRET("Ordered",!NotANumber);
    case 0x08:
			CONDRET("Unordered",NotANumber);

		// Miscellaneous Tests, no BSUN
    case 0x00:
			CONDRET("False",0);
    case 0x0f:
			CONDRET("True",1);

		// Miscellaneous Tests, BSUN
    case 0x10:
			SET_BSUN_ON_NAN();
			CONDRET("Signaling False",0);
    case 0x1f:
			SET_BSUN_ON_NAN();
			CONDRET("Signaling True",1);
    case 0x11:
			SET_BSUN_ON_NAN();
			CONDRET("Signaling Equal",Z);
    case 0x1e:
			SET_BSUN_ON_NAN();
			CONDRET("Signaling Not Equal",!Z);
  }
	CONDRET("",-1);

#undef N
#undef Z
#undef I
#undef NotANumber

}

PUBLIC void REGPARAM2 FFPU fpuop_dbcc(uae_u32 opcode, uae_u32 extra)
{
  uaecptr pc = (uae_u32) m68k_getpc ();
  uae_s32 disp = (uae_s32) (uae_s16) next_iword();
  int cc;

  D(bug("fdbcc_opp %X, %X at %08lx\r\n", (uae_u32)opcode, (uae_u32)extra, m68k_getpc ()));

  cc = fpp_cond(opcode, extra & 0x3f);
  if (cc < 0) {
		m68k_setpc (pc - 4);
		op_illg (opcode);
  } else if (!cc) {
		int reg = opcode & 0x7;

		// TODO_BIGENDIAN
		uae_u16 newv = (uae_u16)(m68k_dreg (regs, reg) & 0xffff) - 1;
		*((uae_u16 *)&m68k_dreg(regs, reg)) = newv;

		if (newv != 0xffff)
	    m68k_setpc (pc + disp);
  }
}

PUBLIC void REGPARAM2 FFPU fpuop_scc(uae_u32 opcode, uae_u32 extra)
{
  uae_u32 ad;
  int cc;

  D(bug("fscc_opp %X, %X at %08lx\r\n", (uae_u32)opcode, (uae_u32)extra, m68k_getpc ()));

  cc = fpp_cond(opcode, extra & 0x3f);
  if (cc < 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
  } else if ((opcode & 0x38) == 0) {
		// TODO_BIGENDIAN
		m68k_dreg (regs, opcode & 7) = (m68k_dreg (regs, opcode & 7) & ~0xff) |
	    (cc ? 0xff : 0x00);
  } else {
		if (get_fp_ad(opcode, &ad)) {
	    put_byte(ad, cc ? 0xff : 0x00);
	  }
  }
}

PUBLIC void REGPARAM2 FFPU fpuop_trapcc(uae_u32 opcode, uaecptr oldpc)
{
  int cc;

  D(bug("ftrapcc_opp %X at %08lx\r\n", (uae_u32)opcode, m68k_getpc ()));

#if I3_ON_FTRAPCC
#error "FIXME: _asm int 3"
	_asm int 3
#endif

	// This must be broken.
  cc = fpp_cond(opcode, opcode & 0x3f);

  if (cc < 0) {
		m68k_setpc (oldpc);
		op_illg (opcode);
  } else if (cc)
		Exception(7, oldpc - 2);
}

// NOTE that we get here also when there is a FNOP (nontrapping false, displ 0)
PUBLIC void REGPARAM2 FFPU fpuop_bcc(uae_u32 opcode, uaecptr pc, uae_u32 extra)
{
  int cc;

  D(bug("fbcc_opp %X, %X at %08lx, jumpto=%X\r\n", (uae_u32)opcode, (uae_u32)extra, m68k_getpc (), extra ));

  cc = fpp_cond(opcode, opcode & 0x3f);
  if (cc < 0) {
		m68k_setpc (pc);
		op_illg (opcode);
  } else if (cc) {
		if ((opcode & 0x40) == 0)
	    extra = (uae_s32) (uae_s16) extra;
		m68k_setpc (pc + extra);
  }
}

// FSAVE has no post-increment
// 0x1f180000 == IDLE state frame, coprocessor version number 1F
PUBLIC void REGPARAM2 FFPU fpuop_save(uae_u32 opcode)
{
  uae_u32 ad;
  int incr = (opcode & 0x38) == 0x20 ? -1 : 1;
  int i;

  D(bug("fsave_opp at %08lx\r\n", m68k_getpc ()));

  if (get_fp_ad(opcode, &ad)) {
		if (FPU is_integral) {
			// Put 4 byte 68040 IDLE frame.
			if (incr < 0) {
				ad -= 4;
				put_long (ad, 0x41000000);
			} else {
				put_long (ad, 0x41000000);
				ad += 4;
			}
		} else {
			// Put 28 byte 68881 IDLE frame.
			if (incr < 0) {
				D(bug("fsave_opp pre-decrement\r\n"));
				ad -= 4;
				// What's this? Some BIU flags, or (incorrectly placed) command/condition?
				put_long (ad, 0x70000000);
				for (i = 0; i < 5; i++) {
					ad -= 4;
					put_long (ad, 0x00000000);
				}
				ad -= 4;
				put_long (ad, 0x1f180000); // IDLE, vers 1f
			} else {
				put_long (ad, 0x1f180000); // IDLE, vers 1f
				ad += 4;
				for (i = 0; i < 5; i++) {
					put_long (ad, 0x00000000);
					ad += 4;
				}
				// What's this? Some BIU flags, or (incorrectly placed) command/condition?
				put_long (ad, 0x70000000);
				ad += 4;
			}
		}
		if ((opcode & 0x38) == 0x18) {
			m68k_areg (regs, opcode & 7) = ad; // Never executed on a 68881
			D(bug("PROBLEM: fsave_opp post-increment\r\n"));
		}
		if ((opcode & 0x38) == 0x20) {
			m68k_areg (regs, opcode & 7) = ad;
			D(bug("fsave_opp pre-decrement %X -> A%d\r\n",ad,opcode & 7));
		}
  }
}

PRIVATE void FFPU do_null_frestore ()
{
	// A null-restore operation sets FP7-FP0 positive, nonsignaling NANs.
	for( int i=0; i<8; i++ ) {
		MAKE_NAN( FPU registers[i] );
	}

	FPU instruction_address = 0;
	set_fpcr(0);
	set_fpsr(0);

	x86_status_word = SW_INITIAL;
	x86_status_word_accrued = 0;
	FPU fpsr.quotient = 0;

	x86_control_word = CW_INITIAL;
/*  _asm	FLDCW   x86_control_word
	_asm	FNCLEX */
	__asm__ __volatile__("fldcw %0\n\tfnclex" : : "m" (x86_control_word));
}

// FSAVE has no pre-decrement
PUBLIC void REGPARAM2 FFPU fpuop_restore(uae_u32 opcode)
{
  uae_u32 ad;
  uae_u32 d;
  int incr = (opcode & 0x38) == 0x20 ? -1 : 1;

  D(bug("frestore_opp at %08lx\r\n", m68k_getpc ()));

  if (get_fp_ad(opcode, &ad)) {
		if (FPU is_integral) {
			// 68040
			if (incr < 0) {
				D(bug("PROBLEM: frestore_opp incr < 0\r\n"));
				// this may be wrong, but it's never called.
				ad -= 4;
				d = get_long (ad);
				if ((d & 0xff000000) == 0) { // NULL
					D(bug("frestore_opp found NULL frame at %X\r\n",ad-4));
					do_null_frestore();
				} else if ((d & 0x00ff0000) == 0) { // IDLE
					D(bug("frestore_opp found IDLE frame at %X\r\n",ad-4));
				} else if ((d & 0x00ff0000) == 0x00300000) { // UNIMP
					D(bug("PROBLEM: frestore_opp found UNIMP frame at %X\r\n",ad-4));
					ad -= 44;
				} else if ((d & 0x00ff0000) == 0x00600000) { // BUSY
					D(bug("PROBLEM: frestore_opp found BUSY frame at %X\r\n",ad-4));
					ad -= 92;
				} else {
					D(bug("PROBLEM: frestore_opp did not find a frame at %X, d=%X\r\n",ad-4,d));
				}
			} else {
				d = get_long (ad);
				D(bug("frestore_opp frame at %X = %X\r\n",ad,d));
				ad += 4;
				if ((d & 0xff000000) == 0) { // NULL
					D(bug("frestore_opp found NULL frame at %X\r\n",ad-4));
					do_null_frestore();
				} else if ((d & 0x00ff0000) == 0) { // IDLE
					D(bug("frestore_opp found IDLE frame at %X\r\n",ad-4));
				} else if ((d & 0x00ff0000) == 0x00300000) { // UNIMP
					D(bug("PROBLEM: frestore_opp found UNIMP frame at %X\r\n",ad-4));
					ad += 44;
				} else if ((d & 0x00ff0000) == 0x00600000) { // BUSY
					D(bug("PROBLEM: frestore_opp found BUSY frame at %X\r\n",ad-4));
					ad += 92;
				} else {
					D(bug("PROBLEM: frestore_opp did not find a frame at %X, d=%X\r\n",ad-4,d));
				}
			}
		} else {
			// 68881
			if (incr < 0) {
				D(bug("PROBLEM: frestore_opp incr < 0\r\n"));
				// this may be wrong, but it's never called.
				ad -= 4;
				d = get_long (ad);
				if ((d & 0xff000000) == 0) { // NULL
					do_null_frestore();
				} else if ((d & 0x00ff0000) == 0x00180000) {
					ad -= 6 * 4;
				} else if ((d & 0x00ff0000) == 0x00380000) {
					ad -= 14 * 4;
				} else if ((d & 0x00ff0000) == 0x00b40000) {
					ad -= 45 * 4;
				}
			} else {
				d = get_long (ad);
				D(bug("frestore_opp frame at %X = %X\r\n",ad,d));
				ad += 4;
				if ((d & 0xff000000) == 0) { // NULL
					D(bug("frestore_opp found NULL frame at %X\r\n",ad-4));
					do_null_frestore();
				} else if ((d & 0x00ff0000) == 0x00180000) { // IDLE
					D(bug("frestore_opp found IDLE frame at %X\r\n",ad-4));
					ad += 6 * 4;
				} else if ((d & 0x00ff0000) == 0x00380000) {// UNIMP? shouldn't it be 3C?
					ad += 14 * 4;
					D(bug("PROBLEM: frestore_opp found UNIMP? frame at %X\r\n",ad-4));
				} else if ((d & 0x00ff0000) == 0x00b40000) {// BUSY
					D(bug("PROBLEM: frestore_opp found BUSY frame at %X\r\n",ad-4));
					ad += 45 * 4;
				} else {
					D(bug("PROBLEM: frestore_opp did not find a frame at %X, d=%X\r\n",ad-4,d));
				}
			}
		}

		if ((opcode & 0x38) == 0x18) {
			m68k_areg (regs, opcode & 7) = ad;
			D(bug("frestore_opp post-increment %X -> A%d\r\n",ad,opcode & 7));
		}
		if ((opcode & 0x38) == 0x20) {
			m68k_areg (regs, opcode & 7) = ad; // Never executed on a 68881
			D(bug("PROBLEM: frestore_opp pre-decrement\r\n"));
		}
  }
}


/* ---------------------------- Old-style interface ---------------------------- */

// #ifndef OPTIMIZED_8BIT_MEMORY_ACCESS
PUBLIC void REGPARAM2 FFPU fpuop_arithmetic(uae_u32 opcode, uae_u32 extra)
{
	uae_u32 mask = (extra & 0xFC7F) | ((opcode & 0x0038) << 4);
	(*fpufunctbl[mask])(opcode,extra);
}
// #endif


/* ---------------------------- Illegal ---------------------------- */

PRIVATE void REGPARAM2 FFPU fpuop_illg( uae_u32 opcode, uae_u32 extra )
{
	D(bug("ILLEGAL F OP 2 %X\r\n",opcode));

#if I3_ON_ILLEGAL_FPU_OP
#error "FIXME: asm int 3"
	_asm int 3
#endif

	m68k_setpc (m68k_getpc () - 4);
	op_illg (opcode);
	dump_registers( "END  ");
}


/* ---------------------------- FPP -> <ea> ---------------------------- */

PRIVATE void REGPARAM2 FFPU fpuop_fmove_2_ea( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVE -> <ea>\r\n"));

	if (put_fp_value (FPU registers[(extra >> 7) & 7], opcode, extra) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
	}

	/*
	Needed (among other things) by some Pack5/Elems68k transcendental
	functions, they require the ACCR_INEX flag after a "MOVE.D, Dreg".
	However, now put_fp_value() is responsible of clearing the exceptions
	and merging statuses.
	*/

	/*
	WORD sw_temp;
	_asm FNSTSW sw_temp
	if(sw_temp & SW_PE) {
		_asm FNCLEX
		x86_status_word |= SW_PE;
		x86_status_word_accrued |= SW_PE;
	}
	*/

	dump_registers( "END  ");
}


/* ---------------------------- CONTROL REGS -> Dreg ---------------------------- */

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_none_2_Dreg( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM control(none) -> D%d\r\n", opcode & 7));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpiar_2_Dreg( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM FPU instruction_address (%X) -> D%d\r\n", FPU instruction_address, opcode & 7));
	m68k_dreg (regs, opcode & 7) = FPU instruction_address;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpsr_2_Dreg( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM regs.FPU fpsr (%X) -> D%d\r\n", get_fpsr(), opcode & 7));
	m68k_dreg (regs, opcode & 7) = get_fpsr();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_2_Dreg( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM regs.FPU fpcr (%X) -> D%d\r\n", get_fpcr(), opcode & 7));
	m68k_dreg (regs, opcode & 7) = get_fpcr();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpsr_fpiar_2_Dreg( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM regs.FPU fpsr (%X) -> D%d\r\n", get_fpsr(), opcode & 7));
	m68k_dreg (regs, opcode & 7) = get_fpsr();
	D(bug("FMOVEM FPU instruction_address (%X) -> D%d\r\n", FPU instruction_address, opcode & 7));
	m68k_dreg (regs, opcode & 7) = FPU instruction_address;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpiar_2_Dreg( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM regs.FPU fpcr (%X) -> D%d\r\n", get_fpcr(), opcode & 7));
	m68k_dreg (regs, opcode & 7) = get_fpcr();
	D(bug("FMOVEM FPU instruction_address (%X) -> D%d\r\n", FPU instruction_address, opcode & 7));
	m68k_dreg (regs, opcode & 7) = FPU instruction_address;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpsr_2_Dreg( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM regs.FPU fpcr (%X) -> D%d\r\n", get_fpcr(), opcode & 7));
	m68k_dreg (regs, opcode & 7) = get_fpcr();
	D(bug("FMOVEM regs.FPU fpsr (%X) -> D%d\r\n", get_fpsr(), opcode & 7));
	m68k_dreg (regs, opcode & 7) = get_fpsr();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpsr_fpiar_2_Dreg( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM regs.FPU fpcr (%X) -> D%d\r\n", get_fpcr(), opcode & 7));
	m68k_dreg (regs, opcode & 7) = get_fpcr();
	D(bug("FMOVEM regs.FPU fpsr (%X) -> D%d\r\n", get_fpsr(), opcode & 7));
	m68k_dreg (regs, opcode & 7) = get_fpsr();
	D(bug("FMOVEM FPU instruction_address (%X) -> D%d\r\n", FPU instruction_address, opcode & 7));
	m68k_dreg (regs, opcode & 7) = FPU instruction_address;
	dump_registers( "END  ");
}


/* ---------------------------- Dreg -> CONTROL REGS ---------------------------- */

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Dreg_2_none( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM D%d -> control(none)\r\n", opcode & 7));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Dreg_2_fpiar( uae_u32 opcode, uae_u32 extra )
{
	FPU instruction_address = m68k_dreg (regs, opcode & 7);
	D(bug("FMOVEM D%d (%X) -> FPU instruction_address\r\n", opcode & 7, FPU instruction_address));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Dreg_2_fpsr( uae_u32 opcode, uae_u32 extra )
{
	set_fpsr( m68k_dreg (regs, opcode & 7) );
	D(bug("FMOVEM D%d (%X) -> regs.FPU fpsr\r\n", opcode & 7, get_fpsr()));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Dreg_2_fpsr_fpiar( uae_u32 opcode, uae_u32 extra )
{
	set_fpsr( m68k_dreg (regs, opcode & 7) );
	D(bug("FMOVEM D%d (%X) -> regs.FPU fpsr\r\n", opcode & 7, get_fpsr()));
	FPU instruction_address = m68k_dreg (regs, opcode & 7);
	D(bug("FMOVEM D%d (%X) -> FPU instruction_address\r\n", opcode & 7, FPU instruction_address));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Dreg_2_fpcr( uae_u32 opcode, uae_u32 extra )
{
	set_fpcr( m68k_dreg (regs, opcode & 7) );
	D(bug("FMOVEM D%d (%X) -> regs.FPU fpcr\r\n", opcode & 7, get_fpcr()));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Dreg_2_fpcr_fpiar( uae_u32 opcode, uae_u32 extra )
{
	set_fpcr( m68k_dreg (regs, opcode & 7) );
	D(bug("FMOVEM D%d (%X) -> regs.FPU fpcr\r\n", opcode & 7, get_fpcr()));
	FPU instruction_address = m68k_dreg (regs, opcode & 7);
	D(bug("FMOVEM D%d (%X) -> FPU instruction_address\r\n", opcode & 7, FPU instruction_address));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Dreg_2_fpcr_fpsr( uae_u32 opcode, uae_u32 extra )
{
	set_fpcr( m68k_dreg (regs, opcode & 7) );
	D(bug("FMOVEM D%d (%X) -> regs.FPU fpcr\r\n", opcode & 7, get_fpcr()));
	set_fpsr( m68k_dreg (regs, opcode & 7) );
	D(bug("FMOVEM D%d (%X) -> regs.FPU fpsr\r\n", opcode & 7, get_fpsr()));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Dreg_2_fpcr_fpsr_fpiar( uae_u32 opcode, uae_u32 extra )
{
	set_fpcr( m68k_dreg (regs, opcode & 7) );
	D(bug("FMOVEM D%d (%X) -> regs.FPU fpcr\r\n", opcode & 7, get_fpcr()));
	set_fpsr( m68k_dreg (regs, opcode & 7) );
	D(bug("FMOVEM D%d (%X) -> regs.FPU fpsr\r\n", opcode & 7, get_fpsr()));
	FPU instruction_address = m68k_dreg (regs, opcode & 7);
	D(bug("FMOVEM D%d (%X) -> FPU instruction_address\r\n", opcode & 7, FPU instruction_address));
	dump_registers( "END  ");
}


/* ---------------------------- CONTROL REGS -> Areg ---------------------------- */

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_none_2_Areg( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM control(none) -> A%d\r\n", opcode & 7));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpiar_2_Areg( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM FPU instruction_address (%X) -> A%d\r\n", FPU instruction_address, opcode & 7));
	m68k_areg (regs, opcode & 7) = FPU instruction_address;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpsr_2_Areg( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM regs.FPU fpsr (%X) -> A%d\r\n", get_fpsr(), opcode & 7));
	m68k_areg (regs, opcode & 7) = get_fpsr();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_2_Areg( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM regs.FPU fpcr (%X) -> A%d\r\n", get_fpcr(), opcode & 7));
	m68k_areg (regs, opcode & 7) = get_fpcr();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpsr_fpiar_2_Areg( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM regs.FPU fpsr (%X) -> A%d\r\n", get_fpsr(), opcode & 7));
	m68k_areg (regs, opcode & 7) = get_fpsr();
	D(bug("FMOVEM FPU instruction_address (%X) -> A%d\r\n", FPU instruction_address, opcode & 7));
	m68k_areg (regs, opcode & 7) = FPU instruction_address;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpiar_2_Areg( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM regs.FPU fpcr (%X) -> A%d\r\n", get_fpcr(), opcode & 7));
	m68k_areg (regs, opcode & 7) = get_fpcr();
	D(bug("FMOVEM FPU instruction_address (%X) -> A%d\r\n", FPU instruction_address, opcode & 7));
	m68k_areg (regs, opcode & 7) = FPU instruction_address;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpsr_2_Areg( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM regs.FPU fpcr (%X) -> A%d\r\n", get_fpcr(), opcode & 7));
	m68k_areg (regs, opcode & 7) = get_fpcr();
	D(bug("FMOVEM regs.FPU fpsr (%X) -> A%d\r\n", get_fpsr(), opcode & 7));
	m68k_areg (regs, opcode & 7) = get_fpsr();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpsr_fpiar_2_Areg( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM regs.FPU fpcr (%X) -> A%d\r\n", get_fpcr(), opcode & 7));
	m68k_areg (regs, opcode & 7) = get_fpcr();
	D(bug("FMOVEM regs.FPU fpsr (%X) -> A%d\r\n", get_fpsr(), opcode & 7));
	m68k_areg (regs, opcode & 7) = get_fpsr();
	D(bug("FMOVEM FPU instruction_address (%X) -> A%d\r\n", FPU instruction_address, opcode & 7));
	m68k_areg (regs, opcode & 7) = FPU instruction_address;
	dump_registers( "END  ");
}


/* ---------------------------- Areg -> CONTROL REGS ---------------------------- */

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Areg_2_none( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM A%d -> control(none)\r\n", opcode & 7));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Areg_2_fpiar( uae_u32 opcode, uae_u32 extra )
{
	FPU instruction_address = m68k_areg (regs, opcode & 7);
	D(bug("FMOVEM A%d (%X) -> FPU instruction_address\r\n", opcode & 7, FPU instruction_address));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Areg_2_fpsr( uae_u32 opcode, uae_u32 extra )
{
	set_fpsr( m68k_areg (regs, opcode & 7) );
	D(bug("FMOVEM A%d (%X) -> regs.FPU fpsr\r\n", opcode & 7, get_fpsr()));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Areg_2_fpsr_fpiar( uae_u32 opcode, uae_u32 extra )
{
	set_fpsr( m68k_areg (regs, opcode & 7) );
	D(bug("FMOVEM A%d (%X) -> regs.FPU fpsr\r\n", opcode & 7, get_fpsr()));
	FPU instruction_address = m68k_areg (regs, opcode & 7);
	D(bug("FMOVEM A%d (%X) -> FPU instruction_address\r\n", opcode & 7, FPU instruction_address));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Areg_2_fpcr( uae_u32 opcode, uae_u32 extra )
{
	set_fpcr( m68k_areg (regs, opcode & 7) );
	D(bug("FMOVEM A%d (%X) -> regs.FPU fpcr\r\n", opcode & 7, get_fpcr()));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Areg_2_fpcr_fpiar( uae_u32 opcode, uae_u32 extra )
{
	set_fpcr( m68k_areg (regs, opcode & 7) );
	D(bug("FMOVEM A%d (%X) -> regs.FPU fpcr\r\n", opcode & 7, get_fpcr()));
	FPU instruction_address = m68k_areg (regs, opcode & 7);
	D(bug("FMOVEM A%d (%X) -> FPU instruction_address\r\n", opcode & 7, FPU instruction_address));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Areg_2_fpcr_fpsr( uae_u32 opcode, uae_u32 extra )
{
	set_fpcr( m68k_areg (regs, opcode & 7) );
	D(bug("FMOVEM A%d (%X) -> regs.FPU fpcr\r\n", opcode & 7, get_fpcr()));
	set_fpsr( m68k_areg (regs, opcode & 7) );
	D(bug("FMOVEM A%d (%X) -> regs.FPU fpsr\r\n", opcode & 7, get_fpsr()));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Areg_2_fpcr_fpsr_fpiar( uae_u32 opcode, uae_u32 extra )
{
	set_fpcr( m68k_areg (regs, opcode & 7) );
	D(bug("FMOVEM A%d (%X) -> regs.FPU fpcr\r\n", opcode & 7, get_fpcr()));
	set_fpsr( m68k_areg (regs, opcode & 7) );
	D(bug("FMOVEM A%d (%X) -> regs.FPU fpsr\r\n", opcode & 7, get_fpsr()));
	FPU instruction_address = m68k_areg (regs, opcode & 7);
	D(bug("FMOVEM A%d (%X) -> FPU instruction_address\r\n", opcode & 7, FPU instruction_address));
	dump_registers( "END  ");
}


/* ---------------------------- CONTROL REGS -> --MEMORY---------------------------- */

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_none_2_Mem_predecrement( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM Control regs (none) -> mem\r\n" ));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpiar_2_Mem_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 4;
		put_long (ad, FPU instruction_address);
		D(bug("FMOVEM FPU instruction_address (%X) -> mem %X\r\n", FPU instruction_address, ad ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpsr_2_Mem_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 4;
		put_long (ad, get_fpsr());
		D(bug("FMOVEM regs.FPU fpsr (%X) -> mem %X\r\n", get_fpsr(), ad ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpsr_fpiar_2_Mem_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 8;
		put_long (ad, get_fpsr());
		D(bug("FMOVEM regs.FPU fpsr (%X) -> mem %X\r\n", get_fpsr(), ad ));
		put_long (ad+4, FPU instruction_address);
		D(bug("FMOVEM FPU instruction_address (%X) -> mem %X\r\n", FPU instruction_address, ad+4 ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_2_Mem_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 4;
		put_long (ad, get_fpcr());
		D(bug("FMOVEM regs.FPU fpcr (%X) -> mem %X\r\n", get_fpcr(), ad ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpiar_2_Mem_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 8;
		put_long (ad, get_fpcr());
		D(bug("FMOVEM regs.FPU fpcr (%X) -> mem %X\r\n", get_fpcr(), ad ));
		put_long (ad+4, FPU instruction_address);
		D(bug("FMOVEM FPU instruction_address (%X) -> mem %X\r\n", FPU instruction_address, ad+4 ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpsr_2_Mem_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 8;
		put_long (ad, get_fpcr());
		D(bug("FMOVEM regs.FPU fpcr (%X) -> mem %X\r\n", get_fpcr(), ad ));
		put_long (ad+4, get_fpsr());
		D(bug("FMOVEM regs.FPU fpsr (%X) -> mem %X\r\n", get_fpsr(), ad+4 ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpsr_fpiar_2_Mem_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 12;
		put_long (ad, get_fpcr());
		D(bug("FMOVEM regs.FPU fpcr (%X) -> mem %X\r\n", get_fpcr(), ad ));
		put_long (ad+4, get_fpsr());
		D(bug("FMOVEM regs.FPU fpsr (%X) -> mem %X\r\n", get_fpsr(), ad+4 ));
		put_long (ad+8, FPU instruction_address);
		D(bug("FMOVEM FPU instruction_address (%X) -> mem %X\r\n", FPU instruction_address, ad+8 ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}


/* ---------------------------- CONTROL REGS -> MEMORY++ ---------------------------- */

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_none_2_Mem_postincrement( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM Control regs (none) -> mem\r\n" ));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpiar_2_Mem_postincrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, FPU instruction_address);
		D(bug("FMOVEM FPU instruction_address (%X) -> mem %X\r\n", FPU instruction_address, ad ));
		m68k_areg (regs, opcode & 7) = ad+4;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpsr_2_Mem_postincrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, get_fpsr());
		D(bug("FMOVEM regs.FPU fpsr (%X) -> mem %X\r\n", get_fpsr(), ad ));
		m68k_areg (regs, opcode & 7) = ad+4;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpsr_fpiar_2_Mem_postincrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, get_fpsr());
		D(bug("FMOVEM regs.FPU fpsr (%X) -> mem %X\r\n", get_fpsr(), ad ));
		put_long (ad+4, FPU instruction_address);
		D(bug("FMOVEM FPU instruction_address (%X) -> mem %X\r\n", FPU instruction_address, ad+4 ));
		m68k_areg (regs, opcode & 7) = ad+8;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_2_Mem_postincrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, get_fpcr());
		D(bug("FMOVEM regs.FPU fpcr (%X) -> mem %X\r\n", get_fpcr(), ad ));
		m68k_areg (regs, opcode & 7) = ad+4;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpiar_2_Mem_postincrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, get_fpcr());
		D(bug("FMOVEM regs.FPU fpcr (%X) -> mem %X\r\n", get_fpcr(), ad ));
		put_long (ad+4, FPU instruction_address);
		D(bug("FMOVEM FPU instruction_address (%X) -> mem %X\r\n", FPU instruction_address, ad+4 ));
		m68k_areg (regs, opcode & 7) = ad+8;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpsr_2_Mem_postincrement( uae_u32 opcode, uae_u32 extra )
{
	dump_registers( "END  ");
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, get_fpcr());
		D(bug("FMOVEM regs.FPU fpcr (%X) -> mem %X\r\n", get_fpcr(), ad ));
		put_long (ad+4, get_fpsr());
		D(bug("FMOVEM regs.FPU fpsr (%X) -> mem %X\r\n", get_fpsr(), ad+4 ));
		m68k_areg (regs, opcode & 7) = ad+8;
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpsr_fpiar_2_Mem_postincrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, get_fpcr());
		D(bug("FMOVEM regs.FPU fpcr (%X) -> mem %X\r\n", get_fpcr(), ad ));
		put_long (ad+4, get_fpsr());
		D(bug("FMOVEM regs.FPU fpsr (%X) -> mem %X\r\n", get_fpsr(), ad+4 ));
		put_long (ad+8, FPU instruction_address);
		D(bug("FMOVEM FPU instruction_address (%X) -> mem %X\r\n", FPU instruction_address, ad+8 ));
		m68k_areg (regs, opcode & 7) = ad+12;
		dump_registers( "END  ");
	}
}


/* ---------------------------- CONTROL REGS -> MEMORY ---------------------------- */

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_none_2_Mem( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM Control regs (none) -> mem\r\n" ));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpiar_2_Mem( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, FPU instruction_address);
		D(bug("FMOVEM FPU instruction_address (%X) -> mem %X\r\n", FPU instruction_address, ad ));
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpsr_2_Mem( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, get_fpsr());
		D(bug("FMOVEM regs.FPU fpsr (%X) -> mem %X\r\n", get_fpsr(), ad ));
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpsr_fpiar_2_Mem( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, get_fpsr());
		D(bug("FMOVEM regs.FPU fpsr (%X) -> mem %X\r\n", get_fpsr(), ad ));
		put_long (ad+4, FPU instruction_address);
		D(bug("FMOVEM FPU instruction_address (%X) -> mem %X\r\n", FPU instruction_address, ad+4 ));
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_2_Mem( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, get_fpcr());
		D(bug("FMOVEM regs.FPU fpcr (%X) -> mem %X\r\n", get_fpcr(), ad ));
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpiar_2_Mem( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, get_fpcr());
		D(bug("FMOVEM regs.FPU fpcr (%X) -> mem %X\r\n", get_fpcr(), ad ));
		put_long (ad+4, FPU instruction_address);
		D(bug("FMOVEM FPU instruction_address (%X) -> mem %X\r\n", FPU instruction_address, ad+4 ));
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpsr_2_Mem( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, get_fpcr());
		D(bug("FMOVEM regs.FPU fpcr (%X) -> mem %X\r\n", get_fpcr(), ad ));
		put_long (ad+4, get_fpsr());
		D(bug("FMOVEM regs.FPU fpsr (%X) -> mem %X\r\n", get_fpsr(), ad+4 ));
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpsr_fpiar_2_Mem( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, get_fpcr());
		D(bug("FMOVEM regs.FPU fpcr (%X) -> mem %X\r\n", get_fpcr(), ad ));
		put_long (ad+4, get_fpsr());
		D(bug("FMOVEM regs.FPU fpsr (%X) -> mem %X\r\n", get_fpsr(), ad+4 ));
		put_long (ad+8, FPU instruction_address);
		D(bug("FMOVEM FPU instruction_address (%X) -> mem %X\r\n", FPU instruction_address, ad+8 ));
		dump_registers( "END  ");
	}
}


/* ---------------------------- --MEMORY -> CONTROL REGS ---------------------------- */

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_none_predecrement( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM --Mem -> control(none)\r\n"));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpiar_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 4;
		FPU instruction_address = get_long (ad);
		D(bug("FMOVEM mem %X (%X) -> FPU instruction_address\r\n", ad, FPU instruction_address ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpsr_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 4;
		set_fpsr( get_long (ad) );
		D(bug("FMOVEM mem %X (%X) -> regs.FPU fpsr\r\n", ad, get_fpsr() ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpsr_fpiar_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 8;
		set_fpsr( get_long (ad) );
		D(bug("FMOVEM mem %X (%X) -> regs.FPU fpsr\r\n", ad, get_fpsr() ));
		FPU instruction_address = get_long (ad+4);
		D(bug("FMOVEM mem %X (%X) -> FPU instruction_address\r\n", ad+4, FPU instruction_address ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 4;
		set_fpcr( get_long (ad) );
		D(bug("FMOVEM mem %X (%X) -> regs.FPU fpcr\r\n", ad, get_fpcr() ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_fpiar_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 8;
		set_fpcr( get_long (ad) );
		D(bug("FMOVEM mem %X (%X) -> regs.FPU fpcr\r\n", ad, get_fpcr() ));
		FPU instruction_address = get_long (ad+4);
		D(bug("FMOVEM mem %X (%X) -> FPU instruction_address\r\n", ad+4, FPU instruction_address ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_fpsr_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 8;
		set_fpcr( get_long (ad) );
		D(bug("FMOVEM mem %X (%X) -> regs.FPU fpcr\r\n", ad, get_fpcr() ));
		set_fpsr( get_long (ad+4) );
		D(bug("FMOVEM mem %X (%X) -> regs.FPU fpsr\r\n", ad+4, get_fpsr() ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_fpsr_fpiar_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 12;
		set_fpcr( get_long (ad) );
		D(bug("FMOVEM mem %X (%X) -> regs.FPU fpcr\r\n", ad, get_fpcr() ));
		set_fpsr( get_long (ad+4) );
		D(bug("FMOVEM mem %X (%X) -> regs.FPU fpsr\r\n", ad+4, get_fpsr() ));
		FPU instruction_address = get_long (ad+8);
		D(bug("FMOVEM mem %X (%X) -> FPU instruction_address\r\n", ad+8, FPU instruction_address ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}


/* ---------------------------- CONTROL REGS -> MEMORY++ ---------------------------- */

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_none_postincrement( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM Mem++ -> control(none)\r\n"));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpiar_postincrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		FPU instruction_address = get_long (ad);
		D(bug("FMOVEM mem %X (%X) -> FPU instruction_address\r\n", ad, FPU instruction_address ));
		m68k_areg (regs, opcode & 7) = ad+4;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpsr_postincrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		set_fpsr( get_long (ad) );
		D(bug("FMOVEM mem %X (%X) -> regs.FPU fpsr\r\n", ad, get_fpsr() ));
		m68k_areg (regs, opcode & 7) = ad+4;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpsr_fpiar_postincrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		set_fpsr( get_long (ad) );
		D(bug("FMOVEM mem %X (%X) -> regs.FPU fpsr\r\n", ad, get_fpsr() ));
		FPU instruction_address = get_long (ad+4);
		D(bug("FMOVEM mem %X (%X) -> FPU instruction_address\r\n", ad+4, FPU instruction_address ));
		m68k_areg (regs, opcode & 7) = ad+8;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_postincrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		set_fpcr( get_long (ad) );
		D(bug("FMOVEM mem %X (%X) -> regs.FPU fpcr\r\n", ad, get_fpcr() ));
		m68k_areg (regs, opcode & 7) = ad+4;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_fpiar_postincrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		set_fpcr( get_long (ad) );
		D(bug("FMOVEM mem %X (%X) -> regs.FPU fpcr\r\n", ad, get_fpcr() ));
		FPU instruction_address = get_long (ad+4);
		D(bug("FMOVEM mem %X (%X) -> FPU instruction_address\r\n", ad+4, FPU instruction_address ));
		m68k_areg (regs, opcode & 7) = ad+8;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_fpsr_postincrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		set_fpcr( get_long (ad) );
		D(bug("FMOVEM mem %X (%X) -> regs.FPU fpcr\r\n", ad, get_fpcr() ));
		set_fpsr( get_long (ad+4) );
		D(bug("FMOVEM mem %X (%X) -> regs.FPU fpsr\r\n", ad+4, get_fpsr() ));
		m68k_areg (regs, opcode & 7) = ad+8;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_fpsr_fpiar_postincrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		set_fpcr( get_long (ad) );
		D(bug("FMOVEM mem %X (%X) -> regs.FPU fpcr\r\n", ad, get_fpcr() ));
		set_fpsr( get_long (ad+4) );
		D(bug("FMOVEM mem %X (%X) -> regs.FPU fpsr\r\n", ad+4, get_fpsr() ));
		FPU instruction_address = get_long (ad+8);
		D(bug("FMOVEM mem %X (%X) -> FPU instruction_address\r\n", ad+8, FPU instruction_address ));
		m68k_areg (regs, opcode & 7) = ad+12;
		dump_registers( "END  ");
	}
}


/* ----------------------------   MEMORY -> CONTROL REGS  ---------------------------- */
/* ----------------------------            and            ---------------------------- */
/* ---------------------------- IMMEDIATE -> CONTROL REGS ---------------------------- */

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_none_2_Mem( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVEM Mem -> control(none)\r\n"));
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpiar_2_Mem( uae_u32 opcode, uae_u32 extra )
{
	if ((opcode & 0x3f) == 0x3c) {
		FPU instruction_address = next_ilong();
		D(bug("FMOVEM #<%X> -> FPU instruction_address\r\n", FPU instruction_address));
	} else {
		uae_u32 ad;
		if (get_fp_ad(opcode, &ad)) {
			FPU instruction_address = get_long (ad);
			D(bug("FMOVEM mem %X (%X) -> FPU instruction_address\r\n", ad, FPU instruction_address ));
		}
	}
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpsr_2_Mem( uae_u32 opcode, uae_u32 extra )
{
	if ((opcode & 0x3f) == 0x3c) {
		set_fpsr( next_ilong() );
		D(bug("FMOVEM #<%X> -> regs.FPU fpsr\r\n", get_fpsr()));
	} else {
		uae_u32 ad;
		if (get_fp_ad(opcode, &ad)) {
			set_fpsr( get_long (ad) );
			D(bug("FMOVEM mem %X (%X) -> regs.FPU fpsr\r\n", ad, get_fpsr() ));
		}
	}
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpsr_fpiar_2_Mem( uae_u32 opcode, uae_u32 extra )
{
	if ((opcode & 0x3f) == 0x3c) {
		set_fpsr( next_ilong() );
		D(bug("FMOVEM #<%X> -> regs.FPU fpsr\r\n", get_fpsr()));
		FPU instruction_address = next_ilong();
		D(bug("FMOVEM #<%X> -> FPU instruction_address\r\n", FPU instruction_address));
	} else {
		uae_u32 ad;
		if (get_fp_ad(opcode, &ad)) {
			set_fpsr( get_long (ad) );
			D(bug("FMOVEM mem %X (%X) -> regs.FPU fpsr\r\n", ad, get_fpsr() ));
			FPU instruction_address = get_long (ad+4);
			D(bug("FMOVEM mem %X (%X) -> FPU instruction_address\r\n", ad+4, FPU instruction_address ));
		}
	}
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_2_Mem( uae_u32 opcode, uae_u32 extra )
{
	if ((opcode & 0x3f) == 0x3c) {
		set_fpcr( next_ilong() );
		D(bug("FMOVEM #<%X> -> regs.FPU fpcr\r\n", get_fpcr()));
	} else {
		uae_u32 ad;
		if (get_fp_ad(opcode, &ad)) {
			set_fpcr( get_long (ad) );
			D(bug("FMOVEM mem %X (%X) -> regs.FPU fpcr\r\n", ad, get_fpcr() ));
		}
	}
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_fpiar_2_Mem( uae_u32 opcode, uae_u32 extra )
{
	if ((opcode & 0x3f) == 0x3c) {
		set_fpcr( next_ilong() );
		D(bug("FMOVEM #<%X> -> regs.FPU fpcr\r\n", get_fpcr()));
		FPU instruction_address = next_ilong();
		D(bug("FMOVEM #<%X> -> FPU instruction_address\r\n", FPU instruction_address));
	} else {
		uae_u32 ad;
		if (get_fp_ad(opcode, &ad)) {
			set_fpcr( get_long (ad) );
			D(bug("FMOVEM mem %X (%X) -> regs.FPU fpcr\r\n", ad, get_fpcr() ));
			FPU instruction_address = get_long (ad+4);
			D(bug("FMOVEM mem %X (%X) -> FPU instruction_address\r\n", ad+4, FPU instruction_address ));
		}
	}
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_fpsr_2_Mem( uae_u32 opcode, uae_u32 extra )
{
	if ((opcode & 0x3f) == 0x3c) {
		set_fpcr( next_ilong() );
		D(bug("FMOVEM #<%X> -> regs.FPU fpcr\r\n", get_fpcr()));
		set_fpsr( next_ilong() );
		D(bug("FMOVEM #<%X> -> regs.FPU fpsr\r\n", get_fpsr()));
	} else {
		uae_u32 ad;
		if (get_fp_ad(opcode, &ad)) {
			set_fpcr( get_long (ad) );
			D(bug("FMOVEM mem %X (%X) -> regs.FPU fpcr\r\n", ad, get_fpcr() ));
			set_fpsr( get_long (ad+4) );
			D(bug("FMOVEM mem %X (%X) -> regs.FPU fpsr\r\n", ad+4, get_fpsr() ));
		}
	}
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_fpsr_fpiar_2_Mem( uae_u32 opcode, uae_u32 extra )
{
	if ((opcode & 0x3f) == 0x3c) {
		set_fpcr( next_ilong() );
		D(bug("FMOVEM #<%X> -> regs.FPU fpcr\r\n", get_fpcr()));
		set_fpsr( next_ilong() );
		D(bug("FMOVEM #<%X> -> regs.FPU fpsr\r\n", get_fpsr()));
		FPU instruction_address = next_ilong();
		D(bug("FMOVEM #<%X> -> FPU instruction_address\r\n", FPU instruction_address));
	} else {
		uae_u32 ad;
		if (get_fp_ad(opcode, &ad)) {
			set_fpcr( get_long (ad) );
			D(bug("FMOVEM mem %X (%X) -> regs.FPU fpcr\r\n", ad, get_fpcr() ));
			set_fpsr( get_long (ad+4) );
			D(bug("FMOVEM mem %X (%X) -> regs.FPU fpsr\r\n", ad+4, get_fpsr() ));
			FPU instruction_address = get_long (ad+8);
			D(bug("FMOVEM mem %X (%X) -> FPU instruction_address\r\n", ad+8, FPU instruction_address ));
		}
	}
	dump_registers( "END  ");
}


/* ---------------------------- FMOVEM MEMORY -> FPP ---------------------------- */

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_static_pred_postincrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = extra & 0xff;
	D(bug("FMOVEM memory->FPP\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=7; reg>=0; reg-- ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				ad -= 4;
				wrd3 = get_long (ad);
				ad -= 4;
				wrd2 = get_long (ad);
				ad -= 4;
				wrd1 = get_long (ad);
				to_exten_no_normalize (wrd1, wrd2, wrd3,FPU registers[reg]);
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_static_pred_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = extra & 0xff;
	D(bug("FMOVEM memory->FPP\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=7; reg>=0; reg-- ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				ad -= 4;
				wrd3 = get_long (ad);
				ad -= 4;
				wrd2 = get_long (ad);
				ad -= 4;
				wrd1 = get_long (ad);
				to_exten_no_normalize (wrd1, wrd2, wrd3,FPU registers[reg]);
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_static_pred( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = extra & 0xff;
	D(bug("FMOVEM memory->FPP\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=7; reg>=0; reg-- ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				ad -= 4;
				wrd3 = get_long (ad);
				ad -= 4;
				wrd2 = get_long (ad);
				ad -= 4;
				wrd1 = get_long (ad);
				to_exten_no_normalize (wrd1, wrd2, wrd3,FPU registers[reg]);
			}
			list <<= 1;
		}
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_dynamic_pred_postincrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
	D(bug("FMOVEM memory->FPP\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=7; reg>=0; reg-- ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				ad -= 4;
				wrd3 = get_long (ad);
				ad -= 4;
				wrd2 = get_long (ad);
				ad -= 4;
				wrd1 = get_long (ad);
				to_exten_no_normalize (wrd1, wrd2, wrd3,FPU registers[reg]);
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_dynamic_pred_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
	D(bug("FMOVEM memory->FPP\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=7; reg>=0; reg-- ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				ad -= 4;
				wrd3 = get_long (ad);
				ad -= 4;
				wrd2 = get_long (ad);
				ad -= 4;
				wrd1 = get_long (ad);
				to_exten_no_normalize (wrd1, wrd2, wrd3,FPU registers[reg]);
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_dynamic_pred( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
	D(bug("FMOVEM memory->FPP\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=7; reg>=0; reg-- ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				ad -= 4;
				wrd3 = get_long (ad);
				ad -= 4;
				wrd2 = get_long (ad);
				ad -= 4;
				wrd1 = get_long (ad);
				to_exten_no_normalize (wrd1, wrd2, wrd3,FPU registers[reg]);
			}
			list <<= 1;
		}
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_static_postinc_postincrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = extra & 0xff;
	D(bug("FMOVEM memory->FPP\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=0; reg<8; reg++ ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				wrd1 = get_long (ad);
				ad += 4;
				wrd2 = get_long (ad);
				ad += 4;
				wrd3 = get_long (ad);
				ad += 4;
				to_exten_no_normalize (wrd1, wrd2, wrd3,FPU registers[reg]);
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_static_postinc_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = extra & 0xff;
	D(bug("FMOVEM memory->FPP\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=0; reg<8; reg++ ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				wrd1 = get_long (ad);
				ad += 4;
				wrd2 = get_long (ad);
				ad += 4;
				wrd3 = get_long (ad);
				ad += 4;
				to_exten_no_normalize (wrd1, wrd2, wrd3,FPU registers[reg]);
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_static_postinc( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = extra & 0xff;
	D(bug("FMOVEM memory->FPP\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=0; reg<8; reg++ ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				wrd1 = get_long (ad);
				ad += 4;
				wrd2 = get_long (ad);
				ad += 4;
				wrd3 = get_long (ad);
				ad += 4;
				to_exten_no_normalize (wrd1, wrd2, wrd3,FPU registers[reg]);
			}
			list <<= 1;
		}
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_dynamic_postinc_postincrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
	D(bug("FMOVEM memory->FPP\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=0; reg<8; reg++ ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				wrd1 = get_long (ad);
				ad += 4;
				wrd2 = get_long (ad);
				ad += 4;
				wrd3 = get_long (ad);
				ad += 4;
				to_exten_no_normalize (wrd1, wrd2, wrd3,FPU registers[reg]);
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_dynamic_postinc_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
	D(bug("FMOVEM memory->FPP\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=0; reg<8; reg++ ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				wrd1 = get_long (ad);
				ad += 4;
				wrd2 = get_long (ad);
				ad += 4;
				wrd3 = get_long (ad);
				ad += 4;
				to_exten_no_normalize (wrd1, wrd2, wrd3,FPU registers[reg]);
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_dynamic_postinc( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
	D(bug("FMOVEM memory->FPP\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=0; reg<8; reg++ ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				wrd1 = get_long (ad);
				ad += 4;
				wrd2 = get_long (ad);
				ad += 4;
				wrd3 = get_long (ad);
				ad += 4;
				to_exten_no_normalize (wrd1, wrd2, wrd3,FPU registers[reg]);
			}
			list <<= 1;
		}
		dump_registers( "END  ");
	}
}


/* ---------------------------- FPP -> FMOVEM MEMORY ---------------------------- */

PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_static_pred_postincrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = extra & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=7; reg>=0; reg-- ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(FPU registers[reg],&wrd1, &wrd2, &wrd3);
				ad -= 4;
				put_long (ad, wrd3);
				ad -= 4;
				put_long (ad, wrd2);
				ad -= 4;
				put_long (ad, wrd1);
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_static_pred_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = extra & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=7; reg>=0; reg-- ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(FPU registers[reg],&wrd1, &wrd2, &wrd3);
				ad -= 4;
				put_long (ad, wrd3);
				ad -= 4;
				put_long (ad, wrd2);
				ad -= 4;
				put_long (ad, wrd1);
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_static_pred( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = extra & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=7; reg>=0; reg-- ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(FPU registers[reg],&wrd1, &wrd2, &wrd3);
				ad -= 4;
				put_long (ad, wrd3);
				ad -= 4;
				put_long (ad, wrd2);
				ad -= 4;
				put_long (ad, wrd1);
			}
			list <<= 1;
		}
		dump_registers( "END  ");
	}
}
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_dynamic_pred_postincrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=7; reg>=0; reg-- ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(FPU registers[reg],&wrd1, &wrd2, &wrd3);
				ad -= 4;
				put_long (ad, wrd3);
				ad -= 4;
				put_long (ad, wrd2);
				ad -= 4;
				put_long (ad, wrd1);
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_dynamic_pred_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=7; reg>=0; reg-- ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(FPU registers[reg],&wrd1, &wrd2, &wrd3);
				ad -= 4;
				put_long (ad, wrd3);
				ad -= 4;
				put_long (ad, wrd2);
				ad -= 4;
				put_long (ad, wrd1);
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_dynamic_pred( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=7; reg>=0; reg-- ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(FPU registers[reg],&wrd1, &wrd2, &wrd3);
				ad -= 4;
				put_long (ad, wrd3);
				ad -= 4;
				put_long (ad, wrd2);
				ad -= 4;
				put_long (ad, wrd1);
			}
			list <<= 1;
		}
		dump_registers( "END  ");
	}
}
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_static_postinc_postincrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = extra & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=0; reg<8; reg++ ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(FPU registers[reg],&wrd1, &wrd2, &wrd3);
				put_long (ad, wrd1);
				ad += 4;
				put_long (ad, wrd2);
				ad += 4;
				put_long (ad, wrd3);
				ad += 4;
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_static_postinc_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = extra & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=0; reg<8; reg++ ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(FPU registers[reg],&wrd1, &wrd2, &wrd3);
				put_long (ad, wrd1);
				ad += 4;
				put_long (ad, wrd2);
				ad += 4;
				put_long (ad, wrd3);
				ad += 4;
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_static_postinc( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = extra & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=0; reg<8; reg++ ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(FPU registers[reg],&wrd1, &wrd2, &wrd3);
				put_long (ad, wrd1);
				ad += 4;
				put_long (ad, wrd2);
				ad += 4;
				put_long (ad, wrd3);
				ad += 4;
			}
			list <<= 1;
		}
		dump_registers( "END  ");
	}
}
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_dynamic_postinc_postincrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=0; reg<8; reg++ ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(FPU registers[reg],&wrd1, &wrd2, &wrd3);
				put_long (ad, wrd1);
				ad += 4;
				put_long (ad, wrd2);
				ad += 4;
				put_long (ad, wrd3);
				ad += 4;
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_dynamic_postinc_predecrement( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=0; reg<8; reg++ ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(FPU registers[reg],&wrd1, &wrd2, &wrd3);
				put_long (ad, wrd1);
				ad += 4;
				put_long (ad, wrd2);
				ad += 4;
				put_long (ad, wrd3);
				ad += 4;
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_registers( "END  ");
	}
}
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_dynamic_postinc( uae_u32 opcode, uae_u32 extra )
{
	uae_u32 ad, list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=0; reg<8; reg++ ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(FPU registers[reg],&wrd1, &wrd2, &wrd3);
				put_long (ad, wrd1);
				ad += 4;
				put_long (ad, wrd2);
				ad += 4;
				put_long (ad, wrd3);
				ad += 4;
			}
			list <<= 1;
		}
		dump_registers( "END  ");
	}
}


/* ---------------------------- FMOVEM CONSTANT ROM -> FPP ---------------------------- */

PRIVATE void REGPARAM2 FFPU fpuop_do_fldpi( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: Pi\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_pi, sizeof(fpu_register) );
	x86_status_word = SW_FINITE | FPSR_EXCEPTION_INEX2;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fldlg2( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: Log 10 (2)\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_lg2, sizeof(fpu_register) );
	x86_status_word = SW_FINITE | FPSR_EXCEPTION_INEX2;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_e( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: e\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_e, sizeof(fpu_register) );
	x86_status_word = SW_FINITE | FPSR_EXCEPTION_INEX2;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fldl2e( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: Log 2 (e)\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_l2e, sizeof(fpu_register) );
	x86_status_word = SW_FINITE | FPSR_EXCEPTION_INEX2;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_log_10_e( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: Log 10 (e)\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_log_10_e, sizeof(fpu_register) );
	x86_status_word = SW_FINITE | FPSR_EXCEPTION_INEX2;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fldz( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: zero\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_z, sizeof(fpu_register) );
	x86_status_word = SW_Z;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fldln2( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: ln(2)\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_ln2, sizeof(fpu_register) );
	x86_status_word = SW_FINITE | FPSR_EXCEPTION_INEX2;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_ln_10( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: ln(10)\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_ln_10, sizeof(fpu_register) );
	x86_status_word = SW_FINITE | FPSR_EXCEPTION_INEX2;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fld1( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e0\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_1, sizeof(fpu_register) );
	x86_status_word = SW_FINITE;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e1( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e1\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_1e1, sizeof(fpu_register) );
	x86_status_word = SW_FINITE;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e2( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e2\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_1e2, sizeof(fpu_register) );
	x86_status_word = SW_FINITE;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e4( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e4\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_1e4, sizeof(fpu_register) );
	x86_status_word = SW_FINITE;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e8( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e8\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_1e8, sizeof(fpu_register) );
	x86_status_word = SW_FINITE | FPSR_EXCEPTION_INEX2; // Is it really FPSR_EXCEPTION_INEX2?
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e16( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e16\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_1e16, sizeof(fpu_register) );
	x86_status_word = SW_FINITE | FPSR_EXCEPTION_INEX2;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e32( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e32\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_1e32, sizeof(fpu_register) );
	x86_status_word = SW_FINITE | FPSR_EXCEPTION_INEX2;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e64( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e64\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_1e64, sizeof(fpu_register) );
	x86_status_word = SW_FINITE | FPSR_EXCEPTION_INEX2;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e128( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e128\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_1e128, sizeof(fpu_register) );
	x86_status_word = SW_FINITE | FPSR_EXCEPTION_INEX2;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e256( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e256\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_1e256, sizeof(fpu_register) );
	x86_status_word = SW_FINITE | FPSR_EXCEPTION_INEX2;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e512( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e512\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_1e512, sizeof(fpu_register) );
	x86_status_word = SW_FINITE | FPSR_EXCEPTION_INEX2;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e1024( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e1024\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_1e1024, sizeof(fpu_register) );
	x86_status_word = SW_FINITE | FPSR_EXCEPTION_INEX2;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e2048( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e2048\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_1e2048, sizeof(fpu_register) );
	x86_status_word = SW_FINITE | FPSR_EXCEPTION_INEX2;
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e4096( uae_u32 opcode, uae_u32 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e4096\r\n"));
	memcpy( &FPU registers[(extra>>7) & 7], &const_1e4096, sizeof(fpu_register) );
	x86_status_word = SW_FINITE | FPSR_EXCEPTION_INEX2;
	dump_registers( "END  ");
}


/* ---------------------------- ALU ---------------------------- */

PRIVATE void REGPARAM2 FFPU fpuop_do_fmove( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FMOVE %s\r\n",etos(src)));
	do_fmove( FPU registers[reg], src );
	build_ex_status();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fint( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FINT %s, opcode=%X, extra=%X, ta %X\r\n",etos(src),opcode,extra,m68k_getpc()));
	do_fint( FPU registers[reg], src );
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fsinh( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FSINH %s\r\n",etos(src)));
	do_fsinh( FPU registers[reg], src );
	build_ex_status();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fintrz( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FINTRZ %s\r\n",etos(src)));
	do_fintrz( FPU registers[reg], src );
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fsqrt( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FSQRT %s\r\n",etos(src)));
	do_fsqrt( FPU registers[reg], src );
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_flognp1( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FLOGNP1 %s\r\n",etos(src)));
	do_flognp1( FPU registers[reg], src );
	build_ex_status();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fetoxm1( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FETOXM1 %s\r\n",etos(src)));
	do_fetoxm1( FPU registers[reg], src );
	build_ex_status();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_ftanh( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FTANH %s\r\n",etos(src)));
	do_ftanh( FPU registers[reg], src );
	build_ex_status();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fatan( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FATAN %s\r\n",etos(src)));
	do_fatan( FPU registers[reg], src );
	build_ex_status();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fasin( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FASIN %s\r\n",etos(src)));
	do_fasin( FPU registers[reg], src );
	build_ex_status();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fatanh( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FATANH %s\r\n",etos(src)));
	do_fatanh( FPU registers[reg], src );
	build_ex_status();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fsin( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FSIN %s\r\n",etos(src)));
	do_fsin( FPU registers[reg], src );
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_ftan( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FTAN %s\r\n",etos(src)));
	do_ftan( FPU registers[reg], src );
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fetox( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FETOX %s\r\n",etos(src)));
	do_fetox( FPU registers[reg], src );
	build_ex_status();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_ftwotox( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FTWOTOX %s\r\n",etos(src)));
	do_ftwotox( FPU registers[reg], src );
	build_ex_status();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_ftentox( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FTENTOX %s\r\n",etos(src)));
	do_ftentox( FPU registers[reg], src );
	build_ex_status();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_flogn( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FLOGN %s\r\n",etos(src)));
	do_flogn( FPU registers[reg], src );
	build_ex_status();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_flog10( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FLOG10 %s\r\n",etos(src)));
	do_flog10( FPU registers[reg], src );
	build_ex_status();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_flog2( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FLOG2 %s\r\n",etos(src)));
	do_flog2( FPU registers[reg], src );
	build_ex_status();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fabs( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FABS %s\r\n",etos(src)));
	do_fabs( FPU registers[reg], src );
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fcosh( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FCOSH %s\r\n",etos(src)));
	do_fcosh( FPU registers[reg], src );
	build_ex_status();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fneg( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FNEG %s\r\n",etos(src)));
	do_fneg( FPU registers[reg], src );
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_facos( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FACOS %s\r\n",etos(src)));
	do_facos( FPU registers[reg], src );
	build_ex_status();
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fcos( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FCOS %s\r\n",etos(src)));
	do_fcos( FPU registers[reg], src );
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fgetexp( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FGETEXP %s\r\n",etos(src)));

	if( IS_INFINITY(src) ) {
		MAKE_NAN( FPU registers[reg] );
		do_ftst( FPU registers[reg] );
		x86_status_word |= SW_IE;
	} else {
		do_fgetexp( FPU registers[reg], src );
	}
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fgetman( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FGETMAN %s\r\n",etos(src)));
	if( IS_INFINITY(src) ) {
		MAKE_NAN( FPU registers[reg] );
		do_ftst( FPU registers[reg] );
		x86_status_word |= SW_IE;
	} else {
		do_fgetman( FPU registers[reg], src );
	}
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fdiv( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FDIV %s\r\n",etos(src)));
	do_fdiv( FPU registers[reg], src );
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fmod( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FMOD %s\r\n",etos(src)));

#if USE_3_BIT_QUOTIENT
	do_fmod( FPU registers[reg], src );
#else
	if( (x86_control_word & X86_ROUNDING_MODE) == CW_RC_ZERO ) {
		do_fmod_dont_set_cw( FPU registers[reg], src );
	} else {
		do_fmod( FPU registers[reg], src );
	}
#endif
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_frem( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FREM %s\r\n",etos(src)));
#if USE_3_BIT_QUOTIENT
	do_frem( FPU registers[reg], src );
#else
	if( (x86_control_word & X86_ROUNDING_MODE) == CW_RC_NEAR ) {
		do_frem_dont_set_cw( FPU registers[reg], src );
	} else {
		do_frem( FPU registers[reg], src );
	}
#endif
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fadd( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FADD %s\r\n",etos(src)));
	do_fadd( FPU registers[reg], src );
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fmul( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FMUL %s\r\n",etos(src)));
	do_fmul( FPU registers[reg], src );
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fsgldiv( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FSGLDIV %s\r\n",etos(src)));
	do_fsgldiv( FPU registers[reg], src );
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fscale( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FSCALE %s, opcode=%X, extra=%X, ta %X\r\n",etos(src),opcode,extra,m68k_getpc()));
	if( IS_INFINITY(FPU registers[reg]) ) {
		MAKE_NAN( FPU registers[reg] );
		do_ftst( FPU registers[reg] );
		x86_status_word |= SW_IE;
	} else {
		// When the absolute value of the source operand is >= 2^14,
		// an overflow or underflow always results.
		do_fscale( FPU registers[reg], src );
	}
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fsglmul( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FSGLMUL %s\r\n",etos(src)));
	do_fsglmul( FPU registers[reg], src );
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fsub( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FSUB %s\r\n",etos(src)));
	do_fsub( FPU registers[reg], src );
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fsincos( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FSINCOS %s\r\n",etos(src)));
	do_fsincos( FPU registers[reg], FPU registers[extra & 7], src );
	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_fcmp( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FCMP %s\r\n",etos(src)));

	if( IS_INFINITY(src) ) {
		if( IS_NEGATIVE(src) ) {
			if( IS_INFINITY(FPU registers[reg]) && IS_NEGATIVE(FPU registers[reg]) ) {
				x86_status_word = SW_Z | SW_N;
				D(bug("-INF FCMP -INF -> NZ\r\n"));
			} else {
				x86_status_word = SW_FINITE;
				D(bug("X FCMP -INF -> None\r\n"));
			}
		} else {
			if( IS_INFINITY(FPU registers[reg]) && !IS_NEGATIVE(FPU registers[reg]) ) {
				x86_status_word = SW_Z;
				D(bug("+INF FCMP +INF -> Z\r\n"));
			} else {
				x86_status_word = SW_N;
				D(bug("X FCMP +INF -> N\r\n"));
			}
		}
	} else if( IS_INFINITY(FPU registers[reg]) ) {
		if( IS_NEGATIVE(FPU registers[reg]) ) {
			x86_status_word = SW_N;
			D(bug("-INF FCMP X -> Negative\r\n"));
		} else {
			x86_status_word = SW_FINITE;
			D(bug("+INF FCMP X -> None\r\n"));
		}
	} else {
		do_fcmp( FPU registers[reg], src );
	}

	dump_registers( "END  ");
}

PRIVATE void REGPARAM2 FFPU fpuop_do_ftst( uae_u32 opcode, uae_u32 extra )
{
	int reg = (extra >> 7) & 7;
  fpu_register src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_registers( "END  ");
		return;
	}
	D(bug("FTST %s\r\n",etos(src)));
	do_ftst( src );
	build_ex_status();
	dump_registers( "END  ");
}



/* ---------------------------- SETUP TABLES ---------------------------- */

PRIVATE void FFPU build_fpp_opp_lookup_table ()
{
	for( uae_u32 opcode=0; opcode<=0x38; opcode+=8 ) {
		for( uae_u32 extra=0; extra<65536; extra++ ) {
			uae_u32 mask = (extra & 0xFC7F) | ((opcode & 0x0038) << 4);
			fpufunctbl[mask] = & FFPU fpuop_illg;

			switch ((extra >> 13) & 0x7) {
				case 3:
					fpufunctbl[mask] = & FFPU fpuop_fmove_2_ea;
					break;
				case 4:
				case 5:
					if ((opcode & 0x38) == 0) {
						if (extra & 0x2000) { // dr bit
							switch( extra & 0x1C00 ) {
								case 0x0000:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_none_2_Dreg;
									break;
								case 0x0400:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpiar_2_Dreg;
									break;
								case 0x0800:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpsr_2_Dreg;
									break;
								case 0x0C00:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpsr_fpiar_2_Dreg;
									break;
								case 0x1000:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpcr_2_Dreg;
									break;
								case 0x1400:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpcr_fpiar_2_Dreg;
									break;
								case 0x1800:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpcr_fpsr_2_Dreg;
									break;
								case 0x1C00:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpcr_fpsr_fpiar_2_Dreg;
									break;
							}
						} else {
							switch( extra & 0x1C00 ) {
								case 0x0000:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Dreg_2_none;
									break;
								case 0x0400:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Dreg_2_fpiar;
									break;
								case 0x0800:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Dreg_2_fpsr;
									break;
								case 0x0C00:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Dreg_2_fpsr_fpiar;
									break;
								case 0x1000:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Dreg_2_fpcr;
									break;
								case 0x1400:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Dreg_2_fpcr_fpiar;
									break;
								case 0x1800:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Dreg_2_fpcr_fpsr;
									break;
								case 0x1C00:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Dreg_2_fpcr_fpsr_fpiar;
									break;
							}
						}
					} else if ((opcode & 0x38) == 8) {
						if (extra & 0x2000) { // dr bit
							switch( extra & 0x1C00 ) {
								case 0x0000:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_none_2_Areg;
									break;
								case 0x0400:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpiar_2_Areg;
									break;
								case 0x0800:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpsr_2_Areg;
									break;
								case 0x0C00:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpsr_fpiar_2_Areg;
									break;
								case 0x1000:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpcr_2_Areg;
									break;
								case 0x1400:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpcr_fpiar_2_Areg;
									break;
								case 0x1800:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpcr_fpsr_2_Areg;
									break;
								case 0x1C00:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpcr_fpsr_fpiar_2_Areg;
									break;
							}
						} else {
							switch( extra & 0x1C00 ) {
								case 0x0000:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Areg_2_none;
									break;
								case 0x0400:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Areg_2_fpiar;
									break;
								case 0x0800:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Areg_2_fpsr;
									break;
								case 0x0C00:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Areg_2_fpsr_fpiar;
									break;
								case 0x1000:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Areg_2_fpcr;
									break;
								case 0x1400:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Areg_2_fpcr_fpiar;
									break;
								case 0x1800:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Areg_2_fpcr_fpsr;
									break;
								case 0x1C00:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Areg_2_fpcr_fpsr_fpiar;
									break;
							}
						}
					} else if (extra & 0x2000) {
						if ((opcode & 0x38) == 0x20) {
							switch( extra & 0x1C00 ) {
								case 0x0000:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_none_2_Mem_predecrement;
									break;
								case 0x0400:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpiar_2_Mem_predecrement;
									break;
								case 0x0800:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpsr_2_Mem_predecrement;
									break;
								case 0x0C00:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpsr_fpiar_2_Mem_predecrement;
									break;
								case 0x1000:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpcr_2_Mem_predecrement;
									break;
								case 0x1400:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpcr_fpiar_2_Mem_predecrement;
									break;
								case 0x1800:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpcr_fpsr_2_Mem_predecrement;
									break;
								case 0x1C00:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpcr_fpsr_fpiar_2_Mem_predecrement;
									break;
							}
						} else if ((opcode & 0x38) == 0x18) {
							switch( extra & 0x1C00 ) {
								case 0x0000:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_none_2_Mem_postincrement;
									break;
								case 0x0400:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpiar_2_Mem_postincrement;
									break;
								case 0x0800:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpsr_2_Mem_postincrement;
									break;
								case 0x0C00:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpsr_fpiar_2_Mem_postincrement;
									break;
								case 0x1000:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpcr_2_Mem_postincrement;
									break;
								case 0x1400:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpcr_fpiar_2_Mem_postincrement;
									break;
								case 0x1800:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpcr_fpsr_2_Mem_postincrement;
									break;
								case 0x1C00:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpcr_fpsr_fpiar_2_Mem_postincrement;
									break;
							}
						} else {
							switch( extra & 0x1C00 ) {
								case 0x0000:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_none_2_Mem;
									break;
								case 0x0400:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpiar_2_Mem;
									break;
								case 0x0800:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpsr_2_Mem;
									break;
								case 0x0C00:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpsr_fpiar_2_Mem;
									break;
								case 0x1000:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpcr_2_Mem;
									break;
								case 0x1400:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpcr_fpiar_2_Mem;
									break;
								case 0x1800:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpcr_fpsr_2_Mem;
									break;
								case 0x1C00:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_fpcr_fpsr_fpiar_2_Mem;
									break;
							}
						}
					} else {
						if ((opcode & 0x38) == 0x20) {
							switch( extra & 0x1C00 ) {
								case 0x0000:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_none_predecrement;
									break;
								case 0x0400:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpiar_predecrement;
									break;
								case 0x0800:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpsr_predecrement;
									break;
								case 0x0C00:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpsr_fpiar_predecrement;
									break;
								case 0x1000:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpcr_predecrement;
									break;
								case 0x1400:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpcr_fpiar_predecrement;
									break;
								case 0x1800:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpcr_fpsr_predecrement;
									break;
								case 0x1C00:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpcr_fpsr_fpiar_predecrement;
									break;
							}
						} else if ((opcode & 0x38) == 0x18) {
							switch( extra & 0x1C00 ) {
								case 0x0000:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_none_postincrement;
									break;
								case 0x0400:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpiar_postincrement;
									break;
								case 0x0800:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpsr_postincrement;
									break;
								case 0x0C00:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpsr_fpiar_postincrement;
									break;
								case 0x1000:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpcr_postincrement;
									break;
								case 0x1400:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpcr_fpiar_postincrement;
									break;
								case 0x1800:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpcr_fpsr_postincrement;
									break;
								case 0x1C00:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpcr_fpsr_fpiar_postincrement;
									break;
							}
						} else {
							switch( extra & 0x1C00 ) {
								case 0x0000:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_none_2_Mem;
									break;
								case 0x0400:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpiar_2_Mem;
									break;
								case 0x0800:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpsr_2_Mem;
									break;
								case 0x0C00:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpsr_fpiar_2_Mem;
									break;
								case 0x1000:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpcr_2_Mem;
									break;
								case 0x1400:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpcr_fpiar_2_Mem;
									break;
								case 0x1800:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpcr_fpsr_2_Mem;
									break;
								case 0x1C00:
									fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpcr_fpsr_fpiar_2_Mem;
									break;
							}
						}
					break;
				case 6:
					switch ((extra >> 11) & 3) {
						case 0:	/* static pred */
							if ((opcode & 0x38) == 0x18) // post-increment?
								fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpp_static_pred_postincrement;
							else if ((opcode & 0x38) == 0x20) // pre-decrement?
								fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpp_static_pred_predecrement;
							else
								fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpp_static_pred;
							break;
						case 1:	/* dynamic pred */
							if ((opcode & 0x38) == 0x18) // post-increment?
								fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpp_dynamic_pred_postincrement;
							else if ((opcode & 0x38) == 0x20) // pre-decrement?
								fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpp_dynamic_pred_predecrement;
							else
								fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpp_dynamic_pred;
							break;
						case 2:	/* static postinc */
							if ((opcode & 0x38) == 0x18) // post-increment?
								fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpp_static_postinc_postincrement;
							else if ((opcode & 0x38) == 0x20) // pre-decrement?
								fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpp_static_postinc_predecrement;
							else
								fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpp_static_postinc;
							break;
						case 3:	/* dynamic postinc */
							if ((opcode & 0x38) == 0x18) // post-increment?
								fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpp_dynamic_postinc_postincrement;
							else if ((opcode & 0x38) == 0x20) // pre-decrement?
								fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpp_dynamic_postinc_predecrement;
							else
								fpufunctbl[mask] = & FFPU fpuop_fmovem_Mem_2_fpp_dynamic_postinc;
							break;
					}
					break;
				case 7:
					switch ((extra >> 11) & 3) {
						case 0:	/* static pred */
							if ((opcode & 0x38) == 0x18) // post-increment?
								fpufunctbl[mask] = & FFPU fpuop_fmovem_fpp_2_Mem_static_pred_postincrement;
							else if ((opcode & 0x38) == 0x20) // pre-decrement?
								fpufunctbl[mask] = & FFPU fpuop_fmovem_fpp_2_Mem_static_pred_predecrement;
							else
								fpufunctbl[mask] = & FFPU fpuop_fmovem_fpp_2_Mem_static_pred;
							break;
						case 1:	/* dynamic pred */
							if ((opcode & 0x38) == 0x18) // post-increment?
								fpufunctbl[mask] = & FFPU fpuop_fmovem_fpp_2_Mem_dynamic_pred_postincrement;
							else if ((opcode & 0x38) == 0x20) // pre-decrement?
								fpufunctbl[mask] = & FFPU fpuop_fmovem_fpp_2_Mem_dynamic_pred_predecrement;
							else
								fpufunctbl[mask] = & FFPU fpuop_fmovem_fpp_2_Mem_dynamic_pred;
							break;
						case 2:	/* static postinc */
							if ((opcode & 0x38) == 0x18) // post-increment?
								fpufunctbl[mask] = & FFPU fpuop_fmovem_fpp_2_Mem_static_postinc_postincrement;
							else if ((opcode & 0x38) == 0x20) // pre-decrement?
								fpufunctbl[mask] = & FFPU fpuop_fmovem_fpp_2_Mem_static_postinc_predecrement;
							else
								fpufunctbl[mask] = & FFPU fpuop_fmovem_fpp_2_Mem_static_postinc;
							break;
						case 3:	/* dynamic postinc */
							if ((opcode & 0x38) == 0x18) // post-increment?
								fpufunctbl[mask] = & FFPU fpuop_fmovem_fpp_2_Mem_dynamic_postinc_postincrement;
							else if ((opcode & 0x38) == 0x20) // pre-decrement?
								fpufunctbl[mask] = & FFPU fpuop_fmovem_fpp_2_Mem_dynamic_postinc_predecrement;
							else
								fpufunctbl[mask] = & FFPU fpuop_fmovem_fpp_2_Mem_dynamic_postinc;
							break;
					}
					break;
				case 0:
				case 2:
					if ((extra & 0xfc00) == 0x5c00) {
						switch (extra & 0x7f) {
							case 0x00:
								fpufunctbl[mask] = & FFPU fpuop_do_fldpi;
								break;
							case 0x0b:
								fpufunctbl[mask] = & FFPU fpuop_do_fldlg2;
								break;
							case 0x0c:
								fpufunctbl[mask] = & FFPU fpuop_do_load_const_e;
								break;
							case 0x0d:
								fpufunctbl[mask] = & FFPU fpuop_do_fldl2e;
								break;
							case 0x0e:
								fpufunctbl[mask] = & FFPU fpuop_do_load_const_log_10_e;
								break;
							case 0x0f:
								fpufunctbl[mask] = & FFPU fpuop_do_fldz;
								break;
							case 0x30:
								fpufunctbl[mask] = & FFPU fpuop_do_fldln2;
								break;
							case 0x31:
								fpufunctbl[mask] = & FFPU fpuop_do_load_const_ln_10;
								break;
							case 0x32:
								fpufunctbl[mask] = & FFPU fpuop_do_fld1;
								break;
							case 0x33:
								fpufunctbl[mask] = & FFPU fpuop_do_load_const_1e1;
								break;
							case 0x34:
								fpufunctbl[mask] = & FFPU fpuop_do_load_const_1e2;
								break;
							case 0x35:
								fpufunctbl[mask] = & FFPU fpuop_do_load_const_1e4;
								break;
							case 0x36:
								fpufunctbl[mask] = & FFPU fpuop_do_load_const_1e8;
								break;
							case 0x37:
								fpufunctbl[mask] = & FFPU fpuop_do_load_const_1e16;
								break;
							case 0x38:
								fpufunctbl[mask] = & FFPU fpuop_do_load_const_1e32;
								break;
							case 0x39:
								fpufunctbl[mask] = & FFPU fpuop_do_load_const_1e64;
								break;
							case 0x3a:
								fpufunctbl[mask] = & FFPU fpuop_do_load_const_1e128;
								break;
							case 0x3b:
								fpufunctbl[mask] = & FFPU fpuop_do_load_const_1e256;
								break;
							case 0x3c:
								fpufunctbl[mask] = & FFPU fpuop_do_load_const_1e512;
								break;
							case 0x3d:
								fpufunctbl[mask] = & FFPU fpuop_do_load_const_1e1024;
								break;
							case 0x3e:
								fpufunctbl[mask] = & FFPU fpuop_do_load_const_1e2048;
								break;
							case 0x3f:
								fpufunctbl[mask] = & FFPU fpuop_do_load_const_1e4096;
								break;
						}
						break;
					}
					
					switch (extra & 0x7f) {
						case 0x00:
							fpufunctbl[mask] = & FFPU fpuop_do_fmove;
							break;
						case 0x01:
							fpufunctbl[mask] = & FFPU fpuop_do_fint;
							break;
						case 0x02:
							fpufunctbl[mask] = & FFPU fpuop_do_fsinh;
							break;
						case 0x03:
							fpufunctbl[mask] = & FFPU fpuop_do_fintrz;
							break;
						case 0x04:
							fpufunctbl[mask] = & FFPU fpuop_do_fsqrt;
							break;
						case 0x06:
							fpufunctbl[mask] = & FFPU fpuop_do_flognp1;
							break;
						case 0x08:
							fpufunctbl[mask] = & FFPU fpuop_do_fetoxm1;
							break;
						case 0x09:
							fpufunctbl[mask] = & FFPU fpuop_do_ftanh;
							break;
						case 0x0a:
							fpufunctbl[mask] = & FFPU fpuop_do_fatan;
							break;
						case 0x0c:
							fpufunctbl[mask] = & FFPU fpuop_do_fasin;
							break;
						case 0x0d:
							fpufunctbl[mask] = & FFPU fpuop_do_fatanh;
							break;
						case 0x0e:
							fpufunctbl[mask] = & FFPU fpuop_do_fsin;
							break;
						case 0x0f:
							fpufunctbl[mask] = & FFPU fpuop_do_ftan;
							break;
						case 0x10:
							fpufunctbl[mask] = & FFPU fpuop_do_fetox;
							break;
						case 0x11:
							fpufunctbl[mask] = & FFPU fpuop_do_ftwotox;
							break;
						case 0x12:
							fpufunctbl[mask] = & FFPU fpuop_do_ftentox;
							break;
						case 0x14:
							fpufunctbl[mask] = & FFPU fpuop_do_flogn;
							break;
						case 0x15:
							fpufunctbl[mask] = & FFPU fpuop_do_flog10;
							break;
						case 0x16:
							fpufunctbl[mask] = & FFPU fpuop_do_flog2;
							break;
						case 0x18:
							fpufunctbl[mask] = & FFPU fpuop_do_fabs;
							break;
						case 0x19:
							fpufunctbl[mask] = & FFPU fpuop_do_fcosh;
							break;
						case 0x1a:
							fpufunctbl[mask] = & FFPU fpuop_do_fneg;
							break;
						case 0x1c:
							fpufunctbl[mask] = & FFPU fpuop_do_facos;
							break;
						case 0x1d:
							fpufunctbl[mask] = & FFPU fpuop_do_fcos;
							break;
						case 0x1e:
							fpufunctbl[mask] = & FFPU fpuop_do_fgetexp;
							break;
						case 0x1f:
							fpufunctbl[mask] = & FFPU fpuop_do_fgetman;
							break;
						case 0x20:
							fpufunctbl[mask] = & FFPU fpuop_do_fdiv;
							break;
						case 0x21:
							fpufunctbl[mask] = & FFPU fpuop_do_fmod;
							break;
						case 0x22:
							fpufunctbl[mask] = & FFPU fpuop_do_fadd;
							break;
						case 0x23:
							fpufunctbl[mask] = & FFPU fpuop_do_fmul;
							break;
						case 0x24:
							fpufunctbl[mask] = & FFPU fpuop_do_fsgldiv;
							break;
						case 0x25:
							fpufunctbl[mask] = & FFPU fpuop_do_frem;
							break;
						case 0x26:
							fpufunctbl[mask] = & FFPU fpuop_do_fscale;
							break;
						case 0x27:
							fpufunctbl[mask] = & FFPU fpuop_do_fsglmul;
							break;
						case 0x28:
							fpufunctbl[mask] = & FFPU fpuop_do_fsub;
							break;
						case 0x30:
						case 0x31:
						case 0x32:
						case 0x33:
						case 0x34:
						case 0x35:
						case 0x36:
						case 0x37:
							fpufunctbl[mask] = & FFPU fpuop_do_fsincos;
							break;
						case 0x38:
							fpufunctbl[mask] = & FFPU fpuop_do_fcmp;
							break;
						case 0x3a:
							fpufunctbl[mask] = & FFPU fpuop_do_ftst;
							break;
					}
				}
			}
		}
	}
}

/* ---------------------------- CONSTANTS ---------------------------- */

PRIVATE void FFPU set_constant ( fpu_register & f, char *name, double value, uae_s32 mult )
{
	FPU_CONSISTENCY_CHECK_START();
	if(mult == 1) {
/*		_asm {
			MOV			ESI, [f]
			FLD     QWORD PTR [value]
			FSTP    TBYTE PTR [ESI]
		} */
		__asm__ __volatile__(
			"fldl	%1\n"
				"fstpt	%0\n"
			:	"=m" (f)
			:	"m" (value)
			);
	} else {
/*		_asm {
			MOV			ESI, [f]
			FILD    DWORD PTR [mult]
			FLD     QWORD PTR [value]
			FMUL
			FSTP    TBYTE PTR [ESI]
		} */
		__asm__ __volatile__(
			"fildl	%2\n"
				"fldl	%1\n"
				"fmul	\n"
				"fstpt	%0\n"
			:	"=m" (f)
			:	"m" (value), "m" (mult)
			);
	}
	D(bug("set_constant (%s,%.04f) = %s\r\n",name,(float)value,etos(f)));
	FPU_CONSISTENCY_CHECK_STOP( mult==1 ? "set_constant(mult==1)" : "set_constant(mult>1)" );
}

PRIVATE void FFPU do_fldpi ( fpu_register & dest )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		FLDPI
		FXAM
    FNSTSW  x86_status_word
		MOV			EDI, [dest]
		FSTP    TBYTE PTR [EDI]
	} */
	__asm__ __volatile__(
			"fldpi	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (x86_status_word), "=m" (dest)
		);
	FPU_CONSISTENCY_CHECK_STOP("do_fldpi");
}

PRIVATE void FFPU do_fldlg2 ( fpu_register & dest )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		FLDLG2
		FXAM
    FNSTSW  x86_status_word
		MOV			EDI, [dest]
		FSTP    TBYTE PTR [EDI]
	} */
	__asm__ __volatile__(
			"fldlg2	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (x86_status_word), "=m" (dest)
		);
	FPU_CONSISTENCY_CHECK_STOP("do_fldlg2");
}

PRIVATE void FFPU do_fldl2e ( fpu_register & dest )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		FLDL2E
		FXAM
    FNSTSW  x86_status_word
		MOV			EDI, [dest]
		FSTP    TBYTE PTR [EDI]
	} */
	__asm__ __volatile__(
			"fldl2e	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (x86_status_word), "=m" (dest)
		);
	FPU_CONSISTENCY_CHECK_STOP("do_fldl2e");
}

PRIVATE void FFPU do_fldz ( fpu_register & dest )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		FLDZ
		FXAM
    FNSTSW  x86_status_word
		MOV			EDI, [dest]
		FSTP    TBYTE PTR [EDI]
	} */
	__asm__ __volatile__(
			"fldz	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (x86_status_word), "=m" (dest)
		);
	FPU_CONSISTENCY_CHECK_STOP("do_fldz");
}

PRIVATE void FFPU do_fldln2 ( fpu_register & dest )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		FLDLN2
		FXAM
    FNSTSW  x86_status_word
		MOV			EDI, [dest]
		FSTP    TBYTE PTR [EDI]
	} */
	__asm__ __volatile__(
			"fldln2	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (x86_status_word), "=m" (dest)
		);
	FPU_CONSISTENCY_CHECK_STOP("do_fldln2");
}

PRIVATE void FFPU do_fld1 ( fpu_register & dest )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		FLD1
		FXAM
    FNSTSW  x86_status_word
		MOV			EDI, [dest]
		FSTP    TBYTE PTR [EDI]
	} */
	__asm__ __volatile__(
			"fld1	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (x86_status_word), "=m" (dest)
		);
	FPU_CONSISTENCY_CHECK_STOP("do_fld1");
}


/* ---------------------------- MAIN INIT ---------------------------- */

#ifdef HAVE_SIGACTION
// Mega hackaround-that-happens-to-work: the following way to handle
// SIGFPE just happens to make the "fsave" below in fpu_init() *NOT*
// to abort with a floating point exception. However, we never
// actually reach sigfpe_handler().
static void sigfpe_handler(int code, siginfo_t *sip, void *)
{
	if (code == SIGFPE && sip->si_code == FPE_FLTINV) {
		fprintf(stderr, "Invalid floating point operation\n");
		abort();
	}
}
#endif

PUBLIC void FFPU fpu_init( bool integral_68040 )
{
	static bool done_first_time_initialization = false;
	if (!done_first_time_initialization) {
		fpu_init_native_fflags();
		fpu_init_native_exceptions();
		fpu_init_native_accrued_exceptions();
#ifdef HAVE_SIGACTION
		struct sigaction fpe_sa;
		sigemptyset(&fpe_sa.sa_mask);
		fpe_sa.sa_sigaction = sigfpe_handler;
		fpe_sa.sa_flags = SA_SIGINFO;
		sigaction(SIGFPE, &fpe_sa, 0);
#endif
		done_first_time_initialization = true;
	}

	__asm__ __volatile__("fsave %0" : "=m" (m_fpu_state_original));
	
	FPU is_integral = integral_68040;
	FPU instruction_address = 0;
	set_fpcr(0);
	set_fpsr(0);

	x86_control_word = CW_INITIAL; 
	x86_status_word = SW_INITIAL;
	x86_status_word_accrued = 0;
	FPU fpsr.quotient = 0;

	for( int i=0; i<8; i++ ) {
		MAKE_NAN( FPU registers[i] );
	}
	
	build_fpp_opp_lookup_table();

	__asm__ __volatile__("fninit\nfldcw %0" : : "m" (x86_control_word));

	do_fldpi( const_pi );
	do_fldlg2( const_lg2 );
	do_fldl2e( const_l2e );
	do_fldz( const_z );
	do_fldln2( const_ln2 );
	do_fld1( const_1 );

	set_constant( const_e,			"e",			exp (1.0), 1 );
	set_constant( const_log_10_e,	"Log 10 (e)",	log (exp (1.0)) / log (10.0), 1 );
	set_constant( const_ln_10,		"ln(10)",		log (10.0), 1 );
	set_constant( const_1e1,		"1.0e1",		1.0e1, 1 );
	set_constant( const_1e2,		"1.0e2",		1.0e2, 1 );
	set_constant( const_1e4,		"1.0e4",		1.0e4, 1 );
	set_constant( const_1e8,		"1.0e8",		1.0e8, 1 );
	set_constant( const_1e16,		"1.0e16",		1.0e16, 1 );
	set_constant( const_1e32,		"1.0e32",		1.0e32, 1 );
	set_constant( const_1e64,		"1.0e64",		1.0e64, 1 ) ;
	set_constant( const_1e128,		"1.0e128",		1.0e128, 1 );
	set_constant( const_1e256,		"1.0e256",		1.0e256, 1 );
	set_constant( const_1e512,		"1.0e512",		1.0e256, 10 );
	set_constant( const_1e1024,		"1.0e1024",		1.0e256, 100 );
	set_constant( const_1e2048,		"1.0e2048",		1.0e256, 1000 );
	set_constant( const_1e4096,		"1.0e4096",		1.0e256, 10000 );
	
	// Just in case.
	__asm__ __volatile__("fninit\nfldcw %0" : : "m" (x86_control_word));
}

PUBLIC void FFPU fpu_exit( void )
{
	__asm__ __volatile__("frstor %0" : : "m" (m_fpu_state_original));
}

PUBLIC void FFPU fpu_reset( void )
{
	fpu_exit();
	fpu_init(FPU is_integral);
}
