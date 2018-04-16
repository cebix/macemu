/*
 *  compiler/compemu_midfunc_arm2.h - Native MIDFUNCS for ARM (JIT v2)
 *
 * Copyright (c) 2014 Jens Heitmann of ARAnyM dev team (see AUTHORS)
 *
 * Inspired by Christian Bauer's Basilisk II
 *
 *  Original 68040 JIT compiler for UAE, copyright 2000-2002 Bernd Meyer
 *
 *  Adaptation for Basilisk II and improvements, copyright 2000-2002
 *    Gwenole Beauchesne
 *
 *  Basilisk II (C) 1997-2002 Christian Bauer
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
 *
 *  Note:
 *  	File is included by compemu.h
 *
 */

// Arm optimized midfunc
extern const uae_u32 ARM_CCR_MAP[];

DECLARE_MIDFUNC(restore_inverted_carry(void));

// ADD
DECLARE_MIDFUNC(jnf_ADD(W4 d, RR4 s, RR4 v));
DECLARE_MIDFUNC(jnf_ADD_imm(W4 d, RR4 s, IMM v));
DECLARE_MIDFUNC(jff_ADD_b(W4 d, RR1 s, RR1 v));
DECLARE_MIDFUNC(jff_ADD_w(W4 d, RR2 s, RR2 v));
DECLARE_MIDFUNC(jff_ADD_l(W4 d, RR4 s, RR4 v));
DECLARE_MIDFUNC(jff_ADD_b_imm(W4 d, RR1 s, IMM v));
DECLARE_MIDFUNC(jff_ADD_w_imm(W4 d, RR2 s, IMM v));
DECLARE_MIDFUNC(jff_ADD_l_imm(W4 d, RR4 s, IMM v));

// ADDA
DECLARE_MIDFUNC(jnf_ADDA_b(W4 d, RR1 s));
DECLARE_MIDFUNC(jnf_ADDA_w(W4 d, RR2 s));
DECLARE_MIDFUNC(jnf_ADDA_l(W4 d, RR4 s));

// ADDX
DECLARE_MIDFUNC(jnf_ADDX(W4 d, RR4 s, RR4 v));
DECLARE_MIDFUNC(jff_ADDX_b(W4 d, RR1 s, RR4 v));
DECLARE_MIDFUNC(jff_ADDX_w(W4 d, RR2 s, RR4 v));
DECLARE_MIDFUNC(jff_ADDX_l(W4 d, RR4 s, RR4 v));

// AND
DECLARE_MIDFUNC(jnf_AND(W4 d, RR4 s, RR4 v));
DECLARE_MIDFUNC(jff_AND_b(W4 d, RR1 s, RR1 v));
DECLARE_MIDFUNC(jff_AND_w(W4 d, RR2 s, RR2 v));
DECLARE_MIDFUNC(jff_AND_l(W4 d, RR4 s, RR4 v));

// ANDSR
DECLARE_MIDFUNC(jff_ANDSR(IMM s, IMM x));

// ASL
DECLARE_MIDFUNC(jff_ASL_b_imm(W4 d, RR4 s, IMM i));
DECLARE_MIDFUNC(jff_ASL_w_imm(W4 d, RR4 s, IMM i));
DECLARE_MIDFUNC(jff_ASL_l_imm(W4 d, RR4 s, IMM i));
DECLARE_MIDFUNC(jff_ASL_b_reg(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_ASL_w_reg(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_ASL_l_reg(W4 d, RR4 s, RR4 i));

// ASLW
DECLARE_MIDFUNC(jff_ASLW(W4 d, RR4 s));
DECLARE_MIDFUNC(jnf_ASLW(W4 d, RR4 s));

// ASR
DECLARE_MIDFUNC(jnf_ASR_b_imm(W4 d, RR4 s, IMM i));
DECLARE_MIDFUNC(jnf_ASR_w_imm(W4 d, RR4 s, IMM i));
DECLARE_MIDFUNC(jnf_ASR_l_imm(W4 d, RR4 s, IMM i));
DECLARE_MIDFUNC(jff_ASR_b_imm(W4 d, RR4 s, IMM i));
DECLARE_MIDFUNC(jff_ASR_w_imm(W4 d, RR4 s, IMM i));
DECLARE_MIDFUNC(jff_ASR_l_imm(W4 d, RR4 s, IMM i));
DECLARE_MIDFUNC(jnf_ASR_b_reg(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jnf_ASR_w_reg(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jnf_ASR_l_reg(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_ASR_b_reg(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_ASR_w_reg(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_ASR_l_reg(W4 d, RR4 s, RR4 i));

// ASRW
DECLARE_MIDFUNC(jff_ASRW(W4 d, RR4 s));
DECLARE_MIDFUNC(jnf_ASRW(W4 d, RR4 s));

// BCHG
DECLARE_MIDFUNC(jnf_BCHG_b_imm(RW4 d, IMM s));
DECLARE_MIDFUNC(jnf_BCHG_l_imm(RW4 d, IMM s));

DECLARE_MIDFUNC(jff_BCHG_b_imm(RW4 d, IMM s));
DECLARE_MIDFUNC(jff_BCHG_l_imm(RW4 d, IMM s));

DECLARE_MIDFUNC(jnf_BCHG_b(RW4 d, RR4 s));
DECLARE_MIDFUNC(jnf_BCHG_l(RW4 d, RR4 s));

DECLARE_MIDFUNC(jff_BCHG_b(RW4 d, RR4 s));
DECLARE_MIDFUNC(jff_BCHG_l(RW4 d, RR4 s));

// BCLR
DECLARE_MIDFUNC(jnf_BCLR_b_imm(RW4 d, IMM s));
DECLARE_MIDFUNC(jnf_BCLR_l_imm(RW4 d, IMM s));

DECLARE_MIDFUNC(jnf_BCLR_b(RW4 d, RR4 s));
DECLARE_MIDFUNC(jnf_BCLR_l(RW4 d, RR4 s));

DECLARE_MIDFUNC(jff_BCLR_b_imm(RW4 d, IMM s));
DECLARE_MIDFUNC(jff_BCLR_l_imm(RW4 d, IMM s));

DECLARE_MIDFUNC(jff_BCLR_b(RW4 d, RR4 s));
DECLARE_MIDFUNC(jff_BCLR_l(RW4 d, RR4 s));

// BSET
DECLARE_MIDFUNC(jnf_BSET_b_imm(RW4 d, IMM s));
DECLARE_MIDFUNC(jnf_BSET_l_imm(RW4 d, IMM s));

DECLARE_MIDFUNC(jnf_BSET_b(RW4 d, RR4 s));
DECLARE_MIDFUNC(jnf_BSET_l(RW4 d, RR4 s));

DECLARE_MIDFUNC(jff_BSET_b_imm(RW4 d, IMM s));
DECLARE_MIDFUNC(jff_BSET_l_imm(RW4 d, IMM s));

DECLARE_MIDFUNC(jff_BSET_b(RW4 d, RR4 s));
DECLARE_MIDFUNC(jff_BSET_l(RW4 d, RR4 s));

// BTST
DECLARE_MIDFUNC(jff_BTST_b_imm(RR4 d, IMM s));
DECLARE_MIDFUNC(jff_BTST_l_imm(RR4 d, IMM s));

DECLARE_MIDFUNC(jff_BTST_b(RR4 d, RR4 s));
DECLARE_MIDFUNC(jff_BTST_l(RR4 d, RR4 s));

// CLR
DECLARE_MIDFUNC (jnf_CLR(W4 d));
DECLARE_MIDFUNC (jff_CLR(W4 d));

// CMP
DECLARE_MIDFUNC(jff_CMP_b(RR1 d, RR1 s));
DECLARE_MIDFUNC(jff_CMP_w(RR2 d, RR2 s));
DECLARE_MIDFUNC(jff_CMP_l(RR4 d, RR4 s));

// CMPA
DECLARE_MIDFUNC(jff_CMPA_b(RR1 d, RR1 s));
DECLARE_MIDFUNC(jff_CMPA_w(RR2 d, RR2 s));
DECLARE_MIDFUNC(jff_CMPA_l(RR4 d, RR4 s));

// EOR
DECLARE_MIDFUNC(jnf_EOR(W4 d, RR4 s, RR4 v));
DECLARE_MIDFUNC(jff_EOR_b(W4 d, RR1 s, RR1 v));
DECLARE_MIDFUNC(jff_EOR_w(W4 d, RR2 s, RR2 v));
DECLARE_MIDFUNC(jff_EOR_l(W4 d, RR4 s, RR4 v));

// EORSR
DECLARE_MIDFUNC(jff_EORSR(IMM s, IMM x));

// EXT
DECLARE_MIDFUNC(jnf_EXT_b(W4 d, RR4 s));
DECLARE_MIDFUNC(jnf_EXT_w(W4 d, RR4 s));
DECLARE_MIDFUNC(jnf_EXT_l(W4 d, RR4 s));
DECLARE_MIDFUNC(jff_EXT_b(W4 d, RR4 s));
DECLARE_MIDFUNC(jff_EXT_w(W4 d, RR4 s));
DECLARE_MIDFUNC(jff_EXT_l(W4 d, RR4 s));

// LSL
DECLARE_MIDFUNC(jnf_LSL_imm(W4 d, RR4 s, IMM i));
DECLARE_MIDFUNC(jnf_LSL_reg(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_LSL_b_imm(W4 d, RR4 s, IMM i));
DECLARE_MIDFUNC(jff_LSL_w_imm(W4 d, RR4 s, IMM i));
DECLARE_MIDFUNC(jff_LSL_l_imm(W4 d, RR4 s, IMM i));
DECLARE_MIDFUNC(jff_LSL_b_reg(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_LSL_w_reg(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_LSL_l_reg(W4 d, RR4 s, RR4 i));

// LSLW
DECLARE_MIDFUNC(jff_LSLW(W4 d, RR4 s));
DECLARE_MIDFUNC(jnf_LSLW(W4 d, RR4 s));

// LSR
DECLARE_MIDFUNC(jnf_LSR_b_imm(W4 d, RR4 s, IMM i));
DECLARE_MIDFUNC(jnf_LSR_w_imm(W4 d, RR4 s, IMM i));
DECLARE_MIDFUNC(jnf_LSR_l_imm(W4 d, RR4 s, IMM i));
DECLARE_MIDFUNC(jff_LSR_b_imm(W4 d, RR4 s, IMM i));
DECLARE_MIDFUNC(jff_LSR_w_imm(W4 d, RR4 s, IMM i));
DECLARE_MIDFUNC(jff_LSR_l_imm(W4 d, RR4 s, IMM i));
DECLARE_MIDFUNC(jnf_LSR_b_reg(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jnf_LSR_w_reg(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jnf_LSR_l_reg(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_LSR_b_reg(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_LSR_w_reg(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_LSR_l_reg(W4 d, RR4 s, RR4 i));

// LSRW
DECLARE_MIDFUNC(jff_LSRW(W4 d, RR4 s));
DECLARE_MIDFUNC(jnf_LSRW(W4 d, RR4 s));

// MOVE
DECLARE_MIDFUNC(jnf_MOVE(W4 d, RR4 s));
DECLARE_MIDFUNC(jff_MOVE_b_imm(W4 d, IMM i));
DECLARE_MIDFUNC(jff_MOVE_w_imm(W4 d, IMM i));
DECLARE_MIDFUNC(jff_MOVE_l_imm(W4 d, IMM i));
DECLARE_MIDFUNC(jff_MOVE_b(W4 d, RR1 s));
DECLARE_MIDFUNC(jff_MOVE_w(W4 d, RR2 s));
DECLARE_MIDFUNC(jff_MOVE_l(W4 d, RR4 s));

// MOVE16
DECLARE_MIDFUNC(jnf_MOVE16(RR4 d, RR4 s));

// MOVEA
DECLARE_MIDFUNC(jnf_MOVEA_w(W4 d, RR2 s));
DECLARE_MIDFUNC(jnf_MOVEA_l(W4 d, RR4 s));

// MULS
DECLARE_MIDFUNC (jnf_MULS(RW4 d, RR4 s));
DECLARE_MIDFUNC (jff_MULS(RW4 d, RR4 s));
DECLARE_MIDFUNC (jnf_MULS32(RW4 d, RR4 s));
DECLARE_MIDFUNC (jff_MULS32(RW4 d, RR4 s));
DECLARE_MIDFUNC (jnf_MULS64(RW4 d, RW4 s));
DECLARE_MIDFUNC (jff_MULS64(RW4 d, RW4 s));

// MULU
DECLARE_MIDFUNC (jnf_MULU(RW4 d, RR4 s));
DECLARE_MIDFUNC (jff_MULU(RW4 d, RR4 s));
DECLARE_MIDFUNC (jnf_MULU32(RW4 d, RR4 s));
DECLARE_MIDFUNC (jff_MULU32(RW4 d, RR4 s));
DECLARE_MIDFUNC (jnf_MULU64(RW4 d, RW4 s));
DECLARE_MIDFUNC (jff_MULU64(RW4 d, RW4 s));

// NEG
DECLARE_MIDFUNC(jnf_NEG(W4 d, RR4 s));
DECLARE_MIDFUNC(jff_NEG_b(W4 d, RR1 s));
DECLARE_MIDFUNC(jff_NEG_w(W4 d, RR2 s));
DECLARE_MIDFUNC(jff_NEG_l(W4 d, RR4 s));

// NEGX
DECLARE_MIDFUNC(jnf_NEGX(W4 d, RR4 s));
DECLARE_MIDFUNC(jff_NEGX_b(W4 d, RR1 s));
DECLARE_MIDFUNC(jff_NEGX_w(W4 d, RR2 s));
DECLARE_MIDFUNC(jff_NEGX_l(W4 d, RR4 s));

// NOT
DECLARE_MIDFUNC(jnf_NOT(W4 d, RR4 s));
DECLARE_MIDFUNC(jff_NOT_b(W4 d, RR1 s));
DECLARE_MIDFUNC(jff_NOT_w(W4 d, RR2 s));
DECLARE_MIDFUNC(jff_NOT_l(W4 d, RR4 s));

// OR
DECLARE_MIDFUNC(jnf_OR(W4 d, RR4 s, RR4 v));
DECLARE_MIDFUNC(jff_OR_b(W4 d, RR1 s, RR1 v));
DECLARE_MIDFUNC(jff_OR_w(W4 d, RR2 s, RR2 v));
DECLARE_MIDFUNC(jff_OR_l(W4 d, RR4 s, RR4 v));

// ORSR
DECLARE_MIDFUNC(jff_ORSR(IMM s, IMM x));

// ROL
DECLARE_MIDFUNC(jnf_ROL_b(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jnf_ROL_w(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jnf_ROL_l(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_ROL_b(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_ROL_w(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_ROL_l(W4 d, RR4 s, RR4 i));

// ROLW
DECLARE_MIDFUNC(jff_ROLW(W4 d, RR4 s));
DECLARE_MIDFUNC(jnf_ROLW(W4 d, RR4 s));

// RORW
DECLARE_MIDFUNC(jff_RORW(W4 d, RR4 s));
DECLARE_MIDFUNC(jnf_RORW(W4 d, RR4 s));

// ROXL
DECLARE_MIDFUNC(jnf_ROXL_b(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jnf_ROXL_w(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jnf_ROXL_l(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_ROXL_b(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_ROXL_w(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_ROXL_l(W4 d, RR4 s, RR4 i));

// ROXLW
DECLARE_MIDFUNC(jff_ROXLW(W4 d, RR4 s));
DECLARE_MIDFUNC(jnf_ROXLW(W4 d, RR4 s));

// ROR
DECLARE_MIDFUNC(jnf_ROR_b(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jnf_ROR_w(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jnf_ROR_l(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_ROR_b(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_ROR_w(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_ROR_l(W4 d, RR4 s, RR4 i));

// ROXR
DECLARE_MIDFUNC(jnf_ROXR_b(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jnf_ROXR_w(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jnf_ROXR_l(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_ROXR_b(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_ROXR_w(W4 d, RR4 s, RR4 i));
DECLARE_MIDFUNC(jff_ROXR_l(W4 d, RR4 s, RR4 i));

// ROXRW
DECLARE_MIDFUNC(jff_ROXRW(W4 d, RR4 s));
DECLARE_MIDFUNC(jnf_ROXRW(W4 d, RR4 s));

// SUB
DECLARE_MIDFUNC(jnf_SUB_b_imm(W4 d, RR4 s, IMM v));
DECLARE_MIDFUNC(jnf_SUB_b(W4 d, RR4 s, RR4 v));
DECLARE_MIDFUNC(jnf_SUB_w_imm(W4 d, RR4 s, IMM v));
DECLARE_MIDFUNC(jnf_SUB_w(W4 d, RR4 s, RR4 v));
DECLARE_MIDFUNC(jnf_SUB_l_imm(W4 d, RR4 s, IMM v));
DECLARE_MIDFUNC(jnf_SUB_l(W4 d, RR4 s, RR4 v));
DECLARE_MIDFUNC(jff_SUB_b(W4 d, RR1 s, RR1 v));
DECLARE_MIDFUNC(jff_SUB_w(W4 d, RR2 s, RR2 v));
DECLARE_MIDFUNC(jff_SUB_l(W4 d, RR4 s, RR4 v));
DECLARE_MIDFUNC(jff_SUB_b_imm(W4 d, RR1 s, IMM v));
DECLARE_MIDFUNC(jff_SUB_w_imm(W4 d, RR2 s, IMM v));
DECLARE_MIDFUNC(jff_SUB_l_imm(W4 d, RR4 s, IMM v));

// SUBA
DECLARE_MIDFUNC(jnf_SUBA_b(W4 d, RR1 s));
DECLARE_MIDFUNC(jnf_SUBA_w(W4 d, RR2 s));
DECLARE_MIDFUNC(jnf_SUBA_l(W4 d, RR4 s));

// SUBX
DECLARE_MIDFUNC(jnf_SUBX(W4 d, RR4 s, RR4 v));
DECLARE_MIDFUNC(jff_SUBX_b(W4 d, RR1 s, RR4 v));
DECLARE_MIDFUNC(jff_SUBX_w(W4 d, RR2 s, RR4 v));
DECLARE_MIDFUNC(jff_SUBX_l(W4 d, RR4 s, RR4 v));

// SWAP
DECLARE_MIDFUNC (jnf_SWAP(RW4 d));
DECLARE_MIDFUNC (jff_SWAP(RW4 d));

// TST
DECLARE_MIDFUNC (jff_TST_b(RR1 s));
DECLARE_MIDFUNC (jff_TST_w(RR2 s));
DECLARE_MIDFUNC (jff_TST_l(RR4 s));

