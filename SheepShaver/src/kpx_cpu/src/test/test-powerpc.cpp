/*
 *  test-powerpc.cpp - PowerPC regression testing
 *
 *  Kheperix (C) 2003-2005 Gwenole Beauchesne
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

// NOTE: Results file md5sum: 3e29432abb6e21e625a2eef8cf2f0840 ($Revision$)

#include <vector>
#include <limits>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <netinet/in.h> // ntohl(), htonl()
#include <setjmp.h>
#include <signal.h>
#include <ctype.h>
#include <math.h>

#if defined(__powerpc__) || defined(__ppc__)
#define NATIVE_POWERPC
#endif

#if EMU_KHEPERIX
#include "sysdeps.h"
#include "vm_alloc.h"
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

#if EMU_QEMU
extern "C" {
#include "target-ppc/cpu.h"
extern void tb_flush();
}
typedef uint32_t uint32;
typedef uintptr_t uintptr;
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

// Define units to skip during testing
#define SKIP_ALU_OPS	0
#define SKIP_FPU_OPS	0
#define SKIP_VMX_OPS	0

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
#define TEST_VMX_LOADSH	1
#define TEST_VMX_LOAD	1
#define TEST_VMX_ARITH	1


// Partial PowerPC runtime assembler from GNU lightning
#undef  _I
#define _I(X)			((uint32)(X))
#undef  _UL
#define _UL(X)			((uint32)(X))
#undef  _MASK
#define _MASK(N)		((uint32)((1<<(N)))-1)
#undef  _ck_s
#define _ck_s(W,I)		(_UL(I) & _MASK(W))
#undef  _ck_u
#define _ck_u(W,I)    	(_UL(I) & _MASK(W))
#undef  _ck_su
#define _ck_su(W,I)    	(_UL(I) & _MASK(W))
#undef  _u1
#define _u1(I)          _ck_u( 1,I)
#undef  _u5
#define _u5(I)          _ck_u( 5,I)
#undef  _u6
#define _u6(I)          _ck_u( 6,I)
#undef  _u9
#define _u9(I)          _ck_u( 9,I)
#undef  _u10
#define _u10(I)         _ck_u(10,I)
#undef  _u11
#define _u11(I)			_ck_u(11,I)
#undef  _s16
#define _s16(I)         _ck_s(16,I)

#undef  _D
#define _D(   OP,RD,RA,         DD )  	_I((_u6(OP)<<26)|(_u5(RD)<<21)|(_u5(RA)<<16)|                _s16(DD)                          )
#undef  _X
#define _X(   OP,RD,RA,RB,   XO,RC )  	_I((_u6(OP)<<26)|(_u5(RD)<<21)|(_u5(RA)<<16)|( _u5(RB)<<11)|              (_u10(XO)<<1)|_u1(RC))
#undef  _XO
#define _XO(  OP,RD,RA,RB,OE,XO,RC )  	_I((_u6(OP)<<26)|(_u5(RD)<<21)|(_u5(RA)<<16)|( _u5(RB)<<11)|(_u1(OE)<<10)|( _u9(XO)<<1)|_u1(RC))
#undef  _M
#define _M(   OP,RS,RA,SH,MB,ME,RC )  	_I((_u6(OP)<<26)|(_u5(RS)<<21)|(_u5(RA)<<16)|( _u5(SH)<<11)|(_u5(MB)<< 6)|( _u5(ME)<<1)|_u1(RC))
#undef  _VX
#define _VX(  OP,VD,VA,VB,   XO    )	_I((_u6(OP)<<26)|(_u5(VD)<<21)|(_u5(VA)<<16)|( _u5(VB)<<11)|               _u11(XO)            )
#undef  _VXR
#define _VXR( OP,VD,VA,VB,   XO,RC )	_I((_u6(OP)<<26)|(_u5(VD)<<21)|(_u5(VA)<<16)|( _u5(VB)<<11)|              (_u1(RC)<<10)|_u10(XO))
#undef  _VA
#define _VA(  OP,VD,VA,VB,VC,XO    )	_I((_u6(OP)<<26)|(_u5(VD)<<21)|(_u5(VA)<<16)|( _u5(VB)<<11)|(_u5(VC)<< 6)|  _u6(XO)            )

// PowerPC opcodes
static inline uint32 POWERPC_LI(int RD, uint32 v) { return _D(14,RD,00,(v&0xffff)); }
static inline uint32 POWERPC_MR(int RD, int RA) { return _X(31,RA,RD,RA,444,0); }
static inline uint32 POWERPC_MFCR(int RD) { return _X(31,RD,00,00,19,0); }
static inline uint32 POWERPC_LVX(int vD, int rA, int rB) { return _X(31,vD,rA,rB,103,0); }
static inline uint32 POWERPC_STVX(int vS, int rA, int rB) { return _X(31,vS,rA,rB,231,0); }
static inline uint32 POWERPC_MFSPR(int rD, int SPR) { return _X(31,rD,(SPR&0x1f),((SPR>>5)&0x1f),339,0); }
static inline uint32 POWERPC_MTSPR(int rS, int SPR) { return _X(31,rS,(SPR&0x1f),((SPR>>5)&0x1f),467,0); }
const uint32 POWERPC_NOP = 0x60000000;
const uint32 POWERPC_BLR = 0x4e800020;
const uint32 POWERPC_BLRL = 0x4e800021;
const uint32 POWERPC_ILLEGAL = 0x00000000;
const uint32 POWERPC_EMUL_OP = 0x18000000;

// Invalidate test cache
#ifdef NATIVE_POWERPC
static void inline ppc_flush_icache_range(uint32 *start_p, uint32 length)
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
static void inline ppc_flush_icache_range(uint32 *start_p, uint32 length)
{
}
#endif

#if EMU_KHEPERIX
// Wrappers when building from SheepShaver tree
#ifdef SHEEPSHAVER
uint32 ROMBase = 0x40800000;
int64 TimebaseSpeed = 25000000;	// Default:  25 MHz
uint32 PVR = 0x000c0000;		// Default: 7400 (with AltiVec)

bool PrefsFindBool(const char *name)
{
	return false;
}

uint64 GetTicks_usec(void)
{
	return clock();
}

void HandleInterrupt(powerpc_registers *)
{
}

#if PPC_ENABLE_JIT && PPC_REENTRANT_JIT
void init_emul_op_trampolines(basic_dyngen & dg)
{
}
#endif
#endif

struct powerpc_cpu_base
	: public powerpc_cpu
{
	powerpc_cpu_base();
	void init_decoder();
	void execute_return(uint32 opcode);
	void invalidate_cache_range(uint32 *start, uint32 size)
		{ powerpc_cpu::invalidate_cache_range((uintptr)start, ((uintptr)start) + size); }

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
#ifndef SHEEPSHAVER
	: powerpc_cpu(NULL)
#endif
{
	init_decoder();
}

void powerpc_cpu_base::execute_return(uint32 opcode)
{
	spcflags().set(SPCFLAG_CPU_EXEC_RETURN);
}

void powerpc_cpu_base::init_decoder()
{
	static const instr_info_t return_ii_table[] = {
		{ "return",
		  (execute_pmf)&powerpc_cpu_base::execute_return,
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
	void enable_jit() { }
	void invalidate_cache() { }
	void invalidate_cache_range(uint32 *start, uint32 size) { }

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
	void enable_jit() { }
	void invalidate_cache() { }
	void invalidate_cache_range(uint32 *start, uint32 size) { }

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

#if EMU_QEMU
class powerpc_cpu_base
{
	CPUPPCState *ppc;
public:
	powerpc_cpu_base();
	~powerpc_cpu_base();
	void execute(uintptr);
	void enable_jit() { }
	void invalidate_cache() { tb_flush(); }
	void invalidate_cache_range(uint32 *start, uint32 size) { invalidate_cache(); }

	uint32 emul_get_xer() const;
	void emul_set_xer(uint32 value);
	uint32 emul_get_cr() const;
	void emul_set_cr(uint32 value);
	uint32 get_lr() const				{ return ppc->LR; }
	void set_lr(uint32 value)			{ ppc->LR = value; }
	uint32 get_gpr(int i) const			{ return ppc->gpr[i]; }
	void set_gpr(int i, uint32 value)	{ ppc->gpr[i] = value; }
};

uint32 powerpc_cpu_base::emul_get_xer() const
{
	uint32 xer = 0;
	for (int i = 0; i < 32; i++)
		xer |= ppc->xer[i] << i;
	return xer;
}

void powerpc_cpu_base::emul_set_xer(uint32 value)
{
	for (int i = 0; i < 32; i++)
		ppc->xer[i] = (value >> i) & 1;
}

uint32 powerpc_cpu_base::emul_get_cr() const
{
	uint32 cr = 0;
	for (int i = 0; i < 8; i++)
		cr |= (ppc->crf[i] & 15) << (28 - 4 * i);
	return cr;
}

void powerpc_cpu_base::emul_set_cr(uint32 value)
{
	for (int i = 0; i < 8; i++)
		ppc->crf[i] = (value >> (28 - 4 * i)) & 15;
}

powerpc_cpu_base::powerpc_cpu_base()
{
	ppc = cpu_ppc_init();
}

powerpc_cpu_base::~powerpc_cpu_base()
{
	cpu_ppc_close(ppc);
}

void powerpc_cpu_base::execute(uintptr entry_point)
{
	ppc->nip = entry_point;
	cpu_exec(ppc);
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

// Vector registers
typedef bit_field< 11, 15 > vA_field;
typedef bit_field< 16, 20 > vB_field;
typedef bit_field< 21, 25 > vC_field;
typedef bit_field<  6, 10 > vD_field;
typedef bit_field<  6, 10 > vS_field;
typedef bit_field< 22, 25 > vSH_field;

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

// Flag: does the host support AltiVec instructions?
static bool has_altivec = true;

// A 128-bit AltiVec register
typedef uint8 vector_t[16];

class aligned_vector_t {
	struct {
		vector_t v;
		uint8 pad[16];
	} vs;
public:
	aligned_vector_t()
		{ clear(); }
	void clear()
		{ memset(addr(), 0, sizeof(vector_t)); }
	void copy(vector_t const & vi, int n = sizeof(vector_t))
		{ clear(); memcpy(addr(), &vi, n); }
	vector_t *addr() const
		{ return (vector_t *)(((char *)&vs.v) + (16 - (((uintptr)&vs.v) % 16))); }
	vector_t const & value() const
		{ return *addr(); }
	vector_t & value()
		{ return *addr(); }
};

union vector_helper_t {
	vector_t v;
	uint8	b[16];
	uint16	h[8];
	uint32	w[4];
	float	f[4];
};

static void print_vector(vector_t const & v, char type = 'b')
{
	vector_helper_t x;
	memcpy(&x.b, &v, sizeof(vector_t));

	printf("{");
	switch (type) {
	case 'b':
	default:
		for (int i = 0; i < 16; i++) {
			if (i != 0)
				printf(",");
			printf(" %02x", x.b[i]);
		}
		break;
	case 'h':
		for (int i = 0; i < 8; i++) {
			if (i != 0)
				printf(",");
			printf(" %04x", x.h[i]);
		}
		break;
	case 'w':
		for (int i = 0; i < 4; i++) {
			if (i != 0)
				printf(",");
			printf(" %08x", x.w[i]);
		}
		break;
	case 'f':
	case 'e': // estimate result
	case 'l': // estimate log2 result
		for (int i = 0; i < 4; i++) {
			x.w[i] = ntohl(x.w[i]);
			if (i != 0)
				printf(",");
			printf(" %g", x.f[i]);
		}
		break;
	}
	printf(" }");
}

static inline bool do_float_equals(float a, float b, float tolerance)
{
	if (a == b)
		return true;

	if (isnan(a) && isnan(b))
		return true;

	if (isinf(a) && isinf(b) && signbit(a) == signbit(b))
		return true;

	if ((b < (a + tolerance)) && (b > (a - tolerance)))
		return true;

	return false;
}

static inline bool float_equals(float a, float b)
{
	return do_float_equals(a, b, 3 * std::numeric_limits<float>::epsilon());
}

static bool vector_equals(char type, vector_t const & a, vector_t const & b)
{
	// the vector is in ppc big endian format
	float tolerance;
	switch (type) {
	case 'f':
		tolerance = 3 * std::numeric_limits<float>::epsilon();
		goto do_compare;
	case 'l': // FIXME: this does not handle |x-1|<=1/8 case
		tolerance = 1. / 32.;
		goto do_compare;
	case 'e':
		tolerance = 1. / 4096.;
	  do_compare:
		for (int i = 0; i < 4; i++) {
			union { float f; uint32 i; } u, v;
			u.i = ntohl(((uint32 *)&a)[i]);
			v.i = ntohl(((uint32 *)&b)[i]);
			if (!do_float_equals(u.f, v.f, tolerance))
				return false;
		}
		return true;
	}

	return memcmp(&a, &b, sizeof(vector_t)) == 0;
}

static bool vector_all_eq(char type, vector_t const & b)
{
	uint32 v;
	vector_helper_t x;
	memcpy(&x.v, &b, sizeof(vector_t));

	bool all_eq = true;
	switch (type) {
	case 'b':
	default:
		v = x.b[0];
		for (int i = 1; all_eq && i < 16; i++)
			if (x.b[i] != v)
				all_eq = false;
		break;
	case 'h':
		v = x.h[0];
		for (int i = 1; all_eq && i < 8; i++)
			if (x.h[i] != v)
				all_eq = false;
		break;
	case 'w':
	case 'f':
		v = x.w[0];
		for (int i = 1; all_eq && i < 4; i++)
			if (x.w[i] != v)
				all_eq = false;
		break;
	}
	return all_eq;
}

// Define PowerPC tester
class powerpc_test_cpu
	: public powerpc_cpu_base
{
#ifdef NATIVE_POWERPC
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
		{ invalidate_cache_range(start, size); ppc_flush_icache_range(start, size); }

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
	void get_vector(vector_t & v);
	void put_vector(vector_t const & v);

	// Initial CR0, XER states
	uint32 init_cr;
	uint32 init_xer;

	// XER preset values to test with
	std::vector<uint32> xer_values;
	void gen_xer_values(uint32 use_mask, uint32 set_mask);

	// Emulated registers IDs
	enum {
		RD = 3,
		RA = 4,
		RB = 5,
		RC = 6,
		VSCR = 7,
	};

	// Operands
	enum {
		__,
		vD, vS, vA, vB, vC, vI, vN,
		rD, rS, rA, rB, rC,
	};

	struct vector_test_t {
		uint8	name[14];
		char	type;
		char	op_type;
		uint32	opcode;
		uint8	operands[4];
	};

	struct vector_value_t {
		char type;
		vector_t v;
	};

	static const uint32 reg_values[];
	static const uint32 imm_values[];
	static const uint32 msk_values[];
	static const vector_value_t vector_values[];
	static const vector_value_t vector_fp_values[];

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

	void test_one_vector(uint32 *code, vector_test_t const & vt, uint8 *rA, uint8 *rB = 0, uint8 *rC = 0);
	void test_one_vector(uint32 *code, vector_test_t const & vt, vector_t const *vA = 0, vector_t const *vB = 0, vector_t const *vC = 0)
		{ test_one_vector(code, vt, (uint8 *)vA, (uint8 *)vB, (uint8 *)vC); }
	void test_vector_load(void);
	void test_vector_load_for_shift(void);
	void test_vector_arith(void);
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

void powerpc_test_cpu::get_vector(vector_t & v)
{
	if (fread(&v, sizeof(v), 1, results_file) != 1) {
		fprintf(stderr, "ERROR: unexpected end of results file\n");
		exit(EXIT_FAILURE);
	}
}

void powerpc_test_cpu::put_vector(vector_t const & v)
{
	if (fwrite(&v, sizeof(v), 1, results_file) != 1) {
		fprintf(stderr, "could not write vector to results file\n");
		exit(EXIT_FAILURE);
	}
}

void powerpc_test_cpu::execute(uint32 *code_p)
{
	static uint32 code[2];
	code[0] = htonl(POWERPC_BLRL);
	code[1] = htonl(POWERPC_EMUL_OP);

#ifndef NATIVE_POWERPC
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
#ifdef NATIVE_POWERPC
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

	if (SKIP_ALU_OPS)
		return;

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

	for (uint32 mask = 0x80000000; mask != 0; mask >>= 1) {
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

// Template-generated vector values
const powerpc_test_cpu::vector_value_t powerpc_test_cpu::vector_values[] = {
	{'w',{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{'w',{0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01}},
	{'w',{0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02}},
	{'w',{0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03}},
	{'w',{0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04}},
	{'w',{0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05}},
	{'w',{0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06}},
	{'w',{0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07}},
	{'w',{0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08}},
	{'w',{0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10}},
	{'w',{0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18}},
	{'w',{0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20}},
	{'w',{0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28}},
	{'w',{0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30}},
	{'w',{0x38,0x38,0x38,0x38,0x38,0x38,0x38,0x38,0x38,0x38,0x38,0x38,0x38,0x38,0x38,0x38}},
	{'w',{0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40}},
	{'w',{0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48,0x48}},
	{'w',{0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50}},
	{'w',{0x58,0x58,0x58,0x58,0x58,0x58,0x58,0x58,0x58,0x58,0x58,0x58,0x58,0x58,0x58,0x58}},
	{'w',{0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60}},
	{'w',{0x68,0x68,0x68,0x68,0x68,0x68,0x68,0x68,0x68,0x68,0x68,0x68,0x68,0x68,0x68,0x68}},
	{'w',{0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70}},
	{'w',{0x78,0x78,0x78,0x78,0x78,0x78,0x78,0x78,0x78,0x78,0x78,0x78,0x78,0x78,0x78,0x78}},
	{'w',{0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00}},
	{'w',{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04}},
	{'w',{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10}},
	{'w',{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}},
	{'w',{0x11,0x11,0x11,0x11,0x22,0x22,0x22,0x22,0x33,0x33,0x33,0x33,0x44,0x44,0x44,0x44}},
	{'w',{0x88,0x88,0x88,0x88,0x77,0x77,0x77,0x77,0x66,0x66,0x66,0x66,0x55,0x55,0x55,0x55}},
	{'w',{0x99,0x99,0x99,0x99,0xaa,0xaa,0xaa,0xaa,0xbb,0xbb,0xbb,0xbb,0xcc,0xcc,0xcc,0xcc}},
	{'w',{0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xee,0xee,0xee,0xee,0xdd,0xdd,0xdd,0xdd}},
	{'w',{0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff}},
	{'h',{0x00,0x00,0x11,0x11,0x22,0x22,0x33,0x33,0x44,0x44,0x55,0x55,0x66,0x66,0x77,0x77}},
	{'h',{0x00,0x01,0x00,0x02,0x00,0x03,0x00,0x04,0x00,0x05,0x00,0x06,0x00,0x07,0x00,0x08}},
	{'h',{0x00,0x16,0x00,0x15,0x00,0x14,0x00,0x13,0x00,0x12,0x00,0x10,0x00,0x10,0x00,0x09}},
	{'b',{0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff}},
	{'b',{0xff,0xee,0xdd,0xcc,0xbb,0xaa,0x99,0x88,0x77,0x66,0x55,0x44,0x33,0x22,0x11,0x00}},
	{'b',{0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f}},
	{'b',{0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f}},
	{'b',{0x2f,0x2e,0x2d,0x2c,0x2b,0x2a,0x29,0x28,0x27,0x26,0x25,0x24,0x23,0x22,0x21,0x20}}
};

const powerpc_test_cpu::vector_value_t powerpc_test_cpu::vector_fp_values[] = {
	{'f',{0x80,0x00,0x00,0x00,0x80,0x00,0x00,0x00,0x80,0x00,0x00,0x00,0x80,0x00,0x00,0x00}}, // -0, -0, -0, -0
	{'f',{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}}, // 0, 0, 0, 0
	{'f',{0xbf,0x80,0x00,0x00,0xbf,0x80,0x00,0x00,0xbf,0x80,0x00,0x00,0xbf,0x80,0x00,0x00}}, // -1, -1, -1, -1
	{'f',{0x3f,0x80,0x00,0x00,0x3f,0x80,0x00,0x00,0x3f,0x80,0x00,0x00,0x3f,0x80,0x00,0x00}}, // 1, 1, 1, 1
	{'f',{0xc0,0x00,0x00,0x00,0xc0,0x00,0x00,0x00,0xc0,0x00,0x00,0x00,0xc0,0x00,0x00,0x00}}, // -2, -2, -2, -2
	{'f',{0x40,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x40,0x00,0x00,0x00}}, // 2, 2, 2, 2
	{'f',{0xc0,0x00,0x00,0x00,0xbf,0x80,0x00,0x00,0x3f,0x80,0x00,0x00,0x40,0x00,0x00,0x00}}, // -2, -1, 1, 2
	{'f',{0xc0,0x40,0x00,0x00,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x40,0x00,0x00}}, // -3, -0, 0, 3
	{'f',{0x40,0x00,0x00,0x00,0x3f,0x80,0x00,0x00,0xbf,0x80,0x00,0x00,0xc0,0x00,0x00,0x00}}  // 2, 1, -1, -2
};

void powerpc_test_cpu::test_one_vector(uint32 *code, vector_test_t const & vt, uint8 *rAp, uint8 *rBp, uint8 *rCp)
{
#if TEST_VMX_OPS
	static vector_t native_vD;
	memset(&native_vD, 0, sizeof(native_vD));
	static vector_helper_t native_vSCR;
	memset(&native_vSCR, 0, sizeof(native_vSCR));
	static aligned_vector_t dummy_vector;
	dummy_vector.clear();
	if (!rAp) rAp = (uint8 *)dummy_vector.addr();
	if (!rBp) rBp = (uint8 *)dummy_vector.addr();
	if (!rCp) rCp = (uint8 *)dummy_vector.addr();
#ifdef NATIVE_POWERPC
	// Invoke native code
	const uint32 save_cr = native_get_cr();
	native_set_cr(init_cr);
	native_vSCR.w[3] = 0;
	typedef void (*func_t)(uint8 *, uint8 *, uint8 *, uint8 *, uint8 *);
	func_t func = (func_t)code;
	func((uint8 *)&native_vD, rAp, rBp, rCp, native_vSCR.b);
	const uint32 native_cr = native_get_cr();
	const uint32 native_vscr = native_vSCR.w[3];
	native_set_cr(save_cr);
	if (results_file) {
		put_vector(native_vD);
		put32(native_cr);
		put32(native_vscr);
	}
#else
	get_vector(native_vD);
	const uint32 native_cr = get32();
	const uint32 native_vscr = get32();
#endif

	if (SKIP_VMX_OPS)
		return;

	// Invoke emulated code
	static aligned_vector_t emul_vD;
	emul_vD.clear();
	static aligned_vector_t emul_vSCR;
	emul_vSCR.clear();
	emul_set_cr(init_cr);
	set_gpr(RD, (uintptr)emul_vD.addr());
	set_gpr(RA, (uintptr)rAp);
	set_gpr(RB, (uintptr)rBp);
	set_gpr(RC, (uintptr)rCp);
	set_gpr(VSCR, (uintptr)emul_vSCR.addr());
	execute(code);
	vector_helper_t emul_vSCR_helper;
	memcpy(&emul_vSCR_helper, emul_vSCR.addr(), sizeof(vector_t));
	const uint32 emul_cr = emul_get_cr();
	const uint32 emul_vscr = ntohl(emul_vSCR_helper.w[3]);

	++tests;

	bool ok = vector_equals(vt.type, native_vD, emul_vD.value())
		&& native_cr == emul_cr
		&& native_vscr == emul_vscr;

	if (!ok) {
		printf("FAIL: %s [%08x]\n", vt.name, vt.opcode);
		errors++;
	}
	else if (verbose) {
		printf("PASS: %s [%08x]\n", vt.name, vt.opcode);
	}

	if (!ok || verbose) {
#if ENABLE_MON
		disass_ppc(stdout, (uintptr)code, vt.opcode);
#endif
		char op_type = tolower(vt.op_type);
		if (!op_type)
			op_type = vt.type;
#define PRINT_OPERAND(N, vX, rX)									\
		switch (vt.operands[N]) {									\
		case vX:													\
			printf(#vX " = ");										\
			print_vector(*((vector_t *)rX##p));						\
			printf("\n");											\
			break;													\
		case vI:													\
		case vN:													\
			printf(#vX " = %d\n", vX##_field::extract(vt.opcode));	\
			break;													\
		case rX:													\
			printf(#rX " = %08x", rX##p);							\
			if (rX##p) switch (op_type) {							\
			case 'b': printf(" [%02x]", *rX##p); break;				\
			case 'h': printf(" [%04x]", *((uint16 *)rX##p)); break;	\
			case 'w': printf(" [%08x]", *((uint32 *)rX##p)); break;	\
			}														\
			printf("\n");											\
			break;													\
		}
		PRINT_OPERAND(1, vA, rA);
		PRINT_OPERAND(2, vB, rB);
		PRINT_OPERAND(3, vC, rC);
#undef  PRINT_OPERAND
		printf("vD.N = ");
		print_vector(native_vD, vt.type);
		printf("\n");
		printf("vD.E = ");
		print_vector(emul_vD.value(), vt.type);
		printf("\n");
		printf("CR.N = %08x ; VSCR.N = %08x\n", native_cr, native_vscr);
		printf("CR.E = %08x ; VSCR.E = %08x\n", emul_cr, emul_vscr);
	}
#endif
}

void powerpc_test_cpu::test_vector_load_for_shift(void)
{
#if TEST_VMX_LOADSH
	// Tested instructions
	static const vector_test_t tests[] = {
		{ "lvsl",  'b', 0, _X (31,00,00,00,  6,0), { vD, rA, rB } },
		{ "lvsr",  'b', 0, _X (31,00,00,00, 38,0), { vD, rA, rB } },
	};

	// Code template
	static uint32 code[] = {
		POWERPC_MFSPR(12, 256),			// mfvrsave r12
		_D(15,0,0,0x1000),				// lis r0,0x1000 ([v3])
		POWERPC_MTSPR(0, 256),			// mtvrsave r0
		POWERPC_LI(RA, 0),				// li rB,<val>
		0,								// <insn>
		POWERPC_STVX(RD, 0, RD),		// stvx v3,r3(0)
		POWERPC_MTSPR(12, 256),			// mtvrsave r12
		POWERPC_BLR						// blr
	};

	int i_opcode = -1;
	const int n_instructions = sizeof(code) / sizeof(code[0]);
	for (int i = 0; i < n_instructions; i++) {
		if (code[i] == 0) {
			i_opcode = i;
			break;
		}
	}
	assert(i_opcode != -1);

	const int n_elements = sizeof(tests) / sizeof(tests[0]);
	for (int i = 0; i < n_elements; i++) {
		vector_test_t const & vt = tests[i];
		code[i_opcode] = vt.opcode;
		vD_field::insert(code[i_opcode], RD);
		rA_field::insert(code[i_opcode], 00);
		rB_field::insert(code[i_opcode], RA);

		printf("Testing %s\n", vt.name);
		for (int j = 0; j < 32; j++) {
			UIMM_field::insert(code[i_opcode - 1], j);
			flush_icache_range(code, sizeof(code));
			test_one_vector(code, vt, (uint8 *)NULL);
		}
	}
#endif
}

void powerpc_test_cpu::test_vector_load(void)
{
#if TEST_VMX_LOAD
	// Tested instructions
	static const vector_test_t tests[] = {
		{ "lvebx",  'b', 0, _X (31,00,00,00,  7,0), { vD, rA, rB } },
		{ "lvehx",  'h', 0, _X (31,00,00,00, 39,0), { vD, rA, rB } },
		{ "lvewx",  'w', 0, _X (31,00,00,00, 71,0), { vD, rA, rB } }
	};

	// Code template
	static uint32 code[] = {
		POWERPC_MFSPR(12, 256),			// mfvrsave r12
		_D(15,0,0,0x1000),				// lis r0,0x1000 ([v3])
		POWERPC_MTSPR(0, 256),			// mtvrsave r0
		POWERPC_LVX(RD, 0, RD),			// lvx v3,r3(0)
		0,								// <insn>
		POWERPC_STVX(RD, 0, RD),		// stvx v3,r3(0)
		POWERPC_MTSPR(12, 256),			// mtvrsave r12
		POWERPC_BLR						// blr
	};

	int i_opcode = -1;
	const int n_instructions = sizeof(code) / sizeof(code[0]);
	for (int i = 0; i < n_instructions; i++) {
		if (code[i] == 0) {
			i_opcode = i;
			break;
		}
	}
	assert(i_opcode != -1);

	const int n_elements = sizeof(tests) / sizeof(tests[0]);
	for (int i = 0; i < n_elements; i++) {
		vector_test_t const & vt = tests[i];
		code[i_opcode] = vt.opcode;
		vD_field::insert(code[i_opcode], RD);
		rA_field::insert(code[i_opcode], 00);
		rB_field::insert(code[i_opcode], RA);
		flush_icache_range(code, sizeof(code));

		printf("Testing %s\n", vt.name);
		const int n_vector_values = sizeof(vector_values)/sizeof(vector_values[0]);
		for (int j = 0; j < n_vector_values; j++) {
			static aligned_vector_t av;
			switch (vt.type) {
			case 'b':
				for (int k = 0; k < 16; k++) {
					av.copy(*(vector_t *)((uint8 *)(&vector_values[j].v) + 1 * k), 16 - 1 * k);
					test_one_vector(code, vt, av.addr());
				}
				break;
			case 'h':
				for (int k = 0; k < 8; k++) {
					av.copy(*(vector_t *)((uint8 *)(&vector_values[j].v) + 2 * k), 16 - 2 * k);
					test_one_vector(code, vt, av.addr());
				}
				break;
			case 'w':
				for (int k = 0; k < 4; k++) {
					av.copy(*(vector_t *)((uint8 *)(&vector_values[j].v) + 4 * k), 16 - 4 * k);
					test_one_vector(code, vt, av.addr());
				}
				break;
			}
		}
	}
#endif
}

void powerpc_test_cpu::test_vector_arith(void)
{
#if TEST_VMX_ARITH
	// Tested instructions
	static const vector_test_t tests[] = {
		{ "vaddcuw",	'w',  0 , _VX(04,RD,RA,RB, 384), { vD, vA, vB } },
		{ "vaddfp",		'f',  0 , _VX(04,RD,RA,RB,  10), { vD, vA, vB } },
		{ "vaddsbs",	'b',  0 , _VX(04,RD,RA,RB, 768), { vD, vA, vB } },
		{ "vaddshs",	'h',  0 , _VX(04,RD,RA,RB, 832), { vD, vA, vB } },
		{ "vaddsws",	'w',  0 , _VX(04,RD,RA,RB, 896), { vD, vA, vB } },
		{ "vaddubm",	'b',  0 , _VX(04,RD,RA,RB,   0), { vD, vA, vB } },
		{ "vaddubs",	'b',  0 , _VX(04,RD,RA,RB, 512), { vD, vA, vB } },
		{ "vadduhm",	'h',  0 , _VX(04,RD,RA,RB,  64), { vD, vA, vB } },
		{ "vadduhs",	'h',  0 , _VX(04,RD,RA,RB, 576), { vD, vA, vB } },
		{ "vadduwm",	'w',  0 , _VX(04,RD,RA,RB, 128), { vD, vA, vB } },
		{ "vadduws",	'w',  0 , _VX(04,RD,RA,RB, 640), { vD, vA, vB } },
		{ "vand",		'w',  0 , _VX(04,RD,RA,RB,1028), { vD, vA, vB } },
		{ "vandc",		'w',  0 , _VX(04,RD,RA,RB,1092), { vD, vA, vB } },
		{ "vavgsb",		'b',  0 , _VX(04,RD,RA,RB,1282), { vD, vA, vB } },
		{ "vavgsh",		'h',  0 , _VX(04,RD,RA,RB,1346), { vD, vA, vB } },
		{ "vavgsw",		'w',  0 , _VX(04,RD,RA,RB,1410), { vD, vA, vB } },
		{ "vavgub",		'b',  0 , _VX(04,RD,RA,RB,1026), { vD, vA, vB } },
		{ "vavguh",		'h',  0 , _VX(04,RD,RA,RB,1090), { vD, vA, vB } },
		{ "vavguw",		'w',  0 , _VX(04,RD,RA,RB,1154), { vD, vA, vB } },
		{ "vcfsx",		'f', 'w', _VX(04,RD,00,RB, 842), { vD, vI, vB } },
		{ "vcfux",		'f', 'w', _VX(04,RD,00,RB, 778), { vD, vI, vB } },
		{ "vcmpbfp",	'w', 'f', _VXR(04,RD,RA,RB,966,0), { vD, vA, vB } },
		{ "vcmpbfp.",	'w', 'f', _VXR(04,RD,RA,RB,966,1), { vD, vA, vB } },
		{ "vcmpeqfp",	'w', 'f', _VXR(04,RD,RA,RB,198,0), { vD, vA, vB } },
		{ "vcmpeqfp.",	'w', 'f', _VXR(04,RD,RA,RB,198,1), { vD, vA, vB } },
		{ "vcmpequb",	'b',  0 , _VXR(04,RD,RA,RB,  6,0), { vD, vA, vB } },
		{ "vcmpequb.",	'b',  0 , _VXR(04,RD,RA,RB,  6,1), { vD, vA, vB } },
		{ "vcmpequh",	'h',  0 , _VXR(04,RD,RA,RB, 70,0), { vD, vA, vB } },
		{ "vcmpequh.",	'h',  0 , _VXR(04,RD,RA,RB, 70,1), { vD, vA, vB } },
		{ "vcmpequw",	'w',  0 , _VXR(04,RD,RA,RB,134,0), { vD, vA, vB } },
		{ "vcmpequw.",	'w',  0 , _VXR(04,RD,RA,RB,134,1), { vD, vA, vB } },
		{ "vcmpgefp",	'w', 'f', _VXR(04,RD,RA,RB,454,0), { vD, vA, vB } },
		{ "vcmpgefp.",	'w', 'f', _VXR(04,RD,RA,RB,454,1), { vD, vA, vB } },
		{ "vcmpgtfp",	'w', 'f', _VXR(04,RD,RA,RB,710,0), { vD, vA, vB } },
		{ "vcmpgtfp.",	'w', 'f', _VXR(04,RD,RA,RB,710,1), { vD, vA, vB } },
		{ "vcmpgtsb",	'b',  0 , _VXR(04,RD,RA,RB,774,0), { vD, vA, vB } },
		{ "vcmpgtsb.",	'b',  0 , _VXR(04,RD,RA,RB,774,1), { vD, vA, vB } },
		{ "vcmpgtsh",	'h',  0 , _VXR(04,RD,RA,RB,838,0), { vD, vA, vB } },
		{ "vcmpgtsh.",	'h',  0 , _VXR(04,RD,RA,RB,838,1), { vD, vA, vB } },
		{ "vcmpgtsw",	'w',  0 , _VXR(04,RD,RA,RB,902,0), { vD, vA, vB } },
		{ "vcmpgtsw.",	'w',  0 , _VXR(04,RD,RA,RB,902,1), { vD, vA, vB } },
		{ "vcmpgtub",	'b',  0 , _VXR(04,RD,RA,RB,518,0), { vD, vA, vB } },
		{ "vcmpgtub.",	'b',  0 , _VXR(04,RD,RA,RB,518,1), { vD, vA, vB } },
		{ "vcmpgtuh",	'h',  0 , _VXR(04,RD,RA,RB,582,0), { vD, vA, vB } },
		{ "vcmpgtuh.",	'h',  0 , _VXR(04,RD,RA,RB,582,1), { vD, vA, vB } },
		{ "vcmpgtuw",	'w',  0 , _VXR(04,RD,RA,RB,646,0), { vD, vA, vB } },
		{ "vcmpgtuw.",	'w',  0 , _VXR(04,RD,RA,RB,646,1), { vD, vA, vB } },
		{ "vctsxs",		'w', 'f', _VX(04,RD,00,RB, 970), { vD, vI, vB } },
		{ "vctuxs",		'w', 'f', _VX(04,RD,00,RB, 906), { vD, vI, vB } },
		{ "vexptefp",	'f',  0 , _VX(04,RD,00,RB, 394), { vD, __, vB } },
		{ "vlogefp",	'l', 'f', _VX(04,RD,00,RB, 458), { vD, __, vB } },
		{ "vmaddfp",	'f',  0 , _VA(04,RD,RA,RB,RC,46),{ vD, vA, vB, vC } },
		{ "vmaxfp",		'f',  0 , _VX(04,RD,RA,RB,1034), { vD, vA, vB } },
		{ "vmaxsb",		'b',  0 , _VX(04,RD,RA,RB, 258), { vD, vA, vB } },
		{ "vmaxsh",		'h',  0 , _VX(04,RD,RA,RB, 322), { vD, vA, vB } },
		{ "vmaxsw",		'w',  0 , _VX(04,RD,RA,RB, 386), { vD, vA, vB } },
		{ "vmaxub",		'b',  0 , _VX(04,RD,RA,RB,   2), { vD, vA, vB } },
		{ "vmaxuh",		'h',  0 , _VX(04,RD,RA,RB,  66), { vD, vA, vB } },
		{ "vmaxuw",		'w',  0 , _VX(04,RD,RA,RB, 130), { vD, vA, vB } },
		{ "vmhaddshs",	'h',  0 , _VA(04,RD,RA,RB,RC,32),{ vD, vA, vB, vC } },
		{ "vmhraddshs",	'h',  0 , _VA(04,RD,RA,RB,RC,33),{ vD, vA, vB, vC } },
		{ "vminfp",		'f',  0 , _VX(04,RD,RA,RB,1098), { vD, vA, vB } },
		{ "vminsb",		'b',  0 , _VX(04,RD,RA,RB, 770), { vD, vA, vB } },
		{ "vminsh",		'h',  0 , _VX(04,RD,RA,RB, 834), { vD, vA, vB } },
		{ "vminsw",		'w',  0 , _VX(04,RD,RA,RB, 898), { vD, vA, vB } },
		{ "vminub",		'b',  0 , _VX(04,RD,RA,RB, 514), { vD, vA, vB } },
		{ "vminuh",		'h',  0 , _VX(04,RD,RA,RB, 578), { vD, vA, vB } },
		{ "vminuw",		'w',  0 , _VX(04,RD,RA,RB, 642), { vD, vA, vB } },
		{ "vmladduhm",	'h',  0 , _VA(04,RD,RA,RB,RC,34),{ vD, vA, vB, vC } },
		{ "vmrghb",		'b',  0 , _VX(04,RD,RA,RB,  12), { vD, vA, vB } },
		{ "vmrghh",		'h',  0 , _VX(04,RD,RA,RB,  76), { vD, vA, vB } },
		{ "vmrghw",		'w',  0 , _VX(04,RD,RA,RB, 140), { vD, vA, vB } },
		{ "vmrglb",		'b',  0 , _VX(04,RD,RA,RB, 268), { vD, vA, vB } },
		{ "vmrglh",		'h',  0 , _VX(04,RD,RA,RB, 332), { vD, vA, vB } },
		{ "vmrglw",		'w',  0 , _VX(04,RD,RA,RB, 396), { vD, vA, vB } },
		{ "vmsummbm",	'b',  0 , _VA(04,RD,RA,RB,RC,37),{ vD, vA, vB, vC } },
		{ "vmsumshm",	'h',  0 , _VA(04,RD,RA,RB,RC,40),{ vD, vA, vB, vC } },
		{ "vmsumshs",	'h',  0 , _VA(04,RD,RA,RB,RC,41),{ vD, vA, vB, vC } },
		{ "vmsumubm",	'b',  0 , _VA(04,RD,RA,RB,RC,36),{ vD, vA, vB, vC } },
		{ "vmsumuhm",	'h',  0 , _VA(04,RD,RA,RB,RC,38),{ vD, vA, vB, vC } },
		{ "vmsumuhs",	'h',  0 , _VA(04,RD,RA,RB,RC,39),{ vD, vA, vB, vC } },
		{ "vmulesb",	'b',  0 , _VX(04,RD,RA,RB, 776), { vD, vA, vB } },
		{ "vmulesh",	'h',  0 , _VX(04,RD,RA,RB, 840), { vD, vA, vB } },
		{ "vmuleub",	'b',  0 , _VX(04,RD,RA,RB, 520), { vD, vA, vB } },
		{ "vmuleuh",	'h',  0 , _VX(04,RD,RA,RB, 584), { vD, vA, vB } },
		{ "vmulosb",	'b',  0 , _VX(04,RD,RA,RB, 264), { vD, vA, vB } },
		{ "vmulosh",	'h',  0 , _VX(04,RD,RA,RB, 328), { vD, vA, vB } },
		{ "vmuloub",	'b',  0 , _VX(04,RD,RA,RB,   8), { vD, vA, vB } },
		{ "vmulouh",	'h',  0 , _VX(04,RD,RA,RB,  72), { vD, vA, vB } },
		{ "vnmsubfp",	'f',  0 , _VA(04,RD,RA,RB,RC,47),{ vD, vA, vB, vC } },
		{ "vnor",		'w',  0 , _VX(04,RD,RA,RB,1284), { vD, vA, vB } },
		{ "vor",		'w',  0 , _VX(04,RD,RA,RB,1156), { vD, vA, vB } },
		{ "vperm",		'b',  0 , _VA(04,RD,RA,RB,RC,43),{ vD, vA, vB, vC } },
		{ "vpkpx",		'h',  0 , _VX(04,RD,RA,RB, 782), { vD, vA, vB } },
		{ "vpkshss",	'b',  0 , _VX(04,RD,RA,RB, 398), { vD, vA, vB } },
		{ "vpkshus",	'b',  0 , _VX(04,RD,RA,RB, 270), { vD, vA, vB } },
		{ "vpkswss",	'h',  0 , _VX(04,RD,RA,RB, 462), { vD, vA, vB } },
		{ "vpkswus",	'h',  0 , _VX(04,RD,RA,RB, 334), { vD, vA, vB } },
		{ "vpkuhum",	'b',  0 , _VX(04,RD,RA,RB,  14), { vD, vA, vB } },
		{ "vpkuhus",	'b',  0 , _VX(04,RD,RA,RB, 142), { vD, vA, vB } },
		{ "vpkuwum",	'h',  0 , _VX(04,RD,RA,RB,  78), { vD, vA, vB } },
		{ "vpkuwus",	'h',  0 , _VX(04,RD,RA,RB, 206), { vD, vA, vB } },
		{ "vrefp",		'e', 'f', _VX(04,RD,00,RB, 266), { vD, __, vB } },
		{ "vrfim",		'f',  0 , _VX(04,RD,00,RB, 714), { vD, __, vB } },
		{ "vrfin",		'f',  0 , _VX(04,RD,00,RB, 522), { vD, __, vB } },
		{ "vrfip",		'f',  0 , _VX(04,RD,00,RB, 650), { vD, __, vB } },
		{ "vrfiz",		'f',  0 , _VX(04,RD,00,RB, 586), { vD, __, vB } },
		{ "vrlb",		'b',  0 , _VX(04,RD,RA,RB,   4), { vD, vA, vB } },
		{ "vrlh",		'h',  0 , _VX(04,RD,RA,RB,  68), { vD, vA, vB } },
		{ "vrlw",		'w',  0 , _VX(04,RD,RA,RB, 132), { vD, vA, vB } },
		{ "vrsqrtefp",	'e', 'f', _VX(04,RD,00,RB, 330), { vD, __, vB } },
		{ "vsel",		'b',  0 , _VA(04,RD,RA,RB,RC,42),{ vD, vA, vB, vC } },
		{ "vsl",		'b', 'B', _VX(04,RD,RA,RB, 452), { vD, vA, vB } },
		{ "vslb",		'b',  0 , _VX(04,RD,RA,RB, 260), { vD, vA, vB } },
		{ "vsldoi",		'b',  0 , _VA(04,RD,RA,RB,00,44),{ vD, vA, vB, vN } },
		{ "vslh",		'h',  0 , _VX(04,RD,RA,RB, 324), { vD, vA, vB } },
		{ "vslo",		'b',  0 , _VX(04,RD,RA,RB,1036), { vD, vA, vB } },
		{ "vslw",		'w',  0 , _VX(04,RD,RA,RB, 388), { vD, vA, vB } },
		{ "vspltb",		'b',  0 , _VX(04,RD,00,RB, 524), { vD, vI, vB } },
		{ "vsplth",		'h',  0 , _VX(04,RD,00,RB, 588), { vD, vI, vB } },
		{ "vspltisb",	'b',  0 , _VX(04,RD,00,00, 780), { vD, vI } },
		{ "vspltish",	'h',  0 , _VX(04,RD,00,00, 844), { vD, vI } },
		{ "vspltisw",	'w',  0 , _VX(04,RD,00,00, 908), { vD, vI } },
		{ "vspltw",		'w',  0 , _VX(04,RD,00,RB, 652), { vD, vI, vB } },
		{ "vsr",		'b', 'B', _VX(04,RD,RA,RB, 708), { vD, vA, vB } },
		{ "vsrab",		'b',  0 , _VX(04,RD,RA,RB, 772), { vD, vA, vB } },
		{ "vsrah",		'h',  0 , _VX(04,RD,RA,RB, 836), { vD, vA, vB } },
		{ "vsraw",		'w',  0 , _VX(04,RD,RA,RB, 900), { vD, vA, vB } },
		{ "vsrb",		'b',  0 , _VX(04,RD,RA,RB, 516), { vD, vA, vB } },
		{ "vsrh",		'h',  0 , _VX(04,RD,RA,RB, 580), { vD, vA, vB } },
		{ "vsro",		'b',  0 , _VX(04,RD,RA,RB,1100), { vD, vA, vB } },
		{ "vsrw",		'w',  0 , _VX(04,RD,RA,RB, 644), { vD, vA, vB } },
		{ "vsubcuw",	'w',  0 , _VX(04,RD,RA,RB,1408), { vD, vA, vB } },
		{ "vsubfp",		'f',  0 , _VX(04,RD,RA,RB,  74), { vD, vA, vB } },
		{ "vsubsbs",	'b',  0 , _VX(04,RD,RA,RB,1792), { vD, vA, vB } },
		{ "vsubshs",	'h',  0 , _VX(04,RD,RA,RB,1856), { vD, vA, vB } },
		{ "vsubsws",	'w',  0 , _VX(04,RD,RA,RB,1920), { vD, vA, vB } },
		{ "vsububm",	'b',  0 , _VX(04,RD,RA,RB,1024), { vD, vA, vB } },
		{ "vsububs",	'b',  0 , _VX(04,RD,RA,RB,1536), { vD, vA, vB } },
		{ "vsubuhm",	'h',  0 , _VX(04,RD,RA,RB,1088), { vD, vA, vB } },
		{ "vsubuhs",	'h',  0 , _VX(04,RD,RA,RB,1600), { vD, vA, vB } },
		{ "vsubuwm",	'w',  0 , _VX(04,RD,RA,RB,1152), { vD, vA, vB } },
		{ "vsubuws",	'w',  0 , _VX(04,RD,RA,RB,1664), { vD, vA, vB } },
		{ "vsum2sws",	'w',  0 , _VX(04,RD,RA,RB,1672), { vD, vA, vB } },
		{ "vsum4sbs",	'w',  0 , _VX(04,RD,RA,RB,1800), { vD, vA, vB } },
		{ "vsum4shs",	'w',  0 , _VX(04,RD,RA,RB,1608), { vD, vA, vB } },
		{ "vsum4ubs",	'w',  0 , _VX(04,RD,RA,RB,1544), { vD, vA, vB } },
		{ "vsumsws",	'w',  0 , _VX(04,RD,RA,RB,1928), { vD, vA, vB } },
		{ "vupkhpx",	'w',  0 , _VX(04,RD,00,RB, 846), { vD, __, vB } },
		{ "vupkhsb",	'h',  0 , _VX(04,RD,00,RB, 526), { vD, __, vB } },
		{ "vupkhsh",	'w',  0 , _VX(04,RD,00,RB, 590), { vD, __, vB } },
		{ "vupklpx",	'w',  0 , _VX(04,RD,00,RB, 974), { vD, __, vB } },
		{ "vupklsb",	'h',  0 , _VX(04,RD,00,RB, 654), { vD, __, vB } },
		{ "vupklsh",	'w',  0 , _VX(04,RD,00,RB, 718), { vD, __, vB } },
		{ "vxor",		'w',  0 , _VX(04,RD,RA,RB,1220), { vD, vA, vB } },
	};

	// Code template
	static uint32 code[] = {
		POWERPC_MFSPR(12, 256),			// mfvrsave	r12
		_D(15,0,0,0x9e00),				// lis		r0,0x9e00 ([v0;v3-v6])
		POWERPC_MTSPR(0, 256),			// mtvrsave	r0
		POWERPC_LVX(RA, 0, RA),			// lvx		v4,r4(0)
		POWERPC_LVX(RB, 0, RB),			// lvx		v5,r5(0)
		POWERPC_LVX(RC, 0, RC),			// lvx		v6,r6(0)
		POWERPC_LVX(0, 0, VSCR),		// lvx		v0,r7(0)
		_VX(04,00,00,00,1604),			// mtvscr	v0
		0,								// <op>		v3,v4,v5
		_VX(04,00,00,00,1540),			// mfvscr	v0
		POWERPC_STVX(0, 0, VSCR),		// stvx		v0,r7(0)
		POWERPC_STVX(RD, 0, RD),		// stvx		v3,r3(0)
		POWERPC_MTSPR(12, 256),			// mtvrsave	r12
		POWERPC_BLR						// blr
	};

	int i_opcode = -1;
	const int n_instructions = sizeof(code) / sizeof(code[0]);
	for (int i = 0; i < n_instructions; i++) {
		if (code[i] == 0) {
			i_opcode = i;
			break;
		}
	}
	assert(i_opcode != -1);

	const int n_elements = sizeof(tests) / sizeof(tests[0]);
	for (int n = 0; n < n_elements; n++) {
		vector_test_t vt = tests[n];
		code[i_opcode] = vt.opcode;
		flush_icache_range(code, sizeof(code));

		// Operand type
		char op_type = vt.op_type;
		if (!op_type)
			op_type = vt.type;

		// Operand values
		int n_vector_values;
		const vector_value_t *vvp;
		if (op_type == 'f') {
			n_vector_values = sizeof(vector_fp_values)/sizeof(vector_fp_values[0]);
			vvp = vector_fp_values;
		}
		else {
			n_vector_values = sizeof(vector_values)/sizeof(vector_values[0]);
			vvp = vector_values;
		}

		printf("Testing %s\n", vt.name);
		static aligned_vector_t avi, avj, avk;
		if (vt.operands[1] == vA && vt.operands[2] == vB && vt.operands[3] == vC) {
			for (int i = 0; i < n_vector_values; i++) {
				avi.copy(vvp[i].v);
				for (int j = 0; j < n_vector_values; j++) {
					avj.copy(vvp[j].v);
					for (int k = 0; k < n_vector_values; k++) {
						avk.copy(vvp[k].v);
						test_one_vector(code, vt, avi.addr(), avj.addr(), avk.addr());
					}
				}
			}
		}
		else if (vt.operands[1] == vA && vt.operands[2] == vB && vt.operands[3] == vN) {
			for (int i = 0; i < 16; i++) {
				vSH_field::insert(vt.opcode, i);
				code[i_opcode] = vt.opcode;
				flush_icache_range(code, sizeof(code));
				avi.copy(vvp[i].v);
				for (int j = 0; j < n_vector_values; j++) {
					avj.copy(vvp[j].v);
					for (int k = 0; k < n_vector_values; k++)
						test_one_vector(code, vt, avi.addr(), avj.addr());
				}
			}
		}
		else if (vt.operands[1] == vA && vt.operands[2] == vB) {
			for (int i = 0; i < n_vector_values; i++) {
				avi.copy(vvp[i].v);
				for (int j = 0; j < n_vector_values; j++) {
					if (op_type == 'B') {
						if (!vector_all_eq('b', vvp[j].v))
							continue;
					}
					avj.copy(vvp[j].v);
					test_one_vector(code, vt, avi.addr(), avj.addr());
				}
			}
		}
		else if (vt.operands[1] == vI && vt.operands[2] == vB) {
			for (int i = 0; i < 32; i++) {
				rA_field::insert(vt.opcode, i);
				code[i_opcode] = vt.opcode;
				flush_icache_range(code, sizeof(code));
				for (int j = 0; j < n_vector_values; j++) {
					avj.copy(vvp[j].v);
					test_one_vector(code, vt, NULL, avj.addr());
				}
			}
		}
		else if (vt.operands[1] == vI) {
			for (int i = 0; i < 32; i++) {
				rA_field::insert(vt.opcode, i);
				code[i_opcode] = vt.opcode;
				flush_icache_range(code, sizeof(code));
				test_one_vector(code, vt);
			}
		}
		else if (vt.operands[1] == __ && vt.operands[2] == vB) {
			for (int i = 0; i < n_vector_values; i++) {
				avi.copy(vvp[i].v);
				test_one_vector(code, vt, NULL, avi.addr());
			}
		}
		else {
			printf("ERROR: unhandled test case\n");
			abort();
		}
	}
#endif
}

// Illegal handler to catch out AltiVec instruction
#ifdef NATIVE_POWERPC
static sigjmp_buf env;

static void sigill_handler(int sig)
{
	has_altivec = false;
	siglongjmp(env, 1);
}
#endif

bool powerpc_test_cpu::test(void)
{
	// Tests initialization
	tests = errors = 0;
	init_cr = init_xer = 0;

	// Execution ALU tests
#if TEST_ALU_OPS
	test_add();
	test_sub();
	test_mul();
	test_div();
	test_shift();
	test_rotate();
	test_logical();
	test_compare();
	test_cr_logical();
#endif

	// Execute VMX tests
#if TEST_VMX_OPS
	if (has_altivec) {
		test_vector_load_for_shift();
		test_vector_load();
		test_vector_arith();
	}
#endif

	printf("%u errors out of %u tests\n", errors, tests);
	return errors == 0;
}

int main(int argc, char *argv[])
{
#ifdef EMU_KHEPERIX
	// Initialize VM system (predecode cache uses vm_acquire())
	vm_init();
#endif

	FILE *fp = NULL;
	powerpc_test_cpu *ppc = new powerpc_test_cpu;

	if (argc > 1) {
		const char *arg = argv[1];
		if (strcmp(arg, "--jit") == 0) {
			--argc;
			argv[1] = argv[0];
			++argv;
			ppc->enable_jit();
		}
	}

	if (argc > 1) {
		const char *file = argv[1];
#ifdef NATIVE_POWERPC
		if ((fp = fopen(file, "wb")) == NULL) {
			fprintf(stderr, "ERROR: can't open %s for writing\n", file);
			return EXIT_FAILURE;
		}
#else
		if ((fp = fopen(file, "rb")) == NULL) {
			fprintf(stderr, "ERROR: can't open %s for reading\n", file);
			return EXIT_FAILURE;
		}
#endif
		ppc->set_results_file(fp);

		// Use a large enough buffer
		static char buffer[4096];
		setvbuf(fp, buffer, _IOFBF, sizeof(buffer));
	}

	// We need a results file on non PowerPC platforms
#ifndef NATIVE_POWERPC
	if (fp == NULL) {
		fprintf(stderr, "ERROR: a results file for reference is required\n");
		return EXIT_FAILURE;
	}
#endif

	// Check if host CPU supports AltiVec instructions
	has_altivec = true;
#ifdef NATIVE_POWERPC
	signal(SIGILL, sigill_handler);
	if (!sigsetjmp(env, 1))
		asm volatile(".long 0x10000484"); // vor v0,v0,v0
	signal(SIGILL, SIG_DFL);
#endif

	bool ok = ppc->test();
	if (fp) fclose(fp);
	delete ppc;
	return !ok;
}
