 /* 
  * UAE - The Un*x Amiga Emulator
  * 
  * MC68000 emulation - machine dependent bits
  *
  * Copyright 1996 Bernd Schmidt
  */

#ifdef __i386__

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
