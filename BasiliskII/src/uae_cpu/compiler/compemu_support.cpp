/*
 *  compiler/compemu_support.cpp - Core dynamic translation engine
 *
 *  Original 68040 JIT compiler for UAE, copyright 2000-2002 Bernd Meyer
 *
 *  Adaptation for Basilisk II and improvements, copyright 2000-2005
 *    Gwenole Beauchesne
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
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

#if !REAL_ADDRESSING && !DIRECT_ADDRESSING
#error "Only Real or Direct Addressing is supported with the JIT Compiler"
#endif

#if X86_ASSEMBLY && !SAHF_SETO_PROFITABLE
#error "Only [LS]AHF scheme to [gs]et flags is supported with the JIT Compiler"
#endif

/* NOTE: support for AMD64 assumes translation cache and other code
 * buffers are allocated into a 32-bit address space because (i) B2/JIT
 * code is not 64-bit clean and (ii) it's faster to resolve branches
 * that way.
 */
#if !defined(__i386__) && !defined(__x86_64__)
#error "Only IA-32 and X86-64 targets are supported with the JIT Compiler"
#endif

#define USE_MATCH 0

/* kludge for Brian, so he can compile under MSVC++ */
#define USE_NORMAL_CALLING_CONVENTION 0

#ifndef WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#endif

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "main.h"
#include "prefs.h"
#include "user_strings.h"
#include "vm_alloc.h"

#include "m68k.h"
#include "memory.h"
#include "readcpu.h"
#include "newcpu.h"
#include "comptbl.h"
#include "compiler/compemu.h"
#include "fpu/fpu.h"
#include "fpu/flags.h"

#define DEBUG 1
#include "debug.h"

#ifdef ENABLE_MON
#include "mon.h"
#endif

#ifndef WIN32
#define PROFILE_COMPILE_TIME		1
#define PROFILE_UNTRANSLATED_INSNS	1
#endif

#if defined(__x86_64__) && 0
#define RECORD_REGISTER_USAGE		1
#endif

#ifdef WIN32
#undef write_log
#define write_log dummy_write_log
static void dummy_write_log(const char *, ...) { }
#endif

#if JIT_DEBUG
#undef abort
#define abort() do { \
	fprintf(stderr, "Abort in file %s at line %d\n", __FILE__, __LINE__); \
	exit(EXIT_FAILURE); \
} while (0)
#endif

#if RECORD_REGISTER_USAGE
static uint64 reg_count[16];
static int reg_count_local[16];

static int reg_count_compare(const void *ap, const void *bp)
{
    const int a = *((int *)ap);
    const int b = *((int *)bp);
    return reg_count[b] - reg_count[a];
}
#endif

#if PROFILE_COMPILE_TIME
#include <time.h>
static uae_u32 compile_count	= 0;
static clock_t compile_time		= 0;
static clock_t emul_start_time	= 0;
static clock_t emul_end_time	= 0;
#endif

#if PROFILE_UNTRANSLATED_INSNS
const int untranslated_top_ten = 20;
static uae_u32 raw_cputbl_count[65536] = { 0, };
static uae_u16 opcode_nums[65536];

static int untranslated_compfn(const void *e1, const void *e2)
{
	return raw_cputbl_count[*(const uae_u16 *)e1] < raw_cputbl_count[*(const uae_u16 *)e2];
}
#endif

static compop_func *compfunctbl[65536];
static compop_func *nfcompfunctbl[65536];
static cpuop_func *nfcpufunctbl[65536];
uae_u8* comp_pc_p;

// From newcpu.cpp
extern bool quit_program;

// gb-- Extra data for Basilisk II/JIT
#if JIT_DEBUG
static bool		JITDebug			= false;	// Enable runtime disassemblers through mon?
#else
const bool		JITDebug			= false;	// Don't use JIT debug mode at all
#endif
#if USE_INLINING
static bool		follow_const_jumps	= true;		// Flag: translation through constant jumps	
#else
const bool		follow_const_jumps	= false;
#endif

const uae_u32	MIN_CACHE_SIZE		= 1024;		// Minimal translation cache size (1 MB)
static uae_u32	cache_size			= 0;		// Size of total cache allocated for compiled blocks
static uae_u32	current_cache_size	= 0;		// Cache grows upwards: how much has been consumed already
static bool		lazy_flush			= true;		// Flag: lazy translation cache invalidation
static bool		avoid_fpu			= true;		// Flag: compile FPU instructions ?
static bool		have_cmov			= false;	// target has CMOV instructions ?
static bool		have_lahf_lm		= true;		// target has LAHF supported in long mode ?
static bool		have_rat_stall		= true;		// target has partial register stalls ?
const bool		tune_alignment		= true;		// Tune code alignments for running CPU ?
const bool		tune_nop_fillers	= true;		// Tune no-op fillers for architecture
static bool		setzflg_uses_bsf	= false;	// setzflg virtual instruction can use native BSF instruction correctly?
static int		align_loops			= 32;		// Align the start of loops
static int		align_jumps			= 32;		// Align the start of jumps
static int		optcount[10]		= {
	10,		// How often a block has to be executed before it is translated
	0,		// How often to use naive translation
	0, 0, 0, 0,
	-1, -1, -1, -1
};

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

static inline bool may_trap(uae_u32 opcode)
{
	return (prop[opcode].cflow & fl_trap);
}

static inline unsigned int cft_map (unsigned int f)
{
#ifndef HAVE_GET_WORD_UNSWAPPED
    return f;
#else
    return ((f >> 8) & 255) | ((f & 255) << 8);
#endif
}

uae_u8* start_pc_p;
uae_u32 start_pc;
uae_u32 current_block_pc_p;
static uintptr current_block_start_target;
uae_u32 needed_flags;
static uintptr next_pc_p;
static uintptr taken_pc_p;
static int branch_cc;
static int redo_current_block;

int segvcount=0;
int soft_flush_count=0;
int hard_flush_count=0;
int checksum_count=0;
static uae_u8* current_compile_p=NULL;
static uae_u8* max_compile_start;
static uae_u8* compiled_code=NULL;
static uae_s32 reg_alloc_run;
const int POPALLSPACE_SIZE = 1024; /* That should be enough space */
static uae_u8* popallspace=NULL;

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
cacheline cache_tags[TAGSIZE];
int letit=0;
blockinfo* hold_bi[MAX_HOLD_BI];
blockinfo* active;
blockinfo* dormant;

/* 68040 */
extern struct cputbl op_smalltbl_0_nf[];
extern struct comptbl op_smalltbl_0_comp_nf[];
extern struct comptbl op_smalltbl_0_comp_ff[];

/* 68020 + 68881 */
extern struct cputbl op_smalltbl_1_nf[];

/* 68020 */
extern struct cputbl op_smalltbl_2_nf[];

/* 68010 */
extern struct cputbl op_smalltbl_3_nf[];

/* 68000 */
extern struct cputbl op_smalltbl_4_nf[];

/* 68000 slow but compatible.  */
extern struct cputbl op_smalltbl_5_nf[];

static void flush_icache_hard(int n);
static void flush_icache_lazy(int n);
static void flush_icache_none(int n);
void (*flush_icache)(int n) = flush_icache_none;



bigstate live;
smallstate empty_ss;
smallstate default_ss;
static int optlev;

static int writereg(int r, int size);
static void unlock2(int r);
static void setlock(int r);
static int readreg_specific(int r, int size, int spec);
static int writereg_specific(int r, int size, int spec);
static void prepare_for_call_1(void);
static void prepare_for_call_2(void);
static void align_target(uae_u32 a);

static uae_s32 nextused[VREGS];

uae_u32 m68k_pc_offset;

/* Some arithmetic ooperations can be optimized away if the operands
 * are known to be constant. But that's only a good idea when the
 * side effects they would have on the flags are not important. This
 * variable indicates whether we need the side effects or not
 */
uae_u32 needflags=0;

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

static __inline__ blockinfo* get_blockinfo(uae_u32 cl)
{
    return cache_tags[cl+1].bi;
}

static __inline__ blockinfo* get_blockinfo_addr(void* addr)
{
    blockinfo*  bi=get_blockinfo(cacheline(addr));

    while (bi) {
	if (bi->pc_p==addr)
	    return bi;
	bi=bi->next_same_cl;
    }
    return NULL;
}

		
/*******************************************************************
 * All sorts of list related functions for all of the lists        *
 *******************************************************************/

static __inline__ void remove_from_cl_list(blockinfo* bi)
{
    uae_u32 cl=cacheline(bi->pc_p);

    if (bi->prev_same_cl_p) 
	*(bi->prev_same_cl_p)=bi->next_same_cl;
    if (bi->next_same_cl)
	bi->next_same_cl->prev_same_cl_p=bi->prev_same_cl_p;
    if (cache_tags[cl+1].bi)
	cache_tags[cl].handler=cache_tags[cl+1].bi->handler_to_use;
    else
	cache_tags[cl].handler=(cpuop_func *)popall_execute_normal;
}

static __inline__ void remove_from_list(blockinfo* bi)
{
    if (bi->prev_p) 
	*(bi->prev_p)=bi->next;
    if (bi->next)
	bi->next->prev_p=bi->prev_p;
}

static __inline__ void remove_from_lists(blockinfo* bi)
{
    remove_from_list(bi);
    remove_from_cl_list(bi);
}

static __inline__ void add_to_cl_list(blockinfo* bi)
{
    uae_u32 cl=cacheline(bi->pc_p);
    
    if (cache_tags[cl+1].bi)
	cache_tags[cl+1].bi->prev_same_cl_p=&(bi->next_same_cl);
    bi->next_same_cl=cache_tags[cl+1].bi;

    cache_tags[cl+1].bi=bi;
    bi->prev_same_cl_p=&(cache_tags[cl+1].bi);
	
    cache_tags[cl].handler=bi->handler_to_use;
}

static __inline__ void raise_in_cl_list(blockinfo* bi)
{
    remove_from_cl_list(bi);
    add_to_cl_list(bi);
}

static __inline__ void add_to_active(blockinfo* bi)
{
    if (active) 
	active->prev_p=&(bi->next);
    bi->next=active;

    active=bi;
    bi->prev_p=&active;
}

static __inline__ void add_to_dormant(blockinfo* bi)
{
    if (dormant) 
	dormant->prev_p=&(bi->next);
    bi->next=dormant;

    dormant=bi;
    bi->prev_p=&dormant;
}

static __inline__ void remove_dep(dependency* d)
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
static __inline__ void remove_deps(blockinfo* bi)
{
    remove_dep(&(bi->dep[0]));
    remove_dep(&(bi->dep[1]));
}

static __inline__ void adjust_jmpdep(dependency* d, cpuop_func* a)
{
    *(d->jmp_off)=(uintptr)a-((uintptr)d->jmp_off+4);
}

/********************************************************************
 * Soft flush handling support functions                            *
 ********************************************************************/

static __inline__ void set_dhtu(blockinfo* bi, cpuop_func* dh)
{
    //write_log("bi is %p\n",bi);
    if (dh!=bi->direct_handler_to_use) {
	dependency* x=bi->deplist;
	//write_log("bi->deplist=%p\n",bi->deplist);
	while (x) {
	    //write_log("x is %p\n",x);
	    //write_log("x->next is %p\n",x->next);
	    //write_log("x->prev_p is %p\n",x->prev_p);
	    
	    if (x->jmp_off) {
		adjust_jmpdep(x,dh);
	    }
	    x=x->next;
	}
	bi->direct_handler_to_use=dh;
    }
}

static __inline__ void invalidate_block(blockinfo* bi)
{
    int i;

    bi->optlevel=0;
    bi->count=optcount[0]-1;
    bi->handler=NULL;
    bi->handler_to_use=(cpuop_func *)popall_execute_normal;
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

static __inline__ void create_jmpdep(blockinfo* bi, int i, uae_u32* jmpaddr, uae_u32 target)
{
    blockinfo*  tbi=get_blockinfo_addr((void*)(uintptr)target);
    
    Dif(!tbi) {
	write_log("Could not create jmpdep!\n");
	abort();
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

static __inline__ void block_need_recompile(blockinfo * bi)
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

static __inline__ void mark_callers_recompile(blockinfo * bi)
{
  dependency *x = bi->deplist;

  while (x)	{
	dependency *next = x->next;	/* This disappears when we mark for
								 * recompilation and thus remove the
								 * blocks from the lists */
	if (x->jmp_off) {
	  blockinfo *cbi = x->source;

	  Dif(cbi->status == BI_INVALID) {
		// write_log("invalid block in dependency list\n"); // FIXME?
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
		//write_log("Status %d in mark_callers\n",cbi->status); // FIXME?
	  }
	}
	x = next;
  }
}

static __inline__ blockinfo* get_blockinfo_addr_new(void* addr, int setstate)
{
    blockinfo*  bi=get_blockinfo_addr(addr);
    int i;

    if (!bi) {
	for (i=0;i<MAX_HOLD_BI && !bi;i++) {
	    if (hold_bi[i]) {
		uae_u32 cl=cacheline(addr);
		
		bi=hold_bi[i];
		hold_bi[i]=NULL;
		bi->pc_p=(uae_u8 *)addr;
		invalidate_block(bi);
		add_to_active(bi);
		add_to_cl_list(bi);
		
	    }
	}
    }
    if (!bi) {
	write_log("Looking for blockinfo, can't find free one\n");
	abort();
    }
    return bi;
}

static void prepare_block(blockinfo* bi);

/* Managment of blockinfos.

   A blockinfo struct is allocated whenever a new block has to be
   compiled. If the list of free blockinfos is empty, we allocate a new
   pool of blockinfos and link the newly created blockinfos altogether
   into the list of free blockinfos. Otherwise, we simply pop a structure
   off the free list.

   Blockinfo are lazily deallocated, i.e. chained altogether in the
   list of free blockinfos whenvever a translation cache flush (hard or
   soft) request occurs.
*/

template< class T >
class LazyBlockAllocator
{
	enum {
		kPoolSize = 1 + 4096 / sizeof(T)
	};
	struct Pool {
		T chunk[kPoolSize];
		Pool * next;
	};
	Pool * mPools;
	T * mChunks;
public:
	LazyBlockAllocator() : mPools(0), mChunks(0) { }
	~LazyBlockAllocator();
	T * acquire();
	void release(T * const);
};

template< class T >
LazyBlockAllocator<T>::~LazyBlockAllocator()
{
	Pool * currentPool = mPools;
	while (currentPool) {
		Pool * deadPool = currentPool;
		currentPool = currentPool->next;
		free(deadPool);
	}
}

template< class T >
T * LazyBlockAllocator<T>::acquire()
{
	if (!mChunks) {
		// There is no chunk left, allocate a new pool and link the
		// chunks into the free list
		Pool * newPool = (Pool *)malloc(sizeof(Pool));
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

	void release(T * const chunk) {
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

static __inline__ checksum_info *alloc_checksum_info(void)
{
	checksum_info *csi = ChecksumInfoAllocator.acquire();
	csi->next = NULL;
	return csi;
}

static __inline__ void free_checksum_info(checksum_info *csi)
{
	csi->next = NULL;
	ChecksumInfoAllocator.release(csi);
}

static __inline__ void free_checksum_info_chain(checksum_info *csi)
{
	while (csi != NULL) {
		checksum_info *csi2 = csi->next;
		free_checksum_info(csi);
		csi = csi2;
	}
}

static __inline__ blockinfo *alloc_blockinfo(void)
{
	blockinfo *bi = BlockInfoAllocator.acquire();
#if USE_CHECKSUM_INFO
	bi->csi = NULL;
#endif
	return bi;
}

static __inline__ void free_blockinfo(blockinfo *bi)
{
#if USE_CHECKSUM_INFO
	free_checksum_info_chain(bi->csi);
	bi->csi = NULL;
#endif
	BlockInfoAllocator.release(bi);
}

static __inline__ void alloc_blockinfos(void) 
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

static  void emit_init(void)
{
}

static __inline__ void emit_byte(uae_u8 x)
{
    *target++=x;
}

static __inline__ void emit_word(uae_u16 x)
{
    *((uae_u16*)target)=x;
    target+=2;
}

static __inline__ void emit_long(uae_u32 x)
{
    *((uae_u32*)target)=x;
    target+=4;
}

static __inline__ void emit_quad(uae_u64 x)
{
    *((uae_u64*)target)=x;
    target+=8;
}

static __inline__ void emit_block(const uae_u8 *block, uae_u32 blocklen)
{
	memcpy((uae_u8 *)target,block,blocklen);
	target+=blocklen;
}

static __inline__ uae_u32 reverse32(uae_u32 v)
{
#if 1
	// gb-- We have specialized byteswapping functions, just use them
	return do_byteswap_32(v);
#else
	return ((v>>24)&0xff) | ((v>>8)&0xff00) | ((v<<8)&0xff0000) | ((v<<24)&0xff000000);
#endif
}

/********************************************************************
 * Getting the information about the target CPU                     *
 ********************************************************************/

#include "codegen_x86.cpp"

void set_target(uae_u8* t)
{
    target=t;
}

static __inline__ uae_u8* get_target_noopt(void)
{
    return target;
}

__inline__ uae_u8* get_target(void)
{
    return get_target_noopt();
}


/********************************************************************
 * Flags status handling. EMIT TIME!                                *
 ********************************************************************/

static void bt_l_ri_noclobber(R4 r, IMM i);

static void make_flags_live_internal(void)
{
    if (live.flags_in_flags==VALID)
	return;
    Dif (live.flags_on_stack==TRASH) {
	write_log("Want flags, got something on stack, but it is TRASH\n");
	abort();
    }
    if (live.flags_on_stack==VALID) {
	int tmp;
	tmp=readreg_specific(FLAGTMP,4,FLAG_NREG2);
	raw_reg_to_flags(tmp);
	unlock2(tmp);

	live.flags_in_flags=VALID;
	return;
    }
    write_log("Huh? live.flags_in_flags=%d, live.flags_on_stack=%d, but need to make live\n",
	   live.flags_in_flags,live.flags_on_stack);
    abort();
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
	abort();
    else  {
	int tmp;
	tmp=writereg_specific(FLAGTMP,4,FLAG_NREG1);
	raw_flags_to_reg(tmp);
	unlock2(tmp);
    }
    live.flags_on_stack=VALID;
}

static __inline__ void clobber_flags(void)
{
    if (live.flags_in_flags==VALID && live.flags_on_stack!=VALID)
	flags_to_stack();
    live.flags_in_flags=TRASH;
}

/* Prepare for leaving the compiled stuff */
static __inline__ void flush_flags(void)
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

static inline void ru_set(uae_u16 *mask, int reg)
{
#if USE_OPTIMIZED_CALLS
	*mask |= 1 << reg;
#endif
}

static inline bool ru_get(const uae_u16 *mask, int reg)
{
#if USE_OPTIMIZED_CALLS
	return (*mask & (1 << reg));
#else
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
	uae_u16 reg, ext;
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
		write_log("ru_fill: %04x = { %04x, %04x }\n",
				  real_opcode, ru->rmask, ru->wmask);
		abort();
	}
}

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

static __inline__ void big_to_small_state(bigstate * b, smallstate * s)
{
  int i;
	
  for (i = 0; i < VREGS; i++)
	s->virt[i] = vstate[i];
  for (i = 0; i < N_REGS; i++)
	s->nat[i] = nstate[i];
}

static __inline__ int callers_need_recompile(bigstate * b, smallstate * s)
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

static __inline__ void log_startblock(void)
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
static __inline__ void log_isused(int n)
{
  if (nstate[n] == L_UNKNOWN)
	nstate[n] = L_UNAVAIL;
}

static __inline__ void log_visused(int r)
{
  if (vstate[r] == L_UNKNOWN)
	vstate[r] = L_NEEDED;
}

static __inline__ void do_load_reg(int n, int r)
{
  if (r == FLAGTMP)
	raw_load_flagreg(n, r);
  else if (r == FLAGX)
	raw_load_flagx(n, r);
  else
	raw_mov_l_rm(n, (uintptr) live.state[r].mem);
}

static __inline__ void check_load_reg(int n, int r)
{
  raw_mov_l_rm(n, (uintptr) live.state[r].mem);
}

static __inline__ void log_vwrite(int r)
{
  vwritten[r] = 1;
}

/* Using an n-reg to hold a v-reg */
static __inline__ void log_isreg(int n, int r)
{
  static int count = 0;
  
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

static __inline__ void log_clobberreg(int r)
{
  if (vstate[r] == L_UNKNOWN)
	vstate[r] = L_UNNEEDED;
}

/* This ends all possibility of clever register allocation */

static __inline__ void log_flush(void)
{
  int i;
  
  for (i = 0; i < VREGS; i++)
	if (vstate[i] == L_UNKNOWN)
	  vstate[i] = L_NEEDED;
  for (i = 0; i < N_REGS; i++)
	if (nstate[i] == L_UNKNOWN)
	  nstate[i] = L_UNAVAIL;
}

static __inline__ void log_dump(void)
{
  int i;
  
  return;
  
  write_log("----------------------\n");
  for (i = 0; i < N_REGS; i++) {
	switch (nstate[i]) {
	case L_UNKNOWN:
	  write_log("Nat %d : UNKNOWN\n", i);
	  break;
	case L_UNAVAIL:
	  write_log("Nat %d : UNAVAIL\n", i);
	  break;
	default:
	  write_log("Nat %d : %d\n", i, nstate[i]);
	  break;
	}
  }
  for (i = 0; i < VREGS; i++) {
	if (vstate[i] == L_UNNEEDED)
	  write_log("Virt %d: UNNEEDED\n", i);
  }
}

/********************************************************************
 * register status handling. EMIT TIME!                             *
 ********************************************************************/

static __inline__ void set_status(int r, int status)
{
	if (status == ISCONST)
		log_clobberreg(r);
    live.state[r].status=status;
}

static __inline__ int isinreg(int r)
{
    return live.state[r].status==CLEAN || live.state[r].status==DIRTY;
}

static __inline__ void adjust_nreg(int r, uae_u32 val)
{
    if (!val)
	return;
    raw_lea_l_brr(r,r,val);
}

static  void tomem(int r)
{
    int rr=live.state[r].realreg;

    if (isinreg(r)) {
	if (live.state[r].val && live.nat[rr].nholds==1
		&& !live.nat[rr].locked) {
	    // write_log("RemovingA offset %x from reg %d (%d) at %p\n",
	    //   live.state[r].val,r,rr,target); 
	    adjust_nreg(rr,live.state[r].val);
	    live.state[r].val=0;
	    live.state[r].dirtysize=4;
	    set_status(r,DIRTY);
	}
    }

    if (live.state[r].status==DIRTY) {
	switch (live.state[r].dirtysize) {
	 case 1: raw_mov_b_mr((uintptr)live.state[r].mem,rr); break;
	 case 2: raw_mov_w_mr((uintptr)live.state[r].mem,rr); break;
	 case 4: raw_mov_l_mr((uintptr)live.state[r].mem,rr); break;
	 default: abort();
	}
	log_vwrite(r);
	set_status(r,CLEAN);
	live.state[r].dirtysize=0;
    }
}

static __inline__ int isconst(int r)
{
    return live.state[r].status==ISCONST;
}

int is_const(int r)
{
    return isconst(r);
}

static __inline__ void writeback_const(int r)
{
    if (!isconst(r))
	return;
    Dif (live.state[r].needflush==NF_HANDLER) {
	write_log("Trying to write back constant NF_HANDLER!\n");
	abort();
    }

    raw_mov_l_mi((uintptr)live.state[r].mem,live.state[r].val);
	log_vwrite(r);
    live.state[r].val=0;
    set_status(r,INMEM);
}

static __inline__ void tomem_c(int r)
{
    if (isconst(r)) {
	writeback_const(r);
    }
    else
	tomem(r);
}

static  void evict(int r)
{
    int rr;

    if (!isinreg(r))
	return;
    tomem(r);
    rr=live.state[r].realreg;

    Dif (live.nat[rr].locked &&
	live.nat[rr].nholds==1) {
	write_log("register %d in nreg %d is locked!\n",r,live.state[r].realreg);
	abort();
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

static __inline__ void free_nreg(int r)
{
    int i=live.nat[r].nholds;

    while (i) {
	int vr;

	--i;
	vr=live.nat[r].holds[i];
	evict(vr);
    }
    Dif (live.nat[r].nholds!=0) {
	write_log("Failed to free nreg %d, nholds is %d\n",r,live.nat[r].nholds);
	abort();
    }
}

/* Use with care! */
static __inline__ void isclean(int r)
{
    if (!isinreg(r))
	return;
    live.state[r].validsize=4;
    live.state[r].dirtysize=0;
    live.state[r].val=0;
    set_status(r,CLEAN);
}

static __inline__ void disassociate(int r)
{
    isclean(r);
    evict(r);
}

static __inline__ void set_const(int r, uae_u32 val)
{
    disassociate(r);
    live.state[r].val=val;
    set_status(r,ISCONST);
}

static __inline__ uae_u32 get_offset(int r)
{
    return live.state[r].val;
}

static  int alloc_reg_hinted(int r, int size, int willclobber, int hint)
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
	abort();

    if (live.nat[bestreg].nholds>0) {
	free_nreg(bestreg);
    }
    if (isinreg(r)) {
	int rr=live.state[r].realreg;
	/* This will happen if we read a partially dirty register at a
	   bigger size */
	Dif (willclobber || live.state[r].validsize>=size)
	    abort();
	Dif (live.nat[rr].nholds!=1)
	    abort();
	if (size==4 && live.state[r].validsize==2) {
		log_isused(bestreg);
		log_visused(r);
	    raw_mov_l_rm(bestreg,(uintptr)live.state[r].mem);
	    raw_bswap_32(bestreg);
	    raw_zero_extend_16_rr(rr,rr);
	    raw_zero_extend_16_rr(bestreg,bestreg);
	    raw_bswap_32(bestreg);
	    raw_lea_l_brr_indexed(rr,rr,bestreg,1,0);
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
		raw_mov_l_ri(bestreg,live.state[r].val);
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
		raw_mov_l_ri(bestreg,live.state[r].val);
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

static  int alloc_reg(int r, int size, int willclobber)
{
    return alloc_reg_hinted(r,size,willclobber,-1);
}

static  void unlock2(int r)
{
    Dif (!live.nat[r].locked)
	abort();
    live.nat[r].locked--;
}

static  void setlock(int r)
{
    live.nat[r].locked++;
}


static void mov_nregs(int d, int s)
{
    int ns=live.nat[s].nholds;
    int nd=live.nat[d].nholds;
    int i;

    if (s==d)
	return;

    if (nd>0) 
	free_nreg(d);

	log_isused(d);
    raw_mov_l_rr(d,s);

    for (i=0;i<live.nat[s].nholds;i++) {
	int vs=live.nat[s].holds[i];

	live.state[vs].realreg=d;
	live.state[vs].realind=i;
	live.nat[d].holds[i]=vs;
    }
    live.nat[d].nholds=live.nat[s].nholds;

    live.nat[s].nholds=0;
}


static __inline__ void make_exclusive(int r, int size, int spec)
{
    int clobber;
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
	    write_log("natreg %d holds %d vregs, %d not exclusive\n",
		   rr,live.nat[rr].nholds,r);
	    abort();
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
	    raw_lea_l_brr(nr,rr,oldstate.val);
	    live.state[r].val=0;
	    live.state[r].dirtysize=4;
	    set_status(r,DIRTY);
	}
	else
	    raw_mov_l_rr(nr,rr);  /* Make another copy */
    }
    unlock2(rr); 
}

static __inline__ void add_offset(int r, uae_u32 off)
{
    live.state[r].val+=off;
}

static __inline__ void remove_offset(int r, int spec)
{
    reg_status oldstate;
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
	write_log("Validsize=%d in remove_offset\n",live.state[r].validsize);
	abort();
    }
    make_exclusive(r,0,-1);
    /* make_exclusive might have done the job already */
    if (live.state[r].val==0)
	return;
    
    rr=live.state[r].realreg;

    if (live.nat[rr].nholds==1) {
	//write_log("RemovingB offset %x from reg %d (%d) at %p\n",
	//       live.state[r].val,r,rr,target); 
	adjust_nreg(rr,live.state[r].val);
	live.state[r].dirtysize=4;
	live.state[r].val=0;
	set_status(r,DIRTY);
	return;
    }
    write_log("Failed in remove_offset\n");
    abort();
}

static __inline__ void remove_all_offsets(void)
{
    int i;

    for (i=0;i<VREGS;i++)
	remove_offset(i,-1);
}

static inline void flush_reg_count(void)
{
#if RECORD_REGISTER_USAGE
    for (int r = 0; r < 16; r++)
	if (reg_count_local[r])
	    ADDQim(reg_count_local[r], ((uintptr)reg_count) + (8 * r), X86_NOREG, X86_NOREG, 1);
#endif
}

static inline void record_register(int r)
{
#if RECORD_REGISTER_USAGE
    if (r < 16)
	reg_count_local[r]++;
#endif
}

static __inline__ int readreg_general(int r, int size, int spec, int can_offset)
{
    int n;
    int answer=-1;
    
    record_register(r);
	if (live.state[r].status==UNDEF) {
		write_log("WARNING: Unexpected read of undefined register %d\n",r);
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
static __inline__ int writereg_general(int r, int size, int spec)
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
	    abort();
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
	    write_log("Problem with val\n");
	    abort();
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

static __inline__ int rmw_general(int r, int wsize, int rsize, int spec)
{
    int n;
    int answer=-1;
    
    record_register(r);
	if (live.state[r].status==UNDEF) {
		write_log("WARNING: Unexpected read of undefined register %d\n",r);
	}
    remove_offset(r,spec);
    make_exclusive(r,0,spec);

    Dif (wsize<rsize) {
	write_log("Cannot handle wsize<rsize in rmw_general()\n");
	abort();
    }
    if (isinreg(r) && live.state[r].validsize>=rsize) {
	n=live.state[r].realreg;
	Dif (live.nat[n].nholds!=1)
	    abort();

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
	write_log("Problem with val(rmw)\n");
	abort();
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
static void bt_l_ri_noclobber(R4 r, IMM i)
{
    int size=4;
    if (i<16)
	size=2;
    r=readreg(r,size);
    raw_bt_l_ri(r,i);
    unlock2(r);
}

/********************************************************************
 * FPU register status handling. EMIT TIME!                         *
 ********************************************************************/

static  void f_tomem(int r)
{
    if (live.fate[r].status==DIRTY) {
#if USE_LONG_DOUBLE
	raw_fmov_ext_mr((uintptr)live.fate[r].mem,live.fate[r].realreg); 
#else
	raw_fmov_mr((uintptr)live.fate[r].mem,live.fate[r].realreg); 
#endif
	live.fate[r].status=CLEAN;
    }
}

static  void f_tomem_drop(int r)
{
    if (live.fate[r].status==DIRTY) {
#if USE_LONG_DOUBLE
	raw_fmov_ext_mr_drop((uintptr)live.fate[r].mem,live.fate[r].realreg); 
#else
	raw_fmov_mr_drop((uintptr)live.fate[r].mem,live.fate[r].realreg); 
#endif
	live.fate[r].status=INMEM;
    }
}


static __inline__ int f_isinreg(int r)
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
	write_log("FPU register %d in nreg %d is locked!\n",r,live.fate[r].realreg);
	abort();
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

static __inline__ void f_free_nreg(int r)
{
    int i=live.fat[r].nholds;

    while (i) {
	int vr;

	--i;
	vr=live.fat[r].holds[i];
	f_evict(vr);
    }
    Dif (live.fat[r].nholds!=0) {
	write_log("Failed to free nreg %d, nholds is %d\n",r,live.fat[r].nholds);
	abort();
    }
}


/* Use with care! */
static __inline__ void f_isclean(int r)
{
    if (!f_isinreg(r))
	return;
    live.fate[r].status=CLEAN;
}

static __inline__ void f_disassociate(int r)
{
    f_isclean(r);
    f_evict(r);
}



static  int f_alloc_reg(int r, int willclobber)
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
	abort();

    if (live.fat[bestreg].nholds>0) {
	f_free_nreg(bestreg);
    }
    if (f_isinreg(r)) {
	f_evict(r);
    }

    if (!willclobber) {
	if (live.fate[r].status!=UNDEF) {
#if USE_LONG_DOUBLE
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

static  void f_unlock(int r)
{
    Dif (!live.fat[r].locked)
	abort();
    live.fat[r].locked--;
}

static  void f_setlock(int r)
{
    live.fat[r].locked++;
}

static __inline__ int f_readreg(int r)
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

static __inline__ void f_make_exclusive(int r, int clobber)
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
	    write_log("realreg %d holds %d (",rr,live.fat[rr].nholds);
	    for (i=0;i<live.fat[rr].nholds;i++) {
		write_log(" %d(%d,%d)",live.fat[rr].holds[i],
		       live.fate[live.fat[rr].holds[i]].realreg,
		       live.fate[live.fat[rr].holds[i]].realind);
	    }
	    write_log("\n");
	    abort();
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


static __inline__ int f_writereg(int r)
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

static int f_rmw(int r)
{
    int n;

    f_make_exclusive(r,0);
    if (f_isinreg(r)) {
	n=live.fate[r].realreg;
    }
    else 
	n=f_alloc_reg(r,0);
    live.fate[r].status=DIRTY;
    live.fat[n].locked++;
    live.fat[n].touched=touchcnt++;
    return n;
}

static void fflags_into_flags_internal(uae_u32 tmp)
{
    int r;

    clobber_flags();
    r=f_readreg(FP_RESULT);
	if (FFLAG_NREG_CLOBBER_CONDITION) {
	int tmp2=tmp;
	tmp=writereg_specific(tmp,4,FFLAG_NREG);
	raw_fflags_into_flags(r);
	unlock2(tmp);
	forget_about(tmp2);
	}
	else
    raw_fflags_into_flags(r);
    f_unlock(r);
    live_flags();
}




/********************************************************************
 * CPU functions exposed to gencomp. Both CREATE and EMIT time      *
 ********************************************************************/

/* 
 *  RULES FOR HANDLING REGISTERS:
 *
 *  * In the function headers, order the parameters 
 *     - 1st registers written to
 *     - 2nd read/modify/write registers
 *     - 3rd registers read from
 *  * Before calling raw_*, you must call readreg, writereg or rmw for
 *    each register
 *  * The order for this is
 *     - 1st call remove_offset for all registers written to with size<4
 *     - 2nd call readreg for all registers read without offset
 *     - 3rd call rmw for all rmw registers
 *     - 4th call readreg_offset for all registers that can handle offsets
 *     - 5th call get_offset for all the registers from the previous step
 *     - 6th call writereg for all written-to registers
 *     - 7th call raw_*
 *     - 8th unlock2 all registers that were locked
 */

MIDFUNC(0,live_flags,(void))
{
    live.flags_on_stack=TRASH;
    live.flags_in_flags=VALID;
    live.flags_are_important=1;
}
MENDFUNC(0,live_flags,(void))

MIDFUNC(0,dont_care_flags,(void))
{
    live.flags_are_important=0;
}
MENDFUNC(0,dont_care_flags,(void))


MIDFUNC(0,duplicate_carry,(void))
{
    evict(FLAGX);
    make_flags_live_internal();
    COMPCALL(setcc_m)((uintptr)live.state[FLAGX].mem,2);
	log_vwrite(FLAGX);
}
MENDFUNC(0,duplicate_carry,(void))

MIDFUNC(0,restore_carry,(void))
{
    if (!have_rat_stall) { /* Not a P6 core, i.e. no partial stalls */
	bt_l_ri_noclobber(FLAGX,0);
    }
    else {  /* Avoid the stall the above creates.
	       This is slow on non-P6, though.
	    */
	COMPCALL(rol_b_ri(FLAGX,8));
	isclean(FLAGX);
    }
}
MENDFUNC(0,restore_carry,(void))

MIDFUNC(0,start_needflags,(void))
{
    needflags=1;
}
MENDFUNC(0,start_needflags,(void))

MIDFUNC(0,end_needflags,(void))
{
    needflags=0;
}
MENDFUNC(0,end_needflags,(void))

MIDFUNC(0,make_flags_live,(void))
{
    make_flags_live_internal();
}
MENDFUNC(0,make_flags_live,(void))

MIDFUNC(1,fflags_into_flags,(W2 tmp))
{
    clobber_flags();
    fflags_into_flags_internal(tmp);
}
MENDFUNC(1,fflags_into_flags,(W2 tmp))


MIDFUNC(2,bt_l_ri,(R4 r, IMM i)) /* This is defined as only affecting C */
{    
    int size=4;
    if (i<16)
	size=2;
    CLOBBER_BT;
    r=readreg(r,size);
    raw_bt_l_ri(r,i);
    unlock2(r);
}
MENDFUNC(2,bt_l_ri,(R4 r, IMM i)) /* This is defined as only affecting C */

MIDFUNC(2,bt_l_rr,(R4 r, R4 b)) /* This is defined as only affecting C */
{
    CLOBBER_BT;
    r=readreg(r,4);
    b=readreg(b,4);
    raw_bt_l_rr(r,b);
    unlock2(r);
    unlock2(b);
}
MENDFUNC(2,bt_l_rr,(R4 r, R4 b)) /* This is defined as only affecting C */

MIDFUNC(2,btc_l_ri,(RW4 r, IMM i)) 
{    
    int size=4;
    if (i<16)
	size=2;
    CLOBBER_BT;
    r=rmw(r,size,size);
    raw_btc_l_ri(r,i);
    unlock2(r);
}
MENDFUNC(2,btc_l_ri,(RW4 r, IMM i)) 

MIDFUNC(2,btc_l_rr,(RW4 r, R4 b)) 
{
    CLOBBER_BT;
    b=readreg(b,4);
    r=rmw(r,4,4);
    raw_btc_l_rr(r,b);
    unlock2(r);
    unlock2(b);
}
MENDFUNC(2,btc_l_rr,(RW4 r, R4 b)) 


MIDFUNC(2,btr_l_ri,(RW4 r, IMM i)) 
{    
    int size=4;
    if (i<16)
	size=2;
    CLOBBER_BT;
    r=rmw(r,size,size);
    raw_btr_l_ri(r,i);
    unlock2(r);
}
MENDFUNC(2,btr_l_ri,(RW4 r, IMM i)) 

MIDFUNC(2,btr_l_rr,(RW4 r, R4 b)) 
{
    CLOBBER_BT;
    b=readreg(b,4);
    r=rmw(r,4,4);
    raw_btr_l_rr(r,b);
    unlock2(r);
    unlock2(b);
}
MENDFUNC(2,btr_l_rr,(RW4 r, R4 b)) 


MIDFUNC(2,bts_l_ri,(RW4 r, IMM i)) 
{    
    int size=4;
    if (i<16)
	size=2;
    CLOBBER_BT;
    r=rmw(r,size,size);
    raw_bts_l_ri(r,i);
    unlock2(r);
}
MENDFUNC(2,bts_l_ri,(RW4 r, IMM i)) 

MIDFUNC(2,bts_l_rr,(RW4 r, R4 b)) 
{
    CLOBBER_BT;
    b=readreg(b,4);
    r=rmw(r,4,4);
    raw_bts_l_rr(r,b);
    unlock2(r);
    unlock2(b);
}
MENDFUNC(2,bts_l_rr,(RW4 r, R4 b)) 

MIDFUNC(2,mov_l_rm,(W4 d, IMM s))
{
    CLOBBER_MOV;
    d=writereg(d,4);
    raw_mov_l_rm(d,s);
    unlock2(d);
}
MENDFUNC(2,mov_l_rm,(W4 d, IMM s))


MIDFUNC(1,call_r,(R4 r)) /* Clobbering is implicit */
{
    r=readreg(r,4);
    raw_call_r(r);
    unlock2(r);
}
MENDFUNC(1,call_r,(R4 r)) /* Clobbering is implicit */

MIDFUNC(2,sub_l_mi,(IMM d, IMM s)) 
{
    CLOBBER_SUB;
    raw_sub_l_mi(d,s) ;
}
MENDFUNC(2,sub_l_mi,(IMM d, IMM s)) 

MIDFUNC(2,mov_l_mi,(IMM d, IMM s)) 
{
    CLOBBER_MOV;
    raw_mov_l_mi(d,s) ;
}
MENDFUNC(2,mov_l_mi,(IMM d, IMM s)) 

MIDFUNC(2,mov_w_mi,(IMM d, IMM s)) 
{
    CLOBBER_MOV;
    raw_mov_w_mi(d,s) ;
}
MENDFUNC(2,mov_w_mi,(IMM d, IMM s)) 

MIDFUNC(2,mov_b_mi,(IMM d, IMM s)) 
{
    CLOBBER_MOV;
    raw_mov_b_mi(d,s) ;
}
MENDFUNC(2,mov_b_mi,(IMM d, IMM s)) 

MIDFUNC(2,rol_b_ri,(RW1 r, IMM i))
{
	if (!i && !needflags)
		return;
    CLOBBER_ROL;
    r=rmw(r,1,1);
    raw_rol_b_ri(r,i);
    unlock2(r);
}
MENDFUNC(2,rol_b_ri,(RW1 r, IMM i))

MIDFUNC(2,rol_w_ri,(RW2 r, IMM i))
{
	if (!i && !needflags)
		return;
    CLOBBER_ROL;
    r=rmw(r,2,2);
    raw_rol_w_ri(r,i);
    unlock2(r);
}
MENDFUNC(2,rol_w_ri,(RW2 r, IMM i))

MIDFUNC(2,rol_l_ri,(RW4 r, IMM i))
{
	if (!i && !needflags)
		return;
    CLOBBER_ROL;
    r=rmw(r,4,4);
    raw_rol_l_ri(r,i);
    unlock2(r);
}
MENDFUNC(2,rol_l_ri,(RW4 r, IMM i))

MIDFUNC(2,rol_l_rr,(RW4 d, R1 r)) 
{ 
    if (isconst(r)) {
	COMPCALL(rol_l_ri)(d,(uae_u8)live.state[r].val);
	return;
    }
    CLOBBER_ROL;
    r=readreg_specific(r,1,SHIFTCOUNT_NREG);
    d=rmw(d,4,4);
    Dif (r!=1) {
	write_log("Illegal register %d in raw_rol_b\n",r);
	abort();
    }
    raw_rol_l_rr(d,r) ;
    unlock2(r);
    unlock2(d);
}
MENDFUNC(2,rol_l_rr,(RW4 d, R1 r)) 

MIDFUNC(2,rol_w_rr,(RW2 d, R1 r)) 
{ /* Can only do this with r==1, i.e. cl */
  
    if (isconst(r)) {
	COMPCALL(rol_w_ri)(d,(uae_u8)live.state[r].val);
	return;
    }
    CLOBBER_ROL;
    r=readreg_specific(r,1,SHIFTCOUNT_NREG);
    d=rmw(d,2,2);
    Dif (r!=1) {
	write_log("Illegal register %d in raw_rol_b\n",r);
	abort();
    }
    raw_rol_w_rr(d,r) ;
    unlock2(r);
    unlock2(d);
}
MENDFUNC(2,rol_w_rr,(RW2 d, R1 r)) 

MIDFUNC(2,rol_b_rr,(RW1 d, R1 r)) 
{ /* Can only do this with r==1, i.e. cl */
  
    if (isconst(r)) {
	COMPCALL(rol_b_ri)(d,(uae_u8)live.state[r].val);
	return;
    }

    CLOBBER_ROL;
    r=readreg_specific(r,1,SHIFTCOUNT_NREG);
    d=rmw(d,1,1);
    Dif (r!=1) {
	write_log("Illegal register %d in raw_rol_b\n",r);
	abort();
    }
    raw_rol_b_rr(d,r) ;
    unlock2(r);
    unlock2(d);
}
MENDFUNC(2,rol_b_rr,(RW1 d, R1 r)) 


MIDFUNC(2,shll_l_rr,(RW4 d, R1 r)) 
{ 
    if (isconst(r)) {
	COMPCALL(shll_l_ri)(d,(uae_u8)live.state[r].val);
	return;
    }
    CLOBBER_SHLL;
    r=readreg_specific(r,1,SHIFTCOUNT_NREG);
    d=rmw(d,4,4);
    Dif (r!=1) {
	write_log("Illegal register %d in raw_rol_b\n",r);
	abort();
    }
    raw_shll_l_rr(d,r) ;
    unlock2(r);
    unlock2(d);
}
MENDFUNC(2,shll_l_rr,(RW4 d, R1 r)) 

MIDFUNC(2,shll_w_rr,(RW2 d, R1 r)) 
{ /* Can only do this with r==1, i.e. cl */
  
    if (isconst(r)) {
	COMPCALL(shll_w_ri)(d,(uae_u8)live.state[r].val);
	return;
    }
    CLOBBER_SHLL;
    r=readreg_specific(r,1,SHIFTCOUNT_NREG);
    d=rmw(d,2,2);
    Dif (r!=1) {
	write_log("Illegal register %d in raw_shll_b\n",r);
	abort();
    }
    raw_shll_w_rr(d,r) ;
    unlock2(r);
    unlock2(d);
}
MENDFUNC(2,shll_w_rr,(RW2 d, R1 r)) 

MIDFUNC(2,shll_b_rr,(RW1 d, R1 r)) 
{ /* Can only do this with r==1, i.e. cl */
  
    if (isconst(r)) {
	COMPCALL(shll_b_ri)(d,(uae_u8)live.state[r].val);
	return;
    }

    CLOBBER_SHLL;
    r=readreg_specific(r,1,SHIFTCOUNT_NREG);
    d=rmw(d,1,1);
    Dif (r!=1) {
	write_log("Illegal register %d in raw_shll_b\n",r);
	abort();
    }
    raw_shll_b_rr(d,r) ;
    unlock2(r);
    unlock2(d);
}
MENDFUNC(2,shll_b_rr,(RW1 d, R1 r)) 


MIDFUNC(2,ror_b_ri,(R1 r, IMM i))
{
	if (!i && !needflags)
		return;
    CLOBBER_ROR;
    r=rmw(r,1,1);
    raw_ror_b_ri(r,i);
    unlock2(r);
}
MENDFUNC(2,ror_b_ri,(R1 r, IMM i))

MIDFUNC(2,ror_w_ri,(R2 r, IMM i))
{
	if (!i && !needflags)
		return;
    CLOBBER_ROR;
    r=rmw(r,2,2);
    raw_ror_w_ri(r,i);
    unlock2(r);
}
MENDFUNC(2,ror_w_ri,(R2 r, IMM i))

MIDFUNC(2,ror_l_ri,(R4 r, IMM i))
{
	if (!i && !needflags)
		return;
    CLOBBER_ROR;
    r=rmw(r,4,4);
    raw_ror_l_ri(r,i);
    unlock2(r);
}
MENDFUNC(2,ror_l_ri,(R4 r, IMM i))

MIDFUNC(2,ror_l_rr,(R4 d, R1 r)) 
{ 
    if (isconst(r)) {
	COMPCALL(ror_l_ri)(d,(uae_u8)live.state[r].val);
	return;
    }
    CLOBBER_ROR;
    r=readreg_specific(r,1,SHIFTCOUNT_NREG);
    d=rmw(d,4,4);
    raw_ror_l_rr(d,r) ;
    unlock2(r);
    unlock2(d);
}
MENDFUNC(2,ror_l_rr,(R4 d, R1 r)) 

MIDFUNC(2,ror_w_rr,(R2 d, R1 r)) 
{ 
    if (isconst(r)) {
	COMPCALL(ror_w_ri)(d,(uae_u8)live.state[r].val);
	return;
    }
    CLOBBER_ROR;
    r=readreg_specific(r,1,SHIFTCOUNT_NREG);
    d=rmw(d,2,2);
    raw_ror_w_rr(d,r) ;
    unlock2(r);
    unlock2(d);
}
MENDFUNC(2,ror_w_rr,(R2 d, R1 r)) 

MIDFUNC(2,ror_b_rr,(R1 d, R1 r)) 
{   
    if (isconst(r)) {
	COMPCALL(ror_b_ri)(d,(uae_u8)live.state[r].val);
	return;
    }

    CLOBBER_ROR;
    r=readreg_specific(r,1,SHIFTCOUNT_NREG);
    d=rmw(d,1,1);
    raw_ror_b_rr(d,r) ;
    unlock2(r);
    unlock2(d);
}
MENDFUNC(2,ror_b_rr,(R1 d, R1 r)) 

MIDFUNC(2,shrl_l_rr,(RW4 d, R1 r)) 
{ 
    if (isconst(r)) {
	COMPCALL(shrl_l_ri)(d,(uae_u8)live.state[r].val);
	return;
    }
    CLOBBER_SHRL;
    r=readreg_specific(r,1,SHIFTCOUNT_NREG);
    d=rmw(d,4,4);
    Dif (r!=1) {
	write_log("Illegal register %d in raw_rol_b\n",r);
	abort();
    }
    raw_shrl_l_rr(d,r) ;
    unlock2(r);
    unlock2(d);
}
MENDFUNC(2,shrl_l_rr,(RW4 d, R1 r)) 

MIDFUNC(2,shrl_w_rr,(RW2 d, R1 r)) 
{ /* Can only do this with r==1, i.e. cl */
  
    if (isconst(r)) {
	COMPCALL(shrl_w_ri)(d,(uae_u8)live.state[r].val);
	return;
    }
    CLOBBER_SHRL;
    r=readreg_specific(r,1,SHIFTCOUNT_NREG);
    d=rmw(d,2,2);
    Dif (r!=1) {
	write_log("Illegal register %d in raw_shrl_b\n",r);
	abort();
    }
    raw_shrl_w_rr(d,r) ;
    unlock2(r);
    unlock2(d);
}
MENDFUNC(2,shrl_w_rr,(RW2 d, R1 r)) 

MIDFUNC(2,shrl_b_rr,(RW1 d, R1 r)) 
{ /* Can only do this with r==1, i.e. cl */
  
    if (isconst(r)) {
	COMPCALL(shrl_b_ri)(d,(uae_u8)live.state[r].val);
	return;
    }

    CLOBBER_SHRL;
    r=readreg_specific(r,1,SHIFTCOUNT_NREG);
    d=rmw(d,1,1);
    Dif (r!=1) {
	write_log("Illegal register %d in raw_shrl_b\n",r);
	abort();
    }
    raw_shrl_b_rr(d,r) ;
    unlock2(r);
    unlock2(d);
}
MENDFUNC(2,shrl_b_rr,(RW1 d, R1 r)) 



MIDFUNC(2,shll_l_ri,(RW4 r, IMM i))
{
    if (!i && !needflags)
	return;
    if (isconst(r) && !needflags) {
	live.state[r].val<<=i;
	return;
    }
    CLOBBER_SHLL;
    r=rmw(r,4,4);
    raw_shll_l_ri(r,i);
    unlock2(r);
}
MENDFUNC(2,shll_l_ri,(RW4 r, IMM i))

MIDFUNC(2,shll_w_ri,(RW2 r, IMM i))
{
    if (!i && !needflags)
	return;
    CLOBBER_SHLL;
    r=rmw(r,2,2);
    raw_shll_w_ri(r,i);
    unlock2(r);
}
MENDFUNC(2,shll_w_ri,(RW2 r, IMM i))

MIDFUNC(2,shll_b_ri,(RW1 r, IMM i))
{
    if (!i && !needflags)
	return;
    CLOBBER_SHLL;
    r=rmw(r,1,1);
    raw_shll_b_ri(r,i);
    unlock2(r);
}
MENDFUNC(2,shll_b_ri,(RW1 r, IMM i))

MIDFUNC(2,shrl_l_ri,(RW4 r, IMM i))
{
    if (!i && !needflags)
	return;
    if (isconst(r) && !needflags) {
	live.state[r].val>>=i;
	return;
    }
    CLOBBER_SHRL;
    r=rmw(r,4,4);
    raw_shrl_l_ri(r,i);
    unlock2(r);
}
MENDFUNC(2,shrl_l_ri,(RW4 r, IMM i))

MIDFUNC(2,shrl_w_ri,(RW2 r, IMM i))
{
    if (!i && !needflags)
	return;
    CLOBBER_SHRL;
    r=rmw(r,2,2);
    raw_shrl_w_ri(r,i);
    unlock2(r);
}
MENDFUNC(2,shrl_w_ri,(RW2 r, IMM i))

MIDFUNC(2,shrl_b_ri,(RW1 r, IMM i))
{
    if (!i && !needflags)
	return;
    CLOBBER_SHRL;
    r=rmw(r,1,1);
    raw_shrl_b_ri(r,i);
    unlock2(r);
}
MENDFUNC(2,shrl_b_ri,(RW1 r, IMM i))

MIDFUNC(2,shra_l_ri,(RW4 r, IMM i))
{
    if (!i && !needflags)
	return;
    CLOBBER_SHRA;
    r=rmw(r,4,4);
    raw_shra_l_ri(r,i);
    unlock2(r);
}
MENDFUNC(2,shra_l_ri,(RW4 r, IMM i))

MIDFUNC(2,shra_w_ri,(RW2 r, IMM i))
{
    if (!i && !needflags)
	return;
    CLOBBER_SHRA;
    r=rmw(r,2,2);
    raw_shra_w_ri(r,i);
    unlock2(r);
}
MENDFUNC(2,shra_w_ri,(RW2 r, IMM i))

MIDFUNC(2,shra_b_ri,(RW1 r, IMM i))
{
    if (!i && !needflags)
	return;
    CLOBBER_SHRA;
    r=rmw(r,1,1);
    raw_shra_b_ri(r,i);
    unlock2(r);
}
MENDFUNC(2,shra_b_ri,(RW1 r, IMM i))

MIDFUNC(2,shra_l_rr,(RW4 d, R1 r)) 
{ 
    if (isconst(r)) {
	COMPCALL(shra_l_ri)(d,(uae_u8)live.state[r].val);
	return;
    }
    CLOBBER_SHRA;
    r=readreg_specific(r,1,SHIFTCOUNT_NREG);
    d=rmw(d,4,4);
    Dif (r!=1) {
	write_log("Illegal register %d in raw_rol_b\n",r);
	abort();
    }
    raw_shra_l_rr(d,r) ;
    unlock2(r);
    unlock2(d);
}
MENDFUNC(2,shra_l_rr,(RW4 d, R1 r)) 

MIDFUNC(2,shra_w_rr,(RW2 d, R1 r)) 
{ /* Can only do this with r==1, i.e. cl */
  
    if (isconst(r)) {
	COMPCALL(shra_w_ri)(d,(uae_u8)live.state[r].val);
	return;
    }
    CLOBBER_SHRA;
    r=readreg_specific(r,1,SHIFTCOUNT_NREG);
    d=rmw(d,2,2);
    Dif (r!=1) {
	write_log("Illegal register %d in raw_shra_b\n",r);
	abort();
    }
    raw_shra_w_rr(d,r) ;
    unlock2(r);
    unlock2(d);
}
MENDFUNC(2,shra_w_rr,(RW2 d, R1 r)) 

MIDFUNC(2,shra_b_rr,(RW1 d, R1 r)) 
{ /* Can only do this with r==1, i.e. cl */
  
    if (isconst(r)) {
	COMPCALL(shra_b_ri)(d,(uae_u8)live.state[r].val);
	return;
    }

    CLOBBER_SHRA;
    r=readreg_specific(r,1,SHIFTCOUNT_NREG);
    d=rmw(d,1,1);
    Dif (r!=1) {
	write_log("Illegal register %d in raw_shra_b\n",r);
	abort();
    }
    raw_shra_b_rr(d,r) ;
    unlock2(r);
    unlock2(d);
}
MENDFUNC(2,shra_b_rr,(RW1 d, R1 r)) 


MIDFUNC(2,setcc,(W1 d, IMM cc))
{
    CLOBBER_SETCC;
    d=writereg(d,1);
    raw_setcc(d,cc);
    unlock2(d);
}
MENDFUNC(2,setcc,(W1 d, IMM cc))

MIDFUNC(2,setcc_m,(IMM d, IMM cc))
{
    CLOBBER_SETCC;
    raw_setcc_m(d,cc);
}
MENDFUNC(2,setcc_m,(IMM d, IMM cc))

MIDFUNC(3,cmov_b_rr,(RW1 d, R1 s, IMM cc))
{
    if (d==s)
	return;
    CLOBBER_CMOV;
    s=readreg(s,1);
    d=rmw(d,1,1);
    raw_cmov_b_rr(d,s,cc);
    unlock2(s);
    unlock2(d);
}
MENDFUNC(3,cmov_b_rr,(RW1 d, R1 s, IMM cc))

MIDFUNC(3,cmov_w_rr,(RW2 d, R2 s, IMM cc))
{
    if (d==s)
	return;
    CLOBBER_CMOV;
    s=readreg(s,2);
    d=rmw(d,2,2);
    raw_cmov_w_rr(d,s,cc);
    unlock2(s);
    unlock2(d);
}
MENDFUNC(3,cmov_w_rr,(RW2 d, R2 s, IMM cc))

MIDFUNC(3,cmov_l_rr,(RW4 d, R4 s, IMM cc))
{
    if (d==s)
	return;
    CLOBBER_CMOV;
    s=readreg(s,4);
    d=rmw(d,4,4);
    raw_cmov_l_rr(d,s,cc);
    unlock2(s);
    unlock2(d);
}
MENDFUNC(3,cmov_l_rr,(RW4 d, R4 s, IMM cc))

MIDFUNC(3,cmov_l_rm,(RW4 d, IMM s, IMM cc))
{
    CLOBBER_CMOV;
    d=rmw(d,4,4);
    raw_cmov_l_rm(d,s,cc);
    unlock2(d);
}
MENDFUNC(3,cmov_l_rm,(RW4 d, IMM s, IMM cc))

MIDFUNC(2,bsf_l_rr,(W4 d, W4 s))
{
    CLOBBER_BSF;
    s = readreg(s, 4);
    d = writereg(d, 4);
    raw_bsf_l_rr(d, s);
    unlock2(s);
    unlock2(d);
}
MENDFUNC(2,bsf_l_rr,(W4 d, W4 s))

/* Set the Z flag depending on the value in s. Note that the
   value has to be 0 or -1 (or, more precisely, for non-zero
   values, bit 14 must be set)! */
MIDFUNC(2,simulate_bsf,(W4 tmp, RW4 s))
{
    CLOBBER_BSF;
    s=rmw_specific(s,4,4,FLAG_NREG3);
    tmp=writereg(tmp,4);
    raw_flags_set_zero(s, tmp);
    unlock2(tmp);
    unlock2(s);
}
MENDFUNC(2,simulate_bsf,(W4 tmp, RW4 s))

MIDFUNC(2,imul_32_32,(RW4 d, R4 s))
{
    CLOBBER_MUL;
    s=readreg(s,4);
    d=rmw(d,4,4);
    raw_imul_32_32(d,s);
    unlock2(s);
    unlock2(d);
}
MENDFUNC(2,imul_32_32,(RW4 d, R4 s))

MIDFUNC(2,imul_64_32,(RW4 d, RW4 s))
{
    CLOBBER_MUL;
    s=rmw_specific(s,4,4,MUL_NREG2);
    d=rmw_specific(d,4,4,MUL_NREG1);
    raw_imul_64_32(d,s);
    unlock2(s);
    unlock2(d);
}
MENDFUNC(2,imul_64_32,(RW4 d, RW4 s))

MIDFUNC(2,mul_64_32,(RW4 d, RW4 s))
{
    CLOBBER_MUL;
    s=rmw_specific(s,4,4,MUL_NREG2);
    d=rmw_specific(d,4,4,MUL_NREG1);
    raw_mul_64_32(d,s);
    unlock2(s);
    unlock2(d);
}
MENDFUNC(2,mul_64_32,(RW4 d, RW4 s))

MIDFUNC(2,mul_32_32,(RW4 d, R4 s))
{
    CLOBBER_MUL;
    s=readreg(s,4);
    d=rmw(d,4,4);
    raw_mul_32_32(d,s);
    unlock2(s);
    unlock2(d);
}
MENDFUNC(2,mul_32_32,(RW4 d, R4 s))

#if SIZEOF_VOID_P == 8
MIDFUNC(2,sign_extend_32_rr,(W4 d, R2 s))
{
    int isrmw;

    if (isconst(s)) {
	set_const(d,(uae_s32)live.state[s].val);
	return;
    }

    CLOBBER_SE32;
    isrmw=(s==d);
    if (!isrmw) {
	s=readreg(s,4);
	d=writereg(d,4);
    }
    else {  /* If we try to lock this twice, with different sizes, we
	       are int trouble! */
	s=d=rmw(s,4,4);
    }
    raw_sign_extend_32_rr(d,s);
    if (!isrmw) {
	unlock2(d);
	unlock2(s);
    }
    else {
	unlock2(s);
    }
}
MENDFUNC(2,sign_extend_32_rr,(W4 d, R2 s))
#endif

MIDFUNC(2,sign_extend_16_rr,(W4 d, R2 s))
{
    int isrmw;

    if (isconst(s)) {
	set_const(d,(uae_s32)(uae_s16)live.state[s].val);
	return;
    }

    CLOBBER_SE16;
    isrmw=(s==d);
    if (!isrmw) {
	s=readreg(s,2);
	d=writereg(d,4);
    }
    else {  /* If we try to lock this twice, with different sizes, we
	       are int trouble! */
	s=d=rmw(s,4,2);
    }
    raw_sign_extend_16_rr(d,s);
    if (!isrmw) {
	unlock2(d);
	unlock2(s);
    }
    else {
	unlock2(s);
    }
}
MENDFUNC(2,sign_extend_16_rr,(W4 d, R2 s))

MIDFUNC(2,sign_extend_8_rr,(W4 d, R1 s))
{
    int isrmw;

    if (isconst(s)) {
	set_const(d,(uae_s32)(uae_s8)live.state[s].val);
	return;
    }

    isrmw=(s==d);
    CLOBBER_SE8;
    if (!isrmw) {
	s=readreg(s,1);
	d=writereg(d,4);
    }
    else {  /* If we try to lock this twice, with different sizes, we
	       are int trouble! */
	s=d=rmw(s,4,1);
    }
  
    raw_sign_extend_8_rr(d,s);

    if (!isrmw) {
	unlock2(d);
	unlock2(s);
    }
    else {
	unlock2(s);
    }
}
MENDFUNC(2,sign_extend_8_rr,(W4 d, R1 s))


MIDFUNC(2,zero_extend_16_rr,(W4 d, R2 s))
{
    int isrmw;

    if (isconst(s)) {
	set_const(d,(uae_u32)(uae_u16)live.state[s].val);
	return;
    }

    isrmw=(s==d);
    CLOBBER_ZE16;
    if (!isrmw) {
	s=readreg(s,2);
	d=writereg(d,4);
    }
    else {  /* If we try to lock this twice, with different sizes, we
	       are int trouble! */
	s=d=rmw(s,4,2);
    }
    raw_zero_extend_16_rr(d,s);
    if (!isrmw) {
	unlock2(d);
	unlock2(s);
    }
    else {
	unlock2(s);
    }
}
MENDFUNC(2,zero_extend_16_rr,(W4 d, R2 s))

MIDFUNC(2,zero_extend_8_rr,(W4 d, R1 s))
{
    int isrmw;
    if (isconst(s)) {
	set_const(d,(uae_u32)(uae_u8)live.state[s].val);
	return;
    }

    isrmw=(s==d);
    CLOBBER_ZE8;
    if (!isrmw) {
	s=readreg(s,1);
	d=writereg(d,4);
    }
    else {  /* If we try to lock this twice, with different sizes, we
	       are int trouble! */
	s=d=rmw(s,4,1);
    }
  
    raw_zero_extend_8_rr(d,s);

    if (!isrmw) {
	unlock2(d);
	unlock2(s);
    }
    else {
	unlock2(s);
    }
}
MENDFUNC(2,zero_extend_8_rr,(W4 d, R1 s))

MIDFUNC(2,mov_b_rr,(W1 d, R1 s))
{
    if (d==s)
	return;
    if (isconst(s)) {
	COMPCALL(mov_b_ri)(d,(uae_u8)live.state[s].val);
	return;
    }

    CLOBBER_MOV;
    s=readreg(s,1);
    d=writereg(d,1);
    raw_mov_b_rr(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,mov_b_rr,(W1 d, R1 s))

MIDFUNC(2,mov_w_rr,(W2 d, R2 s))
{
    if (d==s)
	return;
    if (isconst(s)) {
	COMPCALL(mov_w_ri)(d,(uae_u16)live.state[s].val);
	return;
    }

    CLOBBER_MOV;
    s=readreg(s,2);
    d=writereg(d,2);
    raw_mov_w_rr(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,mov_w_rr,(W2 d, R2 s))


MIDFUNC(4,mov_l_rrm_indexed,(W4 d,R4 baser, R4 index, IMM factor))
{
    CLOBBER_MOV;
    baser=readreg(baser,4);
    index=readreg(index,4);
    d=writereg(d,4);

    raw_mov_l_rrm_indexed(d,baser,index,factor);
    unlock2(d);
    unlock2(baser);
    unlock2(index);
}
MENDFUNC(4,mov_l_rrm_indexed,(W4 d,R4 baser, R4 index, IMM factor))

MIDFUNC(4,mov_w_rrm_indexed,(W2 d, R4 baser, R4 index, IMM factor))
{
    CLOBBER_MOV;
    baser=readreg(baser,4);
    index=readreg(index,4);
    d=writereg(d,2);

    raw_mov_w_rrm_indexed(d,baser,index,factor);
    unlock2(d);
    unlock2(baser);
    unlock2(index);
}
MENDFUNC(4,mov_w_rrm_indexed,(W2 d, R4 baser, R4 index, IMM factor))

MIDFUNC(4,mov_b_rrm_indexed,(W1 d, R4 baser, R4 index, IMM factor))
{
    CLOBBER_MOV;
    baser=readreg(baser,4);
    index=readreg(index,4);
    d=writereg(d,1);

    raw_mov_b_rrm_indexed(d,baser,index,factor);

    unlock2(d);
    unlock2(baser);
    unlock2(index);
}
MENDFUNC(4,mov_b_rrm_indexed,(W1 d, R4 baser, R4 index, IMM factor))


MIDFUNC(4,mov_l_mrr_indexed,(R4 baser, R4 index, IMM factor, R4 s))
{
    CLOBBER_MOV;
    baser=readreg(baser,4);
    index=readreg(index,4);
    s=readreg(s,4);

    Dif (baser==s || index==s) 
	abort();


    raw_mov_l_mrr_indexed(baser,index,factor,s);
    unlock2(s);
    unlock2(baser);
    unlock2(index);
}
MENDFUNC(4,mov_l_mrr_indexed,(R4 baser, R4 index, IMM factor, R4 s))

MIDFUNC(4,mov_w_mrr_indexed,(R4 baser, R4 index, IMM factor, R2 s))
{
    CLOBBER_MOV;
    baser=readreg(baser,4);
    index=readreg(index,4);
    s=readreg(s,2);

    raw_mov_w_mrr_indexed(baser,index,factor,s);
    unlock2(s);
    unlock2(baser);
    unlock2(index);
}
MENDFUNC(4,mov_w_mrr_indexed,(R4 baser, R4 index, IMM factor, R2 s))

MIDFUNC(4,mov_b_mrr_indexed,(R4 baser, R4 index, IMM factor, R1 s))
{
    CLOBBER_MOV;
    s=readreg(s,1);
    baser=readreg(baser,4);
    index=readreg(index,4);

    raw_mov_b_mrr_indexed(baser,index,factor,s);
    unlock2(s);
    unlock2(baser);
    unlock2(index);
}
MENDFUNC(4,mov_b_mrr_indexed,(R4 baser, R4 index, IMM factor, R1 s))


MIDFUNC(5,mov_l_bmrr_indexed,(IMM base, R4 baser, R4 index, IMM factor, R4 s))
{
    int basereg=baser;
    int indexreg=index;

    CLOBBER_MOV;
    s=readreg(s,4);
    baser=readreg_offset(baser,4);
    index=readreg_offset(index,4);

    base+=get_offset(basereg);
    base+=factor*get_offset(indexreg);

    raw_mov_l_bmrr_indexed(base,baser,index,factor,s);
    unlock2(s);
    unlock2(baser);
    unlock2(index);
}
MENDFUNC(5,mov_l_bmrr_indexed,(IMM base, R4 baser, R4 index, IMM factor, R4 s))

MIDFUNC(5,mov_w_bmrr_indexed,(IMM base, R4 baser, R4 index, IMM factor, R2 s))
{
    int basereg=baser;
    int indexreg=index;

    CLOBBER_MOV;
    s=readreg(s,2);
    baser=readreg_offset(baser,4);
    index=readreg_offset(index,4);

    base+=get_offset(basereg);
    base+=factor*get_offset(indexreg);

    raw_mov_w_bmrr_indexed(base,baser,index,factor,s);
    unlock2(s);
    unlock2(baser);
    unlock2(index);
}
MENDFUNC(5,mov_w_bmrr_indexed,(IMM base, R4 baser, R4 index, IMM factor, R2 s))

MIDFUNC(5,mov_b_bmrr_indexed,(IMM base, R4 baser, R4 index, IMM factor, R1 s))
{
    int basereg=baser;
    int indexreg=index;

    CLOBBER_MOV;
    s=readreg(s,1);
    baser=readreg_offset(baser,4);
    index=readreg_offset(index,4);

    base+=get_offset(basereg);
    base+=factor*get_offset(indexreg);

    raw_mov_b_bmrr_indexed(base,baser,index,factor,s);
    unlock2(s);
    unlock2(baser);
    unlock2(index);
}
MENDFUNC(5,mov_b_bmrr_indexed,(IMM base, R4 baser, R4 index, IMM factor, R1 s))



/* Read a long from base+baser+factor*index */
MIDFUNC(5,mov_l_brrm_indexed,(W4 d, IMM base, R4 baser, R4 index, IMM factor))
{
    int basereg=baser;
    int indexreg=index;

    CLOBBER_MOV;
    baser=readreg_offset(baser,4);
    index=readreg_offset(index,4);
    base+=get_offset(basereg);
    base+=factor*get_offset(indexreg);    
    d=writereg(d,4);
    raw_mov_l_brrm_indexed(d,base,baser,index,factor);
    unlock2(d);
    unlock2(baser);
    unlock2(index);
}
MENDFUNC(5,mov_l_brrm_indexed,(W4 d, IMM base, R4 baser, R4 index, IMM factor))


MIDFUNC(5,mov_w_brrm_indexed,(W2 d, IMM base, R4 baser, R4 index, IMM factor))
{
    int basereg=baser;
    int indexreg=index;

    CLOBBER_MOV;
    remove_offset(d,-1);
    baser=readreg_offset(baser,4);
    index=readreg_offset(index,4);
    base+=get_offset(basereg);
    base+=factor*get_offset(indexreg);    
    d=writereg(d,2);
    raw_mov_w_brrm_indexed(d,base,baser,index,factor);
    unlock2(d);
    unlock2(baser);
    unlock2(index);
}
MENDFUNC(5,mov_w_brrm_indexed,(W2 d, IMM base, R4 baser, R4 index, IMM factor))


MIDFUNC(5,mov_b_brrm_indexed,(W1 d, IMM base, R4 baser, R4 index, IMM factor))
{
    int basereg=baser;
    int indexreg=index;

    CLOBBER_MOV;
    remove_offset(d,-1);
    baser=readreg_offset(baser,4);
    index=readreg_offset(index,4);
    base+=get_offset(basereg);
    base+=factor*get_offset(indexreg);    
    d=writereg(d,1);
    raw_mov_b_brrm_indexed(d,base,baser,index,factor);
    unlock2(d);
    unlock2(baser);
    unlock2(index);
}
MENDFUNC(5,mov_b_brrm_indexed,(W1 d, IMM base, R4 baser, R4 index, IMM factor))

/* Read a long from base+factor*index */
MIDFUNC(4,mov_l_rm_indexed,(W4 d, IMM base, R4 index, IMM factor))
{
    int indexreg=index;

    if (isconst(index)) {
	COMPCALL(mov_l_rm)(d,base+factor*live.state[index].val);
	return;
    }

    CLOBBER_MOV;
    index=readreg_offset(index,4);
    base+=get_offset(indexreg)*factor;
    d=writereg(d,4);

    raw_mov_l_rm_indexed(d,base,index,factor);
    unlock2(index);
    unlock2(d);
}
MENDFUNC(4,mov_l_rm_indexed,(W4 d, IMM base, R4 index, IMM factor))


/* read the long at the address contained in s+offset and store in d */
MIDFUNC(3,mov_l_rR,(W4 d, R4 s, IMM offset))
{
    if (isconst(s)) {
	COMPCALL(mov_l_rm)(d,live.state[s].val+offset);
	return;
    }
    CLOBBER_MOV;
    s=readreg(s,4);
    d=writereg(d,4);

    raw_mov_l_rR(d,s,offset);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(3,mov_l_rR,(W4 d, R4 s, IMM offset))

/* read the word at the address contained in s+offset and store in d */
MIDFUNC(3,mov_w_rR,(W2 d, R4 s, IMM offset))
{
    if (isconst(s)) {
	COMPCALL(mov_w_rm)(d,live.state[s].val+offset);
	return;
    }
    CLOBBER_MOV;
    s=readreg(s,4);
    d=writereg(d,2);

    raw_mov_w_rR(d,s,offset);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(3,mov_w_rR,(W2 d, R4 s, IMM offset))

/* read the word at the address contained in s+offset and store in d */
MIDFUNC(3,mov_b_rR,(W1 d, R4 s, IMM offset))
{
    if (isconst(s)) {
	COMPCALL(mov_b_rm)(d,live.state[s].val+offset);
	return;
    }
    CLOBBER_MOV;
    s=readreg(s,4);
    d=writereg(d,1);

    raw_mov_b_rR(d,s,offset);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(3,mov_b_rR,(W1 d, R4 s, IMM offset))

/* read the long at the address contained in s+offset and store in d */
MIDFUNC(3,mov_l_brR,(W4 d, R4 s, IMM offset))
{
    int sreg=s;
    if (isconst(s)) {
	COMPCALL(mov_l_rm)(d,live.state[s].val+offset);
	return;
    }
    CLOBBER_MOV;
    s=readreg_offset(s,4);
    offset+=get_offset(sreg);
    d=writereg(d,4);
    
    raw_mov_l_brR(d,s,offset);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(3,mov_l_brR,(W4 d, R4 s, IMM offset))

/* read the word at the address contained in s+offset and store in d */
MIDFUNC(3,mov_w_brR,(W2 d, R4 s, IMM offset))
{
    int sreg=s;
    if (isconst(s)) {
	COMPCALL(mov_w_rm)(d,live.state[s].val+offset);
	return;
    }
    CLOBBER_MOV;
    remove_offset(d,-1);
    s=readreg_offset(s,4);
    offset+=get_offset(sreg);
    d=writereg(d,2);

    raw_mov_w_brR(d,s,offset);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(3,mov_w_brR,(W2 d, R4 s, IMM offset))

/* read the word at the address contained in s+offset and store in d */
MIDFUNC(3,mov_b_brR,(W1 d, R4 s, IMM offset))
{
    int sreg=s;
    if (isconst(s)) {
	COMPCALL(mov_b_rm)(d,live.state[s].val+offset);
	return;
    }
    CLOBBER_MOV;
    remove_offset(d,-1);
    s=readreg_offset(s,4);
    offset+=get_offset(sreg);
    d=writereg(d,1);

    raw_mov_b_brR(d,s,offset);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(3,mov_b_brR,(W1 d, R4 s, IMM offset))

MIDFUNC(3,mov_l_Ri,(R4 d, IMM i, IMM offset))
{
    int dreg=d;
    if (isconst(d)) {
	COMPCALL(mov_l_mi)(live.state[d].val+offset,i);
	return;
    }

    CLOBBER_MOV;
    d=readreg_offset(d,4);
    offset+=get_offset(dreg);
    raw_mov_l_Ri(d,i,offset);
    unlock2(d);
}
MENDFUNC(3,mov_l_Ri,(R4 d, IMM i, IMM offset))

MIDFUNC(3,mov_w_Ri,(R4 d, IMM i, IMM offset))
{
    int dreg=d;
    if (isconst(d)) {
	COMPCALL(mov_w_mi)(live.state[d].val+offset,i);
	return;
    }

    CLOBBER_MOV;
    d=readreg_offset(d,4);
    offset+=get_offset(dreg);
    raw_mov_w_Ri(d,i,offset);
    unlock2(d);
}
MENDFUNC(3,mov_w_Ri,(R4 d, IMM i, IMM offset))

MIDFUNC(3,mov_b_Ri,(R4 d, IMM i, IMM offset))
{
    int dreg=d;
    if (isconst(d)) {
	COMPCALL(mov_b_mi)(live.state[d].val+offset,i);
	return;
    }

    CLOBBER_MOV;
    d=readreg_offset(d,4);
    offset+=get_offset(dreg);
    raw_mov_b_Ri(d,i,offset);
    unlock2(d);
}
MENDFUNC(3,mov_b_Ri,(R4 d, IMM i, IMM offset))

     /* Warning! OFFSET is byte sized only! */
MIDFUNC(3,mov_l_Rr,(R4 d, R4 s, IMM offset))
{
    if (isconst(d)) {
	COMPCALL(mov_l_mr)(live.state[d].val+offset,s);
	return;
    }
    if (isconst(s)) {
	COMPCALL(mov_l_Ri)(d,live.state[s].val,offset);
	return;
    }

    CLOBBER_MOV;
    s=readreg(s,4);
    d=readreg(d,4);

    raw_mov_l_Rr(d,s,offset);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(3,mov_l_Rr,(R4 d, R4 s, IMM offset))

MIDFUNC(3,mov_w_Rr,(R4 d, R2 s, IMM offset))
{
    if (isconst(d)) {
	COMPCALL(mov_w_mr)(live.state[d].val+offset,s);
	return;
    }
    if (isconst(s)) {
	COMPCALL(mov_w_Ri)(d,(uae_u16)live.state[s].val,offset);
	return;
    }

    CLOBBER_MOV;
    s=readreg(s,2);
    d=readreg(d,4);
    raw_mov_w_Rr(d,s,offset);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(3,mov_w_Rr,(R4 d, R2 s, IMM offset))

MIDFUNC(3,mov_b_Rr,(R4 d, R1 s, IMM offset))
{
    if (isconst(d)) {
	COMPCALL(mov_b_mr)(live.state[d].val+offset,s);
	return;
    }
    if (isconst(s)) {
	COMPCALL(mov_b_Ri)(d,(uae_u8)live.state[s].val,offset);
	return;
    }

    CLOBBER_MOV;
    s=readreg(s,1);
    d=readreg(d,4);
    raw_mov_b_Rr(d,s,offset);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(3,mov_b_Rr,(R4 d, R1 s, IMM offset))

MIDFUNC(3,lea_l_brr,(W4 d, R4 s, IMM offset))
{
    if (isconst(s)) {
	COMPCALL(mov_l_ri)(d,live.state[s].val+offset);
	return;
    }
#if USE_OFFSET
    if (d==s) {
	add_offset(d,offset);
	return;
    }
#endif
    CLOBBER_LEA;
    s=readreg(s,4);
    d=writereg(d,4);
    raw_lea_l_brr(d,s,offset);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(3,lea_l_brr,(W4 d, R4 s, IMM offset))

MIDFUNC(5,lea_l_brr_indexed,(W4 d, R4 s, R4 index, IMM factor, IMM offset))
{
    if (!offset) {
	COMPCALL(lea_l_rr_indexed)(d,s,index,factor);
	return;
    }
    CLOBBER_LEA;
    s=readreg(s,4);
    index=readreg(index,4);
    d=writereg(d,4);

    raw_lea_l_brr_indexed(d,s,index,factor,offset);
    unlock2(d);
    unlock2(index);
    unlock2(s);
}
MENDFUNC(5,lea_l_brr_indexed,(W4 d, R4 s, R4 index, IMM factor, IMM offset))

MIDFUNC(4,lea_l_rr_indexed,(W4 d, R4 s, R4 index, IMM factor))
{
    CLOBBER_LEA;
    s=readreg(s,4);
    index=readreg(index,4);
    d=writereg(d,4);

    raw_lea_l_rr_indexed(d,s,index,factor);
    unlock2(d);
    unlock2(index);
    unlock2(s);
}
MENDFUNC(4,lea_l_rr_indexed,(W4 d, R4 s, R4 index, IMM factor))

/* write d to the long at the address contained in s+offset */
MIDFUNC(3,mov_l_bRr,(R4 d, R4 s, IMM offset))
{
    int dreg=d;
    if (isconst(d)) {
	COMPCALL(mov_l_mr)(live.state[d].val+offset,s);
	return;
    }

    CLOBBER_MOV;
    s=readreg(s,4);
    d=readreg_offset(d,4);
    offset+=get_offset(dreg);

    raw_mov_l_bRr(d,s,offset);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(3,mov_l_bRr,(R4 d, R4 s, IMM offset))

/* write the word at the address contained in s+offset and store in d */
MIDFUNC(3,mov_w_bRr,(R4 d, R2 s, IMM offset))
{
    int dreg=d;

    if (isconst(d)) {
	COMPCALL(mov_w_mr)(live.state[d].val+offset,s);
	return;
    }

    CLOBBER_MOV;
    s=readreg(s,2);
    d=readreg_offset(d,4);
    offset+=get_offset(dreg);
    raw_mov_w_bRr(d,s,offset);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(3,mov_w_bRr,(R4 d, R2 s, IMM offset))

MIDFUNC(3,mov_b_bRr,(R4 d, R1 s, IMM offset))
{
    int dreg=d;
    if (isconst(d)) {
	COMPCALL(mov_b_mr)(live.state[d].val+offset,s);
	return;
    }

    CLOBBER_MOV;
    s=readreg(s,1);
    d=readreg_offset(d,4);
    offset+=get_offset(dreg);
    raw_mov_b_bRr(d,s,offset);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(3,mov_b_bRr,(R4 d, R1 s, IMM offset))

MIDFUNC(1,bswap_32,(RW4 r))
{
    int reg=r;

    if (isconst(r)) {
	uae_u32 oldv=live.state[r].val;
	live.state[r].val=reverse32(oldv);
	return;
    }
    
    CLOBBER_SW32;
    r=rmw(r,4,4);  
    raw_bswap_32(r);
    unlock2(r);
}
MENDFUNC(1,bswap_32,(RW4 r))

MIDFUNC(1,bswap_16,(RW2 r))
{
    if (isconst(r)) {
	uae_u32 oldv=live.state[r].val;
	live.state[r].val=((oldv>>8)&0xff) | ((oldv<<8)&0xff00) |
	    (oldv&0xffff0000);
	return;
    }

    CLOBBER_SW16;
    r=rmw(r,2,2);
  
    raw_bswap_16(r);
    unlock2(r);
}
MENDFUNC(1,bswap_16,(RW2 r))



MIDFUNC(2,mov_l_rr,(W4 d, R4 s))
{
    int olds;

    if (d==s) { /* How pointless! */
	return;
    }
    if (isconst(s)) {
	COMPCALL(mov_l_ri)(d,live.state[s].val);
	return;
    }
    olds=s;
    disassociate(d);
    s=readreg_offset(s,4);
    live.state[d].realreg=s;
    live.state[d].realind=live.nat[s].nholds;
    live.state[d].val=live.state[olds].val;
    live.state[d].validsize=4;
    live.state[d].dirtysize=4;
    set_status(d,DIRTY);

    live.nat[s].holds[live.nat[s].nholds]=d;
    live.nat[s].nholds++;
    log_clobberreg(d);
    /* write_log("Added %d to nreg %d(%d), now holds %d regs\n",
       d,s,live.state[d].realind,live.nat[s].nholds); */
    unlock2(s);
}
MENDFUNC(2,mov_l_rr,(W4 d, R4 s))

MIDFUNC(2,mov_l_mr,(IMM d, R4 s))
{
    if (isconst(s)) {
	COMPCALL(mov_l_mi)(d,live.state[s].val);
	return;
    }
    CLOBBER_MOV;
    s=readreg(s,4);

    raw_mov_l_mr(d,s);
    unlock2(s);
}
MENDFUNC(2,mov_l_mr,(IMM d, R4 s))


MIDFUNC(2,mov_w_mr,(IMM d, R2 s))
{
    if (isconst(s)) {
	COMPCALL(mov_w_mi)(d,(uae_u16)live.state[s].val);
	return;
    }
    CLOBBER_MOV;
    s=readreg(s,2);

    raw_mov_w_mr(d,s);
    unlock2(s);
}
MENDFUNC(2,mov_w_mr,(IMM d, R2 s))

MIDFUNC(2,mov_w_rm,(W2 d, IMM s))
{
    CLOBBER_MOV;
    d=writereg(d,2);

    raw_mov_w_rm(d,s);
    unlock2(d);
}
MENDFUNC(2,mov_w_rm,(W2 d, IMM s))

MIDFUNC(2,mov_b_mr,(IMM d, R1 s))
{
    if (isconst(s)) {
	COMPCALL(mov_b_mi)(d,(uae_u8)live.state[s].val);
	return;
    }

    CLOBBER_MOV;
    s=readreg(s,1);

    raw_mov_b_mr(d,s);
    unlock2(s);
}
MENDFUNC(2,mov_b_mr,(IMM d, R1 s))

MIDFUNC(2,mov_b_rm,(W1 d, IMM s))
{
    CLOBBER_MOV;
    d=writereg(d,1);

    raw_mov_b_rm(d,s);
    unlock2(d);
}
MENDFUNC(2,mov_b_rm,(W1 d, IMM s))

MIDFUNC(2,mov_l_ri,(W4 d, IMM s))
{
    set_const(d,s);
    return;
}
MENDFUNC(2,mov_l_ri,(W4 d, IMM s))

MIDFUNC(2,mov_w_ri,(W2 d, IMM s))
{
    CLOBBER_MOV;
    d=writereg(d,2);

    raw_mov_w_ri(d,s);
    unlock2(d);
}
MENDFUNC(2,mov_w_ri,(W2 d, IMM s))

MIDFUNC(2,mov_b_ri,(W1 d, IMM s))
{
    CLOBBER_MOV;
    d=writereg(d,1);

    raw_mov_b_ri(d,s);
    unlock2(d);
}
MENDFUNC(2,mov_b_ri,(W1 d, IMM s))


MIDFUNC(2,add_l_mi,(IMM d, IMM s)) 
{
    CLOBBER_ADD;
    raw_add_l_mi(d,s) ;
}
MENDFUNC(2,add_l_mi,(IMM d, IMM s)) 

MIDFUNC(2,add_w_mi,(IMM d, IMM s)) 
{
    CLOBBER_ADD;
    raw_add_w_mi(d,s) ;
}
MENDFUNC(2,add_w_mi,(IMM d, IMM s)) 

MIDFUNC(2,add_b_mi,(IMM d, IMM s)) 
{
    CLOBBER_ADD;
    raw_add_b_mi(d,s) ;
}
MENDFUNC(2,add_b_mi,(IMM d, IMM s)) 


MIDFUNC(2,test_l_ri,(R4 d, IMM i))
{
    CLOBBER_TEST;
    d=readreg(d,4);

    raw_test_l_ri(d,i);
    unlock2(d);
}
MENDFUNC(2,test_l_ri,(R4 d, IMM i))

MIDFUNC(2,test_l_rr,(R4 d, R4 s))
{
    CLOBBER_TEST;
    d=readreg(d,4);
    s=readreg(s,4);

    raw_test_l_rr(d,s);;
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,test_l_rr,(R4 d, R4 s))

MIDFUNC(2,test_w_rr,(R2 d, R2 s))
{
    CLOBBER_TEST;
    d=readreg(d,2);
    s=readreg(s,2);

    raw_test_w_rr(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,test_w_rr,(R2 d, R2 s))

MIDFUNC(2,test_b_rr,(R1 d, R1 s))
{
    CLOBBER_TEST;
    d=readreg(d,1);
    s=readreg(s,1);

    raw_test_b_rr(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,test_b_rr,(R1 d, R1 s))


MIDFUNC(2,and_l_ri,(RW4 d, IMM i))
{
	if (isconst(d) && !needflags) {
		live.state[d].val &= i;
		return;
	}

    CLOBBER_AND;
    d=rmw(d,4,4);

    raw_and_l_ri(d,i);
    unlock2(d);
}
MENDFUNC(2,and_l_ri,(RW4 d, IMM i))

MIDFUNC(2,and_l,(RW4 d, R4 s))
{
    CLOBBER_AND;
    s=readreg(s,4);
    d=rmw(d,4,4);

    raw_and_l(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,and_l,(RW4 d, R4 s))

MIDFUNC(2,and_w,(RW2 d, R2 s))
{
    CLOBBER_AND;
    s=readreg(s,2);
    d=rmw(d,2,2);

    raw_and_w(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,and_w,(RW2 d, R2 s))

MIDFUNC(2,and_b,(RW1 d, R1 s))
{
    CLOBBER_AND;
    s=readreg(s,1);
    d=rmw(d,1,1);

    raw_and_b(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,and_b,(RW1 d, R1 s))

// gb-- used for making an fpcr value in compemu_fpp.cpp
MIDFUNC(2,or_l_rm,(RW4 d, IMM s))
{
    CLOBBER_OR;
    d=rmw(d,4,4);
	
    raw_or_l_rm(d,s);
    unlock2(d);
}
MENDFUNC(2,or_l_rm,(RW4 d, IMM s))

MIDFUNC(2,or_l_ri,(RW4 d, IMM i))
{
    if (isconst(d) && !needflags) {
	live.state[d].val|=i;
	return;
    }
    CLOBBER_OR;
    d=rmw(d,4,4);

    raw_or_l_ri(d,i);
    unlock2(d);
}
MENDFUNC(2,or_l_ri,(RW4 d, IMM i))

MIDFUNC(2,or_l,(RW4 d, R4 s))
{
    if (isconst(d) && isconst(s) && !needflags) {
	live.state[d].val|=live.state[s].val;
	return;
    }
    CLOBBER_OR;
    s=readreg(s,4);
    d=rmw(d,4,4);

    raw_or_l(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,or_l,(RW4 d, R4 s))

MIDFUNC(2,or_w,(RW2 d, R2 s))
{
    CLOBBER_OR;
    s=readreg(s,2);
    d=rmw(d,2,2);

    raw_or_w(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,or_w,(RW2 d, R2 s))

MIDFUNC(2,or_b,(RW1 d, R1 s))
{
    CLOBBER_OR;
    s=readreg(s,1);
    d=rmw(d,1,1);

    raw_or_b(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,or_b,(RW1 d, R1 s))

MIDFUNC(2,adc_l,(RW4 d, R4 s))
{
    CLOBBER_ADC;
    s=readreg(s,4);
    d=rmw(d,4,4);

    raw_adc_l(d,s);

    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,adc_l,(RW4 d, R4 s))

MIDFUNC(2,adc_w,(RW2 d, R2 s))
{
    CLOBBER_ADC;
    s=readreg(s,2);
    d=rmw(d,2,2);

    raw_adc_w(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,adc_w,(RW2 d, R2 s))

MIDFUNC(2,adc_b,(RW1 d, R1 s))
{
    CLOBBER_ADC;
    s=readreg(s,1);
    d=rmw(d,1,1);

    raw_adc_b(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,adc_b,(RW1 d, R1 s))

MIDFUNC(2,add_l,(RW4 d, R4 s))
{
    if (isconst(s)) {
	COMPCALL(add_l_ri)(d,live.state[s].val);
	return;
    }

    CLOBBER_ADD;
    s=readreg(s,4);
    d=rmw(d,4,4);

    raw_add_l(d,s);

    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,add_l,(RW4 d, R4 s))

MIDFUNC(2,add_w,(RW2 d, R2 s))
{
    if (isconst(s)) {
	COMPCALL(add_w_ri)(d,(uae_u16)live.state[s].val);
	return;
    }

    CLOBBER_ADD;
    s=readreg(s,2);
    d=rmw(d,2,2);

    raw_add_w(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,add_w,(RW2 d, R2 s))

MIDFUNC(2,add_b,(RW1 d, R1 s))
{
    if (isconst(s)) {
	COMPCALL(add_b_ri)(d,(uae_u8)live.state[s].val);
	return;
    }

    CLOBBER_ADD;
    s=readreg(s,1);
    d=rmw(d,1,1);

    raw_add_b(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,add_b,(RW1 d, R1 s))

MIDFUNC(2,sub_l_ri,(RW4 d, IMM i))
{
    if (!i && !needflags)
	return;
    if (isconst(d) && !needflags) {
	live.state[d].val-=i;
	return;
    }
#if USE_OFFSET 
    if (!needflags) {
	add_offset(d,-i);
	return;
    }
#endif

    CLOBBER_SUB;
    d=rmw(d,4,4);

    raw_sub_l_ri(d,i);
    unlock2(d);
}
MENDFUNC(2,sub_l_ri,(RW4 d, IMM i))

MIDFUNC(2,sub_w_ri,(RW2 d, IMM i))
{
    if (!i && !needflags)
	return;

    CLOBBER_SUB;
    d=rmw(d,2,2);

    raw_sub_w_ri(d,i);
    unlock2(d);
}
MENDFUNC(2,sub_w_ri,(RW2 d, IMM i))

MIDFUNC(2,sub_b_ri,(RW1 d, IMM i))
{
    if (!i && !needflags)
	return;

    CLOBBER_SUB;
    d=rmw(d,1,1);

    raw_sub_b_ri(d,i);

    unlock2(d);
}
MENDFUNC(2,sub_b_ri,(RW1 d, IMM i))

MIDFUNC(2,add_l_ri,(RW4 d, IMM i))
{
    if (!i && !needflags)
	return;
    if (isconst(d) && !needflags) {
	live.state[d].val+=i;
	return;
    }
#if USE_OFFSET 
    if (!needflags) {
	add_offset(d,i);
	return;
    }
#endif
    CLOBBER_ADD;
    d=rmw(d,4,4);
    raw_add_l_ri(d,i);
    unlock2(d);
}
MENDFUNC(2,add_l_ri,(RW4 d, IMM i))

MIDFUNC(2,add_w_ri,(RW2 d, IMM i))
{
    if (!i && !needflags)
	return;

    CLOBBER_ADD;
    d=rmw(d,2,2);

    raw_add_w_ri(d,i);
    unlock2(d);
}
MENDFUNC(2,add_w_ri,(RW2 d, IMM i))

MIDFUNC(2,add_b_ri,(RW1 d, IMM i))
{
    if (!i && !needflags)
	return;

    CLOBBER_ADD;
    d=rmw(d,1,1);

    raw_add_b_ri(d,i);

    unlock2(d);
}
MENDFUNC(2,add_b_ri,(RW1 d, IMM i))

MIDFUNC(2,sbb_l,(RW4 d, R4 s))
{
    CLOBBER_SBB;
    s=readreg(s,4);
    d=rmw(d,4,4);

    raw_sbb_l(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,sbb_l,(RW4 d, R4 s))

MIDFUNC(2,sbb_w,(RW2 d, R2 s))
{
    CLOBBER_SBB;
    s=readreg(s,2);
    d=rmw(d,2,2);

    raw_sbb_w(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,sbb_w,(RW2 d, R2 s))

MIDFUNC(2,sbb_b,(RW1 d, R1 s))
{
    CLOBBER_SBB;
    s=readreg(s,1);
    d=rmw(d,1,1);

    raw_sbb_b(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,sbb_b,(RW1 d, R1 s))

MIDFUNC(2,sub_l,(RW4 d, R4 s))
{
    if (isconst(s)) {
	COMPCALL(sub_l_ri)(d,live.state[s].val);
	return;
    }

    CLOBBER_SUB;
    s=readreg(s,4);
    d=rmw(d,4,4);

    raw_sub_l(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,sub_l,(RW4 d, R4 s))

MIDFUNC(2,sub_w,(RW2 d, R2 s))
{
    if (isconst(s)) {
	COMPCALL(sub_w_ri)(d,(uae_u16)live.state[s].val);
	return;
    }

    CLOBBER_SUB;
    s=readreg(s,2);
    d=rmw(d,2,2);

    raw_sub_w(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,sub_w,(RW2 d, R2 s))

MIDFUNC(2,sub_b,(RW1 d, R1 s))
{
    if (isconst(s)) {
	COMPCALL(sub_b_ri)(d,(uae_u8)live.state[s].val);
	return;
    }

    CLOBBER_SUB;
    s=readreg(s,1);
    d=rmw(d,1,1);

    raw_sub_b(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,sub_b,(RW1 d, R1 s))

MIDFUNC(2,cmp_l,(R4 d, R4 s))
{
    CLOBBER_CMP;
    s=readreg(s,4);
    d=readreg(d,4);

    raw_cmp_l(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,cmp_l,(R4 d, R4 s))

MIDFUNC(2,cmp_l_ri,(R4 r, IMM i))
{
    CLOBBER_CMP;
    r=readreg(r,4);

    raw_cmp_l_ri(r,i);
    unlock2(r);
}
MENDFUNC(2,cmp_l_ri,(R4 r, IMM i))

MIDFUNC(2,cmp_w,(R2 d, R2 s))
{
    CLOBBER_CMP;
    s=readreg(s,2);
    d=readreg(d,2);

    raw_cmp_w(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,cmp_w,(R2 d, R2 s))

MIDFUNC(2,cmp_b,(R1 d, R1 s))
{
    CLOBBER_CMP;
    s=readreg(s,1);
    d=readreg(d,1);

    raw_cmp_b(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,cmp_b,(R1 d, R1 s))


MIDFUNC(2,xor_l,(RW4 d, R4 s))
{
    CLOBBER_XOR;
    s=readreg(s,4);
    d=rmw(d,4,4);

    raw_xor_l(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,xor_l,(RW4 d, R4 s))

MIDFUNC(2,xor_w,(RW2 d, R2 s))
{
    CLOBBER_XOR;
    s=readreg(s,2);
    d=rmw(d,2,2);

    raw_xor_w(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,xor_w,(RW2 d, R2 s))

MIDFUNC(2,xor_b,(RW1 d, R1 s))
{
    CLOBBER_XOR;
    s=readreg(s,1);
    d=rmw(d,1,1);

    raw_xor_b(d,s);
    unlock2(d);
    unlock2(s);
}
MENDFUNC(2,xor_b,(RW1 d, R1 s))

MIDFUNC(5,call_r_11,(W4 out1, R4 r, R4 in1, IMM osize, IMM isize))
{
    clobber_flags();
    remove_all_offsets();
    if (osize==4) {
	if (out1!=in1 && out1!=r) {
	    COMPCALL(forget_about)(out1);
	}
    }
    else {
	tomem_c(out1);
    }

    in1=readreg_specific(in1,isize,REG_PAR1);
    r=readreg(r,4);
    prepare_for_call_1();  /* This should ensure that there won't be
			      any need for swapping nregs in prepare_for_call_2
			   */
#if USE_NORMAL_CALLING_CONVENTION
    raw_push_l_r(in1);
#endif
    unlock2(in1);
    unlock2(r);

    prepare_for_call_2();
    raw_call_r(r);

#if USE_NORMAL_CALLING_CONVENTION
    raw_inc_sp(4);
#endif


    live.nat[REG_RESULT].holds[0]=out1;
    live.nat[REG_RESULT].nholds=1;
    live.nat[REG_RESULT].touched=touchcnt++;

    live.state[out1].realreg=REG_RESULT;
    live.state[out1].realind=0;
    live.state[out1].val=0;
    live.state[out1].validsize=osize;
    live.state[out1].dirtysize=osize;
    set_status(out1,DIRTY);
}
MENDFUNC(5,call_r_11,(W4 out1, R4 r, R4 in1, IMM osize, IMM isize))

MIDFUNC(5,call_r_02,(R4 r, R4 in1, R4 in2, IMM isize1, IMM isize2))
{
    clobber_flags();
    remove_all_offsets();
    in1=readreg_specific(in1,isize1,REG_PAR1);
    in2=readreg_specific(in2,isize2,REG_PAR2);
    r=readreg(r,4);
    prepare_for_call_1();  /* This should ensure that there won't be
			      any need for swapping nregs in prepare_for_call_2
			   */
#if USE_NORMAL_CALLING_CONVENTION
    raw_push_l_r(in2);
    raw_push_l_r(in1);
#endif
    unlock2(r);
    unlock2(in1);
    unlock2(in2);
    prepare_for_call_2();
    raw_call_r(r);
#if USE_NORMAL_CALLING_CONVENTION
    raw_inc_sp(8);
#endif
}
MENDFUNC(5,call_r_02,(R4 r, R4 in1, R4 in2, IMM isize1, IMM isize2))

/* forget_about() takes a mid-layer register */
MIDFUNC(1,forget_about,(W4 r))
{
    if (isinreg(r))
	disassociate(r);
    live.state[r].val=0;
    set_status(r,UNDEF);
}
MENDFUNC(1,forget_about,(W4 r))

MIDFUNC(0,nop,(void))
{
    raw_nop();
}
MENDFUNC(0,nop,(void))


MIDFUNC(1,f_forget_about,(FW r))
{
    if (f_isinreg(r))
	f_disassociate(r);
    live.fate[r].status=UNDEF;
}
MENDFUNC(1,f_forget_about,(FW r))

MIDFUNC(1,fmov_pi,(FW r))
{
    r=f_writereg(r);
    raw_fmov_pi(r);
    f_unlock(r);
}
MENDFUNC(1,fmov_pi,(FW r))

MIDFUNC(1,fmov_log10_2,(FW r))
{
    r=f_writereg(r);
    raw_fmov_log10_2(r);
    f_unlock(r);
}
MENDFUNC(1,fmov_log10_2,(FW r))

MIDFUNC(1,fmov_log2_e,(FW r))
{
    r=f_writereg(r);
    raw_fmov_log2_e(r);
    f_unlock(r);
}
MENDFUNC(1,fmov_log2_e,(FW r))

MIDFUNC(1,fmov_loge_2,(FW r))
{
    r=f_writereg(r);
    raw_fmov_loge_2(r);
    f_unlock(r);
}
MENDFUNC(1,fmov_loge_2,(FW r))

MIDFUNC(1,fmov_1,(FW r))
{
    r=f_writereg(r);
    raw_fmov_1(r);
    f_unlock(r);
}
MENDFUNC(1,fmov_1,(FW r))

MIDFUNC(1,fmov_0,(FW r))
{
    r=f_writereg(r);
    raw_fmov_0(r);
    f_unlock(r);
}
MENDFUNC(1,fmov_0,(FW r))

MIDFUNC(2,fmov_rm,(FW r, MEMR m))
{
    r=f_writereg(r);
    raw_fmov_rm(r,m);
    f_unlock(r);
}
MENDFUNC(2,fmov_rm,(FW r, MEMR m))

MIDFUNC(2,fmovi_rm,(FW r, MEMR m))
{
    r=f_writereg(r);
    raw_fmovi_rm(r,m);
    f_unlock(r);
}
MENDFUNC(2,fmovi_rm,(FW r, MEMR m))

MIDFUNC(2,fmovi_mr,(MEMW m, FR r))
{
    r=f_readreg(r);
    raw_fmovi_mr(m,r);
    f_unlock(r);
}
MENDFUNC(2,fmovi_mr,(MEMW m, FR r))

MIDFUNC(2,fmovs_rm,(FW r, MEMR m))
{
    r=f_writereg(r);
    raw_fmovs_rm(r,m);
    f_unlock(r);
}
MENDFUNC(2,fmovs_rm,(FW r, MEMR m))

MIDFUNC(2,fmovs_mr,(MEMW m, FR r))
{
    r=f_readreg(r);
    raw_fmovs_mr(m,r);
    f_unlock(r);
}
MENDFUNC(2,fmovs_mr,(MEMW m, FR r))

MIDFUNC(2,fmov_ext_mr,(MEMW m, FR r))
{
    r=f_readreg(r);
    raw_fmov_ext_mr(m,r);
    f_unlock(r);
}
MENDFUNC(2,fmov_ext_mr,(MEMW m, FR r))

MIDFUNC(2,fmov_mr,(MEMW m, FR r))
{
    r=f_readreg(r);
    raw_fmov_mr(m,r);
    f_unlock(r);
}
MENDFUNC(2,fmov_mr,(MEMW m, FR r))

MIDFUNC(2,fmov_ext_rm,(FW r, MEMR m))
{
    r=f_writereg(r);
    raw_fmov_ext_rm(r,m);
    f_unlock(r);
}
MENDFUNC(2,fmov_ext_rm,(FW r, MEMR m))

MIDFUNC(2,fmov_rr,(FW d, FR s))
{
    if (d==s) { /* How pointless! */
	return;
    }
#if USE_F_ALIAS
    f_disassociate(d);
    s=f_readreg(s);
    live.fate[d].realreg=s;
    live.fate[d].realind=live.fat[s].nholds;
    live.fate[d].status=DIRTY;
    live.fat[s].holds[live.fat[s].nholds]=d;
    live.fat[s].nholds++;
    f_unlock(s);
#else
    s=f_readreg(s);
    d=f_writereg(d);
    raw_fmov_rr(d,s);
    f_unlock(s);
    f_unlock(d);
#endif
}
MENDFUNC(2,fmov_rr,(FW d, FR s))

MIDFUNC(2,fldcw_m_indexed,(R4 index, IMM base))
{
    index=readreg(index,4);

    raw_fldcw_m_indexed(index,base);
    unlock2(index);
}
MENDFUNC(2,fldcw_m_indexed,(R4 index, IMM base))

MIDFUNC(1,ftst_r,(FR r))
{
    r=f_readreg(r);
    raw_ftst_r(r);
    f_unlock(r);
}
MENDFUNC(1,ftst_r,(FR r))

MIDFUNC(0,dont_care_fflags,(void))
{
    f_disassociate(FP_RESULT);
}
MENDFUNC(0,dont_care_fflags,(void))

MIDFUNC(2,fsqrt_rr,(FW d, FR s))
{
    s=f_readreg(s);
    d=f_writereg(d);
    raw_fsqrt_rr(d,s);
    f_unlock(s);
    f_unlock(d);
}
MENDFUNC(2,fsqrt_rr,(FW d, FR s))

MIDFUNC(2,fabs_rr,(FW d, FR s))
{
    s=f_readreg(s);
    d=f_writereg(d);
    raw_fabs_rr(d,s);
    f_unlock(s);
    f_unlock(d);
}
MENDFUNC(2,fabs_rr,(FW d, FR s))

MIDFUNC(2,fsin_rr,(FW d, FR s))
{
    s=f_readreg(s);
    d=f_writereg(d);
    raw_fsin_rr(d,s);
    f_unlock(s);
    f_unlock(d);
}
MENDFUNC(2,fsin_rr,(FW d, FR s))

MIDFUNC(2,fcos_rr,(FW d, FR s))
{
    s=f_readreg(s);
    d=f_writereg(d);
    raw_fcos_rr(d,s);
    f_unlock(s);
    f_unlock(d);
}
MENDFUNC(2,fcos_rr,(FW d, FR s))

MIDFUNC(2,ftwotox_rr,(FW d, FR s))
{
    s=f_readreg(s);
    d=f_writereg(d);
    raw_ftwotox_rr(d,s);
    f_unlock(s);
    f_unlock(d);
}
MENDFUNC(2,ftwotox_rr,(FW d, FR s))

MIDFUNC(2,fetox_rr,(FW d, FR s))
{
    s=f_readreg(s);
    d=f_writereg(d);
    raw_fetox_rr(d,s);
    f_unlock(s);
    f_unlock(d);
}
MENDFUNC(2,fetox_rr,(FW d, FR s))

MIDFUNC(2,frndint_rr,(FW d, FR s))
{
    s=f_readreg(s);
    d=f_writereg(d);
    raw_frndint_rr(d,s);
    f_unlock(s);
    f_unlock(d);
}
MENDFUNC(2,frndint_rr,(FW d, FR s))

MIDFUNC(2,flog2_rr,(FW d, FR s))
{
    s=f_readreg(s);
    d=f_writereg(d);
    raw_flog2_rr(d,s);
    f_unlock(s);
    f_unlock(d);
}
MENDFUNC(2,flog2_rr,(FW d, FR s))

MIDFUNC(2,fneg_rr,(FW d, FR s))
{
    s=f_readreg(s);
    d=f_writereg(d);
    raw_fneg_rr(d,s);
    f_unlock(s);
    f_unlock(d);
}
MENDFUNC(2,fneg_rr,(FW d, FR s))

MIDFUNC(2,fadd_rr,(FRW d, FR s))
{
    s=f_readreg(s);
    d=f_rmw(d);
    raw_fadd_rr(d,s);
    f_unlock(s);
    f_unlock(d);
}
MENDFUNC(2,fadd_rr,(FRW d, FR s))

MIDFUNC(2,fsub_rr,(FRW d, FR s))
{
    s=f_readreg(s);
    d=f_rmw(d);
    raw_fsub_rr(d,s);
    f_unlock(s);
    f_unlock(d);
}
MENDFUNC(2,fsub_rr,(FRW d, FR s))

MIDFUNC(2,fcmp_rr,(FR d, FR s))
{
    d=f_readreg(d);
    s=f_readreg(s);
    raw_fcmp_rr(d,s);
    f_unlock(s);
    f_unlock(d);
}
MENDFUNC(2,fcmp_rr,(FR d, FR s))

MIDFUNC(2,fdiv_rr,(FRW d, FR s))
{
    s=f_readreg(s);
    d=f_rmw(d);
    raw_fdiv_rr(d,s);
    f_unlock(s);
    f_unlock(d);
}
MENDFUNC(2,fdiv_rr,(FRW d, FR s))

MIDFUNC(2,frem_rr,(FRW d, FR s))
{
    s=f_readreg(s);
    d=f_rmw(d);
    raw_frem_rr(d,s);
    f_unlock(s);
    f_unlock(d);
}
MENDFUNC(2,frem_rr,(FRW d, FR s))

MIDFUNC(2,frem1_rr,(FRW d, FR s))
{
    s=f_readreg(s);
    d=f_rmw(d);
    raw_frem1_rr(d,s);
    f_unlock(s);
    f_unlock(d);
}
MENDFUNC(2,frem1_rr,(FRW d, FR s))

MIDFUNC(2,fmul_rr,(FRW d, FR s))
{
    s=f_readreg(s);
    d=f_rmw(d);
    raw_fmul_rr(d,s);
    f_unlock(s);
    f_unlock(d);
}
MENDFUNC(2,fmul_rr,(FRW d, FR s))

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
	    write_log("Register %d should be constant, but isn't\n",r);
	    abort();
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

void compiler_init(void)
{
	static bool initialized = false;
	if (initialized)
		return;

#if JIT_DEBUG
	// JIT debug mode ?
	JITDebug = PrefsFindBool("jitdebug");
#endif
	write_log("<JIT compiler> : enable runtime disassemblers : %s\n", JITDebug ? "yes" : "no");
	
#ifdef USE_JIT_FPU
	// Use JIT compiler for FPU instructions ?
	avoid_fpu = !PrefsFindBool("jitfpu");
#else
	// JIT FPU is always disabled
	avoid_fpu = true;
#endif
	write_log("<JIT compiler> : compile FPU instructions : %s\n", !avoid_fpu ? "yes" : "no");
	
	// Get size of the translation cache (in KB)
	cache_size = PrefsFindInt32("jitcachesize");
	write_log("<JIT compiler> : requested translation cache size : %d KB\n", cache_size);
	
	// Initialize target CPU (check for features, e.g. CMOV, rat stalls)
	raw_init_cpu();
	setzflg_uses_bsf = target_check_bsf();
	write_log("<JIT compiler> : target processor has CMOV instructions : %s\n", have_cmov ? "yes" : "no");
	write_log("<JIT compiler> : target processor can suffer from partial register stalls : %s\n", have_rat_stall ? "yes" : "no");
	write_log("<JIT compiler> : alignment for loops, jumps are %d, %d\n", align_loops, align_jumps);
	
	// Translation cache flush mechanism
	lazy_flush = PrefsFindBool("jitlazyflush");
	write_log("<JIT compiler> : lazy translation cache invalidation : %s\n", str_on_off(lazy_flush));
	flush_icache = lazy_flush ? flush_icache_lazy : flush_icache_hard;
	
	// Compiler features
	write_log("<JIT compiler> : register aliasing : %s\n", str_on_off(1));
	write_log("<JIT compiler> : FP register aliasing : %s\n", str_on_off(USE_F_ALIAS));
	write_log("<JIT compiler> : lazy constant offsetting : %s\n", str_on_off(USE_OFFSET));
#if USE_INLINING
	follow_const_jumps = PrefsFindBool("jitinline");
#endif
	write_log("<JIT compiler> : translate through constant jumps : %s\n", str_on_off(follow_const_jumps));
	write_log("<JIT compiler> : separate blockinfo allocation : %s\n", str_on_off(USE_SEPARATE_BIA));
	
	// Build compiler tables
	build_comp();
	
	initialized = true;
	
#if PROFILE_UNTRANSLATED_INSNS
	write_log("<JIT compiler> : gather statistics on untranslated insns count\n");
#endif

#if PROFILE_COMPILE_TIME
	write_log("<JIT compiler> : gather statistics on translation time\n");
	emul_start_time = clock();
#endif
}

void compiler_exit(void)
{
#if PROFILE_COMPILE_TIME
	emul_end_time = clock();
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
	
#if PROFILE_COMPILE_TIME
	write_log("### Compile Block statistics\n");
	write_log("Number of calls to compile_block : %d\n", compile_count);
	uae_u32 emul_time = emul_end_time - emul_start_time;
	write_log("Total emulation time   : %.1f sec\n", double(emul_time)/double(CLOCKS_PER_SEC));
	write_log("Total compilation time : %.1f sec (%.1f%%)\n", double(compile_time)/double(CLOCKS_PER_SEC),
		100.0*double(compile_time)/double(emul_time));
	write_log("\n");
#endif

#if PROFILE_UNTRANSLATED_INSNS
	uae_u64 untranslated_count = 0;
	for (int i = 0; i < 65536; i++) {
		opcode_nums[i] = i;
		untranslated_count += raw_cputbl_count[i];
	}
	write_log("Sorting out untranslated instructions count...\n");
	qsort(opcode_nums, 65536, sizeof(uae_u16), untranslated_compfn);
	write_log("\nRank  Opc      Count Name\n");
	for (int i = 0; i < untranslated_top_ten; i++) {
		uae_u32 count = raw_cputbl_count[opcode_nums[i]];
		struct instr *dp;
		struct mnemolookup *lookup;
		if (!count)
			break;
		dp = table68k + opcode_nums[i];
		for (lookup = lookuptab; lookup->mnemo != dp->mnemo; lookup++)
			;
		write_log("%03d: %04x %10lu %s\n", i, opcode_nums[i], count, lookup->name);
	}
#endif

#if RECORD_REGISTER_USAGE
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
	    printf("%c%d : %16ld %2.1f%% [%2.1f]\n", r < 8 ? 'D' : 'A', r % 8,
		   reg_count[r],
		   100.0*double(reg_count[r])/double(tot_reg_count),
		   100.0*double(cum_reg_count)/double(tot_reg_count));
	}
#endif
}

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
	
	// Enable JIT for 68020+ emulation only
	if (CPUType < 2) {
		write_log("<JIT compiler> : JIT is not supported in 680%d0 emulation mode, disabling.\n", CPUType);
		return false;
	}

	return true;
}

void init_comp(void)
{
    int i;
    uae_s8* cb=can_byte;
    uae_s8* cw=can_word;
    uae_s8* au=always_used;

#if RECORD_REGISTER_USAGE
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
	    live.state[i].mem=((uae_u32*)&regs)+i;
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
	
    live.state[FLAGTMP].mem=(uae_u32*)&(regflags.cznv);
    live.state[FLAGTMP].needflush=NF_TOMEM;
    set_status(FLAGTMP,INMEM);

    live.state[NEXT_HANDLER].needflush=NF_HANDLER;
    set_status(NEXT_HANDLER,UNDEF);

    for (i=0;i<VFREGS;i++) {
	if (i<8) { /* First 8 registers map to 68k FPU registers */
	    live.fate[i].mem=(uae_u32*)fpu_register_address(i);
	    live.fate[i].needflush=NF_TOMEM;
	    live.fate[i].status=INMEM;
	}
	else if (i==FP_RESULT) {
	    live.fate[i].mem=(uae_u32*)(&fpu.result);
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

/* Only do this if you really mean it! The next call should be to init!*/
void flush(int save_regs)
{
    int fi,i;
    
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
	    if (live.state[i].needflush==NF_TOMEM) {
		switch(live.state[i].status) {
		 case INMEM:   
		    if (live.state[i].val) {
			raw_add_l_mi((uintptr)live.state[i].mem,live.state[i].val);
			log_vwrite(i);
			live.state[i].val=0;
		    }
		    break;
		 case CLEAN:   
		 case DIRTY:   
		    remove_offset(i,-1); tomem(i); break;
		 case ISCONST: 
		    if (i!=PC_P) 
			writeback_const(i); 
		    break;
		 default: break;
		}
		Dif (live.state[i].val && i!=PC_P) {
		    write_log("Register %d still has val %x\n",
			   i,live.state[i].val);
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
    if (needflags) {
	write_log("Warning! flush with needflags=1!\n");
    }
}

static void flush_keepflags(void)
{
    int fi,i;
    
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
		remove_offset(i,-1); tomem(i); break;
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

void freescratch(void)
{
    int i;
    for (i=0;i<N_REGS;i++)
	if (live.nat[i].locked && i!=4)
	    write_log("Warning! %d is locked\n",i);

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
			*target++=0x90;
	}
}

static __inline__ int isinrom(uintptr addr)
{
	return ((addr >= (uintptr)ROMBaseHost) && (addr < (uintptr)ROMBaseHost + ROMSize));
}

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

/********************************************************************
 * Memory access and related functions, CREATE time                 *
 ********************************************************************/

void register_branch(uae_u32 not_taken, uae_u32 taken, uae_u8 cond)
{
    next_pc_p=not_taken;
    taken_pc_p=taken;
    branch_cc=cond;
}


static uae_u32 get_handler_address(uae_u32 addr)
{
    uae_u32 cl=cacheline(addr);
    blockinfo* bi=get_blockinfo_addr_new((void*)(uintptr)addr,0);
    return (uintptr)&(bi->direct_handler_to_use);
}

static uae_u32 get_handler(uae_u32 addr)
{
    uae_u32 cl=cacheline(addr);
    blockinfo* bi=get_blockinfo_addr_new((void*)(uintptr)addr,0);
    return (uintptr)bi->direct_handler_to_use;
}

static void load_handler(int reg, uae_u32 addr)
{
    mov_l_rm(reg,get_handler_address(addr));
}

/* This version assumes that it is writing *real* memory, and *will* fail
 *  if that assumption is wrong! No branches, no second chances, just
 *  straight go-for-it attitude */

static void writemem_real(int address, int source, int size, int tmp, int clobber)
{
    int f=tmp;

	if (clobber)
	    f=source;

	switch(size) {
	 case 1: mov_b_bRr(address,source,MEMBaseDiff); break; 
	 case 2: mov_w_rr(f,source); bswap_16(f); mov_w_bRr(address,f,MEMBaseDiff); break;
	 case 4: mov_l_rr(f,source); bswap_32(f); mov_l_bRr(address,f,MEMBaseDiff); break;
	}
	forget_about(tmp);
	forget_about(f);
}

void writebyte(int address, int source, int tmp)
{
	writemem_real(address,source,1,tmp,0);
}

static __inline__ void writeword_general(int address, int source, int tmp,
					 int clobber)
{
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

static __inline__ void writelong_general(int address, int source, int tmp, 
					 int clobber)
{
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
 *  if that assumption is wrong! No branches, no second chances, just
 *  straight go-for-it attitude */

static void readmem_real(int address, int dest, int size, int tmp)
{
    int f=tmp; 

    if (size==4 && address!=dest)
	f=dest;

	switch(size) {
	 case 1: mov_b_brR(dest,address,MEMBaseDiff); break; 
	 case 2: mov_w_brR(dest,address,MEMBaseDiff); bswap_16(dest); break;
	 case 4: mov_l_brR(dest,address,MEMBaseDiff); bswap_32(dest); break;
	}
	forget_about(tmp);
}

void readbyte(int address, int dest, int tmp)
{
	readmem_real(address,dest,1,tmp);
}

void readword(int address, int dest, int tmp)
{
	readmem_real(address,dest,2,tmp);
}

void readlong(int address, int dest, int tmp)
{
	readmem_real(address,dest,4,tmp);
}

void get_n_addr(int address, int dest, int tmp)
{
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

#if REAL_ADDRESSING
	mov_l_rr(dest, address);
#elif DIRECT_ADDRESSING
	lea_l_brr(dest,address,MEMBaseDiff);
#endif
	forget_about(tmp);
}

void get_n_addr_jmp(int address, int dest, int tmp)
{
	/* For this, we need to get the same address as the rest of UAE
	 would --- otherwise we end up translating everything twice */
    get_n_addr(address,dest,tmp);
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
    if (enabled!=letit)
	flush_icache_hard(77);
    letit=enabled;
}

int get_cache_state(void)
{
    return letit;
}

uae_u32 get_jitted_size(void)
{
    if (compiled_code)
	return current_compile_p-compiled_code;
    return 0;
}

const int CODE_ALLOC_MAX_ATTEMPTS = 10;
const int CODE_ALLOC_BOUNDARIES   = 128 * 1024; // 128 KB

static uint8 *do_alloc_code(uint32 size, int depth)
{
#if defined(__linux__) && 0
	/*
	  This is a really awful hack that is known to work on Linux at
	  least.
	  
	  The trick here is to make sure the allocated cache is nearby
	  code segment, and more precisely in the positive half of a
	  32-bit address space. i.e. addr < 0x80000000. Actually, it
	  turned out that a 32-bit binary run on AMD64 yields a cache
	  allocated around 0xa0000000, thus causing some troubles when
	  translating addresses from m68k to x86.
	*/
	static uint8 * code_base = NULL;
	if (code_base == NULL) {
		uintptr page_size = getpagesize();
		uintptr boundaries = CODE_ALLOC_BOUNDARIES;
		if (boundaries < page_size)
			boundaries = page_size;
		code_base = (uint8 *)sbrk(0);
		for (int attempts = 0; attempts < CODE_ALLOC_MAX_ATTEMPTS; attempts++) {
			if (vm_acquire_fixed(code_base, size) == 0) {
				uint8 *code = code_base;
				code_base += size;
				return code;
			}
			code_base += boundaries;
		}
		return NULL;
	}

	if (vm_acquire_fixed(code_base, size) == 0) {
		uint8 *code = code_base;
		code_base += size;
		return code;
	}

	if (depth >= CODE_ALLOC_MAX_ATTEMPTS)
		return NULL;

	return do_alloc_code(size, depth + 1);
#else
	uint8 *code = (uint8 *)vm_acquire(size);
	return code == VM_MAP_FAILED ? NULL : code;
#endif
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
		flush_icache_hard(6);
		vm_release(compiled_code, cache_size * 1024);
		compiled_code = 0;
	}
	
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
		write_log("<JIT compiler> : actual translation cache size : %d KB at 0x%08X\n", cache_size, compiled_code);
		max_compile_start = compiled_code + cache_size*1024 - BYTES_PER_INST;
		current_compile_p = compiled_code;
		current_cache_size = 0;
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
		uae_u32*pos;

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
	    write_log("%08x ",*pos);
	    pos++;
	    len-=4;
	}
	write_log(" bla\n");
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
	abort();
    raise_in_cl_list(bi);
    execute_normal();
    return;
}
static void cache_miss(void)
{
    blockinfo*  bi=get_blockinfo_addr(regs.pc_p);
    uae_u32     cl=cacheline(regs.pc_p);
    blockinfo*  bi2=get_blockinfo(cl);

    if (!bi) {
	execute_normal(); /* Compile this block now */
	return;
    }
    Dif (!bi2 || bi==bi2) {
	write_log("Unexplained cache miss %p %p\n",bi,bi2);
	abort();
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
    
    checksum_count++;

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
	isgood=called_check_checksum(bi);
    }
    if (isgood) {
	/*	write_log("reactivate %p/%p (%x %x/%x %x)\n",bi,bi->pc_p,
		c1,c2,bi->c1,bi->c2);*/
	remove_from_list(bi);
	add_to_active(bi);
	raise_in_cl_list(bi);
	bi->status=BI_ACTIVE;
    }
    else {
	/* This block actually changed. We need to invalidate it,
	   and set it up to be recompiled */
	/* write_log("discard %p/%p (%x %x/%x %x)\n",bi,bi->pc_p,
	   c1,c2,bi->c1,bi->c2); */
	invalidate_block(bi);
	raise_in_cl_list(bi);
    }
    return isgood;
}

static int called_check_checksum(blockinfo* bi) 
{
    dependency* x=bi->deplist;
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

    /* These are not the droids you are looking for...  */
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

static __inline__ void match_states(blockinfo* bi)
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
		// write_log("unneeded reg %d at %p\n",i,target);
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

static __inline__ void create_popalls(void)
{
  int i,r;

  if ((popallspace = alloc_code(POPALLSPACE_SIZE)) == NULL) {
	  write_log("FATAL: Could not allocate popallspace!\n");
	  abort();
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
  for (i=N_REGS;i--;) {
      if (need_to_preserve[i])
	  raw_push_l_r(i);
  }
  raw_dec_sp(stack_space);
  r=REG_PC_TMP;
  raw_mov_l_rm(r,(uintptr)&regs.pc_p);
  raw_and_l_ri(r,TAGMASK);
  raw_jmp_m_indexed((uintptr)cache_tags,r,SIZEOF_VOID_P);

  /* now the exit points */
  align_target(align_jumps);
  popall_do_nothing=get_target();
  raw_inc_sp(stack_space);
  for (i=0;i<N_REGS;i++) {
      if (need_to_preserve[i])
	  raw_pop_l_r(i);
  }
  raw_jmp((uintptr)do_nothing);
  
  align_target(align_jumps);
  popall_execute_normal=get_target();
  raw_inc_sp(stack_space);
  for (i=0;i<N_REGS;i++) {
      if (need_to_preserve[i])
	  raw_pop_l_r(i);
  }
  raw_jmp((uintptr)execute_normal);

  align_target(align_jumps);
  popall_cache_miss=get_target();
  raw_inc_sp(stack_space);
  for (i=0;i<N_REGS;i++) {
      if (need_to_preserve[i])
	  raw_pop_l_r(i);
  }
  raw_jmp((uintptr)cache_miss);

  align_target(align_jumps);
  popall_recompile_block=get_target();
  raw_inc_sp(stack_space);
  for (i=0;i<N_REGS;i++) {
      if (need_to_preserve[i])
	  raw_pop_l_r(i);
  }
  raw_jmp((uintptr)recompile_block);

  align_target(align_jumps);
  popall_exec_nostats=get_target();
  raw_inc_sp(stack_space);
  for (i=0;i<N_REGS;i++) {
      if (need_to_preserve[i])
	  raw_pop_l_r(i);
  }
  raw_jmp((uintptr)exec_nostats);

  align_target(align_jumps);
  popall_check_checksum=get_target();
  raw_inc_sp(stack_space);
  for (i=0;i<N_REGS;i++) {
      if (need_to_preserve[i])
	  raw_pop_l_r(i);
  }
  raw_jmp((uintptr)check_checksum);

  // no need to further write into popallspace
  vm_protect(popallspace, POPALLSPACE_SIZE, VM_PAGE_READ | VM_PAGE_EXECUTE);
}

static __inline__ void reset_lists(void)
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
    bi->direct_pen=(cpuop_func *)get_target();
    raw_mov_l_rm(0,(uintptr)&(bi->pc_p));
    raw_mov_l_mr((uintptr)&regs.pc_p,0);
    raw_jmp((uintptr)popall_execute_normal);

    align_target(align_jumps);
    bi->direct_pcc=(cpuop_func *)get_target();
    raw_mov_l_rm(0,(uintptr)&(bi->pc_p));
    raw_mov_l_mr((uintptr)&regs.pc_p,0);
    raw_jmp((uintptr)popall_check_checksum);
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

// OPCODE is in big endian format, use cft_map() beforehand, if needed.
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

static bool merge_blacklist()
{
	const char *blacklist = PrefsFindString("jitblacklist");
	if (blacklist) {
		const char *p = blacklist;
		for (;;) {
			if (*p == 0)
				return true;

			int opcode1 = read_opcode(p);
			if (opcode1 < 0)
				return false;
			p += 4;

			int opcode2 = opcode1;
			if (*p == '-') {
				p++;
				opcode2 = read_opcode(p);
				if (opcode2 < 0)
					return false;
				p += 4;
			}

			if (*p == 0 || *p == ',' || *p == ';') {
				write_log("<JIT compiler> : blacklist opcodes : %04x-%04x\n", opcode1, opcode2);
				for (int opcode = opcode1; opcode <= opcode2; opcode++)
					reset_compop(cft_map(opcode));

				if (*p == ',' || *p++ == ';')
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
	int i;
    int jumpcount=0;
    unsigned long opcode;
    struct comptbl* tbl=op_smalltbl_0_comp_ff;
    struct comptbl* nftbl=op_smalltbl_0_comp_nf;
    int count;
	int cpu_level = 0;			// 68000 (default)
	if (CPUType == 4)
		cpu_level = 4;			// 68040 with FPU
	else {
		if (FPUType)
			cpu_level = 3;		// 68020 with FPU
		else if (CPUType >= 2)
			cpu_level = 2;		// 68020
		else if (CPUType == 1)
			cpu_level = 1;
	}
    struct cputbl *nfctbl = (
				   cpu_level == 4 ? op_smalltbl_0_nf
			     : cpu_level == 3 ? op_smalltbl_1_nf
			     : cpu_level == 2 ? op_smalltbl_2_nf
			     : cpu_level == 1 ? op_smalltbl_3_nf
			     : op_smalltbl_4_nf);

    write_log ("<JIT compiler> : building compiler function tables\n");
	
	for (opcode = 0; opcode < 65536; opcode++) {
		reset_compop(opcode);
		nfcpufunctbl[opcode] = op_illg_1;
		prop[opcode].use_flags = 0x1f;
		prop[opcode].set_flags = 0x1f;
		prop[opcode].cflow = fl_trap; // ILLEGAL instructions do trap
	}
	
	for (i = 0; tbl[i].opcode < 65536; i++) {
		int cflow = table68k[tbl[i].opcode].cflow;
		if (follow_const_jumps && (tbl[i].specific & 16))
			cflow = fl_const_jump;
		else
			cflow &= ~fl_const_jump;
		prop[cft_map(tbl[i].opcode)].cflow = cflow;

		int uses_fpu = tbl[i].specific & 32;
		if (uses_fpu && avoid_fpu)
			compfunctbl[cft_map(tbl[i].opcode)] = NULL;
		else
			compfunctbl[cft_map(tbl[i].opcode)] = tbl[i].handler;
	}

    for (i = 0; nftbl[i].opcode < 65536; i++) {
		int uses_fpu = tbl[i].specific & 32;
		if (uses_fpu && avoid_fpu)
			nfcompfunctbl[cft_map(nftbl[i].opcode)] = NULL;
		else
			nfcompfunctbl[cft_map(nftbl[i].opcode)] = nftbl[i].handler;
		
		nfcpufunctbl[cft_map(nftbl[i].opcode)] = nfctbl[i].handler;
    }

	for (i = 0; nfctbl[i].handler; i++) {
		nfcpufunctbl[cft_map(nfctbl[i].opcode)] = nfctbl[i].handler;
	}

    for (opcode = 0; opcode < 65536; opcode++) {
		compop_func *f;
		compop_func *nff;
		cpuop_func *nfcf;
		int isaddx,cflow;

		if (table68k[opcode].mnemo == i_ILLG || table68k[opcode].clev > cpu_level)
			continue;

		if (table68k[opcode].handler != -1) {
			f = compfunctbl[cft_map(table68k[opcode].handler)];
			nff = nfcompfunctbl[cft_map(table68k[opcode].handler)];
			nfcf = nfcpufunctbl[cft_map(table68k[opcode].handler)];
			cflow = prop[cft_map(table68k[opcode].handler)].cflow;
			isaddx = prop[cft_map(table68k[opcode].handler)].is_addx;
			prop[cft_map(opcode)].cflow = cflow;
			prop[cft_map(opcode)].is_addx = isaddx;
			compfunctbl[cft_map(opcode)] = f;
			nfcompfunctbl[cft_map(opcode)] = nff;
			Dif (nfcf == op_illg_1)
			abort();
			nfcpufunctbl[cft_map(opcode)] = nfcf;
		}
		prop[cft_map(opcode)].set_flags = table68k[opcode].flagdead;
		prop[cft_map(opcode)].use_flags = table68k[opcode].flaglive;
		/* Unconditional jumps don't evaluate condition codes, so they
		 * don't actually use any flags themselves */
		if (prop[cft_map(opcode)].cflow & fl_const_jump)
			prop[cft_map(opcode)].use_flags = 0;
    }
	for (i = 0; nfctbl[i].handler != NULL; i++) {
		if (nfctbl[i].specific)
			nfcpufunctbl[cft_map(tbl[i].opcode)] = nfctbl[i].handler;
	}

	/* Merge in blacklist */
	if (!merge_blacklist())
		write_log("<JIT compiler> : blacklist merge failure!\n");

    count=0;
    for (opcode = 0; opcode < 65536; opcode++) {
	if (compfunctbl[cft_map(opcode)])
	    count++;
    }
	write_log("<JIT compiler> : supposedly %d compileable opcodes!\n",count);

    /* Initialise state */
    create_popalls();
    alloc_cache();
    reset_lists();

    for (i=0;i<TAGSIZE;i+=2) {
	cache_tags[i].handler=(cpuop_func *)popall_execute_normal;
	cache_tags[i+1].bi=NULL;
    }
    
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


static void flush_icache_none(int n)
{
	/* Nothing to do.  */
}
    
static void flush_icache_hard(int n)
{
    uae_u32 i;
    blockinfo* bi, *dbi;

    hard_flush_count++;
#if 0
    write_log("Flush Icache_hard(%d/%x/%p), %u KB\n",
	   n,regs.pc,regs.pc_p,current_cache_size/1024);
	current_cache_size = 0;
#endif
    bi=active;
    while(bi) {
	cache_tags[cacheline(bi->pc_p)].handler=(cpuop_func *)popall_execute_normal;
	cache_tags[cacheline(bi->pc_p)+1].bi=NULL;
	dbi=bi; bi=bi->next;
	free_blockinfo(dbi);
    }
    bi=dormant;
    while(bi) {
	cache_tags[cacheline(bi->pc_p)].handler=(cpuop_func *)popall_execute_normal;
	cache_tags[cacheline(bi->pc_p)+1].bi=NULL;
	dbi=bi; bi=bi->next;
	free_blockinfo(dbi);
    }

    reset_lists();
    if (!compiled_code)
	return;
    current_compile_p=compiled_code;
	SPCFLAGS_SET( SPCFLAG_JIT_EXEC_RETURN ); /* To get out of compiled code */
}


/* "Soft flushing" --- instead of actually throwing everything away,
   we simply mark everything as "needs to be checked". 
*/

static inline void flush_icache_lazy(int n)
{
    uae_u32 i;
    blockinfo* bi;
    blockinfo* bi2;

        soft_flush_count++;
	if (!active)
	    return;

	bi=active;
	while (bi) {
	    uae_u32 cl=cacheline(bi->pc_p);
		if (bi->status==BI_INVALID ||
			bi->status==BI_NEED_RECOMP) { 
		if (bi==cache_tags[cl+1].bi) 
		    cache_tags[cl].handler=(cpuop_func *)popall_execute_normal;
		bi->handler_to_use=(cpuop_func *)popall_execute_normal;
		set_dhtu(bi,bi->direct_pen);
	    bi->status=BI_INVALID;
	    }
	    else {
		if (bi==cache_tags[cl+1].bi) 
		    cache_tags[cl].handler=(cpuop_func *)popall_check_checksum;
		bi->handler_to_use=(cpuop_func *)popall_check_checksum;
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

void flush_icache_range(uae_u8 *start_p, uae_u32 length)
{
	if (!active)
		return;

#if LAZY_FLUSH_ICACHE_RANGE
	blockinfo *bi = active;
	while (bi) {
#if USE_CHECKSUM_INFO
		bool candidate = false;
		for (checksum_info *csi = bi->csi; csi; csi = csi->next) {
			if (((start_p - csi->start_p) < csi->length) ||
				((csi->start_p - start_p) < length)) {
				candidate = true;
				break;
			}
		}
#else
		// Assume system is consistent and would invalidate the right range
		const bool candidate = (bi->pc_p - start_p) < length;
#endif
		blockinfo *dbi = bi;
		bi = bi->next;
		if (candidate) {
			uae_u32 cl = cacheline(dbi->pc_p);
			if (dbi->status == BI_INVALID || dbi->status == BI_NEED_RECOMP) {
				if (dbi == cache_tags[cl+1].bi) 
					cache_tags[cl].handler = (cpuop_func *)popall_execute_normal;
				dbi->handler_to_use = (cpuop_func *)popall_execute_normal;
				set_dhtu(dbi, dbi->direct_pen);
				dbi->status = BI_INVALID;
			}
			else {
				if (dbi == cache_tags[cl+1].bi) 
					cache_tags[cl].handler = (cpuop_func *)popall_check_checksum;
				dbi->handler_to_use = (cpuop_func *)popall_check_checksum;
				set_dhtu(dbi, dbi->direct_pcc);
				dbi->status = BI_NEED_CHECK;
			}
			remove_from_list(dbi);
			add_to_dormant(dbi);
		}
	}
	return;
#endif
	flush_icache(-1);
}

static void catastrophe(void)
{
    abort();
}

int failure;

#define TARGET_M68K		0
#define TARGET_POWERPC	1
#define TARGET_X86		2
#define TARGET_X86_64	3
#if defined(i386) || defined(__i386__)
#define TARGET_NATIVE	TARGET_X86
#endif
#if defined(powerpc) || defined(__powerpc__)
#define TARGET_NATIVE	TARGET_POWERPC
#endif
#if defined(x86_64) || defined(__x86_64__)
#define TARGET_NATIVE	TARGET_X86_64
#endif

#ifdef ENABLE_MON
static uae_u32 mon_read_byte_jit(uintptr addr)
{
	uae_u8 *m = (uae_u8 *)addr;
	return (uintptr)(*m);
}
 
static void mon_write_byte_jit(uintptr addr, uae_u32 b)
{
	uae_u8 *m = (uae_u8 *)addr;
	*m = b;
}
#endif

void disasm_block(int target, uint8 * start, size_t length)
{
	if (!JITDebug)
		return;
	
#if defined(JIT_DEBUG) && defined(ENABLE_MON)
	char disasm_str[200];
	sprintf(disasm_str, "%s $%x $%x",
			target == TARGET_M68K ? "d68" :
			target == TARGET_X86 ? "d86" :
			target == TARGET_X86_64 ? "d8664" :
			target == TARGET_POWERPC ? "d" : "x",
			start, start + length - 1);
	
	uae_u32 (*old_mon_read_byte)(uintptr) = mon_read_byte;
	void (*old_mon_write_byte)(uintptr, uae_u32) = mon_write_byte;
	
	mon_read_byte = mon_read_byte_jit;
	mon_write_byte = mon_write_byte_jit;
	
	char *arg[5] = {"mon", "-m", "-r", disasm_str, NULL};
	mon(4, arg);
	
	mon_read_byte = old_mon_read_byte;
	mon_write_byte = old_mon_write_byte;
#endif
}

static void disasm_native_block(uint8 *start, size_t length)
{
	disasm_block(TARGET_NATIVE, start, length);
}

static void disasm_m68k_block(uint8 *start, size_t length)
{
	disasm_block(TARGET_M68K, start, length);
}

#ifdef HAVE_GET_WORD_UNSWAPPED
# define DO_GET_OPCODE(a) (do_get_mem_word_unswapped((uae_u16 *)(a)))
#else
# define DO_GET_OPCODE(a) (do_get_mem_word((uae_u16 *)(a)))
#endif

#if JIT_DEBUG
static uae_u8 *last_regs_pc_p = 0;
static uae_u8 *last_compiled_block_addr = 0;

void compiler_dumpstate(void)
{
	if (!JITDebug)
		return;
	
	write_log("### Host addresses\n");
	write_log("MEM_BASE    : %x\n", MEMBaseDiff);
	write_log("PC_P        : %p\n", &regs.pc_p);
	write_log("SPCFLAGS    : %p\n", &regs.spcflags);
	write_log("D0-D7       : %p-%p\n", &regs.regs[0], &regs.regs[7]);
	write_log("A0-A7       : %p-%p\n", &regs.regs[8], &regs.regs[15]);
	write_log("\n");
	
	write_log("### M68k processor state\n");
	m68k_dumpstate(0);
	write_log("\n");
	
	write_log("### Block in Mac address space\n");
	write_log("M68K block   : %p\n",
			  (void *)(uintptr)get_virtual_address(last_regs_pc_p));
	write_log("Native block : %p (%d bytes)\n",
			  (void *)(uintptr)get_virtual_address(last_compiled_block_addr),
			  get_blockinfo_addr(last_regs_pc_p)->direct_handler_size);
	write_log("\n");
}
#endif

static void compile_block(cpu_history* pc_hist, int blocklen)
{
    if (letit && compiled_code) {
#if PROFILE_COMPILE_TIME
	compile_count++;
	clock_t start_time = clock();
#endif
#if JIT_DEBUG
	bool disasm_block = false;
#endif
	
	/* OK, here we need to 'compile' a block */
	int i;
	int r;
	int was_comp=0;
	uae_u8 liveflags[MAXRUN+1];
#if USE_CHECKSUM_INFO
	bool trace_in_rom = isinrom((uintptr)pc_hist[0].location);
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
	if (current_compile_p>=max_compile_start)
	    flush_icache_hard(7);

	alloc_blockinfos();

	bi=get_blockinfo_addr_new(pc_hist[0].location,0);
	bi2=get_blockinfo(cl);

	optlev=bi->optlevel;
	if (bi->status!=BI_INVALID) {
	    Dif (bi!=bi2) { 
		/* I don't think it can happen anymore. Shouldn't, in 
		   any case. So let's make sure... */
		write_log("WOOOWOO count=%d, ol=%d %p %p\n",
		       bi->count,bi->optlevel,bi->handler_to_use,
		       cache_tags[cl].handler);
		abort();
	    }

	    Dif (bi->count!=-1 && bi->status!=BI_NEED_RECOMP) {
		write_log("bi->count=%d, bi->status=%d\n",bi->count,bi->status);
		/* What the heck? We are not supposed to be here! */
		abort();
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
	
	liveflags[blocklen]=0x1f; /* All flags needed afterwards */
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

		liveflags[i]=((liveflags[i+1]&
			       (~prop[op].set_flags))|
			      prop[op].use_flags);
		if (prop[op].is_addx && (liveflags[i+1]&FLAG_Z)==0)
		    liveflags[i]&= ~FLAG_Z;
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

	bi->direct_handler=(cpuop_func *)get_target();
	set_dhtu(bi,bi->direct_handler);
	bi->status=BI_COMPILING;
	current_block_start_target=(uintptr)get_target();
	
	log_startblock();
	
	if (bi->count>=0) { /* Need to generate countdown code */
	    raw_mov_l_mi((uintptr)&regs.pc_p,(uintptr)pc_hist[0].location);
	    raw_sub_l_mi((uintptr)&(bi->count),1);
	    raw_jl((uintptr)popall_recompile_block);
	}
	if (optlev==0) { /* No need to actually translate */
	    /* Execute normally without keeping stats */
	    raw_mov_l_mi((uintptr)&regs.pc_p,(uintptr)pc_hist[0].location);
	    raw_jmp((uintptr)popall_exec_nostats); 
	}
	else {
	    reg_alloc_run=0;
	    next_pc_p=0;
	    taken_pc_p=0;
	    branch_cc=0;
		
	    comp_pc_p=(uae_u8*)pc_hist[0].location;
	    init_comp();
	    was_comp=1;

#ifdef USE_CPU_EMUL_SERVICES
	    raw_sub_l_mi((uintptr)&emulated_ticks,blocklen);
	    raw_jcc_b_oponly(NATIVE_CC_GT);
	    uae_s8 *branchadd=(uae_s8*)get_target();
	    emit_byte(0);
	    raw_call((uintptr)cpu_do_check_ticks);
	    *branchadd=(uintptr)get_target()-((uintptr)branchadd+1);
#endif

#if JIT_DEBUG
		if (JITDebug) {
			raw_mov_l_mi((uintptr)&last_regs_pc_p,(uintptr)pc_hist[0].location);
			raw_mov_l_mi((uintptr)&last_compiled_block_addr,current_block_start_target);
		}
#endif
		
	    for (i=0;i<blocklen &&
		     get_target_noopt()<max_compile_start;i++) {
		cpuop_func **cputbl;
		compop_func **comptbl;
		uae_u32 opcode=DO_GET_OPCODE(pc_hist[i].location);
		needed_flags=(liveflags[i+1] & prop[opcode].set_flags);
		if (!needed_flags) {
		    cputbl=nfcpufunctbl;
		    comptbl=nfcompfunctbl;
		}
		else {
		    cputbl=cpufunctbl;
		    comptbl=compfunctbl;
		}

#if FLIGHT_RECORDER
		{
		    mov_l_ri(S1, get_virtual_address((uae_u8 *)(pc_hist[i].location)) | 1);
		    clobber_flags();
		    remove_all_offsets();
		    int arg = readreg_specific(S1,4,REG_PAR1);
		    prepare_for_call_1();
		    unlock2(arg);
		    prepare_for_call_2();
		    raw_call((uintptr)m68k_record_step);
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
		}
		
		if (failure) {
		    if (was_comp) {
			flush(1);
			was_comp=0;
		    }
		    raw_mov_l_ri(REG_PAR1,(uae_u32)opcode);
#if USE_NORMAL_CALLING_CONVENTION
		    raw_push_l_r(REG_PAR1);
#endif
		    raw_mov_l_mi((uintptr)&regs.pc_p,
				 (uintptr)pc_hist[i].location);
		    raw_call((uintptr)cputbl[opcode]);
#if PROFILE_UNTRANSLATED_INSNS
			// raw_cputbl_count[] is indexed with plain opcode (in m68k order)
			raw_add_l_mi((uintptr)&raw_cputbl_count[cft_map(opcode)],1);
#endif
#if USE_NORMAL_CALLING_CONVENTION
		    raw_inc_sp(4);
#endif
		    
		    if (i < blocklen - 1) {
			uae_s8* branchadd;
			
			raw_mov_l_rm(0,(uintptr)specflags);
			raw_test_l_rr(0,0);
			raw_jz_b_oponly();
			branchadd=(uae_s8 *)get_target();
			emit_byte(0);
			raw_jmp((uintptr)popall_do_nothing);
			*branchadd=(uintptr)get_target()-(uintptr)branchadd-1;
		    }
		}
	    }
#if 1 /* This isn't completely kosher yet; It really needs to be
	 be integrated into a general inter-block-dependency scheme */
	    if (next_pc_p && taken_pc_p &&
		was_comp && taken_pc_p==current_block_pc_p) {
		blockinfo* bi1=get_blockinfo_addr_new((void*)next_pc_p,0);
		blockinfo* bi2=get_blockinfo_addr_new((void*)taken_pc_p,0);
		uae_u8 x=bi1->needed_flags;
		
		if (x==0xff || 1) {  /* To be on the safe side */
		    uae_u16* next=(uae_u16*)next_pc_p;
		    uae_u32 op=DO_GET_OPCODE(next);

		    x=0x1f;
		    x&=(~prop[op].set_flags);
		    x|=prop[op].use_flags;
		}
		
		x|=bi2->needed_flags;
		if (!(x & FLAG_CZNV)) { 
		    /* We can forget about flags */
		    dont_care_flags();
		    extra_len+=2; /* The next instruction now is part of this
				     block */
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
		raw_jcc_l_oponly(cc);
		branchadd=(uae_u32*)get_target();
		emit_long(0);
		
		/* predicted outcome */
		tbi=get_blockinfo_addr_new((void*)t1,1);
		match_states(tbi);
		raw_cmp_l_mi((uintptr)specflags,0);
		raw_jcc_l_oponly(4);
		tba=(uae_u32*)get_target();
		emit_long(get_handler(t1)-((uintptr)tba+4));
		raw_mov_l_mi((uintptr)&regs.pc_p,t1);
		flush_reg_count();
		raw_jmp((uintptr)popall_do_nothing);
		create_jmpdep(bi,0,tba,t1);

		align_target(align_jumps);
		/* not-predicted outcome */
		*branchadd=(uintptr)get_target()-((uintptr)branchadd+4);
		live=tmp; /* Ouch again */
		tbi=get_blockinfo_addr_new((void*)t2,1);
		match_states(tbi);

		//flush(1); /* Can only get here if was_comp==1 */
		raw_cmp_l_mi((uintptr)specflags,0);
		raw_jcc_l_oponly(4);
		tba=(uae_u32*)get_target();
		emit_long(get_handler(t2)-((uintptr)tba+4));
		raw_mov_l_mi((uintptr)&regs.pc_p,t2);
		flush_reg_count();
		raw_jmp((uintptr)popall_do_nothing);
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
			raw_and_l_ri(r,TAGMASK);
			int r2 = (r==0) ? 1 : 0;
			raw_mov_l_ri(r2,(uintptr)popall_do_nothing);
			raw_cmp_l_mi((uintptr)specflags,0);
			raw_cmov_l_rm_indexed(r2,(uintptr)cache_tags,r,SIZEOF_VOID_P,NATIVE_CC_EQ);
			raw_jmp_r(r2);
		}
		else if (was_comp && isconst(PC_P)) {
		    uae_u32 v=live.state[PC_P].val;
		    uae_u32* tba;
		    blockinfo* tbi;

		    tbi=get_blockinfo_addr_new((void*)(uintptr)v,1);
		    match_states(tbi);

			raw_cmp_l_mi((uintptr)specflags,0);
			raw_jcc_l_oponly(4);
		    tba=(uae_u32*)get_target();
		    emit_long(get_handler(v)-((uintptr)tba+4));
		    raw_mov_l_mi((uintptr)&regs.pc_p,v);
		    raw_jmp((uintptr)popall_do_nothing);
		    create_jmpdep(bi,0,tba,v);
		}
		else {
		    r=REG_PC_TMP;
		    raw_mov_l_rm(r,(uintptr)&regs.pc_p);
			raw_and_l_ri(r,TAGMASK);
			int r2 = (r==0) ? 1 : 0;
			raw_mov_l_ri(r2,(uintptr)popall_do_nothing);
			raw_cmp_l_mi((uintptr)specflags,0);
			raw_cmov_l_rm_indexed(r2,(uintptr)cache_tags,r,SIZEOF_VOID_P,NATIVE_CC_EQ);
			raw_jmp_r(r2);
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
	
	current_cache_size += get_target() - (uae_u8 *)current_compile_p;
	
#if JIT_DEBUG
	if (JITDebug)
		bi->direct_handler_size = get_target() - (uae_u8 *)current_block_start_target;
	
	if (JITDebug && disasm_block) {
		uaecptr block_addr = start_pc + ((char *)pc_hist[0].location - (char *)start_pc_p);
		D(bug("M68K block @ 0x%08x (%d insns)\n", block_addr, blocklen));
		uae_u32 block_size = ((uae_u8 *)pc_hist[blocklen - 1].location - (uae_u8 *)pc_hist[0].location) + 1;
		disasm_m68k_block((uae_u8 *)pc_hist[0].location, block_size);
		D(bug("Compiled block @ 0x%08x\n", pc_hist[0].location));
		disasm_native_block((uae_u8 *)current_block_start_target, bi->direct_handler_size);
		getchar();
	}
#endif
	
	log_dump();
	align_target(align_jumps);

	/* This is the non-direct handler */
	bi->handler=
	    bi->handler_to_use=(cpuop_func *)get_target();
	raw_cmp_l_mi((uintptr)&regs.pc_p,(uintptr)pc_hist[0].location);
	raw_jnz((uintptr)popall_cache_miss);
	comp_pc_p=(uae_u8*)pc_hist[0].location;

	bi->status=BI_FINALIZING;
	init_comp();
	match_states(bi);
	flush(1);

	raw_jmp((uintptr)bi->direct_handler);

	current_compile_p=get_target();
	raise_in_cl_list(bi);
	
	/* We will flush soon, anyway, so let's do it now */
	if (current_compile_p>=max_compile_start)
		flush_icache_hard(7);
	
	bi->status=BI_ACTIVE;
	if (redo_current_block)
	    block_need_recompile(bi);
	
#if PROFILE_COMPILE_TIME
	compile_time += (clock() - start_time);
#endif
    }

    /* Account for compilation time */
    cpu_do_check_ticks();
}

void do_nothing(void)
{
    /* What did you expect this to do? */
}

void exec_nostats(void)
{
	for (;;)  { 
		uae_u32 opcode = GET_OPCODE;
#if FLIGHT_RECORDER
		m68k_record_step(m68k_getpc());
#endif
		(*cpufunctbl[opcode])(opcode);
		cpu_check_ticks();
		if (end_block(opcode) || SPCFLAGS_TEST(SPCFLAG_ALL)) {
			return; /* We will deal with the spcflags in the caller */
		}
	}
}

void execute_normal(void)
{
	if (!check_for_cache_miss()) {
		cpu_history pc_hist[MAXRUN];
		int blocklen = 0;
#if REAL_ADDRESSING || DIRECT_ADDRESSING
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
			m68k_record_step(m68k_getpc());
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

typedef void (*compiled_handler)(void);

static void m68k_do_compile_execute(void)
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

void m68k_compile_execute (void)
{
    for (;;) {
	  if (quit_program)
		break;
	  m68k_do_compile_execute();
    }
}
