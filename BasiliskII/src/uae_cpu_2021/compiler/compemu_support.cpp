/*
 * compiler/compemu_support.cpp - Core dynamic translation engine
 *
 * Copyright (c) 2001-2009 Milan Jurik of ARAnyM dev team (see AUTHORS)
 * 
 * Inspired by Christian Bauer's Basilisk II
 *
 * This file is part of the ARAnyM project which builds a new and powerful
 * TOS/FreeMiNT compatible virtual machine running on almost any hardware.
 *
 * JIT compiler m68k -> IA-32 and AMD64 / ARM
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

#ifdef USE_JIT

#ifdef UAE

#define writemem_special writemem
#define readmem_special  readmem

#else
//#if !FIXED_ADDRESSING
//#error "Only Fixed Addressing is supported with the JIT Compiler"
//#endif

#if defined(X86_ASSEMBLY) && !SAHF_SETO_PROFITABLE
#error "Only [LS]AHF scheme to [gs]et flags is supported with the JIT Compiler"
#endif

/* NOTE: support for AMD64 assumes translation cache and other code
 * buffers are allocated into a 32-bit address space because (i) B2/JIT
 * code is not 64-bit clean and (ii) it's faster to resolve branches
 * that way.
 */
#if !defined(CPU_i386) && !defined(CPU_x86_64) && !defined(CPU_arm)
#error "Only IA-32, X86-64 and ARM v6 targets are supported with the JIT Compiler"
#endif
#endif

#define USE_MATCH 0

/* kludge for Brian, so he can compile under MSVC++ */
#define USE_NORMAL_CALLING_CONVENTION 0

// #include "sysconfig.h"
#include "sysdeps.h"

#ifdef JIT

#ifdef UAE
#include "options.h"
#include "events.h"
#include "uae/memory.h"
#include "custom.h"
#else
#include "cpu_emulation.h"
#include "main.h"
#include "prefs.h"
#include "vm_alloc.h"

#include "m68k.h"
#include "memory.h"
#include "readcpu.h"
#endif
#include "newcpu.h"
#include "comptbl.h"
#ifdef UAE
#include "compemu.h"
#ifdef FSUAE
#include "codegen_udis86.h"
#endif
#else
#include "compiler/compemu.h"
#include "fpu/fpu.h"
#include "fpu/flags.h"
// #include "parameters.h"
static void build_comp(void);
#endif
// #include "verify.h"

// #define jit_log(format, ...) \
// 	uae_log("JIT: " format "\n", ##__VA_ARGS__);
#define D2 D

#ifdef UAE
#ifdef FSUAE
#include "uae/fs.h"
#endif
#include "uae/log.h"

#if defined(__pie__) || defined (__PIE__)
#error Position-independent code (PIE) cannot be used with JIT
#endif

#include "uae/vm.h"
#define VM_PAGE_READ UAE_VM_READ
#define VM_PAGE_WRITE UAE_VM_WRITE
#define VM_PAGE_EXECUTE UAE_VM_EXECUTE
#define VM_MAP_FAILED UAE_VM_ALLOC_FAILED
#define VM_MAP_DEFAULT 1
#define VM_MAP_32BIT 1
#define vm_protect(address, size, protect) uae_vm_protect(address, size, protect)
#define vm_release(address, size) uae_vm_free(address, size)

static inline void *vm_acquire(size_t size, int options = VM_MAP_DEFAULT)
{
	assert(options == (VM_MAP_DEFAULT | VM_MAP_32BIT));
	return uae_vm_alloc(size, UAE_VM_32BIT, UAE_VM_READ_WRITE);
}

#define UNUSED(x)
#include "uae.h"
#include "uae/log.h"
#define jit_log(format, ...) \
	uae_log("JIT: " format "\n", ##__VA_ARGS__);
#define jit_log2(format, ...)

#define MEMBaseDiff uae_p32(NATMEM_OFFSET)

#ifdef NATMEM_OFFSET
#define FIXED_ADDRESSING 1
#endif

#define SAHF_SETO_PROFITABLE

// %%% BRIAN KING WAS HERE %%%
extern bool canbang;

#include "compemu_prefs.cpp"

#define uint32 uae_u32
#define uint8 uae_u8

static inline int distrust_check(int value)
{
#ifdef JIT_ALWAYS_DISTRUST
	return 1;
#else
	int distrust = value;
#ifdef FSUAE
	switch (value) {
	case 0: distrust = 0; break;
	case 1: distrust = 1; break;
	case 2: distrust = ((start_pc & 0xF80000) == 0xF80000); break;
	case 3: distrust = !have_done_picasso; break;
	default: abort();
	}
#endif
	return distrust;
#endif
}

static inline int distrust_byte(void)
{
	return distrust_check(currprefs.comptrustbyte);
}

static inline int distrust_word(void)
{
	return distrust_check(currprefs.comptrustword);
}

static inline int distrust_long(void)
{
	return distrust_check(currprefs.comptrustlong);
}

static inline int distrust_addr(void)
{
	return distrust_check(currprefs.comptrustnaddr);
}

#else
#define DEBUG 0
#include "debug.h"

#define NATMEM_OFFSET MEMBaseDiff
#define canbang 1
#define op_illg op_illg_1

#ifdef WINUAE_ARANYM
void jit_abort(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	abort();
}
#endif

#if DEBUG
#define PROFILE_COMPILE_TIME		1
#define PROFILE_UNTRANSLATED_INSNS	1
#endif
#endif

# include <csignal>
# include <cstdlib>
# include <cerrno>
# include <cassert>

#if defined(CPU_x86_64) && 0
#define RECORD_REGISTER_USAGE		1
#endif

#ifdef JIT_DEBUG
#undef abort
#define abort() do { \
	fprintf(stderr, "Abort in file %s at line %d\n", __FILE__, __LINE__); \
	compiler_dumpstate(); \
	exit(EXIT_FAILURE); \
} while (0)
#endif

#ifdef RECORD_REGISTER_USAGE
static uint64 reg_count[16];
static uint64 reg_count_local[16];

static int reg_count_compare(const void *ap, const void *bp)
{
    const int a = *((int *)ap);
    const int b = *((int *)bp);
    return reg_count[b] - reg_count[a];
}
#endif

#ifdef PROFILE_COMPILE_TIME
#include <time.h>
static uae_u32 compile_count	= 0;
static clock_t compile_time		= 0;
static clock_t emul_start_time	= 0;
static clock_t emul_end_time	= 0;
#endif

#ifdef PROFILE_UNTRANSLATED_INSNS
static const int untranslated_top_ten = 50;
static uae_u32 raw_cputbl_count[65536] = { 0, };
static uae_u16 opcode_nums[65536];


static int untranslated_compfn(const void *e1, const void *e2)
{
	return raw_cputbl_count[*(const uae_u16 *)e1] < raw_cputbl_count[*(const uae_u16 *)e2];
}
#endif

static compop_func *compfunctbl[65536];
static compop_func *nfcompfunctbl[65536];
#ifdef NOFLAGS_SUPPORT
static cpuop_func *nfcpufunctbl[65536];
#endif
uae_u8* comp_pc_p;

#ifdef UAE
/* defined in uae.h */
#else
// External variables
// newcpu.cpp
extern int quit_program;
#endif

// gb-- Extra data for Basilisk II/JIT
#ifdef JIT_DEBUG
static bool		JITDebug			= false;	// Enable runtime disassemblers through mon?
// #define JITDebug bx_options.jit.jitdebug		// Enable runtime disassemblers through mon?
#else
const bool		JITDebug			= false;
// #define JITDebug false			// Don't use JIT debug mode at all
#endif
#if USE_INLINING
#ifdef UAE
#define follow_const_jumps (currprefs.comp_constjump != 0)
#else
static bool follow_const_jumps = true; // Flag: translation through constant jumps
#endif
#else
const bool follow_const_jumps = false;
#endif

const uae_u32 MIN_CACHE_SIZE = 1024; // Minimal translation cache size (1 MB)
static uae_u32 cache_size = 0; // Size of total cache allocated for compiled blocks
static uae_u32		current_cache_size	= 0;		// Cache grows upwards: how much has been consumed already
static bool		lazy_flush		= true;	// Flag: lazy translation cache invalidation
// Flag: compile FPU instructions ?
#ifdef UAE
#ifdef USE_JIT_FPU
#define avoid_fpu (!currprefs.compfpu)
#else
#define avoid_fpu (true)
#endif
#else
static bool avoid_fpu = true; // Flag: compile FPU instructions ?
// #ifdef USE_JIT_FPU
// #define avoid_fpu (!bx_options.jit.jitfpu)
// #else
// #define avoid_fpu (true)
// #endif
#endif
static bool		have_cmov		= false;	// target has CMOV instructions ?
static bool		have_rat_stall		= true;	// target has partial register stalls ?
const bool		tune_alignment		= true;	// Tune code alignments for running CPU ?
const bool		tune_nop_fillers	= true;	// Tune no-op fillers for architecture
static bool		setzflg_uses_bsf	= false;	// setzflg virtual instruction can use native BSF instruction correctly?
static int		align_loops		= 32;	// Align the start of loops
static int		align_jumps		= 32;	// Align the start of jumps
static int		optcount[10]		= {
#ifdef UAE
	4,		// How often a block has to be executed before it is translated
#else
	10,		// How often a block has to be executed before it is translated
#endif
	0,		// How often to use naive translation
	0, 0, 0, 0,
	-1, -1, -1, -1
};

#ifdef UAE
/* FIXME: op_properties is currently in compemu.h */

op_properties prop[65536];

static inline bool is_const_jump(uae_u32 opcode)
{
	return prop[opcode].is_const_jump != 0;
}
#else
struct op_properties {
	uae_u8 use_flags;
	uae_u8 set_flags;
	uae_u8 is_addx;
	uae_u8 cflow;
};
static op_properties prop[65536];

static inline int end_block(uae_u32 opcode)
{
	return (prop[opcode].cflow & fl_end_block);
}

static inline bool is_const_jump(uae_u32 opcode)
{
	return (prop[opcode].cflow == fl_const_jump);
}

#if 0
static inline bool may_trap(uae_u32 opcode)
{
	return (prop[opcode].cflow & fl_trap);
}
#endif

#endif

static inline unsigned int cft_map (unsigned int f)
{
#ifdef UAE
	return f;
#else
#if !defined(HAVE_GET_WORD_UNSWAPPED) || defined(FULLMMU)
	return f;
#else
	return ((f >> 8) & 255) | ((f & 255) << 8);
#endif
#endif
}

uae_u8* start_pc_p;
uae_u32 start_pc;
uae_u32 current_block_pc_p;
static uintptr current_block_start_target;
uae_u32 needed_flags;
static uintptr next_pc_p;
static uintptr taken_pc_p;
static int     branch_cc;
static int redo_current_block;

#ifdef UAE
int segvcount=0;
#endif
static uae_u8* current_compile_p=NULL;
static uae_u8* max_compile_start;
static uae_u8* compiled_code=NULL;
static uae_s32 reg_alloc_run;
const int POPALLSPACE_SIZE = 2048; /* That should be enough space */
static uae_u8 *popallspace=NULL;

void* pushall_call_handler=NULL;
static void* popall_do_nothing=NULL;
static void* popall_exec_nostats=NULL;
static void* popall_execute_normal=NULL;
static void* popall_cache_miss=NULL;
static void* popall_recompile_block=NULL;
static void* popall_check_checksum=NULL;

/* The 68k only ever executes from even addresses. So right now, we
 * waste half the entries in this array
 * UPDATE: We now use those entries to store the start of the linked
 * lists that we maintain for each hash result.
 */
static cacheline cache_tags[TAGSIZE];
static int cache_enabled=0;
static blockinfo* hold_bi[MAX_HOLD_BI];
static blockinfo* active;
static blockinfo* dormant;

#ifdef NOFLAGS_SUPPORT
/* 68040 */
extern const struct cputbl op_smalltbl_0_nf[];
#endif
extern const struct comptbl op_smalltbl_0_comp_nf[];
extern const struct comptbl op_smalltbl_0_comp_ff[];

#ifdef NOFLAGS_SUPPORT
/* 68020 + 68881 */
extern const struct cputbl op_smalltbl_1_nf[];
/* 68020 */
extern const struct cputbl op_smalltbl_2_nf[];
/* 68010 */
extern const struct cputbl op_smalltbl_3_nf[];
/* 68000 */
extern const struct cputbl op_smalltbl_4_nf[];
/* 68000 slow but compatible.  */
extern const struct cputbl op_smalltbl_5_nf[];
#endif

static void flush_icache_hard(void);
static void flush_icache_lazy(void);
static void flush_icache_none(void);
void (*flush_icache)(void) = flush_icache_none;

static bigstate live;
static smallstate empty_ss;
static smallstate default_ss;
static int optlev;

static int writereg(int r, int size);
static void unlock2(int r);
static void setlock(int r);
static int readreg_specific(int r, int size, int spec);
static int writereg_specific(int r, int size, int spec);

static void inline write_jmp_target(uae_u32 *jmpaddr, cpuop_func* a);

uae_u32 m68k_pc_offset;

/* Some arithmetic operations can be optimized away if the operands
 * are known to be constant. But that's only a good idea when the
 * side effects they would have on the flags are not important. This
 * variable indicates whether we need the side effects or not
 */
static uae_u32 needflags=0;

/* Flag handling is complicated.
 *
 * x86 instructions create flags, which quite often are exactly what we
 * want. So at times, the "68k" flags are actually in the x86 flags.
 *
 * Then again, sometimes we do x86 instructions that clobber the x86
 * flags, but don't represent a corresponding m68k instruction. In that
 * case, we have to save them.
 *
 * We used to save them to the stack, but now store them back directly
 * into the regflags.cznv of the traditional emulation. Thus some odd
 * names.
 *
 * So flags can be in either of two places (used to be three; boy were
 * things complicated back then!); And either place can contain either
 * valid flags or invalid trash (and on the stack, there was also the
 * option of "nothing at all", now gone). A couple of variables keep
 * track of the respective states.
 *
 * To make things worse, we might or might not be interested in the flags.
 * by default, we are, but a call to dont_care_flags can change that
 * until the next call to live_flags. If we are not, pretty much whatever
 * is in the register and/or the native flags is seen as valid.
 */

static inline blockinfo* get_blockinfo(uae_u32 cl)
{
	return cache_tags[cl+1].bi;
}

static inline blockinfo* get_blockinfo_addr(void* addr)
{
	blockinfo*  bi=get_blockinfo(cacheline(addr));

	while (bi) {
		if (bi->pc_p==addr)
			return bi;
		bi=bi->next_same_cl;
	}
	return NULL;
}

#ifdef WINUAE_ARANYM
/*******************************************************************
 * Disassembler support                                            *
 *******************************************************************/

#define TARGET_M68K		0
#define TARGET_POWERPC	1
#define TARGET_X86		2
#define TARGET_X86_64	3
#define TARGET_ARM		4
#if defined(CPU_i386)
#define TARGET_NATIVE	TARGET_X86
#endif
#if defined(CPU_powerpc)
#define TARGET_NATIVE	TARGET_POWERPC
#endif
#if defined(CPU_x86_64)
#define TARGET_NATIVE	TARGET_X86_64
#endif
#if defined(CPU_arm)
#define TARGET_NATIVE	TARGET_ARM
#endif
// #include "disasm-glue.h"

bool disasm_this_inst;

#if defined(JIT_DEBUG) || (defined(HAVE_DISASM_NATIVE) && defined(HAVE_DISASM_M68K))
static void disasm_block(int disasm_target, const uint8 *start, size_t length)
{
	UNUSED(start);
	UNUSED(length);
	switch (disasm_target)
	{
	case TARGET_M68K:
#if defined(HAVE_DISASM_M68K)
		{
			char buf[256];

			disasm_info.memory_vma = ((memptr)((uintptr_t)(start) - MEMBaseDiff));
			while (length > 0)
			{
				int isize = m68k_disasm_to_buf(&disasm_info, buf, 1);
				bug("%s", buf);
				if (isize < 0)
					break;
				if ((uintptr)isize > length)
					break;
				length -= isize;
			}
		}
#endif
		break;
	case TARGET_X86:
	case TARGET_X86_64:
#if defined(HAVE_DISASM_X86)
		{
			const uint8 *end = start + length;
			char buf[256];

			while (start < end)
			{
				start = x86_disasm(start, buf, 1);
				bug("%s", buf);
			}
		}
#endif
		break;
	case TARGET_ARM:
#if defined(HAVE_DISASM_ARM)
		{
			const uint8 *end = start + length;
			char buf[256];

			while (start < end)
			{
				start = arm_disasm(start, buf, 1);
				bug("%s", buf);
			}
		}
#endif
		break;
	}
}

static inline void disasm_native_block(const uint8 *start, size_t length)
{
	disasm_block(TARGET_NATIVE, start, length);
}

static inline void disasm_m68k_block(const uint8 *start, size_t length)
{
	disasm_block(TARGET_M68K, start, length);
}
#endif
#endif /* WINUAE_ARANYM */


/*******************************************************************
 * All sorts of list related functions for all of the lists        *
 *******************************************************************/

static inline void remove_from_cl_list(blockinfo* bi)
{
	uae_u32 cl=cacheline(bi->pc_p);

	if (bi->prev_same_cl_p)
		*(bi->prev_same_cl_p)=bi->next_same_cl;
	if (bi->next_same_cl)
		bi->next_same_cl->prev_same_cl_p=bi->prev_same_cl_p;
	if (cache_tags[cl+1].bi)
		cache_tags[cl].handler=cache_tags[cl+1].bi->handler_to_use;
	else
		cache_tags[cl].handler=(cpuop_func*)popall_execute_normal;
}

static inline void remove_from_list(blockinfo* bi)
{
	if (bi->prev_p)
		*(bi->prev_p)=bi->next;
	if (bi->next)
		bi->next->prev_p=bi->prev_p;
}

#if 0
static inline void remove_from_lists(blockinfo* bi)
{
	remove_from_list(bi);
	remove_from_cl_list(bi);
}
#endif

static inline void add_to_cl_list(blockinfo* bi)
{
	uae_u32 cl=cacheline(bi->pc_p);

	if (cache_tags[cl+1].bi)
		cache_tags[cl+1].bi->prev_same_cl_p=&(bi->next_same_cl);
	bi->next_same_cl=cache_tags[cl+1].bi;

	cache_tags[cl+1].bi=bi;
	bi->prev_same_cl_p=&(cache_tags[cl+1].bi);

	cache_tags[cl].handler=bi->handler_to_use;
}

static inline void raise_in_cl_list(blockinfo* bi)
{
	remove_from_cl_list(bi);
	add_to_cl_list(bi);
}

static inline void add_to_active(blockinfo* bi)
{
	if (active)
		active->prev_p=&(bi->next);
	bi->next=active;

	active=bi;
	bi->prev_p=&active;
}

static inline void add_to_dormant(blockinfo* bi)
{
	if (dormant)
		dormant->prev_p=&(bi->next);
	bi->next=dormant;

	dormant=bi;
	bi->prev_p=&dormant;
}

static inline void remove_dep(dependency* d)
{
	if (d->prev_p)
		*(d->prev_p)=d->next;
	if (d->next)
		d->next->prev_p=d->prev_p;
	d->prev_p=NULL;
	d->next=NULL;
}

/* This block's code is about to be thrown away, so it no longer
   depends on anything else */
static inline void remove_deps(blockinfo* bi)
{
	remove_dep(&(bi->dep[0]));
	remove_dep(&(bi->dep[1]));
}

static inline void adjust_jmpdep(dependency* d, cpuop_func* a)
{
	write_jmp_target(d->jmp_off, a);
}

/********************************************************************
 * Soft flush handling support functions                            *
 ********************************************************************/

static inline void set_dhtu(blockinfo* bi, cpuop_func *dh)
{
	jit_log2("bi is %p",bi);
	if (dh!=bi->direct_handler_to_use) {
		dependency* x=bi->deplist;
		jit_log2("bi->deplist=%p",bi->deplist);
		while (x) {
			jit_log2("x is %p",x);
			jit_log2("x->next is %p",x->next);
			jit_log2("x->prev_p is %p",x->prev_p);

			if (x->jmp_off) {
				adjust_jmpdep(x,dh);
			}
			x=x->next;
		}
		bi->direct_handler_to_use=dh;
	}
}

static inline void invalidate_block(blockinfo* bi)
{
	int i;

	bi->optlevel=0;
	bi->count=optcount[0]-1;
	bi->handler=NULL;
	bi->handler_to_use=(cpuop_func*)popall_execute_normal;
	bi->direct_handler=NULL;
	set_dhtu(bi,bi->direct_pen);
	bi->needed_flags=0xff;
	bi->status=BI_INVALID;
	for (i=0;i<2;i++) {
		bi->dep[i].jmp_off=NULL;
		bi->dep[i].target=NULL;
	}
	remove_deps(bi);
}

static inline void create_jmpdep(blockinfo* bi, int i, uae_u32* jmpaddr, uae_u32 target)
{
	blockinfo*  tbi=get_blockinfo_addr((void*)(uintptr)target);

	Dif(!tbi) {
		jit_abort("Could not create jmpdep!");
	}
	bi->dep[i].jmp_off=jmpaddr;
	bi->dep[i].source=bi;
	bi->dep[i].target=tbi;
	bi->dep[i].next=tbi->deplist;
	if (bi->dep[i].next)
		bi->dep[i].next->prev_p=&(bi->dep[i].next);
	bi->dep[i].prev_p=&(tbi->deplist);
	tbi->deplist=&(bi->dep[i]);
}

static inline void block_need_recompile(blockinfo * bi)
{
	uae_u32 cl = cacheline(bi->pc_p);

	set_dhtu(bi, bi->direct_pen);
	bi->direct_handler = bi->direct_pen;

	bi->handler_to_use = (cpuop_func *)popall_execute_normal;
	bi->handler = (cpuop_func *)popall_execute_normal;
	if (bi == cache_tags[cl + 1].bi)
		cache_tags[cl].handler = (cpuop_func *)popall_execute_normal;
	bi->status = BI_NEED_RECOMP;
}

#if USE_MATCH
static inline void mark_callers_recompile(blockinfo * bi)
{
  dependency *x = bi->deplist;

  while (x) {
	dependency *next = x->next;	/* This disappears when we mark for
								 * recompilation and thus remove the
								 * blocks from the lists */
	if (x->jmp_off) {
	  blockinfo *cbi = x->source;

	  Dif(cbi->status == BI_INVALID) {
		jit_log("invalid block in dependency list"); // FIXME?
		// abort();
	  }
	  if (cbi->status == BI_ACTIVE || cbi->status == BI_NEED_CHECK) {
		block_need_recompile(cbi);
		mark_callers_recompile(cbi);
	  }
	  else if (cbi->status == BI_COMPILING) {
		redo_current_block = 1;
	  }
	  else if (cbi->status == BI_NEED_RECOMP) {
		/* nothing */
	  }
	  else {
		jit_log2("Status %d in mark_callers",cbi->status); // FIXME?
	  }
	}
	x = next;
  }
}
#endif

static inline blockinfo* get_blockinfo_addr_new(void* addr, int /* setstate */)
{
	blockinfo*  bi=get_blockinfo_addr(addr);
	int i;

	if (!bi) {
		for (i=0;i<MAX_HOLD_BI && !bi;i++) {
			if (hold_bi[i]) {
				(void)cacheline(addr);

				bi=hold_bi[i];
				hold_bi[i]=NULL;
				bi->pc_p=(uae_u8*)addr;
				invalidate_block(bi);
				add_to_active(bi);
				add_to_cl_list(bi);

			}
		}
	}
	if (!bi) {
		jit_abort("Looking for blockinfo, can't find free one");
	}
	return bi;
}

static void prepare_block(blockinfo* bi);

/* Managment of blockinfos.

   A blockinfo struct is allocated whenever a new block has to be
   compiled. If the list of free blockinfos is empty, we allocate a new
   pool of blockinfos and link the newly created blockinfos altogether
   into the list of free blockinfos. Otherwise, we simply pop a structure
   of the free list.

   Blockinfo are lazily deallocated, i.e. chained altogether in the
   list of free blockinfos whenvever a translation cache flush (hard or
   soft) request occurs.
*/

template< class T >
class LazyBlockAllocator
{
	enum {
		kPoolSize = 1 + (16384 - sizeof(T) - sizeof(void *)) / sizeof(T)
	};
	struct Pool {
		T chunk[kPoolSize];
		Pool * next;
	};
	Pool * mPools;
	T * mChunks;
public:
	LazyBlockAllocator() : mPools(0), mChunks(0) { }
#ifdef UAE
#else
	~LazyBlockAllocator();
#endif
	T * acquire();
	void release(T * const);
};

#ifdef UAE
/* uae_vm_release may do logging, which isn't safe to do when the application
 * is shutting down. Better to release memory manually with a function call
 * to a release_all method on shutdown, or even simpler, just let the OS
 * handle it (we're shutting down anyway). */
#else
template< class T >
LazyBlockAllocator<T>::~LazyBlockAllocator()
{
	Pool * currentPool = mPools;
	while (currentPool) {
		Pool * deadPool = currentPool;
		currentPool = currentPool->next;
		vm_release(deadPool, sizeof(Pool));
	}
}
#endif

template< class T >
T * LazyBlockAllocator<T>::acquire()
{
	if (!mChunks) {
		// There is no chunk left, allocate a new pool and link the
		// chunks into the free list
		Pool * newPool = (Pool *)vm_acquire(sizeof(Pool), VM_MAP_DEFAULT | VM_MAP_32BIT);
		if (newPool == VM_MAP_FAILED) {
			jit_abort("Could not allocate block pool!");
		}
		for (T * chunk = &newPool->chunk[0]; chunk < &newPool->chunk[kPoolSize]; chunk++) {
			chunk->next = mChunks;
			mChunks = chunk;
		}
		newPool->next = mPools;
		mPools = newPool;
	}
	T * chunk = mChunks;
	mChunks = chunk->next;
	return chunk;
}

template< class T >
void LazyBlockAllocator<T>::release(T * const chunk)
{
	chunk->next = mChunks;
	mChunks = chunk;
}

template< class T >
class HardBlockAllocator
{
public:
	T * acquire() {
		T * data = (T *)current_compile_p;
		current_compile_p += sizeof(T);
		return data;
	}

	void release(T * const ) {
		// Deallocated on invalidation
	}
};

#if USE_SEPARATE_BIA
static LazyBlockAllocator<blockinfo> BlockInfoAllocator;
static LazyBlockAllocator<checksum_info> ChecksumInfoAllocator;
#else
static HardBlockAllocator<blockinfo> BlockInfoAllocator;
static HardBlockAllocator<checksum_info> ChecksumInfoAllocator;
#endif

static inline checksum_info *alloc_checksum_info(void)
{
	checksum_info *csi = ChecksumInfoAllocator.acquire();
	csi->next = NULL;
	return csi;
}

static inline void free_checksum_info(checksum_info *csi)
{
	csi->next = NULL;
	ChecksumInfoAllocator.release(csi);
}

static inline void free_checksum_info_chain(checksum_info *csi)
{
	while (csi != NULL) {
		checksum_info *csi2 = csi->next;
		free_checksum_info(csi);
		csi = csi2;
	}
}

static inline blockinfo *alloc_blockinfo(void)
{
	blockinfo *bi = BlockInfoAllocator.acquire();
#if USE_CHECKSUM_INFO
	bi->csi = NULL;
#endif
	return bi;
}

static inline void free_blockinfo(blockinfo *bi)
{
#if USE_CHECKSUM_INFO
	free_checksum_info_chain(bi->csi);
	bi->csi = NULL;
#endif
	BlockInfoAllocator.release(bi);
}

static inline void alloc_blockinfos(void)
{
	int i;
	blockinfo* bi;

	for (i=0;i<MAX_HOLD_BI;i++) {
		if (hold_bi[i])
			return;
		bi=hold_bi[i]=alloc_blockinfo();
		prepare_block(bi);
	}
}

/********************************************************************
 * Functions to emit data into memory, and other general support    *
 ********************************************************************/

static uae_u8* target;

static inline void emit_byte(uae_u8 x)
{
	*target++=x;
}

static inline void skip_n_bytes(int n) {
	target += n;
}

static inline void skip_byte()
{
	skip_n_bytes(1);
}

static inline void skip_word()
{
	skip_n_bytes(2);
}

static inline void skip_long()
{
	skip_n_bytes(4);
}

static inline void skip_quad()
{
	skip_n_bytes(8);
}

static inline void emit_word(uae_u16 x)
{
	*((uae_u16*)target)=x;
	skip_word();
}

static inline void emit_long(uae_u32 x)
{
	*((uae_u32*)target)=x;
	skip_long();
}

static inline void emit_quad(uae_u64 x)
{
	*((uae_u64*) target) = x;
	skip_quad();
}

static inline void emit_block(const uae_u8 *block, uae_u32 blocklen)
{
	memcpy((uae_u8 *)target,block,blocklen);
	target+=blocklen;
}

#define MAX_COMPILE_PTR		max_compile_start

static inline uae_u32 reverse32(uae_u32 v)
{
#ifdef WINUAE_ARANYM
	// gb-- We have specialized byteswapping functions, just use them
	return do_byteswap_32(v);
#else
	return ((v>>24)&0xff) | ((v>>8)&0xff00) | ((v<<8)&0xff0000) | ((v<<24)&0xff000000);
#endif
}

void set_target(uae_u8* t)
{
	target=t;
}

static inline uae_u8* get_target_noopt(void)
{
	return target;
}

inline uae_u8* get_target(void)
{
	return get_target_noopt();
}

/********************************************************************
 * New version of data buffer: interleave data and code             *
 ********************************************************************/
#if defined(USE_DATA_BUFFER)

#define DATA_BUFFER_SIZE 1024    // Enlarge POPALLSPACE_SIZE if this value is greater than 768
#define DATA_BUFFER_MAXOFFSET 4096 - 32 // max range between emit of data and use of data
static uae_u8* data_writepos = 0;
static uae_u8* data_endpos = 0;
#if DEBUG
static long data_wasted = 0;
#endif

static inline void compemu_raw_branch(IMM d);

static inline void data_check_end(long n, long codesize)
{
	if(data_writepos + n > data_endpos || get_target_noopt() + codesize - data_writepos > DATA_BUFFER_MAXOFFSET)
	{
		// Start new buffer
#if DEBUG
		if(data_writepos < data_endpos)
			data_wasted += data_endpos - data_writepos;
#endif
		compemu_raw_branch(DATA_BUFFER_SIZE);
		data_writepos = get_target_noopt();
		data_endpos = data_writepos + DATA_BUFFER_SIZE;
		set_target(get_target_noopt() + DATA_BUFFER_SIZE);
	}
}

static inline long data_word_offs(uae_u16 x)
{
	data_check_end(4, 4);
#ifdef WORDS_BIGENDIAN
	*((uae_u16*)data_writepos)=x;
	data_writepos += 2;
	*((uae_u16*)data_writepos)=0;
	data_writepos += 2;
#else
	*((uae_u32*)data_writepos)=x;
	data_writepos += 4;
#endif
	return (long)data_writepos - (long)get_target_noopt() - 12;
}

static inline long data_long(uae_u32 x, long codesize)
{
	data_check_end(4, codesize);
	*((uae_u32*)data_writepos)=x;
	data_writepos += 4;
	return (long)data_writepos - 4;
}

static inline long data_long_offs(uae_u32 x)
{
	data_check_end(4, 4);
	*((uae_u32*)data_writepos)=x;
	data_writepos += 4;
	return (long)data_writepos - (long)get_target_noopt() - 12;
}

static inline long get_data_offset(long t)
{
	return t - (long)get_target_noopt() - 8;
}

static inline void reset_data_buffer(void)
{
	data_writepos = 0;
	data_endpos = 0;
}

#endif
/********************************************************************
 * Getting the information about the target CPU                     *
 ********************************************************************/

#if defined(CPU_arm)
#include "codegen_arm.cpp"
#endif
#if defined(CPU_i386) || defined(CPU_x86_64)
#include "codegen_x86.cpp"
#endif


/********************************************************************
 * Flags status handling. EMIT TIME!                                *
 ********************************************************************/

static void bt_l_ri_noclobber(RR4 r, IMM i);

static void make_flags_live_internal(void)
{
	if (live.flags_in_flags==VALID)
		return;
	Dif (live.flags_on_stack==TRASH) {
		jit_abort("Want flags, got something on stack, but it is TRASH");
	}
	if (live.flags_on_stack==VALID) {
		int tmp;
		tmp=readreg_specific(FLAGTMP,4,FLAG_NREG2);
		raw_reg_to_flags(tmp);
		unlock2(tmp);

		live.flags_in_flags=VALID;
		return;
	}
	jit_abort("Huh? live.flags_in_flags=%d, live.flags_on_stack=%d, but need to make live",
		live.flags_in_flags,live.flags_on_stack);
}

static void flags_to_stack(void)
{
	if (live.flags_on_stack==VALID)
		return;
	if (!live.flags_are_important) {
		live.flags_on_stack=VALID;
		return;
	}
	Dif (live.flags_in_flags!=VALID)
		jit_abort("flags_to_stack != VALID");
	else  {
		int tmp;
		tmp=writereg_specific(FLAGTMP,4,FLAG_NREG1);
		raw_flags_to_reg(tmp);
		unlock2(tmp);
	}
	live.flags_on_stack=VALID;
}

static inline void clobber_flags(void)
{
	if (live.flags_in_flags==VALID && live.flags_on_stack!=VALID)
		flags_to_stack();
	live.flags_in_flags=TRASH;
}

/* Prepare for leaving the compiled stuff */
static inline void flush_flags(void)
{
	flags_to_stack();
	return;
}

int touchcnt;

/********************************************************************
 * Partial register flushing for optimized calls                    *
 ********************************************************************/

struct regusage {
	uae_u16 rmask;
	uae_u16 wmask;
};

#if 0
static inline void ru_set(uae_u16 *mask, int reg)
{
#if USE_OPTIMIZED_CALLS
	*mask |= 1 << reg;
#else
	UNUSED(mask);
	UNUSED(reg);
#endif
}

static inline bool ru_get(const uae_u16 *mask, int reg)
{
#if USE_OPTIMIZED_CALLS
	return (*mask & (1 << reg));
#else
	UNUSED(mask);
	UNUSED(reg);
	/* Default: instruction reads & write to register */
	return true;
#endif
}

static inline void ru_set_read(regusage *ru, int reg)
{
	ru_set(&ru->rmask, reg);
}

static inline void ru_set_write(regusage *ru, int reg)
{
	ru_set(&ru->wmask, reg);
}

static inline bool ru_read_p(const regusage *ru, int reg)
{
	return ru_get(&ru->rmask, reg);
}

static inline bool ru_write_p(const regusage *ru, int reg)
{
	return ru_get(&ru->wmask, reg);
}

static void ru_fill_ea(regusage *ru, int reg, amodes mode,
					   wordsizes size, int write_mode)
{
	switch (mode) {
	case Areg:
		reg += 8;
		/* fall through */
	case Dreg:
		ru_set(write_mode ? &ru->wmask : &ru->rmask, reg);
		break;
	case Ad16:
		/* skip displacment */
		m68k_pc_offset += 2;
	case Aind:
	case Aipi:
	case Apdi:
		ru_set_read(ru, reg+8);
		break;
	case Ad8r:
		ru_set_read(ru, reg+8);
		/* fall through */
	case PC8r: {
		uae_u16 dp = comp_get_iword((m68k_pc_offset+=2)-2);
		reg = (dp >> 12) & 15;
		ru_set_read(ru, reg);
		if (dp & 0x100)
			m68k_pc_offset += (((dp & 0x30) >> 3) & 7) + ((dp & 3) * 2);
		break;
	}
	case PC16:
	case absw:
	case imm0:
	case imm1:
		m68k_pc_offset += 2;
		break;
	case absl:
	case imm2:
		m68k_pc_offset += 4;
		break;
	case immi:
		m68k_pc_offset += (size == sz_long) ? 4 : 2;
		break;
	}
}

/* TODO: split into a static initialization part and a dynamic one
   (instructions depending on extension words) */

static void ru_fill(regusage *ru, uae_u32 opcode)
{
	m68k_pc_offset += 2;

	/* Default: no register is used or written to */
	ru->rmask = 0;
	ru->wmask = 0;

	uae_u32 real_opcode = cft_map(opcode);
	struct instr *dp = &table68k[real_opcode];

	bool rw_dest = true;
	bool handled = false;

	/* Handle some instructions specifically */
	uae_u16 ext;
	switch (dp->mnemo) {
	case i_BFCHG:
	case i_BFCLR:
	case i_BFEXTS:
	case i_BFEXTU:
	case i_BFFFO:
	case i_BFINS:
	case i_BFSET:
	case i_BFTST:
		ext = comp_get_iword((m68k_pc_offset+=2)-2);
		if (ext & 0x800) ru_set_read(ru, (ext >> 6) & 7);
		if (ext & 0x020) ru_set_read(ru, ext & 7);
		ru_fill_ea(ru, dp->dreg, (amodes)dp->dmode, (wordsizes)dp->size, 1);
		if (dp->dmode == Dreg)
			ru_set_read(ru, dp->dreg);
		switch (dp->mnemo) {
		case i_BFEXTS:
		case i_BFEXTU:
		case i_BFFFO:
			ru_set_write(ru, (ext >> 12) & 7);
			break;
		case i_BFINS:
			ru_set_read(ru, (ext >> 12) & 7);
			/* fall through */
		case i_BFCHG:
		case i_BFCLR:
		case i_BSET:
			if (dp->dmode == Dreg)
				ru_set_write(ru, dp->dreg);
			break;
		}
		handled = true;
		rw_dest = false;
		break;

	case i_BTST:
		rw_dest = false;
		break;

	case i_CAS:
	{
		ext = comp_get_iword((m68k_pc_offset+=2)-2);
		int Du = ext & 7;
		ru_set_read(ru, Du);
		int Dc = (ext >> 6) & 7;
		ru_set_read(ru, Dc);
		ru_set_write(ru, Dc);
		break;
	}
	case i_CAS2:
	{
		int Dc1, Dc2, Du1, Du2, Rn1, Rn2;
		ext = comp_get_iword((m68k_pc_offset+=2)-2);
		Rn1 = (ext >> 12) & 15;
		Du1 = (ext >> 6) & 7;
		Dc1 = ext & 7;
		ru_set_read(ru, Rn1);
		ru_set_read(ru, Du1);
		ru_set_read(ru, Dc1);
		ru_set_write(ru, Dc1);
		ext = comp_get_iword((m68k_pc_offset+=2)-2);
		Rn2 = (ext >> 12) & 15;
		Du2 = (ext >> 6) & 7;
		Dc2 = ext & 7;
		ru_set_read(ru, Rn2);
		ru_set_read(ru, Du2);
		ru_set_write(ru, Dc2);
		break;
	}
	case i_DIVL: case i_MULL:
		m68k_pc_offset += 2;
		break;
	case i_LEA:
	case i_MOVE: case i_MOVEA: case i_MOVE16:
		rw_dest = false;
		break;
	case i_PACK: case i_UNPK:
		rw_dest = false;
		m68k_pc_offset += 2;
		break;
	case i_TRAPcc:
		m68k_pc_offset += (dp->size == sz_long) ? 4 : 2;
		break;
	case i_RTR:
		/* do nothing, just for coverage debugging */
		break;
	/* TODO: handle EXG instruction */
	}

	/* Handle A-Traps better */
	if ((real_opcode & 0xf000) == 0xa000) {
		handled = true;
	}

	/* Handle EmulOps better */
	if ((real_opcode & 0xff00) == 0x7100) {
		handled = true;
		ru->rmask = 0xffff;
		ru->wmask = 0;
	}

	if (dp->suse && !handled)
		ru_fill_ea(ru, dp->sreg, (amodes)dp->smode, (wordsizes)dp->size, 0);

	if (dp->duse && !handled)
		ru_fill_ea(ru, dp->dreg, (amodes)dp->dmode, (wordsizes)dp->size, 1);

	if (rw_dest)
		ru->rmask |= ru->wmask;

	handled = handled || dp->suse || dp->duse;

	/* Mark all registers as used/written if the instruction may trap */
	if (may_trap(opcode)) {
		handled = true;
		ru->rmask = 0xffff;
		ru->wmask = 0xffff;
	}

	if (!handled) {
		jit_abort("ru_fill: %04x = { %04x, %04x }",
				  real_opcode, ru->rmask, ru->wmask);
	}
}
#endif

/********************************************************************
 * register allocation per block logging                            *
 ********************************************************************/

static uae_s8 vstate[VREGS];
static uae_s8 vwritten[VREGS];
static uae_s8 nstate[N_REGS];

#define L_UNKNOWN -127
#define L_UNAVAIL -1
#define L_NEEDED -2
#define L_UNNEEDED -3

#if USE_MATCH
static inline void big_to_small_state(bigstate * /* b */, smallstate * s)
{
  int i;
	
  for (i = 0; i < VREGS; i++)
	s->virt[i] = vstate[i];
  for (i = 0; i < N_REGS; i++)
	s->nat[i] = nstate[i];
}

static inline int callers_need_recompile(bigstate * /* b */, smallstate * s)
{
  int i;
  int reverse = 0;

  for (i = 0; i < VREGS; i++) {
	if (vstate[i] != L_UNNEEDED && s->virt[i] == L_UNNEEDED)
	  return 1;
	if (vstate[i] == L_UNNEEDED && s->virt[i] != L_UNNEEDED)
	  reverse++;
  }
  for (i = 0; i < N_REGS; i++) {
	if (nstate[i] >= 0 && nstate[i] != s->nat[i])
	  return 1;
	if (nstate[i] < 0 && s->nat[i] >= 0)
	  reverse++;
  }
  if (reverse >= 2 && USE_MATCH)
	return 1;	/* In this case, it might be worth recompiling the
				 * callers */
  return 0;
}
#endif

static inline void log_startblock(void)
{
	int i;

	for (i = 0; i < VREGS; i++) {
		vstate[i] = L_UNKNOWN;
		vwritten[i] = 0;
	}
	for (i = 0; i < N_REGS; i++)
		nstate[i] = L_UNKNOWN;
}

/* Using an n-reg for a temp variable */
static inline void log_isused(int n)
{
	if (nstate[n] == L_UNKNOWN)
		nstate[n] = L_UNAVAIL;
}

static inline void log_visused(int r)
{
	if (vstate[r] == L_UNKNOWN)
		vstate[r] = L_NEEDED;
}

static inline void do_load_reg(int n, int r)
{
	if (r == FLAGTMP)
		raw_load_flagreg(n);
	else if (r == FLAGX)
		raw_load_flagx(n);
	else
		compemu_raw_mov_l_rm(n, (uintptr) live.state[r].mem);
}

#if 0
static inline void check_load_reg(int n, int r)
{
	compemu_raw_mov_l_rm(n, (uintptr) live.state[r].mem);
}
#endif

static inline void log_vwrite(int r)
{
	vwritten[r] = 1;
}

/* Using an n-reg to hold a v-reg */
static inline void log_isreg(int n, int r)
{
	if (nstate[n] == L_UNKNOWN && r < 16 && !vwritten[r] && USE_MATCH)
		nstate[n] = r;
	else {
		do_load_reg(n, r);
		if (nstate[n] == L_UNKNOWN)
			nstate[n] = L_UNAVAIL;
	}
	if (vstate[r] == L_UNKNOWN)
		vstate[r] = L_NEEDED;
}

static inline void log_clobberreg(int r)
{
	if (vstate[r] == L_UNKNOWN)
		vstate[r] = L_UNNEEDED;
}

/* This ends all possibility of clever register allocation */

static inline void log_flush(void)
{
	int i;

	for (i = 0; i < VREGS; i++)
		if (vstate[i] == L_UNKNOWN)
			vstate[i] = L_NEEDED;
	for (i = 0; i < N_REGS; i++)
		if (nstate[i] == L_UNKNOWN)
			nstate[i] = L_UNAVAIL;
}

static inline void log_dump(void)
{
	int i;

	return;

	jit_log("----------------------");
	for (i = 0; i < N_REGS; i++) {
		switch (nstate[i]) {
		case L_UNKNOWN:
			jit_log("Nat %d : UNKNOWN", i);
			break;
		case L_UNAVAIL:
			jit_log("Nat %d : UNAVAIL", i);
			break;
		default:
			jit_log("Nat %d : %d", i, nstate[i]);
			break;
		}
	}
	for (i = 0; i < VREGS; i++) {
		if (vstate[i] == L_UNNEEDED) {
			jit_log("Virt %d: UNNEEDED", i);
		}
	}
}

/********************************************************************
 * register status handling. EMIT TIME!                             *
 ********************************************************************/

static inline void set_status(int r, int status)
{
	if (status == ISCONST)
		log_clobberreg(r);
	live.state[r].status=status;
}

static inline int isinreg(int r)
{
	return live.state[r].status==CLEAN || live.state[r].status==DIRTY;
}

static inline void adjust_nreg(int r, uae_u32 val)
{
	if (!val)
		return;
	compemu_raw_lea_l_brr(r,r,val);
}

static void tomem(int r)
{
	int rr=live.state[r].realreg;

	if (isinreg(r)) {
		if (live.state[r].val && live.nat[rr].nholds==1
			&& !live.nat[rr].locked) {
			jit_log2("RemovingA offset %x from reg %d (%d) at %p", live.state[r].val,r,rr,target);
			adjust_nreg(rr,live.state[r].val);
			live.state[r].val=0;
			live.state[r].dirtysize=4;
			set_status(r,DIRTY);
		}
	}

	if (live.state[r].status==DIRTY) {
		switch (live.state[r].dirtysize) {
		case 1: compemu_raw_mov_b_mr((uintptr)live.state[r].mem,rr); break;
		case 2: compemu_raw_mov_w_mr((uintptr)live.state[r].mem,rr); break;
		case 4: compemu_raw_mov_l_mr((uintptr)live.state[r].mem,rr); break;
		default: abort();
		}
		log_vwrite(r);
		set_status(r,CLEAN);
		live.state[r].dirtysize=0;
	}
}

static inline int isconst(int r)
{
	return live.state[r].status==ISCONST;
}

int is_const(int r)
{
	return isconst(r);
}

static inline void writeback_const(int r)
{
	if (!isconst(r))
		return;
	Dif (live.state[r].needflush==NF_HANDLER) {
		jit_abort("Trying to write back constant NF_HANDLER!");
	}

	compemu_raw_mov_l_mi((uintptr)live.state[r].mem,live.state[r].val);
	log_vwrite(r);
	live.state[r].val=0;
	set_status(r,INMEM);
}

static inline void tomem_c(int r)
{
	if (isconst(r)) {
		writeback_const(r);
	}
	else
		tomem(r);
}

static void evict(int r)
{
	int rr;

	if (!isinreg(r))
		return;
	tomem(r);
	rr=live.state[r].realreg;

	Dif (live.nat[rr].locked &&
		live.nat[rr].nholds==1) {
		jit_abort("register %d in nreg %d is locked!",r,live.state[r].realreg);
	}

	live.nat[rr].nholds--;
	if (live.nat[rr].nholds!=live.state[r].realind) { /* Was not last */
		int topreg=live.nat[rr].holds[live.nat[rr].nholds];
		int thisind=live.state[r].realind;

		live.nat[rr].holds[thisind]=topreg;
		live.state[topreg].realind=thisind;
	}
	live.state[r].realreg=-1;
	set_status(r,INMEM);
}

static inline void free_nreg(int r)
{
	int i=live.nat[r].nholds;

	while (i) {
		int vr;

		--i;
		vr=live.nat[r].holds[i];
		evict(vr);
	}
	Dif (live.nat[r].nholds!=0) {
		jit_abort("Failed to free nreg %d, nholds is %d",r,live.nat[r].nholds);
	}
}

/* Use with care! */
static inline void isclean(int r)
{
	if (!isinreg(r))
		return;
	live.state[r].validsize=4;
	live.state[r].dirtysize=0;
	live.state[r].val=0;
	set_status(r,CLEAN);
}

static inline void disassociate(int r)
{
	isclean(r);
	evict(r);
}

/* XXFIXME: val may be 64bit address for PC_P */
static inline void set_const(int r, uae_u32 val)
{
	disassociate(r);
	live.state[r].val=val;
	set_status(r,ISCONST);
}

static inline uae_u32 get_offset(int r)
{
	return live.state[r].val;
}

static int alloc_reg_hinted(int r, int size, int willclobber, int hint)
{
	int bestreg;
	uae_s32 when;
	int i;
	uae_s32 badness=0; /* to shut up gcc */
	bestreg=-1;
	when=2000000000;

	/* XXX use a regalloc_order table? */
	for (i=0;i<N_REGS;i++) {
		badness=live.nat[i].touched;
		if (live.nat[i].nholds==0)
			badness=0;
		if (i==hint)
			badness-=200000000;
		if (!live.nat[i].locked && badness<when) {
			if ((size==1 && live.nat[i].canbyte) ||
				(size==2 && live.nat[i].canword) ||
				(size==4)) {
				bestreg=i;
				when=badness;
				if (live.nat[i].nholds==0 && hint<0)
					break;
				if (i==hint)
					break;
			}
		}
	}
	Dif (bestreg==-1)
		jit_abort("alloc_reg_hinted bestreg=-1");

	if (live.nat[bestreg].nholds>0) {
		free_nreg(bestreg);
	}
	if (isinreg(r)) {
		int rr=live.state[r].realreg;
		/* This will happen if we read a partially dirty register at a
		   bigger size */
		Dif (willclobber || live.state[r].validsize>=size)
			jit_abort("willclobber || live.state[r].validsize>=size");
		Dif (live.nat[rr].nholds!=1)
			jit_abort("live.nat[rr].nholds!=1");
		if (size==4 && live.state[r].validsize==2) {
			log_isused(bestreg);
			log_visused(r);
			compemu_raw_mov_l_rm(bestreg,(uintptr)live.state[r].mem);
			compemu_raw_bswap_32(bestreg);
			compemu_raw_zero_extend_16_rr(rr,rr);
			compemu_raw_zero_extend_16_rr(bestreg,bestreg);
			compemu_raw_bswap_32(bestreg);
			compemu_raw_lea_l_rr_indexed(rr, rr, bestreg, 1);
			live.state[r].validsize=4;
			live.nat[rr].touched=touchcnt++;
			return rr;
		}
		if (live.state[r].validsize==1) {
			/* Nothing yet */
		}
		evict(r);
	}

	if (!willclobber) {
		if (live.state[r].status!=UNDEF) {
			if (isconst(r)) {
				compemu_raw_mov_l_ri(bestreg,live.state[r].val);
				live.state[r].val=0;
				live.state[r].dirtysize=4;
				set_status(r,DIRTY);
				log_isused(bestreg);
			}
			else {
				log_isreg(bestreg, r);  /* This will also load it! */
				live.state[r].dirtysize=0;
				set_status(r,CLEAN);
			}
		}
		else {
			live.state[r].val=0;
			live.state[r].dirtysize=0;
			set_status(r,CLEAN);
			log_isused(bestreg);
		}
		live.state[r].validsize=4;
	}
	else { /* this is the easiest way, but not optimal. FIXME! */
		/* Now it's trickier, but hopefully still OK */
		if (!isconst(r) || size==4) {
			live.state[r].validsize=size;
			live.state[r].dirtysize=size;
			live.state[r].val=0;
			set_status(r,DIRTY);
			if (size == 4) {
				log_clobberreg(r);
				log_isused(bestreg);
			}
			else {
				log_visused(r);
				log_isused(bestreg);
			}
		}
		else {
			if (live.state[r].status!=UNDEF)
				compemu_raw_mov_l_ri(bestreg,live.state[r].val);
			live.state[r].val=0;
			live.state[r].validsize=4;
			live.state[r].dirtysize=4;
			set_status(r,DIRTY);
			log_isused(bestreg);
		}
	}
	live.state[r].realreg=bestreg;
	live.state[r].realind=live.nat[bestreg].nholds;
	live.nat[bestreg].touched=touchcnt++;
	live.nat[bestreg].holds[live.nat[bestreg].nholds]=r;
	live.nat[bestreg].nholds++;

	return bestreg;
}

/*
static int alloc_reg(int r, int size, int willclobber)
{
	return alloc_reg_hinted(r,size,willclobber,-1);
}
*/

static void unlock2(int r)
{
	Dif (!live.nat[r].locked)
		jit_abort("unlock2 %d not locked", r);
	live.nat[r].locked--;
}

static void setlock(int r)
{
	live.nat[r].locked++;
}


static void mov_nregs(int d, int s)
{
	int nd=live.nat[d].nholds;
	int i;

	if (s==d)
		return;

	if (nd>0)
		free_nreg(d);

	log_isused(d);
	compemu_raw_mov_l_rr(d,s);

	for (i=0;i<live.nat[s].nholds;i++) {
		int vs=live.nat[s].holds[i];

		live.state[vs].realreg=d;
		live.state[vs].realind=i;
		live.nat[d].holds[i]=vs;
	}
	live.nat[d].nholds=live.nat[s].nholds;

	live.nat[s].nholds=0;
}


static inline void make_exclusive(int r, int size, int spec)
{
	reg_status oldstate;
	int rr=live.state[r].realreg;
	int nr;
	int nind;
	int ndirt=0;
	int i;

	if (!isinreg(r))
		return;
	if (live.nat[rr].nholds==1)
		return;
	for (i=0;i<live.nat[rr].nholds;i++) {
		int vr=live.nat[rr].holds[i];
		if (vr!=r &&
			(live.state[vr].status==DIRTY || live.state[vr].val))
			ndirt++;
	}
	if (!ndirt && size<live.state[r].validsize && !live.nat[rr].locked) {
		/* Everything else is clean, so let's keep this register */
		for (i=0;i<live.nat[rr].nholds;i++) {
			int vr=live.nat[rr].holds[i];
			if (vr!=r) {
				evict(vr);
				i--; /* Try that index again! */
			}
		}
		Dif (live.nat[rr].nholds!=1) {
			jit_abort("natreg %d holds %d vregs, %d not exclusive",
				rr,live.nat[rr].nholds,r);
		}
		return;
	}

	/* We have to split the register */
	oldstate=live.state[r];

	setlock(rr); /* Make sure this doesn't go away */
	/* Forget about r being in the register rr */
	disassociate(r);
	/* Get a new register, that we will clobber completely */
	if (oldstate.status==DIRTY) {
		/* If dirtysize is <4, we need a register that can handle the
		   eventual smaller memory store! Thanks to Quake68k for exposing
		   this detail ;-) */
		nr=alloc_reg_hinted(r,oldstate.dirtysize,1,spec);
	}
	else {
		nr=alloc_reg_hinted(r,4,1,spec);
	}
	nind=live.state[r].realind;
	live.state[r]=oldstate;   /* Keep all the old state info */
	live.state[r].realreg=nr;
	live.state[r].realind=nind;

	if (size<live.state[r].validsize) {
		if (live.state[r].val) {
			/* Might as well compensate for the offset now */
			compemu_raw_lea_l_brr(nr,rr,oldstate.val);
			live.state[r].val=0;
			live.state[r].dirtysize=4;
			set_status(r,DIRTY);
		}
		else
			compemu_raw_mov_l_rr(nr,rr);  /* Make another copy */
	}
	unlock2(rr);
}

static inline void add_offset(int r, uae_u32 off)
{
	live.state[r].val+=off;
}

static inline void remove_offset(int r, int spec)
{
	int rr;

	if (isconst(r))
		return;
	if (live.state[r].val==0)
		return;
	if (isinreg(r) && live.state[r].validsize<4)
		evict(r);

	if (!isinreg(r))
		alloc_reg_hinted(r,4,0,spec);

	Dif (live.state[r].validsize!=4) {
		jit_abort("Validsize=%d in remove_offset",live.state[r].validsize);
	}
	make_exclusive(r,0,-1);
	/* make_exclusive might have done the job already */
	if (live.state[r].val==0)
		return;

	rr=live.state[r].realreg;

	if (live.nat[rr].nholds==1) {
		jit_log2("RemovingB offset %x from reg %d (%d) at %p", live.state[r].val,r,rr,target);
		adjust_nreg(rr,live.state[r].val);
		live.state[r].dirtysize=4;
		live.state[r].val=0;
		set_status(r,DIRTY);
		return;
	}
	jit_abort("Failed in remove_offset");
}

static inline void remove_all_offsets(void)
{
	int i;

	for (i=0;i<VREGS;i++)
		remove_offset(i,-1);
}

static inline void flush_reg_count(void)
{
#ifdef RECORD_REGISTER_USAGE
	for (int r = 0; r < 16; r++)
		if (reg_count_local[r])
			ADDQim(reg_count_local[r], ((uintptr)reg_count) + (8 * r), X86_NOREG, X86_NOREG, 1);
#endif
}

static inline void record_register(int r)
{
#ifdef RECORD_REGISTER_USAGE
	if (r < 16)
		reg_count_local[r]++;
#else
	UNUSED(r);
#endif
}

static inline int readreg_general(int r, int size, int spec, int can_offset)
{
	int n;
	int answer=-1;

	record_register(r);
	if (live.state[r].status==UNDEF) {
		jit_log("WARNING: Unexpected read of undefined register %d",r);
	}
	if (!can_offset)
		remove_offset(r,spec);

	if (isinreg(r) && live.state[r].validsize>=size) {
		n=live.state[r].realreg;
		switch(size) {
		case 1:
			if (live.nat[n].canbyte || spec>=0) {
				answer=n;
			}
			break;
		case 2:
			if (live.nat[n].canword || spec>=0) {
				answer=n;
			}
			break;
		case 4:
			answer=n;
			break;
		default: abort();
		}
		if (answer<0)
			evict(r);
	}
	/* either the value was in memory to start with, or it was evicted and
	   is in memory now */
	if (answer<0) {
		answer=alloc_reg_hinted(r,spec>=0?4:size,0,spec);
	}

	if (spec>=0 && spec!=answer) {
		/* Too bad */
		mov_nregs(spec,answer);
		answer=spec;
	}
	live.nat[answer].locked++;
	live.nat[answer].touched=touchcnt++;
	return answer;
}



static int readreg(int r, int size)
{
	return readreg_general(r,size,-1,0);
}

static int readreg_specific(int r, int size, int spec)
{
	return readreg_general(r,size,spec,0);
}

static int readreg_offset(int r, int size)
{
	return readreg_general(r,size,-1,1);
}

/* writereg_general(r, size, spec)
 *
 * INPUT
 * - r    : mid-layer register
 * - size : requested size (1/2/4)
 * - spec : -1 if find or make a register free, otherwise specifies
 *          the physical register to use in any case
 *
 * OUTPUT
 * - hard (physical, x86 here) register allocated to virtual register r
 */
static inline int writereg_general(int r, int size, int spec)
{
	int n;
	int answer=-1;

	record_register(r);
	if (size<4) {
		remove_offset(r,spec);
	}

	make_exclusive(r,size,spec);
	if (isinreg(r)) {
		int nvsize=size>live.state[r].validsize?size:live.state[r].validsize;
		int ndsize=size>live.state[r].dirtysize?size:live.state[r].dirtysize;
		n=live.state[r].realreg;

		Dif (live.nat[n].nholds!=1)
			jit_abort("live.nat[%d].nholds!=1", n);
		switch(size) {
		case 1:
			if (live.nat[n].canbyte || spec>=0) {
				live.state[r].dirtysize=ndsize;
				live.state[r].validsize=nvsize;
				answer=n;
			}
			break;
		case 2:
			if (live.nat[n].canword || spec>=0) {
				live.state[r].dirtysize=ndsize;
				live.state[r].validsize=nvsize;
				answer=n;
			}
			break;
		case 4:
			live.state[r].dirtysize=ndsize;
			live.state[r].validsize=nvsize;
			answer=n;
			break;
		default: abort();
		}
		if (answer<0)
			evict(r);
	}
	/* either the value was in memory to start with, or it was evicted and
	   is in memory now */
	if (answer<0) {
		answer=alloc_reg_hinted(r,size,1,spec);
	}
	if (spec>=0 && spec!=answer) {
		mov_nregs(spec,answer);
		answer=spec;
	}
	if (live.state[r].status==UNDEF)
		live.state[r].validsize=4;
	live.state[r].dirtysize=size>live.state[r].dirtysize?size:live.state[r].dirtysize;
	live.state[r].validsize=size>live.state[r].validsize?size:live.state[r].validsize;

	live.nat[answer].locked++;
	live.nat[answer].touched=touchcnt++;
	if (size==4) {
		live.state[r].val=0;
	}
	else {
		Dif (live.state[r].val) {
			jit_abort("Problem with val");
		}
	}
	set_status(r,DIRTY);
	return answer;
}

static int writereg(int r, int size)
{
	return writereg_general(r,size,-1);
}

static int writereg_specific(int r, int size, int spec)
{
	return writereg_general(r,size,spec);
}

static inline int rmw_general(int r, int wsize, int rsize, int spec)
{
	int n;
	int answer=-1;

	record_register(r);
	if (live.state[r].status==UNDEF) {
		jit_log("WARNING: Unexpected read of undefined register %d",r);
	}
	remove_offset(r,spec);
	make_exclusive(r,0,spec);

	Dif (wsize<rsize) {
		jit_abort("Cannot handle wsize<rsize in rmw_general()");
	}
	if (isinreg(r) && live.state[r].validsize>=rsize) {
		n=live.state[r].realreg;
		Dif (live.nat[n].nholds!=1)
			jit_abort("live.nat[%d].nholds!=1", n);

		switch(rsize) {
		case 1:
			if (live.nat[n].canbyte || spec>=0) {
				answer=n;
			}
			break;
		case 2:
			if (live.nat[n].canword || spec>=0) {
				answer=n;
			}
			break;
		case 4:
			answer=n;
			break;
		default: abort();
		}
		if (answer<0)
			evict(r);
	}
	/* either the value was in memory to start with, or it was evicted and
	   is in memory now */
	if (answer<0) {
		answer=alloc_reg_hinted(r,spec>=0?4:rsize,0,spec);
	}

	if (spec>=0 && spec!=answer) {
		/* Too bad */
		mov_nregs(spec,answer);
		answer=spec;
	}
	if (wsize>live.state[r].dirtysize)
		live.state[r].dirtysize=wsize;
	if (wsize>live.state[r].validsize)
		live.state[r].validsize=wsize;
	set_status(r,DIRTY);

	live.nat[answer].locked++;
	live.nat[answer].touched=touchcnt++;

	Dif (live.state[r].val) {
		jit_abort("Problem with val(rmw)");
	}
	return answer;
}

static int rmw(int r, int wsize, int rsize)
{
	return rmw_general(r,wsize,rsize,-1);
}

static int rmw_specific(int r, int wsize, int rsize, int spec)
{
	return rmw_general(r,wsize,rsize,spec);
}


/* needed for restoring the carry flag on non-P6 cores */
static void bt_l_ri_noclobber(RR4 r, IMM i)
{
	int size=4;
	if (i<16)
		size=2;
	r=readreg(r,size);
	compemu_raw_bt_l_ri(r,i);
	unlock2(r);
}

/********************************************************************
 * FPU register status handling. EMIT TIME!                         *
 ********************************************************************/

static void f_tomem(int r)
{
	if (live.fate[r].status==DIRTY) {
#if defined(USE_LONG_DOUBLE)
		raw_fmov_ext_mr((uintptr)live.fate[r].mem,live.fate[r].realreg);
#else
		raw_fmov_mr((uintptr)live.fate[r].mem,live.fate[r].realreg);
#endif
		live.fate[r].status=CLEAN;
	}
}

static void f_tomem_drop(int r)
{
	if (live.fate[r].status==DIRTY) {
#if defined(USE_LONG_DOUBLE)
		raw_fmov_ext_mr_drop((uintptr)live.fate[r].mem,live.fate[r].realreg);
#else
		raw_fmov_mr_drop((uintptr)live.fate[r].mem,live.fate[r].realreg);
#endif
		live.fate[r].status=INMEM;
	}
}


static inline int f_isinreg(int r)
{
	return live.fate[r].status==CLEAN || live.fate[r].status==DIRTY;
}

static void f_evict(int r)
{
	int rr;

	if (!f_isinreg(r))
		return;
	rr=live.fate[r].realreg;
	if (live.fat[rr].nholds==1)
		f_tomem_drop(r);
	else
		f_tomem(r);

	Dif (live.fat[rr].locked &&
		live.fat[rr].nholds==1) {
		jit_abort("FPU register %d in nreg %d is locked!",r,live.fate[r].realreg);
	}

	live.fat[rr].nholds--;
	if (live.fat[rr].nholds!=live.fate[r].realind) { /* Was not last */
		int topreg=live.fat[rr].holds[live.fat[rr].nholds];
		int thisind=live.fate[r].realind;
		live.fat[rr].holds[thisind]=topreg;
		live.fate[topreg].realind=thisind;
	}
	live.fate[r].status=INMEM;
	live.fate[r].realreg=-1;
}

static inline void f_free_nreg(int r)
{
	int i=live.fat[r].nholds;

	while (i) {
		int vr;

		--i;
		vr=live.fat[r].holds[i];
		f_evict(vr);
	}
	Dif (live.fat[r].nholds!=0) {
		jit_abort("Failed to free nreg %d, nholds is %d",r,live.fat[r].nholds);
	}
}


/* Use with care! */
static inline void f_isclean(int r)
{
	if (!f_isinreg(r))
		return;
	live.fate[r].status=CLEAN;
}

static inline void f_disassociate(int r)
{
	f_isclean(r);
	f_evict(r);
}



static int f_alloc_reg(int r, int willclobber)
{
	int bestreg;
	uae_s32 when;
	int i;
	uae_s32 badness;
	bestreg=-1;
	when=2000000000;
	for (i=N_FREGS;i--;) {
		badness=live.fat[i].touched;
		if (live.fat[i].nholds==0)
			badness=0;

		if (!live.fat[i].locked && badness<when) {
			bestreg=i;
			when=badness;
			if (live.fat[i].nholds==0)
				break;
		}
	}
	Dif (bestreg==-1)
		jit_abort("bestreg==-1");

	if (live.fat[bestreg].nholds>0) {
		f_free_nreg(bestreg);
	}
	if (f_isinreg(r)) {
		f_evict(r);
	}

	if (!willclobber) {
		if (live.fate[r].status!=UNDEF) {
#if defined(USE_LONG_DOUBLE)
			raw_fmov_ext_rm(bestreg,(uintptr)live.fate[r].mem);
#else
			raw_fmov_rm(bestreg,(uintptr)live.fate[r].mem);
#endif
		}
		live.fate[r].status=CLEAN;
	}
	else {
		live.fate[r].status=DIRTY;
	}
	live.fate[r].realreg=bestreg;
	live.fate[r].realind=live.fat[bestreg].nholds;
	live.fat[bestreg].touched=touchcnt++;
	live.fat[bestreg].holds[live.fat[bestreg].nholds]=r;
	live.fat[bestreg].nholds++;

	return bestreg;
}

static void f_unlock(int r)
{
	Dif (!live.fat[r].locked)
		jit_abort ("unlock %d", r);
	live.fat[r].locked--;
}

static void f_setlock(int r)
{
	live.fat[r].locked++;
}

static inline int f_readreg(int r)
{
	int n;
	int answer=-1;

	if (f_isinreg(r)) {
		n=live.fate[r].realreg;
		answer=n;
	}
	/* either the value was in memory to start with, or it was evicted and
	   is in memory now */
	if (answer<0)
		answer=f_alloc_reg(r,0);

	live.fat[answer].locked++;
	live.fat[answer].touched=touchcnt++;
	return answer;
}

static inline void f_make_exclusive(int r, int clobber)
{
	freg_status oldstate;
	int rr=live.fate[r].realreg;
	int nr;
	int nind;
	int ndirt=0;
	int i;

	if (!f_isinreg(r))
		return;
	if (live.fat[rr].nholds==1)
		return;
	for (i=0;i<live.fat[rr].nholds;i++) {
		int vr=live.fat[rr].holds[i];
		if (vr!=r && live.fate[vr].status==DIRTY)
			ndirt++;
	}
	if (!ndirt && !live.fat[rr].locked) {
		/* Everything else is clean, so let's keep this register */
		for (i=0;i<live.fat[rr].nholds;i++) {
			int vr=live.fat[rr].holds[i];
			if (vr!=r) {
				f_evict(vr);
				i--; /* Try that index again! */
			}
		}
		Dif (live.fat[rr].nholds!=1) {
			jit_log("realreg %d holds %d (",rr,live.fat[rr].nholds);
			for (i=0;i<live.fat[rr].nholds;i++) {
				jit_log(" %d(%d,%d)",live.fat[rr].holds[i],
					live.fate[live.fat[rr].holds[i]].realreg,
					live.fate[live.fat[rr].holds[i]].realind);
			}
			jit_log("");
			jit_abort("x");
		}
		return;
	}

	/* We have to split the register */
	oldstate=live.fate[r];

	f_setlock(rr); /* Make sure this doesn't go away */
	/* Forget about r being in the register rr */
	f_disassociate(r);
	/* Get a new register, that we will clobber completely */
	nr=f_alloc_reg(r,1);
	nind=live.fate[r].realind;
	if (!clobber)
		raw_fmov_rr(nr,rr);  /* Make another copy */
	live.fate[r]=oldstate;   /* Keep all the old state info */
	live.fate[r].realreg=nr;
	live.fate[r].realind=nind;
	f_unlock(rr);
}


static inline int f_writereg(int r)
{
	int n;
	int answer=-1;

	f_make_exclusive(r,1);
	if (f_isinreg(r)) {
		n=live.fate[r].realreg;
		answer=n;
	}
	if (answer<0) {
		answer=f_alloc_reg(r,1);
	}
	live.fate[r].status=DIRTY;
	live.fat[answer].locked++;
	live.fat[answer].touched=touchcnt++;
	return answer;
}

/********************************************************************
 * Support functions, internal                                      *
 ********************************************************************/


static void align_target(uae_u32 a)
{
	if (!a)
		return;

	if (tune_nop_fillers)
		raw_emit_nop_filler(a - (((uintptr)target) & (a - 1)));
	else {
		/* Fill with NOPs --- makes debugging with gdb easier */
		while ((uintptr)target&(a-1))
			emit_byte(0x90); // Attention x86 specific code
	}
}

static inline int isinrom(uintptr addr)
{
#ifdef UAE
	return (addr >= uae_p32(kickmem_bank.baseaddr) &&
			addr < uae_p32(kickmem_bank.baseaddr + 8 * 65536));
#else
	return ((addr >= (uintptr)ROMBaseHost) && (addr < (uintptr)ROMBaseHost + ROMSize));
#endif
}

#if defined(UAE) || defined(FLIGHT_RECORDER)
static void flush_all(void)
{
	int i;

	log_flush();
	for (i=0;i<VREGS;i++)
		if (live.state[i].status==DIRTY) {
			if (!call_saved[live.state[i].realreg]) {
				tomem(i);
			}
		}
	for (i=0;i<VFREGS;i++)
		if (f_isinreg(i))
			f_evict(i);
	raw_fp_cleanup_drop();
}

/* Make sure all registers that will get clobbered by a call are
   save and sound in memory */
static void prepare_for_call_1(void)
{
	flush_all();  /* If there are registers that don't get clobbered,
		           * we should be a bit more selective here */
}

/* We will call a C routine in a moment. That will clobber all registers,
   so we need to disassociate everything */
static void prepare_for_call_2(void)
{
	int i;
	for (i=0;i<N_REGS;i++)
		if (!call_saved[i] && live.nat[i].nholds>0)
			free_nreg(i);

	for (i=0;i<N_FREGS;i++)
		if (live.fat[i].nholds>0)
			f_free_nreg(i);

	live.flags_in_flags=TRASH;  /* Note: We assume we already rescued the
								   flags at the very start of the call_r
								   functions! */
}
#endif

#if defined(CPU_arm)
#include "compemu_midfunc_arm.cpp"

#if defined(USE_JIT2)
#include "compemu_midfunc_arm2.cpp"
#endif
#endif

#if defined(CPU_i386) || defined(CPU_x86_64)
#include "compemu_midfunc_x86.cpp"
#endif


/********************************************************************
 * Support functions exposed to gencomp. CREATE time                *
 ********************************************************************/

void set_zero(int r, int tmp)
{
	if (setzflg_uses_bsf)
		bsf_l_rr(r,r);
	else
		simulate_bsf(tmp,r);
}

int kill_rodent(int r)
{
	return KILLTHERAT &&
		have_rat_stall &&
		(live.state[r].status==INMEM ||
		 live.state[r].status==CLEAN ||
		 live.state[r].status==ISCONST ||
		 live.state[r].dirtysize==4);
}

uae_u32 get_const(int r)
{
	Dif (!isconst(r)) {
		jit_abort("Register %d should be constant, but isn't",r);
	}
	return live.state[r].val;
}

void sync_m68k_pc(void)
{
	if (m68k_pc_offset) {
		add_l_ri(PC_P,m68k_pc_offset);
		comp_pc_p+=m68k_pc_offset;
		m68k_pc_offset=0;
	}
}

/* for building exception frames */
void compemu_exc_make_frame(int format, int sr, int ret, int nr, int tmp)
{
	lea_l_brr(SP_REG, SP_REG, -2);
	mov_l_ri(tmp, (format << 12) + (nr * 4));	/* format | vector */
	writeword(SP_REG, tmp, tmp);

	lea_l_brr(SP_REG, SP_REG, -4);
	writelong(SP_REG, ret, tmp);

	lea_l_brr(SP_REG, SP_REG, -2);
	writeword_clobber(SP_REG, sr, tmp);
	remove_offset(SP_REG, -1);
	if (isinreg(SP_REG))
		evict(SP_REG);
	else
		flush_reg(SP_REG);
}

void compemu_make_sr(int sr, int tmp)
{
	flush_flags(); /* low level */
	flush_reg(FLAGX);

#ifdef OPTIMIZED_FLAGS

	/*
	 * x86 EFLAGS: (!SAHF_SETO_PROFITABLE)
	 * FEDCBA98 76543210
     * ----V--- NZ-----C
     *
     * <--AH--> <--AL--> (SAHF_SETO_PROFITABLE)
     * FEDCBA98 76543210
     * NZxxxxxC xxxxxxxV
     *
     * arm RFLAGS:
     * FEDCBA98 76543210 FEDCBA98 76543210
     * NZCV---- -------- -------- --------
     *
     * -> m68k SR:
     * --S--III ---XNZVC
     *
     * Master-Bit and traceflags are ignored here,
     * since they are not emulated in JIT code
     */
	mov_l_rm(sr, uae_p32(live.state[FLAGTMP].mem));
	mov_l_ri(tmp, FLAGVAL_N|FLAGVAL_Z|FLAGVAL_V|FLAGVAL_C);
	and_l(sr, tmp);
	mov_l_rr(tmp, sr);

#if (defined(CPU_i386) && defined(X86_ASSEMBLY)) || (defined(CPU_x86_64) && defined(X86_64_ASSEMBLY))
#ifndef SAHF_SETO_PROFITABLE
	ror_b_ri(sr, FLAGBIT_N - 3);                             /* move NZ into position; C->4 */
	shrl_w_ri(tmp, FLAGBIT_V - 1);                           /* move V into position in tmp */
	or_l(sr, tmp);                                           /* or V flag to SR */
	mov_l_rr(tmp, sr);
	shrl_b_ri(tmp, (8 - (FLAGBIT_N - 3)) - FLAGBIT_C);       /* move C into position in tmp */
	or_l(sr, tmp);                                           /* or C flag to SR */
#else
	ror_w_ri(sr, FLAGBIT_N - 3);                             /* move NZ in position; V->4, C->12 */
	shrl_w_ri(tmp, (16 - (FLAGBIT_N - 3)) - FLAGBIT_V - 1);  /* move V into position in tmp; C->9 */
	or_l(sr, tmp);                                           /* or V flag to SR */
	shrl_w_ri(tmp, FLAGBIT_C + FLAGBIT_V - 1);               /* move C into position in tmp */
	or_l(sr, tmp);                                           /* or C flag to SR */
#endif
	mov_l_ri(tmp, 0x0f);
	and_l(sr, tmp);

	mov_b_rm(tmp, uae_p32(&regflags.x));
	and_l_ri(tmp, FLAGVAL_X);
	shll_l_ri(tmp, 4);
	or_l(sr, tmp);

#elif defined(CPU_arm) && defined(ARM_ASSEMBLY)
	shrl_l_ri(sr, FLAGBIT_N - 3);                            /* move NZ into position */
	ror_l_ri(tmp, FLAGBIT_C - 1);                            /* move C into position in tmp; V->31 */
	and_l_ri(sr, 0xc);
	or_l(sr, tmp);                                           /* or C flag to SR */
	shrl_l_ri(tmp, 31);                                      /* move V into position in tmp */
	or_l(sr, tmp);                                           /* or V flag to SR */

	mov_b_rm(tmp, uae_p32(&regflags.x));
	and_l_ri(tmp, FLAGVAL_X);
	shrl_l_ri(tmp, FLAGBIT_X - 4);
	or_l(sr, tmp);

#else
#error "unknown CPU"
#endif

#else

	xor_l(sr, sr);
	xor_l(tmp, tmp);
	mov_b_rm(tmp, uae_p32(&regs.c));
	shll_l_ri(tmp, 0);
	or_l(sr, tmp);
	mov_b_rm(tmp, uae_p32(&regs.v));
	shll_l_ri(tmp, 1);
	or_l(sr, tmp);
	mov_b_rm(tmp, uae_p32(&regs.z));
	shll_l_ri(tmp, 2);
	or_l(sr, tmp);
	mov_b_rm(tmp, uae_p32(&regs.n));
	shll_l_ri(tmp, 3);
	or_l(sr, tmp);

#endif /* OPTIMIZED_FLAGS */

	mov_b_rm(tmp, uae_p32(&regs.s));
	shll_l_ri(tmp, 13);
	or_l(sr, tmp);
	mov_l_rm(tmp, uae_p32(&regs.intmask));
	shll_l_ri(tmp, 8);
	or_l(sr, tmp);
	and_l_ri(sr, 0x271f);
	mov_w_mr(uae_p32(&regs.sr), sr);
}

void compemu_enter_super(int sr)
{
#if 0
	fprintf(stderr, "enter_super: isinreg=%d rr=%d nholds=%d\n", isinreg(SP_REG), live.state[SP_REG].realreg, isinreg(SP_REG) ? live.nat[live.state[SP_REG].realreg].nholds : -1);
#endif
	remove_offset(SP_REG, -1);
	if (isinreg(SP_REG))
		evict(SP_REG);
	else
		flush_reg(SP_REG);
	/*
	 * equivalent to:
	 * if (!regs.s)
	 * {
	 *	regs.usp = m68k_areg(regs, 7);
	 *	m68k_areg(regs, 7) = regs.isp;
	 *  regs.s = 1;
	 *  mmu_set_super(1);
	 * }
	 */
	test_l_ri(sr, 0x2000);
#if defined(CPU_i386) || defined(CPU_x86_64)
	compemu_raw_jnz_b_oponly();
	uae_u8 *branchadd = get_target();
	skip_byte();
#elif defined(CPU_arm)
	compemu_raw_jnz_b_oponly();
	uae_u8 *branchadd = get_target();
	skip_byte();
#endif
	mov_l_mr((uintptr)&regs.usp, SP_REG);
	mov_l_rm(SP_REG, uae_p32(&regs.isp));
	mov_b_mi(uae_p32(&regs.s), 1);
#if defined(CPU_i386) || defined(CPU_x86_64)
	*branchadd = get_target() - (branchadd + 1);
#elif defined(CPU_arm)
	*((uae_u32 *)branchadd - 3) = get_target() - (branchadd + 1);
#endif
}

/********************************************************************
 * Scratch registers management                                     *
 ********************************************************************/

struct scratch_t {
	uae_u32		regs[VREGS];
	fpu_register	fregs[VFREGS];
};

static scratch_t scratch;

/********************************************************************
 * Support functions exposed to newcpu                              *
 ********************************************************************/

static inline const char *str_on_off(bool b)
{
	return b ? "on" : "off";
}

#ifdef UAE
static
#endif
void compiler_init(void)
{
	static bool initialized = false;
	if (initialized)
		return;

#ifdef UAE
#else
#ifdef JIT_DEBUG
	// JIT debug mode ?
	JITDebug = PrefsFindBool("jitdebug");
#endif
	jit_log("<JIT compiler> : enable runtime disassemblers : %s", JITDebug ? "yes" : "no");

#ifdef USE_JIT_FPU
	// Use JIT compiler for FPU instructions ?
	avoid_fpu = !PrefsFindBool("jitfpu");
#else
	// JIT FPU is always disabled
	avoid_fpu = true;
#endif
	jit_log("<JIT compiler> : compile FPU instructions : %s", !avoid_fpu ? "yes" : "no");

	// Get size of the translation cache (in KB)
	cache_size = PrefsFindInt32("jitcachesize");
	jit_log("<JIT compiler> : requested translation cache size : %d KB", cache_size);

	setzflg_uses_bsf = target_check_bsf();
	jit_log("<JIT compiler> : target processor has CMOV instructions : %s", have_cmov ? "yes" : "no");
	jit_log("<JIT compiler> : target processor can suffer from partial register stalls : %s", have_rat_stall ? "yes" : "no");
	jit_log("<JIT compiler> : alignment for loops, jumps are %d, %d", align_loops, align_jumps);
#if defined(CPU_i386) || defined(CPU_x86_64)
	jit_log("<JIT compiler> : target processor has SSE2 instructions : %s", cpuinfo.x86_has_xmm2 ? "yes" : "no");
	jit_log("<JIT compiler> : cache linesize is %lu", (unsigned long)cpuinfo.x86_clflush_size);
#endif

	// Translation cache flush mechanism
	lazy_flush = PrefsFindBool("jitlazyflush");
	jit_log("<JIT compiler> : lazy translation cache invalidation : %s", str_on_off(lazy_flush));
	flush_icache = lazy_flush ? flush_icache_lazy : flush_icache_hard;

	// Compiler features
	jit_log("<JIT compiler> : register aliasing : %s", str_on_off(1));
	jit_log("<JIT compiler> : FP register aliasing : %s", str_on_off(USE_F_ALIAS));
	jit_log("<JIT compiler> : lazy constant offsetting : %s", str_on_off(USE_OFFSET));
#if USE_INLINING
	follow_const_jumps = PrefsFindBool("jitinline");
#endif
	jit_log("<JIT compiler> : block inlining : %s", str_on_off(follow_const_jumps));
	jit_log("<JIT compiler> : separate blockinfo allocation : %s", str_on_off(USE_SEPARATE_BIA));

	// Build compiler tables
	init_table68k ();
	build_comp();
#endif

	initialized = true;

#ifdef PROFILE_UNTRANSLATED_INSNS
	jit_log("<JIT compiler> : gather statistics on untranslated insns count");
#endif

#ifdef PROFILE_COMPILE_TIME
	jit_log("<JIT compiler> : gather statistics on translation time");
	emul_start_time = clock();
#endif
}

#ifdef UAE
static
#endif
void compiler_exit(void)
{
#ifdef PROFILE_COMPILE_TIME
	emul_end_time = clock();
#endif

#ifdef UAE
#else
#if DEBUG
#if defined(USE_DATA_BUFFER)
	jit_log("data_wasted = %ld bytes", data_wasted);
#endif
#endif

	// Deallocate translation cache
	if (compiled_code) {
		vm_release(compiled_code, cache_size * 1024);
		compiled_code = 0;
	}

	// Deallocate popallspace
	if (popallspace) {
		vm_release(popallspace, POPALLSPACE_SIZE);
		popallspace = 0;
	}
#endif

#ifdef PROFILE_COMPILE_TIME
	jit_log("### Compile Block statistics");
	jit_log("Number of calls to compile_block : %d", compile_count);
	uae_u32 emul_time = emul_end_time - emul_start_time;
	jit_log("Total emulation time   : %.1f sec", double(emul_time)/double(CLOCKS_PER_SEC));
	jit_log("Total compilation time : %.1f sec (%.1f%%)", double(compile_time)/double(CLOCKS_PER_SEC), 100.0*double(compile_time)/double(emul_time));
#endif

#ifdef PROFILE_UNTRANSLATED_INSNS
	uae_u64 untranslated_count = 0;
	for (int i = 0; i < 65536; i++) {
		opcode_nums[i] = i;
		untranslated_count += raw_cputbl_count[i];
	}
	bug("Sorting out untranslated instructions count...");
	qsort(opcode_nums, 65536, sizeof(uae_u16), untranslated_compfn);
	jit_log("Rank  Opc      Count Name");
	for (int i = 0; i < untranslated_top_ten; i++) {
		uae_u32 count = raw_cputbl_count[opcode_nums[i]];
		struct instr *dp;
		struct mnemolookup *lookup;
		if (!count)
			break;
		dp = table68k + opcode_nums[i];
		for (lookup = lookuptab; lookup->mnemo != (instrmnem)dp->mnemo; lookup++)
			;
		bug("%03d: %04x %10u %s", i, opcode_nums[i], count, lookup->name);
	}
#endif

#ifdef RECORD_REGISTER_USAGE
	int reg_count_ids[16];
	uint64 tot_reg_count = 0;
	for (int i = 0; i < 16; i++) {
		reg_count_ids[i] = i;
		tot_reg_count += reg_count[i];
	}
	qsort(reg_count_ids, 16, sizeof(int), reg_count_compare);
	uint64 cum_reg_count = 0;
	for (int i = 0; i < 16; i++) {
		int r = reg_count_ids[i];
		cum_reg_count += reg_count[r];
		jit_log("%c%d : %16ld %2.1f%% [%2.1f]", r < 8 ? 'D' : 'A', r % 8,
		   reg_count[r],
		   100.0*double(reg_count[r])/double(tot_reg_count),
		   100.0*double(cum_reg_count)/double(tot_reg_count));
	}
#endif

	// exit_table68k();
}

#ifdef UAE
#else
bool compiler_use_jit(void)
{
	// Check for the "jit" prefs item
	if (!PrefsFindBool("jit"))
		return false;
	
	// Don't use JIT if translation cache size is less then MIN_CACHE_SIZE KB
	if (PrefsFindInt32("jitcachesize") < MIN_CACHE_SIZE) {
		write_log("<JIT compiler> : translation cache size is less than %d KB. Disabling JIT.\n", MIN_CACHE_SIZE);
		return false;
	}
	
	return true;
}
#endif

static void init_comp(void)
{
	int i;
	uae_s8* cb=can_byte;
	uae_s8* cw=can_word;
	uae_s8* au=always_used;

#ifdef RECORD_REGISTER_USAGE
	for (i=0;i<16;i++)
		reg_count_local[i] = 0;
#endif

	for (i=0;i<VREGS;i++) {
		live.state[i].realreg=-1;
		live.state[i].needflush=NF_SCRATCH;
		live.state[i].val=0;
		set_status(i,UNDEF);
	}

	for (i=0;i<VFREGS;i++) {
		live.fate[i].status=UNDEF;
		live.fate[i].realreg=-1;
		live.fate[i].needflush=NF_SCRATCH;
	}

	for (i=0;i<VREGS;i++) {
		if (i<16) { /* First 16 registers map to 68k registers */
			live.state[i].mem=&regs.regs[i];
			live.state[i].needflush=NF_TOMEM;
			set_status(i,INMEM);
		}
		else
			live.state[i].mem=scratch.regs+i;
	}
	live.state[PC_P].mem=(uae_u32*)&(regs.pc_p);
	live.state[PC_P].needflush=NF_TOMEM;
	set_const(PC_P,(uintptr)comp_pc_p);

	live.state[FLAGX].mem=(uae_u32*)&(regflags.x);
	live.state[FLAGX].needflush=NF_TOMEM;
	set_status(FLAGX,INMEM);

#if defined(CPU_arm)
	live.state[FLAGTMP].mem=(uae_u32*)&(regflags.nzcv);
#else
	live.state[FLAGTMP].mem=(uae_u32*)&(regflags.cznv);
#endif
	live.state[FLAGTMP].needflush=NF_TOMEM;
	set_status(FLAGTMP,INMEM);

	live.state[NEXT_HANDLER].needflush=NF_HANDLER;
	set_status(NEXT_HANDLER,UNDEF);

	for (i=0;i<VFREGS;i++) {
		if (i<8) { /* First 8 registers map to 68k FPU registers */
#ifdef UAE
			live.fate[i].mem=(uae_u32*)(&regs.fp[i].fp);
#else
			live.fate[i].mem=(uae_u32*)fpu_register_address(i);
#endif
			live.fate[i].needflush=NF_TOMEM;
			live.fate[i].status=INMEM;
		}
		else if (i==FP_RESULT) {
#ifdef UAE
			live.fate[i].mem=(uae_u32*)(&regs.fp_result);
#else
			live.fate[i].mem=(uae_u32*)(&fpu.result);
#endif
			live.fate[i].needflush=NF_TOMEM;
			live.fate[i].status=INMEM;
		}
		else
			live.fate[i].mem=(uae_u32*)(&scratch.fregs[i]);
	}


	for (i=0;i<N_REGS;i++) {
		live.nat[i].touched=0;
		live.nat[i].nholds=0;
		live.nat[i].locked=0;
		if (*cb==i) {
			live.nat[i].canbyte=1; cb++;
		} else live.nat[i].canbyte=0;
		if (*cw==i) {
			live.nat[i].canword=1; cw++;
		} else live.nat[i].canword=0;
		if (*au==i) {
			live.nat[i].locked=1; au++;
		}
	}

	for (i=0;i<N_FREGS;i++) {
		live.fat[i].touched=0;
		live.fat[i].nholds=0;
		live.fat[i].locked=0;
	}

	touchcnt=1;
	m68k_pc_offset=0;
	live.flags_in_flags=TRASH;
	live.flags_on_stack=VALID;
	live.flags_are_important=1;

	raw_fp_init();
}

void flush_reg(int reg)
{
	if (live.state[reg].needflush==NF_TOMEM)
	{
		switch (live.state[reg].status)
		{
		case INMEM:
			if (live.state[reg].val)
			{
				compemu_raw_add_l_mi((uintptr)live.state[reg].mem, live.state[reg].val);
				log_vwrite(reg);
				live.state[reg].val = 0;
			}
			break;
		case CLEAN:
		case DIRTY:
			remove_offset(reg, -1);
			tomem(reg);
			break;
		case ISCONST:
			if (reg != PC_P)
				writeback_const(reg);
			break;
		default:
			break;
		}
		Dif (live.state[reg].val && reg!=PC_P)
		{
			jit_log("Register %d still has val %x", reg, live.state[reg].val);
		}
	}
}

/* Only do this if you really mean it! The next call should be to init!*/
void flush(int save_regs)
{
	int i;

	log_flush();
	flush_flags(); /* low level */
	sync_m68k_pc(); /* mid level */

	if (save_regs) {
		for (i=0;i<VFREGS;i++) {
			if (live.fate[i].needflush==NF_SCRATCH ||
				live.fate[i].status==CLEAN) {
				f_disassociate(i);
			}
		}
		for (i=0;i<VREGS;i++) {
			flush_reg(i);
		}
		for (i=0;i<VFREGS;i++) {
			if (live.fate[i].needflush==NF_TOMEM &&
				live.fate[i].status==DIRTY) {
				f_evict(i);
			}
		}
		raw_fp_cleanup_drop();
	}
	if (needflags) {
		jit_log("Warning! flush with needflags=1!");
	}
}

#if 0
static void flush_keepflags(void)
{
	int i;

	for (i=0;i<VFREGS;i++) {
		if (live.fate[i].needflush==NF_SCRATCH ||
			live.fate[i].status==CLEAN) {
			f_disassociate(i);
		}
	}
	for (i=0;i<VREGS;i++) {
		if (live.state[i].needflush==NF_TOMEM) {
			switch(live.state[i].status) {
			case INMEM:
				/* Can't adjust the offset here --- that needs "add" */
				break;
			case CLEAN:
			case DIRTY:
				remove_offset(i,-1);
				tomem(i);
				break;
			case ISCONST:
				if (i!=PC_P)
					writeback_const(i);
				break;
			default: break;
			}
		}
	}
	for (i=0;i<VFREGS;i++) {
		if (live.fate[i].needflush==NF_TOMEM &&
			live.fate[i].status==DIRTY) {
			f_evict(i);
		}
	}
	raw_fp_cleanup_drop();
}
#endif

static void freescratch(void)
{
	int i;
	for (i=0;i<N_REGS;i++)
#if defined(CPU_arm)
		if (live.nat[i].locked && i != REG_WORK1 && i != REG_WORK2)
#else
		if (live.nat[i].locked && i != ESP_INDEX
#if defined(UAE) && defined(CPU_x86_64)
			&& i != R12_INDEX
#endif
			)
#endif
		{
			jit_log("Warning! %d is locked",i);
		}

	for (i=0;i<VREGS;i++)
		if (live.state[i].needflush==NF_SCRATCH) {
			forget_about(i);
		}

	for (i=0;i<VFREGS;i++)
		if (live.fate[i].needflush==NF_SCRATCH) {
			f_forget_about(i);
		}
}

/********************************************************************
 * Memory access and related functions, CREATE time                 *
 ********************************************************************/

void register_branch(uae_u32 not_taken, uae_u32 taken, uae_u8 cond)
{
	next_pc_p=not_taken;
	taken_pc_p=taken;
	branch_cc=cond;
}

/* Note: get_handler may fail in 64 Bit environments, if direct_handler_to_use is
 * outside 32 bit
 */
static uintptr get_handler(uintptr addr)
{
	blockinfo* bi=get_blockinfo_addr_new((void*)(uintptr)addr,0);
	return (uintptr)bi->direct_handler_to_use;
}

/* This version assumes that it is writing *real* memory, and *will* fail
 * if that assumption is wrong! No branches, no second chances, just
 * straight go-for-it attitude */

static void writemem_real(int address, int source, int size, int tmp, int clobber)
{
	int f=tmp;

#ifdef NATMEM_OFFSET
	if (canbang) {  /* Woohoo! go directly at the memory! */
		if (clobber)
			f=source;

		switch(size) {
			case 1: mov_b_bRr(address,source,MEMBaseDiff); break;
			case 2: mov_w_rr(f,source); mid_bswap_16(f); mov_w_bRr(address,f,MEMBaseDiff); break;
			case 4: mov_l_rr(f,source); mid_bswap_32(f); mov_l_bRr(address,f,MEMBaseDiff); break;
		}
		forget_about(tmp);
		forget_about(f);
		return;
	}
#endif

#ifdef UAE
	mov_l_rr(f,address);
	shrl_l_ri(f,16);  /* The index into the baseaddr table */
	mov_l_rm_indexed(f,uae_p32(baseaddr),f,SIZEOF_VOID_P); /* FIXME: is SIZEOF_VOID_P correct? */

	if (address==source) { /* IBrowse does this! */
		if (size > 1) {
			add_l(f,address); /* f now holds the final address */
			switch (size) {
			case 2: mid_bswap_16(source); mov_w_Rr(f,source,0);
				mid_bswap_16(source); return;
			case 4: mid_bswap_32(source); mov_l_Rr(f,source,0);
				mid_bswap_32(source); return;
			}
		}
	}
	switch (size) { /* f now holds the offset */
	case 1: mov_b_mrr_indexed(address,f,1,source); break;
	case 2: mid_bswap_16(source); mov_w_mrr_indexed(address,f,1,source);
		mid_bswap_16(source); break;	   /* base, index, source */
	case 4: mid_bswap_32(source); mov_l_mrr_indexed(address,f,1,source);
		mid_bswap_32(source); break;
	}
#endif
}

#ifdef UAE
static inline void writemem(int address, int source, int offset, int size, int tmp)
{
	int f=tmp;

	mov_l_rr(f,address);
	shrl_l_ri(f,16);   /* The index into the mem bank table */
	mov_l_rm_indexed(f,uae_p32(mem_banks),f,SIZEOF_VOID_P); /* FIXME: is SIZEOF_VOID_P correct? */
	/* Now f holds a pointer to the actual membank */
	mov_l_rR(f,f,offset);
	/* Now f holds the address of the b/w/lput function */
	call_r_02(f,address,source,4,size);
	forget_about(tmp);
}
#endif

void writebyte(int address, int source, int tmp)
{
#ifdef UAE
	if ((special_mem & S_WRITE) || distrust_byte())
		writemem_special(address, source, 5 * SIZEOF_VOID_P, 1, tmp);
	else
#endif
		writemem_real(address,source,1,tmp,0);
}

static inline void writeword_general(int address, int source, int tmp,
	int clobber)
{
#ifdef UAE
	if ((special_mem & S_WRITE) || distrust_word())
		writemem_special(address, source, 4 * SIZEOF_VOID_P, 2, tmp);
	else
#endif
		writemem_real(address,source,2,tmp,clobber);
}

void writeword_clobber(int address, int source, int tmp)
{
	writeword_general(address,source,tmp,1);
}

void writeword(int address, int source, int tmp)
{
	writeword_general(address,source,tmp,0);
}

static inline void writelong_general(int address, int source, int tmp,
	int clobber)
{
#ifdef UAE
	if ((special_mem & S_WRITE) || distrust_long())
		writemem_special(address, source, 3 * SIZEOF_VOID_P, 4, tmp);
	else
#endif
		writemem_real(address,source,4,tmp,clobber);
}

void writelong_clobber(int address, int source, int tmp)
{
	writelong_general(address,source,tmp,1);
}

void writelong(int address, int source, int tmp)
{
	writelong_general(address,source,tmp,0);
}



/* This version assumes that it is reading *real* memory, and *will* fail
 * if that assumption is wrong! No branches, no second chances, just
 * straight go-for-it attitude */

static void readmem_real(int address, int dest, int size, int tmp)
{
	int f=tmp;

	if (size==4 && address!=dest)
		f=dest;

#ifdef NATMEM_OFFSET
	if (canbang) {  /* Woohoo! go directly at the memory! */
		switch(size) {
			case 1: mov_b_brR(dest,address,MEMBaseDiff); break;
			case 2: mov_w_brR(dest,address,MEMBaseDiff); mid_bswap_16(dest); break;
			case 4: mov_l_brR(dest,address,MEMBaseDiff); mid_bswap_32(dest); break;
		}
		forget_about(tmp);
		(void) f;
		return;
	}
#endif

#ifdef UAE
	mov_l_rr(f,address);
	shrl_l_ri(f,16);   /* The index into the baseaddr table */
	mov_l_rm_indexed(f,uae_p32(baseaddr),f,SIZEOF_VOID_P); /* FIXME: is SIZEOF_VOID_P correct? */
	/* f now holds the offset */

	switch(size) {
	case 1: mov_b_rrm_indexed(dest,address,f,1); break;
	case 2: mov_w_rrm_indexed(dest,address,f,1); mid_bswap_16(dest); break;
	case 4: mov_l_rrm_indexed(dest,address,f,1); mid_bswap_32(dest); break;
	}
	forget_about(tmp);
#endif
}



#ifdef UAE
static inline void readmem(int address, int dest, int offset, int size, int tmp)
{
	int f=tmp;

	mov_l_rr(f,address);
	shrl_l_ri(f,16);   /* The index into the mem bank table */
	mov_l_rm_indexed(f,uae_p32(mem_banks),f,SIZEOF_VOID_P); /* FIXME: is SIZEOF_VOID_P correct? */
	/* Now f holds a pointer to the actual membank */
	mov_l_rR(f,f,offset);
	/* Now f holds the address of the b/w/lget function */
	call_r_11(dest,f,address,size,4);
	forget_about(tmp);
}
#endif

void readbyte(int address, int dest, int tmp)
{
#ifdef UAE
	if ((special_mem & S_READ) || distrust_byte())
		readmem_special(address, dest, 2 * SIZEOF_VOID_P, 1, tmp);
	else
#endif
		readmem_real(address,dest,1,tmp);
}

void readword(int address, int dest, int tmp)
{
#ifdef UAE
	if ((special_mem & S_READ) || distrust_word())
		readmem_special(address, dest, 1 * SIZEOF_VOID_P, 2, tmp);
	else
#endif
		readmem_real(address,dest,2,tmp);
}

void readlong(int address, int dest, int tmp)
{
#ifdef UAE
	if ((special_mem & S_READ) || distrust_long())
		readmem_special(address, dest, 0 * SIZEOF_VOID_P, 4, tmp);
	else
#endif
		readmem_real(address,dest,4,tmp);
}

void get_n_addr(int address, int dest, int tmp)
{
#ifdef UAE
	if (special_mem || distrust_addr()) {
		/* This one might appear a bit odd... */
		readmem(address, dest, 6 * SIZEOF_VOID_P, 4, tmp);
		return;
	}
#endif

	// a is the register containing the virtual address
	// after the offset had been fetched
	int a=tmp;

	// f is the register that will contain the offset
	int f=tmp;

	// a == f == tmp if (address == dest)
	if (address!=dest) {
		a=address;
		f=dest;
	}

#ifdef NATMEM_OFFSET
	if (canbang) {
//#if FIXED_ADDRESSING
		lea_l_brr(dest,address,MEMBaseDiff);
//#else
//# error "Only fixed adressing mode supported"
//#endif
		forget_about(tmp);
		(void) f;
		(void) a;
		return;
	}
#endif

#ifdef UAE
	mov_l_rr(f,address);
	mov_l_rr(dest,address); // gb-- nop if dest==address
	shrl_l_ri(f,16);
	mov_l_rm_indexed(f,uae_p32(baseaddr),f,SIZEOF_VOID_P); /* FIXME: is SIZEOF_VOID_P correct? */
	add_l(dest,f);
	forget_about(tmp);
#endif
}

void get_n_addr_jmp(int address, int dest, int tmp)
{
#ifdef WINUAE_ARANYM
	/* For this, we need to get the same address as the rest of UAE
	 would --- otherwise we end up translating everything twice */
	get_n_addr(address,dest,tmp);
#else
	int f=tmp;
	if (address!=dest)
		f=dest;
	mov_l_rr(f,address);
	shrl_l_ri(f,16);   /* The index into the baseaddr bank table */
	mov_l_rm_indexed(dest,uae_p32(baseaddr),f,SIZEOF_VOID_P); /* FIXME: is SIZEOF_VOID_P correct? */
	add_l(dest,address);
	and_l_ri (dest, ~1);
	forget_about(tmp);
#endif
}


/* base is a register, but dp is an actual value. 
   target is a register, as is tmp */
void calc_disp_ea_020(int base, uae_u32 dp, int target, int tmp)
{
	int reg = (dp >> 12) & 15;
	int regd_shift=(dp >> 9) & 3;

	if (dp & 0x100) {
		int ignorebase=(dp&0x80);
		int ignorereg=(dp&0x40);
		int addbase=0;
		int outer=0;

		if ((dp & 0x30) == 0x20) addbase = (uae_s32)(uae_s16)comp_get_iword((m68k_pc_offset+=2)-2);
		if ((dp & 0x30) == 0x30) addbase = comp_get_ilong((m68k_pc_offset+=4)-4);

		if ((dp & 0x3) == 0x2) outer = (uae_s32)(uae_s16)comp_get_iword((m68k_pc_offset+=2)-2);
		if ((dp & 0x3) == 0x3) outer = comp_get_ilong((m68k_pc_offset+=4)-4);

		if ((dp & 0x4) == 0) {  /* add regd *before* the get_long */
			if (!ignorereg) {
				if ((dp & 0x800) == 0)
					sign_extend_16_rr(target,reg);
				else
					mov_l_rr(target,reg);
				shll_l_ri(target,regd_shift);
			}
			else
				mov_l_ri(target,0);

			/* target is now regd */
			if (!ignorebase)
				add_l(target,base);
			add_l_ri(target,addbase);
			if (dp&0x03) readlong(target,target,tmp);
		} else { /* do the getlong first, then add regd */
			if (!ignorebase) {
				mov_l_rr(target,base);
				add_l_ri(target,addbase);
			}
			else
				mov_l_ri(target,addbase);
			if (dp&0x03) readlong(target,target,tmp);

			if (!ignorereg) {
				if ((dp & 0x800) == 0)
					sign_extend_16_rr(tmp,reg);
				else
					mov_l_rr(tmp,reg);
				shll_l_ri(tmp,regd_shift);
				/* tmp is now regd */
				add_l(target,tmp);
			}
		}
		add_l_ri(target,outer);
	}
	else { /* 68000 version */
		if ((dp & 0x800) == 0) { /* Sign extend */
			sign_extend_16_rr(target,reg);
			lea_l_brr_indexed(target,base,target,1<<regd_shift,(uae_s32)((uae_s8)dp));
		}
		else {
			lea_l_brr_indexed(target,base,reg,1<<regd_shift,(uae_s32)((uae_s8)dp));
		}
	}
	forget_about(tmp);
}





void set_cache_state(int enabled)
{
	if (enabled!=cache_enabled)
		flush_icache_hard();
	cache_enabled=enabled;
}

int get_cache_state(void)
{
	return cache_enabled;
}

uae_u32 get_jitted_size(void)
{
	if (compiled_code)
		return current_compile_p-compiled_code;
	return 0;
}

static uint8 *do_alloc_code(uint32 size, int depth)
{
	UNUSED(depth);
	uint8 *code = (uint8 *)vm_acquire(size, VM_MAP_DEFAULT | VM_MAP_32BIT);
	return code == VM_MAP_FAILED ? NULL : code;
}

static inline uint8 *alloc_code(uint32 size)
{
	uint8 *ptr = do_alloc_code(size, 0);
	/* allocated code must fit in 32-bit boundaries */
	assert((uintptr)ptr <= 0xffffffff);
	return ptr;
}

void alloc_cache(void)
{
	if (compiled_code) {
		flush_icache_hard();
		vm_release(compiled_code, cache_size * 1024);
		compiled_code = 0;
	}

#ifdef UAE
	cache_size = currprefs.cachesize;
#endif
	if (cache_size == 0)
		return;

	while (!compiled_code && cache_size) {
		if ((compiled_code = alloc_code(cache_size * 1024)) == NULL) {
			compiled_code = 0;
			cache_size /= 2;
		}
	}
	vm_protect(compiled_code, cache_size * 1024, VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_EXECUTE);
	
	if (compiled_code) {
		jit_log("<JIT compiler> : actual translation cache size : %d KB at %p-%p", cache_size, compiled_code, compiled_code + cache_size*1024);
#ifdef USE_DATA_BUFFER
		max_compile_start = compiled_code + cache_size*1024 - BYTES_PER_INST - DATA_BUFFER_SIZE;
#else
		max_compile_start = compiled_code + cache_size*1024 - BYTES_PER_INST;
#endif
		current_compile_p = compiled_code;
		current_cache_size = 0;
#if defined(USE_DATA_BUFFER)
		reset_data_buffer();
#endif
	}
}

extern void op_illg_1 (uae_u32 opcode) REGPARAM;

static void calc_checksum(blockinfo* bi, uae_u32* c1, uae_u32* c2)
{
	uae_u32 k1 = 0;
	uae_u32 k2 = 0;

#if USE_CHECKSUM_INFO
	checksum_info *csi = bi->csi;
	Dif(!csi) abort();
	while (csi) {
		uae_s32 len = csi->length;
		uintptr tmp = (uintptr)csi->start_p;
#else
		uae_s32 len = bi->len;
		uintptr tmp = (uintptr)bi->min_pcp;
#endif
		uae_u32* pos;

		len += (tmp & 3);
		tmp &= ~((uintptr)3);
		pos = (uae_u32 *)tmp;

		if (len >= 0 && len <= MAX_CHECKSUM_LEN) {
			while (len > 0) {
				k1 += *pos;
				k2 ^= *pos;
				pos++;
				len -= 4;
			}
		}

#if USE_CHECKSUM_INFO
		csi = csi->next;
	}
#endif

	*c1 = k1;
	*c2 = k2;
}

#if 0
static void show_checksum(CSI_TYPE* csi)
{
	uae_u32 k1=0;
	uae_u32 k2=0;
	uae_s32 len=CSI_LENGTH(csi);
	uae_u32 tmp=(uintptr)CSI_START_P(csi);
	uae_u32* pos;

	len+=(tmp&3);
	tmp&=(~3);
	pos=(uae_u32*)tmp;

	if (len<0 || len>MAX_CHECKSUM_LEN) {
		return;
	}
	else {
		while (len>0) {
			jit_log("%08x ",*pos);
			pos++;
			len-=4;
		}
		jit_log(" bla");
	}
}
#endif


int check_for_cache_miss(void)
{
	blockinfo* bi=get_blockinfo_addr(regs.pc_p);

	if (bi) {
		int cl=cacheline(regs.pc_p);
		if (bi!=cache_tags[cl+1].bi) {
			raise_in_cl_list(bi);
			return 1;
		}
	}
	return 0;
}


static void recompile_block(void)
{
	/* An existing block's countdown code has expired. We need to make
	   sure that execute_normal doesn't refuse to recompile due to a
	   perceived cache miss... */
	blockinfo*  bi=get_blockinfo_addr(regs.pc_p);

	Dif (!bi)
		jit_abort("recompile_block");
	raise_in_cl_list(bi);
	execute_normal();
	return;
}
static void cache_miss(void)
{
	blockinfo*  bi=get_blockinfo_addr(regs.pc_p);
#if COMP_DEBUG
	uae_u32     cl=cacheline(regs.pc_p);
	blockinfo*  bi2=get_blockinfo(cl);
#endif

	if (!bi) {
		execute_normal(); /* Compile this block now */
		return;
	}
	Dif (!bi2 || bi==bi2) {
		jit_abort("Unexplained cache miss %p %p",bi,bi2);
	}
	raise_in_cl_list(bi);
	return;
}

static int called_check_checksum(blockinfo* bi);

static inline int block_check_checksum(blockinfo* bi)
{
	uae_u32     c1,c2;
	bool        isgood;

	if (bi->status!=BI_NEED_CHECK)
		return 1;  /* This block is in a checked state */

	if (bi->c1 || bi->c2)
		calc_checksum(bi,&c1,&c2);
	else {
		c1=c2=1;  /* Make sure it doesn't match */
	}

	isgood=(c1==bi->c1 && c2==bi->c2);

	if (isgood) {
		/* This block is still OK. So we reactivate. Of course, that
		   means we have to move it into the needs-to-be-flushed list */
		bi->handler_to_use=bi->handler;
		set_dhtu(bi,bi->direct_handler);
		bi->status=BI_CHECKING;
		isgood=called_check_checksum(bi) != 0;
	}
	if (isgood) {
		jit_log2("reactivate %p/%p (%x %x/%x %x)",bi,bi->pc_p, c1,c2,bi->c1,bi->c2);
		remove_from_list(bi);
		add_to_active(bi);
		raise_in_cl_list(bi);
		bi->status=BI_ACTIVE;
	}
	else {
		/* This block actually changed. We need to invalidate it,
		   and set it up to be recompiled */
		jit_log2("discard %p/%p (%x %x/%x %x)",bi,bi->pc_p, c1,c2,bi->c1,bi->c2);
		invalidate_block(bi);
		raise_in_cl_list(bi);
	}
	return isgood;
}

static int called_check_checksum(blockinfo* bi)
{
	int isgood=1;
	int i;

	for (i=0;i<2 && isgood;i++) {
		if (bi->dep[i].jmp_off) {
			isgood=block_check_checksum(bi->dep[i].target);
		}
	}
	return isgood;
}

static void check_checksum(void)
{
	blockinfo*  bi=get_blockinfo_addr(regs.pc_p);
	uae_u32     cl=cacheline(regs.pc_p);
	blockinfo*  bi2=get_blockinfo(cl);

	/* These are not the droids you are looking for... */
	if (!bi) {
		/* Whoever is the primary target is in a dormant state, but
		   calling it was accidental, and we should just compile this
		   new block */
		execute_normal();
		return;
	}
	if (bi!=bi2) {
		/* The block was hit accidentally, but it does exist. Cache miss */
		cache_miss();
		return;
	}

	if (!block_check_checksum(bi))
		execute_normal();
}

static inline void match_states(blockinfo* bi)
{
	int i;
	smallstate* s=&(bi->env);

	if (bi->status==BI_NEED_CHECK) {
		block_check_checksum(bi);
	}
	if (bi->status==BI_ACTIVE ||
		bi->status==BI_FINALIZING) {  /* Deal with the *promises* the
						 block makes (about not using
						 certain vregs) */
		for (i=0;i<16;i++) {
			if (s->virt[i]==L_UNNEEDED) {
				jit_log2("unneeded reg %d at %p",i,target);
				COMPCALL(forget_about)(i); // FIXME
			}
		}
	}
	flush(1);

	/* And now deal with the *demands* the block makes */
	for (i=0;i<N_REGS;i++) {
		int v=s->nat[i];
		if (v>=0) {
			// printf("Loading reg %d into %d at %p\n",v,i,target);
			readreg_specific(v,4,i);
			// do_load_reg(i,v);
			// setlock(i);
		}
	}
	for (i=0;i<N_REGS;i++) {
		int v=s->nat[i];
		if (v>=0) {
			unlock2(i);
		}
	}
}

static inline void create_popalls(void)
{
	int i,r;

	if (popallspace == NULL) {
		if ((popallspace = alloc_code(POPALLSPACE_SIZE)) == NULL) {
			jit_log("WARNING: Could not allocate popallspace!");
#ifdef UAE
			if (currprefs.cachesize > 0)
#endif
			{
				jit_abort("Could not allocate popallspace!");
			}
#ifdef UAE
			/* This is not fatal if JIT is not used. If JIT is
			 * turned on, it will crash, but it would have crashed
			 * anyway. */
			return;
#endif
		}
	}
	vm_protect(popallspace, POPALLSPACE_SIZE, VM_PAGE_READ | VM_PAGE_WRITE);

	int stack_space = STACK_OFFSET;
	for (i=0;i<N_REGS;i++) {
		if (need_to_preserve[i])
			stack_space += sizeof(void *);
	}
	stack_space %= STACK_ALIGN;
	if (stack_space)
		stack_space = STACK_ALIGN - stack_space;

	current_compile_p=popallspace;
	set_target(current_compile_p);

#if defined(USE_DATA_BUFFER)
	reset_data_buffer();
#endif

	/* We need to guarantee 16-byte stack alignment on x86 at any point
	   within the JIT generated code. We have multiple exit points
	   possible but a single entry. A "jmp" is used so that we don't
	   have to generate stack alignment in generated code that has to
	   call external functions (e.g. a generic instruction handler).

	   In summary, JIT generated code is not leaf so we have to deal
	   with it here to maintain correct stack alignment. */
	align_target(align_jumps);
	current_compile_p=get_target();
	pushall_call_handler=get_target();
	raw_push_regs_to_preserve();
	raw_dec_sp(stack_space);
	r=REG_PC_TMP;
	compemu_raw_mov_l_rm(r, uae_p32(&regs.pc_p));
	compemu_raw_and_l_ri(r,TAGMASK);
	{
		assert(sizeof(cache_tags[0]) == sizeof(void *));
		// verify(sizeof(cache_tags[0]) == sizeof(void *));
	}
	compemu_raw_jmp_m_indexed(uae_p32(cache_tags), r, sizeof(void *));

	/* now the exit points */
	align_target(align_jumps);
	popall_do_nothing=get_target();
	raw_inc_sp(stack_space);
	raw_pop_preserved_regs();
	compemu_raw_jmp(uae_p32(do_nothing));

	align_target(align_jumps);
	popall_execute_normal=get_target();
	raw_inc_sp(stack_space);
	raw_pop_preserved_regs();
	compemu_raw_jmp(uae_p32(execute_normal));

	align_target(align_jumps);
	popall_cache_miss=get_target();
	raw_inc_sp(stack_space);
	raw_pop_preserved_regs();
	compemu_raw_jmp(uae_p32(cache_miss));

	align_target(align_jumps);
	popall_recompile_block=get_target();
	raw_inc_sp(stack_space);
	raw_pop_preserved_regs();
	compemu_raw_jmp(uae_p32(recompile_block));

	align_target(align_jumps);
	popall_exec_nostats=get_target();
	raw_inc_sp(stack_space);
	raw_pop_preserved_regs();
	compemu_raw_jmp(uae_p32(exec_nostats));

	align_target(align_jumps);
	popall_check_checksum=get_target();
	raw_inc_sp(stack_space);
	raw_pop_preserved_regs();
	compemu_raw_jmp(uae_p32(check_checksum));

#if defined(USE_DATA_BUFFER)
	reset_data_buffer();
#endif

#ifdef UAE
#ifdef USE_UDIS86
	UDISFN(pushall_call_handler, get_target());
#endif
#endif
	// no need to further write into popallspace
	vm_protect(popallspace, POPALLSPACE_SIZE, VM_PAGE_READ | VM_PAGE_EXECUTE);
	// No need to flush. Initialized and not modified
	// flush_cpu_icache((void *)popallspace, (void *)target);
}

static inline void reset_lists(void)
{
	int i;

	for (i=0;i<MAX_HOLD_BI;i++)
		hold_bi[i]=NULL;
	active=NULL;
	dormant=NULL;
}

static void prepare_block(blockinfo* bi)
{
	int i;

	set_target(current_compile_p);
	align_target(align_jumps);
	bi->direct_pen=(cpuop_func*)get_target();
	compemu_raw_mov_l_rm(0,(uintptr)&(bi->pc_p));
	compemu_raw_mov_l_mr((uintptr)&regs.pc_p,0);
	compemu_raw_jmp((uintptr)popall_execute_normal);

	align_target(align_jumps);
	bi->direct_pcc=(cpuop_func*)get_target();
	compemu_raw_mov_l_rm(0,(uintptr)&(bi->pc_p));
	compemu_raw_mov_l_mr((uintptr)&regs.pc_p,0);
	compemu_raw_jmp((uintptr)popall_check_checksum);
	flush_cpu_icache((void *)current_compile_p, (void *)target);
	current_compile_p=get_target();

	bi->deplist=NULL;
	for (i=0;i<2;i++) {
		bi->dep[i].prev_p=NULL;
		bi->dep[i].next=NULL;
	}
	bi->env=default_ss;
	bi->status=BI_INVALID;
	bi->havestate=0;
	//bi->env=empty_ss;
}

#ifdef UAE
void compemu_reset(void)
{
	set_cache_state(0);
}
#endif

#ifdef UAE
#else
// OPCODE is in big endian format, use cft_map() beforehand, if needed.
#endif
static inline void reset_compop(int opcode)
{
	compfunctbl[opcode] = NULL;
	nfcompfunctbl[opcode] = NULL;
}

static int read_opcode(const char *p)
{
	int opcode = 0;
	for (int i = 0; i < 4; i++) {
		int op = p[i];
		switch (op) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			opcode = (opcode << 4) | (op - '0');
			break;
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
			opcode = (opcode << 4) | ((op - 'a') + 10);
			break;
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
			opcode = (opcode << 4) | ((op - 'A') + 10);
			break;
		default:
			return -1;
		}
	}
	return opcode;
}


#ifdef USE_JIT_FPU
static struct {
	const char *name;
	bool *const disabled;
} const jit_opcodes[] = {
	{ "fbcc", &jit_disable.fbcc },
	{ "fdbcc", &jit_disable.fdbcc },
	{ "fscc", &jit_disable.fscc },
	{ "ftrapcc", &jit_disable.ftrapcc },
	{ "fsave", &jit_disable.fsave },
	{ "frestore", &jit_disable.frestore },
	{ "fmove", &jit_disable.fmove },
	{ "fmovec", &jit_disable.fmovec },
	{ "fmovem", &jit_disable.fmovem },
	{ "fmovecr", &jit_disable.fmovecr },
	{ "fint", &jit_disable.fint },
	{ "fsinh", &jit_disable.fsinh },
	{ "fintrz", &jit_disable.fintrz },
	{ "fsqrt", &jit_disable.fsqrt },
	{ "flognp1", &jit_disable.flognp1 },
	{ "fetoxm1", &jit_disable.fetoxm1 },
	{ "ftanh", &jit_disable.ftanh },
	{ "fatan", &jit_disable.fatan },
	{ "fasin", &jit_disable.fasin },
	{ "fatanh", &jit_disable.fatanh },
	{ "fsin", &jit_disable.fsin },
	{ "ftan", &jit_disable.ftan },
	{ "fetox", &jit_disable.fetox },
	{ "ftwotox", &jit_disable.ftwotox },
	{ "ftentox", &jit_disable.ftentox },
	{ "flogn", &jit_disable.flogn },
	{ "flog10", &jit_disable.flog10 },
	{ "flog2", &jit_disable.flog2 },
	{ "fabs", &jit_disable.fabs },
	{ "fcosh", &jit_disable.fcosh },
	{ "fneg", &jit_disable.fneg },
	{ "facos", &jit_disable.facos },
	{ "fcos", &jit_disable.fcos },
	{ "fgetexp", &jit_disable.fgetexp },
	{ "fgetman", &jit_disable.fgetman },
	{ "fdiv", &jit_disable.fdiv },
	{ "fmod", &jit_disable.fmod },
	{ "fadd", &jit_disable.fadd },
	{ "fmul", &jit_disable.fmul },
	{ "fsgldiv", &jit_disable.fsgldiv },
	{ "frem", &jit_disable.frem },
	{ "fscale", &jit_disable.fscale },
	{ "fsglmul", &jit_disable.fsglmul },
	{ "fsub", &jit_disable.fsub },
	{ "fsincos", &jit_disable.fsincos },
	{ "fcmp", &jit_disable.fcmp },
	{ "ftst", &jit_disable.ftst },
};

static bool read_fpu_opcode(const char **pp)
{
	const char *p = *pp;
	const char *end;
	size_t len;
	unsigned int i;
	
	end = p;
	while (*end != '\0' && *end != ',')
		end++;
	len = end - p;
	if (*end != '\0')
		end++;
	for (i = 0; i < (sizeof(jit_opcodes) / sizeof(jit_opcodes[0])); i++)
	{
		if (len == strlen(jit_opcodes[i].name) && strncasecmp(jit_opcodes[i].name, p, len) == 0)
		{
			*jit_opcodes[i].disabled = true;
			jit_log("<JIT compiler> : disabled %s", jit_opcodes[i].name);
			*pp = end;
			return true;
		}
	}
	return false;
}
#endif

static bool merge_blacklist()
{
#ifdef UAE
	const char *blacklist = "";
#else
	const char *blacklist = PrefsFindString("jitblacklist");
#endif
#ifdef USE_JIT_FPU
	for (unsigned int i = 0; i < (sizeof(jit_opcodes) / sizeof(jit_opcodes[0])); i++)
		*jit_opcodes[i].disabled = false;
#endif
	if (blacklist[0] != '\0') {
		const char *p = blacklist;
		for (;;) {
			if (*p == 0)
				return true;

			int opcode1 = read_opcode(p);
			if (opcode1 < 0)
			{
#ifdef USE_JIT_FPU
				if (read_fpu_opcode(&p))
					continue;
#endif
				bug("<JIT compiler> : invalid opcode %s", p);
				return false;
			}
			p += 4;

			int opcode2 = opcode1;
			if (*p == '-') {
				p++;
				opcode2 = read_opcode(p);
				if (opcode2 < 0)
				{
					bug("<JIT compiler> : invalid opcode %s", p);
					return false;
				}
				p += 4;
			}

			if (*p == 0 || *p == ',') {
				jit_log("<JIT compiler> : blacklist opcodes : %04x-%04x", opcode1, opcode2);
				for (int opcode = opcode1; opcode <= opcode2; opcode++)
					reset_compop(cft_map(opcode));

				if (*(p++) == ',')
					continue;

				return true;
			}

			return false;
		}
	}
	return true;
}

void build_comp(void)
{
#ifdef FSUAE
	if (!g_fs_uae_jit_compiler) {
		jit_log("JIT: JIT compiler is not enabled");
		return;
	}
#endif
	int i;
	unsigned long opcode;
	const struct comptbl* tbl=op_smalltbl_0_comp_ff;
	const struct comptbl* nftbl=op_smalltbl_0_comp_nf;
	int count;
#ifdef WINUAE_ARANYM
	unsigned int cpu_level = 4; 		// 68040
#if 0
	const struct cputbl *nfctbl = op_smalltbl_0_nf;
#endif
#else
#ifdef NOFLAGS_SUPPORT
	struct comptbl *nfctbl = (currprefs.cpu_level >= 5 ? op_smalltbl_0_nf
		: currprefs.cpu_level == 4 ? op_smalltbl_1_nf
		: (currprefs.cpu_level == 2 || currprefs.cpu_level == 3) ? op_smalltbl_2_nf
		: currprefs.cpu_level == 1 ? op_smalltbl_3_nf
		: ! currprefs.cpu_compatible ? op_smalltbl_4_nf
		: op_smalltbl_5_nf);
#endif
#endif
	// Initialize target CPU (check for features, e.g. CMOV, rat stalls)
	raw_init_cpu();

#ifdef NATMEM_OFFSET
#ifdef UAE
#ifdef JIT_EXCEPTION_HANDLER
	install_exception_handler();
#endif
#endif
#endif

	jit_log("<JIT compiler> : building compiler function tables");
	
	for (opcode = 0; opcode < 65536; opcode++) {
		reset_compop(opcode);
#ifdef NOFLAGS_SUPPORT
		nfcpufunctbl[opcode] = op_illg;
#endif
		prop[opcode].use_flags = FLAG_ALL;
		prop[opcode].set_flags = FLAG_ALL;
#ifdef UAE
		prop[opcode].is_jump=1;
#else
		prop[opcode].cflow = fl_trap; // ILLEGAL instructions do trap
#endif
	}

	for (i = 0; tbl[i].opcode < 65536; i++) {
#ifdef UAE
		int isjmp = (tbl[i].specific & COMP_OPCODE_ISJUMP);
		int isaddx = (tbl[i].specific & COMP_OPCODE_ISADDX);
		int iscjmp = (tbl[i].specific & COMP_OPCODE_ISCJUMP);

		prop[cft_map(tbl[i].opcode)].is_jump = isjmp;
		prop[cft_map(tbl[i].opcode)].is_const_jump = iscjmp;
		prop[cft_map(tbl[i].opcode)].is_addx = isaddx;
#else
		int cflow = table68k[tbl[i].opcode].cflow;
		if (follow_const_jumps && (tbl[i].specific & COMP_OPCODE_ISCJUMP))
			cflow = fl_const_jump;
		else
			cflow &= ~fl_const_jump;
		prop[cft_map(tbl[i].opcode)].cflow = cflow;
#endif

		bool uses_fpu = (tbl[i].specific & COMP_OPCODE_USES_FPU) != 0;
		if (uses_fpu && avoid_fpu)
			compfunctbl[cft_map(tbl[i].opcode)] = NULL;
		else
			compfunctbl[cft_map(tbl[i].opcode)] = tbl[i].handler;
	}

	for (i = 0; nftbl[i].opcode < 65536; i++) {
		bool uses_fpu = (tbl[i].specific & COMP_OPCODE_USES_FPU) != 0;
		if (uses_fpu && avoid_fpu)
			nfcompfunctbl[cft_map(nftbl[i].opcode)] = NULL;
		else
			nfcompfunctbl[cft_map(nftbl[i].opcode)] = nftbl[i].handler;
#ifdef NOFLAGS_SUPPORT
		nfcpufunctbl[cft_map(nftbl[i].opcode)] = nfctbl[i].handler;
#endif
	}

#ifdef NOFLAGS_SUPPORT
	for (i = 0; nfctbl[i].handler; i++) {
		nfcpufunctbl[cft_map(nfctbl[i].opcode)] = nfctbl[i].handler;
	}
#endif

	for (opcode = 0; opcode < 65536; opcode++) {
		compop_func *f;
		compop_func *nff;
#ifdef NOFLAGS_SUPPORT
		cpuop_func *nfcf;
#endif
		int isaddx;
#ifdef UAE
		int isjmp,iscjmp;
#else
		int cflow;
#endif

#ifdef UAE
		int cpu_level = (currprefs.cpu_model - 68000) / 10;
		if (cpu_level > 4)
			cpu_level--;
#endif
		if ((instrmnem)table68k[opcode].mnemo == i_ILLG || table68k[opcode].clev > cpu_level)
			continue;

		if (table68k[opcode].handler != -1) {
			f = compfunctbl[cft_map(table68k[opcode].handler)];
			nff = nfcompfunctbl[cft_map(table68k[opcode].handler)];
#ifdef NOFLAGS_SUPPORT
			nfcf = nfcpufunctbl[cft_map(table68k[opcode].handler)];
#endif
			isaddx = prop[cft_map(table68k[opcode].handler)].is_addx;
			prop[cft_map(opcode)].is_addx = isaddx;
#ifdef UAE
			isjmp = prop[cft_map(table68k[opcode].handler)].is_jump;
			iscjmp = prop[cft_map(table68k[opcode].handler)].is_const_jump;
			prop[cft_map(opcode)].is_jump = isjmp;
			prop[cft_map(opcode)].is_const_jump = iscjmp;
#else
			cflow = prop[cft_map(table68k[opcode].handler)].cflow;
			prop[cft_map(opcode)].cflow = cflow;
#endif
			compfunctbl[cft_map(opcode)] = f;
			nfcompfunctbl[cft_map(opcode)] = nff;
#ifdef NOFLAGS_SUPPORT
			Dif (nfcf == op_illg)
				abort();
			nfcpufunctbl[cft_map(opcode)] = nfcf;
#endif
		}
		prop[cft_map(opcode)].set_flags = table68k[opcode].flagdead;
		prop[cft_map(opcode)].use_flags = table68k[opcode].flaglive;
		/* Unconditional jumps don't evaluate condition codes, so they
		 * don't actually use any flags themselves */
#ifdef UAE
		if (prop[cft_map(opcode)].is_const_jump)
#else
		if (prop[cft_map(opcode)].cflow & fl_const_jump)
#endif
			prop[cft_map(opcode)].use_flags = 0;
	}
#ifdef NOFLAGS_SUPPORT
	for (i = 0; nfctbl[i].handler != NULL; i++) {
		if (nfctbl[i].specific)
			nfcpufunctbl[cft_map(tbl[i].opcode)] = nfctbl[i].handler;
	}
#endif

	/* Merge in blacklist */
	if (!merge_blacklist())
	{
		jit_log("<JIT compiler> : blacklist merge failure!");
	}

	count=0;
	for (opcode = 0; opcode < 65536; opcode++) {
		if (compfunctbl[cft_map(opcode)])
			count++;
	}
	jit_log("<JIT compiler> : supposedly %d compileable opcodes!",count);

	/* Initialise state */
	create_popalls();
	alloc_cache();
	reset_lists();

	for (i=0;i<TAGSIZE;i+=2) {
		cache_tags[i].handler=(cpuop_func*)popall_execute_normal;
		cache_tags[i+1].bi=NULL;
	}
#ifdef UAE
	compemu_reset();
#endif

#if 0
	for (i=0;i<N_REGS;i++) {
		empty_ss.nat[i].holds=-1;
		empty_ss.nat[i].validsize=0;
		empty_ss.nat[i].dirtysize=0;
	}
#endif
	for (i=0;i<VREGS;i++) {
		empty_ss.virt[i]=L_NEEDED;
	}
	for (i=0;i<N_REGS;i++) {
		empty_ss.nat[i]=L_UNKNOWN;
	}
	default_ss=empty_ss;
}


static void flush_icache_none(void)
{
	/* Nothing to do.  */
}

void flush_icache_hard(void)
{
	blockinfo* bi, *dbi;

#ifndef UAE
	jit_log("JIT: Flush Icache_hard(%d/%x/%p), %u KB",
		n,regs.pc,regs.pc_p,current_cache_size/1024);
#endif
	bi=active;
	while(bi) {
		cache_tags[cacheline(bi->pc_p)].handler=(cpuop_func*)popall_execute_normal;
		cache_tags[cacheline(bi->pc_p)+1].bi=NULL;
		dbi=bi; bi=bi->next;
		free_blockinfo(dbi);
	}
	bi=dormant;
	while(bi) {
		cache_tags[cacheline(bi->pc_p)].handler=(cpuop_func*)popall_execute_normal;
		cache_tags[cacheline(bi->pc_p)+1].bi=NULL;
		dbi=bi; bi=bi->next;
		free_blockinfo(dbi);
	}

	reset_lists();
	if (!compiled_code)
		return;

#if defined(USE_DATA_BUFFER)
	reset_data_buffer();
#endif

	current_compile_p=compiled_code;
#ifdef UAE
	set_special(0); /* To get out of compiled code */
#else
	SPCFLAGS_SET( SPCFLAG_JIT_EXEC_RETURN ); /* To get out of compiled code */
#endif
}


/* "Soft flushing" --- instead of actually throwing everything away,
   we simply mark everything as "needs to be checked".
*/

static inline void flush_icache_lazy(void)
{
	blockinfo* bi;
	blockinfo* bi2;

	if (!active)
		return;

	bi=active;
	while (bi) {
		uae_u32 cl=cacheline(bi->pc_p);
		if (bi->status==BI_INVALID ||
			bi->status==BI_NEED_RECOMP) { 
			if (bi==cache_tags[cl+1].bi)
				cache_tags[cl].handler=(cpuop_func*)popall_execute_normal;
			bi->handler_to_use=(cpuop_func*)popall_execute_normal;
			set_dhtu(bi,bi->direct_pen);
			bi->status=BI_INVALID;
		}
		else {
			if (bi==cache_tags[cl+1].bi)
				cache_tags[cl].handler=(cpuop_func*)popall_check_checksum;
			bi->handler_to_use=(cpuop_func*)popall_check_checksum;
			set_dhtu(bi,bi->direct_pcc);
			bi->status=BI_NEED_CHECK;
		}
		bi2=bi;
		bi=bi->next;
	}
	/* bi2 is now the last entry in the active list */
	bi2->next=dormant;
	if (dormant)
		dormant->prev_p=&(bi2->next);

	dormant=active;
	active->prev_p=&dormant;
	active=NULL;
}


#if 0
static void flush_icache_range(uae_u32 start, uae_u32 length)
{
	if (!active)
		return;

#if LAZY_FLUSH_ICACHE_RANGE
	uae_u8 *start_p = get_real_address(start);
	blockinfo *bi = active;
	while (bi) {
#if USE_CHECKSUM_INFO
		bool invalidate = false;
		for (checksum_info *csi = bi->csi; csi && !invalidate; csi = csi->next)
			invalidate = (((start_p - csi->start_p) < csi->length) ||
						  ((csi->start_p - start_p) < length));
#else
		// Assume system is consistent and would invalidate the right range
		const bool invalidate = (bi->pc_p - start_p) < length;
#endif
		if (invalidate) {
			uae_u32 cl = cacheline(bi->pc_p);
			if (bi == cache_tags[cl + 1].bi)
					cache_tags[cl].handler = (cpuop_func *)popall_execute_normal;
			bi->handler_to_use = (cpuop_func *)popall_execute_normal;
			set_dhtu(bi, bi->direct_pen);
			bi->status = BI_NEED_RECOMP;
		}
		bi = bi->next;
	}
	return;
#else
		UNUSED(start);
		UNUSED(length);
#endif
	flush_icache();
}
#endif


int failure;

#ifdef UAE
static inline unsigned int get_opcode_cft_map(unsigned int f)
{
	return ((f >> 8) & 255) | ((f & 255) << 8);
}
#define DO_GET_OPCODE(a) (get_opcode_cft_map((uae_u16)*(a)))
#else
#if defined(HAVE_GET_WORD_UNSWAPPED) && !defined(FULLMMU)
# define DO_GET_OPCODE(a) (do_get_mem_word_unswapped((uae_u16 *)(a)))
#else
# define DO_GET_OPCODE(a) (do_get_mem_word((uae_u16 *)(a)))
#endif
#endif

#ifdef JIT_DEBUG
static uae_u8 *last_regs_pc_p = 0;
static uae_u8 *last_compiled_block_addr = 0;

void compiler_dumpstate(void)
{
	if (!JITDebug)
		return;
	
	jit_log("### Host addresses");
	jit_log("MEM_BASE    : %lx", (unsigned long)MEMBaseDiff);
	jit_log("PC_P        : %p", &regs.pc_p);
	jit_log("SPCFLAGS    : %p", &regs.spcflags);
	jit_log("D0-D7       : %p-%p", &regs.regs[0], &regs.regs[7]);
	jit_log("A0-A7       : %p-%p", &regs.regs[8], &regs.regs[15]);
	jit_log(" ");
	
	jit_log("### M68k processor state");
	m68k_dumpstate(stderr, 0);
	jit_log(" ");
	
	jit_log("### Block in Atari address space");
	jit_log("M68K block   : %p",
			  (void *)(uintptr)last_regs_pc_p);
	if (last_regs_pc_p != 0) {
		jit_log("Native block : %p (%d bytes)",
			  (void *)last_compiled_block_addr,
			  get_blockinfo_addr(last_regs_pc_p)->direct_handler_size);
	}
	jit_log(" ");
}
#endif


#if 0 /* debugging helpers; activate as needed */
static void print_exc_frame(uae_u32 opcode)
{
	int nr = (opcode & 0x0f) + 32;
	if (nr != 0x45 && /* Timer-C */
		nr != 0x1c && /* VBL */
		nr != 0x46)   /* ACIA */
	{
		memptr sp = m68k_areg(regs, 7);
		uae_u16 sr = get_word(sp);
		fprintf(stderr, "Exc:%02x  SP: %08x  USP: %08x  SR: %04x  PC: %08x  Format: %04x", nr, sp, regs.usp, sr, get_long(sp + 2), get_word(sp + 6));
		if (nr >= 32 && nr < 48)
		{
			fprintf(stderr, "  Opcode: $%04x", sr & 0x2000 ? get_word(sp + 8) : get_word(regs.usp));
		}
		fprintf(stderr, "\n");
	}
}

static void push_all_nat(void)
{
	raw_pushfl();
	raw_push_l_r(EAX_INDEX);
	raw_push_l_r(ECX_INDEX);
	raw_push_l_r(EDX_INDEX);
	raw_push_l_r(EBX_INDEX);
	raw_push_l_r(EBP_INDEX);
	raw_push_l_r(EDI_INDEX);
	raw_push_l_r(ESI_INDEX);
	raw_push_l_r(R8_INDEX);
	raw_push_l_r(R9_INDEX);
	raw_push_l_r(R10_INDEX);
	raw_push_l_r(R11_INDEX);
	raw_push_l_r(R12_INDEX);
	raw_push_l_r(R13_INDEX);
	raw_push_l_r(R14_INDEX);
	raw_push_l_r(R15_INDEX);
}

static void pop_all_nat(void)
{
	raw_pop_l_r(R15_INDEX);
	raw_pop_l_r(R14_INDEX);
	raw_pop_l_r(R13_INDEX);
	raw_pop_l_r(R12_INDEX);
	raw_pop_l_r(R11_INDEX);
	raw_pop_l_r(R10_INDEX);
	raw_pop_l_r(R9_INDEX);
	raw_pop_l_r(R8_INDEX);
	raw_pop_l_r(ESI_INDEX);
	raw_pop_l_r(EDI_INDEX);
	raw_pop_l_r(EBP_INDEX);
	raw_pop_l_r(EBX_INDEX);
	raw_pop_l_r(EDX_INDEX);
	raw_pop_l_r(ECX_INDEX);
	raw_pop_l_r(EAX_INDEX);
	raw_popfl();
}
#endif

#if 0
static void print_inst(void)
{
	disasm_m68k_block(regs.fault_pc + (uint8 *)MEMBaseDiff, 1);
}
#endif


#ifdef UAE
void compile_block(cpu_history *pc_hist, int blocklen, int totcycles)
{
	if (cache_enabled && compiled_code && currprefs.cpu_model >= 68020) {
#else
static void compile_block(cpu_history* pc_hist, int blocklen)
{
	if (cache_enabled && compiled_code) {
#endif
#ifdef PROFILE_COMPILE_TIME
		compile_count++;
		clock_t start_time = clock();
#endif
#ifdef JIT_DEBUG
		bool disasm_block = false;
#endif

		/* OK, here we need to 'compile' a block */
		int i;
		int r;
		int was_comp=0;
		uae_u8 liveflags[MAXRUN+1];
#if USE_CHECKSUM_INFO
		bool trace_in_rom = isinrom((uintptr)pc_hist[0].location) != 0;
		uintptr max_pcp=(uintptr)pc_hist[blocklen - 1].location;
		uintptr min_pcp=max_pcp;
#else
		uintptr max_pcp=(uintptr)pc_hist[0].location;
		uintptr min_pcp=max_pcp;
#endif
		uae_u32 cl=cacheline(pc_hist[0].location);
		void* specflags=(void*)&regs.spcflags;
		blockinfo* bi=NULL;
		blockinfo* bi2;
		int extra_len=0;

		redo_current_block=0;
		if (current_compile_p >= MAX_COMPILE_PTR)
			flush_icache_hard();

		alloc_blockinfos();

		bi=get_blockinfo_addr_new(pc_hist[0].location,0);
		bi2=get_blockinfo(cl);

		optlev=bi->optlevel;
		if (bi->status!=BI_INVALID) {
			Dif (bi!=bi2) {
				/* I don't think it can happen anymore. Shouldn't, in
				   any case. So let's make sure... */
				jit_abort("WOOOWOO count=%d, ol=%d %p %p", bi->count,bi->optlevel,bi->handler_to_use, cache_tags[cl].handler);
			}

			Dif (bi->count!=-1 && bi->status!=BI_NEED_RECOMP) {
				jit_abort("bi->count=%d, bi->status=%d,bi->optlevel=%d",bi->count,bi->status,bi->optlevel);
				/* What the heck? We are not supposed to be here! */
			}
		}
		if (bi->count==-1) {
			optlev++;
			while (!optcount[optlev])
				optlev++;
			bi->count=optcount[optlev]-1;
		}
		current_block_pc_p=(uintptr)pc_hist[0].location;

		remove_deps(bi); /* We are about to create new code */
		bi->optlevel=optlev;
		bi->pc_p=(uae_u8*)pc_hist[0].location;
#if USE_CHECKSUM_INFO
		free_checksum_info_chain(bi->csi);
		bi->csi = NULL;
#endif

		liveflags[blocklen]=FLAG_ALL; /* All flags needed afterwards */
		i=blocklen;
		while (i--) {
			uae_u16* currpcp=pc_hist[i].location;
			uae_u32 op=DO_GET_OPCODE(currpcp);

#if USE_CHECKSUM_INFO
			trace_in_rom = trace_in_rom && isinrom((uintptr)currpcp);
			if (follow_const_jumps && is_const_jump(op)) {
				checksum_info *csi = alloc_checksum_info();
				csi->start_p = (uae_u8 *)min_pcp;
				csi->length = max_pcp - min_pcp + LONGEST_68K_INST;
				csi->next = bi->csi;
				bi->csi = csi;
				max_pcp = (uintptr)currpcp;
			}
			min_pcp = (uintptr)currpcp;
#else
			if ((uintptr)currpcp<min_pcp)
				min_pcp=(uintptr)currpcp;
			if ((uintptr)currpcp>max_pcp)
				max_pcp=(uintptr)currpcp;
#endif

#ifdef UAE
			if (!currprefs.compnf) {
				liveflags[i]=FLAG_ALL;
			}
			else
#endif
			{
				liveflags[i] = ((liveflags[i+1] & (~prop[op].set_flags))|prop[op].use_flags);
				if (prop[op].is_addx && (liveflags[i+1]&FLAG_Z)==0)
					liveflags[i]&= ~FLAG_Z;
			}
		}

#if USE_CHECKSUM_INFO
		checksum_info *csi = alloc_checksum_info();
		csi->start_p = (uae_u8 *)min_pcp;
		csi->length = max_pcp - min_pcp + LONGEST_68K_INST;
		csi->next = bi->csi;
		bi->csi = csi;
#endif

		bi->needed_flags=liveflags[0];

		align_target(align_loops);
		was_comp=0;

		bi->direct_handler=(cpuop_func*)get_target();
		set_dhtu(bi,bi->direct_handler);
		bi->status=BI_COMPILING;
		current_block_start_target=(uintptr)get_target();
	
		log_startblock();

		if (bi->count>=0) { /* Need to generate countdown code */
			compemu_raw_mov_l_mi((uintptr)&regs.pc_p,(uintptr)pc_hist[0].location);
			compemu_raw_sub_l_mi((uintptr)&(bi->count),1);
			compemu_raw_jl((uintptr)popall_recompile_block);
		}
		if (optlev==0) { /* No need to actually translate */
			/* Execute normally without keeping stats */
			compemu_raw_mov_l_mi((uintptr)&regs.pc_p,(uintptr)pc_hist[0].location);
			compemu_raw_jmp((uintptr)popall_exec_nostats);
		}
		else {
			reg_alloc_run=0;
			next_pc_p=0;
			taken_pc_p=0;
			branch_cc=0; // Only to be initialized. Will be set together with next_pc_p

			comp_pc_p=(uae_u8*)pc_hist[0].location;
			init_comp();
			was_comp=1;

#ifdef USE_CPU_EMUL_SERVICES
			compemu_raw_sub_l_mi((uintptr)&emulated_ticks,blocklen);
			compemu_raw_jcc_b_oponly(NATIVE_CC_GT);
			uae_u8 *branchadd=get_target();
			skip_byte();
			raw_dec_sp(STACK_SHADOW_SPACE);
			compemu_raw_call((uintptr)cpu_do_check_ticks);
			raw_inc_sp(STACK_SHADOW_SPACE);
			*branchadd=get_target()-(branchadd+1);
#endif

#ifdef JIT_DEBUG
			if (JITDebug) {
				compemu_raw_mov_l_mi((uintptr)&last_regs_pc_p,(uintptr)pc_hist[0].location);
				compemu_raw_mov_l_mi((uintptr)&last_compiled_block_addr,current_block_start_target);
			}
#endif

			for (i=0;i<blocklen && get_target_noopt() < MAX_COMPILE_PTR;i++) {
				cpuop_func **cputbl;
				compop_func **comptbl;
				uae_u32 opcode=DO_GET_OPCODE(pc_hist[i].location);
				needed_flags=(liveflags[i+1] & prop[opcode].set_flags);
#ifdef UAE
				special_mem=pc_hist[i].specmem;
				if (!needed_flags && currprefs.compnf)
#else
				if (!needed_flags)
#endif
				{
#ifdef NOFLAGS_SUPPORT
					cputbl=nfcpufunctbl;
#else
					cputbl=cpufunctbl;
#endif
					comptbl=nfcompfunctbl;
				}
				else {
					cputbl=cpufunctbl;
					comptbl=compfunctbl;
				}

#if FLIGHT_RECORDER
				{
					/* store also opcode to second register */
					clobber_flags();
					remove_all_offsets();
					prepare_for_call_1();
					prepare_for_call_2();
					raw_mov_l_ri(REG_PAR1, (memptr)((uintptr)pc_hist[i].location - MEMBaseDiff));
					raw_mov_w_ri(REG_PAR2, cft_map(opcode));
					raw_dec_sp(STACK_SHADOW_SPACE);
					compemu_raw_call((uintptr)m68k_record_step);
					raw_inc_sp(STACK_SHADOW_SPACE);
				}
#endif

				failure = 1; // gb-- defaults to failure state
				if (comptbl[opcode] && optlev>1) {
					failure=0;
					if (!was_comp) {
						comp_pc_p=(uae_u8*)pc_hist[i].location;
						init_comp();
					}
					was_comp=1;

#if defined(HAVE_DISASM_NATIVE) && defined(HAVE_DISASM_M68K)
/* debugging helpers; activate as needed */
#if 1
					disasm_this_inst = false;
					const uae_u8 *start_m68k_thisinst = (const uae_u8 *)pc_hist[i].location;
					uae_u8 *start_native_thisinst = get_target();
#endif
#endif

#ifdef WINUAE_ARANYM
					bool isnop = do_get_mem_word(pc_hist[i].location) == 0x4e71 ||
						((i + 1) < blocklen && do_get_mem_word(pc_hist[i+1].location) == 0x4e71);
					
					if (isnop)
						compemu_raw_mov_l_mi((uintptr)&regs.fault_pc, ((uintptr)(pc_hist[i].location)) - MEMBaseDiff);
#endif

					comptbl[opcode](opcode);
					freescratch();
					if (!(liveflags[i+1] & FLAG_CZNV)) {
						/* We can forget about flags */
						dont_care_flags();
					}
#if INDIVIDUAL_INST
					flush(1);
					nop();
					flush(1);
					was_comp=0;
#endif
#ifdef WINUAE_ARANYM
					/*
					 * workaround for buserror handling: on a "nop", write registers back
					 */
					if (isnop)
					{
						flush(1);
						nop();
						was_comp=0;
					}
#endif
#if defined(HAVE_DISASM_NATIVE) && defined(HAVE_DISASM_M68K)

/* debugging helpers; activate as needed */
#if 0
					disasm_m68k_block(start_m68k_thisinst, 1);
					push_all_nat();
					compemu_raw_mov_l_mi(uae_p32(&regs.fault_pc), (uintptr)start_m68k_thisinst - MEMBaseDiff);
					raw_dec_sp(STACK_SHADOW_SPACE);
					compemu_raw_call(uae_p32(print_instn));
					raw_inc_sp(STACK_SHADOW_SPACE);
					pop_all_nat();
#endif

					if (disasm_this_inst)
					{
						disasm_m68k_block(start_m68k_thisinst, 1);
#if 1
						disasm_native_block(start_native_thisinst, get_target() - start_native_thisinst);
#endif

#if 0
						push_all_nat();

						raw_dec_sp(STACK_SHADOW_SPACE);
						compemu_raw_mov_l_ri(REG_PAR1, (uae_u32)cft_map(opcode));
						compemu_raw_call((uintptr)print_exc_frame);
						raw_inc_sp(STACK_SHADOW_SPACE);

						pop_all_nat();
#endif

						if (failure)
						{
							bug("(discarded)");
							target = start_native_thisinst;
						}
					}
#endif
				}

				if (failure) {
					if (was_comp) {
						flush(1);
						was_comp=0;
					}
					compemu_raw_mov_l_ri(REG_PAR1,(uae_u32)opcode);
#if USE_NORMAL_CALLING_CONVENTION
					raw_push_l_r(REG_PAR1);
#endif
					compemu_raw_mov_l_mi((uintptr)&regs.pc_p,
						(uintptr)pc_hist[i].location);
					raw_dec_sp(STACK_SHADOW_SPACE);
					compemu_raw_call((uintptr)cputbl[opcode]);
					raw_inc_sp(STACK_SHADOW_SPACE);
#ifdef PROFILE_UNTRANSLATED_INSNS
					// raw_cputbl_count[] is indexed with plain opcode (in m68k order)
					compemu_raw_add_l_mi((uintptr)&raw_cputbl_count[cft_map(opcode)],1);
#endif
#if USE_NORMAL_CALLING_CONVENTION
					raw_inc_sp(4);
#endif

					if (i < blocklen - 1) {
						uae_u8* branchadd;

						/* if (SPCFLAGS_TEST(SPCFLAG_ALL)) popall_do_nothing() */
						compemu_raw_mov_l_rm(0, (uintptr)specflags);
						compemu_raw_test_l_rr(0,0);
#if defined(USE_DATA_BUFFER)
						data_check_end(8, 64);  // just a pessimistic guess...
#endif
						compemu_raw_jz_b_oponly();
						branchadd=get_target();
						skip_byte();
#ifdef UAE
						raw_sub_l_mi(uae_p32(&countdown),scaled_cycles(totcycles));
#endif
						compemu_raw_jmp((uintptr)popall_do_nothing);
						*branchadd = get_target() - (branchadd + 1);
					}
				}
			}
#if 1 /* This isn't completely kosher yet; It really needs to be
		 be integrated into a general inter-block-dependency scheme */
			if (next_pc_p && taken_pc_p &&
				was_comp && taken_pc_p==current_block_pc_p)
			{
				blockinfo* bi1=get_blockinfo_addr_new((void*)next_pc_p,0);
				blockinfo* bi2=get_blockinfo_addr_new((void*)taken_pc_p,0);
				uae_u8 x=bi1->needed_flags;

				if (x==0xff || 1) {  /* To be on the safe side */
					uae_u16* next=(uae_u16*)next_pc_p;
					uae_u32 op=DO_GET_OPCODE(next);

					x=FLAG_ALL;
					x&=(~prop[op].set_flags);
					x|=prop[op].use_flags;
				}

				x|=bi2->needed_flags;
				if (!(x & FLAG_CZNV)) {
					/* We can forget about flags */
					dont_care_flags();
					extra_len+=2; /* The next instruction now is part of this block */
				}
			}
#endif
			log_flush();

			if (next_pc_p) { /* A branch was registered */
				uintptr t1=next_pc_p;
				uintptr t2=taken_pc_p;
				int     cc=branch_cc;

				uae_u32* branchadd;
				uae_u32* tba;
				bigstate tmp;
				blockinfo* tbi;

				if (taken_pc_p<next_pc_p) {
					/* backward branch. Optimize for the "taken" case ---
					   which means the raw_jcc should fall through when
					   the 68k branch is taken. */
					t1=taken_pc_p;
					t2=next_pc_p;
					cc=branch_cc^1;
				}

				tmp=live; /* ouch! This is big... */
#if defined(USE_DATA_BUFFER)
				data_check_end(32, 128); // just a pessimistic guess...
#endif
				compemu_raw_jcc_l_oponly(cc);
				branchadd=(uae_u32*)get_target();
				skip_long();

				/* predicted outcome */
				tbi=get_blockinfo_addr_new((void*)t1,1);
				match_states(tbi);
#ifdef UAE
				raw_sub_l_mi(uae_p32(&countdown),scaled_cycles(totcycles));
				raw_jcc_l_oponly(NATIVE_CC_PL);
#else
				compemu_raw_cmp_l_mi8((uintptr)specflags,0);
				compemu_raw_jcc_l_oponly(NATIVE_CC_EQ);
#endif
				tba=(uae_u32*)get_target();
				emit_jmp_target(get_handler(t1));
				compemu_raw_mov_l_mi((uintptr)&regs.pc_p,t1);
				flush_reg_count();
				compemu_raw_jmp((uintptr)popall_do_nothing);
				create_jmpdep(bi,0,tba,t1);

				align_target(align_jumps);
				/* not-predicted outcome */
				write_jmp_target(branchadd, (cpuop_func *)get_target());
				live=tmp; /* Ouch again */
				tbi=get_blockinfo_addr_new((void*)t2,1);
				match_states(tbi);

				//flush(1); /* Can only get here if was_comp==1 */
#ifdef UAE
				raw_sub_l_mi(uae_p32(&countdown),scaled_cycles(totcycles));
				raw_jcc_l_oponly(NATIVE_CC_PL);
#else
				compemu_raw_cmp_l_mi8((uintptr)specflags,0);
				compemu_raw_jcc_l_oponly(NATIVE_CC_EQ);
#endif
				tba=(uae_u32*)get_target();
				emit_jmp_target(get_handler(t2));
				compemu_raw_mov_l_mi((uintptr)&regs.pc_p,t2);
				flush_reg_count();
				compemu_raw_jmp((uintptr)popall_do_nothing);
				create_jmpdep(bi,1,tba,t2);
			}
			else
			{
				if (was_comp) {
					flush(1);
				}
				flush_reg_count();

				/* Let's find out where next_handler is... */
				if (was_comp && isinreg(PC_P)) {
					r=live.state[PC_P].realreg;
					compemu_raw_and_l_ri(r,TAGMASK);
					int r2 = (r==0) ? 1 : 0;
					compemu_raw_mov_l_ri(r2,(uintptr)popall_do_nothing);
#ifdef UAE
					raw_sub_l_mi(uae_p32(&countdown),scaled_cycles(totcycles));
					raw_cmov_l_rm_indexed(r2,(uintptr)cache_tags,r,sizeof(void *),NATIVE_CC_PL);
#else
					compemu_raw_cmp_l_mi8((uintptr)specflags,0);
					compemu_raw_cmov_l_rm_indexed(r2,(uintptr)cache_tags,r,sizeof(void *),NATIVE_CC_EQ);
#endif
					compemu_raw_jmp_r(r2);
				}
				else if (was_comp && isconst(PC_P)) {
					uintptr v = live.state[PC_P].val;
					uae_u32* tba;
					blockinfo* tbi;

					tbi = get_blockinfo_addr_new((void*) v, 1);
					match_states(tbi);

#ifdef UAE
					raw_sub_l_mi(uae_p32(&countdown),scaled_cycles(totcycles));
					raw_jcc_l_oponly(NATIVE_CC_PL);
#else
					compemu_raw_cmp_l_mi8((uintptr)specflags,0);
					compemu_raw_jcc_l_oponly(NATIVE_CC_EQ);
#endif
					tba=(uae_u32*)get_target();
					emit_jmp_target(get_handler(v));
					compemu_raw_mov_l_mi((uintptr)&regs.pc_p,v);
					compemu_raw_jmp((uintptr)popall_do_nothing);
					create_jmpdep(bi,0,tba,v);
				}
				else {
					r=REG_PC_TMP;
					compemu_raw_mov_l_rm(r,(uintptr)&regs.pc_p);
					compemu_raw_and_l_ri(r,TAGMASK);
					int r2 = (r==0) ? 1 : 0;
					compemu_raw_mov_l_ri(r2,(uintptr)popall_do_nothing);
#ifdef UAE
					raw_sub_l_mi(uae_p32(&countdown),scaled_cycles(totcycles));
					raw_cmov_l_rm_indexed(r2,(uintptr)cache_tags,r,sizeof(void *),NATIVE_CC_PL);
#else
					compemu_raw_cmp_l_mi8((uintptr)specflags,0);
					compemu_raw_cmov_l_rm_indexed(r2,(uintptr)cache_tags,r,sizeof(void *),NATIVE_CC_EQ);
#endif
					compemu_raw_jmp_r(r2);
				}
			}
		}

#if USE_MATCH
		if (callers_need_recompile(&live,&(bi->env))) {
			mark_callers_recompile(bi);
		}

		big_to_small_state(&live,&(bi->env));
#endif

#if USE_CHECKSUM_INFO
		remove_from_list(bi);
		if (trace_in_rom) {
			// No need to checksum that block trace on cache invalidation
			free_checksum_info_chain(bi->csi);
			bi->csi = NULL;
			add_to_dormant(bi);
		}
		else {
			calc_checksum(bi,&(bi->c1),&(bi->c2));
			add_to_active(bi);
		}
#else
		if (next_pc_p+extra_len>=max_pcp &&
			next_pc_p+extra_len<max_pcp+LONGEST_68K_INST)
			max_pcp=next_pc_p+extra_len;  /* extra_len covers flags magic */
		else
			max_pcp+=LONGEST_68K_INST;

		bi->len=max_pcp-min_pcp;
		bi->min_pcp=min_pcp;

		remove_from_list(bi);
		if (isinrom(min_pcp) && isinrom(max_pcp)) {
			add_to_dormant(bi); /* No need to checksum it on cache flush.
								   Please don't start changing ROMs in
								   flight! */
		}
		else {
			calc_checksum(bi,&(bi->c1),&(bi->c2));
			add_to_active(bi);
		}
#endif

		current_cache_size += get_target() - current_compile_p;

#ifdef JIT_DEBUG
		bi->direct_handler_size = get_target() - (uae_u8 *)current_block_start_target;

		if (JITDebug && disasm_block) {
			uaecptr block_addr = start_pc + ((char *)pc_hist[0].location - (char *)start_pc_p);
			jit_log("M68K block @ 0x%08x (%d insns)", block_addr, blocklen);
			uae_u32 block_size = ((uae_u8 *)pc_hist[blocklen - 1].location - (uae_u8 *)pc_hist[0].location) + 1;
#ifdef WINUAE_ARANYM
			disasm_m68k_block((const uae_u8 *)pc_hist[0].location, block_size);
#endif
			jit_log("Compiled block @ %p", pc_hist[0].location);
#ifdef WINUAE_ARANYM
			disasm_native_block((const uae_u8 *)current_block_start_target, bi->direct_handler_size);
#endif
			UNUSED(block_addr);
		}
#endif

		log_dump();
		align_target(align_jumps);

#ifdef UAE
#ifdef USE_UDIS86
		UDISFN(current_block_start_target, target)
#endif
#endif

		/* This is the non-direct handler */
		bi->handler=
			bi->handler_to_use=(cpuop_func *)get_target();
		compemu_raw_cmp_l_mi((uintptr)&regs.pc_p,(uintptr)pc_hist[0].location);
		compemu_raw_jnz((uintptr)popall_cache_miss);
		comp_pc_p=(uae_u8*)pc_hist[0].location;

		bi->status=BI_FINALIZING;
		init_comp();
		match_states(bi);
		flush(1);

		compemu_raw_jmp((uintptr)bi->direct_handler);

		flush_cpu_icache((void *)current_block_start_target, (void *)target);
		current_compile_p=get_target();
		raise_in_cl_list(bi);
#ifdef UAE
		bi->nexthandler=current_compile_p;
#endif

		/* We will flush soon, anyway, so let's do it now */
		if (current_compile_p >= MAX_COMPILE_PTR)
			flush_icache_hard();

		bi->status=BI_ACTIVE;
		if (redo_current_block)
			block_need_recompile(bi);

#ifdef PROFILE_COMPILE_TIME
		compile_time += (clock() - start_time);
#endif
#ifdef UAE
		/* Account for compilation time */
		do_extra_cycles(totcycles);
#endif
	}

#ifdef USE_CPU_EMUL_SERVICES
	/* Account for compilation time */
	cpu_do_check_ticks();
#endif
}

#ifdef UAE
    /* Slightly different function defined in newcpu.cpp */
#else
void do_nothing(void)
{
	/* What did you expect this to do? */
}
#endif

#ifdef UAE
    /* Different implementation in newcpu.cpp */
#else
void exec_nostats(void)
{
	for (;;)  { 
		uae_u32 opcode = GET_OPCODE;
#if FLIGHT_RECORDER
		m68k_record_step(m68k_getpc(), cft_map(opcode));
#endif
		(*cpufunctbl[opcode])(opcode);
		cpu_check_ticks();
		if (end_block(opcode) || SPCFLAGS_TEST(SPCFLAG_ALL)) {
			return; /* We will deal with the spcflags in the caller */
		}
	}
}
#endif

#ifdef UAE
/* FIXME: check differences against UAE execute_normal (newcpu.cpp) */
#else
void execute_normal(void)
{
	if (!check_for_cache_miss()) {
		cpu_history pc_hist[MAXRUN];
		int blocklen = 0;
#if 0 && FIXED_ADDRESSING
		start_pc_p = regs.pc_p;
		start_pc = get_virtual_address(regs.pc_p);
#else
		start_pc_p = regs.pc_oldp;
		start_pc = regs.pc; 
#endif
		for (;;)  { /* Take note: This is the do-it-normal loop */
			pc_hist[blocklen++].location = (uae_u16 *)regs.pc_p;
			uae_u32 opcode = GET_OPCODE;
#if FLIGHT_RECORDER
			m68k_record_step(m68k_getpc(), cft_map(opcode));
#endif
			(*cpufunctbl[opcode])(opcode);
			cpu_check_ticks();
			if (end_block(opcode) || SPCFLAGS_TEST(SPCFLAG_ALL) || blocklen>=MAXRUN) {
				compile_block(pc_hist, blocklen);
				return; /* We will deal with the spcflags in the caller */
			}
			/* No need to check regs.spcflags, because if they were set,
			we'd have ended up inside that "if" */
		}
	}
}
#endif

typedef void (*compiled_handler)(void);

#ifdef UAE
/* FIXME: check differences against UAE m68k_do_compile_execute */
#else
void m68k_do_compile_execute(void)
{
	for (;;) {
		((compiled_handler)(pushall_call_handler))();
		/* Whenever we return from that, we should check spcflags */
		if (SPCFLAGS_TEST(SPCFLAG_ALL)) {
			if (m68k_do_specialties ())
				return;
		}
	}
}
#endif

#ifdef UAE
/* FIXME: check differences against UAE m68k_compile_execute */
#else
void m68k_compile_execute (void)
{
setjmpagain:
	TRY(prb) {
		for (;;) {
			if (quit_program > 0) {
				if (quit_program == 1) {
#if FLIGHT_RECORDER
					dump_flight_recorder();
#endif
					break;
				}
				quit_program = 0;
				m68k_reset ();
			}
			m68k_do_compile_execute();
		}
	}
	CATCH(prb) {
		jit_log("m68k_compile_execute: exception %d pc=%08x (%08x+%p-%p) fault_pc=%08x addr=%08x -> %08x sp=%08x",
			int(prb),
			m68k_getpc(),
			regs.pc, regs.pc_p, regs.pc_oldp,
			regs.fault_pc,
			regs.mmu_fault_addr, get_long (regs.vbr + 4*prb),
			regs.regs[15]);
		flush_icache();
		Exception(prb, 0);
		goto setjmpagain;
	}
}
#endif

#endif /* JIT */

#endif
