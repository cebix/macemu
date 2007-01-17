/*
 *  ppc-decode.cpp - PowerPC instructions decoder
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

#include "cpu/ppc/ppc-cpu.hpp"
#include "cpu/ppc/ppc-bitfields.hpp"
#include "cpu/ppc/ppc-operands.hpp"
#include "cpu/ppc/ppc-operations.hpp"
#include "cpu/ppc/ppc-instructions.hpp"

#define DEBUG 0
#include "debug.h"

#define EXECUTE_0(HANDLER) \
&powerpc_cpu::execute_##HANDLER

#define EXECUTE_1(HANDLER, ARG1) \
&powerpc_cpu::execute_##HANDLER<ARG1>

#define EXECUTE_2(HANDLER, ARG1, ARG2) \
&powerpc_cpu::execute_##HANDLER<ARG1, ARG2>

#define EXECUTE_3(HANDLER, ARG1, ARG2, ARG3) \
&powerpc_cpu::execute_##HANDLER<ARG1, ARG2, ARG3>

#define EXECUTE_4(HANDLER, ARG1, ARG2, ARG3, ARG4) \
&powerpc_cpu::execute_##HANDLER<ARG1, ARG2, ARG3, ARG4>

#define EXECUTE_7(HANDLER, ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7) \
&powerpc_cpu::execute_##HANDLER<ARG1, ARG2, ARG3, ARG4, ARG5, ARG6, ARG7>

#define EXECUTE_ADDITION(RA, RB, RC, CA, OE, Rc) \
&powerpc_cpu::execute_addition<operand_##RA, operand_##RB, operand_##RC, CA, OE, Rc>

#define EXECUTE_GENERIC_ARITH(OP, RD, RA, RB, RC, OE, Rc) \
&powerpc_cpu::execute_generic_arith<op_##OP, operand_##RD, operand_##RA, operand_##RB, operand_##RC, OE, Rc>

#define EXECUTE_BRANCH(PC, BO, DP, AA, LK) \
&powerpc_cpu::execute_branch<operand_##PC, BO, operand_##DP, AA, LK>

#define EXECUTE_COMPARE(RB, CT) \
&powerpc_cpu::execute_compare<operand_##RB, CT>

#define EXECUTE_CR_OP(OP) \
&powerpc_cpu::execute_cr_op<op_##OP>

#define EXECUTE_FP_ARITH(FP, OP, RD, RA, RB, RC, Rc, FPSCR) \
&powerpc_cpu::execute_fp_arith<FP, op_##OP, operand_fp_##RD, operand_fp_##RA, operand_fp_##RB, operand_fp_##RC, Rc, FPSCR>

#define EXECUTE_LOADSTORE(OP, RA, RB, LD, SZ, UP, RX) \
&powerpc_cpu::execute_loadstore<op_##OP, operand_##RA, operand_##RB, LD, SZ, UP, RX>

#define EXECUTE_LOADSTORE_MULTIPLE(RA, DP, LD) \
&powerpc_cpu::execute_loadstore_multiple<operand_##RA, operand_##DP, LD>

#define EXECUTE_LOAD_STRING(RA, IM, NB) \
&powerpc_cpu::execute_load_string<operand_##RA, IM, operand_##NB>

#define EXECUTE_STORE_STRING(RA, IM, NB) \
&powerpc_cpu::execute_store_string<operand_##RA, IM, operand_##NB>

#define EXECUTE_SHIFT(OP, RD, RA, SH, SO, CA, Rc) \
&powerpc_cpu::execute_shift<op_##OP, operand_##RD, operand_##RA, operand_##SH, op_##SO, CA, Rc>

#define EXECUTE_FP_LOADSTORE(RA, RB, LD, DB, UP) \
&powerpc_cpu::execute_fp_loadstore<operand_##RA, operand_##RB, LD, DB, UP>

#define EXECUTE_VECTOR_LOADSTORE(OP, VD, RA, RB) \
&powerpc_cpu::execute_vector_##OP<operand_vD_##VD, operand_##RA, operand_##RB>

#define EXECUTE_VECTOR_ARITH(OP, VD, VA, VB, VC) \
&powerpc_cpu::execute_vector_arith<op_##OP, operand_vD_##VD, operand_vA_##VA, operand_vB_##VB, operand_vC_##VC, fake_bit_field< bool, false >, 0 >

#define EXECUTE_VECTOR_ARITH_MIXED(OP, VD, VA, VB, VC) \
&powerpc_cpu::execute_vector_arith_mixed<op_##OP, operand_vD_##VD, operand_vA_##VA, operand_vB_##VB, operand_vC_##VC>

#define EXECUTE_VECTOR_ARITH_ODD(ODD, OP, VD, VA, VB, VC) \
&powerpc_cpu::execute_vector_arith_odd<ODD, op_##OP, operand_vD_##VD, operand_vA_##VA, operand_vB_##VB, operand_vC_##VC>

#define EXECUTE_VECTOR_MERGE(VD, VA, VB, LO) \
&powerpc_cpu::execute_vector_merge<operand_vD_##VD, operand_vA_##VA, operand_vB_##VB, LO>

#define EXECUTE_VECTOR_COMPARE(OP, VD, VA, VB, C1) \
&powerpc_cpu::execute_vector_arith<op_##OP, operand_vD_##VD, operand_vA_##VA, operand_vB_##VB, operand_vC_NONE, vRC_BIT_G, C1>

#define EXECUTE_VECTOR_PACK(VD, VA, VB) \
&powerpc_cpu::execute_vector_pack<operand_vD_##VD, operand_vA_##VA, operand_vB_##VB>

#define EXECUTE_VECTOR_UNPACK(LO, VD, VB) \
&powerpc_cpu::execute_vector_unpack<LO, operand_vD_##VD, operand_vB_##VB>

#define EXECUTE_VECTOR_SHIFT_OCTET(SD, VD, VA, VB, SH) \
&powerpc_cpu::execute_vector_shift_octet<SD, operand_vD_##VD, operand_vA_##VA, operand_vB_##VB, operand_##SH>

#define EXECUTE_VECTOR_SPLAT(OP, VD, VB, IM) \
&powerpc_cpu::execute_vector_splat<op_##OP, operand_vD_##VD, operand_vB_##VB, IM>

#define EXECUTE_VECTOR_SUM(SZ, VD, VA, VB) \
&powerpc_cpu::execute_vector_sum<SZ, operand_vD_##VD, operand_vA_##VA, operand_vB_##VB>

const powerpc_cpu::instr_info_t powerpc_cpu::powerpc_ii_table[] = {
	{ "invalid",
	  EXECUTE_0(illegal),
	  PPC_I(INVALID),
	  INVALID_form, 0, 0, CFLOW_TRAP
	},
	{ "add",
	  EXECUTE_ADDITION(RA, RB, NONE, CA_BIT_0, OE_BIT_G, RC_BIT_G),
	  PPC_I(ADD),
	  XO_form, 31, 266, CFLOW_NORMAL
	},
	{ "addc",
	  EXECUTE_ADDITION(RA, RB, NONE, CA_BIT_1, OE_BIT_G, RC_BIT_G),
	  PPC_I(ADDC),
	  XO_form, 31,  10, CFLOW_NORMAL
	},
	{ "adde",
	  EXECUTE_ADDITION(RA, RB, XER_CA, CA_BIT_1, OE_BIT_G, RC_BIT_G),
	  PPC_I(ADDE),
	  XO_form, 31, 138, CFLOW_NORMAL
	},
	{ "addi",
	  EXECUTE_ADDITION(RA_or_0, SIMM, NONE, CA_BIT_0, OE_BIT_0, RC_BIT_0),
	  PPC_I(ADDI),
	  D_form, 14, 0, CFLOW_NORMAL
	},
	{ "addic",
	  EXECUTE_ADDITION(RA, SIMM, NONE, CA_BIT_1, OE_BIT_0, RC_BIT_0),
	  PPC_I(ADDIC),
	  D_form, 12, 0, CFLOW_NORMAL
	},
	{ "addic.",
	  EXECUTE_ADDITION(RA, SIMM, NONE, CA_BIT_1, OE_BIT_0, RC_BIT_1),
	  PPC_I(ADDIC_),
	  D_form, 13, 0, CFLOW_NORMAL
	},
	{ "addis",
	  EXECUTE_ADDITION(RA_or_0, SIMM_shifted, NONE, CA_BIT_0, OE_BIT_0, RC_BIT_0),
	  PPC_I(ADDIS),
	  D_form, 15, 0, CFLOW_NORMAL
	},
	{ "addme",
	  EXECUTE_ADDITION(RA, MINUS_ONE, XER_CA, CA_BIT_1, OE_BIT_G, RC_BIT_G),
	  PPC_I(ADDME),
	  XO_form, 31, 234, CFLOW_NORMAL
	},
	{ "addze",
	  EXECUTE_ADDITION(RA, ZERO, XER_CA, CA_BIT_1, OE_BIT_G, RC_BIT_G),
	  PPC_I(ADDZE),
	  XO_form, 31, 202, CFLOW_NORMAL
	},
	{ "and",
	  EXECUTE_GENERIC_ARITH(and, RA, RS, RB, NONE, OE_BIT_0, RC_BIT_G),
	  PPC_I(AND),
	  X_form, 31, 28, CFLOW_NORMAL
	},
	{ "andc",
	  EXECUTE_GENERIC_ARITH(andc, RA, RS, RB, NONE, OE_BIT_0, RC_BIT_G),
	  PPC_I(ANDC),
	  X_form, 31, 60, CFLOW_NORMAL
	},
	{ "andi.",
	  EXECUTE_GENERIC_ARITH(and, RA, RS, UIMM, NONE, OE_BIT_0, RC_BIT_1),
	  PPC_I(ANDI),
	  D_form, 28, 0, CFLOW_NORMAL
	},
	{ "andis.",
	  EXECUTE_GENERIC_ARITH(and, RA, RS, UIMM_shifted, NONE, OE_BIT_0, RC_BIT_1),
	  PPC_I(ANDIS),
	  D_form, 29, 0, CFLOW_NORMAL
	},
	{ "b",
	  EXECUTE_BRANCH(PC, immediate_value<BO_MAKE(0,0,0,0)>, LI, AA_BIT_G, LK_BIT_G),
	  PPC_I(B),
	  I_form, 18, 0, CFLOW_BRANCH
	},
	{ "bc",
	  EXECUTE_BRANCH(PC, operand_BO, BD, AA_BIT_G, LK_BIT_G),
	  PPC_I(BC),
	  B_form, 16, 0, CFLOW_BRANCH
	},
	{ "bcctr",
	  EXECUTE_BRANCH(CTR, operand_BO, ZERO, AA_BIT_0, LK_BIT_G),
	  PPC_I(BCCTR),
	  XL_form, 19, 528, CFLOW_BRANCH
	},
	{ "bclr",
	  EXECUTE_BRANCH(LR, operand_BO, ZERO, AA_BIT_0, LK_BIT_G),
	  PPC_I(BCLR),
	  XL_form, 19, 16, CFLOW_BRANCH
	},
	{ "cmp",
	  EXECUTE_COMPARE(RB, int32),
	  PPC_I(CMP),
	  X_form, 31, 0, CFLOW_NORMAL
	},
	{ "cmpi",
	  EXECUTE_COMPARE(SIMM, int32),
	  PPC_I(CMPI),
	  D_form, 11, 0, CFLOW_NORMAL
	},
	{ "cmpl",
	  EXECUTE_COMPARE(RB, uint32),
	  PPC_I(CMPL),
	  X_form, 31, 32, CFLOW_NORMAL
	},
	{ "cmpli",
	  EXECUTE_COMPARE(UIMM, uint32),
	  PPC_I(CMPLI),
	  D_form, 10, 0, CFLOW_NORMAL
	},
	{ "cntlzw",
	  EXECUTE_GENERIC_ARITH(cntlzw, RA, RS, NONE, NONE, OE_BIT_0, RC_BIT_G),
	  PPC_I(CNTLZW),
	  X_form, 31, 26, CFLOW_NORMAL
	},
	{ "crand",
	  EXECUTE_CR_OP(and),
	  PPC_I(CRAND),
	  XL_form, 19, 257, CFLOW_NORMAL
	},
	{ "crandc",
	  EXECUTE_CR_OP(andc),
	  PPC_I(CRANDC),
	  XL_form, 19, 129, CFLOW_NORMAL
	},
	{ "creqv",
	  EXECUTE_CR_OP(eqv),
	  PPC_I(CREQV),
	  XL_form, 19, 289, CFLOW_NORMAL
	},
	{ "crnand",
	  EXECUTE_CR_OP(nand),
	  PPC_I(CRNAND),
	  XL_form, 19, 225, CFLOW_NORMAL
	},
	{ "crnor",
	  EXECUTE_CR_OP(nor),
	  PPC_I(CRNOR),
	  XL_form, 19, 33, CFLOW_NORMAL
	},
	{ "cror",
	  EXECUTE_CR_OP(or),
	  PPC_I(CROR),
	  XL_form, 19, 449, CFLOW_NORMAL
	},
	{ "crorc",
	  EXECUTE_CR_OP(orc),
	  PPC_I(CRORC),
	  XL_form, 19, 417, CFLOW_NORMAL
	},
	{ "crxor",
	  EXECUTE_CR_OP(xor),
	  PPC_I(CRXOR),
	  XL_form, 19, 193, CFLOW_NORMAL
	},
	{ "dcba",
	  EXECUTE_0(nop),
	  PPC_I(DCBA),
	  X_form, 31, 758, CFLOW_NORMAL
	},
	{ "dcbf",
	  EXECUTE_0(nop),
	  PPC_I(DCBF),
	  X_form, 31, 86, CFLOW_NORMAL
	},
	{ "dcbi",
	  EXECUTE_0(nop),
	  PPC_I(DCBI),
	  X_form, 31, 470, CFLOW_NORMAL
	},
	{ "dcbst",
	  EXECUTE_0(nop),
	  PPC_I(DCBST),
	  X_form, 31, 54, CFLOW_NORMAL
	},
	{ "dcbt",
	  EXECUTE_0(nop),
	  PPC_I(DCBT),
	  X_form, 31, 278, CFLOW_NORMAL
	},
	{ "dcbtst",
	  EXECUTE_0(nop),
	  PPC_I(DCBTST),
	  X_form, 31, 246, CFLOW_NORMAL
	},
	{ "dcbz",
	  EXECUTE_2(dcbz, operand_RA_or_0, operand_RB),
	  PPC_I(DCBZ),
	  X_form, 31, 1014, CFLOW_NORMAL
	},
	{ "divw",
	  EXECUTE_3(divide, true, OE_BIT_G, RC_BIT_G),
	  PPC_I(DIVW),
	  XO_form, 31, 491, CFLOW_NORMAL
	},
	{ "divwu",
	  EXECUTE_3(divide, false, OE_BIT_G, RC_BIT_G),
	  PPC_I(DIVWU),
	  XO_form, 31, 459, CFLOW_NORMAL
	},
	{ "dss",
	  EXECUTE_0(nop),
	  PPC_I(DSS),
	  X_form, 31, 822, CFLOW_NORMAL
	},
	{ "dst",
	  EXECUTE_0(nop),
	  PPC_I(DST),
	  X_form, 31, 342, CFLOW_NORMAL
	},
	{ "dstst",
	  EXECUTE_0(nop),
	  PPC_I(DST),
	  X_form, 31, 374, CFLOW_NORMAL
	},
	{ "eciwx",
	  EXECUTE_0(nop),
	  PPC_I(ECIWX),
	  X_form, 31, 310, CFLOW_NORMAL
	},
	{ "ecowx",
	  EXECUTE_0(nop),
	  PPC_I(ECOWX),
	  X_form, 31, 438, CFLOW_NORMAL
	},
	{ "eieio",
	  EXECUTE_0(nop),
	  PPC_I(EIEIO),
	  X_form, 31, 854, CFLOW_NORMAL
	},
	{ "eqv",
	  EXECUTE_GENERIC_ARITH(eqv, RA, RS, RB, NONE, OE_BIT_0, RC_BIT_G),
	  PPC_I(EQV),
	  X_form, 31, 284, CFLOW_NORMAL
	},
	{ "extsb",
	  EXECUTE_GENERIC_ARITH(sign_extend_8_32, RA, RS, NONE, NONE, OE_BIT_0, RC_BIT_G),
	  PPC_I(EXTSB),
	  X_form, 31, 954, CFLOW_NORMAL
	},
	{ "extsh",
	  EXECUTE_GENERIC_ARITH(sign_extend_16_32, RA, RS, NONE, NONE, OE_BIT_0, RC_BIT_G),
	  PPC_I(EXTSH),
	  X_form, 31, 922, CFLOW_NORMAL
	},
	{ "fabs",
	  EXECUTE_FP_ARITH(double, fabs, RD, RB, NONE, NONE, RC_BIT_G, false),
	  PPC_I(FABS),
	  X_form, 63, 264, CFLOW_NORMAL
	},
	{ "fadd",
	  EXECUTE_FP_ARITH(double, fadd, RD, RA, RB, NONE, RC_BIT_G, true),
	  PPC_I(FADD),
	  A_form, 63, 21, CFLOW_NORMAL
	},
	{ "fadds",
	  EXECUTE_FP_ARITH(float, fadd, RD, RA, RB, NONE, RC_BIT_G, true),
	  PPC_I(FADDS),
	  A_form, 59, 21, CFLOW_NORMAL
	},
	{ "fcmpo",
	  EXECUTE_1(fp_compare, true),
	  PPC_I(FCMPO),
	  X_form, 63, 32, CFLOW_NORMAL
	},
	{ "fcmpu",
	  EXECUTE_1(fp_compare, false),
	  PPC_I(FCMPU),
	  X_form, 63, 0, CFLOW_NORMAL
	},
	{ "fctiw",
	  EXECUTE_2(fp_int_convert, operand_FPSCR_RN, RC_BIT_G),
	  PPC_I(FCTIW),
	  X_form, 63, 14, CFLOW_NORMAL
	},
	{ "fctiwz",
	  EXECUTE_2(fp_int_convert, operand_ONE, RC_BIT_G),
	  PPC_I(FCTIWZ),
	  X_form, 63, 15, CFLOW_NORMAL
	},
	{ "fdiv",
	  EXECUTE_FP_ARITH(double, fdiv, RD, RA, RB, NONE, RC_BIT_G, true),
	  PPC_I(FDIV),
	  A_form, 63, 18, CFLOW_NORMAL
	},
	{ "fdivs",
	  EXECUTE_FP_ARITH(float, fdiv, RD, RA, RB, NONE, RC_BIT_G, true),
	  PPC_I(FDIVS),
	  A_form, 59, 18, CFLOW_NORMAL
	},
	{ "fmadd",
	  EXECUTE_FP_ARITH(double, fmadd, RD, RA, RC, RB, RC_BIT_G, true),
	  PPC_I(FMADD),
	  A_form, 63, 29, CFLOW_NORMAL
	},
	{ "fmadds",
	  EXECUTE_FP_ARITH(float, fmadd, RD, RA, RC, RB, RC_BIT_G, true),
	  PPC_I(FMADDS),
	  A_form, 59, 29, CFLOW_NORMAL
	},
	{ "fmr",
	  EXECUTE_FP_ARITH(double, fnop, RD, RB, NONE, NONE, RC_BIT_G, false),
	  PPC_I(FMR),
	  X_form, 63, 72, CFLOW_NORMAL
	},
	{ "fmsub",
	  EXECUTE_FP_ARITH(double, fmsub, RD, RA, RC, RB, RC_BIT_G, true),
	  PPC_I(FMSUB),
	  A_form, 63, 28, CFLOW_NORMAL
	},
	{ "fmsubs",
	  EXECUTE_FP_ARITH(float, fmsub, RD, RA, RC, RB, RC_BIT_G, true),
	  PPC_I(FMSUBS),
	  A_form, 59, 28, CFLOW_NORMAL
	},
	{ "fmul",
	  EXECUTE_FP_ARITH(double, fmul, RD, RA, RC, NONE, RC_BIT_G, true),
	  PPC_I(FMUL),
	  A_form, 63, 25, CFLOW_NORMAL
	},
	{ "fmuls",
	  EXECUTE_FP_ARITH(float, fmul, RD, RA, RC, NONE, RC_BIT_G, true),
	  PPC_I(FMULS),
	  A_form, 59, 25, CFLOW_NORMAL
	},
	{ "fnabs",
	  EXECUTE_FP_ARITH(double, fnabs, RD, RB, NONE, NONE, RC_BIT_G, false),
	  PPC_I(FNABS),
	  X_form, 63, 136, CFLOW_NORMAL
	},
	{ "fneg",
	  EXECUTE_FP_ARITH(double, fneg, RD, RB, NONE, NONE, RC_BIT_G, false),
	  PPC_I(FNEG),
	  X_form, 63, 40, CFLOW_NORMAL
	},
	{ "fnmadd",
	  EXECUTE_FP_ARITH(double, fnmadd, RD, RA, RC, RB, RC_BIT_G, true),
	  PPC_I(FNMADD),
	  A_form, 63, 31, CFLOW_NORMAL
	},
	{ "fnmadds",
	  EXECUTE_FP_ARITH(double, fnmadds, RD, RA, RC, RB, RC_BIT_G, true),
	  PPC_I(FNMADDS),
	  A_form, 59, 31, CFLOW_NORMAL
	},
	{ "fnmsub",
	  EXECUTE_FP_ARITH(double, fnmsub, RD, RA, RC, RB, RC_BIT_G, true),
	  PPC_I(FNMSUB),
	  A_form, 63, 30, CFLOW_NORMAL
	},
	{ "fnmsubs",
	  EXECUTE_FP_ARITH(double, fnmsubs, RD, RA, RC, RB, RC_BIT_G, true),
	  PPC_I(FNMSUBS),
	  A_form, 59, 30, CFLOW_NORMAL
	},
	{ "frsp",
	  EXECUTE_1(fp_round, RC_BIT_G),
	  PPC_I(FRSP),
	  X_form, 63, 12, CFLOW_NORMAL
	},
	{ "fsel",
	  EXECUTE_FP_ARITH(double, fsel, RD, RA, RC, RB, RC_BIT_G, false),
	  PPC_I(FSEL),
	  A_form, 63, 23, CFLOW_NORMAL
	},
	{ "fsub",
	  EXECUTE_FP_ARITH(double, fsub, RD, RA, RB, NONE, RC_BIT_G, true),
	  PPC_I(FSUB),
	  A_form, 63, 20, CFLOW_NORMAL
	},
	{ "fsubs",
	  EXECUTE_FP_ARITH(float, fsub, RD, RA, RB, NONE, RC_BIT_G, true),
	  PPC_I(FSUBS),
	  A_form, 59, 20, CFLOW_NORMAL
	},
	{ "icbi",
	  EXECUTE_2(icbi, operand_RA_or_0, operand_RB),
	  PPC_I(ICBI),
	  X_form, 31, 982, CFLOW_NORMAL
	},
	{ "isync",
	  EXECUTE_0(isync),
	  PPC_I(ISYNC),
	  X_form, 19, 150, CFLOW_NORMAL
	},
	{ "lbz",
	  EXECUTE_LOADSTORE(nop, RA_or_0, D, true, 1, false, false),
	  PPC_I(LBZ),
	  D_form, 34, 0, CFLOW_NORMAL
	},
	{ "lbzu",
	  EXECUTE_LOADSTORE(nop, RA, D, true, 1, true, false),
	  PPC_I(LBZU),
	  D_form, 35, 0, CFLOW_NORMAL
	},
	{ "lbzux",
	  EXECUTE_LOADSTORE(nop, RA, RB, true, 1, true, false),
	  PPC_I(LBZUX),
	  X_form, 31, 119, CFLOW_NORMAL
	},
	{ "lbzx",
	  EXECUTE_LOADSTORE(nop, RA_or_0, RB, true, 1, false, false),
	  PPC_I(LBZX),
	  X_form, 31, 87, CFLOW_NORMAL
	},
	{ "lfd",
	  EXECUTE_FP_LOADSTORE(RA_or_0, D, true, true, false),
	  PPC_I(LFD),
	  D_form, 50, 0, CFLOW_NORMAL
	},
	{ "lfdu",
	  EXECUTE_FP_LOADSTORE(RA, D, true, true, true),
	  PPC_I(LFDU),
	  D_form, 51, 0, CFLOW_NORMAL
	},
	{ "lfdux",
	  EXECUTE_FP_LOADSTORE(RA, RB, true, true, true),
	  PPC_I(LFDUX),
	  X_form, 31, 631, CFLOW_NORMAL
	},
	{ "lfdx",
	  EXECUTE_FP_LOADSTORE(RA_or_0, RB, true, true, false),
	  PPC_I(LFDX),
	  X_form, 31, 599, CFLOW_NORMAL
	},
	{ "lfs",
	  EXECUTE_FP_LOADSTORE(RA_or_0, D, true, false, false),
	  PPC_I(LFS),
	  D_form, 48, 0, CFLOW_NORMAL
	},
	{ "lfsu",
	  EXECUTE_FP_LOADSTORE(RA, D, true, false, true),
	  PPC_I(LFSU),
	  D_form, 49, 0, CFLOW_NORMAL
	},
	{ "lfsux",
	  EXECUTE_FP_LOADSTORE(RA, RB, true, false, true),
	  PPC_I(LFSUX),
	  X_form, 31, 567, CFLOW_NORMAL
	},
	{ "lfsx",
	  EXECUTE_FP_LOADSTORE(RA_or_0, RB, true, false, false),
	  PPC_I(LFSX),
	  X_form, 31, 535, CFLOW_NORMAL
	},
	{ "lha",
	  EXECUTE_LOADSTORE(sign_extend_16_32, RA_or_0, D, true, 2, false, false),
	  PPC_I(LHA),
	  D_form, 42, 0, CFLOW_NORMAL
	},
	{ "lhau",
	  EXECUTE_LOADSTORE(sign_extend_16_32, RA, D, true, 2, true, false),
	  PPC_I(LHAU),
	  D_form, 43, 0, CFLOW_NORMAL
	},
	{ "lhaux",
	  EXECUTE_LOADSTORE(sign_extend_16_32, RA, RB, true, 2, true, false),
	  PPC_I(LHAUX),
	  X_form, 31, 375, CFLOW_NORMAL
	},
	{ "lhax",
	  EXECUTE_LOADSTORE(sign_extend_16_32, RA_or_0, RB, true, 2, false, false),
	  PPC_I(LHAX),
	  X_form, 31, 343, CFLOW_NORMAL
	},
	{ "lhbrx",
	  EXECUTE_LOADSTORE(nop, RA_or_0, RB, true, 2, false, true),
	  PPC_I(LHBRX),
	  X_form, 31, 790, CFLOW_NORMAL
	},
	{ "lhz",
	  EXECUTE_LOADSTORE(nop, RA_or_0, D, true, 2, false, false),
	  PPC_I(LHZ),
	  D_form, 40, 0, CFLOW_NORMAL
	},
	{ "lhzu",
	  EXECUTE_LOADSTORE(nop, RA, D, true, 2, true, false),
	  PPC_I(LHZU),
	  D_form, 41, 0, CFLOW_NORMAL
	},
	{ "lhzux",
	  EXECUTE_LOADSTORE(nop, RA, RB, true, 2, true, false),
	  PPC_I(LHZUX),
	  X_form, 31, 311, CFLOW_NORMAL
	},
	{ "lhzx",
	  EXECUTE_LOADSTORE(nop, RA_or_0, RB, true, 2, false, false),
	  PPC_I(LHZX),
	  X_form, 31, 279, CFLOW_NORMAL
	},
	{ "lmw",
	  EXECUTE_LOADSTORE_MULTIPLE(RA_or_0, D, true),
	  PPC_I(LMW),
	  D_form, 46, 0, CFLOW_NORMAL
	},
	{ "lswi",
	  EXECUTE_LOAD_STRING(RA_or_0, true, NB),
	  PPC_I(LSWI),
	  X_form, 31, 597, CFLOW_NORMAL
	},
	{ "lswx",
	  EXECUTE_LOAD_STRING(RA_or_0, false, XER_COUNT),
	  PPC_I(LSWX),
	  X_form, 31, 533, CFLOW_NORMAL
	},
	{ "lvebx",
	  EXECUTE_VECTOR_LOADSTORE(load, V16QIm, RA_or_0, RB),
	  PPC_I(LVEBX),
	  X_form, 31, 7, CFLOW_NORMAL
	},
	{ "lvehx",
	  EXECUTE_VECTOR_LOADSTORE(load, V8HIm, RA_or_0, RB),
	  PPC_I(LVEHX),
	  X_form, 31, 39, CFLOW_NORMAL
	},
	{ "lvewx",
	  EXECUTE_VECTOR_LOADSTORE(load, V4SI, RA_or_0, RB),
	  PPC_I(LVEWX),
	  X_form, 31, 71, CFLOW_NORMAL
	},
	{ "lvsl",
	  EXECUTE_1(vector_load_for_shift, 1),
	  PPC_I(LVSL),
	  X_form, 31, 6, CFLOW_NORMAL
	},
	{ "lvsr",
	  EXECUTE_1(vector_load_for_shift, 0),
	  PPC_I(LVSR),
	  X_form, 31, 38, CFLOW_NORMAL
	},
	{ "lvx",
	  EXECUTE_VECTOR_LOADSTORE(load, V2DI, RA_or_0, RB),
	  PPC_I(LVX),
	  X_form, 31, 103, CFLOW_NORMAL
	},
	{ "lvxl",
	  EXECUTE_VECTOR_LOADSTORE(load, V2DI, RA_or_0, RB),
	  PPC_I(LVXL),
	  X_form, 31, 359, CFLOW_NORMAL
	},
	{ "lwarx",
	  EXECUTE_1(lwarx, operand_RA_or_0),
	  PPC_I(LWARX),
	  X_form, 31, 20, CFLOW_NORMAL
	},
	{ "lwbrx",
	  EXECUTE_LOADSTORE(nop, RA_or_0, RB, true, 4, false, true),
	  PPC_I(LWBRX),
	  X_form, 31, 534, CFLOW_NORMAL
	},
	{ "lwz",
	  EXECUTE_LOADSTORE(nop, RA_or_0, D, true, 4, false, false),
	  PPC_I(LWZ),
	  D_form, 32, 0, CFLOW_NORMAL
	},
	{ "lwzu",
	  EXECUTE_LOADSTORE(nop, RA, D, true, 4, true, false),
	  PPC_I(LWZU),
	  D_form, 33, 0, CFLOW_NORMAL
	},
	{ "lwzux",
	  EXECUTE_LOADSTORE(nop, RA, RB, true, 4, true, false),
	  PPC_I(LWZUX),
	  X_form, 31, 55, CFLOW_NORMAL
	},
	{ "lwzx",
	  EXECUTE_LOADSTORE(nop, RA_or_0, RB, true, 4, false, false),
	  PPC_I(LWZX),
	  X_form, 31, 23, CFLOW_NORMAL
	},
	{ "mcrf",
	  EXECUTE_0(mcrf),
	  PPC_I(MCRF),
	  XL_form, 19, 0, CFLOW_NORMAL
	},
	{ "mcrfs",
	  EXECUTE_0(mcrfs),
	  PPC_I(MCRFS),
	  X_form, 63, 64, CFLOW_NORMAL
	},
	{ "mcrxr",
	  EXECUTE_0(mcrxr),
	  PPC_I(MCRXR),
	  X_form, 31, 512, CFLOW_NORMAL
	},
	{ "mfcr",
	  EXECUTE_GENERIC_ARITH(nop, RD, CR, NONE, NONE, OE_BIT_0, RC_BIT_0),
	  PPC_I(MFCR),
	  X_form, 31, 19, CFLOW_NORMAL
	},
	{ "mffs",
	  EXECUTE_1(mffs, RC_BIT_G),
	  PPC_I(MFFS),
	  X_form, 63, 583, CFLOW_NORMAL
	},
	{ "mfmsr",
	  EXECUTE_0(mfmsr),
	  PPC_I(MFMSR),
	  X_form, 31, 83, CFLOW_NORMAL
	},
	{ "mfspr",
	  EXECUTE_1(mfspr, operand_SPR),
	  PPC_I(MFSPR),
	  XFX_form, 31, 339, CFLOW_NORMAL
	},
	{ "mftb",
	  EXECUTE_1(mftbr, operand_TBR),
	  PPC_I(MFTB),
	  XFX_form, 31, 371, CFLOW_NORMAL
	},
	{ "mfvscr",
	  EXECUTE_0(mfvscr),
	  PPC_I(MFVSCR),
	  VX_form, 4, 1540, CFLOW_NORMAL
	},
	{ "mtcrf",
	  EXECUTE_0(mtcrf),
	  PPC_I(MTCRF),
	  XFX_form, 31, 144, CFLOW_NORMAL
	},
	{ "mtfsb0",
	  EXECUTE_2(mtfsb, immediate_value<0>, RC_BIT_G),
	  PPC_I(MTFSB0),
	  X_form, 63, 70, CFLOW_NORMAL
	},
	{ "mtfsb1",
	  EXECUTE_2(mtfsb, immediate_value<1>, RC_BIT_G),
	  PPC_I(MTFSB1),
	  X_form, 63, 38, CFLOW_NORMAL
	},
	{ "mtfsf",
	  EXECUTE_3(mtfsf, operand_FM, operand_fp_dw_RB, RC_BIT_G),
	  PPC_I(MTFSF),
	  XFL_form, 63, 711, CFLOW_NORMAL
	},
	{ "mtfsfi",
	  EXECUTE_2(mtfsfi, operand_IMM, RC_BIT_G),
	  PPC_I(MTFSFI),
	  X_form, 63, 134, CFLOW_NORMAL
	},
	{ "mtspr",
	  EXECUTE_1(mtspr, operand_SPR),
	  PPC_I(MTSPR),
	  XFX_form, 31, 467, CFLOW_NORMAL
	},
	{ "mtvscr",
	  EXECUTE_0(mtvscr),
	  PPC_I(MTVSCR),
	  VX_form, 4, 1604, CFLOW_NORMAL
	},
	{ "mulhw",
	  EXECUTE_4(multiply, true, true, OE_BIT_0, RC_BIT_G),
	  PPC_I(MULHW),
	  XO_form, 31, 75, CFLOW_NORMAL
	},
	{ "mulhwu",
	  EXECUTE_4(multiply, true, false, OE_BIT_0, RC_BIT_G),
	  PPC_I(MULHWU),
	  XO_form, 31, 11, CFLOW_NORMAL
	},
	{ "mulli",
	  EXECUTE_GENERIC_ARITH(smul, RD, RA, SIMM, NONE, OE_BIT_0, RC_BIT_0),
	  PPC_I(MULLI),
	  D_form, 7, 0, CFLOW_NORMAL
	},
	{ "mullw",
	  EXECUTE_4(multiply, false, true, OE_BIT_G, RC_BIT_G),
	  PPC_I(MULLW),
	  XO_form, 31, 235, CFLOW_NORMAL
	},
	{ "nand",
	  EXECUTE_GENERIC_ARITH(nand, RA, RS, RB, NONE, OE_BIT_0, RC_BIT_G),
	  PPC_I(NAND),
	  X_form, 31, 476, CFLOW_NORMAL
	},
	{ "neg",
	  EXECUTE_GENERIC_ARITH(neg, RD, RA, NONE, NONE, OE_BIT_G, RC_BIT_G),
	  PPC_I(NEG),
	  XO_form, 31, 104, CFLOW_NORMAL
	},
	{ "nor",
	  EXECUTE_GENERIC_ARITH(nor, RA, RS, RB, NONE, OE_BIT_0, RC_BIT_G),
	  PPC_I(NOR),
	  XO_form, 31, 124, CFLOW_NORMAL
	},
	{ "or",
	  EXECUTE_GENERIC_ARITH(or, RA, RS, RB, NONE, OE_BIT_0, RC_BIT_G),
	  PPC_I(OR),
	  XO_form, 31, 444, CFLOW_NORMAL
	},
	{ "orc",
	  EXECUTE_GENERIC_ARITH(orc, RA, RS, RB, NONE, OE_BIT_0, RC_BIT_G),
	  PPC_I(ORC),
	  XO_form, 31, 412, CFLOW_NORMAL
	},
	{ "ori",
	  EXECUTE_GENERIC_ARITH(or, RA, RS, UIMM, NONE, OE_BIT_0, RC_BIT_0),
	  PPC_I(ORI),
	  D_form, 24, 0, CFLOW_NORMAL
	},
	{ "oris",
	  EXECUTE_GENERIC_ARITH(or, RA, RS, UIMM_shifted, NONE, OE_BIT_0, RC_BIT_0),
	  PPC_I(ORIS),
	  D_form, 25, 0, CFLOW_NORMAL
	},
	{ "rlwimi",
	  EXECUTE_3(rlwimi, operand_SH, operand_MASK, RC_BIT_G),
	  PPC_I(RLWIMI),
	  M_form, 20, 0, CFLOW_NORMAL
	},
	{ "rlwinm",
	  EXECUTE_GENERIC_ARITH(ppc_rlwinm, RA, RS, SH, MASK, OE_BIT_0, RC_BIT_G),
	  PPC_I(RLWINM),
	  M_form, 21, 0, CFLOW_NORMAL
	},
	{ "rlwnm",
	  EXECUTE_GENERIC_ARITH(ppc_rlwnm, RA, RS, RB, MASK, OE_BIT_0, RC_BIT_G),
	  PPC_I(RLWNM),
	  M_form, 23, 0, CFLOW_NORMAL
	},
	{ "sc",
	  EXECUTE_0(syscall),
	  PPC_I(SC),
	  SC_form, 17, 0, CFLOW_NORMAL
	},
	{ "slw",
	  EXECUTE_SHIFT(shll, RA, RS, RB, andi<0x3f>, CA_BIT_0, RC_BIT_G),
	  PPC_I(SLW),
	  X_form, 31, 24, CFLOW_NORMAL
	},
	{ "sraw",
	  EXECUTE_SHIFT(shra, RA, RS, RB, andi<0x3f>, CA_BIT_1, RC_BIT_G),
	  PPC_I(SRAW),
	  X_form, 31, 792, CFLOW_NORMAL
	},
	{ "srawi",
	  EXECUTE_SHIFT(shra, RA, RS, SH, andi<0x1f>, CA_BIT_1, RC_BIT_G),
	  PPC_I(SRAWI),
	  X_form, 31, 824, CFLOW_NORMAL
	},
	{ "srw",
	  EXECUTE_SHIFT(shrl, RA, RS, RB, andi<0x3f>, CA_BIT_0, RC_BIT_G),
	  PPC_I(SRW),
	  X_form, 31, 536, CFLOW_NORMAL
	},
	{ "stb",
	  EXECUTE_LOADSTORE(nop, RA_or_0, D, false, 1, false, false),
	  PPC_I(STB),
	  D_form, 38, 0, CFLOW_NORMAL
	},
	{ "stbu",
	  EXECUTE_LOADSTORE(nop, RA, D, false, 1, true, false),
	  PPC_I(STBU),
	  D_form, 39, 0, CFLOW_NORMAL
	},
	{ "stbux",
	  EXECUTE_LOADSTORE(nop, RA, RB, false, 1, true, false),
	  PPC_I(STBUX),
	  X_form, 31, 247, CFLOW_NORMAL
	},
	{ "stbx",
	  EXECUTE_LOADSTORE(nop, RA_or_0, RB, false, 1, false, false),
	  PPC_I(STBX),
	  X_form, 31, 215, CFLOW_NORMAL
	},
	{ "stfd",
	  EXECUTE_FP_LOADSTORE(RA_or_0, D, false, true, false),
	  PPC_I(STFD),
	  D_form, 54, 0, CFLOW_NORMAL
	},
	{ "stfdu",
	  EXECUTE_FP_LOADSTORE(RA, D, false, true, true),
	  PPC_I(STFDU),
	  D_form, 55, 0, CFLOW_NORMAL
	},
	{ "stfdux",
	  EXECUTE_FP_LOADSTORE(RA, RB, false, true, true),
	  PPC_I(STFDUX),
	  X_form, 31, 759, CFLOW_NORMAL
	},
	{ "stfdx",
	  EXECUTE_FP_LOADSTORE(RA_or_0, RB, false, true, false),
	  PPC_I(STFDX),
	  X_form, 31, 727, CFLOW_NORMAL
	},
	{ "stfs",
	  EXECUTE_FP_LOADSTORE(RA_or_0, D, false, false, false),
	  PPC_I(STFS),
	  D_form, 52, 0, CFLOW_NORMAL
	},
	{ "stfsu",
	  EXECUTE_FP_LOADSTORE(RA, D, false, false, true),
	  PPC_I(STFSU),
	  D_form, 53, 0, CFLOW_NORMAL
	},
	{ "stfsux",
	  EXECUTE_FP_LOADSTORE(RA, RB, false, false, true),
	  PPC_I(STFSUX),
	  X_form, 31, 695, CFLOW_NORMAL
	},
	{ "stfsx",
	  EXECUTE_FP_LOADSTORE(RA_or_0, RB, false, false, false),
	  PPC_I(STFSX),
	  X_form, 31, 663, CFLOW_NORMAL
	},
	{ "sth",
	  EXECUTE_LOADSTORE(nop, RA_or_0, D, false, 2, false, false),
	  PPC_I(STH),
	  D_form, 44, 0, CFLOW_NORMAL
	},
	{ "sthbrx",
	  EXECUTE_LOADSTORE(nop, RA_or_0, RB, false, 2, false, true),
	  PPC_I(STHBRX),
	  X_form, 31, 918, CFLOW_NORMAL
	},
	{ "sthu",
	  EXECUTE_LOADSTORE(nop, RA, D, false, 2, true, false),
	  PPC_I(STHU),
	  D_form, 45, 0, CFLOW_NORMAL
	},
	{ "sthux",
	  EXECUTE_LOADSTORE(nop, RA, RB, false, 2, true, false),
	  PPC_I(STHUX),
	  X_form, 31, 439, CFLOW_NORMAL
	},
	{ "sthx",
	  EXECUTE_LOADSTORE(nop, RA_or_0, RB, false, 2, false, false),
	  PPC_I(STHX),
	  X_form, 31, 407, CFLOW_NORMAL
	},
	{ "stmw",
	  EXECUTE_LOADSTORE_MULTIPLE(RA_or_0, D, false),
	  PPC_I(STMW),
	  D_form, 47, 0, CFLOW_NORMAL
	},
	{ "stswi",
	  EXECUTE_STORE_STRING(RA_or_0, true, NB),
	  PPC_I(STSWI),
	  X_form, 31, 725, CFLOW_NORMAL
	},
	{ "stswx",
	  EXECUTE_STORE_STRING(RA_or_0, false, XER_COUNT),
	  PPC_I(STSWX),
	  X_form, 31, 661, CFLOW_NORMAL
	},
	{ "stvebx",
	  EXECUTE_VECTOR_LOADSTORE(store, V16QIm, RA_or_0, RB),
	  PPC_I(STVEBX),
	  X_form, 31, 135, CFLOW_NORMAL
	},
	{ "stvehx",
	  EXECUTE_VECTOR_LOADSTORE(store, V8HIm, RA_or_0, RB),
	  PPC_I(STVEHX),
	  X_form, 31, 167, CFLOW_NORMAL
	},
	{ "stvewx",
	  EXECUTE_VECTOR_LOADSTORE(store, V4SI, RA_or_0, RB),
	  PPC_I(STVEWX),
	  X_form, 31, 199, CFLOW_NORMAL
	},
	{ "stvx",
	  EXECUTE_VECTOR_LOADSTORE(store, V2DI, RA_or_0, RB),
	  PPC_I(STVX),
	  X_form, 31, 231, CFLOW_NORMAL
	},
	{ "stvxl",
	  EXECUTE_VECTOR_LOADSTORE(store, V2DI, RA_or_0, RB),
	  PPC_I(STVXL),
	  X_form, 31, 487, CFLOW_NORMAL
	},
	{ "stw",
	  EXECUTE_LOADSTORE(nop, RA_or_0, D, false, 4, false, false),
	  PPC_I(STW),
	  D_form, 36, 0, CFLOW_NORMAL
	},
	{ "stwbrx",
	  EXECUTE_LOADSTORE(nop, RA_or_0, RB, false, 4, false, true),
	  PPC_I(STWBRX),
	  X_form, 31, 662, CFLOW_NORMAL
	},
	{ "stwcx.",
	  EXECUTE_1(stwcx, operand_RA_or_0),
	  PPC_I(STWCX),
	  X_form, 31, 150, CFLOW_NORMAL
	},
	{ "stwu",
	  EXECUTE_LOADSTORE(nop, RA, D, false, 4, true, false),
	  PPC_I(STWU),
	  D_form, 37, 0, CFLOW_NORMAL
	},
	{ "stwux",
	  EXECUTE_LOADSTORE(nop, RA, RB, false, 4, true, false),
	  PPC_I(STWUX),
	  X_form, 31, 183, CFLOW_NORMAL
	},
	{ "stwx",
	  EXECUTE_LOADSTORE(nop, RA_or_0, RB, false, 4, false, false),
	  PPC_I(STWX),
	  X_form, 31, 151, CFLOW_NORMAL
	},
	{ "subf",
	  EXECUTE_ADDITION(RA_compl, RB, ONE, CA_BIT_0, OE_BIT_G, RC_BIT_G),
	  PPC_I(SUBF),
	  XO_form, 31, 40, CFLOW_NORMAL
	},
	{ "subfc",
	  EXECUTE_ADDITION(RA_compl, RB, ONE, CA_BIT_1, OE_BIT_G, RC_BIT_G),
	  PPC_I(SUBFC),
	  XO_form, 31, 8, CFLOW_NORMAL
	},
	{ "subfe",
	  EXECUTE_ADDITION(RA_compl, RB, XER_CA, CA_BIT_1, OE_BIT_G, RC_BIT_G),
	  PPC_I(SUBFE),
	  XO_form, 31, 136, CFLOW_NORMAL
	},
	{ "subfic",
	  EXECUTE_ADDITION(RA_compl, SIMM, ONE, CA_BIT_1, OE_BIT_0, RC_BIT_0),
	  PPC_I(SUBFIC),
	  D_form, 8, 0, CFLOW_NORMAL
	},
	{ "subfme",
	  EXECUTE_ADDITION(RA_compl, XER_CA, MINUS_ONE, CA_BIT_1, OE_BIT_G, RC_BIT_G),
	  PPC_I(SUBFME),
	  XO_form, 31, 232, CFLOW_NORMAL
	},
	{ "subfze",
	  EXECUTE_ADDITION(RA_compl, XER_CA, ZERO, CA_BIT_1, OE_BIT_G, RC_BIT_G),
	  PPC_I(SUBFZE),
	  XO_form, 31, 200, CFLOW_NORMAL
	},
	{ "sync",
	  EXECUTE_0(nop),
	  PPC_I(SYNC),
	  X_form, 31, 598, CFLOW_NORMAL
	},
	{ "xor",
	  EXECUTE_GENERIC_ARITH(xor, RA, RS, RB, NONE, OE_BIT_0, RC_BIT_G),
	  PPC_I(XOR),
	  X_form, 31, 316, CFLOW_NORMAL
	},
	{ "xori",
	  EXECUTE_GENERIC_ARITH(xor, RA, RS, UIMM, NONE, OE_BIT_0, RC_BIT_0),
	  PPC_I(XORI),
	  D_form, 26, 0, CFLOW_NORMAL
	},
	{ "xoris",
	  EXECUTE_GENERIC_ARITH(xor, RA, RS, UIMM_shifted, NONE, OE_BIT_0, RC_BIT_0),
	  PPC_I(XORIS),
	  D_form, 27, 0, CFLOW_NORMAL
	},
	{ "vaddcuw",
	  EXECUTE_VECTOR_ARITH(addcuw, V4SI, V4SI, V4SI, NONE),
	  PPC_I(VADDCUW),
	  VX_form, 4, 384, CFLOW_NORMAL
	},
	{ "vaddfp",
	  EXECUTE_VECTOR_ARITH(fadds, V4SF, V4SF, V4SF, NONE),
	  PPC_I(VADDFP),
	  VX_form, 4, 10, CFLOW_NORMAL
	},
	{ "vaddsbs",
	  EXECUTE_VECTOR_ARITH(add, V16QI_SAT<int8>, V16QI_SAT<int8>, V16QI_SAT<int8>, NONE),
	  PPC_I(VADDSBS),
	  VX_form, 4, 768, CFLOW_NORMAL
	},
	{ "vaddshs",
	  EXECUTE_VECTOR_ARITH(add, V8HI_SAT<int16>, V8HI_SAT<int16>, V8HI_SAT<int16>, NONE),
	  PPC_I(VADDSHS),
	  VX_form, 4, 832, CFLOW_NORMAL
	},
	{ "vaddsws",
	  EXECUTE_VECTOR_ARITH(add_64, V4SI_SAT<int32>, V4SI_SAT<int32>, V4SI_SAT<int32>, NONE),
	  PPC_I(VADDSWS),
	  VX_form, 4, 896, CFLOW_NORMAL
	},
	{ "vaddubm",
	  EXECUTE_VECTOR_ARITH(add, V16QI, V16QI, V16QI, NONE),
	  PPC_I(VADDUBM),
	  VX_form, 4, 0, CFLOW_NORMAL
	},
	{ "vaddubs",
	  EXECUTE_VECTOR_ARITH(add, V16QI_SAT<uint8>, V16QI_SAT<uint8>, V16QI_SAT<uint8>, NONE),
	  PPC_I(VADDUBS),
	  VX_form, 4, 512, CFLOW_NORMAL
	},
	{ "vadduhm",
	  EXECUTE_VECTOR_ARITH(add, V8HI, V8HI, V8HI, NONE),
	  PPC_I(VADDUHM),
	  VX_form, 4, 64, CFLOW_NORMAL
	},
	{ "vadduhs",
	  EXECUTE_VECTOR_ARITH(add, V8HI_SAT<uint16>, V8HI_SAT<uint16>, V8HI_SAT<uint16>, NONE),
	  PPC_I(VADDUHS),
	  VX_form, 4, 576, CFLOW_NORMAL
	},
	{ "vadduwm",
	  EXECUTE_VECTOR_ARITH(add, V4SI, V4SI, V4SI, NONE),
	  PPC_I(VADDUWM),
	  VX_form, 4, 128, CFLOW_NORMAL
	},
	{ "vadduws",
	  EXECUTE_VECTOR_ARITH(add_64, V4SI_SAT<uint32>, V4SI_SAT<uint32>, V4SI_SAT<uint32>, NONE),
	  PPC_I(VADDUWS),
	  VX_form, 4, 640, CFLOW_NORMAL
	},
	{ "vand",
	  EXECUTE_VECTOR_ARITH(and_64, V2DI, V2DI, V2DI, NONE),
	  PPC_I(VAND),
	  VX_form, 4, 1028, CFLOW_NORMAL
	},
	{ "vandc",
	  EXECUTE_VECTOR_ARITH(andc_64, V2DI, V2DI, V2DI, NONE),
	  PPC_I(VANDC),
	  VX_form, 4, 1092, CFLOW_NORMAL
	},
	{ "vavgsb",
	  EXECUTE_VECTOR_ARITH(avgsb, V16QI, V16QI, V16QI, NONE),
	  PPC_I(VAVGSB),
	  VX_form, 4, 1282, CFLOW_NORMAL
	},
	{ "vavgsh",
	  EXECUTE_VECTOR_ARITH(avgsh, V8HI, V8HI, V8HI, NONE),
	  PPC_I(VAVGSH),
	  VX_form, 4, 1346, CFLOW_NORMAL
	},
	{ "vavgsw",
	  EXECUTE_VECTOR_ARITH(avgsw, V4SI, V4SI, V4SI, NONE),
	  PPC_I(VAVGSW),
	  VX_form, 4, 1410, CFLOW_NORMAL
	},
	{ "vavgub",
	  EXECUTE_VECTOR_ARITH(avgub, V16QI, V16QI, V16QI, NONE),
	  PPC_I(VAVGUB),
	  VX_form, 4, 1026, CFLOW_NORMAL
	},
	{ "vavguh",
	  EXECUTE_VECTOR_ARITH(avguh, V8HI, V8HI, V8HI, NONE),
	  PPC_I(VAVGUH),
	  VX_form, 4, 1090, CFLOW_NORMAL
	},
	{ "vavguw",
	  EXECUTE_VECTOR_ARITH(avguw, V4SI, V4SI, V4SI, NONE),
	  PPC_I(VAVGUW),
	  VX_form, 4, 1154, CFLOW_NORMAL
	},
	{ "vcfsx",
	  EXECUTE_VECTOR_ARITH(cvt_si2fp<int32>, V4SF, UIMM, V4SIs, NONE),
	  PPC_I(VCFSX),
	  VX_form, 4, 842, CFLOW_NORMAL
	},
	{ "vcfux",
	  EXECUTE_VECTOR_ARITH(cvt_si2fp<uint32>, V4SF, UIMM, V4SI, NONE),
	  PPC_I(VCFUX),
	  VX_form, 4, 778, CFLOW_NORMAL
	},
	{ "vcmpbfp",
	  EXECUTE_VECTOR_COMPARE(cmpbfp, V4SI, V4SF, V4SF, 0),
	  PPC_I(VCMPBFP),
	  VXR_form, 4, 966, CFLOW_NORMAL
	},
	{ "vcmpeqfp",
	  EXECUTE_VECTOR_COMPARE(cmp_eq<float>, V4SI, V4SF, V4SF, 1),
	  PPC_I(VCMPEQFP),
	  VXR_form, 4, 198, CFLOW_NORMAL
	},
	{ "vcmpequb",
	  EXECUTE_VECTOR_COMPARE(cmp_eq<uint8>, V16QI, V16QI, V16QI, 1),
	  PPC_I(VCMPEQUB),
	  VXR_form, 4, 6, CFLOW_NORMAL
	},
	{ "vcmpequh",
	  EXECUTE_VECTOR_COMPARE(cmp_eq<uint16>, V8HI, V8HI, V8HI, 1),
	  PPC_I(VCMPEQUH),
	  VXR_form, 4, 70, CFLOW_NORMAL
	},
	{ "vcmpequw",
	  EXECUTE_VECTOR_COMPARE(cmp_eq<uint32>, V4SI, V4SI, V4SI, 1),
	  PPC_I(VCMPEQUW),
	  VXR_form, 4, 134, CFLOW_NORMAL
	},
	{ "vcmpgefp",
	  EXECUTE_VECTOR_COMPARE(cmp_ge<float>, V4SI, V4SF, V4SF, 1),
	  PPC_I(VCMPGEFP),
	  VXR_form, 4, 454, CFLOW_NORMAL
	},
	{ "vcmpgtfp",
	  EXECUTE_VECTOR_COMPARE(cmp_gt<float>, V4SI, V4SF, V4SF, 1),
	  PPC_I(VCMPGTFP),
	  VXR_form, 4, 710, CFLOW_NORMAL
	},
	{ "vcmpgtsb",
	  EXECUTE_VECTOR_COMPARE(cmp_gt<int8>, V16QI, V16QIs, V16QIs, 1),
	  PPC_I(VCMPGTSB),
	  VXR_form, 4, 774, CFLOW_NORMAL
	},
	{ "vcmpgtsh",
	  EXECUTE_VECTOR_COMPARE(cmp_gt<int16>, V8HI, V8HIs, V8HIs, 1),
	  PPC_I(VCMPGTSH),
	  VXR_form, 4, 838, CFLOW_NORMAL
	},
	{ "vcmpgtsw",
	  EXECUTE_VECTOR_COMPARE(cmp_gt<int32>, V4SI, V4SIs, V4SIs, 1),
	  PPC_I(VCMPGTSW),
	  VXR_form, 4, 902, CFLOW_NORMAL
	},
	{ "vcmpgtub",
	  EXECUTE_VECTOR_COMPARE(cmp_gt<uint8>, V16QI, V16QI, V16QI, 1),
	  PPC_I(VCMPGTUB),
	  VXR_form, 4, 518, CFLOW_NORMAL
	},
	{ "vcmpgtuh",
	  EXECUTE_VECTOR_COMPARE(cmp_gt<uint16>, V8HI, V8HI, V8HI, 1),
	  PPC_I(VCMPGTUH),
	  VXR_form, 4, 582, CFLOW_NORMAL
	},
	{ "vcmpgtuw",
	  EXECUTE_VECTOR_COMPARE(cmp_gt<uint32>, V4SI, V4SI, V4SI, 1),
	  PPC_I(VCMPGTUW),
	  VXR_form, 4, 646, CFLOW_NORMAL
	},
	{ "vctsxs",
	  EXECUTE_VECTOR_ARITH(cvt_fp2si, V4SI_SAT<int32>, UIMM, V4SF, NONE),
	  PPC_I(VCTSXS),
	  VX_form, 4, 970, CFLOW_NORMAL
	},
	{ "vctuxs",
	  EXECUTE_VECTOR_ARITH(cvt_fp2si, V4SI_SAT<uint32>, UIMM, V4SF, NONE),
	  PPC_I(VCTUXS),
	  VX_form, 4, 906, CFLOW_NORMAL
	},
	{ "vexptefp",
	  EXECUTE_VECTOR_ARITH(exp2, V4SF, NONE, V4SF, NONE),
	  PPC_I(VEXPTEFP),
	  VX_form, 4, 394, CFLOW_NORMAL
	},
	{ "vlogefp",
	  EXECUTE_VECTOR_ARITH(log2, V4SF, NONE, V4SF, NONE),
	  PPC_I(VLOGEFP),
	  VX_form, 4, 458, CFLOW_NORMAL
	},
	{ "vmaddfp",
	  EXECUTE_VECTOR_ARITH(vmaddfp, V4SF, V4SF, V4SF, V4SF),
	  PPC_I(VMADDFP),
	  VA_form, 4, 46, CFLOW_NORMAL
	},
	{ "vmaxfp",
	  EXECUTE_VECTOR_ARITH(max<float>, V4SF, V4SF, V4SF, NONE),
	  PPC_I(VMAXFP),
	  VX_form, 4, 1034, CFLOW_NORMAL
	},
	{ "vmaxsb",
	  EXECUTE_VECTOR_ARITH(max<int8>, V16QI, V16QI, V16QI, NONE),
	  PPC_I(VMAXSB),
	  VX_form, 4, 258, CFLOW_NORMAL
	},
	{ "vmaxsh",
	  EXECUTE_VECTOR_ARITH(max<int16>, V8HI, V8HI, V8HI, NONE),
	  PPC_I(VMAXSH),
	  VX_form, 4, 322, CFLOW_NORMAL
	},
	{ "vmaxsw",
	  EXECUTE_VECTOR_ARITH(max<int32>, V4SI, V4SI, V4SI, NONE),
	  PPC_I(VMAXSW),
	  VX_form, 4, 386, CFLOW_NORMAL
	},
	{ "vmaxub",
	  EXECUTE_VECTOR_ARITH(max<uint8>, V16QI, V16QI, V16QI, NONE),
	  PPC_I(VMAXUB),
	  VX_form, 4, 2, CFLOW_NORMAL
	},
	{ "vmaxuh",
	  EXECUTE_VECTOR_ARITH(max<uint16>, V8HI, V8HI, V8HI, NONE),
	  PPC_I(VMAXUH),
	  VX_form, 4, 66, CFLOW_NORMAL
	},
	{ "vmaxuw",
	  EXECUTE_VECTOR_ARITH(max<uint32>, V4SI, V4SI, V4SI, NONE),
	  PPC_I(VMAXUW),
	  VX_form, 4, 130, CFLOW_NORMAL
	},
	{ "vmhaddshs",
	  EXECUTE_VECTOR_ARITH(mhraddsh<0>, V8HI_SAT<int16>, V8HI_SAT<int16>, V8HI_SAT<int16>, V8HI_SAT<int16>),
	  PPC_I(VMHADDSHS),
	  VA_form, 4, 32, CFLOW_NORMAL
	},
	{ "vmhraddshs",
	  EXECUTE_VECTOR_ARITH(mhraddsh<0x4000>, V8HI_SAT<int16>, V8HI_SAT<int16>, V8HI_SAT<int16>, V8HI_SAT<int16>),
	  PPC_I(VMHRADDSHS),
	  VA_form, 4, 33, CFLOW_NORMAL
	},
	{ "vminfp",
	  EXECUTE_VECTOR_ARITH(min<float>, V4SF, V4SF, V4SF, NONE),
	  PPC_I(VMINFP),
	  VX_form, 4, 1098, CFLOW_NORMAL
	},
	{ "vminsb",
	  EXECUTE_VECTOR_ARITH(min<int8>, V16QI, V16QI, V16QI, NONE),
	  PPC_I(VMINSB),
	  VX_form, 4, 770, CFLOW_NORMAL
	},
	{ "vminsh",
	  EXECUTE_VECTOR_ARITH(min<int16>, V8HI, V8HI, V8HI, NONE),
	  PPC_I(VMINSH),
	  VX_form, 4, 834, CFLOW_NORMAL
	},
	{ "vminsw",
	  EXECUTE_VECTOR_ARITH(min<int32>, V4SI, V4SI, V4SI, NONE),
	  PPC_I(VMINSW),
	  VX_form, 4, 898, CFLOW_NORMAL
	},
	{ "vminub",
	  EXECUTE_VECTOR_ARITH(min<uint8>, V16QI, V16QI, V16QI, NONE),
	  PPC_I(VMINUB),
	  VX_form, 4, 514, CFLOW_NORMAL
	},
	{ "vminuh",
	  EXECUTE_VECTOR_ARITH(min<uint16>, V8HI, V8HI, V8HI, NONE),
	  PPC_I(VMINUH),
	  VX_form, 4, 578, CFLOW_NORMAL
	},
	{ "vminuw",
	  EXECUTE_VECTOR_ARITH(min<uint32>, V4SI, V4SI, V4SI, NONE),
	  PPC_I(VMINUW),
	  VX_form, 4, 642, CFLOW_NORMAL
	},
	{ "vmladduhm",
	  EXECUTE_VECTOR_ARITH(mladduh, V8HI, V8HI, V8HI, V8HI),
	  PPC_I(VMLADDUHM),
	  VA_form, 4, 34, CFLOW_NORMAL
	},
	{ "vmrghb",
	  EXECUTE_VECTOR_MERGE(V16QIm, V16QIm, V16QIm, 0),
	  PPC_I(VMRGHB),
	  VX_form, 4, 12, CFLOW_NORMAL
	},
	{ "vmrghh",
	  EXECUTE_VECTOR_MERGE(V8HIm, V8HIm, V8HIm, 0),
	  PPC_I(VMRGHH),
	  VX_form, 4, 76, CFLOW_NORMAL
	},
	{ "vmrghw",
	  EXECUTE_VECTOR_MERGE(V4SI, V4SI, V4SI, 0),
	  PPC_I(VMRGHW),
	  VX_form, 4, 140, CFLOW_NORMAL
	},
	{ "vmrglb",
	  EXECUTE_VECTOR_MERGE(V16QIm, V16QIm, V16QIm, 1),
	  PPC_I(VMRGLB),
	  VX_form, 4, 268, CFLOW_NORMAL
	},
	{ "vmrglh",
	  EXECUTE_VECTOR_MERGE(V8HIm, V8HIm, V8HIm, 1),
	  PPC_I(VMRGLH),
	  VX_form, 4, 332, CFLOW_NORMAL
	},
	{ "vmrglw",
	  EXECUTE_VECTOR_MERGE(V4SI, V4SI, V4SI, 1),
	  PPC_I(VMRGLW),
	  VX_form, 4, 396, CFLOW_NORMAL
	},
	{ "vmsummbm",
	  EXECUTE_VECTOR_ARITH_MIXED(smul, V4SI, V16QI_SAT<int8>, V16QI_SAT<uint8>, V4SI),
	  PPC_I(VMSUMMBM),
	  VA_form, 4, 37, CFLOW_NORMAL
	},
	{ "vmsumshm",
	  EXECUTE_VECTOR_ARITH_MIXED(smul, V4SI, V8HI_SAT<int16>, V8HI_SAT<int16>, V4SI),
	  PPC_I(VMSUMSHM),
	  VA_form, 4, 40, CFLOW_NORMAL
	},
	{ "vmsumshs",
	  EXECUTE_VECTOR_ARITH_MIXED(smul_64, V4SI_SAT<int32>, V8HI_SAT<int16>, V8HI_SAT<int16>, V4SIs),
	  PPC_I(VMSUMSHS),
	  VA_form, 4, 41, CFLOW_NORMAL
	},
	{ "vmsumubm",
	  EXECUTE_VECTOR_ARITH_MIXED(mul, V4SI, V16QI, V16QI, V4SI),
	  PPC_I(VMSUMUBM),
	  VA_form, 4, 36, CFLOW_NORMAL
	},
	{ "vmsumuhm",
	  EXECUTE_VECTOR_ARITH_MIXED(mul, V4SI, V8HI, V8HI, V4SI),
	  PPC_I(VMSUMUHM),
	  VA_form, 4, 38, CFLOW_NORMAL
	},
	{ "vmsumuhs",
	  EXECUTE_VECTOR_ARITH_MIXED(mul, V4SI_SAT<uint32>, V8HI, V8HI, V4SI),
	  PPC_I(VMSUMUHS),
	  VA_form, 4, 39, CFLOW_NORMAL
	},
	{ "vmulesb",
	  EXECUTE_VECTOR_ARITH_ODD(0, smul, V8HIm, V16QIm_SAT<int8>, V16QIm_SAT<int8>, NONE),
	  PPC_I(VMULESB),
	  VX_form, 4, 776, CFLOW_NORMAL
	},
	{ "vmulesh",
	  EXECUTE_VECTOR_ARITH_ODD(0, smul, V4SI, V8HIm_SAT<int16>, V8HIm_SAT<int16>, NONE),
	  PPC_I(VMULESH),
	  VX_form, 4, 840, CFLOW_NORMAL
	},
	{ "vmuleub",
	  EXECUTE_VECTOR_ARITH_ODD(0, mul, V8HIm, V16QIm, V16QIm, NONE),
	  PPC_I(VMULEUB),
	  VX_form, 4, 520, CFLOW_NORMAL
	},
	{ "vmuleuh",
	  EXECUTE_VECTOR_ARITH_ODD(0, mul, V4SI, V8HIm, V8HIm, NONE),
	  PPC_I(VMULEUH),
	  VX_form, 4, 584, CFLOW_NORMAL
	},
	{ "vmulosb",
	  EXECUTE_VECTOR_ARITH_ODD(1, smul, V8HIm, V16QIm_SAT<int8>, V16QIm_SAT<int8>, NONE),
	  PPC_I(VMULOSB),
	  VX_form, 4, 264, CFLOW_NORMAL
	},
	{ "vmulosh",
	  EXECUTE_VECTOR_ARITH_ODD(1, smul, V4SI, V8HIm_SAT<int16>, V8HIm_SAT<int16>, NONE),
	  PPC_I(VMULOSH),
	  VX_form, 4, 328, CFLOW_NORMAL
	},
	{ "vmuloub",
	  EXECUTE_VECTOR_ARITH_ODD(1, mul, V8HIm, V16QIm, V16QIm, NONE),
	  PPC_I(VMULOUB),
	  VX_form, 4, 8, CFLOW_NORMAL
	},
	{ "vmulouh",
	  EXECUTE_VECTOR_ARITH_ODD(1, mul, V4SI, V8HIm, V8HIm, NONE),
	  PPC_I(VMULOUH),
	  VX_form, 4, 72, CFLOW_NORMAL
	},
	{ "vnmsubfp",
	  EXECUTE_VECTOR_ARITH(vnmsubfp, V4SF, V4SF, V4SF, V4SF),
	  PPC_I(VNMSUBFP),
	  VA_form, 4, 47, CFLOW_NORMAL
	},
	{ "vnor",
	  EXECUTE_VECTOR_ARITH(nor_64, V2DI, V2DI, V2DI, NONE),
	  PPC_I(VNOR),
	  VX_form, 4, 1284, CFLOW_NORMAL
	},
	{ "vor",
	  EXECUTE_VECTOR_ARITH(or_64, V2DI, V2DI, V2DI, NONE),
	  PPC_I(VOR),
	  VX_form, 4, 1156, CFLOW_NORMAL
	},
	{ "vperm",
	  EXECUTE_0(vector_permute),
	  PPC_I(VPERM),
	  VA_form, 4, 43, CFLOW_NORMAL
	},
	{ "vpkpx",
	  EXECUTE_0(vector_pack_pixel),
	  PPC_I(VPKPX),
	  VX_form, 4, 782, CFLOW_NORMAL
	},
	{ "vpkshss",
	  EXECUTE_VECTOR_PACK(V16QIm_SAT<int8>, V8HIm, V8HIm),
	  PPC_I(VPKSHSS),
	  VX_form, 4, 398, CFLOW_NORMAL
	},
	{ "vpkshus",
	  EXECUTE_VECTOR_PACK(V16QIm_SAT<uint8>, V8HIm, V8HIm),
	  PPC_I(VPKSHUS),
	  VX_form, 4, 270, CFLOW_NORMAL
	},
	{ "vpkswss",
	  EXECUTE_VECTOR_PACK(V8HIm_SAT<int16>, V4SI, V4SI),
	  PPC_I(VPKSWSS),
	  VX_form, 4, 462, CFLOW_NORMAL
	},
	{ "vpkswus",
	  EXECUTE_VECTOR_PACK(V8HIm_SAT<uint16>, V4SI, V4SI),
	  PPC_I(VPKSWUS),
	  VX_form, 4, 334, CFLOW_NORMAL
	},
	{ "vpkuhum",
	  EXECUTE_VECTOR_PACK(V16QIm, V8HIm, V8HIm),
	  PPC_I(VPKUHUM),
	  VX_form, 4, 14, CFLOW_NORMAL
	},
	{ "vpkuhus",
	  EXECUTE_VECTOR_PACK(V16QIm_USAT<uint8>, V8HIm, V8HIm),
	  PPC_I(VPKUHUS),
	  VX_form, 4, 142, CFLOW_NORMAL
	},
	{ "vpkuwum",
	  EXECUTE_VECTOR_PACK(V8HIm, V4SI, V4SI),
	  PPC_I(VPKUWUM),
	  VX_form, 4, 78, CFLOW_NORMAL
	},
	{ "vpkuwus",
	  EXECUTE_VECTOR_PACK(V8HIm_USAT<uint16>, V4SI, V4SI),
	  PPC_I(VPKUWUS),
	  VX_form, 4, 206, CFLOW_NORMAL
	},
	{ "vrefp",
	  EXECUTE_VECTOR_ARITH(fres, V4SF, NONE, V4SF, NONE),
	  PPC_I(VREFP),
	  VX_form, 4, 266, CFLOW_NORMAL
	},
	{ "vrfim",
	  EXECUTE_VECTOR_ARITH(frsim, V4SF, NONE, V4SF, NONE),
	  PPC_I(VRFIM),
	  VX_form, 4, 714, CFLOW_NORMAL
	},
	{ "vrfin",
	  EXECUTE_VECTOR_ARITH(frsin, V4SF, NONE, V4SF, NONE),
	  PPC_I(VRFIN),
	  VX_form, 4, 522, CFLOW_NORMAL
	},
	{ "vrfip",
	  EXECUTE_VECTOR_ARITH(frsip, V4SF, NONE, V4SF, NONE),
	  PPC_I(VRFIP),
	  VX_form, 4, 650, CFLOW_NORMAL
	},
	{ "vrfiz",
	  EXECUTE_VECTOR_ARITH(frsiz, V4SF, NONE, V4SF, NONE),
	  PPC_I(VRFIZ),
	  VX_form, 4, 586, CFLOW_NORMAL
	},
	{ "vrlb",
	  EXECUTE_VECTOR_ARITH(vrl<uint8>, V16QI, V16QI, V16QI, NONE),
	  PPC_I(VRLB),
	  VX_form, 4, 4, CFLOW_NORMAL
	},
	{ "vrlh",
	  EXECUTE_VECTOR_ARITH(vrl<uint16>, V8HI, V8HI, V8HI, NONE),
	  PPC_I(VRLH),
	  VX_form, 4, 68, CFLOW_NORMAL
	},
	{ "vrlw",
	  EXECUTE_VECTOR_ARITH(vrl<uint32>, V4SI, V4SI, V4SI, NONE),
	  PPC_I(VRLW),
	  VX_form, 4, 132, CFLOW_NORMAL
	},
	{ "vrsqrtefp",
	  EXECUTE_VECTOR_ARITH(frsqrt, V4SF, NONE, V4SF, NONE),
	  PPC_I(VRSQRTEFP),
	  VX_form, 4, 330, CFLOW_NORMAL
	},
	{ "vsel",
	  EXECUTE_VECTOR_ARITH(vsel, V4SI, V4SI, V4SI, V4SI),
	  PPC_I(VSEL),
	  VA_form, 4, 42, CFLOW_NORMAL
	},
	{ "vsl",
	  EXECUTE_1(vector_shift, -1),
	  PPC_I(VSL),
	  VX_form, 4, 452, CFLOW_NORMAL
	},
	{ "vslb",
	  EXECUTE_VECTOR_ARITH(vsl<uint8>, V16QI, V16QI, V16QI, NONE),
	  PPC_I(VSLB),
	  VX_form, 4, 260, CFLOW_NORMAL
	},
	{ "vsldoi",
	  EXECUTE_VECTOR_SHIFT_OCTET(-1, V16QIm, V16QIm, V16QIm, SHB),
	  PPC_I(VSLDOI),
	  VA_form, 4, 44, CFLOW_NORMAL
	},
	{ "vslh",
	  EXECUTE_VECTOR_ARITH(vsl<uint16>, V8HI, V8HI, V8HI, NONE),
	  PPC_I(VSLH),
	  VX_form, 4, 324, CFLOW_NORMAL
	},
	{ "vslo",
	  EXECUTE_VECTOR_SHIFT_OCTET(-1, V16QIm, V16QIm, NONE, SHBO),
	  PPC_I(VSLO),
	  VX_form, 4, 1036, CFLOW_NORMAL
	},
	{ "vslw",
	  EXECUTE_VECTOR_ARITH(vsl<uint32>, V4SI, V4SI, V4SI, NONE),
	  PPC_I(VSLW),
	  VX_form, 4, 388, CFLOW_NORMAL
	},
	{ "vspltb",
	  EXECUTE_VECTOR_SPLAT(nop, V16QI, V16QIm, false),
	  PPC_I(VSPLTB),
	  VX_form, 4, 524, CFLOW_NORMAL
	},
	{ "vsplth",
	  EXECUTE_VECTOR_SPLAT(nop, V8HI, V8HIm, false),
	  PPC_I(VSPLTH),
	  VX_form, 4, 588, CFLOW_NORMAL
	},
	{ "vspltisb",
	  EXECUTE_VECTOR_SPLAT(sign_extend_5_32, V16QI, UIMM, true),
	  PPC_I(VSPLTISB),
	  VX_form, 4, 780, CFLOW_NORMAL
	},
	{ "vspltish",
	  EXECUTE_VECTOR_SPLAT(sign_extend_5_32, V8HI, UIMM, true),
	  PPC_I(VSPLTISH),
	  VX_form, 4, 844, CFLOW_NORMAL
	},
	{ "vspltisw",
	  EXECUTE_VECTOR_SPLAT(sign_extend_5_32, V4SI, UIMM, true),
	  PPC_I(VSPLTISW),
	  VX_form, 4, 908, CFLOW_NORMAL
	},
	{ "vspltw",
	  EXECUTE_VECTOR_SPLAT(nop, V4SI, V4SI, false),
	  PPC_I(VSPLTW),
	  VX_form, 4, 652, CFLOW_NORMAL
	},
	{ "vsr",
	  EXECUTE_1(vector_shift, +1),
	  PPC_I(VSR),
	  VX_form, 4, 708, CFLOW_NORMAL
	},
	{ "vsrab",
	  EXECUTE_VECTOR_ARITH(vsr<int8>, V16QI, V16QIs, V16QI, NONE),
	  PPC_I(VSRAB),
	  VX_form, 4, 772, CFLOW_NORMAL
	},
	{ "vsrah",
	  EXECUTE_VECTOR_ARITH(vsr<int16>, V8HI, V8HIs, V8HI, NONE),
	  PPC_I(VSRAH),
	  VX_form, 4, 836, CFLOW_NORMAL
	},
	{ "vsraw",
	  EXECUTE_VECTOR_ARITH(vsr<int32>, V4SI, V4SIs, V4SIs, NONE),
	  PPC_I(VSRAW),
	  VX_form, 4, 900, CFLOW_NORMAL
	},
	{ "vsrb",
	  EXECUTE_VECTOR_ARITH(vsr<uint8>, V16QI, V16QI, V16QI, NONE),
	  PPC_I(VSRB),
	  VX_form, 4, 516, CFLOW_NORMAL
	},
	{ "vsrh",
	  EXECUTE_VECTOR_ARITH(vsr<uint16>, V8HI, V8HI, V8HI, NONE),
	  PPC_I(VSRH),
	  VX_form, 4, 580, CFLOW_NORMAL
	},
	{ "vsro",
	  EXECUTE_VECTOR_SHIFT_OCTET(+1, V16QIm, V16QIm, NONE, SHBO),
	  PPC_I(VSRO),
	  VX_form, 4, 1100, CFLOW_NORMAL
	},
	{ "vsrw",
	  EXECUTE_VECTOR_ARITH(vsr<uint32>, V4SI, V4SI, V4SI, NONE),
	  PPC_I(VSRW),
	  VX_form, 4, 644, CFLOW_NORMAL
	},
	{ "vsubcuw",
	  EXECUTE_VECTOR_ARITH(subcuw, V4SI, V4SI, V4SI, NONE),
	  PPC_I(VSUBCUW),
	  VX_form, 4, 1408, CFLOW_NORMAL
	},
	{ "vsubfp",
	  EXECUTE_VECTOR_ARITH(fsubs, V4SF, V4SF, V4SF, NONE),
	  PPC_I(VSUBFP),
	  VX_form, 4, 74, CFLOW_NORMAL
	},
	{ "vsubsbs",
	  EXECUTE_VECTOR_ARITH(sub, V16QI_SAT<int8>, V16QI_SAT<int8>, V16QI_SAT<int8>, NONE),
	  PPC_I(VSUBSBS),
	  VX_form, 4, 1792, CFLOW_NORMAL
	},
	{ "vsubshs",
	  EXECUTE_VECTOR_ARITH(sub, V8HI_SAT<int16>, V8HI_SAT<int16>, V8HI_SAT<int16>, NONE),
	  PPC_I(VSUBSHS),
	  VX_form, 4, 1856, CFLOW_NORMAL
	},
	{ "vsubsws",
	  EXECUTE_VECTOR_ARITH(sub_64, V4SI_SAT<int32>, V4SI_SAT<int32>, V4SI_SAT<int32>, NONE),
	  PPC_I(VSUBSWS),
	  VX_form, 4, 1920, CFLOW_NORMAL
	},
	{ "vsububm",
	  EXECUTE_VECTOR_ARITH(sub, V16QI, V16QI, V16QI, NONE),
	  PPC_I(VSUBUBM),
	  VX_form, 4, 1024, CFLOW_NORMAL
	},
	{ "vsububs",
	  EXECUTE_VECTOR_ARITH(sub, V16QI_SAT<uint8>, V16QI_SAT<uint8>, V16QI_SAT<uint8>, NONE),
	  PPC_I(VSUBUBS),
	  VX_form, 4, 1536, CFLOW_NORMAL
	},
	{ "vsubuhm",
	  EXECUTE_VECTOR_ARITH(sub, V8HI, V8HI, V8HI, NONE),
	  PPC_I(VSUBUHM),
	  VX_form, 4, 1088, CFLOW_NORMAL
	},
	{ "vsubuhs",
	  EXECUTE_VECTOR_ARITH(sub, V8HI_SAT<uint16>, V8HI_SAT<uint16>, V8HI_SAT<uint16>, NONE),
	  PPC_I(VSUBUHS),
	  VX_form, 4, 1600, CFLOW_NORMAL
	},
	{ "vsubuwm",
	  EXECUTE_VECTOR_ARITH(sub, V4SI, V4SI, V4SI, NONE),
	  PPC_I(VSUBUWM),
	  VX_form, 4, 1152, CFLOW_NORMAL
	},
	{ "vsubuws",
	  EXECUTE_VECTOR_ARITH(sub_64, V4SI_SAT<uint32>, V4SI_SAT<uint32>, V4SI_SAT<uint32>, NONE),
	  PPC_I(VSUBUWS),
	  VX_form, 4, 1664, CFLOW_NORMAL
	},
	{ "vsumsws",
	  EXECUTE_VECTOR_SUM(1, V4SI_SAT<int32>, V4SIs, V4SIs),
	  PPC_I(VSUMSWS),
	  VX_form, 4, 1928, CFLOW_NORMAL
	},
	{ "vsum2sws",
	  EXECUTE_VECTOR_SUM(2, V4SI_SAT<int32>, V4SIs, V4SIs),
	  PPC_I(VSUM2SWS),
	  VX_form, 4, 1672, CFLOW_NORMAL
	},
	{ "vsum4sbs",
	  EXECUTE_VECTOR_SUM(4, V4SI_SAT<int32>, V16QIs, V4SIs),
	  PPC_I(VSUM4SBS),
	  VX_form, 4, 1800, CFLOW_NORMAL
	},
	{ "vsum4shs",
	  EXECUTE_VECTOR_SUM(4, V4SI_SAT<int32>, V8HIs, V4SIs),
	  PPC_I(VSUM4SHS),
	  VX_form, 4, 1608, CFLOW_NORMAL
	},
	{ "vsum4ubs",
	  EXECUTE_VECTOR_SUM(4, V4SI_SAT<uint32>, V16QI, V4SI),
	  PPC_I(VSUM4UBS),
	  VX_form, 4, 1544, CFLOW_NORMAL
	},
	{ "vupkhpx",
	  EXECUTE_1(vector_unpack_pixel, 0),
	  PPC_I(VUPKHPX),
	  VX_form, 4, 846, CFLOW_NORMAL
	},
	{ "vupkhsb",
	  EXECUTE_VECTOR_UNPACK(0, V8HIms, V16QIms),
	  PPC_I(VUPKHSB),
	  VX_form, 4, 526, CFLOW_NORMAL
	},
	{ "vupkhsh",
	  EXECUTE_VECTOR_UNPACK(0, V4SIs, V8HIms),
	  PPC_I(VUPKHSH),
	  VX_form, 4, 590, CFLOW_NORMAL
	},
	{ "vupklpx",
	  EXECUTE_1(vector_unpack_pixel, 1),
	  PPC_I(VUPKLPX),
	  VX_form, 4, 974, CFLOW_NORMAL
	},
	{ "vupklsb",
	  EXECUTE_VECTOR_UNPACK(1, V8HIms, V16QIms),
	  PPC_I(VUPKLSB),
	  VX_form, 4, 654, CFLOW_NORMAL
	},
	{ "vupklsh",
	  EXECUTE_VECTOR_UNPACK(1, V4SIs, V8HIms),
	  PPC_I(VUPKLSH),
	  VX_form, 4, 718, CFLOW_NORMAL
	},
	{ "vxor",
	  EXECUTE_VECTOR_ARITH(xor_64, V2DI, V2DI, V2DI, NONE),
	  PPC_I(VXOR),
	  VX_form, 4, 1220, CFLOW_NORMAL
	}
};

void powerpc_cpu::init_decoder()
{
	const int ii_count = sizeof(powerpc_ii_table)/sizeof(powerpc_ii_table[0]);
	D(bug("PowerPC decode table has %d entries\n", ii_count));
	assert(ii_count < (1 << (8 * sizeof(ii_index_t))));
	ii_table.reserve(ii_count);

	for (int i = 0; i < ii_count; i++) {
		const instr_info_t * ii = &powerpc_ii_table[i];
		init_decoder_entry(ii);
	}
}

void powerpc_cpu::init_decoder_entry(const instr_info_t * ii)
{
	ii_table.push_back(*ii);
	const ii_index_t ii_index = ii_table.size() - 1;

	assert((ii->format == INVALID_form && ii_index == 0) ||
		   (ii->format != INVALID_form && ii_index != 0) );

	switch (ii->format) {
	case INVALID_form:
		// Initialize all index table
		for (int i = 0; i < II_INDEX_TABLE_SIZE; i++)
			ii_index_table[i] = ii_index;
		break;

	case B_form:
	case D_form:
	case I_form:
	case M_form:
		// Primary opcode only
		for (int j = 0; j < 2048; j++)
			ii_index_table[make_ii_index(ii->opcode, j)] = ii_index;
		break;

	case SC_form:
		// Primary opcode only, with reserved bits
		ii_index_table[make_ii_index(ii->opcode, 2)] = ii_index;
		break;

	case X_form:
	case XL_form:
	case XFX_form:
	case XFL_form:
		// Extended opcode in bits 21..30
		ii_index_table[make_ii_index(ii->opcode, (ii->xo << 1)    )] = ii_index;
		ii_index_table[make_ii_index(ii->opcode, (ii->xo << 1) | 1)] = ii_index;
		break;

	case XO_form:
		// Extended opcode in bits 22..30, with OE bit 21
		ii_index_table[make_ii_index(ii->opcode,             (ii->xo << 1)    )] = ii_index;
		ii_index_table[make_ii_index(ii->opcode, (1 << 10) | (ii->xo << 1)    )] = ii_index;
		ii_index_table[make_ii_index(ii->opcode,             (ii->xo << 1) | 1)] = ii_index;
		ii_index_table[make_ii_index(ii->opcode, (1 << 10) | (ii->xo << 1) | 1)] = ii_index;
		break;

	case A_form:
		// Extended opcode in bits 26..30
		for (int j = 0; j < 32; j++) {
			ii_index_table[make_ii_index(ii->opcode, (j << 6) | (ii->xo << 1)    )] = ii_index;
			ii_index_table[make_ii_index(ii->opcode, (j << 6) | (ii->xo << 1) | 1)] = ii_index;
		}
		break;

	case VX_form:
		// Extended opcode in bits 21..31
		ii_index_table[make_ii_index(ii->opcode, ii->xo)] = ii_index;
		break;

	case VXR_form:
		// Extended opcode in bits 22..31
		ii_index_table[make_ii_index(ii->opcode,             ii->xo)] = ii_index;
		ii_index_table[make_ii_index(ii->opcode, (1 << 10) | ii->xo)] = ii_index;
		break;

	case VA_form:
		// Extended opcode in bits 26..31
		for (int j = 0; j < 32; j++)
			ii_index_table[make_ii_index(ii->opcode, (j << 6) | ii->xo)] = ii_index;
		break;

	default:
		fprintf(stderr, "Unhandled form %d\n", ii->format);
		abort();
		break;
	}
}
