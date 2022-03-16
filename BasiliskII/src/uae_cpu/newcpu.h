/*
 * UAE - The Un*x Amiga Emulator
 *
 * MC68000 emulation
 *
 * Copyright 1995 Bernd Schmidt
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

#ifndef NEWCPU_H
#define NEWCPU_H

#ifndef FLIGHT_RECORDER
#define FLIGHT_RECORDER 0
#endif

#include "m68k.h"
#include "readcpu.h"
#include "spcflags.h"

#if ENABLE_MON
#include "mon.h"
#include "mon_disass.h"
#endif


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
#define cpuop_end()			do { cpuop_tag("end"); } while (0)

typedef void REGPARAM2 cpuop_func (uae_u32) REGPARAM;
 
struct cputbl {
    cpuop_func *handler;
    uae_u16 specific;
    uae_u16 opcode;
};

extern cpuop_func *cpufunctbl[65536] ASM_SYM("cpufunctbl");

#if USE_JIT
typedef void compop_func (uae_u32) REGPARAM;

struct comptbl {
    compop_func *handler;
	uae_u32		specific;
	uae_u32		opcode;
};
#endif

extern void REGPARAM2 op_illg (uae_u32) REGPARAM;
extern void m68k_dumpstate(uaecptr *nextpc);

typedef char flagtype;

struct regstruct {
    uae_u32		regs[16];

    uae_u32		pc;
    uae_u8 *	pc_p;
    uae_u8 *	pc_oldp;

	spcflags_t	spcflags;
    int			intmask;

    uae_u32		vbr, sfc, dfc;
    uaecptr		usp, isp, msp;
    uae_u16		sr;
    flagtype	t1;
    flagtype	t0;
    flagtype	s;
    flagtype	m;
    flagtype	x;
    flagtype	stopped;

#if USE_PREFETCH_BUFFER
    /* Fellow sources say this is 4 longwords. That's impossible. It needs
     * to be at least a longword. The HRM has some cryptic comment about two
     * instructions being on the same longword boundary.
     * The way this is implemented now seems like a good compromise.
     */
    uae_u32 prefetch;
#endif
};

extern regstruct regs, lastint_regs;

#define m68k_dreg(r,num) ((r).regs[(num)])
#define m68k_areg(r,num) (((r).regs + 8)[(num)])

#define get_ibyte(o) do_get_mem_byte((uae_u8 *)(regs.pc_p + (o) + 1))
#define get_iword(o) do_get_mem_word((uae_u16 *)(regs.pc_p + (o)))
#define get_ilong(o) do_get_mem_long((uae_u32 *)(regs.pc_p + (o)))

#ifdef HAVE_GET_WORD_UNSWAPPED
#define GET_OPCODE (do_get_mem_word_unswapped (regs.pc_p))
#else
#define GET_OPCODE (get_iword (0))
#endif

#if USE_PREFETCH_BUFFER
static __inline__ uae_u32 get_ibyte_prefetch (uae_s32 o)
{
    if (o > 3 || o < 0)
	return do_get_mem_byte((uae_u8 *)(regs.pc_p + o + 1));

    return do_get_mem_byte((uae_u8 *)(((uae_u8 *)&regs.prefetch) + o + 1));
}
static __inline__ uae_u32 get_iword_prefetch (uae_s32 o)
{
    if (o > 3 || o < 0)
	return do_get_mem_word((uae_u16 *)(regs.pc_p + o));

    return do_get_mem_word((uae_u16 *)(((uae_u8 *)&regs.prefetch) + o));
}
static __inline__ uae_u32 get_ilong_prefetch (uae_s32 o)
{
    if (o > 3 || o < 0)
	return do_get_mem_long((uae_u32 *)(regs.pc_p + o));
    if (o == 0)
	return do_get_mem_long(&regs.prefetch);
    return (do_get_mem_word (((uae_u16 *)&regs.prefetch) + 1) << 16) | do_get_mem_word ((uae_u16 *)(regs.pc_p + 4));
}
#endif

static __inline__ void fill_prefetch_0 (void)
{
#if USE_PREFETCH_BUFFER
    uae_u32 r;
#ifdef UNALIGNED_PROFITABLE
    r = *(uae_u32 *)regs.pc_p;
    regs.prefetch = r;
#else
    r = do_get_mem_long ((uae_u32 *)regs.pc_p);
    do_put_mem_long (&regs.prefetch, r);
#endif
#endif
}

#if 0
static __inline__ void fill_prefetch_2 (void)
{
    uae_u32 r = do_get_mem_long (&regs.prefetch) << 16;
    uae_u32 r2 = do_get_mem_word (((uae_u16 *)regs.pc_p) + 1);
    r |= r2;
    do_put_mem_long (&regs.prefetch, r);
}
#else
#define fill_prefetch_2 fill_prefetch_0
#endif

static __inline__ uaecptr m68k_getpc (void)
{
#if REAL_ADDRESSING || DIRECT_ADDRESSING
	return get_virtual_address(regs.pc_p);
#else
	return regs.pc + ((char *)regs.pc_p - (char *)regs.pc_oldp);
#endif
}

static __inline__ void m68k_setpc (uaecptr newpc)
{
#if ENABLE_MON
	uae_u32 previous_pc = m68k_getpc();
#endif

#if REAL_ADDRESSING || DIRECT_ADDRESSING
	regs.pc_p = get_real_address(newpc);
#else
	regs.pc_p = regs.pc_oldp = get_real_address(newpc);
	regs.pc = newpc;
#endif

#if ENABLE_MON
	if (IS_BREAK_POINT(newpc)) {
		printf("Stopped at break point address: %08x. Last PC: %08x\n", newpc, previous_pc);
		m68k_dumpstate(NULL);
		const char *arg[4] = {"mon", "-m", "-r", NULL};
		mon(3, arg);
	}
#endif // end of #if ENABLE_MON
}

static __inline__ void m68k_incpc (uae_s32 delta)
{
#if ENABLE_MON
	uae_u32 previous_pc = m68k_getpc();
#endif
	regs.pc_p += (delta);
#if ENABLE_MON
	uaecptr next_pc = m68k_getpc();
	if (IS_BREAK_POINT(next_pc)) {
		printf("Stopped at break point address: %08x. Last PC: %08x\n", next_pc, previous_pc);
		m68k_dumpstate(NULL);
		const char *arg[4] = {"mon", "-m", "-r", NULL};
		mon(3, arg);
	}
#endif // end of #if ENABLE_MON
}

/* These are only used by the 68020/68881 code, and therefore don't
 * need to handle prefetch.  */
static __inline__ uae_u32 next_ibyte (void)
{
    uae_u32 r = get_ibyte (0);
    m68k_incpc (2);
    return r;
}

static __inline__ uae_u32 next_iword (void)
{
    uae_u32 r = get_iword (0);
    m68k_incpc (2);
    return r;
}

static __inline__ uae_u32 next_ilong (void)
{
    uae_u32 r = get_ilong (0);
    m68k_incpc (4);
    return r;
}

#define m68k_setpc_fast m68k_setpc
#define m68k_setpc_bcc  m68k_setpc
#define m68k_setpc_rte  m68k_setpc

static __inline__ void m68k_do_rts(void)
{
	    m68k_setpc(get_long(m68k_areg(regs, 7)));
	        m68k_areg(regs, 7) += 4;
}
 
static __inline__ void m68k_do_bsr(uaecptr oldpc, uae_s32 offset)
{
	    m68k_areg(regs, 7) -= 4;
	        put_long(m68k_areg(regs, 7), oldpc);
		    m68k_incpc(offset);
}
 
static __inline__ void m68k_do_jsr(uaecptr oldpc, uaecptr dest)
{
	    m68k_areg(regs, 7) -= 4;
	        put_long(m68k_areg(regs, 7), oldpc);
		    m68k_setpc(dest);
}

static __inline__ void m68k_setstopped (int stop)
{
    regs.stopped = stop;
    /* A traced STOP instruction drops through immediately without
       actually stopping.  */
    if (stop && (regs.spcflags & SPCFLAG_DOTRACE) == 0)
    SPCFLAGS_SET( SPCFLAG_STOP );
}

extern uae_u32 get_disp_ea_020 (uae_u32 base, uae_u32 dp);
extern uae_u32 get_disp_ea_000 (uae_u32 base, uae_u32 dp);

extern uae_s32 ShowEA (int reg, amodes mode, wordsizes size, char *buf);

extern void MakeSR (void);
extern void MakeFromSR (void);
extern void Exception (int, uaecptr);
extern void dump_counts (void);
extern int m68k_move2c (int, uae_u32 *);
extern int m68k_movec2 (int, uae_u32 *);
extern void m68k_divl (uae_u32, uae_u32, uae_u16, uaecptr);
extern void m68k_mull (uae_u32, uae_u32, uae_u16);
extern void m68k_emulop (uae_u32);
extern void m68k_emulop_return (void);
extern void init_m68k (void);
extern void exit_m68k (void);
extern void m68k_dumpstate (uaecptr *);
extern void m68k_disasm (uaecptr, uaecptr *, int);
extern void m68k_reset (void);
extern void m68k_enter_debugger(void);
extern int m68k_do_specialties(void);

extern void mmu_op (uae_u32, uae_u16);

/* Opcode of faulting instruction */
extern uae_u16 last_op_for_exception_3;
/* PC at fault time */
extern uaecptr last_addr_for_exception_3;
/* Address that generated the exception */
extern uaecptr last_fault_for_exception_3;

#define CPU_OP_NAME(a) op ## a

/* 68020 + 68881 */
extern struct cputbl op_smalltbl_0_ff[];
/* 68020 */
extern struct cputbl op_smalltbl_1_ff[];
/* 68010 */
extern struct cputbl op_smalltbl_2_ff[];
/* 68000 */
extern struct cputbl op_smalltbl_3_ff[];
/* 68000 slow but compatible.  */
extern struct cputbl op_smalltbl_4_ff[];

#if FLIGHT_RECORDER
extern void m68k_record_step(uaecptr) REGPARAM;
#endif
extern void m68k_do_execute(void);
extern void m68k_execute(void);
#if USE_JIT
extern void m68k_compile_execute(void);
#endif
extern void cpu_do_check_ticks(void);
#ifdef USE_CPU_EMUL_SERVICES
extern int32 emulated_ticks;

static inline void cpu_check_ticks(void)
{
	if (--emulated_ticks <= 0)
		cpu_do_check_ticks();
}
#else
extern uint16 emulated_ticks;
static inline void cpu_check_ticks(void)
{
	if (!++emulated_ticks)
		cpu_do_check_ticks();
}
#endif
 
#endif /* NEWCPU_H */
