/*
 *  jit-target-codegen.hpp - Target specific code generator
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

#ifndef JIT_X86_CODEGEN_H
#define JIT_X86_CODEGEN_H

// XXX drop <codegen_x86.h> (Basilisk II, lightning) => merge into here
#define X86_FLAT_REGISTERS		0
#define X86_OPTIMIZE_ALU		1
#define X86_OPTIMIZE_ROTSHI		1
#include "cpu/jit/x86/codegen_x86.h"

#if defined(__GNUC__)
#define x86_emit_failure(MSG)	gen_failure(MSG, __FILE__, __LINE__, __FUNCTION__)
#else
#define x86_emit_failure(MSG)	gen_failure(MSG, __FILE__, __LINE__)
#endif
#define x86_code_pointer		__my_cached_code_ptr
#define x86_get_target()		x86_code_pointer
#define x86_emit_byte(v)		(void)(*x86_code_pointer++ = (v))
#define x86_emit_word(v)		(*((uint16 *)x86_code_pointer) = (v), (void)(x86_code_pointer += 2))
#define x86_emit_long(v)		(*((uint32 *)x86_code_pointer) = (v), (void)(x86_code_pointer += 4))
#define x86_emit_quad(v)		(*((uint64 *)x86_code_pointer) = (v), (void)(x86_code_pointer += 8))

struct x86_immediate_operand {
	const int32 value;

	explicit x86_immediate_operand(int32 imm)
		: value(imm)
		{ }
};

struct x86_memory_operand {
	const int32 MD;
	const int8 MB;
	const int8 MI;
	const int8 MS;

	x86_memory_operand(int32 d, int b, int i = X86_NOREG, int s = 1)
		: MD(d), MB(b), MI(i), MS(s)
		{ }
};

class x86_codegen
	: public basic_jit_cache
{
protected:

	void gen_failure(const char *msg, const char *file, int line, const char *fn = NULL);

public:

	/* XXX this avoids emit_XXX() functions because GCC cannot
	   optimize out intermediate code_ptr() updates */
#define GEN_CODE(CODE) do {						\
		uint8 *x86_code_pointer = code_ptr();	\
		CODE;									\
		set_code_ptr(x86_code_pointer);			\
} while (0)

public:

	void gen_insn(int type, int op, int s, int d);
	void gen_insn(int type, int op, x86_memory_operand const & mem, int d);

private:

#define DEFINE_OP(SZ, SFX)																			\
	void gen_arith_##SZ(int op, int s, int d)														\
		{ GEN_CODE(_ALU##SFX##rr(op, s, d)); }														\
	void gen_arith_##SZ(int op, x86_memory_operand const & mem, int d)								\
		{ GEN_CODE(_ALU##SFX##mr(op, mem.MD, mem.MB, mem.MI, mem.MS, d)); }							\
	void gen_arith_##SZ(int op, int s, x86_memory_operand const & mem)								\
		{ GEN_CODE(_ALU##SFX##rm(op, s, mem.MD, mem.MB, mem.MI, mem.MS)); }							\
	void gen_arith_##SZ(int op, x86_immediate_operand const & imm, int d)							\
		{ GEN_CODE(_ALU##SFX##ir(op, imm.value, d)); }												\
	void gen_arith_##SZ(int op, x86_immediate_operand const & imm, x86_memory_operand const & mem)	\
		{ GEN_CODE(_ALU##SFX##im(op, imm.value, mem.MD, mem.MB, mem.MI, mem.MS)); }

	DEFINE_OP(8, B);
	DEFINE_OP(16, W);
	DEFINE_OP(32, L);

#undef DEFINE_OP

public:

#define DEFINE_OP_SZ(SZ, NAME, OP)																\
	void gen_##NAME##_##SZ(int s, int d)														\
		{ gen_arith_##SZ(X86_##OP, s, d); }														\
	void gen_##NAME##_##SZ(x86_memory_operand const & mem, int d)								\
		{ gen_arith_##SZ(X86_##OP, mem, d); }													\
	void gen_##NAME##_##SZ(int s, x86_memory_operand const & mem)								\
		{ gen_arith_##SZ(X86_##OP, s, mem); }													\
	void gen_##NAME##_##SZ(x86_immediate_operand const & imm, int d)							\
		{ gen_arith_##SZ(X86_##OP, imm, d); }													\
	void gen_##NAME##_##SZ(x86_immediate_operand const & imm, x86_memory_operand const & mem)	\
		{ gen_arith_##SZ(X86_##OP, imm, mem); }

#define DEFINE_OP(NAME, OP)						\
	DEFINE_OP_SZ(8, NAME, OP)					\
	DEFINE_OP_SZ(16, NAME, OP)					\
	DEFINE_OP_SZ(32, NAME, OP)

	DEFINE_OP(add, ADD);
	DEFINE_OP(sub, SUB);
	DEFINE_OP(adc, ADC);
	DEFINE_OP(sbb, SBB);
	DEFINE_OP(cmp, CMP);
	DEFINE_OP(or, OR);
	DEFINE_OP(xor, XOR);
	DEFINE_OP(and, AND);

#undef DEFINE_OP
#undef DEFINE_OP_SZ

private:

#define DEFINE_OP(SZ, SFX)																			\
	void gen_rotshi_##SZ(int op, int s, int d)														\
		{ GEN_CODE(_ROTSHI##SFX##rr(op, s, d)); }													\
	void gen_rotshi_##SZ(int op, int s, x86_memory_operand const & mem)								\
		{ GEN_CODE(_ROTSHI##SFX##rm(op, s, mem.MD, mem.MB, mem.MI, mem.MS)); }						\
	void gen_rotshi_##SZ(int op, x86_immediate_operand const & imm, int d)							\
		{ GEN_CODE(_ROTSHI##SFX##ir(op, imm.value, d)); }											\
	void gen_rotshi_##SZ(int op, x86_immediate_operand const & imm, x86_memory_operand const & mem)	\
		{ GEN_CODE(_ROTSHI##SFX##im(op, imm.value, mem.MD, mem.MB, mem.MI, mem.MS)); }

	DEFINE_OP(8, B);
	DEFINE_OP(16, W);
	DEFINE_OP(32, L);

#undef DEFINE_OP

public:

#define DEFINE_OP_SZ(SZ, NAME, OP)																\
	void gen_##NAME##_##SZ(int s, int d)														\
		{ gen_rotshi_##SZ(X86_##OP, s, d); }													\
	void gen_##NAME##_##SZ(int s, x86_memory_operand const & mem)								\
		{ gen_rotshi_##SZ(X86_##OP, s, mem); }													\
	void gen_##NAME##_##SZ(x86_immediate_operand const & imm, int d)							\
		{ gen_rotshi_##SZ(X86_##OP, imm, d); }													\
	void gen_##NAME##_##SZ(x86_immediate_operand const & imm, x86_memory_operand const & mem)	\
		{ gen_rotshi_##SZ(X86_##OP, imm, mem); }

#define DEFINE_OP(NAME, OP)						\
	DEFINE_OP_SZ(8, NAME, OP)					\
	DEFINE_OP_SZ(16, NAME, OP)					\
	DEFINE_OP_SZ(32, NAME, OP)

	DEFINE_OP(rol, ROL);
	DEFINE_OP(ror, ROR);
	DEFINE_OP(rcl, RCL);
	DEFINE_OP(rcr, RCR);
	DEFINE_OP(shl, SHL);
	DEFINE_OP(shr, SHR);
	DEFINE_OP(sar, SAR);

#undef DEFINE_OP
#undef DEFINE_OP_SZ

private:

#define DEFINE_OP(SZ, SFX)																			\
	void gen_bitop_##SZ(int op, int s, int d)														\
		{ GEN_CODE(_BT##SFX##rr(op, s, d)); }														\
	void gen_bitop_##SZ(int op, int s, x86_memory_operand const & mem)								\
		{ GEN_CODE(_BT##SFX##rm(op, s, mem.MD, mem.MB, mem.MI, mem.MS)); }							\
	void gen_bitop_##SZ(int op, x86_immediate_operand const & imm, int d)							\
		{ GEN_CODE(_BT##SFX##ir(op, imm.value, d)); }												\
	void gen_bitop_##SZ(int op, x86_immediate_operand const & imm, x86_memory_operand const & mem)	\
		{ GEN_CODE(_BT##SFX##im(op, imm.value, mem.MD, mem.MB, mem.MI, mem.MS)); }

	DEFINE_OP(16, W);
	DEFINE_OP(32, L);

#undef DEFINE_OP

public:

#define DEFINE_OP_SZ(SZ, NAME, OP)																\
	void gen_##NAME##_##SZ(int s, int d)														\
		{ gen_bitop_##SZ(X86_##OP, s, d); }														\
	void gen_##NAME##_##SZ(int s, x86_memory_operand const & mem)								\
		{ gen_bitop_##SZ(X86_##OP, s, mem); }													\
	void gen_##NAME##_##SZ(x86_immediate_operand const & imm, int d)							\
		{ gen_bitop_##SZ(X86_##OP, imm, d); }													\
	void gen_##NAME##_##SZ(x86_immediate_operand const & imm, x86_memory_operand const & mem)	\
		{ gen_bitop_##SZ(X86_##OP, imm, mem); }

#define DEFINE_OP(NAME, OP)						\
	DEFINE_OP_SZ(16, NAME, OP)					\
	DEFINE_OP_SZ(32, NAME, OP)

	DEFINE_OP(bt, BT);
	DEFINE_OP(btc, BTC);
	DEFINE_OP(bts, BTS);
	DEFINE_OP(btr, BTR);

#undef DEFINE_OP
#undef DEFINE_OP_SZ

public:

#define DEFINE_OP(SZ, SFX)																	\
	void gen_mov_##SZ(int s, int d)															\
		{ GEN_CODE(MOV##SFX##rr(s, d)); }													\
	void gen_mov_##SZ(x86_memory_operand const & mem, int d)								\
		{ GEN_CODE(MOV##SFX##mr(mem.MD, mem.MB, mem.MI, mem.MS, d)); }						\
	void gen_mov_##SZ(int s, x86_memory_operand const & mem)								\
		{ GEN_CODE(MOV##SFX##rm(s, mem.MD, mem.MB, mem.MI, mem.MS)); }						\
	void gen_mov_##SZ(x86_immediate_operand const & imm, int d)								\
		{ GEN_CODE(MOV##SFX##ir(imm.value, d)); }											\
	void gen_mov_##SZ(x86_immediate_operand const & imm, x86_memory_operand const & mem)	\
		{ GEN_CODE(MOV##SFX##im(imm.value, mem.MD, mem.MB, mem.MI, mem.MS)); }				\
	void gen_test_##SZ(int s, int d)														\
		{ GEN_CODE(TEST##SFX##rr(s, d)); }													\
	void gen_test_##SZ(int s, x86_memory_operand const & mem)								\
		{ GEN_CODE(TEST##SFX##rm(s, mem.MD, mem.MB, mem.MI, mem.MS)); }						\
	void gen_test_##SZ(x86_immediate_operand const & imm, int d)							\
		{ GEN_CODE(TEST##SFX##ir(imm.value, d)); }											\
	void gen_test_##SZ(x86_immediate_operand const & imm, x86_memory_operand const & mem)	\
		{ GEN_CODE(TEST##SFX##im(imm.value, mem.MD, mem.MB, mem.MI, mem.MS)); }

	DEFINE_OP(8, B);
	DEFINE_OP(16, W);
	DEFINE_OP(32, L);

#undef DEFINE_OP

private:

	void gen_unary_8(int op, int r)
		{ GEN_CODE(_UNARYBr(op, r)); }
	void gen_unary_8(int op, x86_memory_operand const & mem)
		{ GEN_CODE(_UNARYBm(op, mem.MD, mem.MB, mem.MI, mem.MS)); }
	void gen_unary_16(int op, int r)
		{ GEN_CODE(_UNARYWr(op, r)); }
	void gen_unary_16(int op, x86_memory_operand const & mem)
		{ GEN_CODE(_UNARYWm(op, mem.MD, mem.MB, mem.MI, mem.MS)); }
	void gen_unary_32(int op, int r)
		{ GEN_CODE(_UNARYLr(op, r)); }
	void gen_unary_32(int op, x86_memory_operand const & mem)
		{ GEN_CODE(_UNARYLm(op, mem.MD, mem.MB, mem.MI, mem.MS)); }

public:

#define DEFINE_OP_SZ(SZ, NAME, OP)							\
	void gen_##NAME##_##SZ(int r)							\
		{ gen_unary_##SZ(X86_##OP, r); }					\
	void gen_##NAME##_##SZ(x86_memory_operand const & mem)	\
		{ gen_unary_##SZ(X86_##OP, mem); }

#define DEFINE_OP(NAME, OP)						\
	DEFINE_OP_SZ(8, NAME, OP)					\
	DEFINE_OP_SZ(16, NAME, OP)					\
	DEFINE_OP_SZ(32, NAME, OP)

	DEFINE_OP(not, NOT);
	DEFINE_OP(neg, NEG);
	DEFINE_OP(mul, MUL);
	DEFINE_OP(div, DIV);
	DEFINE_OP(imul, IMUL);
	DEFINE_OP(idiv, IDIV);

#undef DEFINE_OP
#undef DEFINE_OP_SZ

	void gen_imul_16(int s, int d)
		{ GEN_CODE(IMULWrr(s, d)); }
	void gen_imul_16(x86_memory_operand const & mem, int d)
		{ GEN_CODE(IMULWmr(mem.MD, mem.MB, mem.MI, mem.MS, d)); }
	void gen_imul_16(int s, x86_immediate_operand const & imm, int d)
		{ GEN_CODE(IMULWirr(imm.value, s, d)); }
	void gen_imul_16(x86_memory_operand const & mem, x86_immediate_operand const & imm, int d)
		{ GEN_CODE(IMULWimr(imm.value, mem.MD, mem.MB, mem.MI, mem.MS, d)); }
	void gen_imul_32(int s, int d)
		{ GEN_CODE(IMULLrr(s, d)); }
	void gen_imul_32(x86_memory_operand const & mem, int d)
		{ GEN_CODE(IMULLmr(mem.MD, mem.MB, mem.MI, mem.MS, d)); }
	void gen_imul_32(x86_immediate_operand const & imm, int d)
		{ GEN_CODE(IMULLir(imm.value, d)); }
	void gen_imul_32(x86_immediate_operand const & imm, int s, int d)
		{ GEN_CODE(IMULLirr(imm.value, s, d)); }
	void gen_imul_32(x86_memory_operand const & mem, x86_immediate_operand const & imm, int d)
		{ GEN_CODE(IMULLimr(imm.value, mem.MD, mem.MB, mem.MI, mem.MS, d)); }

public:

	void gen_call(x86_immediate_operand const & target)
		{ GEN_CODE(CALLm(target.value)); }
	void gen_call(int r)
		{ GEN_CODE(CALLsr(r)); }
	void gen_call(x86_memory_operand const & mem)
		{ GEN_CODE(CALLsm(mem.MD, mem.MB, mem.MI, mem.MS)); }
	void gen_jmp(x86_immediate_operand const & target)
		{ GEN_CODE(JMPm(target.value)); }
	void gen_jmp(int r)
		{ GEN_CODE(JMPsr(r)); }
	void gen_jmp(x86_memory_operand const & mem)
		{ GEN_CODE(JMPsm(mem.MD, mem.MB, mem.MI, mem.MS)); }
	void gen_jcc(int cc, x86_immediate_operand const & target)
		{ GEN_CODE(JCCim(cc, target.value)); }
	void gen_jcc_offset(int cc, x86_immediate_operand const & offset)
		{ GEN_CODE(JCCii(cc, offset.value)); }
	void gen_setcc(int cc, int r)
		{ GEN_CODE(SETCCir(cc, r)); }
	void gen_setcc(int cc, x86_memory_operand const & mem)
		{ GEN_CODE(SETCCim(cc, mem.MD, mem.MB, mem.MI, mem.MS)); }
	void gen_cmov_16(int cc, int r, int d)
		{ GEN_CODE(CMOVWrr(cc, r, d)); }
	void gen_cmov_16(int cc, x86_memory_operand const & mem, int d)
		{ GEN_CODE(CMOVWmr(cc, mem.MD, mem.MB, mem.MI, mem.MS, d)); }
	void gen_cmov_32(int cc, int r, int d)
		{ GEN_CODE(CMOVLrr(cc, r, d)); }
	void gen_cmov_32(int cc, x86_memory_operand const & mem, int d)
		{ GEN_CODE(CMOVLmr(cc, mem.MD, mem.MB, mem.MI, mem.MS, d)); }

public:

	void gen_push(int r)
		{ GEN_CODE(PUSHLr(r)); }
	void gen_push(x86_memory_operand const & mem)
		{ GEN_CODE(PUSHLm(mem.MD, mem.MB, mem.MI, mem.MS)); }
	void gen_pop(int r)
		{ GEN_CODE(POPLr(r)); }
	void gen_pop(x86_memory_operand const & mem)
		{ GEN_CODE(POPLm(mem.MD, mem.MB, mem.MI, mem.MS)); }
	void gen_pusha(void)
		{ GEN_CODE(PUSHA()); }
	void gen_popa(void)
		{ GEN_CODE(POPA()); }
	void gen_pushf(void)
		{ GEN_CODE(PUSHF()); }
	void gen_popf(void)
		{ GEN_CODE(POPF()); }

public:

#define DEFINE_OP_SZ(SZ, SFX, NAME, OP)									\
	void gen_##NAME##_##SZ(int s, int d)								\
		{ GEN_CODE(OP##SFX##rr(s, d)); }								\
	void gen_##NAME##_##SZ(int s, x86_memory_operand const & mem)		\
		{ GEN_CODE(OP##SFX##rm(s, mem.MD, mem.MB, mem.MI, mem.MS)); }

#define DEFINE_OP(NAME, OP)						\
	DEFINE_OP_SZ(8, B, NAME, OP)				\
	DEFINE_OP_SZ(16, W, NAME, OP)				\
	DEFINE_OP_SZ(32, L, NAME, OP)

	DEFINE_OP(cmpxchg, CMPXCHG);
	DEFINE_OP(xadd, XADD);
	DEFINE_OP(xchg, XCHG);

#undef DEFINE_OP
#undef DEFINE_OP_SZ

public:

#define DEFINE_OP(NAME, OP)											\
	void gen_##NAME(int s, int d)									\
		{ GEN_CODE(OP##rr(s, d)); }									\
	void gen_##NAME(x86_memory_operand const & mem, int d)			\
		{ GEN_CODE(OP##mr(mem.MD, mem.MB, mem.MI, mem.MS, d)); }

	DEFINE_OP(bsf_16, BSFW);
	DEFINE_OP(bsr_16, BSRW);
	DEFINE_OP(bsf_32, BSFL);
	DEFINE_OP(bsr_32, BSRL);
	DEFINE_OP(mov_sx_8_16, MOVSBW);
	DEFINE_OP(mov_zx_8_16, MOVZBW);
	DEFINE_OP(mov_sx_8_32, MOVSBL);
	DEFINE_OP(mov_zx_8_32, MOVZBL);
	DEFINE_OP(mov_sx_16_32, MOVSWL);
	DEFINE_OP(mov_zx_16_32, MOVZWL);

#undef DEFINE_OP

public:

	void gen_bswap_32(int r)
		{ GEN_CODE(BSWAPLr(r)); }
	void gen_lea_32(x86_memory_operand const & mem, int d)
		{ GEN_CODE(LEALmr(mem.MD, mem.MB, mem.MI, mem.MS, d)); }
	void gen_clc(void)
		{ GEN_CODE(CLC()); }
	void gen_stc(void)
		{ GEN_CODE(STC()); }
	void gen_cmc(void)
		{ GEN_CODE(CMC()); }
	void gen_lahf(void)
		{ GEN_CODE(LAHF()); }
	void gen_sahf(void)
		{ GEN_CODE(SAHF()); }

private:

	void gen_sse_arith_ss(int op, int s, int d)
		{ GEN_CODE(_SSESSrr(op, s, d)); }
	void gen_sse_arith_ss(int op, x86_memory_operand const & mem, int d)
		{ GEN_CODE(_SSESSmr(op, mem.MD, mem.MB, mem.MI, mem.MS, d)); }
	void gen_sse_arith_sd(int op, int s, int d)
		{ GEN_CODE(_SSESDrr(op, s, d)); }
	void gen_sse_arith_sd(int op, x86_memory_operand const & mem, int d)
		{ GEN_CODE(_SSESDmr(op, mem.MD, mem.MB, mem.MI, mem.MS, d)); }
	void gen_sse_arith_ps(int op, int s, int d)
		{ GEN_CODE(_SSEPSrr(op, s, d)); }
	void gen_sse_arith_ps(int op, x86_memory_operand const & mem, int d)
		{ GEN_CODE(_SSEPSmr(op, mem.MD, mem.MB, mem.MI, mem.MS, d)); }
	void gen_sse_arith_pd(int op, int s, int d)
		{ GEN_CODE(_SSEPDrr(op, s, d)); }
	void gen_sse_arith_pd(int op, x86_memory_operand const & mem, int d)
		{ GEN_CODE(_SSEPDmr(op, mem.MD, mem.MB, mem.MI, mem.MS, d)); }

public:

	void gen_cmpps(int cc, int s, int d)
		{ GEN_CODE(CMPPSrr(cc, s, d)); }
	void gen_cmpps(int cc, x86_memory_operand const & mem, int d)
		{ GEN_CODE(CMPPSmr(cc, mem.MD, mem.MB, mem.MI, mem.MS, d)); }
	void gen_cmppd(int cc, int s, int d)
		{ GEN_CODE(CMPPDrr(cc, s, d)); }
	void gen_cmppd(int cc, x86_memory_operand const & mem, int d)
		{ GEN_CODE(CMPPDmr(cc, mem.MD, mem.MB, mem.MI, mem.MS, d)); }
	void gen_cmpss(int cc, int s, int d)
		{ GEN_CODE(CMPSSrr(cc, s, d)); }
	void gen_cmpss(int cc, x86_memory_operand const & mem, int d)
		{ GEN_CODE(CMPSSmr(cc, mem.MD, mem.MB, mem.MI, mem.MS, d)); }
	void gen_cmpsd(int cc, int s, int d)
		{ GEN_CODE(CMPSDrr(cc, s, d)); }
	void gen_cmpsd(int cc, x86_memory_operand const & mem, int d)
		{ GEN_CODE(CMPSDmr(cc, mem.MD, mem.MB, mem.MI, mem.MS, d)); }

public:

#define DEFINE_OP_SS(NAME, OP)									\
	void gen_##NAME##ss(x86_memory_operand const & mem, int d)	\
		{ gen_sse_arith_ss(X86_SSE_##OP, mem, d); }				\
	void gen_##NAME##ss(int s, int d)							\
		{ gen_sse_arith_ss(X86_SSE_##OP, s, d); }
#define DEFINE_OP_SD(NAME, OP)									\
	void gen_##NAME##sd(x86_memory_operand const & mem, int d)	\
		{ gen_sse_arith_sd(X86_SSE_##OP, mem, d); }				\
	void gen_##NAME##sd(int s, int d)							\
		{ gen_sse_arith_sd(X86_SSE_##OP, s, d); }
#define DEFINE_OP_PS(NAME, OP)									\
	void gen_##NAME##ps(x86_memory_operand const & mem, int d)	\
		{ gen_sse_arith_ps(X86_SSE_##OP, mem, d); }				\
	void gen_##NAME##ps(int s, int d)							\
		{ gen_sse_arith_ps(X86_SSE_##OP, s, d); }
#define DEFINE_OP_PD(NAME, OP)									\
	void gen_##NAME##pd(x86_memory_operand const & mem, int d)	\
		{ gen_sse_arith_pd(X86_SSE_##OP, mem, d); }				\
	void gen_##NAME##pd(int s, int d)							\
		{ gen_sse_arith_pd(X86_SSE_##OP, s, d); }

#define DEFINE_OP_S(NAME, OP)					\
	DEFINE_OP_SS(NAME, OP)						\
	DEFINE_OP_SD(NAME, OP)
#define DEFINE_OP_P(NAME, OP)					\
	DEFINE_OP_PS(NAME, OP)						\
	DEFINE_OP_PD(NAME, OP)
#define DEFINE_OP(NAME, OP)						\
	DEFINE_OP_S(NAME, OP)						\
	DEFINE_OP_P(NAME, OP)

	DEFINE_OP(add, ADD);
	DEFINE_OP(sub, SUB);
	DEFINE_OP(mul, MUL);
	DEFINE_OP(div, DIV);
	DEFINE_OP(min, MIN);
	DEFINE_OP(max, MAX);
	DEFINE_OP_P(and, AND);
	DEFINE_OP_P(andn, ANDN);
	DEFINE_OP_P(or, OR);
	DEFINE_OP_P(xor, XOR);
	DEFINE_OP_SS(rcp, RCP);
	DEFINE_OP_PS(rcp, RCP);
	DEFINE_OP_SS(rsqrt, RSQRT);
	DEFINE_OP_PS(rsqrt, RSQRT);
	DEFINE_OP_SS(sqrt, SQRT);
	DEFINE_OP_PS(sqrt, SQRT);
	DEFINE_OP_SS(comi, COMI);
	DEFINE_OP_SD(comi, COMI);
	DEFINE_OP_SS(ucomi, UCOMI);
	DEFINE_OP_SD(ucomi, UCOMI);

#undef DEFINE_OP
#undef DEFINE_OP_S
#undef DEFINE_OP_P
#undef DEFINE_OP_SS
#undef DEFINE_OP_SD
#undef DEFINE_OP_PS
#undef DEFINE_OP_PD

public:

#define DEFINE_OP(NAME, OP)											\
	void gen_##NAME(int s, int d)									\
		{ GEN_CODE(OP##rr(s, d)); }									\
	void gen_##NAME(x86_memory_operand const & mem, int d)			\
		{ GEN_CODE(OP##mr(mem.MD, mem.MB, mem.MI, mem.MS, d)); }	\
	void gen_##NAME(int s, x86_memory_operand const & mem)			\
		{ GEN_CODE(OP##rm(s, mem.MD, mem.MB, mem.MI, mem.MS)); }

	DEFINE_OP(movaps, MOVAPS);
	DEFINE_OP(movapd, MOVAPD);
	DEFINE_OP(movdqa, MOVDQA);
	DEFINE_OP(movdqu, MOVDQU);

#undef DEFINE_OP

public:

#define DEFINE_OP(NAME, OP)											\
	void gen_##NAME(int s, int d)									\
		{ GEN_CODE(OP##rr(s, d)); }									\
	void gen_##NAME(x86_memory_operand const & mem, int d)			\
		{ GEN_CODE(OP##mr(mem.MD, mem.MB, mem.MI, mem.MS, d)); }

	DEFINE_OP(movd_lx, MOVDLX);

#undef DEFINE_OP

public:

#define DEFINE_OP(NAME, OP)											\
	void gen_##NAME(int s, int d)									\
		{ GEN_CODE(OP##rr(s, d)); }									\
	void gen_##NAME(int s, x86_memory_operand const & mem)			\
		{ GEN_CODE(OP##rm(s, mem.MD, mem.MB, mem.MI, mem.MS)); }

	DEFINE_OP(movd_xl, MOVDXL);

#undef DEFINE_OP

private:

	void gen_sse_arith(int op1, int op2, int s, int d)
		{ GEN_CODE(_SSELrr(op1, op2, s, _rX, d, _rX)); }
	void gen_sse_arith(int op1, int op2, x86_memory_operand const & mem, int d)
		{ GEN_CODE(_SSELmr(op1, op2, mem.MD, mem.MB, mem.MI, mem.MS, d, _rX)); }
	void gen_sse_arith(int op1, int op2, x86_immediate_operand const & imm, int s, int d)
		{ GEN_CODE(_SSELirr(op1, op2, imm.value, s, d)); }
	void gen_sse_arith(int op1, int op2, x86_immediate_operand const & imm, x86_memory_operand const & mem, int d)
		{ GEN_CODE(_SSELimr(op1, op2, imm.value, mem.MD, mem.MB, mem.MI, mem.MS, d)); }
	void gen_sse_arith(int op1, int op2, int mri, x86_immediate_operand const & imm, int d)
		{ GEN_CODE(_SSELir(op1, op2, mri, imm.value, d)); }

public:

#define DEFINE_OP(NAME, OP1, OP2)							\
	void gen_##NAME(int s, int d)							\
		{ gen_sse_arith(OP1, OP2, s, d); }					\
	void gen_##NAME(x86_memory_operand const & mem, int d)	\
		{ gen_sse_arith(OP1, OP2, mem, d); }

#define DEFINE_OP_IRR(NAME, OP1, OP2)								 \
	void gen_##NAME(x86_immediate_operand const & imm, int s, int d) \
		{ gen_sse_arith(OP1, OP2, imm, s, d); }

#define DEFINE_OP_IXR(NAME, OP1, OP2)														  \
	DEFINE_OP_IRR(NAME, OP1, OP2)															  \
	void gen_##NAME(x86_immediate_operand const & imm, x86_memory_operand const & mem, int d) \
		{ gen_sse_arith(OP1, OP2, imm, mem, d); }

#define DEFINE_OP_IR(NAME, OP1, OP2, MRI)						\
	void gen_##NAME(x86_immediate_operand const & imm, int d)	\
		{ gen_sse_arith(OP1, OP2, MRI, imm, d); }

	DEFINE_OP(packssdw, 0x66, 0x6b);
	DEFINE_OP(packsswb, 0x66, 0x63);
	DEFINE_OP(packuswb, 0x66, 0x67);
	DEFINE_OP(paddb, 0x66, 0xfc);
	DEFINE_OP(paddd, 0x66, 0xfe);
	DEFINE_OP(paddq, 0x66, 0xd4);
	DEFINE_OP(paddsb, 0x66, 0xec);
	DEFINE_OP(paddsw, 0x66, 0xed);
	DEFINE_OP(paddusb, 0x66, 0xdc);
	DEFINE_OP(paddusw, 0x66, 0xdd);
	DEFINE_OP(paddw, 0x66, 0xfd);
	DEFINE_OP(pand, 0x66, 0xdb);
	DEFINE_OP(pandn, 0x66, 0xdf);
	DEFINE_OP(pavgb, 0x66, 0xe0);
	DEFINE_OP(pavgw, 0x66, 0xe3);
	DEFINE_OP(pcmpeqb, 0x66, 0x74);
	DEFINE_OP(pcmpeqd, 0x66, 0x76);
	DEFINE_OP(pcmpeqw, 0x66, 0x75);
	DEFINE_OP(pcmpgtb, 0x66, 0x64);
	DEFINE_OP(pcmpgtd, 0x66, 0x66);
	DEFINE_OP(pcmpgtw, 0x66, 0x65);
	DEFINE_OP_IRR(pextrw, 0x66, 0xc5);
	DEFINE_OP_IRR(pinsrw, 0x66, 0xc4);
	DEFINE_OP(pmaddwd, 0x66, 0xf5);
	DEFINE_OP(pmaxsw, 0x66, 0xee);
	DEFINE_OP(pmaxub, 0x66, 0xde);
	DEFINE_OP(pminsw, 0x66, 0xea);
	DEFINE_OP(pminub, 0x66, 0xda);
	DEFINE_OP(pmovmskb, 0x66, 0xd7);
	DEFINE_OP(pmulhuw, 0x66, 0xe4);
	DEFINE_OP(pmulhw, 0x66, 0xe5);
	DEFINE_OP(pmullw, 0x66, 0xd5);
	DEFINE_OP(pmuludq, 0x66, 0xf4);
	DEFINE_OP(por, 0x66, 0xeb);
	DEFINE_OP(psadbw, 0x66, 0xf6);
	DEFINE_OP_IXR(pshufd, 0x66, 0x70);
	DEFINE_OP_IXR(pshufhw, 0xf3, 0x70);
	DEFINE_OP_IXR(pshuflhw, 0xf2, 0x70);
	DEFINE_OP(pslld, 0x66, 0xf2);
	DEFINE_OP_IR(pslld, 0x66, 0x72, 6);
	DEFINE_OP_IR(pslldq, 0x66, 0x73, 7);
	DEFINE_OP(psllq, 0x66, 0xf3);
	DEFINE_OP_IR(psllq, 0x66, 0x73, 6);
	DEFINE_OP(psllw, 0x66, 0xf1);
	DEFINE_OP_IR(psllw, 0x66, 0x71, 6);
	DEFINE_OP(psrad, 0x66, 0xe2);
	DEFINE_OP_IR(psrad, 0x66, 0x72, 4);
	DEFINE_OP(psraw, 0x66, 0xe1);
	DEFINE_OP_IR(psraw, 0x66, 0x71, 4);
	DEFINE_OP(psrld, 0x66, 0xd2);
	DEFINE_OP_IR(psrld, 0x66, 0x72, 2);
	DEFINE_OP_IR(psrldq, 0x66, 0x73, 3);
	DEFINE_OP(psrlq, 0x66, 0xd3);
	DEFINE_OP_IR(psrlq, 0x66, 0x73, 2);
	DEFINE_OP(psrlw, 0x66, 0xd1);
	DEFINE_OP_IR(psrlw, 0x66, 0x71, 2);
	DEFINE_OP(psubb, 0x66, 0xf8);
	DEFINE_OP(psubd, 0x66, 0xfa);
	DEFINE_OP(psubq, 0x66, 0xfb);
	DEFINE_OP(psubsb, 0x66, 0xe8);
	DEFINE_OP(psubsw, 0x66, 0xe9);
	DEFINE_OP(psubusb, 0x66, 0xd8);
	DEFINE_OP(psubusw, 0x66, 0xd9);
	DEFINE_OP(psubw, 0x66, 0xf9);
	DEFINE_OP(punpckhbw, 0x66, 0x68);
	DEFINE_OP(punpckhdq, 0x66, 0x6a);
	DEFINE_OP(punpckhqdq, 0x66, 0x6d);
	DEFINE_OP(punpckhwd, 0x66, 0x69);
	DEFINE_OP(punpcklbw, 0x66, 0x60);
	DEFINE_OP(punpckldq, 0x66, 0x62);
	DEFINE_OP(punpcklqdq, 0x66, 0x6c);
	DEFINE_OP(punpcklwd, 0x66, 0x61);
	DEFINE_OP(pxor, 0x66, 0xef);

#undef DEFINE_OP
#undef DEFINE_OP_IR
#undef DEFINE_OP_IRR
#undef DEFINE_OP_IXR
};

enum {
	X86_INSN_ALU_8,
	X86_INSN_ALU_16,
	X86_INSN_ALU_32,
	X86_INSN_BIT_16,
	X86_INSN_BIT_32,
	X86_INSN_ROT_8,
	X86_INSN_ROT_16,
	X86_INSN_ROT_32,
	X86_INSN_SSE_SS,
	X86_INSN_SSE_SD,
	X86_INSN_SSE_PS,
	X86_INSN_SSE_PD,
	X86_INSN_SSE_PI,
};

inline void
x86_codegen::gen_insn(int type, int op, int s, int d)
{
	switch (type) {
	case X86_INSN_SSE_SS:
		gen_sse_arith_ss(op, s, d);
		break;
	case X86_INSN_SSE_SD:
		gen_sse_arith_sd(op, s, d);
		break;
	case X86_INSN_SSE_PS:
		gen_sse_arith_ps(op, s, d);
		break;
	case X86_INSN_SSE_PD:
		gen_sse_arith_pd(op, s, d);
		break;
	case X86_INSN_SSE_PI:
		gen_sse_arith(0x66, op, s, d);
		break;
	default:
		abort();
	}
}

inline void
x86_codegen::gen_insn(int type, int op, x86_memory_operand const & mem, int d)
{
	switch (type) {
	case X86_INSN_SSE_SS:
		gen_sse_arith_ss(op, mem, d);
		break;
	case X86_INSN_SSE_SD:
		gen_sse_arith_sd(op, mem, d);
		break;
	case X86_INSN_SSE_PS:
		gen_sse_arith_ps(op, mem, d);
		break;
	case X86_INSN_SSE_PD:
		gen_sse_arith_pd(op, mem, d);
		break;
	case X86_INSN_SSE_PI:
		gen_sse_arith(0x66, op, mem, d);
		break;
	default:
		abort();
	}
}

inline void
x86_codegen::gen_failure(const char *msg, const char *file, int line, const char *fn)
{
	fprintf(stderr, "JIT failure in function %s from file %s at line %d: %s\n",
			fn ? fn : "<unknown>", file, line, msg);
	abort();
}

#endif /* JIT_X86_CODEGEN_H */
