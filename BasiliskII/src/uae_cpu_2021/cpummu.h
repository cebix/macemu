/*
 * cpummu.h - MMU emulation
 *
 * Copyright (c) 2001-2004 Milan Jurik of ARAnyM dev team (see AUTHORS)
 * 
 * Inspired by UAE MMU patch
 *
 * This file is part of the ARAnyM project which builds a new and powerful
 * TOS/FreeMiNT compatible virtual machine running on almost any hardware.
 *
 * ARAnyM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ARAnyM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ARAnyM; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef CPUMMU_H
#define CPUMMU_H

#include "registers.h"

# include <cstdlib>

#define MMU_TEST_PTEST					1
#define MMU_TEST_VERBOSE				2
#define MMU_TEST_FORCE_TABLE_SEARCH		4
#define MMU_TEST_NO_BUSERR				8

extern void mmu_dump_tables(void);

#define MMU_TTR_LOGICAL_BASE				0xff000000
#define MMU_TTR_LOGICAL_MASK				0x00ff0000
#define MMU_TTR_BIT_ENABLED					(1 << 15)
#define MMU_TTR_BIT_SFIELD_ENABLED			(1 << 14)
#define MMU_TTR_BIT_SFIELD_SUPER			(1 << 13)
#define MMU_TTR_SFIELD_SHIFT				13
#define MMU_TTR_UX_MASK						((1 << 9) | (1 << 8))
#define MMU_TTR_UX_SHIFT					8
#define MMU_TTR_CACHE_MASK					((1 << 6) | (1 << 5))
#define MMU_TTR_CACHE_SHIFT					5
#define MMU_TTR_BIT_WRITE_PROTECT			(1 << 2)

#define MMU_UDT_MASK	3
#define MMU_PDT_MASK	3

#define MMU_DES_WP			4
#define MMU_DES_USED		8

/* page descriptors only */
#define MMU_DES_MODIFIED	16
#define MMU_DES_SUPER		(1 << 7)
#define MMU_DES_GLOBAL		(1 << 10)

#define MMU_ROOT_PTR_ADDR_MASK			0xfffffe00
#define MMU_PTR_PAGE_ADDR_MASK_8		0xffffff80
#define MMU_PTR_PAGE_ADDR_MASK_4		0xffffff00

#define MMU_PAGE_INDIRECT_MASK			0xfffffffc
#define MMU_PAGE_ADDR_MASK_8			0xffffe000
#define MMU_PAGE_ADDR_MASK_4			0xfffff000
#define MMU_PAGE_UR_MASK_8				((1 << 12) | (1 << 11))
#define MMU_PAGE_UR_MASK_4				(1 << 11)
#define MMU_PAGE_UR_SHIFT				11

#define MMU_MMUSR_ADDR_MASK				0xfffff000
#define MMU_MMUSR_B						(1 << 11)
#define MMU_MMUSR_G						(1 << 10)
#define MMU_MMUSR_U1					(1 << 9)
#define MMU_MMUSR_U0					(1 << 8)
#define MMU_MMUSR_Ux					(MMU_MMUSR_U1 | MMU_MMUSR_U0)
#define MMU_MMUSR_S						(1 << 7)
#define MMU_MMUSR_CM					((1 << 6) | ( 1 << 5))
#define MMU_MMUSR_M						(1 << 4)
#define MMU_MMUSR_W						(1 << 2)
#define MMU_MMUSR_T						(1 << 1)
#define MMU_MMUSR_R						(1 << 0)

/* special status word (access error stack frame) */
#define MMU_SSW_TM		0x0007
#define MMU_SSW_TT		0x0018
#define MMU_SSW_SIZE	0x0060
#define  MMU_SSW_SIZE_B	0x0020
#define  MMU_SSW_SIZE_W	0x0040
#define  MMU_SSW_SIZE_L	0x0000
#define MMU_SSW_RW		0x0100
#define MMU_SSW_LK		0x0200
#define MMU_SSW_ATC		0x0400
#define MMU_SSW_MA		0x0800

#define TTR_I0	4
#define TTR_I1	5
#define TTR_D0	6
#define TTR_D1	7

#define TTR_NO_MATCH	0
#define TTR_NO_WRITE	1
#define TTR_OK_MATCH	2

struct mmu_atc_line {
	uae_u16 tag;
	unsigned tt : 1;
	unsigned valid_data : 1;
	unsigned valid_inst : 1;
	unsigned global : 1;
	unsigned modified : 1;
	unsigned write_protect : 1;
	unsigned hw : 1;
	unsigned bus_fault : 1;
	uaecptr phys;
};

/*
 * We don't need to store the whole logical address in the atc cache, as part of
 * it is encoded as index into the cache. 14 bits of the address are stored in
 * the tag, this means at least 6 bits must go into the index. The upper two
 * bits of the tag define the type of data in the atc line:
 * - 00: a normal memory address
 * - 11: invalid memory address or hardware access
 *       (generated via ~ATC_TAG(addr) in the slow path)
 * - 10: empty atc line
 */

#define ATC_TAG_SHIFT		18
#define ATC_TAG(addr)		((uae_u32)(addr) >> ATC_TAG_SHIFT)


#define ATC_L1_SIZE_LOG		8
#define ATC_L1_SIZE			(1 << ATC_L1_SIZE_LOG)

#define ATC_L1_INDEX(addr)	(((addr) >> 12) % ATC_L1_SIZE)

/*
 * first level atc cache
 * indexed by [super][data][rw][idx]
 */

typedef struct mmu_atc_line mmu_atc_l1_array[2][2][ATC_L1_SIZE];
extern mmu_atc_l1_array atc_l1[2];
extern mmu_atc_l1_array *current_atc;

#define ATC_L2_SIZE_LOG		12
#define ATC_L2_SIZE			(1 << ATC_L2_SIZE_LOG)

#define ATC_L2_INDEX(addr)	((((addr) >> 12) ^ ((addr) >> (32 - ATC_L2_SIZE_LOG))) % ATC_L2_SIZE)

extern struct mmu_atc_line atc_l2[2][ATC_L2_SIZE];

/*
 * lookup address in the level 1 atc cache,
 * the data and write arguments are constant in the common,
 * thus allows gcc to generate a constant offset.
 */
static ALWAYS_INLINE int mmu_lookup(uaecptr addr, bool data, bool write,
									  struct mmu_atc_line **cl)
{
	addr >>= 12;
	*cl = &(*current_atc)[data][write][addr % ATC_L1_SIZE];
	return (*cl)->tag == addr >> (ATC_TAG_SHIFT - 12);
}

/*
 * similiar to mmu_user_lookup, but for the use of the moves instruction
 */
static ALWAYS_INLINE int mmu_user_lookup(uaecptr addr, bool super, bool data,
										   bool write, struct mmu_atc_line **cl)
{
	addr >>= 12;
	*cl = &atc_l1[super][data][write][addr % ATC_L1_SIZE];
	return (*cl)->tag == addr >> (ATC_TAG_SHIFT - 12);
}

extern REGPARAM2 uae_u16 mmu_get_word_unaligned(uaecptr addr, int data);
extern REGPARAM2 uae_u32 mmu_get_long_unaligned(uaecptr addr, int data);

extern REGPARAM2 uae_u8 mmu_get_byte_slow(uaecptr addr, int super, int data,
										  int size, struct mmu_atc_line *cl);
extern REGPARAM2 uae_u16 mmu_get_word_slow(uaecptr addr, int super, int data,
										   int size, struct mmu_atc_line *cl);
extern REGPARAM2 uae_u32 mmu_get_long_slow(uaecptr addr, int super, int data,
										   int size, struct mmu_atc_line *cl);
extern REGPARAM2 uae_u64 mmu_get_quad_slow(uaecptr addr, int super, int data,
										   struct mmu_atc_line *cl);

extern REGPARAM2 void mmu_put_word_unaligned(uaecptr addr, uae_u16 val, int data);
extern REGPARAM2 void mmu_put_long_unaligned(uaecptr addr, uae_u32 val, int data);

extern REGPARAM2 void mmu_put_byte_slow(uaecptr addr, uae_u8 val, int super, int data,
										int size, struct mmu_atc_line *cl);
extern REGPARAM2 void mmu_put_word_slow(uaecptr addr, uae_u16 val, int super, int data,
										int size, struct mmu_atc_line *cl);
extern REGPARAM2 void mmu_put_long_slow(uaecptr addr, uae_u32 val, int super, int data,
										int size, struct mmu_atc_line *cl);
extern REGPARAM2 void mmu_put_quad_slow(uaecptr addr, uae_u64 val, int super, int data,
										struct mmu_atc_line *cl);

extern void mmu_make_transparent_region(uaecptr baseaddr, uae_u32 size, int datamode);

static inline void mmu_set_ttr(int regno, uae_u32 val)
{
	uae_u32 * ttr;
	switch(regno)	{
		case TTR_I0:	ttr = &regs.itt0;	break;
		case TTR_I1:	ttr = &regs.itt1;	break;
		case TTR_D0:	ttr = &regs.dtt0;	break;
		case TTR_D1:	ttr = &regs.dtt1;	break;
		default: abort();
	}
	*ttr = val;
}

static inline void mmu_set_mmusr(uae_u32 val)
{
	regs.mmusr = val;
}

#define FC_DATA		(regs.s ? 5 : 1)
#define FC_INST		(regs.s ? 6 : 2)

extern uaecptr REGPARAM2 mmu_translate(uaecptr addr, int super, int data, int write);

extern uae_u32 REGPARAM2 sfc_get_long(uaecptr addr);
extern uae_u16 REGPARAM2 sfc_get_word(uaecptr addr);
extern uae_u8 REGPARAM2 sfc_get_byte(uaecptr addr);
extern void REGPARAM2 dfc_put_long(uaecptr addr, uae_u32 val);
extern void REGPARAM2 dfc_put_word(uaecptr addr, uae_u16 val);
extern void REGPARAM2 dfc_put_byte(uaecptr addr, uae_u8 val);


extern void REGPARAM2 mmu_flush_atc(uaecptr addr, bool super, bool global);
extern void REGPARAM2 mmu_flush_atc_all(bool global);
extern void REGPARAM2 mmu_op(uae_u32 opcode, uae_u16 extra);

#ifdef FULLMMU

extern void REGPARAM2 mmu_reset(void);
extern void REGPARAM2 mmu_set_tc(uae_u16 tc);
extern void REGPARAM2 mmu_set_super(bool super);

#else

static inline void mmu_reset(void)
{
}

static inline void mmu_set_tc(uae_u16 /*tc*/)
{
}

static inline void mmu_set_super(bool /*super*/)
{
}

#endif

#endif /* CPUMMU_H */
/*
vim:ts=4:sw=4:
*/
