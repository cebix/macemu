/*
 *  ppc-translate.cpp - PowerPC dynamic translation
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

#include "sysdeps.h"
#include "cpu/ppc/ppc-cpu.hpp"
#include "cpu/ppc/ppc-instructions.hpp"
#include "cpu/ppc/ppc-operands.hpp"

#if PPC_ENABLE_JIT
#include "cpu/jit/dyngen-exec.h"
#endif

#ifdef SHEEPSHAVER
#include "cpu_emulation.h"
#endif

#include <stdio.h>

#define DEBUG 1
#include "debug.h"

#if ENABLE_MON
#include "mon.h"
#include "mon_disass.h"
#endif

// Define to enable const branches optimization
#define FOLLOW_CONST_JUMPS 1

// FIXME: define ROM areas
static inline bool is_read_only_memory(uintptr addr)
{
#ifdef SHEEPSHAVER
	if ((addr - ROM_BASE) < ROM_AREA_SIZE)
		return true;
#endif
	return false;
}

// Returns TRUE if we can directly generate a jump to the target block
// XXX mixing front-end and back-end conditions is not a very good idea...
static inline bool direct_chaining_possible(uint32 bpc, uint32 tpc)
{
#ifndef DYNGEN_FAST_DISPATCH
	return false;
#endif
	return ((bpc ^ tpc) >> 12) == 0 || is_read_only_memory(tpc);
}


/**
 *		Basic block disassemblers
 **/

#define TARGET_M68K		0
#define TARGET_POWERPC	1
#define TARGET_X86		2
#define TARGET_AMD64	3
#if defined(i386) || defined(__i386__)
#define TARGET_NATIVE	TARGET_X86
#endif
#if defined(x86_64) || defined(__x86_64__)
#define TARGET_NATIVE	TARGET_AMD64
#endif
#if defined(powerpc) || defined(__powerpc__) || defined(__ppc__)
#define TARGET_NATIVE	TARGET_POWERPC
#endif

#if PPC_ENABLE_JIT
static void disasm_block(int target, uint8 *start, uint32 length)
{
#if ENABLE_MON
	char disasm_str[200];
	sprintf(disasm_str, "%s $%x $%x",
			target == TARGET_M68K ? "d68" :
			target == TARGET_X86 ? "d86" :
			target == TARGET_AMD64 ? "d8664" :
			target == TARGET_POWERPC ? "d" : "x",
			start, start + length - 1);

	char *arg[] = {"mon",
#ifdef SHEEPSHAVER
				   "-m",
#endif
				   "-r", disasm_str, NULL};
	mon(sizeof(arg)/sizeof(arg[0]) - 1, arg);
#endif
}

static void disasm_translation(uint32 src_addr, uint32 src_len,
							   uint8* dst_addr, uint32 dst_len)
{
	printf("### Block at %08x translated to %p (%d bytes)\n", src_addr, dst_addr, dst_len);
	printf("IN:\n");
	disasm_block(TARGET_POWERPC, vm_do_get_real_address(src_addr), src_len);
	printf("OUT:\n");
#ifdef TARGET_NATIVE
	disasm_block(TARGET_NATIVE, dst_addr, dst_len);
#else
	printf("unsupported disassembler for this archicture\n");
#endif
}
#endif


/**
 *		DynGen dynamic code translation
 **/

#if PPC_ENABLE_JIT
powerpc_cpu::block_info *
powerpc_cpu::compile_block(uint32 entry_point)
{
#if DEBUG
	bool disasm = false;
#else
	const bool disasm = false;
#endif

#if PPC_PROFILE_COMPILE_TIME
	compile_count++;
	clock_t start_time = clock();
#endif

	powerpc_jit & dg = codegen;
	codegen_context_t cg_context(dg);
	cg_context.entry_point = entry_point;
  again:
	block_info *bi = block_cache.new_blockinfo();
	bi->init(entry_point);
	bi->entry_point = dg.gen_start(entry_point);

	// Direct block chaining support variables
	bool use_direct_block_chaining = false;

	int compile_status;
	uint32 dpc = entry_point - 4;
	uint32 min_pc, max_pc;
	min_pc = max_pc = entry_point;
	uint32 sync_pc = dpc;
	uint32 sync_pc_offset = 0;
	bool done_compile = false;
	while (!done_compile) {
		uint32 opcode = vm_read_memory_4(dpc += 4);
		const instr_info_t *ii = decode(opcode);
		if (ii->cflow & CFLOW_END_BLOCK)
			done_compile = true;

		// Assume we can compile this opcode
		compile_status = COMPILE_CODE_OK;

#if PPC_FLIGHT_RECORDER
		if (is_logging()) {
			typedef void (*func_t)(dyngen_cpu_base, uint32, uint32);
			func_t func = (func_t)nv_mem_fun((execute_pmf)&powerpc_cpu::do_record_step).ptr();
			dg.gen_invoke_CPU_im_im(func, dpc, opcode);
		}
#endif

		union operands_t {
			struct {
				int size, sign;
				int do_update;
				int do_indexed;
			} mem;
			struct {
				uint32 target;
			} jmp;
		};
		operands_t op;

		switch (ii->mnemo) {
		case PPC_I(LBZ):		// Load Byte and Zero
			op.mem.size = 1;
			op.mem.sign = 0;
			op.mem.do_update = 0;
			op.mem.do_indexed = 0;
			goto do_load;
		case PPC_I(LBZU):		// Load Byte and Zero with Update
			op.mem.size = 1;
			op.mem.sign = 0;
			op.mem.do_update = 1;
			op.mem.do_indexed = 0;
			goto do_load;
		case PPC_I(LBZUX):		// Load Byte and Zero with Update Indexed
			op.mem.size = 1;
			op.mem.sign = 0;
			op.mem.do_update = 1;
			op.mem.do_indexed = 1;
			goto do_load;
		case PPC_I(LBZX):		// Load Byte and Zero Indexed
			op.mem.size = 1;
			op.mem.sign = 0;
			op.mem.do_update = 0;
			op.mem.do_indexed = 1;
			goto do_load;
		case PPC_I(LHA):		// Load Half Word Algebraic
			op.mem.size = 2;
			op.mem.sign = 1;
			op.mem.do_update = 0;
			op.mem.do_indexed = 0;
			goto do_load;
		case PPC_I(LHAU):		// Load Half Word Algebraic with Update
			op.mem.size = 2;
			op.mem.sign = 1;
			op.mem.do_update = 1;
			op.mem.do_indexed = 0;
			goto do_load;
		case PPC_I(LHAUX):		// Load Half Word Algebraic with Update Indexed
			op.mem.size = 2;
			op.mem.sign = 1;
			op.mem.do_update = 1;
			op.mem.do_indexed = 1;
			goto do_load;
		case PPC_I(LHAX):		// Load Half Word Algebraic Indexed
			op.mem.size = 2;
			op.mem.sign = 1;
			op.mem.do_update = 0;
			op.mem.do_indexed = 1;
			goto do_load;
		case PPC_I(LHZ):		// Load Half Word and Zero
			op.mem.size = 2;
			op.mem.sign = 0;
			op.mem.do_update = 0;
			op.mem.do_indexed = 0;
			goto do_load;
		case PPC_I(LHZU):		// Load Half Word and Zero with Update
			op.mem.size = 2;
			op.mem.sign = 0;
			op.mem.do_update = 1;
			op.mem.do_indexed = 0;
			goto do_load;
		case PPC_I(LHZUX):		// Load Half Word and Zero with Update Indexed
			op.mem.size = 2;
			op.mem.sign = 0;
			op.mem.do_update = 1;
			op.mem.do_indexed = 1;
			goto do_load;
		case PPC_I(LHZX):		// Load Half Word and Zero Indexed
			op.mem.size = 2;
			op.mem.sign = 0;
			op.mem.do_update = 0;
			op.mem.do_indexed = 1;
			goto do_load;
		case PPC_I(LWZ):		// Load Word and Zero
			op.mem.size = 4;
			op.mem.sign = 0;
			op.mem.do_update = 0;
			op.mem.do_indexed = 0;
			goto do_load;
		case PPC_I(LWZU):		// Load Word and Zero with Update
			op.mem.size = 4;
			op.mem.sign = 0;
			op.mem.do_update = 1;
			op.mem.do_indexed = 0;
			goto do_load;
		case PPC_I(LWZUX):		// Load Word and Zero with Update Indexed
			op.mem.size = 4;
			op.mem.sign = 0;
			op.mem.do_update = 1;
			op.mem.do_indexed = 1;
			goto do_load;
		case PPC_I(LWZX):		// Load Word and Zero Indexed
			op.mem.size = 4;
			op.mem.sign = 0;
			op.mem.do_update = 0;
			op.mem.do_indexed = 1;
			goto do_load;
		{
		  do_load:
			// Extract RZ operand
			const int rA = rA_field::extract(opcode);
			if (rA == 0 && !op.mem.do_update)
				dg.gen_mov_32_T1_im(0);
			else
				dg.gen_load_T1_GPR(rA);

			// Extract index operand
			if (op.mem.do_indexed)
				dg.gen_load_T2_GPR(rB_field::extract(opcode));

			switch (op.mem.size) {
			case 1:
				if (op.mem.do_indexed)
					dg.gen_load_u8_T0_T1_T2();
				else
					dg.gen_load_u8_T0_T1_im(operand_D::get(this, opcode));
				break;
			case 2:
				if (op.mem.do_indexed) {
					if (op.mem.sign)
						dg.gen_load_s16_T0_T1_T2();
					else
						dg.gen_load_u16_T0_T1_T2();
				}
				else {
					const int32 offset = operand_D::get(this, opcode);
					if (op.mem.sign)
						dg.gen_load_s16_T0_T1_im(offset);
					else
						dg.gen_load_u16_T0_T1_im(offset);
				}
				break;
			case 4:
				if (op.mem.do_indexed) {
					dg.gen_load_u32_T0_T1_T2();
				}
				else {
					const int32 offset = operand_D::get(this, opcode);
					dg.gen_load_u32_T0_T1_im(offset);
				}
				break;
			}

			// Commit result
			dg.gen_store_T0_GPR(rD_field::extract(opcode));

			// Update RA
			if (op.mem.do_update) {
				if (op.mem.do_indexed)
					dg.gen_add_32_T1_T2();
				else
					dg.gen_add_32_T1_im(operand_D::get(this, opcode));
				dg.gen_store_T1_GPR(rA);
			}
			break;
		}
		case PPC_I(STB):		// Store Byte
			op.mem.size = 1;
			op.mem.do_update = 0;
			op.mem.do_indexed = 0;
			goto do_store;
		case PPC_I(STBU):		// Store Byte with Update
			op.mem.size = 1;
			op.mem.do_update = 1;
			op.mem.do_indexed = 0;
			goto do_store;
		case PPC_I(STBUX):		// Store Byte with Update Indexed
			op.mem.size = 1;
			op.mem.do_update = 1;
			op.mem.do_indexed = 1;
			goto do_store;
		case PPC_I(STBX):		// Store Byte Indexed
			op.mem.size = 1;
			op.mem.do_update = 0;
			op.mem.do_indexed = 1;
			goto do_store;
		case PPC_I(STH):		// Store Half Word
			op.mem.size = 2;
			op.mem.do_update = 0;
			op.mem.do_indexed = 0;
			goto do_store;
		case PPC_I(STHU):		// Store Half Word with Update
			op.mem.size = 2;
			op.mem.do_update = 1;
			op.mem.do_indexed = 0;
			goto do_store;
		case PPC_I(STHUX):		// Store Half Word with Update Indexed
			op.mem.size = 2;
			op.mem.do_update = 1;
			op.mem.do_indexed = 1;
			goto do_store;
		case PPC_I(STHX):		// Store Half Word Indexed
			op.mem.size = 2;
			op.mem.do_update = 0;
			op.mem.do_indexed = 1;
			goto do_store;
		case PPC_I(STW):		// Store Word
			op.mem.size = 4;
			op.mem.do_update = 0;
			op.mem.do_indexed = 0;
			goto do_store;
		case PPC_I(STWU):		// Store Word with Update
			op.mem.size = 4;
			op.mem.do_update = 1;
			op.mem.do_indexed = 0;
			goto do_store;
		case PPC_I(STWUX):		// Store Word with Update Indexed
			op.mem.size = 4;
			op.mem.do_update = 1;
			op.mem.do_indexed = 1;
			goto do_store;
		case PPC_I(STWX):		// Store Word Indexed
			op.mem.size = 4;
			op.mem.do_update = 0;
			op.mem.do_indexed = 1;
			goto do_store;
		{
		  do_store:
			// Extract RZ operand
			const int rA = rA_field::extract(opcode);
			if (rA == 0 && !op.mem.do_update)
				dg.gen_mov_32_T1_im(0);
			else
				dg.gen_load_T1_GPR(rA);

			// Extract index operand
			if (op.mem.do_indexed)
				dg.gen_load_T2_GPR(rB_field::extract(opcode));

			// Load register to commit to memory
			dg.gen_load_T0_GPR(rS_field::extract(opcode));

			switch (op.mem.size) {
			case 1:
				if (op.mem.do_indexed)
					dg.gen_store_8_T0_T1_T2();
				else
					dg.gen_store_8_T0_T1_im(operand_D::get(this, opcode));
				break;
			case 2:
				if (op.mem.do_indexed)
					dg.gen_store_16_T0_T1_T2();
				else
					dg.gen_store_16_T0_T1_im(operand_D::get(this, opcode));
				break;
			case 4:
				if (op.mem.do_indexed)
					dg.gen_store_32_T0_T1_T2();
				else
					dg.gen_store_32_T0_T1_im(operand_D::get(this, opcode));
				break;
			}

			// Update RA
			if (op.mem.do_update) {
				if (op.mem.do_indexed)
					dg.gen_add_32_T1_T2();
				else
					dg.gen_add_32_T1_im(operand_D::get(this, opcode));
				dg.gen_store_T1_GPR(rA);
			}
			break;
		}
		case PPC_I(LMW):		// Load Multiple Word
		case PPC_I(STMW):		// Store Multiple Word
		{
			const int rA = rA_field::extract(opcode);
			if (rA == 0)
				dg.gen_mov_32_T0_im(operand_D::get(this, opcode));
			else {
				dg.gen_load_T0_GPR(rA);
				dg.gen_add_32_T0_im(operand_D::get(this, opcode));
			}
			switch (ii->mnemo) {
			case PPC_I(LMW):  dg.gen_lmw_T0(rD_field::extract(opcode));  break;
			case PPC_I(STMW): dg.gen_stmw_T0(rS_field::extract(opcode)); break;
			}
			break;
		}
#if KPX_MAX_CPUS == 1
		case PPC_I(STWCX):		// Store Word Conditional Indexed
		case PPC_I(LWARX):		// Load Word and Reserve Indexed
		{
			const int rA = rA_field::extract(opcode);
			const int rB = rB_field::extract(opcode);
			if (rA == 0)
				dg.gen_load_T1_GPR(rB);
			else {
				dg.gen_load_T1_GPR(rA);
				dg.gen_load_T2_GPR(rB);
				dg.gen_add_32_T1_T2();
			}
			switch (ii->mnemo) {
			case PPC_I(LWARX):
				dg.gen_lwarx_T0_T1();
				dg.gen_store_T0_GPR(rD_field::extract(opcode));
				break;
			case PPC_I(STWCX):
				dg.gen_load_T0_GPR(rS_field::extract(opcode));
				dg.gen_stwcx_T0_T1();
				break;
			}
			break;
		}
#endif
		case PPC_I(BC):			// Branch Conditional
		{
			const int bo = BO_field::extract(opcode);
#if FOLLOW_CONST_JUMPS
			if (!BO_CONDITIONAL_BRANCH(bo) && !BO_DECREMENT_CTR(bo)) {
				if (LK_field::test(opcode)) {
					const uint32 npc = dpc + 4;
					dg.gen_store_im_LR(npc);
				}
				if (AA_field::test(opcode))
					dpc = 0;
				op.jmp.target = ((dpc + operand_BD::get(this, opcode)) & -4);
				goto do_const_jump;
			}
#endif
			const uint32 tpc = ((AA_field::test(opcode) ? 0 : dpc) + operand_BD::get(this, opcode)) & -4;
			const uint32 npc = dpc + 4;
#if DYNGEN_DIRECT_BLOCK_CHAINING
			// Use direct block chaining for in-page jumps or jumps to ROM area
			if (direct_chaining_possible(bi->pc, tpc)) {
				use_direct_block_chaining = true;
				bi->li[0].jmp_pc = tpc;
				// Make sure it's a conditional branch
				if (BO_CONDITIONAL_BRANCH(bo) || BO_DECREMENT_CTR(bo))
					bi->li[1].jmp_pc = npc;
			}
#endif

			if (LK_field::test(opcode))
				dg.gen_store_im_LR(npc);

			dg.gen_bc(bo, BI_field::extract(opcode), tpc, npc, use_direct_block_chaining);
			break;
		}
		case PPC_I(BCCTR):		// Branch Conditional to Count Register
			dg.gen_load_T0_CTR_aligned();
			goto do_branch;
		case PPC_I(BCLR):		// Branch Conditional to Link Register
			dg.gen_load_T0_LR_aligned();
			goto do_branch;
		{
		  do_branch:
			const int bo = BO_field::extract(opcode);
			const int bi = BI_field::extract(opcode);

			const uint32 npc = dpc + 4;
			if (LK_field::test(opcode))
				dg.gen_store_im_LR(npc);

			dg.gen_bc(bo, bi, (uint32)-1, npc, use_direct_block_chaining);
			break;
		}
		case PPC_I(B):			// Branch
			goto do_call;
		{
#if FOLLOW_CONST_JUMPS
		  do_const_jump:
			sync_pc = dpc = op.jmp.target - 4;
			sync_pc_offset = 0;
			if (dpc < min_pc)
				min_pc = dpc;
			else if (dpc > max_pc)
				max_pc = dpc;
			done_compile = false;
			break;
#endif
		  do_call:
			uint32 tpc = AA_field::test(opcode) ? 0 : dpc;
			tpc = (tpc + operand_LI::get(this, opcode)) & -4;

			const uint32 npc = dpc + 4;
			if (LK_field::test(opcode))
				dg.gen_store_im_LR(npc);
#if FOLLOW_CONST_JUMPS
			else {
				op.jmp.target = tpc;
				goto do_const_jump;
			}
#endif

#if DYNGEN_DIRECT_BLOCK_CHAINING
			// Use direct block chaining, addresses will be resolved at execution
			if (direct_chaining_possible(bi->pc, tpc)) {
				use_direct_block_chaining = true;
				bi->li[0].jmp_pc = tpc;
			}
#endif

			// BO field is built so that we always branch to pc
			dg.gen_bc(BO_MAKE(0,0,0,0), 0, tpc, 0, use_direct_block_chaining);
			break;
		}
		case PPC_I(CMP):		// Compare
		{
			dg.gen_load_T0_GPR(rA_field::extract(opcode));
			dg.gen_load_T1_GPR(rB_field::extract(opcode));
			dg.gen_compare_T0_T1(crfD_field::extract(opcode));
			break;
		}
		case PPC_I(CMPI):		// Compare Immediate
		{
			dg.gen_load_T0_GPR(rA_field::extract(opcode));
			dg.gen_compare_T0_im(crfD_field::extract(opcode), operand_SIMM::get(this, opcode));
			break;
		}
		case PPC_I(CMPL):		// Compare Logical
		{
			dg.gen_load_T0_GPR(rA_field::extract(opcode));
			dg.gen_load_T1_GPR(rB_field::extract(opcode));
			dg.gen_compare_logical_T0_T1(crfD_field::extract(opcode));
			break;
		}
		case PPC_I(CMPLI):		// Compare Logical Immediate
		{
			dg.gen_load_T0_GPR(rA_field::extract(opcode));
			dg.gen_compare_logical_T0_im(crfD_field::extract(opcode), operand_UIMM::get(this, opcode));
			break;
		}
		case PPC_I(CRAND):		// Condition Register AND
		case PPC_I(CRANDC):		// Condition Register AND with Complement
		case PPC_I(CREQV):		// Condition Register Equivalent
		case PPC_I(CRNAND):		// Condition Register NAND
		case PPC_I(CRNOR):		// Condition Register NOR
		case PPC_I(CROR):		// Condition Register OR
		case PPC_I(CRORC):		// Condition Register OR with Complement
		case PPC_I(CRXOR):		// Condition Register XOR
		{
			dg.gen_load_T0_crb(crbA_field::extract(opcode));
			dg.gen_load_T1_crb(crbB_field::extract(opcode));
			switch (ii->mnemo) {
			case PPC_I(CRAND):	dg.gen_and_32_T0_T1();	break;
			case PPC_I(CRANDC):	dg.gen_andc_32_T0_T1();	break;
			case PPC_I(CREQV):	dg.gen_eqv_32_T0_T1();	break;
			case PPC_I(CRNAND):	dg.gen_nand_32_T0_T1();	break;
			case PPC_I(CRNOR):	dg.gen_nor_32_T0_T1();	break;
			case PPC_I(CROR):	dg.gen_or_32_T0_T1();	break;
			case PPC_I(CRORC):	dg.gen_orc_32_T0_T1();	break;
			case PPC_I(CRXOR):	dg.gen_xor_32_T0_T1();	break;
			default: abort();
			}
			dg.gen_store_T0_crb(crbD_field::extract(opcode));
			break;
		}
		case PPC_I(AND):		// AND
		case PPC_I(ANDC):		// AND with Complement
		case PPC_I(EQV):		// Equivalent
		case PPC_I(NAND):		// NAND
		case PPC_I(NOR):		// NOR
		case PPC_I(ORC):		// ORC
		case PPC_I(XOR):		// XOR
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			dg.gen_load_T1_GPR(rB_field::extract(opcode));
			switch (ii->mnemo) {
			case PPC_I(AND):	dg.gen_and_32_T0_T1();	break;
			case PPC_I(ANDC):	dg.gen_andc_32_T0_T1();	break;
			case PPC_I(EQV):	dg.gen_eqv_32_T0_T1();	break;
			case PPC_I(NAND):	dg.gen_nand_32_T0_T1();	break;
			case PPC_I(NOR):	dg.gen_nor_32_T0_T1();	break;
			case PPC_I(ORC):	dg.gen_orc_32_T0_T1();	break;
			case PPC_I(XOR):	dg.gen_xor_32_T0_T1();	break;
			default: abort();
			}
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(OR):			// OR
		{
			const int rS = rS_field::extract(opcode);
			const int rB = rB_field::extract(opcode);
			const int rA = rA_field::extract(opcode);
			dg.gen_load_T0_GPR(rS);
			if (rS != rB) {		// Not MR case
				dg.gen_load_T1_GPR(rB);
				dg.gen_or_32_T0_T1();
			}
			dg.gen_store_T0_GPR(rA);
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(ORI):		// OR Immediate
		{
			const int rA = rA_field::extract(opcode);
			const int rS = rS_field::extract(opcode);
			const uint32 val = operand_UIMM::get(this, opcode);
			if (val == 0) {
				if (rA != rS) { // Skip NOP, handle register move
					dg.gen_load_T0_GPR(rS);
					dg.gen_store_T0_GPR(rA);
				}
			}
			else {
				dg.gen_load_T0_GPR(rS);
				dg.gen_or_32_T0_im(val);
				dg.gen_store_T0_GPR(rA);
			}
			break;
		}
		case PPC_I(XORI):		// XOR Immediate
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			dg.gen_xor_32_T0_im(operand_UIMM::get(this, opcode));
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			break;
		}
		case PPC_I(ORIS):		// OR Immediate Shifted
		case PPC_I(XORIS):		// XOR Immediate Shifted
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			uint32 val = operand_UIMM_shifted::get(this, opcode);
			switch (ii->mnemo) {
			case PPC_I(ORIS):	dg.gen_or_32_T0_im(val);	break;
			case PPC_I(XORIS):	dg.gen_xor_32_T0_im(val);	break;
			default: abort();
			}
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			break;
		}
		case PPC_I(ANDI):		// AND Immediate
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			dg.gen_and_32_T0_im(operand_UIMM::get(this, opcode));
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(ANDIS):		// AND Immediate Shifted
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			dg.gen_and_32_T0_im(operand_UIMM_shifted::get(this, opcode));
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(EXTSB):		// Extend Sign Byte
		case PPC_I(EXTSH):		// Extend Sign Half Word
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			switch (ii->mnemo) {
			case PPC_I(EXTSB):	dg.gen_se_8_32_T0();	break;
			case PPC_I(EXTSH):	dg.gen_se_16_32_T0();	break;
			default: abort();
			}
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(NEG):		// Negate
		{
			dg.gen_load_T0_GPR(rA_field::extract(opcode));
			if (OE_field::test(opcode))
				dg.gen_nego_T0();
			else
				dg.gen_neg_32_T0();
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			dg.gen_store_T0_GPR(rD_field::extract(opcode));
			break;
		}
		case PPC_I(MFCR):		// Move from Condition Register
		{
			dg.gen_load_T0_CR();
			dg.gen_store_T0_GPR(rD_field::extract(opcode));
			break;
		}
		case PPC_I(MFSPR):		// Move from Special-Purpose Register
		{
			const int spr = operand_SPR::get(this, opcode);
			switch (spr) {
			case powerpc_registers::SPR_XER:
				dg.gen_load_T0_XER();
				break;
			case powerpc_registers::SPR_LR:
				dg.gen_load_T0_LR();
				break;
			case powerpc_registers::SPR_CTR:
				dg.gen_load_T0_CTR();
				break;
			case powerpc_registers::SPR_VRSAVE:
				dg.gen_load_T0_VRSAVE();
				break;
#ifdef SHEEPSHAVER
			case powerpc_registers::SPR_SDR1:
				dg.gen_mov_32_T0_im(0xdead001f);
				break;
			case powerpc_registers::SPR_PVR: {
				extern uint32 PVR;
				dg.gen_mov_32_T0_im(PVR);
				break;
			}
			default:
				dg.gen_mov_32_T0_im(0);
				break;
#else
			default: goto do_generic;
#endif
			}
			dg.gen_store_T0_GPR(rD_field::extract(opcode));
			break;
		}
		case PPC_I(MTSPR):		// Move to Special-Purpose Register
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			const int spr = operand_SPR::get(this, opcode);
			switch (spr) {
			case powerpc_registers::SPR_XER:
				dg.gen_store_T0_XER();
				break;
			case powerpc_registers::SPR_LR:
				dg.gen_store_T0_LR();
				break;
			case powerpc_registers::SPR_CTR:
				dg.gen_store_T0_CTR();
				break;
			case powerpc_registers::SPR_VRSAVE:
				dg.gen_store_T0_VRSAVE();
				break;
#ifndef SHEEPSHAVER
			default: goto do_generic;
#endif
			}
			break;
		}
		case PPC_I(ADD):		// Add
		case PPC_I(ADDC):		// Add Carrying
		case PPC_I(ADDE):		// Add Extended
		case PPC_I(SUBF):		// Subtract From
		case PPC_I(SUBFC):		// Subtract from Carrying
		case PPC_I(SUBFE):		// Subtract from Extended
		case PPC_I(MULLW):		// Multiply Low Word
		case PPC_I(DIVW):		// Divide Word
		case PPC_I(DIVWU):		// Divide Word Unsigned
		{
			dg.gen_load_T0_GPR(rA_field::extract(opcode));
			dg.gen_load_T1_GPR(rB_field::extract(opcode));
			if (OE_field::test(opcode)) {
				switch (ii->mnemo) {
				case PPC_I(ADD):	dg.gen_addo_T0_T1();	break;
				case PPC_I(ADDC):	dg.gen_addco_T0_T1();	break;
				case PPC_I(ADDE):	dg.gen_addeo_T0_T1();	break;
				case PPC_I(SUBF):	dg.gen_subfo_T0_T1();	break;
				case PPC_I(SUBFC):	dg.gen_subfco_T0_T1();	break;
				case PPC_I(SUBFE):	dg.gen_subfeo_T0_T1();	break;
				case PPC_I(MULLW):	dg.gen_mullwo_T0_T1();	break;
				case PPC_I(DIVW):	dg.gen_divwo_T0_T1();	break;
				case PPC_I(DIVWU):	dg.gen_divwuo_T0_T1();	break;
				default: abort();
				}
			}
			else {
				switch (ii->mnemo) {
				case PPC_I(ADD):	dg.gen_add_32_T0_T1();	break;
				case PPC_I(ADDC):	dg.gen_addc_T0_T1();	break;
				case PPC_I(ADDE):	dg.gen_adde_T0_T1();	break;
				case PPC_I(SUBF):	dg.gen_subf_T0_T1();	break;
				case PPC_I(SUBFC):	dg.gen_subfc_T0_T1();	break;
				case PPC_I(SUBFE):	dg.gen_subfe_T0_T1();	break;
				case PPC_I(MULLW):	dg.gen_umul_32_T0_T1();	break;
				case PPC_I(DIVW):	dg.gen_divw_T0_T1();	break;
				case PPC_I(DIVWU):	dg.gen_divwu_T0_T1();	break;
				default: abort();
				}
			}
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			dg.gen_store_T0_GPR(rD_field::extract(opcode));
			break;
		}
		case PPC_I(ADDIC):		// Add Immediate Carrying
		case PPC_I(ADDIC_):		// Add Immediate Carrying and Record
		case PPC_I(SUBFIC):		// Subtract from Immediate Carrying
		{
			dg.gen_load_T0_GPR(rA_field::extract(opcode));
			const uint32 val = operand_SIMM::get(this, opcode);
			switch (ii->mnemo) {
			case PPC_I(ADDIC):
				dg.gen_addc_T0_im(val);
				break;
			case PPC_I(ADDIC_):
				dg.gen_addc_T0_im(val);
				dg.gen_record_cr0_T0();
				break;
			case PPC_I(SUBFIC):
				dg.gen_subfc_T0_im(val);
				break;
			  defautl:
				abort();
			}
			dg.gen_store_T0_GPR(rD_field::extract(opcode));
			break;
		}
		case PPC_I(ADDME):		// Add to Minus One Extended
		case PPC_I(ADDZE):		// Add to Zero Extended
		case PPC_I(SUBFME):		// Subtract from Minus One Extended
		case PPC_I(SUBFZE):		// Subtract from Zero Extended
		{
			dg.gen_load_T0_GPR(rA_field::extract(opcode));
			if (OE_field::test(opcode)) {
				switch (ii->mnemo) {
				case PPC_I(ADDME):	dg.gen_addmeo_T0();		break;
				case PPC_I(ADDZE):	dg.gen_addzeo_T0();		break;
				case PPC_I(SUBFME):	dg.gen_subfmeo_T0();	break;
				case PPC_I(SUBFZE):	dg.gen_subfzeo_T0();	break;
				default: abort();
				}
			}
			else {
				switch (ii->mnemo) {
				case PPC_I(ADDME):	dg.gen_addme_T0();		break;
				case PPC_I(ADDZE):	dg.gen_addze_T0();		break;
				case PPC_I(SUBFME):	dg.gen_subfme_T0();		break;
				case PPC_I(SUBFZE):	dg.gen_subfze_T0();		break;
				default: abort();
				}
			}
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			dg.gen_store_T0_GPR(rD_field::extract(opcode));
			break;
		}
		case PPC_I(ADDI):		// Add Immediate
		{
			const int rA = rA_field::extract(opcode);
			const int rD = rD_field::extract(opcode);
			if (rA == 0)		// li rD,value
				dg.gen_mov_32_T0_im(operand_SIMM::get(this, opcode));
			else {
				dg.gen_load_T0_GPR(rA);
				dg.gen_add_32_T0_im(operand_SIMM::get(this, opcode));
			}
			dg.gen_store_T0_GPR(rD);
			break;
		}
		case PPC_I(ADDIS):		// Add Immediate Shifted
		{
			const int rA = rA_field::extract(opcode);
			const int rD = rD_field::extract(opcode);
			if (rA == 0)		// lis rD,value
				dg.gen_mov_32_T0_im(operand_SIMM_shifted::get(this, opcode));
			else {
				dg.gen_load_T0_GPR(rA);
				dg.gen_add_32_T0_im(operand_SIMM_shifted::get(this, opcode));
			}
			dg.gen_store_T0_GPR(rD);
			break;
		}
		case PPC_I(RLWIMI):		// Rotate Left Word Immediate then Mask Insert
		{
			const int rA = rA_field::extract(opcode);
			const int rS = rS_field::extract(opcode);
			const int SH = SH_field::extract(opcode);
			const int MB = MB_field::extract(opcode);
			const int ME = ME_field::extract(opcode);
			dg.gen_load_T0_GPR(rA);
			dg.gen_load_T1_GPR(rS);
			const uint32 m = mask_operand::compute(MB, ME);
			dg.gen_rlwimi_T0_T1(SH, m);
			dg.gen_store_T0_GPR(rA);
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(RLWINM):		// Rotate Left Word Immediate then AND with Mask
		{
			const int rS = rS_field::extract(opcode);
			const int rA = rA_field::extract(opcode);
			const int SH = SH_field::extract(opcode);
			const int MB = MB_field::extract(opcode);
			const int ME = ME_field::extract(opcode);
			const uint32 m = mask_operand::compute(MB, ME);
			dg.gen_load_T0_GPR(rS);
			if (MB == 0) {
				if (ME == 31) {
					// rotlwi rA,rS,SH
					if (SH > 0)
						dg.gen_rol_32_T0_im(SH);
				}
				else if (ME == (31 - SH)) {
					// slwi rA,rS,SH
					dg.gen_lsl_32_T0_im(SH);
				}
				else if (SH == 0) {
					// andi rA,rS,MASK(0,ME)
					dg.gen_and_32_T0_im(m);
				}
				else goto do_generic_rlwinm;
			}
			else if (ME == 31) {
				if (SH == (32 - MB)) {
					// srwi rA,rS,SH
					dg.gen_lsr_32_T0_im(MB);
				}
				else if (SH == 0) {
					// andi rA,rS,MASK(MB,31)
					dg.gen_and_32_T0_im(m);
				}
				else goto do_generic_rlwinm;
			}
			else {
				// rlwinm rA,rS,SH,MB,ME
			  do_generic_rlwinm:
				dg.gen_rlwinm_T0_T1(SH, m);
			}
			dg.gen_store_T0_GPR(rA);
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(RLWNM):		// Rotate Left Word then AND with Mask
		{
			const int rS = rS_field::extract(opcode);
			const int rB = rB_field::extract(opcode);
			const int rA = rA_field::extract(opcode);
			const int MB = MB_field::extract(opcode);
			const int ME = ME_field::extract(opcode);
			const uint32 m = mask_operand::compute(MB, ME);
			dg.gen_load_T0_GPR(rS);
			dg.gen_load_T1_GPR(rB);
			if (MB == 0 && ME == 31)
				dg.gen_rol_32_T0_T1();
			else
				dg.gen_rlwnm_T0_T1(m);
			dg.gen_store_T0_GPR(rA);
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(CNTLZW):		// Count Leading Zeros Word
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			dg.gen_cntlzw_32_T0();
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(SLW):		// Shift Left Word
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			dg.gen_load_T1_GPR(rB_field::extract(opcode));
			dg.gen_slw_T0_T1();
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(SRW):		// Shift Right Word
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			dg.gen_load_T1_GPR(rB_field::extract(opcode));
			dg.gen_srw_T0_T1();
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(SRAW):		// Shift Right Algebraic Word
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			dg.gen_load_T1_GPR(rB_field::extract(opcode));
			dg.gen_sraw_T0_T1();
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(SRAWI):		// Shift Right Algebraic Word Immediate
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			dg.gen_sraw_T0_im(SH_field::extract(opcode));
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(MULHW):		// Multiply High Word
		case PPC_I(MULHWU):		// Multiply High Word Unsigned
		{
			dg.gen_load_T0_GPR(rA_field::extract(opcode));
			dg.gen_load_T1_GPR(rB_field::extract(opcode));
			if (ii->mnemo == PPC_I(MULHW))
				dg.gen_mulhw_T0_T1();
			else
				dg.gen_mulhwu_T0_T1();
			dg.gen_store_T0_GPR(rD_field::extract(opcode));
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(MULLI):		// Multiply Low Immediate
		{
			dg.gen_load_T0_GPR(rA_field::extract(opcode));
			dg.gen_mulli_T0_im(operand_SIMM::get(this, opcode));
			dg.gen_store_T0_GPR(rD_field::extract(opcode));
			break;
		}
		case PPC_I(DCBZ):		// Data Cache Block Clear to Zero
		{
			const int rA = rA_field::extract(opcode);
			const int rB = rB_field::extract(opcode);
			if (rA == 0)
				dg.gen_load_T0_GPR(rB);
			else {
				dg.gen_load_T0_GPR(rA);
				dg.gen_load_T1_GPR(rB);
				dg.gen_add_32_T0_T1();
			}
			dg.gen_dcbz_T0();
			break;
		}
		case PPC_I(DCBA):		// Data Cache Block Allocate
		case PPC_I(DCBF):		// Data Cache Block Flush
		case PPC_I(DCBI):		// Data Cache Block Invalidate
		case PPC_I(DCBST):		// Data Cache Block Store
		case PPC_I(DCBT):		// Data Cache Block Touch
		case PPC_I(DCBTST):		// Data Cache Block Touch for Store
		case PPC_I(ECIWX):		// External Control In Word Indexed
		case PPC_I(ECOWX):		// External Control Out Word Indexed
		case PPC_I(EIEIO):		// Enforce In-Order Execution of I/O
		case PPC_I(SYNC):		// Synchronize
		{
			break;
		}
		case PPC_I(ISYNC):		// Instruction synchronize
		{
			typedef void (*func_t)(dyngen_cpu_base);
			func_t func = (func_t)nv_mem_fun(&powerpc_cpu::execute_invalidate_cache_range).ptr();
			dg.gen_invoke_CPU(func);
			break;
		}
		case PPC_I(MTCRF):		// Move to Condition Register Fields
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			dg.gen_mtcrf_T0_im(field2mask[CRM_field::extract(opcode)]);
			break;
		}
		case PPC_I(MCRF):		// Move Condition Register Field
		{
			dg.gen_load_T0_crf(crfS_field::extract(opcode));
			dg.gen_store_T0_crf(crfD_field::extract(opcode));
			break;
		}
		case PPC_I(LFD):		// Load Floating-Point Double
			op.mem.size = 8;
			op.mem.do_update = 0;
			op.mem.do_indexed = 0;
			goto do_fp_load;;
		case PPC_I(LFDU):		// Load Floating-Point Double with Update
			op.mem.size = 8;
			op.mem.do_update = 1;
			op.mem.do_indexed = 0;
			goto do_fp_load;
		case PPC_I(LFDUX):		// Load Floating-Point Double with Update Indexed
			op.mem.size = 8;
			op.mem.do_update = 1;
			op.mem.do_indexed = 1;
			goto do_fp_load;
		case PPC_I(LFDX):		// Load Floating-Point Double Indexed
			op.mem.size = 8;
			op.mem.do_update = 0;
			op.mem.do_indexed = 1;
			goto do_fp_load;
		case PPC_I(LFS):		// Load Floating-Point Single
			op.mem.size = 4;
			op.mem.do_update = 0;
			op.mem.do_indexed = 0;
			goto do_fp_load;
		case PPC_I(LFSU):		// Load Floating-Point Single with Update
			op.mem.size = 4;
			op.mem.do_update = 1;
			op.mem.do_indexed = 0;
			goto do_fp_load;
		case PPC_I(LFSUX):		// Load Floating-Point Single with Update Indexed
			op.mem.size = 4;
			op.mem.do_update = 1;
			op.mem.do_indexed = 1;
			goto do_fp_load;
		case PPC_I(LFSX):		// Load Floating-Point Single Indexed
			op.mem.size = 4;
			op.mem.do_update = 0;
			op.mem.do_indexed = 1;
			goto do_fp_load;
		{
		  do_fp_load:
			// Extract RZ operand
			const int rA = rA_field::extract(opcode);
			if (rA == 0 && !op.mem.do_update)
				dg.gen_mov_32_T1_im(0);
			else
				dg.gen_load_T1_GPR(rA);

			// Extract index operand
			if (op.mem.do_indexed)
				dg.gen_load_T2_GPR(rB_field::extract(opcode));

			// Load floating point data
			if (op.mem.size == 8) {
				if (op.mem.do_indexed)
					dg.gen_load_double_FD_T1_T2();
				else
					dg.gen_load_double_FD_T1_im(operand_D::get(this, opcode));
			}
			else {
				if (op.mem.do_indexed)
					dg.gen_load_single_FD_T1_T2();
				else
					dg.gen_load_single_FD_T1_im(operand_D::get(this, opcode));
			}

			// Commit result
			dg.gen_store_FD_FPR(frD_field::extract(opcode));

			// Update RA
			if (op.mem.do_update) {
				if (op.mem.do_indexed)
					dg.gen_add_32_T1_T2();
				else
					dg.gen_add_32_T1_im(operand_D::get(this, opcode));
				dg.gen_store_T1_GPR(rA);
			}
			break;
		}
		case PPC_I(STFD):		// Store Floating-Point Double
			op.mem.size = 8;
			op.mem.do_update = 0;
			op.mem.do_indexed = 0;
			goto do_fp_store;
		case PPC_I(STFDU):		// Store Floating-Point Double with Update
			op.mem.size = 8;
			op.mem.do_update = 1;
			op.mem.do_indexed = 0;
			goto do_fp_store;
		case PPC_I(STFDUX):		// Store Floating-Point Double with Update Indexed
			op.mem.size = 8;
			op.mem.do_update = 1;
			op.mem.do_indexed = 1;
			goto do_fp_store;
		case PPC_I(STFDX):		// Store Floating-Point Double Indexed
			op.mem.size = 8;
			op.mem.do_update = 0;
			op.mem.do_indexed = 1;
			goto do_fp_store;
		case PPC_I(STFS):		// Store Floating-Point Single
			op.mem.size = 4;
			op.mem.do_update = 0;
			op.mem.do_indexed = 0;
			goto do_fp_store;
		case PPC_I(STFSU):		// Store Floating-Point Single with Update
			op.mem.size = 4;
			op.mem.do_update = 1;
			op.mem.do_indexed = 0;
			goto do_fp_store;
		case PPC_I(STFSUX):		// Store Floating-Point Single with Update Indexed
			op.mem.size = 4;
			op.mem.do_update = 1;
			op.mem.do_indexed = 1;
			goto do_fp_store;
		case PPC_I(STFSX):		// Store Floating-Point Single Indexed
			op.mem.size = 4;
			op.mem.do_update = 0;
			op.mem.do_indexed = 1;
			goto do_fp_store;
		{
		  do_fp_store:
			// Extract RZ operand
			const int rA = rA_field::extract(opcode);
			if (rA == 0 && !op.mem.do_update)
				dg.gen_mov_32_T1_im(0);
			else
				dg.gen_load_T1_GPR(rA);

			// Extract index operand
			if (op.mem.do_indexed)
				dg.gen_load_T2_GPR(rB_field::extract(opcode));

			// Load register to commit to memory
			dg.gen_load_F0_FPR(frS_field::extract(opcode));

			// Store floating point data
			if (op.mem.size == 8) {
				if (op.mem.do_indexed)
					dg.gen_store_double_F0_T1_T2();
				else
					dg.gen_store_double_F0_T1_im(operand_D::get(this, opcode));
			}
			else {
				if (op.mem.do_indexed)
					dg.gen_store_single_F0_T1_T2();
				else
					dg.gen_store_single_F0_T1_im(operand_D::get(this, opcode));
			}

			// Update RA
			if (op.mem.do_update) {
				if (op.mem.do_indexed)
					dg.gen_add_32_T1_T2();
				else
					dg.gen_add_32_T1_im(operand_D::get(this, opcode));
				dg.gen_store_T1_GPR(rA);
			}
			break;
		}
#if PPC_ENABLE_FPU_EXCEPTIONS == 0
		case PPC_I(FABS):		// Floating Absolute Value
		case PPC_I(FNABS):		// Floating Negative Absolute Value
		case PPC_I(FNEG):		// Floating Negate
		case PPC_I(FMR):		// Floating Move Register
		{
			dg.gen_load_F0_FPR(frB_field::extract(opcode));
			switch (ii->mnemo) {
			case PPC_I(FABS):  dg.gen_fabs_FD_F0();  break;
			case PPC_I(FNABS): dg.gen_fnabs_FD_F0(); break;
			case PPC_I(FNEG):  dg.gen_fneg_FD_F0();  break;
			case PPC_I(FMR):   dg.gen_fmov_FD_F0(); break;
			}
			dg.gen_store_FD_FPR(frD_field::extract(opcode));
			if (Rc_field::test(opcode))
				dg.gen_record_cr1();
			break;
		}
		case PPC_I(FADD):		// Floating Add (Double-Precision)
		case PPC_I(FSUB):		// Floating Subtract (Double-Precision)
		case PPC_I(FMUL):		// Floating Multiply (Double-Precision)
		case PPC_I(FDIV):		// Floating Divide (Double-Precision)
		case PPC_I(FADDS):		// Floating Add (Single-Precision)
		case PPC_I(FSUBS):		// Floating Subtract (Single-Precision)
		case PPC_I(FMULS):		// Floating Multiply (Single-Precision)
		case PPC_I(FDIVS):		// Floating Divide (Single-Precision)
		{
			dg.gen_load_F0_FPR(frA_field::extract(opcode));
			if (ii->mnemo == PPC_I(FMUL) || ii->mnemo == PPC_I(FMULS))
				dg.gen_load_F1_FPR(frC_field::extract(opcode));
			else
				dg.gen_load_F1_FPR(frB_field::extract(opcode));
			switch (ii->mnemo) {
			case PPC_I(FADD): dg.gen_fadd_FD_F0_F1(); break;
			case PPC_I(FSUB): dg.gen_fsub_FD_F0_F1(); break;
			case PPC_I(FMUL): dg.gen_fmul_FD_F0_F1(); break;
			case PPC_I(FDIV): dg.gen_fdiv_FD_F0_F1(); break;
			case PPC_I(FADDS): dg.gen_fadds_FD_F0_F1(); break;
			case PPC_I(FSUBS): dg.gen_fsubs_FD_F0_F1(); break;
			case PPC_I(FMULS): dg.gen_fmuls_FD_F0_F1(); break;
			case PPC_I(FDIVS): dg.gen_fdivs_FD_F0_F1(); break;
			}
			dg.gen_store_FD_FPR(frD_field::extract(opcode));
			if (Rc_field::test(opcode))
				dg.gen_record_cr1();
			break;
		}
		case PPC_I(FMADD):		// Floating Multiply-Add (Double-Precision)
		case PPC_I(FMSUB):		// Floating Multiply-Subtract (Double-Precision)
		case PPC_I(FNMADD):		// Floating Negative Multiply-Add (Double-Precision)
		case PPC_I(FNMSUB):		// Floating Negative Multiply-Subract (Double-Precision)
		case PPC_I(FMADDS):		// Floating Multiply-Add (Single-Precision)
		case PPC_I(FMSUBS):		// Floating Multiply-Subtract (Single-Precision)
		case PPC_I(FNMADDS):	// Floating Negative Multiply-Add (Single-Precision)
		case PPC_I(FNMSUBS):	// Floating Negative Multiply-Subract (Single-Precision)
		{
			dg.gen_load_F0_FPR(frA_field::extract(opcode));
			dg.gen_load_F1_FPR(frC_field::extract(opcode));
			dg.gen_load_F2_FPR(frB_field::extract(opcode));
			switch (ii->mnemo) {
			case PPC_I(FMADD): dg.gen_fmadd_FD_F0_F1_F2(); break;
			case PPC_I(FMSUB): dg.gen_fmsub_FD_F0_F1_F2(); break;
			case PPC_I(FNMADD): dg.gen_fnmadd_FD_F0_F1_F2(); break;
			case PPC_I(FNMSUB): dg.gen_fnmsub_FD_F0_F1_F2(); break;
			case PPC_I(FMADDS): dg.gen_fmadds_FD_F0_F1_F2(); break;
			case PPC_I(FMSUBS): dg.gen_fmsubs_FD_F0_F1_F2(); break;
			case PPC_I(FNMADDS): dg.gen_fnmadds_FD_F0_F1_F2(); break;
			case PPC_I(FNMSUBS): dg.gen_fnmsubs_FD_F0_F1_F2(); break;
			}
			dg.gen_store_FD_FPR(frD_field::extract(opcode));
			if (Rc_field::test(opcode))
				dg.gen_record_cr1();
			break;
		}
#endif
		case PPC_I(LVEWX):
		case PPC_I(LVX):
		case PPC_I(LVXL):
		case PPC_I(STVEWX):
		case PPC_I(STVX):
		case PPC_I(STVXL):
			assert(vD_field::mask() == vS_field::mask());
			assert(vA_field::mask() == rA_field::mask());
			assert(vB_field::mask() == rB_field::mask());
			// fall-through
		case PPC_I(VCMPEQFP):
		case PPC_I(VCMPEQUB):
		case PPC_I(VCMPEQUH):
		case PPC_I(VCMPEQUW):
		case PPC_I(VCMPGEFP):
		case PPC_I(VCMPGTFP):
		case PPC_I(VCMPGTSB):
		case PPC_I(VCMPGTSH):
		case PPC_I(VCMPGTSW):
		{
			const int vD = vD_field::extract(opcode);
			const int vA = vA_field::extract(opcode);
			const int vB = vB_field::extract(opcode);
			if (!dg.gen_vector_compare(ii->mnemo, vD, vA, vB, vRc_field::test(opcode)))
				goto do_generic;
			break;
		}
		case PPC_I(VADDFP):
		case PPC_I(VADDUBM):
		case PPC_I(VADDUHM):
		case PPC_I(VADDUWM):
		case PPC_I(VAND):
		case PPC_I(VANDC):
		case PPC_I(VAVGUB):
		case PPC_I(VAVGUH):
		case PPC_I(VMAXSH):
		case PPC_I(VMAXUB):
		case PPC_I(VMINSH):
		case PPC_I(VMINUB):
		case PPC_I(VNOR):
		case PPC_I(VOR):
		case PPC_I(VSUBFP):
		case PPC_I(VSUBUBM):
		case PPC_I(VSUBUHM):
		case PPC_I(VSUBUWM):
		case PPC_I(VXOR):
		case PPC_I(VREFP):
		case PPC_I(VRSQRTEFP):
		{
			const int vD = vD_field::extract(opcode);
			const int vA = vA_field::extract(opcode);
			const int vB = vB_field::extract(opcode);
			if (!dg.gen_vector_2(ii->mnemo, vD, vA, vB))
				goto do_generic;
			break;
		}
		case PPC_I(VSEL):
		case PPC_I(VMADDFP):
		case PPC_I(VNMSUBFP):
		{
			const int vD = vD_field::extract(opcode);
			const int vA = vA_field::extract(opcode);
			const int vB = vB_field::extract(opcode);
			const int vC = vC_field::extract(opcode);
			if (!dg.gen_vector_3(ii->mnemo, vD, vA, vB, vC))
				goto do_generic;
			break;
		}
		case PPC_I(VSLDOI):
		{
			const int vD = vD_field::extract(opcode);
			const int vA = vA_field::extract(opcode);
			const int vB = vB_field::extract(opcode);
			const int SH = vSH_field::extract(opcode);
			if (!dg.gen_vector_3(ii->mnemo, vD, vA, vB, SH))
				goto do_generic;
			break;
		}
		case PPC_I(MFVSCR):
		{
			if (!dg.gen_vector_1(ii->mnemo, vD_field::extract(opcode)))
				goto do_generic;
			break;
		}
		case PPC_I(MTVSCR):
		{
			if (!dg.gen_vector_1(ii->mnemo, vB_field::extract(opcode)))
				goto do_generic;
			break;
		}
		case PPC_I(VSPLTISB):
		case PPC_I(VSPLTISH):
		case PPC_I(VSPLTISW):
		{
			const int vD = vD_field::extract(opcode);
			const int SIMM = op_sign_extend_5_32::apply(vUIMM_field::extract(opcode));
			if (!dg.gen_vector_2(ii->mnemo, vD, SIMM, 0))
				goto do_generic;
			break;
		}
		case PPC_I(VSPLTB):
		case PPC_I(VSPLTH):
		case PPC_I(VSPLTW):
		{
			const int vD = vD_field::extract(opcode);
			const int UIMM = vUIMM_field::extract(opcode);
			const int vB = vB_field::extract(opcode);
			if (!dg.gen_vector_2(ii->mnemo, vD, UIMM, vB))
				goto do_generic;
			break;
		}
		default:				// Direct call to instruction handler
		{
			typedef void (*func_t)(dyngen_cpu_base, uint32);
			func_t func;
		  do_generic:
			func = (func_t)ii->execute.ptr();
			goto do_invoke;
		  do_illegal:
			func = (func_t)nv_mem_fun(&powerpc_cpu::execute_illegal).ptr();
			goto do_invoke;	
		  do_invoke:
#if PPC_PROFILE_GENERIC_CALLS
			if (ii->mnemo <= PPC_I(MAX)) {
				uintptr mem = (uintptr)&generic_calls_count[ii->mnemo];
				if (mem <= 0xffffffff)
					dg.gen_inc_32_mem(mem);
			}
#endif
			cg_context.pc = dpc;
			cg_context.opcode = opcode;
			cg_context.instr_info = ii;
			cg_context.done_compile = done_compile;
			compile_status = compile1(cg_context);
			switch (compile_status) {
			case COMPILE_FAILURE:
			case COMPILE_EPILOGUE_OK:
				if ((dpc - sync_pc) > sync_pc_offset) {
					sync_pc = dpc;
					sync_pc_offset = 0;
					if (compile_status == COMPILE_EPILOGUE_OK)
						break;
					dg.gen_set_PC_im(dpc);
				}
				sync_pc_offset += 4;
				dg.gen_invoke_CPU_im(func, opcode);
				compile_status = COMPILE_CODE_OK; // could generate code, though a call to handler
				break;
			}
			done_compile = cg_context.done_compile;
		}
		}
		if (dg.full_translation_cache()) {
			// Invalidate cache and start again
			invalidate_cache();
			goto again;
		}
	}
	// Do nothing if block has special epilogue code generated already
	assert(compile_status != COMPILE_FAILURE);
	if (compile_status != COMPILE_EPILOGUE_OK) {
		// In direct block chaining mode, this code is reached only if
		// there are pending spcflags, i.e. get out of this block
		if (!use_direct_block_chaining) {
			// TODO: optimize this to a direct jump to pregenerated code?
			dg.gen_mov_ad_A0_im((uintptr)bi);
			dg.gen_jump_next_A0();
		}
		dg.gen_exec_return();
	}
	bi->end_pc = dpc;
	if (dpc < min_pc)
		min_pc = dpc;
	else if (dpc > max_pc)
		max_pc = dpc;
	bi->min_pc = min_pc;
	bi->max_pc = max_pc;

#if DYNGEN_DIRECT_BLOCK_CHAINING
	// Generate backpatch trampolines
	if (use_direct_block_chaining) {
		typedef void *(*func_t)(dyngen_cpu_base);
		func_t func = (func_t)nv_mem_fun(&powerpc_cpu::compile_chain_block).ptr();
		for (int i = 0; i < block_info::MAX_TARGETS; i++) {
			if (bi->li[i].jmp_pc != block_info::INVALID_PC) {
				uint8 *p = dg.gen_align(16);
				dg.gen_mov_ad_A0_im(((uintptr)bi) | i);
				dg.gen_invoke_CPU_A0_ret_A0(func);
				dg.gen_jmp_A0();
				assert(dg.jmp_addr[i] != NULL);
				bi->li[i].jmp_addr = dg.jmp_addr[i];
				bi->li[i].jmp_resolve_addr = p;
				dg_set_jmp_target_noflush(bi->li[i].jmp_addr, bi->li[i].jmp_resolve_addr);
			}
		}
	}
#endif

	bi->size = dg.code_ptr() - bi->entry_point;
	if (disasm)
		disasm_translation(entry_point, dpc - entry_point + 4, bi->entry_point, bi->size);

	dg.gen_end();
	block_cache.add_to_cl_list(bi);
	if (is_read_only_memory(bi->pc))
		block_cache.add_to_dormant_list(bi);
	else
		block_cache.add_to_active_list(bi);
#if PPC_PROFILE_COMPILE_TIME
	compile_time += (clock() - start_time);
#endif
	return bi;
}
#endif
