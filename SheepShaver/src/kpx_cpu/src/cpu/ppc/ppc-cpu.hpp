/*
 *  ppc-cpu.hpp - PowerPC CPU definition
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

#ifndef PPC_CPU_H
#define PPC_CPU_H

#include "basic-cpu.hpp"
#include "nvmemfun.hpp"
#include "cpu/vm.hpp"
#include "cpu/block-cache.hpp"
#include "cpu/ppc/ppc-config.hpp"
#include "cpu/ppc/ppc-bitfields.hpp"
#include "cpu/ppc/ppc-blockinfo.hpp"
#include "cpu/ppc/ppc-registers.hpp"
#include "cpu/ppc/ppc-jit.hpp"
#include "cpu/ppc/ppc-instructions.hpp"
#include <vector>

class powerpc_cpu
#ifndef SHEEPSHAVER
	: public basic_cpu
#endif
{
	// NOTE: PowerPC registers structure shall be aligned on 16-byte
	// boundaries for the AltiVec registers to be used in native code
	// with aligned load/stores.
	//
	// We can't assume (offsetof(powerpc_cpu, regs) % 16) == 0 since
	// extra data could be inserted prior regs, e.g. pointer to vtable
	struct {
		powerpc_registers regs;
		uint8 pad[16];
	} _regs;

	// Make sure the calculation of the current offset makes use of
	// 'this' as this could make it simplified at compile-time
	powerpc_registers *regs_ptr() const			{ return (powerpc_registers *)((char *)&_regs.regs + (16 - (((char *)&_regs.regs - (char *)this) % 16))); }
	powerpc_registers const & regs() const		{ return *regs_ptr(); }
	powerpc_registers & regs()					{ return *regs_ptr(); }

#if PPC_PROFILE_REGS_USE
	// Registers use statistics
	// NOTE: the emulator is designed to access registers only through
	// the gpr() accessors. The number of calls to gpr() matches exactly
	// the number of register operands for an instruction.
public:
	struct register_info {
		int id;
		uint64 count;
	};
private:
	register_info *reginfo;
	void log_reg(int r) const { reginfo[r].count++; }
#else
	void log_reg(int r) const { }
#endif

protected:

	powerpc_spcflags & spcflags() { return regs().spcflags; }
	powerpc_spcflags const & spcflags() const { return regs().spcflags; }
	powerpc_cr_register & cr() { return regs().cr; }
	powerpc_cr_register const & cr() const { return regs().cr; }
	powerpc_xer_register & xer() { return regs().xer; }
	powerpc_xer_register const & xer() const { return regs().xer; }
	powerpc_vscr & vscr() { return regs().vscr; }
	powerpc_vscr const & vscr() const { return regs().vscr; }

	uint32 vrsave() const		{ return regs().vrsave; }
	uint32 & vrsave()			{ return regs().vrsave; }

	uint32 & fpscr()			{ return regs().fpscr; }
	uint32 fpscr() const		{ return regs().fpscr; }
	uint32 & lr()				{ return regs().lr; }
	uint32 lr() const			{ return regs().lr; }
	uint32 & ctr()				{ return regs().ctr; }
	uint32 ctr() const			{ return regs().ctr; }
	uint32 & pc()				{ return regs().pc; }
	uint32 pc() const			{ return regs().pc; }
	void increment_pc(int o)	{ pc() += o; }

	friend class pc_operand;
	friend class lr_operand;
	friend class ctr_operand;
	friend class cr_operand;
	template< class field > friend class xer_operand;
	template< class field > friend class fpscr_operand;

public:

	uint32 & gpr(int i)			{ log_reg(i); return regs().gpr[i]; }
	uint32 gpr(int i) const		{ log_reg(i); return regs().gpr[i]; }
	double & fpr(int i)			{ return regs().fpr[i].d; }
	double fpr(int i) const		{ return regs().fpr[i].d; }
	uint64 & fpr_dw(int i)		{ return regs().fpr[i].j; }
	uint64 fpr_dw(int i) const	{ return regs().fpr[i].j; }
	powerpc_vr & vr(int i)		{ return regs().vr[i]; }
	powerpc_vr const & vr(int i) const { return regs().vr[i]; }

protected:

	// Condition codes management
	void record_cr(int crfd, int32 value)
		{ cr().compute(crfd, value); cr().set_so(crfd, xer().get_so()); }
	void record_cr0(int32 value)
		{ record_cr(0, value); }
	void record_cr1()
		{ cr().set((cr().get() & ~CR_field<1>::mask()) | ((fpscr() >> 4) & 0x0f000000)); }
	void record_fpscr(int exceptions);
	void record_cr6(powerpc_vr const & vS, bool check_one) {
		if (check_one && (vS.j[0] == UVAL64(0xffffffffffffffff) &&
						  vS.j[1] == UVAL64(0xffffffffffffffff)))
			cr().set(6, 8);
		else if (vS.j[0] == UVAL64(0) && vS.j[1] == UVAL64(0))
			cr().set(6, 2);
		else
			cr().set(6, 0);
	}

	template< class FP >
	void fp_classify(FP x);

protected:

	// Flight recorder
	struct rec_step {
#if PPC_FLIGHT_RECORDER >= 2
		uint32 r[32];
		double fr[32];
		uint32 lr, ctr;
		uint32 cr, xer;
		uint32 fpscr;
#endif
		uint32 pc;
		uint32 opcode;
#ifdef SHEEPSHAVER
		uint32 sp;
		uint32 r24;
#endif
	};

	// Instruction formats
	enum instr_format_t {
		INVALID_form = 0,
		A_form,
		B_form,
		D_form, DS_form,
		I_form,
		M_form,
		MD_form, MDS_form,
		SC_form,
		X_form,
		XFL_form, XFX_form, XL_form, XO_form, XS_form,
		VX_form, VXR_form, VA_form,
	};

	// Control flow types
	enum control_flow_t {
		CFLOW_NORMAL		= 0,
		CFLOW_BRANCH		= 1,
		CFLOW_JUMP			= 2,
		CFLOW_TRAP			= 4,
		CFLOW_CONST_JUMP	= 8,
		CFLOW_END_BLOCK		= CFLOW_BRANCH | CFLOW_JUMP | CFLOW_TRAP
	};

	// Callbacks associated with each instruction
	typedef void (powerpc_cpu::*execute_pmf)(uint32 opcode);
	typedef nv_mem_fun1_t< void, powerpc_cpu, uint32 > execute_fn;

	// Instruction information structure
	struct instr_info_t {
		char			name[12];		// Instruction name
		execute_fn		execute;		// Semantic routine for this instruction
		uint16			mnemo;			// Mnemonic
		uint16			format;			// Instruction format (XO-form, D-form, etc.)
		uint16			opcode;			// Primary opcode
		uint16			xo;				// Extended opcode
		uint16			cflow;			// Mask of control flow information
	};

private:

	// Compile time statistics
#if PPC_PROFILE_COMPILE_TIME
	uint32 compile_count;
	clock_t compile_time;
	clock_t emul_start_time;
#endif

	// Compile blocks statistics
#if PPC_PROFILE_GENERIC_CALLS
	friend int generic_calls_compare(const void *, const void *);
	static uint32 generic_calls_count[PPC_I(MAX)];
#endif

	// Flight recorder data
	static const int LOG_SIZE = 32768;
#if PPC_FLIGHT_RECORDER
	rec_step log[LOG_SIZE];
	bool logging;
	int log_ptr;
	bool log_ptr_wrapped;
#else
	static const bool logging = false;
#endif
	void do_record_step(uint32 pc, uint32 opcode);
	void record_step(uint32 opcode) { do_record_step(pc(), opcode); }

	// Syscall callback must return TRUE if no error occurred
	typedef bool (*syscall_fn)(powerpc_cpu *cpu);
	syscall_fn execute_do_syscall;

	static const instr_info_t powerpc_ii_table[];
	std::vector<instr_info_t> ii_table;
	typedef uint16 ii_index_t;
	static const int II_INDEX_TABLE_SIZE = 0x20000;
	ii_index_t ii_index_table[II_INDEX_TABLE_SIZE];

	// Pack/unpack index into decode table
	uint32 make_ii_index(uint32 opcode, uint32 xo) { return opcode | (xo << 6); }
	uint32 get_ii_index(uint32 opcode) { return (opcode >> 26) | ((opcode & 0x7ff) << 6); }

	// Convert 8-bit field mask (e.g. mtcrf) to bit mask
	uint32 field2mask[256];

	// Check special CPU flags
	bool check_spcflags();

	// Current execute() nested level
	int execute_depth;

public:

	// Initialization & finalization
	void initialize();
#ifdef SHEEPSHAVER
	powerpc_cpu();
#else
	powerpc_cpu(task_struct *parent_task);
#endif
	~powerpc_cpu();

	// Specialised memory allocation (needs to be 16-byte aligned)
	void *operator new(size_t size);
	void operator delete(void *p);

	// Handle flight recorder
#if PPC_FLIGHT_RECORDER
	bool is_logging() const { return logging; }
	void start_log();
	void stop_log();
	void dump_log(const char *filename = NULL);
#else
	bool is_logging() const { return false; }
	void start_log() { }
	void stop_log() { }
	void dump_log(const char *filename = NULL) { }
#endif

	// Dump registers
	void dump_registers();
	void dump_instruction(uint32 opcode);
	void fake_dump_registers(uint32);

	// Start emulation loop
	void execute(uint32 entry);
	void execute();

	// Interrupts handling
	void trigger_interrupt();
	
	// Set VALUE to register ID
	void set_register(int id, any_register const & value);

	// Get register ID
	any_register get_register(int id);

	// Set syscall callback
	void set_syscall_callback(syscall_fn fn) { execute_do_syscall = fn; }

	// Caches invalidation
	void invalidate_cache();
	void invalidate_cache_range(uintptr start, uintptr end);
private:
	struct { uintptr start, end; } cache_range;

protected:

	// Init decoder with one instruction info
	void init_decoder_entry(const instr_info_t * ii);

#if PPC_ENABLE_JIT
	// Dynamic translation engine
	struct codegen_context_t {
		powerpc_dyngen &	codegen;
		uint32				entry_point;
		uint32				pc;
		uint32				opcode;
		const instr_info_t *instr_info;
		bool				done_compile;

		codegen_context_t(powerpc_dyngen & codegen_init)
			: codegen(codegen_init)
			{ }
	};

	// Compile one opcode, returns any of the following status
	enum {
		COMPILE_FAILURE,		// no translation available, call interpreter
		COMPILE_CODE_OK,		// generated code, control flow fall through
		COMPILE_EPILOGUE_OK		// generated code, including basic block epilogue
	};
	virtual int compile1(codegen_context_t & cg_context) { return COMPILE_FAILURE; }

	bool use_jit;
public:
	void enable_jit(uint32 cache_size = 0);
#endif

private:

	// Initializers & destructors
	void init_flight_recorder();
	void init_registers();
	void init_decoder();
	void init_decode_cache();
	void kill_decode_cache();

	// Get instruction info for opcode
	const instr_info_t *decode(uint32 opcode) {
		return &ii_table[ii_index_table[get_ii_index(opcode)]];
	}

	// Block lookup table
	typedef powerpc_block_info block_info;
	block_cache< block_info, lazy_allocator > block_cache;

#if PPC_DECODE_CACHE
	// Decode Cache
	static const uint32 DECODE_CACHE_MAX_ENTRIES = 32768;
	static const uint32 DECODE_CACHE_SIZE = DECODE_CACHE_MAX_ENTRIES * sizeof(block_info::decode_info);
	block_info::decode_info * decode_cache;
	block_info::decode_info * decode_cache_p;
	block_info::decode_info * decode_cache_end_p;
#endif

#if PPC_ENABLE_JIT
	// Dynamic translation engine
	friend class powerpc_dyngen_helper;
	friend class powerpc_dyngen;
	friend class powerpc_jit;
	powerpc_jit codegen;
	block_info *compile_block(uint32 entry);
#if DYNGEN_DIRECT_BLOCK_CHAINING
	void *compile_chain_block(block_info *sbi);
#endif
#endif

	// Semantic action templates
	template< bool SB, bool OE >
	uint32 do_execute_divide(uint32, uint32);
	template< bool EX, bool CA, bool OE >
	uint32 do_execute_addition(uint32, uint32);
	template< bool CA, bool OE >
	uint32 do_execute_subtract(uint32, uint32);
	template< bool OE >
	uint32 do_execute_subtract_extended(uint32, uint32);

	// Instruction handlers
	void execute_nop(uint32 opcode);
	void execute_illegal(uint32 opcode);
	template< class RA, class RB, class RC, class CA, class OE, class Rc >
	void execute_addition(uint32 opcode);
	template< class OP, class RD, class RA, class RB, class RC, class OE, class Rc >
	void execute_generic_arith(uint32 opcode);
	template< class PC, class BO, class DP, class AA, class LK >
	void execute_branch(uint32 opcode);
	template< class RB, typename CT >
	void execute_compare(uint32 opcode);
	template< class OP >
	void execute_cr_op(uint32 opcode);
	template< bool SB, class OE, class Rc >
	void execute_divide(uint32 opcode);
	template< class FP, class OP, class RD, class RA, class RB, class RC, class Rc, bool FPSCR >
	void execute_fp_arith(uint32 opcode);
 	template< class OP, class RA, class RB, bool LD, int SZ, bool UP, bool RX >
	void execute_loadstore(uint32 opcode);
	template< class RA, class DP, bool LD >
	void execute_loadstore_multiple(uint32 opcode);
	template< class RA, bool IM, class NB >
	void execute_load_string(uint32 opcode);
	template< class RA, bool IM, class NB >
	void execute_store_string(uint32 opcode);
	template< class RA >
	void execute_lwarx(uint32 opcode);
	template< class RA >
	void execute_stwcx(uint32 opcode);
	void execute_mcrf(uint32 opcode);
	void execute_mcrfs(uint32 opcode);
	void execute_mcrxr(uint32 opcode);
	void execute_mtcrf(uint32 opcode);
	template< class FM, class RB, class Rc >
	void execute_mtfsf(uint32 opcode);
	template< class RB, class Rc >
	void execute_mtfsfi(uint32 opcode);
	template< class RB, class Rc >
	void execute_mtfsb(uint32 opcode);
	template< bool HI, bool SB, class OE, class Rc >
	void execute_multiply(uint32 opcode);
	template< class Rc >
	void execute_mffs(uint32 opcode);
	void execute_mfmsr(uint32 opcode);
	template< class SPR >
	void execute_mfspr(uint32 opcode);
	template< class TBR >
	void execute_mftbr(uint32 opcode);
	template< class SPR >
	void execute_mtspr(uint32 opcode);
	template< class SH, class MA, class Rc >
	void execute_rlwimi(uint32 opcode);
	template< class OP, class RD, class RA, class SH, class SO, class CA, class Rc >
	void execute_shift(uint32 opcode);
	void execute_syscall(uint32 opcode);
	template< bool OC >
	void execute_fp_compare(uint32 opcode);
 	template< class RA, class RB, bool LD, bool DB, bool UP >
	void execute_fp_loadstore(uint32 opcode);
	template< class RN, class Rc >
	void execute_fp_int_convert(uint32 opcode);
	template< class Rc >
	void execute_fp_round(uint32 opcode);
	template< class RA, class RB >
	void execute_icbi(uint32 opcode);
	void execute_isync(uint32 opcode);
	void execute_invalidate_cache_range();
	template< class RA, class RB >
	void execute_dcbz(uint32 opcode);
	template< bool SL >
	void execute_vector_load_for_shift(uint32 opcode);
	template< class VD, class RA, class RB >
	void execute_vector_load(uint32 opcode);
	template< class VS, class RA, class RB >
	void execute_vector_store(uint32 opcode);
	void execute_mfvscr(uint32 opcode);
	void execute_mtvscr(uint32 opcode);
	template< class OP, class VD, class VA, class VB, class VC, class Rc, int C1 >
	void execute_vector_arith(uint32 opcode);
	template< class OP, class VD, class VA, class VB, class VC >
	void execute_vector_arith_mixed(uint32 opcode);
	template< int ODD, class OP, class VD, class VA, class VB, class VC >
	void execute_vector_arith_odd(uint32 opcode);
	template< class VD, class VA, class VB, int LO >
	void execute_vector_merge(uint32 opcode);
	template< class VD, class VA, class VB >
	void execute_vector_pack(uint32 opcode);
	void execute_vector_pack_pixel(uint32 opcode);
	template< int LO >
	void execute_vector_unpack_pixel(uint32 opcode);
	template< int LO, class VD, class VA >
	void execute_vector_unpack(uint32 opcode);
	void execute_vector_permute(uint32 opcode);
	template< int SD >
	void execute_vector_shift(uint32 opcode);
	template< int SD, class VD, class VA, class VB, class SH >
	void execute_vector_shift_octet(uint32 opcode);
	template< class OP, class VD, class VB, bool IM >
	void execute_vector_splat(uint32 opcode);
	template< int SZ, class VD, class VA, class VB >
	void execute_vector_sum(uint32 opcode);

	// Specialized instruction decoders
	template< class RA, class RB, class RC, class CA >
	execute_fn decode_addition(uint32 opcode);
	template< class RA, class RS >
	execute_fn decode_rlwinm(uint32 opcode);
};


/**
 *	Interrupts handling
 **/

inline void powerpc_cpu::trigger_interrupt()
{
#if PPC_CHECK_INTERRUPTS
	spcflags().set(SPCFLAG_CPU_TRIGGER_INTERRUPT);
#endif
}

#ifdef SHEEPSHAVER
extern void HandleInterrupt(powerpc_registers *r);
#endif

#endif /* PPC_CPU_H */
