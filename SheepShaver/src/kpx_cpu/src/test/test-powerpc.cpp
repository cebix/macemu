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

#include <stdlib.h>
#include "sysdeps.h"
#include "cpu/ppc/ppc-cpu.hpp"

// Partial PowerPC runtime assembler from GNU lightning
#define _I(X)			((uint32)(X))
#define _UL(X)			((uint32)(X))
#define _MASK(N)		((uint32)((1<<(N)))-1)
#define _ck_u(W,I)    	(_UL(I) & _MASK(W))
#define _u1(I)          _ck_u( 1,I)
#define _u5(I)          _ck_u( 5,I)
#define _u6(I)          _ck_u( 6,I)
#define _u9(I)          _ck_u( 9,I)
#define _u10(I)         _ck_u(10,I)

#define _X(   OP,RD,RA,RB,   XO,RC )  	_I((_u6(OP)<<26)|(_u5(RD)<<21)|(_u5(RA)<<16)|( _u5(RB)<<11)|              (_u10(XO)<<1)|_u1(RC))
#define _XO(  OP,RD,RA,RB,OE,XO,RC )  	_I((_u6(OP)<<26)|(_u5(RD)<<21)|(_u5(RA)<<16)|( _u5(RB)<<11)|(_u1(OE)<<10)|( _u9(XO)<<1)|_u1(RC))
#define _M(   OP,RS,RA,SH,MB,ME,RC )  	_I((_u6(OP)<<26)|(_u5(RS)<<21)|(_u5(RA)<<16)|( _u5(SH)<<11)|(_u5(MB)<< 6)|( _u5(ME)<<1)|_u1(RC))


struct exec_return { };

class powerpc_test_cpu
	: public powerpc_cpu
{
	uint32 emul_get_xer() const
		{ return xer().get(); }

	void emul_set_xer(uint32 value)
		{ xer().set(value); }

	uint32 emul_get_cr() const
		{ return cr().get(); }

	void emul_set_cr(uint32 value)
		{ cr().set(value); }

	uint32 native_get_xer() const
		{ uint32 xer; asm volatile ("mfxer %0" : "=r" (xer)); return xer; }

	void native_set_xer(uint32 xer) const
		{ asm volatile ("mtxer %0" : : "r" (xer)); }

	uint32 native_get_cr() const
		{ uint32 cr; asm volatile ("mfcr %0" : "=r" (cr)); return cr; }

	void native_set_cr(uint32 cr) const
		{ asm volatile ("mtcr %0" :  : "r" (cr)); }

	void init_decoder();
	void print_flags(uint32 cr, uint32 xer) const;
	void execute_return(uint32 opcode);
	void execute(uint32 opcode);

public:

	powerpc_test_cpu()
		: powerpc_cpu(NULL)
		{ init_decoder(); }

	bool test(void);
};

void powerpc_test_cpu::execute_return(uint32 opcode)
{
	throw exec_return();
}

void powerpc_test_cpu::init_decoder()
{
#ifndef PPC_NO_STATIC_II_INDEX_TABLE
	static bool initialized = false;
	if (initialized)
		return;
	initialized = true;
#endif

	static const instr_info_t return_ii_table[] = {
		{ "return",
		  (execute_fn)&powerpc_test_cpu::execute_return,
		  NULL,
		  D_form, 6, 0, CFLOW_TRAP
		}
	};

	const int ii_count = sizeof(return_ii_table)/sizeof(return_ii_table[0]);

	for (int i = 0; i < ii_count; i++) {
		const instr_info_t * ii = &return_ii_table[i];
		init_decoder_entry(ii);
	}
}

void powerpc_test_cpu::execute(uint32 opcode)
{
	uint32 code[] = { opcode, 0x18000000 };

	try {
		invalidate_cache();
		pc() = (uintptr)code;
		powerpc_cpu::execute();
	}
	catch (exec_return const &) {
		// Nothing, simply return
	}
}

void powerpc_test_cpu::print_flags(uint32 cr, uint32 xer) const
{
	printf("%s,%s,%s,%s,%s,%s",
		   (cr & CR_LT_field<0>::mask() ? "LT" : "__"),
		   (cr & CR_GT_field<0>::mask() ? "GT" : "__"),
		   (cr & CR_EQ_field<0>::mask() ? "EQ" : "__"),
		   (cr & CR_SO_field<0>::mask() ? "SO" : "__"),
		   (xer & XER_OV_field::mask()  ? "OV" : "__"),
		   (xer & XER_CA_field::mask()  ? "CA" : "__"));
}

bool powerpc_test_cpu::test(void)
{
	const bool verbose = false;
	int errors = 0;

	// Initial CR0, XER state
	uint32 init_cr = native_get_cr() & ~CR_field<0>::mask();
	uint32 init_xer = native_get_xer() & ~(XER_OV_field::mask() | XER_CA_field::mask());

	// Emulated registers IDs
	const int R_ = -1;
	const int RD = 10;
	const int RA = 11;
	const int RB = 12;

	// Instruction formats
	enum {
		____,
		R___,
		RR__,
		RRR_,
		RRIT,
	};

#define TEST_ASM_____(OP,RD,RA,RB,RC) asm volatile (OP : : : "cc")
#define TEST_ASM_R___(OP,RD,RA,RB,RC) asm volatile (OP " %0" : "=r" (RD) : : "cc")
#define TEST_ASM_RR__(OP,RD,RA,RB,RC) asm volatile (OP " %0,%1" : "=r" (RD) : "r" (RA) : "cc")
#define TEST_ASM_RRR_(OP,RD,RA,RB,RC) asm volatile (OP " %0,%1,%2" : "=r" (RD) : "r" (RA), "r" (RB) : "cc")
#define TEST_ASM_RRIT(OP,RD,RA,SH,MK) asm volatile (OP " %0,%1,%2,%3" : "=r" (RD) : "r" (RA), "i" (SH), "T" (MK))

#define TEST_ASM(FORMAT, OP, RD, CR, XER, A0, A1, A2) do {	\
		native_set_xer(init_xer);							\
		native_set_cr(init_cr);								\
		TEST_ASM_##FORMAT(OP, RD, A0, A1, A2);				\
		XER = native_get_xer();								\
		CR = native_get_cr();								\
	} while (0)

#define TEST_EMU_R0(OP,PRE,POST) do { PRE; execute(OP); POST; } while (0)
#define TEST_EMU_RD(OP,RD,VD,PRE) TEST_EMU_R0(OP,PRE,VD=gpr(RD))
#define TEST_EMU_____(OP,RD,VD,R0,A0,R1,A1,R2,A2) TEST_EMU_R0(/**/,/**/)
#define TEST_EMU_R___(OP,RD,VD,R0,A0,R1,A1,R2,A2) TEST_EMU_RD(OP,RD,VD,gpr(R0)=A0)
#define TEST_EMU_RR__(OP,RD,VD,R0,A0,R1,A1,R2,A2) TEST_EMU_RD(OP,RD,VD,gpr(R0)=A0;gpr(R1)=A1)
#define TEST_EMU_RRR_(OP,RD,VD,R0,A0,R1,A1,R2,A2) TEST_EMU_RD(OP,RD,VD,gpr(R0)=A0;gpr(R1)=A1;gpr(R2)=A2)

#define TEST_EMU(FORMAT, OP, RD, VD, CR, XER, R0, A0, R1, A1, R2, A2) do {	\
		emul_set_xer(init_xer);												\
		emul_set_cr(init_cr);												\
		TEST_EMU_##FORMAT(OP, RD, VD, R0, A0, R1, A1, R2, A2);				\
		XER = emul_get_xer();												\
		CR = emul_get_cr();													\
	} while (0)

#define TEST_ONE(FORMAT, NATIVE_OP, EMUL_OP, RD, R0, A0, R1, A1, R2, A2) do {				\
		uint32 native_rd = 0, native_xer, native_cr;										\
		TEST_ASM(FORMAT, NATIVE_OP, native_rd, native_cr, native_xer, A0, A1, A2);			\
		uint32 emul_rd = 0, emul_xer, emul_cr;												\
		TEST_EMU(FORMAT, EMUL_OP, RD, emul_rd, emul_cr, emul_xer, R0, A0, R1, A1, R2, A2);	\
																							\
		bool ok = native_rd == emul_rd														\
			&& native_xer == emul_xer														\
			&& native_cr == emul_cr;														\
																							\
		if (!ok) {																			\
			printf("FAIL: " NATIVE_OP " [%08x]\n", opcode);									\
			errors++;																		\
		}																					\
																							\
		if (!ok || verbose) {																\
			printf(" %08x, %08x => %08x [", ra, rb, native_rd);								\
			print_flags(native_cr, native_xer);												\
			printf("]\n");																	\
			printf(" %08x, %08x => %08x [", ra, rb, emul_rd);								\
			print_flags(emul_cr, emul_xer);													\
			printf("]\n");																	\
		}																					\
	} while (0)

#define TEST_INSTRUCTION(FORMAT, NATIVE_OP, EMUL_OP) do {							\
		printf("Testing " NATIVE_OP "\n");											\
		const uint32 opcode = EMUL_OP;												\
		for (uint32 i = 0; i < 8; i++) {											\
			const uint32 ra = i << 29;												\
			for (uint32 j = 0; j < 8; j++) {										\
				const uint32 rb = j << 29;											\
				TEST_ONE(FORMAT, NATIVE_OP, EMUL_OP, RD, RA, ra, RB, rb, R_, 0);	\
			}																		\
		}																			\
		for (int32 i = -2; i < 2; i++) {											\
			const uint32 ra = i;													\
			for (int32 j = -2; j < 2; j++) {										\
				const uint32 rb = j;												\
				TEST_ONE(FORMAT, NATIVE_OP, EMUL_OP, RD, RA, ra, RB, rb, R_, 0);	\
			}																		\
		}																			\
	} while (0)

	TEST_INSTRUCTION(RRR_,"add",		_XO(31,RD,RA,RB,0,266,0));
	TEST_INSTRUCTION(RRR_,"add.",		_XO(31,RD,RA,RB,0,266,1));
	TEST_INSTRUCTION(RRR_,"addo",		_XO(31,RD,RA,RB,1,266,0));
	TEST_INSTRUCTION(RRR_,"addo." ,		_XO(31,RD,RA,RB,1,266,1));
	TEST_INSTRUCTION(RRR_,"addc.",		_XO(31,RD,RA,RB,0, 10,1));
	TEST_INSTRUCTION(RRR_,"addco.",		_XO(31,RD,RA,RB,1, 10,1));
	TEST_INSTRUCTION(RRR_,"adde.",		_XO(31,RD,RA,RB,0,138,1));
	TEST_INSTRUCTION(RRR_,"addeo.",		_XO(31,RD,RA,RB,1,138,1));
	TEST_INSTRUCTION(RR__,"addme.",		_XO(31,RD,RA,00,0,234,1));
	TEST_INSTRUCTION(RR__,"addmeo.",	_XO(31,RD,RA,00,1,234,1));
	TEST_INSTRUCTION(RR__,"addze.",		_XO(31,RD,RA,00,0,202,1));
	TEST_INSTRUCTION(RR__,"addzeo.",	_XO(31,RD,RA,00,1,202,1));
	init_xer |= XER_CA_field::mask();
	TEST_INSTRUCTION(RRR_,"adde.",		_XO(31,RD,RA,RB,0,138,1));
	TEST_INSTRUCTION(RRR_,"addeo.",		_XO(31,RD,RA,RB,1,138,1));
	TEST_INSTRUCTION(RR__,"addme.",		_XO(31,RD,RA,00,0,234,1));
	TEST_INSTRUCTION(RR__,"addmeo.",	_XO(31,RD,RA,00,1,234,1));
	TEST_INSTRUCTION(RR__,"addze.",		_XO(31,RD,RA,00,0,202,1));
	TEST_INSTRUCTION(RR__,"addzeo.",	_XO(31,RD,RA,00,1,202,1));
	init_xer &= ~XER_CA_field::mask();
	TEST_INSTRUCTION(RRR_,"and.",		_X (31,RA,RD,RB,28,1));
	TEST_INSTRUCTION(RRR_,"andc.",		_X (31,RA,RD,RB,60,1));
	TEST_INSTRUCTION(RR__,"cntlzw.",	_X (31,RA,RD,00,26,1));
	TEST_INSTRUCTION(RRR_,"divw.",		_XO(31,RD,RA,RB,0,491,1));
	TEST_INSTRUCTION(RRR_,"divwo.",		_XO(31,RD,RA,RB,1,491,1));
	TEST_INSTRUCTION(RRR_,"divwu.",		_XO(31,RD,RA,RB,0,459,1));
	TEST_INSTRUCTION(RRR_,"divwuo.",	_XO(31,RD,RA,RB,1,459,1));
	TEST_INSTRUCTION(RRR_,"eqv.",		_X (31,RA,RD,RB,284,1));
	TEST_INSTRUCTION(RRR_,"mulhw.",		_XO(31,RD,RA,RB,0, 75,1));
	TEST_INSTRUCTION(RRR_,"mulhwu.",	_XO(31,RD,RA,RB,0, 11,1));
	TEST_INSTRUCTION(RRR_,"mullw.",		_XO(31,RD,RA,RB,0,235,1));
	TEST_INSTRUCTION(RRR_,"mullwo.",	_XO(31,RD,RA,RB,1,235,1));
	TEST_INSTRUCTION(RRR_,"nand.",		_X (31,RA,RD,RB,476,1));
	TEST_INSTRUCTION(RR__,"neg.",		_XO(31,RD,RA,RB,0,104,1));
	TEST_INSTRUCTION(RR__,"nego.",		_XO(31,RD,RA,RB,1,104,1));
	TEST_INSTRUCTION(RRR_,"nor.",		_X (31,RA,RD,RB,124,1));
	TEST_INSTRUCTION(RRR_,"or.",		_X (31,RA,RD,RB,444,1));
	TEST_INSTRUCTION(RRR_,"orc.",		_X (31,RA,RD,RB,412,1));
	TEST_INSTRUCTION(RRR_,"slw.",		_X (31,RA,RD,RB, 24,1));
	TEST_INSTRUCTION(RRR_,"sraw.",		_X (31,RA,RD,RB,792,1));
	TEST_INSTRUCTION(RRR_,"srw.",		_X (31,RA,RD,RB,536,1));
	TEST_INSTRUCTION(RRR_,"subf.",		_XO(31,RD,RA,RB,0, 40,1));
	TEST_INSTRUCTION(RRR_,"subfo.",		_XO(31,RD,RA,RB,1, 40,1));
	TEST_INSTRUCTION(RRR_,"subfc.",		_XO(31,RD,RA,RB,0,  8,1));
	TEST_INSTRUCTION(RRR_,"subfco.",	_XO(31,RD,RA,RB,1,  8,1));
	TEST_INSTRUCTION(RRR_,"subfe.",		_XO(31,RD,RA,RB,0,136,1));
	TEST_INSTRUCTION(RRR_,"subfeo.",	_XO(31,RD,RA,RB,1,136,1));
	TEST_INSTRUCTION(RR__,"subfme.",	_XO(31,RD,RA,00,0,232,1));
	TEST_INSTRUCTION(RR__,"subfmeo.",	_XO(31,RD,RA,00,1,232,1));
	TEST_INSTRUCTION(RR__,"subfze.",	_XO(31,RD,RA,00,0,200,1));
	TEST_INSTRUCTION(RR__,"subfzeo.",	_XO(31,RD,RA,00,1,200,1));
	init_xer |= XER_CA_field::mask();
	TEST_INSTRUCTION(RRR_,"subfe.",		_XO(31,RD,RA,RB,0,136,1));
	TEST_INSTRUCTION(RRR_,"subfeo.",	_XO(31,RD,RA,RB,1,136,1));
	TEST_INSTRUCTION(RR__,"subfme.",	_XO(31,RD,RA,00,0,232,1));
	TEST_INSTRUCTION(RR__,"subfmeo.",	_XO(31,RD,RA,00,1,232,1));
	TEST_INSTRUCTION(RR__,"subfze.",	_XO(31,RD,RA,00,0,200,1));
	TEST_INSTRUCTION(RR__,"subfzeo.",	_XO(31,RD,RA,00,1,200,1));
	init_xer &= ~XER_CA_field::mask();
	TEST_INSTRUCTION(RRR_,"xor.",		_X (31,RA,RD,RB,316,1));

	printf("%d errors\n", errors);
	return errors != 0;
}

int main()
{
	powerpc_test_cpu ppc;

	if (!ppc.test())
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
