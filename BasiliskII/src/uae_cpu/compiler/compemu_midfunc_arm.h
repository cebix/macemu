/*
 *  compiler/compemu_midfunc_arm.h - Native MIDFUNCS for ARM
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
DECLARE_MIDFUNC(arm_ADD_l(RW4 d, RR4 s));
DECLARE_MIDFUNC(arm_ADD_l_ri(RW4 d, IMM i));
DECLARE_MIDFUNC(arm_ADD_l_ri8(RW4 d, IMM i));
DECLARE_MIDFUNC(arm_SUB_l_ri8(RW4 d, IMM i));
DECLARE_MIDFUNC(arm_AND_b(RW1 d, RR1 s));
DECLARE_MIDFUNC(arm_AND_w(RW2 d, RR2 s));
DECLARE_MIDFUNC(arm_AND_l(RW4 d, RR4 s));
DECLARE_MIDFUNC(arm_AND_l_ri8(RW4 d, IMM i));
DECLARE_MIDFUNC(arm_EOR_b(RW1 d, RR1 s));
DECLARE_MIDFUNC(arm_EOR_l(RW4 d, RR4 s));
DECLARE_MIDFUNC(arm_EOR_w(RW2 d, RR2 s));
DECLARE_MIDFUNC(arm_ORR_b(RW1 d, RR1 s));
DECLARE_MIDFUNC(arm_ORR_l(RW4 d, RR4 s));
DECLARE_MIDFUNC(arm_ORR_w(RW2 d, RR2 s));
DECLARE_MIDFUNC(arm_ROR_l_ri8(RW4 r, IMM i));

// Emulated midfunc
DECLARE_MIDFUNC(bt_l_ri(RR4 r, IMM i));
DECLARE_MIDFUNC(bt_l_rr(RR4 r, RR4 b));
DECLARE_MIDFUNC(btc_l_rr(RW4 r, RR4 b));
DECLARE_MIDFUNC(bts_l_rr(RW4 r, RR4 b));
DECLARE_MIDFUNC(btr_l_rr(RW4 r, RR4 b));
DECLARE_MIDFUNC(mov_l_rm(W4 d, IMM s));
DECLARE_MIDFUNC(mov_l_rm_indexed(W4 d, IMM base, RR4 index, IMM factor));
DECLARE_MIDFUNC(mov_l_mi(IMM d, IMM s));
DECLARE_MIDFUNC(mov_w_mi(IMM d, IMM s));
DECLARE_MIDFUNC(mov_b_mi(IMM d, IMM s));
DECLARE_MIDFUNC(rol_b_ri(RW1 r, IMM i));
DECLARE_MIDFUNC(rol_w_ri(RW2 r, IMM i));
DECLARE_MIDFUNC(rol_l_rr(RW4 d, RR1 r));
DECLARE_MIDFUNC(rol_w_rr(RW2 d, RR1 r));
DECLARE_MIDFUNC(rol_b_rr(RW1 d, RR1 r));
DECLARE_MIDFUNC(rol_l_ri(RW4 r, IMM i));
DECLARE_MIDFUNC(shll_l_rr(RW4 d, RR1 r));
DECLARE_MIDFUNC(shll_w_rr(RW2 d, RR1 r));
DECLARE_MIDFUNC(shll_b_rr(RW1 d, RR1 r));
DECLARE_MIDFUNC(ror_b_ri(RR1 r, IMM i));
DECLARE_MIDFUNC(ror_w_ri(RR2 r, IMM i));
DECLARE_MIDFUNC(ror_l_ri(RR4 r, IMM i));
DECLARE_MIDFUNC(ror_l_rr(RR4 d, RR1 r));
DECLARE_MIDFUNC(ror_w_rr(RR2 d, RR1 r));
DECLARE_MIDFUNC(ror_b_rr(RR1 d, RR1 r));
DECLARE_MIDFUNC(shrl_l_rr(RW4 d, RR1 r));
DECLARE_MIDFUNC(shrl_w_rr(RW2 d, RR1 r));
DECLARE_MIDFUNC(shrl_b_rr(RW1 d, RR1 r));
DECLARE_MIDFUNC(shra_l_rr(RW4 d, RR1 r));
DECLARE_MIDFUNC(shra_w_rr(RW2 d, RR1 r));
DECLARE_MIDFUNC(shra_b_rr(RW1 d, RR1 r));
DECLARE_MIDFUNC(shll_l_ri(RW4 r, IMM i));
DECLARE_MIDFUNC(shll_w_ri(RW2 r, IMM i));
DECLARE_MIDFUNC(shll_b_ri(RW1 r, IMM i));
DECLARE_MIDFUNC(shrl_l_ri(RW4 r, IMM i));
DECLARE_MIDFUNC(shrl_w_ri(RW2 r, IMM i));
DECLARE_MIDFUNC(shrl_b_ri(RW1 r, IMM i));
DECLARE_MIDFUNC(shra_l_ri(RW4 r, IMM i));
DECLARE_MIDFUNC(shra_w_ri(RW2 r, IMM i));
DECLARE_MIDFUNC(shra_b_ri(RW1 r, IMM i));
DECLARE_MIDFUNC(setcc(W1 d, IMM cc));
DECLARE_MIDFUNC(setcc_m(IMM d, IMM cc));
DECLARE_MIDFUNC(cmov_l_rr(RW4 d, RR4 s, IMM cc));
DECLARE_MIDFUNC(bsf_l_rr(W4 d, RR4 s));
DECLARE_MIDFUNC(pop_l(W4 d));
DECLARE_MIDFUNC(push_l(RR4 s));
DECLARE_MIDFUNC(sign_extend_16_rr(W4 d, RR2 s));
DECLARE_MIDFUNC(sign_extend_8_rr(W4 d, RR1 s));
DECLARE_MIDFUNC(zero_extend_16_rr(W4 d, RR2 s));
DECLARE_MIDFUNC(zero_extend_8_rr(W4 d, RR1 s));
DECLARE_MIDFUNC(simulate_bsf(W4 tmp, RW4 s));
DECLARE_MIDFUNC(imul_64_32(RW4 d, RW4 s));
DECLARE_MIDFUNC(mul_64_32(RW4 d, RW4 s));
DECLARE_MIDFUNC(imul_32_32(RW4 d, RR4 s));
DECLARE_MIDFUNC(mov_b_rr(W1 d, RR1 s));
DECLARE_MIDFUNC(mov_w_rr(W2 d, RR2 s));
DECLARE_MIDFUNC(mov_l_rR(W4 d, RR4 s, IMM offset));
DECLARE_MIDFUNC(mov_w_rR(W2 d, RR4 s, IMM offset));
DECLARE_MIDFUNC(mov_l_brR(W4 d, RR4 s, IMM offset));
DECLARE_MIDFUNC(mov_w_brR(W2 d, RR4 s, IMM offset));
DECLARE_MIDFUNC(mov_b_brR(W1 d, RR4 s, IMM offset));
DECLARE_MIDFUNC(mov_l_Ri(RR4 d, IMM i, IMM offset));
DECLARE_MIDFUNC(mov_w_Ri(RR4 d, IMM i, IMM offset));
DECLARE_MIDFUNC(mov_l_Rr(RR4 d, RR4 s, IMM offset));
DECLARE_MIDFUNC(mov_w_Rr(RR4 d, RR2 s, IMM offset));
DECLARE_MIDFUNC(lea_l_brr(W4 d, RR4 s, IMM offset));
DECLARE_MIDFUNC(lea_l_brr_indexed(W4 d, RR4 s, RR4 index, IMM factor, IMM offset));
DECLARE_MIDFUNC(lea_l_rr_indexed(W4 d, RR4 s, RR4 index, IMM factor));
DECLARE_MIDFUNC(mov_l_bRr(RR4 d, RR4 s, IMM offset));
DECLARE_MIDFUNC(mov_w_bRr(RR4 d, RR2 s, IMM offset));
DECLARE_MIDFUNC(mov_b_bRr(RR4 d, RR1 s, IMM offset));
DECLARE_MIDFUNC(mid_bswap_32(RW4 r));
DECLARE_MIDFUNC(mid_bswap_16(RW2 r));
DECLARE_MIDFUNC(mov_l_rr(W4 d, RR4 s));
DECLARE_MIDFUNC(mov_l_mr(IMM d, RR4 s));
DECLARE_MIDFUNC(mov_w_mr(IMM d, RR2 s));
DECLARE_MIDFUNC(mov_w_rm(W2 d, IMM s));
DECLARE_MIDFUNC(mov_b_mr(IMM d, RR1 s));
DECLARE_MIDFUNC(mov_b_rm(W1 d, IMM s));
DECLARE_MIDFUNC(mov_l_ri(W4 d, IMM s));
DECLARE_MIDFUNC(mov_w_ri(W2 d, IMM s));
DECLARE_MIDFUNC(mov_b_ri(W1 d, IMM s));
DECLARE_MIDFUNC(test_l_ri(RR4 d, IMM i));
DECLARE_MIDFUNC(test_l_rr(RR4 d, RR4 s));
DECLARE_MIDFUNC(test_w_rr(RR2 d, RR2 s));
DECLARE_MIDFUNC(test_b_rr(RR1 d, RR1 s));
DECLARE_MIDFUNC(and_l_ri(RW4 d, IMM i));
DECLARE_MIDFUNC(and_l(RW4 d, RR4 s));
DECLARE_MIDFUNC(and_w(RW2 d, RR2 s));
DECLARE_MIDFUNC(and_b(RW1 d, RR1 s));
DECLARE_MIDFUNC(or_l_ri(RW4 d, IMM i));
DECLARE_MIDFUNC(or_l(RW4 d, RR4 s));
DECLARE_MIDFUNC(or_w(RW2 d, RR2 s));
DECLARE_MIDFUNC(or_b(RW1 d, RR1 s));
DECLARE_MIDFUNC(adc_l(RW4 d, RR4 s));
DECLARE_MIDFUNC(adc_w(RW2 d, RR2 s));
DECLARE_MIDFUNC(adc_b(RW1 d, RR1 s));
DECLARE_MIDFUNC(add_l(RW4 d, RR4 s));
DECLARE_MIDFUNC(add_w(RW2 d, RR2 s));
DECLARE_MIDFUNC(add_b(RW1 d, RR1 s));
DECLARE_MIDFUNC(sub_l_ri(RW4 d, IMM i));
DECLARE_MIDFUNC(sub_w_ri(RW2 d, IMM i));
DECLARE_MIDFUNC(sub_b_ri(RW1 d, IMM i));
DECLARE_MIDFUNC(add_l_ri(RW4 d, IMM i));
DECLARE_MIDFUNC(add_w_ri(RW2 d, IMM i));
DECLARE_MIDFUNC(add_b_ri(RW1 d, IMM i));
DECLARE_MIDFUNC(sbb_l(RW4 d, RR4 s));
DECLARE_MIDFUNC(sbb_w(RW2 d, RR2 s));
DECLARE_MIDFUNC(sbb_b(RW1 d, RR1 s));
DECLARE_MIDFUNC(sub_l(RW4 d, RR4 s));
DECLARE_MIDFUNC(sub_w(RW2 d, RR2 s));
DECLARE_MIDFUNC(sub_b(RW1 d, RR1 s));
DECLARE_MIDFUNC(cmp_l(RR4 d, RR4 s));
DECLARE_MIDFUNC(cmp_w(RR2 d, RR2 s));
DECLARE_MIDFUNC(cmp_b(RR1 d, RR1 s));
DECLARE_MIDFUNC(xor_l(RW4 d, RR4 s));
DECLARE_MIDFUNC(xor_w(RW2 d, RR2 s));
DECLARE_MIDFUNC(xor_b(RW1 d, RR1 s));
DECLARE_MIDFUNC(call_r_02(RR4 r, RR4 in1, RR4 in2, IMM isize1, IMM isize2));
DECLARE_MIDFUNC(call_r_11(W4 out1, RR4 r, RR4 in1, IMM osize, IMM isize));
DECLARE_MIDFUNC(live_flags(void));
DECLARE_MIDFUNC(dont_care_flags(void));
DECLARE_MIDFUNC(duplicate_carry(void));
DECLARE_MIDFUNC(restore_carry(void));
DECLARE_MIDFUNC(start_needflags(void));
DECLARE_MIDFUNC(end_needflags(void));
DECLARE_MIDFUNC(make_flags_live(void));
DECLARE_MIDFUNC(forget_about(W4 r));
DECLARE_MIDFUNC(nop(void));

DECLARE_MIDFUNC(f_forget_about(FW r));




