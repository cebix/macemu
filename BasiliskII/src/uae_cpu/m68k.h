 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * MC68000 emulation - machine dependent bits
  *
  * Copyright 1996 Bernd Schmidt
  */

#if defined(__i386__) && defined(X86_ASSEMBLY)

struct flag_struct {
    unsigned int cznv;
    unsigned int x;
};

#define SET_ZFLG(y) (regflags.cznv = (regflags.cznv & ~0x40) | (((y) & 1) << 6))
#define SET_CFLG(y) (regflags.cznv = (regflags.cznv & ~1) | ((y) & 1))
#define SET_VFLG(y) (regflags.cznv = (regflags.cznv & ~0x800) | (((y) & 1) << 11))
#define SET_NFLG(y) (regflags.cznv = (regflags.cznv & ~0x80) | (((y) & 1) << 7))
#define SET_XFLG(y) (regflags.x = (y))

#define GET_ZFLG ((regflags.cznv >> 6) & 1)
#define GET_CFLG (regflags.cznv & 1)
#define GET_VFLG ((regflags.cznv >> 11) & 1)
#define GET_NFLG ((regflags.cznv >> 7) & 1)
#define GET_XFLG (regflags.x & 1)

#define CLEAR_CZNV (regflags.cznv = 0)
#define COPY_CARRY (regflags.x = regflags.cznv)

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

#define x86_flag_testl(v) \
  __asm__ __volatile__ ("testl %1,%1\n\t" \
			"pushfl\n\t" \
			"popl %0\n\t" \
			: "=r" (regflags.cznv) : "r" (v) : "cc")

#define x86_flag_testw(v) \
  __asm__ __volatile__ ("testw %w1,%w1\n\t" \
			"pushfl\n\t" \
			"popl %0\n\t" \
			: "=r" (regflags.cznv) : "r" (v) : "cc")

#define x86_flag_testb(v) \
  __asm__ __volatile__ ("testb %b1,%b1\n\t" \
			"pushfl\n\t" \
			"popl %0\n\t" \
			: "=r" (regflags.cznv) : "q" (v) : "cc")

#define x86_flag_addl(v, s, d) do { \
  __asm__ __volatile__ ("addl %k2,%k1\n\t" \
			"pushfl\n\t" \
			"popl %0\n\t" \
			: "=r" (regflags.cznv), "=r" (v) : "rmi" (s), "1" (d) : "cc"); \
    COPY_CARRY; \
    } while (0)

#define x86_flag_addw(v, s, d) do { \
  __asm__ __volatile__ ("addw %w2,%w1\n\t" \
			"pushfl\n\t" \
			"popl %0\n\t" \
			: "=r" (regflags.cznv), "=r" (v) : "rmi" (s), "1" (d) : "cc"); \
    COPY_CARRY; \
    } while (0)

#define x86_flag_addb(v, s, d) do { \
  __asm__ __volatile__ ("addb %b2,%b1\n\t" \
			"pushfl\n\t" \
			"popl %0\n\t" \
			: "=r" (regflags.cznv), "=q" (v) : "qmi" (s), "1" (d) : "cc"); \
    COPY_CARRY; \
    } while (0)

#define x86_flag_subl(v, s, d) do { \
  __asm__ __volatile__ ("subl %k2,%k1\n\t" \
			"pushfl\n\t" \
			"popl %0\n\t" \
			: "=r" (regflags.cznv), "=r" (v) : "rmi" (s), "1" (d) : "cc"); \
    COPY_CARRY; \
    } while (0)

#define x86_flag_subw(v, s, d) do { \
  __asm__ __volatile__ ("subw %w2,%w1\n\t" \
			"pushfl\n\t" \
			"popl %0\n\t" \
			: "=r" (regflags.cznv), "=r" (v) : "rmi" (s), "1" (d) : "cc"); \
    COPY_CARRY; \
    } while (0)

#define x86_flag_subb(v, s, d) do { \
  __asm__ __volatile__ ("subb %b2,%b1\n\t" \
			"pushfl\n\t" \
			"popl %0\n\t" \
			: "=r" (regflags.cznv), "=q" (v) : "qmi" (s), "1" (d) : "cc"); \
    COPY_CARRY; \
    } while (0)

#define x86_flag_cmpl(s, d) \
  __asm__ __volatile__ ("cmpl %k1,%k2\n\t" \
			"pushfl\n\t" \
			"popl %0\n\t" \
			: "=r" (regflags.cznv) : "rmi" (s), "r" (d) : "cc")

#define x86_flag_cmpw(s, d) \
  __asm__ __volatile__ ("cmpw %w1,%w2\n\t" \
			"pushfl\n\t" \
			"popl %0\n\t" \
			: "=r" (regflags.cznv) : "rmi" (s), "r" (d) : "cc")

#define x86_flag_cmpb(s, d) \
  __asm__ __volatile__ ("cmpb %b1,%b2\n\t" \
			"pushfl\n\t" \
			"popl %0\n\t" \
			: "=r" (regflags.cznv) : "qmi" (s), "q" (d) : "cc")

#elif defined(__sparc__) && (defined(SPARC_V8_ASSEMBLY) || defined(SPARC_V9_ASSEMBLY))

struct flag_struct {
    unsigned char nzvc;
    unsigned char x;
};

extern struct flag_struct regflags;

#define SET_ZFLG(y) (regflags.nzvc = (regflags.nzvc & ~0x04) | (((y) & 1) << 2))
#define SET_CFLG(y) (regflags.nzvc = (regflags.nzvc & ~1) | ((y) & 1))
#define SET_VFLG(y) (regflags.nzvc = (regflags.nzvc & ~0x02) | (((y) & 1) << 1))
#define SET_NFLG(y) (regflags.nzvc = (regflags.nzvc & ~0x08) | (((y) & 1) << 3))
#define SET_XFLG(y) (regflags.x = (y))

#define GET_ZFLG ((regflags.nzvc >> 2) & 1)
#define GET_CFLG (regflags.nzvc & 1)
#define GET_VFLG ((regflags.nzvc >> 1) & 1)
#define GET_NFLG ((regflags.nzvc >> 3) & 1)
#define GET_XFLG (regflags.x & 1)

#define CLEAR_CZNV (regflags.nzvc = 0)
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

#endif
