/*
 *  compiler/compemu_midfunc_x86.h - Native MIDFUNCS for IA-32 and AMD64
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
 *      File is included by compemu.h
 *
 */

DECLARE_MIDFUNC(bt_l_ri(RR4 r, IMM i));
DECLARE_MIDFUNC(bt_l_rr(RR4 r, RR4 b));
DECLARE_MIDFUNC(btc_l_ri(RW4 r, IMM i));
DECLARE_MIDFUNC(btc_l_rr(RW4 r, RR4 b));
DECLARE_MIDFUNC(bts_l_ri(RW4 r, IMM i));
DECLARE_MIDFUNC(bts_l_rr(RW4 r, RR4 b));
DECLARE_MIDFUNC(btr_l_ri(RW4 r, IMM i));
DECLARE_MIDFUNC(btr_l_rr(RW4 r, RR4 b));
DECLARE_MIDFUNC(mov_l_rm(W4 d, IMM s));
DECLARE_MIDFUNC(call_r(RR4 r));
DECLARE_MIDFUNC(sub_l_mi(IMM d, IMM s));
DECLARE_MIDFUNC(mov_l_mi(IMM d, IMM s));
DECLARE_MIDFUNC(mov_w_mi(IMM d, IMM s));
DECLARE_MIDFUNC(mov_b_mi(IMM d, IMM s));
DECLARE_MIDFUNC(rol_b_ri(RW1 r, IMM i));
DECLARE_MIDFUNC(rol_w_ri(RW2 r, IMM i));
DECLARE_MIDFUNC(rol_l_ri(RW4 r, IMM i));
DECLARE_MIDFUNC(rol_l_rr(RW4 d, RR1 r));
DECLARE_MIDFUNC(rol_w_rr(RW2 d, RR1 r));
DECLARE_MIDFUNC(rol_b_rr(RW1 d, RR1 r));
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
DECLARE_MIDFUNC(cmov_l_rm(RW4 d, IMM s, IMM cc));
DECLARE_MIDFUNC(bsf_l_rr(W4 d, RR4 s));
DECLARE_MIDFUNC(pop_m(IMM d));
DECLARE_MIDFUNC(push_m(IMM d));
DECLARE_MIDFUNC(pop_l(W4 d));
DECLARE_MIDFUNC(push_l_i(IMM i));
DECLARE_MIDFUNC(push_l(RR4 s));
DECLARE_MIDFUNC(clear_16(RW4 r));
DECLARE_MIDFUNC(clear_8(RW4 r));
DECLARE_MIDFUNC(sign_extend_32_rr(W4 d, RR2 s));
DECLARE_MIDFUNC(sign_extend_16_rr(W4 d, RR2 s));
DECLARE_MIDFUNC(sign_extend_8_rr(W4 d, RR1 s));
DECLARE_MIDFUNC(zero_extend_16_rr(W4 d, RR2 s));
DECLARE_MIDFUNC(zero_extend_8_rr(W4 d, RR1 s));
DECLARE_MIDFUNC(imul_64_32(RW4 d, RW4 s));
DECLARE_MIDFUNC(mul_64_32(RW4 d, RW4 s));
DECLARE_MIDFUNC(simulate_bsf(W4 tmp, RW4 s));
DECLARE_MIDFUNC(imul_32_32(RW4 d, RR4 s));
DECLARE_MIDFUNC(mul_32_32(RW4 d, RR4 s));
DECLARE_MIDFUNC(mov_b_rr(W1 d, RR1 s));
DECLARE_MIDFUNC(mov_w_rr(W2 d, RR2 s));
DECLARE_MIDFUNC(mov_l_rrm_indexed(W4 d,RR4 baser, RR4 index, IMM factor));
DECLARE_MIDFUNC(mov_w_rrm_indexed(W2 d, RR4 baser, RR4 index, IMM factor));
DECLARE_MIDFUNC(mov_b_rrm_indexed(W1 d, RR4 baser, RR4 index, IMM factor));
DECLARE_MIDFUNC(mov_l_mrr_indexed(RR4 baser, RR4 index, IMM factor, RR4 s));
DECLARE_MIDFUNC(mov_w_mrr_indexed(RR4 baser, RR4 index, IMM factor, RR2 s));
DECLARE_MIDFUNC(mov_b_mrr_indexed(RR4 baser, RR4 index, IMM factor, RR1 s));
DECLARE_MIDFUNC(mov_l_bmrr_indexed(IMM base, RR4 baser, RR4 index, IMM factor, RR4 s));
DECLARE_MIDFUNC(mov_w_bmrr_indexed(IMM base, RR4 baser, RR4 index, IMM factor, RR2 s));
DECLARE_MIDFUNC(mov_b_bmrr_indexed(IMM base, RR4 baser, RR4 index, IMM factor, RR1 s));
DECLARE_MIDFUNC(mov_l_brrm_indexed(W4 d, IMM base, RR4 baser, RR4 index, IMM factor));
DECLARE_MIDFUNC(mov_w_brrm_indexed(W2 d, IMM base, RR4 baser, RR4 index, IMM factor));
DECLARE_MIDFUNC(mov_b_brrm_indexed(W1 d, IMM base, RR4 baser, RR4 index, IMM factor));
DECLARE_MIDFUNC(mov_l_rm_indexed(W4 d, IMM base, RR4 index, IMM factor));
DECLARE_MIDFUNC(mov_l_rR(W4 d, RR4 s, IMM offset));
DECLARE_MIDFUNC(mov_w_rR(W2 d, RR4 s, IMM offset));
DECLARE_MIDFUNC(mov_b_rR(W1 d, RR4 s, IMM offset));
DECLARE_MIDFUNC(mov_l_brR(W4 d, RR4 s, IMM offset));
DECLARE_MIDFUNC(mov_w_brR(W2 d, RR4 s, IMM offset));
DECLARE_MIDFUNC(mov_b_brR(W1 d, RR4 s, IMM offset));
DECLARE_MIDFUNC(mov_l_Ri(RR4 d, IMM i, IMM offset));
DECLARE_MIDFUNC(mov_w_Ri(RR4 d, IMM i, IMM offset));
DECLARE_MIDFUNC(mov_b_Ri(RR4 d, IMM i, IMM offset));
DECLARE_MIDFUNC(mov_l_Rr(RR4 d, RR4 s, IMM offset));
DECLARE_MIDFUNC(mov_w_Rr(RR4 d, RR2 s, IMM offset));
DECLARE_MIDFUNC(mov_b_Rr(RR4 d, RR1 s, IMM offset));
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
DECLARE_MIDFUNC(add_l_mi(IMM d, IMM s) );
DECLARE_MIDFUNC(add_w_mi(IMM d, IMM s) );
DECLARE_MIDFUNC(add_b_mi(IMM d, IMM s) );
DECLARE_MIDFUNC(test_l_ri(RR4 d, IMM i));
DECLARE_MIDFUNC(test_l_rr(RR4 d, RR4 s));
DECLARE_MIDFUNC(test_w_rr(RR2 d, RR2 s));
DECLARE_MIDFUNC(test_b_rr(RR1 d, RR1 s));
DECLARE_MIDFUNC(and_l_ri(RW4 d, IMM i));
DECLARE_MIDFUNC(and_l(RW4 d, RR4 s));
DECLARE_MIDFUNC(and_w(RW2 d, RR2 s));
DECLARE_MIDFUNC(and_b(RW1 d, RR1 s));
DECLARE_MIDFUNC(or_l_rm(RW4 d, IMM s));
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
DECLARE_MIDFUNC(cmp_l_ri(RR4 r, IMM i));
DECLARE_MIDFUNC(cmp_w(RR2 d, RR2 s));
DECLARE_MIDFUNC(cmp_b(RR1 d, RR1 s));
DECLARE_MIDFUNC(xor_l(RW4 d, RR4 s));
DECLARE_MIDFUNC(xor_w(RW2 d, RR2 s));
DECLARE_MIDFUNC(xor_b(RW1 d, RR1 s));
DECLARE_MIDFUNC(live_flags(void));
DECLARE_MIDFUNC(dont_care_flags(void));
DECLARE_MIDFUNC(duplicate_carry(void));
DECLARE_MIDFUNC(restore_carry(void));
DECLARE_MIDFUNC(start_needflags(void));
DECLARE_MIDFUNC(end_needflags(void));
DECLARE_MIDFUNC(make_flags_live(void));
DECLARE_MIDFUNC(call_r_11(RR4 r, W4 out1, RR4 in1, IMM osize, IMM isize));
DECLARE_MIDFUNC(call_r_02(RR4 r, RR4 in1, RR4 in2, IMM isize1, IMM isize2));
DECLARE_MIDFUNC(forget_about(W4 r));
DECLARE_MIDFUNC(nop(void));

DECLARE_MIDFUNC(f_forget_about(FW r));
DECLARE_MIDFUNC(fmov_pi(FW r));
DECLARE_MIDFUNC(fmov_log10_2(FW r));
DECLARE_MIDFUNC(fmov_log2_e(FW r));
DECLARE_MIDFUNC(fmov_loge_2(FW r));
DECLARE_MIDFUNC(fmov_1(FW r));
DECLARE_MIDFUNC(fmov_0(FW r));
DECLARE_MIDFUNC(fmov_rm(FW r, MEMR m));
DECLARE_MIDFUNC(fmov_mr(MEMW m, FR r));
DECLARE_MIDFUNC(fmovi_rm(FW r, MEMR m));
DECLARE_MIDFUNC(fmovi_mr(MEMW m, FR r));
DECLARE_MIDFUNC(fmovi_mrb(MEMW m, FR r, double *bounds));
DECLARE_MIDFUNC(fmovs_rm(FW r, MEMR m));
DECLARE_MIDFUNC(fmovs_mr(MEMW m, FR r));
DECLARE_MIDFUNC(fcuts_r(FRW r));
DECLARE_MIDFUNC(fcut_r(FRW r));
DECLARE_MIDFUNC(fmov_ext_mr(MEMW m, FR r));
DECLARE_MIDFUNC(fmov_ext_rm(FW r, MEMR m));
DECLARE_MIDFUNC(fmov_rr(FW d, FR s));
DECLARE_MIDFUNC(fldcw_m_indexed(RR4 index, IMM base));
DECLARE_MIDFUNC(ftst_r(FR r));
DECLARE_MIDFUNC(dont_care_fflags(void));
DECLARE_MIDFUNC(fsqrt_rr(FW d, FR s));
DECLARE_MIDFUNC(fabs_rr(FW d, FR s));
DECLARE_MIDFUNC(frndint_rr(FW d, FR s));
DECLARE_MIDFUNC(fgetexp_rr(FW d, FR s));
DECLARE_MIDFUNC(fgetman_rr(FW d, FR s));
DECLARE_MIDFUNC(fsin_rr(FW d, FR s));
DECLARE_MIDFUNC(fcos_rr(FW d, FR s));
DECLARE_MIDFUNC(ftan_rr(FW d, FR s));
DECLARE_MIDFUNC(fsincos_rr(FW d, FW c, FR s));
DECLARE_MIDFUNC(fscale_rr(FRW d, FR s));
DECLARE_MIDFUNC(ftwotox_rr(FW d, FR s));
DECLARE_MIDFUNC(fetox_rr(FW d, FR s));
DECLARE_MIDFUNC(fetoxM1_rr(FW d, FR s));
DECLARE_MIDFUNC(ftentox_rr(FW d, FR s));
DECLARE_MIDFUNC(flog2_rr(FW d, FR s));
DECLARE_MIDFUNC(flogN_rr(FW d, FR s));
DECLARE_MIDFUNC(flogNP1_rr(FW d, FR s));
DECLARE_MIDFUNC(flog10_rr(FW d, FR s));
DECLARE_MIDFUNC(fasin_rr(FW d, FR s));
DECLARE_MIDFUNC(facos_rr(FW d, FR s));
DECLARE_MIDFUNC(fatan_rr(FW d, FR s));
DECLARE_MIDFUNC(fatanh_rr(FW d, FR s));
DECLARE_MIDFUNC(fsinh_rr(FW d, FR s));
DECLARE_MIDFUNC(fcosh_rr(FW d, FR s));
DECLARE_MIDFUNC(ftanh_rr(FW d, FR s));
DECLARE_MIDFUNC(fneg_rr(FW d, FR s));
DECLARE_MIDFUNC(fadd_rr(FRW d, FR s));
DECLARE_MIDFUNC(fsub_rr(FRW d, FR s));
DECLARE_MIDFUNC(fmul_rr(FRW d, FR s));
DECLARE_MIDFUNC(frem_rr(FRW d, FR s));
DECLARE_MIDFUNC(frem1_rr(FRW d, FR s));
DECLARE_MIDFUNC(fdiv_rr(FRW d, FR s));
DECLARE_MIDFUNC(fcmp_rr(FR d, FR s));
DECLARE_MIDFUNC(fflags_into_flags(W2 tmp));
