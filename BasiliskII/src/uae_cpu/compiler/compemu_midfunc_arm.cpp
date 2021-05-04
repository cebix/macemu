/*
 * compiler/compemu_midfunc_arm.cpp - Native MIDFUNCS for ARM
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
 *  	File is included by compemu_support.cpp
 *
 */

/********************************************************************
 * CPU functions exposed to gencomp. Both CREATE and EMIT time      *
 ********************************************************************/

/*
 *  RULES FOR HANDLING REGISTERS:
 *
 *  * In the function headers, order the parameters
 *     - 1st registers written to
 *     - 2nd read/modify/write registers
 *     - 3rd registers read from
 *  * Before calling raw_*, you must call readreg, writereg or rmw for
 *    each register
 *  * The order for this is
 *     - 1st call remove_offset for all registers written to with size<4
 *     - 2nd call readreg for all registers read without offset
 *     - 3rd call rmw for all rmw registers
 *     - 4th call readreg_offset for all registers that can handle offsets
 *     - 5th call get_offset for all the registers from the previous step
 *     - 6th call writereg for all written-to registers
 *     - 7th call raw_*
 *     - 8th unlock2 all registers that were locked
 */

MIDFUNC(0,live_flags,(void))
{
	live.flags_on_stack=TRASH;
	live.flags_in_flags=VALID;
	live.flags_are_important=1;
}

MIDFUNC(0,dont_care_flags,(void))
{
	live.flags_are_important=0;
}

MIDFUNC(0,duplicate_carry,(void))
{
	evict(FLAGX);
	make_flags_live_internal();
	COMPCALL(setcc_m)((uintptr)live.state[FLAGX].mem,NATIVE_CC_CS);
	log_vwrite(FLAGX);
}

MIDFUNC(0,restore_carry,(void))
{
#if defined(USE_JIT2)
	RR4 r=readreg(FLAGX,4);
	MRS_CPSR(REG_WORK1);
	TEQ_ri(r,1);
	CC_BIC_rri(NATIVE_CC_NE, REG_WORK1, REG_WORK1, ARM_C_FLAG);
	CC_ORR_rri(NATIVE_CC_EQ, REG_WORK1, REG_WORK1, ARM_C_FLAG);
	MSR_CPSRf_r(REG_WORK1);
	unlock2(r);
#else
	if (!have_rat_stall) { /* Not a P6 core, i.e. no partial stalls */
		bt_l_ri_noclobber(FLAGX,0);
	}
	else { /* Avoid the stall the above creates.
	 This is slow on non-P6, though.
	 */
		COMPCALL(rol_b_ri(FLAGX,8));
		isclean(FLAGX);
	}
#endif
}

MIDFUNC(0,start_needflags,(void))
{
	needflags=1;
}

MIDFUNC(0,end_needflags,(void))
{
	needflags=0;
}

MIDFUNC(0,make_flags_live,(void))
{
	make_flags_live_internal();
}

MIDFUNC(2,bt_l_ri,(RR4 r, IMM i)) /* This is defined as only affecting C */
{
	int size=4;
	if (i<16)
	size=2;
	CLOBBER_BT;
	r=readreg(r,size);
	raw_bt_l_ri(r,i);
	unlock2(r);
}

MIDFUNC(2,bt_l_rr,(RR4 r, RR4 b)) /* This is defined as only affecting C */
{
	CLOBBER_BT;
	r=readreg(r,4);
	b=readreg(b,4);
	raw_bt_l_rr(r,b);
	unlock2(r);
	unlock2(b);
}

MIDFUNC(2,btc_l_rr,(RW4 r, RR4 b))
{
	CLOBBER_BT;
	b=readreg(b,4);
	r=rmw(r,4,4);
	raw_btc_l_rr(r,b);
	unlock2(r);
	unlock2(b);
}

MIDFUNC(2,btr_l_rr,(RW4 r, RR4 b))
{
	CLOBBER_BT;
	b=readreg(b,4);
	r=rmw(r,4,4);
	raw_btr_l_rr(r,b);
	unlock2(r);
	unlock2(b);
}

MIDFUNC(2,bts_l_rr,(RW4 r, RR4 b))
{
	CLOBBER_BT;
	b=readreg(b,4);
	r=rmw(r,4,4);
	raw_bts_l_rr(r,b);
	unlock2(r);
	unlock2(b);
}

MIDFUNC(2,mov_l_rm,(W4 d, IMM s))
{
	CLOBBER_MOV;
	d=writereg(d,4);
	raw_mov_l_rm(d,s);
	unlock2(d);
}

MIDFUNC(4,mov_l_rm_indexed,(W4 d, IMM base, RR4 index, IMM factor))
{
	CLOBBER_MOV;
	index=readreg(index,4);
	d=writereg(d,4);
	raw_mov_l_rm_indexed(d,base,index,factor);
	unlock2(index);
	unlock2(d);
}

MIDFUNC(2,mov_l_mi,(IMM d, IMM s))
{
	CLOBBER_MOV;
	raw_mov_l_mi(d,s);
}

MIDFUNC(2,mov_w_mi,(IMM d, IMM s))
{
	CLOBBER_MOV;
	raw_mov_w_mi(d,s);
}

MIDFUNC(2,mov_b_mi,(IMM d, IMM s))
{
	CLOBBER_MOV;
	raw_mov_b_mi(d,s);
}

MIDFUNC(2,rol_b_ri,(RW1 r, IMM i))
{
	if (!i && !needflags)
	return;
	CLOBBER_ROL;
	r=rmw(r,1,1);
	raw_rol_b_ri(r,i);
	unlock2(r);
}

MIDFUNC(2,rol_w_ri,(RW2 r, IMM i))
{
	if (!i && !needflags)
	return;
	CLOBBER_ROL;
	r=rmw(r,2,2);
	raw_rol_w_ri(r,i);
	unlock2(r);
}

MIDFUNC(2,rol_l_ri,(RW4 r, IMM i))
{
	if (!i && !needflags)
	return;
	CLOBBER_ROL;
	r=rmw(r,4,4);
	raw_rol_l_ri(r,i);
	unlock2(r);
}

MIDFUNC(2,rol_l_rr,(RW4 d, RR1 r))
{
	if (isconst(r)) {
		COMPCALL(rol_l_ri)(d,(uae_u8)live.state[r].val);
		return;
	}
	CLOBBER_ROL;
	r=readreg(r,1);
	d=rmw(d,4,4);
	raw_rol_l_rr(d,r);
	unlock2(r);
	unlock2(d);
}

MIDFUNC(2,rol_w_rr,(RW2 d, RR1 r))
{ /* Can only do this with r==1, i.e. cl */

	if (isconst(r)) {
		COMPCALL(rol_w_ri)(d,(uae_u8)live.state[r].val);
		return;
	}
	CLOBBER_ROL;
	r=readreg(r,1);
	d=rmw(d,2,2);
	raw_rol_w_rr(d,r);
	unlock2(r);
	unlock2(d);
}

MIDFUNC(2,rol_b_rr,(RW1 d, RR1 r))
{ /* Can only do this with r==1, i.e. cl */

	if (isconst(r)) {
		COMPCALL(rol_b_ri)(d,(uae_u8)live.state[r].val);
		return;
	}
	CLOBBER_ROL;
	r=readreg(r,1);
	d=rmw(d,1,1);
	raw_rol_b_rr(d,r);
	unlock2(r);
	unlock2(d);
}

MIDFUNC(2,shll_l_rr,(RW4 d, RR1 r))
{
	if (isconst(r)) {
		COMPCALL(shll_l_ri)(d,(uae_u8)live.state[r].val);
		return;
	}
	CLOBBER_SHLL;
	r=readreg(r,1);
	d=rmw(d,4,4);
	raw_shll_l_rr(d,r);
	unlock2(r);
	unlock2(d);
}

MIDFUNC(2,shll_w_rr,(RW2 d, RR1 r))
{ /* Can only do this with r==1, i.e. cl */

	if (isconst(r)) {
		COMPCALL(shll_w_ri)(d,(uae_u8)live.state[r].val);
		return;
	}
	CLOBBER_SHLL;
	r=readreg(r,1);
	d=rmw(d,2,2);
	raw_shll_w_rr(d,r);
	unlock2(r);
	unlock2(d);
}

MIDFUNC(2,shll_b_rr,(RW1 d, RR1 r))
{ /* Can only do this with r==1, i.e. cl */

	if (isconst(r)) {
		COMPCALL(shll_b_ri)(d,(uae_u8)live.state[r].val);
		return;
	}
	CLOBBER_SHLL;
	r=readreg(r,1);
	d=rmw(d,1,1);
	raw_shll_b_rr(d,r);
	unlock2(r);
	unlock2(d);
}

MIDFUNC(2,ror_b_ri,(RR1 r, IMM i))
{
	if (!i && !needflags)
	return;
	CLOBBER_ROR;
	r=rmw(r,1,1);
	raw_ror_b_ri(r,i);
	unlock2(r);
}

MIDFUNC(2,ror_w_ri,(RR2 r, IMM i))
{
	if (!i && !needflags)
	return;
	CLOBBER_ROR;
	r=rmw(r,2,2);
	raw_ror_w_ri(r,i);
	unlock2(r);
}

MIDFUNC(2,ror_l_ri,(RR4 r, IMM i))
{
	if (!i && !needflags)
	return;
	CLOBBER_ROR;
	r=rmw(r,4,4);
	raw_ror_l_ri(r,i);
	unlock2(r);
}

MIDFUNC(2,ror_l_rr,(RR4 d, RR1 r))
{
	if (isconst(r)) {
		COMPCALL(ror_l_ri)(d,(uae_u8)live.state[r].val);
		return;
	}
	CLOBBER_ROR;
	r=readreg(r,1);
	d=rmw(d,4,4);
	raw_ror_l_rr(d,r);
	unlock2(r);
	unlock2(d);
}

MIDFUNC(2,ror_w_rr,(RR2 d, RR1 r))
{
	if (isconst(r)) {
		COMPCALL(ror_w_ri)(d,(uae_u8)live.state[r].val);
		return;
	}
	CLOBBER_ROR;
	r=readreg(r,1);
	d=rmw(d,2,2);
	raw_ror_w_rr(d,r);
	unlock2(r);
	unlock2(d);
}

MIDFUNC(2,ror_b_rr,(RR1 d, RR1 r))
{
	if (isconst(r)) {
		COMPCALL(ror_b_ri)(d,(uae_u8)live.state[r].val);
		return;
	}

	CLOBBER_ROR;
	r=readreg(r,1);
	d=rmw(d,1,1);
	raw_ror_b_rr(d,r);
	unlock2(r);
	unlock2(d);
}

MIDFUNC(2,shrl_l_rr,(RW4 d, RR1 r))
{
	if (isconst(r)) {
		COMPCALL(shrl_l_ri)(d,(uae_u8)live.state[r].val);
		return;
	}
	CLOBBER_SHRL;
	r=readreg(r,1);
	d=rmw(d,4,4);
	raw_shrl_l_rr(d,r);
	unlock2(r);
	unlock2(d);
}

MIDFUNC(2,shrl_w_rr,(RW2 d, RR1 r))
{ /* Can only do this with r==1, i.e. cl */

	if (isconst(r)) {
		COMPCALL(shrl_w_ri)(d,(uae_u8)live.state[r].val);
		return;
	}
	CLOBBER_SHRL;
	r=readreg(r,1);
	d=rmw(d,2,2);
	raw_shrl_w_rr(d,r);
	unlock2(r);
	unlock2(d);
}

MIDFUNC(2,shrl_b_rr,(RW1 d, RR1 r))
{ /* Can only do this with r==1, i.e. cl */

	if (isconst(r)) {
		COMPCALL(shrl_b_ri)(d,(uae_u8)live.state[r].val);
		return;
	}

	CLOBBER_SHRL;
	r=readreg(r,1);
	d=rmw(d,1,1);
	raw_shrl_b_rr(d,r);
	unlock2(r);
	unlock2(d);
}

MIDFUNC(2,shll_l_ri,(RW4 r, IMM i))
{
	if (!i && !needflags)
	return;
	if (isconst(r) && !needflags) {
		live.state[r].val<<=i;
		return;
	}
	CLOBBER_SHLL;
	r=rmw(r,4,4);
	raw_shll_l_ri(r,i);
	unlock2(r);
}

MIDFUNC(2,shll_w_ri,(RW2 r, IMM i))
{
	if (!i && !needflags)
	return;
	CLOBBER_SHLL;
	r=rmw(r,2,2);
	raw_shll_w_ri(r,i);
	unlock2(r);
}

MIDFUNC(2,shll_b_ri,(RW1 r, IMM i))
{
	if (!i && !needflags)
	return;
	CLOBBER_SHLL;
	r=rmw(r,1,1);
	raw_shll_b_ri(r,i);
	unlock2(r);
}

MIDFUNC(2,shrl_l_ri,(RW4 r, IMM i))
{
	if (!i && !needflags)
	return;
	if (isconst(r) && !needflags) {
		live.state[r].val>>=i;
		return;
	}
	CLOBBER_SHRL;
	r=rmw(r,4,4);
	raw_shrl_l_ri(r,i);
	unlock2(r);
}

MIDFUNC(2,shrl_w_ri,(RW2 r, IMM i))
{
	if (!i && !needflags)
	return;
	CLOBBER_SHRL;
	r=rmw(r,2,2);
	raw_shrl_w_ri(r,i);
	unlock2(r);
}

MIDFUNC(2,shrl_b_ri,(RW1 r, IMM i))
{
	if (!i && !needflags)
	return;
	CLOBBER_SHRL;
	r=rmw(r,1,1);
	raw_shrl_b_ri(r,i);
	unlock2(r);
}

MIDFUNC(2,shra_l_ri,(RW4 r, IMM i))
{
	if (!i && !needflags)
	return;
	CLOBBER_SHRA;
	r=rmw(r,4,4);
	raw_shra_l_ri(r,i);
	unlock2(r);
}

MIDFUNC(2,shra_w_ri,(RW2 r, IMM i))
{
	if (!i && !needflags)
	return;
	CLOBBER_SHRA;
	r=rmw(r,2,2);
	raw_shra_w_ri(r,i);
	unlock2(r);
}

MIDFUNC(2,shra_b_ri,(RW1 r, IMM i))
{
	if (!i && !needflags)
	return;
	CLOBBER_SHRA;
	r=rmw(r,1,1);
	raw_shra_b_ri(r,i);
	unlock2(r);
}

MIDFUNC(2,shra_l_rr,(RW4 d, RR1 r))
{
	if (isconst(r)) {
		COMPCALL(shra_l_ri)(d,(uae_u8)live.state[r].val);
		return;
	}
	CLOBBER_SHRA;
	r=readreg(r,1);
	d=rmw(d,4,4);
	raw_shra_l_rr(d,r);
	unlock2(r);
	unlock2(d);
}

MIDFUNC(2,shra_w_rr,(RW2 d, RR1 r))
{ /* Can only do this with r==1, i.e. cl */

	if (isconst(r)) {
		COMPCALL(shra_w_ri)(d,(uae_u8)live.state[r].val);
		return;
	}
	CLOBBER_SHRA;
	r=readreg(r,1);
	d=rmw(d,2,2);
	raw_shra_w_rr(d,r);
	unlock2(r);
	unlock2(d);
}

MIDFUNC(2,shra_b_rr,(RW1 d, RR1 r))
{ /* Can only do this with r==1, i.e. cl */

	if (isconst(r)) {
		COMPCALL(shra_b_ri)(d,(uae_u8)live.state[r].val);
		return;
	}

	CLOBBER_SHRA;
	r=readreg(r,1);
	d=rmw(d,1,1);
	raw_shra_b_rr(d,r);
	unlock2(r);
	unlock2(d);
}

MIDFUNC(2,setcc,(W1 d, IMM cc))
{
	CLOBBER_SETCC;
	d=writereg(d,1);
	raw_setcc(d,cc);
	unlock2(d);
}

MIDFUNC(2,setcc_m,(IMM d, IMM cc))
{
	CLOBBER_SETCC;
	raw_setcc_m(d,cc);
}

MIDFUNC(3,cmov_l_rr,(RW4 d, RR4 s, IMM cc))
{
	if (d==s)
	return;
	CLOBBER_CMOV;
	s=readreg(s,4);
	d=rmw(d,4,4);
	raw_cmov_l_rr(d,s,cc);
	unlock2(s);
	unlock2(d);
}

MIDFUNC(2,bsf_l_rr,(W4 d, W4 s))
{
	CLOBBER_BSF;
	s = readreg(s, 4);
	d = writereg(d, 4);
	raw_bsf_l_rr(d, s);
	unlock2(s);
	unlock2(d);
}

/* Set the Z flag depending on the value in s. Note that the
 value has to be 0 or -1 (or, more precisely, for non-zero
 values, bit 14 must be set)! */
MIDFUNC(2,simulate_bsf,(W4 tmp, RW4 s))
{
	CLOBBER_BSF;
	s=rmw_specific(s,4,4,FLAG_NREG3);
	tmp=writereg(tmp,4);
	raw_flags_set_zero(s, tmp);
	unlock2(tmp);
	unlock2(s);
}

MIDFUNC(2,imul_32_32,(RW4 d, RR4 s))
{
	CLOBBER_MUL;
	s=readreg(s,4);
	d=rmw(d,4,4);
	raw_imul_32_32(d,s);
	unlock2(s);
	unlock2(d);
}

MIDFUNC(2,imul_64_32,(RW4 d, RW4 s))
{
	CLOBBER_MUL;
	s=rmw_specific(s,4,4,MUL_NREG2);
	d=rmw_specific(d,4,4,MUL_NREG1);
	raw_imul_64_32(d,s);
	unlock2(s);
	unlock2(d);
}

MIDFUNC(2,mul_64_32,(RW4 d, RW4 s))
{
	CLOBBER_MUL;
	s=rmw_specific(s,4,4,MUL_NREG2);
	d=rmw_specific(d,4,4,MUL_NREG1);
	raw_mul_64_32(d,s);
	unlock2(s);
	unlock2(d);
}

MIDFUNC(2,sign_extend_16_rr,(W4 d, RR2 s))
{
	int isrmw;

	if (isconst(s)) {
		set_const(d,(uae_s32)(uae_s16)live.state[s].val);
		return;
	}

	CLOBBER_SE16;
	isrmw=(s==d);
	if (!isrmw) {
		s=readreg(s,2);
		d=writereg(d,4);
	}
	else { /* If we try to lock this twice, with different sizes, we
	 are int trouble! */
		s=d=rmw(s,4,2);
	}
	raw_sign_extend_16_rr(d,s);
	if (!isrmw) {
		unlock2(d);
		unlock2(s);
	}
	else {
		unlock2(s);
	}
}

MIDFUNC(2,sign_extend_8_rr,(W4 d, RR1 s))
{
	int isrmw;

	if (isconst(s)) {
		set_const(d,(uae_s32)(uae_s8)live.state[s].val);
		return;
	}

	isrmw=(s==d);
	CLOBBER_SE8;
	if (!isrmw) {
		s=readreg(s,1);
		d=writereg(d,4);
	}
	else { /* If we try to lock this twice, with different sizes, we
	 are int trouble! */
		s=d=rmw(s,4,1);
	}

	raw_sign_extend_8_rr(d,s);

	if (!isrmw) {
		unlock2(d);
		unlock2(s);
	}
	else {
		unlock2(s);
	}
}

MIDFUNC(2,zero_extend_16_rr,(W4 d, RR2 s))
{
	int isrmw;

	if (isconst(s)) {
		set_const(d,(uae_u32)(uae_u16)live.state[s].val);
		return;
	}

	isrmw=(s==d);
	CLOBBER_ZE16;
	if (!isrmw) {
		s=readreg(s,2);
		d=writereg(d,4);
	}
	else { /* If we try to lock this twice, with different sizes, we
	 are int trouble! */
		s=d=rmw(s,4,2);
	}
	raw_zero_extend_16_rr(d,s);
	if (!isrmw) {
		unlock2(d);
		unlock2(s);
	}
	else {
		unlock2(s);
	}
}

MIDFUNC(2,zero_extend_8_rr,(W4 d, RR1 s))
{
	int isrmw;
	if (isconst(s)) {
		set_const(d,(uae_u32)(uae_u8)live.state[s].val);
		return;
	}

	isrmw=(s==d);
	CLOBBER_ZE8;
	if (!isrmw) {
		s=readreg(s,1);
		d=writereg(d,4);
	}
	else { /* If we try to lock this twice, with different sizes, we
	 are int trouble! */
		s=d=rmw(s,4,1);
	}

	raw_zero_extend_8_rr(d,s);

	if (!isrmw) {
		unlock2(d);
		unlock2(s);
	}
	else {
		unlock2(s);
	}
}

MIDFUNC(2,mov_b_rr,(W1 d, RR1 s))
{
	if (d==s)
	return;
	if (isconst(s)) {
		COMPCALL(mov_b_ri)(d,(uae_u8)live.state[s].val);
		return;
	}

	CLOBBER_MOV;
	s=readreg(s,1);
	d=writereg(d,1);
	raw_mov_b_rr(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,mov_w_rr,(W2 d, RR2 s))
{
	if (d==s)
	return;
	if (isconst(s)) {
		COMPCALL(mov_w_ri)(d,(uae_u16)live.state[s].val);
		return;
	}

	CLOBBER_MOV;
	s=readreg(s,2);
	d=writereg(d,2);
	raw_mov_w_rr(d,s);
	unlock2(d);
	unlock2(s);
}

/* read the long at the address contained in s+offset and store in d */
MIDFUNC(3,mov_l_rR,(W4 d, RR4 s, IMM offset))
{
	if (isconst(s)) {
		COMPCALL(mov_l_rm)(d,live.state[s].val+offset);
		return;
	}
	CLOBBER_MOV;
	s=readreg(s,4);
	d=writereg(d,4);

	raw_mov_l_rR(d,s,offset);
	unlock2(d);
	unlock2(s);
}

/* read the word at the address contained in s+offset and store in d */
MIDFUNC(3,mov_w_rR,(W2 d, RR4 s, IMM offset))
{
	if (isconst(s)) {
		COMPCALL(mov_w_rm)(d,live.state[s].val+offset);
		return;
	}
	CLOBBER_MOV;
	s=readreg(s,4);
	d=writereg(d,2);

	raw_mov_w_rR(d,s,offset);
	unlock2(d);
	unlock2(s);
}

/* read the long at the address contained in s+offset and store in d */
MIDFUNC(3,mov_l_brR,(W4 d, RR4 s, IMM offset))
{
	int sreg=s;
	if (isconst(s)) {
		COMPCALL(mov_l_rm)(d,live.state[s].val+offset);
		return;
	}
	CLOBBER_MOV;
	s=readreg_offset(s,4);
	offset+=get_offset(sreg);
	d=writereg(d,4);

	raw_mov_l_brR(d,s,offset);
	unlock2(d);
	unlock2(s);
}

/* read the word at the address contained in s+offset and store in d */
MIDFUNC(3,mov_w_brR,(W2 d, RR4 s, IMM offset))
{
	int sreg=s;
	if (isconst(s)) {
		COMPCALL(mov_w_rm)(d,live.state[s].val+offset);
		return;
	}
	CLOBBER_MOV;
	remove_offset(d,-1);
	s=readreg_offset(s,4);
	offset+=get_offset(sreg);
	d=writereg(d,2);

	raw_mov_w_brR(d,s,offset);
	unlock2(d);
	unlock2(s);
}

/* read the word at the address contained in s+offset and store in d */
MIDFUNC(3,mov_b_brR,(W1 d, RR4 s, IMM offset))
{
	int sreg=s;
	if (isconst(s)) {
		COMPCALL(mov_b_rm)(d,live.state[s].val+offset);
		return;
	}
	CLOBBER_MOV;
	remove_offset(d,-1);
	s=readreg_offset(s,4);
	offset+=get_offset(sreg);
	d=writereg(d,1);

	raw_mov_b_brR(d,s,offset);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(3,mov_l_Ri,(RR4 d, IMM i, IMM offset))
{
	int dreg=d;
	if (isconst(d)) {
		COMPCALL(mov_l_mi)(live.state[d].val+offset,i);
		return;
	}

	CLOBBER_MOV;
	d=readreg_offset(d,4);
	offset+=get_offset(dreg);
	raw_mov_l_Ri(d,i,offset);
	unlock2(d);
}

MIDFUNC(3,mov_w_Ri,(RR4 d, IMM i, IMM offset))
{
	int dreg=d;
	if (isconst(d)) {
		COMPCALL(mov_w_mi)(live.state[d].val+offset,i);
		return;
	}

	CLOBBER_MOV;
	d=readreg_offset(d,4);
	offset+=get_offset(dreg);
	raw_mov_w_Ri(d,i,offset);
	unlock2(d);
}

/* Warning! OFFSET is byte sized only! */
MIDFUNC(3,mov_l_Rr,(RR4 d, RR4 s, IMM offset))
{
	if (isconst(d)) {
		COMPCALL(mov_l_mr)(live.state[d].val+offset,s);
		return;
	}
	if (isconst(s)) {
		COMPCALL(mov_l_Ri)(d,live.state[s].val,offset);
		return;
	}

	CLOBBER_MOV;
	s=readreg(s,4);
	d=readreg(d,4);

	raw_mov_l_Rr(d,s,offset);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(3,mov_w_Rr,(RR4 d, RR2 s, IMM offset))
{
	if (isconst(d)) {
		COMPCALL(mov_w_mr)(live.state[d].val+offset,s);
		return;
	}
	if (isconst(s)) {
		COMPCALL(mov_w_Ri)(d,(uae_u16)live.state[s].val,offset);
		return;
	}

	CLOBBER_MOV;
	s=readreg(s,2);
	d=readreg(d,4);
	raw_mov_w_Rr(d,s,offset);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(3,lea_l_brr,(W4 d, RR4 s, IMM offset))
{
	if (isconst(s)) {
		COMPCALL(mov_l_ri)(d,live.state[s].val+offset);
		return;
	}
#if USE_OFFSET
	if (d==s) {
		add_offset(d,offset);
		return;
	}
#endif
	CLOBBER_LEA;
	s=readreg(s,4);
	d=writereg(d,4);
	raw_lea_l_brr(d,s,offset);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(5,lea_l_brr_indexed,(W4 d, RR4 s, RR4 index, IMM factor, IMM offset))
{
	if (!offset) {
		COMPCALL(lea_l_rr_indexed)(d,s,index,factor);
		return;
	}
	CLOBBER_LEA;
	s=readreg(s,4);
	index=readreg(index,4);
	d=writereg(d,4);

	raw_lea_l_brr_indexed(d,s,index,factor,offset);
	unlock2(d);
	unlock2(index);
	unlock2(s);
}

MIDFUNC(4,lea_l_rr_indexed,(W4 d, RR4 s, RR4 index, IMM factor))
{
	CLOBBER_LEA;
	s=readreg(s,4);
	index=readreg(index,4);
	d=writereg(d,4);

	raw_lea_l_rr_indexed(d,s,index,factor);
	unlock2(d);
	unlock2(index);
	unlock2(s);
}

/* write d to the long at the address contained in s+offset */
MIDFUNC(3,mov_l_bRr,(RR4 d, RR4 s, IMM offset))
{
	int dreg=d;
	if (isconst(d)) {
		COMPCALL(mov_l_mr)(live.state[d].val+offset,s);
		return;
	}

	CLOBBER_MOV;
	s=readreg(s,4);
	d=readreg_offset(d,4);
	offset+=get_offset(dreg);

	raw_mov_l_bRr(d,s,offset);
	unlock2(d);
	unlock2(s);
}

/* write the word at the address contained in s+offset and store in d */
MIDFUNC(3,mov_w_bRr,(RR4 d, RR2 s, IMM offset))
{
	int dreg=d;

	if (isconst(d)) {
		COMPCALL(mov_w_mr)(live.state[d].val+offset,s);
		return;
	}

	CLOBBER_MOV;
	s=readreg(s,2);
	d=readreg_offset(d,4);
	offset+=get_offset(dreg);
	raw_mov_w_bRr(d,s,offset);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(3,mov_b_bRr,(RR4 d, RR1 s, IMM offset))
{
	int dreg=d;
	if (isconst(d)) {
		COMPCALL(mov_b_mr)(live.state[d].val+offset,s);
		return;
	}

	CLOBBER_MOV;
	s=readreg(s,1);
	d=readreg_offset(d,4);
	offset+=get_offset(dreg);
	raw_mov_b_bRr(d,s,offset);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(1,mid_bswap_32,(RW4 r))
{

	if (isconst(r)) {
		uae_u32 oldv=live.state[r].val;
		live.state[r].val=reverse32(oldv);
		return;
	}

	CLOBBER_SW32;
	r=rmw(r,4,4);
	raw_bswap_32(r);
	unlock2(r);
}

MIDFUNC(1,mid_bswap_16,(RW2 r))
{
	if (isconst(r)) {
		uae_u32 oldv=live.state[r].val;
		live.state[r].val=((oldv>>8)&0xff) | ((oldv<<8)&0xff00) |
		(oldv&0xffff0000);
		return;
	}

	CLOBBER_SW16;
	r=rmw(r,2,2);

	raw_bswap_16(r);
	unlock2(r);
}

MIDFUNC(2,mov_l_rr,(W4 d, RR4 s))
{
	int olds;

	if (d==s) { /* How pointless! */
		return;
	}
	if (isconst(s)) {
		COMPCALL(mov_l_ri)(d,live.state[s].val);
		return;
	}
	olds=s;
	disassociate(d);
	s=readreg_offset(s,4);
	live.state[d].realreg=s;
	live.state[d].realind=live.nat[s].nholds;
	live.state[d].val=live.state[olds].val;
	live.state[d].validsize=4;
	live.state[d].dirtysize=4;
	set_status(d,DIRTY);

	live.nat[s].holds[live.nat[s].nholds]=d;
	live.nat[s].nholds++;
	log_clobberreg(d);
	D2(panicbug("Added %d to nreg %d(%d), now holds %d regs", d,s,live.state[d].realind,live.nat[s].nholds));
	unlock2(s);
}

MIDFUNC(2,mov_l_mr,(IMM d, RR4 s))
{
	if (isconst(s)) {
		COMPCALL(mov_l_mi)(d,live.state[s].val);
		return;
	}
	CLOBBER_MOV;
	s=readreg(s,4);

	raw_mov_l_mr(d,s);
	unlock2(s);
}

MIDFUNC(2,mov_w_mr,(IMM d, RR2 s))
{
	if (isconst(s)) {
		COMPCALL(mov_w_mi)(d,(uae_u16)live.state[s].val);
		return;
	}
	CLOBBER_MOV;
	s=readreg(s,2);

	raw_mov_w_mr(d,s);
	unlock2(s);
}

MIDFUNC(2,mov_w_rm,(W2 d, IMM s))
{
	CLOBBER_MOV;
	d=writereg(d,2);

	raw_mov_w_rm(d,s);
	unlock2(d);
}

MIDFUNC(2,mov_b_mr,(IMM d, RR1 s))
{
	if (isconst(s)) {
		COMPCALL(mov_b_mi)(d,(uae_u8)live.state[s].val);
		return;
	}

	CLOBBER_MOV;
	s=readreg(s,1);

	raw_mov_b_mr(d,s);
	unlock2(s);
}

MIDFUNC(2,mov_b_rm,(W1 d, IMM s))
{
	CLOBBER_MOV;
	d=writereg(d,1);

	raw_mov_b_rm(d,s);
	unlock2(d);
}

MIDFUNC(2,mov_l_ri,(W4 d, IMM s))
{
	set_const(d,s);
	return;
}

MIDFUNC(2,mov_w_ri,(W2 d, IMM s))
{
	CLOBBER_MOV;
	d=writereg(d,2);

	raw_mov_w_ri(d,s);
	unlock2(d);
}

MIDFUNC(2,mov_b_ri,(W1 d, IMM s))
{
	CLOBBER_MOV;
	d=writereg(d,1);

	raw_mov_b_ri(d,s);
	unlock2(d);
}

MIDFUNC(2,test_l_ri,(RR4 d, IMM i))
{
	CLOBBER_TEST;
	d=readreg(d,4);

	raw_test_l_ri(d,i);
	unlock2(d);
}

MIDFUNC(2,test_l_rr,(RR4 d, RR4 s))
{
	CLOBBER_TEST;
	d=readreg(d,4);
	s=readreg(s,4);

	raw_test_l_rr(d,s);;
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,test_w_rr,(RR2 d, RR2 s))
{
	CLOBBER_TEST;
	d=readreg(d,2);
	s=readreg(s,2);

	raw_test_w_rr(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,test_b_rr,(RR1 d, RR1 s))
{
	CLOBBER_TEST;
	d=readreg(d,1);
	s=readreg(s,1);

	raw_test_b_rr(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,and_l_ri,(RW4 d, IMM i))
{
	if (isconst(d) && !needflags) {
		live.state[d].val &= i;
		return;
	}

	CLOBBER_AND;
	d=rmw(d,4,4);

	raw_and_l_ri(d,i);
	unlock2(d);
}

MIDFUNC(2,and_l,(RW4 d, RR4 s))
{
	CLOBBER_AND;
	s=readreg(s,4);
	d=rmw(d,4,4);

	raw_and_l(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,and_w,(RW2 d, RR2 s))
{
	CLOBBER_AND;
	s=readreg(s,2);
	d=rmw(d,2,2);

	raw_and_w(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,and_b,(RW1 d, RR1 s))
{
	CLOBBER_AND;
	s=readreg(s,1);
	d=rmw(d,1,1);

	raw_and_b(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,or_l_ri,(RW4 d, IMM i))
{
	if (isconst(d) && !needflags) {
		live.state[d].val|=i;
		return;
	}
	CLOBBER_OR;
	d=rmw(d,4,4);

	raw_or_l_ri(d,i);
	unlock2(d);
}

MIDFUNC(2,or_l,(RW4 d, RR4 s))
{
	if (isconst(d) && isconst(s) && !needflags) {
		live.state[d].val|=live.state[s].val;
		return;
	}
	CLOBBER_OR;
	s=readreg(s,4);
	d=rmw(d,4,4);

	raw_or_l(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,or_w,(RW2 d, RR2 s))
{
	CLOBBER_OR;
	s=readreg(s,2);
	d=rmw(d,2,2);

	raw_or_w(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,or_b,(RW1 d, RR1 s))
{
	CLOBBER_OR;
	s=readreg(s,1);
	d=rmw(d,1,1);

	raw_or_b(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,adc_l,(RW4 d, RR4 s))
{
	CLOBBER_ADC;
	s=readreg(s,4);
	d=rmw(d,4,4);

	raw_adc_l(d,s);

	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,adc_w,(RW2 d, RR2 s))
{
	CLOBBER_ADC;
	s=readreg(s,2);
	d=rmw(d,2,2);

	raw_adc_w(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,adc_b,(RW1 d, RR1 s))
{
	CLOBBER_ADC;
	s=readreg(s,1);
	d=rmw(d,1,1);

	raw_adc_b(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,add_l,(RW4 d, RR4 s))
{
	if (isconst(s)) {
		COMPCALL(add_l_ri)(d,live.state[s].val);
		return;
	}

	CLOBBER_ADD;
	s=readreg(s,4);
	d=rmw(d,4,4);

	raw_add_l(d,s);

	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,add_w,(RW2 d, RR2 s))
{
	if (isconst(s)) {
		COMPCALL(add_w_ri)(d,(uae_u16)live.state[s].val);
		return;
	}

	CLOBBER_ADD;
	s=readreg(s,2);
	d=rmw(d,2,2);

	raw_add_w(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,add_b,(RW1 d, RR1 s))
{
	if (isconst(s)) {
		COMPCALL(add_b_ri)(d,(uae_u8)live.state[s].val);
		return;
	}

	CLOBBER_ADD;
	s=readreg(s,1);
	d=rmw(d,1,1);

	raw_add_b(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,sub_l_ri,(RW4 d, IMM i))
{
	if (!i && !needflags)
	return;
	if (isconst(d) && !needflags) {
		live.state[d].val-=i;
		return;
	}
#if USE_OFFSET
	if (!needflags) {
		add_offset(d,-i);
		return;
	}
#endif

	CLOBBER_SUB;
	d=rmw(d,4,4);

	raw_sub_l_ri(d,i);
	unlock2(d);
}

MIDFUNC(2,sub_w_ri,(RW2 d, IMM i))
{
	if (!i && !needflags)
	return;

	CLOBBER_SUB;
	d=rmw(d,2,2);

	raw_sub_w_ri(d,i);
	unlock2(d);
}

MIDFUNC(2,sub_b_ri,(RW1 d, IMM i))
{
	if (!i && !needflags)
	return;

	CLOBBER_SUB;
	d=rmw(d,1,1);

	raw_sub_b_ri(d,i);

	unlock2(d);
}

MIDFUNC(2,add_l_ri,(RW4 d, IMM i))
{
	if (!i && !needflags)
	return;
	if (isconst(d) && !needflags) {
		live.state[d].val+=i;
		return;
	}
#if USE_OFFSET
	if (!needflags) {
		add_offset(d,i);
		return;
	}
#endif
	CLOBBER_ADD;
	d=rmw(d,4,4);
	raw_add_l_ri(d,i);
	unlock2(d);
}

MIDFUNC(2,add_w_ri,(RW2 d, IMM i))
{
	if (!i && !needflags)
	return;

	CLOBBER_ADD;
	d=rmw(d,2,2);

	raw_add_w_ri(d,i);
	unlock2(d);
}

MIDFUNC(2,add_b_ri,(RW1 d, IMM i))
{
	if (!i && !needflags)
	return;

	CLOBBER_ADD;
	d=rmw(d,1,1);

	raw_add_b_ri(d,i);

	unlock2(d);
}

MIDFUNC(2,sbb_l,(RW4 d, RR4 s))
{
	CLOBBER_SBB;
	s=readreg(s,4);
	d=rmw(d,4,4);

	raw_sbb_l(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,sbb_w,(RW2 d, RR2 s))
{
	CLOBBER_SBB;
	s=readreg(s,2);
	d=rmw(d,2,2);

	raw_sbb_w(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,sbb_b,(RW1 d, RR1 s))
{
	CLOBBER_SBB;
	s=readreg(s,1);
	d=rmw(d,1,1);

	raw_sbb_b(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,sub_l,(RW4 d, RR4 s))
{
	if (isconst(s)) {
		COMPCALL(sub_l_ri)(d,live.state[s].val);
		return;
	}

	CLOBBER_SUB;
	s=readreg(s,4);
	d=rmw(d,4,4);

	raw_sub_l(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,sub_w,(RW2 d, RR2 s))
{
	if (isconst(s)) {
		COMPCALL(sub_w_ri)(d,(uae_u16)live.state[s].val);
		return;
	}

	CLOBBER_SUB;
	s=readreg(s,2);
	d=rmw(d,2,2);

	raw_sub_w(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,sub_b,(RW1 d, RR1 s))
{
	if (isconst(s)) {
		COMPCALL(sub_b_ri)(d,(uae_u8)live.state[s].val);
		return;
	}

	CLOBBER_SUB;
	s=readreg(s,1);
	d=rmw(d,1,1);

	raw_sub_b(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,cmp_l,(RR4 d, RR4 s))
{
	CLOBBER_CMP;
	s=readreg(s,4);
	d=readreg(d,4);

	raw_cmp_l(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,cmp_w,(RR2 d, RR2 s))
{
	CLOBBER_CMP;
	s=readreg(s,2);
	d=readreg(d,2);

	raw_cmp_w(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,cmp_b,(RR1 d, RR1 s))
{
	CLOBBER_CMP;
	s=readreg(s,1);
	d=readreg(d,1);

	raw_cmp_b(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,xor_l,(RW4 d, RR4 s))
{
	CLOBBER_XOR;
	s=readreg(s,4);
	d=rmw(d,4,4);

	raw_xor_l(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,xor_w,(RW2 d, RR2 s))
{
	CLOBBER_XOR;
	s=readreg(s,2);
	d=rmw(d,2,2);

	raw_xor_w(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,xor_b,(RW1 d, RR1 s))
{
	CLOBBER_XOR;
	s=readreg(s,1);
	d=rmw(d,1,1);

	raw_xor_b(d,s);
	unlock2(d);
	unlock2(s);
}

#if defined(UAE)
MIDFUNC(5,call_r_02,(RR4 r, RR4 in1, RR4 in2, IMM isize1, IMM isize2))
{
	clobber_flags();
	in1=readreg_specific(in1,isize1,REG_PAR1);
	in2=readreg_specific(in2,isize2,REG_PAR2);
	r=readreg(r,4);
	prepare_for_call_1();
	unlock2(r);
	unlock2(in1);
	unlock2(in2);
	prepare_for_call_2();
	compemu_raw_call_r(r);
}
#endif

#if defined(UAE)
MIDFUNC(5,call_r_11,(W4 out1, RR4 r, RR4 in1, IMM osize, IMM isize))
{
	clobber_flags();

	if (osize==4) {
		if (out1!=in1 && out1!=r) {
			COMPCALL(forget_about)(out1);
		}
	}
	else {
		tomem_c(out1);
	}

	in1=readreg_specific(in1,isize,REG_PAR1);
	r=readreg(r,4);

	prepare_for_call_1();
	unlock2(in1);
	unlock2(r);

	prepare_for_call_2();

	compemu_raw_call_r(r);

	live.nat[REG_RESULT].holds[0]=out1;
	live.nat[REG_RESULT].nholds=1;
	live.nat[REG_RESULT].touched=touchcnt++;

	live.state[out1].realreg=REG_RESULT;
	live.state[out1].realind=0;
	live.state[out1].val=0;
	live.state[out1].validsize=osize;
	live.state[out1].dirtysize=osize;
	set_status(out1,DIRTY);
}
#endif

MIDFUNC(0,nop,(void))
{
	raw_emit_nop();
}

/* forget_about() takes a mid-layer register */
MIDFUNC(1,forget_about,(W4 r))
{
	if (isinreg(r))
	disassociate(r);
	live.state[r].val=0;
	set_status(r,UNDEF);
}

MIDFUNC(1,f_forget_about,(FW r))
{
	if (f_isinreg(r))
	f_disassociate(r);
	live.fate[r].status=UNDEF;
}

// ARM optimized functions

MIDFUNC(2,arm_ADD_l,(RW4 d, RR4 s))
{
	if (isconst(s)) {
		COMPCALL(arm_ADD_l_ri)(d,live.state[s].val);
		return;
	}

	s=readreg(s,4);
	d=rmw(d,4,4);

	raw_ADD_l_rr(d,s);

	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,arm_ADD_l_ri,(RW4 d, IMM i))
{
	if (!i) return;
	if (isconst(d)) {
		live.state[d].val+=i;
		return;
	}
#if USE_OFFSET
	add_offset(d,i);
	return;
#endif
	d=rmw(d,4,4);

	raw_LDR_l_ri(REG_WORK1, i);
	raw_ADD_l_rr(d,REG_WORK1);
	unlock2(d);
}

MIDFUNC(2,arm_ADD_l_ri8,(RW4 d, IMM i))
{
	if (!i) return;
	if (isconst(d)) {
		live.state[d].val+=i;
		return;
	}
#if USE_OFFSET
	add_offset(d,i);
	return;
#endif
	d=rmw(d,4,4);

	raw_ADD_l_rri(d,d,i);
	unlock2(d);
}

MIDFUNC(2,arm_SUB_l_ri8,(RW4 d, IMM i))
{
	if (!i) return;
	if (isconst(d)) {
		live.state[d].val-=i;
		return;
	}
#if USE_OFFSET
	add_offset(d,-i);
	return;
#endif
	d=rmw(d,4,4);

	raw_SUB_l_rri(d,d,i);
	unlock2(d);
}

MIDFUNC(2,arm_AND_l,(RW4 d, RR4 s))
{
	s=readreg(s,4);
	d=rmw(d,4,4);

	raw_AND_l_rr(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,arm_AND_w,(RW2 d, RR2 s))
{
	s=readreg(s,2);
	d=rmw(d,2,2);

	raw_AND_w_rr(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,arm_AND_b,(RW1 d, RR1 s))
{
	s=readreg(s,1);
	d=rmw(d,1,1);

	raw_AND_b_rr(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,arm_AND_l_ri8,(RW4 d, IMM i))
{
	if (isconst(d)) {
		live.state[d].val &= i;
		return;
	}

	d=rmw(d,4,4);

	raw_AND_l_ri(d,i);
	unlock2(d);
}

MIDFUNC(2,arm_EOR_b,(RW1 d, RR1 s))
{
	s=readreg(s,1);
	d=rmw(d,1,1);

	raw_EOR_b_rr(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,arm_EOR_l,(RW4 d, RR4 s))
{
	s=readreg(s,4);
	d=rmw(d,4,4);

	raw_EOR_l_rr(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,arm_EOR_w,(RW2 d, RR2 s))
{
	s=readreg(s,2);
	d=rmw(d,2,2);

	raw_EOR_w_rr(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,arm_ORR_b,(RW1 d, RR1 s))
{
	s=readreg(s,1);
	d=rmw(d,1,1);

	raw_ORR_b_rr(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,arm_ORR_l,(RW4 d, RR4 s))
{
	if (isconst(d) && isconst(s)) {
		live.state[d].val|=live.state[s].val;
		return;
	}
	s=readreg(s,4);
	d=rmw(d,4,4);

	raw_ORR_l_rr(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,arm_ORR_w,(RW2 d, RR2 s))
{
	s=readreg(s,2);
	d=rmw(d,2,2);

	raw_ORR_w_rr(d,s);
	unlock2(d);
	unlock2(s);
}

MIDFUNC(2,arm_ROR_l_ri8,(RW4 r, IMM i))
{
	if (!i)
	return;

	r=rmw(r,4,4);
	raw_ROR_l_ri(r,i);
	unlock2(r);
}

// Other
static inline void flush_cpu_icache(void *start, void *stop)
{

	register void *_beg __asm ("a1") = start;
	register void *_end __asm ("a2") = stop;
	register void *_flg __asm ("a3") = 0;
#ifdef __ARM_EABI__
	register unsigned long _scno __asm ("r7") = 0xf0002;
	__asm __volatile ("swi 0x0		@ sys_cacheflush"
			: "=r" (_beg)
			: "0" (_beg), "r" (_end), "r" (_flg), "r" (_scno));
#else
	__asm __volatile ("swi 0x9f0002		@ sys_cacheflush"
			: "=r" (_beg)
			: "0" (_beg), "r" (_end), "r" (_flg));
#endif
}

static inline void write_jmp_target(uae_u32* jmpaddr, cpuop_func* a) {
	*(jmpaddr) = (uae_u32) a;
	flush_cpu_icache((void *) jmpaddr, (void *) &jmpaddr[1]);
}

static inline void emit_jmp_target(uae_u32 a) {
	emit_long((uae_u32) a);
}
