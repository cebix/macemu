/*
 * UAE - The Un*x Amiga Emulator
 *
 * MC68881/MC68040 emulation
 *
 * Copyright 1996 Herman ten Brugge
 *
 *
 * Following fixes by Lauri Pesonen, July 1999:
 *
 * FMOVEM list handling:
 *  The lookup tables did not work correctly, rewritten.
 * FINT:
 *  (int) cast does not work, fixed.
 *  Further, now honors the FPU fpcr rounding modes.
 * FINTRZ:
 *  (int) cast cannot be used, fixed.
 * FGETEXP:
 *  Input argument value 0 returned erroneous value.
 * FMOD:
 *  (int) cast cannot be used. Replaced by proper rounding.
 *  Quotient byte handling was missing.
 * FREM:
 *  (int) cast cannot be used. Replaced by proper rounding.
 *  Quotient byte handling was missing.
 * FSCALE:
 *  Input argument value 0 was not handled correctly.
 * FMOVEM Control Registers to/from address FPU registers An:
 *  A bug caused the code never been called.
 * FMOVEM Control Registers pre-decrement:
 *  Moving of control regs from memory to FPP was not handled properly,
 *  if not all of the three FPU registers were moved.
 * Condition code "Not Greater Than or Equal":
 *  Returned erroneous value.
 * FSINCOS:
 *  Cosine must be loaded first if same register.
 * FMOVECR:
 *  Status register was not updated (yes, this affects it).
 * FMOVE <ea> -> reg:
 *  Status register was not updated (yes, this affects it).
 * FMOVE reg -> reg:
 *  Status register was not updated.
 * FDBcc:
 *  The loop termination condition was wrong.
 *  Possible leak from int16 to int32 fixed.
 * get_fp_value:
 *  Immediate addressing mode && Operation Length == Byte -> 
 *  Use the low-order byte of the extension word.
 * Now FPU fpcr high 16 bits are always read as zeroes, no matter what was
 * written to them.
 *
 * Other:
 * - Optimized single/double/extended to/from conversion functions.
 *   Huge speed boost, but not (necessarily) portable to other systems.
 *   Enabled/disabled by #define FPU_HAVE_IEEE_DOUBLE 1
 * - Optimized versions of FSCALE, FGETEXP, FGETMAN
 * - Conversion routines now handle NaN and infinity better.
 * - Some constants precalculated. Not all compilers can optimize the
 *   expressions previously used.
 *
 * TODO:
 * - Floating point exceptions.
 * - More Infinity/NaN/overflow/underflow checking.
 * - FPU instruction_address (only needed when exceptions are implemented)
 * - Should be written in assembly to support long doubles.
 * - Precision rounding single/double
 */

#include "sysdeps.h"
#include <stdio.h>
#include "memory.h"
#include "readcpu.h"
#include "newcpu.h"
#include "main.h"
#define FPU_IMPLEMENTATION
#include "fpu/fpu.h"
#include "fpu/fpu_ieee.h"

/* Global FPU context */
fpu_t fpu;

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

/* -------------------------------------------------------------------------- */
/* --- Native Support                                                     --- */
/* -------------------------------------------------------------------------- */

#include "fpu/mathlib.h"
#include "fpu/flags.h"
#include "fpu/exceptions.h"
#include "fpu/rounding.h"
#include "fpu/impl.h"

#include "fpu/mathlib.cpp"
#include "fpu/flags.cpp"
#include "fpu/exceptions.cpp"
#include "fpu/rounding.cpp"

/* -------------------------------------------------------------------------- */
/* --- Debugging                                                          --- */
/* -------------------------------------------------------------------------- */

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

PRIVATE void FFPU dump_registers(const char * str)
{
#if FPU_DEBUG && FPU_DUMP_REGISTERS
	char temp_str[512];

	sprintf(temp_str, "%s: %.04f, %.04f, %.04f, %.04f, %.04f, %.04f, %.04f, %.04f\n",
		str,
		fpu_get_register(0), fpu_get_register(1), fpu_get_register(2),
		fpu_get_register(3), fpu_get_register(4), fpu_get_register(5),
		fpu_get_register(6), fpu_get_register(7) );
	
	fpu_debug((temp_str));
#endif
}

PRIVATE void FFPU dump_first_bytes(uae_u8 * buffer, uae_s32 actual)
{
#if FPU_DEBUG && FPU_DUMP_FIRST_BYTES
	char temp_buf1[256], temp_buf2[10];
	int bytes = sizeof(temp_buf1)/3-1-3;
	if (actual < bytes)
		bytes = actual;
	
	temp_buf1[0] = 0;
	for (int  i = 0; i < bytes; i++) {
		sprintf(temp_buf2, "%02x ", (uae_u32)buffer[i]);
		strcat(temp_buf1, temp_buf2);
	}
	
	strcat(temp_buf1, "\n");
	fpu_debug((temp_buf1));
#endif
}

// Quotient Byte is loaded with the sign and least significant
// seven bits of the quotient.
PRIVATE inline void FFPU make_quotient(fpu_register const & quotient, uae_u32 sign)
{
	uae_u32 lsb = (uae_u32)fp_fabs(quotient) & 0x7f;
	FPU fpsr.quotient = sign | (lsb << 16);
}

// to_single
PRIVATE inline fpu_register FFPU make_single(uae_u32 value)
{
#if 1
	// Use a single, otherwise some checks for NaN, Inf, Zero would have to
	// be performed
	fpu_single result = 0; // = 0 to workaround a compiler bug on SPARC
	fp_declare_init_shape(srp, result, single);
	srp->ieee.negative	= (value >> 31) & 1;
	srp->ieee.exponent	= (value >> 23) & FP_SINGLE_EXP_MAX;
	srp->ieee.mantissa	= value & 0x007fffff;
	fpu_debug(("make_single (%X) = %.04f\n",value,(double)result));
	return result;
#elif 0 /* Original code */
	if ((value & 0x7fffffff) == 0)
		return (0.0);
	
	fpu_register result;
	uae_u32 * p = (uae_u32 *)&result;

	uae_u32 sign = (value & 0x80000000);
	uae_u32 exp  = ((value & 0x7F800000) >> 23) + 1023 - 127;

	p[FLO] = value << 29;
	p[FHI] = sign | (exp << 20) | ((value & 0x007FFFFF) >> 3);

	fpu_debug(("make_single (%X) = %.04f\n",value,(double)result));
	
	return(result);
#endif
}

// from_single
PRIVATE inline uae_u32 FFPU extract_single(fpu_register const & src)
{
#if 1
	fpu_single input = (fpu_single) src;
	fp_declare_init_shape(sip, input, single);
	uae_u32 result	= (sip->ieee.negative << 31)
					| (sip->ieee.exponent << 23)
					| sip->ieee.mantissa;
	fpu_debug(("extract_single (%.04f) = %X\n",(double)src,result));
	return result;
#elif 0 /* Original code */
	if (src == 0.0)
		return 0;
	
	uae_u32 result;
	uae_u32 *p = (uae_u32 *)&src;

	uae_u32 sign = (p[FHI] & 0x80000000);
	uae_u32 exp  = (p[FHI] & 0x7FF00000) >> 20;

	if(exp + 127 < 1023) {
		exp = 0;
	} else if(exp > 1023 + 127) {
		exp = 255;
	} else {
		exp = exp + 127 - 1023;
	}

	result = sign | (exp << 23) | ((p[FHI] & 0x000FFFFF) << 3) | (p[FLO] >> 29);

	fpu_debug(("extract_single (%.04f) = %X\n",(double)src,result));

	return (result);
#endif
}

// to_exten
PRIVATE inline fpu_register FFPU make_extended(uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3)
{
	// is it zero?
	if ((wrd1 & 0x7fff0000) == 0 && wrd2 == 0 && wrd3 == 0)
		return 0.0;

	fpu_register result;
#if USE_QUAD_DOUBLE
	// is it NaN?
	if ((wrd1 & 0x7fff0000) == 0x7fff0000 && wrd2 != 0 && wrd3 != 0) {
		make_nan(result);
		return result;
	}
	// is it inf?
	if ((wrd1 & 0x7ffff000) == 0x7fff0000 && wrd2 == 0 && wrd3 == 0) {
		if ((wrd1 & 0x80000000) == 0)
			make_inf_positive(result);
		else
			make_inf_negative(result);
		return result;
	}
	fp_declare_init_shape(srp, result, extended);
	srp->ieee.negative  = (wrd1 >> 31) & 1;
	srp->ieee.exponent  = (wrd1 >> 16) & FP_EXTENDED_EXP_MAX;
	srp->ieee.mantissa0 = (wrd2 >> 16) & 0xffff;
	srp->ieee.mantissa1 = ((wrd2 & 0xffff) << 16) | ((wrd3 >> 16) & 0xffff);
	srp->ieee.mantissa2 = (wrd3 & 0xffff) << 16;
	srp->ieee.mantissa3 = 0;
#elif USE_LONG_DOUBLE
	fp_declare_init_shape(srp, result, extended);
	srp->ieee.negative	= (wrd1 >> 31) & 1;
	srp->ieee.exponent	= (wrd1 >> 16) & FP_EXTENDED_EXP_MAX;
	srp->ieee.mantissa0	= wrd2;
	srp->ieee.mantissa1	= wrd3;
#else
	uae_u32 sgn = (wrd1 >> 31) & 1;
	uae_u32 exp = (wrd1 >> 16) & 0x7fff;

	// the explicit integer bit is not set, must normalize
	if ((wrd2 & 0x80000000) == 0) {
		fpu_debug(("make_extended denormalized mantissa (%X,%X,%X)\n",wrd1,wrd2,wrd3));
		if (wrd2 | wrd3) {
			// mantissa, not fraction.
			uae_u64 man = ((uae_u64)wrd2 << 32) | wrd3;
			while (exp > 0 && (man & UVAL64(0x8000000000000000)) == 0) {
				man <<= 1;
				exp--;
			}
			wrd2 = (uae_u32)(man >> 32);
			wrd3 = (uae_u32)(man & 0xFFFFFFFF);
		}
		else if (exp != 0x7fff) // zero
			exp = FP_EXTENDED_EXP_BIAS - FP_DOUBLE_EXP_BIAS;
	}

	if (exp < FP_EXTENDED_EXP_BIAS - FP_DOUBLE_EXP_BIAS)
		exp = 0;
	else if (exp > FP_EXTENDED_EXP_BIAS + FP_DOUBLE_EXP_BIAS)
		exp = FP_DOUBLE_EXP_MAX;
	else
		exp += FP_DOUBLE_EXP_BIAS - FP_EXTENDED_EXP_BIAS;
	
	fp_declare_init_shape(srp, result, double);
	srp->ieee.negative  = sgn;
	srp->ieee.exponent  = exp;
	// drop the explicit integer bit
	srp->ieee.mantissa0 = (wrd2 & 0x7fffffff) >> 11;
	srp->ieee.mantissa1 = (wrd2 << 21) | (wrd3 >> 11);
#endif
	fpu_debug(("make_extended (%X,%X,%X) = %.04f\n",wrd1,wrd2,wrd3,(double)result));
	return result;
}

/*
	Would be so much easier with full size floats :(
	... this is so vague.
*/
// make_extended_no_normalize
PRIVATE inline void FFPU make_extended_no_normalize(
	uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3, fpu_register & result
)
{
	// is it zero?
	if ((wrd1 && 0x7fff0000) == 0 && wrd2 == 0 && wrd3 == 0) {
		make_zero_positive(result);
		return;
	}
	// is it NaN?
	if ((wrd1 & 0x7fff0000) == 0x7fff0000 && wrd2 != 0 && wrd3 != 0) {
		make_nan(result);
		return;
	}
#if USE_QUAD_DOUBLE
	// is it inf?
	if ((wrd1 & 0x7ffff000) == 0x7fff0000 && wrd2 == 0 && wrd3 == 0) {
		if ((wrd1 & 0x80000000) == 0)
			make_inf_positive(result);
		else
			make_inf_negative(result);
		return;
	}
	fp_declare_init_shape(srp, result, extended);
	srp->ieee.negative  = (wrd1 >> 31) & 1;
	srp->ieee.exponent  = (wrd1 >> 16) & FP_EXTENDED_EXP_MAX;
	srp->ieee.mantissa0 = (wrd2 >> 16) & 0xffff;
	srp->ieee.mantissa1 = ((wrd2 & 0xffff) << 16) | ((wrd3 >> 16) & 0xffff);
	srp->ieee.mantissa2 = (wrd3 & 0xffff) << 16;
	srp->ieee.mantissa3 = 0;
#elif USE_LONG_DOUBLE
	fp_declare_init_shape(srp, result, extended);
	srp->ieee.negative	= (wrd1 >> 31) & 1;
	srp->ieee.exponent	= (wrd1 >> 16) & FP_EXTENDED_EXP_MAX;
	srp->ieee.mantissa0	= wrd2;
	srp->ieee.mantissa1	= wrd3;
#else
	uae_u32 exp = (wrd1 >> 16) & 0x7fff;
	if (exp < FP_EXTENDED_EXP_BIAS - FP_DOUBLE_EXP_BIAS)
		exp = 0;
	else if (exp > FP_EXTENDED_EXP_BIAS + FP_DOUBLE_EXP_BIAS)
		exp = FP_DOUBLE_EXP_MAX;
	else
		exp += FP_DOUBLE_EXP_BIAS - FP_EXTENDED_EXP_BIAS;
	
	fp_declare_init_shape(srp, result, double);
	srp->ieee.negative  = (wrd1 >> 31) & 1;
	srp->ieee.exponent  = exp;
	// drop the explicit integer bit
	srp->ieee.mantissa0 = (wrd2 & 0x7fffffff) >> 11;
	srp->ieee.mantissa1 = (wrd2 << 21) | (wrd3 >> 11);
#endif
	fpu_debug(("make_extended (%X,%X,%X) = %.04f\n",wrd1,wrd2,wrd3,(double)result));
}

// from_exten
PRIVATE inline void FFPU extract_extended(fpu_register const & src,
	uae_u32 * wrd1, uae_u32 * wrd2, uae_u32 * wrd3
)
{
	if (src == 0.0) {
		*wrd1 = *wrd2 = *wrd3 = 0;
		return;
	}
#if USE_QUAD_DOUBLE
	// FIXME: deal with denormals?
	fp_declare_init_shape(srp, src, extended);
	*wrd1 = (srp->ieee.negative << 31) | (srp->ieee.exponent << 16);
	// always set the explicit integer bit.
	*wrd2 = 0x80000000 | (srp->ieee.mantissa0 << 15) | ((srp->ieee.mantissa1 & 0xfffe0000) >> 17);
	*wrd3 = (srp->ieee.mantissa1 << 15) | ((srp->ieee.mantissa2 & 0xfffe0000) >> 17);
#elif USE_LONG_DOUBLE
	uae_u32 *p = (uae_u32 *)&src;
#ifdef WORDS_BIGENDIAN
	*wrd1 = p[0];
	*wrd2 = p[1];
	*wrd3 = p[2];
#else
	*wrd3 = p[0];
	*wrd2 = p[1];
	*wrd1 = ( (uae_u32)*((uae_u16 *)&p[2]) ) << 16;
#endif
#else
	fp_declare_init_shape(srp, src, double);
	fpu_debug(("extract_extended (%d,%d,%X,%X)\n",
			   srp->ieee.negative , srp->ieee.exponent,
			   srp->ieee.mantissa0, srp->ieee.mantissa1));

	uae_u32 exp = srp->ieee.exponent;

	if (exp == FP_DOUBLE_EXP_MAX)
		exp = FP_EXTENDED_EXP_MAX;
	else
		exp += FP_EXTENDED_EXP_BIAS - FP_DOUBLE_EXP_BIAS;

	*wrd1 = (srp->ieee.negative << 31) | (exp << 16);
	// always set the explicit integer bit.
	*wrd2 = 0x80000000 | (srp->ieee.mantissa0 << 11) | ((srp->ieee.mantissa1 & 0xffe00000) >> 21);
	*wrd3 = srp->ieee.mantissa1 << 11;
#endif
	fpu_debug(("extract_extended (%.04f) = %X,%X,%X\n",(double)src,*wrd1,*wrd2,*wrd3));
}

// to_double
PRIVATE inline fpu_register FFPU make_double(uae_u32 wrd1, uae_u32 wrd2)
{
	union {
		fpu_double value;
		uae_u32    parts[2];
	} dest;
#ifdef WORDS_BIGENDIAN
	dest.parts[0] = wrd1;
	dest.parts[1] = wrd2;
#else
	dest.parts[0] = wrd2;
	dest.parts[1] = wrd1;
#endif
	fpu_debug(("make_double (%X,%X) = %.04f\n",wrd1,wrd2,dest.value));
	return (fpu_register)(dest.value);
}

// from_double
PRIVATE inline void FFPU extract_double(fpu_register const & src, 
	uae_u32 * wrd1, uae_u32 * wrd2
)
{
	union {
		fpu_double value;
		uae_u32    parts[2];
	} dest;
	dest.value = (fpu_double)src;
#ifdef WORDS_BIGENDIAN
	*wrd1 = dest.parts[0];
	*wrd2 = dest.parts[1];
#else
	*wrd2 = dest.parts[0];
	*wrd1 = dest.parts[1];
#endif
	fpu_debug(("extract_double (%.04f) = %X,%X\n",(double)src,*wrd1,*wrd2));
}

// to_pack
PRIVATE inline fpu_register FFPU make_packed(uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3)
{
	fpu_double d;
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

	fpu_debug(("make_packed str = %s\n",str));

	fpu_debug(("make_packed(%X,%X,%X) = %.04f\n",wrd1,wrd2,wrd3,(double)d));
	return d;
}

// from_pack
PRIVATE inline void FFPU extract_packed(fpu_register const & src, uae_u32 * wrd1, uae_u32 * wrd2, uae_u32 * wrd3)
{
	int i;
	int t;
	char *cp;
	char str[100];

	sprintf(str, "%.16e", src);

	fpu_debug(("extract_packed(%.04f,%s)\n",(double)src,str));

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
			if (*cp >= '0' && *cp <= '9')
				t = (t << 4) | (*cp++ - '0');
		}
		*wrd1 |= t << 16;
	}

	fpu_debug(("extract_packed(%.04f) = %X,%X,%X\n",(double)src,*wrd1,*wrd2,*wrd3));
}

PRIVATE inline int FFPU get_fp_value (uae_u32 opcode, uae_u16 extra, fpu_register & src)
{
	uaecptr tmppc;
	uae_u16 tmp;
	int size;
	int mode;
	int reg;
	uae_u32 ad = 0;
	static int sz1[8] = {4, 4, 12, 12, 2, 8, 1, 0};
	static int sz2[8] = {4, 4, 12, 12, 2, 8, 2, 0};

	// fpu_debug(("get_fp_value(%X,%X)\n",(int)opcode,(int)extra));
	// dump_first_bytes( regs.pc_p-4, 16 );

	if ((extra & 0x4000) == 0) {
		src = FPU registers[(extra >> 10) & 7];
		return 1;
	}
	mode = (opcode >> 3) & 7;
	reg = opcode & 7;
	size = (extra >> 10) & 7;

	fpu_debug(("get_fp_value mode=%d, reg=%d, size=%d\n",(int)mode,(int)reg,(int)size));

	switch (mode) {
	case 0:
		switch (size) {
		case 6:
			src = (fpu_register) (uae_s8) m68k_dreg (regs, reg);
			break;
		case 4:
			src = (fpu_register) (uae_s16) m68k_dreg (regs, reg);
			break;
		case 0:
			src = (fpu_register) (uae_s32) m68k_dreg (regs, reg);
			break;
		case 1:
			src = make_single(m68k_dreg (regs, reg));
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
		switch (reg) {
		case 0:
			ad = (uae_s32) (uae_s16) next_iword();
			break;
		case 1:
			ad = next_ilong();
			break;
		case 2:
			ad = m68k_getpc ();
			ad += (uae_s32) (uae_s16) next_iword();
			fpu_debug(("get_fp_value next_iword()=%X\n",ad-m68k_getpc()-2));
			break;
		case 3:
			tmppc = m68k_getpc ();
			tmp = (uae_u16)next_iword();
			ad = get_disp_ea_020 (tmppc, tmp);
			break;
		case 4:
			ad = m68k_getpc ();
			m68k_setpc (ad + sz2[size]);
			// Immediate addressing mode && Operation Length == Byte -> 
			// Use the low-order byte of the extension word.
			if(size == 6) ad++;
				break;
		default:
			return 0;
		}
	}

	fpu_debug(("get_fp_value m68k_getpc()=%X\n",m68k_getpc()));
	fpu_debug(("get_fp_value ad=%X\n",ad));
	fpu_debug(("get_fp_value get_long (ad)=%X\n",get_long (ad)));
	dump_first_bytes( get_real_address(ad)-64, 64 );
	dump_first_bytes( get_real_address(ad), 64 );

	switch (size) {
	case 0:
		src = (fpu_register) (uae_s32) get_long (ad);
		break;
	case 1:
		src = make_single(get_long (ad));
		break;
	case 2: {
		uae_u32 wrd1, wrd2, wrd3;
		wrd1 = get_long (ad);
		ad += 4;
		wrd2 = get_long (ad);
		ad += 4;
		wrd3 = get_long (ad);
		src = make_extended(wrd1, wrd2, wrd3);
		break;
	}
	case 3: {
		uae_u32 wrd1, wrd2, wrd3;
		wrd1 = get_long (ad);
		ad += 4;
		wrd2 = get_long (ad);
		ad += 4;
		wrd3 = get_long (ad);
		src = make_packed(wrd1, wrd2, wrd3);
		break;
	}
	case 4:
		src = (fpu_register) (uae_s16) get_word(ad);
		break;
	case 5: {
		uae_u32 wrd1, wrd2;
		wrd1 = get_long (ad);
		ad += 4;
		wrd2 = get_long (ad);
		src = make_double(wrd1, wrd2);
		break;
	}
	case 6:
		src = (fpu_register) (uae_s8) get_byte(ad);
		break;
	default:
		return 0;
	}
	
	// fpu_debug(("get_fp_value result = %.04f\n",(float)src));
	return 1;
}

/* Convert the FP value to integer according to the current m68k rounding mode */
PRIVATE inline uae_s32 FFPU toint(fpu_register const & src)
{
	fpu_register result;
	switch (get_fpcr() & 0x30) {
	case FPCR_ROUND_ZERO:
		result = fp_round_to_zero(src);
		break;
	case FPCR_ROUND_MINF:
		result = fp_round_to_minus_infinity(src);
		break;
	case FPCR_ROUND_NEAR:
		result = fp_round_to_nearest(src);
		break;
	case FPCR_ROUND_PINF:
		result = fp_round_to_plus_infinity(src);
		break;
	default:
		result = src; /* should never be reached */
		break;
	}
	return (uae_s32)result;
}

PRIVATE inline int FFPU put_fp_value (uae_u32 opcode, uae_u16 extra, fpu_register const & value)
{
	uae_u16 tmp;
	uaecptr tmppc;
	int size;
	int mode;
	int reg;
	uae_u32 ad;
	static int sz1[8] = {4, 4, 12, 12, 2, 8, 1, 0};
	static int sz2[8] = {4, 4, 12, 12, 2, 8, 2, 0};

	// fpu_debug(("put_fp_value(%.04f,%X,%X)\n",(float)value,(int)opcode,(int)extra));

	if ((extra & 0x4000) == 0) {
		int dest_reg = (extra >> 10) & 7;
		FPU registers[dest_reg] = value;
		make_fpsr(FPU registers[dest_reg]);
		return 1;
	}
	mode = (opcode >> 3) & 7;
	reg = opcode & 7;
	size = (extra >> 10) & 7;
	ad = 0xffffffff;
	switch (mode) {
	case 0:
		switch (size) {
		case 6:
			m68k_dreg (regs, reg) = ((toint(value) & 0xff)
									 | (m68k_dreg (regs, reg) & ~0xff));
			break;
		case 4:
			m68k_dreg (regs, reg) = ((toint(value) & 0xffff)
									 | (m68k_dreg (regs, reg) & ~0xffff));
			break;
		case 0:
			m68k_dreg (regs, reg) = toint(value);
			break;
		case 1:
			m68k_dreg (regs, reg) = extract_single(value);
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
		switch (reg) {
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
		case 3:
			tmppc = m68k_getpc ();
			tmp = (uae_u16)next_iword();
			ad = get_disp_ea_020 (tmppc, tmp);
			break;
		case 4:
			ad = m68k_getpc ();
			m68k_setpc (ad + sz2[size]);
			break;
		default:
			return 0;
		}
	}
	switch (size) {
	case 0:
		put_long (ad, toint(value));
		break;
	case 1:
		put_long (ad, extract_single(value));
		break;
	case 2: {
		uae_u32 wrd1, wrd2, wrd3;
		extract_extended(value, &wrd1, &wrd2, &wrd3);
		put_long (ad, wrd1);
		ad += 4;
		put_long (ad, wrd2);
		ad += 4;
		put_long (ad, wrd3);
		break;
	}
	case 3: {
		uae_u32 wrd1, wrd2, wrd3;
		extract_packed(value, &wrd1, &wrd2, &wrd3);
		put_long (ad, wrd1);
		ad += 4;
		put_long (ad, wrd2);
		ad += 4;
		put_long (ad, wrd3);
		break;
	}
	case 4:
		put_word(ad, (uae_s16) toint(value));
		break;
	case 5: {
		uae_u32 wrd1, wrd2;
		extract_double(value, &wrd1, &wrd2);
		put_long (ad, wrd1);
		ad += 4;
		put_long (ad, wrd2);
		break;
	}
	case 6:
		put_byte(ad, (uae_s8) toint(value));
		break;
	default:
		return 0;
	}
	return 1;
}

PRIVATE inline int FFPU get_fp_ad(uae_u32 opcode, uae_u32 * ad)
{
	uae_u16 tmp;
	uaecptr tmppc;
	int mode;
	int reg;

	mode = (opcode >> 3) & 7;
	reg = opcode & 7;
	switch (mode) {
	case 0:
	case 1:
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
		switch (reg) {
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
		case 3:
			tmppc = m68k_getpc ();
			tmp = (uae_u16)next_iword();
			*ad = get_disp_ea_020 (tmppc, tmp);
			break;
		default:
			return 0;
		}
	}
	return 1;
}

#if FPU_DEBUG
# define CONDRET(s,x) fpu_debug(("fpp_cond %s = %d\n",s,(uint32)(x))); return (x)
#else
# define CONDRET(s,x) return (x)
#endif

PRIVATE inline int FFPU fpp_cond(int condition)
{
	int N	= (FPU result < 0.0);
	int Z	= (FPU result == 0.0);
	int NaN	= isnan(FPU result);
	
	if (NaN)
		N = Z = 0;

	switch (condition) {
	case 0x00:	CONDRET("False",0);
	case 0x01:	CONDRET("Equal",Z);
	case 0x02:	CONDRET("Ordered Greater Than",!(NaN || Z || N));
	case 0x03:	CONDRET("Ordered Greater Than or Equal",Z || !(NaN || N));
	case 0x04:	CONDRET("Ordered Less Than",N && !(NaN || Z));
	case 0x05:	CONDRET("Ordered Less Than or Equal",Z || (N && !NaN));
	case 0x06:	CONDRET("Ordered Greater or Less Than",!(NaN || Z));
	case 0x07:	CONDRET("Ordered",!NaN);
	case 0x08:	CONDRET("Unordered",NaN);
	case 0x09:	CONDRET("Unordered or Equal",NaN || Z);
	case 0x0a:	CONDRET("Unordered or Greater Than",NaN || !(N || Z));
	case 0x0b:	CONDRET("Unordered or Greater or Equal",NaN || Z || !N);
	case 0x0c:	CONDRET("Unordered or Less Than",NaN || (N && !Z));
	case 0x0d:	CONDRET("Unordered or Less or Equal",NaN || Z || N);
	case 0x0e:	CONDRET("Not Equal",!Z);
	case 0x0f:	CONDRET("True",1);
	case 0x10:	CONDRET("Signaling False",0);
	case 0x11:	CONDRET("Signaling Equal",Z);
	case 0x12:	CONDRET("Greater Than",!(NaN || Z || N));
	case 0x13:	CONDRET("Greater Than or Equal",Z || !(NaN || N));
	case 0x14:	CONDRET("Less Than",N && !(NaN || Z));
	case 0x15:	CONDRET("Less Than or Equal",Z || (N && !NaN));
	case 0x16:	CONDRET("Greater or Less Than",!(NaN || Z));
	case 0x17:	CONDRET("Greater, Less or Equal",!NaN);
	case 0x18:	CONDRET("Not Greater, Less or Equal",NaN);
	case 0x19:	CONDRET("Not Greater or Less Than",NaN || Z);
	case 0x1a:	CONDRET("Not Less Than or Equal",NaN || !(N || Z));
	case 0x1b:	CONDRET("Not Less Than",NaN || Z || !N);
	case 0x1c:	CONDRET("Not Greater Than or Equal", NaN || (N && !Z));
	case 0x1d:	CONDRET("Not Greater Than",NaN || Z || N);
	case 0x1e:	CONDRET("Signaling Not Equal",!Z);
	case 0x1f:	CONDRET("Signaling True",1);
	default:	CONDRET("",-1);
	}
}

void FFPU fpuop_dbcc(uae_u32 opcode, uae_u32 extra)
{
	fpu_debug(("fdbcc_opp %X, %X at %08lx\n", (uae_u32)opcode, (uae_u32)extra, m68k_getpc ()));

	uaecptr pc = (uae_u32) m68k_getpc ();
	uae_s32 disp = (uae_s32) (uae_s16) next_iword();
	int cc = fpp_cond(extra & 0x3f);
	if (cc == -1) {
		m68k_setpc (pc - 4);
		op_illg (opcode);
	} else if (!cc) {
		int reg = opcode & 0x7;

		// this may have leaked.
		/*
		m68k_dreg (regs, reg) = ((m68k_dreg (regs, reg) & ~0xffff)
				| ((m68k_dreg (regs, reg) - 1) & 0xffff));
		*/
		m68k_dreg (regs, reg) = ((m68k_dreg (regs, reg) & 0xffff0000)
				| (((m68k_dreg (regs, reg) & 0xffff) - 1) & 0xffff));


		// condition reversed.
		// if ((m68k_dreg (regs, reg) & 0xffff) == 0xffff)
		if ((m68k_dreg (regs, reg) & 0xffff) != 0xffff)
		m68k_setpc (pc + disp);
	}
}

void FFPU fpuop_scc(uae_u32 opcode, uae_u32 extra)
{
	fpu_debug(("fscc_opp %X, %X at %08lx\n", (uae_u32)opcode, (uae_u32)extra, m68k_getpc ()));
	
	uae_u32 ad;
	int cc = fpp_cond(extra & 0x3f);
	if (cc == -1) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
	}
	else if ((opcode & 0x38) == 0) {
		m68k_dreg (regs, opcode & 7) = (m68k_dreg (regs, opcode & 7) & ~0xff) |
		(cc ? 0xff : 0x00);
	}
	else if (get_fp_ad(opcode, &ad) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
	}
	else
		put_byte(ad, cc ? 0xff : 0x00);
}

void FFPU fpuop_trapcc(uae_u32 opcode, uaecptr oldpc)
{
	fpu_debug(("ftrapcc_opp %X at %08lx\n", (uae_u32)opcode, m68k_getpc ()));
	
	int cc = fpp_cond(opcode & 0x3f);
	if (cc == -1) {
		m68k_setpc (oldpc);
		op_illg (opcode);
	}
	if (cc)
		Exception(7, oldpc - 2);
}

// NOTE that we get here also when there is a FNOP (nontrapping false, displ 0)
void FFPU fpuop_bcc(uae_u32 opcode, uaecptr pc, uae_u32 extra)
{
	fpu_debug(("fbcc_opp %X, %X at %08lx, jumpto=%X\n", (uae_u32)opcode, (uae_u32)extra, m68k_getpc (), extra ));

	int cc = fpp_cond(opcode & 0x3f);
	if (cc == -1) {
		m68k_setpc (pc);
		op_illg (opcode);
	}
	else if (cc) {
		if ((opcode & 0x40) == 0)
			extra = (uae_s32) (uae_s16) extra;
		m68k_setpc (pc + extra);
	}
}

// FSAVE has no post-increment
// 0x1f180000 == IDLE state frame, coprocessor version number 1F
void FFPU fpuop_save(uae_u32 opcode)
{
	fpu_debug(("fsave_opp at %08lx\n", m68k_getpc ()));

	uae_u32 ad;
	int incr = (opcode & 0x38) == 0x20 ? -1 : 1;
	int i;

	if (get_fp_ad(opcode, &ad) == 0) {
		m68k_setpc (m68k_getpc () - 2);
		op_illg (opcode);
		return;
	}

	if (CPUType == 4) {
		// Put 4 byte 68040 IDLE frame.
		if (incr < 0) {
			ad -= 4;
			put_long (ad, 0x41000000);
		}
		else {
			put_long (ad, 0x41000000);
			ad += 4;
		}
	} else {
		// Put 28 byte 68881 IDLE frame.
		if (incr < 0) {
			fpu_debug(("fsave_opp pre-decrement\n"));
			ad -= 4;
			// What's this? Some BIU flags, or (incorrectly placed) command/condition?
			put_long (ad, 0x70000000);
			for (i = 0; i < 5; i++) {
				ad -= 4;
				put_long (ad, 0x00000000);
			}
			ad -= 4;
			put_long (ad, 0x1f180000); // IDLE, vers 1f
		}
		else {
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
		fpu_debug(("PROBLEM: fsave_opp post-increment\n"));
	}
	if ((opcode & 0x38) == 0x20) {
		m68k_areg (regs, opcode & 7) = ad;
		fpu_debug(("fsave_opp pre-decrement %X -> A%d\n",ad,opcode & 7));
	}
}

// FRESTORE has no pre-decrement
void FFPU fpuop_restore(uae_u32 opcode)
{
	fpu_debug(("frestore_opp at %08lx\n", m68k_getpc ()));

	uae_u32 ad;
	uae_u32 d;
	int incr = (opcode & 0x38) == 0x20 ? -1 : 1;

	if (get_fp_ad(opcode, &ad) == 0) {
		m68k_setpc (m68k_getpc () - 2);
		op_illg (opcode);
		return;
	}

	if (CPUType == 4) {
		// 68040
		if (incr < 0) {
			fpu_debug(("PROBLEM: frestore_opp incr < 0\n"));
			// this may be wrong, but it's never called.
			ad -= 4;
			d = get_long (ad);
			if ((d & 0xff000000) != 0) { // Not a NULL frame?
				if ((d & 0x00ff0000) == 0) { // IDLE
					fpu_debug(("frestore_opp found IDLE frame at %X\n",ad-4));
				}
				else if ((d & 0x00ff0000) == 0x00300000) { // UNIMP
					fpu_debug(("PROBLEM: frestore_opp found UNIMP frame at %X\n",ad-4));
					ad -= 44;
				}
				else if ((d & 0x00ff0000) == 0x00600000) { // BUSY
					fpu_debug(("PROBLEM: frestore_opp found BUSY frame at %X\n",ad-4));
					ad -= 92;
				}
			}
		}
		else {
			d = get_long (ad);
			fpu_debug(("frestore_opp frame at %X = %X\n",ad,d));
			ad += 4;
			if ((d & 0xff000000) != 0) { // Not a NULL frame?
				if ((d & 0x00ff0000) == 0) { // IDLE
					fpu_debug(("frestore_opp found IDLE frame at %X\n",ad-4));
				}
				else if ((d & 0x00ff0000) == 0x00300000) { // UNIMP
					fpu_debug(("PROBLEM: frestore_opp found UNIMP frame at %X\n",ad-4));
					ad += 44;
				}
				else if ((d & 0x00ff0000) == 0x00600000) { // BUSY
					fpu_debug(("PROBLEM: frestore_opp found BUSY frame at %X\n",ad-4));
					ad += 92;
				}
			}
		}
	}
	else {
		// 68881
		if (incr < 0) {
			fpu_debug(("PROBLEM: frestore_opp incr < 0\n"));
			// this may be wrong, but it's never called.
			ad -= 4;
			d = get_long (ad);
			if ((d & 0xff000000) != 0) {
				if ((d & 0x00ff0000) == 0x00180000)
					ad -= 6 * 4;
				else if ((d & 0x00ff0000) == 0x00380000)
					ad -= 14 * 4;
				else if ((d & 0x00ff0000) == 0x00b40000)
					ad -= 45 * 4;
			}
		}
		else {
			d = get_long (ad);
			fpu_debug(("frestore_opp frame at %X = %X\n",ad,d));
			ad += 4;
			if ((d & 0xff000000) != 0) { // Not a NULL frame?
				if ((d & 0x00ff0000) == 0x00180000) { // IDLE
					fpu_debug(("frestore_opp found IDLE frame at %X\n",ad-4));
					ad += 6 * 4;
				}
				else if ((d & 0x00ff0000) == 0x00380000) {// UNIMP? shouldn't it be 3C?
					ad += 14 * 4;
					fpu_debug(("PROBLEM: frestore_opp found UNIMP? frame at %X\n",ad-4));
				}
				else if ((d & 0x00ff0000) == 0x00b40000) {// BUSY
					fpu_debug(("PROBLEM: frestore_opp found BUSY frame at %X\n",ad-4));
					ad += 45 * 4;
				}
			}
		}
	}
	if ((opcode & 0x38) == 0x18) {
		m68k_areg (regs, opcode & 7) = ad;
		fpu_debug(("frestore_opp post-increment %X -> A%d\n",ad,opcode & 7));
	}
	if ((opcode & 0x38) == 0x20) {
		m68k_areg (regs, opcode & 7) = ad; // Never executed on a 68881
		fpu_debug(("PROBLEM: frestore_opp pre-decrement\n"));
	}
}

void FFPU fpuop_arithmetic(uae_u32 opcode, uae_u32 extra)
{
	int reg;
	fpu_register src;

	fpu_debug(("FPP %04lx %04x at %08lx\n", opcode & 0xffff, extra & 0xffff,
			   m68k_getpc () - 4));

	dump_registers( "START");

	switch ((extra >> 13) & 0x7) {
	case 3:
		fpu_debug(("FMOVE -> <ea>\n"));
		if (put_fp_value (opcode, extra, FPU registers[(extra >> 7) & 7]) == 0) {
			m68k_setpc (m68k_getpc () - 4);
			op_illg (opcode);
		}
		dump_registers( "END  ");
		return;
	case 4:
	case 5:
		if ((opcode & 0x38) == 0) {
			if (extra & 0x2000) { // dr bit
				if (extra & 0x1000) {
					// according to the manual, the msb bits are always zero.
					m68k_dreg (regs, opcode & 7) = get_fpcr() & 0xFFFF;
					fpu_debug(("FMOVEM FPU fpcr (%X) -> D%d\n", get_fpcr(), opcode & 7));
				}
				if (extra & 0x0800) {
					m68k_dreg (regs, opcode & 7) = get_fpsr();
					fpu_debug(("FMOVEM FPU fpsr (%X) -> D%d\n", get_fpsr(), opcode & 7));
				}
				if (extra & 0x0400) {
					m68k_dreg (regs, opcode & 7) = FPU instruction_address;
					fpu_debug(("FMOVEM FPU instruction_address (%X) -> D%d\n", FPU instruction_address, opcode & 7));
				}
			}
			else {
				if (extra & 0x1000) {
					set_fpcr( m68k_dreg (regs, opcode & 7) );
					fpu_debug(("FMOVEM D%d (%X) -> FPU fpcr\n", opcode & 7, get_fpcr()));
				}
				if (extra & 0x0800) {
					set_fpsr( m68k_dreg (regs, opcode & 7) );
					fpu_debug(("FMOVEM D%d (%X) -> FPU fpsr\n", opcode & 7, get_fpsr()));
				}
				if (extra & 0x0400) {
					FPU instruction_address = m68k_dreg (regs, opcode & 7);
					fpu_debug(("FMOVEM D%d (%X) -> FPU instruction_address\n", opcode & 7, FPU instruction_address));
				}
			}
//		} else if ((opcode & 0x38) == 1) {
		}
		else if ((opcode & 0x38) == 8) { 
			if (extra & 0x2000) { // dr bit
				if (extra & 0x1000) {
					// according to the manual, the msb bits are always zero.
					m68k_areg (regs, opcode & 7) = get_fpcr() & 0xFFFF;
					fpu_debug(("FMOVEM FPU fpcr (%X) -> A%d\n", get_fpcr(), opcode & 7));
				}
				if (extra & 0x0800) {
					m68k_areg (regs, opcode & 7) = get_fpsr();
					fpu_debug(("FMOVEM FPU fpsr (%X) -> A%d\n", get_fpsr(), opcode & 7));
				}
				if (extra & 0x0400) {
					m68k_areg (regs, opcode & 7) = FPU instruction_address;
					fpu_debug(("FMOVEM FPU instruction_address (%X) -> A%d\n", FPU instruction_address, opcode & 7));
				}
			} else {
				if (extra & 0x1000) {
					set_fpcr( m68k_areg (regs, opcode & 7) );
					fpu_debug(("FMOVEM A%d (%X) -> FPU fpcr\n", opcode & 7, get_fpcr()));
				}
				if (extra & 0x0800) {
					set_fpsr( m68k_areg (regs, opcode & 7) );
					fpu_debug(("FMOVEM A%d (%X) -> FPU fpsr\n", opcode & 7, get_fpsr()));
				}
				if (extra & 0x0400) {
					FPU instruction_address = m68k_areg (regs, opcode & 7);
					fpu_debug(("FMOVEM A%d (%X) -> FPU instruction_address\n", opcode & 7, FPU instruction_address));
				}
			}
		}
		else if ((opcode & 0x3f) == 0x3c) {
			if ((extra & 0x2000) == 0) {
				if (extra & 0x1000) {
					set_fpcr( next_ilong() );
					fpu_debug(("FMOVEM #<%X> -> FPU fpcr\n", get_fpcr()));
				}
				if (extra & 0x0800) {
					set_fpsr( next_ilong() );
					fpu_debug(("FMOVEM #<%X> -> FPU fpsr\n", get_fpsr()));
				}
				if (extra & 0x0400) {
					FPU instruction_address = next_ilong();
					fpu_debug(("FMOVEM #<%X> -> FPU instruction_address\n", FPU instruction_address));
				}
			}
		}
		else if (extra & 0x2000) {
			/* FMOVEM FPP->memory */
			uae_u32 ad;
			int incr = 0;

			if (get_fp_ad(opcode, &ad) == 0) {
				m68k_setpc (m68k_getpc () - 4);
				op_illg (opcode);
				dump_registers( "END  ");
				return;
			}
			if ((opcode & 0x38) == 0x20) {
				if (extra & 0x1000)
					incr += 4;
				if (extra & 0x0800)
					incr += 4;
				if (extra & 0x0400)
					incr += 4;
			}
			ad -= incr;
			if (extra & 0x1000) {
				// according to the manual, the msb bits are always zero.
				put_long (ad, get_fpcr() & 0xFFFF);
				fpu_debug(("FMOVEM FPU fpcr (%X) -> mem %X\n", get_fpcr(), ad ));
				ad += 4;
			}
			if (extra & 0x0800) {
				put_long (ad, get_fpsr());
				fpu_debug(("FMOVEM FPU fpsr (%X) -> mem %X\n", get_fpsr(), ad ));
				ad += 4;
			}
			if (extra & 0x0400) {
				put_long (ad, FPU instruction_address);
				fpu_debug(("FMOVEM FPU instruction_address (%X) -> mem %X\n", FPU instruction_address, ad ));
				ad += 4;
			}
			ad -= incr;
			if ((opcode & 0x38) == 0x18) // post-increment?
				m68k_areg (regs, opcode & 7) = ad;
			if ((opcode & 0x38) == 0x20) // pre-decrement?
				m68k_areg (regs, opcode & 7) = ad;
		}
		else {
			/* FMOVEM memory->FPP */
			uae_u32 ad;

			if (get_fp_ad(opcode, &ad) == 0) {
				m68k_setpc (m68k_getpc () - 4);
				op_illg (opcode);
				dump_registers( "END  ");
				return;
			}

			// ad = (opcode & 0x38) == 0x20 ? ad - 12 : ad;
			int incr = 0;
			if((opcode & 0x38) == 0x20) {
				if (extra & 0x1000)
					incr += 4;
				if (extra & 0x0800)
					incr += 4;
				if (extra & 0x0400)
					incr += 4;
				ad = ad - incr;
			}

			if (extra & 0x1000) {
				set_fpcr( get_long (ad) );
				fpu_debug(("FMOVEM mem %X (%X) -> FPU fpcr\n", ad, get_fpcr() ));
				ad += 4;
			}
			if (extra & 0x0800) {
				set_fpsr( get_long (ad) );
				fpu_debug(("FMOVEM mem %X (%X) -> FPU fpsr\n", ad, get_fpsr() ));
				ad += 4;
			}
			if (extra & 0x0400) {
				FPU instruction_address = get_long (ad);
				fpu_debug(("FMOVEM mem %X (%X) -> FPU instruction_address\n", ad, FPU instruction_address ));
				ad += 4;
			}
			if ((opcode & 0x38) == 0x18) // post-increment?
				m68k_areg (regs, opcode & 7) = ad;
			if ((opcode & 0x38) == 0x20) // pre-decrement?
//				m68k_areg (regs, opcode & 7) = ad - 12;
				m68k_areg (regs, opcode & 7) = ad - incr;
		}
		dump_registers( "END  ");
		return;
	case 6:
	case 7: {
		uae_u32 ad, list = 0;
		int incr = 0;
		if (extra & 0x2000) {
			/* FMOVEM FPP->memory */
			fpu_debug(("FMOVEM FPP->memory\n"));

			if (get_fp_ad(opcode, &ad) == 0) {
				m68k_setpc (m68k_getpc () - 4);
				op_illg (opcode);
				dump_registers( "END  ");
				return;
			}
			switch ((extra >> 11) & 3) {
			case 0:	/* static pred */
				list = extra & 0xff;
				incr = -1;
				break;
			case 1:	/* dynamic pred */
				list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
				incr = -1;
				break;
			case 2:	/* static postinc */
				list = extra & 0xff;
				incr = 1;
				break;
			case 3:	/* dynamic postinc */
				list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
				incr = 1;
				break;
			}

			if (incr < 0) {
				for(reg=7; reg>=0; reg--) {
					uae_u32 wrd1, wrd2, wrd3;
					if( list & 0x80 ) {
						extract_extended(FPU registers[reg],&wrd1, &wrd2, &wrd3);
						ad -= 4;
						put_long (ad, wrd3);
						ad -= 4;
						put_long (ad, wrd2);
						ad -= 4;
						put_long (ad, wrd1);
					}
					list <<= 1;
				}
			}
			else {
				for(reg=0; reg<8; reg++) {
					uae_u32 wrd1, wrd2, wrd3;
					if( list & 0x80 ) {
						extract_extended(FPU registers[reg],&wrd1, &wrd2, &wrd3);
						put_long (ad, wrd1);
						ad += 4;
						put_long (ad, wrd2);
						ad += 4;
						put_long (ad, wrd3);
						ad += 4;
					}
					list <<= 1;
				}
			}
			if ((opcode & 0x38) == 0x18) // post-increment?
				m68k_areg (regs, opcode & 7) = ad;
			if ((opcode & 0x38) == 0x20) // pre-decrement?
				m68k_areg (regs, opcode & 7) = ad;
		}
		else {
			/* FMOVEM memory->FPP */
			fpu_debug(("FMOVEM memory->FPP\n"));

			if (get_fp_ad(opcode, &ad) == 0) {
				m68k_setpc (m68k_getpc () - 4);
				op_illg (opcode);
				dump_registers( "END  ");
				return;
			}
			switch ((extra >> 11) & 3) {
			case 0:	/* static pred */
				fpu_debug(("memory->FMOVEM FPP not legal mode.\n"));
				list = extra & 0xff;
				incr = -1;
				break;
			case 1:	/* dynamic pred */
				fpu_debug(("memory->FMOVEM FPP not legal mode.\n"));
				list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
				incr = -1;
				break;
			case 2:	/* static postinc */
				list = extra & 0xff;
				incr = 1;
				break;
			case 3:	/* dynamic postinc */
				list = m68k_dreg (regs, (extra >> 4) & 3) & 0xff;
				incr = 1;
				break;
			}

			/**/
			if (incr < 0) {
				// not reached
				for(reg=7; reg>=0; reg--) {
					uae_u32 wrd1, wrd2, wrd3;
					if( list & 0x80 ) {
						ad -= 4;
						wrd3 = get_long (ad);
						ad -= 4;
						wrd2 = get_long (ad);
						ad -= 4;
						wrd1 = get_long (ad);
						// FPU registers[reg] = make_extended(wrd1, wrd2, wrd3);
						make_extended_no_normalize (wrd1, wrd2, wrd3, FPU registers[reg]);
					}
					list <<= 1;
				}
			}
			else {
				for(reg=0; reg<8; reg++) {
					uae_u32 wrd1, wrd2, wrd3;
					if( list & 0x80 ) {
						wrd1 = get_long (ad);
						ad += 4;
						wrd2 = get_long (ad);
						ad += 4;
						wrd3 = get_long (ad);
						ad += 4;
						// FPU registers[reg] = make_extended(wrd1, wrd2, wrd3);
						make_extended_no_normalize (wrd1, wrd2, wrd3, FPU registers[reg]);
					}
					list <<= 1;
				}
			}
			if ((opcode & 0x38) == 0x18) // post-increment?
				m68k_areg (regs, opcode & 7) = ad;
			if ((opcode & 0x38) == 0x20) // pre-decrement?
				m68k_areg (regs, opcode & 7) = ad;
		}
		dump_registers( "END  ");
		return;
	}
	case 0:
	case 2:
		reg = (extra >> 7) & 7;
		if ((extra & 0xfc00) == 0x5c00) {
			fpu_debug(("FMOVECR memory->FPP\n"));
			switch (extra & 0x7f) {
			case 0x00:
				// FPU registers[reg] = 4.0 * atan(1.0);
				FPU registers[reg] = 3.1415926535897932384626433832795;
				fpu_debug(("FP const: Pi\n"));
				break;
			case 0x0b:
				// FPU registers[reg] = log10 (2.0);
				FPU registers[reg] = 0.30102999566398119521373889472449;
				fpu_debug(("FP const: Log 10 (2)\n"));
				break;
			case 0x0c:
				// FPU registers[reg] = exp (1.0);
				FPU registers[reg] = 2.7182818284590452353602874713527;
				fpu_debug(("FP const: e\n"));
				break;
			case 0x0d:
				// FPU registers[reg] = log (exp (1.0)) / log (2.0);
				FPU registers[reg] = 1.4426950408889634073599246810019;
				fpu_debug(("FP const: Log 2 (e)\n"));
				break;
			case 0x0e:
				// FPU registers[reg] = log (exp (1.0)) / log (10.0);
				FPU registers[reg] = 0.43429448190325182765112891891661;
				fpu_debug(("FP const: Log 10 (e)\n"));
				break;
			case 0x0f:
				FPU registers[reg] = 0.0;
				fpu_debug(("FP const: zero\n"));
				break;
			case 0x30:
				// FPU registers[reg] = log (2.0);
				FPU registers[reg] = 0.69314718055994530941723212145818;
				fpu_debug(("FP const: ln(2)\n"));
				break;
			case 0x31:
				// FPU registers[reg] = log (10.0);
				FPU registers[reg] = 2.3025850929940456840179914546844;
				fpu_debug(("FP const: ln(10)\n"));
				break;
			case 0x32:
				// ??
				FPU registers[reg] = 1.0e0;
				fpu_debug(("FP const: 1.0e0\n"));
				break;
			case 0x33:
				FPU registers[reg] = 1.0e1;
				fpu_debug(("FP const: 1.0e1\n"));
				break;
			case 0x34:
				FPU registers[reg] = 1.0e2;
				fpu_debug(("FP const: 1.0e2\n"));
				break;
			case 0x35:
				FPU registers[reg] = 1.0e4;
				fpu_debug(("FP const: 1.0e4\n"));
				break;
			case 0x36:
				FPU registers[reg] = 1.0e8;
				fpu_debug(("FP const: 1.0e8\n"));
				break;
			case 0x37:
				FPU registers[reg] = 1.0e16;
				fpu_debug(("FP const: 1.0e16\n"));
				break;
			case 0x38:
				FPU registers[reg] = 1.0e32;
				fpu_debug(("FP const: 1.0e32\n"));
				break;
			case 0x39:
				FPU registers[reg] = 1.0e64;
				fpu_debug(("FP const: 1.0e64\n"));
				break;
			case 0x3a:
				FPU registers[reg] = 1.0e128;
				fpu_debug(("FP const: 1.0e128\n"));
				break;
			case 0x3b:
				FPU registers[reg] = 1.0e256;
				fpu_debug(("FP const: 1.0e256\n"));
				break;
#if USE_LONG_DOUBLE || USE_QUAD_DOUBLE
			case 0x3c:
				FPU registers[reg] = 1.0e512L;
				fpu_debug(("FP const: 1.0e512\n"));
				break;
			case 0x3d:
				FPU registers[reg] = 1.0e1024L;
				fpu_debug(("FP const: 1.0e1024\n"));
				break;
			case 0x3e:
				FPU registers[reg] = 1.0e2048L;
				fpu_debug(("FP const: 1.0e2048\n"));
				break;
			case 0x3f:
				FPU registers[reg] = 1.0e4096L;
				fpu_debug(("FP const: 1.0e4096\n"));
#endif
				break;
			default:
				m68k_setpc (m68k_getpc () - 4);
				op_illg (opcode);
				break;
			}
			// these *do* affect the status reg
			make_fpsr(FPU registers[reg]);
			dump_registers( "END  ");
			return;
		}
		
		if (get_fp_value (opcode, extra, src) == 0) {
			m68k_setpc (m68k_getpc () - 4);
			op_illg (opcode);
			dump_registers( "END  ");
			return;
		}
		fpu_debug(("returned from get_fp_value m68k_getpc()=%X\n",m68k_getpc()));
		
		if (FPU is_integral) {
			// 68040-specific operations
			switch (extra & 0x7f) {
			case 0x40:		/* FSMOVE */
				fpu_debug(("FSMOVE %.04f\n",(double)src));
				FPU registers[reg] = (float)src;
				make_fpsr(FPU registers[reg]);
				break;
			case 0x44:		/* FDMOVE */
				fpu_debug(("FDMOVE %.04f\n",(double)src));
				FPU registers[reg] = (double)src;
				make_fpsr(FPU registers[reg]);
				break;
			case 0x41:		/* FSSQRT */
				fpu_debug(("FSQRT %.04f\n",(double)src));
				FPU registers[reg] = (float)fp_sqrt (src);
				make_fpsr(FPU registers[reg]);
				break;
			case 0x45:		/* FDSQRT */
				fpu_debug(("FSQRT %.04f\n",(double)src));
				FPU registers[reg] = (double)fp_sqrt (src);
				make_fpsr(FPU registers[reg]);
				break;
			case 0x58:		/* FSABS */
				fpu_debug(("FSABS %.04f\n",(double)src));
				FPU registers[reg] = (float)fp_fabs(src);
				make_fpsr(FPU registers[reg]);
				break;
			case 0x5c:		/* FDABS */
				fpu_debug(("FDABS %.04f\n",(double)src));
				FPU registers[reg] = (double)fp_fabs(src);
				make_fpsr(FPU registers[reg]);
				break;
			case 0x5a:		/* FSNEG */
				fpu_debug(("FSNEG %.04f\n",(double)src));
				FPU registers[reg] = (float)-src;
				make_fpsr(FPU registers[reg]);
				break;
			case 0x5e:		/* FDNEG */
				fpu_debug(("FDNEG %.04f\n",(double)src));
				FPU registers[reg] = (double)-src;
				make_fpsr(FPU registers[reg]);
				break;
			case 0x60:		/* FSDIV */
				fpu_debug(("FSDIV %.04f\n",(double)src));
				FPU registers[reg] = (float)(FPU registers[reg] / src);
				make_fpsr(FPU registers[reg]);
				break;
			case 0x64:		/* FDDIV */
				fpu_debug(("FDDIV %.04f\n",(double)src));
				FPU registers[reg] = (double)(FPU registers[reg] / src);
				make_fpsr(FPU registers[reg]);
				break;
			case 0x62:		/* FSADD */
				fpu_debug(("FSADD %.04f\n",(double)src));
				FPU registers[reg] = (float)(FPU registers[reg] + src);
				make_fpsr(FPU registers[reg]);
				break;
			case 0x66:		/* FDADD */
				fpu_debug(("FDADD %.04f\n",(double)src));
				FPU registers[reg] = (double)(FPU registers[reg] + src);
				make_fpsr(FPU registers[reg]);
				break;
			case 0x68:		/* FSSUB */
				fpu_debug(("FSSUB %.04f\n",(double)src));
				FPU registers[reg] = (float)(FPU registers[reg] - src);
				make_fpsr(FPU registers[reg]);
				break;
			case 0x6c:		/* FDSUB */
				fpu_debug(("FDSUB %.04f\n",(double)src));
				FPU registers[reg] = (double)(FPU registers[reg] - src);
				make_fpsr(FPU registers[reg]);
				break;
			case 0x63:		/* FSMUL */
			case 0x67:		/* FDMUL */
				fpu_debug(("FMUL %.04f\n",(double)src));
				get_dest_flags(FPU registers[reg]);
				get_source_flags(src);
				if(fl_dest.in_range && fl_source.in_range) {
					if ((extra & 0x7f) == 0x63)
						FPU registers[reg] = (float)(FPU registers[reg] * src);
					else
						FPU registers[reg] = (double)(FPU registers[reg] * src);
				}
				else if (fl_dest.nan || fl_source.nan || 
						 fl_dest.zero && fl_source.infinity || 
						 fl_dest.infinity && fl_source.zero ) {
					make_nan( FPU registers[reg] );
				}
				else if (fl_dest.zero || fl_source.zero ) {
					if (fl_dest.negative && !fl_source.negative ||
						!fl_dest.negative && fl_source.negative)  {
						make_zero_negative(FPU registers[reg]);
					}
					else {
						make_zero_positive(FPU registers[reg]);
					}
				}
				else {
					if( fl_dest.negative && !fl_source.negative ||
						!fl_dest.negative && fl_source.negative)  {
						make_inf_negative(FPU registers[reg]);
					}
					else {
						make_inf_positive(FPU registers[reg]);
					}
				}
				make_fpsr(FPU registers[reg]);
				break;
			default:
				// Continue decode-execute 6888x instructions below
				goto process_6888x_instructions;
			}
			fpu_debug(("END m68k_getpc()=%X\n",m68k_getpc()));
			dump_registers( "END  ");
			return;
		}

	process_6888x_instructions:		
		switch (extra & 0x7f) {
		case 0x00:		/* FMOVE */
			fpu_debug(("FMOVE %.04f\n",(double)src));
			FPU registers[reg] = src;
			make_fpsr(FPU registers[reg]);
			break;
		case 0x01:		/* FINT */
			fpu_debug(("FINT %.04f\n",(double)src));
			FPU registers[reg] = toint(src);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x02:		/* FSINH */
			fpu_debug(("FSINH %.04f\n",(double)src));
			FPU registers[reg] = fp_sinh (src);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x03:		/* FINTRZ */
			fpu_debug(("FINTRZ %.04f\n",(double)src));
			FPU registers[reg] = fp_round_to_zero(src);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x04:		/* FSQRT */
			fpu_debug(("FSQRT %.04f\n",(double)src));
			FPU registers[reg] = fp_sqrt (src);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x06:		/* FLOGNP1 */
			fpu_debug(("FLOGNP1 %.04f\n",(double)src));
			FPU registers[reg] = fp_log (src + 1.0);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x08:		/* FETOXM1 */
			fpu_debug(("FETOXM1 %.04f\n",(double)src));
			FPU registers[reg] = fp_exp (src) - 1.0;
			make_fpsr(FPU registers[reg]);
			break;
		case 0x09:		/* FTANH */
			fpu_debug(("FTANH %.04f\n",(double)src));
			FPU registers[reg] = fp_tanh (src);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x0a:		/* FATAN */
			fpu_debug(("FATAN %.04f\n",(double)src));
			FPU registers[reg] = fp_atan (src);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x0c:		/* FASIN */
			fpu_debug(("FASIN %.04f\n",(double)src));
			FPU registers[reg] = fp_asin (src);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x0d:		/* FATANH */
			fpu_debug(("FATANH %.04f\n",(double)src));
			FPU registers[reg] = fp_atanh (src);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x0e:		/* FSIN */
			fpu_debug(("FSIN %.04f\n",(double)src));
			FPU registers[reg] = fp_sin (src);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x0f:		/* FTAN */
			fpu_debug(("FTAN %.04f\n",(double)src));
			FPU registers[reg] = fp_tan (src);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x10:		/* FETOX */
			fpu_debug(("FETOX %.04f\n",(double)src));
			FPU registers[reg] = fp_exp (src);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x11:		/* FTWOTOX */
			fpu_debug(("FTWOTOX %.04f\n",(double)src));
			FPU registers[reg] = fp_pow(2.0, src);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x12:		/* FTENTOX */
			fpu_debug(("FTENTOX %.04f\n",(double)src));
			FPU registers[reg] = fp_pow(10.0, src);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x14:		/* FLOGN */
			fpu_debug(("FLOGN %.04f\n",(double)src));
			FPU registers[reg] = fp_log (src);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x15:		/* FLOG10 */
			fpu_debug(("FLOG10 %.04f\n",(double)src));
			FPU registers[reg] = fp_log10 (src);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x16:		/* FLOG2 */
			fpu_debug(("FLOG2 %.04f\n",(double)src));
			FPU registers[reg] = fp_log (src) / fp_log (2.0);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x18:		/* FABS */
			fpu_debug(("FABS %.04f\n",(double)src));
			FPU registers[reg] = fp_fabs(src);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x19:		/* FCOSH */
			fpu_debug(("FCOSH %.04f\n",(double)src));
			FPU registers[reg] = fp_cosh(src);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x1a:		/* FNEG */
			fpu_debug(("FNEG %.04f\n",(double)src));
			FPU registers[reg] = -src;
			make_fpsr(FPU registers[reg]);
			break;
		case 0x1c:		/* FACOS */
			fpu_debug(("FACOS %.04f\n",(double)src));
			FPU registers[reg] = fp_acos(src);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x1d:		/* FCOS */
			fpu_debug(("FCOS %.04f\n",(double)src));
			FPU registers[reg] = fp_cos(src);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x1e:		/* FGETEXP */
			fpu_debug(("FGETEXP %.04f\n",(double)src));
			if( isinf(src) ) {
				make_nan( FPU registers[reg] );
			}
			else {
				FPU registers[reg] = fast_fgetexp( src );
			}
			make_fpsr(FPU registers[reg]);
			break;
		case 0x1f:		/* FGETMAN */
			fpu_debug(("FGETMAN %.04f\n",(double)src));
			if( src == 0 ) {
				FPU registers[reg] = 0;
			}
			else if( isinf(src) ) {
				make_nan( FPU registers[reg] );
			}
			else {
				FPU registers[reg] = src;
				fast_remove_exponent( FPU registers[reg] );
			}
			make_fpsr(FPU registers[reg]);
			break;
		case 0x20:		/* FDIV */
			fpu_debug(("FDIV %.04f\n",(double)src));
			FPU registers[reg] /= src;
			make_fpsr(FPU registers[reg]);
			break;
		case 0x21:		/* FMOD */
			fpu_debug(("FMOD %.04f\n",(double)src));
			// FPU registers[reg] = FPU registers[reg] - (fpu_register) ((int) (FPU registers[reg] / src)) * src;
			{
				fpu_register quot = fp_round_to_zero(FPU registers[reg] / src);
				uae_u32 sign = get_quotient_sign(FPU registers[reg],src);
				FPU registers[reg] = FPU registers[reg] - quot * src;
				make_fpsr(FPU registers[reg]);
				make_quotient(quot, sign);
			}
			break;
		case 0x23:		/* FMUL */
			fpu_debug(("FMUL %.04f\n",(double)src));
			get_dest_flags(FPU registers[reg]);
			get_source_flags(src);
			if(fl_dest.in_range && fl_source.in_range) {
				FPU registers[reg] *= src;
			}
			else if (fl_dest.nan || fl_source.nan || 
					 fl_dest.zero && fl_source.infinity || 
					 fl_dest.infinity && fl_source.zero ) {
				make_nan( FPU registers[reg] );
			}
			else if (fl_dest.zero || fl_source.zero ) {
				if (fl_dest.negative && !fl_source.negative ||
					!fl_dest.negative && fl_source.negative)  {
					make_zero_negative(FPU registers[reg]);
				}
				else {
					make_zero_positive(FPU registers[reg]);
				}
			}
			else {
				if( fl_dest.negative && !fl_source.negative ||
					!fl_dest.negative && fl_source.negative)  {
					make_inf_negative(FPU registers[reg]);
				}
				else {
					make_inf_positive(FPU registers[reg]);
				}
			}
			make_fpsr(FPU registers[reg]);
			break;
		case 0x24:		/* FSGLDIV */
			fpu_debug(("FSGLDIV %.04f\n",(double)src));
			FPU registers[reg] = (float)(FPU registers[reg] / src);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x25:		/* FREM */
			fpu_debug(("FREM %.04f\n",(double)src));
			// FPU registers[reg] = FPU registers[reg] - (double) ((int) (FPU registers[reg] / src + 0.5)) * src;
			{
				fpu_register quot = fp_round_to_nearest(FPU registers[reg] / src);
				uae_u32 sign = get_quotient_sign(FPU registers[reg],src);
				FPU registers[reg] = FPU registers[reg] - quot * src;
				make_fpsr(FPU registers[reg]);
				make_quotient(quot,sign);
			}
			break;

		case 0x26:		/* FSCALE */
			fpu_debug(("FSCALE %.04f\n",(double)src));
			// TODO: overflow flags
			get_dest_flags(FPU registers[reg]);
			get_source_flags(src);
			if (fl_source.in_range && fl_dest.in_range) {
				// When the absolute value of the source operand is >= 2^14,
				// an overflow or underflow always results.
				// Here (int) cast is okay.
				int scale_factor = (int)fp_round_to_zero(src);
#if USE_LONG_DOUBLE || USE_QUAD_DOUBLE
				fp_declare_init_shape(sxp, FPU registers[reg], extended);
				sxp->ieee.exponent += scale_factor;
#else
				fp_declare_init_shape(sxp, FPU registers[reg], double);
				uae_u32 exp = sxp->ieee.exponent + scale_factor;
				if (exp < FP_EXTENDED_EXP_BIAS - FP_DOUBLE_EXP_BIAS)
					exp = 0;
				else if (exp > FP_EXTENDED_EXP_BIAS + FP_DOUBLE_EXP_BIAS)
					exp = FP_DOUBLE_EXP_MAX;
				else
					exp += FP_DOUBLE_EXP_BIAS - FP_EXTENDED_EXP_BIAS;
				sxp->ieee.exponent = exp;
#endif
			}
			else if (fl_source.infinity) {
				// Returns NaN for any Infinity source
				make_nan( FPU registers[reg] );
			}
			make_fpsr(FPU registers[reg]);
			break;
		case 0x27:		/* FSGLMUL */
			fpu_debug(("FSGLMUL %.04f\n",(double)src));
			FPU registers[reg] = (float)(FPU registers[reg] * src);
			make_fpsr(FPU registers[reg]);
			break;
		case 0x28:		/* FSUB */
			fpu_debug(("FSUB %.04f\n",(double)src));
			FPU registers[reg] -= src;
			make_fpsr(FPU registers[reg]);
			break;
		case 0x22:		/* FADD */
			fpu_debug(("FADD %.04f\n",(double)src));
			FPU registers[reg] += src;
			make_fpsr(FPU registers[reg]);
			break;
		case 0x30:		/* FSINCOS */
		case 0x31:
		case 0x32:
		case 0x33:
		case 0x34:
		case 0x35:
		case 0x36:
		case 0x37:
			fpu_debug(("FSINCOS %.04f\n",(double)src));
			// Cosine must be calculated first if same register
			FPU registers[extra & 7] = fp_cos(src);
			FPU registers[reg] = fp_sin (src);
			// Set FPU fpsr according to the sine result
			make_fpsr(FPU registers[reg]);
			break;
		case 0x38:		/* FCMP */
			fpu_debug(("FCMP %.04f\n",(double)src));
			set_fpsr(0);
			make_fpsr(FPU registers[reg] - src);
			break;
		case 0x3a:		/* FTST */
			fpu_debug(("FTST %.04f\n",(double)src));
			set_fpsr(0);
			make_fpsr(src);
			break;
		default:
			fpu_debug(("ILLEGAL F OP %X\n",opcode));
			m68k_setpc (m68k_getpc () - 4);
			op_illg (opcode);
			break;
		}
		fpu_debug(("END m68k_getpc()=%X\n",m68k_getpc()));
		dump_registers( "END  ");
		return;
	}
	
	fpu_debug(("ILLEGAL F OP 2 %X\n",opcode));
	m68k_setpc (m68k_getpc () - 4);
	op_illg (opcode);
	dump_registers( "END  ");
}

/* -------------------------- Initialization -------------------------- */

PRIVATE uae_u8 m_fpu_state_original[108]; // 90/94/108

PUBLIC void FFPU fpu_init (bool integral_68040)
{
	fpu_debug(("fpu_init\n"));
	
	static bool initialized_lookup_tables = false;
	if (!initialized_lookup_tables) {
		fpu_init_native_fflags();
		fpu_init_native_exceptions();
		fpu_init_native_accrued_exceptions();
		initialized_lookup_tables = true;
	}

	FPU is_integral = integral_68040;
	FPU instruction_address = 0;
	FPU fpsr.quotient = 0;
	set_fpcr(0);
	set_fpsr(0);

#if defined(FPU_USE_X86_ROUNDING)
	// Initial state after boot, reset and frestore(null frame)
	x86_control_word = CW_INITIAL;
#elif defined(USE_X87_ASSEMBLY)
	volatile unsigned short int cw;
	__asm__ __volatile__("fnstcw %0" : "=m" (cw));
	cw &= ~0x0300; cw |= 0x0300; // CW_PC_EXTENDED
	cw &= ~0x0C00; cw |= 0x0000; // CW_RC_NEAR
	__asm__ __volatile__("fldcw %0" : : "m" (cw));
#endif

	FPU result = 1;
	
	for (int i = 0; i < 8; i++)
		make_nan(FPU registers[i]);
}

PUBLIC void FFPU fpu_exit (void)
{
	fpu_debug(("fpu_exit\n"));
}

PUBLIC void FFPU fpu_reset (void)
{
	fpu_debug(("fpu_reset\n"));
	fpu_exit();
	fpu_init(FPU is_integral);
}
