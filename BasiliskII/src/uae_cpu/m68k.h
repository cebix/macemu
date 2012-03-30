/* 
 * UAE - The Un*x Amiga Emulator
 * 
 * MC68000 emulation - machine dependent bits
 *
 * Copyright 1996 Bernd Schmidt
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
 */

#ifndef M68K_FLAGS_H
#define M68K_FLAGS_H

#ifdef OPTIMIZED_FLAGS

#if (defined(__i386__) && defined(X86_ASSEMBLY)) || (defined(__x86_64__) && defined(X86_64_ASSEMBLY))

#ifndef SAHF_SETO_PROFITABLE

/* PUSH/POP instructions are naturally 64-bit sized on x86-64, thus
   unsigned long hereunder is either 64-bit or 32-bit wide depending
   on the target.  */
struct flag_struct {
    unsigned long cznv;
    unsigned long x;
};

#define FLAGVAL_Z	0x40
#define FLAGVAL_N	0x80

#define SET_ZFLG(y)	(regflags.cznv = (((uae_u32)regflags.cznv) & ~0x40) | (((y) & 1) << 6))
#define SET_CFLG(y)	(regflags.cznv = (((uae_u32)regflags.cznv) & ~1) | ((y) & 1))
#define SET_VFLG(y)	(regflags.cznv = (((uae_u32)regflags.cznv) & ~0x800) | (((y) & 1) << 11))
#define SET_NFLG(y)	(regflags.cznv = (((uae_u32)regflags.cznv) & ~0x80) | (((y) & 1) << 7))
#define SET_XFLG(y)	(regflags.x = (y))

#define GET_ZFLG	((regflags.cznv >> 6) & 1)
#define GET_CFLG	(regflags.cznv & 1)
#define GET_VFLG	((regflags.cznv >> 11) & 1)
#define GET_NFLG	((regflags.cznv >> 7) & 1)
#define GET_XFLG	(regflags.x & 1)

#define CLEAR_CZNV	(regflags.cznv = 0)
#define GET_CZNV	(regflags.cznv)
#define IOR_CZNV(X)	(regflags.cznv |= (X))
#define SET_CZNV(X)	(regflags.cznv = (X))

#define COPY_CARRY	(regflags.x = regflags.cznv)

extern struct flag_struct regflags __asm__ ("regflags");

static __inline__ int cctrue(int cc)
{
    uae_u32 cznv = regflags.cznv;
    switch(cc){
     case 0: return 1;                       /* T */
     case 1: return 0;                       /* F */
     case 2: return (cznv & 0x41) == 0; /* !GET_CFLG && !GET_ZFLG;  HI */
     case 3: return (cznv & 0x41) != 0; /* GET_CFLG || GET_ZFLG;    LS */
     case 4: return (cznv & 1) == 0;        /* !GET_CFLG;               CC */
     case 5: return (cznv & 1) != 0;           /* GET_CFLG;                CS */
     case 6: return (cznv & 0x40) == 0; /* !GET_ZFLG;               NE */
     case 7: return (cznv & 0x40) != 0; /* GET_ZFLG;                EQ */
     case 8: return (cznv & 0x800) == 0;/* !GET_VFLG;               VC */
     case 9: return (cznv & 0x800) != 0;/* GET_VFLG;                VS */
     case 10:return (cznv & 0x80) == 0; /* !GET_NFLG;               PL */
     case 11:return (cznv & 0x80) != 0; /* GET_NFLG;                MI */
     case 12:return (((cznv << 4) ^ cznv) & 0x800) == 0; /* GET_NFLG == GET_VFLG;             GE */
     case 13:return (((cznv << 4) ^ cznv) & 0x800) != 0;/* GET_NFLG != GET_VFLG;             LT */
     case 14:
	cznv &= 0x8c0;
	return (((cznv << 4) ^ cznv) & 0x840) == 0; /* !GET_ZFLG && (GET_NFLG == GET_VFLG);  GT */
     case 15:
	cznv &= 0x8c0;
	return (((cznv << 4) ^ cznv) & 0x840) != 0; /* GET_ZFLG || (GET_NFLG != GET_VFLG);   LE */
    }
    return 0;
}

#define optflag_testl(v) \
  __asm__ __volatile__ ("andl %1,%1\n\t" \
			"pushf\n\t" \
			"pop %0\n\t" \
			: "=r" (regflags.cznv) : "r" (v) : "cc")

#define optflag_testw(v) \
  __asm__ __volatile__ ("andw %w1,%w1\n\t" \
			"pushf\n\t" \
			"pop %0\n\t" \
			: "=r" (regflags.cznv) : "r" (v) : "cc")

#define optflag_testb(v) \
  __asm__ __volatile__ ("andb %b1,%b1\n\t" \
			"pushf\n\t" \
			"pop %0\n\t" \
			: "=r" (regflags.cznv) : "q" (v) : "cc")

#define optflag_addl(v, s, d) do { \
  __asm__ __volatile__ ("addl %k2,%k1\n\t" \
			"pushf\n\t" \
			"pop %0\n\t" \
			: "=r" (regflags.cznv), "=r" (v) : "rmi" (s), "1" (d) : "cc"); \
    COPY_CARRY; \
    } while (0)

#define optflag_addw(v, s, d) do { \
  __asm__ __volatile__ ("addw %w2,%w1\n\t" \
			"pushf\n\t" \
			"pop %0\n\t" \
			: "=r" (regflags.cznv), "=r" (v) : "rmi" (s), "1" (d) : "cc"); \
    COPY_CARRY; \
    } while (0)

#define optflag_addb(v, s, d) do { \
  __asm__ __volatile__ ("addb %b2,%b1\n\t" \
			"pushf\n\t" \
			"pop %0\n\t" \
			: "=r" (regflags.cznv), "=q" (v) : "qmi" (s), "1" (d) : "cc"); \
    COPY_CARRY; \
    } while (0)

#define optflag_subl(v, s, d) do { \
  __asm__ __volatile__ ("subl %k2,%k1\n\t" \
			"pushf\n\t" \
			"pop %0\n\t" \
			: "=r" (regflags.cznv), "=r" (v) : "rmi" (s), "1" (d) : "cc"); \
    COPY_CARRY; \
    } while (0)

#define optflag_subw(v, s, d) do { \
  __asm__ __volatile__ ("subw %w2,%w1\n\t" \
			"pushf\n\t" \
			"pop %0\n\t" \
			: "=r" (regflags.cznv), "=r" (v) : "rmi" (s), "1" (d) : "cc"); \
    COPY_CARRY; \
    } while (0)

#define optflag_subb(v, s, d) do { \
  __asm__ __volatile__ ("subb %b2,%b1\n\t" \
			"pushf\n\t" \
			"pop %0\n\t" \
			: "=r" (regflags.cznv), "=q" (v) : "qmi" (s), "1" (d) : "cc"); \
    COPY_CARRY; \
    } while (0)

#define optflag_cmpl(s, d) \
  __asm__ __volatile__ ("cmpl %k1,%k2\n\t" \
			"pushf\n\t" \
			"pop %0\n\t" \
			: "=r" (regflags.cznv) : "rmi" (s), "r" (d) : "cc")

#define optflag_cmpw(s, d) \
  __asm__ __volatile__ ("cmpw %w1,%w2\n\t" \
			"pushf\n\t" \
			"pop %0\n\t" \
			: "=r" (regflags.cznv) : "rmi" (s), "r" (d) : "cc")

#define optflag_cmpb(s, d) \
  __asm__ __volatile__ ("cmpb %b1,%b2\n\t" \
			"pushf\n\t" \
			"pop %0\n\t" \
			: "=r" (regflags.cznv) : "qmi" (s), "q" (d) : "cc")

#else

struct flag_struct {
    uae_u32 cznv;
    uae_u32 x;
};

#define FLAGVAL_Z	0x4000
#define FLAGVAL_N	0x8000

#define SET_ZFLG(y)	(regflags.cznv = (regflags.cznv & ~0x4000) | (((y) & 1) << 14))
#define SET_CFLG(y)	(regflags.cznv = (regflags.cznv & ~0x100) | (((y) & 1) << 8))
#define SET_VFLG(y)	(regflags.cznv = (regflags.cznv & ~0x1) | (((y) & 1)))
#define SET_NFLG(y)	(regflags.cznv = (regflags.cznv & ~0x8000) | (((y) & 1) << 15))
#define SET_XFLG(y)	(regflags.x = (y))

#define GET_ZFLG	((regflags.cznv >> 14) & 1)
#define GET_CFLG	((regflags.cznv >> 8) & 1)
#define GET_VFLG	((regflags.cznv >> 0) & 1)
#define GET_NFLG	((regflags.cznv >> 15) & 1)
#define GET_XFLG	(regflags.x & 1)

#define CLEAR_CZNV 	(regflags.cznv = 0)
#define GET_CZNV	(regflags.cznv)
#define IOR_CZNV(X)	(regflags.cznv |= (X))
#define SET_CZNV(X)	(regflags.cznv = (X))

#define COPY_CARRY	(regflags.x = (regflags.cznv)>>8)

extern struct flag_struct regflags __asm__ ("regflags");

static __inline__ int cctrue(int cc)
{
    uae_u32 cznv = regflags.cznv;
    switch(cc){
     case 0: return 1;                       /* T */
     case 1: return 0;                       /* F */
     case 2: return (cznv & 0x4100) == 0; /* !GET_CFLG && !GET_ZFLG;  HI */
     case 3: return (cznv & 0x4100) != 0; /* GET_CFLG || GET_ZFLG;    LS */
     case 4: return (cznv & 0x100) == 0;  /* !GET_CFLG;               CC */
     case 5: return (cznv & 0x100) != 0;  /* GET_CFLG;                CS */
     case 6: return (cznv & 0x4000) == 0; /* !GET_ZFLG;               NE */
     case 7: return (cznv & 0x4000) != 0; /* GET_ZFLG;                EQ */
     case 8: return (cznv & 0x01) == 0;   /* !GET_VFLG;               VC */
     case 9: return (cznv & 0x01) != 0;   /* GET_VFLG;                VS */
     case 10:return (cznv & 0x8000) == 0; /* !GET_NFLG;               PL */
     case 11:return (cznv & 0x8000) != 0; /* GET_NFLG;                MI */
     case 12:return (((cznv << 15) ^ cznv) & 0x8000) == 0; /* GET_NFLG == GET_VFLG;             GE */
     case 13:return (((cznv << 15) ^ cznv) & 0x8000) != 0;/* GET_NFLG != GET_VFLG;             LT */
     case 14:
	cznv &= 0xc001;
	return (((cznv << 15) ^ cznv) & 0xc000) == 0; /* !GET_ZFLG && (GET_NFLG == GET_VFLG);  GT */
     case 15:
	cznv &= 0xc001;
	return (((cznv << 15) ^ cznv) & 0xc000) != 0; /* GET_ZFLG || (GET_NFLG != GET_VFLG);   LE */
    }
    abort();
    return 0;
}

/* Manually emit LAHF instruction so that 64-bit assemblers can grok it */
#if defined __x86_64__ && defined __GNUC__
#define ASM_LAHF ".byte 0x9f"
#else
#define ASM_LAHF "lahf"
#endif

/* Is there any way to do this without declaring *all* memory clobbered?
   I.e. any way to tell gcc that some byte-sized value is in %al? */
#define optflag_testl(v) \
  __asm__ __volatile__ ("andl %0,%0\n\t" \
			ASM_LAHF "\n\t" \
			"seto %%al\n\t" \
			"movb %%al,regflags\n\t" \
			"movb %%ah,regflags+1\n\t" \
			: : "r" (v) : "%eax","cc","memory")

#define optflag_testw(v) \
  __asm__ __volatile__ ("andw %w0,%w0\n\t" \
			ASM_LAHF "\n\t" \
			"seto %%al\n\t" \
			"movb %%al,regflags\n\t" \
			"movb %%ah,regflags+1\n\t" \
			: : "r" (v) : "%eax","cc","memory")

#define optflag_testb(v) \
  __asm__ __volatile__ ("andb %b0,%b0\n\t" \
			ASM_LAHF "\n\t" \
			"seto %%al\n\t" \
			"movb %%al,regflags\n\t" \
			"movb %%ah,regflags+1\n\t" \
			: : "q" (v) : "%eax","cc","memory")

#define optflag_addl(v, s, d) do { \
  __asm__ __volatile__ ("addl %k1,%k0\n\t" \
			ASM_LAHF "\n\t" \
			"seto %%al\n\t" \
			"movb %%al,regflags\n\t" \
			"movb %%ah,regflags+1\n\t" \
			: "=r" (v) : "rmi" (s), "0" (d) : "%eax","cc","memory"); \
			COPY_CARRY; \
	} while (0)

#define optflag_addw(v, s, d) do { \
  __asm__ __volatile__ ("addw %w1,%w0\n\t" \
			ASM_LAHF "\n\t" \
			"seto %%al\n\t" \
			"movb %%al,regflags\n\t" \
			"movb %%ah,regflags+1\n\t" \
			: "=r" (v) : "rmi" (s), "0" (d) : "%eax","cc","memory"); \
			COPY_CARRY; \
    } while (0)

#define optflag_addb(v, s, d) do { \
  __asm__ __volatile__ ("addb %b1,%b0\n\t" \
			ASM_LAHF "\n\t" \
			"seto %%al\n\t" \
			"movb %%al,regflags\n\t" \
			"movb %%ah,regflags+1\n\t" \
			: "=q" (v) : "qmi" (s), "0" (d) : "%eax","cc","memory"); \
			COPY_CARRY; \
    } while (0)

#define optflag_subl(v, s, d) do { \
  __asm__ __volatile__ ("subl %k1,%k0\n\t" \
			ASM_LAHF "\n\t" \
			"seto %%al\n\t" \
			"movb %%al,regflags\n\t" \
			"movb %%ah,regflags+1\n\t" \
			: "=r" (v) : "rmi" (s), "0" (d) : "%eax","cc","memory"); \
			COPY_CARRY; \
    } while (0)

#define optflag_subw(v, s, d) do { \
  __asm__ __volatile__ ("subw %w1,%w0\n\t" \
			ASM_LAHF "\n\t" \
			"seto %%al\n\t" \
			"movb %%al,regflags\n\t" \
			"movb %%ah,regflags+1\n\t" \
			: "=r" (v) : "rmi" (s), "0" (d) : "%eax","cc","memory"); \
			COPY_CARRY; \
    } while (0)

#define optflag_subb(v, s, d) do { \
   __asm__ __volatile__ ("subb %b1,%b0\n\t" \
			ASM_LAHF "\n\t" \
			"seto %%al\n\t" \
			"movb %%al,regflags\n\t" \
			"movb %%ah,regflags+1\n\t" \
			: "=q" (v) : "qmi" (s), "0" (d) : "%eax","cc","memory"); \
			COPY_CARRY; \
    } while (0)

#define optflag_cmpl(s, d) \
  __asm__ __volatile__ ("cmpl %k0,%k1\n\t" \
			ASM_LAHF "\n\t" \
			"seto %%al\n\t" \
			"movb %%al,regflags\n\t" \
			"movb %%ah,regflags+1\n\t" \
			: : "rmi" (s), "r" (d) : "%eax","cc","memory")

#define optflag_cmpw(s, d) \
  __asm__ __volatile__ ("cmpw %w0,%w1\n\t" \
			ASM_LAHF "\n\t" \
			"seto %%al\n\t" \
			"movb %%al,regflags\n\t" \
			"movb %%ah,regflags+1\n\t" \
			: : "rmi" (s), "r" (d) : "%eax","cc","memory");

#define optflag_cmpb(s, d) \
  __asm__ __volatile__ ("cmpb %b0,%b1\n\t" \
			ASM_LAHF "\n\t" \
			"seto %%al\n\t" \
			"movb %%al,regflags\n\t" \
			"movb %%ah,regflags+1\n\t" \
			: : "qmi" (s), "q" (d) : "%eax","cc","memory")

#endif

#elif defined(__sparc__) && (defined(SPARC_V8_ASSEMBLY) || defined(SPARC_V9_ASSEMBLY))

struct flag_struct {
    unsigned char nzvc;
    unsigned char x;
};

extern struct flag_struct regflags;

#define FLAGVAL_Z	0x04
#define FLAGVAL_N	0x08

#define SET_ZFLG(y)	(regflags.nzvc = (regflags.nzvc & ~0x04) | (((y) & 1) << 2))
#define SET_CFLG(y)	(regflags.nzvc = (regflags.nzvc & ~1) | ((y) & 1))
#define SET_VFLG(y)	(regflags.nzvc = (regflags.nzvc & ~0x02) | (((y) & 1) << 1))
#define SET_NFLG(y)	(regflags.nzvc = (regflags.nzvc & ~0x08) | (((y) & 1) << 3))
#define SET_XFLG(y)	(regflags.x = (y))

#define GET_ZFLG	((regflags.nzvc >> 2) & 1)
#define GET_CFLG	(regflags.nzvc & 1)
#define GET_VFLG	((regflags.nzvc >> 1) & 1)
#define GET_NFLG	((regflags.nzvc >> 3) & 1)
#define GET_XFLG	(regflags.x & 1)

#define CLEAR_CZNV 	(regflags.nzvc = 0)
#define GET_CZNV	(reflags.nzvc)
#define IOR_CZNV(X)	(refglags.nzvc |= (X))
#define SET_CZNV(X)	(regflags.nzvc = (X))

#define COPY_CARRY (regflags.x = regflags.nzvc)

static __inline__ int cctrue(int cc)
{
    uae_u32 nzvc = regflags.nzvc;
    switch(cc){
     case 0: return 1;                       /* T */
     case 1: return 0;                       /* F */
     case 2: return (nzvc & 0x05) == 0; /* !GET_CFLG && !GET_ZFLG;  HI */
     case 3: return (nzvc & 0x05) != 0; /* GET_CFLG || GET_ZFLG;    LS */
     case 4: return (nzvc & 1) == 0;        /* !GET_CFLG;               CC */
     case 5: return (nzvc & 1) != 0;           /* GET_CFLG;                CS */
     case 6: return (nzvc & 0x04) == 0; /* !GET_ZFLG;               NE */
     case 7: return (nzvc & 0x04) != 0; /* GET_ZFLG;                EQ */
     case 8: return (nzvc & 0x02) == 0;/* !GET_VFLG;               VC */
     case 9: return (nzvc & 0x02) != 0;/* GET_VFLG;                VS */
     case 10:return (nzvc & 0x08) == 0; /* !GET_NFLG;               PL */
     case 11:return (nzvc & 0x08) != 0; /* GET_NFLG;                MI */
     case 12:return (((nzvc << 2) ^ nzvc) & 0x08) == 0; /* GET_NFLG == GET_VFLG;             GE */
     case 13:return (((nzvc << 2) ^ nzvc) & 0x08) != 0;/* GET_NFLG != GET_VFLG;             LT */
     case 14:
	nzvc &= 0x0e;
	return (((nzvc << 2) ^ nzvc) & 0x0c) == 0; /* !GET_ZFLG && (GET_NFLG == GET_VFLG);  GT */
     case 15:
	nzvc &= 0x0e;
	return (((nzvc << 2) ^ nzvc) & 0x0c) != 0; /* GET_ZFLG || (GET_NFLG != GET_VFLG);   LE */
    }
    return 0;
}

#ifdef SPARC_V8_ASSEMBLY

static inline uae_u32 sparc_v8_flag_add_8(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	sll		%2, 24, %%o0\n"
		"	sll		%3, 24, %%o1\n"
		"	addcc	%%o0, %%o1, %%o0\n"
		"	addx	%%g0, %%g0, %%o1	! X,C flags\n"
		"	srl		%%o0, 24, %0\n"
		"	stb		%%o1, [%1 + 1]\n"
		"	bl,a	.+8\n"
		"	or		%%o1, 0x08, %%o1	! N flag\n"
		"	bz,a	.+8\n"
		"	or		%%o1, 0x04, %%o1	! Z flag\n"
		"	bvs,a	.+8\n"
		"	or		%%o1, 0x02, %%o1	! V flag\n"
		"	stb		%%o1, [%1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
	return value;
}

static inline uae_u32 sparc_v8_flag_add_16(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	sll		%2, 16, %%o0\n"
		"	sll		%3, 16, %%o1\n"
		"	addcc	%%o0, %%o1, %%o0\n"
		"	addx	%%g0, %%g0, %%o1	! X,C flags\n"
		"	srl		%%o0, 16, %0\n"
		"	stb		%%o1, [%1 + 1]\n"
		"	bl,a	.+8\n"
		"	or		%%o1, 0x08, %%o1	! N flag\n"
		"	bz,a	.+8\n"
		"	or		%%o1, 0x04, %%o1	! Z flag\n"
		"	bvs,a	.+8\n"
		"	or		%%o1, 0x02, %%o1	! V flag\n"
		"	stb		%%o1, [%1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
	return value;
}

static inline uae_u32 sparc_v8_flag_add_32(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	addcc	%2, %3, %0\n"
		"	addx	%%g0, %%g0, %%o0	! X,C flags\n"
		"	stb		%%o0, [%1 + 1]\n"
		"	bl,a	.+8\n"
		"	or		%%o0, 0x08, %%o0	! N flag\n"
		"	bz,a	.+8\n"
		"	or		%%o0, 0x04, %%o0	! Z flag\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0	! V flag\n"
		"	stb		%%o0, [%1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0"
	);
	return value;
}

static inline uae_u32 sparc_v8_flag_sub_8(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	sll		%2, 24, %%o0\n"
		"	sll		%3, 24, %%o1\n"
		"	subcc	%%o0, %%o1, %%o0\n"
		"	addx	%%g0, %%g0, %%o1	! X,C flags\n"
		"	srl		%%o0, 24, %0\n"
		"	stb		%%o1, [%1 + 1]\n"
		"	bl,a	.+8\n"
		"	or		%%o1, 0x08, %%o1	! N flag\n"
		"	bz,a	.+8\n"
		"	or		%%o1, 0x04, %%o1	! Z flag\n"
		"	bvs,a	.+8\n"
		"	or		%%o1, 0x02, %%o1	! V flag\n"
		"	stb		%%o1, [%1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
	return value;
}

static inline uae_u32 sparc_v8_flag_sub_16(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	sll		%2, 16, %%o0\n"
		"	sll		%3, 16, %%o1\n"
		"	subcc	%%o0, %%o1, %%o0\n"
		"	addx	%%g0, %%g0, %%o1	! X,C flags\n"
		"	srl		%%o0, 16, %0\n"
		"	stb		%%o1, [%1 + 1]\n"
		"	bl,a	.+8\n"
		"	or		%%o1, 0x08, %%o1	! N flag\n"
		"	bz,a	.+8\n"
		"	or		%%o1, 0x04, %%o1	! Z flag\n"
		"	bvs,a	.+8\n"
		"	or		%%o1, 0x02, %%o1	! V flag\n"
		"	stb		%%o1, [%1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
	return value;
}

static inline uae_u32 sparc_v8_flag_sub_32(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	subcc	%2, %3, %0\n"
		"	addx	%%g0, %%g0, %%o0	! X,C flags\n"
		"	stb		%%o0, [%1 + 1]\n"
		"	bl,a	.+8\n"
		"	or		%%o0, 0x08, %%o0	! N flag\n"
		"	bz,a	.+8\n"
		"	or		%%o0, 0x04, %%o0	! Z flag\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0	! V flag\n"
		"	stb		%%o0, [%1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0"
	);
	return value;
}

static inline void sparc_v8_flag_cmp_8(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	__asm__	("\n"
		"	sll		%1, 24, %%o0\n"
		"	sll		%2, 24, %%o1\n"
		"	subcc	%%o0, %%o1, %%g0\n"
		"	addx	%%g0, %%g0, %%o0	! C flag\n"
		"	bl,a	.+8\n"
		"	or		%%o0, 0x08, %%o0	! N flag\n"
		"	bz,a	.+8\n"
		"	or		%%o0, 0x04, %%o0	! Z flag\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0	! V flag\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
}

static inline void sparc_v8_flag_cmp_16(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	__asm__	("\n"
		"	sll		%1, 16, %%o0\n"
		"	sll		%2, 16, %%o1\n"
		"	subcc	%%o0, %%o1, %%g0\n"
		"	addx	%%g0, %%g0, %%o0	! C flag\n"
		"	bl,a	.+8\n"
		"	or		%%o0, 0x08, %%o0	! N flag\n"
		"	bz,a	.+8\n"
		"	or		%%o0, 0x04, %%o0	! Z flag\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0	! V flag\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
}

static inline void sparc_v8_flag_cmp_32(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	__asm__	("\n"
		"	subcc	%1, %2, %%o1\n"
		"	srl		%%o1, 31, %%o0\n"
		"	sll		%%o0, 3, %%o0\n"
		"	addx	%%o0, %%g0, %%o0\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0\n"
		"	subcc	%%g0, %%o1, %%g0\n"
		"	addx	%%g0, 7, %%o1\n"
		"	and		%%o1, 0x04, %%o1\n"
		"	or		%%o0, %%o1, %%o0\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
}

static inline uae_u32 sparc_v8_flag_addx_8(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	ldub	[%1 + 1], %%o1		! Get the X Flag\n"
		"	subcc	%%g0, %%o1, %%g0	! Set the SPARC carry flag, if X set\n"
		"	addxcc	%2, %3, %0\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
	return value;
}

#if 0
VERY SLOW...
static inline uae_u32 sparc_v8_flag_addx_8(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	sll		%2, 24, %%o0\n"
		"	sll		%3, 24, %%o1\n"
		"	addcc	%%o0, %%o1, %%o0\n"
		"	addx	%%g0, %%g0, %%o1	! X,C flags\n"
		"	bvs,a	.+8\n"
		"	or		%%o1, 0x02, %%o1	! V flag\n"
		"	ldub	[%1 + 1], %%o2\n"
		"	subcc	%%g0, %%o2, %%g0\n"
		"	addx	%%g0, %%g0, %%o2\n"
		"	sll		%%o2, 24, %%o2\n"
		"	addcc	%%o0, %%o2, %%o0\n"
		"	srl		%%o0, 24, %0\n"
		"	addx	%%g0, %%g0, %%o2\n"
		"	or		%%o1, %%o2, %%o1	! update X,C flags\n"
		"	bl,a	.+8\n"
		"	or		%%o1, 0x08, %%o1	! N flag\n"
		"	ldub	[%1], %%o0			! retreive the old NZVC flags (XXX)\n"
		"	bvs,a	.+8\n"
		"	or		%%o1, 0x02, %%o1	! update V flag\n"
		"	and		%%o0, 0x04, %%o0	! (XXX) but keep only Z flag\n"
		"	and		%%o1, 1, %%o2		! keep C flag in %%o2\n"
		"	bnz,a	.+8\n"
		"	or		%%g0, %%g0, %%o0	! Z flag cleared if non-zero result\n"
		"	stb		%%o2, [%1 + 1]		! store the X flag\n"
		"	or		%%o1, %%o0, %%o1\n"
		"	stb		%%o1, [%1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1", "o2"
	);
	return value;
}
#endif

static inline uae_u32 sparc_v8_flag_addx_32(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	ldub	[%1 + 1], %%o0		! Get the X Flag\n"
		"	subcc	%%g0, %%o0, %%g0	! Set the SPARC carry flag, if X set\n"
		"	addxcc	%2, %3, %0\n"
		"	ldub	[%1], %%o0			! retreive the old NZVC flags\n"
		"	and		%%o0, 0x04, %%o0	! but keep only Z flag\n"
		"	addx	%%o0, %%g0, %%o0	! X,C flags\n"
		"	bl,a	.+8\n"
		"	or		%%o0, 0x08, %%o0	! N flag\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0	! V flag\n"
		"	bnz,a	.+8\n"
		"	and		%%o0, 0x0B, %%o0	! Z flag cleared if result is non-zero\n"
		"	stb		%%o0, [%1]\n"
		"	stb		%%o0, [%1 + 1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0"
	);
	return value;
}

#endif /* SPARC_V8_ASSEMBLY */

#ifdef SPARC_V9_ASSEMBLY

static inline uae_u32 sparc_v9_flag_add_8(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	sll		%2, 24, %%o0\n"
		"	sll		%3, 24, %%o1\n"
		"	addcc	%%o0, %%o1, %%o0\n"
		"	rd		%%ccr, %%o1\n"
		"	srl		%%o0, 24, %0\n"
		"	stb		%%o1, [%1]\n"
		"	stb		%%o1, [%1+1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
	return value;
}

static inline uae_u32 sparc_v9_flag_add_16(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	sll		%2, 16, %%o0\n"
		"	sll		%3, 16, %%o1\n"
		"	addcc	%%o0, %%o1, %%o0\n"
		"	rd		%%ccr, %%o1\n"
		"	srl		%%o0, 16, %0\n"
		"	stb		%%o1, [%1]\n"
		"	stb		%%o1, [%1+1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
	return value;
}

static inline uae_u32 sparc_v9_flag_add_32(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	addcc	%2, %3, %0\n"
		"	rd		%%ccr, %%o0\n"
		"	stb		%%o0, [%1]\n"
		"	stb		%%o0, [%1+1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0"
	);
	return value;
}

static inline uae_u32 sparc_v9_flag_sub_8(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	sll		%2, 24, %%o0\n"
		"	sll		%3, 24, %%o1\n"
		"	subcc	%%o0, %%o1, %%o0\n"
		"	rd		%%ccr, %%o1\n"
		"	srl		%%o0, 24, %0\n"
		"	stb		%%o1, [%1]\n"
		"	stb		%%o1, [%1+1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
	return value;
}

static inline uae_u32 sparc_v9_flag_sub_16(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	sll		%2, 16, %%o0\n"
		"	sll		%3, 16, %%o1\n"
		"	subcc	%%o0, %%o1, %%o0\n"
		"	rd		%%ccr, %%o1\n"
		"	srl		%%o0, 16, %0\n"
		"	stb		%%o1, [%1]\n"
		"	stb		%%o1, [%1+1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
	return value;
}

static inline uae_u32 sparc_v9_flag_sub_32(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	subcc	%2, %3, %0\n"
		"	rd		%%ccr, %%o0\n"
		"	stb		%%o0, [%1]\n"
		"	stb		%%o0, [%1+1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0"
	);
	return value;
}

static inline void sparc_v9_flag_cmp_8(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	__asm__	("\n"
		"	sll		%1, 24, %%o0\n"
		"	sll		%2, 24, %%o1\n"
		"	subcc	%%o0, %%o1, %%g0\n"
		"	rd		%%ccr, %%o0\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
}

static inline void sparc_v9_flag_cmp_16(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	__asm__	("\n"
		"	sll		%1, 16, %%o0\n"
		"	sll		%2, 16, %%o1\n"
		"	subcc	%%o0, %%o1, %%g0\n"
		"	rd		%%ccr, %%o0\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
}

static inline void sparc_v9_flag_cmp_32(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	__asm__	("\n"
		"	subcc	%1, %2, %%g0\n"
#if 0
		"	subcc	%1, %2, %%o1\n"
		"	srl		%%o1, 31, %%o0\n"
		"	sll		%%o0, 3, %%o0\n"
		"	addx	%%o0, %%g0, %%o0\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0\n"
		"	subcc	%%g0, %%o1, %%g0\n"
		"	addx	%%g0, 7, %%o1\n"
		"	and		%%o1, 0x04, %%o1\n"
		"	or		%%o0, %%o1, %%o0\n"
#endif
#if 0
		"	subcc	%1, %2, %%o1\n"
		"	srl		%%o1, 31, %%o0\n"
		"	sll		%%o0, 3, %%o0\n"
		"	addx	%%o0, %%g0, %%o0\n"
		"	bvs,pt,a	.+8\n"
		"	or		%%o0, 0x02, %%o0\n"
		"	subcc	%%g0, %%o1, %%g0\n"
		"	addx	%%g0, 7, %%o1\n"
		"	and		%%o1, 0x04, %%o1\n"
		"	or		%%o0, %%o1, %%o0\n"
		"	stb		%%o0, [%0]\n"
#endif
		"	rd		%%ccr, %%o0\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
}

#if 1
static inline void sparc_v9_flag_test_8(flag_struct *flags, uae_u32 val)
{
	__asm__	("\n"
		"	sll		%1, 24, %%o0\n"
		"	subcc	%%o0, %%g0, %%g0\n"
		"	rd		%%ccr, %%o0\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (val)
	:	"cc", "o0"
	);
}

static inline void sparc_v9_flag_test_16(flag_struct *flags, uae_u32 val)
{
	__asm__	("\n"
		"	sll		%1, 16, %%o0\n"
		"	subcc	%%o0, %%g0, %%g0\n"
		"	rd		%%ccr, %%o0\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (val)
	:	"cc", "o0"
	);
}

static inline void sparc_v9_flag_test_32(flag_struct *flags, uae_u32 val)
{
	__asm__	("\n"
		"	subcc	%1, %%g0, %%g0\n"
		"	rd		%%ccr, %%o0\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (val)
	:	"cc", "o0"
	);
}
#else
static inline void sparc_v9_flag_test_8(flag_struct *flags, uae_u32 val)
{
	__asm__	("\n"
		"	sll		%1, 24, %%o0\n"
		"	subcc	%%o0, %%g0, %%o1\n"
		"	srl		%%o1, 31, %%o0\n"
		"	sll		%%o0, 3, %%o0\n"
		"	addx	%%o0, %%g0, %%o0\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0\n"
		"	subcc	%%g0, %%o1, %%g0\n"
		"	addx	%%g0, 7, %%o1\n"
		"	and		%%o1, 0x04, %%o1\n"
		"	or		%%o0, %%o1, %%o0\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (val)
	:	"cc", "o0", "o1"
	);
}

static inline void sparc_v9_flag_test_16(flag_struct *flags, uae_u32 val)
{
	__asm__	("\n"
		"	sll		%1, 16, %%o0\n"
		"	subcc	%%o0, %%g0, %%o1\n"
		"	srl		%%o1, 31, %%o0\n"
		"	sll		%%o0, 3, %%o0\n"
		"	addx	%%o0, %%g0, %%o0\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0\n"
		"	subcc	%%g0, %%o1, %%g0\n"
		"	addx	%%g0, 7, %%o1\n"
		"	and		%%o1, 0x04, %%o1\n"
		"	or		%%o0, %%o1, %%o0\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (val)
	:	"cc", "o0", "o1"
	);
}

static inline void sparc_v9_flag_test_32(flag_struct *flags, uae_u32 val)
{
	__asm__	("\n"
		"	subcc	%1, %%g0, %%o1\n"
		"	srl		%%o1, 31, %%o0\n"
		"	sll		%%o0, 3, %%o0\n"
		"	addx	%%o0, %%g0, %%o0\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0\n"
		"	subcc	%%g0, %%o1, %%g0\n"
		"	addx	%%g0, 7, %%o1\n"
		"	and		%%o1, 0x04, %%o1\n"
		"	or		%%o0, %%o1, %%o0\n"
		"	stb		%%o0, [%0]\n"
	:	/* no outputs */
	:	"r" (flags), "r" (val)
	:	"cc", "o0", "o1"
	);
}
#endif

static inline uae_u32 sparc_v9_flag_addx_8(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	ldub	[%1 + 1], %%o1		! Get the X Flag\n"
		"	subcc	%%g0, %%o1, %%g0	! Set the SPARC carry flag, if X set\n"
		"	addxcc	%2, %3, %0\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0", "o1"
	);
	return value;
}

static inline uae_u32 sparc_v9_flag_addx_32(flag_struct *flags, uae_u32 src, uae_u32 dst)
{
	uae_u32 value;
	__asm__	("\n"
		"	ldub	[%1 + 1], %%o0		! Get the X Flag\n"
		"	subcc	%%g0, %%o0, %%g0	! Set the SPARC carry flag, if X set\n"
		"	addxcc	%2, %3, %0\n"
		"	ldub	[%1], %%o0			! retreive the old NZVC flags\n"
		"	and		%%o0, 0x04, %%o0	! but keep only Z flag\n"
		"	addx	%%o0, %%g0, %%o0	! X,C flags\n"
		"	bl,a	.+8\n"
		"	or		%%o0, 0x08, %%o0	! N flag\n"
		"	bvs,a	.+8\n"
		"	or		%%o0, 0x02, %%o0	! V flag\n"
		"	bnz,a	.+8\n"
		"	and		%%o0, 0x0B, %%o0	! Z flag cleared if result is non-zero\n"
		"	stb		%%o0, [%1]\n"
		"	stb		%%o0, [%1 + 1]\n"
	:	"=&r" (value)
	:	"r" (flags), "r" (dst), "r" (src)
	:	"cc", "o0"
	);
	return value;
}

#endif /* SPARC_V9_ASSEMBLY */

#endif

#else

struct flag_struct {
    unsigned int c;
    unsigned int z;
    unsigned int n;
    unsigned int v; 
    unsigned int x;
};

extern struct flag_struct regflags;

#define ZFLG (regflags.z)
#define NFLG (regflags.n)
#define CFLG (regflags.c)
#define VFLG (regflags.v)
#define XFLG (regflags.x)

#define SET_CFLG(x) (CFLG = (x))
#define SET_NFLG(x) (NFLG = (x))
#define SET_VFLG(x) (VFLG = (x))
#define SET_ZFLG(x) (ZFLG = (x))
#define SET_XFLG(x) (XFLG = (x))

#define GET_CFLG CFLG
#define GET_NFLG NFLG
#define GET_VFLG VFLG
#define GET_ZFLG ZFLG
#define GET_XFLG XFLG

#define CLEAR_CZNV do { \
 SET_CFLG (0); \
 SET_ZFLG (0); \
 SET_NFLG (0); \
 SET_VFLG (0); \
} while (0)

#define COPY_CARRY (SET_XFLG (GET_CFLG))

static __inline__ int cctrue(const int cc)
{
    switch(cc){
     case 0: return 1;                       /* T */
     case 1: return 0;                       /* F */
     case 2: return !CFLG && !ZFLG;          /* HI */
     case 3: return CFLG || ZFLG;            /* LS */
     case 4: return !CFLG;                   /* CC */
     case 5: return CFLG;                    /* CS */
     case 6: return !ZFLG;                   /* NE */
     case 7: return ZFLG;                    /* EQ */
     case 8: return !VFLG;                   /* VC */
     case 9: return VFLG;                    /* VS */
     case 10:return !NFLG;                   /* PL */
     case 11:return NFLG;                    /* MI */
     case 12:return NFLG == VFLG;            /* GE */
     case 13:return NFLG != VFLG;            /* LT */
     case 14:return !ZFLG && (NFLG == VFLG); /* GT */
     case 15:return ZFLG || (NFLG != VFLG);  /* LE */
    }
    return 0;
}

#endif /* OPTIMIZED_FLAGS */

#endif /* M68K_FLAGS_H */
