/*
 *  compiler/compemu.h - Public interface and definitions
 *
 *  Original 68040 JIT compiler for UAE, copyright 2000-2002 Bernd Meyer
 *
 *  Adaptation for Basilisk II and improvements, copyright 2000-2005
 *    Gwenole Beauchesne
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
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
 */

#ifndef COMPEMU_H
#define COMPEMU_H

#include "newcpu.h"

#if USE_JIT

#if JIT_DEBUG
/* dump some information (m68k block, x86 block addresses) about the compiler state */
extern void compiler_dumpstate(void);
#endif

/* Now that we do block chaining, and also have linked lists on each tag,
   TAGMASK can be much smaller and still do its job. Saves several megs
   of memory! */
#define TAGMASK 0x0000ffff
#define TAGSIZE (TAGMASK+1)
#define MAXRUN 1024
#define cacheline(x) (((uintptr)x)&TAGMASK)

extern uae_u8* start_pc_p;
extern uae_u32 start_pc;

struct blockinfo_t;

struct cpu_history {
	uae_u16 * location;
};

union cacheline {
	cpuop_func * handler;
	blockinfo_t * bi;
};

/* Use new spill/reload strategy when calling external functions */
#define USE_OPTIMIZED_CALLS 0
#if USE_OPTIMIZED_CALLS
#error implementation in progress
#endif

/* (gb) When on, this option can save save up to 30% compilation time
 *  when many lazy flushes occur (e.g. apps in MacOS 8.x).
 */
#define USE_SEPARATE_BIA 1

/* Use chain of checksum_info_t to compute the block checksum */
#define USE_CHECKSUM_INFO 1

/* Use code inlining, aka follow-up of constant jumps */
#define USE_INLINING 1

/* Inlining requires the chained checksuming information */
#if USE_INLINING
#undef  USE_CHECKSUM_INFO
#define USE_CHECKSUM_INFO 1
#endif

/* Does flush_icache_range() only check for blocks falling in the requested range? */
#define LAZY_FLUSH_ICACHE_RANGE 0

#define USE_F_ALIAS 1
#define USE_OFFSET 1
#define COMP_DEBUG 1

#if COMP_DEBUG
#define Dif(x) if (x)
#else
#define Dif(x) if (0)
#endif

#define SCALE 2

#define BYTES_PER_INST 10240  /* paranoid ;-) */
#define LONGEST_68K_INST 16 /* The number of bytes the longest possible
			       68k instruction takes */
#define MAX_CHECKSUM_LEN 2048 /* The maximum size we calculate checksums
				 for. Anything larger will be flushed
				 unconditionally even with SOFT_FLUSH */
#define MAX_HOLD_BI 3  /* One for the current block, and up to two
			  for jump targets */

#define INDIVIDUAL_INST 0
#if 1
// gb-- my format from readcpu.cpp is not the same
#define FLAG_X    0x0010
#define FLAG_N    0x0008
#define FLAG_Z    0x0004
#define FLAG_V    0x0002
#define FLAG_C    0x0001
#else
#define FLAG_C    0x0010
#define FLAG_V    0x0008
#define FLAG_Z    0x0004
#define FLAG_N    0x0002
#define FLAG_X    0x0001
#endif
#define FLAG_CZNV (FLAG_C | FLAG_Z | FLAG_N | FLAG_V)
#define FLAG_ZNV  (FLAG_Z | FLAG_N | FLAG_V)

#define KILLTHERAT 1  /* Set to 1 to avoid some partial_rat_stalls */

/* Whether to preserve registers across calls to JIT compiled routines */
#if defined(X86_ASSEMBLY) || defined(X86_64_ASSEMBLY)
#define USE_PUSH_POP 0
#else
#define USE_PUSH_POP 1
#endif

#if defined(__x86_64__)
#define N_REGS 16 /* really only 15, but they are numbered 0-3,5-15 */
#else
#define N_REGS 8  /* really only 7, but they are numbered 0,1,2,3,5,6,7 */
#endif
#define N_FREGS 6 /* That leaves us two positions on the stack to play with */

/* Functions exposed to newcpu, or to what was moved from newcpu.c to
 * compemu_support.c */
extern void compiler_init(void);
extern void compiler_exit(void);
extern bool compiler_use_jit(void);
extern void init_comp(void);
extern void flush(int save_regs);
extern void small_flush(int save_regs);
extern void set_target(uae_u8* t);
extern uae_u8* get_target(void);
extern void freescratch(void);
extern void build_comp(void);
extern void set_cache_state(int enabled);
extern int get_cache_state(void);
extern uae_u32 get_jitted_size(void);
extern void (*flush_icache)(int n);
extern void alloc_cache(void);
extern int check_for_cache_miss(void);

/* JIT FPU compilation */
extern void comp_fpp_opp (uae_u32 opcode, uae_u16 extra);
extern void comp_fbcc_opp (uae_u32 opcode);
extern void comp_fscc_opp (uae_u32 opcode, uae_u16 extra);

extern uae_u32 needed_flags;
extern cacheline cache_tags[];
extern uae_u8* comp_pc_p;
extern void* pushall_call_handler;

#define VREGS 32
#define VFREGS 16

#define INMEM 1
#define CLEAN 2
#define DIRTY 3
#define UNDEF 4
#define ISCONST 5

typedef struct {
  uae_u32* mem;
  uae_u32 val;
  uae_u8 is_swapped;
  uae_u8 status;
  uae_s8 realreg; /* gb-- realreg can hold -1 */
  uae_u8 realind; /* The index in the holds[] array */
  uae_u8 needflush;
  uae_u8 validsize;
  uae_u8 dirtysize;
  uae_u8 dummy;
} reg_status;

typedef struct {
  uae_u32* mem;
  double val;
  uae_u8 status;
  uae_s8 realreg; /* gb-- realreg can hold -1 */
  uae_u8 realind;  
  uae_u8 needflush;
} freg_status;

#define PC_P 16
#define FLAGX 17
#define FLAGTMP 18
#define NEXT_HANDLER 19
#define S1 20
#define S2 21
#define S3 22
#define S4 23
#define S5 24
#define S6 25
#define S7 26
#define S8 27
#define S9 28
#define S10 29
#define S11 30
#define S12 31

#define FP_RESULT 8
#define FS1 9
#define FS2 10
#define FS3 11

typedef struct {
  uae_u32 touched;
  uae_s8 holds[VREGS];
  uae_u8 nholds;
  uae_u8 canbyte;
  uae_u8 canword;
  uae_u8 locked;
} n_status;

typedef struct {
  uae_u32 touched;
  uae_s8 holds[VFREGS];
  uae_u8 nholds;
  uae_u8 locked;
} fn_status;

/* For flag handling */
#define NADA 1
#define TRASH 2
#define VALID 3

/* needflush values */
#define NF_SCRATCH   0
#define NF_TOMEM     1
#define NF_HANDLER   2

typedef struct {
    /* Integer part */
    reg_status state[VREGS];
    n_status   nat[N_REGS];
    uae_u32 flags_on_stack;
    uae_u32 flags_in_flags;
    uae_u32 flags_are_important;
    /* FPU part */
    freg_status fate[VFREGS];
    fn_status   fat[N_FREGS];

    /* x86 FPU part */
    uae_s8 spos[N_FREGS];
    uae_s8 onstack[6];
    uae_s8 tos;
} bigstate;

typedef struct {
  /* Integer part */
  char virt[VREGS];
  char nat[N_REGS];
} smallstate;

extern bigstate live;
extern int touchcnt;


#define IMM uae_s32
#define R1  uae_u32
#define R2  uae_u32
#define R4  uae_u32
#define W1  uae_u32
#define W2  uae_u32
#define W4  uae_u32
#define RW1 uae_u32
#define RW2 uae_u32
#define RW4 uae_u32
#define MEMR uae_u32
#define MEMW uae_u32
#define MEMRW uae_u32

#define FW   uae_u32
#define FR   uae_u32
#define FRW  uae_u32

#define MIDFUNC(nargs,func,args) void func args
#define MENDFUNC(nargs,func,args) 
#define COMPCALL(func) func

#define LOWFUNC(flags,mem,nargs,func,args) static __inline__ void func args
#define LENDFUNC(flags,mem,nargs,func,args) 

/* What we expose to the outside */
#define DECLARE_MIDFUNC(func) extern void func
DECLARE_MIDFUNC(bt_l_ri(R4 r, IMM i));
DECLARE_MIDFUNC(bt_l_rr(R4 r, R4 b));
DECLARE_MIDFUNC(btc_l_ri(RW4 r, IMM i));
DECLARE_MIDFUNC(btc_l_rr(RW4 r, R4 b));
DECLARE_MIDFUNC(bts_l_ri(RW4 r, IMM i));
DECLARE_MIDFUNC(bts_l_rr(RW4 r, R4 b));
DECLARE_MIDFUNC(btr_l_ri(RW4 r, IMM i));
DECLARE_MIDFUNC(btr_l_rr(RW4 r, R4 b));
DECLARE_MIDFUNC(mov_l_rm(W4 d, IMM s));
DECLARE_MIDFUNC(call_r(R4 r));
DECLARE_MIDFUNC(sub_l_mi(IMM d, IMM s));
DECLARE_MIDFUNC(mov_l_mi(IMM d, IMM s));
DECLARE_MIDFUNC(mov_w_mi(IMM d, IMM s));
DECLARE_MIDFUNC(mov_b_mi(IMM d, IMM s));
DECLARE_MIDFUNC(rol_b_ri(RW1 r, IMM i));
DECLARE_MIDFUNC(rol_w_ri(RW2 r, IMM i));
DECLARE_MIDFUNC(rol_l_ri(RW4 r, IMM i));
DECLARE_MIDFUNC(rol_l_rr(RW4 d, R1 r));
DECLARE_MIDFUNC(rol_w_rr(RW2 d, R1 r));
DECLARE_MIDFUNC(rol_b_rr(RW1 d, R1 r));
DECLARE_MIDFUNC(shll_l_rr(RW4 d, R1 r));
DECLARE_MIDFUNC(shll_w_rr(RW2 d, R1 r));
DECLARE_MIDFUNC(shll_b_rr(RW1 d, R1 r));
DECLARE_MIDFUNC(ror_b_ri(R1 r, IMM i));
DECLARE_MIDFUNC(ror_w_ri(R2 r, IMM i));
DECLARE_MIDFUNC(ror_l_ri(R4 r, IMM i));
DECLARE_MIDFUNC(ror_l_rr(R4 d, R1 r));
DECLARE_MIDFUNC(ror_w_rr(R2 d, R1 r));
DECLARE_MIDFUNC(ror_b_rr(R1 d, R1 r));
DECLARE_MIDFUNC(shrl_l_rr(RW4 d, R1 r));
DECLARE_MIDFUNC(shrl_w_rr(RW2 d, R1 r));
DECLARE_MIDFUNC(shrl_b_rr(RW1 d, R1 r));
DECLARE_MIDFUNC(shra_l_rr(RW4 d, R1 r));
DECLARE_MIDFUNC(shra_w_rr(RW2 d, R1 r));
DECLARE_MIDFUNC(shra_b_rr(RW1 d, R1 r));
DECLARE_MIDFUNC(shll_l_ri(RW4 r, IMM i));
DECLARE_MIDFUNC(shll_w_ri(RW2 r, IMM i));
DECLARE_MIDFUNC(shll_b_ri(RW1 r, IMM i));
DECLARE_MIDFUNC(shrl_l_ri(RW4 r, IMM i));
DECLARE_MIDFUNC(shrl_w_ri(RW2 r, IMM i));
DECLARE_MIDFUNC(shrl_b_ri(RW1 r, IMM i));
DECLARE_MIDFUNC(shra_l_ri(RW4 r, IMM i));
DECLARE_MIDFUNC(shra_w_ri(RW2 r, IMM i));
DECLARE_MIDFUNC(shra_b_ri(RW1 r, IMM i));
DECLARE_MIDFUNC(setcc(W1 d, IMM cc));
DECLARE_MIDFUNC(setcc_m(IMM d, IMM cc));
DECLARE_MIDFUNC(cmov_l_rr(RW4 d, R4 s, IMM cc));
DECLARE_MIDFUNC(cmov_l_rm(RW4 d, IMM s, IMM cc));
DECLARE_MIDFUNC(bsf_l_rr(W4 d, R4 s));
DECLARE_MIDFUNC(pop_m(IMM d));
DECLARE_MIDFUNC(push_m(IMM d));
DECLARE_MIDFUNC(pop_l(W4 d));
DECLARE_MIDFUNC(push_l_i(IMM i));
DECLARE_MIDFUNC(push_l(R4 s));
DECLARE_MIDFUNC(clear_16(RW4 r));
DECLARE_MIDFUNC(clear_8(RW4 r));
DECLARE_MIDFUNC(sign_extend_16_rr(W4 d, R2 s));
DECLARE_MIDFUNC(sign_extend_8_rr(W4 d, R1 s));
DECLARE_MIDFUNC(zero_extend_16_rr(W4 d, R2 s));
DECLARE_MIDFUNC(zero_extend_8_rr(W4 d, R1 s));
DECLARE_MIDFUNC(imul_64_32(RW4 d, RW4 s));
DECLARE_MIDFUNC(mul_64_32(RW4 d, RW4 s));
DECLARE_MIDFUNC(imul_32_32(RW4 d, R4 s));
DECLARE_MIDFUNC(mul_32_32(RW4 d, R4 s));
DECLARE_MIDFUNC(mov_b_rr(W1 d, R1 s));
DECLARE_MIDFUNC(mov_w_rr(W2 d, R2 s));
DECLARE_MIDFUNC(mov_l_rrm_indexed(W4 d,R4 baser, R4 index, IMM factor));
DECLARE_MIDFUNC(mov_w_rrm_indexed(W2 d, R4 baser, R4 index, IMM factor));
DECLARE_MIDFUNC(mov_b_rrm_indexed(W1 d, R4 baser, R4 index, IMM factor));
DECLARE_MIDFUNC(mov_l_mrr_indexed(R4 baser, R4 index, IMM factor, R4 s));
DECLARE_MIDFUNC(mov_w_mrr_indexed(R4 baser, R4 index, IMM factor, R2 s));
DECLARE_MIDFUNC(mov_b_mrr_indexed(R4 baser, R4 index, IMM factor, R1 s));
DECLARE_MIDFUNC(mov_l_bmrr_indexed(IMM base, R4 baser, R4 index, IMM factor, R4 s));
DECLARE_MIDFUNC(mov_w_bmrr_indexed(IMM base, R4 baser, R4 index, IMM factor, R2 s));
DECLARE_MIDFUNC(mov_b_bmrr_indexed(IMM base, R4 baser, R4 index, IMM factor, R1 s));
DECLARE_MIDFUNC(mov_l_brrm_indexed(W4 d, IMM base, R4 baser, R4 index, IMM factor));
DECLARE_MIDFUNC(mov_w_brrm_indexed(W2 d, IMM base, R4 baser, R4 index, IMM factor));
DECLARE_MIDFUNC(mov_b_brrm_indexed(W1 d, IMM base, R4 baser, R4 index, IMM factor));
DECLARE_MIDFUNC(mov_l_rm_indexed(W4 d, IMM base, R4 index, IMM factor));
DECLARE_MIDFUNC(mov_l_rR(W4 d, R4 s, IMM offset));
DECLARE_MIDFUNC(mov_w_rR(W2 d, R4 s, IMM offset));
DECLARE_MIDFUNC(mov_b_rR(W1 d, R4 s, IMM offset));
DECLARE_MIDFUNC(mov_l_brR(W4 d, R4 s, IMM offset));
DECLARE_MIDFUNC(mov_w_brR(W2 d, R4 s, IMM offset));
DECLARE_MIDFUNC(mov_b_brR(W1 d, R4 s, IMM offset));
DECLARE_MIDFUNC(mov_l_Ri(R4 d, IMM i, IMM offset));
DECLARE_MIDFUNC(mov_w_Ri(R4 d, IMM i, IMM offset));
DECLARE_MIDFUNC(mov_b_Ri(R4 d, IMM i, IMM offset));
DECLARE_MIDFUNC(mov_l_Rr(R4 d, R4 s, IMM offset));
DECLARE_MIDFUNC(mov_w_Rr(R4 d, R2 s, IMM offset));
DECLARE_MIDFUNC(mov_b_Rr(R4 d, R1 s, IMM offset));
DECLARE_MIDFUNC(lea_l_brr(W4 d, R4 s, IMM offset));
DECLARE_MIDFUNC(lea_l_brr_indexed(W4 d, R4 s, R4 index, IMM factor, IMM offset));
DECLARE_MIDFUNC(lea_l_rr_indexed(W4 d, R4 s, R4 index, IMM factor));
DECLARE_MIDFUNC(mov_l_bRr(R4 d, R4 s, IMM offset));
DECLARE_MIDFUNC(mov_w_bRr(R4 d, R2 s, IMM offset));
DECLARE_MIDFUNC(mov_b_bRr(R4 d, R1 s, IMM offset));
DECLARE_MIDFUNC(bswap_32(RW4 r));
DECLARE_MIDFUNC(bswap_16(RW2 r));
DECLARE_MIDFUNC(mov_l_rr(W4 d, R4 s));
DECLARE_MIDFUNC(mov_l_mr(IMM d, R4 s));
DECLARE_MIDFUNC(mov_w_mr(IMM d, R2 s));
DECLARE_MIDFUNC(mov_w_rm(W2 d, IMM s));
DECLARE_MIDFUNC(mov_b_mr(IMM d, R1 s));
DECLARE_MIDFUNC(mov_b_rm(W1 d, IMM s));
DECLARE_MIDFUNC(mov_l_ri(W4 d, IMM s));
DECLARE_MIDFUNC(mov_w_ri(W2 d, IMM s));
DECLARE_MIDFUNC(mov_b_ri(W1 d, IMM s));
DECLARE_MIDFUNC(add_l_mi(IMM d, IMM s) );
DECLARE_MIDFUNC(add_w_mi(IMM d, IMM s) );
DECLARE_MIDFUNC(add_b_mi(IMM d, IMM s) );
DECLARE_MIDFUNC(test_l_ri(R4 d, IMM i));
DECLARE_MIDFUNC(test_l_rr(R4 d, R4 s));
DECLARE_MIDFUNC(test_w_rr(R2 d, R2 s));
DECLARE_MIDFUNC(test_b_rr(R1 d, R1 s));
DECLARE_MIDFUNC(and_l_ri(RW4 d, IMM i));
DECLARE_MIDFUNC(and_l(RW4 d, R4 s));
DECLARE_MIDFUNC(and_w(RW2 d, R2 s));
DECLARE_MIDFUNC(and_b(RW1 d, R1 s));
DECLARE_MIDFUNC(or_l_rm(RW4 d, IMM s));
DECLARE_MIDFUNC(or_l_ri(RW4 d, IMM i));
DECLARE_MIDFUNC(or_l(RW4 d, R4 s));
DECLARE_MIDFUNC(or_w(RW2 d, R2 s));
DECLARE_MIDFUNC(or_b(RW1 d, R1 s));
DECLARE_MIDFUNC(adc_l(RW4 d, R4 s));
DECLARE_MIDFUNC(adc_w(RW2 d, R2 s));
DECLARE_MIDFUNC(adc_b(RW1 d, R1 s));
DECLARE_MIDFUNC(add_l(RW4 d, R4 s));
DECLARE_MIDFUNC(add_w(RW2 d, R2 s));
DECLARE_MIDFUNC(add_b(RW1 d, R1 s));
DECLARE_MIDFUNC(sub_l_ri(RW4 d, IMM i));
DECLARE_MIDFUNC(sub_w_ri(RW2 d, IMM i));
DECLARE_MIDFUNC(sub_b_ri(RW1 d, IMM i));
DECLARE_MIDFUNC(add_l_ri(RW4 d, IMM i));
DECLARE_MIDFUNC(add_w_ri(RW2 d, IMM i));
DECLARE_MIDFUNC(add_b_ri(RW1 d, IMM i));
DECLARE_MIDFUNC(sbb_l(RW4 d, R4 s));
DECLARE_MIDFUNC(sbb_w(RW2 d, R2 s));
DECLARE_MIDFUNC(sbb_b(RW1 d, R1 s));
DECLARE_MIDFUNC(sub_l(RW4 d, R4 s));
DECLARE_MIDFUNC(sub_w(RW2 d, R2 s));
DECLARE_MIDFUNC(sub_b(RW1 d, R1 s));
DECLARE_MIDFUNC(cmp_l(R4 d, R4 s));
DECLARE_MIDFUNC(cmp_l_ri(R4 r, IMM i));
DECLARE_MIDFUNC(cmp_w(R2 d, R2 s));
DECLARE_MIDFUNC(cmp_b(R1 d, R1 s));
DECLARE_MIDFUNC(xor_l(RW4 d, R4 s));
DECLARE_MIDFUNC(xor_w(RW2 d, R2 s));
DECLARE_MIDFUNC(xor_b(RW1 d, R1 s));
DECLARE_MIDFUNC(live_flags(void));
DECLARE_MIDFUNC(dont_care_flags(void));
DECLARE_MIDFUNC(duplicate_carry(void));
DECLARE_MIDFUNC(restore_carry(void));
DECLARE_MIDFUNC(start_needflags(void));
DECLARE_MIDFUNC(end_needflags(void));
DECLARE_MIDFUNC(make_flags_live(void));
DECLARE_MIDFUNC(call_r_11(R4 r, W4 out1, R4 in1, IMM osize, IMM isize));
DECLARE_MIDFUNC(call_r_02(R4 r, R4 in1, R4 in2, IMM isize1, IMM isize2));
DECLARE_MIDFUNC(forget_about(W4 r));
DECLARE_MIDFUNC(nop(void));

DECLARE_MIDFUNC(f_forget_about(FW r));
DECLARE_MIDFUNC(fmov_pi(FW r));
DECLARE_MIDFUNC(fmov_log10_2(FW r));
DECLARE_MIDFUNC(fmov_log2_e(FW r));
DECLARE_MIDFUNC(fmov_loge_2(FW r));
DECLARE_MIDFUNC(fmov_1(FW r));
DECLARE_MIDFUNC(fmov_0(FW r));
DECLARE_MIDFUNC(fmov_rm(FW r, MEMR m));
DECLARE_MIDFUNC(fmovi_rm(FW r, MEMR m));
DECLARE_MIDFUNC(fmovi_mr(MEMW m, FR r));
DECLARE_MIDFUNC(fmovs_rm(FW r, MEMR m));
DECLARE_MIDFUNC(fmovs_mr(MEMW m, FR r));
DECLARE_MIDFUNC(fmov_mr(MEMW m, FR r));
DECLARE_MIDFUNC(fmov_ext_mr(MEMW m, FR r));
DECLARE_MIDFUNC(fmov_ext_rm(FW r, MEMR m));
DECLARE_MIDFUNC(fmov_rr(FW d, FR s));
DECLARE_MIDFUNC(fldcw_m_indexed(R4 index, IMM base));
DECLARE_MIDFUNC(ftst_r(FR r));
DECLARE_MIDFUNC(dont_care_fflags(void));
DECLARE_MIDFUNC(fsqrt_rr(FW d, FR s));
DECLARE_MIDFUNC(fabs_rr(FW d, FR s));
DECLARE_MIDFUNC(frndint_rr(FW d, FR s));
DECLARE_MIDFUNC(fsin_rr(FW d, FR s));
DECLARE_MIDFUNC(fcos_rr(FW d, FR s));
DECLARE_MIDFUNC(ftwotox_rr(FW d, FR s));
DECLARE_MIDFUNC(fetox_rr(FW d, FR s));
DECLARE_MIDFUNC(flog2_rr(FW d, FR s));
DECLARE_MIDFUNC(fneg_rr(FW d, FR s));
DECLARE_MIDFUNC(fadd_rr(FRW d, FR s));
DECLARE_MIDFUNC(fsub_rr(FRW d, FR s));
DECLARE_MIDFUNC(fmul_rr(FRW d, FR s));
DECLARE_MIDFUNC(frem_rr(FRW d, FR s));
DECLARE_MIDFUNC(frem1_rr(FRW d, FR s));
DECLARE_MIDFUNC(fdiv_rr(FRW d, FR s));
DECLARE_MIDFUNC(fcmp_rr(FR d, FR s));
DECLARE_MIDFUNC(fflags_into_flags(W2 tmp));
#undef DECLARE_MIDFUNC

extern int failure;
#define FAIL(x) do { failure|=x; } while (0)

/* Convenience functions exposed to gencomp */
extern uae_u32 m68k_pc_offset;
extern void readbyte(int address, int dest, int tmp);
extern void readword(int address, int dest, int tmp);
extern void readlong(int address, int dest, int tmp);
extern void writebyte(int address, int source, int tmp);
extern void writeword(int address, int source, int tmp);
extern void writelong(int address, int source, int tmp);
extern void writeword_clobber(int address, int source, int tmp);
extern void writelong_clobber(int address, int source, int tmp);
extern void get_n_addr(int address, int dest, int tmp);
extern void get_n_addr_jmp(int address, int dest, int tmp);
extern void calc_disp_ea_020(int base, uae_u32 dp, int target, int tmp);
/* Set native Z flag only if register is zero */
extern void set_zero(int r, int tmp);
extern int kill_rodent(int r);
extern void sync_m68k_pc(void);
extern uae_u32 get_const(int r);
extern int  is_const(int r);
extern void register_branch(uae_u32 not_taken, uae_u32 taken, uae_u8 cond);

#define comp_get_ibyte(o) do_get_mem_byte((uae_u8 *)(comp_pc_p + (o) + 1))
#define comp_get_iword(o) do_get_mem_word((uae_u16 *)(comp_pc_p + (o)))
#define comp_get_ilong(o) do_get_mem_long((uae_u32 *)(comp_pc_p + (o)))

struct blockinfo_t;

typedef struct dep_t {
  uae_u32*            jmp_off;
  struct blockinfo_t* target;
  struct blockinfo_t* source;
  struct dep_t**      prev_p;
  struct dep_t*       next;
} dependency;

typedef struct checksum_info_t {
  uae_u8 *start_p;
  uae_u32 length;
  struct checksum_info_t *next;
} checksum_info;

typedef struct blockinfo_t {
    uae_s32 count;
    cpuop_func* direct_handler_to_use;
    cpuop_func* handler_to_use;
    /* The direct handler does not check for the correct address */

    cpuop_func* handler; 
    cpuop_func* direct_handler;

    cpuop_func* direct_pen;
    cpuop_func* direct_pcc;

    uae_u8* pc_p;
    
    uae_u32 c1;     
    uae_u32 c2;
#if USE_CHECKSUM_INFO
    checksum_info *csi;
#else
    uae_u32 len;
    uae_u32 min_pcp; 
#endif

    struct blockinfo_t* next_same_cl;
    struct blockinfo_t** prev_same_cl_p;  
    struct blockinfo_t* next;
    struct blockinfo_t** prev_p; 

    uae_u8 optlevel;  
    uae_u8 needed_flags;  
    uae_u8 status;  
    uae_u8 havestate;
    
    dependency  dep[2];  /* Holds things we depend on */
    dependency* deplist; /* List of things that depend on this */
    smallstate  env;
	
#if JIT_DEBUG
	/* (gb) size of the compiled block (direct handler) */
	uae_u32 direct_handler_size;
#endif
} blockinfo;

#define BI_INVALID 0
#define BI_ACTIVE 1
#define BI_NEED_RECOMP 2
#define BI_NEED_CHECK 3
#define BI_CHECKING 4
#define BI_COMPILING 5
#define BI_FINALIZING 6

void execute_normal(void);
void exec_nostats(void);
void do_nothing(void);

#else

static __inline__ void flush_icache(int) { }
static __inline__ void build_comp() { }

#endif /* !USE_JIT */

#endif /* COMPEMU_H */
