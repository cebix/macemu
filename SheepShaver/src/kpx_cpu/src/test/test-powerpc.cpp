/*
 *  test-powerpc.cpp - PowerPC regression testing
 *
 *  Kheperix (C) 2003 Gwenole Beauchesne
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

#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <netinet/in.h> // ntohl(), htonl()

#if EMU_KHEPERIX
#include "sysdeps.h"
#include "cpu/ppc/ppc-cpu.hpp"
#include "cpu/ppc/ppc-instructions.hpp"
#endif

#if EMU_MICROLIB
#include <ppcemul.h>
typedef unsigned int uint32;
typedef unsigned long uintptr;
#undef RD
#undef RA
#undef RB
#undef FB
#undef FE
#endif

#if EMU_MODEL3PPC
extern "C" {
#include "ppc.h"
}
typedef unsigned int uint32;
typedef unsigned long uintptr;
typedef uint32_t UINT32;
typedef char CHAR;
typedef int BOOL;
#endif

// Disassemblers needed for debugging purposes
#if ENABLE_MON
#include "mon.h"
#include "mon_disass.h"
#endif

// Define units to test (in-order: ALU, FPU, VMX)
#define TEST_ALU_OPS	1
#if EMU_KHEPERIX
#define TEST_FPU_OPS	1
#define TEST_VMX_OPS	1
#endif

// Define instructions to test
#define TEST_ADD		1
#define TEST_SUB		1
#define TEST_MUL		1
#define TEST_DIV		1
#define TEST_SHIFT		1
#define TEST_ROTATE		1
#define TEST_MISC		1
#define TEST_LOGICAL	1
#define TEST_COMPARE	1
#define TEST_CR_LOGICAL	1

// Partial PowerPC runtime assembler from GNU lightning
#define _I(X)			((uint32)(X))
#define _UL(X)			((uint32)(X))
#define _MASK(N)		((uint32)((1<<(N)))-1)
#define _ck_s(W,I)		(_UL(I) & _MASK(W))
#define _ck_u(W,I)    	(_UL(I) & _MASK(W))
#define _ck_su(W,I)    	(_UL(I) & _MASK(W))
#define _u1(I)          _ck_u( 1,I)
#define _u5(I)          _ck_u( 5,I)
#define _u6(I)          _ck_u( 6,I)
#define _u9(I)          _ck_u( 9,I)
#define _u10(I)         _ck_u(10,I)
#define _s16(I)         _ck_s(16,I)

#define _D(   OP,RD,RA,         DD )  	_I((_u6(OP)<<26)|(_u5(RD)<<21)|(_u5(RA)<<16)|                _s16(DD)                          )
#define _X(   OP,RD,RA,RB,   XO,RC )  	_I((_u6(OP)<<26)|(_u5(RD)<<21)|(_u5(RA)<<16)|( _u5(RB)<<11)|              (_u10(XO)<<1)|_u1(RC))
#define _XO(  OP,RD,RA,RB,OE,XO,RC )  	_I((_u6(OP)<<26)|(_u5(RD)<<21)|(_u5(RA)<<16)|( _u5(RB)<<11)|(_u1(OE)<<10)|( _u9(XO)<<1)|_u1(RC))
#define _M(   OP,RS,RA,SH,MB,ME,RC )  	_I((_u6(OP)<<26)|(_u5(RS)<<21)|(_u5(RA)<<16)|( _u5(SH)<<11)|(_u5(MB)<< 6)|( _u5(ME)<<1)|_u1(RC))

// PowerPC opcodes
static inline uint32 POWERPC_MR(int RD, int RA) { return _X(31,RA,RD,RA,444,0); }
static inline uint32 POWERPC_MFCR(int RD) { return _X(31,RD,00,00,19,0); }
const uint32 POWERPC_NOP = 0x60000000;
const uint32 POWERPC_BLR = 0x4e800020;
const uint32 POWERPC_BLRL = 0x4e800021;
const uint32 POWERPC_ILLEGAL = 0x00000000;
const uint32 POWERPC_EMUL_OP = 0x18000000;

// Invalidate test cache
#if defined(__powerpc__)
static void inline flush_icache_range(uint32 *start_p, uint32 length)
{
	const int MIN_CACHE_LINE_SIZE = 8; /* conservative value */

	unsigned long start = (unsigned long)start_p;
	unsigned long stop  = start + length;
    unsigned long p;

    p = start & ~(MIN_CACHE_LINE_SIZE - 1);
    stop = (stop + MIN_CACHE_LINE_SIZE - 1) & ~(MIN_CACHE_LINE_SIZE - 1);
    
    for (p = start; p < stop; p += MIN_CACHE_LINE_SIZE) {
        asm volatile ("dcbst 0,%0" : : "r"(p) : "memory");
    }
    asm volatile ("sync" : : : "memory");
    for (p = start; p < stop; p += MIN_CACHE_LINE_SIZE) {
        asm volatile ("icbi 0,%0" : : "r"(p) : "memory");
    }
    asm volatile ("sync" : : : "memory");
    asm volatile ("isync" : : : "memory");
}
#else
static void inline flush_icache_range(uint32 *start_p, uint32 length)
{
}
#endif

#if EMU_KHEPERIX
struct powerpc_cpu_base
	: public powerpc_cpu
{
	powerpc_cpu_base();
	void init_decoder();
	void execute_return(uint32 opcode);

	uint32 emul_get_xer() const			{ return xer().get(); }
	void emul_set_xer(uint32 value)		{ xer().set(value); }
	uint32 emul_get_cr() const			{ return cr().get(); }
	void emul_set_cr(uint32 value)		{ cr().set(value); }
	uint32 get_lr() const				{ return lr(); }
	void set_lr(uint32 value)			{ lr() = value; }
	uint32 get_gpr(int i) const			{ return gpr(i); }
	void set_gpr(int i, uint32 value)	{ gpr(i) = value; }
};

powerpc_cpu_base::powerpc_cpu_base()
	: powerpc_cpu(NULL)
{
	init_decoder();
}

void powerpc_cpu_base::execute_return(uint32 opcode)
{
	spcflags().set(SPCFLAG_CPU_EXEC_RETURN);
}

void powerpc_cpu_base::init_decoder()
{
#ifndef PPC_NO_STATIC_II_INDEX_TABLE
	static bool initialized = false;
	if (initialized)
		return;
	initialized = true;
#endif

	static const instr_info_t return_ii_table[] = {
		{ "return",
		  (execute_pmf)&powerpc_cpu_base::execute_return,
		  NULL,
		  PPC_I(MAX),
		  D_form, 6, 0, CFLOW_JUMP
		}
	};

	const int ii_count = sizeof(return_ii_table)/sizeof(return_ii_table[0]);

	for (int i = 0; i < ii_count; i++) {
		const instr_info_t * ii = &return_ii_table[i];
		init_decoder_entry(ii);
	}
}
#endif

#if EMU_MICROLIB
static volatile bool ppc_running = false;

struct powerpc_cpu_base
{
	powerpc_cpu_base();
	void execute(uintptr);

	void invalidate_cache()				{ }
	uint32 emul_get_xer() const			{ return XER; }
	void emul_set_xer(uint32 value)		{ XER = value; }
	uint32 emul_get_cr() const			{ return CR; }
	void emul_set_cr(uint32 value)		{ CR = value; }
	uint32 get_lr() const				{ return LR; }
	void set_lr(uint32 value)			{ LR = value; }
	uint32 get_gpr(int i) const			{ return GPR(i); }
	void set_gpr(int i, uint32 value)	{ GPR(i) = value; }
};

void sheep_impl(ppc_inst_t inst)
{
	ppc_running = false;
}

extern "C" void init_table(int opcd, void (*impl)(ppc_inst_t), char *(*bin2c)(ppc_inst_t, addr_t, char *), char *(*disasm)(ppc_inst_t, addr_t, char *), void (*translate)(ppc_inst_t, struct DecodedInstruction *), void (*xmlize)(ppc_inst_t, addr_t, char *), char *mnemonic);

powerpc_cpu_base::powerpc_cpu_base()
{
	ppc_init();
	init_table(6, sheep_impl, NULL, NULL, NULL, NULL, "sheep");
}

#define ppc_code_fetch(A)  ntohl(*((uint32 *)(A)))

void powerpc_cpu_base::execute(uintptr entry_point)
{
	PC = entry_point;

	ppc_running = true;
	while (ppc_running) {
		ppc_inst_t inst = ppc_code_fetch(PC);
		ppc_execute(inst);
	}
}
#endif

#if EMU_MODEL3PPC
extern "C" BOOL DisassemblePowerPC(UINT32, UINT32, CHAR *, CHAR *, BOOL);
BOOL DisassemblePowerPC(UINT32, UINT32, CHAR *, CHAR *, BOOL) { }

static volatile bool ppc_running = false;

struct powerpc_cpu_base
{
	powerpc_cpu_base();
	void execute(uintptr);

	void invalidate_cache()				{ }
	uint32 emul_get_xer() const			{ return ppc_get_reg(PPC_REG_XER); }
	void emul_set_xer(uint32 value)		{ ppc_set_reg(PPC_REG_XER, value); }
	uint32 emul_get_cr() const			{ return ppc_get_reg(PPC_REG_CR); }
	void emul_set_cr(uint32 value)		{ ppc_set_reg(PPC_REG_CR, value); }
	uint32 get_lr() const				{ return ppc_get_reg(PPC_REG_LR); }
	void set_lr(uint32 value)			{ ppc_set_reg(PPC_REG_LR, value); }
	uint32 get_gpr(int i) const			{ return ppc_get_r(i); }
	void set_gpr(int i, uint32 value)	{ ppc_set_r(i, value); }
};

static uint32 read_32(uint32 a)
{
	return ntohl(*((uint32 *)a));
}

static uint32 read_op(uint32 a)
{
	uint32 opcode = read_32(a);
	if (opcode == POWERPC_EMUL_OP) {
		ppc_running = false;
		return POWERPC_NOP;
	}
	return opcode;
}

powerpc_cpu_base::powerpc_cpu_base()
{
	ppc_init(NULL);
	ppc_set_read_32_handler((void *)&read_32);
	ppc_set_read_op_handler((void *)&read_op);
}

void powerpc_cpu_base::execute(uintptr entry_point)
{
	ppc_set_reg(PPC_REG_PC, entry_point);

	ppc_running = true;
	while (ppc_running)
		ppc_run(1);
}
#endif

// Define bit-fields
#if !EMU_KHEPERIX
template< int FB, int FE >
struct static_mask {
	enum { value = (0xffffffff >> FB) ^ (0xffffffff >> (FE + 1)) };
};

template< int FB >
struct static_mask<FB, 31> {
	enum { value  = 0xffffffff >> FB };
};

template< int FB, int FE >
struct bit_field {
	static inline uint32 mask() {
		return static_mask<FB, FE>::value;
	}
	static inline bool test(uint32 value) {
		return value & mask();
	}
	static inline uint32 extract(uint32 value) {
		const uint32 m = mask() >> (31 - FE);
		return (value >> (31 - FE)) & m;
	}
	static inline void insert(uint32 & data, uint32 value) {
		const uint32 m = mask();
		data = (data & ~m) | ((value << (31 - FE)) & m);
	}
};

// General purpose registers
typedef bit_field< 11, 15 > rA_field;
typedef bit_field< 16, 20 > rB_field;
typedef bit_field<  6, 10 > rD_field;
typedef bit_field<  6, 10 > rS_field;

// Condition registers
typedef bit_field< 11, 15 > crbA_field;
typedef bit_field< 16, 20 > crbB_field;
typedef bit_field<  6, 10 > crbD_field;
typedef bit_field<  6,  8 > crfD_field;
typedef bit_field< 11, 13 > crfS_field;

// CR register fields
template< int CRn > struct CR_field : bit_field< 4*CRn+0, 4*CRn+3 > { };
template< int CRn > struct CR_LT_field : bit_field< 4*CRn+0, 4*CRn+0 > { };
template< int CRn > struct CR_GT_field : bit_field< 4*CRn+1, 4*CRn+1 > { };
template< int CRn > struct CR_EQ_field : bit_field< 4*CRn+2, 4*CRn+2 > { };
template< int CRn > struct CR_SO_field : bit_field< 4*CRn+3, 4*CRn+3 > { };
template< int CRn > struct CR_UN_field : bit_field< 4*CRn+3, 4*CRn+3 > { };

// Immediates
typedef bit_field< 16, 31 > UIMM_field;
typedef bit_field< 21, 25 > MB_field;
typedef bit_field< 26, 30 > ME_field;
typedef bit_field< 16, 20 > SH_field;

// XER register fields
typedef bit_field<  0,  0 > XER_SO_field;
typedef bit_field<  1,  1 > XER_OV_field;
typedef bit_field<  2,  2 > XER_CA_field;
#endif
#undef  CA
#define CA XER_CA_field::mask()
#undef  OV
#define OV XER_OV_field::mask()
#undef  SO
#define SO XER_SO_field::mask()

// Define PowerPC tester
class powerpc_test_cpu
	: public powerpc_cpu_base
{
#if defined(__powerpc__)
	uint32 native_get_xer() const
		{ uint32 xer; asm volatile ("mfxer %0" : "=r" (xer)); return xer; }

	void native_set_xer(uint32 xer) const
		{ asm volatile ("mtxer %0" : : "r" (xer)); }

	uint32 native_get_cr() const
		{ uint32 cr; asm volatile ("mfcr %0" : "=r" (cr)); return cr; }

	void native_set_cr(uint32 cr) const
		{ asm volatile ("mtcr %0" :  : "r" (cr)); }
#endif

	void flush_icache_range(uint32 *start, uint32 size)
		{ invalidate_cache(); ::flush_icache_range(start, size); }

	void print_xer_flags(uint32 xer) const;
	void print_flags(uint32 cr, uint32 xer, int crf = 0) const;
	void execute(uint32 *code);

public:

	powerpc_test_cpu();
	~powerpc_test_cpu();

	bool test(void);

	void set_results_file(FILE *fp)
		{ results_file = fp; }

private:

	static const bool verbose = false;
	uint32 tests, errors;

	// Results file for reference
	FILE *results_file;
	uint32 get32();
	void put32(uint32 v);

	// Initial CR0, XER state
	uint32 init_cr;
	uint32 init_xer;

	// XER preset values to test with
	std::vector<uint32> xer_values;
	void gen_xer_values(uint32 use_mask, uint32 set_mask);

	// Emulated registers IDs
	enum {
		RD = 3,
		RA = 4,
		RB = 5
	};

	static const uint32 reg_values[];
	static const uint32 imm_values[];
	static const uint32 msk_values[];

	void test_one_1(uint32 *code, const char *insn, uint32 a1, uint32 a2, uint32 a3, uint32 a0 = 0);
	void test_one(uint32 *code, const char *insn, uint32 a1, uint32 a2, uint32 a3, uint32 a0 = 0);
	void test_instruction_CNTLZ(const char *insn, uint32 opcode);
	void test_instruction_RR___(const char *insn, uint32 opcode);
	void test_instruction_RRI__(const char *insn, uint32 opcode);
#define  test_instruction_RRK__ test_instruction_RRI__
	void test_instruction_RRS__(const char *insn, uint32 opcode);
	void test_instruction_RRR__(const char *insn, uint32 opcode);
	void test_instruction_RRRSH(const char *insn, uint32 opcode);
	void test_instruction_RRIII(const char *insn, uint32 opcode);
	void test_instruction_RRRII(const char *insn, uint32 opcode);
	void test_instruction_CRR__(const char *insn, uint32 opcode);
	void test_instruction_CRI__(const char *insn, uint32 opcode);
#define  test_instruction_CRK__ test_instruction_CRI__
	void test_instruction_CCC__(const char *insn, uint32 opcode);

	void test_add(void);
	void test_sub(void);
	void test_mul(void);
	void test_div(void);
	void test_shift(void);
	void test_rotate(void);
	void test_logical(void);
	void test_compare(void);
	void test_cr_logical(void);
};

powerpc_test_cpu::powerpc_test_cpu()
	: powerpc_cpu_base(), results_file(NULL)
{
#if ENABLE_MON
	mon_init();
#endif
}

powerpc_test_cpu::~powerpc_test_cpu()
{
#if ENABLE_MON
	mon_exit();
#endif
}

uint32 powerpc_test_cpu::get32()
{
	uint32 v;
	if (fread(&v, sizeof(v), 1, results_file) != 1) {
		fprintf(stderr, "ERROR: unexpected end of results file\n");
		exit(EXIT_FAILURE);
	}
	return ntohl(v);
}

void powerpc_test_cpu::put32(uint32 v)
{
	uint32 out = htonl(v);
	if (fwrite(&out, sizeof(out), 1, results_file) != 1) {
		fprintf(stderr, "could not write item to results file\n");
		exit(EXIT_FAILURE);
	}
}

void powerpc_test_cpu::execute(uint32 *code_p)
{
	static uint32 code[2];
	code[0] = htonl(POWERPC_BLRL);
	code[1] = htonl(POWERPC_EMUL_OP);

#if !defined(__powerpc__)
	const int n_func_words = 1024;
	static uint32 func[n_func_words];
	static int old_i;
  again:
	int i = old_i;
	for (int j = 0; ; j++, i++) {
		if (i >= n_func_words) {
			old_i = 0;
			invalidate_cache();
			goto again;
		}
		uint32 opcode = code_p[j];
		func[i] = htonl(opcode);
		if (opcode == POWERPC_BLR)
			break;
	}
	code_p = &func[old_i];
	old_i = i;
#endif

	assert((uintptr)code_p <= UINT_MAX);
	set_lr((uintptr)code_p);

	assert((uintptr)code <= UINT_MAX);
	powerpc_cpu_base::execute((uintptr)code);
}

void powerpc_test_cpu::gen_xer_values(uint32 use_mask, uint32 set_mask)
{
	const uint32 mask = use_mask | set_mask;

	// Always test with XER=0
	xer_values.clear();
	xer_values.push_back(0);

	// Iterate over XER fields, only handle CA, OV, SO
	for (uint32 m = 0x80000000; m != 0; m >>= 1) {
		if (m & (CA | OV | SO) & mask) {
			const int n_xer_values = xer_values.size();
			for (int i = 0; i < n_xer_values; i++)
				xer_values.push_back(xer_values[i] | m);
		}
	}

#if 0
	printf("%d XER values\n", xer_values.size());
	for (int i = 0; i < xer_values.size(); i++) {
		print_xer_flags(xer_values[i]);
		printf("\n");
	}
#endif
}

void powerpc_test_cpu::print_xer_flags(uint32 xer) const
{
	printf("%s,%s,%s",
		   (xer & XER_CA_field::mask() ? "CA" : "__"),
		   (xer & XER_OV_field::mask() ? "OV" : "__"),
		   (xer & XER_SO_field::mask() ? "SO" : "__"));
}

void powerpc_test_cpu::print_flags(uint32 cr, uint32 xer, int crf) const
{
	cr = cr << (4 * crf);
	printf("%s,%s,%s,%s,%s,%s",
		   (cr & CR_LT_field<0>::mask() ? "LT" : "__"),
		   (cr & CR_GT_field<0>::mask() ? "GT" : "__"),
		   (cr & CR_EQ_field<0>::mask() ? "EQ" : "__"),
		   (cr & CR_SO_field<0>::mask() ? "SO" : "__"),
		   (xer & XER_OV_field::mask()  ? "OV" : "__"),
		   (xer & XER_CA_field::mask()  ? "CA" : "__"));
}

#define TEST_INSTRUCTION(FORMAT, NATIVE_OP, EMUL_OP) do {	\
	printf("Testing " NATIVE_OP "\n");						\
	test_instruction_##FORMAT(NATIVE_OP, EMUL_OP);			\
} while (0)

void powerpc_test_cpu::test_one(uint32 *code, const char *insn, uint32 a1, uint32 a2, uint32 a3, uint32 a0)
{
	// Iterate over test XER values as input
	const int n_xer_values = xer_values.size();
	for (int i = 0; i < n_xer_values; i++) {
		init_xer = xer_values[i];
		test_one_1(code, insn, a1, a2, a3, a0);
	}
	init_xer = 0;
}

void powerpc_test_cpu::test_one_1(uint32 *code, const char *insn, uint32 a1, uint32 a2, uint32 a3, uint32 a0)
{
#if defined(__powerpc__)
	// Invoke native code
	const uint32 save_xer = native_get_xer();
	const uint32 save_cr = native_get_cr();
	native_set_xer(init_xer);
	native_set_cr(init_cr);
	typedef uint32 (*func_t)(uint32, uint32, uint32);
	func_t func = (func_t)code;
	const uint32 native_rd = func(a0, a1, a2);
	const uint32 native_xer = native_get_xer();
	const uint32 native_cr = native_get_cr();
	native_set_xer(save_xer);
	native_set_cr(save_cr);
	if (results_file) {
		put32(native_rd);
		put32(native_xer);
		put32(native_cr);
	}
#else
	const uint32 native_rd = get32();
	const uint32 native_xer = get32();
	const uint32 native_cr = get32();
#endif

	// Invoke emulated code
	emul_set_xer(init_xer);
	emul_set_cr(init_cr);
	set_gpr(RD, a0);
	set_gpr(RA, a1);
	set_gpr(RB, a2);
	execute(code);
	const uint32 emul_rd = get_gpr(RD);
	const uint32 emul_xer = emul_get_xer();
	const uint32 emul_cr = emul_get_cr();
	
	++tests;

	bool ok = native_rd == emul_rd
		&& native_xer == emul_xer
		&& native_cr == emul_cr;

	if (code[0] == POWERPC_MR(0, RA))
		code++;

	if (!ok) {
		printf("FAIL: %s [%08x]\n", insn, code[0]);
		errors++;
	}
	else if (verbose) {
		printf("PASS: %s [%08x]\n", insn, code[0]);
	}

	if (!ok || verbose) {
#if ENABLE_MON
		disass_ppc(stdout, (uintptr)code, code[0]);
#endif
#define PRINT_OPERANDS(PREFIX) do {						\
			printf(" %08x, %08x, %08x, %08x => %08x [",	\
				   a0, a1, a2, a3, PREFIX##_rd);		\
			print_flags(PREFIX##_cr, PREFIX##_xer);		\
			printf("]\n");								\
		} while (0)
		PRINT_OPERANDS(native);
		PRINT_OPERANDS(emul);
#undef  PRINT_OPERANDS
	}
}

const uint32 powerpc_test_cpu::reg_values[] = {
	0x00000000, 0x10000000, 0x20000000,
	0x30000000, 0x40000000, 0x50000000,
	0x60000000, 0x70000000, 0x80000000,
	0x90000000, 0xa0000000, 0xb0000000,
	0xc0000000, 0xd0000000, 0xe0000000,
	0xf0000000, 0xfffffffd, 0xfffffffe,
	0xffffffff, 0x00000001, 0x00000002,
	0x00000003, 0x11111111, 0x22222222,
	0x33333333, 0x44444444, 0x55555555,
	0x66666666, 0x77777777, 0x88888888,
	0x99999999, 0xaaaaaaaa, 0xbbbbbbbb,
	0xcccccccc, 0xdddddddd, 0xeeeeeeee
};

const uint32 powerpc_test_cpu::imm_values[] = {
	0x0000, 0x1000, 0x2000,
	0x3000, 0x4000, 0x5000,
	0x6000, 0x7000, 0x8000,
	0x9000, 0xa000, 0xb000,
	0xc000, 0xd000, 0xe000,
	0xf000, 0xfffd, 0xfffe,
	0xffff, 0x0001, 0x0002,
	0x0003, 0x1111, 0x2222,
	0x3333, 0x4444, 0x5555,
	0x6666, 0x7777, 0x8888,
	0x9999, 0xaaaa, 0xbbbb,
	0xcccc, 0xdddd, 0xeeee
};

const uint32 powerpc_test_cpu::msk_values[] = {
	0, 1,
//	15, 16, 17,
	30, 31
};

void powerpc_test_cpu::test_instruction_CNTLZ(const char *insn, uint32 opcode)
{
	// Test code
	static uint32 code[] = {
		POWERPC_ILLEGAL, POWERPC_BLR,
		POWERPC_MR(0, RA), POWERPC_ILLEGAL, POWERPC_BLR
	};

	// Input values
	const int n_values = sizeof(reg_values)/sizeof(reg_values[0]);

	code[0] = code[3] = opcode;			// <op> RD,RA,RB
	rA_field::insert(code[3], 0);		// <op> RD,R0,RB
	flush_icache_range(code, sizeof(code));

	for (uint32 mask = 0x800000000; mask != 0; mask >>= 1) {
		uint32 ra = mask;
		test_one(&code[0], insn, ra, 0, 0);
		test_one(&code[2], insn, ra, 0, 0);
	}
	// random values (including zero)
	for (int i = 0; i < n_values; i++) {
		uint32 ra = reg_values[i];
		test_one(&code[0], insn, ra, 0, 0);
		test_one(&code[2], insn, ra, 0, 0);
	}
}

void powerpc_test_cpu::test_instruction_RR___(const char *insn, uint32 opcode)
{
	// Test code
	static uint32 code[] = {
		POWERPC_ILLEGAL, POWERPC_BLR,
		POWERPC_MR(0, RA), POWERPC_ILLEGAL, POWERPC_BLR
	};

	// Input values
	const int n_values = sizeof(reg_values)/sizeof(reg_values[0]);

	code[0] = code[3] = opcode;			// <op> RD,RA,RB
	rA_field::insert(code[3], 0);		// <op> RD,R0,RB
	flush_icache_range(code, sizeof(code));

	for (int i = 0; i < n_values; i++) {
		uint32 ra = reg_values[i];
		test_one(&code[0], insn, ra, 0, 0);
		test_one(&code[2], insn, ra, 0, 0);
	}
}

void powerpc_test_cpu::test_instruction_RRI__(const char *insn, uint32 opcode)
{
	// Test code
	static uint32 code[] = {
		POWERPC_ILLEGAL, POWERPC_BLR,
		POWERPC_MR(0, RA), POWERPC_ILLEGAL, POWERPC_BLR
	};

	// Input values
	const int n_reg_values = sizeof(reg_values)/sizeof(reg_values[0]);
	const int n_imm_values = sizeof(imm_values)/sizeof(imm_values[0]);

	for (int j = 0; j < n_imm_values; j++) {
		const uint32 im = imm_values[j];
		uint32 op = opcode;
		UIMM_field::insert(op, im);
		code[0] = code[3] = op;				// <op> RD,RA,IM
		rA_field::insert(code[3], 0);		// <op> RD,R0,IM
		flush_icache_range(code, sizeof(code));
		for (int i = 0; i < n_reg_values; i++) {
			const uint32 ra = reg_values[i];
			test_one(&code[0], insn, ra, im, 0);
			test_one(&code[2], insn, ra, im, 0);
		}
	}
}

void powerpc_test_cpu::test_instruction_RRS__(const char *insn, uint32 opcode)
{
	// Test code
	static uint32 code[] = {
		POWERPC_ILLEGAL, POWERPC_BLR,
		POWERPC_MR(0, RA), POWERPC_ILLEGAL, POWERPC_BLR
	};

	// Input values
	const int n_values = sizeof(reg_values)/sizeof(reg_values[0]);

	for (int j = 0; j < 32; j++) {
		const uint32 sh = j;
		SH_field::insert(opcode, sh);
		code[0] = code[3] = opcode;
		rA_field::insert(code[3], 0);
		flush_icache_range(code, sizeof(code));
		for (int i = 0; i < n_values; i++) {
			const uint32 ra = reg_values[i];
			test_one(&code[0], insn, ra, sh, 0);
		}
	}
}

void powerpc_test_cpu::test_instruction_RRR__(const char *insn, uint32 opcode)
{
	// Test code
	static uint32 code[] = {
		POWERPC_ILLEGAL, POWERPC_BLR,
		POWERPC_MR(0, RA), POWERPC_ILLEGAL, POWERPC_BLR
	};

	// Input values
	const int n_values = sizeof(reg_values)/sizeof(reg_values[0]);

	code[0] = code[3] = opcode;			// <op> RD,RA,RB
	rA_field::insert(code[3], 0);		// <op> RD,R0,RB
	flush_icache_range(code, sizeof(code));

	for (int i = 0; i < n_values; i++) {
		const uint32 ra = reg_values[i];
		for (int j = 0; j < n_values; j++) {
			const uint32 rb = reg_values[j];
			test_one(&code[0], insn, ra, rb, 0);
			test_one(&code[2], insn, ra, rb, 0);
		}
	}
}

void powerpc_test_cpu::test_instruction_RRRSH(const char *insn, uint32 opcode)
{
	// Test code
	static uint32 code[] = {
		POWERPC_ILLEGAL, POWERPC_BLR,
		POWERPC_MR(0, RA), POWERPC_ILLEGAL, POWERPC_BLR
	};

	// Input values
	const int n_values = sizeof(reg_values)/sizeof(reg_values[0]);

	code[0] = code[3] = opcode;			// <op> RD,RA,RB
	rA_field::insert(code[3], 0);		// <op> RD,R0,RB
	flush_icache_range(code, sizeof(code));

	for (int i = 0; i < n_values; i++) {
		const uint32 ra = reg_values[i];
		for (int j = 0; j <= 64; j++) {
			const uint32 rb = j;
			test_one(&code[0], insn, ra, rb, 0);
			test_one(&code[2], insn, ra, rb, 0);
		}
	}
}

void powerpc_test_cpu::test_instruction_RRIII(const char *insn, uint32 opcode)
{
	// Test code
	static uint32 code[] = {
		POWERPC_ILLEGAL, POWERPC_BLR,
		POWERPC_MR(0, RA), POWERPC_ILLEGAL, POWERPC_BLR
	};

	// Input values
	const int n_reg_values = sizeof(reg_values)/sizeof(reg_values[0]);
	const int n_msk_values = sizeof(msk_values)/sizeof(msk_values[0]);

	for (int sh = 0; sh < 32; sh++) {
		for (int i_mb = 0; i_mb < n_msk_values; i_mb++) {
			const uint32 mb = msk_values[i_mb];
			for (int i_me = 0; i_me < n_msk_values; i_me++) {
				const uint32 me = msk_values[i_me];
				SH_field::insert(opcode, sh);
				MB_field::insert(opcode, mb);
				ME_field::insert(opcode, me);
				code[0] = opcode;
				code[3] = opcode;
				rA_field::insert(code[3], 0);
				flush_icache_range(code, sizeof(code));
				for (int i = 0; i < n_reg_values; i++) {
					const uint32 ra = reg_values[i];
					test_one(&code[0], insn, ra, sh, 0, 0);
					test_one(&code[2], insn, ra, sh, 0, 0);
				}
			}
		}
	}
}

void powerpc_test_cpu::test_instruction_RRRII(const char *insn, uint32 opcode)
{
	// Test code
	static uint32 code[] = {
		POWERPC_ILLEGAL, POWERPC_BLR,
		POWERPC_MR(0, RA), POWERPC_ILLEGAL, POWERPC_BLR
	};

	// Input values
	const int n_reg_values = sizeof(reg_values)/sizeof(reg_values[0]);
	const int n_msk_values = sizeof(msk_values)/sizeof(msk_values[0]);

	for (int i_mb = 0; i_mb < n_msk_values; i_mb++) {
		const uint32 mb = msk_values[i_mb];
		for (int i_me = 0; i_me < n_msk_values; i_me++) {
			const uint32 me = msk_values[i_me];
			MB_field::insert(opcode, mb);
			ME_field::insert(opcode, me);
			code[0] = opcode;
			code[3] = opcode;
			rA_field::insert(code[3], 0);
			flush_icache_range(code, sizeof(code));
			for (int i = 0; i < n_reg_values; i++) {
				const uint32 ra = reg_values[i];
				for (int j = -1; j <= 33; j++) {
					const uint32 rb = j;
					test_one(&code[0], insn, ra, rb, 0, 0);
					test_one(&code[2], insn, ra, rb, 0, 0);
				}
			}
		}
	}
}

void powerpc_test_cpu::test_instruction_CRR__(const char *insn, uint32 opcode)
{
	// Test code
	static uint32 code[] = {
		POWERPC_ILLEGAL, POWERPC_BLR,
		POWERPC_MR(0, RA), POWERPC_ILLEGAL, POWERPC_BLR
	};

	// Input values
	const int n_values = sizeof(reg_values)/sizeof(reg_values[0]);

	for (int k = 0; k < 8; k++) {
		crfD_field::insert(opcode, k);
		code[0] = code[3] = opcode;			// <op> crfD,RA,RB
		rA_field::insert(code[3], 0);		// <op> crfD,R0,RB
		flush_icache_range(code, sizeof(code));
		for (int i = 0; i < n_values; i++) {
			const uint32 ra = reg_values[i];
			for (int j = 0; j < n_values; j++) {
			const uint32 rb = reg_values[j];
			test_one(&code[0], insn, ra, rb, 0);
			test_one(&code[2], insn, ra, rb, 0);
			}
		}
	}
}

void powerpc_test_cpu::test_instruction_CRI__(const char *insn, uint32 opcode)
{
	// Test code
	static uint32 code[] = {
		POWERPC_ILLEGAL, POWERPC_BLR,
		POWERPC_MR(0, RA), POWERPC_ILLEGAL, POWERPC_BLR
	};

	// Input values
	const int n_reg_values = sizeof(reg_values)/sizeof(reg_values[0]);
	const int n_imm_values = sizeof(imm_values)/sizeof(imm_values[0]);

	for (int k = 0; k < 8; k++) {
		crfD_field::insert(opcode, k);
		for (int j = 0; j < n_imm_values; j++) {
			const uint32 im = imm_values[j];
			UIMM_field::insert(opcode, im);
			code[0] = code[3] = opcode;			// <op> crfD,RA,SIMM
			rA_field::insert(code[3], 0);		// <op> crfD,R0,SIMM
			flush_icache_range(code, sizeof(code));
			for (int i = 0; i < n_reg_values; i++) {
				const uint32 ra = reg_values[i];
				test_one(&code[0], insn, ra, im, 0);
				test_one(&code[2], insn, ra, im, 0);
			}
		}
	}
}

void powerpc_test_cpu::test_instruction_CCC__(const char *insn, uint32 opcode)
{
	// Test code
	static uint32 code[] = {
		POWERPC_ILLEGAL, POWERPC_MFCR(RD), POWERPC_BLR,
	};

	const uint32 saved_cr = init_cr;
	crbD_field::insert(opcode, 0);

	// Loop over crbA=[4-7] (crf1), crbB=[28-31] (crf7)
	for (int crbA = 4; crbA <= 7; crbA++) {
		crbA_field::insert(opcode, crbA);
		for (int crbB = 28; crbB <= 31; crbB++) {
			crbB_field::insert(opcode, crbB);
			code[0] = opcode;
			flush_icache_range(code, sizeof(code));
			// Generate CR values for (crf1, crf7)
			uint32 cr = 0;
			for (int i = 0; i < 16; i++) {
				CR_field<1>::insert(cr, i);
				for (int j = 0; j < 16; j++) {
					CR_field<7>::insert(cr, j);
					init_cr = cr;
					test_one(&code[0], insn, init_cr, 0, 0);
				}
			}
		}
	}
	init_cr = saved_cr;
}

void powerpc_test_cpu::test_add(void)
{
#if TEST_ADD
	gen_xer_values(0, 0);
	TEST_INSTRUCTION(RRI__,"addi",		_D (14,RD,RA,00));
	TEST_INSTRUCTION(RRI__,"addis",		_D (15,RD,RA,00));
	gen_xer_values(0, SO);
	TEST_INSTRUCTION(RRR__,"add",		_XO(31,RD,RA,RB,0,266,0));
	TEST_INSTRUCTION(RRR__,"add.",		_XO(31,RD,RA,RB,0,266,1));
	gen_xer_values(0, SO|OV);
	TEST_INSTRUCTION(RRR__,"addo",		_XO(31,RD,RA,RB,1,266,0));
	TEST_INSTRUCTION(RRR__,"addo." ,	_XO(31,RD,RA,RB,1,266,1));
	gen_xer_values(0, SO|CA);
	TEST_INSTRUCTION(RRR__,"addc",		_XO(31,RD,RA,RB,0, 10,0));
	TEST_INSTRUCTION(RRR__,"addc.",		_XO(31,RD,RA,RB,0, 10,1));
	TEST_INSTRUCTION(RRI__,"addic",		_D (12,RD,RA,00));
	TEST_INSTRUCTION(RRI__,"addic.",	_D (13,RD,RA,00));
	gen_xer_values(0, SO|CA|OV);
	TEST_INSTRUCTION(RRR__,"addco",		_XO(31,RD,RA,RB,1, 10,0));
	TEST_INSTRUCTION(RRR__,"addco.",	_XO(31,RD,RA,RB,1, 10,1));
	gen_xer_values(CA, SO|CA);
	TEST_INSTRUCTION(RRR__,"adde",		_XO(31,RD,RA,RB,0,138,0));
	TEST_INSTRUCTION(RRR__,"adde.",		_XO(31,RD,RA,RB,0,138,1));
	TEST_INSTRUCTION(RR___,"addme",		_XO(31,RD,RA,00,0,234,0));
	TEST_INSTRUCTION(RR___,"addme.",	_XO(31,RD,RA,00,0,234,1));
	TEST_INSTRUCTION(RR___,"addze",		_XO(31,RD,RA,00,0,202,0));
	TEST_INSTRUCTION(RR___,"addze.",	_XO(31,RD,RA,00,0,202,1));
	gen_xer_values(CA, SO|CA|OV);
	TEST_INSTRUCTION(RRR__,"addeo",		_XO(31,RD,RA,RB,1,138,0));
	TEST_INSTRUCTION(RRR__,"addeo.",	_XO(31,RD,RA,RB,1,138,1));
	TEST_INSTRUCTION(RR___,"addmeo",	_XO(31,RD,RA,00,1,234,0));
	TEST_INSTRUCTION(RR___,"addmeo.",	_XO(31,RD,RA,00,1,234,1));
	TEST_INSTRUCTION(RR___,"addzeo",	_XO(31,RD,RA,00,1,202,0));
	TEST_INSTRUCTION(RR___,"addzeo.",	_XO(31,RD,RA,00,1,202,1));
#endif
}

void powerpc_test_cpu::test_sub(void)
{
#if TEST_SUB
	gen_xer_values(0, SO);
	TEST_INSTRUCTION(RRR__,"subf",		_XO(31,RD,RA,RB,0, 40,0));
	TEST_INSTRUCTION(RRR__,"subf.",		_XO(31,RD,RA,RB,0, 40,1));
	gen_xer_values(0, SO|OV);
	TEST_INSTRUCTION(RRR__,"subfo",		_XO(31,RD,RA,RB,1, 40,0));
	TEST_INSTRUCTION(RRR__,"subfo.",	_XO(31,RD,RA,RB,1, 40,1));
	gen_xer_values(0, SO|CA);
	TEST_INSTRUCTION(RRR__,"subfc",		_XO(31,RD,RA,RB,0,  8,0));
	TEST_INSTRUCTION(RRR__,"subfc.",	_XO(31,RD,RA,RB,0,  8,1));
	gen_xer_values(0, SO|CA|OV);
	TEST_INSTRUCTION(RRR__,"subfco",	_XO(31,RD,RA,RB,1,  8,0));
	TEST_INSTRUCTION(RRR__,"subfco.",	_XO(31,RD,RA,RB,1,  8,1));
	gen_xer_values(0, CA);
	TEST_INSTRUCTION(RRI__,"subfic",	_D ( 8,RD,RA,00));
	gen_xer_values(CA, SO|CA);
	TEST_INSTRUCTION(RRR__,"subfe",		_XO(31,RD,RA,RB,0,136,0));
	TEST_INSTRUCTION(RRR__,"subfe.",	_XO(31,RD,RA,RB,0,136,1));
	TEST_INSTRUCTION(RR___,"subfme",	_XO(31,RD,RA,00,0,232,0));
	TEST_INSTRUCTION(RR___,"subfme.",	_XO(31,RD,RA,00,0,232,1));
	TEST_INSTRUCTION(RR___,"subfze",	_XO(31,RD,RA,00,0,200,0));
	TEST_INSTRUCTION(RR___,"subfze.",	_XO(31,RD,RA,00,0,200,1));
	gen_xer_values(CA, SO|CA|OV);
	TEST_INSTRUCTION(RRR__,"subfeo",	_XO(31,RD,RA,RB,1,136,0));
	TEST_INSTRUCTION(RRR__,"subfeo.",	_XO(31,RD,RA,RB,1,136,1));
	TEST_INSTRUCTION(RR___,"subfmeo",	_XO(31,RD,RA,00,1,232,0));
	TEST_INSTRUCTION(RR___,"subfmeo.",	_XO(31,RD,RA,00,1,232,1));
	TEST_INSTRUCTION(RR___,"subfzeo",	_XO(31,RD,RA,00,1,200,0));
	TEST_INSTRUCTION(RR___,"subfzeo.",	_XO(31,RD,RA,00,1,200,1));
#endif
}

void powerpc_test_cpu::test_mul(void)
{
#if TEST_MUL
	gen_xer_values(0, SO);
	TEST_INSTRUCTION(RRR__,"mulhw",		_XO(31,RD,RA,RB,0, 75,0));
	TEST_INSTRUCTION(RRR__,"mulhw.",	_XO(31,RD,RA,RB,0, 75,1));
	TEST_INSTRUCTION(RRR__,"mulhwu",	_XO(31,RD,RA,RB,0, 11,0));
	TEST_INSTRUCTION(RRR__,"mulhwu.",	_XO(31,RD,RA,RB,0, 11,1));
	TEST_INSTRUCTION(RRI__,"mulli",		_D ( 7,RD,RA,00));
	TEST_INSTRUCTION(RRR__,"mullw",		_XO(31,RD,RA,RB,0,235,0));
	TEST_INSTRUCTION(RRR__,"mullw.",	_XO(31,RD,RA,RB,0,235,1));
	gen_xer_values(0, SO|OV);
	TEST_INSTRUCTION(RRR__,"mullwo",	_XO(31,RD,RA,RB,1,235,0));
	TEST_INSTRUCTION(RRR__,"mullwo.",	_XO(31,RD,RA,RB,1,235,1));
#endif
}

void powerpc_test_cpu::test_div(void)
{
#if TEST_DIV
	gen_xer_values(0, SO);
	TEST_INSTRUCTION(RRR__,"divw",		_XO(31,RD,RA,RB,0,491,0));
	TEST_INSTRUCTION(RRR__,"divw.",		_XO(31,RD,RA,RB,0,491,1));
	TEST_INSTRUCTION(RRR__,"divwu",		_XO(31,RD,RA,RB,0,459,0));
	TEST_INSTRUCTION(RRR__,"divwu.",	_XO(31,RD,RA,RB,0,459,1));
	gen_xer_values(0, SO|OV);
	TEST_INSTRUCTION(RRR__,"divwo",		_XO(31,RD,RA,RB,1,491,0));
	TEST_INSTRUCTION(RRR__,"divwo.",	_XO(31,RD,RA,RB,1,491,1));
	TEST_INSTRUCTION(RRR__,"divwuo",	_XO(31,RD,RA,RB,1,459,0));
	TEST_INSTRUCTION(RRR__,"divwuo.",	_XO(31,RD,RA,RB,1,459,1));
#endif
}

void powerpc_test_cpu::test_logical(void)
{
#if TEST_LOGICAL
	gen_xer_values(0, SO);
	TEST_INSTRUCTION(RRR__,"and",		_X (31,RA,RD,RB,28,0));
	TEST_INSTRUCTION(RRR__,"and.",		_X (31,RA,RD,RB,28,1));
	TEST_INSTRUCTION(RRR__,"andc",		_X (31,RA,RD,RB,60,0));
	TEST_INSTRUCTION(RRR__,"andc.",		_X (31,RA,RD,RB,60,1));
	TEST_INSTRUCTION(RRK__,"andi.",		_D (28,RA,RD,00));
	TEST_INSTRUCTION(RRK__,"andis.",	_D (29,RA,RD,00));
	TEST_INSTRUCTION(CNTLZ,"cntlzw",	_X (31,RA,RD,00,26,0));
	TEST_INSTRUCTION(CNTLZ,"cntlzw.",	_X (31,RA,RD,00,26,1));
	TEST_INSTRUCTION(RRR__,"eqv",		_X (31,RA,RD,RB,284,0));
	TEST_INSTRUCTION(RRR__,"eqv.",		_X (31,RA,RD,RB,284,1));
	TEST_INSTRUCTION(RR___,"extsb",		_X (31,RA,RD,00,954,0));
	TEST_INSTRUCTION(RR___,"extsb.",	_X (31,RA,RD,00,954,1));
	TEST_INSTRUCTION(RR___,"extsh",		_X (31,RA,RD,00,922,0));
	TEST_INSTRUCTION(RR___,"extsh.",	_X (31,RA,RD,00,922,1));
	TEST_INSTRUCTION(RRR__,"nand",		_X (31,RA,RD,RB,476,0));
	TEST_INSTRUCTION(RRR__,"nand.",		_X (31,RA,RD,RB,476,1));
	TEST_INSTRUCTION(RR___,"neg",		_XO(31,RD,RA,RB,0,104,0));
	TEST_INSTRUCTION(RR___,"neg.",		_XO(31,RD,RA,RB,0,104,1));
	TEST_INSTRUCTION(RRR__,"nor",		_X (31,RA,RD,RB,124,0));
	TEST_INSTRUCTION(RRR__,"nor.",		_X (31,RA,RD,RB,124,1));
	TEST_INSTRUCTION(RRR__,"or",		_X (31,RA,RD,RB,444,0));
	TEST_INSTRUCTION(RRR__,"or.",		_X (31,RA,RD,RB,444,1));
	TEST_INSTRUCTION(RRR__,"orc",		_X (31,RA,RD,RB,412,0));
	TEST_INSTRUCTION(RRR__,"orc.",		_X (31,RA,RD,RB,412,1));
	TEST_INSTRUCTION(RRK__,"ori",		_D (24,RA,RD,00));
	TEST_INSTRUCTION(RRK__,"oris",		_D (25,RA,RD,00));
	TEST_INSTRUCTION(RRR__,"xor",		_X (31,RA,RD,RB,316,0));
	TEST_INSTRUCTION(RRR__,"xor.",		_X (31,RA,RD,RB,316,1));
	TEST_INSTRUCTION(RRK__,"xori",		_D (26,RA,RD,00));
	TEST_INSTRUCTION(RRK__,"xoris",		_D (27,RA,RD,00));
	gen_xer_values(0, SO|OV);
	TEST_INSTRUCTION(RR___,"nego",		_XO(31,RD,RA,RB,1,104,0));
	TEST_INSTRUCTION(RR___,"nego.",		_XO(31,RD,RA,RB,1,104,1));
#endif
}

void powerpc_test_cpu::test_shift(void)
{
#if TEST_SHIFT
	gen_xer_values(0, SO);
	TEST_INSTRUCTION(RRRSH,"slw",		_X (31,RA,RD,RB, 24,0));
	TEST_INSTRUCTION(RRRSH,"slw.",		_X (31,RA,RD,RB, 24,1));
	TEST_INSTRUCTION(RRRSH,"sraw",		_X (31,RA,RD,RB,792,0));
	TEST_INSTRUCTION(RRRSH,"sraw.",		_X (31,RA,RD,RB,792,1));
	TEST_INSTRUCTION(RRS__,"srawi",		_X (31,RA,RD,00,824,0));
	TEST_INSTRUCTION(RRS__,"srawi.",	_X (31,RA,RD,00,824,1));
	TEST_INSTRUCTION(RRRSH,"srw",		_X (31,RA,RD,RB,536,0));
	TEST_INSTRUCTION(RRRSH,"srw.",		_X (31,RA,RD,RB,536,1));
#endif
}

void powerpc_test_cpu::test_rotate(void)
{
#if TEST_ROTATE
	gen_xer_values(0, SO);
	TEST_INSTRUCTION(RRIII,"rlwimi",	_M (20,RA,RD,00,00,00,0));
	TEST_INSTRUCTION(RRIII,"rlwimi.",	_M (20,RA,RD,00,00,00,1));
	TEST_INSTRUCTION(RRIII,"rlwinm",	_M (21,RA,RD,00,00,00,0));
	TEST_INSTRUCTION(RRIII,"rlwinm.",	_M (21,RA,RD,00,00,00,1));
	TEST_INSTRUCTION(RRRII,"rlwnm",		_M (23,RA,RD,RB,00,00,0));
	TEST_INSTRUCTION(RRRII,"rlwnm.",	_M (23,RA,RD,RB,00,00,1));
#endif
}

void powerpc_test_cpu::test_compare(void)
{
#if TEST_COMPARE
	gen_xer_values(0, SO);
	TEST_INSTRUCTION(CRR__,"cmp",		_X (31,00,RA,RB,000,0));
	TEST_INSTRUCTION(CRI__,"cmpi",		_D (11,00,RA,00));
	TEST_INSTRUCTION(CRR__,"cmpl",		_X (31,00,RA,RB, 32,0));
	TEST_INSTRUCTION(CRK__,"cmpli",		_D (10,00,RA,00));
#endif
}

void powerpc_test_cpu::test_cr_logical(void)
{
#if TEST_CR_LOGICAL
	gen_xer_values(0, SO);
	TEST_INSTRUCTION(CCC__,"crand",		_X (19,00,00,00,257,0));
	TEST_INSTRUCTION(CCC__,"crandc",	_X (19,00,00,00,129,0));
	TEST_INSTRUCTION(CCC__,"creqv",		_X (19,00,00,00,289,0));
	TEST_INSTRUCTION(CCC__,"crnand",	_X (19,00,00,00,225,0));
	TEST_INSTRUCTION(CCC__,"crnor",		_X (19,00,00,00, 33,0));
	TEST_INSTRUCTION(CCC__,"cror",		_X (19,00,00,00,449,0));
	TEST_INSTRUCTION(CCC__,"crorc",		_X (19,00,00,00,417,0));
	TEST_INSTRUCTION(CCC__,"crxor",		_X (19,00,00,00,193,0));
#endif
}

bool powerpc_test_cpu::test(void)
{
	// Tests initialization
	tests = errors = 0;
	init_cr = init_xer = 0;

	// Execution tests
	test_add();
	test_sub();
	test_mul();
	test_div();
	test_shift();
	test_rotate();
	test_logical();
	test_compare();
	test_cr_logical();

	printf("%u errors out of %u tests\n", errors, tests);
	return errors != 0;
}

int main(int argc, char *argv[])
{
	FILE *fp = NULL;
	powerpc_test_cpu ppc;

	if (argc > 1) {
		const char *file = argv[1];
#if defined(__powerpc__)
		if ((fp = fopen(file, "w")) == NULL) {
			fprintf(stderr, "ERROR: can't open %s for writing\n", file);
			return EXIT_FAILURE;
		}
#else
		if ((fp = fopen(file, "r")) == NULL) {
			fprintf(stderr, "ERROR: can't open %s for reading\n", file);
			return EXIT_FAILURE;
		}
#endif
		ppc.set_results_file(fp);

		// Use a large enough buffer
		static char buffer[4096];
		setvbuf(fp, buffer, _IOFBF, sizeof(buffer));
	}

	// We need a results file on non PowerPC platforms
#if !defined(__powerpc__)
	if (fp == NULL) {
		fprintf(stderr, "ERROR: a results file for reference is required\n");
		return EXIT_FAILURE;
	}
#endif

	int ret = ppc.test();
	if (fp) fclose(fp);
	return !ret;
}
