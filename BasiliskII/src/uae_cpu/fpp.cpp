 /*
  * UAE - The Un*x Amiga Emulator
  *
  * MC68881 emulation
  *
  * Copyright 1996 Herman ten Brugge
  */

#include <math.h>
#include <stdio.h>

#include "sysdeps.h"

#include "memory.h"
#include "readcpu.h"
#include "newcpu.h"

#if 1

#define	DEBUG_FPP	0

/* single   : S  8*E 23*F */
/* double   : S 11*E 52*F */
/* extended : S 15*E 64*F */
/* E = 0 & F = 0 -> 0 */
/* E = MAX & F = 0 -> Infin */
/* E = MAX & F # 0 -> NotANumber */
/* E = biased by 127 (single) ,1023 (double) ,16383 (extended) */

static __inline__ double to_single (uae_u32 value)
{
    double frac;

    if ((value & 0x7fffffff) == 0)
	return (0.0);
    frac = (double) ((value & 0x7fffff) | 0x800000) / 8388608.0;
    if (value & 0x80000000)
	frac = -frac;
    return (ldexp (frac, ((value >> 23) & 0xff) - 127));
}

static __inline__ uae_u32 from_single (double src)
{
    int expon;
    uae_u32 tmp;
    double frac;

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
    return (tmp | (((expon + 127 - 1) & 0xff) << 23) |
	    (((int) (frac * 16777216.0)) & 0x7fffff));
}

static __inline__ double to_exten(uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3)
{
    double frac;

    if ((wrd1 & 0x7fff0000) == 0 && wrd2 == 0 && wrd3 == 0)
	return 0.0;
    frac = (double) wrd2 / 2147483648.0 +
	(double) wrd3 / 9223372036854775808.0;
    if (wrd1 & 0x80000000)
	frac = -frac;
    return ldexp (frac, ((wrd1 >> 16) & 0x7fff) - 16383);
}

static __inline__ void from_exten(double src, uae_u32 * wrd1, uae_u32 * wrd2, uae_u32 * wrd3)
{
    int expon;
    double frac;

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
}

static __inline__ double to_double(uae_u32 wrd1, uae_u32 wrd2)
{
    double frac;

    if ((wrd1 & 0x7fffffff) == 0 && wrd2 == 0)
	return 0.0;
    frac = (double) ((wrd1 & 0xfffff) | 0x100000) / 1048576.0 +
	(double) wrd2 / 4503599627370496.0;
    if (wrd1 & 0x80000000)
	frac = -frac;
    return ldexp (frac, ((wrd1 >> 20) & 0x7ff) - 1023);
}

static __inline__ void from_double(double src, uae_u32 * wrd1, uae_u32 * wrd2)
{
    int expon;
    int tmp;
    double frac;

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
}

static __inline__ double to_pack(uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3)
{
    double d;
    char *cp;
    char str[100];

    cp = str;
    if (wrd1 & 0x80000000)
	*cp++ = '-';
    *cp++ = (wrd1 & 0xf) + '0';
    *cp++ = '.';
    *cp++ = ((wrd2 >> 28) & 0xf) + '0';
    *cp++ = ((wrd2 >> 24) & 0xf) + '0';
    *cp++ = ((wrd2 >> 20) & 0xf) + '0';
    *cp++ = ((wrd2 >> 16) & 0xf) + '0';
    *cp++ = ((wrd2 >> 12) & 0xf) + '0';
    *cp++ = ((wrd2 >> 8) & 0xf) + '0';
    *cp++ = ((wrd2 >> 4) & 0xf) + '0';
    *cp++ = ((wrd2 >> 0) & 0xf) + '0';
    *cp++ = ((wrd3 >> 28) & 0xf) + '0';
    *cp++ = ((wrd3 >> 24) & 0xf) + '0';
    *cp++ = ((wrd3 >> 20) & 0xf) + '0';
    *cp++ = ((wrd3 >> 16) & 0xf) + '0';
    *cp++ = ((wrd3 >> 12) & 0xf) + '0';
    *cp++ = ((wrd3 >> 8) & 0xf) + '0';
    *cp++ = ((wrd3 >> 4) & 0xf) + '0';
    *cp++ = ((wrd3 >> 0) & 0xf) + '0';
    *cp++ = 'E';
    if (wrd1 & 0x40000000)
	*cp++ = '-';
    *cp++ = ((wrd1 >> 24) & 0xf) + '0';
    *cp++ = ((wrd1 >> 20) & 0xf) + '0';
    *cp++ = ((wrd1 >> 16) & 0xf) + '0';
    *cp = 0;
    sscanf(str, "%le", &d);
    return d;
}

static __inline__ void from_pack(double src, uae_u32 * wrd1, uae_u32 * wrd2, uae_u32 * wrd3)
{
    int i;
    int t;
    char *cp;
    char str[100];

    sprintf(str, "%.16e", src);
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
}

static __inline__ int get_fp_value (uae_u32 opcode, uae_u16 extra, double *src)
{
    uaecptr tmppc;
    uae_u16 tmp;
    int size;
    int mode;
    int reg;
    uae_u32 ad = 0;
    static int sz1[8] =
    {4, 4, 12, 12, 2, 8, 1, 0};
    static int sz2[8] =
    {4, 4, 12, 12, 2, 8, 2, 0};

    if ((extra & 0x4000) == 0) {
	*src = regs.fp[(extra >> 10) & 7];
	return 1;
    }
    mode = (opcode >> 3) & 7;
    reg = opcode & 7;
    size = (extra >> 10) & 7;
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
	    break;
	case 3:
	    tmppc = m68k_getpc ();
	    tmp = next_iword();
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
    static int sz1[8] =
    {4, 4, 12, 12, 2, 8, 1, 0};
    static int sz2[8] =
    {4, 4, 12, 12, 2, 8, 2, 0};

    if ((extra & 0x4000) == 0) {
	regs.fp[(extra >> 10) & 7] = value;
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
	    tmp = next_iword();
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
	    tmp = next_iword();
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
	return 0;
    case 0x01:
	return Z;
    case 0x02:
	return !(NotANumber || Z || N);
    case 0x03:
	return Z || !(NotANumber || N);
    case 0x04:
	return N && !(NotANumber || Z);
    case 0x05:
	return Z || (N && !NotANumber);
    case 0x06:
	return !(NotANumber || Z);
    case 0x07:
	return !NotANumber;
    case 0x08:
	return NotANumber;
    case 0x09:
	return NotANumber || Z;
    case 0x0a:
	return NotANumber || !(N || Z);
    case 0x0b:
	return NotANumber || Z || !N;
    case 0x0c:
	return NotANumber || (N && !Z);
    case 0x0d:
	return NotANumber || Z || N;
    case 0x0e:
	return !Z;
    case 0x0f:
	return 1;
    case 0x10:
	return 0;
    case 0x11:
	return Z;
    case 0x12:
	return !(NotANumber || Z || N);
    case 0x13:
	return Z || !(NotANumber || N);
    case 0x14:
	return N && !(NotANumber || Z);
    case 0x15:
	return Z || (N && !NotANumber);
    case 0x16:
	return !(NotANumber || Z);
    case 0x17:
	return !NotANumber;
    case 0x18:
	return NotANumber;
    case 0x19:
	return NotANumber || Z;
    case 0x1a:
	return NotANumber || !(N || Z);
    case 0x1b:
	return NotANumber || Z || !N;
    case 0x1c:
	return NotANumber || (Z && N);
    case 0x1d:
	return NotANumber || Z || N;
    case 0x1e:
	return !Z;
    case 0x1f:
	return 1;
    }
    return -1;
}

void fdbcc_opp(uae_u32 opcode, uae_u16 extra)
{
    uaecptr pc = (uae_u32) m68k_getpc ();
    uae_s32 disp = (uae_s32) (uae_s16) next_iword();
    int cc;

#if DEBUG_FPP
    printf("fdbcc_opp at %08lx\n", m68k_getpc ());
    fflush(stdout);
#endif
    cc = fpp_cond(opcode, extra & 0x3f);
    if (cc == -1) {
	m68k_setpc (pc - 4);
	op_illg (opcode);
    } else if (!cc) {
	int reg = opcode & 0x7;

	m68k_dreg (regs, reg) = ((m68k_dreg (regs, reg) & ~0xffff)
				| ((m68k_dreg (regs, reg) - 1) & 0xffff));
	if ((m68k_dreg (regs, reg) & 0xffff) == 0xffff)
	    m68k_setpc (pc + disp);
    }
}

void fscc_opp(uae_u32 opcode, uae_u16 extra)
{
    uae_u32 ad;
    int cc;

#if DEBUG_FPP
    printf("fscc_opp at %08lx\n", m68k_getpc ());
    fflush(stdout);
#endif
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

#if DEBUG_FPP
    printf("ftrapcc_opp at %08lx\n", m68k_getpc ());
    fflush(stdout);
#endif
    cc = fpp_cond(opcode, opcode & 0x3f);
    if (cc == -1) {
	m68k_setpc (oldpc);
	op_illg (opcode);
    }
    if (cc)
	Exception(7, oldpc - 2);
}

void fbcc_opp(uae_u32 opcode, uaecptr pc, uae_u32 extra)
{
    int cc;

#if DEBUG_FPP
    printf("fbcc_opp at %08lx\n", m68k_getpc ());
    fflush(stdout);
#endif
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

void fsave_opp(uae_u32 opcode)
{
    uae_u32 ad;
    int incr = (opcode & 0x38) == 0x20 ? -1 : 1;
    int i;

#if DEBUG_FPP
    printf("fsave_opp at %08lx\n", m68k_getpc ());
    fflush(stdout);
#endif
    if (get_fp_ad(opcode, &ad) == 0) {
	m68k_setpc (m68k_getpc () - 2);
	op_illg (opcode);
	return;
    }
    if (incr < 0) {
	ad -= 4;
	put_long (ad, 0x70000000);
	for (i = 0; i < 5; i++) {
	    ad -= 4;
	    put_long (ad, 0x00000000);
	}
	ad -= 4;
	put_long (ad, 0x1f180000);
    } else {
	put_long (ad, 0x1f180000);
	ad += 4;
	for (i = 0; i < 5; i++) {
	    put_long (ad, 0x00000000);
	    ad += 4;
	}
	put_long (ad, 0x70000000);
	ad += 4;
    }
    if ((opcode & 0x38) == 0x18)
	m68k_areg (regs, opcode & 7) = ad;
    if ((opcode & 0x38) == 0x20)
	m68k_areg (regs, opcode & 7) = ad;
}

void frestore_opp(uae_u32 opcode)
{
    uae_u32 ad;
    uae_u32 d;
    int incr = (opcode & 0x38) == 0x20 ? -1 : 1;

#if DEBUG_FPP
    printf("frestore_opp at %08lx\n", m68k_getpc ());
    fflush(stdout);
#endif
    if (get_fp_ad(opcode, &ad) == 0) {
	m68k_setpc (m68k_getpc () - 2);
	op_illg (opcode);
	return;
    }
    if (incr < 0) {
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
	ad += 4;
	if ((d & 0xff000000) != 0) {
	    if ((d & 0x00ff0000) == 0x00180000)
		ad += 6 * 4;
	    else if ((d & 0x00ff0000) == 0x00380000)
		ad += 14 * 4;
	    else if ((d & 0x00ff0000) == 0x00b40000)
		ad += 45 * 4;
	}
    }
    if ((opcode & 0x38) == 0x18)
	m68k_areg (regs, opcode & 7) = ad;
    if ((opcode & 0x38) == 0x20)
	m68k_areg (regs, opcode & 7) = ad;
}

void fpp_opp(uae_u32 opcode, uae_u16 extra)
{
    int reg;
    double src;

#if DEBUG_FPP
    printf("FPP %04lx %04x at %08lx\n", opcode & 0xffff, extra & 0xffff,
	   m68k_getpc () - 4);
    fflush(stdout);
#endif
    switch ((extra >> 13) & 0x7) {
    case 3:
	if (put_fp_value (regs.fp[(extra >> 7) & 7], opcode, extra) == 0) {
	    m68k_setpc (m68k_getpc () - 4);
	    op_illg (opcode);
	}
	return;
    case 4:
    case 5:
	if ((opcode & 0x38) == 0) {
	    if (extra & 0x2000) {
		if (extra & 0x1000)
		    m68k_dreg (regs, opcode & 7) = regs.fpcr;
		if (extra & 0x0800)
		    m68k_dreg (regs, opcode & 7) = regs.fpsr;
		if (extra & 0x0400)
		    m68k_dreg (regs, opcode & 7) = regs.fpiar;
	    } else {
		if (extra & 0x1000)
		    regs.fpcr = m68k_dreg (regs, opcode & 7);
		if (extra & 0x0800)
		    regs.fpsr = m68k_dreg (regs, opcode & 7);
		if (extra & 0x0400)
		    regs.fpiar = m68k_dreg (regs, opcode & 7);
	    }
	} else if ((opcode & 0x38) == 1) {
	    if (extra & 0x2000) {
		if (extra & 0x1000)
		    m68k_areg (regs, opcode & 7) = regs.fpcr;
		if (extra & 0x0800)
		    m68k_areg (regs, opcode & 7) = regs.fpsr;
		if (extra & 0x0400)
		    m68k_areg (regs, opcode & 7) = regs.fpiar;
	    } else {
		if (extra & 0x1000)
		    regs.fpcr = m68k_areg (regs, opcode & 7);
		if (extra & 0x0800)
		    regs.fpsr = m68k_areg (regs, opcode & 7);
		if (extra & 0x0400)
		    regs.fpiar = m68k_areg (regs, opcode & 7);
	    }
	} else if ((opcode & 0x3f) == 0x3c) {
	    if ((extra & 0x2000) == 0) {
		if (extra & 0x1000)
		    regs.fpcr = next_ilong();
		if (extra & 0x0800)
		    regs.fpsr = next_ilong();
		if (extra & 0x0400)
		    regs.fpiar = next_ilong();
	    }
	} else if (extra & 0x2000) {
	    /* FMOVEM FPP->memory */
	    uae_u32 ad;
	    int incr = 0;

	    if (get_fp_ad(opcode, &ad) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
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
		put_long (ad, regs.fpcr);
		ad += 4;
	    }
	    if (extra & 0x0800) {
		put_long (ad, regs.fpsr);
		ad += 4;
	    }
	    if (extra & 0x0400) {
		put_long (ad, regs.fpiar);
		ad += 4;
	    }
	    ad -= incr;
	    if ((opcode & 0x38) == 0x18)
		m68k_areg (regs, opcode & 7) = ad;
	    if ((opcode & 0x38) == 0x20)
		m68k_areg (regs, opcode & 7) = ad;
	} else {
	    /* FMOVEM memory->FPP */
	    uae_u32 ad;

	    if (get_fp_ad(opcode, &ad) == 0) {
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		return;
	    }
	    ad = (opcode & 0x38) == 0x20 ? ad - 12 : ad;
	    if (extra & 0x1000) {
		regs.fpcr = get_long (ad);
		ad += 4;
	    }
	    if (extra & 0x0800) {
		regs.fpsr = get_long (ad);
		ad += 4;
	    }
	    if (extra & 0x0400) {
		regs.fpiar = get_long (ad);
		ad += 4;
	    }
	    if ((opcode & 0x38) == 0x18)
		m68k_areg (regs, opcode & 7) = ad;
	    if ((opcode & 0x38) == 0x20)
		m68k_areg (regs, opcode & 7) = ad - 12;
	}
	return;
    case 6:
    case 7:
	{
	    uae_u32 ad, list = 0;
	    int incr = 0;
	    if (extra & 0x2000) {
		/* FMOVEM FPP->memory */
		if (get_fp_ad(opcode, &ad) == 0) {
		    m68k_setpc (m68k_getpc () - 4);
		    op_illg (opcode);
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
		if ((opcode & 0x38) == 0x18)
		    m68k_areg (regs, opcode & 7) = ad;
		if ((opcode & 0x38) == 0x20)
		    m68k_areg (regs, opcode & 7) = ad;
	    } else {
		/* FMOVEM memory->FPP */
		if (get_fp_ad(opcode, &ad) == 0) {
		    m68k_setpc (m68k_getpc () - 4);
		    op_illg (opcode);
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
		if ((opcode & 0x38) == 0x18)
		    m68k_areg (regs, opcode & 7) = ad;
		if ((opcode & 0x38) == 0x20)
		    m68k_areg (regs, opcode & 7) = ad;
	    }
	}
	return;
    case 0:
    case 2:
	reg = (extra >> 7) & 7;
	if ((extra & 0xfc00) == 0x5c00) {
	    switch (extra & 0x7f) {
	    case 0x00:
		regs.fp[reg] = 4.0 * atan(1.0);
		break;
	    case 0x0b:
		regs.fp[reg] = log10 (2.0);
		break;
	    case 0x0c:
		regs.fp[reg] = exp (1.0);
		break;
	    case 0x0d:
		regs.fp[reg] = log (exp (1.0)) / log (2.0);
		break;
	    case 0x0e:
		regs.fp[reg] = log (exp (1.0)) / log (10.0);
		break;
	    case 0x0f:
		regs.fp[reg] = 0.0;
		break;
	    case 0x30:
		regs.fp[reg] = log (2.0);
		break;
	    case 0x31:
		regs.fp[reg] = log (10.0);
		break;
	    case 0x32:
		regs.fp[reg] = 1.0e0;
		break;
	    case 0x33:
		regs.fp[reg] = 1.0e1;
		break;
	    case 0x34:
		regs.fp[reg] = 1.0e2;
		break;
	    case 0x35:
		regs.fp[reg] = 1.0e4;
		break;
	    case 0x36:
		regs.fp[reg] = 1.0e8;
		break;
	    case 0x37:
		regs.fp[reg] = 1.0e16;
		break;
	    case 0x38:
		regs.fp[reg] = 1.0e32;
		break;
	    case 0x39:
		regs.fp[reg] = 1.0e64;
		break;
	    case 0x3a:
		regs.fp[reg] = 1.0e128;
		break;
	    case 0x3b:
		regs.fp[reg] = 1.0e256;
		break;
#if 0
	    case 0x3c:
		regs.fp[reg] = 1.0e512;
		break;
	    case 0x3d:
		regs.fp[reg] = 1.0e1024;
		break;
	    case 0x3e:
		regs.fp[reg] = 1.0e2048;
		break;
	    case 0x3f:
		regs.fp[reg] = 1.0e4096;
		break;
#endif
	    default:
		m68k_setpc (m68k_getpc () - 4);
		op_illg (opcode);
		break;
	    }
	    return;
	}
	if (get_fp_value (opcode, extra, &src) == 0) {
	    m68k_setpc (m68k_getpc () - 4);
	    op_illg (opcode);
	    return;
	}
	switch (extra & 0x7f) {
	case 0x00:		/* FMOVE */
	    regs.fp[reg] = src;
	    break;
	case 0x01:		/* FINT */
	    regs.fp[reg] = (int) (src + 0.5);
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x02:		/* FSINH */
	    regs.fp[reg] = sinh (src);
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x03:		/* FINTRZ */
	    regs.fp[reg] = (int) src;
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x04:		/* FSQRT */
	    regs.fp[reg] = sqrt (src);
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x06:		/* FLOGNP1 */
	    regs.fp[reg] = log (src + 1.0);
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x08:		/* FETOXM1 */
	    regs.fp[reg] = exp (src) - 1.0;
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x09:		/* FTANH */
	    regs.fp[reg] = tanh (src);
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x0a:		/* FATAN */
	    regs.fp[reg] = atan (src);
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x0c:		/* FASIN */
	    regs.fp[reg] = asin (src);
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x0d:		/* FATANH */
#if 1				/* The BeBox doesn't have atanh, and it isn't in the HPUX libm either */
	    regs.fp[reg] = log ((1 + src) / (1 - src)) / 2;
#else
	    regs.fp[reg] = atanh (src);
#endif
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x0e:		/* FSIN */
	    regs.fp[reg] = sin (src);
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x0f:		/* FTAN */
	    regs.fp[reg] = tan (src);
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x10:		/* FETOX */
	    regs.fp[reg] = exp (src);
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x11:		/* FTWOTOX */
	    regs.fp[reg] = pow(2.0, src);
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x12:		/* FTENTOX */
	    regs.fp[reg] = pow(10.0, src);
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x14:		/* FLOGN */
	    regs.fp[reg] = log (src);
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x15:		/* FLOG10 */
	    regs.fp[reg] = log10 (src);
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x16:		/* FLOG2 */
	    regs.fp[reg] = log (src) / log (2.0);
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x18:		/* FABS */
	    regs.fp[reg] = src < 0 ? -src : src;
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x19:		/* FCOSH */
	    regs.fp[reg] = cosh(src);
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x1a:		/* FNEG */
	    regs.fp[reg] = -src;
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x1c:		/* FACOS */
	    regs.fp[reg] = acos(src);
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x1d:		/* FCOS */
	    regs.fp[reg] = cos(src);
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x1e:		/* FGETEXP */
	    {
		int expon;
		frexp (src, &expon);
		regs.fp[reg] = (double) (expon - 1);
		regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		    (regs.fp[reg] < 0 ? 0x8000000 : 0);
	    }
	    break;
	case 0x1f:		/* FGETMAN */
	    {
		int expon;
		regs.fp[reg] = frexp (src, &expon) * 2.0;
		regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		    (regs.fp[reg] < 0 ? 0x8000000 : 0);
	    }
	    break;
	case 0x20:		/* FDIV */
	    regs.fp[reg] /= src;
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x21:		/* FMOD */
	    regs.fp[reg] = regs.fp[reg] -
		(double) ((int) (regs.fp[reg] / src)) * src;
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x22:		/* FADD */
	    regs.fp[reg] += src;
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x23:		/* FMUL */
	    regs.fp[reg] *= src;
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x24:		/* FSGLDIV */
	    regs.fp[reg] /= src;
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x25:		/* FREM */
	    regs.fp[reg] = regs.fp[reg] -
		(double) ((int) (regs.fp[reg] / src + 0.5)) * src;
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x26:		/* FSCALE */
	    regs.fp[reg] *= exp (log (2.0) * src);
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x27:		/* FSGLMUL */
	    regs.fp[reg] *= src;
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x28:		/* FSUB */
	    regs.fp[reg] -= src;
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x30:		/* FSINCOS */
	case 0x31:
	case 0x32:
	case 0x33:
	case 0x34:
	case 0x35:
	case 0x36:
	case 0x37:
	    regs.fp[reg] = sin (src);
	    regs.fp[extra & 7] = cos(src);
	    regs.fpsr = (regs.fp[reg] == 0 ? 0x4000000 : 0) |
		(regs.fp[reg] < 0 ? 0x8000000 : 0);
	    break;
	case 0x38:		/* FCMP */
	    {
		double tmp = regs.fp[reg] - src;
		regs.fpsr = (tmp == 0 ? 0x4000000 : 0) |
		    (tmp < 0 ? 0x8000000 : 0);
	    }
	    break;
	case 0x3a:		/* FTST */
	    regs.fpsr = (src == 0 ? 0x4000000 : 0) |
		(src < 0 ? 0x8000000 : 0);
	    break;
	default:
	    m68k_setpc (m68k_getpc () - 4);
	    op_illg (opcode);
	    break;
	}
	return;
    }
    m68k_setpc (m68k_getpc () - 4);
    op_illg (opcode);
}

#endif
