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

#ifndef JIT_AMD64_CODEGEN_H
#define JIT_AMD64_CODEGEN_H

#define X86_TARGET_64BIT 1
#include "cpu/jit/x86/jit-target-codegen.hpp"

class amd64_codegen
	: public x86_codegen
{
public:

#define DEFINE_OP(NAME, OP)											\
	void gen_##NAME(int s, int d)									\
		{ GEN_CODE(OP##rr(s, d)); }									\
	void gen_##NAME(x86_memory_operand const & mem, int d)			\
		{ GEN_CODE(OP##mr(mem.MD, mem.MB, mem.MI, mem.MS, d)); }

	DEFINE_OP(sx_8_64, MOVSBQ);
	DEFINE_OP(zx_8_64, MOVZBQ);
	DEFINE_OP(sx_16_64, MOVSWQ);
	DEFINE_OP(zx_16_64, MOVZWQ);
	DEFINE_OP(sx_32_64, MOVSLQ);

#undef DEFINE_OP

public:

	void gen_push(int r)
		{ GEN_CODE(PUSHQr(r)); }
	void gen_push(x86_memory_operand const & mem)
		{ GEN_CODE(PUSHQm(mem.MD, mem.MB, mem.MI, mem.MS)); }
	void gen_pop(int r)
		{ GEN_CODE(POPQr(r)); }
	void gen_pop(x86_memory_operand const & mem)
		{ GEN_CODE(POPQm(mem.MD, mem.MB, mem.MI, mem.MS)); }
	void gen_bswap_64(int r)
		{ GEN_CODE(BSWAPQr(r)); }
	void gen_lea_64(x86_memory_operand const & mem, int d)
		{ GEN_CODE(LEAQmr(mem.MD, mem.MB, mem.MI, mem.MS, d)); }

public:

	void gen_mov_64(int s, int d)
		{ GEN_CODE(MOVQrr(s, d)); }
	void gen_mov_64(x86_memory_operand const & mem, int d)
		{ GEN_CODE(MOVQmr(mem.MD, mem.MB, mem.MI, mem.MS, d)); }
	void gen_mov_64(int s, x86_memory_operand const & mem)
		{ GEN_CODE(MOVQrm(s, mem.MD, mem.MB, mem.MI, mem.MS)); }
	void gen_mov_64(x86_immediate_operand const & imm, int d)
		{ GEN_CODE(MOVQir(imm.value, d)); }
	void gen_mov_64(x86_immediate_operand const & imm, x86_memory_operand const & mem)
		{ GEN_CODE(MOVQim(imm.value, mem.MD, mem.MB, mem.MI, mem.MS)); }

private:

	void gen_rotshi_64(int op, int s, int d)
		{ GEN_CODE(_ROTSHIQrr(op, s, d)); }
	void gen_rotshi_64(int op, int s, x86_memory_operand const & mem)
		{ GEN_CODE(_ROTSHIQrm(op, s, mem.MD, mem.MB, mem.MI, mem.MS)); }
	void gen_rotshi_64(int op, x86_immediate_operand const & imm, int d)
		{ GEN_CODE(_ROTSHIQir(op, imm.value, d)); }
	void gen_rotshi_64(int op, x86_immediate_operand const & imm, x86_memory_operand const & mem)
		{ GEN_CODE(_ROTSHIQim(op, imm.value, mem.MD, mem.MB, mem.MI, mem.MS)); }

public:

#define DEFINE_OP(NAME, OP)																	\
	void gen_##NAME##_64(int s, int d)														\
		{ gen_rotshi_64(X86_##OP, s, d); }													\
	void gen_##NAME##_64(int s, x86_memory_operand const & mem)								\
		{ gen_rotshi_64(X86_##OP, s, mem); }												\
	void gen_##NAME##_64(x86_immediate_operand const & imm, int d)							\
		{ gen_rotshi_64(X86_##OP, imm, d); }												\
	void gen_##NAME##_64(x86_immediate_operand const & imm, x86_memory_operand const & mem)	\
		{ gen_rotshi_64(X86_##OP, imm, mem); }

	DEFINE_OP(rol, ROL);
	DEFINE_OP(ror, ROR);
	DEFINE_OP(rcl, RCL);
	DEFINE_OP(rcr, RCR);
	DEFINE_OP(shl, SHL);
	DEFINE_OP(shr, SHR);
	DEFINE_OP(sar, SAR);

#undef DEFINE_OP
};

#endif /* JIT_AMD64_CODEGEN_H */
