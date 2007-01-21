/*
 *  basic-dyngen.hpp - Basic code generator
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

#ifndef BASIC_DYNGEN_H
#define BASIC_DYNGEN_H

#include "cpu/jit/jit-config.hpp"
#include "cpu/jit/jit-codegen.hpp"

// Set jump target address
static inline void dg_set_jmp_target_noflush(uint8 *jmp_addr, uint8 *addr)
{
#if defined(__powerpc__) || defined(__ppc__)
	// patch the branch destination
	uint32 *ptr = (uint32 *)jmp_addr;
	uint32 val  = *ptr;
    val = (val & ~0x03fffffc) | ((addr - jmp_addr) & 0x03fffffc);
    *ptr = val;
#endif
#if defined(__i386__) || defined(__x86_64__)
	// patch the branch destination
	*(uint32 *)jmp_addr = addr - (jmp_addr + 4);
#endif
}

static inline void dg_set_jmp_target(uint8 *jmp_addr, uint8 *addr)
{
	dg_set_jmp_target_noflush(jmp_addr, addr);
#if defined(__powerpc__) || defined(__ppc__)
    // flush icache
    asm volatile ("dcbst 0,%0" : : "r"(ptr) : "memory");
    asm volatile ("sync" : : : "memory");
    asm volatile ("icbi 0,%0" : : "r"(ptr) : "memory");
    asm volatile ("sync" : : : "memory");
    asm volatile ("isync" : : : "memory");
#endif
}

#ifdef SHEEPSHAVER
class powerpc_cpu;
typedef powerpc_cpu *dyngen_cpu_base;
#else
class basic_cpu;
typedef basic_cpu *dyngen_cpu_base;
#endif

class basic_dyngen
	: public jit_codegen
{
	uint8 *execute_func;
	uint8 *gen_code_start;
	dyngen_cpu_base parent_cpu;

	// Can we generate a direct call to target function?
	bool direct_jump_possible(uintptr target) const;
	bool direct_call_possible(uintptr target) const;

	// Generic code generators
#	define DEFINE_CST(NAME,VALUE) static const unsigned long NAME = VALUE;
#	define DEFINE_GEN(NAME,RET,ARGS) RET NAME ARGS;
#	include "basic-dyngen-ops.hpp"

public:

	// Constructor, parent CPU required
	basic_dyngen(dyngen_cpu_base cpu);

	// Initialization
	bool initialize(void);

	// Return CPU context associated to this code generator
	dyngen_cpu_base cpu() const
		{ return parent_cpu; }

	// Align on ALIGN byte boundaries, ALIGN must be a power of 2
	uint8 *gen_align(int align = 16);

	// Start code generation of a new block
	// Align on 16-byte boundaries
	// Returns pointer to entry point
	uint8 *gen_start();

	// Stop code generation of the block
	// Returns FALSE if translation cache is full
	bool gen_end() const;

	// Execute compiled function at ENTRY_POINT
	void execute(uint8 *entry_point);

	// Return from compiled code
	void gen_exec_return();

	// Function calls
	void gen_jmp(const uint8 *target);
	void gen_invoke(void (*func)(void));
	void gen_invoke_T0(void (*func)(uint32));
	void gen_invoke_T0_T1(void (*func)(uint32, uint32));
	void gen_invoke_T0_T1_T2(void (*func)(uint32, uint32, uint32));
	void gen_invoke_T0_ret_T0(uint32 (*func)(uint32));
	void gen_invoke_im(void (*func)(uint32), uint32 value);
	void gen_invoke_CPU(void (*func)(dyngen_cpu_base));
	void gen_invoke_CPU_T0(void (*func)(dyngen_cpu_base, uint32));
	void gen_invoke_CPU_im(void (*func)(dyngen_cpu_base, uint32), uint32 value);
	void gen_invoke_CPU_im_im(void (*func)(dyngen_cpu_base, uint32, uint32), uint32 param1, uint32 param2);
	void gen_invoke_CPU_A0_ret_A0(void *(*func)(dyngen_cpu_base));

	// Raw aliases
#define DEFINE_ALIAS_RAW(NAME, ARGLIST, ARGS) \
	void gen_##NAME ARGLIST { gen_op_##NAME ARGS; }

#define DEFINE_ALIAS_0(NAME)	DEFINE_ALIAS_RAW(NAME,(),())
#define DEFINE_ALIAS_1(NAME)	DEFINE_ALIAS_RAW(NAME,(long p1),(p1))
#define DEFINE_ALIAS_2(NAME)	DEFINE_ALIAS_RAW(NAME,(long p1,long p2),(p1,p2))
#define DEFINE_ALIAS_3(NAME)	DEFINE_ALIAS_RAW(NAME,(long p1,long p2,long p3),(p1,p2,p3))
#define DEFINE_ALIAS(NAME, N)	DEFINE_ALIAS_##N(NAME)

	// Register moves
	void gen_mov_32_T0_im(int32 value);
	DEFINE_ALIAS(mov_32_T0_T1,0);
	DEFINE_ALIAS(mov_32_T0_T2,0);
	void gen_mov_32_T1_im(int32 value);
	DEFINE_ALIAS(mov_32_T1_T0,0);
	DEFINE_ALIAS(mov_32_T1_T2,0);
	void gen_mov_32_T2_im(int32 value);
	DEFINE_ALIAS(mov_32_T2_T0,0);
	DEFINE_ALIAS(mov_32_T2_T1,0);
	DEFINE_ALIAS(mov_ad_A0_im,1);
	DEFINE_ALIAS(mov_ad_A1_im,1);
	DEFINE_ALIAS(mov_ad_A2_im,1);

	// Arithmetic operations
	DEFINE_ALIAS(add_32_T0_T1,0);
	DEFINE_ALIAS(add_32_T0_T2,0);
	void gen_add_32_T0_im(int32 value);
	DEFINE_ALIAS(sub_32_T0_T1,0);
	DEFINE_ALIAS(sub_32_T0_T2,0);
	void gen_sub_32_T0_im(int32 value);
	DEFINE_ALIAS(add_32_T1_T0,0);
	DEFINE_ALIAS(add_32_T1_T2,0);
	void gen_add_32_T1_im(int32 value);
	DEFINE_ALIAS(sub_32_T1_T0,0);
	DEFINE_ALIAS(sub_32_T1_T2,0);
	void gen_sub_32_T1_im(int32 value);
	DEFINE_ALIAS(umul_32_T0_T1,0);
	DEFINE_ALIAS(smul_32_T0_T1,0);
	DEFINE_ALIAS(udiv_32_T0_T1,0);
	DEFINE_ALIAS(sdiv_32_T0_T1,0);
	DEFINE_ALIAS(xchg_32_T0_T1,0);
	DEFINE_ALIAS(bswap_16_T0,0);
	DEFINE_ALIAS(bswap_32_T0,0);

	// Logical operations
	DEFINE_ALIAS(neg_32_T0,0);
	DEFINE_ALIAS(not_32_T0,0);
	DEFINE_ALIAS(not_32_T1,0);
	DEFINE_ALIAS(and_32_T0_T1,0);
	DEFINE_ALIAS(and_32_T0_im,1);
	DEFINE_ALIAS(or_32_T0_T1,0);
	DEFINE_ALIAS(or_32_T0_im,1);
	DEFINE_ALIAS(xor_32_T0_T1,0);
	DEFINE_ALIAS(xor_32_T0_im,1);
	DEFINE_ALIAS(orc_32_T0_T1,0);
	DEFINE_ALIAS(andc_32_T0_T1,0);
	DEFINE_ALIAS(nand_32_T0_T1,0);
	DEFINE_ALIAS(nor_32_T0_T1,0);
	DEFINE_ALIAS(eqv_32_T0_T1,0);

	// Shift/Rotate operations
	DEFINE_ALIAS(lsl_32_T0_T1,0);
	DEFINE_ALIAS(lsl_32_T0_im,1);
	DEFINE_ALIAS(lsr_32_T0_T1,0);
	DEFINE_ALIAS(lsr_32_T0_im,1);
	DEFINE_ALIAS(asr_32_T0_T1,0);
	DEFINE_ALIAS(asr_32_T0_im,1);
	DEFINE_ALIAS(rol_32_T0_T1,0);
	DEFINE_ALIAS(rol_32_T0_im,1);
	DEFINE_ALIAS(ror_32_T0_T1,0);
	DEFINE_ALIAS(ror_32_T0_im,1);

	// Sign-/Zero-extension
	DEFINE_ALIAS(se_16_32_T0,0);
	DEFINE_ALIAS(se_16_32_T1,0);
	DEFINE_ALIAS(ze_16_32_T0,0);
	DEFINE_ALIAS(se_8_32_T0,0);
	DEFINE_ALIAS(ze_8_32_T0,0);

	// Jump instructions
	DEFINE_ALIAS(jmp_slow,1);
	DEFINE_ALIAS(jmp_fast,1);
	DEFINE_ALIAS(jmp_A0,0);

	// Load/Store instructions
	DEFINE_ALIAS(load_u32_T0_T1_T2,0);
	void gen_load_u32_T0_T1_im(int32 offset);
	DEFINE_ALIAS(load_s32_T0_T1_T2,0);
	void gen_load_s32_T0_T1_im(int32 offset);
	DEFINE_ALIAS(load_u16_T0_T1_T2,0);
	void gen_load_u16_T0_T1_im(int32 offset);
	DEFINE_ALIAS(load_s16_T0_T1_T2,0);
	void gen_load_s16_T0_T1_im(int32 offset);
	DEFINE_ALIAS(load_u8_T0_T1_T2,0);
	void gen_load_u8_T0_T1_im(int32 offset);
	DEFINE_ALIAS(load_s8_T0_T1_T2,0);
	void gen_load_s8_T0_T1_im(int32 offset);
	DEFINE_ALIAS(store_32_T0_T1_T2,0);
	void gen_store_32_T0_T1_im(int32 offset);
	DEFINE_ALIAS(store_16_T0_T1_T2,0);
	void gen_store_16_T0_T1_im(int32 offset);
	DEFINE_ALIAS(store_8_T0_T1_T2,0);
	void gen_store_8_T0_T1_im(int32 offset);

#undef DEFINE_ALIAS
#undef DEFINE_ALIAS_0
#undef DEFINE_ALIAS_1
#undef DEFINE_ALIAS_2
#undef DEFINE_ALIAS_3
#undef DEFINE_ALIAS_RAW

	// Address of jump offset to patch for direct chaining
	static const int MAX_JUMPS = 2;
	uint8 *jmp_addr[MAX_JUMPS];
};

inline bool
basic_dyngen::direct_jump_possible(uintptr target) const
{
#if defined(__powerpc__) || defined(__ppc__)
	const uintptr LI_OFFSET_MAX = 1 << 26;
	return (((target - (uintptr)code_ptr()) < LI_OFFSET_MAX) ||
			(((uintptr)code_ptr() - target) < LI_OFFSET_MAX));
#endif
#if defined(__i386__)
	return true;
#endif
#if defined(__x86_64__)
	const intptr offset = (intptr)target - (intptr)code_ptr() - sizeof(void *);
	return offset <= 0xffffffff;
#endif
	return false;
}

inline void
basic_dyngen::gen_jmp(const uint8 *target_p)
{
	const uintptr target = (uintptr)target_p;
	if (direct_jump_possible(target))
		gen_op_jmp_fast(target);
	else
		gen_op_jmp_slow(target);
}

inline void
basic_dyngen::execute(uint8 *entry_point)
{
	typedef void (*func_t)(uint8 *, dyngen_cpu_base);
	func_t func = (func_t)execute_func;
	func(entry_point, parent_cpu);
}

inline void
basic_dyngen::gen_exec_return()
{
	gen_jmp(execute_func + op_exec_return_offset);
}

inline bool
basic_dyngen::direct_call_possible(uintptr target) const
{
#if defined(__powerpc__) || defined(__ppc__)
	const uintptr LI_OFFSET_MAX = 1 << 26;
	return (((target - (uintptr)code_ptr()) < LI_OFFSET_MAX) ||
			(((uintptr)code_ptr() - target) < LI_OFFSET_MAX));
#endif
#if defined(__i386__)
	return true;
#endif
#if defined(__x86_64__)
	const intptr offset = (intptr)target - (intptr)code_ptr() - sizeof(void *);
	return offset <= 0xffffffff;
#endif
	return false;
}

inline uint8 *
basic_dyngen::gen_start()
{
	for (int i = 0; i < MAX_JUMPS; i++)
		jmp_addr[i] = NULL;
	gen_code_start = gen_align();
	return gen_code_start;
}

inline bool
basic_dyngen::gen_end() const
{
	flush_icache_range((unsigned long)gen_code_start, (unsigned long)code_ptr());
	return !full_translation_cache();
}

#define DEFINE_OP(REG)								\
inline void											\
basic_dyngen::gen_mov_32_##REG##_im(int32 value)	\
{													\
	if (value == 0)									\
		gen_op_mov_32_##REG##_0();					\
	else											\
		gen_op_mov_32_##REG##_im(value);			\
}

DEFINE_OP(T0);
DEFINE_OP(T1);
DEFINE_OP(T2);

#undef DEFINE_OP

#define DEFINE_OP(OP,REG)										\
inline void														\
basic_dyngen::gen_##OP##_32_##REG##_im(int32 value)				\
{																\
	if (value == 0)			return;								\
	else if (value == 1)	gen_op_##OP##_32_##REG##_1();		\
	else if (value == 2)	gen_op_##OP##_32_##REG##_2();		\
	else if (value == 4)	gen_op_##OP##_32_##REG##_4();		\
	else if (value == 8)	gen_op_##OP##_32_##REG##_8();		\
	else					gen_op_##OP##_32_##REG##_im(value);	\
}

DEFINE_OP(add,T0);
DEFINE_OP(add,T1);
DEFINE_OP(sub,T0);
DEFINE_OP(sub,T1);

#undef DEFINE_OP

#define DEFINE_OP(NAME,REG,SIZE)								\
inline void														\
basic_dyngen::gen_##NAME##_##SIZE##_##REG##_T1_im(int32 offset)	\
{																\
	if (offset == 0)											\
		gen_op_##NAME##_##SIZE##_##REG##_T1_0();				\
	else														\
		gen_op_##NAME##_##SIZE##_##REG##_T1_im(offset);			\
}

DEFINE_OP(load,T0,u32);
DEFINE_OP(load,T0,s32);
DEFINE_OP(store,T0,32);
DEFINE_OP(load,T0,u16);
DEFINE_OP(load,T0,s16);
DEFINE_OP(store,T0,16);
DEFINE_OP(load,T0,u8);
DEFINE_OP(load,T0,s8);
DEFINE_OP(store,T0,8);

#undef DEFINE_OP

#endif /* BASIC_DYNGEN_H */
