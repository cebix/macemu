/*
 * newcpu.h - CPU emulation
 *
 * Copyright (c) 2009 ARAnyM dev team (see AUTHORS)
 * 
 * Inspired by Christian Bauer's Basilisk II
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
 /*
  * UAE - The Un*x Amiga Emulator
  *
  * MC68000 emulation
  *
  * Copyright 1995 Bernd Schmidt
  */

#ifndef NEWCPU_H
#define NEWCPU_H

#include "sysdeps.h"
#include "registers.h"
#include "spcflags.h"
#include "m68k.h"
#include "memory.h"

# include <csetjmp>

extern struct fixup {
    int flag;
    uae_u32 reg;
    uaecptr value;
}fixup;

extern int areg_byteinc[];
extern int imm8_table[];

extern int movem_index1[256];
extern int movem_index2[256];
extern int movem_next[256];

extern int broken_in;

#ifdef X86_ASSEMBLY
/* This hack seems to force all register saves (pushl %reg) to be moved to the
   begining of the function, thus making it possible to cpuopti to remove them
   since m68k_run_1 will save those registers before calling the instruction
   handler */
# define cpuop_tag(tag)		__asm__ __volatile__ ( "#cpuop_" tag )
#else
# define cpuop_tag(tag)		;
#endif

#define cpuop_begin()		do { cpuop_tag("begin"); } while (0)
#define cpuop_end()		do { cpuop_tag("end"); } while (0)

typedef void REGPARAM2 cpuop_func (uae_u32) REGPARAM;

struct cputbl {
    cpuop_func *handler;
    uae_u16 specific;
    uae_u16 opcode;
};

extern cpuop_func *cpufunctbl[65536];

#ifdef USE_JIT
typedef void compop_func (uae_u32) REGPARAM;

struct comptbl {
    compop_func *handler;
	uae_u32		opcode;
	uae_u32		specific;
#define COMP_OPCODE_ISJUMP      0x0001
#define COMP_OPCODE_LONG_OPCODE 0x0002
#define COMP_OPCODE_CMOV        0x0004
#define COMP_OPCODE_ISADDX      0x0008
#define COMP_OPCODE_ISCJUMP     0x0010
#define COMP_OPCODE_USES_FPU    0x0020
};
#endif

extern void REGPARAM2 op_illg (uae_u32) REGPARAM;

#define m68k_dreg(r,num) ((r).regs[(num)])
#define m68k_areg(r,num) (((r).regs + 8)[(num)])

#ifdef FULLMMU
static ALWAYS_INLINE uae_u8 get_ibyte(uae_u32 o)
{
	return mmu_get_byte(m68k_getpc() + o + 1, 0, sz_byte);
}
static ALWAYS_INLINE uae_u16 get_iword(uae_u32 o)
{
	return mmu_get_word(m68k_getpc() + o, 0, sz_word);
}
static ALWAYS_INLINE uae_u32 get_ilong(uae_u32 o)
{
	uaecptr addr = m68k_getpc() + o;

	if (unlikely(is_unaligned(addr, 4)))
		return mmu_get_long_unaligned(addr, 0);
	return mmu_get_long(addr, 0, sz_long);
}

#else
#define get_ibyte(o) do_get_mem_byte((uae_u8 *)(get_real_address(m68k_getpc(), 0, sz_byte) + (o) + 1))
#define get_iword(o) do_get_mem_word((uae_u16 *)(get_real_address(m68k_getpc(), 0, sz_word) + (o)))
#define get_ilong(o) do_get_mem_long((uae_u32 *)(get_real_address(m68k_getpc(), 0, sz_long) + (o)))
#endif

#if 0
static inline uae_u32 get_ibyte_prefetch (uae_s32 o)
{
    if (o > 3 || o < 0)
	return do_get_mem_byte((uae_u8 *)(do_get_real_address(regs.pcp, false, false) + o + 1));

    return do_get_mem_byte((uae_u8 *)(((uae_u8 *)&regs.prefetch) + o + 1));
}
static inline uae_u32 get_iword_prefetch (uae_s32 o)
{
    if (o > 3 || o < 0)
	return do_get_mem_word((uae_u16 *)(do_get_real_address(regs.pcp, false, false) + o));

    return do_get_mem_word((uae_u16 *)(((uae_u8 *)&regs.prefetch) + o));
}
static inline uae_u32 get_ilong_prefetch (uae_s32 o)
{
    if (o > 3 || o < 0)
	return do_get_mem_long((uae_u32 *)(do_get_real_address(regs.pcp, false, false) + o));
    if (o == 0)
	return do_get_mem_long(&regs.prefetch);
    return (do_get_mem_word (((uae_u16 *)&regs.prefetch) + 1) << 16) | do_get_mem_word ((uae_u16 *)(do_get_real_address(regs.pcp, false, false) + 4));
}
#endif

#ifdef FULLMMU
#define m68k_incpc(o) (regs.pc += (o))
#else
#define m68k_incpc(o) (regs.pc_p += (o))
#endif

static inline void fill_prefetch_0 (void)
{
#if USE_PREFETCH_BUFFER
    uae_u32 r;
#ifdef UNALIGNED_PROFITABLE
    r = *(uae_u32 *)do_get_real_address(m68k_getpc(), false, false);
    regs.prefetch = r;
#else
    r = do_get_mem_long ((uae_u32 *)do_get_real_address(m68k_getpc(), false, false));
    do_put_mem_long (&regs.prefetch, r);
#endif
#endif
}

#if 0
static inline void fill_prefetch_2 (void)
{
    uae_u32 r = do_get_mem_long (&regs.prefetch) << 16;
    uae_u32 r2 = do_get_mem_word (((uae_u16 *)do_get_real_address(regs.pcp, false, false)) + 1);
    r |= r2;
    do_put_mem_long (&regs.prefetch, r);
}
#else
#define fill_prefetch_2 fill_prefetch_0
#endif

/* These are only used by the 68020/68881 code, and therefore don't
 * need to handle prefetch.  */
static inline uae_u32 next_ibyte (void)
{
    uae_u32 r = get_ibyte (0);
    m68k_incpc (2);
    return r;
}

static inline uae_u32 next_iword (void)
{
    uae_u32 r = get_iword (0);
    m68k_incpc (2);
    return r;
}

static inline uae_u32 next_ilong (void)
{
    uae_u32 r = get_ilong (0);
    m68k_incpc (4);
    return r;
}

static inline void m68k_setpc (uaecptr newpc)
{
#ifndef FULLMMU
    regs.pc_p = regs.pc_oldp = get_real_address(newpc, 0, sz_word);
#endif
    regs.fault_pc = regs.pc = newpc;
}

#define m68k_setpc_fast m68k_setpc
#define m68k_setpc_bcc  m68k_setpc
#define m68k_setpc_rte  m68k_setpc

static inline void m68k_do_rts(void)
{
    m68k_setpc(get_long(m68k_areg(regs, 7)));
    m68k_areg(regs, 7) += 4;
}
 
static inline void m68k_do_bsr(uaecptr oldpc, uae_s32 offset)
{
    put_long(m68k_areg(regs, 7) - 4, oldpc);
    m68k_areg(regs, 7) -= 4;
    m68k_incpc(offset);
}
 
static inline void m68k_do_jsr(uaecptr oldpc, uaecptr dest)
{
    put_long(m68k_areg(regs, 7) - 4, oldpc);
    m68k_areg(regs, 7) -= 4;
    m68k_setpc(dest);
}

static inline void m68k_setstopped (int stop)
{
    regs.stopped = stop;
    /* A traced STOP instruction drops through immediately without
       actually stopping.  */
   if (stop && !( SPCFLAGS_TEST( SPCFLAG_DOTRACE )))
	SPCFLAGS_SET( SPCFLAG_STOP );
}

#ifdef FULLMMU
# define GET_OPCODE (get_iword (0))
#elif defined ARAM_PAGE_CHECK
# ifdef HAVE_GET_WORD_UNSWAPPED
#  define GET_OPCODE (do_get_mem_word_unswapped((uae_u16*)(pc + pc_offset)));
# else
#  define GET_OPCODE (do_get_mem_word((uae_u16*)(pc + pc_offset)));
# endif
#else
# ifdef HAVE_GET_WORD_UNSWAPPED
#  define GET_OPCODE (do_get_mem_word_unswapped ((uae_u16*)get_real_address(m68k_getpc(), 0, sz_word)))
# else
#  define GET_OPCODE (get_iword (0))
# endif
#endif

extern REGPARAM uae_u32 get_disp_ea_020 (uae_u32 base, uae_u32 dp);
extern REGPARAM uae_u32 get_disp_ea_000 (uae_u32 base, uae_u32 dp);
extern REGPARAM uae_u32 get_bitfield(uae_u32 src, uae_u32 bdata[2], uae_s32 offset, int width);
extern REGPARAM void put_bitfield(uae_u32 dst, uae_u32 bdata[2], uae_u32 val, uae_s32 offset, int width);



extern void MakeSR (void);
extern void MakeFromSR (void);
extern void Exception (int, uaecptr);
extern void ex_rte(void);
extern void dump_counts (void);
extern int m68k_move2c (int, uae_u32 *);
extern int m68k_movec2 (int, uae_u32 *);
extern void m68k_divl (uae_u32, uae_u32, uae_u16, uaecptr);
extern void m68k_mull (uae_u32, uae_u32, uae_u16);
extern void m68k_emulop (uae_u32);
extern void m68k_emulop_return (void);
extern void m68k_natfeat_id(void);
extern void m68k_natfeat_call(void);
extern void init_m68k (void);
extern void exit_m68k (void);
extern void m68k_dumpstate (FILE *, uaecptr *);
extern void m68k_disasm (FILE *, uaecptr, uaecptr *, int);
extern void newm68k_disasm(FILE *, uaecptr, uaecptr *, unsigned int);
extern void showDisasm(uaecptr);
extern void m68k_reset (void);
extern void m68k_enter_debugger(void);
extern int m68k_do_specialties(void);
extern void m68k_instr_set(void);
uae_u32 linea68000(uae_u16 opcode);

/* Opcode of faulting instruction */
extern uae_u16 last_op_for_exception_3;
/* PC at fault time */
extern uaecptr last_addr_for_exception_3;
/* Address that generated the exception */
extern uaecptr last_fault_for_exception_3;

#define CPU_OP_NAME(a) op ## a

/* 68040+ 68881 */
extern const struct cputbl op_smalltbl_0_ff[];
extern const struct cputbl op_smalltbl_0_nf[];

#ifdef FLIGHT_RECORDER
extern void m68k_record_step(uaecptr, int);
#endif

extern void m68k_do_execute(void);
extern void m68k_execute(void);
#ifdef USE_JIT
extern void m68k_compile_execute(void);
extern void m68k_do_compile_execute(void);
#endif
#ifdef USE_CPU_EMUL_SERVICES
extern int32 emulated_ticks;
extern void cpu_do_check_ticks(void);

static inline void cpu_check_ticks(void)
{
	if (--emulated_ticks <= 0)
		cpu_do_check_ticks();
}
#else
#define cpu_check_ticks()
#define cpu_do_check_ticks()
#endif

cpuop_func op_illg_1;

#endif /* NEWCPU_H */
