 /*
  * UAE - The Un*x Amiga Emulator
  *
  * m68k emulation
  *
  * Copyright 1996 Bernd Schmidt
  */

#include "sysdeps.h"

#include "m68k.h"
#include "memory.h"
#include "readcpu.h"
#include "newcpu.h"
#include "compiler.h"

#define USER_PROGRAMS_BEHAVE 1

#ifdef USE_COMPILER

#include <sys/mman.h>

char *address_space, *good_address_map;

code_execfunc exec_me;
uae_u8 nr_bbs_to_run = 1;
int nr_bbs_start = 40;

static int compile_failure;
static int quiet_compile = 1;
int i_want_to_die = 1;
static int n_compiled = 0;
static int n_max_comp = 99999999;
static uaecptr call_only_me = 0;

int patched_syscalls = 0;

static int count_bits(uae_u16 v)
{
    int bits = 0;
    while (v != 0) {
	if (v & 1)
	    bits++;
	v >>= 1;
    }
    return bits;
}

static uae_u16 bitswap(uae_u16 v)
{
    uae_u16 newv = 0;
    uae_u16 m1 = 1, m2 = 0x8000;
    int i;

    for (i = 0; i < 16; i++) {
	if (v & m1)
	    newv |= m2;
	m2 >>= 1;
	m1 <<= 1;
    }
    return newv;
}

static long long compiled_hits = 0;

/* 16K areas with 512 byte blocks */
#define SUBUNIT_ORDER 9
#define PAGE_SUBUNIT (1 << SUBUNIT_ORDER)
#define PAGE_ALLOC_UNIT (PAGE_SUBUNIT * 32)

static int zerofd;
static int zeroff;
static struct code_page *first_code_page;

static struct code_page *new_code_page(void)
{
    struct code_page *ncp;

    ncp = (struct code_page *)mmap(NULL, PAGE_ALLOC_UNIT,
				   PROT_EXEC|PROT_READ|PROT_WRITE, MAP_PRIVATE,
				   zerofd, zeroff);
    zeroff += PAGE_ALLOC_UNIT;
    if (ncp) {
	ncp->next = first_code_page;
	first_code_page = ncp;
	ncp->allocmask = 1; /* what a waste */
    }
    return ncp;
}

#define NUM_HASH 32768 /* larger values cause some paging on my 16MB machine */
#define HASH_MASK (NUM_HASH-1)
#define MAX_UNUSED_HASH 512

static int SCAN_MARK = 1; /* Number of calls after which to scan a function */
static int COMPILE_MARK = 5; /* Number of calls after which to compile a function */

/* The main address -> function lookup hashtable. We use the lower bits of
 * the address as hash function. */
static struct hash_entry cpu_hash[NUM_HASH];
/* These aren't really LRU lists... They used to be, but keeping them in that
 * order is costly. The hash LRU list is now a two-part list: Functions that have
 * no code allocated for them are placed at the beginning. Such entries can be
 * recycled when we need a new hash entry. */
static struct hash_block lru_first_block;
static struct hash_entry lru_first_hash;
static struct hash_entry *freelist_hash;
static struct hash_block *freelist_block;
static int num_unused_hash;

static int m68k_scan_func(struct hash_entry *);
static int m68k_compile_block(struct hash_block *);

static char *alloc_code(struct hash_block *hb, int ninsns)
{
    struct code_page *cp;
    long int allocsize = (ninsns * 32 + PAGE_SUBUNIT-1) & ~(PAGE_SUBUNIT-1);
    uae_u32 allocmask;
    int allocbits;
    int j;
    int last_bit;

    if (allocsize >= (PAGE_ALLOC_UNIT - (1 << SUBUNIT_ORDER)))
	return NULL;
    allocbits = (allocsize >> SUBUNIT_ORDER);
    allocmask = (1 << allocbits) - 1;

    for (cp = first_code_page; cp != NULL; cp = cp->next) {
	uae_u32 thispage_alloc = cp->allocmask;
	for (j = 1; j < (33 - allocbits); j++) {
	    if ((cp->allocmask & (allocmask << j)) == 0) {
		goto found_page;
	    }
	}
    }

    /* Nothing large enough free: make a new page */
    cp = new_code_page();
    if (cp == NULL)
	return NULL;
    j = 1;

found_page:
    /* See whether there is in fact more space for us. If so, allocate all of
     * it. compile_block() will free everything it didn't need. */

    allocmask <<= j;
    last_bit = allocbits + j;
    while (last_bit < 32 && (cp->allocmask & (1 << last_bit)) == 0) {
	allocmask |= 1 << last_bit;
	allocsize += PAGE_SUBUNIT;
	last_bit++;
    }

    hb->page_allocmask = allocmask;
    hb->cpage = cp;
    cp->allocmask |= allocmask;
    hb->compile_start = ((char *)cp + (j << SUBUNIT_ORDER));
    hb->alloclen = allocsize;
    return hb->compile_start;
}

static void remove_hash_from_lists(struct hash_entry *h)
{
    h->lru_next->lru_prev = h->lru_prev;
    h->lru_prev->lru_next = h->lru_next;

    h->next->prev = h->prev;
    h->prev->next = h->next;
}

static void lru_touch(struct hash_entry *h)
{
    h->lru_next->lru_prev = h->lru_prev;
    h->lru_prev->lru_next = h->lru_next;

    h->lru_next = &lru_first_hash;
    h->lru_prev = lru_first_hash.lru_prev;
    h->lru_prev->lru_next = h;
    lru_first_hash.lru_prev = h;
}

static void lru_untouch(struct hash_entry *h)
{
    h->lru_next->lru_prev = h->lru_prev;
    h->lru_prev->lru_next = h->lru_next;

    h->lru_prev = &lru_first_hash;
    h->lru_next = lru_first_hash.lru_next;
    h->lru_next->lru_prev = h;
    lru_first_hash.lru_next = h;
}

static void forget_block(struct hash_block *hb)
{
    struct hash_entry *h = hb->he_first;

    hb->lru_next->lru_prev = hb->lru_prev;
    hb->lru_prev->lru_next = hb->lru_next;

    hb->lru_next = freelist_block;
    freelist_block = hb;

    if (hb->cpage != NULL)
	fprintf(stderr, "Discarding block with code. Tsk.\n");

    do {
	struct hash_entry *next = h->next_same_block;
	h->block = NULL;
	h->execute = NULL;
	h->next_same_block = NULL;
	h = next;
	num_unused_hash++;
	lru_untouch(h);
    } while (h != hb->he_first);
    compiler_flush_jsr_stack();
}

static void lru_touch_block(struct hash_block *h)
{
    h->lru_next->lru_prev = h->lru_prev;
    h->lru_prev->lru_next = h->lru_next;

    h->lru_next = &lru_first_block;
    h->lru_prev = lru_first_block.lru_prev;
    h->lru_prev->lru_next = h;
    lru_first_block.lru_prev = h;
}

static __inline__ int check_block(struct hash_block *hb)
{
#ifndef RELY_ON_LOADSEG_DETECTION
    struct hash_entry *h = hb->he_first;

    do {
	struct hash_entry *next = h->next_same_block;
	if (h->matchword != *(uae_u32 *)get_real_address(h->addr))
	    return 0;
	h = next;
    } while (h != hb->he_first);
#endif
    return 1;
}

uae_u32 flush_icache(void)
{
    struct hash_block *hb = lru_first_block.lru_next;

    while (hb != &lru_first_block) {
	struct hash_block *next = hb->lru_next;
	if (hb->cpage != NULL) {
	    /* Address in chipmem? Then forget about block*/
	    if ((hb->he_first->addr & ~0xF80000) != 0xF80000) {
		hb->cpage->allocmask &= ~hb->page_allocmask;
		hb->cpage = NULL;
		forget_block(hb);
	    }
	}
	hb = next;
    }
    return m68k_dreg(regs, 0);
}

void possible_loadseg(void)
{
    fprintf(stderr, "Possible LoadSeg() detected\n");
    flush_icache();
}

static struct hash_block *new_block(void)
{
    struct hash_block *b = freelist_block;

    if (b != NULL) {
	freelist_block = b->lru_next;
    } else
	b = (struct hash_block *)malloc(sizeof *b);
    b->nrefs = 0;
    b->cpage = NULL;
    b->he_first = NULL;
    b->translated = b->untranslatable = b->allocfailed = 0;
    return b;
}

static struct hash_entry *get_free_hash(void)
{
    struct hash_entry *h;

    for (;;) {
	h = freelist_hash;
	if (h != NULL) {
	    freelist_hash = h->next_same_block;
	    break;
	}
	h = lru_first_hash.lru_next;
	if (num_unused_hash >= MAX_UNUSED_HASH && h->block == NULL
	    && !h->locked)
	{
	    remove_hash_from_lists(h);
	    num_unused_hash--;
	    break;
	}
	h = (struct hash_entry *)malloc(sizeof(struct hash_entry));
	h->next_same_block = NULL;
	h->addr = (uaecptr)-1;
	break;
    }
    num_unused_hash++;
    h->block = NULL;
    h->ncalls = 0;
    h->locked = h->cacheflush = 0;
    h->execute = NULL;
    return h;
}

static struct hash_entry *new_hash(uaecptr addr)
{
    struct hash_entry *h = get_free_hash();

    h->addr = addr;

    /* Chain the new node */
    h->prev = cpu_hash + ((addr >> 1) & HASH_MASK);
    h->next = h->prev->next;
    h->next->prev = h->prev->next = h;

    h->lru_next = &lru_first_hash;
    h->lru_prev = lru_first_hash.lru_prev;
    h->lru_prev->lru_next = h;
    lru_first_hash.lru_prev = h;

    h->next_same_block = NULL;

    return h;
}
static struct hash_entry *find_hash(uaecptr addr)
{
    struct hash_entry *h;
    struct hash_entry *h1 = cpu_hash + ((addr >> 1) & HASH_MASK);

    if (h1->next->addr == addr)
	return h1->next;

    for (h = h1->next; h != h1; h = h->next) {
	if (h->addr == addr) {
	    /* Put it at the head of the list so that the above shortcut
	     * works the next time we come here */
	    h->next->prev = h->prev; h->prev->next = h->next;
	    h->prev = h1;
	    h->next = h1->next;
	    h->next->prev = h->prev->next = h;
	    return h;
	}
    }
    return NULL;
}

static struct hash_entry *get_hash_for_func(uaecptr addr, int mark_locked)
{
    struct hash_entry *h = find_hash(addr);
    if (h == NULL)
	h = new_hash (addr);
#if 0 /* Too expensive */
    else
	lru_touch(h);
#endif
    if (mark_locked)
	h->locked = 1;
    return h;
}

static struct hash_entry *get_hash(uaecptr addr)
{
    struct hash_entry *h = get_hash_for_func(addr, 0);

    if (h->block == NULL) {
	if (++h->ncalls == SCAN_MARK) {
	    m68k_scan_func(h);
	}
    } else
	if (!h->block->untranslatable && h->block->nrefs++ == COMPILE_MARK) {
	    lru_touch_block(h->block);
	    if (m68k_compile_block(h->block)) {
		h->block->untranslatable = 1;
	    } else {
		h->block->translated = 1;
	    }
	}
    return h;
}

void special_flush_hash(uaecptr addr)
{
    struct hash_entry *h = get_hash_for_func(addr, 0);

    h->cacheflush = 1;
}

static __inline__ void m68k_setpc_hash(uaecptr newpc)
{
    struct hash_entry *h = get_hash(newpc);

    if (h->cacheflush)
	flush_icache();

    if (h->execute != NULL) {
	if ((h->addr & 0xF80000) == 0xF80000 || check_block(h->block)) {
	    compiled_hits++;
	    if (i_want_to_die && (call_only_me == 0 || call_only_me == newpc)) {
		exec_me = h->execute;
		nr_bbs_to_run = nr_bbs_start;
		regs.spcflags |= SPCFLAG_EXEC;
	    }
	} else
	    flush_icache();
    }
    regs.pc = newpc;
    regs.pc_p = regs.pc_oldp = get_real_address(newpc);
}

static __inline__ void m68k_setpc_nohash(uaecptr newpc)
{
#if 0
    /* This is probably not too good for efficiency... FIXME */
    struct hash_entry *h = find_hash(newpc);

    if (h != NULL && h->cacheflush)
	flush_icache();
#endif
    regs.pc = newpc;
    regs.pc_p = regs.pc_oldp = get_real_address(newpc);
}

void m68k_setpc(uaecptr newpc)
{
    m68k_setpc_hash(newpc);
}

void m68k_setpc_fast(uaecptr newpc)
{
    m68k_setpc_nohash(newpc);
}

void m68k_setpc_rte(uaecptr newpc)
{
    m68k_setpc_nohash(newpc);
}

void m68k_setpc_bcc(uaecptr newpc)
{
    m68k_setpc_hash(newpc);
}

static void hash_init(void)
{
    int i;
    struct hash_entry **hepp;

    freelist_block = NULL;
    freelist_hash = NULL;

    for(i = 0; i < NUM_HASH; i++) {
	cpu_hash[i].next = cpu_hash[i].prev = cpu_hash + i;
	cpu_hash[i].lru_next = cpu_hash[i].lru_prev = NULL;
	cpu_hash[i].block = NULL;
	cpu_hash[i].locked = 0; cpu_hash[i].cacheflush = 0;
	cpu_hash[i].addr = (uaecptr)-1;
    }

    lru_first_hash.lru_next = lru_first_hash.lru_prev = &lru_first_hash;
    lru_first_block.lru_next = lru_first_block.lru_prev = &lru_first_block;

    num_unused_hash = 0;
}

static void code_init(void)
{
    first_code_page = NULL;
    zerofd = open("/dev/zero", O_RDWR);
    zeroff = 0;
}

#define CC68K_C 16
#define CC68K_V 8
#define CC68K_Z 4
#define CC68K_N 2
#define CC68K_X 1

static __inline__ int cc_flagmask_68k(const int cc)
{
    switch(cc){
     case 0: return 0;                       /* T */
     case 1: return 0;                       /* F */
     case 2: return CC68K_C|CC68K_Z;         /* HI */
     case 3: return CC68K_C|CC68K_Z;         /* LS */
     case 4: return CC68K_C;                 /* CC */
     case 5: return CC68K_C;                 /* CS */
     case 6: return CC68K_Z;                 /* NE */
     case 7: return CC68K_Z;                 /* EQ */
     case 8: return CC68K_V;                 /* VC */
     case 9: return CC68K_V;                 /* VS */
     case 10:return CC68K_N;                 /* PL */
     case 11:return CC68K_N;                 /* MI */
     case 12:return CC68K_N|CC68K_V;         /* GE */
     case 13:return CC68K_N|CC68K_V;         /* LT */
     case 14:return CC68K_N|CC68K_V|CC68K_Z; /* GT */
     case 15:return CC68K_N|CC68K_V|CC68K_Z; /* LE */
    }
    abort();
    return 0;
}

static __inline__ void translate_step_over_ea(uae_u8 **pcpp, amodes m,
					      wordsizes size)
{
    switch (m) {
     case Areg:
     case Dreg:
     case Aind:
     case Aipi:
     case Apdi:
     case immi:
	break;

     case imm:
	if (size == sz_long)
	    goto is_long;
	/* fall through */
     case Ad16:
     case PC16:
     case imm0:
     case imm1:
     case absw:
	(*pcpp)+=2;
	break;
     case Ad8r:
     case PC8r:
	{
	    uae_u16 extra = *(*pcpp)++;
	    extra <<= 8;
	    extra |= *(*pcpp)++;
	    /* @@@ handle 68020 stuff here */
	}
	break;
     case absl:
     case imm2:
	is_long:
	(*pcpp) += 4;
	break;
    }
}

static struct instr *translate_getnextinsn(uae_u8 **pcpp)
{
    uae_u16 opcode;
    struct instr *dp;

    opcode = *(*pcpp)++ << 8;
    opcode |= *(*pcpp)++;

    if (cpufunctbl[opcode] == op_illg) {
	opcode = 0x4AFC;
    }
    dp = table68k + opcode;
    if (dp->suse) {
	translate_step_over_ea(pcpp, dp->smode, dp->size);
    }
    if (dp->duse) {
	translate_step_over_ea(pcpp, dp->dmode, dp->size);
    }
    return dp;
}

#define CB_STACKSIZE 200
#define BB_STACKSIZE 200

static uae_u32 condbranch_stack[CB_STACKSIZE];
static int condbranch_src_stack[CB_STACKSIZE];

struct bb_info {
    struct hash_entry *h;
    uaecptr stopaddr;
    int can_compile_last;
    struct bb_info *bb_next1, *bb_next2;
    int flags_live_at_end;
    int flags_live_at_start;
    int first_iip, last_iip;
} bb_stack[BB_STACKSIZE];

static int top_bb;

static uaecptr bcc_target_stack[BB_STACKSIZE];

static int new_bcc_target(uaecptr addr)
{
    int i;

    for (i = 0; i < top_bb; i++)
	if (bcc_target_stack[i] == addr)
	    return 1;

    if (top_bb == BB_STACKSIZE)
	return 0;
    bcc_target_stack[top_bb++] = addr;
    return 1;
}

static int bcc_compfn(const void *a, const void *b)
{
    uaecptr *a1 = (uaecptr *)a, *b1 = (uaecptr *)b;

    if (*a1 == *b1)
	printf("BUG!!\n");

    if (*a1 < *b1)
	return 1;
    return -1;
}

static int bb_compfn(const void *a, const void *b)
{
    struct bb_info *a1 = (struct bb_info *)a, *b1 = (struct bb_info *)b;

    if (a1->h->addr == b1->h->addr)
	printf("BUG!!\n");

    if (a1->h->addr < b1->h->addr)
	return -1;
    return 1;
}

static int find_basic_blocks(struct hash_entry *h)
{
    int current_bb = 0;

    top_bb = 0;
    bcc_target_stack[0] = h->addr;
    new_bcc_target(h->addr);

    while (top_bb > current_bb) {
	uaecptr addr = bcc_target_stack[current_bb];
	int ninsns = 0;
	uae_u8 *realpc = get_real_address(addr);
	uae_u8 *rpc_start = realpc;

	for(;;) {
	    uaecptr thisinsn_addr = (realpc - rpc_start) + addr;
	    uae_u8 *rpc_save = realpc;
	    struct instr *dp = translate_getnextinsn(&realpc);
	    uaecptr nextinsn_addr = (realpc - rpc_start) + addr;

	    if (dp->mnemo == i_RTS || dp->mnemo == i_RTE
		|| dp->mnemo == i_RTR || dp->mnemo == i_RTD
		|| dp->mnemo == i_JMP || dp->mnemo == i_ILLG)
	    {
		break;
	    }

	    if (dp->mnemo == i_BSR || dp->mnemo == i_JSR) {
		if (!new_bcc_target(nextinsn_addr))
		    return 0;
		break;
	    }

	    if (dp->mnemo == i_DBcc) {
		uaecptr newaddr = thisinsn_addr + 2 + (uae_s16)((*(rpc_save+2) << 8) | *(rpc_save+3));
		if (!new_bcc_target(nextinsn_addr))
		    return 0;
		if (!new_bcc_target(newaddr))
		    return 0;
		break;
	    }

	    if (dp->mnemo == i_Bcc) {
		uaecptr newaddr;
		if (dp->smode == imm1)
		    newaddr = thisinsn_addr + 2 + (uae_s16)((*(rpc_save+2) << 8) | *(rpc_save+3));
		else
		    newaddr = thisinsn_addr + 2 + (uae_s8)dp->sreg;

		if (dp->cc != 0)
		    if (!new_bcc_target(nextinsn_addr))
			return 0;
		if (!new_bcc_target(newaddr))
		    return 0;
		break;
	    }
	}
	current_bb++;
    }

    qsort(bcc_target_stack, top_bb, sizeof (uaecptr), bcc_compfn);

    return 1;
}

static int m68k_scan_func(struct hash_entry *h)
{
    int i;
    struct hash_block *found_block;
    struct hash_entry **hepp;

    if (!find_basic_blocks(h))
	return 0;

    found_block = NULL;

    /* First, lock the hash entries we already have to prevent grief */
    for (i = 0; i < top_bb; i++) {
	struct hash_entry *h = find_hash(bcc_target_stack[i]);
	if (h != NULL)
	    h->locked = 1;
    }

    /* Allocate new ones */
    for (i = 0; i < top_bb; i++) {
	struct hash_entry *h = get_hash_for_func(bcc_target_stack[i], 1);
	bb_stack[i].h = h;
#if 0 /* This doesn't work in all cases */
	if (h->block != NULL && h->block != found_block) {
	    if (found_block == NULL) {
		if (h->block->cpage != NULL)
		    fprintf(stderr, "Found compiled code\n");
		else
		    found_block = h->block;
	    } else {
		fprintf(stderr, "Multiple blocks found.\n");
		if (h->block->cpage == NULL)
		    forget_block(h->block);
		else if (found_block->cpage == NULL) {
		    forget_block(found_block);
		    found_block = h->block;
		} else
		    fprintf(stderr, "Bad case.\n");
	    }
	}
#endif
    }
    if (found_block == NULL) {
	found_block = new_block();

	found_block->lru_next = &lru_first_block;
	found_block->lru_prev = lru_first_block.lru_prev;
	found_block->lru_prev->lru_next = found_block;
	lru_first_block.lru_prev = found_block;
    }

    hepp = &found_block->he_first;
    found_block->he_first = NULL;
    for (i = 0; i < top_bb; i++) {
	struct bb_info *bb = bb_stack + i;

	if (bb->h->block == NULL) {
	    num_unused_hash--;
	    lru_touch(bb->h);
	    bb->h->block = found_block;
	    *hepp = bb->h;
	    hepp = &bb->h->next_same_block;
	}
    }
    *hepp = found_block->he_first;
    return 1;
}

struct ea_reg_info {
    enum { eat_reg, eat_imem, eat_amem, eat_const } ea_type;
    int regs_set:16;
    int regs_used:16;
    int nr_scratch;
    uae_u32 temp1, temp2;
};

#define MAX_TRANSLATE 2048
struct insn_info_struct {
    uaecptr address;
    struct instr *dp;
    int flags_set;
    int flags_used;
    int flags_live_at_end;
    int jump_target;
    int jumps_to;
    char *compiled_jumpaddr; /* Address to use for jumps to this insn */
    char *compiled_fillin;   /* Address where to put offset if this is a Bcc */
    int regs_set:16;
    int regs_used:16;
    int stop_translation:2;
    int sync_cache:1;
    int sync_flags:1;
    int ccuser_follows:1;
} insn_info [MAX_TRANSLATE];

#define EA_NONE 0
#define EA_LOAD 1
#define EA_STORE 2
#define EA_MODIFY 4

#if 0
static void analyze_ea_for_insn(amodes mode, int reg, wordsizes size,
				struct ea_reg_info *eai,
				uae_u8 **pcpp, uaecptr pca,
				int ea_purpose)
{
    uae_u8 *p = *pcpp;

    switch(mode) {
     case Dreg:
	eai->ea_type = eat_reg;
	if (size != sz_long && (ea_purpose & EA_STORE))
	    ea_purpose |= EA_LOAD;
	if (ea_purpose & EA_LOAD)
	    eai->regs_used |= 1 << reg;
	if (ea_purpose & EA_STORE)
	    eai->regs_set |= 1 << reg;
	break;

     case Areg:
	eai->ea_type = eat_reg;
	if (size != sz_long && (ea_purpose & EA_STORE))
	    printf("Areg != long\n");
	if (ea_purpose & EA_LOAD)
	    eai->regs_used |= 1 << (8+reg);
	if (ea_purpose & EA_STORE)
	    eai->regs_set |= 1 << (8+reg);
	break;

     case Ad16:
     case Aind:
     case Apdi:
     case Aipi:
	eai->ea_type = eat_imem;
	eai->regs_used |= 1 << (8+reg);
	break;

     case Ad8r:
	eai->ea_type = eat_imem;
	pii->regs_used |= 1 << (8+reg);

	eai->temp = (uae_u16)((*p << 8) | *(p+1));
	r = (eai->temp & 0x7000) >> 12;
	(*pcpp) += 2; p += 2;

	if (eai->temp1 & 0x8000)
	    pii->regs_used |= 1 << (8+r);
	else
	    pii->regs_used |= 1 << r;
	break;

     case PC8r:
	eai->ea_type = eat_imem;
	eai->temp1 = (uae_u16)do_get_mem_word((uae_u16 *)p);
	eai->temp2 = pca + (uae_s8)eai->temp1;
	(*pcpp) += 2; p += 2;
	r = (eai->temp1 & 0x7000) >> 12;

	if (eai->temp1 & 0x8000)
	    pii->regs_used |= 1 << (8+r);
	else
	    pii->regs_used |= 1 << r;
	break;

     case PC16:
	eai->ea_type = eat_amem;
	eai->temp1 = pca + (uae_s16)do_get_mem_word((uae_u16 *)p);
	(*pcpp) += 2;
	break;

     case absw:
	eai->ea_type = eat_amem;
	eai->temp1 = (uae_s16)do_get_mem_word((uae_u16 *)p);
	(*pcpp) += 2;
	break;

     case absl:
	eai->ea_type = eat_amem;
	eai->temp1 = (uae_s32)do_get_mem_long((uae_u32 *)p);
	(*pcpp) += 4;
	break;

     case imm:
	if (size == sz_long)
	    goto imm2_const;
	if (size == sz_word)
	    goto imm1_const;

	/* fall through */
     case imm0:
	eai->ea_type = eat_imm;
	eai->temp1 = (uae_s8)*(p+1);
	(*pcpp) += 2;
	break;

     case imm1:
	imm1_const:
	eai->ea_type = eat_imm;
	eai->temp1 = (uae_s16)do_get_mem_word((uae_u16 *)p);
	(*pcpp) += 2;
	break;

     case imm2:
	imm2_const:
	eai->ea_type = eat_imm;
	eai->temp1 = (uae_s32)do_get_mem_long((uae_u32 *)p);
	(*pcpp) += 4;
	break;

     case immi:
	eai->ea_type = eat_imm;
	eai->temp1 = (uae_s8)reg;
	break;

     default:
	break;
    }
}
#endif
static struct bb_info *find_bb(struct hash_entry *h)
{
    int i;

    if (h == NULL)
	printf("Bug...\n");

    for (i = 0; i < top_bb; i++)
	if (bb_stack[i].h == h)
	    return bb_stack + i;
    if (!quiet_compile)
	fprintf(stderr, "BB not found!\n");
    return NULL;
}

static int m68k_scan_block(struct hash_block *hb, int *movem_count)
{
    struct hash_entry *h = hb->he_first;
    int i, iip, last_iip;
    int changed, round;

    top_bb = 0;

    do {
	struct bb_info *bb = bb_stack + top_bb;
	bb->h = h;
	bb->bb_next1 = NULL;
	bb->bb_next2 = NULL;
	h = h->next_same_block;
	top_bb++;
    } while (h != hb->he_first);

    qsort(bb_stack, top_bb, sizeof (struct bb_info), bb_compfn);

    *movem_count = 0;

    iip = 0;
    for (i = 0; i < top_bb; i++) {
	struct bb_info *bb = bb_stack + i;
	uae_u8 *realpc = get_real_address(bb->h->addr);
	uae_u8 *rpc_start = realpc;
	uaecptr stop_addr = 0;
	int live_at_start = 31, may_clear_las = 31;
	struct insn_info_struct *prev_ii = NULL;

	if (i < top_bb - 1)
	    stop_addr = (bb+1)->h->addr;
	bb->first_iip = iip;

	for (;;) {
	    struct insn_info_struct *thisii = insn_info + iip;
	    uaecptr thisinsn_addr = (realpc - rpc_start) + bb->h->addr;
	    uae_u8 *rpc_save = realpc;
	    struct instr *dp = translate_getnextinsn(&realpc);
	    uaecptr nextinsn_addr = (realpc - rpc_start) + bb->h->addr;

	    int fset = dp->flagdead == -1 ? 31 : dp->flagdead;
	    int fuse = dp->flaglive == -1 ? 31 : dp->flaglive;

	    if (thisinsn_addr == stop_addr) {
		bb->bb_next1 = find_bb (find_hash (thisinsn_addr));
		break;
	    }

	    if (dp->mnemo == i_Scc || dp->mnemo == i_Bcc || dp->mnemo == i_DBcc) {
		fset = 0, fuse = cc_flagmask_68k(dp->cc);
		if (prev_ii && dp->mnemo != i_Scc) /* Don't use Scc here: ea can cause an exit */
		    prev_ii->ccuser_follows = 1;
	    }

	    may_clear_las &= ~fuse;
	    live_at_start &= ~(fset & may_clear_las);

	    thisii->dp = dp;
	    thisii->address = thisinsn_addr;
	    thisii->stop_translation = 0;
	    thisii->ccuser_follows = 0;
/*	    thisii->have_reginfo = 0;*/
	    thisii->jump_target = 0;
	    thisii->sync_cache = thisii->sync_flags = 0;
	    thisii->flags_set = fset;
	    thisii->flags_used = fuse;
	    thisii->regs_set = 0;
	    thisii->regs_used = 0;
	    iip++;
	    if (iip == MAX_TRANSLATE)
		return 0;

	    if (dp->mnemo == i_RTS || dp->mnemo == i_RTE
		|| dp->mnemo == i_RTR || dp->mnemo == i_RTD
		|| dp->mnemo == i_JMP || dp->mnemo == i_ILLG)
	    {
		thisii->flags_used = 31;
		thisii->regs_used = 65535;
		thisii->stop_translation = dp->mnemo == i_RTS || dp->mnemo == i_JMP ? 2 : 1;
		break;
	    }
	    if (dp->mnemo == i_BSR || dp->mnemo == i_JSR)
	    {
		thisii->flags_used = 31;
		thisii->regs_used = 65535;
		bb->can_compile_last = 1;
		bb->bb_next1 = find_bb (get_hash_for_func (nextinsn_addr, 1));
		if (bb->bb_next1 == NULL)
		    thisii->stop_translation = 1;
		break;
	    }

	    if (dp->mnemo == i_DBcc) {
		uaecptr newaddr = thisinsn_addr + 2 + (uae_s16)((*(rpc_save+2) << 8) | *(rpc_save+3));
		bb->can_compile_last = 1;
		bb->bb_next1 = find_bb (get_hash_for_func (newaddr, 1));
		if (bb->bb_next1 == NULL)
		    thisii->stop_translation = 1;
		bb->bb_next2 = find_bb (get_hash_for_func (nextinsn_addr, 1));
		if (bb->bb_next2 == NULL)
		    thisii->stop_translation = 1;
		thisii->regs_used = 65535;
		break;
	    }

	    if (dp->mnemo == i_Bcc) {
		uaecptr newaddr;
		if (dp->smode == imm1)
		    newaddr = thisinsn_addr + 2 + (uae_s16)((*(rpc_save+2) << 8) | *(rpc_save+3));
		else
		    newaddr = thisinsn_addr + 2 + (uae_s8)dp->sreg;
		bb->can_compile_last = 1;
		bb->bb_next1 = find_bb(get_hash_for_func(newaddr, 1));
		if (bb->bb_next1 == NULL)
		    thisii->stop_translation = 1;
		if (dp->cc != 0) {
		    bb->bb_next2 = find_bb(get_hash_for_func(nextinsn_addr, 1));
		    if (bb->bb_next2 == NULL)
			thisii->stop_translation = 1;
		}
		thisii->regs_used = 65535;
		break;
	    }

	    if (dp->mnemo == i_MVMLE || dp->mnemo == i_MVMEL) {
		uae_u16 regmask = (*(rpc_save + 2) << 8) | (*(rpc_save + 3));
		*movem_count += count_bits(regmask);
		if (dp->dmode == Apdi)
		    regmask = bitswap(regmask);
		if (dp->mnemo == i_MVMLE)
		    thisii->regs_used = regmask;
		else
		    thisii->regs_set = regmask;
	    }

	    prev_ii = thisii;
	}
	bb->last_iip = iip - 1;
	bb->flags_live_at_start = live_at_start;
    }
    last_iip = iip;
    round = 0;
    do {
	changed = 0;
	for (i = 0; i < top_bb; i++) {
	    struct bb_info *bb = bb_stack + i;
	    int mnemo;
	    int current_live;
	    struct instr *dp;

	    iip = bb->last_iip;
	    mnemo = insn_info[iip].dp->mnemo;

	    /* Fix up branches */
	    if (round == 0 && (mnemo == i_DBcc || mnemo == i_Bcc)) {
		if (bb->bb_next1 != NULL) {
		    insn_info[bb->last_iip].jumps_to = bb->bb_next1->first_iip;
		    insn_info[bb->bb_next1->first_iip].jump_target = 1;
		}
	    }

	    /* And take care of flag life information */
	    dp = insn_info[iip].dp;
	    if (insn_info[iip].stop_translation)
		current_live = 31;
	    else if (dp->mnemo == i_DBcc || dp->mnemo == i_Bcc) {
		current_live = 0;
		if (bb->bb_next1 != NULL)
		    current_live |= bb->bb_next1->flags_live_at_start;
		if (bb->bb_next2 != NULL)
		    current_live |= bb->bb_next2->flags_live_at_start;
	    } else {
		if (bb->bb_next1 == NULL && bb->bb_next2 == NULL)
		    fprintf(stderr, "Can't happen\n");
		current_live = 0;
		if (bb->bb_next1 != NULL)
		    current_live |= bb->bb_next1->flags_live_at_start;
		if (bb->bb_next2 != NULL)
		    current_live |= bb->bb_next2->flags_live_at_start;
	    }

	    do {
		insn_info[iip].flags_live_at_end = current_live;
		current_live &= ~insn_info[iip].flags_set;
		current_live |= insn_info[iip].flags_used;
	    } while (iip-- != bb->first_iip);

	    if (bb->flags_live_at_start != current_live && !quiet_compile)
		fprintf(stderr, "Fascinating %d!\n", round), changed = 1;
	    bb->flags_live_at_start = current_live;
	}
	round++;
    } while (changed);
    return last_iip;
}

#define MAX_JSRS 4096 /* must be a power of two */

static uaecptr jsr_rets[MAX_JSRS];
static struct hash_entry *jsr_hash[MAX_JSRS];
static int jsr_num;
static struct hash_entry dummy_hash; /* This is for safety purposes only */


static void jsr_stack_init(void)
{
    jsr_num = 0;
    dummy_hash.execute = NULL;
}

void compiler_flush_jsr_stack(void)
{
    jsr_num = 0;
}

void m68k_do_rts(void)
{
    m68k_setpc(get_long(m68k_areg(regs, 7)));
    m68k_areg(regs, 7) += 4;
    if (jsr_num > 0)
	jsr_num--;
}

void m68k_do_jsr(uaecptr oldpc, uaecptr dest)
{
    struct hash_entry *h = find_hash(oldpc);

    if (jsr_num == MAX_JSRS)
	compiler_flush_jsr_stack();
    if (h == NULL) {
	jsr_hash[jsr_num] = &dummy_hash;
	jsr_rets[jsr_num++] = 0xC0DEDBAD;
    } else {
	jsr_hash[jsr_num] = h;
	jsr_rets[jsr_num++] = oldpc;
    }
    m68k_areg(regs, 7) -= 4;
    put_long(m68k_areg(regs, 7), oldpc);
    m68k_setpc(dest);
}

void m68k_do_bsr(uaecptr oldpc, uae_s32 offset)
{
    m68k_do_jsr(oldpc, m68k_getpc() + offset);
}

/* Here starts the actual compiling part */

static char *compile_current_addr;
static char *compile_last_addr;

static __inline__ void assemble(uae_u8 a)
{
    if (compile_current_addr < compile_last_addr) {
	*compile_current_addr++ = a;
    } else {
	compile_failure = 1;
    }
}

static __inline__ void assemble_ulong(uae_u32 a)
{
    assemble(a);
    assemble(a >> 8);
    assemble(a >> 16);
    assemble(a >> 24);
}

static __inline__ void assemble_ulong_68k(uae_u32 a)
{
    assemble(a >> 24);
    assemble(a >> 16);
    assemble(a >> 8);
    assemble(a);
}

static __inline__ void assemble_uword(uae_u16 a)
{
    assemble(a);
    assemble(a >> 8);
}

static __inline__ void assemble_long(void *a)
{
    assemble_ulong((uae_u32)a);
}

static __inline__ void compile_org(char *addr)
{
    compile_current_addr = addr;
}

static __inline__ char *compile_here(void)
{
    return compile_current_addr;
}

#define r_EAX 0
#define r_ECX 1
#define r_EDX 2
#define r_EBX 3
#define r_ESP 4
#define r_EBP 5
#define r_ESI 6
#define r_EDI 7

#define r_AH 0x84
#define r_CH 0x85
#define r_DH 0x86
#define r_BH 0x87

#define ALL_X86_REGS 255
#define ADDRESS_X86_REGS ((1 << r_EBP) | (1 << r_ESI) | (1 << r_EDI))
#define DATA_X86_REGS ((1 << r_EAX) | (1 << r_EDX) | (1 << r_EBX) | (1 << r_ECX))

#define BO_NORMAL 0
#define BO_SWAPPED_LONG 1
#define BO_SWAPPED_WORD 2

struct register_mapping {
    int dreg_map[8], areg_map[8]; /* 68000 register cache */
    int x86_const_offset[8];
    int x86_dirty[8];
    int x86_cache_reg[8]; /* Regs used for the 68000 register cache */
    int x86_cr_type[8]; /* Caching data or address register? */
    int x86_locked[8]; /* Regs used for some purpose */
    int x86_users[8];
    int x86_byteorder[8];
    int x86_verified[8];
};

/*
 * First, code to compile some primitive x86 instructions
 */

static void compile_lea_reg_with_offset(int dstreg, int srcreg, uae_u32 srcoffs)
{
    assemble(0x8D);
    if (srcreg == -2) {
	assemble(0x05 + 8*dstreg);
	assemble_ulong(srcoffs);
    } else if ((uae_s32)srcoffs >= -128 && (uae_s32)srcoffs <= 127) {
	assemble(0x40 + 8*dstreg + srcreg);
	assemble(srcoffs);
    } else {
	assemble(0x80 + 8*dstreg + srcreg);
	assemble_ulong(srcoffs);
    }
}

static void compile_move_reg_reg(int dstreg, int srcreg, wordsizes size)
{
    if (size == sz_byte
	&& (((1 << dstreg) & DATA_X86_REGS) == 0
	    || ((1 << srcreg) & DATA_X86_REGS) == 0))
    {
	fprintf(stderr, "Moving wrong register types!\n");
    }
    if (size == sz_word)
	assemble(0x66);
    if (size == sz_byte)
	assemble(0x88);
    else
	assemble(0x89);
    assemble(0xC0 + dstreg + 8*srcreg);
}

static void compile_move_between_reg_mem_regoffs(int dstreg, int srcreg,
						 uae_u32 srcoffs, wordsizes size,
						 int code)
{
    if (size == sz_byte && (dstreg & 0x80) != 0)
	dstreg &= ~0x80;
    else if ((size == sz_byte
	      && ((1 << dstreg) & DATA_X86_REGS) == 0)
	     || (size != sz_byte && (dstreg & 0x80) != 0))
    {
	fprintf(stderr, "Moving wrong register types!\n");
    }
    if (size == sz_word)
	assemble(0x66);
    if (size == sz_byte)
	assemble(code);
    else
	assemble(code + 1);

    if (srcreg == -2) {
	assemble(0x05 + 8*dstreg);
	assemble_ulong(srcoffs);
    } else if ((uae_s32)srcoffs >= -128 && (uae_s32)srcoffs <= 127) {
	assemble(0x40 + 8*dstreg + srcreg);
	assemble(srcoffs);
    } else {
	assemble(0x80 + 8*dstreg + srcreg);
	assemble_ulong(srcoffs);
    }
}

static void compile_move_reg_from_mem_regoffs(int dstreg, int srcreg,
					      uae_u32 srcoffs, wordsizes size)
{
    compile_move_between_reg_mem_regoffs(dstreg, srcreg, srcoffs, size, 0x8A);
}

static void compile_move_reg_to_mem_regoffs(int dstreg, uae_u32 dstoffs,
					    int srcreg, wordsizes size)
{
    compile_move_between_reg_mem_regoffs(srcreg, dstreg, dstoffs, size, 0x88);
}

static void compile_byteswap(int x86r, wordsizes size, int save_flags)
{
    switch(size) {
     case sz_word:
	if (save_flags)
	    assemble(0x9C);
	assemble(0x66); /* rolw $8,x86r */
	assemble(0xC1);
	assemble(0xC0 + x86r);
	assemble(8);
	if (save_flags)
	    assemble(0x9D);
	break;
     case sz_long:
	assemble(0x0F); /* bswapl x86r */
	assemble(0xC8+x86r);
	break;
     default:
	break;
    }
}

static void compile_force_byteorder(struct register_mapping *map, int x86r,
				    int desired_bo, int save_flags)
{
    if (x86r < 0 || map->x86_byteorder[x86r] == desired_bo)
	return;

    if (map->x86_byteorder[x86r] == BO_SWAPPED_LONG)
	compile_byteswap(x86r, sz_long, save_flags);
    else if (map->x86_byteorder[x86r] == BO_SWAPPED_WORD)
	compile_byteswap(x86r, sz_word, save_flags);

    if (desired_bo == BO_SWAPPED_LONG)
	compile_byteswap(x86r, sz_long, save_flags);
    else if (desired_bo == BO_SWAPPED_WORD)
	compile_byteswap(x86r, sz_word, save_flags);
    map->x86_byteorder[x86r] = desired_bo;
}

/* Add a constant offset to a x86 register. If it's in the cache, make sure
 * we update the const_offset value. The flags are unaffected by this */

static void compile_offset_reg(struct register_mapping *map, int x86r,
			       uae_u32 offset)
{
    int cached_68k;

    if (offset == 0 || x86r == -1 || x86r == -2)
	return;

    compile_force_byteorder(map, x86r, BO_NORMAL, 1);
    cached_68k = map->x86_cache_reg[x86r];
    if (cached_68k != -1) {
	map->x86_const_offset[x86r] -= offset;
	map->x86_dirty[x86r] = 1;
    }
    compile_lea_reg_with_offset(x86r, x86r, offset);
}

static int get_unused_x86_register(struct register_mapping *map)
{
    int x86r;
    for (x86r = 0; x86r < 24; x86r++) {
	if (map->x86_cache_reg[x86r] != -1)
	    continue;
	if (map->x86_users[x86r] > 0)
	    continue;

	map->x86_verified[x86r] = 0;
	map->x86_byteorder[x86r] = BO_NORMAL;
	return x86r;
    }
    return -1;
}

/*
 * sync_reg() may not touch the flags
 * If may_clobber is 1 and the reg had an offset, the reg will be offsetted
 * by this function
 */
static void sync_reg(struct register_mapping *map, int x86r, void *m68kr,
		     uae_u32 offset, int dirty, int may_clobber)
{
    if (dirty || offset != 0)
	compile_force_byteorder(map, x86r, BO_NORMAL, 1);
    if (offset != 0) {
	if (may_clobber) {
	    compile_lea_reg_with_offset(x86r, x86r, offset);
	    dirty = 1;
	} else {
	    int tmpr = get_unused_x86_register(map);
	    if (tmpr != -1) {
		compile_lea_reg_with_offset(tmpr, x86r, offset);
		x86r = tmpr;
		dirty = 1;
	    } else {
		compile_lea_reg_with_offset(x86r, x86r, offset);
		assemble(0x89);          /* movl x86r,m68kr */
		assemble(0x05 + (x86r << 3));
		assemble_long(m68kr);
		compile_lea_reg_with_offset(x86r, x86r, -offset);
		return;
	    }
	}
    }
    if (dirty) {
	assemble(0x89);          /* movl x86r,m68kr */
	assemble(0x05 + (x86r << 3));
	assemble_long(m68kr);
    }
}

static void sync_reg_cache(struct register_mapping *map, int flush)
{
    int i;

    for (i = 0; i < 8; i++) {
	int cr68k = map->x86_cache_reg[i];
	if (cr68k != -1) {
	    if (map->x86_cr_type[i] == 1) {
		sync_reg(map, i, regs.regs + cr68k, map->x86_const_offset[i], map->x86_dirty[i], 1);
		if (flush)
		    map->dreg_map[cr68k] = -1;
	    } else {
		sync_reg(map, i, regs.regs + 8 + cr68k, map->x86_const_offset[i], map->x86_dirty[i], 1);
		if (flush)
		    map->areg_map[cr68k] = -1;
	    }
	    if (flush)
		map->x86_cache_reg[i] = -1;
	    map->x86_const_offset[i] = 0;
	}
    }
    memset(map->x86_dirty, 0, sizeof map->x86_dirty);
}

static void remove_x86r_from_cache(struct register_mapping *map, int x86r,
				   int may_clobber)
{
    int j;
    int reg_68k;

    if (x86r == -1)
	return;

    reg_68k = map->x86_cache_reg[x86r];

    if (reg_68k == -1)
	return;

    if (map->x86_cr_type[x86r] == 1) {
	map->dreg_map[reg_68k] = -1;
	sync_reg(map, x86r, regs.regs + reg_68k, map->x86_const_offset[x86r],
		 map->x86_dirty[x86r], may_clobber);
    } else {
	map->areg_map[reg_68k] = -1;
	sync_reg(map, x86r, regs.regs + 8 + reg_68k,  map->x86_const_offset[x86r],
		 map->x86_dirty[x86r], may_clobber);
    }
    map->x86_dirty[x86r] = 0;
    map->x86_cache_reg[x86r] = -1;
    map->x86_const_offset[x86r] = 0;
    map->x86_verified[x86r] = 0;
    map->x86_byteorder[x86r] = BO_NORMAL;
}

static int get_free_x86_register(struct register_mapping *map,
				 int preferred_mask)
{
    int cnt;
    for (cnt = 0; cnt < 24; cnt++) {
	int x86r = cnt & 7;
	/* In the first two passes, try to get one of the preferred regs */
	if (cnt < 16 && ((1 << x86r) & preferred_mask) == 0)
	    continue;
	/* In the first pass, don't discard any registers from the cache */
	if (cnt < 8 && map->x86_cache_reg[x86r] != -1)
	    continue;
	/* Never use locked registers */
	if (map->x86_users[x86r] > 0)
	    continue;

	remove_x86r_from_cache(map, x86r, 1);
	map->x86_dirty[x86r] = 0;
	map->x86_cache_reg[x86r] = -1;
	map->x86_const_offset[x86r] = 0;
	map->x86_verified[x86r] = 0;
	map->x86_byteorder[x86r] = BO_NORMAL;
	return x86r;
    }
    printf("Out of registers!\n");
    return -1;
}

static int get_typed_x86_register(struct register_mapping *map,
				  int preferred_mask)
{
    int cnt;
    for (cnt = 0; cnt < 16; cnt++) {
	int x86r = cnt & 7;
	/* Get one of the preferred regs */
	if (((1 << x86r) & preferred_mask) == 0)
	    continue;
	/* In the first pass, don't discard any registers from the cache */
	if (cnt < 8 && map->x86_cache_reg[x86r] != -1)
	    continue;
	/* Never use locked registers */
	if (map->x86_users[x86r] > 0)
	    continue;

	remove_x86r_from_cache(map, x86r, 1);
	map->x86_dirty[x86r] = 0;
	map->x86_cache_reg[x86r] = -1;
	map->x86_const_offset[x86r] = 0;
	map->x86_verified[x86r] = 0;
	map->x86_byteorder[x86r] = BO_NORMAL;
	return x86r;
    }
    printf("Out of type registers!\n");
    return -1;
}

static void compile_unlock_reg(struct register_mapping *map, int reg)
{
    if (reg >= 0) {
	if (--map->x86_users[reg] == 0)
	    map->x86_locked[reg] = 0;

    }
}

static void lock_reg(struct register_mapping *map, int x86r, int lock_type)
{
#if 1
    switch (map->x86_locked[x86r]) {
     case 0:
	if (map->x86_users[x86r] != 0)
	    printf("Users for an unlocked reg!\n");
	break;
     case 1:
	if (lock_type == 2)
	    printf("Locking shared reg for exclusive use!\n");
	break;
     case 2:
	printf("Locking exclusive reg!\n");
	break;
     default:
	printf("Unknown lock?\n");
	break;
    }
#endif
    map->x86_locked[x86r] = lock_type;
    map->x86_users[x86r]++;
}

static int get_and_lock_68k_reg(struct register_mapping *map, int reg, int is_dreg,
				int preferred, int no_offset, int lock_type)
{
    int x86r;
    int *regmap;
    uae_u32 *reghome;
    uae_u32 const_off = 0;

    if (reg < 0 || reg > 7) {
	printf("Mad compiler disease\n");
	return 0;
    }

    if (is_dreg)
	regmap = map->dreg_map, reghome = regs.regs;
    else
	regmap = map->areg_map, reghome = regs.regs + 8;

    if (preferred == 0)
	preferred = ALL_X86_REGS;

    x86r = regmap[reg];
    if (x86r == -1) {
	x86r = get_free_x86_register(map, preferred);
	assemble(0x8B); assemble(0x05 + (x86r << 3)); /* movl regs.d[reg],x86r */
	assemble_long(reghome + reg);
	map->x86_cache_reg[x86r] = reg;
	map->x86_cr_type[x86r] = is_dreg;
	map->x86_const_offset[x86r] = 0;
	map->x86_dirty[x86r] = 0;
	map->x86_verified[x86r] = 0;
	map->x86_byteorder[x86r] = BO_NORMAL;
	regmap[reg] = x86r;
    } else {
	const_off = map->x86_const_offset[x86r];

	if (map->x86_locked[x86r] == 2
	    || (map->x86_locked[x86r] == 1 && (lock_type == 2 || (const_off != 0 && no_offset))))
	{
	    int newr;
	    int old_dirty = 0;
	    int old_verified;
	    int old_bo;

	    newr = get_free_x86_register(map, preferred);
	    if (const_off == 0) {
		compile_move_reg_reg(newr, x86r, sz_long);
	    } else {
		compile_force_byteorder(map, x86r, BO_NORMAL, 1);
		compile_lea_reg_with_offset(newr, x86r, const_off);
		old_dirty = 1;
		const_off = 0;
	    }
	    /* Remove old reg from cache... */
	    map->x86_cache_reg[x86r] = -1;
	    map->x86_cr_type[x86r] = is_dreg;
	    map->x86_const_offset[x86r] = 0;
	    old_dirty |= map->x86_dirty[x86r];
	    old_verified = map->x86_verified[x86r];
	    old_bo = map->x86_byteorder[x86r];
	    map->x86_verified[x86r] = 0;
	    map->x86_dirty[x86r] = 0;
	    x86r = newr;
	    /* ... and make the new one the cache register */
	    map->x86_cache_reg[x86r] = reg;
	    map->x86_cr_type[x86r] = is_dreg;
	    map->x86_const_offset[x86r] = 0;
	    map->x86_dirty[x86r] = old_dirty;
	    map->x86_verified[x86r] = old_verified;
	    map->x86_byteorder[x86r] = old_bo;
	    regmap[reg] = x86r;
	}
    }
    if (no_offset && const_off != 0) {
	if (map->x86_locked[x86r] != 0)
	    printf("modifying locked reg\n");
	compile_force_byteorder(map, x86r, BO_NORMAL, 1);
	compile_lea_reg_with_offset(x86r, x86r, map->x86_const_offset[x86r]);
	map->x86_const_offset[x86r] = 0;
	map->x86_dirty[x86r] = 1;
    }
    lock_reg(map, x86r, lock_type);
    return x86r;
}

/*
 * Move a constant to a register. Don't do anything if we already have a
 * register, even if it is offset by a constant
 */

static int compile_force_const_reg(struct register_mapping *map, int x86r,
				   uae_u32 *offs, int desired)
{
    int newr = x86r;

    if (newr == -2) {
	if (desired == 0)
	    newr = get_free_x86_register(map, ALL_X86_REGS);
	else
	    newr = get_typed_x86_register(map, desired);

	assemble(0xB8 + newr);
	assemble_ulong(*offs);
	*offs = 0;
    }
    map->x86_users[newr]++;
    return newr;
}

static void compile_extend_long(struct register_mapping *map, int x86r,
				wordsizes size)
{
    if (x86r < 0) {
	printf("Bad reg in extend_long\n");
	return;
    }

    compile_force_byteorder(map, x86r, BO_NORMAL, 1);

    if (size != sz_long) {
	if (x86r == r_EAX && size == sz_word) {
	    assemble(0x98); /* cwtl */
	} else {
	    assemble(0x0F);
	    if (size == sz_byte) {
		assemble(0xBE);
	    } else {
		assemble(0xBF);
	    }
	    assemble(0xC0 + x86r*9);
	}
    }
}

struct ea_info {
    int reg;
    amodes mode;
    wordsizes size;
    int address_reg;    /* The x86 reg holding the address, or -1 if ea doesn't refer to memory
			 * -2 if it refers to memory, but only with a constant address */
    uae_u32 addr_const_off; /* Constant offset to the address */
    int data_reg;         /* The x86 reg that holds the data. -1 if data is not present yet.
			   * -2 if data is constant */
    uae_u32 data_const_off;
    int flags;            /* Extra info. Contains the dp field of d8r modes */
    int purpose;
};

static void init_eainfo(struct ea_info *eai)
{
    eai->address_reg = -1;
    eai->addr_const_off = 0;
    eai->data_reg = -1;
    eai->data_const_off = 0;
}

struct insn_reg_needs {
    int checkpoint_no;
    int dreg_needed[8], areg_needed[8];
    int dreg_mask[8], areg_mask[8];
};

/*
 * This structure holds information about predec/postinc addressing modes.
 */

struct pid_undo {
    int used;
    int x86r[2];
    int m68kr[2];
    int dirty[2];
    int offs[2];
};

static void add_undo(struct pid_undo *pud, int x86r, int m68kr, int offs,
		     int dirty)
{
    int i;
    for (i = 0; i < pud->used; i++)
	if (pud->m68kr[i] == m68kr)
	    return;
    pud->m68kr[i] = m68kr;
    pud->x86r[i] = x86r;
    pud->offs[i] = offs;
    pud->dirty[i] = dirty;
    pud->used++;
}

/*
 * Lock previous contents of address registers used in predec/postinc modes
 * for generate_possible_exit().
 */

static void compile_prepare_undo(struct register_mapping *map, amodes mode,
			      int reg, struct pid_undo *pud)
{
    int x86r;

    switch(mode){
     default:
	break;

     case Apdi:
	x86r = get_and_lock_68k_reg(map, reg, 0, ADDRESS_X86_REGS, 0, 1);
	/* This saves recording the byteorder in the pud structure, and we'll
	 * need it in normal byteorder anyway */
	compile_force_byteorder(map, x86r, BO_NORMAL, 0);
	/*
	 * Add this reg with its current offset to the undo buffer.
	 * Since we have locked it, we are certain that it will not be
	 * modified.
	 */
	add_undo(pud, x86r, reg, map->x86_const_offset[x86r], map->x86_dirty[x86r]);
	break;

     case Aipi:
	x86r = get_and_lock_68k_reg(map, reg, 0, ADDRESS_X86_REGS, 0, 1);
	compile_force_byteorder(map, x86r, BO_NORMAL, 0);
	add_undo(pud, x86r, reg, map->x86_const_offset[x86r], map->x86_dirty[x86r]);
	break;
    }
}

/*
 * Load all the registers absolutely needed to calculate and verify thea
 * address. Load other registers if convenient.
 * This contains a fair amount of magic to get the register cache working right.
 */

static void compile_prepareea(struct register_mapping *map, amodes mode,
			      int reg, wordsizes size, uae_u8 **pcpp, uaecptr pca,
			      struct ea_info *eainf, int eaino, int ea_purpose,
			      int pidmult)
{
    struct ea_info *eai = eainf + eaino;
    int pdival = size == sz_byte && reg != 7 ? 1 : size == sz_long ? 4 : 2;
    uae_u8 *p = *pcpp;
    uae_u16 dp;
    int r;
    int x86r, tmpr;

    pdival *= pidmult;

    init_eainfo(eai);
    eai->mode = mode;
    eai->size = size;
    eai->reg = reg;

    switch(mode){
     case Dreg:
     case Areg:
	break;

     case Ad16:
	eai->addr_const_off = (uae_s16)do_get_mem_word((uae_u16 *)p);
	(*pcpp) += 2; p += 2;
	x86r = eai->address_reg = get_and_lock_68k_reg(map, reg, 0, ADDRESS_X86_REGS, 0, 1);
	compile_force_byteorder(map, x86r, BO_NORMAL, 0);
	eai->addr_const_off += map->x86_const_offset[x86r];
	break;

     case Aind:
	x86r = eai->address_reg = get_and_lock_68k_reg(map, reg, 0, ADDRESS_X86_REGS, 0, 1);
	compile_force_byteorder(map, x86r, BO_NORMAL, 0);
	eai->addr_const_off = map->x86_const_offset[x86r];
	break;

     case Apdi:
	x86r = eai->address_reg = get_and_lock_68k_reg(map, reg, 0, ADDRESS_X86_REGS, 0, 1);
	compile_force_byteorder(map, x86r, BO_NORMAL, 0);
	map->x86_const_offset[x86r] -= pdival;
	eai->addr_const_off = map->x86_const_offset[x86r];
	break;

     case Aipi:
	x86r = eai->address_reg = get_and_lock_68k_reg(map, reg, 0, ADDRESS_X86_REGS, 0, 1);
	compile_force_byteorder(map, x86r, BO_NORMAL, 0);
	eai->addr_const_off = map->x86_const_offset[x86r];
	map->x86_const_offset[x86r] += pdival;
	break;

     case Ad8r:
	dp = (uae_s16)do_get_mem_word((uae_u16 *)p);
	r = (dp & 0x7000) >> 12;
	(*pcpp) += 2; p += 2;

	tmpr = get_and_lock_68k_reg(map, reg, 0, ADDRESS_X86_REGS, 0, 1);
	compile_force_byteorder(map, tmpr, BO_NORMAL, 0);
	eai->addr_const_off = map->x86_const_offset[tmpr] + (uae_s8)dp;

	if (dp & 0x800) {
	    x86r = get_and_lock_68k_reg(map, r, dp & 0x8000 ? 0 : 1, ADDRESS_X86_REGS, 0, 2);
	    remove_x86r_from_cache(map, x86r, 0);
	    compile_force_byteorder(map, x86r, BO_NORMAL, 0);
	    eai->addr_const_off += map->x86_const_offset[x86r];
	} else {
	    x86r = get_and_lock_68k_reg(map, r, dp & 0x8000 ? 0 : 1, ADDRESS_X86_REGS, 1, 2);
	    remove_x86r_from_cache(map, x86r, 0);
	    compile_force_byteorder(map, x86r, BO_NORMAL, 0);
	}
	eai->address_reg = x86r;

	r = (dp & 0x7000) >> 12;

	if (dp & 0x800) {
	    if (eai->addr_const_off == 0) {
		assemble(0x03); assemble(0xC0 + tmpr + x86r*8); /* addl basereg,addrreg */
	    } else if ((uae_s32)eai->addr_const_off >= -128 && (uae_s32)eai->addr_const_off <= 127) {
		assemble(0x8D);
		assemble(0x44 + x86r*8); /* leal disp8(dispreg,basereg),dispreg */
		assemble(x86r*8 + tmpr);
		assemble(eai->addr_const_off);
	    } else {
		assemble(0x8D);
		assemble(0x84 + x86r*8); /* leal disp32(dispreg,basereg),dispreg */
		assemble(x86r*8 + tmpr);
		assemble_ulong(eai->addr_const_off);
	    }
	    eai->addr_const_off = 0;
	} else {
	    assemble(0x0F); assemble(0xBF);
	    assemble(0xC0 + x86r*9); /* movswl dispreg,addrreg */
	    assemble(0x03); assemble(0xC0 + tmpr + x86r*8); /* addl basereg,addrreg */
	}
	compile_unlock_reg(map, tmpr);
	break;

     case PC8r:
	dp = (uae_s16)do_get_mem_word((uae_u16 *)p);
	(*pcpp) += 2; p += 2;
	r = (dp & 0x7000) >> 12;
	eai->addr_const_off = pca + (uae_s8)dp;
	if (dp & 0x800) {
	    x86r = get_and_lock_68k_reg(map, r, dp & 0x8000 ? 0 : 1, ADDRESS_X86_REGS, 0, 1);
	    remove_x86r_from_cache(map, x86r, 0);
	    compile_force_byteorder(map, x86r, BO_NORMAL, 0);
	    eai->addr_const_off += map->x86_const_offset[x86r];
	} else {
	    x86r = get_and_lock_68k_reg(map, r, dp & 0x8000 ? 0 : 1, ADDRESS_X86_REGS, 1, 2);
	    remove_x86r_from_cache(map, x86r, 0);
	    compile_force_byteorder(map, x86r, BO_NORMAL, 0);

	    assemble(0x0F); assemble(0xBF);
	    assemble(0xC0 + x86r*9); /* movswl dispreg,addrreg */
	}
	eai->address_reg = x86r;
	break;

     case PC16:
	eai->addr_const_off = pca + (uae_s16)do_get_mem_word((uae_u16 *)p);
	eai->address_reg = -2;
	(*pcpp) += 2; p += 2;
	break;

     case absw:
	eai->addr_const_off = (uae_s16)do_get_mem_word((uae_u16 *)p);
	eai->address_reg = -2;
	(*pcpp) += 2; p += 2;
	break;

     case absl:
	eai->addr_const_off = (uae_s32)do_get_mem_long((uae_u32 *)p);
	eai->address_reg = -2;
	(*pcpp) += 4; p += 4;
	break;

     case imm:
	if (size == sz_long)
	    goto imm2_const;
	if (size == sz_word)
	    goto imm1_const;

	/* fall through */
     case imm0:
	eai->data_const_off = (uae_s8)*(p+1);
	eai->data_reg = -2;
	(*pcpp) += 2; p += 2;
	break;

     case imm1:
	imm1_const:
	eai->data_const_off = (uae_s16)do_get_mem_word((uae_u16 *)p);
	eai->data_reg = -2;
	(*pcpp) += 2; p += 2;
	break;

     case imm2:
	imm2_const:
	eai->data_const_off = (uae_s32)do_get_mem_long((uae_u32 *)p);
	eai->data_reg = -2;
	(*pcpp) += 4; p += 4;
	break;

     case immi:
	eai->data_const_off = (uae_s8)reg;
	eai->data_reg = -2;
	break;

     default:
	break;
    }
    eai->purpose = ea_purpose;
}

static void compile_get_excl_lock(struct register_mapping *map, struct ea_info *eai)
{
    int x86r = eai->data_reg;

    if (x86r >= 0 && map->x86_locked[x86r] == 1) {
	int newr;
	if (eai->size == sz_byte)
	    newr = get_typed_x86_register(map, DATA_X86_REGS);
	else
	    newr = get_free_x86_register(map, ALL_X86_REGS);

	compile_move_reg_reg(newr, x86r, sz_long);
	eai->data_reg = newr;
	lock_reg(map, eai->data_reg, 2);
    }
}

/*
 * Some functions to assemble some 386 opcodes which have a similar
 * structure (ADD, AND, OR, etc.). These take source and destination
 * addressing modes, check their validity and assemble a complete
 * 386 instruction.
 */

static __inline__ int rmop_long(struct ea_info *eai)
{
    if (eai->data_reg == -2)
	printf("rmop for const\n");
    else if (eai->data_reg == -1) {
	if (eai->address_reg == -2)
	    return 5;
	if (eai->address_reg == -1) {
	    /* This must be a 68k register in its home location */
	    return 5;
	}
#if 0 /* We need to add address_space... */
	if (eai->addr_const_off == 0 && eai->address_reg != r_EBP) {
	    return eai->address_reg;
	}
	else if ((uae_s32)eai->addr_const_off >= -128 && (uae_s32)eai->addr_const_off <= 127) {
	    return eai->address_reg | 0x40;
	}
#endif
	return eai->address_reg | 0x80;
    } else {
	if (eai->size == sz_byte && ((1 << eai->data_reg) & DATA_X86_REGS) == 0)
	    printf("wrong type reg in rmop\n");
	if (eai->data_const_off != 0)
	    printf("data_const_off in rmop\n");
	return 0xC0 + eai->data_reg;
    }
    return 0;
}

static __inline__ int rmop_short(struct ea_info *eai)
{
    if (eai->data_reg == -2)
	printf("rmop_short for const\n");
    else if (eai->data_reg == -1) {
	printf("rmop_short for mem\n");
    } else {
	if (eai->size == sz_byte && ((1 << eai->data_reg) & DATA_X86_REGS) == 0)
	    printf("wrong type reg in rmop_short\n");
	if (eai->data_const_off != 0)
	    printf("data_const_off in rmop_short\n");
	return eai->data_reg*8;
    }
    return 0;
}

static __inline__ void rmop_finalize(struct ea_info *eai)
{
    if (eai->data_reg == -2)
	assemble_ulong(eai->data_const_off);
    else if (eai->data_reg == -1) {
	if (eai->address_reg == -2)
	    /* Constant address */
	    assemble_long(address_space + (uae_s32)eai->addr_const_off);
	else if (eai->address_reg == -1) {
	    /* Register in its home location */
	    if (eai->mode == Areg)
		assemble_long(regs.regs + 8  + eai->reg);
	    else
		assemble_long(regs.regs + eai->reg);
	} else {
#if 0
	    /* Indirect address with offset */
	    if ((uae_s32)eai->addr_const_off >= -128 && (uae_s32)eai->addr_const_off <= 127) {
	    }
#endif
	    assemble_long(address_space + (uae_s32)eai->addr_const_off);
	}
    }
}

static void compile_eas(struct register_mapping *map, struct ea_info *eainf, int eaino_s, int eaino_d,
			int optype)
{
    struct ea_info *eais = eainf + eaino_s;
    struct ea_info *eaid = eainf + eaino_d;
    int szflag = eais->size == sz_byte ? 0 : 1;
    int swapflag = 0;
    int opcode;

    if (eais->data_reg == -1) {
	compile_force_byteorder(map, eais->address_reg, BO_NORMAL, 0);
	eais = eainf + eaino_d;
	eaid = eainf + eaino_s;
	swapflag = 1;
    }
    if (eais->data_reg == -1) {
	compile_force_byteorder(map, eais->address_reg, BO_NORMAL, 0);
    }

    if (eais->size == sz_word)
	assemble(0x66);

    if (eais->data_reg == -2) {
	assemble(0x80+szflag);
	assemble(8*optype | rmop_long(eaid));
	rmop_finalize(eaid);
	switch(eais->size) {
	 case sz_byte: assemble(eais->data_const_off); break;
	 case sz_word: assemble_uword(eais->data_const_off); break;
	 case sz_long: assemble_ulong(eais->data_const_off); break;
	}
    } else {
	assemble(8*optype | szflag | 2*swapflag);
	assemble(rmop_long(eaid) | rmop_short(eais));
	rmop_finalize(eaid);
    }
}

static void compile_fetchmem(struct register_mapping *map, struct ea_info *eai)
{
    int x86r;
    if (eai->size == sz_byte)
	x86r = get_typed_x86_register(map, DATA_X86_REGS);
    else
	x86r = get_free_x86_register(map, ALL_X86_REGS);

    lock_reg(map, x86r, 2);
    compile_force_byteorder(map, eai->address_reg, BO_NORMAL, 0);
    compile_move_reg_from_mem_regoffs(x86r, eai->address_reg,
				      (uae_u32)(eai->addr_const_off + address_space),
				      eai->size);
    map->x86_verified[x86r] = 0;
    switch (eai->size) {
     case sz_byte: map->x86_byteorder[x86r] = BO_NORMAL; break;
     case sz_word: map->x86_byteorder[x86r] = BO_SWAPPED_WORD; break;
     case sz_long: map->x86_byteorder[x86r] = BO_SWAPPED_LONG; break;
    }
    eai->data_reg = x86r;
    eai->data_const_off = 0;
}

static void compile_fetchimm(struct register_mapping *map, struct ea_info *eai, int byteorder)
{
    int x86r;
    if (eai->size == sz_byte)
	x86r = get_typed_x86_register(map, DATA_X86_REGS);
    else
	x86r = get_free_x86_register(map, ALL_X86_REGS);

    switch (byteorder) {
     case BO_SWAPPED_LONG:
	eai->data_const_off = (((eai->data_const_off & 0xFF000000) >> 24)
			       | ((eai->data_const_off & 0xFF0000) >> 8)
			       | ((eai->data_const_off & 0xFF00) << 8)
			       | ((eai->data_const_off & 0xFF) << 24));
	break;
     case BO_SWAPPED_WORD:
	eai->data_const_off = (((eai->data_const_off & 0xFF00) >> 8)
			       | ((eai->data_const_off & 0xFF) << 8)
			       | (eai->data_const_off & 0xFFFF0000));
	break;
     case BO_NORMAL:
	break;
    }
    lock_reg(map, x86r, 2);
    map->x86_byteorder[x86r] = byteorder; map->x86_verified[x86r] = 0;

    switch (eai->size) {
     case sz_byte: assemble(0xC6); assemble(0xC0 + x86r); assemble(eai->data_const_off); break;
     case sz_word: assemble(0x66); assemble(0xC7); assemble(0xC0 + x86r); assemble_uword(eai->data_const_off); break;
     case sz_long: assemble(0xC7); assemble(0xC0 + x86r); assemble_ulong(eai->data_const_off); break;
    }
    eai->data_reg = x86r;
    eai->data_const_off = 0;
}

/*
 * 1: reg
 * 2: mem
 * 4: imm
 */

static int binop_alternatives[] = {
    7, 1,
    5, 3,
    0, 0
};

static int binop_worda_alternatives[] = {
    1, 3,
    0, 0
};

static int regonly_alternatives[] = {
    1, 1,
    0, 0
};

static void compile_loadeas(struct register_mapping *map, struct ea_info *eainf,
			   int eaino_s, int eaino_d, int *alternatives,
			   int scramble_poss, int load_dest)
{
    struct ea_info *eais = eainf + eaino_s;
    struct ea_info *eaid = eainf + eaino_d;
    int scrambled_bo = eaid->size == sz_long ? BO_SWAPPED_LONG : eaid->size == sz_word ? BO_SWAPPED_WORD : BO_NORMAL;
    int i, scrambled = 0;
    int best = 0;
    int bestcost = -1;
    int *ap;
    uae_u32 *sregp = NULL, *dregp = NULL;
    int screg = -1, dcreg = -1;
    int stype = -1, dtype = -1;
    int asrc, adst;
    int regprefs = eais->size == sz_byte ? DATA_X86_REGS : 0;

    if (eais->mode == Dreg) {
	stype = 0;
	screg = map->dreg_map[eais->reg];
	if (screg == -1)
	    sregp = regs.regs + eais->reg;
    } else if (eais->mode == Areg) {
	stype = 0;
	screg = map->areg_map[eais->reg];
	if (screg == -1)
	    sregp = regs.regs + 8 + eais->reg;
    } else if (eais->data_reg == -2) {
	stype = -2;
    }

    if (eaid->mode == Dreg) {
	dtype = 0;
	dcreg = map->dreg_map[eaid->reg];
	if (dcreg == -1)
	    dregp = regs.regs + eaid->reg;
    } else if (eaid->mode == Areg) {
	dtype = 0;
	dcreg = map->areg_map[eaid->reg];
	if (dcreg == -1)
	    dregp = regs.regs + 8 + eaid->reg;
    } else if (eaid->data_reg == -2) {
	dtype = -2;
    }

    ap = alternatives;

    for (i = 0;; i++) {
	int cost = 0;

	asrc = *ap++;
	if (asrc == 0)
	    break;
	adst = *ap++;

	if (stype == -2 && (asrc & 4) == 0)
	    cost++;
	else if (stype == -1 && ((asrc & 2) == 0 || (eais->size != sz_byte && !scramble_poss)))
	    cost++;
	else if (stype == 0 && screg == -1 && (asrc & 2) == 0)
	    cost++;

	if (dtype == -1 && ((adst & 2) == 0 || (eaid->size != sz_byte && !scramble_poss)))
	    /* The !load_dest case isn't handled by the current code,
	     * and it isn't desirable anyway. Use a different alternative
	     */
	    cost += load_dest ? 1 : 100;
	else if (dtype == 0 && dcreg == -1 && (adst & 2) == 0)
	    cost++;

	if (bestcost == -1 || cost < bestcost) {
	    bestcost = cost;
	    best = i;
	}
    }

    asrc = alternatives[2*best];
    adst = alternatives[2*best+1];

    if (dtype == -1) {
	if (load_dest) {
	    if ((adst & 2) == 0 || (eaid->size != sz_byte && !scramble_poss))
		compile_fetchmem(map, eaid);
	} else {
	    if ((adst & 2) == 0) {
		printf("Not loading memory operand. Prepare to die.\n");
		if (eaid->size == sz_byte)
		    eaid->data_reg = get_typed_x86_register(map, DATA_X86_REGS);
		else
		    eaid->data_reg = get_free_x86_register(map, ALL_X86_REGS);
	    }
	}
	/* Scrambled in both mem and reg cases */
	if (eaid->size != sz_byte && scramble_poss)
	    scrambled = 1;
    } else {
	if (dcreg == -1 && !load_dest && (adst & 2) == 0 && eaid->size == sz_long) {
	    /* We need a register, but we don't need to fetch the old data.
	     * See storeea for some more code handling this case. This first
	     * if statement could be eliminated, we would generate some
	     * superfluous moves. This is an optimization. If it were not
	     * done, the mem-mem-move warning could be commented in in
	     * storeea. */
	    if (eaid->size == sz_byte)
		eaid->data_reg = get_typed_x86_register(map, DATA_X86_REGS);
	    else
		eaid->data_reg = get_free_x86_register(map, ALL_X86_REGS);
	    eaid->data_const_off = 0;
	} else if ((dcreg == -1 && (adst & 2) == 0) || dcreg != -1) {
	    int reg_bo;
	    eaid->data_reg = get_and_lock_68k_reg(map, eaid->reg, eaid->mode == Dreg, regprefs, 1, 2);
	    eaid->data_const_off = 0;

	    reg_bo = map->x86_byteorder[eaid->data_reg];

	    if (reg_bo != BO_NORMAL) {
		if (reg_bo != scrambled_bo)
		    compile_force_byteorder(map, eaid->data_reg, BO_NORMAL, 0);
		else if (scramble_poss)
		    scrambled = 1;
	    }
	}
    }

    if (stype == -2) {
	/* @@@ may need to scramble imm, this is a workaround */
	if ((asrc & 4) == 0 || scrambled)
	    compile_fetchimm(map, eais, scrambled ? scrambled_bo : BO_NORMAL);
    } else if (stype == -1) {
	if ((asrc & 2) == 0 || (eais->size != sz_byte && !scrambled))
	    compile_fetchmem(map, eais);
    } else {
	if ((screg == -1 && (asrc & 2) == 0) || screg != -1) {
	    eais->data_reg = get_and_lock_68k_reg(map, eais->reg, eais->mode == Dreg, regprefs, 1, 2);
	    eais->data_const_off = 0;
	}
    }

    /* Optimization */
    if (scrambled && eais->data_reg >= 0 && !load_dest
	&& map->x86_byteorder[eais->data_reg] == BO_NORMAL
	&& eaid->size == sz_long && dtype == 0)
	scrambled = 0;

    if (regprefs != 0 && eais->data_reg >= 0 && ((1 << eais->data_reg) & regprefs) == 0) {
	int tmpr = get_typed_x86_register(map, regprefs);
	compile_move_reg_reg(tmpr, eais->data_reg, sz_long);
	eais->data_reg = tmpr;
    }

    if (regprefs != 0 && eaid->data_reg >= 0 && ((1 << eaid->data_reg) & regprefs) == 0) {
	int tmpr = get_typed_x86_register(map, regprefs);
	compile_move_reg_reg(tmpr, eaid->data_reg, sz_long);
	eaid->data_reg = tmpr;
    }

    /* Now set the byteorder once and for all (should already be correct for
     * most cases) */
    if (scrambled) {
	if (eaid->data_reg >= 0)
	    compile_force_byteorder(map, eaid->data_reg, scrambled_bo, 0);
	if (eais->data_reg >= 0)
	    compile_force_byteorder(map, eais->data_reg, scrambled_bo, 0);
    } else {
	if (eaid->data_reg >= 0)
	    compile_force_byteorder(map, eaid->data_reg, BO_NORMAL, 0);
	if (eais->data_reg >= 0)
	    compile_force_byteorder(map, eais->data_reg, BO_NORMAL, 0);
    }
}

static void compile_fetchea(struct register_mapping *map, struct ea_info *eainf,
			    int eaino, int asrc)
{
    struct ea_info *eais = eainf + eaino;
    int scrambled_bo = eais->size == sz_long ? BO_SWAPPED_LONG : eais->size == sz_word ? BO_SWAPPED_WORD : BO_NORMAL;
    int i, scrambled = 0;
    int best = 0;
    int bestcost = -1;
    int *ap;
    uae_u32 *sregp = NULL;
    int screg = -1, stype = -1;
    int regprefs = eais->size == sz_byte ? DATA_X86_REGS : 0;

    if (eais->mode == Dreg) {
	stype = 0;
	screg = map->dreg_map[eais->reg];
	if (screg == -1)
	    sregp = regs.regs + eais->reg;
    } else if (eais->mode == Areg) {
	stype = 0;
	screg = map->areg_map[eais->reg];
	if (screg == -1)
	    sregp = regs.regs + 8 + eais->reg;
    } else if (eais->data_reg == -2) {
	stype = -2;
    }

    if (stype == -2) {
	if ((asrc & 4) == 0)
	    compile_fetchimm(map, eais, scrambled ? scrambled_bo : BO_NORMAL);
    } else if (stype == -1) {
	if ((asrc & 2) == 0 || eais->size != sz_byte)
	    compile_fetchmem(map, eais);
    } else {
	if ((screg == -1 && (asrc & 2) == 0) || screg != -1) {
	    eais->data_reg = get_and_lock_68k_reg(map, eais->reg, eais->mode == Dreg, regprefs, 1, 2);
	    eais->data_const_off = 0;
	}
    }

    if (eais->data_reg >= 0)
	compile_force_byteorder(map, eais->data_reg, BO_NORMAL, 0);
}

/*
 * compile_note_modify() should be called on destination EAs obtained from
 * compile_loadeas(), if their value was modified (e.g. by the compile_eas()
 * function)
 */

static void compile_note_modify(struct register_mapping *map, struct ea_info *eainf,
				int eaino)
{
    struct ea_info *eai = eainf + eaino;
    int newr;
    int szflag = eai->size == sz_byte ? 0 : 1;

    if (eai->mode == Dreg) {
	/* We only need to do something if we have the value in a register,
	 * otherwise, the home location was modified already */
	if (eai->data_reg >= 0) {
	    if (eai->data_reg != map->dreg_map[eai->reg]) {
		remove_x86r_from_cache(map, eai->data_reg, 0);
		if (map->dreg_map[eai->reg] >= 0)
		    remove_x86r_from_cache(map, map->dreg_map[eai->reg], 0);
		map->x86_cache_reg[eai->data_reg] = eai->reg;
		map->x86_cr_type[eai->data_reg] = 1;
		map->dreg_map[eai->reg] = eai->data_reg;
	    }
	    map->x86_verified[eai->data_reg] = 0;
	    map->x86_const_offset[eai->data_reg] = eai->data_const_off;
	    map->x86_dirty[eai->data_reg] = 1;
	}
	return;
    } else if (eai->mode == Areg) {
	if (eai->size != sz_long)
	    printf("Areg put != long\n");

	/* We only need to do something if we have the value in a register,
	 * otherwise, the home location was modified already */
	if (eai->data_reg >= 0) {
	    if (eai->data_reg != map->areg_map[eai->reg]) {
		remove_x86r_from_cache(map, eai->data_reg, 0);
		if (map->areg_map[eai->reg] >= 0)
		    remove_x86r_from_cache(map, map->areg_map[eai->reg], 0);
		map->x86_cache_reg[eai->data_reg] = eai->reg;
		map->x86_cr_type[eai->data_reg] = 0;
		map->areg_map[eai->reg] = eai->data_reg;
	    }
	    map->x86_verified[eai->data_reg] = 0;
	    map->x86_const_offset[eai->data_reg] = eai->data_const_off;
	    map->x86_dirty[eai->data_reg] = 1;
	}
	return;
    } else {
	/* Storing to memory from reg? */
	if (eai->data_reg >= 0) {
	    compile_offset_reg(map, eai->data_reg, eai->data_const_off);

	    switch (eai->size) {
	     case sz_byte: compile_force_byteorder(map, eai->data_reg, BO_NORMAL, 1); break;
	     case sz_word: compile_force_byteorder(map, eai->data_reg, BO_SWAPPED_WORD, 1); break;
	     case sz_long: compile_force_byteorder(map, eai->data_reg, BO_SWAPPED_LONG, 1); break;
	    }
	    compile_force_byteorder(map, eai->address_reg, BO_NORMAL, 0);
	    compile_move_reg_to_mem_regoffs(eai->address_reg,
					    (uae_u32)(eai->addr_const_off + address_space),
					    eai->data_reg, eai->size);
	}
    }
}

static void compile_storeea(struct register_mapping *map, struct ea_info *eainf,
			    int eaino_s, int eaino_d)
{
    struct ea_info *eais = eainf + eaino_s;
    struct ea_info *eaid = eainf + eaino_d;
    int newr, cacher;
    int szflag = eaid->size == sz_byte ? 0 : 1;

    if (eaid->mode == Dreg) {
	/* Is the reg to move from already the register cache reg for the
	 * destination? */
	if (eais->data_reg >= 0 && eais->data_reg == map->dreg_map[eaid->reg]) {
	    map->x86_dirty[eais->data_reg] = 1; map->x86_verified[eais->data_reg] = 0;
	    map->x86_const_offset[eais->data_reg] = eais->data_const_off;
	    return;
	}
	/* Is the destination register in its home location? */
	if (map->dreg_map[eaid->reg] < 0) {
	    if (eais->data_reg == -2) {
		/* Move immediate to regs.regs */
		if (eaid->size == sz_word) assemble(0x66);
		assemble(0xC6 + szflag); assemble(0x05); assemble_long(regs.regs + eaid->reg);
		switch (eaid->size) {
		 case sz_byte: assemble(eais->data_const_off); break;
		 case sz_word: assemble_uword(eais->data_const_off); break;
		 case sz_long: assemble_ulong(eais->data_const_off); break;
		}
	    } else if (eais->data_reg == -1) {
#if 0
		printf("Shouldn't happen (mem-mem-move)\n");
#endif
		/* This _can_ happen: move.l $4,d0, if d0 isn't in the
		 * cache, will come here. But a reg will be allocated for
		 * dest. We use this. This _really_ shouldn't happen if
		 * the size isn't long. */
		if (eaid->size != sz_long)
		    printf("_Really_ shouldn't happen (Dreg case)\n");
		map->x86_cache_reg[eaid->data_reg] = eaid->reg;
		map->x86_cr_type[eaid->data_reg] = 1;
		map->x86_const_offset[eaid->data_reg] = eaid->data_const_off;
		map->dreg_map[eaid->reg] = eaid->data_reg;
		map->x86_verified[eaid->data_reg] = 0;
		goto have_cache_reg_d;
	    } else {
		if (eais->size == sz_long) {
		    /* Make this the new register cache reg */
		    remove_x86r_from_cache(map, eais->data_reg, 0);
		    map->x86_cache_reg[eais->data_reg] = eaid->reg;
		    map->x86_cr_type[eais->data_reg] = 1;
		    map->x86_const_offset[eais->data_reg] = eais->data_const_off;
		    map->dreg_map[eaid->reg] = eais->data_reg;
		    map->x86_verified[eais->data_reg] = 0;
		} else {
		    /* Move from reg to regs.regs */
		    compile_force_byteorder(map, eais->data_reg, BO_NORMAL, 1);
		    compile_offset_reg (map, eais->data_reg, eais->data_const_off);
		    if (eaid->size == sz_word) assemble(0x66);
		    assemble(0x88 + szflag); assemble(0x05 + 8*eais->data_reg);
		    assemble_long(regs.regs + eaid->reg);
		}
	    }
	} else {
	    int destr;

	    have_cache_reg_d:

	    destr = map->dreg_map[eaid->reg];
	    if (eaid->size != sz_long)
		compile_force_byteorder(map, destr, BO_NORMAL, 1);

	    if (eais->data_reg == -2) {
		/* Move immediate to reg */
		if (eaid->size == sz_word) assemble(0x66);
		assemble(0xC6 + szflag); assemble(0xC0 + destr);
		switch (eaid->size) {
		 case sz_byte: assemble(eais->data_const_off); break;
		 case sz_word: assemble_uword(eais->data_const_off); break;
		 case sz_long: assemble_ulong(eais->data_const_off); break;
		}
		/* normal byteorder comes either from force above or from long
		 * const move */
		map->x86_byteorder[destr] = BO_NORMAL;
	    } else if (eais->data_reg == -1) {
		if (eais->mode == Dreg) {
		    compile_move_reg_from_mem_regoffs(destr, -2, (uae_u32)(regs.regs + eais->reg),
						      eais->size);
		    map->x86_byteorder[destr] = BO_NORMAL;
		} else if (eais->mode == Areg) {
		    compile_move_reg_from_mem_regoffs(destr, -2, (uae_u32)(regs.regs + 8 + eais->reg),
						      eais->size);
		    map->x86_byteorder[destr] = BO_NORMAL;
		} else {
		    /* Move mem to reg */
		    compile_force_byteorder(map, eais->address_reg, BO_NORMAL, 0);
		    compile_move_reg_from_mem_regoffs(destr, eais->address_reg,
						      (uae_u32)(eais->addr_const_off + address_space),
						      eais->size);

		    switch (eais->size) {
		     case sz_byte: map->x86_byteorder[destr] = BO_NORMAL; break;
		     case sz_word: map->x86_byteorder[destr] = BO_SWAPPED_WORD; break;
		     case sz_long: map->x86_byteorder[destr] = BO_SWAPPED_LONG; break;
		    }
		}
	    } else {
		if (eais->size == sz_long) {
		    /* Make this the new register cache reg */
		    remove_x86r_from_cache(map, eais->data_reg, 0);
		    remove_x86r_from_cache(map, destr, 0);
		    map->x86_cache_reg[eais->data_reg] = eaid->reg;
		    map->x86_cr_type[eais->data_reg] = 1;
		    map->x86_const_offset[eais->data_reg] = eais->data_const_off;
		    map->dreg_map[eaid->reg] = eais->data_reg;
		    map->x86_verified[eais->data_reg] = 0;
		} else {
		    /* Move from reg to reg */
		    compile_force_byteorder(map, eais->data_reg, BO_NORMAL, 1);
		    compile_offset_reg (map, eais->data_reg, eais->data_const_off);
		    if (eaid->size == sz_word) assemble(0x66);
		    assemble(0x88 + szflag); assemble(0xC0 + destr + 8*eais->data_reg);
		}
	    }
	}

	if (map->dreg_map[eaid->reg] >= 0)
	    map->x86_dirty[map->dreg_map[eaid->reg]] = 1;
	return;
    } else if (eaid->mode == Areg) {
	if (eaid->size != sz_long)
	    printf("Areg put != long\n");

	/* Is the reg to move from already the register cache reg for the
	 * destination? */
	if (eais->data_reg >= 0 && eais->data_reg == map->areg_map[eaid->reg]) {
	    map->x86_dirty[eais->data_reg] = 1; map->x86_verified[eais->data_reg] = 0;
	    map->x86_const_offset[eais->data_reg] = eais->data_const_off;
	    return;
	}
	/* Is the destination register in its home location? */
	if (map->areg_map[eaid->reg] < 0) {
	    if (eais->data_reg == -2) {
		/* Move immediate to regs.regs */
		assemble(0xC7); assemble(0x05); assemble_long(regs.regs + 8 + eaid->reg);
		assemble_ulong(eais->data_const_off);
	    } else if (eais->data_reg == -1) {
#if 0 /* see above... */
		printf("Shouldn't happen (mem-mem-move)\n");
#endif
		map->x86_cache_reg[eaid->data_reg] = eaid->reg;
		map->x86_cr_type[eaid->data_reg] = 0;
		map->x86_const_offset[eaid->data_reg] = eaid->data_const_off;
		map->areg_map[eaid->reg] = eaid->data_reg;
		map->x86_verified[eaid->data_reg] = 0;
		goto have_cache_reg_a;
	    } else {
		/* Make this the new register cache reg */
		remove_x86r_from_cache(map, eais->data_reg, 0);
		map->x86_cache_reg[eais->data_reg] = eaid->reg;
		map->x86_cr_type[eais->data_reg] = 0;
		map->x86_const_offset[eais->data_reg] = eais->data_const_off;
		map->areg_map[eaid->reg] = eais->data_reg;
		map->x86_verified[eais->data_reg] = 0;
	    }
	} else {
	    int destr;

	    have_cache_reg_a:

	    destr = map->areg_map[eaid->reg];
	    if (eaid->size != sz_long)
		compile_force_byteorder(map, destr, BO_NORMAL, 1);

	    if (eais->data_reg == -2) {
		/* Move immediate to reg */
		assemble(0xC7); assemble(0xC0 + destr);
		assemble_ulong(eais->data_const_off);

		/* normal byteorder comes either from force above or from long
		 * const move */
		map->x86_byteorder[destr] = BO_NORMAL;
	    } else if (eais->data_reg == -1) {
		if (eais->mode == Dreg) {
		    compile_move_reg_from_mem_regoffs(destr, -2, (uae_u32)(regs.regs + eais->reg),
						      eais->size);
		    map->x86_byteorder[destr] = BO_NORMAL;
		} else if (eais->mode == Areg) {
		    compile_move_reg_from_mem_regoffs(destr, -2, (uae_u32)(regs.regs + 8 + eais->reg),
						      eais->size);
		    map->x86_byteorder[destr] = BO_NORMAL;
		} else {
		    /* Move mem to reg */
		    compile_force_byteorder(map, eais->address_reg, BO_NORMAL, 0);
		    compile_move_reg_from_mem_regoffs(destr, eais->address_reg,
						      (uae_u32)(eais->addr_const_off + address_space),
						      eais->size);

		    map->x86_byteorder[destr] = BO_SWAPPED_LONG;
		}
	    } else {
		/* Make this the new register cache reg */
		remove_x86r_from_cache(map, eais->data_reg, 0);
		remove_x86r_from_cache(map, destr, 0);
		map->x86_cache_reg[eais->data_reg] = eaid->reg;
		map->x86_cr_type[eais->data_reg] = 0;
		map->x86_const_offset[eais->data_reg] = eais->data_const_off;
		map->areg_map[eaid->reg] = eais->data_reg;
		map->x86_verified[eais->data_reg] = 0;
	    }
	}

	if (map->areg_map[eaid->reg] >= 0)
	    map->x86_dirty[map->areg_map[eaid->reg]] = 1;
	return;
    }

    if (eais->data_reg == -1)
	printf("Storing to mem, but not from reg\n");
    /* Correct the byteorder */
    if (eais->data_reg != -2) {
	compile_offset_reg(map, eais->data_reg, eais->data_const_off);

	switch (eaid->size) {
	 case sz_byte: compile_force_byteorder(map, eais->data_reg, BO_NORMAL, 1); break;
	 case sz_word: compile_force_byteorder(map, eais->data_reg, BO_SWAPPED_WORD, 1); break;
	 case sz_long: compile_force_byteorder(map, eais->data_reg, BO_SWAPPED_LONG, 1); break;
	}
	compile_force_byteorder(map, eaid->address_reg, BO_NORMAL, 0);
	compile_move_reg_to_mem_regoffs(eaid->address_reg,
					(uae_u32)(eaid->addr_const_off + address_space),
					eais->data_reg, eaid->size);
    } else {
	switch (eaid->size) {
	 case sz_long:
	    eais->data_const_off = (((eais->data_const_off & 0xFF000000) >> 24)
				 | ((eais->data_const_off & 0xFF0000) >> 8)
				 | ((eais->data_const_off & 0xFF00) << 8)
				 | ((eais->data_const_off & 0xFF) << 24));
	    break;
	 case sz_word:
	    eais->data_const_off = (((eais->data_const_off & 0xFF00) >> 8)
				 | ((eais->data_const_off & 0xFF) << 8));
	    break;
	}
	compile_force_byteorder(map, eaid->address_reg, BO_NORMAL, 0);
	/* generate code to move valueoffset,eaoffset(eareg) */
	switch(eaid->size) {
	 case sz_byte: assemble(0xC6); break;
	 case sz_word: assemble(0x66); /* fall through */
	 case sz_long: assemble(0xC7); break;
	}
	if (eaid->address_reg == -2) { /* absolute or PC-relative */
	    assemble(0x05);
	    assemble_long(eaid->addr_const_off + address_space);
	} else {
	    assemble(0x80 + eaid->address_reg);
	    assemble_long(eaid->addr_const_off + address_space);
	}
	switch(eaid->size) {
	 case sz_byte: assemble(eais->data_const_off); break;
	 case sz_word: assemble_uword(eais->data_const_off); break;
	 case sz_long: assemble_ulong(eais->data_const_off); break;
	}
    }
}

#define CE_STACK_SIZE 1000

static struct {
    struct register_mapping map;
    char *jmpoffs;
    uae_u32 address;
    int noflush:1;
} compile_exit_stack[CE_STACK_SIZE];

static int cesp;

static struct register_mapping current_exit_regmap;

static void generate_exit(struct register_mapping *map, int address)
{
    int i;

    if (map != NULL)
	sync_reg_cache (map, 1);
    assemble(0xB8); /* movl $new_pc,%eax */
    assemble_ulong(address);
    assemble(0xC3); /* RET */
}

static void copy_map_with_undo(struct register_mapping *dst,
			       struct register_mapping *src,
			       struct pid_undo *pud)
{
    int i;
    *dst = *src;
    for (i = 0; i < pud->used; i++) {
	int m68kr = pud->m68kr[i];
	int x86r = pud->x86r[i];
	int old_cr = dst->areg_map[m68kr];
	if (old_cr != -1) {
	    dst->x86_cache_reg[old_cr] = -1;
	}
	dst->x86_cache_reg[x86r] = m68kr;
	dst->areg_map[m68kr] = x86r;
	dst->x86_cr_type[x86r] = 0;
	dst->x86_const_offset[x86r] = pud->offs[i];
	dst->x86_dirty[x86r] = pud->dirty[i];
    }
}

static void unlock_pud(struct register_mapping *map, struct pid_undo *pud)
{
    int i;
    for (i = 0; i < pud->used; i++) {
	compile_unlock_reg(map, pud->x86r[i]);
    }
}

static int exits_necessary;

static void generate_possible_exit(struct register_mapping *map,
				   struct ea_info *eai, int iip,
				   struct pid_undo *pud)
{
    struct register_mapping exit_regmap;

    if (!exits_necessary) {
	unlock_pud(map, pud);
	return;
    }

    compile_force_byteorder(map, eai->address_reg, BO_NORMAL, 0);
    switch (eai->address_reg) {
     case -1:
	/* EA doesn't refer to memory */
	break;
     case -2:
	/* Only a constant offset */
	eai->addr_const_off &= (1<<24)-1;
	if (!good_address_map[eai->addr_const_off]) {
	    copy_map_with_undo(&exit_regmap, map, pud);
	    generate_exit(&exit_regmap, insn_info[iip].address);
	}
	break;
     default:
	if (map->x86_verified[eai->address_reg])
	    break;
	map->x86_verified[eai->address_reg] = 1;
	if (cesp == CE_STACK_SIZE) {
	    copy_map_with_undo(&exit_regmap, map, pud);
	    generate_exit(&exit_regmap, insn_info[iip].address);
	    break;
	}
	copy_map_with_undo(&compile_exit_stack[cesp].map, map, pud);
	compile_exit_stack[cesp].address = insn_info[iip].address;
	assemble(0x80); assemble(0xB8 + eai->address_reg); /* cmpb $0, good_address_map(x86r) */
	assemble_long(good_address_map + eai->addr_const_off);
	assemble(0);
	assemble(0x0F); assemble(0x84); /* JE finish */
	compile_exit_stack[cesp].jmpoffs = compile_here();
	compile_exit_stack[cesp].noflush = 0;
	assemble_ulong(0);
	cesp++;
	break;
    }
    unlock_pud(map, pud);
}

static void finish_exits(void)
{
    int i;
    for (i = 0; i < cesp; i++) {
	char *exitpoint = compile_here();
	char *nextpoint;

	if (compile_exit_stack[i].noflush)
	    generate_exit(NULL, compile_exit_stack[i].address);
	else
	    generate_exit(&compile_exit_stack[i].map, compile_exit_stack[i].address);
	nextpoint = compile_here();
	compile_org(compile_exit_stack[i].jmpoffs);
	assemble_ulong(exitpoint - (compile_exit_stack[i].jmpoffs + 4));
	compile_org(nextpoint);
    }
}

static void finish_condjumps(int lastiip)
{
    int iip;
    char *lastptr = compile_here();
    for (iip = 0; iip < lastiip; iip++) {
	char *fillin = insn_info[iip].compiled_fillin;
	if (fillin != NULL) {
	    compile_org(insn_info[iip].compiled_fillin);
	    assemble_ulong(insn_info[insn_info[iip].jumps_to].compiled_jumpaddr - (fillin + 4));
	}
    }
    compile_org(lastptr);
}

#define CC_X_FROM_86C 1
#define CC_C_FROM_86C 2
#define CC_Z_FROM_86Z 4
#define CC_V_FROM_86V 8
#define CC_N_FROM_86N 16
#define CC_TEST_REG   32
#define CC_Z_FROM_86C 64
#define CC_SAHF       128
#define CC_TEST_CONST 256
#define CC_AFTER_RO   512
#define CC_AFTER_ROX  1024

static unsigned int cc_status;
static int cc_reg;
static uae_u32 cc_offset;
static wordsizes cc_size;

static void compile_do_cc_test_reg(struct register_mapping *map)
{
    compile_force_byteorder(map, cc_reg, BO_NORMAL, 1);
    if (cc_offset != 0)
	printf("Pull my finger\n");
    if (cc_size == sz_word) /* test ccreg */
	assemble(0x66);
    if (cc_size == sz_byte)
	assemble(0x84);
    else
	assemble(0x85);
    assemble(0xC0 + 9*cc_reg);
}

static int compile_flush_cc_cache(struct register_mapping *map, int status,
				  int live_at_end, int user_follows,
				  int user_live_at_end, int user_ccval)
{
    int status_for_user = 0;

    if (user_follows) {
	int need_for_user = 0;
	int user_flagmask = cc_flagmask_68k(user_ccval);

	if (user_flagmask & CC68K_C)
	    need_for_user |= CC_C_FROM_86C;
	if (user_flagmask & CC68K_Z)
	    need_for_user |= CC_Z_FROM_86Z;
	if (user_flagmask & CC68K_N)
	    need_for_user |= CC_N_FROM_86N;
	if (user_flagmask & CC68K_V)
	    need_for_user |= CC_V_FROM_86V;

	/* Check whether we can satisfy the user's needs in a simple way. */
	if ((need_for_user & status) == need_for_user)
	    status_for_user = status;
	else if (user_flagmask == CC68K_Z && status == CC_Z_FROM_86C)
	    status_for_user = status;
	else if (status == CC_TEST_REG && (user_flagmask & (CC68K_C|CC68K_V|CC68K_Z|CC68K_N)) != 0) {
	    if (cc_reg == -2) {
		status_for_user = CC_TEST_CONST;
	    } else {
		compile_do_cc_test_reg(map);
		status_for_user = status = (CC_C_FROM_86C | CC_Z_FROM_86Z | CC_N_FROM_86N | CC_V_FROM_86V);
	    }
	} else if (status == CC_AFTER_RO) {
	    /* We fake some information here... */
	    if (user_flagmask == CC68K_C && (user_live_at_end & ~CC68K_C) == 0)
		status = status_for_user = CC_C_FROM_86C;
	    else if (((user_flagmask | user_live_at_end) & (CC68K_C|CC68K_V)) == 0) {
		status = CC_TEST_REG; user_live_at_end = CC68K_Z|CC68K_N|CC68K_V;
		status_for_user = (CC_C_FROM_86C | CC_Z_FROM_86Z | CC_N_FROM_86N | CC_V_FROM_86V);
	    } else
		status_for_user = CC_SAHF;
	} else if (status == CC_AFTER_ROX) {
	    if (user_flagmask == CC68K_C && (user_live_at_end & ~(CC68K_C|CC68K_X)) == 0)
		status = status_for_user = CC_C_FROM_86C;
	    else if (((user_flagmask | user_live_at_end) & (CC68K_C|CC68K_X|CC68K_V)) == 0) {
		status = CC_TEST_REG; user_live_at_end = CC68K_Z|CC68K_N|CC68K_V;
		status_for_user = (CC_C_FROM_86C | CC_Z_FROM_86Z | CC_N_FROM_86N | CC_V_FROM_86V);
	    } else
		status_for_user = CC_SAHF;
	} else if (need_for_user != 0) {
	    /* No way to handle it easily */
	    status_for_user = CC_SAHF;
	}
	if (status_for_user != CC_SAHF)
	    live_at_end = user_live_at_end;
    }

    /*
     * Now store the flags which are live at the end of this insn and set by
     * us into their home locations
     */
    if (status == CC_TEST_REG) {
	if ((live_at_end & (CC68K_C|CC68K_V|CC68K_Z|CC68K_N)) == 0)
	    goto all_ok;

	if (cc_reg == -2) {
	    uae_u8 f = 0;
	    if (cc_size == sz_byte) {
		f |= (cc_offset & 0x80) ? 0x80 : 0;
		f |= (cc_offset & 0xFF) == 0 ? 0x40 : 0;
	    } else if (cc_size == sz_byte) {
		f |= (cc_offset & 0x8000) ? 0x80 : 0;
		f |= (cc_offset & 0xFFFF) == 0 ? 0x40 : 0;
	    } else {
		f |= (cc_offset & 0x80000000) ? 0x80 : 0;
		f |= (cc_offset & 0xFFFFFFFF) == 0 ? 0x40 : 0;
	    }
	    assemble(0xC7); assemble(0x05);
	    assemble_long((char*)&regflags);
	    assemble_uword(f);
	} else {
	    int tmpr = get_free_x86_register(map, ALL_X86_REGS);
	    compile_do_cc_test_reg(map);

	    /* pushfl; popl tmpr; movl tempr, regflags */
	    assemble(0x9C); assemble(0x58+tmpr);
	    compile_move_reg_to_mem_regoffs(-2, (uae_u32)&regflags, tmpr, sz_long);
	}
    } else if (status == CC_Z_FROM_86C) {
	if ((live_at_end & CC68K_Z) != 0) {
	    int tmpr = get_typed_x86_register(map, DATA_X86_REGS);
	    assemble(0x9C);
	    /* setnc tmpr; shl $6, tmpr; andb $~0x40, regflags; orb tmpr, regflags */
	    assemble(0x0F); assemble(0x93); assemble(0xC0 + tmpr);
	    assemble(0xC0); assemble(4*8 + 0xC0 + tmpr); assemble(6);
	    assemble(0x80); assemble(0x05+0x20); assemble_long(&regflags); assemble((uae_u8)~0x40);
	    assemble(0x08); assemble(0x05+ tmpr*8); assemble_long(&regflags);
	    assemble(0x9D);
	}
    } else if (status == CC_AFTER_RO || status == CC_AFTER_ROX) {
	int tmpr = get_typed_x86_register(map, DATA_X86_REGS);
	assemble(0x9C);
	compile_do_cc_test_reg(map);
	/* pushfl; popl tmpr; andl $0xff,tmpr (mask out V flag which is cleared after rotates) */
	assemble(0x9C); assemble(0x58 + tmpr);
	assemble(0x81); assemble(0xC0 + tmpr + 8*4); assemble_ulong(0xFF);
	assemble(0x9D);
	/* adc $0, tmpr */
	assemble(0x80); assemble(0xC0 + tmpr + 8*2); assemble(0);
	compile_move_reg_to_mem_regoffs(-2, (uae_u32)&regflags, tmpr, sz_long);
	if (status == CC_AFTER_ROX)
	    compile_move_reg_to_mem_regoffs(-2, 4 + (uae_u32)&regflags, tmpr, sz_long);
    } else if (status != 0) {
	assert((status & CC_TEST_REG) == 0);
	assert (status == (CC_C_FROM_86C | CC_Z_FROM_86Z | CC_N_FROM_86N | CC_X_FROM_86C | CC_V_FROM_86V)
		|| status == (CC_C_FROM_86C | CC_Z_FROM_86Z | CC_N_FROM_86N | CC_V_FROM_86V)
		|| status == CC_C_FROM_86C);

	if ((status & CC_X_FROM_86C) == 0)
	    live_at_end &= ~CC68K_X;

	if (status == CC_C_FROM_86C && (live_at_end & CC68K_C) != 0)
	    fprintf(stderr, "Shouldn't be needing C here!\n");
	else if (live_at_end) {
	    if ((live_at_end & CC68K_X) == 0)
		status &= ~CC_X_FROM_86C;

	    if (live_at_end) {
		if ((status & CC_X_FROM_86C) != 0 && live_at_end == CC68K_X) {
		    /* SETC regflags + 4 */
		    assemble(0x0F); assemble(0x92);
		    assemble(0x05); assemble_long(4 + (uae_u32)&regflags);
		} else {
		    int tmpr = get_free_x86_register(map, ALL_X86_REGS);
		    /* pushfl; popl tmpr; movl tempr, regflags */
		    assemble(0x9C); assemble(0x58+tmpr);
		    compile_move_reg_to_mem_regoffs(-2, (uae_u32)&regflags, tmpr, sz_long);

		    if (status & CC_X_FROM_86C) {
			compile_move_reg_to_mem_regoffs(-2, 4 + (uae_u32)&regflags, tmpr, sz_word);
		    }
		}
	    }
	}
    }

    all_ok:
    return status_for_user;
}

static char *compile_condbranch(struct register_mapping *map, int iip,
				int new_cc_status)
{
    int cc = insn_info[iip].dp->cc;
    int flagsused = cc_flagmask_68k(cc);
    int flagsneeded = 0;
    char *undo_pointer = compile_here();

    if (flagsused & CC68K_C)
	flagsneeded |= CC_C_FROM_86C;
    if (flagsused & CC68K_Z)
	flagsneeded |= CC_Z_FROM_86Z;
    if (flagsused & CC68K_N)
	flagsneeded |= CC_N_FROM_86N;
    if (flagsused & CC68K_V)
	flagsneeded |= CC_V_FROM_86V;

    if (flagsneeded == 0)
	/* Fine */;
    else if (new_cc_status == CC_SAHF) {
	int tmpr = get_free_x86_register(map, ALL_X86_REGS);
	compile_move_reg_from_mem_regoffs(tmpr, -2, (uae_u32)&regflags, sz_long);
	assemble(0x66); assemble(0x50+tmpr); assemble(0x66); assemble(0x9D);
	new_cc_status = CC_C_FROM_86C|CC_Z_FROM_86Z|CC_N_FROM_86N|CC_V_FROM_86V;
    } else if (new_cc_status == CC_TEST_CONST) {
	int n,z;
	switch(cc_size) {
	 case sz_byte: n = ((uae_s8)cc_offset) < 0; z = ((uae_s8)cc_offset) == 0; break;
	 case sz_word: n = ((uae_s16)cc_offset) < 0; z = ((uae_s16)cc_offset) == 0; break;
	 case sz_long: n = ((uae_s32)cc_offset) < 0; z = ((uae_s32)cc_offset) == 0; break;
	}
#define Bcc_TRUE 0
#define Bcc_FALSE 1
	flagsneeded = 0;
	new_cc_status = 0;
	switch (cc) {
	 case 2: cc = !z ? Bcc_TRUE : Bcc_FALSE; break; /* !CFLG && !ZFLG */
	 case 3: cc = z ? Bcc_TRUE : Bcc_FALSE; break; /* CFLG || ZFLG */
	 case 4: cc = Bcc_TRUE; break; /* !CFLG */
	 case 5: cc = Bcc_FALSE; break; /* CFLG */
	 case 6: cc = !z ? Bcc_TRUE : Bcc_FALSE; break; /* !ZFLG */
	 case 7: cc = z ? Bcc_TRUE : Bcc_FALSE; break; /* ZFLG */
	 case 8: cc = Bcc_TRUE; break; /* !VFLG */
	 case 9: cc = Bcc_FALSE; break; /* VFLG */
	 case 10:cc = !n ? Bcc_TRUE : Bcc_FALSE; break; /* !NFLG */
	 case 11:cc = n ? Bcc_TRUE : Bcc_FALSE; break; /* NFLG */
	 case 12:cc = !n ? Bcc_TRUE : Bcc_FALSE; break; /* NFLG == VFLG */
	 case 13:cc = n ? Bcc_TRUE : Bcc_FALSE; break; /* NFLG != VFLG */
	 case 14:cc = !n && !z ? Bcc_TRUE : Bcc_FALSE; break; /* !ZFLG && (NFLG == VFLG) */
	 case 15:cc = n || z ? Bcc_TRUE : Bcc_FALSE; break; /* ZFLG || (NFLG != VFLG) */
	}
    } else if (new_cc_status == CC_Z_FROM_86C) {
	if (cc == 6 || cc == 7) {
	    cc = (cc - 2) ^ 1;
	    /* Fake... */
	    flagsneeded = new_cc_status = CC_C_FROM_86C;
	} else if (cc != 0 && cc != 1)
	    printf("Groan!\n");
    }

    if (cc == 1)
	return NULL;

    if ((flagsneeded & new_cc_status) == flagsneeded) {
	char *result;
	/* We can generate a simple branch */
	if (cc == 0)
	    assemble(0xE9);
	else
	    assemble(0x0F);
	switch(cc) {
	 case 2: assemble(0x87); break;          /* HI */
	 case 3: assemble(0x86); break;          /* LS */
	 case 4: assemble(0x83); break;          /* CC */
	 case 5: assemble(0x82); break;          /* CS */
	 case 6: assemble(0x85); break;          /* NE */
	 case 7: assemble(0x84); break;          /* EQ */
	 case 8: assemble(0x81); break;          /* VC */
	 case 9: assemble(0x80); break;          /* VS */
	 case 10:assemble(0x89); break;          /* PL */
	 case 11:assemble(0x88); break;          /* MI */
	 case 12:assemble(0x8D); break;          /* GE */
	 case 13:assemble(0x8C); break;          /* LT */
	 case 14:assemble(0x8F); break;          /* GT */
	 case 15:assemble(0x8E); break;          /* LE */
	}
	result = compile_here();
	assemble_ulong(0);
	return result;
    }
    printf("Uhhuh.\n");
    return NULL;
}

static void compile_handle_bcc(struct register_mapping *map, int iip,
			       int new_cc_status)
{
    insn_info[iip].compiled_fillin = compile_condbranch(map, iip, new_cc_status);
}

static void compile_handle_dbcc(struct register_mapping *map, int iip,
				int new_cc_status, int dreg)
{
    char *fillin1 = compile_condbranch(map, iip, new_cc_status);

    /* subw $1,dreg; jnc ... */
    assemble(0x66); assemble(0x83); assemble(0x05 + 5*8);
    assemble_long(regs.regs + dreg);
    assemble(1);
    assemble(0x0F); assemble(0x83);
    insn_info[iip].compiled_fillin = compile_here();
    assemble_ulong(0);
    if (fillin1 != NULL) {
	char *oldp = compile_here();
	compile_org(fillin1);
	assemble_ulong(oldp - (fillin1+4));
	compile_org(oldp);
    }
}

static void handle_bit_insns(struct register_mapping *map, struct ea_info *eainf,
			     int eaino_s, int eaino_d, instrmnem optype)
{
    struct ea_info *srcea = eainf + eaino_s, *dstea = eainf + eaino_d;
    int code = (optype == i_BTST ? 0
		: optype == i_BSET ? 1
		: optype == i_BCLR ? 2
		: /* optype == i_BCHG */ 3);

    compile_fetchea(map, eainf, eaino_s, 5);
    compile_fetchea(map, eainf, eaino_d, 3);

    if (srcea->data_reg != -2) {
	compile_force_byteorder(map, srcea->data_reg, BO_NORMAL, 0);
	remove_x86r_from_cache(map, srcea->data_reg, 0);
	/* andl $something,srcreg */
	assemble(0x83); assemble(0xC0 + 4*8 + srcea->data_reg);
	if (dstea->size == sz_byte)
	    assemble(7);
	else
	    assemble(31);
    } else
	if (dstea->size == sz_byte)
	    srcea->data_const_off &= 7;
	else
	    srcea->data_const_off &= 31;

    /* Areg isn't possible here */
    if (dstea->mode == Dreg && dstea->data_reg == -1) {
	if (srcea->data_reg == -2) {
	    assemble(0x0F); assemble(0xBA); assemble(5 + 8*(4 + code));
	    assemble_long(regs.regs + dstea->reg);
	    assemble(srcea->data_const_off);
	} else {
	    assemble(0x0F); assemble(0xA3 + 8*code);
	    assemble(5 + srcea->data_reg*8);
	    assemble_long(regs.regs + dstea->reg);
	}
    } else if (dstea->data_reg >= 0) {
	compile_force_byteorder(map, dstea->data_reg, BO_NORMAL, 0);
	if (srcea->data_reg == -2) {
	    assemble(0x0F); assemble(0xBA); assemble(0xC0 + dstea->data_reg + 8*(4 + code));
	    assemble(srcea->data_const_off);
	} else {
	    assemble(0x0F); assemble(0xA3 + 8*code);
	    assemble(0xC0 + dstea->data_reg + srcea->data_reg*8);
	}
	if (optype != i_BTST)
	    map->x86_dirty[dstea->data_reg] = 1;
    } else {
	int addr_code = dstea->address_reg == -2 ? 5 : dstea->address_reg + 0x80;
	compile_force_byteorder(map, dstea->address_reg, BO_NORMAL, 0);
	/* We have an address in memory */
	if (dstea->data_reg != -1)
	    printf("Things don't look good in handle_bit_insns\n");
	if (srcea->data_reg == -2) {
	    assemble(0x0F); assemble(0xBA);
	    assemble(addr_code + 8*(4 + code));
	    assemble_long(address_space + dstea->addr_const_off);
	    assemble(srcea->data_const_off);
	} else {
	    assemble(0x0F); assemble(0xA3 + 8*code);
	    assemble(addr_code + srcea->data_reg*8);
	    assemble_long(address_space + dstea->addr_const_off);
	}

    }
    cc_status = CC_Z_FROM_86C;
}

static int do_rotshi = 1;

static void handle_rotshi(struct register_mapping *map, int iip,
			  uae_u8 *realpc, uaecptr current_addr, struct pid_undo *pud)
{
    struct ea_info eai;
    int amode_reg = insn_info[iip].dp->sreg;
    int amode_mode = insn_info[iip].dp->smode;
    wordsizes size = insn_info[iip].dp->size;
    int shiftcount;
    int mnemo = insn_info[iip].dp->mnemo;
    int shiftcode;
    int locked_eax_for_sahf = 0;

    switch(mnemo) {
     case i_ASLW: shiftcount = 1; mnemo = i_ASL; break;
     case i_ASRW: shiftcount = 1; mnemo = i_ASR; break;
     case i_LSLW: shiftcount = 1; mnemo = i_LSL; break;
     case i_LSRW: shiftcount = 1; mnemo = i_LSR; break;
     case i_ROLW: shiftcount = 1; mnemo = i_ROL; break;
     case i_RORW: shiftcount = 1; mnemo = i_ROR; break;
     case i_ROXLW:shiftcount = 1; mnemo = i_ROXL;break;
     case i_ROXRW:shiftcount = 1; mnemo = i_ROXR;break;
     default:
	amode_reg = insn_info[iip].dp->dreg;
	amode_mode = insn_info[iip].dp->dmode;
	shiftcount = insn_info[iip].dp->sreg;
	break;
    }
    if ((insn_info[iip].flags_live_at_end & CC68K_V) != 0) {
	if (mnemo == i_ASL) {
	    generate_exit(map, insn_info[iip].address);
	    printf("Can't handle this shift\n");
	    return;
	} else if (mnemo == i_ASR || mnemo == i_LSR || mnemo == i_LSL) {
	    remove_x86r_from_cache(map, r_EAX, 1);
	    locked_eax_for_sahf = 1;
	    lock_reg(map, r_EAX, 2);
	}

    }
    if (mnemo == i_ROXR || mnemo == i_ROXL) {
	remove_x86r_from_cache(map, r_EAX, 1);
	lock_reg(map, r_EAX, 2);
	compile_move_reg_from_mem_regoffs(r_AH, -2, 4 + (uae_u32)&regflags, sz_byte);
    }
    compile_prepare_undo(map, amode_mode, amode_reg, pud);
    compile_prepareea(map, amode_mode, amode_reg, size,
		      &realpc, current_addr,
		      &eai, 0, EA_LOAD|EA_STORE|EA_MODIFY, 1);

    generate_possible_exit(map, &eai, iip, pud);

    compile_fetchea(map, &eai, 0, 1);
    compile_force_byteorder(map, eai.data_reg, BO_NORMAL, 0);

    switch (mnemo) {
     case i_ASL:
	shiftcode = 4; cc_status = CC_C_FROM_86C | CC_Z_FROM_86Z | CC_N_FROM_86N | CC_V_FROM_86V | CC_X_FROM_86C;
	break;
     case i_LSL:
	shiftcode = 4; cc_status = CC_C_FROM_86C | CC_Z_FROM_86Z | CC_N_FROM_86N | CC_V_FROM_86V | CC_X_FROM_86C;
	break;
     case i_LSR:
	shiftcode = 5; cc_status = CC_C_FROM_86C | CC_Z_FROM_86Z | CC_N_FROM_86N | CC_V_FROM_86V | CC_X_FROM_86C;
	break;
     case i_ASR:
	shiftcode = 7; cc_status = CC_C_FROM_86C | CC_Z_FROM_86Z | CC_N_FROM_86N | CC_V_FROM_86V | CC_X_FROM_86C;
	break;
     case i_ROR:
	shiftcode = 1; cc_status = CC_AFTER_RO;
	break;
     case i_ROL:
	shiftcode = 0; cc_status = CC_AFTER_RO;
	break;
     case i_ROXL:
	shiftcode = 2; assemble(0x9E); /* SAHF */ cc_status = CC_AFTER_ROX; compile_unlock_reg(map, r_EAX);
	break;
     case i_ROXR:
	shiftcode = 3; assemble(0x9E); /* SAHF */ cc_status = CC_AFTER_ROX; compile_unlock_reg(map, r_EAX);
	break;
    }

    if (size == sz_word)
	assemble(0x66);
    assemble((shiftcount == 1 ? 0xD0 : 0xC0) + (size == sz_byte ? 0 : 1));
    assemble(shiftcode*8+0xC0 + eai.data_reg);
    if (shiftcount != 1) assemble(shiftcount);
    cc_offset = 0; cc_size = size; cc_reg = eai.data_reg;

    if (locked_eax_for_sahf) {
	/* The trick here is that the overflow flag isn't put into AH in SAHF */
	assemble(0x9E);
	assemble(0x0B); assemble(9*1 + 0xC0);
	assemble(0x9F);
	compile_unlock_reg(map, r_EAX);
    }
    compile_note_modify(map, &eai, 0);
}

static void handle_rotshi_variable(struct register_mapping *map, int iip,
				   uae_u8 *realpc, uaecptr current_addr,
				   struct pid_undo *pud)
{
    struct ea_info eais, eaid;
    int mnemo = insn_info[iip].dp->mnemo;
    int shiftcode;
    char *tmp1, *tmp2;
    int locked_eax_for_sahf = 0;

    remove_x86r_from_cache(map, r_ECX, 1);
    lock_reg(map, r_ECX, 2);

    if ((insn_info[iip].flags_live_at_end & CC68K_V) != 0) {
	if (mnemo == i_ASL) {
	    generate_exit(map, insn_info[iip].address);
	    printf("Can't handle this shift (var)\n");
	    return;
	} else if (mnemo == i_ASR || mnemo == i_LSR || mnemo == i_LSL) {
	    remove_x86r_from_cache(map, r_EAX, 1);
	    locked_eax_for_sahf = 1;
	    lock_reg(map, r_EAX, 2);
	}

    }
    if (mnemo == i_ROXR || mnemo == i_ROXL) {
	remove_x86r_from_cache(map, r_EAX, 1);
	lock_reg(map, r_EAX, 2);
	compile_move_reg_from_mem_regoffs(r_AH, -2, 4 + (uae_u32)&regflags,
					  sz_byte);
    }
    /* Both src and dest are Dreg modes */
    compile_prepareea(map, insn_info[iip].dp->smode, insn_info[iip].dp->sreg,
		      sz_long, &realpc, current_addr,
		      &eais, 0, EA_LOAD, 1);
    compile_prepareea(map, insn_info[iip].dp->dmode, insn_info[iip].dp->dreg,
		      insn_info[iip].dp->size, &realpc, current_addr,
		      &eaid, 0, EA_LOAD|EA_STORE|EA_MODIFY, 1);

    compile_fetchea(map, &eais, 0, 1);
    compile_fetchea(map, &eaid, 1, 1);
    compile_force_byteorder(map, eais.data_reg, BO_NORMAL, 0);
    compile_force_byteorder(map, eaid.data_reg, BO_NORMAL, 0);
    compile_move_reg_reg(r_ECX, eais.data_reg, sz_long);
    /* Test against zero, and test bit 6. If 1 <= count <= 31, we can do the
     * operation, otherwise, we have to exit */
    assemble(0xF6); assemble(0xC0 + r_ECX); assemble(0x1F);
    assemble(0x74); assemble(9);
    assemble(0xF6); assemble(0xC0 + r_ECX); assemble(0x20);

    assemble(0x0F); assemble(0x85); tmp1 = compile_here(); assemble_ulong(0);
    generate_exit(map, insn_info[iip].address);
    tmp2 = compile_here(); compile_org (tmp1); assemble_ulong((tmp2-tmp1) + 4); compile_org(tmp2);

    switch (mnemo) {
     case i_ASL:
	shiftcode = 4; cc_status = CC_C_FROM_86C | CC_Z_FROM_86Z | CC_N_FROM_86N | CC_V_FROM_86V | CC_X_FROM_86C;
	break;
     case i_LSL:
	shiftcode = 4; cc_status = CC_C_FROM_86C | CC_Z_FROM_86Z | CC_N_FROM_86N | CC_V_FROM_86V | CC_X_FROM_86C;
	break;
     case i_LSR:
	shiftcode = 5; cc_status = CC_C_FROM_86C | CC_Z_FROM_86Z | CC_N_FROM_86N | CC_V_FROM_86V | CC_X_FROM_86C;
	break;
     case i_ASR:
	shiftcode = 7; cc_status = CC_C_FROM_86C | CC_Z_FROM_86Z | CC_N_FROM_86N | CC_V_FROM_86V | CC_X_FROM_86C;
	break;
     case i_ROR:
	shiftcode = 1; cc_status = CC_AFTER_RO;
	break;
     case i_ROL:
	shiftcode = 0; cc_status = CC_AFTER_RO;
	break;
     case i_ROXL:
	shiftcode = 2; assemble(0x9E); /* SAHF */ cc_status = CC_AFTER_ROX; compile_unlock_reg(map, r_EAX);
	break;
     case i_ROXR:
	shiftcode = 3; assemble(0x9E); /* SAHF */ cc_status = CC_AFTER_ROX; compile_unlock_reg(map, r_EAX);
	break;
    }

    if (insn_info[iip].dp->size == sz_word)
	assemble(0x66);
    assemble(0xD2 + (insn_info[iip].dp->size == sz_byte ? 0 : 1));
    assemble(shiftcode*8+0xC0 + eaid.data_reg);
    cc_offset = 0; cc_size = insn_info[iip].dp->size; cc_reg = eaid.data_reg;

    if (locked_eax_for_sahf) {
	/* The trick here is that the overflow flag isn't put into AH in SAHF */
	assemble(0x9E);
	assemble(0x0B); assemble(9*1 + 0xC0);
	assemble(0x9F);
	compile_unlock_reg(map, r_EAX);
    }
    compile_note_modify(map, &eaid, 0);
    compile_unlock_reg(map, r_ECX);
}

static uae_u32 testmask = 0xF80000, testval = 0xF80000;

static int m68k_compile_block(struct hash_block *hb)
{
    int movem_extra = 0;
    int last_iip = m68k_scan_block(hb, &movem_extra);
    struct register_mapping map;
    int i, iip, szflag;
    uae_u8 *realpc_start = NULL;
    struct bb_info *current_bb;
    int cc_status_for_bcc = CC_SAHF;
    struct insn_reg_needs reg_needs_init;

    cesp = 0;

    if (n_compiled > n_max_comp)
	return 1;
    else if (n_compiled++ == n_max_comp)
	printf("X\n");

    cc_status = 0; compile_failure = 0;

    /* Kickstart ROM address? */
    if ((hb->he_first->addr & 0xF80000) != 0xF80000
	&& 0 && !patched_syscalls)
	return 1;

    exits_necessary = ((hb->he_first->addr & 0xF80000) == 0xF80000 || !USER_PROGRAMS_BEHAVE);

    if (alloc_code (hb, last_iip + movem_extra) == NULL) {
	hb->allocfailed = 1;
	return 0;
    }
    compile_org(hb->compile_start);
    compile_last_addr = (char *)hb->compile_start + hb->alloclen;

    /* m68k_scan_block() will leave this all set up */
    current_bb = bb_stack;

    for (i = 0; i < 8; i++) {
	map.dreg_map[i] = map.areg_map[i] = -1;
	map.x86_dirty[i] = 0;
	map.x86_cache_reg[i] = -1;
	map.x86_cr_type[i] = 0;
	map.x86_const_offset[i] = 0;
	map.x86_verified[i] = 0;
	map.x86_byteorder[i] = BO_NORMAL;
    }

    reg_needs_init.checkpoint_no = 0;
    for (i = 0; i < 8; i++) {
	reg_needs_init.dreg_needed[i] = reg_needs_init.areg_needed[i] = -1;
	reg_needs_init.dreg_mask[i] = reg_needs_init.areg_mask[i] = ALL_X86_REGS;
    }

    for (iip = 0; iip < last_iip && !compile_failure; iip++) {
	uae_u8 *realpc;
	struct ea_info eainfo[8];
	uaecptr current_addr;
	struct pid_undo pub;
	struct insn_reg_needs this_reg_needs = reg_needs_init;

	/* Set up locks for a new insn. We don't bother to clear this
	 * properly after compiling one insn. */
	for (i = 0; i < 8; i++) {
	    map.x86_users[i] = i == r_ESP ? 1 : 0;
	    map.x86_locked[i] = i == r_ESP ? 2 : 0;
	}

	pub.used = 0;
	current_addr = insn_info[iip].address + 2;

	if (iip == current_bb->first_iip) {
	    sync_reg_cache(&map, 1);
	    if (!quiet_compile)
		printf("Compiling %08lx\n", current_bb->h->addr);

	    realpc_start = get_real_address(current_bb->h->addr);
	    current_bb->h->execute = (code_execfunc)compile_here();
	    current_bb->h->matchword = *(uae_u32 *)realpc_start;
	    cc_status_for_bcc = CC_SAHF;
	}

	realpc = realpc_start + (current_addr - current_bb->h->addr);

	insn_info[iip].compiled_jumpaddr = compile_here();
	insn_info[iip].compiled_fillin = NULL;

	if (insn_info[iip].jump_target) {
	    if (cesp == CE_STACK_SIZE) {
		generate_exit(NULL, insn_info[iip].address);
		compile_failure = 1;
	    } else {
		assemble(0xFE); assemble(0x05 + 8*1); assemble_long(&nr_bbs_to_run);
		assemble(0x0F); assemble(0x84); /* JE finish */
		compile_exit_stack[cesp].noflush = 1;
		compile_exit_stack[cesp].address = current_bb->h;
		compile_exit_stack[cesp].jmpoffs = compile_here();
		assemble_ulong(0);
		cesp++;
	    }
	}
	/*
	 * This will sort out all insns we can't compile, including
	 * jumps out of this block */
	if (insn_info[iip].stop_translation == 1) {
	    generate_exit(&map, insn_info[iip].address);
	    cc_status = 0;
	} else switch (insn_info[iip].dp->mnemo) {
	 case i_NOP:
	    cc_status = 0;
	    if (!quiet_compile)
		printf("Compiling a NOP\n");
	    break;

	 case i_RTS:
	    sync_reg_cache(&map, 1);
	    lock_reg(&map, r_ECX, 2);
	    lock_reg(&map, r_EBX, 2);
	    {
		char *tmp1, *tmp2, *tmp3;

		/* fetch (A7) */
		assemble(0x8B); assemble(0x5 + r_EBX*8); assemble_long(regs.regs + 15);
		assemble(0x8B); assemble(0x80 + 9*r_EBX); assemble_long(address_space);
		assemble(0x0F); /* bswapl x86r */
		assemble(0xC8 + r_EBX);
		/* fetch jsr_num */
		assemble(0x8B); assemble(0x5 + r_ECX*8); assemble_long(&jsr_num);
		assemble(0x09); assemble(0xC0 + 9*r_ECX);
		assemble(0x0F); assemble(0x84); tmp1 = compile_here(); assemble_ulong(0);
		assemble(0xFF); assemble(1*8 + 0xC0 + r_ECX);
		/* cmpl %ebx,disp32(,%ecx,4) */
		assemble(0x39); assemble(0x04 + 8*r_EBX); assemble(0x8d);
		assemble_long(jsr_rets);
		assemble(0x0F); assemble(0x85); tmp2 = compile_here(); assemble_ulong(0);
		/* movl disp32(,%ecx,4),%ebx */
		assemble(0x8B); assemble(0x04 + 8*r_EBX); assemble(0x8d);
		assemble_long(jsr_hash);
		/* movl execute(%ebx), %ebx */
		assemble(0x8B); assemble(0x040 + 9*r_EBX); assemble((int)&((struct hash_entry *)0)->execute);
		assemble(0x09); assemble(0xC0 + 9*r_EBX);
		assemble(0x0F); assemble(0x85); tmp3 = compile_here(); assemble_ulong(0);
		compile_org(tmp1); assemble_ulong(tmp3 - tmp1);
		compile_org(tmp2); assemble_ulong(tmp3 - tmp2);
		compile_org(tmp3 + 4);
		generate_exit(&map, insn_info[iip].address);
		tmp1 = compile_here();
		compile_org(tmp3); assemble_ulong((tmp1-tmp3)-4);
		compile_org(tmp1);
		assemble(0x89); assemble(0x5 + r_ECX*8); assemble_long(&jsr_num);
		assemble(0x83); assemble(0x05 + 5*8); assemble_long(regs.regs + 15); assemble(-4);
		/* Off we go */
		assemble(0xFF); assemble(4*8 + 0xC0 + r_EBX);
	    }
	    break;

	 case i_JMP:
	    sync_reg_cache(&map, 1);
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 0, EA_LOAD, 1);
	    {
		char *tmp1, *tmp2, *tmp3;

		struct hash_entry *tmph;
		if (eainfo[0].address_reg != -2 || (tmph = get_hash_for_func(eainfo[0].addr_const_off, 1)) == 0) {
		    if (eainfo[0].address_reg != -2 && !quiet_compile)
			printf("Can't compile indirect JMP\n");
		    generate_exit(&map, insn_info[iip].address);
		    break;
		}
		/* check whether the destination has compiled code */
		assemble(0x8B); assemble(r_EBX*8 + 0x05); assemble_long(&(tmph->execute));
		assemble(0x09); assemble(0xC0 + 9*r_EBX);
		assemble(0x0F); assemble(0x85); tmp1 = compile_here(); assemble_ulong(0);
		generate_exit(&map, insn_info[iip].address);
		tmp2 = compile_here(); compile_org(tmp1);
		assemble_ulong((tmp2 - tmp1) - 4);
		compile_org(tmp2);
		/* Off we go */
		assemble(0xFF); assemble(4*8 + 0xC0 + r_EBX);
	    }
	    cc_status = 0;
	    break;

	 case i_JSR:
	    sync_reg_cache(&map, 1);
	    lock_reg(&map, r_ECX, 2);
	    lock_reg(&map, r_EBX, 2);
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 0, EA_LOAD, 1);
	    {
		char *tmp1, *tmp2, *tmp3;

		struct hash_entry *tmph;
		if (eainfo[0].address_reg != -2 || (tmph = get_hash_for_func(eainfo[0].addr_const_off, 1)) == 0) {
		    if (eainfo[0].address_reg != -2 && !quiet_compile)
			printf("Can't compile indirect JSR\n");
		    generate_exit(&map, insn_info[iip].address);
		    break;
		}
		assert(iip + 1 < last_iip);
		assert(iip == current_bb->last_iip);
		/* check whether the destination has compiled code */
		assemble(0x8B); assemble(r_EBX*8 + 0x05); assemble_long(&(tmph->execute));
		assemble(0x09); assemble(0xC0 + 9*r_EBX);
		assemble(0x0F); assemble(0x84); tmp3 = compile_here(); assemble_ulong(0);
		/* check for stack overflow */
		assemble(0x8B); assemble(r_ECX*8 + 0x05); assemble_long(&jsr_num);
		assemble(0xF7); assemble(0xC0+r_ECX); assemble_ulong(MAX_JSRS);
		assemble(0x0F); assemble(0x84); tmp1 = compile_here(); assemble_ulong(0);
		generate_exit(&map, insn_info[iip].address);
		tmp2 = compile_here(); compile_org(tmp1); assemble_ulong((tmp2 - tmp1) - 4);
		compile_org(tmp3); assemble_ulong(tmp1-tmp3);
		compile_org(tmp2);
		/* movl $something,disp32(,%ecx,4) */
		assemble(0xC7); assemble(0x04); assemble(0x8d);
		assemble_long(jsr_rets); assemble_ulong(insn_info[iip+1].address);
		assemble(0xC7); assemble(0x04); assemble(0x8d);
		assemble_long(jsr_hash); assemble_long((current_bb + 1)->h);
		/* incl jsr_num */
		assemble(0xFF); assemble(0x05); assemble_long(&jsr_num);
		/* Put things on the 68k stack */
		assemble(0x83); assemble(0x05 + 5*8); assemble_long(regs.regs + 15); assemble(4);
		assemble(0x8B); assemble(r_ECX*8+ 0x05); assemble_long(regs.regs + 15);
		assemble(0xC7); assemble(0x80 + r_ECX); assemble_long(address_space);
		assemble_ulong_68k(insn_info[iip+1].address);
		/* Off we go */
		assemble(0xFF); assemble(4*8 + 0xC0 + r_EBX);
	    }
	    break;

	 case i_BSR:
	    sync_reg_cache(&map, 1);
	    lock_reg(&map, r_ECX, 2);
	    lock_reg(&map, r_EBX, 2);
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 0, EA_LOAD, 1);
	    {
		char *tmp1, *tmp2, *tmp3;
		uaecptr dest = insn_info[iip].address + 2 + (uae_s32)eainfo[0].data_const_off;
		struct hash_entry *tmph;
		if ((tmph = get_hash_for_func(dest, 1)) == 0) {
		    generate_exit(&map, insn_info[iip].address);
		    break;
		}
		assert(iip + 1 < last_iip);
		assert(iip == current_bb->last_iip);

		/* check whether the destination has compiled code */
		assemble(0x8B); assemble(r_EBX*8 + 0x05); assemble_long(&(tmph->execute));
		assemble(0x09); assemble(0xC0 + 9*r_EBX);
		assemble(0x0F); assemble(0x84); tmp3 = compile_here(); assemble_ulong(0);
		/* check for stack overflow */
		assemble(0x8B); assemble(r_ECX*8 + 0x05); assemble_long(&jsr_num);
		assemble(0xF7); assemble(0xC0+r_ECX); assemble_ulong(MAX_JSRS);
		assemble(0x0F); assemble(0x84); tmp1 = compile_here(); assemble_ulong(0);
		generate_exit(&map, insn_info[iip].address);
		tmp2 = compile_here(); compile_org(tmp1); assemble_ulong((tmp2 - tmp1) - 4);
		compile_org(tmp3); assemble_ulong(tmp1-tmp3);
		compile_org(tmp2);
		/* movl $something,disp32(,%ecx,4) */
		assemble(0xC7); assemble(0x04); assemble(0x8d);
		assemble_long(jsr_rets); assemble_ulong(insn_info[iip+1].address);
		assemble(0xC7); assemble(0x04); assemble(0x8d);
		assemble_long(jsr_hash); assemble_long((current_bb + 1)->h);
		/* incl jsr_num */
		assemble(0xFF); assemble(0x05); assemble_long(&jsr_num);
		/* Put things on the 68k stack */
		assemble(0x83); assemble(0x05 + 5*8); assemble_long(regs.regs + 15); assemble(4);
		assemble(0x8B); assemble(r_ECX*8+ 0x05); assemble_long(regs.regs + 15);
		assemble(0xC7); assemble(0x80 + r_ECX); assemble_long(address_space);
		assemble_ulong_68k(insn_info[iip+1].address);
		/* Off we go */
		assemble(0xFF); assemble(4*8 + 0xC0 + r_EBX);
	    }
	    break;

	 case i_Bcc:
	    sync_reg_cache(&map, 0);
	    compile_handle_bcc(&map, iip, cc_status_for_bcc);
	    cc_status = 0;
	    break;

	 case i_DBcc:
	    sync_reg_cache(&map, 0);
	    remove_x86r_from_cache(&map, map.dreg_map[insn_info[iip].dp->sreg], 1);
	    compile_handle_dbcc(&map, iip, cc_status_for_bcc,
				insn_info[iip].dp->sreg);
	    cc_status = 0;
	    break;
#if 0
	 case i_Scc:
	    compile_prepare_undo(&map, insn_info[iip].dp->smode, insn_info[iip].dp->sreg, &pub);
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 0, EA_STORE, 1);

	    generate_possible_exit(&map, eainfo, iip, &pub);
	    srcreg2 = get_;
	    compile_note_modify(&map, eainfo, 0);

	    cc_status = 0;
	    break;
#endif
	 case i_ADD:
	 case i_SUB:
	 case i_CMP:
	 case i_CMPM:
	    compile_prepare_undo(&map, insn_info[iip].dp->smode, insn_info[iip].dp->sreg, &pub);
	    compile_prepare_undo(&map, insn_info[iip].dp->dmode, insn_info[iip].dp->dreg, &pub);
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 0, EA_LOAD, 1);
	    compile_prepareea(&map, insn_info[iip].dp->dmode,
			      insn_info[iip].dp->dreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 1,
			      (insn_info[iip].dp->mnemo == i_ADD || insn_info[iip].dp->mnemo == i_SUB
			       ? EA_MODIFY | EA_LOAD | EA_STORE
			       : EA_LOAD | EA_STORE), 1);

	    generate_possible_exit(&map, eainfo, iip, &pub);
	    generate_possible_exit(&map, eainfo+1, iip, &pub);

	    compile_loadeas(&map, eainfo, 0, 1, binop_alternatives, 0, 1);

	    switch (insn_info[iip].dp->mnemo) {
	     case i_ADD: compile_eas(&map, eainfo, 0, 1, 0); break;
	     case i_SUB: compile_eas(&map, eainfo, 0, 1, 5); break;
	     case i_CMP: case i_CMPM: compile_eas(&map, eainfo, 0, 1, 7); break;
	    }

	    if (insn_info[iip].dp->mnemo != i_CMP && insn_info[iip].dp->mnemo != i_CMPM)
		compile_note_modify(&map, eainfo, 1);
	    switch (insn_info[iip].dp->mnemo) {
	     case i_ADD:
	     case i_SUB:
		cc_status = CC_X_FROM_86C | CC_Z_FROM_86Z | CC_C_FROM_86C | CC_V_FROM_86V | CC_N_FROM_86N;
		break;
	     case i_CMP:
	     case i_CMPM:
		cc_status = CC_Z_FROM_86Z | CC_C_FROM_86C | CC_V_FROM_86V | CC_N_FROM_86N;
		break;
	    }
	    break;

	 case i_ADDX:
	 case i_SUBX:
	    compile_prepare_undo(&map, insn_info[iip].dp->smode, insn_info[iip].dp->sreg, &pub);
	    compile_prepare_undo(&map, insn_info[iip].dp->dmode, insn_info[iip].dp->dreg, &pub);
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 0, EA_LOAD, 1);
	    compile_prepareea(&map, insn_info[iip].dp->dmode,
			      insn_info[iip].dp->dreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 1, EA_MODIFY | EA_LOAD | EA_STORE, 1);

	    generate_possible_exit(&map, eainfo, iip, &pub);
	    generate_possible_exit(&map, eainfo+1, iip, &pub);

	    compile_loadeas(&map, eainfo, 0, 1, binop_alternatives, 0, 1);

	    /* bt $0, regflags+4 ; get carry */
	    assemble(0x0F); assemble(0xBA); assemble(0x5+4*8);
	    assemble_ulong(4 + (uae_u32)&regflags); assemble(0);

	    switch (insn_info[iip].dp->mnemo) {
	     case i_ADDX: compile_eas(&map, eainfo, 0, 1, 2); break;
	     case i_SUBX: compile_eas(&map, eainfo, 0, 1, 3); break;
	    }
	    compile_note_modify(&map, eainfo, 1);

	    if (insn_info[iip].flags_live_at_end & CC68K_Z) {
		/* Darn. */
		int tmpr = get_free_x86_register(&map, ALL_X86_REGS);
		/* pushfl; popl tmpr */
		assemble(0x9C); assemble(0x58+tmpr);
		/* Magic! */
		/* andl tmpr, regflags; andl $~0x40,tmpr; orl tmpr, regflags */
		assemble(0x21); assemble(0x05 + 8*tmpr); assemble_long(&regflags);
		assemble(0x81); assemble(0xC0 + 8*4 + tmpr); assemble_ulong(~0x40);
		assemble(0x09); assemble(0x05 + 8*tmpr); assemble_long(&regflags);
		compile_move_reg_to_mem_regoffs(-2, 4 + (uae_u32)&regflags, tmpr, sz_long);
		cc_status = 0;
	    } else {
		/* Lies! */
		cc_status = CC_X_FROM_86C | CC_Z_FROM_86Z |CC_C_FROM_86C |CC_V_FROM_86V |CC_N_FROM_86N;
	    }
	    break;

	 case i_MULU:
	 case i_MULS:
	    compile_prepare_undo(&map, insn_info[iip].dp->smode, insn_info[iip].dp->sreg, &pub);
	    compile_prepare_undo(&map, insn_info[iip].dp->dmode, insn_info[iip].dp->dreg, &pub);
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 0, EA_LOAD, 1);
	    compile_prepareea(&map, insn_info[iip].dp->dmode,
			      insn_info[iip].dp->dreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 1, EA_MODIFY | EA_LOAD | EA_STORE, 1);

	    generate_possible_exit(&map, eainfo, iip, &pub);
	    generate_possible_exit(&map, eainfo+1, iip, &pub);

	    compile_loadeas(&map, eainfo, 0, 1, regonly_alternatives, 0, 1);

	    /* Extend the regs properly */
	    remove_x86r_from_cache(&map, eainfo[0].data_reg, 0);
	    switch (insn_info[iip].dp->mnemo) {
	     case i_MULU:
		assemble(0x81); assemble(0xC0+4*8 + eainfo[0].data_reg); assemble_ulong(0xFFFF);
		assemble(0x81); assemble(0xC0+4*8 + eainfo[1].data_reg); assemble_ulong(0xFFFF);
		break;
	     case i_MULS:
		assemble(0x0F); assemble(0xBF); assemble(0xC0 + 9*eainfo[0].data_reg);
		assemble(0x0F); assemble(0xBF); assemble(0xC0 + 9*eainfo[1].data_reg);
		break;
	    }
	    /* and multiply */
	    assemble(0x0F); assemble(0xAF); assemble(0xC0 + 8*eainfo[1].data_reg + eainfo[0].data_reg);
	    compile_note_modify(&map, eainfo, 1);
	    cc_status = CC_TEST_REG;
	    cc_reg = eainfo[1].data_reg;
	    cc_offset = 0;
	    cc_size = sz_long;
	    break;

	 case i_ADDA:
	 case i_SUBA:
	 case i_CMPA:
	    compile_prepare_undo(&map, insn_info[iip].dp->smode, insn_info[iip].dp->sreg, &pub);
	    compile_prepare_undo(&map, insn_info[iip].dp->dmode, insn_info[iip].dp->dreg, &pub);
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 0, EA_LOAD, 1);
	    compile_prepareea(&map, insn_info[iip].dp->dmode,
			      insn_info[iip].dp->dreg,
			      sz_long, &realpc, current_addr,
			      eainfo, 1,
			      (insn_info[iip].dp->mnemo == i_ADDA || insn_info[iip].dp->mnemo == i_SUBA
			       ? EA_MODIFY | EA_LOAD | EA_STORE
			       : EA_LOAD | EA_STORE),
			      1);

	    generate_possible_exit(&map, eainfo, iip, &pub);

	    compile_loadeas(&map, eainfo, 0, 1,
			    insn_info[iip].dp->size == sz_word ? binop_worda_alternatives : binop_alternatives,
			    0, 1);

	    if (insn_info[iip].dp->size == sz_word) {
		remove_x86r_from_cache(&map, eainfo[0].data_reg, 0);
		compile_extend_long(&map, eainfo[0].data_reg, sz_word);
	    }
	    eainfo[0].size = sz_long;

	    switch (insn_info[iip].dp->mnemo) {
	     case i_ADDA: compile_eas(&map, eainfo, 0, 1, 0); break;
	     case i_SUBA: compile_eas(&map, eainfo, 0, 1, 5); break;
	     case i_CMPA: compile_eas(&map, eainfo, 0, 1, 7); break;
	    }

	    if (insn_info[iip].dp->mnemo == i_CMPA) {
		cc_status = CC_Z_FROM_86Z |CC_C_FROM_86C |CC_V_FROM_86V |CC_N_FROM_86N;
	    } else {
		compile_note_modify(&map, eainfo, 1);
		cc_status = 0;
	    }
	    break;

	 case i_MOVE:
	    compile_prepare_undo(&map, insn_info[iip].dp->smode, insn_info[iip].dp->sreg, &pub);
	    compile_prepare_undo(&map, insn_info[iip].dp->dmode, insn_info[iip].dp->dreg, &pub);
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 0, EA_LOAD, 1);
	    compile_prepareea(&map, insn_info[iip].dp->dmode,
			      insn_info[iip].dp->dreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 1, EA_STORE, 1);

	    generate_possible_exit(&map, eainfo, iip, &pub);
	    generate_possible_exit(&map, eainfo + 1, iip, &pub);

	    compile_loadeas(&map, eainfo, 0, 1, binop_alternatives, 1, 0);
	    compile_storeea(&map, eainfo, 0, 1);

	    if (eainfo[0].data_reg == -2) {
		cc_status = CC_TEST_REG;
		cc_reg = -2;
		cc_offset = eainfo[0].data_const_off;
	    } else if (eainfo[0].data_reg == -1) {
		if (eainfo[1].data_reg == -1)
		    printf("Don't know where to get flags from\n");
		cc_status = CC_TEST_REG;
		cc_offset = 0;
		cc_reg = eainfo[1].data_reg;
	    } else {
		cc_status = CC_TEST_REG;
		cc_reg = eainfo[0].data_reg;
		cc_offset = 0;
	    }
	    cc_size = eainfo[0].size;

	    break;

	 case i_MOVEA:
	    compile_prepare_undo(&map, insn_info[iip].dp->smode, insn_info[iip].dp->sreg, &pub);
	    compile_prepare_undo(&map, insn_info[iip].dp->dmode, insn_info[iip].dp->dreg, &pub);
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 0, EA_LOAD, 1);
	    compile_prepareea(&map, insn_info[iip].dp->dmode,
			      insn_info[iip].dp->dreg,
			      sz_long, &realpc, current_addr,
			      eainfo, 1, EA_STORE, 1);

	    generate_possible_exit(&map, eainfo, iip, &pub);

	    compile_loadeas(&map, eainfo, 0, 1,
			    insn_info[iip].dp->size == sz_word ? binop_worda_alternatives : binop_alternatives,
			    0, 0);

	    if (insn_info[iip].dp->size == sz_word) {
		remove_x86r_from_cache(&map, eainfo[0].data_reg, 0);
		compile_extend_long(&map, eainfo[0].data_reg, sz_word);
	    }
	    eainfo[0].size = sz_long;

	    compile_storeea(&map, eainfo, 0, 1);

	    cc_status = 0;
	    break;

	 case i_EXG:
	    if (insn_info[iip].dp->smode != insn_info[iip].dp->dmode
		|| insn_info[iip].dp->sreg != insn_info[iip].dp->dreg)
	    {
		compile_prepareea(&map, insn_info[iip].dp->smode,
				  insn_info[iip].dp->sreg,
				  sz_long, &realpc, current_addr,
				  eainfo, 0, EA_LOAD|EA_STORE, 1);
		compile_prepareea(&map, insn_info[iip].dp->dmode,
				  insn_info[iip].dp->dreg,
				  sz_long, &realpc, current_addr,
				  eainfo, 1, EA_LOAD|EA_STORE, 1);

		compile_loadeas(&map, eainfo, 0, 1, regonly_alternatives, 0, 1);
		compile_storeea(&map, eainfo, 1, 0);
		compile_storeea(&map, eainfo, 0, 1);
	    }

	    cc_status = 0;
	    break;

	 case i_LINK:
	    compile_prepare_undo(&map, Apdi, 7, &pub);
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      sz_long, &realpc, current_addr,
			      eainfo, 0, EA_LOAD|EA_STORE, 1);
	    compile_prepareea(&map, insn_info[iip].dp->dmode,
			      insn_info[iip].dp->dreg,
			      sz_long, &realpc, current_addr,
			      eainfo, 1, EA_LOAD, 1);
	    compile_prepareea(&map, Apdi, 7, sz_long, &realpc, current_addr,
			      eainfo, 2, EA_STORE, 1);

	    generate_possible_exit(&map, eainfo+2, iip, &pub);

	    compile_fetchea(&map, eainfo, 0, 1);
	    /* we know this is a constant - no need to fetch it*/
	    /* compile_fetchea(&map, eainfo, 1); */
	    compile_storeea(&map, eainfo, 0, 2); /* An -> -(A7) */

	    compile_prepareea(&map, Areg, 7, sz_long, &realpc, current_addr,
			      eainfo, 3, EA_STORE, 1);
	    compile_fetchea(&map, eainfo, 3, 1);
	    compile_storeea(&map, eainfo, 3, 0); /* A7 -> An */

	    /* @@@ 020 */
	    compile_prepareea(&map, Areg, 7, sz_long, &realpc, current_addr,
			      eainfo, 4, EA_LOAD, 1);
	    compile_prepareea(&map, Areg, 7, sz_long, &realpc, current_addr,
			      eainfo, 5, EA_STORE, 1);
	    compile_fetchea(&map, eainfo, 4, 1);
	    eainfo[4].data_const_off += (uae_s16)eainfo[1].data_const_off;
	    compile_storeea(&map, eainfo, 4, 5); /* A7+off -> A7 */
	    cc_status = 0;
	    break;

	 case i_UNLK:
	    compile_prepareea(&map, Areg,
			      insn_info[iip].dp->sreg,
			      sz_long, &realpc, current_addr,
			      eainfo, 0, EA_LOAD, 1);
	    compile_prepareea(&map, Areg, 7, sz_long, &realpc, current_addr,
			      eainfo, 1, EA_STORE, 1);

	    generate_possible_exit(&map, eainfo + 0, iip, &pub);

	    compile_fetchea(&map, eainfo, 0, 1);
	    compile_storeea(&map, eainfo, 0, 1);

	    /* The Apdi could of course point to a non-memory area, but undos
	     * are difficult here, and anyway: which program does evil hacks
	     * with UNLK? */
	    compile_prepareea(&map, Aipi, 7, sz_long, &realpc, current_addr,
			      eainfo, 2, EA_LOAD, 1);
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      sz_long, &realpc, current_addr,
			      eainfo, 3, EA_STORE, 1);
	    compile_fetchea(&map, eainfo, 2, 1);
	    compile_storeea(&map, eainfo, 2, 3);

	    cc_status = 0;
	    break;

	 case i_OR:
	 case i_AND:
	 case i_EOR:
	    compile_prepare_undo(&map, insn_info[iip].dp->smode, insn_info[iip].dp->sreg, &pub);
	    compile_prepare_undo(&map, insn_info[iip].dp->dmode, insn_info[iip].dp->dreg, &pub);
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 0, EA_LOAD, 1);
	    compile_prepareea(&map, insn_info[iip].dp->dmode,
			      insn_info[iip].dp->dreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 1, EA_MODIFY | EA_LOAD | EA_STORE, 1);

	    generate_possible_exit(&map, eainfo, iip, &pub);
	    generate_possible_exit(&map, eainfo + 1, iip, &pub);

	    compile_loadeas(&map, eainfo, 0, 1, binop_alternatives, 0, 1);

	    switch (insn_info[iip].dp->mnemo) {
	     case i_AND: compile_eas(&map, eainfo, 0, 1, 4); break;
	     case i_EOR: compile_eas(&map, eainfo, 0, 1, 6); break;
	     case i_OR:  compile_eas(&map, eainfo, 0, 1, 1); break;
	    }

	    compile_note_modify(&map, eainfo, 1);
	    cc_status = CC_Z_FROM_86Z | CC_C_FROM_86C | CC_V_FROM_86V | CC_N_FROM_86N;
	    break;

	 case i_TST:
	    compile_prepare_undo(&map, insn_info[iip].dp->smode, insn_info[iip].dp->sreg, &pub);
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 0, EA_LOAD, 1);

	    generate_possible_exit(&map, eainfo, iip, &pub);

	    compile_fetchea(&map, eainfo, 0, 1);
	    cc_status = CC_TEST_REG;
	    cc_reg = eainfo[0].data_reg;
	    cc_offset = 0;
	    cc_size = eainfo[0].size;
	    break;

	 case i_CLR:
	    compile_prepare_undo(&map, insn_info[iip].dp->smode, insn_info[iip].dp->sreg, &pub);
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 0, EA_STORE, 1);
	    compile_prepareea(&map, immi, 0, sz_long, &realpc, current_addr,
			      eainfo, 1, EA_LOAD, 1);
	    generate_possible_exit(&map, eainfo + 0, iip, &pub);
	    compile_loadeas(&map, eainfo, 1, 0, binop_alternatives, 1, 0);
	    compile_storeea(&map, eainfo, 1, 0);

	    cc_status = CC_TEST_REG;
	    cc_reg = -2;
	    cc_offset = 0;
	    cc_size = eainfo[0].size;
	    break;

	 case i_EXT:
	    /* No exits, no undo - this is always a Dreg; fetchea will get it in a reg
	     * without offset */
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      insn_info[iip].dp->size == sz_long ? sz_word : sz_byte,
			      &realpc, current_addr,
			      eainfo, 0, EA_LOAD|EA_STORE, 1);
	    compile_fetchea(&map, eainfo, 0, 1);
	    compile_force_byteorder(&map, eainfo[0].data_reg, BO_NORMAL, 0);

	    if (insn_info[iip].dp->size == sz_word)
		assemble(0x66);
	    assemble(0x0F);
	    if (insn_info[iip].dp->size == sz_long)
		assemble(0xBF);
	    else
		assemble(0xBE);

	    assemble(0xC0 + 9*eainfo[0].data_reg);
	    map.x86_dirty[eainfo[0].data_reg] = 1;

	    cc_status = CC_TEST_REG;
	    cc_reg = eainfo[0].data_reg;
	    cc_offset = 0;
	    cc_size = eainfo[0].size;
	    break;

	 case i_NOT:
	 case i_NEG:
	    szflag = insn_info[iip].dp->size == sz_byte ? 0 : 1;

	    compile_prepare_undo(&map, insn_info[iip].dp->smode, insn_info[iip].dp->sreg, &pub);
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      insn_info[iip].dp->size,
			      &realpc, current_addr,
			      eainfo, 0, EA_LOAD|EA_STORE, 1);

	    generate_possible_exit(&map, eainfo, iip, &pub);

	    compile_fetchea(&map, eainfo, 0, 1);
	    compile_force_byteorder(&map, eainfo[0].data_reg, BO_NORMAL, 0);

	    if (insn_info[iip].dp->size == sz_word)
		assemble(0x66);
	    assemble(0xF6 + szflag);

	    assemble(0xC0 + eainfo[0].data_reg + 8*(insn_info[iip].dp->mnemo == i_NOT ? 2 : 3));
	    compile_note_modify(&map, eainfo, 0);

	    if (insn_info[iip].dp->mnemo == i_NEG)
		cc_status = CC_Z_FROM_86Z | CC_C_FROM_86C | CC_V_FROM_86V | CC_N_FROM_86N | CC_X_FROM_86C;
	    else {
		cc_status = CC_TEST_REG;
		cc_reg = eainfo[0].data_reg;
		cc_offset = 0;
		cc_size = eainfo[0].size;
	    }
	    break;

	 case i_SWAP:
	    /* No exits, no undo - this is always a Dreg; fetchea will get it in a reg
	     * without offset */
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg, sz_long,
			      &realpc, current_addr,
			      eainfo, 0, EA_LOAD|EA_STORE, 1);

	    compile_fetchea(&map, eainfo, 0, 1);
	    compile_force_byteorder(&map, eainfo[0].data_reg, BO_NORMAL, 0);

	    /* roll $16, srcreg */
	    assemble(0xC1); assemble(0xC0 + eainfo[0].data_reg); assemble(16);

	    /* @@@ un-shortcut */
	    map.x86_dirty[eainfo[0].data_reg] = 1;

	    cc_status = CC_TEST_REG;
	    cc_reg = eainfo[0].data_reg;
	    cc_offset = 0;
	    cc_size = eainfo[0].size;
	    break;

	 case i_LEA:
	    /* No exits necessary here: never touches memory */
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 0, 0, 1);
	    eainfo[0].data_reg = eainfo[0].address_reg;
	    eainfo[0].data_const_off = eainfo[0].addr_const_off;
	    eainfo[0].address_reg = -1;
	    compile_get_excl_lock(&map, eainfo + 0);
	    compile_prepareea(&map, insn_info[iip].dp->dmode,
			      insn_info[iip].dp->dreg,
			      sz_long, &realpc, current_addr,
			      eainfo, 1, EA_STORE, 1);
	    compile_storeea(&map, eainfo, 0, 1);
	    cc_status = 0;
	    break;

	 case i_PEA:
	    compile_prepare_undo(&map, Apdi, 7, &pub);
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 0, 0, 1);
	    eainfo[0].data_reg = eainfo[0].address_reg;
	    eainfo[0].data_const_off = eainfo[0].addr_const_off;
	    eainfo[0].address_reg = -1;
	    compile_get_excl_lock(&map, eainfo + 0);
	    compile_prepareea(&map, Apdi, 7, sz_long, &realpc, current_addr,
			      eainfo, 1, EA_STORE, 1);

	    generate_possible_exit(&map, eainfo+1, iip, &pub);
	    compile_storeea(&map, eainfo, 0, 1);

	    cc_status = 0;
	    break;

	 case i_MVMEL:
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      sz_word, &realpc, current_addr,
			      eainfo, 0, EA_LOAD, 1);
	    sync_reg_cache(&map, 0);
	    {
		/* Scratch 0 holds the registers while they are being moved
		 * from/to memory. Scratch 1 points at regs.d. Scratch 2
		 * points at the base addr in memory where to fetch data
		 * from.
		 */
		int scratch0, scratch1, scratch2;
		uae_u16 mask = eainfo[0].data_const_off;
		int bits = count_bits(mask);
		int size = insn_info[iip].dp->size == sz_long ? 4 : 2;
		int i;
		uae_u8 x86amode;
		uae_u32 current_offs = 0;

		compile_prepare_undo(&map, insn_info[iip].dp->dmode, insn_info[iip].dp->dreg, &pub);
		/* !!! Note current_addr + 2 here! */
		compile_prepareea(&map, insn_info[iip].dp->dmode,
				  insn_info[iip].dp->dreg,
				  insn_info[iip].dp->size, &realpc, current_addr + 2,
				  eainfo, 1, EA_LOAD, bits);

		generate_possible_exit(&map, eainfo + 1, iip, &pub);

		scratch0 = get_free_x86_register(&map, ADDRESS_X86_REGS);
		lock_reg(&map, scratch0, 2);
		scratch1 = get_free_x86_register(&map, ADDRESS_X86_REGS);
		lock_reg(&map, scratch1, 2);
		scratch2 = get_free_x86_register(&map, ADDRESS_X86_REGS);
		lock_reg(&map, scratch2, 2);
		compile_force_byteorder(&map, eainfo[1].address_reg, BO_NORMAL, 0);

		compile_lea_reg_with_offset(scratch1, -2, (uae_u32)regs.regs);
		compile_lea_reg_with_offset(scratch2, eainfo[1].address_reg,
					    (uae_u32)(address_space + eainfo[1].addr_const_off));

		for (i = 0; i < 16; i++) {
		    int r68k = i;
		    int *cache68k = i < 8 ? map.dreg_map : map.areg_map;
		    if (mask & 1
			&& (i < 8
			    || insn_info[iip].dp->dmode != Aipi
			    || (r68k & 7) != insn_info[iip].dp->dreg)) {
			int tmpr = cache68k[r68k & 7];

			if (tmpr != -1) {
			    cache68k[r68k & 7] = -1;
			    map.x86_cache_reg[tmpr] = -1;
			}
			compile_move_reg_from_mem_regoffs(scratch0, scratch2,
							  current_offs, insn_info[iip].dp->size);
			if (size == 2) {
			    assemble(0x66); /* rolw $8,scratch0 */
			    assemble(0xC1);
			    assemble(0xC0 + scratch0);
			    assemble(8);
			    assemble(0x0F); assemble(0xBF); /* extend */
			    assemble(0xC0 + 9*scratch0);
			} else {
			    assemble(0x0F); /* bswapl scratch0 */
			    assemble(0xC8 + scratch0);
			}
			compile_move_reg_to_mem_regoffs(scratch1, (char *)(regs.regs + r68k) - (char *)regs.regs,
							scratch0, sz_long);
		    }
		    if (mask & 1)
			current_offs += size;
		    mask >>= 1;
		}
	    }
	    cc_status = 0;
	    break;

	 case i_MVMLE:
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      sz_word, &realpc, current_addr,
			      eainfo, 0, EA_LOAD, 1);
	    sync_reg_cache(&map, 0);
	    {
		int scratch0,scratch1,scratch2;
		uae_u16 mask = eainfo[0].data_const_off;
		int bits = count_bits(mask);
		int size = insn_info[iip].dp->size == sz_long ? 4 : 2;
		int i;
		uae_u8 x86amode;
		uae_u32 current_offs = 0;
		int addrareg = -1;
		if (insn_info[iip].dp->dmode == Aind
		    || insn_info[iip].dp->dmode == Apdi
		    || insn_info[iip].dp->dmode == Aipi
		    || insn_info[iip].dp->dmode == Ad16
		    || insn_info[iip].dp->dmode == Ad8r)
		{
		    addrareg = get_and_lock_68k_reg(&map, insn_info[iip].dp->dreg, 0, ADDRESS_X86_REGS, 1, 2);
		    compile_force_byteorder(&map, addrareg, BO_NORMAL, 0);
		}
		if (insn_info[iip].dp->dmode == Apdi)
		    mask = bitswap(mask);
		/* !!! Note current_addr + 2 here! */
		compile_prepare_undo(&map, insn_info[iip].dp->dmode, insn_info[iip].dp->dreg, &pub);
		compile_prepareea(&map, insn_info[iip].dp->dmode,
				  insn_info[iip].dp->dreg,
				  insn_info[iip].dp->size, &realpc, current_addr + 2,
				  eainfo, 1, EA_STORE, bits);

		generate_possible_exit(&map, eainfo + 1, iip, &pub);

		scratch0 = get_free_x86_register(&map, ADDRESS_X86_REGS);
		lock_reg(&map, scratch0, 2);
		scratch1 = get_free_x86_register(&map, ADDRESS_X86_REGS);
		lock_reg(&map, scratch1, 2);
		scratch2 = get_free_x86_register(&map, ADDRESS_X86_REGS);
		lock_reg(&map, scratch2, 2);

		compile_force_byteorder(&map, eainfo[1].address_reg, BO_NORMAL, 0);

		compile_lea_reg_with_offset(scratch1, -2, (uae_u32)regs.regs);
		compile_lea_reg_with_offset(scratch2, eainfo[1].address_reg,
					    (uae_u32)(address_space + eainfo[1].addr_const_off));

		for (i = 0; i < 16; i++) {
		    int r68k = i;
		    if (mask & 1) {
			/* move from 68k reg */
			if (i < 8 || (i & 7) != insn_info[iip].dp->dreg || addrareg == -1) {
			    compile_move_reg_from_mem_regoffs(scratch0, scratch1, (char *)(regs.regs + r68k) - (char *)regs.regs,
							      sz_long);
			} else {
			    assemble(0x8B); assemble(0xC0 + 8*scratch0 + addrareg);
			}

			if (size == 2) {
			    assemble(0x66); /* rolw $8,scratch0 */
			    assemble(0xC1);
			    assemble(0xC0 + scratch0); assemble(8);
			} else {
			    assemble(0x0F); /* bswapl scratch0 */
			    assemble(0xC8 + scratch0);
			}
			compile_move_reg_to_mem_regoffs(scratch2, current_offs,
							scratch0, insn_info[iip].dp->size);
		    }
		    if (mask & 1)
			current_offs += size;
		    mask >>= 1;
		}
	    }
	    cc_status = 0;
	    break;
#if 1
	 case i_BTST:
	 case i_BSET:
	 case i_BCLR:
	 case i_BCHG:
	    compile_prepare_undo(&map, insn_info[iip].dp->smode, insn_info[iip].dp->sreg, &pub);
	    compile_prepare_undo(&map, insn_info[iip].dp->dmode, insn_info[iip].dp->dreg, &pub);
	    compile_prepareea(&map, insn_info[iip].dp->smode,
			      insn_info[iip].dp->sreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 0, EA_LOAD, 1);
	    compile_prepareea(&map, insn_info[iip].dp->dmode,
			      insn_info[iip].dp->dreg,
			      insn_info[iip].dp->size, &realpc, current_addr,
			      eainfo, 1, 0, 1);

	    generate_possible_exit(&map, eainfo, iip, &pub);
	    generate_possible_exit(&map, eainfo + 1, iip, &pub);

	    handle_bit_insns(&map, eainfo, 0, 1, insn_info[iip].dp->mnemo);
	    break;

	 case i_ASL: case i_ASR: case i_LSL: case i_LSR:
	 case i_ROL: case i_ROR: case i_ROXL:case i_ROXR:
	    if (insn_info[iip].dp->smode == Dreg && do_rotshi) {
		handle_rotshi_variable(&map, iip, realpc, current_addr, &pub);
		break;
	    }
	    /* fall through */
	 case i_ASLW: case i_ASRW: case i_LSLW: case i_LSRW:
	 case i_ROLW: case i_RORW: case i_ROXLW:case i_ROXRW:
	    if (do_rotshi) {
		handle_rotshi(&map, iip, realpc, current_addr, &pub);
		break;
	    }
#endif
	 default:
	    generate_exit(&map, insn_info[iip].address); cc_status = 0;
	    break;
	}
	if (insn_info[iip].ccuser_follows)
	    cc_status_for_bcc = compile_flush_cc_cache(&map, cc_status,
				   insn_info[iip].flags_live_at_end,
				   1, insn_info[iip+1].flags_live_at_end,
				   insn_info[iip+1].dp->cc);
	else
	    cc_status_for_bcc = compile_flush_cc_cache(&map, cc_status,
				   insn_info[iip].flags_live_at_end,
				   0, 0, 0);

	if (iip == current_bb->last_iip) {
	    current_bb++;
	}
    }
    if (compile_failure)
	goto oops;

    /* Compile all exits that we prepared earlier */
    finish_exits();
    if (compile_failure)
	goto oops;
    finish_condjumps(last_iip);
    {
	int needed_len = compile_here() - hb->compile_start;
	int allocsize = (needed_len + PAGE_SUBUNIT - 1) & ~(PAGE_SUBUNIT-1);
	uae_u32 allocmask;
	int allocbits;

	allocbits = (allocsize >> SUBUNIT_ORDER);
	allocmask = (1 << allocbits) - 1;
	while ((allocmask & hb->page_allocmask) != allocmask)
	    allocmask <<= 1;
	if ((hb->page_allocmask & ~allocmask) != 0 && !quiet_compile)
	    fprintf(stderr, "Gaining some bits: %08lx\n", hb->page_allocmask & ~allocmask);
	hb->cpage->allocmask &= ~hb->page_allocmask;
	hb->page_allocmask = allocmask;
	hb->cpage->allocmask |= allocmask;
    }
    return 0;

    oops:
    if (1 || !quiet_compile)
	fprintf(stderr, "Compile failed!\n");
    hb->cpage->allocmask &= ~hb->page_allocmask;
    hb->cpage = NULL;
    hb->untranslatable = 1;
    {
	struct hash_entry *h = hb->he_first;

	do {
	    h->execute = NULL;
	    h = h->next_same_block;
	} while (h != hb->he_first);
    }
    return 1;
}

void compiler_init(void)
{
    code_init();
    hash_init();
    jsr_stack_init();
}

/*
 * Why do compilers always have to be so complicated? And I thought GCC was
 * a mess...
 */

#endif /* USE_COMPILER */
