/*
 *  fpu_x86.cpp - 68881/68040 fpu code for x86/Windows an Linux/x86.
 *
 *  Basilisk II (C) 1997-2001 Christian Bauer
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
 *		Call fpu_init() and fpu_set_integral_fpu() before cpu thread starts up.
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
 *	regs.fpcr always contains the real 68881/68040 control word.
 *
 *	regs.fpsr is not kept up-to-date, for efficiency reasons.
 *	Most of the FPU commands update this in a way or another,
 *	but it is not read nearly that often. Therefore, three host-specific
 *	words hold the status byte and exception byte ("sw"), accrued exception
 *	byte ("sw_accrued") and the quotient byte ("sw_quotient"), as explained below.
 *
 *	CONDITION CODE - QUOTIENT - EXCEPTION STATUS - ACCRUED EXCEPTION
 *		CONDITION CODE (N,Z,I,NAN)
 *			updated after each opcode, if needed.
 *			x86 assembly opcodes call FXAM and store the status word to "sw".
 *			When regs.fpsr is actually used, the value of "sw" is translated.
 *		QUOTIENT BYTE
 *			Updated by frem, fmod, frestore(null frame)
 *			Stored in "sw_quotient" in correct bit position, combined when
 *			regs.fpsr is actually used.
 *		EXCEPTION STATUS (BSUN,SNAN,OPERR,OVFL,UNFL,DZ,INEX2,INEX1)
 *			updated after each opcode, if needed.
 *			Saved in x86 form in "sw".
 *			When regs.fpsr is actually used, the value of "sw" is translated.
 *			Only fcc_op can set BSUN
 *		ACCRUED EXCEPTION (ACCR_IOP,ACCR_OVFL,ACCR_UNFL,ACCR_DZ,ACCR_INEX)
 *			updated after each opcode, if needed.
 *			Logically OR'ed in x86 form to "sw_accrued".
 *			When regs.fpsr is actually used, the value of "sw_accrued" is translated.
 *		
 *		When "sw" and "sw_accrued" are stored, all pending x86 FPU
 *		exceptions are cleared, if there are any.
 *
 *		Writing to "regs.fpsr" reverse-maps to x86 status/exception values
 *		and stores the values in "sw", "sw_accrued" and "sw_quotient".
 *
 *		So, "sw" and "sw_accrued" are not in correct bit positions
 *		and have x86 values, but "sw_quotient" is at correct position.
 *
 *		Note that it does not matter that the reverse-mapping is not exact
 *		(both SW_IE and SW_DE are mapped to ACCR_IOP, but ACCR_IOP maps to SW_IE
 *		only), the MacOS always sees the correct exception bits.
 *
 *		Also note the usage of the fake BSUN flag SW_FAKE_BSUN. If you change the
 *		x86 FPU code, you must make sure that you don't generate any FPU stack faults.
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

#include "sysdeps.h"
#include "memory.h"
#include "readcpu.h"
#include "newcpu.h"
#include "fpu/fpu_x86.h"
#include "fpu/fpu_x86_asm.h"

/* ---------------------------- Compatibility ---------------------------- */

#define BYTE		uint8
#define WORD		uint16
#define DWORD		uint32
#define _ASM		__asm__ __volatile__
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
		DEBUG						1 or 0
		USE_CONSISTENCY_CHECKING	0
		I3_ON_ILLEGAL_FPU_OP		0 -- and this won't change
		I3_ON_FTRAPCC				0 -- and this won't change
*/
#define USE_3_BIT_QUOTIENT			1

#define DEBUG						0
#define USE_CONSISTENCY_CHECKING	0

#define I3_ON_ILLEGAL_FPU_OP		0
#define I3_ON_FTRAPCC				0


/* ---------------------------- Registers ---------------------------- */

// "regs.fp" is not used. regs.fpcr, regs.fpsr and regs.fpiar are used.

typedef BYTE *float80;
typedef BYTE float80_s[10];
typedef BYTE float80_s_aligned[16];

static float80_s_aligned fp_reg[8];


/* ---------------------------- Debugging ---------------------------- */

#if DEBUG
//#pragma optimize("",off)
#endif

// extern "C" {int debugging_fpp = 0;}

#include "debug.h"

#if DEBUG
#undef __inline__
#define __inline__
//#undef D
//#define D(x) { if(debugging_fpp) { (x); } }

static void dump_first_bytes_buf( char *b, BYTE *buf, int32 actual )
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

static void dump_first_bytes( BYTE *buf, int32 actual )
{
	char msg[256];
	dump_first_bytes_buf( msg, buf, actual );
	D(bug("%s\n", msg));
}

#define CONDRET(s,x) D(bug("fpp_cond %s = %d\r\n",s,(uint32)(x))); return (x)
static char * etos( float80 e ) REGPARAM;
static char * etos( float80 e )
{
	static char _s[10][30];
	static int _ix = 0;
	float f;

/*	_asm {
		MOV			EDI, [e]
		FLD     TBYTE PTR [EDI]
    FSTP    DWORD PTR f
	} */
	
	_ASM(	"fldt	%1\n"
			"fstp	%0\n"
		:	"=m" (f)
		:	"m" (*e)
		);
	
	if(++_ix >= 10) _ix = 0;

	sprintf( _s[_ix], "%.04f", (float)f );
	return( _s[_ix] );
}
static void dump_fp_regs( char *s )
{
	char b[512];

	sprintf(
		b,
		"%s: %s, %s, %s, %s, %s, %s, %s, %s\r\n",
		s,
		etos(fp_reg[0]),
		etos(fp_reg[1]),
		etos(fp_reg[2]),
		etos(fp_reg[3]),
		etos(fp_reg[4]),
		etos(fp_reg[5]),
		etos(fp_reg[6]),
		etos(fp_reg[7])
	);
	D(bug((char*)b));
}

#else
#define CONDRET(s,x) return (x)
#define dump_fp_regs(s) {}
#endif


/* ---------------------------- FPU consistency ---------------------------- */

#if USE_CONSISTENCY_CHECKING
static uae_u16 checked_sw_atstart;

static void FPU_CONSISTENCY_CHECK_START()
{
/*	_asm {
	  FNSTSW checked_sw_atstart
	} */
	_ASM("fnstsw %0" : "=m" (checked_sw_atstart));
}

static void FPU_CONSISTENCY_CHECK_STOP(char *name)
{
	uae_u16 checked_sw_atend;
//	_asm FNSTSW checked_sw_atend
	_ASM("fnstsw %0" : "=m" (checked_sw_attend));
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
			msg, "Op %s, sw before=%X, sw after=%X\r\n", 
			name, (int)checked_sw_atstart, (int)checked_sw_atend
		);
		OutputDebugString(msg);
	}
	*/
}
#else
#define FPU_CONSISTENCY_CHECK_START()
#define FPU_CONSISTENCY_CHECK_STOP(name)
#endif


/* ---------------------------- FPU type ---------------------------- */

static uae_u32 is_integral_68040_fpu = 0;


/* ---------------------------- Status byte ---------------------------- */

// Extend the SW_* codes
#define SW_FAKE_BSUN			SW_SF

// Shorthand
#define SW_EXCEPTION_MASK (SW_ES|SW_SF|SW_PE|SW_UE|SW_OE|SW_ZE|SW_DE|SW_IE)
// #define SW_EXCEPTION_MASK (SW_SF|SW_PE|SW_UE|SW_OE|SW_ZE|SW_DE|SW_IE)

// Map x86 FXAM codes -> m68k fpu status byte
#define SW_Z_I_NAN_MASK			(SW_C0|SW_C2|SW_C3)
#define SW_Z					(SW_C3)
#define SW_I					(SW_C0|SW_C2)
#define SW_NAN					(SW_C0)
#define SW_FINITE				(SW_C2)
#define SW_EMPTY_REGISTER		(SW_C0|SW_C3)
#define SW_DENORMAL				(SW_C2|SW_C3)
#define SW_UNSUPPORTED			(0)
#define SW_N					(SW_C1)

// Initial state after boot, reset and frestore(null frame)
#define SW_INITIAL				SW_FINITE

// These hold the contents of the fpsr, in Intel values.
static DWORD sw = SW_INITIAL;
static DWORD sw_accrued = 0;
static DWORD sw_quotient = 0;


/* ---------------------------- Control word ---------------------------- */

// Initial state after boot, reset and frestore(null frame)
#define CW_INITIAL	(CW_RESET|CW_X|CW_PC_EXTENDED|CW_RC_NEAR|CW_PM|CW_UM|CW_OM|CW_ZM|CW_DM|CW_IM)

static WORD cw = CW_INITIAL; 


/* ---------------------------- FMOVECR constants ---------------------------- */

static float80_s
	// Suported by x86 FPU
	const_pi,
	const_lg2,
	const_l2e,
	const_z,
	const_ln2,
	const_1,

	// Not suported by x86 FPU
	const_e,
	const_log_10_e,
	const_ln_10,
	const_1e1,
	const_1e2,
	const_1e4,
	const_1e8,
	const_1e16,
	const_1e32,
	const_1e64,
	const_1e128,
	const_1e256,
	const_1e512,
	const_1e1024,
	const_1e2048,
	const_1e4096;


/* ---------------------------- Saved host FPU state ---------------------------- */

static BYTE m_fpu_state_original[108]; // 90/94/108


/* ---------------------------- Map tables ---------------------------- */

typedef void REGPARAM2 fpuop_func( uae_u32, uae_u16 );
extern "C" { fpuop_func *fpufunctbl[65536]; }

// Control word -- need only one-way mapping
static const uae_u16 cw_rc_mac2host[] = {
	CW_RC_NEAR,
	CW_RC_ZERO,
	CW_RC_DOWN,
	CW_RC_UP
};
static const uae_u16 cw_pc_mac2host[] = {
	CW_PC_EXTENDED,
	CW_PC_SINGLE,
	CW_PC_DOUBLE,
	CW_PC_RESERVED
};

// Status word -- need two-way mapping
static uae_u32 sw_cond_host2mac[ 0x48 ];
static uae_u16 sw_cond_mac2host[ 16 ];

static uae_u32 exception_host2mac[ 0x80 ];
static uae_u32 exception_mac2host[ 0x100 ];

static uae_u32 accrued_exception_host2mac[ 0x40 ];
static uae_u32 accrued_exception_mac2host[ 0x20 ];


/* ---------------------------- Control functions ---------------------------- */

/*
	Exception enable byte is ignored, but the same value is returned
	that was previously set.
*/
static void __inline__ set_host_fpu_control_word()
{
	cw = (cw & ~(X86_ROUND_CONTROL_MASK|X86_PRECISION_CONTROL_MASK)) | 
				cw_rc_mac2host[ (regs.fpcr & ROUND_CONTROL_MASK) >> 4 ] |
				cw_pc_mac2host[ (regs.fpcr & PRECISION_CONTROL_MASK) >> 6 ];
	
	// Writing to control register is very slow (no register renaming on
	// ppro++, and most of all, it's one of those serializing commands).
/*	_asm {
    FLDCW   cw
	} */
	_ASM("fldcw %0" : : "m" (cw));
}


/* ---------------------------- Status functions ---------------------------- */

static void __inline__ SET_BSUN_ON_NAN()
{
	if( (sw & (SW_Z_I_NAN_MASK)) == SW_NAN ) {
		sw |= SW_FAKE_BSUN;
		sw_accrued |= SW_IE;
	}
}

static void __inline__ build_ex_status()
{
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw_accrued |= sw;
	}
}

static void __inline__ to_fpsr()
{
	regs.fpsr = 
			sw_cond_host2mac[ (sw & 0x4700) >> 8 ] |
			sw_quotient |
			exception_host2mac[ sw & (SW_FAKE_BSUN|SW_PE|SW_UE|SW_OE|SW_ZE|SW_DE|SW_IE) ] |
			accrued_exception_host2mac[ sw_accrued & (SW_PE|SW_UE|SW_OE|SW_ZE|SW_DE|SW_IE) ]
			;
}

static void __inline__ from_fpsr()
{
	sw = 
			sw_cond_mac2host[ (regs.fpsr & 0x0F000000) >> 24 ] |
			exception_mac2host[ (regs.fpsr & 0x0000FF00) >> 8 ];
	sw_quotient = regs.fpsr & 0x00FF0000;
	sw_accrued =  accrued_exception_mac2host[ (regs.fpsr & 0xF8) >> 3 ];
}


// TODO_BIGENDIAN; all of these.
/* ---------------------------- Type functions ---------------------------- */

/*
When the FPU creates a NAN, the NAN always contains the same bit pattern
in the mantissa. All bits of the mantissa are ones for any precision.
When the user creates a NAN, any nonzero bit pattern can be stored in the mantissa.
*/
static __inline__ void MAKE_NAN(float80 p)
{
	// Make it non-signaling.
	memset( p, 0xFF, sizeof(float80_s)-1 );
	p[9] = 0x7F;
}

/*
For single- and double-precision infinities the fraction is a zero.
For extended-precision infinities, the mantissa’s MSB, the explicit
integer bit, can be either one or zero.
*/
static __inline__ uae_u32 IS_INFINITY(float80 p)
{
	if( ((p[9] & 0x7F) == 0x7F) && p[8] == 0xFF ) {
		if( *((uae_u32 *)p) == 0 &&
				( *((uae_u32 *)&p[4]) & 0x7FFFFFFF ) == 0 )
		{
			return(1);
		}
	}
	return(0);
}

static __inline__ uae_u32 IS_NAN(float80 p)
{
	if( ((p[9] & 0x7F) == 0x7F) && p[8] == 0xFF ) {
		if( *((uae_u32 *)p) != 0 ||
				( *((uae_u32 *)&p[4]) & 0x7FFFFFFF ) != 0 )
		{
			return(1);
		}
	}
	return(0);
}

static __inline__ uae_u32 IS_ZERO(float80 p)
{
	return *((uae_u32 *)p) == 0 &&
				 *((uae_u32 *)&p[4]) == 0 &&
				 ( *((uae_u16 *)&p[8]) & 0x7FFF ) == 0;
}

static __inline__ void MAKE_INF_POSITIVE(float80 p)
{
	memset( p, 0, sizeof(float80_s)-2 );
	*((uae_u16 *)&p[8]) = 0x7FFF;
}

static __inline__ void MAKE_INF_NEGATIVE(float80 p)
{
	memset( p, 0, sizeof(float80_s)-2 );
	*((uae_u16 *)&p[8]) = 0xFFFF;
}

static __inline__ void MAKE_ZERO_POSITIVE(float80 p)
{
	memset( p, 0, sizeof(float80_s) );
}

static __inline__ void MAKE_ZERO_NEGATIVE(float80 *p)
{
	memset( p, 0, sizeof(float80_s) );
	*((uae_u32 *)&p[4]) = 0x80000000;
}

static __inline__ uae_u32 IS_NEGATIVE(float80 p)
{
	return( (p[9] & 0x80) != 0 );
}


/* ---------------------------- Conversions ---------------------------- */

static void signed_to_extended( uae_s32 x, float80 f ) REGPARAM;
static void signed_to_extended( uae_s32 x, float80 f )
{
	FPU_CONSISTENCY_CHECK_START();
	
/*	_asm {
		MOV			ESI, [f]
    FILD    DWORD PTR [x]
		FSTP    TBYTE PTR [ESI]
	} */
	
	_ASM(	"fildl	%1\n"
			"fstpt	%0\n"
		:	"=m" (*f)
		:	"m" (x)
		);
	
	D(bug("signed_to_extended (%X) = %s\r\n",(int)x,etos(f)));
	FPU_CONSISTENCY_CHECK_STOP("signed_to_extended");
}

static uae_s32 extended_to_signed_32( float80 f ) REGPARAM;
static uae_s32 extended_to_signed_32( float80 f )
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

	_ASM(	"fldt	%2\n"
			"fistpl	%0\n"
			"fnstsw	%1\n"
		:	"=m" (tmp), "=m" (sw_temp)
		:	"m" (*f)
		);
	
	if(sw_temp & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		if(sw_temp & (SW_OE|SW_UE|SW_DE|SW_IE)) { // Map SW_OE to OPERR.
			sw |= SW_IE;
			sw_accrued |= SW_IE;
			// Setting the value to zero might not be the right way to go,
			// but I'll leave it like this for now.
			tmp = 0;
		}
		if(sw_temp & SW_PE) {
			sw |= SW_PE;
			sw_accrued |= SW_PE;
		}
	}

	D(bug("extended_to_signed_32 (%s) = %X\r\n",etos(f),(int)tmp));
	FPU_CONSISTENCY_CHECK_STOP("extended_to_signed_32");
	return tmp;
}

static uae_s16 extended_to_signed_16( float80 f ) REGPARAM;
static uae_s16 extended_to_signed_16( float80 f )
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
	
	_ASM(	"fldt	%2\n"
			"fistp	%0\n"
			"fnstsw	%1\n"
		:	"=m" (tmp), "=m" (sw_temp)
		:	"m" (*f)
		);

	if(sw_temp & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		if(sw_temp & (SW_OE|SW_UE|SW_DE|SW_IE)) { // Map SW_OE to OPERR.
			sw |= SW_IE;
			sw_accrued |= SW_IE;
			tmp = 0;
		}
		if(sw_temp & SW_PE) {
			sw |= SW_PE;
			sw_accrued |= SW_PE;
		}
	}

	D(bug("extended_to_signed_16 (%s) = %X\r\n",etos(f),(int)tmp));
	FPU_CONSISTENCY_CHECK_STOP("extended_to_signed_16");
	return tmp;
}

static uae_s8 extended_to_signed_8( float80 f ) REGPARAM;
static uae_s8 extended_to_signed_8( float80 f )
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
	
	_ASM(	"fldt	%2\n"
			"fistp	%0\n"
			"fnstsw	%1\n"
		:	"=m" (tmp), "=m" (sw_temp)
		:	"m" (*f)
		);

	if(sw_temp & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		if(sw_temp & (SW_OE|SW_UE|SW_DE|SW_IE)) { // Map SW_OE to OPERR.
			sw |= SW_IE;
			sw_accrued |= SW_IE;
			tmp = 0;
		}
		if(sw_temp & SW_PE) {
			sw |= SW_PE;
			sw_accrued |= SW_PE;
		}
	}

	if(tmp > 127 || tmp < -128) { // OPERR
		sw |= SW_IE;
		sw_accrued |= SW_IE;
	}

	D(bug("extended_to_signed_8 (%s) = %X\r\n",etos(f),(int)tmp));
	FPU_CONSISTENCY_CHECK_STOP("extended_to_signed_8");
	return (uae_s8)tmp;
}

static void double_to_extended( double x, float80 f ) REGPARAM;
static void double_to_extended( double x, float80 f )
{
	FPU_CONSISTENCY_CHECK_START();

/*	_asm {
		MOV			EDI, [f]
    FLD     QWORD PTR [x]
		FSTP    TBYTE PTR [EDI]
	} */
	
	_ASM(	"fldl	%1\n"
			"fstpt	%0\n"
		:	"=m" (*f)
		:	"m" (x)
		);
	
	FPU_CONSISTENCY_CHECK_STOP("double_to_extended");
}

static double extended_to_double( float80 f ) REGPARAM;
static double extended_to_double( float80 f )
{
	FPU_CONSISTENCY_CHECK_START();
	volatile double result;

/*	_asm {
		MOV			ESI, [f]
		FLD     TBYTE PTR [ESI]
    FSTP    QWORD PTR result
	} */
	
	_ASM(	"fldt	%1\n"
			"fstpl	%0\n"
		:	"=m" (result)
		:	"m" (*f)
		);
	
	FPU_CONSISTENCY_CHECK_STOP("extended_to_double");
	return result;
}

static void to_single( uae_u32 src, float80 f ) REGPARAM;
static void to_single( uae_u32 src, float80 f )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [f]
    FLD     DWORD PTR src
		FSTP    TBYTE PTR [ESI]
	} */
	
	_ASM(	"flds	%1\n"
			"fstpt	%0\n"
		:	"=m" (*f)
		:	"m" (src)
		);

	D(bug("to_single (%X) = %s\r\n",src,etos(f)));
	FPU_CONSISTENCY_CHECK_STOP("to_single");
}

// TODO_BIGENDIAN
static void to_exten_no_normalize( uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3, float80 f ) REGPARAM;
static void to_exten_no_normalize( uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3, float80 f )
{
	FPU_CONSISTENCY_CHECK_START();
	uae_u32 *p = (uae_u32 *)f;

	uae_u32 sign =  (wrd1 & 0x80000000) >> 16;
	uae_u32 exp  = (wrd1 >> 16) & 0x7fff;
	p[0] = wrd3;
	p[1] = wrd2;
	*((uae_u16 *)&p[2]) = (uae_u16)(sign | exp);

	D(bug("to_exten_no_normalize (%X,%X,%X) = %s\r\n",wrd1,wrd2,wrd3,etos(f)));
	FPU_CONSISTENCY_CHECK_STOP("to_exten_no_normalize");
}

static void to_exten( uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3, float80 f ) REGPARAM;
static void to_exten( uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3, float80 f )
{
	FPU_CONSISTENCY_CHECK_START();
	uae_u32 *p = (uae_u32 *)f;

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

static void to_double( uae_u32 wrd1, uae_u32 wrd2, float80 f ) REGPARAM;
static void to_double( uae_u32 wrd1, uae_u32 wrd2, float80 f )
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
	
	_ASM(	"fldl	%1\n"
			"fstpt	%0\n"
		:	"=m" (*f)
		:	"m" (src.q)
		);

	D(bug("to_double (%X,%X) = %s\r\n",wrd1,wrd2,etos(f)));
	FPU_CONSISTENCY_CHECK_STOP("to_double");
}

static uae_u32 from_single( float80 f ) REGPARAM;
static uae_u32 from_single( float80 f )
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
	
	_ASM(	"fldt	%2\n"
			"fstps	%0\n"
			"fnstsw	%1\n"
		:	"=m" (dest), "=m" (sw_temp)
		:	"m" (*f)
		);

	sw_temp &= SW_EXCEPTION_MASK;
	if(sw_temp) {
//		_asm FNCLEX
		asm("fnclex");
		sw = (sw & ~SW_EXCEPTION_MASK) | sw_temp;
		sw_accrued |= sw_temp;
	}

	D(bug("from_single (%s) = %X\r\n",etos(f),dest));
	FPU_CONSISTENCY_CHECK_STOP("from_single");
	return dest;
}

// TODO_BIGENDIAN
static void from_exten( float80 f, uae_u32 *wrd1, uae_u32 *wrd2, uae_u32 *wrd3 ) REGPARAM;
static void from_exten( float80 f, uae_u32 *wrd1, uae_u32 *wrd2, uae_u32 *wrd3 )
{
	FPU_CONSISTENCY_CHECK_START();
	uae_u32 *p = (uae_u32 *)f;
	*wrd3 = p[0];
	*wrd2 = p[1];
	*wrd1 = ( (uae_u32)*((uae_u16 *)&p[2]) ) << 16;

	D(bug("from_exten (%s) = %X,%X,%X\r\n",etos(f),*wrd1,*wrd2,*wrd3));
	FPU_CONSISTENCY_CHECK_STOP("from_exten");
}

static void from_double( float80 f, uae_u32 *wrd1, uae_u32 *wrd2 ) REGPARAM;
static void from_double( float80 f, uae_u32 *wrd1, uae_u32 *wrd2 )
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
	
	_ASM(	"fldt	%2\n"
			"fstpl	%0\n"
			"fnstsw	%1\n"
		:	"=m" (dest), "=m" (sw_temp)
		:	"m" (*f)
		);

	sw_temp &= SW_EXCEPTION_MASK;
	if(sw_temp) {
//		_asm FNCLEX
		asm("fnclex");
		sw = (sw & ~SW_EXCEPTION_MASK) | sw_temp;
		sw_accrued |= sw_temp;
	}

	// TODO: There is a partial memory stall, nothing happens until FSTP retires.
	// On PIII, could use MMX move w/o any penalty.
	*wrd2 = dest[0];
	*wrd1 = dest[1];

	D(bug("from_double (%s) = %X,%X\r\n",etos(f),dest[1],dest[0]));
	FPU_CONSISTENCY_CHECK_STOP("from_double");
}

static void do_fmove( float80 dest, float80 src ) REGPARAM;
static void do_fmove( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
		FXAM
    FNSTSW  sw
		FSTP    TBYTE PTR [EDI]
	} */
	
	_ASM(	"fldt	%2\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (sw), "=m" (*dest)
		:	"m" (*src)
		);
	FPU_CONSISTENCY_CHECK_STOP("do_fmove");
}

/*
static void do_fmove_no_status( float80 dest, float80 src ) REGPARAM;
static void do_fmove_no_status( float80 dest, float80 src )
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

static void do_fint( float80 dest, float80 src ) REGPARAM;
static void do_fint( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
		FRNDINT
		FXAM
    FNSTSW  sw
		FSTP    TBYTE PTR [EDI]
	} */
	_ASM(	"fldt	%2\n"
			"frndint\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (sw), "=m" (*dest)
		:	"m" (*src)
		);
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw &= ~(SW_EXCEPTION_MASK - SW_PE);
		sw_accrued |= sw;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fint");
}

static void do_fintrz( float80 dest, float80 src ) REGPARAM;
static void do_fintrz( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
	volatile WORD cw_temp;

/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
		FSTCW   cw_temp
		and			cw_temp, ~X86_ROUND_CONTROL_MASK
		or			cw_temp, CW_RC_ZERO
    FLDCW   cw_temp
    FLD     TBYTE PTR [ESI]
		FRNDINT
		FXAM
    FNSTSW  sw
    FLDCW   cw
		FSTP    TBYTE PTR [EDI]
	} */
	
	_ASM(	"fstcw	%0\n"
			"andl	$(~X86_ROUND_CONTROL_MASK), %0\n"
			"orl	$CW_RC_ZERO, %0\n"
			"fldcw	%0\n"
			"fldt	%3\n"
			"frndint\n"
			"fxam	\n"
			"fnstsw	%1\n"
			"fldcw	%4\n"
			"fstpt	%2\n"
		:	"+m" (cw_temp), "=m" (sw), "=m" (*dest)
		:	"m" (*src), "m" (cw)
		);
	
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw &= ~(SW_EXCEPTION_MASK - SW_PE);
		sw_accrued |= sw;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fintrz");
}

static void do_fsqrt( float80 dest, float80 src ) REGPARAM;
static void do_fsqrt( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
		FSQRT
		FXAM
    FNSTSW  sw
		FSTP    TBYTE PTR [EDI]
	} */
	
	_ASM(	"fldt	%2\n"
			"fsqrt	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (sw), "=m" (*dest)
		:	"m" (*src)
		);
	
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw &= ~(SW_EXCEPTION_MASK - SW_IE - SW_PE);
		sw_accrued |= sw;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fsqrt");
}

static void do_ftst( float80 src ) REGPARAM;
static void do_ftst( float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
    FLD     TBYTE PTR [ESI]
		FXAM
    FNSTSW  sw
		FSTP    ST(0)
	} */
	
	_ASM(	"fldt	%1\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstp	%%st(0)\n"
		:	"=m" (sw)
		:	"m" (*src)
		);
	
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw &= ~SW_EXCEPTION_MASK;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_ftst");
}

// These functions are calculated in 53 bits accuracy only.
// Exception checking is not complete.
static void do_fsinh( float80 dest, float80 src ) REGPARAM;
static void do_fsinh( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = sinh(x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_fsinh");
}

static void do_flognp1( float80 dest, float80 src ) REGPARAM;
static void do_flognp1( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = log (x + 1.0);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_flognp1");
}

static void do_fetoxm1( float80 dest, float80 src ) REGPARAM;
static void do_fetoxm1( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = exp (x) - 1.0;
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_fetoxm1");
}

static void do_ftanh( float80 dest, float80 src ) REGPARAM;
static void do_ftanh( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = tanh (x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_ftanh");
}

static void do_fatan( float80 dest, float80 src ) REGPARAM;
static void do_fatan( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = atan (x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_fatan");
}

static void do_fasin( float80 dest, float80 src ) REGPARAM;
static void do_fasin( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = asin (x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_fasin");
}

static void do_fatanh( float80 dest, float80 src ) REGPARAM;
static void do_fatanh( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = log ((1 + x) / (1 - x)) / 2;
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_fatanh");
}

static void do_fetox( float80 dest, float80 src ) REGPARAM;
static void do_fetox( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = exp (x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_fetox");
}

static void do_ftwotox( float80 dest, float80 src ) REGPARAM;
static void do_ftwotox( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = pow(2.0, x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_ftwotox");
}

static void do_ftentox( float80 dest, float80 src ) REGPARAM;
static void do_ftentox( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = pow(10.0, x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_ftentox");
}

static void do_flogn( float80 dest, float80 src ) REGPARAM;
static void do_flogn( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = log (x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_flogn");
}

static void do_flog10( float80 dest, float80 src ) REGPARAM;
static void do_flog10( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = log10 (x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_flog10");
}

static void do_flog2( float80 dest, float80 src ) REGPARAM;
static void do_flog2( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = log (x) / log (2.0);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_flog2");
}

static void do_facos( float80 dest, float80 src ) REGPARAM;
static void do_facos( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = acos(x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_facos");
}

static void do_fcosh( float80 dest, float80 src ) REGPARAM;
static void do_fcosh( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
	double x, y;
	x = extended_to_double( src );
	y = cosh(x);
	double_to_extended( y, dest );
	do_ftst( dest );
	FPU_CONSISTENCY_CHECK_STOP("do_fcosh");
}

static void do_fsin( float80 dest, float80 src ) REGPARAM;
static void do_fsin( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
		FSIN
		FXAM
    FNSTSW  sw
		FSTP    TBYTE PTR [EDI]
	} */
	_ASM(	"fldt	%2\n"
			"fsin	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (sw), "=m" (*dest)
		:	"m" (*src)
		);
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw &= ~(SW_EXCEPTION_MASK - SW_IE - SW_PE);
		sw_accrued |= sw;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fsin");
}

// TODO: Should check for out-of-range condition (partial tangent)
static void do_ftan( float80 dest, float80 src ) REGPARAM;
static void do_ftan( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
		FPTAN
		FSTP    ST(0)	; pop 1.0 (the 8087/287 compatibility thing)
		FXAM
    FNSTSW  sw
		FSTP    TBYTE PTR [EDI]
	} */
	_ASM(	"fldt	%2\n"
			"fptan	\n"
			"fstp	%%st(0)\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (sw), "=m" (*dest)
		:	"m" (*src)
		);
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw &= ~(SW_EXCEPTION_MASK - SW_IE - SW_PE - SW_UE);
		sw_accrued |= sw;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_ftan");
}

static void do_fabs( float80 dest, float80 src ) REGPARAM;
static void do_fabs( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
		FABS
		FXAM
    FNSTSW  sw
		FSTP    TBYTE PTR [EDI]
	} */
	_ASM(	"fldt	%2\n"
			"fabs	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (sw), "=m" (*dest)
		:	"m" (*src)
		);
	// x86 fabs should not rise any exceptions (except stack underflow)
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw &= ~SW_EXCEPTION_MASK;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fabs");
}

static void do_fneg( float80 dest, float80 src ) REGPARAM;
static void do_fneg( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
		FCHS
		FXAM
    FNSTSW  sw
		FSTP    TBYTE PTR [EDI]
	} */
	_ASM(	"fldt	%2\n"
			"fchs	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (sw), "=m" (*dest)
		:	"m" (*src)
		);
	// x86 fchs should not rise any exceptions (except stack underflow)
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw &= ~SW_EXCEPTION_MASK;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fneg");
}

static void do_fcos( float80 dest, float80 src ) REGPARAM;
static void do_fcos( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
		FCOS
		FXAM
    FNSTSW  sw
		FSTP    TBYTE PTR [EDI]
	} */
	_ASM(	"fldt	%2\n"
			"fcos	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (sw), "=m" (*dest)
		:	"m" (*src)
		);
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw &= ~(SW_EXCEPTION_MASK - SW_IE - SW_PE);
		sw_accrued |= sw;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fcos");
}

static void do_fgetexp( float80 dest, float80 src ) REGPARAM;
static void do_fgetexp( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
		FXTRACT
		FSTP    ST(0)						; pop mantissa
		FXAM
    FNSTSW  sw
		FSTP    TBYTE PTR [EDI]
	} */
	_ASM(	"fldt	%2\n"
			"fxtract\n"
			"fstp	%%st(0)\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (sw), "=m" (*dest)
		:	"m" (*src)
		);
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw &= ~SW_EXCEPTION_MASK;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fgetexp");
}

static void do_fgetman( float80 dest, float80 src ) REGPARAM;
static void do_fgetman( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
		FXTRACT
		FXAM
    FNSTSW  sw
		FSTP    TBYTE PTR [EDI]
		FSTP    ST(0)						; pop exponent
	} */
	_ASM(	"fldt	%2\n"
			"fxtract\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
			"fstp	%%st(0)\n"
		:	"=m" (sw), "=m" (*dest)
		:	"m" (*src)
		);
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw &= ~SW_EXCEPTION_MASK;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fgetman");
}

static void do_fdiv( float80 dest, float80 src ) REGPARAM;
static void do_fdiv( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
    FLD     TBYTE PTR [EDI]
		FDIV		ST(0),ST(1)
		FXAM
    FNSTSW  sw
		FSTP    TBYTE PTR [EDI]
		FSTP    ST(0)
	} */
	_ASM(	"fldt	%2\n"
			"fldt	%1\n"
			"fdiv	%%st(1), %%st(0)\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
			"fstp	%%st(0)\n"
		:	"=m" (sw), "+m" (*dest)
		:	"m" (*src)
		);
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw_accrued |= sw;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fdiv");
}

// The sign of the quotient is the exclusive-OR of the sign bits
// of the source and destination operands.
// Quotient Byte is loaded with the sign and least significant
// seven bits of the quotient.

static void do_fmod( float80 dest, float80 src ) REGPARAM;
static void do_fmod( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();

	volatile uint16 status;
	uae_u32 quot;
#if !USE_3_BIT_QUOTIENT
	WORD cw_temp;
#endif

	uae_u32 sign = (dest[9] ^ src[9]) & 0x80;

/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]

#if !USE_3_BIT_QUOTIENT
		MOV			CX, cw
		AND			CX, ~X86_ROUND_CONTROL_MASK
		OR			CX, CW_RC_ZERO
		MOV			cw_temp, CX
    FLDCW   cw_temp

		FLD     TBYTE PTR [ESI]
		FLD     TBYTE PTR [EDI]
		FDIV		ST(0),ST(1)
		FABS
    FISTP   DWORD PTR quot
		FSTP    ST(0)
    FLDCW   cw
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
    FNSTSW  sw

		FSTP    TBYTE PTR [EDI]
		FSTP    ST(0)
	} */
	
#if !USE_3_BIT_QUOTIENT
	
	_ASM(	"movl	%6, %%ecx\n"	// %6: cw		(read)
			"andl	$(~X86_ROUND_CONTROL_MASK), %%ecx\n"
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
			"fnstsw	%3\n"			// %3: sw		(write)
			"fstpt	%4\n"
			"fstp	%%st(0)\n"
		:	"+m" (cw_temp), "+m" (quot), "+m" (status), "=m" (sw), "+m" (*dest)
		:	"m" (*src), "m" (cw)
		:	"ecx"
		);
	
#else
	
	_ASM(	"fldt	%3\n"
			"fldt	%2\n"
			"0:\n"					// partial_loop
			"fprem	\n"
			"fnstsw	%0\n"			// %0: status	(read/write)
			"testl	$SW_C2, %0\n"
			"jne	0b\n"
			"fxam	\n"
			"fnstsw	%1\n"			// %1: sw		(write)
			"fstpt	%2\n"
			"fstp	%%st(0)\n"
		:	"+m" (status), "=m" (sw), "+m" (*dest)
		:	"m" (*src)
		);
	
#endif
	
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw &= ~(SW_EXCEPTION_MASK - SW_IE - SW_UE);
		sw_accrued |= sw;
	}

#if USE_3_BIT_QUOTIENT
	// SW_C1 Set to least significant bit of quotient (Q0).
	// SW_C3 Set to bit 1 (Q1) of the quotient.
	// SW_C0 Set to bit 2 (Q2) of the quotient.
	quot = ((status & SW_C0) >> 6) | ((status & SW_C3) >> 13) | ((status & SW_C1) >> 9);
	sw_quotient = (sign | quot) << 16;
#else
	sw_quotient = (sign | (quot&0x7F)) << 16;
#endif

	FPU_CONSISTENCY_CHECK_STOP("do_fmod");
}

static void do_frem( float80 dest, float80 src ) REGPARAM;
static void do_frem( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();

	volatile uint16 status;
	uae_u32 quot;
#if !USE_3_BIT_QUOTIENT
	WORD cw_temp;
#endif

	uae_u32 sign = (dest[9] ^ src[9]) & 0x80;

/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]

#if !USE_3_BIT_QUOTIENT
		MOV			CX, cw
		AND			CX, ~X86_ROUND_CONTROL_MASK
		OR			CX, CW_RC_NEAR
		MOV			cw_temp, CX
    FLDCW   cw_temp

		FLD     TBYTE PTR [ESI]
		FLD     TBYTE PTR [EDI]
		FDIV		ST(0),ST(1)
		FABS
    FISTP   DWORD PTR quot
		FSTP    ST(0)
    FLDCW   cw
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
    FNSTSW  sw
		FSTP    TBYTE PTR [EDI]
		FSTP    ST(0)
	} */

#if !USE_3_BIT_QUOTIENT
	
	_ASM(	"movl	%6, %%ecx\n"	// %6: cw		(read)
			"andl	$(~X86_ROUND_CONTROL_MASK), %%ecx\n"
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
			"fnstsw	%3\n"			// %3: sw		(write)
			"fstpt	%4\n"
			"fstp	%%st(0)\n"
		:	"+m" (cw_temp), "+m" (quot), "+m" (status), "=m" (sw), "+m" (*dest)
		:	"m" (*src), "m" (cw)
		:	"ecx"
		);
	
#else
	
	_ASM(	"fldt	%3\n"
			"fldt	%2\n"
			"0:\n"					// partial_loop
			"fprem1	\n"
			"fnstsw	%0\n"			// %0: status	(read/write)
			"testl	$SW_C2, %0\n"
			"jne	0b\n"
			"fxam	\n"
			"fnstsw	%1\n"			// %1: sw		(write)
			"fstpt	%2\n"
			"fstp	%%st(0)\n"
		:	"+m" (status), "=m" (sw), "+m" (*dest)
		:	"m" (*src)
		);
	
#endif
	
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw &= ~(SW_EXCEPTION_MASK - SW_IE - SW_UE);
		sw_accrued |= sw;
	}

#if USE_3_BIT_QUOTIENT
	// SW_C1 Set to least significant bit of quotient (Q0).
	// SW_C3 Set to bit 1 (Q1) of the quotient.
	// SW_C0 Set to bit 2 (Q2) of the quotient.
	quot = ((status & SW_C0) >> 6) | ((status & SW_C3) >> 13) | ((status & SW_C1) >> 9);
	sw_quotient = (sign | quot) << 16;
#else
	sw_quotient = (sign | (quot&0x7F)) << 16;
#endif

	FPU_CONSISTENCY_CHECK_STOP("do_frem");
}

// Faster versions. The current rounding mode is already correct.
#if !USE_3_BIT_QUOTIENT
static void do_fmod_dont_set_cw( float80 dest, float80 src ) REGPARAM;
static void do_fmod_dont_set_cw( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();

	volatile uint16 status;
	uae_u32 quot;

	uae_u32 sign = (dest[9] ^ src[9]) & 0x80;

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
    FNSTSW  sw

		FSTP    TBYTE PTR [EDI]
		FSTP    ST(0)
	}
	if(sw & SW_EXCEPTION_MASK) {
		_asm FNCLEX
		sw &= ~(SW_EXCEPTION_MASK - SW_IE - SW_UE);
		sw_accrued |= sw;
	}
	sw_quotient = (sign | (quot&0x7F)) << 16;
	FPU_CONSISTENCY_CHECK_STOP("do_fmod_dont_set_cw");
}

static void do_frem_dont_set_cw( float80 dest, float80 src ) REGPARAM;
static void do_frem_dont_set_cw( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();

	volatile uint16 status;
	uae_u32 quot;

	uae_u32 sign = (dest[9] ^ src[9]) & 0x80;

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
    FNSTSW  sw
		FSTP    TBYTE PTR [EDI]
		FSTP    ST(0)
	}
	if(sw & SW_EXCEPTION_MASK) {
		_asm FNCLEX
		sw &= ~(SW_EXCEPTION_MASK - SW_IE - SW_UE);
		sw_accrued |= sw;
	}
	sw_quotient = (sign | (quot&0x7F)) << 16;
	FPU_CONSISTENCY_CHECK_STOP("do_frem_dont_set_cw");
}
#endif //USE_3_BIT_QUOTIENT

static void do_fadd( float80 dest, float80 src ) REGPARAM;
static void do_fadd( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
    FLD     TBYTE PTR [EDI]
		FADD
		FXAM
    FNSTSW  sw
		FSTP    TBYTE PTR [EDI]
	} */
	_ASM(	"fldt	%2\n"
			"fldt	%1\n"
			"fadd	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (sw), "+m" (*dest)
		:	"m" (*src)
		);
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw &= ~(SW_EXCEPTION_MASK - SW_IE - SW_UE - SW_OE - SW_PE);
		sw_accrued |= sw;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fadd");
}

static void do_fmul( float80 dest, float80 src ) REGPARAM;
static void do_fmul( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
    FLD     TBYTE PTR [EDI]
		FMUL
		FXAM
    FNSTSW  sw
		FSTP    TBYTE PTR [EDI]
	} */
	_ASM(	"fldt	%2\n"
			"fldt	%1\n"
			"fmul	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (sw), "+m" (*dest)
		:	"m" (*src)
		);
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw_accrued |= sw;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fmul");
}

static void do_fsgldiv( float80 dest, float80 src ) REGPARAM;
static void do_fsgldiv( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
	volatile WORD cw_temp;
/*	_asm {
		FSTCW   cw_temp
		and			cw_temp, ~X86_PRECISION_CONTROL_MASK
		or			cw_temp, PRECISION_CONTROL_SINGLE
    FLDCW   cw_temp

		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
    FLD     TBYTE PTR [EDI]
		FDIV		ST(0),ST(1)
		FXAM
    FNSTSW  sw
		FSTP    TBYTE PTR [EDI]
		FSTP    ST(0)
    FLDCW   cw
	} */
	_ASM(	"fstcw	%0\n"
			"andl	$(~X86_PRECISION_CONTROL_MASK), %0\n"
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
		:	"+m" (cw_temp), "=m" (sw), "+m" (*dest)
		:	"m" (*src), "m" (cw)
		);
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw_accrued |= sw;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fsgldiv");
}

static void do_fscale( float80 dest, float80 src ) REGPARAM;
static void do_fscale( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
    FLD     TBYTE PTR [EDI]
		FSCALE
		FXAM
    FNSTSW  sw
		FSTP    TBYTE PTR [EDI]
		FSTP    ST(0)
	} */
	_ASM(	"fldt	%2\n"
			"fldt	%1\n"
			"fscale	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
			"fstp	%%st(0)\n"
		:	"=m" (sw), "+m" (*dest)
		:	"m" (*src)
		);
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw &= ~(SW_EXCEPTION_MASK - SW_UE - SW_OE);
		sw_accrued |= sw;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fscale");
}

static void do_fsglmul( float80 dest, float80 src ) REGPARAM;
static void do_fsglmul( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
	volatile WORD cw_temp;

/*	_asm {
		FSTCW   cw_temp
		and			cw_temp, ~X86_PRECISION_CONTROL_MASK
		or			cw_temp, PRECISION_CONTROL_SINGLE
    FLDCW   cw_temp

		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
    FLD     TBYTE PTR [EDI]
		FMUL
		FXAM
    FNSTSW  sw
		FSTP    TBYTE PTR [EDI]

    FLDCW   cw
	} */
	_ASM(	"fstcw	%0\n"
			"andl	$(~X86_PRECISION_CONTROL_MASK), %0\n"
			"orl	$PRECISION_CONTROL_SINGLE, %0\n"
			"fldcw	%0\n"
			"fldt	%3\n"
			"fldt	%2\n"
			"fmul	\n"
			"fxam	\n"
			"fnstsw	%1\n"
			"fstpt	%2\n"
			"fldcw	%4\n"
		:	"+m" (cw_temp), "=m" (sw), "+m" (*dest)
		:	"m" (*src), "m" (sw)
		);
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw_accrued |= sw;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fsglmul");
}

static void do_fsub( float80 dest, float80 src ) REGPARAM;
static void do_fsub( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
    FLD     TBYTE PTR [EDI]
		FSUB		ST(0),ST(1)
		FXAM
    FNSTSW  sw
		FSTP    TBYTE PTR [EDI]
		FSTP    ST(0)
	} */
	_ASM(	"fldt	%2\n"
			"fldt	%1\n"
			"fsub	%%st(1), %%st(0)\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
			"fstp	%%st(0)\n"
		:	"=m" (sw), "+m" (*dest)
		:	"m" (*src)
		);
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw &= ~(SW_EXCEPTION_MASK - SW_IE - SW_UE - SW_OE - SW_PE);
		sw_accrued |= sw;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fsub");
}

static void do_fsincos( float80 dest_sin, float80 dest_cos, float80 src ) REGPARAM;
static void do_fsincos( float80 dest_sin, float80 dest_cos, float80 src )
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
    FNSTSW  sw
		FSTP    TBYTE PTR [EDI]
		FSTP    ST(0)
	} */
	_ASM(	"fldt	%3\n"
			"fsincos\n"
			"fstpt	%1\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%2\n"
			"fstp	%%st(0)\n"
		:	"=m" (sw), "=m" (*dest_cos), "=m" (*dest_sin)
		:	"m" (*src)
		);
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw &= ~(SW_EXCEPTION_MASK - SW_IE - SW_UE - SW_PE);
		sw_accrued |= sw;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fsincos");
}

static void do_fcmp( float80 dest, float80 src ) REGPARAM;
static void do_fcmp( float80 dest, float80 src )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		MOV			ESI, [src]
		MOV			EDI, [dest]
    FLD     TBYTE PTR [ESI]
    FLD     TBYTE PTR [EDI]
		FSUB    ST(0),ST(1)
		FXAM
    FNSTSW  sw
		FSTP    ST(0)
		FSTP    ST(0)
	} */
	_ASM(	"fldt	%2\n"
			"fldt	%1\n"
			"fsub	%%st(1), %%st(0)\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstp	%%st(0)\n"
			"fstp	%%st(0)\n"
		:	"=m" (sw)
		:	"m" (*dest), "m" (*src)
		);
	if(sw & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		sw &= ~SW_EXCEPTION_MASK;
	}
	FPU_CONSISTENCY_CHECK_STOP("do_fcmp");
}

// More or less original. Should be reviewed.
static double to_pack(uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3) REGPARAM;
static double to_pack(uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3)
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
static void from_pack(double src, uae_u32 * wrd1, uae_u32 * wrd2, uae_u32 * wrd3) REGPARAM;
static void from_pack(double src, uae_u32 * wrd1, uae_u32 * wrd2, uae_u32 * wrd3)
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

	volatile WORD sw_temp;
//	_asm FNSTSW sw_temp
	_ASM("fnstsw %0" : "=m" (sw_temp));
	if(sw_temp & SW_EXCEPTION_MASK) {
//		_asm FNCLEX
		_ASM("fnclex");
		if(sw_temp & SW_PE) {
			sw |= SW_PE;
			sw_accrued |= SW_PE;
		}
	}

	/*
	OPERR is set if the k-factor > + 17 or the magnitude of
	the decimal exponent exceeds three digits;
	cleared otherwise.
	*/
	if(exponent_digit_count > 3) {
		sw |= SW_IE;
		sw_accrued |= SW_IE;
	}

	FPU_CONSISTENCY_CHECK_STOP("from_pack");
}

static int sz1[8] = {4, 4, 12, 12, 2, 8, 1, 0};
static int sz2[8] = {4, 4, 12, 12, 2, 8, 2, 0};

static int get_fp_value (uae_u32 opcode, uae_u16 extra, float80 src) REGPARAM;
static int get_fp_value (uae_u32 opcode, uae_u16 extra, float80 src)
{
	// D(bug("get_fp_value(%X,%X)\r\n",(int)opcode,(int)extra));
	// dump_first_bytes( regs.pc_p-4, 16 );

  if ((extra & 0x4000) == 0) {
		memcpy( src, fp_reg[(extra >> 10) & 7], sizeof(float80_s) );
		// do_fmove_no_status( src, fp_reg[(extra >> 10) & 7] );
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

	// D(bug("get_fp_value result = %.04f\r\n",(float)*src));

  return 1;
}

static int put_fp_value (float80 value, uae_u32 opcode, uae_u16 extra) REGPARAM;
static int put_fp_value (float80 value, uae_u32 opcode, uae_u16 extra)
{
	// D(bug("put_fp_value(%.04f,%X,%X)\r\n",(float)value,(int)opcode,(int)extra));

  if ((extra & 0x4000) == 0) {
		int dest_reg = (extra >> 10) & 7;
		do_fmove( fp_reg[dest_reg], value );
		build_ex_status();
		return 1;
  }

  int mode = (opcode >> 3) & 7;
  int reg = opcode & 7;
  int size = (extra >> 10) & 7;
  uae_u32 ad = 0xffffffff;

	// Clear exception status
	sw &= ~SW_EXCEPTION_MASK;

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
			
			sw &= ~SW_EXCEPTION_MASK;
			if(wrd3) { // TODO: not correct! Just a "smart" guess.
				sw |= SW_PE;
				sw_accrued |= SW_PE;
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

static int get_fp_ad(uae_u32 opcode, uae_u32 * ad) REGPARAM;
static int get_fp_ad(uae_u32 opcode, uae_u32 * ad)
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
			dump_fp_regs( "END  ");
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
					dump_fp_regs( "END  ");
					return 0;
			}
  }
  return 1;
}

static int fpp_cond(uae_u32 opcode, int condition) REGPARAM;
static int fpp_cond(uae_u32 opcode, int condition)
{

#define N				(sw & SW_N)
#define Z				((sw & (SW_Z_I_NAN_MASK)) == SW_Z)
#define I				((sw & (SW_Z_I_NAN_MASK)) == (SW_I))
#define NotANumber		((sw & (SW_Z_I_NAN_MASK)) == SW_NAN)

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

void REGPARAM2 fdbcc_opp(uae_u32 opcode, uae_u16 extra)
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

void REGPARAM2 fscc_opp(uae_u32 opcode, uae_u16 extra)
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

void REGPARAM2 ftrapcc_opp(uae_u32 opcode, uaecptr oldpc)
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
void REGPARAM2 fbcc_opp(uae_u32 opcode, uaecptr pc, uae_u32 extra)
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
void REGPARAM2 fsave_opp(uae_u32 opcode)
{
  uae_u32 ad;
  int incr = (opcode & 0x38) == 0x20 ? -1 : 1;
  int i;

  D(bug("fsave_opp at %08lx\r\n", m68k_getpc ()));

  if (get_fp_ad(opcode, &ad)) {
		if (is_integral_68040_fpu) {
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

static void do_null_frestore()
{
	// A null-restore operation sets FP7-FP0 positive, nonsignaling NANs.
	for( int i=0; i<8; i++ ) {
		MAKE_NAN( fp_reg[i] );
	}

	regs.fpiar = 0;
	regs.fpcr = 0;
	regs.fpsr = 0;

	sw = SW_INITIAL;
	sw_accrued = 0;
	sw_quotient = 0;

	cw = CW_INITIAL;
/*  _asm	FLDCW   cw
	_asm	FNCLEX */
	_ASM("fldcw %0\n\tfnclex" : : "m" (cw));
}

// FSAVE has no pre-decrement
void REGPARAM2 frestore_opp(uae_u32 opcode)
{
  uae_u32 ad;
  uae_u32 d;
  int incr = (opcode & 0x38) == 0x20 ? -1 : 1;

  D(bug("frestore_opp at %08lx\r\n", m68k_getpc ()));

  if (get_fp_ad(opcode, &ad)) {
		if (is_integral_68040_fpu) {
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
void REGPARAM2 fpp_opp(uae_u32 opcode, uae_u16 extra)
{
	uae_u32 mask = (extra & 0xFC7F) | ((opcode & 0x0038) << 4);
	(*fpufunctbl[mask])(opcode,extra);
}
// #endif


/* ---------------------------- Illegal ---------------------------- */

void REGPARAM2 fpuop_illg( uae_u32 opcode, uae_u16 extra )
{
	D(bug("ILLEGAL F OP 2 %X\r\n",opcode));

#if I3_ON_ILLEGAL_FPU_OP
#error "FIXME: asm int 3"
	_asm int 3
#endif

	m68k_setpc (m68k_getpc () - 4);
	op_illg (opcode);
	dump_fp_regs( "END  ");
}


/* ---------------------------- FPP -> <ea> ---------------------------- */

void REGPARAM2 fpuop_fmove_2_ea( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVE -> <ea>\r\n"));

	if (put_fp_value (fp_reg[(extra >> 7) & 7], opcode, extra) == 0) {
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
		sw |= SW_PE;
		sw_accrued |= SW_PE;
	}
	*/

	dump_fp_regs( "END  ");
}


/* ---------------------------- CONTROL REGS -> Dreg ---------------------------- */

void REGPARAM2 fpuop_fmovem_none_2_Dreg( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVEM control(none) -> D%d\r\n", opcode & 7));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_fpiar_2_Dreg( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVEM regs.fpiar (%X) -> D%d\r\n", regs.fpiar, opcode & 7));
	m68k_dreg (regs, opcode & 7) = regs.fpiar;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_fpsr_2_Dreg( uae_u32 opcode, uae_u16 extra )
{
	to_fpsr();
	D(bug("FMOVEM regs.fpsr (%X) -> D%d\r\n", regs.fpsr, opcode & 7));
	m68k_dreg (regs, opcode & 7) = regs.fpsr;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_fpcr_2_Dreg( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVEM regs.fpcr (%X) -> D%d\r\n", regs.fpcr, opcode & 7));
	m68k_dreg (regs, opcode & 7) = regs.fpcr;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_fpsr_fpiar_2_Dreg( uae_u32 opcode, uae_u16 extra )
{
	to_fpsr();
	D(bug("FMOVEM regs.fpsr (%X) -> D%d\r\n", regs.fpsr, opcode & 7));
	m68k_dreg (regs, opcode & 7) = regs.fpsr;
	D(bug("FMOVEM regs.fpiar (%X) -> D%d\r\n", regs.fpiar, opcode & 7));
	m68k_dreg (regs, opcode & 7) = regs.fpiar;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_fpcr_fpiar_2_Dreg( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVEM regs.fpcr (%X) -> D%d\r\n", regs.fpcr, opcode & 7));
	m68k_dreg (regs, opcode & 7) = regs.fpcr;
	D(bug("FMOVEM regs.fpiar (%X) -> D%d\r\n", regs.fpiar, opcode & 7));
	m68k_dreg (regs, opcode & 7) = regs.fpiar;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_fpcr_fpsr_2_Dreg( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVEM regs.fpcr (%X) -> D%d\r\n", regs.fpcr, opcode & 7));
	m68k_dreg (regs, opcode & 7) = regs.fpcr;
	to_fpsr();
	D(bug("FMOVEM regs.fpsr (%X) -> D%d\r\n", regs.fpsr, opcode & 7));
	m68k_dreg (regs, opcode & 7) = regs.fpsr;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_fpcr_fpsr_fpiar_2_Dreg( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVEM regs.fpcr (%X) -> D%d\r\n", regs.fpcr, opcode & 7));
	m68k_dreg (regs, opcode & 7) = regs.fpcr;
	to_fpsr();
	D(bug("FMOVEM regs.fpsr (%X) -> D%d\r\n", regs.fpsr, opcode & 7));
	m68k_dreg (regs, opcode & 7) = regs.fpsr;
	D(bug("FMOVEM regs.fpiar (%X) -> D%d\r\n", regs.fpiar, opcode & 7));
	m68k_dreg (regs, opcode & 7) = regs.fpiar;
	dump_fp_regs( "END  ");
}


/* ---------------------------- Dreg -> CONTROL REGS ---------------------------- */

void REGPARAM2 fpuop_fmovem_Dreg_2_none( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVEM D%d -> control(none)\r\n", opcode & 7));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Dreg_2_fpiar( uae_u32 opcode, uae_u16 extra )
{
	regs.fpiar = m68k_dreg (regs, opcode & 7);
	D(bug("FMOVEM D%d (%X) -> regs.fpiar\r\n", opcode & 7, regs.fpiar));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Dreg_2_fpsr( uae_u32 opcode, uae_u16 extra )
{
	regs.fpsr = m68k_dreg (regs, opcode & 7);
	from_fpsr();
	D(bug("FMOVEM D%d (%X) -> regs.fpsr\r\n", opcode & 7, regs.fpsr));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Dreg_2_fpsr_fpiar( uae_u32 opcode, uae_u16 extra )
{
	regs.fpsr = m68k_dreg (regs, opcode & 7);
	from_fpsr();
	D(bug("FMOVEM D%d (%X) -> regs.fpsr\r\n", opcode & 7, regs.fpsr));
	regs.fpiar = m68k_dreg (regs, opcode & 7);
	D(bug("FMOVEM D%d (%X) -> regs.fpiar\r\n", opcode & 7, regs.fpiar));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Dreg_2_fpcr( uae_u32 opcode, uae_u16 extra )
{
	regs.fpcr = m68k_dreg (regs, opcode & 7);
	set_host_fpu_control_word();
	D(bug("FMOVEM D%d (%X) -> regs.fpcr\r\n", opcode & 7, regs.fpcr));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Dreg_2_fpcr_fpiar( uae_u32 opcode, uae_u16 extra )
{
	regs.fpcr = m68k_dreg (regs, opcode & 7);
	set_host_fpu_control_word();
	D(bug("FMOVEM D%d (%X) -> regs.fpcr\r\n", opcode & 7, regs.fpcr));
	regs.fpiar = m68k_dreg (regs, opcode & 7);
	D(bug("FMOVEM D%d (%X) -> regs.fpiar\r\n", opcode & 7, regs.fpiar));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Dreg_2_fpcr_fpsr( uae_u32 opcode, uae_u16 extra )
{
	regs.fpcr = m68k_dreg (regs, opcode & 7);
	set_host_fpu_control_word();
	D(bug("FMOVEM D%d (%X) -> regs.fpcr\r\n", opcode & 7, regs.fpcr));
	regs.fpsr = m68k_dreg (regs, opcode & 7);
	from_fpsr();
	D(bug("FMOVEM D%d (%X) -> regs.fpsr\r\n", opcode & 7, regs.fpsr));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Dreg_2_fpcr_fpsr_fpiar( uae_u32 opcode, uae_u16 extra )
{
	regs.fpcr = m68k_dreg (regs, opcode & 7);
	set_host_fpu_control_word();
	D(bug("FMOVEM D%d (%X) -> regs.fpcr\r\n", opcode & 7, regs.fpcr));
	regs.fpsr = m68k_dreg (regs, opcode & 7);
	from_fpsr();
	D(bug("FMOVEM D%d (%X) -> regs.fpsr\r\n", opcode & 7, regs.fpsr));
	regs.fpiar = m68k_dreg (regs, opcode & 7);
	D(bug("FMOVEM D%d (%X) -> regs.fpiar\r\n", opcode & 7, regs.fpiar));
	dump_fp_regs( "END  ");
}


/* ---------------------------- CONTROL REGS -> Areg ---------------------------- */

void REGPARAM2 fpuop_fmovem_none_2_Areg( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVEM control(none) -> A%d\r\n", opcode & 7));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_fpiar_2_Areg( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVEM regs.fpiar (%X) -> A%d\r\n", regs.fpiar, opcode & 7));
	m68k_areg (regs, opcode & 7) = regs.fpiar;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_fpsr_2_Areg( uae_u32 opcode, uae_u16 extra )
{
	to_fpsr();
	D(bug("FMOVEM regs.fpsr (%X) -> A%d\r\n", regs.fpsr, opcode & 7));
	m68k_areg (regs, opcode & 7) = regs.fpsr;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_fpcr_2_Areg( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVEM regs.fpcr (%X) -> A%d\r\n", regs.fpcr, opcode & 7));
	m68k_areg (regs, opcode & 7) = regs.fpcr;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_fpsr_fpiar_2_Areg( uae_u32 opcode, uae_u16 extra )
{
	to_fpsr();
	D(bug("FMOVEM regs.fpsr (%X) -> A%d\r\n", regs.fpsr, opcode & 7));
	m68k_areg (regs, opcode & 7) = regs.fpsr;
	D(bug("FMOVEM regs.fpiar (%X) -> A%d\r\n", regs.fpiar, opcode & 7));
	m68k_areg (regs, opcode & 7) = regs.fpiar;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_fpcr_fpiar_2_Areg( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVEM regs.fpcr (%X) -> A%d\r\n", regs.fpcr, opcode & 7));
	m68k_areg (regs, opcode & 7) = regs.fpcr;
	D(bug("FMOVEM regs.fpiar (%X) -> A%d\r\n", regs.fpiar, opcode & 7));
	m68k_areg (regs, opcode & 7) = regs.fpiar;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_fpcr_fpsr_2_Areg( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVEM regs.fpcr (%X) -> A%d\r\n", regs.fpcr, opcode & 7));
	m68k_areg (regs, opcode & 7) = regs.fpcr;
	to_fpsr();
	D(bug("FMOVEM regs.fpsr (%X) -> A%d\r\n", regs.fpsr, opcode & 7));
	m68k_areg (regs, opcode & 7) = regs.fpsr;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_fpcr_fpsr_fpiar_2_Areg( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVEM regs.fpcr (%X) -> A%d\r\n", regs.fpcr, opcode & 7));
	m68k_areg (regs, opcode & 7) = regs.fpcr;
	to_fpsr();
	D(bug("FMOVEM regs.fpsr (%X) -> A%d\r\n", regs.fpsr, opcode & 7));
	m68k_areg (regs, opcode & 7) = regs.fpsr;
	D(bug("FMOVEM regs.fpiar (%X) -> A%d\r\n", regs.fpiar, opcode & 7));
	m68k_areg (regs, opcode & 7) = regs.fpiar;
	dump_fp_regs( "END  ");
}


/* ---------------------------- Areg -> CONTROL REGS ---------------------------- */

void REGPARAM2 fpuop_fmovem_Areg_2_none( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVEM A%d -> control(none)\r\n", opcode & 7));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Areg_2_fpiar( uae_u32 opcode, uae_u16 extra )
{
	regs.fpiar = m68k_areg (regs, opcode & 7);
	D(bug("FMOVEM A%d (%X) -> regs.fpiar\r\n", opcode & 7, regs.fpiar));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Areg_2_fpsr( uae_u32 opcode, uae_u16 extra )
{
	regs.fpsr = m68k_areg (regs, opcode & 7);
	from_fpsr();
	D(bug("FMOVEM A%d (%X) -> regs.fpsr\r\n", opcode & 7, regs.fpsr));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Areg_2_fpsr_fpiar( uae_u32 opcode, uae_u16 extra )
{
	regs.fpsr = m68k_areg (regs, opcode & 7);
	from_fpsr();
	D(bug("FMOVEM A%d (%X) -> regs.fpsr\r\n", opcode & 7, regs.fpsr));
	regs.fpiar = m68k_areg (regs, opcode & 7);
	D(bug("FMOVEM A%d (%X) -> regs.fpiar\r\n", opcode & 7, regs.fpiar));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Areg_2_fpcr( uae_u32 opcode, uae_u16 extra )
{
	regs.fpcr = m68k_areg (regs, opcode & 7);
	set_host_fpu_control_word();
	D(bug("FMOVEM A%d (%X) -> regs.fpcr\r\n", opcode & 7, regs.fpcr));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Areg_2_fpcr_fpiar( uae_u32 opcode, uae_u16 extra )
{
	regs.fpcr = m68k_areg (regs, opcode & 7);
	set_host_fpu_control_word();
	D(bug("FMOVEM A%d (%X) -> regs.fpcr\r\n", opcode & 7, regs.fpcr));
	regs.fpiar = m68k_areg (regs, opcode & 7);
	D(bug("FMOVEM A%d (%X) -> regs.fpiar\r\n", opcode & 7, regs.fpiar));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Areg_2_fpcr_fpsr( uae_u32 opcode, uae_u16 extra )
{
	regs.fpcr = m68k_areg (regs, opcode & 7);
	set_host_fpu_control_word();
	D(bug("FMOVEM A%d (%X) -> regs.fpcr\r\n", opcode & 7, regs.fpcr));
	regs.fpsr = m68k_areg (regs, opcode & 7);
	from_fpsr();
	D(bug("FMOVEM A%d (%X) -> regs.fpsr\r\n", opcode & 7, regs.fpsr));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Areg_2_fpcr_fpsr_fpiar( uae_u32 opcode, uae_u16 extra )
{
	regs.fpcr = m68k_areg (regs, opcode & 7);
	set_host_fpu_control_word();
	D(bug("FMOVEM A%d (%X) -> regs.fpcr\r\n", opcode & 7, regs.fpcr));
	regs.fpsr = m68k_areg (regs, opcode & 7);
	from_fpsr();
	D(bug("FMOVEM A%d (%X) -> regs.fpsr\r\n", opcode & 7, regs.fpsr));
	regs.fpiar = m68k_areg (regs, opcode & 7);
	D(bug("FMOVEM A%d (%X) -> regs.fpiar\r\n", opcode & 7, regs.fpiar));
	dump_fp_regs( "END  ");
}


/* ---------------------------- CONTROL REGS -> --MEMORY---------------------------- */

void REGPARAM2 fpuop_fmovem_none_2_Mem_predecrement( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVEM Control regs (none) -> mem\r\n" ));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_fpiar_2_Mem_predecrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 4;
		put_long (ad, regs.fpiar);
		D(bug("FMOVEM regs.fpiar (%X) -> mem %X\r\n", regs.fpiar, ad ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_fpsr_2_Mem_predecrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 4;
		to_fpsr();
		put_long (ad, regs.fpsr);
		D(bug("FMOVEM regs.fpsr (%X) -> mem %X\r\n", regs.fpsr, ad ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_fpsr_fpiar_2_Mem_predecrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 8;
		to_fpsr();
		put_long (ad, regs.fpsr);
		D(bug("FMOVEM regs.fpsr (%X) -> mem %X\r\n", regs.fpsr, ad ));
		put_long (ad+4, regs.fpiar);
		D(bug("FMOVEM regs.fpiar (%X) -> mem %X\r\n", regs.fpiar, ad+4 ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_fpcr_2_Mem_predecrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 4;
		put_long (ad, regs.fpcr);
		D(bug("FMOVEM regs.fpcr (%X) -> mem %X\r\n", regs.fpcr, ad ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_fpcr_fpiar_2_Mem_predecrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 8;
		put_long (ad, regs.fpcr);
		D(bug("FMOVEM regs.fpcr (%X) -> mem %X\r\n", regs.fpcr, ad ));
		put_long (ad+4, regs.fpiar);
		D(bug("FMOVEM regs.fpiar (%X) -> mem %X\r\n", regs.fpiar, ad+4 ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_fpcr_fpsr_2_Mem_predecrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 8;
		put_long (ad, regs.fpcr);
		D(bug("FMOVEM regs.fpcr (%X) -> mem %X\r\n", regs.fpcr, ad ));
		to_fpsr();
		put_long (ad+4, regs.fpsr);
		D(bug("FMOVEM regs.fpsr (%X) -> mem %X\r\n", regs.fpsr, ad+4 ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_fpcr_fpsr_fpiar_2_Mem_predecrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 12;
		put_long (ad, regs.fpcr);
		D(bug("FMOVEM regs.fpcr (%X) -> mem %X\r\n", regs.fpcr, ad ));
		to_fpsr();
		put_long (ad+4, regs.fpsr);
		D(bug("FMOVEM regs.fpsr (%X) -> mem %X\r\n", regs.fpsr, ad+4 ));
		put_long (ad+8, regs.fpiar);
		D(bug("FMOVEM regs.fpiar (%X) -> mem %X\r\n", regs.fpiar, ad+8 ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}


/* ---------------------------- CONTROL REGS -> MEMORY++ ---------------------------- */

void REGPARAM2 fpuop_fmovem_none_2_Mem_postincrement( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVEM Control regs (none) -> mem\r\n" ));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_fpiar_2_Mem_postincrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, regs.fpiar);
		D(bug("FMOVEM regs.fpiar (%X) -> mem %X\r\n", regs.fpiar, ad ));
		m68k_areg (regs, opcode & 7) = ad+4;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_fpsr_2_Mem_postincrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		to_fpsr();
		put_long (ad, regs.fpsr);
		D(bug("FMOVEM regs.fpsr (%X) -> mem %X\r\n", regs.fpsr, ad ));
		m68k_areg (regs, opcode & 7) = ad+4;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_fpsr_fpiar_2_Mem_postincrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		to_fpsr();
		put_long (ad, regs.fpsr);
		D(bug("FMOVEM regs.fpsr (%X) -> mem %X\r\n", regs.fpsr, ad ));
		put_long (ad+4, regs.fpiar);
		D(bug("FMOVEM regs.fpiar (%X) -> mem %X\r\n", regs.fpiar, ad+4 ));
		m68k_areg (regs, opcode & 7) = ad+8;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_fpcr_2_Mem_postincrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, regs.fpcr);
		D(bug("FMOVEM regs.fpcr (%X) -> mem %X\r\n", regs.fpcr, ad ));
		m68k_areg (regs, opcode & 7) = ad+4;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_fpcr_fpiar_2_Mem_postincrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, regs.fpcr);
		D(bug("FMOVEM regs.fpcr (%X) -> mem %X\r\n", regs.fpcr, ad ));
		put_long (ad+4, regs.fpiar);
		D(bug("FMOVEM regs.fpiar (%X) -> mem %X\r\n", regs.fpiar, ad+4 ));
		m68k_areg (regs, opcode & 7) = ad+8;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_fpcr_fpsr_2_Mem_postincrement( uae_u32 opcode, uae_u16 extra )
{
	dump_fp_regs( "END  ");
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, regs.fpcr);
		D(bug("FMOVEM regs.fpcr (%X) -> mem %X\r\n", regs.fpcr, ad ));
		to_fpsr();
		put_long (ad+4, regs.fpsr);
		D(bug("FMOVEM regs.fpsr (%X) -> mem %X\r\n", regs.fpsr, ad+4 ));
		m68k_areg (regs, opcode & 7) = ad+8;
	}
}

void REGPARAM2 fpuop_fmovem_fpcr_fpsr_fpiar_2_Mem_postincrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, regs.fpcr);
		D(bug("FMOVEM regs.fpcr (%X) -> mem %X\r\n", regs.fpcr, ad ));
		to_fpsr();
		put_long (ad+4, regs.fpsr);
		D(bug("FMOVEM regs.fpsr (%X) -> mem %X\r\n", regs.fpsr, ad+4 ));
		put_long (ad+8, regs.fpiar);
		D(bug("FMOVEM regs.fpiar (%X) -> mem %X\r\n", regs.fpiar, ad+8 ));
		m68k_areg (regs, opcode & 7) = ad+12;
		dump_fp_regs( "END  ");
	}
}


/* ---------------------------- CONTROL REGS -> MEMORY ---------------------------- */

void REGPARAM2 fpuop_fmovem_none_2_Mem( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVEM Control regs (none) -> mem\r\n" ));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_fpiar_2_Mem( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, regs.fpiar);
		D(bug("FMOVEM regs.fpiar (%X) -> mem %X\r\n", regs.fpiar, ad ));
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_fpsr_2_Mem( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		to_fpsr();
		put_long (ad, regs.fpsr);
		D(bug("FMOVEM regs.fpsr (%X) -> mem %X\r\n", regs.fpsr, ad ));
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_fpsr_fpiar_2_Mem( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		to_fpsr();
		put_long (ad, regs.fpsr);
		D(bug("FMOVEM regs.fpsr (%X) -> mem %X\r\n", regs.fpsr, ad ));
		put_long (ad+4, regs.fpiar);
		D(bug("FMOVEM regs.fpiar (%X) -> mem %X\r\n", regs.fpiar, ad+4 ));
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_fpcr_2_Mem( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, regs.fpcr);
		D(bug("FMOVEM regs.fpcr (%X) -> mem %X\r\n", regs.fpcr, ad ));
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_fpcr_fpiar_2_Mem( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, regs.fpcr);
		D(bug("FMOVEM regs.fpcr (%X) -> mem %X\r\n", regs.fpcr, ad ));
		put_long (ad+4, regs.fpiar);
		D(bug("FMOVEM regs.fpiar (%X) -> mem %X\r\n", regs.fpiar, ad+4 ));
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_fpcr_fpsr_2_Mem( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, regs.fpcr);
		D(bug("FMOVEM regs.fpcr (%X) -> mem %X\r\n", regs.fpcr, ad ));
		to_fpsr();
		put_long (ad+4, regs.fpsr);
		D(bug("FMOVEM regs.fpsr (%X) -> mem %X\r\n", regs.fpsr, ad+4 ));
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_fpcr_fpsr_fpiar_2_Mem( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		put_long (ad, regs.fpcr);
		D(bug("FMOVEM regs.fpcr (%X) -> mem %X\r\n", regs.fpcr, ad ));
		to_fpsr();
		put_long (ad+4, regs.fpsr);
		D(bug("FMOVEM regs.fpsr (%X) -> mem %X\r\n", regs.fpsr, ad+4 ));
		put_long (ad+8, regs.fpiar);
		D(bug("FMOVEM regs.fpiar (%X) -> mem %X\r\n", regs.fpiar, ad+8 ));
		dump_fp_regs( "END  ");
	}
}


/* ---------------------------- --MEMORY -> CONTROL REGS ---------------------------- */

void REGPARAM2 fpuop_fmovem_Mem_2_none_predecrement( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVEM --Mem -> control(none)\r\n"));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpiar_predecrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 4;
		regs.fpiar = get_long (ad);
		D(bug("FMOVEM mem %X (%X) -> regs.fpiar\r\n", ad, regs.fpiar ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpsr_predecrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 4;
		regs.fpsr = get_long (ad);
		from_fpsr();
		D(bug("FMOVEM mem %X (%X) -> regs.fpsr\r\n", ad, regs.fpsr ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpsr_fpiar_predecrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 8;
		regs.fpsr = get_long (ad);
		from_fpsr();
		D(bug("FMOVEM mem %X (%X) -> regs.fpsr\r\n", ad, regs.fpsr ));
		regs.fpiar = get_long (ad+4);
		D(bug("FMOVEM mem %X (%X) -> regs.fpiar\r\n", ad+4, regs.fpiar ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpcr_predecrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 4;
		regs.fpcr = get_long (ad);
		set_host_fpu_control_word();
		D(bug("FMOVEM mem %X (%X) -> regs.fpcr\r\n", ad, regs.fpcr ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpcr_fpiar_predecrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 8;
		regs.fpcr = get_long (ad);
		set_host_fpu_control_word();
		D(bug("FMOVEM mem %X (%X) -> regs.fpcr\r\n", ad, regs.fpcr ));
		regs.fpiar = get_long (ad+4);
		D(bug("FMOVEM mem %X (%X) -> regs.fpiar\r\n", ad+4, regs.fpiar ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpcr_fpsr_predecrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 8;
		regs.fpcr = get_long (ad);
		set_host_fpu_control_word();
		D(bug("FMOVEM mem %X (%X) -> regs.fpcr\r\n", ad, regs.fpcr ));
		regs.fpsr = get_long (ad+4);
		from_fpsr();
		D(bug("FMOVEM mem %X (%X) -> regs.fpsr\r\n", ad+4, regs.fpsr ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpcr_fpsr_fpiar_predecrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		ad -= 12;
		regs.fpcr = get_long (ad);
		set_host_fpu_control_word();
		D(bug("FMOVEM mem %X (%X) -> regs.fpcr\r\n", ad, regs.fpcr ));
		regs.fpsr = get_long (ad+4);
		from_fpsr();
		D(bug("FMOVEM mem %X (%X) -> regs.fpsr\r\n", ad+4, regs.fpsr ));
		regs.fpiar = get_long (ad+8);
		D(bug("FMOVEM mem %X (%X) -> regs.fpiar\r\n", ad+8, regs.fpiar ));
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}


/* ---------------------------- CONTROL REGS -> MEMORY++ ---------------------------- */

void REGPARAM2 fpuop_fmovem_Mem_2_none_postincrement( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVEM Mem++ -> control(none)\r\n"));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpiar_postincrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		regs.fpiar = get_long (ad);
		D(bug("FMOVEM mem %X (%X) -> regs.fpiar\r\n", ad, regs.fpiar ));
		m68k_areg (regs, opcode & 7) = ad+4;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpsr_postincrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		regs.fpsr = get_long (ad);
		from_fpsr();
		D(bug("FMOVEM mem %X (%X) -> regs.fpsr\r\n", ad, regs.fpsr ));
		m68k_areg (regs, opcode & 7) = ad+4;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpsr_fpiar_postincrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		regs.fpsr = get_long (ad);
		from_fpsr();
		D(bug("FMOVEM mem %X (%X) -> regs.fpsr\r\n", ad, regs.fpsr ));
		regs.fpiar = get_long (ad+4);
		D(bug("FMOVEM mem %X (%X) -> regs.fpiar\r\n", ad+4, regs.fpiar ));
		m68k_areg (regs, opcode & 7) = ad+8;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpcr_postincrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		regs.fpcr = get_long (ad);
		set_host_fpu_control_word();
		D(bug("FMOVEM mem %X (%X) -> regs.fpcr\r\n", ad, regs.fpcr ));
		m68k_areg (regs, opcode & 7) = ad+4;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpcr_fpiar_postincrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		regs.fpcr = get_long (ad);
		set_host_fpu_control_word();
		D(bug("FMOVEM mem %X (%X) -> regs.fpcr\r\n", ad, regs.fpcr ));
		regs.fpiar = get_long (ad+4);
		D(bug("FMOVEM mem %X (%X) -> regs.fpiar\r\n", ad+4, regs.fpiar ));
		m68k_areg (regs, opcode & 7) = ad+8;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpcr_fpsr_postincrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		regs.fpcr = get_long (ad);
		set_host_fpu_control_word();
		D(bug("FMOVEM mem %X (%X) -> regs.fpcr\r\n", ad, regs.fpcr ));
		regs.fpsr = get_long (ad+4);
		from_fpsr();
		D(bug("FMOVEM mem %X (%X) -> regs.fpsr\r\n", ad+4, regs.fpsr ));
		m68k_areg (regs, opcode & 7) = ad+8;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpcr_fpsr_fpiar_postincrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad;
	if (get_fp_ad(opcode, &ad)) {
		regs.fpcr = get_long (ad);
		set_host_fpu_control_word();
		D(bug("FMOVEM mem %X (%X) -> regs.fpcr\r\n", ad, regs.fpcr ));
		regs.fpsr = get_long (ad+4);
		from_fpsr();
		D(bug("FMOVEM mem %X (%X) -> regs.fpsr\r\n", ad+4, regs.fpsr ));
		regs.fpiar = get_long (ad+8);
		D(bug("FMOVEM mem %X (%X) -> regs.fpiar\r\n", ad+8, regs.fpiar ));
		m68k_areg (regs, opcode & 7) = ad+12;
		dump_fp_regs( "END  ");
	}
}


/* ----------------------------   MEMORY -> CONTROL REGS  ---------------------------- */
/* ----------------------------            and            ---------------------------- */
/* ---------------------------- IMMEDIATE -> CONTROL REGS ---------------------------- */

void REGPARAM2 fpuop_fmovem_Mem_2_none_2_Mem( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVEM Mem -> control(none)\r\n"));
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpiar_2_Mem( uae_u32 opcode, uae_u16 extra )
{
	if ((opcode & 0x3f) == 0x3c) {
		regs.fpiar = next_ilong();
		D(bug("FMOVEM #<%X> -> regs.fpiar\r\n", regs.fpiar));
	} else {
		uae_u32 ad;
		if (get_fp_ad(opcode, &ad)) {
			regs.fpiar = get_long (ad);
			D(bug("FMOVEM mem %X (%X) -> regs.fpiar\r\n", ad, regs.fpiar ));
		}
	}
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpsr_2_Mem( uae_u32 opcode, uae_u16 extra )
{
	if ((opcode & 0x3f) == 0x3c) {
		regs.fpsr = next_ilong();
		from_fpsr();
		D(bug("FMOVEM #<%X> -> regs.fpsr\r\n", regs.fpsr));
	} else {
		uae_u32 ad;
		if (get_fp_ad(opcode, &ad)) {
			regs.fpsr = get_long (ad);
			from_fpsr();
			D(bug("FMOVEM mem %X (%X) -> regs.fpsr\r\n", ad, regs.fpsr ));
		}
	}
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpsr_fpiar_2_Mem( uae_u32 opcode, uae_u16 extra )
{
	if ((opcode & 0x3f) == 0x3c) {
		regs.fpsr = next_ilong();
		from_fpsr();
		D(bug("FMOVEM #<%X> -> regs.fpsr\r\n", regs.fpsr));
		regs.fpiar = next_ilong();
		D(bug("FMOVEM #<%X> -> regs.fpiar\r\n", regs.fpiar));
	} else {
		uae_u32 ad;
		if (get_fp_ad(opcode, &ad)) {
			regs.fpsr = get_long (ad);
			from_fpsr();
			D(bug("FMOVEM mem %X (%X) -> regs.fpsr\r\n", ad, regs.fpsr ));
			regs.fpiar = get_long (ad+4);
			D(bug("FMOVEM mem %X (%X) -> regs.fpiar\r\n", ad+4, regs.fpiar ));
		}
	}
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpcr_2_Mem( uae_u32 opcode, uae_u16 extra )
{
	if ((opcode & 0x3f) == 0x3c) {
		regs.fpcr = next_ilong();
		set_host_fpu_control_word();
		D(bug("FMOVEM #<%X> -> regs.fpcr\r\n", regs.fpcr));
	} else {
		uae_u32 ad;
		if (get_fp_ad(opcode, &ad)) {
			regs.fpcr = get_long (ad);
			set_host_fpu_control_word();
			D(bug("FMOVEM mem %X (%X) -> regs.fpcr\r\n", ad, regs.fpcr ));
		}
	}
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpcr_fpiar_2_Mem( uae_u32 opcode, uae_u16 extra )
{
	if ((opcode & 0x3f) == 0x3c) {
		regs.fpcr = next_ilong();
		set_host_fpu_control_word();
		D(bug("FMOVEM #<%X> -> regs.fpcr\r\n", regs.fpcr));
		regs.fpiar = next_ilong();
		D(bug("FMOVEM #<%X> -> regs.fpiar\r\n", regs.fpiar));
	} else {
		uae_u32 ad;
		if (get_fp_ad(opcode, &ad)) {
			regs.fpcr = get_long (ad);
			set_host_fpu_control_word();
			D(bug("FMOVEM mem %X (%X) -> regs.fpcr\r\n", ad, regs.fpcr ));
			regs.fpiar = get_long (ad+4);
			D(bug("FMOVEM mem %X (%X) -> regs.fpiar\r\n", ad+4, regs.fpiar ));
		}
	}
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpcr_fpsr_2_Mem( uae_u32 opcode, uae_u16 extra )
{
	if ((opcode & 0x3f) == 0x3c) {
		regs.fpcr = next_ilong();
		set_host_fpu_control_word();
		D(bug("FMOVEM #<%X> -> regs.fpcr\r\n", regs.fpcr));
		regs.fpsr = next_ilong();
		from_fpsr();
		D(bug("FMOVEM #<%X> -> regs.fpsr\r\n", regs.fpsr));
	} else {
		uae_u32 ad;
		if (get_fp_ad(opcode, &ad)) {
			regs.fpcr = get_long (ad);
			set_host_fpu_control_word();
			D(bug("FMOVEM mem %X (%X) -> regs.fpcr\r\n", ad, regs.fpcr ));
			regs.fpsr = get_long (ad+4);
			from_fpsr();
			D(bug("FMOVEM mem %X (%X) -> regs.fpsr\r\n", ad+4, regs.fpsr ));
		}
	}
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpcr_fpsr_fpiar_2_Mem( uae_u32 opcode, uae_u16 extra )
{
	if ((opcode & 0x3f) == 0x3c) {
		regs.fpcr = next_ilong();
		set_host_fpu_control_word();
		D(bug("FMOVEM #<%X> -> regs.fpcr\r\n", regs.fpcr));
		regs.fpsr = next_ilong();
		from_fpsr();
		D(bug("FMOVEM #<%X> -> regs.fpsr\r\n", regs.fpsr));
		regs.fpiar = next_ilong();
		D(bug("FMOVEM #<%X> -> regs.fpiar\r\n", regs.fpiar));
	} else {
		uae_u32 ad;
		if (get_fp_ad(opcode, &ad)) {
			regs.fpcr = get_long (ad);
			set_host_fpu_control_word();
			D(bug("FMOVEM mem %X (%X) -> regs.fpcr\r\n", ad, regs.fpcr ));
			regs.fpsr = get_long (ad+4);
			from_fpsr();
			D(bug("FMOVEM mem %X (%X) -> regs.fpsr\r\n", ad+4, regs.fpsr ));
			regs.fpiar = get_long (ad+8);
			D(bug("FMOVEM mem %X (%X) -> regs.fpiar\r\n", ad+8, regs.fpiar ));
		}
	}
	dump_fp_regs( "END  ");
}


/* ---------------------------- FMOVEM MEMORY -> FPP ---------------------------- */

void REGPARAM2 fpuop_fmovem_Mem_2_fpp_static_pred_postincrement( uae_u32 opcode, uae_u16 extra )
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
				to_exten_no_normalize (wrd1, wrd2, wrd3,fp_reg[reg]);
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpp_static_pred_predecrement( uae_u32 opcode, uae_u16 extra )
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
				to_exten_no_normalize (wrd1, wrd2, wrd3,fp_reg[reg]);
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpp_static_pred( uae_u32 opcode, uae_u16 extra )
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
				to_exten_no_normalize (wrd1, wrd2, wrd3,fp_reg[reg]);
			}
			list <<= 1;
		}
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpp_dynamic_pred_postincrement( uae_u32 opcode, uae_u16 extra )
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
				to_exten_no_normalize (wrd1, wrd2, wrd3,fp_reg[reg]);
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpp_dynamic_pred_predecrement( uae_u32 opcode, uae_u16 extra )
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
				to_exten_no_normalize (wrd1, wrd2, wrd3,fp_reg[reg]);
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpp_dynamic_pred( uae_u32 opcode, uae_u16 extra )
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
				to_exten_no_normalize (wrd1, wrd2, wrd3,fp_reg[reg]);
			}
			list <<= 1;
		}
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpp_static_postinc_postincrement( uae_u32 opcode, uae_u16 extra )
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
				to_exten_no_normalize (wrd1, wrd2, wrd3,fp_reg[reg]);
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpp_static_postinc_predecrement( uae_u32 opcode, uae_u16 extra )
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
				to_exten_no_normalize (wrd1, wrd2, wrd3,fp_reg[reg]);
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpp_static_postinc( uae_u32 opcode, uae_u16 extra )
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
				to_exten_no_normalize (wrd1, wrd2, wrd3,fp_reg[reg]);
			}
			list <<= 1;
		}
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpp_dynamic_postinc_postincrement( uae_u32 opcode, uae_u16 extra )
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
				to_exten_no_normalize (wrd1, wrd2, wrd3,fp_reg[reg]);
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpp_dynamic_postinc_predecrement( uae_u32 opcode, uae_u16 extra )
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
				to_exten_no_normalize (wrd1, wrd2, wrd3,fp_reg[reg]);
			}
			list <<= 1;
		}
		m68k_areg (regs, opcode & 7) = ad;
		dump_fp_regs( "END  ");
	}
}

void REGPARAM2 fpuop_fmovem_Mem_2_fpp_dynamic_postinc( uae_u32 opcode, uae_u16 extra )
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
				to_exten_no_normalize (wrd1, wrd2, wrd3,fp_reg[reg]);
			}
			list <<= 1;
		}
		dump_fp_regs( "END  ");
	}
}


/* ---------------------------- FPP -> FMOVEM MEMORY ---------------------------- */

void REGPARAM2 fpuop_fmovem_fpp_2_Mem_static_pred_postincrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad, list = extra & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=7; reg>=0; reg-- ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(fp_reg[reg],&wrd1, &wrd2, &wrd3);
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
		dump_fp_regs( "END  ");
	}
}
void REGPARAM2 fpuop_fmovem_fpp_2_Mem_static_pred_predecrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad, list = extra & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=7; reg>=0; reg-- ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(fp_reg[reg],&wrd1, &wrd2, &wrd3);
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
		dump_fp_regs( "END  ");
	}
}
void REGPARAM2 fpuop_fmovem_fpp_2_Mem_static_pred( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad, list = extra & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=7; reg>=0; reg-- ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(fp_reg[reg],&wrd1, &wrd2, &wrd3);
				ad -= 4;
				put_long (ad, wrd3);
				ad -= 4;
				put_long (ad, wrd2);
				ad -= 4;
				put_long (ad, wrd1);
			}
			list <<= 1;
		}
		dump_fp_regs( "END  ");
	}
}
void REGPARAM2 fpuop_fmovem_fpp_2_Mem_dynamic_pred_postincrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad, list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=7; reg>=0; reg-- ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(fp_reg[reg],&wrd1, &wrd2, &wrd3);
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
		dump_fp_regs( "END  ");
	}
}
void REGPARAM2 fpuop_fmovem_fpp_2_Mem_dynamic_pred_predecrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad, list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=7; reg>=0; reg-- ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(fp_reg[reg],&wrd1, &wrd2, &wrd3);
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
		dump_fp_regs( "END  ");
	}
}
void REGPARAM2 fpuop_fmovem_fpp_2_Mem_dynamic_pred( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad, list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=7; reg>=0; reg-- ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(fp_reg[reg],&wrd1, &wrd2, &wrd3);
				ad -= 4;
				put_long (ad, wrd3);
				ad -= 4;
				put_long (ad, wrd2);
				ad -= 4;
				put_long (ad, wrd1);
			}
			list <<= 1;
		}
		dump_fp_regs( "END  ");
	}
}
void REGPARAM2 fpuop_fmovem_fpp_2_Mem_static_postinc_postincrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad, list = extra & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=0; reg<8; reg++ ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(fp_reg[reg],&wrd1, &wrd2, &wrd3);
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
		dump_fp_regs( "END  ");
	}
}
void REGPARAM2 fpuop_fmovem_fpp_2_Mem_static_postinc_predecrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad, list = extra & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=0; reg<8; reg++ ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(fp_reg[reg],&wrd1, &wrd2, &wrd3);
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
		dump_fp_regs( "END  ");
	}
}
void REGPARAM2 fpuop_fmovem_fpp_2_Mem_static_postinc( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad, list = extra & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=0; reg<8; reg++ ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(fp_reg[reg],&wrd1, &wrd2, &wrd3);
				put_long (ad, wrd1);
				ad += 4;
				put_long (ad, wrd2);
				ad += 4;
				put_long (ad, wrd3);
				ad += 4;
			}
			list <<= 1;
		}
		dump_fp_regs( "END  ");
	}
}
void REGPARAM2 fpuop_fmovem_fpp_2_Mem_dynamic_postinc_postincrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad, list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=0; reg<8; reg++ ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(fp_reg[reg],&wrd1, &wrd2, &wrd3);
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
		dump_fp_regs( "END  ");
	}
}
void REGPARAM2 fpuop_fmovem_fpp_2_Mem_dynamic_postinc_predecrement( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad, list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=0; reg<8; reg++ ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(fp_reg[reg],&wrd1, &wrd2, &wrd3);
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
		dump_fp_regs( "END  ");
	}
}
void REGPARAM2 fpuop_fmovem_fpp_2_Mem_dynamic_postinc( uae_u32 opcode, uae_u16 extra )
{
	uae_u32 ad, list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
	D(bug("FMOVEM FPP->memory\r\n"));
	if (get_fp_ad(opcode, &ad)) {
		for( int reg=0; reg<8; reg++ ) {
			uae_u32 wrd1, wrd2, wrd3;
			if( list & 0x80 ) {
				from_exten(fp_reg[reg],&wrd1, &wrd2, &wrd3);
				put_long (ad, wrd1);
				ad += 4;
				put_long (ad, wrd2);
				ad += 4;
				put_long (ad, wrd3);
				ad += 4;
			}
			list <<= 1;
		}
		dump_fp_regs( "END  ");
	}
}


/* ---------------------------- FMOVEM CONSTANT ROM -> FPP ---------------------------- */

void REGPARAM2 fpuop_do_fldpi( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: Pi\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_pi, sizeof(float80_s) );
	sw = SW_FINITE | INEX2;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fldlg2( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: Log 10 (2)\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_lg2, sizeof(float80_s) );
	sw = SW_FINITE | INEX2;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_load_const_e( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: e\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_e, sizeof(float80_s) );
	sw = SW_FINITE | INEX2;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fldl2e( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: Log 2 (e)\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_l2e, sizeof(float80_s) );
	sw = SW_FINITE | INEX2;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_load_const_log_10_e( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: Log 10 (e)\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_log_10_e, sizeof(float80_s) );
	sw = SW_FINITE | INEX2;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fldz( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: zero\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_z, sizeof(float80_s) );
	sw = SW_Z;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fldln2( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: ln(2)\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_ln2, sizeof(float80_s) );
	sw = SW_FINITE | INEX2;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_load_const_ln_10( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: ln(10)\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_ln_10, sizeof(float80_s) );
	sw = SW_FINITE | INEX2;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fld1( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e0\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_1, sizeof(float80_s) );
	sw = SW_FINITE;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_load_const_1e1( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e1\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_1e1, sizeof(float80_s) );
	sw = SW_FINITE;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_load_const_1e2( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e2\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_1e2, sizeof(float80_s) );
	sw = SW_FINITE;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_load_const_1e4( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e4\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_1e4, sizeof(float80_s) );
	sw = SW_FINITE;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_load_const_1e8( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e8\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_1e8, sizeof(float80_s) );
	sw = SW_FINITE | INEX2; // Is it really INEX2?
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_load_const_1e16( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e16\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_1e16, sizeof(float80_s) );
	sw = SW_FINITE | INEX2;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_load_const_1e32( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e32\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_1e32, sizeof(float80_s) );
	sw = SW_FINITE | INEX2;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_load_const_1e64( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e64\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_1e64, sizeof(float80_s) );
	sw = SW_FINITE | INEX2;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_load_const_1e128( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e128\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_1e128, sizeof(float80_s) );
	sw = SW_FINITE | INEX2;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_load_const_1e256( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e256\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_1e256, sizeof(float80_s) );
	sw = SW_FINITE | INEX2;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_load_const_1e512( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e512\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_1e512, sizeof(float80_s) );
	sw = SW_FINITE | INEX2;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_load_const_1e1024( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e1024\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_1e1024, sizeof(float80_s) );
	sw = SW_FINITE | INEX2;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_load_const_1e2048( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e2048\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_1e2048, sizeof(float80_s) );
	sw = SW_FINITE | INEX2;
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_load_const_1e4096( uae_u32 opcode, uae_u16 extra )
{
	D(bug("FMOVECR memory->FPP FP const: 1.0e4096\r\n"));
	memcpy( fp_reg[(extra>>7) & 7], const_1e4096, sizeof(float80_s) );
	sw = SW_FINITE | INEX2;
	dump_fp_regs( "END  ");
}


/* ---------------------------- ALU ---------------------------- */

void REGPARAM2 fpuop_do_fmove( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FMOVE %s\r\n",etos(src)));
	do_fmove( fp_reg[reg], src );
	build_ex_status();
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fint( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FINT %s, opcode=%X, extra=%X, ta %X\r\n",etos(src),opcode,extra,m68k_getpc()));
	do_fint( fp_reg[reg], src );
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fsinh( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FSINH %s\r\n",etos(src)));
	do_fsinh( fp_reg[reg], src );
	build_ex_status();
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fintrz( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FINTRZ %s\r\n",etos(src)));
	do_fintrz( fp_reg[reg], src );
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fsqrt( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FSQRT %s\r\n",etos(src)));
	do_fsqrt( fp_reg[reg], src );
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_flognp1( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FLOGNP1 %s\r\n",etos(src)));
	do_flognp1( fp_reg[reg], src );
	build_ex_status();
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fetoxm1( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FETOXM1 %s\r\n",etos(src)));
	do_fetoxm1( fp_reg[reg], src );
	build_ex_status();
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_ftanh( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FTANH %s\r\n",etos(src)));
	do_ftanh( fp_reg[reg], src );
	build_ex_status();
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fatan( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FATAN %s\r\n",etos(src)));
	do_fatan( fp_reg[reg], src );
	build_ex_status();
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fasin( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FASIN %s\r\n",etos(src)));
	do_fasin( fp_reg[reg], src );
	build_ex_status();
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fatanh( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FATANH %s\r\n",etos(src)));
	do_fatanh( fp_reg[reg], src );
	build_ex_status();
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fsin( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FSIN %s\r\n",etos(src)));
	do_fsin( fp_reg[reg], src );
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_ftan( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FTAN %s\r\n",etos(src)));
	do_ftan( fp_reg[reg], src );
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fetox( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FETOX %s\r\n",etos(src)));
	do_fetox( fp_reg[reg], src );
	build_ex_status();
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_ftwotox( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FTWOTOX %s\r\n",etos(src)));
	do_ftwotox( fp_reg[reg], src );
	build_ex_status();
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_ftentox( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FTENTOX %s\r\n",etos(src)));
	do_ftentox( fp_reg[reg], src );
	build_ex_status();
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_flogn( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FLOGN %s\r\n",etos(src)));
	do_flogn( fp_reg[reg], src );
	build_ex_status();
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_flog10( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FLOG10 %s\r\n",etos(src)));
	do_flog10( fp_reg[reg], src );
	build_ex_status();
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_flog2( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FLOG2 %s\r\n",etos(src)));
	do_flog2( fp_reg[reg], src );
	build_ex_status();
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fabs( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FABS %s\r\n",etos(src)));
	do_fabs( fp_reg[reg], src );
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fcosh( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FCOSH %s\r\n",etos(src)));
	do_fcosh( fp_reg[reg], src );
	build_ex_status();
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fneg( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FNEG %s\r\n",etos(src)));
	do_fneg( fp_reg[reg], src );
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_facos( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FACOS %s\r\n",etos(src)));
	do_facos( fp_reg[reg], src );
	build_ex_status();
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fcos( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FCOS %s\r\n",etos(src)));
	do_fcos( fp_reg[reg], src );
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fgetexp( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FGETEXP %s\r\n",etos(src)));

	if( IS_INFINITY(src) ) {
		MAKE_NAN( fp_reg[reg] );
		do_ftst( fp_reg[reg] );
		sw |= SW_IE;
	} else {
		do_fgetexp( fp_reg[reg], src );
	}
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fgetman( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FGETMAN %s\r\n",etos(src)));
	if( IS_INFINITY(src) ) {
		MAKE_NAN( fp_reg[reg] );
		do_ftst( fp_reg[reg] );
		sw |= SW_IE;
	} else {
		do_fgetman( fp_reg[reg], src );
	}
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fdiv( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FDIV %s\r\n",etos(src)));
	do_fdiv( fp_reg[reg], src );
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fmod( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FMOD %s\r\n",etos(src)));

#if USE_3_BIT_QUOTIENT
	do_fmod( fp_reg[reg], src );
#else
	if( (cw & X86_ROUND_CONTROL_MASK) == CW_RC_ZERO ) {
		do_fmod_dont_set_cw( fp_reg[reg], src );
	} else {
		do_fmod( fp_reg[reg], src );
	}
#endif
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_frem( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FREM %s\r\n",etos(src)));
#if USE_3_BIT_QUOTIENT
	do_frem( fp_reg[reg], src );
#else
	if( (cw & X86_ROUND_CONTROL_MASK) == CW_RC_NEAR ) {
		do_frem_dont_set_cw( fp_reg[reg], src );
	} else {
		do_frem( fp_reg[reg], src );
	}
#endif
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fadd( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FADD %s\r\n",etos(src)));
	do_fadd( fp_reg[reg], src );
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fmul( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FMUL %s\r\n",etos(src)));
	do_fmul( fp_reg[reg], src );
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fsgldiv( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FSGLDIV %s\r\n",etos(src)));
	do_fsgldiv( fp_reg[reg], src );
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fscale( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FSCALE %s, opcode=%X, extra=%X, ta %X\r\n",etos(src),opcode,extra,m68k_getpc()));
	if( IS_INFINITY(fp_reg[reg]) ) {
		MAKE_NAN( fp_reg[reg] );
		do_ftst( fp_reg[reg] );
		sw |= SW_IE;
	} else {
		// When the absolute value of the source operand is >= 2^14,
		// an overflow or underflow always results.
		do_fscale( fp_reg[reg], src );
	}
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fsglmul( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FSGLMUL %s\r\n",etos(src)));
	do_fsglmul( fp_reg[reg], src );
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fsub( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FSUB %s\r\n",etos(src)));
	do_fsub( fp_reg[reg], src );
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fsincos( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FSINCOS %s\r\n",etos(src)));
	do_fsincos( fp_reg[reg], fp_reg[extra & 7], src );
	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_fcmp( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FCMP %s\r\n",etos(src)));

	if( IS_INFINITY(src) ) {
		if( IS_NEGATIVE(src) ) {
			if( IS_INFINITY(fp_reg[reg]) && IS_NEGATIVE(fp_reg[reg]) ) {
				sw = SW_Z | SW_N;
				D(bug("-INF FCMP -INF -> NZ\r\n"));
			} else {
				sw = SW_FINITE;
				D(bug("X FCMP -INF -> None\r\n"));
			}
		} else {
			if( IS_INFINITY(fp_reg[reg]) && !IS_NEGATIVE(fp_reg[reg]) ) {
				sw = SW_Z;
				D(bug("+INF FCMP +INF -> Z\r\n"));
			} else {
				sw = SW_N;
				D(bug("X FCMP +INF -> N\r\n"));
			}
		}
	} else if( IS_INFINITY(fp_reg[reg]) ) {
		if( IS_NEGATIVE(fp_reg[reg]) ) {
			sw = SW_N;
			D(bug("-INF FCMP X -> Negative\r\n"));
		} else {
			sw = SW_FINITE;
			D(bug("+INF FCMP X -> None\r\n"));
		}
	} else {
		do_fcmp( fp_reg[reg], src );
	}

	dump_fp_regs( "END  ");
}

void REGPARAM2 fpuop_do_ftst( uae_u32 opcode, uae_u16 extra )
{
	int reg = (extra >> 7) & 7;
  float80_s src;
	if (get_fp_value (opcode, extra, src) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		dump_fp_regs( "END  ");
		return;
	}
	D(bug("FTST %s\r\n",etos(src)));
	do_ftst( src );
	build_ex_status();
	dump_fp_regs( "END  ");
}



/* ---------------------------- SET FPU MODE ---------------------------- */

void fpu_set_integral_fpu( bool is_integral )
{
	is_integral_68040_fpu = (uae_u32)is_integral;
}


/* ---------------------------- SETUP TABLES ---------------------------- */

static void build_fpp_opp_lookup_table()
{
	for( uae_u32 opcode=0; opcode<=0x38; opcode+=8 ) {
		for( uae_u32 extra=0; extra<65536; extra++ ) {
			uae_u32 mask = (extra & 0xFC7F) | ((opcode & 0x0038) << 4);
			fpufunctbl[mask] = fpuop_illg;

			switch ((extra >> 13) & 0x7) {
				case 3:
					fpufunctbl[mask] = fpuop_fmove_2_ea;
					break;
				case 4:
				case 5:
					if ((opcode & 0x38) == 0) {
						if (extra & 0x2000) { // dr bit
							switch( extra & 0x1C00 ) {
								case 0x0000:
									fpufunctbl[mask] = fpuop_fmovem_none_2_Dreg;
									break;
								case 0x0400:
									fpufunctbl[mask] = fpuop_fmovem_fpiar_2_Dreg;
									break;
								case 0x0800:
									fpufunctbl[mask] = fpuop_fmovem_fpsr_2_Dreg;
									break;
								case 0x0C00:
									fpufunctbl[mask] = fpuop_fmovem_fpsr_fpiar_2_Dreg;
									break;
								case 0x1000:
									fpufunctbl[mask] = fpuop_fmovem_fpcr_2_Dreg;
									break;
								case 0x1400:
									fpufunctbl[mask] = fpuop_fmovem_fpcr_fpiar_2_Dreg;
									break;
								case 0x1800:
									fpufunctbl[mask] = fpuop_fmovem_fpcr_fpsr_2_Dreg;
									break;
								case 0x1C00:
									fpufunctbl[mask] = fpuop_fmovem_fpcr_fpsr_fpiar_2_Dreg;
									break;
							}
						} else {
							switch( extra & 0x1C00 ) {
								case 0x0000:
									fpufunctbl[mask] = fpuop_fmovem_Dreg_2_none;
									break;
								case 0x0400:
									fpufunctbl[mask] = fpuop_fmovem_Dreg_2_fpiar;
									break;
								case 0x0800:
									fpufunctbl[mask] = fpuop_fmovem_Dreg_2_fpsr;
									break;
								case 0x0C00:
									fpufunctbl[mask] = fpuop_fmovem_Dreg_2_fpsr_fpiar;
									break;
								case 0x1000:
									fpufunctbl[mask] = fpuop_fmovem_Dreg_2_fpcr;
									break;
								case 0x1400:
									fpufunctbl[mask] = fpuop_fmovem_Dreg_2_fpcr_fpiar;
									break;
								case 0x1800:
									fpufunctbl[mask] = fpuop_fmovem_Dreg_2_fpcr_fpsr;
									break;
								case 0x1C00:
									fpufunctbl[mask] = fpuop_fmovem_Dreg_2_fpcr_fpsr_fpiar;
									break;
							}
						}
					} else if ((opcode & 0x38) == 8) {
						if (extra & 0x2000) { // dr bit
							switch( extra & 0x1C00 ) {
								case 0x0000:
									fpufunctbl[mask] = fpuop_fmovem_none_2_Areg;
									break;
								case 0x0400:
									fpufunctbl[mask] = fpuop_fmovem_fpiar_2_Areg;
									break;
								case 0x0800:
									fpufunctbl[mask] = fpuop_fmovem_fpsr_2_Areg;
									break;
								case 0x0C00:
									fpufunctbl[mask] = fpuop_fmovem_fpsr_fpiar_2_Areg;
									break;
								case 0x1000:
									fpufunctbl[mask] = fpuop_fmovem_fpcr_2_Areg;
									break;
								case 0x1400:
									fpufunctbl[mask] = fpuop_fmovem_fpcr_fpiar_2_Areg;
									break;
								case 0x1800:
									fpufunctbl[mask] = fpuop_fmovem_fpcr_fpsr_2_Areg;
									break;
								case 0x1C00:
									fpufunctbl[mask] = fpuop_fmovem_fpcr_fpsr_fpiar_2_Areg;
									break;
							}
						} else {
							switch( extra & 0x1C00 ) {
								case 0x0000:
									fpufunctbl[mask] = fpuop_fmovem_Areg_2_none;
									break;
								case 0x0400:
									fpufunctbl[mask] = fpuop_fmovem_Areg_2_fpiar;
									break;
								case 0x0800:
									fpufunctbl[mask] = fpuop_fmovem_Areg_2_fpsr;
									break;
								case 0x0C00:
									fpufunctbl[mask] = fpuop_fmovem_Areg_2_fpsr_fpiar;
									break;
								case 0x1000:
									fpufunctbl[mask] = fpuop_fmovem_Areg_2_fpcr;
									break;
								case 0x1400:
									fpufunctbl[mask] = fpuop_fmovem_Areg_2_fpcr_fpiar;
									break;
								case 0x1800:
									fpufunctbl[mask] = fpuop_fmovem_Areg_2_fpcr_fpsr;
									break;
								case 0x1C00:
									fpufunctbl[mask] = fpuop_fmovem_Areg_2_fpcr_fpsr_fpiar;
									break;
							}
						}
					} else if (extra & 0x2000) {
						if ((opcode & 0x38) == 0x20) {
							switch( extra & 0x1C00 ) {
								case 0x0000:
									fpufunctbl[mask] = fpuop_fmovem_none_2_Mem_predecrement;
									break;
								case 0x0400:
									fpufunctbl[mask] = fpuop_fmovem_fpiar_2_Mem_predecrement;
									break;
								case 0x0800:
									fpufunctbl[mask] = fpuop_fmovem_fpsr_2_Mem_predecrement;
									break;
								case 0x0C00:
									fpufunctbl[mask] = fpuop_fmovem_fpsr_fpiar_2_Mem_predecrement;
									break;
								case 0x1000:
									fpufunctbl[mask] = fpuop_fmovem_fpcr_2_Mem_predecrement;
									break;
								case 0x1400:
									fpufunctbl[mask] = fpuop_fmovem_fpcr_fpiar_2_Mem_predecrement;
									break;
								case 0x1800:
									fpufunctbl[mask] = fpuop_fmovem_fpcr_fpsr_2_Mem_predecrement;
									break;
								case 0x1C00:
									fpufunctbl[mask] = fpuop_fmovem_fpcr_fpsr_fpiar_2_Mem_predecrement;
									break;
							}
						} else if ((opcode & 0x38) == 0x18) {
							switch( extra & 0x1C00 ) {
								case 0x0000:
									fpufunctbl[mask] = fpuop_fmovem_none_2_Mem_postincrement;
									break;
								case 0x0400:
									fpufunctbl[mask] = fpuop_fmovem_fpiar_2_Mem_postincrement;
									break;
								case 0x0800:
									fpufunctbl[mask] = fpuop_fmovem_fpsr_2_Mem_postincrement;
									break;
								case 0x0C00:
									fpufunctbl[mask] = fpuop_fmovem_fpsr_fpiar_2_Mem_postincrement;
									break;
								case 0x1000:
									fpufunctbl[mask] = fpuop_fmovem_fpcr_2_Mem_postincrement;
									break;
								case 0x1400:
									fpufunctbl[mask] = fpuop_fmovem_fpcr_fpiar_2_Mem_postincrement;
									break;
								case 0x1800:
									fpufunctbl[mask] = fpuop_fmovem_fpcr_fpsr_2_Mem_postincrement;
									break;
								case 0x1C00:
									fpufunctbl[mask] = fpuop_fmovem_fpcr_fpsr_fpiar_2_Mem_postincrement;
									break;
							}
						} else {
							switch( extra & 0x1C00 ) {
								case 0x0000:
									fpufunctbl[mask] = fpuop_fmovem_none_2_Mem;
									break;
								case 0x0400:
									fpufunctbl[mask] = fpuop_fmovem_fpiar_2_Mem;
									break;
								case 0x0800:
									fpufunctbl[mask] = fpuop_fmovem_fpsr_2_Mem;
									break;
								case 0x0C00:
									fpufunctbl[mask] = fpuop_fmovem_fpsr_fpiar_2_Mem;
									break;
								case 0x1000:
									fpufunctbl[mask] = fpuop_fmovem_fpcr_2_Mem;
									break;
								case 0x1400:
									fpufunctbl[mask] = fpuop_fmovem_fpcr_fpiar_2_Mem;
									break;
								case 0x1800:
									fpufunctbl[mask] = fpuop_fmovem_fpcr_fpsr_2_Mem;
									break;
								case 0x1C00:
									fpufunctbl[mask] = fpuop_fmovem_fpcr_fpsr_fpiar_2_Mem;
									break;
							}
						}
					} else {
						if ((opcode & 0x38) == 0x20) {
							switch( extra & 0x1C00 ) {
								case 0x0000:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_none_predecrement;
									break;
								case 0x0400:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpiar_predecrement;
									break;
								case 0x0800:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpsr_predecrement;
									break;
								case 0x0C00:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpsr_fpiar_predecrement;
									break;
								case 0x1000:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpcr_predecrement;
									break;
								case 0x1400:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpcr_fpiar_predecrement;
									break;
								case 0x1800:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpcr_fpsr_predecrement;
									break;
								case 0x1C00:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpcr_fpsr_fpiar_predecrement;
									break;
							}
						} else if ((opcode & 0x38) == 0x18) {
							switch( extra & 0x1C00 ) {
								case 0x0000:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_none_postincrement;
									break;
								case 0x0400:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpiar_postincrement;
									break;
								case 0x0800:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpsr_postincrement;
									break;
								case 0x0C00:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpsr_fpiar_postincrement;
									break;
								case 0x1000:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpcr_postincrement;
									break;
								case 0x1400:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpcr_fpiar_postincrement;
									break;
								case 0x1800:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpcr_fpsr_postincrement;
									break;
								case 0x1C00:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpcr_fpsr_fpiar_postincrement;
									break;
							}
						} else {
							switch( extra & 0x1C00 ) {
								case 0x0000:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_none_2_Mem;
									break;
								case 0x0400:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpiar_2_Mem;
									break;
								case 0x0800:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpsr_2_Mem;
									break;
								case 0x0C00:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpsr_fpiar_2_Mem;
									break;
								case 0x1000:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpcr_2_Mem;
									break;
								case 0x1400:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpcr_fpiar_2_Mem;
									break;
								case 0x1800:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpcr_fpsr_2_Mem;
									break;
								case 0x1C00:
									fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpcr_fpsr_fpiar_2_Mem;
									break;
							}
						}
					break;
				case 6:
					switch ((extra >> 11) & 3) {
						case 0:	/* static pred */
							if ((opcode & 0x38) == 0x18) // post-increment?
								fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpp_static_pred_postincrement;
							else if ((opcode & 0x38) == 0x20) // pre-decrement?
								fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpp_static_pred_predecrement;
							else
								fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpp_static_pred;
							break;
						case 1:	/* dynamic pred */
							if ((opcode & 0x38) == 0x18) // post-increment?
								fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpp_dynamic_pred_postincrement;
							else if ((opcode & 0x38) == 0x20) // pre-decrement?
								fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpp_dynamic_pred_predecrement;
							else
								fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpp_dynamic_pred;
							break;
						case 2:	/* static postinc */
							if ((opcode & 0x38) == 0x18) // post-increment?
								fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpp_static_postinc_postincrement;
							else if ((opcode & 0x38) == 0x20) // pre-decrement?
								fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpp_static_postinc_predecrement;
							else
								fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpp_static_postinc;
							break;
						case 3:	/* dynamic postinc */
							if ((opcode & 0x38) == 0x18) // post-increment?
								fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpp_dynamic_postinc_postincrement;
							else if ((opcode & 0x38) == 0x20) // pre-decrement?
								fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpp_dynamic_postinc_predecrement;
							else
								fpufunctbl[mask] = fpuop_fmovem_Mem_2_fpp_dynamic_postinc;
							break;
					}
					break;
				case 7:
					switch ((extra >> 11) & 3) {
						case 0:	/* static pred */
							if ((opcode & 0x38) == 0x18) // post-increment?
								fpufunctbl[mask] = fpuop_fmovem_fpp_2_Mem_static_pred_postincrement;
							else if ((opcode & 0x38) == 0x20) // pre-decrement?
								fpufunctbl[mask] = fpuop_fmovem_fpp_2_Mem_static_pred_predecrement;
							else
								fpufunctbl[mask] = fpuop_fmovem_fpp_2_Mem_static_pred;
							break;
						case 1:	/* dynamic pred */
							if ((opcode & 0x38) == 0x18) // post-increment?
								fpufunctbl[mask] = fpuop_fmovem_fpp_2_Mem_dynamic_pred_postincrement;
							else if ((opcode & 0x38) == 0x20) // pre-decrement?
								fpufunctbl[mask] = fpuop_fmovem_fpp_2_Mem_dynamic_pred_predecrement;
							else
								fpufunctbl[mask] = fpuop_fmovem_fpp_2_Mem_dynamic_pred;
							break;
						case 2:	/* static postinc */
							if ((opcode & 0x38) == 0x18) // post-increment?
								fpufunctbl[mask] = fpuop_fmovem_fpp_2_Mem_static_postinc_postincrement;
							else if ((opcode & 0x38) == 0x20) // pre-decrement?
								fpufunctbl[mask] = fpuop_fmovem_fpp_2_Mem_static_postinc_predecrement;
							else
								fpufunctbl[mask] = fpuop_fmovem_fpp_2_Mem_static_postinc;
							break;
						case 3:	/* dynamic postinc */
							if ((opcode & 0x38) == 0x18) // post-increment?
								fpufunctbl[mask] = fpuop_fmovem_fpp_2_Mem_dynamic_postinc_postincrement;
							else if ((opcode & 0x38) == 0x20) // pre-decrement?
								fpufunctbl[mask] = fpuop_fmovem_fpp_2_Mem_dynamic_postinc_predecrement;
							else
								fpufunctbl[mask] = fpuop_fmovem_fpp_2_Mem_dynamic_postinc;
							break;
					}
					break;
				case 0:
				case 2:
					if ((extra & 0xfc00) == 0x5c00) {
						switch (extra & 0x7f) {
							case 0x00:
								fpufunctbl[mask] = fpuop_do_fldpi;
								break;
							case 0x0b:
								fpufunctbl[mask] = fpuop_do_fldlg2;
								break;
							case 0x0c:
								fpufunctbl[mask] = fpuop_do_load_const_e;
								break;
							case 0x0d:
								fpufunctbl[mask] = fpuop_do_fldl2e;
								break;
							case 0x0e:
								fpufunctbl[mask] = fpuop_do_load_const_log_10_e;
								break;
							case 0x0f:
								fpufunctbl[mask] = fpuop_do_fldz;
								break;
							case 0x30:
								fpufunctbl[mask] = fpuop_do_fldln2;
								break;
							case 0x31:
								fpufunctbl[mask] = fpuop_do_load_const_ln_10;
								break;
							case 0x32:
								fpufunctbl[mask] = fpuop_do_fld1;
								break;
							case 0x33:
								fpufunctbl[mask] = fpuop_do_load_const_1e1;
								break;
							case 0x34:
								fpufunctbl[mask] = fpuop_do_load_const_1e2;
								break;
							case 0x35:
								fpufunctbl[mask] = fpuop_do_load_const_1e4;
								break;
							case 0x36:
								fpufunctbl[mask] = fpuop_do_load_const_1e8;
								break;
							case 0x37:
								fpufunctbl[mask] = fpuop_do_load_const_1e16;
								break;
							case 0x38:
								fpufunctbl[mask] = fpuop_do_load_const_1e32;
								break;
							case 0x39:
								fpufunctbl[mask] = fpuop_do_load_const_1e64;
								break;
							case 0x3a:
								fpufunctbl[mask] = fpuop_do_load_const_1e128;
								break;
							case 0x3b:
								fpufunctbl[mask] = fpuop_do_load_const_1e256;
								break;
							case 0x3c:
								fpufunctbl[mask] = fpuop_do_load_const_1e512;
								break;
							case 0x3d:
								fpufunctbl[mask] = fpuop_do_load_const_1e1024;
								break;
							case 0x3e:
								fpufunctbl[mask] = fpuop_do_load_const_1e2048;
								break;
							case 0x3f:
								fpufunctbl[mask] = fpuop_do_load_const_1e4096;
								break;
						}
						break;
					}
					
					switch (extra & 0x7f) {
						case 0x00:
							fpufunctbl[mask] = fpuop_do_fmove;
							break;
						case 0x01:
							fpufunctbl[mask] = fpuop_do_fint;
							break;
						case 0x02:
							fpufunctbl[mask] = fpuop_do_fsinh;
							break;
						case 0x03:
							fpufunctbl[mask] = fpuop_do_fintrz;
							break;
						case 0x04:
							fpufunctbl[mask] = fpuop_do_fsqrt;
							break;
						case 0x06:
							fpufunctbl[mask] = fpuop_do_flognp1;
							break;
						case 0x08:
							fpufunctbl[mask] = fpuop_do_fetoxm1;
							break;
						case 0x09:
							fpufunctbl[mask] = fpuop_do_ftanh;
							break;
						case 0x0a:
							fpufunctbl[mask] = fpuop_do_fatan;
							break;
						case 0x0c:
							fpufunctbl[mask] = fpuop_do_fasin;
							break;
						case 0x0d:
							fpufunctbl[mask] = fpuop_do_fatanh;
							break;
						case 0x0e:
							fpufunctbl[mask] = fpuop_do_fsin;
							break;
						case 0x0f:
							fpufunctbl[mask] = fpuop_do_ftan;
							break;
						case 0x10:
							fpufunctbl[mask] = fpuop_do_fetox;
							break;
						case 0x11:
							fpufunctbl[mask] = fpuop_do_ftwotox;
							break;
						case 0x12:
							fpufunctbl[mask] = fpuop_do_ftentox;
							break;
						case 0x14:
							fpufunctbl[mask] = fpuop_do_flogn;
							break;
						case 0x15:
							fpufunctbl[mask] = fpuop_do_flog10;
							break;
						case 0x16:
							fpufunctbl[mask] = fpuop_do_flog2;
							break;
						case 0x18:
							fpufunctbl[mask] = fpuop_do_fabs;
							break;
						case 0x19:
							fpufunctbl[mask] = fpuop_do_fcosh;
							break;
						case 0x1a:
							fpufunctbl[mask] = fpuop_do_fneg;
							break;
						case 0x1c:
							fpufunctbl[mask] = fpuop_do_facos;
							break;
						case 0x1d:
							fpufunctbl[mask] = fpuop_do_fcos;
							break;
						case 0x1e:
							fpufunctbl[mask] = fpuop_do_fgetexp;
							break;
						case 0x1f:
							fpufunctbl[mask] = fpuop_do_fgetman;
							break;
						case 0x20:
							fpufunctbl[mask] = fpuop_do_fdiv;
							break;
						case 0x21:
							fpufunctbl[mask] = fpuop_do_fmod;
							break;
						case 0x22:
							fpufunctbl[mask] = fpuop_do_fadd;
							break;
						case 0x23:
							fpufunctbl[mask] = fpuop_do_fmul;
							break;
						case 0x24:
							fpufunctbl[mask] = fpuop_do_fsgldiv;
							break;
						case 0x25:
							fpufunctbl[mask] = fpuop_do_frem;
							break;
						case 0x26:
							fpufunctbl[mask] = fpuop_do_fscale;
							break;
						case 0x27:
							fpufunctbl[mask] = fpuop_do_fsglmul;
							break;
						case 0x28:
							fpufunctbl[mask] = fpuop_do_fsub;
							break;
						case 0x30:
						case 0x31:
						case 0x32:
						case 0x33:
						case 0x34:
						case 0x35:
						case 0x36:
						case 0x37:
							fpufunctbl[mask] = fpuop_do_fsincos;
							break;
						case 0x38:
							fpufunctbl[mask] = fpuop_do_fcmp;
							break;
						case 0x3a:
							fpufunctbl[mask] = fpuop_do_ftst;
							break;
					}
				}
			}
		}
	}
}

static void build_fpsr_lookup_tables()
{
	uae_u32 i;

	// Mapping for "sw" -> fpsr condition code
	for( i=0; i<0x48; i++ ) {
		sw_cond_host2mac[i] = 0;
		switch( (i << 8) & (SW_Z_I_NAN_MASK) ) {
			case SW_UNSUPPORTED:
			case SW_NAN:
			case SW_EMPTY_REGISTER:
				sw_cond_host2mac[i] |= 0x1000000;
				break;
			case SW_FINITE:
			case SW_DENORMAL:
				break;
			case SW_I:
				sw_cond_host2mac[i] |= 0x2000000;
				break;
			case SW_Z:
				sw_cond_host2mac[i] |= 0x4000000;
				break;
		}
		if( (i << 8) & SW_N ) {
			sw_cond_host2mac[i] |= 0x8000000;
		}
	}

	// Mapping for fpsr condition code -> "sw"
	for( i=0; i<16; i++ ) {
		if( (i << 24) & 0x1000000 ) {
			sw_cond_mac2host[i] = SW_NAN;
		} else if( (i << 24) & 0x4000000 ) {
			sw_cond_mac2host[i] = SW_Z;
		} else if( (i << 24) & 0x2000000 ) {
			sw_cond_mac2host[i] = SW_I;
		} else {
			sw_cond_mac2host[i] = SW_FINITE;
		}
		if( (i << 24) & 0x8000000 ) {
			sw_cond_mac2host[i] |= SW_N;
		}
	}

	// Mapping for "sw" -> fpsr exception byte
	for( i=0; i<0x80; i++ ) {
		exception_host2mac[i] = 0;

		if(i & SW_FAKE_BSUN) {
			exception_host2mac[i] |= BSUN;
		}
		// precision exception
		if(i & SW_PE) {
			exception_host2mac[i] |= INEX2;
		}
		// underflow exception
		if(i & SW_UE) {
			exception_host2mac[i] |= UNFL;
		}
		// overflow exception
		if(i & SW_OE) {
			exception_host2mac[i] |= OVFL;
		}
		// zero divide exception
		if(i & SW_ZE) {
			exception_host2mac[i] |= DZ;
		}
		// denormalized operand exception.
		// wrong, but should not get here, normalization is done in elsewhere
		if(i & SW_DE) {
			exception_host2mac[i] |= SNAN;
		}
		// invalid operation exception
		if(i & SW_IE) {
			exception_host2mac[i] |= OPERR;
		}
	}

	// Mapping for fpsr exception byte -> "sw"
	for( i=0; i<0x100; i++ ) {
		int fpsr = (i << 8);
		exception_mac2host[i] = 0;

		// BSUN; make sure that you don't generate FPU stack faults.
		if(fpsr & BSUN) {
			exception_mac2host[i] |= SW_FAKE_BSUN;
		}
		// precision exception
		if(fpsr & INEX2) {
			exception_mac2host[i] |= SW_PE;
		}
		// underflow exception
		if(fpsr & UNFL) {
			exception_mac2host[i] |= SW_UE;
		}
		// overflow exception
		if(fpsr & OVFL) {
			exception_mac2host[i] |= SW_OE;
		}
		// zero divide exception
		if(fpsr & DZ) {
			exception_mac2host[i] |= SW_ZE;
		}
		// denormalized operand exception
		if(fpsr & SNAN) {
			exception_mac2host[i] |= SW_DE; //Wrong
		}
		// invalid operation exception
		if(fpsr & OPERR) {
			exception_mac2host[i] |= SW_IE;
		}
	}

	/*
		68881/68040 accrued exceptions accumulate as follows:
			Accrued.IOP		|= (Exception.SNAN | Exception.OPERR)
			Accrued.OVFL	|= (Exception.OVFL)
			Accrued.UNFL	|= (Exception.UNFL | Exception.INEX2)
			Accrued.DZ		|= (Exception.DZ)
			Accrued.INEX	|= (Exception.INEX1 | Exception.INEX2 | Exception.OVFL)
	*/

	// Mapping for "sw_accrued" -> fpsr accrued exception byte
	for( i=0; i<0x40; i++ ) {
		accrued_exception_host2mac[i] = 0;

		// precision exception
		if(i & SW_PE) {
			accrued_exception_host2mac[i] |= ACCR_INEX;
		}
		// underflow exception
		if(i & SW_UE) {
			accrued_exception_host2mac[i] |= ACCR_UNFL;
		}
		// overflow exception
		if(i & SW_OE) {
			accrued_exception_host2mac[i] |= ACCR_OVFL;
		}
		// zero divide exception
		if(i & SW_ZE) {
			accrued_exception_host2mac[i] |= ACCR_DZ;
		}
		// denormalized operand exception
		if(i & SW_DE) {
			accrued_exception_host2mac[i] |= ACCR_IOP; //??????
		}
		// invalid operation exception
		if(i & SW_IE) {
			accrued_exception_host2mac[i] |= ACCR_IOP;
		}
	}

	// Mapping for fpsr accrued exception byte -> "sw_accrued"
	for( i=0; i<0x20; i++ ) {
		int fpsr = (i << 3);
		accrued_exception_mac2host[i] = 0;

		// precision exception
		if(fpsr & ACCR_INEX) {
			accrued_exception_mac2host[i] |= SW_PE;
		}
		// underflow exception
		if(fpsr & ACCR_UNFL) {
			accrued_exception_mac2host[i] |= SW_UE;
		}
		// overflow exception
		if(fpsr & ACCR_OVFL) {
			accrued_exception_mac2host[i] |= SW_OE;
		}
		// zero divide exception
		if(fpsr & ACCR_DZ) {
			accrued_exception_mac2host[i] |= SW_ZE;
		}
		// What about SW_DE; //??????
		// invalid operation exception
		if(fpsr & ACCR_IOP) {
			accrued_exception_mac2host[i] |= SW_IE;
		}
	}
}

/* ---------------------------- CONSTANTS ---------------------------- */

static void set_constant( float80 f, char *name, double value, uae_s32 mult )
{
	FPU_CONSISTENCY_CHECK_START();
	if(mult == 1) {
/*		_asm {
			MOV			ESI, [f]
			FLD     QWORD PTR [value]
			FSTP    TBYTE PTR [ESI]
		} */
		_ASM(	"fldl	%1\n"
				"fstpt	%0\n"
			:	"=m" (*f)
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
		_ASM(	"fildl	%2\n"
				"fldl	%1\n"
				"fmul	\n"
				"fstpt	%0\n"
			:	"=m" (*f)
			:	"m" (value), "m" (mult)
			);
	}
	D(bug("set_constant (%s,%.04f) = %s\r\n",name,(float)value,etos(f)));
	FPU_CONSISTENCY_CHECK_STOP( mult==1 ? "set_constant(mult==1)" : "set_constant(mult>1)" );
}

static void do_fldpi( float80 dest ) REGPARAM;
static void do_fldpi( float80 dest )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		FLDPI
		FXAM
    FNSTSW  sw
		MOV			EDI, [dest]
		FSTP    TBYTE PTR [EDI]
	} */
	_ASM(	"fldpi	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (sw), "=m" (*dest)
		);
	FPU_CONSISTENCY_CHECK_STOP("do_fldpi");
}

static void do_fldlg2( float80 dest ) REGPARAM;
static void do_fldlg2( float80 dest )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		FLDLG2
		FXAM
    FNSTSW  sw
		MOV			EDI, [dest]
		FSTP    TBYTE PTR [EDI]
	} */
	_ASM(	"fldlg2	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (sw), "=m" (*dest)
		);
	FPU_CONSISTENCY_CHECK_STOP("do_fldlg2");
}

static void do_fldl2e( float80 dest ) REGPARAM;
static void do_fldl2e( float80 dest )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		FLDL2E
		FXAM
    FNSTSW  sw
		MOV			EDI, [dest]
		FSTP    TBYTE PTR [EDI]
	} */
	_ASM(	"fldl2e	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (sw), "=m" (*dest)
		);
	FPU_CONSISTENCY_CHECK_STOP("do_fldl2e");
}

static void do_fldz( float80 dest ) REGPARAM;
static void do_fldz( float80 dest )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		FLDZ
		FXAM
    FNSTSW  sw
		MOV			EDI, [dest]
		FSTP    TBYTE PTR [EDI]
	} */
	_ASM(	"fldz	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (sw), "=m" (*dest)
		);
	FPU_CONSISTENCY_CHECK_STOP("do_fldz");
}

static void do_fldln2( float80 dest ) REGPARAM;
static void do_fldln2( float80 dest )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		FLDLN2
		FXAM
    FNSTSW  sw
		MOV			EDI, [dest]
		FSTP    TBYTE PTR [EDI]
	} */
	_ASM(	"fldln2	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (sw), "=m" (*dest)
		);
	FPU_CONSISTENCY_CHECK_STOP("do_fldln2");
}

static void do_fld1( float80 dest ) REGPARAM;
static void do_fld1( float80 dest )
{
	FPU_CONSISTENCY_CHECK_START();
/*	_asm {
		FLD1
		FXAM
    FNSTSW  sw
		MOV			EDI, [dest]
		FSTP    TBYTE PTR [EDI]
	} */
	_ASM(	"fld1	\n"
			"fxam	\n"
			"fnstsw	%0\n"
			"fstpt	%1\n"
		:	"=m" (sw), "=m" (*dest)
		);
	FPU_CONSISTENCY_CHECK_STOP("do_fld1");
}


/* ---------------------------- MAIN INIT ---------------------------- */

void fpu_init( void )
{
/*	_asm {
    FSAVE   m_fpu_state_original
	} */
	_ASM("fsave %0" : "=m" (m_fpu_state_original));

	regs.fpiar = 0;
	regs.fpcr = 0;
	regs.fpsr = 0;

	cw = CW_INITIAL; 
	sw = SW_INITIAL;
	sw_accrued = 0;
	sw_quotient = 0;

	for( int i=0; i<8; i++ ) {
		MAKE_NAN( fp_reg[i] );
	}
	
	build_fpsr_lookup_tables();
	build_fpp_opp_lookup_table();

/*	_asm {
		FNINIT
		FLDCW   cw
	} */
	_ASM("fninit\nfldcw %0" : : "m" (cw));

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
/*	_asm {
		FNINIT
		FLDCW   cw
	} */
	_ASM("fninit\nfldcw %0" : : "m" (cw));
}

void fpu_exit( void )
{
/*	_asm {
    FRSTOR  m_fpu_state_original
		// FNINIT
	} */
	_ASM("frstor %0" : : "m" (m_fpu_state_original));
}

void fpu_reset( void )
{
	fpu_exit();
	fpu_init();
}

#if DEBUG
#pragma optimize("",on)
#endif
