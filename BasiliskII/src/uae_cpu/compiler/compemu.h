/*
 * compiler/compemu.h - Public interface and definitions
 *
 * Copyright (c) 2001-2004 Milan Jurik of ARAnyM dev team (see AUTHORS)
 *
 * Inspired by Christian Bauer's Basilisk II
 *
 * This file is part of the ARAnyM project which builds a new and powerful
 * TOS/FreeMiNT compatible virtual machine running on almost any hardware.
 *
 * JIT compiler m68k -> IA-32 and AMD64
 *
 * Original 68040 JIT compiler for UAE, copyright 2000-2002 Bernd Meyer
 * Adaptation for Basilisk II and improvements, copyright 2000-2004 Gwenole Beauchesne
 * Portions related to CPU detection come from linux/arch/i386/kernel/setup.c
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

#ifndef COMPEMU_H
#define COMPEMU_H

// #include "sysconfig.h"
#include "newcpu.h"

#ifdef UAE
#ifdef CPU_64_BIT
typedef uae_u64 uintptr;
#else
typedef uae_u32 uintptr;
#endif
/* FIXME: cpummu.cpp also checks for USE_JIT, possibly others */
#define USE_JIT
#endif

#ifdef USE_JIT

#ifdef JIT_DEBUG
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
	uae_u16* location;
#ifdef UAE
	uae_u8  specmem;
#endif
};

union cacheline {
	cpuop_func* handler;
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
#if defined(CPU_arm)
#define LONGEST_68K_INST 256 /* The number of bytes the longest possible
			       68k instruction takes */
#else
#define LONGEST_68K_INST 16 /* The number of bytes the longest possible
			       68k instruction takes */
#endif
#define MAX_CHECKSUM_LEN 2048 /* The maximum size we calculate checksums
				 for. Anything larger will be flushed
				 unconditionally even with SOFT_FLUSH */
#define MAX_HOLD_BI 3  /* One for the current block, and up to two
			  for jump targets */

#define INDIVIDUAL_INST 0
#ifdef WINUAE_ARANYM
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
#define FLAG_ALL  (FLAG_C | FLAG_Z | FLAG_N | FLAG_V | FLAG_X)
#define FLAG_ZNV  (FLAG_Z | FLAG_N | FLAG_V)

#define KILLTHERAT 1  /* Set to 1 to avoid some partial_rat_stalls */

#if defined(CPU_arm)
#define USE_DATA_BUFFER
#define N_REGS 13  /* really 16, but 13 to 15 are SP, LR, PC */
#else
#if defined(CPU_x86_64)
#define N_REGS 16 /* really only 15, but they are numbered 0-3,5-15 */
#else
#define N_REGS 8  /* really only 7, but they are numbered 0,1,2,3,5,6,7 */
#endif
#endif
#define N_FREGS 6 /* That leaves us two positions on the stack to play with */

/* Functions exposed to newcpu, or to what was moved from newcpu.c to
 * compemu_support.c */
#ifdef WINUAE_ARANYM
extern void compiler_init(void);
extern void compiler_exit(void);
extern bool compiler_use_jit(void);
#endif
extern void flush(int save_regs);
void flush_reg(int reg);
extern void set_target(uae_u8* t);
extern uae_u8* get_target(void);
#ifdef UAE
extern void build_comp(void);
#endif
extern void set_cache_state(int enabled);
extern int get_cache_state(void);
extern uae_u32 get_jitted_size(void);
#ifdef JIT
extern void (*flush_icache)(void);
#endif
extern void alloc_cache(void);
extern int check_for_cache_miss(void);

/* JIT FPU compilation */
struct jit_disable_opcodes {
	bool fbcc;
	bool fdbcc;
	bool fscc;
	bool ftrapcc;
	bool fsave;
	bool frestore;
	bool fmove;
	bool fmovem;
	bool fmovec;  /* for move control register */
	bool fmovecr; /* for move from constant rom */
	bool fint;
	bool fsinh;
	bool fintrz;
	bool fsqrt;
	bool flognp1;
	bool fetoxm1;
	bool ftanh;
	bool fatan;
	bool fasin;
	bool fatanh;
	bool fsin;
	bool ftan;
	bool fetox;
	bool ftwotox;
	bool ftentox;
	bool flogn;
	bool flog10;
	bool flog2;
	bool fabs;
	bool fcosh;
	bool fneg;
	bool facos;
	bool fcos;
	bool fgetexp;
	bool fgetman;
	bool fdiv;
	bool fmod;
	bool fadd;
	bool fmul;
	bool fsgldiv;
	bool frem;
	bool fscale;
	bool fsglmul;
	bool fsub;
	bool fsincos;
	bool fcmp;
	bool ftst;
};
extern struct jit_disable_opcodes jit_disable;


extern void comp_fpp_opp (uae_u32 opcode, uae_u16 extra);
extern void comp_fbcc_opp (uae_u32 opcode);
extern void comp_fscc_opp (uae_u32 opcode, uae_u16 extra);
void comp_fdbcc_opp (uae_u32 opcode, uae_u16 extra);
void comp_ftrapcc_opp (uae_u32 opcode, uaecptr oldpc);
void comp_fsave_opp (uae_u32 opcode);
void comp_frestore_opp (uae_u32 opcode);

extern uae_u32 needed_flags;
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

#define SP_REG 15
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
	uae_s8 virt[VREGS];
	uae_s8 nat[N_REGS];
} smallstate;

extern int touchcnt;

#define IMM  uae_s32
#define RR1  uae_u32
#define RR2  uae_u32
#define RR4  uae_u32
/*
  R1, R2, R4 collides with ARM registers defined in ucontext
#define R1  uae_u32
#define R2  uae_u32
#define R4  uae_u32
*/
#define W1  uae_u32
#define W2  uae_u32
#define W4  uae_u32
#define RW1 uae_u32
#define RW2 uae_u32
#define RW4 uae_u32
#define MEMR uae_u32
#define MEMW uae_u32
#define MEMRW    uae_u32
#define MEMPTR   uintptr
#define MEMPTRR  MEMPTR
#define MEMPTRW  MEMPTR
#define MEMPTRRW MEMPTR

#define FW   uae_u32
#define FR   uae_u32
#define FRW  uae_u32

#define MIDFUNC(nargs,func,args) void func args
#define COMPCALL(func) func

#define LOWFUNC(flags,mem,nargs,func,args) static inline void func args

/* What we expose to the outside */
#define DECLARE_MIDFUNC(func) extern void func

#if defined(CPU_arm)

#include "compemu_midfunc_arm.h"

#if defined(USE_JIT2)
#include "compemu_midfunc_arm2.h"
#endif
#endif

#if defined(CPU_i386) || defined(CPU_x86_64)
#include "compemu_midfunc_x86.h"
#endif

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
#define SYNC_PC_OFFSET 100
extern void sync_m68k_pc(void);
extern uae_u32 get_const(int r);
extern int  is_const(int r);
extern void register_branch(uae_u32 not_taken, uae_u32 taken, uae_u8 cond);
void compemu_make_sr(int sr, int tmp);
void compemu_enter_super(int sr);
void compemu_exc_make_frame(int format, int sr, int currpc, int nr, int tmp);
void compemu_bkpt(void);
extern bool disasm_this_inst;

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

#ifdef UAE
    uae_u8* nexthandler;
#endif
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

#ifdef JIT_DEBUG
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

static inline void flush_icache(void) { }

#endif /* !USE_JIT */

#ifdef UAE

typedef struct {
    uae_u8 type;
    uae_u8 reg;
    uae_u32 next;
} regacc;

#define JIT_EXCEPTION_HANDLER
// #define JIT_ALWAYS_DISTRUST

/* ARAnyM uses fpu_register name, used in scratch_t */
/* FIXME: check that no ARAnyM code assumes different floating point type */
typedef fptype fpu_register;

extern void compile_block(cpu_history* pc_hist, int blocklen, int totcyles);

#define MAXCYCLES (1000 * CYCLE_UNIT)
#define scaled_cycles(x) (currprefs.m68k_speed<0?(((x)/SCALE)?(((x)/SCALE<MAXCYCLES?((x)/SCALE):MAXCYCLES)):1):(x))

/* Flags for Bernie during development/debugging. Should go away eventually */
#define DISTRUST_CONSISTENT_MEM 0

typedef struct {
    uae_u8 use_flags;
    uae_u8 set_flags;
    uae_u8 is_jump;
    uae_u8 is_addx;
    uae_u8 is_const_jump;
} op_properties;

extern op_properties prop[65536];

STATIC_INLINE int end_block(uae_u16 opcode)
{
    return prop[opcode].is_jump ||
	(prop[opcode].is_const_jump && !currprefs.comp_constjump);
}

#ifdef _WIN32
LONG WINAPI EvalException(LPEXCEPTION_POINTERS info);
#if defined(_MSC_VER) && !defined(NO_WIN32_EXCEPTION_HANDLER)
#ifdef _WIN64
/* Structured exception handling is table based for Windows x86-64, so
 * Windows will not be able to find the exception handler. */
#else
#define USE_STRUCTURED_EXCEPTION_HANDLING
#endif
#endif
#endif

void jit_abort(const char *format,...);
#if SIZEOF_TCHAR != 1
void jit_abort(const TCHAR *format, ...);
#endif

#else

#ifdef WINUAE_ARANYM
#define jit_log(format, ...) D(bug(format, ##__VA_ARGS__))
#define jit_log2(format, ...) D2(bug(format, ##__VA_ARGS__))
void jit_abort(const char *format,...) __attribute__((format(printf, 1, 2))) __attribute__((__noreturn__));
#else
#define jit_abort(...) abort()
#define jit_log panicbug
#define jit_log2(...)
#endif

#endif /* UAE */

#ifdef CPU_64_BIT
static inline uae_u32 check_uae_p32(uintptr address, const char *file, int line)
{
	if (address > (uintptr_t) 0xffffffff) {
		jit_abort("JIT: 64-bit pointer (0x%llx) at %s:%d (fatal)",
			(unsigned long long)address, file, line);
	}
	return (uae_u32) address;
}
#define uae_p32(x) (check_uae_p32((uintptr)(x), __FILE__, __LINE__))
#else
#define uae_p32(x) ((uae_u32)(x))
#endif

#endif /* COMPEMU_H */
