/*
 * UAE - The Un*x Amiga Emulator
 *
 * MC68881 emulation
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
 *  Further, now honors the fpcr rounding modes.
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
 * FMOVEM Control Registers to/from address registers An:
 *  A bug caused the code never been called.
 * FMOVEM Control Registers pre-decrement:
 *  Moving of control regs from memory to FPP was not handled properly,
 *  if not all of the three registers were moved.
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
 * Now fpcr high 16 bits are always read as zeores, no matter what was
 * written to them.
 *
 * Other:
 * - Optimized single/double/extended to/from conversion functions.
 *   Huge speed boost, but not (necessarily) portable to other systems.
 *   Enabled/disabled by #define HAVE_IEEE_DOUBLE 1
 * - Optimized versions of FSCALE, FGETEXP, FGETMAN
 * - Conversion routines now handle NaN and infinity better.
 * - Some constants precalculated. Not all compilers can optimize the
 *   expressions previously used.
 *
 * TODO:
 * - Floating point exceptions.
 * - More Infinity/NaN/overflow/underflow checking.
 * - FPIAR (only needed when exceptions are implemented)
 * - Should be written in assembly to support long doubles.
 * - Precision rounding single/double
 */

#include "sysdeps.h"

#include <math.h>
#include <stdio.h>

#include "memory.h"
#include "readcpu.h"
#include "newcpu.h"
#include "main.h"

#define DEBUG 0
#include "debug.h"


// Only define if you have IEEE 64 bit doubles.
#define HAVE_IEEE_DOUBLE 1

#ifdef WORDS_BIGENDIAN
#define FLO 1
#define FHI 0
#else
#define FLO 0
#define FHI 1
#endif

// fpcr rounding modes
#define ROUND_TO_NEAREST			0
#define ROUND_TO_ZERO				0x10
#define ROUND_TO_NEGATIVE_INFINITY	0x20
#define ROUND_TO_POSITIVE_INFINITY	0x30

/* single   : S  8*E 23*F */
/* double   : S 11*E 52*F */
/* extended : S 15*E 64*F */
/* E = 0 & F = 0 -> 0 */
/* E = MAX & F = 0 -> Infin */
/* E = MAX & F # 0 -> NotANumber */
/* E = biased by 127 (single) ,1023 (double) ,16383 (extended) */


#if DEBUG

#define CONDRET(s,x) D(bug("fpp_cond %s = %d\r\n",s,(uint32)(x))); return (x)

static void dump_fp_regs( char *s )
{
	char b[512];

	sprintf( 
		b, 
		"%s: %.04f, %.04f, %.04f, %.04f, %.04f, %.04f, %.04f, %.04f\r\n", 
		s,
		(float)regs.fp[0],
		(float)regs.fp[1],
		(float)regs.fp[2],
		(float)regs.fp[3],
		(float)regs.fp[4],
		(float)regs.fp[5],
		(float)regs.fp[6],
		(float)regs.fp[7]
	);
	D(bug((char*)b));
}

static void dump_first_bytes( uint8 *buf, int32 actual )
{
	char b[256], bb[10];
	int32 i, bytes = sizeof(b)/3-1-3;
	if (actual < bytes)
		bytes = actual;

	*b = 0;
	for (i=0; i<bytes; i++) {
		sprintf( bb, "%02x ", (uint32)buf[i] );
		strcat( b, bb );
	}
	strcat((char*)b,"\r\n");
	D(bug((char*)b));
}
#else
#define CONDRET(s,x) return (x)
#define dump_fp_regs(s) {}
#define dump_first_bytes(b,a) {}
#endif


static __inline__ double round_to_zero( double x )
{
	if(x < 0) {
		return ceil(x);
	} else {
		return floor(x);
	}
}

static __inline__ double round_to_nearest( double x )
{
	return floor(x + 0.5);
}


#define CLEAR_EX_STATUS() regs.fpsr &= 0xFFFF00FF


#if HAVE_IEEE_DOUBLE


// full words to avoid partial register stalls.
typedef struct {
	uae_u32 in_range;
	uae_u32 zero;
	uae_u32 infinity;
	uae_u32 nan;
	uae_u32 negative;
} double_flags;
double_flags fl_dest, fl_source;

static __inline__ uae_u32 IS_NAN(uae_u32 *p)
{
	if( (p[FHI] & 0x7FF00000) == 0x7FF00000 ) {
		// logical or is faster here.
		if( (p[FHI] & 0x000FFFFF) || p[FLO] ) {
			return(1);
		}
	}
	return(0);
}

static __inline__ uae_u32 IS_INFINITY(uae_u32 *p)
{
	if( ((p[FHI] & 0x7FF00000) == 0x7FF00000) && p[FLO] == 0 ) {
		return(1);
	}
	return(0);
}

static __inline__ uae_u32 IS_NEGATIVE(uae_u32 *p)
{
	return( (p[FHI] & 0x80000000) != 0 );
}

static __inline__ uae_u32 IS_ZERO(uae_u32 *p)
{
	return( ((p[FHI] & 0x7FF00000) == 0) && p[FLO] == 0 );
}

// This should not touch the quotient.
/*
#define MAKE_FPSR(fpsr,r) \
					fpsr = (fpsr & 0x00FFFFFF) | \
								 (r == 0 ? 0x4000000 : 0) | \
								 (r < 0 ? 0x8000000 : 0) | \
								 (IS_NAN((uae_u32 *)&r) ? 0x1000000 : 0) | \
								 (IS_INFINITY((uae_u32 *)&r) ? 0x2000000 : 0)
*/
#define MAKE_FPSR(fpsr,r) \
					fpsr = (fpsr & 0x00FFFFFF) | \
								 (IS_ZERO((uae_u32 *)&r) ? 0x4000000 : 0) | \
								 (IS_NEGATIVE((uae_u32 *)&r) ? 0x8000000 : 0) | \
								 (IS_NAN((uae_u32 *)&r) ? 0x1000000 : 0) | \
								 (IS_INFINITY((uae_u32 *)&r) ? 0x2000000 : 0)

static __inline__ void GET_DEST_FLAGS(uae_u32 *p)
{
	fl_dest.negative = IS_NEGATIVE(p);
	fl_dest.zero = IS_ZERO(p);
	fl_dest.infinity = IS_INFINITY(p);
	fl_dest.nan = IS_NAN(p);
	fl_dest.in_range = !fl_dest.zero && !fl_dest.infinity && !fl_dest.nan;
}

static __inline__ void GET_SOURCE_FLAGS(uae_u32 *p)
{
	fl_source.negative = IS_NEGATIVE(p);
	fl_source.zero = IS_ZERO(p);
	fl_source.infinity = IS_INFINITY(p);
	fl_source.nan = IS_NAN(p);
	fl_source.in_range = !fl_source.zero && !fl_source.infinity && !fl_source.nan;
}

static __inline__ void MAKE_NAN(uae_u32 *p)
{
	p[FLO] = 0xFFFFFFFF;
	p[FHI] = 0x7FFFFFFF;
}

static __inline__ void MAKE_ZERO_POSITIVE(uae_u32 *p)
{
	p[FLO] = p[FHI] = 0;
}

static __inline__ void MAKE_ZERO_NEGATIVE(uae_u32 *p)
{
	p[FLO] = 0;
	p[FHI] = 0x80000000;
}

static __inline__ void MAKE_INF_POSITIVE(uae_u32 *p)
{
	p[FLO] = 0;
	p[FHI] = 0x7FF00000;
}

static __inline__ void MAKE_INF_NEGATIVE(uae_u32 *p)
{
	p[FLO] = 0;
	p[FHI] = 0xFFF00000;
}

static __inline__ void FAST_SCALE(uae_u32 *p, int add)
{
	int exp;

	exp = (p[FHI] & 0x7FF00000) >> 20;
	// TODO: overflow flags
	exp += add;
	if(exp >= 2047) {
		MAKE_INF_POSITIVE(p);
	} else if(exp < 0) {
		// keep sign (+/- 0)
		p[FHI] &= 0x80000000;
	} else {
		p[FHI] = (p[FHI] & 0x800FFFFF) | ((uae_u32)exp << 20);
	}
}

static __inline__ double FAST_FGETEXP(uae_u32 *p)
{
	int exp = (p[FHI] & 0x7FF00000) >> 20;
	return( exp - 1023 );
}

// Normalize to range 1..2
static __inline__ void FAST_REMOVE_EXPONENT(uae_u32 *p)
{
	p[FHI] = (p[FHI] & 0x800FFFFF) | 0x3FF00000;
}

// The sign of the quotient is the exclusive-OR of the sign bits
// of the source and destination operands.
static __inline__ uae_u32 GET_QUOTIENT_SIGN(uae_u32 *a, uae_u32 *b)
{
	return( ((a[FHI] ^ b[FHI]) & 0x80000000) ? 0x800000 : 0);
}

// Quotient Byte is loaded with the sign and least significant
// seven bits of the quotient.
static __inline__ uae_u32 MAKE_QUOTIENT( uae_u32 fpsr, double quot, uae_u32 sign )
{
	uae_u32 lsb = (uae_u32)fabs(quot) & 0x7F;
	return (fpsr & 0xFF00FFFF) | sign | (lsb << 16);
}

static __inline__ double to_single (uae_u32 value)
{
	double result;
	uae_u32 *p;

	if ((value & 0x7fffffff) == 0) return (0.0);

	p = (uae_u32 *)&result;

	uae_u32 sign = (value & 0x80000000);
	uae_u32 exp  = ((value & 0x7F800000) >> 23) + 1023 - 127;

	p[FLO] = value << 29;
	p[FHI] = sign | (exp << 20) | ((value & 0x007FFFFF) >> 3);

	D(bug("to_single (%X) = %.04f\r\n",value,(float)result));

	return(result);
}

static __inline__ uae_u32 from_single (double src)
{
	uae_u32 result;
	uae_u32 *p = (uae_u32 *)&src;

  if (src == 0.0) return 0;

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

	D(bug("from_single (%.04f) = %X\r\n",(float)src,result));

  return (result);
}

static __inline__ double to_exten(uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3)
{
	double result;
	uae_u32 *p = (uae_u32 *)&result;

  if ((wrd1 & 0x7fff0000) == 0 && wrd2 == 0 && wrd3 == 0) return 0.0;

	uae_u32 sign =  wrd1 & 0x80000000;
	uae_u32 exp  = (wrd1 >> 16) & 0x7fff;

	// The explicit integer bit is not set, must normalize.
	if((wrd2 & 0x80000000) == 0) {
		D(bug("to_exten denormalized mantissa (%X,%X,%X)\r\n",wrd1,wrd2,wrd3));
		if( wrd2 | wrd3 ) {
			// mantissa, not fraction.
			uae_u64 man = ((uae_u64)wrd2 << 32) | wrd3;
			while( (man & UVAL64(0x8000000000000000)) == 0 ) {
				man <<= 1;
				exp--;
			}
			wrd2 = (uae_u32)( man >> 32 );
			wrd3 = (uae_u32)( man & 0xFFFFFFFF );
		} else {
			if(exp == 0x7FFF) {
				// Infinity.
			} else {
				// Zero
				exp = 16383 - 1023;
			}
		}
	}

	if(exp < 16383 - 1023) {
		// should set underflow.
		exp = 0;
	} else if(exp > 16383 + 1023) {
		// should set overflow.
		exp = 2047;
	} else {
		exp = exp + 1023 - 16383;
	}

	// drop the explicit integer bit.
	p[FLO] = (wrd2 << 21) | (wrd3 >> 11);
	p[FHI] = sign | (exp << 20) | ((wrd2 & 0x7FFFFFFF) >> 11);

	D(bug("to_exten (%X,%X,%X) = %.04f\r\n",wrd1,wrd2,wrd3,(float)result));

	return(result);
}

static __inline__ void from_exten(double src, uae_u32 * wrd1, uae_u32 * wrd2, uae_u32 * wrd3)
{
	uae_u32 *p = (uae_u32 *)&src;

	if (src == 0.0) {
		*wrd1 = *wrd2 = *wrd3 = 0;
		return;
  }

	D(bug("from_exten (%X,%X)\r\n",p[FLO],p[FHI]));

	uae_u32 sign =  p[FHI] & 0x80000000;

	uae_u32 exp  = ((p[FHI] >> 20) & 0x7ff);
	// Check for maximum
	if(exp == 0x7FF) {
		exp = 0x7FFF;
	} else {
		exp  += 16383 - 1023;
	}

	*wrd1 = sign | (exp << 16);
	// always set the explicit integer bit.
	*wrd2 = 0x80000000 | ((p[FHI] & 0x000FFFFF) << 11) | ((p[FLO] & 0xFFE00000) >> 21);
	*wrd3 = p[FLO] << 11;

	D(bug("from_exten (%.04f) = %X,%X,%X\r\n",(float)src,*wrd1,*wrd2,*wrd3));
}

static __inline__ double to_double(uae_u32 wrd1, uae_u32 wrd2)
{
	double result;
	uae_u32 *p;

  if ((wrd1 & 0x7fffffff) == 0 && wrd2 == 0) return 0.0;

	p = (uae_u32 *)&result;
	p[FLO] = wrd2;
	p[FHI] = wrd1;

	D(bug("to_double (%X,%X) = %.04f\r\n",wrd1,wrd2,(float)result));

	return(result);
}

static __inline__ void from_double(double src, uae_u32 * wrd1, uae_u32 * wrd2)
{
  if (src == 0.0) {
		*wrd1 = *wrd2 = 0;
		return;
  }
	uae_u32 *p = (uae_u32 *)&src;
	*wrd2 = p[FLO];
	*wrd1 = p[FHI];

	D(bug("from_double (%.04f) = %X,%X\r\n",(float)src,*wrd1,*wrd2));
}



#else // !HAVE_IEEE_DOUBLE


#define MAKE_FPSR(fpsr,r) fpsr = (fpsr & 0x00FFFFFF) | (r == 0 ? 0x4000000 : 0) | (r < 0 ? 0x8000000 : 0)


static __inline__ double to_single (uae_u32 value)
{
	double frac, result;

	if ((value & 0x7fffffff) == 0)
		return (0.0);
	frac = (double) ((value & 0x7fffff) | 0x800000) / 8388608.0;
	if (value & 0x80000000)
		frac = -frac;
	result = ldexp (frac, (int)((value >> 23) & 0xff) - 127);

	D(bug("to_single (%X) = %.04f\r\n",value,(float)result));

	return (result);
}

static __inline__ uae_u32 from_single (double src)
{
	int expon;
	uae_u32 tmp, result;
	double frac;
#if DEBUG
	double src0 = src;
#endif

  if (src == 0.0)
		return 0;
  if (src < 0) {
		tmp = 0x80000000;
		src = -src;
  } else {
		tmp = 0;
  }
  frac = frexp (src, &expon);
  frac += 0.5 / 16777216.0;
  if (frac >= 1.0) {
		frac /= 2.0;
		expon++;
  }
	result = tmp | (((expon + 127 - 1) & 0xff) << 23) | (((int) (frac * 16777216.0)) & 0x7fffff);

	// D(bug("from_single (%.04f) = %X\r\n",(float)src0,result));

  return (result);
}

static __inline__ double to_exten(uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3)
{
  double frac, result;

  if ((wrd1 & 0x7fff0000) == 0 && wrd2 == 0 && wrd3 == 0)
		return 0.0;
  frac = (double) wrd2 / 2147483648.0 +
					(double) wrd3 / 9223372036854775808.0;
  if (wrd1 & 0x80000000)
		frac = -frac;
  result = ldexp (frac, (int)((wrd1 >> 16) & 0x7fff) - 16383);

	D(bug("to_exten (%X,%X,%X) = %.04f\r\n",wrd1,wrd2,wrd3,(float)result));

  return result;
}

static __inline__ void from_exten(double src, uae_u32 * wrd1, uae_u32 * wrd2, uae_u32 * wrd3)
{
  int expon;
	double frac;
#if DEBUG
	double src0 = src;
#endif

  if (src == 0.0) {
		*wrd1 = 0;
		*wrd2 = 0;
		*wrd3 = 0;
		return;
  }
  if (src < 0) {
		*wrd1 = 0x80000000;
		src = -src;
  } else {
		*wrd1 = 0;
  }
  frac = frexp (src, &expon);
  frac += 0.5 / 18446744073709551616.0;
  if (frac >= 1.0) {
		frac /= 2.0;
		expon++;
  }
  *wrd1 |= (((expon + 16383 - 1) & 0x7fff) << 16);
  *wrd2 = (uae_u32) (frac * 4294967296.0);
  *wrd3 = (uae_u32) (frac * 18446744073709551616.0 - *wrd2 * 4294967296.0);

	// D(bug("from_exten (%.04f) = %X,%X,%X\r\n",(float)src0,*wrd1,*wrd2,*wrd3));
}

static __inline__ double to_double(uae_u32 wrd1, uae_u32 wrd2)
{
  double frac, result;

  if ((wrd1 & 0x7fffffff) == 0 && wrd2 == 0)
		return 0.0;
  frac = (double) ((wrd1 & 0xfffff) | 0x100000) / 1048576.0 +
					(double) wrd2 / 4503599627370496.0;
  if (wrd1 & 0x80000000)
		frac = -frac;
	result = ldexp (frac, (int)((wrd1 >> 20) & 0x7ff) - 1023);

	D(bug("to_double (%X,%X) = %.04f\r\n",wrd1,wrd2,(float)result));

  return result;
}

static __inline__ void from_double(double src, uae_u32 * wrd1, uae_u32 * wrd2)
{
  int expon;
  int tmp;
	double frac;
#if DEBUG
	double src0 = src;
#endif

  if (src == 0.0) {
		*wrd1 = 0;
		*wrd2 = 0;
		return;
  }
  if (src < 0) {
		*wrd1 = 0x80000000;
		src = -src;
  } else {
		*wrd1 = 0;
  }
  frac = frexp (src, &expon);
  frac += 0.5 / 9007199254740992.0;
  if (frac >= 1.0) {
		frac /= 2.0;
		expon++;
  }
  tmp = (uae_u32) (frac * 2097152.0);
  *wrd1 |= (((expon + 1023 - 1) & 0x7ff) << 20) | (tmp & 0xfffff);
  *wrd2 = (uae_u32) (frac * 9007199254740992.0 - tmp * 4294967296.0);

	// D(bug("from_double (%.04f) = %X,%X\r\n",(float)src0,*wrd1,*wrd2));
}
#endif // HAVE_IEEE_DOUBLE

static __inline__ double to_pack(uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3)
{
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
  return d;
}

static __inline__ void from_pack(double src, uae_u32 * wrd1, uae_u32 * wrd2, uae_u32 * wrd3)
{
  int i;
  int t;
  char *cp;
  char str[100];

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
			if (*cp >= '0' && *cp <= '9')
				t = (t << 4) | (*cp++ - '0');
		}
		*wrd1 |= t << 16;
  }

	D(bug("from_pack(%.04f) = %X,%X,%X\r\n",(float)src,*wrd1,*wrd2,*wrd3));
}

static __inline__ int get_fp_value (uae_u32 opcode, uae_u16 extra, double *src)
{
  uaecptr tmppc;
  uae_u16 tmp;
  int size;
  int mode;
  int reg;
  uae_u32 ad = 0;
  static int sz1[8] = {4, 4, 12, 12, 2, 8, 1, 0};
  static int sz2[8] = {4, 4, 12, 12, 2, 8, 2, 0};

	// D(bug("get_fp_value(%X,%X)\r\n",(int)opcode,(int)extra));
	// dump_first_bytes( regs.pc_p-4, 16 );

  if ((extra & 0x4000) == 0) {
		*src = regs.fp[(extra >> 10) & 7];
		return 1;
  }
  mode = (opcode >> 3) & 7;
  reg = opcode & 7;
  size = (extra >> 10) & 7;

	D(bug("get_fp_value mode=%d, reg=%d, size=%d\r\n",(int)mode,(int)reg,(int)size));

  switch (mode) {
    case 0:
			switch (size) {
				case 6:
					*src = (double) (uae_s8) m68k_dreg (regs, reg);
					break;
				case 4:
					*src = (double) (uae_s16) m68k_dreg (regs, reg);
					break;
				case 0:
					*src = (double) (uae_s32) m68k_dreg (regs, reg);
					break;
				case 1:
					*src = to_single(m68k_dreg (regs, reg));
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
	D(bug("get_fp_value next_iword()=%X\r\n",ad-m68k_getpc()-2));
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

	D(bug("get_fp_value m68k_getpc()=%X\r\n",m68k_getpc()));
	D(bug("get_fp_value ad=%X\r\n",ad));
	D(bug("get_fp_value get_long (ad)=%X\r\n",get_long (ad)));
	dump_first_bytes( get_real_address(ad)-64, 64 );
	dump_first_bytes( get_real_address(ad), 64 );

  switch (size) {
    case 0:
			*src = (double) (uae_s32) get_long (ad);
			break;
    case 1:
			*src = to_single(get_long (ad));
			break;

    case 2:{
	    uae_u32 wrd1, wrd2, wrd3;
	    wrd1 = get_long (ad);
	    ad += 4;
	    wrd2 = get_long (ad);
	    ad += 4;
	    wrd3 = get_long (ad);
	    *src = to_exten(wrd1, wrd2, wrd3);
			}
			break;
    case 3:{
	    uae_u32 wrd1, wrd2, wrd3;
	    wrd1 = get_long (ad);
	    ad += 4;
	    wrd2 = get_long (ad);
	    ad += 4;
	    wrd3 = get_long (ad);
	    *src = to_pack(wrd1, wrd2, wrd3);
			}
			break;
    case 4:
			*src = (double) (uae_s16) get_word(ad);
			break;
    case 5:{
	    uae_u32 wrd1, wrd2;
	    wrd1 = get_long (ad);
	    ad += 4;
	    wrd2 = get_long (ad);
	    *src = to_double(wrd1, wrd2);
			}
			break;
    case 6:
			*src = (double) (uae_s8) get_byte(ad);
			break;
    default:
			return 0;
  }

	// D(bug("get_fp_value result = %.04f\r\n",(float)*src));

  return 1;
}

static __inline__ int put_fp_value (double value, uae_u32 opcode, uae_u16 extra)
{
  uae_u16 tmp;
  uaecptr tmppc;
  int size;
  int mode;
  int reg;
  uae_u32 ad;
  static int sz1[8] = {4, 4, 12, 12, 2, 8, 1, 0};
  static int sz2[8] = {4, 4, 12, 12, 2, 8, 2, 0};

	// D(bug("put_fp_value(%.04f,%X,%X)\r\n",(float)value,(int)opcode,(int)extra));

  if ((extra & 0x4000) == 0) {
		int dest_reg = (extra >> 10) & 7;
		regs.fp[dest_reg] = value;
		MAKE_FPSR(regs.fpsr,regs.fp[dest_reg]);
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
					m68k_dreg (regs, reg) = (((int) value & 0xff)
				    | (m68k_dreg (regs, reg) & ~0xff));
					break;
				case 4:
					m68k_dreg (regs, reg) = (((int) value & 0xffff)
				    | (m68k_dreg (regs, reg) & ~0xffff));
					break;
				case 0:
					m68k_dreg (regs, reg) = (int) value;
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
			put_long (ad, (uae_s32) value);
			break;
    case 1:
			put_long (ad, from_single(value));
			break;
		case 2:
			{
					uae_u32 wrd1, wrd2, wrd3;
					from_exten(value, &wrd1, &wrd2, &wrd3);
					put_long (ad, wrd1);
					ad += 4;
					put_long (ad, wrd2);
					ad += 4;
					put_long (ad, wrd3);
			}
			break;
    case 3:
			{
					uae_u32 wrd1, wrd2, wrd3;
					from_pack(value, &wrd1, &wrd2, &wrd3);
					put_long (ad, wrd1);
					ad += 4;
					put_long (ad, wrd2);
					ad += 4;
					put_long (ad, wrd3);
			}
			break;
		case 4:
			put_word(ad, (uae_s16) value);
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
			put_byte(ad, (uae_s8) value);
			break;
    default:
			return 0;
  }
  return 1;
}

static __inline__ int get_fp_ad(uae_u32 opcode, uae_u32 * ad)
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

static __inline__ int fpp_cond(uae_u32 opcode, int contition)
{
  int N = (regs.fpsr & 0x8000000) != 0;
  int Z = (regs.fpsr & 0x4000000) != 0;
  /* int I = (regs.fpsr & 0x2000000) != 0; */
  int NotANumber = (regs.fpsr & 0x1000000) != 0;

  switch (contition) {
    case 0x00:
			CONDRET("False",0);
    case 0x01:
			CONDRET("Equal",Z);
    case 0x02:
			CONDRET("Ordered Greater Than",!(NotANumber || Z || N));
    case 0x03:
			CONDRET("Ordered Greater Than or Equal",Z || !(NotANumber || N));
    case 0x04:
			CONDRET("Ordered Less Than",N && !(NotANumber || Z));
    case 0x05:
			CONDRET("Ordered Less Than or Equal",Z || (N && !NotANumber));
    case 0x06:
			CONDRET("Ordered Greater or Less Than",!(NotANumber || Z));
    case 0x07:
			CONDRET("Ordered",!NotANumber);
    case 0x08:
			CONDRET("Unordered",NotANumber);
    case 0x09:
			CONDRET("Unordered or Equal",NotANumber || Z);
    case 0x0a:
			CONDRET("Unordered or Greater Than",NotANumber || !(N || Z));
    case 0x0b:
			CONDRET("Unordered or Greater or Equal",NotANumber || Z || !N);
    case 0x0c:
			CONDRET("Unordered or Less Than",NotANumber || (N && !Z));
    case 0x0d:
			CONDRET("Unordered or Less or Equal",NotANumber || Z || N);
    case 0x0e:
			CONDRET("Not Equal",!Z);
    case 0x0f:
			CONDRET("True",1);
    case 0x10:
			CONDRET("Signaling False",0);
    case 0x11:
			CONDRET("Signaling Equal",Z);
    case 0x12:
			CONDRET("Greater Than",!(NotANumber || Z || N));
    case 0x13:
			CONDRET("Greater Than or Equal",Z || !(NotANumber || N));
    case 0x14:
			CONDRET("Less Than",N && !(NotANumber || Z));
    case 0x15:
			CONDRET("Less Than or Equal",Z || (N && !NotANumber));
    case 0x16:
			CONDRET("Greater or Less Than",!(NotANumber || Z));
    case 0x17:
			CONDRET("Greater, Less or Equal",!NotANumber);
    case 0x18:
			CONDRET("Not Greater, Less or Equal",NotANumber);
    case 0x19:
			CONDRET("Not Greater or Less Than",NotANumber || Z);
    case 0x1a:
			CONDRET("Not Less Than or Equal",NotANumber || !(N || Z));
    case 0x1b:
			CONDRET("Not Less Than",NotANumber || Z || !N);
    case 0x1c:
			// CONDRET("Not Greater Than or Equal",NotANumber || (Z && N));
			CONDRET("Not Greater Than or Equal",!Z && (NotANumber || N));
    case 0x1d:
			CONDRET("Not Greater Than",NotANumber || Z || N);
    case 0x1e:
			CONDRET("Signaling Not Equal",!Z);
    case 0x1f:
			CONDRET("Signaling True",1);
  }
	CONDRET("",-1);
}

void fdbcc_opp(uae_u32 opcode, uae_u16 extra)
{
  uaecptr pc = (uae_u32) m68k_getpc ();
  uae_s32 disp = (uae_s32) (uae_s16) next_iword();
  int cc;

  D(bug("fdbcc_opp %X, %X at %08lx\r\n", (uae_u32)opcode, (uae_u32)extra, m68k_getpc ()));

  cc = fpp_cond(opcode, extra & 0x3f);
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

void fscc_opp(uae_u32 opcode, uae_u16 extra)
{
  uae_u32 ad;
  int cc;

  D(bug("fscc_opp %X, %X at %08lx\r\n", (uae_u32)opcode, (uae_u32)extra, m68k_getpc ()));

  cc = fpp_cond(opcode, extra & 0x3f);
  if (cc == -1) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
  } else if ((opcode & 0x38) == 0) {
		m68k_dreg (regs, opcode & 7) = (m68k_dreg (regs, opcode & 7) & ~0xff) |
	    (cc ? 0xff : 0x00);
  } else {
		if (get_fp_ad(opcode, &ad) == 0) {
	    m68k_setpc (m68k_getpc () - 4);
	    op_illg (opcode);
		} else
	    put_byte(ad, cc ? 0xff : 0x00);
  }
}

void ftrapcc_opp(uae_u32 opcode, uaecptr oldpc)
{
  int cc;

  D(bug("ftrapcc_opp %X at %08lx\r\n", (uae_u32)opcode, m68k_getpc ()));

  cc = fpp_cond(opcode, opcode & 0x3f);
  if (cc == -1) {
		m68k_setpc (oldpc);
		op_illg (opcode);
  }
  if (cc)
		Exception(7, oldpc - 2);
}

// NOTE that we get here also when there is a FNOP (nontrapping false, displ 0)
void fbcc_opp(uae_u32 opcode, uaecptr pc, uae_u32 extra)
{
  int cc;

  D(bug("fbcc_opp %X, %X at %08lx, jumpto=%X\r\n", (uae_u32)opcode, (uae_u32)extra, m68k_getpc (), extra ));

  cc = fpp_cond(opcode, opcode & 0x3f);
  if (cc == -1) {
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
void fsave_opp(uae_u32 opcode)
{
  uae_u32 ad;
  int incr = (opcode & 0x38) == 0x20 ? -1 : 1;
  int i;

  D(bug("fsave_opp at %08lx\r\n", m68k_getpc ()));

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

// FRESTORE has no pre-decrement
void frestore_opp(uae_u32 opcode)
{
  uae_u32 ad;
  uae_u32 d;
  int incr = (opcode & 0x38) == 0x20 ? -1 : 1;

  D(bug("frestore_opp at %08lx\r\n", m68k_getpc ()));

  if (get_fp_ad(opcode, &ad) == 0) {
		m68k_setpc (m68k_getpc () - 2);
		op_illg (opcode);
		return;
  }

	if (CPUType == 4) {
		// 68040
		if (incr < 0) {
		  D(bug("PROBLEM: frestore_opp incr < 0\r\n"));
			// this may be wrong, but it's never called.
			ad -= 4;
			d = get_long (ad);
			if ((d & 0xff000000) != 0) { // Not a NULL frame?
				if ((d & 0x00ff0000) == 0) { // IDLE
				  D(bug("frestore_opp found IDLE frame at %X\r\n",ad-4));
				} else if ((d & 0x00ff0000) == 0x00300000) { // UNIMP
				  D(bug("PROBLEM: frestore_opp found UNIMP frame at %X\r\n",ad-4));
					ad -= 44;
				} else if ((d & 0x00ff0000) == 0x00600000) { // BUSY
				  D(bug("PROBLEM: frestore_opp found BUSY frame at %X\r\n",ad-4));
					ad -= 92;
				}
			}
		} else {
			d = get_long (ad);
		  D(bug("frestore_opp frame at %X = %X\r\n",ad,d));
			ad += 4;
			if ((d & 0xff000000) != 0) { // Not a NULL frame?
				if ((d & 0x00ff0000) == 0) { // IDLE
				  D(bug("frestore_opp found IDLE frame at %X\r\n",ad-4));
				} else if ((d & 0x00ff0000) == 0x00300000) { // UNIMP
				  D(bug("PROBLEM: frestore_opp found UNIMP frame at %X\r\n",ad-4));
					ad += 44;
				} else if ((d & 0x00ff0000) == 0x00600000) { // BUSY
				  D(bug("PROBLEM: frestore_opp found BUSY frame at %X\r\n",ad-4));
					ad += 92;
				}
			}
		}
	} else {
		// 68881
	  if (incr < 0) {
		  D(bug("PROBLEM: frestore_opp incr < 0\r\n"));
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
	  } else {
			d = get_long (ad);
		  D(bug("frestore_opp frame at %X = %X\r\n",ad,d));
			ad += 4;
			if ((d & 0xff000000) != 0) { // Not a NULL frame?
		    if ((d & 0x00ff0000) == 0x00180000) { // IDLE
				  D(bug("frestore_opp found IDLE frame at %X\r\n",ad-4));
					ad += 6 * 4;
		    } else if ((d & 0x00ff0000) == 0x00380000) {// UNIMP? shouldn't it be 3C?
					ad += 14 * 4;
				  D(bug("PROBLEM: frestore_opp found UNIMP? frame at %X\r\n",ad-4));
		    } else if ((d & 0x00ff0000) == 0x00b40000) {// BUSY
				  D(bug("PROBLEM: frestore_opp found BUSY frame at %X\r\n",ad-4));
					ad += 45 * 4;
				}
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

void fpp_opp(uae_u32 opcode, uae_u16 extra)
{
  int reg;
  double src;

  D(bug("FPP %04lx %04x at %08lx\r\n", opcode & 0xffff, extra & 0xffff,
	 m68k_getpc () - 4));

	dump_fp_regs( "START");

  switch ((extra >> 13) & 0x7) {
    case 3:
			D(bug("FMOVE -> <ea>\r\n"));
			if (put_fp_value (regs.fp[(extra >> 7) & 7], opcode, extra) == 0) {
			  m68k_setpc (m68k_getpc () - 4);
				op_illg (opcode);
			}
			dump_fp_regs( "END  ");
			return;
    case 4:
    case 5:
			if ((opcode & 0x38) == 0) {
				if (extra & 0x2000) { // dr bit
					if (extra & 0x1000) {
						// according to the manual, the msb bits are always zero.
						m68k_dreg (regs, opcode & 7) = regs.fpcr & 0xFFFF;
						D(bug("FMOVEM regs.fpcr (%X) -> D%d\r\n", regs.fpcr, opcode & 7));
					}
					if (extra & 0x0800) {
						m68k_dreg (regs, opcode & 7) = regs.fpsr;
						D(bug("FMOVEM regs.fpsr (%X) -> D%d\r\n", regs.fpsr, opcode & 7));
					}
					if (extra & 0x0400) {
						m68k_dreg (regs, opcode & 7) = regs.fpiar;
						D(bug("FMOVEM regs.fpiar (%X) -> D%d\r\n", regs.fpiar, opcode & 7));
					}
				} else {
					if (extra & 0x1000) {
						regs.fpcr = m68k_dreg (regs, opcode & 7);
						D(bug("FMOVEM D%d (%X) -> regs.fpcr\r\n", opcode & 7, regs.fpcr));
					}
					if (extra & 0x0800) {
						regs.fpsr = m68k_dreg (regs, opcode & 7);
						D(bug("FMOVEM D%d (%X) -> regs.fpsr\r\n", opcode & 7, regs.fpsr));
					}
					if (extra & 0x0400) {
						regs.fpiar = m68k_dreg (regs, opcode & 7);
						D(bug("FMOVEM D%d (%X) -> regs.fpiar\r\n", opcode & 7, regs.fpiar));
					}
				}
			// } else if ((opcode & 0x38) == 1) {
			} else if ((opcode & 0x38) == 8) { 
				if (extra & 0x2000) { // dr bit
					if (extra & 0x1000) {
						// according to the manual, the msb bits are always zero.
						m68k_areg (regs, opcode & 7) = regs.fpcr & 0xFFFF;
						D(bug("FMOVEM regs.fpcr (%X) -> A%d\r\n", regs.fpcr, opcode & 7));
					}
					if (extra & 0x0800) {
						m68k_areg (regs, opcode & 7) = regs.fpsr;
						D(bug("FMOVEM regs.fpsr (%X) -> A%d\r\n", regs.fpsr, opcode & 7));
					}
					if (extra & 0x0400) {
						m68k_areg (regs, opcode & 7) = regs.fpiar;
						D(bug("FMOVEM regs.fpiar (%X) -> A%d\r\n", regs.fpiar, opcode & 7));
					}
				} else {
					if (extra & 0x1000) {
						regs.fpcr = m68k_areg (regs, opcode & 7);
						D(bug("FMOVEM A%d (%X) -> regs.fpcr\r\n", opcode & 7, regs.fpcr));
					}
					if (extra & 0x0800) {
						regs.fpsr = m68k_areg (regs, opcode & 7);
						D(bug("FMOVEM A%d (%X) -> regs.fpsr\r\n", opcode & 7, regs.fpsr));
					}
					if (extra & 0x0400) {
						regs.fpiar = m68k_areg (regs, opcode & 7);
						D(bug("FMOVEM A%d (%X) -> regs.fpiar\r\n", opcode & 7, regs.fpiar));
					}
				}
			} else if ((opcode & 0x3f) == 0x3c) {
			  if ((extra & 0x2000) == 0) {
					if (extra & 0x1000) {
						regs.fpcr = next_ilong();
						D(bug("FMOVEM #<%X> -> regs.fpcr\r\n", regs.fpcr));
					}
					if (extra & 0x0800) {
						regs.fpsr = next_ilong();
						D(bug("FMOVEM #<%X> -> regs.fpsr\r\n", regs.fpsr));
					}
					if (extra & 0x0400) {
						regs.fpiar = next_ilong();
						D(bug("FMOVEM #<%X> -> regs.fpiar\r\n", regs.fpiar));
					}
				}
			} else if (extra & 0x2000) {
				/* FMOVEM FPP->memory */

				uae_u32 ad;
				int incr = 0;

				if (get_fp_ad(opcode, &ad) == 0) {
					m68k_setpc (m68k_getpc () - 4);
					op_illg (opcode);
					dump_fp_regs( "END  ");
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
					put_long (ad, regs.fpcr & 0xFFFF);
					D(bug("FMOVEM regs.fpcr (%X) -> mem %X\r\n", regs.fpcr, ad ));
					ad += 4;
				}
				if (extra & 0x0800) {
					put_long (ad, regs.fpsr);
					D(bug("FMOVEM regs.fpsr (%X) -> mem %X\r\n", regs.fpsr, ad ));
					ad += 4;
				}
				if (extra & 0x0400) {
					put_long (ad, regs.fpiar);
					D(bug("FMOVEM regs.fpiar (%X) -> mem %X\r\n", regs.fpiar, ad ));
					ad += 4;
				}
				ad -= incr;
				if ((opcode & 0x38) == 0x18) // post-increment?
					m68k_areg (regs, opcode & 7) = ad;
				if ((opcode & 0x38) == 0x20) // pre-decrement?
					m68k_areg (regs, opcode & 7) = ad;
			} else {
				/* FMOVEM memory->FPP */

				uae_u32 ad;

				if (get_fp_ad(opcode, &ad) == 0) {
					m68k_setpc (m68k_getpc () - 4);
					op_illg (opcode);
					dump_fp_regs( "END  ");
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
					regs.fpcr = get_long (ad);
					D(bug("FMOVEM mem %X (%X) -> regs.fpcr\r\n", ad, regs.fpcr ));
					ad += 4;
				}
				if (extra & 0x0800) {
					regs.fpsr = get_long (ad);
					D(bug("FMOVEM mem %X (%X) -> regs.fpsr\r\n", ad, regs.fpsr ));
					ad += 4;
				}
				if (extra & 0x0400) {
					regs.fpiar = get_long (ad);
					D(bug("FMOVEM mem %X (%X) -> regs.fpiar\r\n", ad, regs.fpiar ));
					ad += 4;
				}
				if ((opcode & 0x38) == 0x18) // post-increment?
					m68k_areg (regs, opcode & 7) = ad;
				if ((opcode & 0x38) == 0x20) // pre-decrement?
					// m68k_areg (regs, opcode & 7) = ad - 12;
					m68k_areg (regs, opcode & 7) = ad - incr;
			}
			dump_fp_regs( "END  ");
			return;
    case 6:
    case 7:
			{
	    uae_u32 ad, list = 0;
	    int incr = 0;
	    if (extra & 0x2000) {
				/* FMOVEM FPP->memory */

				D(bug("FMOVEM FPP->memory\r\n"));

				if (get_fp_ad(opcode, &ad) == 0) {
					m68k_setpc (m68k_getpc () - 4);
					op_illg (opcode);
					dump_fp_regs( "END  ");
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
							from_exten(regs.fp[reg],&wrd1, &wrd2, &wrd3);
							ad -= 4;
							put_long (ad, wrd3);
							ad -= 4;
							put_long (ad, wrd2);
							ad -= 4;
							put_long (ad, wrd1);
						}
						list <<= 1;
					}
				} else {
					for(reg=0; reg<8; reg++) {
						uae_u32 wrd1, wrd2, wrd3;
						if( list & 0x80 ) {
							from_exten(regs.fp[reg],&wrd1, &wrd2, &wrd3);
							put_long (ad, wrd3);
							ad += 4;
							put_long (ad, wrd2);
							ad += 4;
							put_long (ad, wrd1);
							ad += 4;
						}
						list <<= 1;
					}
				}

				/*
				while (list) {
					uae_u32 wrd1, wrd2, wrd3;
					if (incr < 0) {
						from_exten(regs.fp[fpp_movem_index2[list]],
						 &wrd1, &wrd2, &wrd3);
						ad -= 4;
						put_long (ad, wrd3);
						ad -= 4;
						put_long (ad, wrd2);
						ad -= 4;
						put_long (ad, wrd1);
					} else {
						from_exten(regs.fp[fpp_movem_index1[list]],
						 &wrd1, &wrd2, &wrd3);
						put_long (ad, wrd1);
						ad += 4;
						put_long (ad, wrd2);
						ad += 4;
						put_long (ad, wrd3);
						ad += 4;
					}
					list = fpp_movem_next[list];
				}
				*/
				if ((opcode & 0x38) == 0x18) // post-increment?
					m68k_areg (regs, opcode & 7) = ad;
				if ((opcode & 0x38) == 0x20) // pre-decrement?
					m68k_areg (regs, opcode & 7) = ad;
		  } else {
				/* FMOVEM memory->FPP */

				D(bug("FMOVEM memory->FPP\r\n"));

				if (get_fp_ad(opcode, &ad) == 0) {
					m68k_setpc (m68k_getpc () - 4);
					op_illg (opcode);
					dump_fp_regs( "END  ");
					return;
				}
				switch ((extra >> 11) & 3) {
					case 0:	/* static pred */
						D(bug("memory->FMOVEM FPP not legal mode.\r\n"));
						list = extra & 0xff;
						incr = -1;
						break;
					case 1:	/* dynamic pred */
						D(bug("memory->FMOVEM FPP not legal mode.\r\n"));
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
							wrd1 = get_long (ad);
							ad -= 4;
							wrd2 = get_long (ad);
							ad -= 4;
							wrd3 = get_long (ad);
							ad -= 4;
							regs.fp[reg] = to_exten (wrd1, wrd2, wrd3);
						}
						list <<= 1;
					}
				} else {
					for(reg=0; reg<8; reg++) {
						uae_u32 wrd1, wrd2, wrd3;
						if( list & 0x80 ) {
							wrd1 = get_long (ad);
							ad += 4;
							wrd2 = get_long (ad);
							ad += 4;
							wrd3 = get_long (ad);
							ad += 4;
							regs.fp[reg] = to_exten (wrd1, wrd2, wrd3);
						}
						list <<= 1;
					}
				}
				/**/

				/*
				while (list) {
					uae_u32 wrd1, wrd2, wrd3;
					if (incr < 0) {
						ad -= 4;
						wrd3 = get_long (ad);
						ad -= 4;
						wrd2 = get_long (ad);
						ad -= 4;
						wrd1 = get_long (ad);
						regs.fp[fpp_movem_index2[list]] = to_exten (wrd1, wrd2, wrd3);
			    } else {
						wrd1 = get_long (ad);
						ad += 4;
						wrd2 = get_long (ad);
						ad += 4;
						wrd3 = get_long (ad);
						ad += 4;
						regs.fp[fpp_movem_index1[list]] = to_exten (wrd1, wrd2, wrd3);
					}
					list = fpp_movem_next[list];
				}
				*/
				if ((opcode & 0x38) == 0x18) // post-increment?
			    m68k_areg (regs, opcode & 7) = ad;
				if ((opcode & 0x38) == 0x20) // pre-decrement?
			    m68k_areg (regs, opcode & 7) = ad;
	    }
			}
			dump_fp_regs( "END  ");
			return;
    case 0:
    case 2:
			reg = (extra >> 7) & 7;
			if ((extra & 0xfc00) == 0x5c00) {
				D(bug("FMOVECR memory->FPP\r\n"));
				switch (extra & 0x7f) {
					case 0x00:
						// regs.fp[reg] = 4.0 * atan(1.0);
						regs.fp[reg] = 3.1415926535897932384626433832795;
						D(bug("FP const: Pi\r\n"));
						break;
					case 0x0b:
						// regs.fp[reg] = log10 (2.0);
						regs.fp[reg] = 0.30102999566398119521373889472449;
						D(bug("FP const: Log 10 (2)\r\n"));
						break;
					case 0x0c:
						// regs.fp[reg] = exp (1.0);
						regs.fp[reg] = 2.7182818284590452353602874713527;
						D(bug("FP const: e\r\n"));
						break;
					case 0x0d:
						// regs.fp[reg] = log (exp (1.0)) / log (2.0);
						regs.fp[reg] = 1.4426950408889634073599246810019;
						D(bug("FP const: Log 2 (e)\r\n"));
						break;
					case 0x0e:
						// regs.fp[reg] = log (exp (1.0)) / log (10.0);
						regs.fp[reg] = 0.43429448190325182765112891891661;
						D(bug("FP const: Log 10 (e)\r\n"));
						break;
					case 0x0f:
						regs.fp[reg] = 0.0;
						D(bug("FP const: zero\r\n"));
						break;
					case 0x30:
						// regs.fp[reg] = log (2.0);
						regs.fp[reg] = 0.69314718055994530941723212145818;
						D(bug("FP const: ln(2)\r\n"));
						break;
					case 0x31:
						// regs.fp[reg] = log (10.0);
						regs.fp[reg] = 2.3025850929940456840179914546844;
						D(bug("FP const: ln(10)\r\n"));
						break;
					case 0x32:
						// ??
						regs.fp[reg] = 1.0e0;
						D(bug("FP const: 1.0e0\r\n"));
						break;
					case 0x33:
						regs.fp[reg] = 1.0e1;
						D(bug("FP const: 1.0e1\r\n"));
						break;
					case 0x34:
						regs.fp[reg] = 1.0e2;
						D(bug("FP const: 1.0e2\r\n"));
						break;
					case 0x35:
						regs.fp[reg] = 1.0e4;
						D(bug("FP const: 1.0e4\r\n"));
						break;
					case 0x36:
						regs.fp[reg] = 1.0e8;
						D(bug("FP const: 1.0e8\r\n"));
						break;
					case 0x37:
						regs.fp[reg] = 1.0e16;
						D(bug("FP const: 1.0e16\r\n"));
						break;
					case 0x38:
						regs.fp[reg] = 1.0e32;
						D(bug("FP const: 1.0e32\r\n"));
						break;
					case 0x39:
						regs.fp[reg] = 1.0e64;
						D(bug("FP const: 1.0e64\r\n"));
						break;
					case 0x3a:
						regs.fp[reg] = 1.0e128;
						D(bug("FP const: 1.0e128\r\n"));
						break;
					case 0x3b:
						regs.fp[reg] = 1.0e256;
						D(bug("FP const: 1.0e256\r\n"));
						break;
					
					// Valid for 64 bits only (see fpu.cpp)
#if 0
					case 0x3c:
						regs.fp[reg] = 1.0e512;
						D(bug("FP const: 1.0e512\r\n"));
						break;
					case 0x3d:
						regs.fp[reg] = 1.0e1024;
						D(bug("FP const: 1.0e1024\r\n"));
						break;
					case 0x3e:
						regs.fp[reg] = 1.0e2048;
						D(bug("FP const: 1.0e2048\r\n"));
						break;
					case 0x3f:
						regs.fp[reg] = 1.0e4096;
						D(bug("FP const: 1.0e4096\r\n"));
						break;
#endif
					default:
						m68k_setpc (m68k_getpc () - 4);
						op_illg (opcode);
						break;
				}
				// these *do* affect the status reg
				MAKE_FPSR(regs.fpsr,regs.fp[reg]);
				dump_fp_regs( "END  ");
				return;
			}
			if (get_fp_value (opcode, extra, &src) == 0) {
				m68k_setpc (m68k_getpc () - 4);
				op_illg (opcode);
				dump_fp_regs( "END  ");
				return;
			}

			switch (extra & 0x7f) {
				case 0x00:		/* FMOVE */
					D(bug("FMOVE %.04f\r\n",(float)src));
					regs.fp[reg] = src;
					// <ea> -> reg DOES affect the status reg
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x01:		/* FINT */
					D(bug("FINT %.04f\r\n",(float)src));
					// regs.fp[reg] = (int) (src + 0.5);
					switch(regs.fpcr & 0x30) {
						case ROUND_TO_ZERO:
							regs.fp[reg] = round_to_zero(src);
							break;
						case ROUND_TO_NEGATIVE_INFINITY:
							regs.fp[reg] = floor(src);
							break;
						case ROUND_TO_NEAREST:
							regs.fp[reg] = round_to_nearest(src);
							break;
						case ROUND_TO_POSITIVE_INFINITY:
							regs.fp[reg] = ceil(src);
							break;
					}
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x02:		/* FSINH */
					D(bug("FSINH %.04f\r\n",(float)src));
					regs.fp[reg] = sinh (src);
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x03:		/* FINTRZ */
					D(bug("FINTRZ %.04f\r\n",(float)src));
					// regs.fp[reg] = (int) src;
					regs.fp[reg] = round_to_zero(src);
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x04:		/* FSQRT */
					D(bug("FSQRT %.04f\r\n",(float)src));
					regs.fp[reg] = sqrt (src);
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x06:		/* FLOGNP1 */
					D(bug("FLOGNP1 %.04f\r\n",(float)src));
					regs.fp[reg] = log (src + 1.0);
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x08:		/* FETOXM1 */
					D(bug("FETOXM1 %.04f\r\n",(float)src));
					regs.fp[reg] = exp (src) - 1.0;
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x09:		/* FTANH */
					D(bug("FTANH %.04f\r\n",(float)src));
					regs.fp[reg] = tanh (src);
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x0a:		/* FATAN */
					D(bug("FATAN %.04f\r\n",(float)src));
					regs.fp[reg] = atan (src);
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x0c:		/* FASIN */
					D(bug("FASIN %.04f\r\n",(float)src));
					regs.fp[reg] = asin (src);
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x0d:		/* FATANH */
					D(bug("FATANH %.04f\r\n",(float)src));
#if 1				/* The BeBox doesn't have atanh, and it isn't in the HPUX libm either */
					regs.fp[reg] = log ((1 + src) / (1 - src)) / 2;
#else
					regs.fp[reg] = atanh (src);
#endif
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x0e:		/* FSIN */
					D(bug("FSIN %.04f\r\n",(float)src));
					regs.fp[reg] = sin (src);
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x0f:		/* FTAN */
					D(bug("FTAN %.04f\r\n",(float)src));
					regs.fp[reg] = tan (src);
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x10:		/* FETOX */
					D(bug("FETOX %.04f\r\n",(float)src));
					regs.fp[reg] = exp (src);
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x11:		/* FTWOTOX */
					D(bug("FTWOTOX %.04f\r\n",(float)src));
					regs.fp[reg] = pow(2.0, src);
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x12:		/* FTENTOX */
					D(bug("FTENTOX %.04f\r\n",(float)src));
					regs.fp[reg] = pow(10.0, src);
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x14:		/* FLOGN */
					D(bug("FLOGN %.04f\r\n",(float)src));
					regs.fp[reg] = log (src);
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x15:		/* FLOG10 */
					D(bug("FLOG10 %.04f\r\n",(float)src));
					regs.fp[reg] = log10 (src);
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x16:		/* FLOG2 */
					D(bug("FLOG2 %.04f\r\n",(float)src));
					regs.fp[reg] = log (src) / log (2.0);
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x18:		/* FABS */
					D(bug("FABS %.04f\r\n",(float)src));
					regs.fp[reg] = src < 0 ? -src : src;
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x19:		/* FCOSH */
					D(bug("FCOSH %.04f\r\n",(float)src));
					regs.fp[reg] = cosh(src);
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x1a:		/* FNEG */
					D(bug("FNEG %.04f\r\n",(float)src));
					regs.fp[reg] = -src;
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x1c:		/* FACOS */
					D(bug("FACOS %.04f\r\n",(float)src));
					regs.fp[reg] = acos(src);
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x1d:		/* FCOS */
					D(bug("FCOS %.04f\r\n",(float)src));
					regs.fp[reg] = cos(src);
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x1e:		/* FGETEXP */
					D(bug("FGETEXP %.04f\r\n",(float)src));
#if HAVE_IEEE_DOUBLE
					if( IS_INFINITY((uae_u32 *)&src) ) {
						MAKE_NAN( (uae_u32 *)&regs.fp[reg] );
					} else {
						regs.fp[reg] = FAST_FGETEXP( (uae_u32 *)&src );
					}
#else
					if(src == 0) {
						regs.fp[reg] = (double)0;
					} else {
						int expon;
						frexp (src, &expon);
						regs.fp[reg] = (double) (expon - 1);
					}
#endif
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
				case 0x1f:		/* FGETMAN */
					D(bug("FGETMAN %.04f\r\n",(float)src));
#if HAVE_IEEE_DOUBLE
					if( src == 0 ) {
						regs.fp[reg] = 0;
					} else if( IS_INFINITY((uae_u32 *)&src) ) {
						MAKE_NAN( (uae_u32 *)&regs.fp[reg] );
					} else {
						regs.fp[reg] = src;
						FAST_REMOVE_EXPONENT( (uae_u32 *)&regs.fp[reg] );
					}
#else
					{
						int expon;
						regs.fp[reg] = frexp (src, &expon) * 2.0;
					}
#endif
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
					break;
			case 0x20:		/* FDIV */
				D(bug("FDIV %.04f\r\n",(float)src));
				regs.fp[reg] /= src;
				MAKE_FPSR(regs.fpsr,regs.fp[reg]);
				break;
			case 0x21:		/* FMOD */
				D(bug("FMOD %.04f\r\n",(float)src));
				// regs.fp[reg] = regs.fp[reg] - (double) ((int) (regs.fp[reg] / src)) * src;
				{ double quot = round_to_zero(regs.fp[reg] / src);
#if HAVE_IEEE_DOUBLE
					uae_u32 sign = GET_QUOTIENT_SIGN((uae_u32 *)&regs.fp[reg],(uae_u32 *)&src);
#endif
					regs.fp[reg] = regs.fp[reg] - quot * src;
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
#if HAVE_IEEE_DOUBLE
					regs.fpsr = MAKE_QUOTIENT(regs.fpsr,quot,sign);
#endif
				}
				break;
			case 0x22:		/* FADD */
				D(bug("FADD %.04f\r\n",(float)src));
				regs.fp[reg] += src;
				MAKE_FPSR(regs.fpsr,regs.fp[reg]);
				break;
			case 0x23:		/* FMUL */
				D(bug("FMUL %.04f\r\n",(float)src));
#if HAVE_IEEE_DOUBLE
				GET_DEST_FLAGS((uae_u32 *)&regs.fp[reg]);
				GET_SOURCE_FLAGS((uae_u32 *)&src);
				if(fl_dest.in_range && fl_source.in_range) {
					regs.fp[reg] *= src;
				} else if( fl_dest.nan || fl_source.nan || 
					fl_dest.zero && fl_source.infinity || 
					fl_dest.infinity && fl_source.zero )
				{
					MAKE_NAN( (uae_u32 *)&regs.fp[reg] );
				} else if( fl_dest.zero || fl_source.zero ) {
					if( fl_dest.negative && !fl_source.negative ||
					    !fl_dest.negative && fl_source.negative) 
					{
						MAKE_ZERO_NEGATIVE((uae_u32 *)&regs.fp[reg]);
					} else {
						MAKE_ZERO_POSITIVE((uae_u32 *)&regs.fp[reg]);
					}
				} else {
					if( fl_dest.negative && !fl_source.negative ||
					    !fl_dest.negative && fl_source.negative) 
					{
						MAKE_INF_NEGATIVE((uae_u32 *)&regs.fp[reg]);
					} else {
						MAKE_INF_POSITIVE((uae_u32 *)&regs.fp[reg]);
					}
				}
#else
				D(bug("FMUL %.04f\r\n",(float)src));
				regs.fp[reg] *= src;
#endif
				MAKE_FPSR(regs.fpsr,regs.fp[reg]);
				break;
			case 0x24:		/* FSGLDIV */
				D(bug("FSGLDIV %.04f\r\n",(float)src));
				// TODO: round to float.
				regs.fp[reg] /= src;
				MAKE_FPSR(regs.fpsr,regs.fp[reg]);
				break;
			case 0x25:		/* FREM */
				D(bug("FREM %.04f\r\n",(float)src));
				// regs.fp[reg] = regs.fp[reg] - (double) ((int) (regs.fp[reg] / src + 0.5)) * src;
				{ double quot = round_to_nearest(regs.fp[reg] / src);
#if HAVE_IEEE_DOUBLE
					uae_u32 sign = GET_QUOTIENT_SIGN((uae_u32 *)&regs.fp[reg],(uae_u32 *)&src);
#endif
					regs.fp[reg] = regs.fp[reg] - quot * src;
					MAKE_FPSR(regs.fpsr,regs.fp[reg]);
#if HAVE_IEEE_DOUBLE
					regs.fpsr = MAKE_QUOTIENT(regs.fpsr,quot,sign);
#endif
				}
				break;

			case 0x26:		/* FSCALE */
				D(bug("FSCALE %.04f\r\n",(float)src));

				// TODO:
				// Overflow, underflow

#if HAVE_IEEE_DOUBLE
				if( IS_INFINITY((uae_u32 *)&regs.fp[reg]) ) {
					MAKE_NAN( (uae_u32 *)&regs.fp[reg] );
				} else {
					// When the absolute value of the source operand is >= 2^14,
					// an overflow or underflow always results.
					// Here (int) cast is okay.
					FAST_SCALE( (uae_u32 *)&regs.fp[reg], (int)round_to_zero(src) );
				}
#else
				if(src != 0) { // Manual says: src==0 -> FPn
					regs.fp[reg] *= exp (log (2.0) * src);
				}
#endif
				MAKE_FPSR(regs.fpsr,regs.fp[reg]);
				break;
			case 0x27:		/* FSGLMUL */
				D(bug("FSGLMUL %.04f\r\n",(float)src));
				regs.fp[reg] *= src;
				MAKE_FPSR(regs.fpsr,regs.fp[reg]);
				break;
			case 0x28:		/* FSUB */
				D(bug("FSUB %.04f\r\n",(float)src));
				regs.fp[reg] -= src;
				MAKE_FPSR(regs.fpsr,regs.fp[reg]);
				break;
			case 0x30:		/* FSINCOS */
			case 0x31:
			case 0x32:
			case 0x33:
			case 0x34:
			case 0x35:
			case 0x36:
			case 0x37:
				D(bug("FSINCOS %.04f\r\n",(float)src));
				// Cosine must be calculated first if same register
				regs.fp[extra & 7] = cos(src);
				regs.fp[reg] = sin (src);
				// Set fpsr according to the sine result
				MAKE_FPSR(regs.fpsr,regs.fp[reg]);
				break;
			case 0x38:		/* FCMP */
				D(bug("FCMP %.04f\r\n",(float)src));

				// The infinity bit is always cleared by the FCMP
				// instruction since it is not used by any of the
				// conditional predicate equations.

#if HAVE_IEEE_DOUBLE
				if( IS_INFINITY((uae_u32 *)&src) ) {
					if( IS_NEGATIVE((uae_u32 *)&src) ) {
						// negative infinity
						if( IS_INFINITY((uae_u32 *)&regs.fp[reg]) && IS_NEGATIVE((uae_u32 *)&regs.fp[reg]) ) {
							// Zero, Negative
							regs.fpsr = (regs.fpsr & 0x00FFFFFF) | 0x4000000 | 0x8000000;
							D(bug("-INF cmp -INF -> NZ\r\n"));
						} else {
							// None
							regs.fpsr = (regs.fpsr & 0x00FFFFFF);
							D(bug("x cmp -INF -> None\r\n"));
						}
					} else {
						// positive infinity
						if( IS_INFINITY((uae_u32 *)&regs.fp[reg]) && !IS_NEGATIVE((uae_u32 *)&regs.fp[reg]) ) {
							// Zero
							regs.fpsr = (regs.fpsr & 0x00FFFFFF) | 0x4000000;
							D(bug("+INF cmp +INF -> Z\r\n"));
						} else {
							// Negative
							regs.fpsr = (regs.fpsr & 0x00FFFFFF) | 0x8000000;
							D(bug("X cmp +INF -> N\r\n"));
						}
					}
				} else {
					double tmp = regs.fp[reg] - src;
					regs.fpsr = (regs.fpsr & 0x00FFFFFF) | (tmp == 0 ? 0x4000000 : 0) | (tmp < 0 ? 0x8000000 : 0);
				}
#else
				{
					double tmp = regs.fp[reg] - src;
					MAKE_FPSR(regs.fpsr,tmp);
				}
#endif
				break;
			case 0x3a:		/* FTST */
				D(bug("FTST %.04f\r\n",(float)src));
				// MAKE_FPSR(regs.fpsr,regs.fp[reg]);
				MAKE_FPSR(regs.fpsr,src);
				break;
			default:
				D(bug("ILLEGAL F OP %X\r\n",opcode));
				m68k_setpc (m68k_getpc () - 4);
				op_illg (opcode);
				break;
		}
		dump_fp_regs( "END  ");
		return;
  }
	D(bug("ILLEGAL F OP 2 %X\r\n",opcode));
  m68k_setpc (m68k_getpc () - 4);
  op_illg (opcode);
	dump_fp_regs( "END  ");
}
