/*
 *  ppc-bitfields.hpp - Instruction fields
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

#ifndef PPC_BITFIELDS_H
#define PPC_BITFIELDS_H

#include "cpu/ppc/ppc-operations.hpp"

///
///		Bitfield management
///

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

template< class type, type value >
struct fake_bit_field {
	static inline bool test(uint32) {
		return value;
	}
	static inline type extract(uint32) {
		return value;
	}
};

///
///		Instruction Fields
///

// Primary and extended opcode fields
typedef bit_field<  0,  5 > OPCD_field;
typedef bit_field< 21, 30 > XO_10_field;
typedef bit_field< 22, 30 > XO_9_field;
typedef bit_field< 26, 30 > XO_5_field;

// General purpose registers
typedef bit_field< 11, 15 > rA_field;
typedef bit_field< 16, 20 > rB_field;
typedef bit_field<  6, 10 > rD_field;
typedef bit_field<  6, 10 > rS_field;

// Floating-point registers
typedef bit_field< 11, 15 > frA_field;
typedef bit_field< 16, 20 > frB_field;
typedef bit_field< 21, 25 > frC_field;
typedef bit_field<  6, 10 > frD_field;
typedef bit_field<  6, 10 > frS_field;

// Vector registers
typedef bit_field< 11, 15 > vA_field;
typedef bit_field< 16, 20 > vB_field;
typedef bit_field< 21, 25 > vC_field;
typedef bit_field<  6, 10 > vD_field;
typedef bit_field<  6, 10 > vS_field;

typedef bit_field< 21, 21 > vRc_field;
typedef bit_field< 11, 15 > vUIMM_field;
typedef bit_field< 22, 25 > vSH_field;

// Condition registers
typedef bit_field< 11, 15 > crbA_field;
typedef bit_field< 16, 20 > crbB_field;
typedef bit_field<  6, 10 > crbD_field;
typedef bit_field<  6,  8 > crfD_field;
typedef bit_field< 11, 13 > crfS_field;
typedef bit_field< 12, 19 > CRM_field;
typedef bit_field<  7, 14 > FM_field;

// CR register fields
template< int CRn > struct CR_field : bit_field< 4*CRn+0, 4*CRn+3 > { };
template< int CRn > struct CR_LT_field : bit_field< 4*CRn+0, 4*CRn+0 > { };
template< int CRn > struct CR_GT_field : bit_field< 4*CRn+1, 4*CRn+1 > { };
template< int CRn > struct CR_EQ_field : bit_field< 4*CRn+2, 4*CRn+2 > { };
template< int CRn > struct CR_SO_field : bit_field< 4*CRn+3, 4*CRn+3 > { };
template< int CRn > struct CR_UN_field : bit_field< 4*CRn+3, 4*CRn+3 > { };

// Aliases used for CR storage optimization
typedef CR_LT_field<7> standalone_CR_LT_field;
typedef CR_GT_field<7> standalone_CR_GT_field;
typedef CR_EQ_field<7> standalone_CR_EQ_field;
typedef CR_SO_field<7> standalone_CR_SO_field;

// XER register fields
typedef bit_field<  0,  0 > XER_SO_field;
typedef bit_field<  1,  1 > XER_OV_field;
typedef bit_field<  2,  2 > XER_CA_field;
typedef bit_field< 25, 31 > XER_COUNT_field;

// FPSCR register fields
typedef bit_field<  0,  0 > FPSCR_FX_field;
typedef bit_field<  1,  1 > FPSCR_FEX_field;
typedef bit_field<  2,  2 > FPSCR_VX_field;
typedef bit_field<  3,  3 > FPSCR_OX_field;
typedef bit_field<  4,  4 > FPSCR_UX_field;
typedef bit_field<  5,  5 > FPSCR_ZX_field;
typedef bit_field<  6,  6 > FPSCR_XX_field;
typedef bit_field<  7,  7 > FPSCR_VXSNAN_field;
typedef bit_field<  8,  8 > FPSCR_VXISI_field;
typedef bit_field<  9,  9 > FPSCR_VXIDI_field;
typedef bit_field< 10, 10 > FPSCR_VXZDZ_field;
typedef bit_field< 11, 11 > FPSCR_VXIMZ_field;
typedef bit_field< 12, 12 > FPSCR_VXVC_field;
typedef bit_field< 13, 13 > FPSCR_FR_field;
typedef bit_field< 14, 14 > FPSCR_FI_field;
typedef bit_field< 15, 19 > FPSCR_FPRF_field;
typedef bit_field< 21, 21 > FPSCR_VXSOFT_field;
typedef bit_field< 22, 22 > FPSCR_VXSQRT_field;
typedef bit_field< 23, 23 > FPSCR_VXCVI_field;
typedef bit_field< 24, 24 > FPSCR_VE_field;
typedef bit_field< 25, 25 > FPSCR_OE_field;
typedef bit_field< 26, 26 > FPSCR_UE_field;
typedef bit_field< 27, 27 > FPSCR_ZE_field;
typedef bit_field< 28, 28 > FPSCR_XE_field;
typedef bit_field< 29, 29 > FPSCR_NI_field;
typedef bit_field< 30, 31 > FPSCR_RN_field;
typedef bit_field< 16, 19 > FPSCR_FPCC_field;
typedef bit_field< 15, 15 > FPSCR_FPRF_C_field;  // C
typedef bit_field< 16, 16 > FPSCR_FPRF_FL_field; // <
typedef bit_field< 17, 17 > FPSCR_FPRF_FG_field; // >
typedef bit_field< 18, 18 > FPSCR_FPRF_FE_field; // =
typedef bit_field< 19, 19 > FPSCR_FPRF_FU_field; // ?

// Vector Status and Control Register
typedef bit_field< 15, 15 > VSCR_NJ_field;
typedef bit_field< 31, 31 > VSCR_SAT_field;

// Define variations for branch instructions
typedef bit_field< 30, 30 > AA_field;
typedef bit_field< 31, 31 > LK_field;
typedef bit_field< 16, 29 > BD_field;
typedef bit_field< 11, 15 > BI_field;
typedef bit_field<  6, 10 > BO_field;

// Helper macros to deal with BO field
#define BO_MAKE(COND, TRUE, DCTR, CTR0)	(((COND) ? 0 : 16) | ((TRUE) ? 8 : 0) | ((DCTR) ? 0 : 4) | ((CTR0) ? 2 : 0))
#define BO_DECREMENT_CTR(BO)			(((BO) & 0x04) == 0)
#define BO_BRANCH_IF_CTR_ZERO(BO)		(((BO) & 0x02) != 0)
#define BO_CONDITIONAL_BRANCH(BO)		(((BO) & 0x10) == 0)
#define BO_BRANCH_IF_TRUE(BO)			(((BO) & 0x08) != 0)

// Define variations for ALU instructions
typedef bit_field< 31, 31 > Rc_field;
typedef bit_field< 21, 21 > OE_field;
typedef bit_field< 21, 25 > MB_field;
typedef bit_field< 26, 30 > ME_field;
typedef bit_field< 16, 20 > NB_field;
typedef bit_field< 16, 20 > SH_field;

// Immediates
typedef bit_field< 16, 19 > IMM_field;
typedef bit_field< 16, 31 > d_field;
typedef bit_field<  6, 29 > LI_field;
typedef bit_field< 16, 31 > SIMM_field;
typedef bit_field< 16, 31 > UIMM_field;

// Misc
typedef bit_field< 12, 15 > SR_field;
typedef bit_field<  6, 10 > TO_field;
typedef bit_field< 11, 20 > SPR_field;
typedef bit_field< 11, 20 > TBR_field;

// Aliases to ease filling in decode table
#define DEFINE_FAKE_FIELD_ALIAS(NAME)				\
typedef fake_bit_field<bool, false>		NAME##_0;	\
typedef fake_bit_field<bool, true>		NAME##_1

#define DEFINE_FIELD_ALIAS(NAME, FIELD)				\
typedef FIELD##_field					NAME##_G;	\
DEFINE_FAKE_FIELD_ALIAS(NAME)

DEFINE_FAKE_FIELD_ALIAS(CA_BIT);
DEFINE_FIELD_ALIAS(RC_BIT, Rc);
DEFINE_FIELD_ALIAS(OE_BIT, OE);
DEFINE_FIELD_ALIAS(AA_BIT, AA);
DEFINE_FIELD_ALIAS(LK_BIT, LK);
DEFINE_FIELD_ALIAS(BO_BIT, BO);
DEFINE_FIELD_ALIAS(BI_BIT, BI);
DEFINE_FIELD_ALIAS(vRC_BIT, vRc);

#undef DEFINE_FIELD_ALIAS
#undef DEFINE_FAKE_FIELD_ALIAS

typedef fake_bit_field<int, -1>			RA_FIELD_A; // GPR(RA)
typedef rA_field						RA_FIELD_G; // RA ? GPR(RA) : 0
typedef fake_bit_field<int, 0>			RA_FIELD_0; // R0 -> 0

#endif /* PPC_BITFIELDS_H */
