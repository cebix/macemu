/*
 *  ppc-operands.hpp - PowerPC operands definition
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

#ifndef PPC_OPERANDS_H
#define PPC_OPERANDS_H

/**
 *		General purpose registers
 **/

template< class field, class op >
struct input_gpr_op {
	static inline uint32 get(powerpc_cpu * cpu, uint32 opcode) {
		return op::apply(cpu->gpr(field::extract(opcode)));
	}
};

template< class field >
struct input_gpr : input_gpr_op< field, op_nop > { };

template< class field >
struct output_gpr {
	static inline void set(powerpc_cpu * cpu, uint32 opcode, uint32 value) {
		cpu->gpr(field::extract(opcode)) = value;
	}
};

template< class field >
struct gpr_operand : input_gpr< field >, output_gpr< field > { };

template< class field, int except_regno >
struct input_gpr_except {
	static inline uint32 get(powerpc_cpu * cpu, uint32 opcode) {
		const int regno = field::extract(opcode);
		return regno != except_regno ? cpu->gpr(regno) : 0;
	};
	static inline void set(powerpc_cpu * cpu, uint32 opcode, uint32 value) {
		const int regno = field::extract(opcode);
		if (regno == except_regno) abort();
		cpu->gpr(regno) = value;
	}
};

/**
 *		Floating-point registers
 **/

template< class field >
struct input_fpr {
	static inline double get(powerpc_cpu * cpu, uint32 opcode) {
		return cpu->fpr(field::extract(opcode));
	}
};

template< class field >
struct output_fpr {
	static inline void set(powerpc_cpu * cpu, uint32 opcode, double value) {
		cpu->fpr(field::extract(opcode)) = value;
	}
};

template< class field >
struct fpr_operand : input_fpr< field >, output_fpr< field > { };

/**
 *		Immediate operands
 **/

struct null_operand {
	static inline uint32 get(powerpc_cpu *, uint32) {
		return 0;
	}
};
template< class field, class operation >
struct immediate_operand {
	static inline uint32 get(powerpc_cpu *, uint32 opcode) {
		return operation::apply(field::extract(opcode));
	}
};

template< int32 N >
struct immediate_value {
	static inline uint32 get(powerpc_cpu *, uint32) {
		return (uint32)N;
	}
};

struct mask_operand {
	static inline uint32 get(powerpc_cpu *, uint32 opcode) {
		const uint32 mb = MB_field::extract(opcode);
		const uint32 me = ME_field::extract(opcode);
		return ((mb > me) ?
				~(((uint32)-1 >> mb) ^ ((me >= 31) ? 0 : (uint32)-1 >> (me + 1))) :
				(((uint32)-1 >> mb) ^ ((me >= 31) ? 0 : (uint32)-1 >> (me + 1))));
	}
};

/**
 *		Special purpose registers
 **/

struct pc_operand {
	static inline uint32 get(powerpc_cpu * cpu, uint32) {
		return cpu->pc();
	};
};

struct lr_operand {
	static inline uint32 get(powerpc_cpu * cpu, uint32) {
		return cpu->lr();
	};
};

struct ctr_operand {
	static inline uint32 get(powerpc_cpu * cpu, uint32) {
		return cpu->ctr();
	};
};

struct cr_operand {
	static inline uint32 get(powerpc_cpu * cpu, uint32) {
		return cpu->cr().get();
	}
};

template< class field >
struct xer_operand {
	static inline uint32 get(powerpc_cpu * cpu, uint32) {
		return field::extract(cpu->xer().get());
	}
};

template<>
struct xer_operand<XER_CA_field> {
	static inline uint32 get(powerpc_cpu * cpu, uint32) {
		return cpu->xer().get_ca();
	}
};

template<>
struct xer_operand<XER_COUNT_field> {
	static inline uint32 get(powerpc_cpu * cpu, uint32) {
		return cpu->xer().get_count();
	}
};

template< class field >
struct fpscr_operand {
	static inline uint32 get(powerpc_cpu * cpu, uint32) {
		return field::extract(cpu->fpscr());
	}
};

struct spr_operand {
	static inline uint32 get(powerpc_cpu *, uint32 opcode) {
		uint32 spr = SPR_field::extract(opcode);
		return ((spr & 0x1f) << 5) | ((spr >> 5) & 0x1f);
	}
};

struct tbr_operand {
	static inline uint32 get(powerpc_cpu *, uint32 opcode) {
		uint32 tbr = TBR_field::extract(opcode);
		return ((tbr & 0x1f) << 5) | ((tbr >> 5) & 0x1f);
	}
};


/**
 *		Operand aliases for decode table
 **/

typedef null_operand							operand_NONE;
typedef cr_operand								operand_CR;
typedef pc_operand								operand_PC;
typedef lr_operand								operand_LR;
typedef ctr_operand								operand_CTR;
typedef input_gpr_op<rA_field, op_compl>		operand_RA_compl;
typedef gpr_operand<rA_field>					operand_RA;
typedef gpr_operand<rB_field>					operand_RB;
typedef gpr_operand<rS_field>					operand_RS;
typedef gpr_operand<rD_field>					operand_RD;
typedef input_gpr_except<rA_field, 0>			operand_RA_or_0; // RA ? GPR(RA) : 0
typedef immediate_value<0>						operand_RA_is_0; // RA -> 0
typedef immediate_value<1>						operand_ONE;
typedef immediate_value<0>						operand_ZERO;
typedef immediate_value<-1>						operand_MINUS_ONE;
typedef null_operand							operand_fp_NONE;
typedef fpr_operand<frA_field>					operand_fp_RA;
typedef fpr_operand<frB_field>					operand_fp_RB;
typedef fpr_operand<frC_field>					operand_fp_RC;
typedef fpr_operand<frD_field>					operand_fp_RD;
typedef fpr_operand<frS_field>					operand_fp_RS;
typedef xer_operand<XER_CA_field>				operand_XER_CA;
typedef xer_operand<XER_COUNT_field>			operand_XER_COUNT;
typedef fpscr_operand<FPSCR_RN_field>			operand_FPSCR_RN;
typedef spr_operand								operand_SPR;
typedef tbr_operand								operand_TBR;
typedef mask_operand							operand_MASK;

#define DEFINE_IMMEDIATE_OPERAND(NAME, FIELD, OP) \
typedef immediate_operand<FIELD##_field, op_##OP> operand_##NAME

DEFINE_IMMEDIATE_OPERAND(LI, LI, sign_extend_LI_32);
DEFINE_IMMEDIATE_OPERAND(BO, BO, nop);
DEFINE_IMMEDIATE_OPERAND(BD, BD, sign_extend_BD_32);
DEFINE_IMMEDIATE_OPERAND(IMM, IMM, nop);
DEFINE_IMMEDIATE_OPERAND(UIMM, UIMM, zero_extend_16_32);
DEFINE_IMMEDIATE_OPERAND(UIMM_shifted, UIMM, zero_extend_16_32_shifted);
DEFINE_IMMEDIATE_OPERAND(SIMM, SIMM, sign_extend_16_32);
DEFINE_IMMEDIATE_OPERAND(SIMM_shifted, SIMM, sign_extend_16_32_shifted);
DEFINE_IMMEDIATE_OPERAND(D, d, sign_extend_16_32);
DEFINE_IMMEDIATE_OPERAND(NB, NB, nop);
DEFINE_IMMEDIATE_OPERAND(SH, SH, nop);
DEFINE_IMMEDIATE_OPERAND(FM, FM, nop);

#undef DEFINE_IMMEDIATE_OPERAND

#endif /* PPC_OPERANDS_H */
