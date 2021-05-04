/*
 * compiler/codegen_arm.h - IA-32 and AMD64 code generator
 *
 * Copyright (c) 2013 Jens Heitmann of ARAnyM dev team (see AUTHORS)
 * 
 * Inspired by Christian Bauer's Basilisk II
 *
 * This file is part of the ARAnyM project which builds a new and powerful
 * TOS/FreeMiNT compatible virtual machine running on almost any hardware.
 *
 * JIT compiler m68k -> ARM
 *
 * Original 68040 JIT compiler for UAE, copyright 2000-2002 Bernd Meyer
 * This file is derived from CCG, copyright 1999-2003 Ian Piumarta
 * Adaptation for Basilisk II and improvements, copyright 2000-2004 Gwenole Beauchesne
 * Portions related to CPU detection come from linux/arch/i386/kernel/setup.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef ARM_RTASM_H
#define ARM_RTASM_H

/* NOTES
 *
 */

/* --- Configuration ------------------------------------------------------- */

/* CPSR flags */

#define ARM_N_FLAG        0x80000000
#define ARM_Z_FLAG        0x40000000
#define ARM_C_FLAG        0x20000000
#define ARM_V_FLAG        0x10000000
#define ARM_Q_FLAG        0x08000000
#define ARM_CV_FLAGS        (ARM_C_FLAG|ARM_V_FLAG)

#define ARM_GE3                0x00080000
#define ARM_GE2                0x00040000
#define ARM_GE1                0x00020000
#define ARM_GE0                0x00010000

/* --- Macros -------------------------------------------------------------- */

/* ========================================================================= */
/* --- UTILITY ------------------------------------------------------------- */
/* ========================================================================= */

#define _W(c) emit_long(c)
#define _LS2_ADDR(a) (((a) & 0x01f0000f) | (((a) & 0xf0) << 4))

/* ========================================================================= */
/* --- ENCODINGS ----------------------------------------------------------- */
/* ========================================================================= */

#define IMM32(c) (((c) & 0xffffff00) == 0 ? (c) : \
                  ((c) & 0x3fffffc0) == 0 ? (0x100 | (((c) >> 30) & 0x3) | ((((c) & 0x0000003f) << 2))) : \
                  ((c) & 0x0ffffff0) == 0 ? (0x200 | (((c) >> 28) & 0xf) | ((((c) & 0x0000000f) << 4))) : \
                  ((c) & 0x03fffffc) == 0 ? (0x300 | (((c) >> 26) & 0x3f) | ((((c) & 0x00000003) << 6)) ) : \
                  ((c) & 0x00ffffff) == 0 ? (0x400 | (((c) >> 24) & 0xff)) : \
                  ((c) & 0xc03fffff) == 0 ? (0x500 | ((c) >> 22)) : \
                  ((c) & 0xf00fffff) == 0 ? (0x600 | ((c) >> 20)) : \
                  ((c) & 0xfc03ffff) == 0 ? (0x700 | ((c) >> 18)) : \
                  ((c) & 0xff00ffff) == 0 ? (0x800 | ((c) >> 16)) : \
                  ((c) & 0xffc03fff) == 0 ? (0x900 | ((c) >> 14)) : \
                  ((c) & 0xfff00fff) == 0 ? (0xa00 | ((c) >> 12)) : \
                  ((c) & 0xfffc03ff) == 0 ? (0xb00 | ((c) >> 10)) : \
                  ((c) & 0xffff00ff) == 0 ? (0xc00 | ((c) >> 8)) : \
                  ((c) & 0xffffc03f) == 0 ? (0xd00 | ((c) >> 6)) : \
                  ((c) & 0xfffff00f) == 0 ? (0xe00 | ((c) >> 4)) : \
                  ((c) & 0xfffffc03) == 0 ? (0xf00 | ((c) >> 2)) : \
                        0\
                 )

#define SHIFT_IMM(c) (0x02000000 | (IMM32((c))))

#define UNSHIFTED_IMM8(c) (0x02000000 | (c))
#define SHIFT_IMM8_ROR(c,r) (0x02000000 | (c) | ((r >> 1) << 8))

#define SHIFT_REG(Rm) (Rm)
#define SHIFT_LSL_i(Rm,s) ((Rm) | ((s) << 7))
#define SHIFT_LSL_r(Rm,Rs) ((Rm) | ((Rs) << 8) | 0x10)
#define SHIFT_LSR_i(Rm,s) ((Rm) | ((s) << 7) | 0x20)
#define SHIFT_LSR_r(Rm,Rs) ((Rm) | ((Rs) << 8) | 0x30)
#define SHIFT_ASR_i(Rm,s) ((Rm) | ((s) << 7) | 0x40)
#define SHIFT_ASR_r(Rm,Rs) ((Rm) | ((Rs) << 8) | 0x50)
#define SHIFT_ROR_i(Rm,s) ((Rm) | ((s) << 7) | 0x60)
#define SHIFT_ROR_r(Rm,Rs) ((Rm) | ((Rs) << 8) | 0x70)
#define SHIFT_RRX(Rm) ((Rm) | 0x60)
#define SHIFT_PK(Rm,s) ((Rm) | ((s) << 7))

/* Load/Store addressings */
#define ADR_ADD(v) ((1 << 23) | (v))
#define ADR_SUB(v) (v)

#define ADR_IMM(v) ((v) | (1 << 24))
#define ADR_IMMPOST(v) (v)
#define ADR_REG(Rm) ((1 << 25) | (1 << 24) | (Rm))
#define ADR_REGPOST(Rm) ((1 << 25) | (Rm))

#define ADD_IMM(i) ADR_ADD(ADR_IMM(i))
#define SUB_IMM(i) ADR_SUB(ADR_IMM(i))

#define ADD_REG(Rm)	ADR_ADD(ADR_REG(Rm))
#define SUB_REG(Rm)	ADR_SUB(ADR_REG(Rm))

#define ADD_LSL(Rm,i) ADR_ADD(ADR_REG(Rm) | ((i) << 7))
#define SUB_LSL(Rm,i) ADR_SUB(ADR_REG(Rm) | ((i) << 7))

#define ADD_LSR(Rm,i) ADR_ADD(ADR_REG(Rm) | (((i) & 0x1f) << 7) | (1 << 5))
#define SUB_LSR(Rm,i) ADR_SUB(ADR_REG(Rm) | (((i) & 0x1f) << 7) | (1 << 5))

#define ADD_ASR(Rm,i) ADR_ADD(ADR_REG(Rm) | (((i) & 0x1f) << 7) | (2 << 5))
#define SUB_ASR(Rm,i) ADR_SUB(ADR_REG(Rm) | (((i) & 0x1f) << 7) | (2 << 5))

#define ADD_ROR(Rm,i) ADR_ADD(ADR_REG(Rm) | (((i) & 0x1f) << 7) | (3 << 5))
#define SUB_ROR(Rm,i) ADR_SUB(ADR_REG(Rm) | (((i) & 0x1f) << 7) | (3 << 5))

#define ADD_RRX(Rm) ADR_ADD(ADR_REG(Rm) | (3 << 5))
#define SUB_RRX(Rm) ADR_SUB(ADR_REG(Rm) | (3 << 5))

#define ADD2_IMM(i) ADR_ADD(i | (1 << 22))
#define SUB2_IMM(i) ADR_SUB(i | (1 << 22))

#define ADD2_REG(Rm)	ADR_ADD(Rm)
#define SUB2_REG(Rm)	ADR_SUB(Rm)

/* MOV, MVN */
#define _OP1(cc,op,s,Rd,shift) _W(((cc) << 28) | ((op) << 21) | ((s) << 20) | ((Rd) << 12) | (shift))

/* CMP, CMN, TST, TEQ */
#define _OP2(cc,op,Rn,shift) _W(((cc) << 28) | ((op) << 21) | (1 << 20) | ((Rn) << 16) | (shift))

/* ADD, SUB, RSB, ADC, SBC, RSC, AND, BIC, EOR, ORR */
#define _OP3(cc,op,s,Rd,Rn,shift) _W(((cc) << 28) | ((op) << 21) | ((s) << 20) | ((Rn) << 16) | ((Rd) << 12) | (shift))

/* LDR, STR */
#define _LS1(cc,l,b,Rd,Rn,a) _W(((cc) << 28) | (0x01 << 26) | ((l) << 20) | ((b) << 22) | ((Rn) << 16) | ((Rd) << 12) | (a))
#define _LS2(cc,p,l,s,h,Rd,Rn,a) _W(((cc) << 28) | ((p) << 24) | ((l) << 20) | ((Rn) << 16) | ((Rd) << 12) | ((s) << 6) | ((h) << 5) | 0x90 | _LS2_ADDR((a)))

/* ========================================================================= */
/* --- OPCODES ------------------------------------------------------------- */
/* ========================================================================= */

/* Branch instructions */
#ifndef __ANDROID__
enum {
	_B, _BL, _BLX, _BX, _BXJ
};
#endif

/* Data processing instructions */
enum {
	_AND = 0,
	_EOR,
	_SUB,
	_RSB,
	_ADD,
	_ADC,
	_SBC,
	_RSC,
	_TST,
	_TEQ,
	_CMP,
	_CMN,
	_ORR,
	_MOV,
	_BIC,
	_MVN
};

/* Single instruction Multiple Data (SIMD) instructions */

/* Multiply instructions */

/* Parallel instructions */

/* Extend instructions */

/* Miscellaneous arithmetic instrations */

/* Status register transfer instructions */

/* Load and Store instructions */

/* Coprocessor instructions */

/* Exception generation instructions */

/* ========================================================================= */
/* --- ASSEMBLER ----------------------------------------------------------- */
/* ========================================================================= */

#define	NOP()								_W(0xe1a00000)
#define SETEND_BE()							_W(0xf1010200)
#define SETEND_LE()							_W(0xf1010000)

/* Data processing instructions */

/* Opcodes Type 1 */
/* MOVcc rd,#i */
#define CC_MOV_ri8(cc,Rd,i)                 _OP1(cc,_MOV,0,Rd,UNSHIFTED_IMM8(i))
/* MOVcc Rd,#i ROR #s */
#define CC_MOV_ri8RORi(cc,Rd,i,s)           _OP1(cc,_MOV,0,Rd,SHIFT_IMM8_ROR(i,s))
#define CC_MOV_ri(cc,Rd,i)                  _OP1(cc,_MOV,0,Rd,SHIFT_IMM(i))
#define CC_MOV_rr(cc,Rd,Rm)                 _OP1(cc,_MOV,0,Rd,SHIFT_REG(Rm))
#define CC_MOV_rrLSLi(cc,Rd,Rm,i)           _OP1(cc,_MOV,0,Rd,SHIFT_LSL_i(Rm,i))
#define CC_MOV_rrLSLr(cc,Rd,Rm,Rs)          _OP1(cc,_MOV,0,Rd,SHIFT_LSL_r(Rm,Rs))
#define CC_MOV_rrLSRi(cc,Rd,Rm,i)           _OP1(cc,_MOV,0,Rd,SHIFT_LSR_i(Rm,i))
#define CC_MOV_rrLSRr(cc,Rd,Rm,Rs)          _OP1(cc,_MOV,0,Rd,SHIFT_LSR_r(Rm,Rs))
#define CC_MOV_rrASRi(cc,Rd,Rm,i)           _OP1(cc,_MOV,0,Rd,SHIFT_ASR_i(Rm,i))
#define CC_MOV_rrASRr(cc,Rd,Rm,Rs)          _OP1(cc,_MOV,0,Rd,SHIFT_ASR_r(Rm,Rs))
#define CC_MOV_rrRORi(cc,Rd,Rm,i)           _OP1(cc,_MOV,0,Rd,SHIFT_ROR_i(Rm,i))
#define CC_MOV_rrRORr(cc,Rd,Rm,Rs)          _OP1(cc,_MOV,0,Rd,SHIFT_ROR_r(Rm,Rs))
#define CC_MOV_rrRRX(cc,Rd,Rm)              _OP1(cc,_MOV,0,Rd,SHIFT_RRX(Rm))

/* MOV rd,#i */
#define MOV_ri8(Rd,i)                       CC_MOV_ri8(NATIVE_CC_AL,Rd,i)
/* MOV Rd,#i ROR #s */
#define MOV_ri8RORi(Rd,i,s)                 CC_MOV_ri8RORi(NATIVE_CC_AL,Rd,i,s)
#define MOV_ri(Rd,i)                        CC_MOV_ri(NATIVE_CC_AL,Rd,i)
#define MOV_rr(Rd,Rm)                       CC_MOV_rr(NATIVE_CC_AL,Rd,Rm)
#define MOV_rrLSLi(Rd,Rm,i)                 CC_MOV_rrLSLi(NATIVE_CC_AL,Rd,Rm,i)
#define MOV_rrLSLr(Rd,Rm,Rs)                CC_MOV_rrLSLr(NATIVE_CC_AL,Rd,Rm,Rs)
#define MOV_rrLSRi(Rd,Rm,i)                 CC_MOV_rrLSRi(NATIVE_CC_AL,Rd,Rm,i)
#define MOV_rrLSRr(Rd,Rm,Rs)                CC_MOV_rrLSRr(NATIVE_CC_AL,Rd,Rm,Rs)
#define MOV_rrASRi(Rd,Rm,i)                 CC_MOV_rrASRi(NATIVE_CC_AL,Rd,Rm,i)
#define MOV_rrASRr(Rd,Rm,Rs)                CC_MOV_rrASRr(NATIVE_CC_AL,Rd,Rm,Rs)
#define MOV_rrRORi(Rd,Rm,i)                 CC_MOV_rrRORi(NATIVE_CC_AL,Rd,Rm,i)
#define MOV_rrRORr(Rd,Rm,Rs)                CC_MOV_rrRORr(NATIVE_CC_AL,Rd,Rm,Rs)
#define MOV_rrRRX(Rd,Rm)                    CC_MOV_rrRRX(NATIVE_CC_AL,Rd,Rm)

#define CC_MOVS_ri(cc,Rd,i)                 _OP1(cc,_MOV,1,Rd,SHIFT_IMM(i))
#define CC_MOVS_rr(cc,Rd,Rm)                _OP1(cc,_MOV,1,Rd,SHIFT_REG(Rm))
#define CC_MOVS_rrLSLi(cc,Rd,Rm,i)          _OP1(cc,_MOV,1,Rd,SHIFT_LSL_i(Rm,i))
#define CC_MOVS_rrLSLr(cc,Rd,Rm,Rs)         _OP1(cc,_MOV,1,Rd,SHIFT_LSL_r(Rm,Rs))
#define CC_MOVS_rrLSRi(cc,Rd,Rm,i)          _OP1(cc,_MOV,1,Rd,SHIFT_LSR_i(Rm,i))
#define CC_MOVS_rrLSRr(cc,Rd,Rm,Rs)         _OP1(cc,_MOV,1,Rd,SHIFT_LSR_r(Rm,Rs))
#define CC_MOVS_rrASRi(cc,Rd,Rm,i)          _OP1(cc,_MOV,1,Rd,SHIFT_ASR_i(Rm,i))
#define CC_MOVS_rrASRr(cc,Rd,Rm,Rs)         _OP1(cc,_MOV,1,Rd,SHIFT_ASR_r(Rm,Rs))
#define CC_MOVS_rrRORi(cc,Rd,Rm,i)          _OP1(cc,_MOV,1,Rd,SHIFT_ROR_i(Rm,i))
#define CC_MOVS_rrRORr(cc,Rd,Rm,Rs)         _OP1(cc,_MOV,1,Rd,SHIFT_ROR_r(Rm,Rs))
#define CC_MOVS_rrRRX(cc,Rd,Rm)             _OP1(cc,_MOV,1,Rd,SHIFT_RRX(Rm))

#define MOVS_ri(Rd,i)                       CC_MOVS_ri(NATIVE_CC_AL,Rd,i)
#define MOVS_rr(Rd,Rm)                      CC_MOVS_rr(NATIVE_CC_AL,Rd,Rm)
#define MOVS_rrLSLi(Rd,Rm,i)                CC_MOVS_rrLSLi(NATIVE_CC_AL,Rd,Rm,i)
#define MOVS_rrLSLr(Rd,Rm,Rs)               CC_MOVS_rrLSLr(NATIVE_CC_AL,Rd,Rm,Rs)
#define MOVS_rrLSRi(Rd,Rm,i)                CC_MOVS_rrLSRi(NATIVE_CC_AL,Rd,Rm,i)
#define MOVS_rrLSRr(Rd,Rm,Rs)               CC_MOVS_rrLSRr(NATIVE_CC_AL,Rd,Rm,Rs)
#define MOVS_rrASRi(Rd,Rm,i)                CC_MOVS_rrASRi(NATIVE_CC_AL,Rd,Rm,i)
#define MOVS_rrASRr(Rd,Rm,Rs)               CC_MOVS_rrASRr(NATIVE_CC_AL,Rd,Rm,Rs)
#define MOVS_rrRORi(Rd,Rm,i)                CC_MOVS_rrRORi(NATIVE_CC_AL,Rd,Rm,i)
#define MOVS_rrRORr(Rd,Rm,Rs)               CC_MOVS_rrRORr(NATIVE_CC_AL,Rd,Rm,Rs)
#define MOVS_rrRRX(Rd,Rm)                   CC_MOVS_rrRRX(NATIVE_CC_AL,Rd,Rm)

/* MVNcc rd,#i */
#define CC_MVN_ri8(cc,Rd,i)                 _OP1(cc,_MVN,0,Rd,UNSHIFTED_IMM8(i))
/* MVNcc Rd,#i ROR #s */
#define CC_MVN_ri8RORi(cc,Rd,i,s)           _OP1(cc,_MVN,0,Rd,SHIFT_IMM8_ROR(i,s))
#define CC_MVN_ri(cc,Rd,i)              	_OP1(cc,_MVN,0,Rd,SHIFT_IMM(i))
#define CC_MVN_rr(cc,Rd,Rm)             	_OP1(cc,_MVN,0,Rd,SHIFT_REG(Rm))
#define CC_MVN_rrLSLi(cc,Rd,Rm,i)       	_OP1(cc,_MVN,0,Rd,SHIFT_LSL_i(Rm,i))
#define CC_MVN_rrLSLr(cc,Rd,Rm,Rs)      	_OP1(cc,_MVN,0,Rd,SHIFT_LSL_r(Rm,Rs))
#define CC_MVN_rrLSRi(cc,Rd,Rm,i)       	_OP1(cc,_MVN,0,Rd,SHIFT_LSR_i(Rm,i))
#define CC_MVN_rrLSRr(cc,Rd,Rm,Rs)      	_OP1(cc,_MVN,0,Rd,SHIFT_LSR_r(Rm,Rs))
#define CC_MVN_rrASRi(cc,Rd,Rm,i)       	_OP1(cc,_MVN,0,Rd,SHIFT_ASR_i(Rm,i))
#define CC_MVN_rrASRr(cc,Rd,Rm,Rs)      	_OP1(cc,_MVN,0,Rd,SHIFT_ASR_r(Rm,Rs))
#define CC_MVN_rrRORi(cc,Rd,Rm,i)       	_OP1(cc,_MVN,0,Rd,SHIFT_ROR_i(Rm,i))
#define CC_MVN_rrRORr(cc,Rd,Rm,Rs)      	_OP1(cc,_MVN,0,Rd,SHIFT_ROR_r(Rm,Rs))
#define CC_MVN_rrRRX(cc,Rd,Rm)           	_OP1(cc,_MVN,0,Rd,SHIFT_RRX(Rm))

/* MVN rd,#i */
#define MVN_ri8(Rd,i)                       CC_MVN_ri8(NATIVE_CC_AL,Rd,i)
/* MVN Rd,#i ROR #s */
#define MVN_ri8RORi(Rd,i,s)                 CC_MVN_ri8RORi(NATIVE_CC_AL,Rd,i,s)
#define MVN_ri(Rd,i)                        CC_MVN_ri(NATIVE_CC_AL,Rd,i)
#define MVN_rr(Rd,Rm)                       CC_MVN_rr(NATIVE_CC_AL,Rd,Rm)
#define MVN_rrLSLi(Rd,Rm,i)                 CC_MVN_rrLSLi(NATIVE_CC_AL,Rd,Rm,i)
#define MVN_rrLSLr(Rd,Rm,Rs)                CC_MVN_rrLSLr(NATIVE_CC_AL,Rd,Rm,Rs)
#define MVN_rrLSRi(Rd,Rm,i)                 CC_MVN_rrLSRi(NATIVE_CC_AL,Rd,Rm,i)
#define MVN_rrLSRr(Rd,Rm,Rs)                CC_MVN_rrLSRr(NATIVE_CC_AL,Rd,Rm,Rs)
#define MVN_rrASRi(Rd,Rm,i)                 CC_MVN_rrASRi(NATIVE_CC_AL,Rd,Rm,i)
#define MVN_rrASRr(Rd,Rm,Rs)                CC_MVN_rrASRr(NATIVE_CC_AL,Rd,Rm,Rs)
#define MVN_rrRORi(Rd,Rm,i)                 CC_MVN_rrRORi(NATIVE_CC_AL,Rd,Rm,i)
#define MVN_rrRORr(Rd,Rm,Rs)                CC_MVN_rrRORr(NATIVE_CC_AL,Rd,Rm,Rs)
#define MVN_rrRRX(Rd,Rm)                    CC_MVN_rrRRX(NATIVE_CC_AL,Rd,Rm)

#define CC_MVNS_ri(cc,Rd,i)              	_OP1(cc,_MVN,1,Rd,SHIFT_IMM(i))
#define CC_MVNS_rr(cc,Rd,Rm)             	_OP1(cc,_MVN,1,Rd,SHIFT_REG(Rm))
#define CC_MVNS_rrLSLi(cc,Rd,Rm,i)       	_OP1(cc,_MVN,1,Rd,SHIFT_LSL_i(Rm,i))
#define CC_MVNS_rrLSLr(cc,Rd,Rm,Rs)      	_OP1(cc,_MVN,1,Rd,SHIFT_LSL_r(Rm,Rs))
#define CC_MVNS_rrLSRi(cc,Rd,Rm,i)       	_OP1(cc,_MVN,1,Rd,SHIFT_LSR_i(Rm,i))
#define CC_MVNS_rrLSRr(cc,Rd,Rm,Rs)      	_OP1(cc,_MVN,1,Rd,SHIFT_LSR_r(Rm,Rs))
#define CC_MVNS_rrASRi(cc,Rd,Rm,i)       	_OP1(cc,_MVN,1,Rd,SHIFT_ASR_i(Rm,i))
#define CC_MVNS_rrASRr(cc,Rd,Rm,Rs)      	_OP1(cc,_MVN,1,Rd,SHIFT_ASR_r(Rm,Rs))
#define CC_MVNS_rrRORi(cc,Rd,Rm,i)       	_OP1(cc,_MVN,1,Rd,SHIFT_ROR_i(Rm,i))
#define CC_MVNS_rrRORr(cc,Rd,Rm,Rs)      	_OP1(cc,_MVN,1,Rd,SHIFT_ROR_r(Rm,Rs))
#define CC_MVNS_rrRRX(cc,Rd,Rm)           	_OP1(cc,_MVN,1,Rd,SHIFT_RRX(Rm))

#define MVNS_ri(Rd,i)                       CC_MVNS_ri(NATIVE_CC_AL,Rd,i)
#define MVNS_rr(Rd,Rm)                      CC_MVNS_rr(NATIVE_CC_AL,Rd,Rm)
#define MVNS_rrLSLi(Rd,Rm,i)                CC_MVNS_rrLSLi(NATIVE_CC_AL,Rd,Rm,i)
#define MVNS_rrLSLr(Rd,Rm,Rs)               CC_MVNS_rrLSLr(NATIVE_CC_AL,Rd,Rm,Rs)
#define MVNS_rrLSRi(Rd,Rm,i)                CC_MVNS_rrLSRi(NATIVE_CC_AL,Rd,Rm,i)
#define MVNS_rrLSRr(Rd,Rm,Rs)               CC_MVNS_rrLSRr(NATIVE_CC_AL,Rd,Rm,Rs)
#define MVNS_rrASRi(Rd,Rm,i)                CC_MVNS_rrASRi(NATIVE_CC_AL,Rd,Rm,i)
#define MVNS_rrASRr(Rd,Rm,Rs)               CC_MVNS_rrASRr(NATIVE_CC_AL,Rd,Rm,Rs)
#define MVNS_rrRORi(Rd,Rm,i)                CC_MVNS_rrRORi(NATIVE_CC_AL,Rd,Rm,i)
#define MVNS_rrRORr(Rd,Rm,Rs)               CC_MVNS_rrRORr(NATIVE_CC_AL,Rd,Rm,Rs)
#define MVNS_rrRRX(Rd,Rm)                   CC_MVNS_rrRRX(NATIVE_CC_AL,Rd,Rm)

/* Opcodes Type 2 */
#define CC_CMP_ri(cc,Rn,i)                	_OP2(cc,_CMP,Rn,SHIFT_IMM(i))
#define CC_CMP_rr(cc,Rn,Rm)                	_OP2(cc,_CMP,Rn,SHIFT_REG(Rm))
#define CC_CMP_rrLSLi(cc,Rn,Rm,i)        	_OP2(cc,_CMP,Rn,SHIFT_LSL_i(Rm,i))
#define CC_CMP_rrLSLr(cc,Rn,Rm,Rs)        	_OP2(cc,_CMP,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_CMP_rrLSRi(cc,Rn,Rm,i)        	_OP2(cc,_CMP,Rn,SHIFT_LSR_i(Rm,i))
#define CC_CMP_rrLSRr(cc,Rn,Rm,Rs)        	_OP2(cc,_CMP,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_CMP_rrASRi(cc,Rn,Rm,i)        	_OP2(cc,_CMP,Rn,SHIFT_ASR_i(Rm,i))
#define CC_CMP_rrASRr(cc,Rn,Rm,Rs)        	_OP2(cc,_CMP,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_CMP_rrRORi(cc,Rn,Rm,i)        	_OP2(cc,_CMP,Rn,SHIFT_ROR_i(Rm,i))
#define CC_CMP_rrRORr(cc,Rn,Rm,Rs)        	_OP2(cc,_CMP,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_CMP_rrRRX(cc,Rn,Rm)              _OP2(cc,_CMP,Rn,SHIFT_RRX(Rm))

#define CMP_ri(Rn,i)                        CC_CMP_ri(NATIVE_CC_AL,Rn,i)
#define CMP_rr(Rn,Rm)                        CC_CMP_rr(NATIVE_CC_AL,Rn,Rm)
#define CMP_rrLSLi(Rn,Rm,i)                	CC_CMP_rrLSLi(NATIVE_CC_AL,Rn,Rm,i)
#define CMP_rrLSLr(Rn,Rm,Rs)                CC_CMP_rrLSLr(NATIVE_CC_AL,Rn,Rm,Rs)
#define CMP_rrLSRi(Rn,Rm,i)                	CC_CMP_rrLSRi(NATIVE_CC_AL,Rn,Rm,i)
#define CMP_rrLSRr(Rn,Rm,Rs)                CC_CMP_rrLSRr(NATIVE_CC_AL,Rn,Rm,Rs)
#define CMP_rrASRi(Rn,Rm,i)                	CC_CMP_rrASRi(NATIVE_CC_AL,Rn,Rm,i)
#define CMP_rrASRr(Rn,Rm,Rs)                CC_CMP_rrASRr(NATIVE_CC_AL,Rn,Rm,Rs)
#define CMP_rrRORi(Rn,Rm,i)                	CC_CMP_rrRORi(NATIVE_CC_AL,Rn,Rm,i)
#define CMP_rrRORr(Rn,Rm,Rs)                CC_CMP_rrRORr(NATIVE_CC_AL,Rn,Rm,Rs)
#define CMP_rrRRX(Rn,Rm)                	CC_CMP_rrRRX(NATIVE_CC_AL,Rn,Rm)

#define CC_CMN_ri(cc,Rn,i)                	_OP2(cc,_CMN,Rn,SHIFT_IMM(i))
#define CC_CMN_rr(cc,Rn,r)                	_OP2(cc,_CMN,Rn,SHIFT_REG(r))
#define CC_CMN_rrLSLi(cc,Rn,Rm,i)        	_OP2(cc,_CMN,Rn,SHIFT_LSL_i(Rm,i))
#define CC_CMN_rrLSLr(cc,Rn,Rm,Rs)        	_OP2(cc,_CMN,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_CMN_rrLSRi(cc,Rn,Rm,i)        	_OP2(cc,_CMN,Rn,SHIFT_LSR_i(Rm,i))
#define CC_CMN_rrLSRr(cc,Rn,Rm,Rs)        	_OP2(cc,_CMN,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_CMN_rrASRi(cc,Rn,Rm,i)        	_OP2(cc,_CMN,Rn,SHIFT_ASR_i(Rm,i))
#define CC_CMN_rrASRr(cc,Rn,Rm,Rs)        	_OP2(cc,_CMN,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_CMN_rrRORi(cc,Rn,Rm,i)        	_OP2(cc,_CMN,Rn,SHIFT_ROR_i(Rm,i))
#define CC_CMN_rrRORr(cc,Rn,Rm,Rs)        	_OP2(cc,_CMN,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_CMN_rrRRX(cc,Rn,Rm)              _OP2(cc,_CMN,Rn,SHIFT_RRX(Rm))

#define CMN_ri(Rn,i)                    	CC_CMN_ri(NATIVE_CC_AL,Rn,i)
#define CMN_rr(Rn,r)                    	CC_CMN_rr(NATIVE_CC_AL,Rn,r)
#define CMN_rrLSLi(Rn,Rm,i)             	CC_CMN_rrLSLi(NATIVE_CC_AL,Rn,Rm,i)
#define CMN_rrLSLr(Rn,Rm,Rs)            	CC_CMN_rrLSLr(NATIVE_CC_AL,Rn,Rm,Rs)
#define CMN_rrLSRi(Rn,Rm,i)             	CC_CMN_rrLSRi(NATIVE_CC_AL,Rn,Rm,i)
#define CMN_rrLSRr(Rn,Rm,Rs)            	CC_CMN_rrLSRr(NATIVE_CC_AL,Rn,Rm,Rs)
#define CMN_rrASRi(Rn,Rm,i)             	CC_CMN_rrASRi(NATIVE_CC_AL,Rn,Rm,i)
#define CMN_rrASRr(Rn,Rm,Rs)            	CC_CMN_rrASRr(NATIVE_CC_AL,Rn,Rm,Rs)
#define CMN_rrRORi(Rn,Rm,i)             	CC_CMN_rrRORi(NATIVE_CC_AL,Rn,Rm,i)
#define CMN_rrRORr(Rn,Rm,Rs)            	CC_CMN_rrRORr(NATIVE_CC_AL,Rn,Rm,Rs)
#define CMN_rrRRX(Rn,Rm)                	CC_CMN_rrRRX(NATIVE_CC_AL,Rn,Rm)

#define CC_TST_ri(cc,Rn,i)              	_OP2(cc,_TST,Rn,SHIFT_IMM(i))
#define CC_TST_rr(cc,Rn,r)              	_OP2(cc,_TST,Rn,SHIFT_REG(r))
#define CC_TST_rrLSLi(cc,Rn,Rm,i)       	_OP2(cc,_TST,Rn,SHIFT_LSL_i(Rm,i))
#define CC_TST_rrLSLr(cc,Rn,Rm,Rs)      	_OP2(cc,_TST,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_TST_rrLSRi(cc,Rn,Rm,i)       	_OP2(cc,_TST,Rn,SHIFT_LSR_i(Rm,i))
#define CC_TST_rrLSRr(cc,Rn,Rm,Rs)      	_OP2(cc,_TST,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_TST_rrASRi(cc,Rn,Rm,i)       	_OP2(cc,_TST,Rn,SHIFT_ASR_i(Rm,i))
#define CC_TST_rrASRr(cc,Rn,Rm,Rs)      	_OP2(cc,_TST,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_TST_rrRORi(cc,Rn,Rm,i)       	_OP2(cc,_TST,Rn,SHIFT_ROR_i(Rm,i))
#define CC_TST_rrRORr(cc,Rn,Rm,Rs)      	_OP2(cc,_TST,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_TST_rrRRX(cc,Rn,Rm)          	_OP2(cc,_TST,Rn,SHIFT_RRX(Rm))

#define TST_ri(Rn,i)                    	CC_TST_ri(NATIVE_CC_AL,Rn,i)
#define TST_rr(Rn,r)                    	CC_TST_rr(NATIVE_CC_AL,Rn,r)
#define TST_rrLSLi(Rn,Rm,i)             	CC_TST_rrLSLi(NATIVE_CC_AL,Rn,Rm,i)
#define TST_rrLSLr(Rn,Rm,Rs)            	CC_TST_rrLSLr(NATIVE_CC_AL,Rn,Rm,Rs)
#define TST_rrLSRi(Rn,Rm,i)             	CC_TST_rrLSRi(NATIVE_CC_AL,Rn,Rm,i)
#define TST_rrLSRr(Rn,Rm,Rs)            	CC_TST_rrLSRr(NATIVE_CC_AL,Rn,Rm,Rs)
#define TST_rrASRi(Rn,Rm,i)             	CC_TST_rrASRi(NATIVE_CC_AL,Rn,Rm,i)
#define TST_rrASRr(Rn,Rm,Rs)            	CC_TST_rrASRr(NATIVE_CC_AL,Rn,Rm,Rs)
#define TST_rrRORi(Rn,Rm,i)             	CC_TST_rrRORi(NATIVE_CC_AL,Rn,Rm,i)
#define TST_rrRORr(Rn,Rm,Rs)            	CC_TST_rrRORr(NATIVE_CC_AL,Rn,Rm,Rs)
#define TST_rrRRX(Rn,Rm)                	CC_TST_rrRRX(NATIVE_CC_AL,Rn,Rm)

#define CC_TEQ_ri(cc,Rn,i)              	_OP2(cc,_TEQ,Rn,SHIFT_IMM(i))
#define CC_TEQ_rr(cc,Rn,r)              	_OP2(cc,_TEQ,Rn,SHIFT_REG(r))
#define CC_TEQ_rrLSLi(cc,Rn,Rm,i)       	_OP2(cc,_TEQ,Rn,SHIFT_LSL_i(Rm,i))
#define CC_TEQ_rrLSLr(cc,Rn,Rm,Rs)      	_OP2(cc,_TEQ,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_TEQ_rrLSRi(cc,Rn,Rm,i)       	_OP2(cc,_TEQ,Rn,SHIFT_LSR_i(Rm,i))
#define CC_TEQ_rrLSRr(cc,Rn,Rm,Rs)      	_OP2(cc,_TEQ,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_TEQ_rrASRi(cc,Rn,Rm,i)       	_OP2(cc,_TEQ,Rn,SHIFT_ASR_i(Rm,i))
#define CC_TEQ_rrASRr(cc,Rn,Rm,Rs)      	_OP2(cc,_TEQ,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_TEQ_rrRORi(cc,Rn,Rm,i)       	_OP2(cc,_TEQ,Rn,SHIFT_ROR_i(Rm,i))
#define CC_TEQ_rrRORr(cc,Rn,Rm,Rs)      	_OP2(cc,_TEQ,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_TEQ_rrRRX(cc,Rn,Rm)          	_OP2(cc,_TEQ,Rn,SHIFT_RRX(Rm))

#define TEQ_ri(Rn,i)                    	CC_TEQ_ri(NATIVE_CC_AL,Rn,i)
#define TEQ_rr(Rn,r)                    	CC_TEQ_rr(NATIVE_CC_AL,Rn,r)
#define TEQ_rrLSLi(Rn,Rm,i)            		CC_TEQ_rrLSLi(NATIVE_CC_AL,Rn,Rm,i)
#define TEQ_rrLSLr(Rn,Rm,Rs)           	 	CC_TEQ_rrLSLr(NATIVE_CC_AL,Rn,Rm,Rs)
#define TEQ_rrLSRi(Rn,Rm,i)             	CC_TEQ_rrLSRi(NATIVE_CC_AL,Rn,Rm,i)
#define TEQ_rrLSRr(Rn,Rm,Rs)            	CC_TEQ_rrLSRr(NATIVE_CC_AL,Rn,Rm,Rs)
#define TEQ_rrASRi(Rn,Rm,i)             	CC_TEQ_rrASRi(NATIVE_CC_AL,Rn,Rm,i)
#define TEQ_rrASRr(Rn,Rm,Rs)            	CC_TEQ_rrASRr(NATIVE_CC_AL,Rn,Rm,Rs)
#define TEQ_rrRORi(Rn,Rm,i)             	CC_TEQ_rrRORi(NATIVE_CC_AL,Rn,Rm,i)
#define TEQ_rrRORr(Rn,Rm,Rs)            	CC_TEQ_rrRORr(NATIVE_CC_AL,Rn,Rm,Rs)
#define TEQ_rrRRX(Rn,Rm)                	CC_TEQ_rrRRX(NATIVE_CC_AL,Rn,Rm)

/* Opcodes Type 3 */
#define CC_AND_rri(cc,Rd,Rn,i)              _OP3(cc,_AND,0,Rd,Rn,SHIFT_IMM(i))
#define CC_AND_rrr(cc,Rd,Rn,Rm)         	_OP3(cc,_AND,0,Rd,Rn,SHIFT_REG(Rm))
#define CC_AND_rrrLSLi(cc,Rd,Rn,Rm,i)       _OP3(cc,_AND,0,Rd,Rn,SHIFT_LSL_i(Rm,i))
#define CC_AND_rrrLSLr(cc,Rd,Rn,Rm,Rs)      _OP3(cc,_AND,0,Rd,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_AND_rrrLSRi(cc,Rd,Rn,Rm,i)       _OP3(cc,_AND,0,Rd,Rn,SHIFT_LSR_i(Rm,i))
#define CC_AND_rrrLSRr(cc,Rd,Rn,Rm,Rs)      _OP3(cc,_AND,0,Rd,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_AND_rrrASRi(cc,Rd,Rn,Rm,i)       _OP3(cc,_AND,0,Rd,Rn,SHIFT_ASR_i(Rm,i))
#define CC_AND_rrrASRr(cc,Rd,Rn,Rm,Rs)      _OP3(cc,_AND,0,Rd,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_AND_rrrRORi(cc,Rd,Rn,Rm,i)       _OP3(cc,_AND,0,Rd,Rn,SHIFT_ROR_i(Rm,i))
#define CC_AND_rrrRORr(cc,Rd,Rn,Rm,Rs)      _OP3(cc,_AND,0,Rd,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_AND_rrrRRX(cc,Rd,Rn,Rm)         _OP3(cc,_AND,0,Rd,Rn,SHIFT_RRX(Rm))

#define AND_rri(Rd,Rn,i)                 	CC_AND_rri(NATIVE_CC_AL,Rd,Rn,i)
#define AND_rrr(Rd,Rn,Rm)                 	CC_AND_rrr(NATIVE_CC_AL,Rd,Rn,Rm)
#define AND_rrrLSLi(Rd,Rn,Rm,i)         	CC_AND_rrrLSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define AND_rrrLSLr(Rd,Rn,Rm,Rs)         	CC_AND_rrrLSLr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define AND_rrrLSRi(Rd,Rn,Rm,i)         	CC_AND_rrrLSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define AND_rrrLSRr(Rd,Rn,Rm,Rs)         	CC_AND_rrrLSRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define AND_rrrASRi(Rd,Rn,Rm,i)         	CC_AND_rrrASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define AND_rrrASRr(Rd,Rn,Rm,Rs)         	CC_AND_rrrASRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define AND_rrrRORi(Rd,Rn,Rm,i)         	CC_AND_rrrRORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define AND_rrrRORr(Rd,Rn,Rm,Rs)         	CC_AND_rrrRORr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define AND_rrrRRX(Rd,Rn,Rm)                CC_AND_rrrRRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_ANDS_rri(cc,Rd,Rn,i)         	_OP3(cc,_AND,1,Rd,Rn,SHIFT_IMM(i))
#define CC_ANDS_rrr(cc,Rd,Rn,Rm)         	_OP3(cc,_AND,1,Rd,Rn,SHIFT_REG(Rm))
#define CC_ANDS_rrrLSLi(cc,Rd,Rn,Rm,i)      _OP3(cc,_AND,1,Rd,Rn,SHIFT_LSL_i(Rm,i))
#define CC_ANDS_rrrLSLr(cc,Rd,Rn,Rm,Rs)     _OP3(cc,_AND,1,Rd,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_ANDS_rrrLSRi(cc,Rd,Rn,Rm,i)      _OP3(cc,_AND,1,Rd,Rn,SHIFT_LSR_i(Rm,i))
#define CC_ANDS_rrrLSRr(cc,Rd,Rn,Rm,Rs)     _OP3(cc,_AND,1,Rd,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_ANDS_rrrASRi(cc,Rd,Rn,Rm,i)      _OP3(cc,_AND,1,Rd,Rn,SHIFT_ASR_i(Rm,i))
#define CC_ANDS_rrrASRr(cc,Rd,Rn,Rm,Rs)     _OP3(cc,_AND,1,Rd,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_ANDS_rrrRORi(cc,Rd,Rn,Rm,i)      _OP3(cc,_AND,1,Rd,Rn,SHIFT_ROR_i(Rm,i))
#define CC_ANDS_rrrRORr(cc,Rd,Rn,Rm,Rs)     _OP3(cc,_AND,1,Rd,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_ANDS_rrrRRX(cc,Rd,Rn,Rm)         _OP3(cc,_AND,1,Rd,Rn,SHIFT_RRX(Rm))

#define ANDS_rri(Rd,Rn,i)                 	CC_ANDS_rri(NATIVE_CC_AL,Rd,Rn,i)
#define ANDS_rrr(Rd,Rn,Rm)                 	CC_ANDS_rrr(NATIVE_CC_AL,Rd,Rn,Rm)
#define ANDS_rrrLSLi(Rd,Rn,Rm,i)         	CC_ANDS_rrrLSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ANDS_rrrLSLr(Rd,Rn,Rm,Rs)         	CC_ANDS_rrrLSLr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ANDS_rrrLSRi(Rd,Rn,Rm,i)         	CC_ANDS_rrrLSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ANDS_rrrLSRr(Rd,Rn,Rm,Rs)         	CC_ANDS_rrrLSRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ANDS_rrrASRi(Rd,Rn,Rm,i)         	CC_ANDS_rrrASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ANDS_rrrASRr(Rd,Rn,Rm,Rs)         	CC_ANDS_rrrASRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ANDS_rrrRORi(Rd,Rn,Rm,i)         	CC_ANDS_rrrRORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ANDS_rrrRORr(Rd,Rn,Rm,Rs)         	CC_ANDS_rrrRORr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ANDS_rrrRRX(Rd,Rn,Rm)   	        CC_ANDS_rrrRRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_EOR_rri(cc,Rd,Rn,i)          	_OP3(cc,_EOR,0,Rd,Rn,SHIFT_IMM(i))
#define CC_EOR_rrr(cc,Rd,Rn,Rm)         	_OP3(cc,_EOR,0,Rd,Rn,SHIFT_REG(Rm))
#define CC_EOR_rrrLSLi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_EOR,0,Rd,Rn,SHIFT_LSL_i(Rm,i))
#define CC_EOR_rrrLSLr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_EOR,0,Rd,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_EOR_rrrLSRi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_EOR,0,Rd,Rn,SHIFT_LSR_i(Rm,i))
#define CC_EOR_rrrLSRr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_EOR,0,Rd,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_EOR_rrrASRi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_EOR,0,Rd,Rn,SHIFT_ASR_i(Rm,i))
#define CC_EOR_rrrASRr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_EOR,0,Rd,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_EOR_rrrRORi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_EOR,0,Rd,Rn,SHIFT_ROR_i(Rm,i))
#define CC_EOR_rrrRORr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_EOR,0,Rd,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_EOR_rrrRRX(cc,Rd,Rn,Rm)     		_OP3(cc,_EOR,0,Rd,Rn,SHIFT_RRX(Rm))

#define EOR_rri(Rd,Rn,i)                	CC_EOR_rri(NATIVE_CC_AL,Rd,Rn,i)
#define EOR_rrr(Rd,Rn,Rm)               	CC_EOR_rrr(NATIVE_CC_AL,Rd,Rn,Rm)
#define EOR_rrrLSLi(Rd,Rn,Rm,i)         	CC_EOR_rrrLSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define EOR_rrrLSLr(Rd,Rn,Rm,Rs)        	CC_EOR_rrrLSLr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define EOR_rrrLSRi(Rd,Rn,Rm,i)         	CC_EOR_rrrLSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define EOR_rrrLSRr(Rd,Rn,Rm,Rs)        	CC_EOR_rrrLSRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define EOR_rrrASRi(Rd,Rn,Rm,i)         	CC_EOR_rrrASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define EOR_rrrASRr(Rd,Rn,Rm,Rs)        	CC_EOR_rrrASRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define EOR_rrrRORi(Rd,Rn,Rm,i)         	CC_EOR_rrrRORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define EOR_rrrRORr(Rd,Rn,Rm,Rs)        	CC_EOR_rrrRORr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define EOR_rrrRRX(Rd,Rn,Rm)            	CC_EOR_rrrRRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_EORS_rri(cc,Rd,Rn,i)         	_OP3(cc,_EOR,1,Rd,Rn,SHIFT_IMM(i))
#define CC_EORS_rrr(cc,Rd,Rn,Rm)        	_OP3(cc,_EOR,1,Rd,Rn,SHIFT_REG(Rm))
#define CC_EORS_rrrLSLi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_EOR,1,Rd,Rn,SHIFT_LSL_i(Rm,i))
#define CC_EORS_rrrLSLr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_EOR,1,Rd,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_EORS_rrrLSRi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_EOR,1,Rd,Rn,SHIFT_LSR_i(Rm,i))
#define CC_EORS_rrrLSRr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_EOR,1,Rd,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_EORS_rrrASRi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_EOR,1,Rd,Rn,SHIFT_ASR_i(Rm,i))
#define CC_EORS_rrrASRr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_EOR,1,Rd,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_EORS_rrrRORi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_EOR,1,Rd,Rn,SHIFT_ROR_i(Rm,i))
#define CC_EORS_rrrRORr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_EOR,1,Rd,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_EORS_rrrRRX(cc,Rd,Rn,Rm)    		_OP3(cc,_EOR,1,Rd,Rn,SHIFT_RRX(Rm))

#define EORS_rri(Rd,Rn,i)               	CC_EORS_rri(NATIVE_CC_AL,Rd,Rn,i)
#define EORS_rrr(Rd,Rn,Rm)              	CC_EORS_rrr(NATIVE_CC_AL,Rd,Rn,Rm)
#define EORS_rrrLSLi(Rd,Rn,Rm,i)        	CC_EORS_rrrLSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define EORS_rrrLSLr(Rd,Rn,Rm,Rs)       	CC_EORS_rrrLSLr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define EORS_rrrLSRi(Rd,Rn,Rm,i)        	CC_EORS_rrrLSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define EORS_rrrLSRr(Rd,Rn,Rm,Rs)       	CC_EORS_rrrLSRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define EORS_rrrASRi(Rd,Rn,Rm,i)        	CC_EORS_rrrASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define EORS_rrrASRr(Rd,Rn,Rm,Rs)       	CC_EORS_rrrASRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define EORS_rrrRORi(Rd,Rn,Rm,i)        	CC_EORS_rrrRORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define EORS_rrrRORr(Rd,Rn,Rm,Rs)       	CC_EORS_rrrRORr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define EORS_rrrRRX(Rd,Rn,Rm)           	CC_EORS_rrrRRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_SUB_rri(cc,Rd,Rn,i)          	_OP3(cc,_SUB,0,Rd,Rn,SHIFT_IMM(i))
#define CC_SUB_rrr(cc,Rd,Rn,Rm)         	_OP3(cc,_SUB,0,Rd,Rn,SHIFT_REG(Rm))
#define CC_SUB_rrrLSLi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_SUB,0,Rd,Rn,SHIFT_LSL_i(Rm,i))
#define CC_SUB_rrrLSLr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_SUB,0,Rd,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_SUB_rrrLSRi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_SUB,0,Rd,Rn,SHIFT_LSR_i(Rm,i))
#define CC_SUB_rrrLSRr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_SUB,0,Rd,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_SUB_rrrASRi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_SUB,0,Rd,Rn,SHIFT_ASR_i(Rm,i))
#define CC_SUB_rrrASRr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_SUB,0,Rd,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_SUB_rrrRORi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_SUB,0,Rd,Rn,SHIFT_ROR_i(Rm,i))
#define CC_SUB_rrrRORr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_SUB,0,Rd,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_SUB_rrrRRX(cc,Rd,Rn,Rm)     		_OP3(cc,_SUB,0,Rd,Rn,SHIFT_RRX(Rm))

#define SUB_rri(Rd,Rn,i)                	CC_SUB_rri(NATIVE_CC_AL,Rd,Rn,i)
#define SUB_rrr(Rd,Rn,Rm)               	CC_SUB_rrr(NATIVE_CC_AL,Rd,Rn,Rm)
#define SUB_rrrLSLi(Rd,Rn,Rm,i)         	CC_SUB_rrrLSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define SUB_rrrLSLr(Rd,Rn,Rm,Rs)        	CC_SUB_rrrLSLr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define SUB_rrrLSRi(Rd,Rn,Rm,i)         	CC_SUB_rrrLSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define SUB_rrrLSRr(Rd,Rn,Rm,Rs)        	CC_SUB_rrrLSRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define SUB_rrrASRi(Rd,Rn,Rm,i)         	CC_SUB_rrrASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define SUB_rrrASRr(Rd,Rn,Rm,Rs)        	CC_SUB_rrrASRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define SUB_rrrRORi(Rd,Rn,Rm,i)         	CC_SUB_rrrRORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define SUB_rrrRORr(Rd,Rn,Rm,Rs)        	CC_SUB_rrrRORr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define SUB_rrrRRX(Rd,Rn,Rm)            	CC_SUB_rrrRRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_SUBS_rri(cc,Rd,Rn,i)         	_OP3(cc,_SUB,1,Rd,Rn,SHIFT_IMM(i))
#define CC_SUBS_rrr(cc,Rd,Rn,Rm)        	_OP3(cc,_SUB,1,Rd,Rn,SHIFT_REG(Rm))
#define CC_SUBS_rrrLSLi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_SUB,1,Rd,Rn,SHIFT_LSL_i(Rm,i))
#define CC_SUBS_rrrLSLr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_SUB,1,Rd,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_SUBS_rrrLSRi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_SUB,1,Rd,Rn,SHIFT_LSR_i(Rm,i))
#define CC_SUBS_rrrLSRr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_SUB,1,Rd,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_SUBS_rrrASRi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_SUB,1,Rd,Rn,SHIFT_ASR_i(Rm,i))
#define CC_SUBS_rrrASRr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_SUB,1,Rd,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_SUBS_rrrRORi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_SUB,1,Rd,Rn,SHIFT_ROR_i(Rm,i))
#define CC_SUBS_rrrRORr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_SUB,1,Rd,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_SUBS_rrrRRX(cc,Rd,Rn,Rm) 	   	_OP3(cc,_SUB,1,Rd,Rn,SHIFT_RRX(Rm))

#define SUBS_rri(Rd,Rn,i)               	CC_SUBS_rri(NATIVE_CC_AL,Rd,Rn,i)
#define SUBS_rrr(Rd,Rn,Rm)              	CC_SUBS_rrr(NATIVE_CC_AL,Rd,Rn,Rm)
#define SUBS_rrrLSLi(Rd,Rn,Rm,i)        	CC_SUBS_rrrLSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define SUBS_rrrLSLr(Rd,Rn,Rm,Rs)       	CC_SUBS_rrrLSLr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define SUBS_rrrLSRi(Rd,Rn,Rm,i)        	CC_SUBS_rrrLSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define SUBS_rrrLSRr(Rd,Rn,Rm,Rs)       	CC_SUBS_rrrLSRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define SUBS_rrrASRi(Rd,Rn,Rm,i)        	CC_SUBS_rrrASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define SUBS_rrrASRr(Rd,Rn,Rm,Rs)       	CC_SUBS_rrrASRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define SUBS_rrrRORi(Rd,Rn,Rm,i)        	CC_SUBS_rrrRORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define SUBS_rrrRORr(Rd,Rn,Rm,Rs)       	CC_SUBS_rrrRORr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define SUBS_rrrRRX(Rd,Rn,Rm)           	CC_SUBS_rrrRRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_RSB_rri(cc,Rd,Rn,i)          	_OP3(cc,_RSB,0,Rd,Rn,SHIFT_IMM(i))
#define CC_RSB_rrr(cc,Rd,Rn,Rm)         	_OP3(cc,_RSB,0,Rd,Rn,SHIFT_REG(Rm))
#define CC_RSB_rrrLSLi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_RSB,0,Rd,Rn,SHIFT_LSL_i(Rm,i))
#define CC_RSB_rrrLSLr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_RSB,0,Rd,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_RSB_rrrLSRi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_RSB,0,Rd,Rn,SHIFT_LSR_i(Rm,i))
#define CC_RSB_rrrLSRr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_RSB,0,Rd,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_RSB_rrrASRi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_RSB,0,Rd,Rn,SHIFT_ASR_i(Rm,i))
#define CC_RSB_rrrASRr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_RSB,0,Rd,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_RSB_rrrRORi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_RSB,0,Rd,Rn,SHIFT_ROR_i(Rm,i))
#define CC_RSB_rrrRORr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_RSB,0,Rd,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_RSB_rrrRRX(cc,Rd,Rn,Rm)     		_OP3(cc,_RSB,0,Rd,Rn,SHIFT_RRX(Rm))

#define RSB_rri(Rd,Rn,i)                	CC_RSB_rri(NATIVE_CC_AL,Rd,Rn,i)
#define RSB_rrr(Rd,Rn,Rm)               	CC_RSB_rrr(NATIVE_CC_AL,Rd,Rn,Rm)
#define RSB_rrrLSLi(Rd,Rn,Rm,i)         	CC_RSB_rrrLSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define RSB_rrrLSLr(Rd,Rn,Rm,Rs)        	CC_RSB_rrrLSLr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define RSB_rrrLSRi(Rd,Rn,Rm,i)         	CC_RSB_rrrLSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define RSB_rrrLSRr(Rd,Rn,Rm,Rs)        	CC_RSB_rrrLSRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define RSB_rrrASRi(Rd,Rn,Rm,i)         	CC_RSB_rrrASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define RSB_rrrASRr(Rd,Rn,Rm,Rs)        	CC_RSB_rrrASRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define RSB_rrrRORi(Rd,Rn,Rm,i)         	CC_RSB_rrrRORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define RSB_rrrRORr(Rd,Rn,Rm,Rs)        	CC_RSB_rrrRORr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define RSB_rrrRRX(Rd,Rn,Rm)            	CC_RSB_rrrRRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_RSBS_rri(cc,Rd,Rn,i)         	_OP3(cc,_RSB,1,Rd,Rn,SHIFT_IMM(i))
#define CC_RSBS_rrr(cc,Rd,Rn,Rm)        	_OP3(cc,_RSB,1,Rd,Rn,SHIFT_REG(Rm))
#define CC_RSBS_rrrLSLi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_RSB,1,Rd,Rn,SHIFT_LSL_i(Rm,i))
#define CC_RSBS_rrrLSLr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_RSB,1,Rd,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_RSBS_rrrLSRi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_RSB,1,Rd,Rn,SHIFT_LSR_i(Rm,i))
#define CC_RSBS_rrrLSRr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_RSB,1,Rd,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_RSBS_rrrASRi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_RSB,1,Rd,Rn,SHIFT_ASR_i(Rm,i))
#define CC_RSBS_rrrASRr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_RSB,1,Rd,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_RSBS_rrrRORi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_RSB,1,Rd,Rn,SHIFT_ROR_i(Rm,i))
#define CC_RSBS_rrrRORr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_RSB,1,Rd,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_RSBS_rrrRRX(cc,Rd,Rn,Rm)    		_OP3(cc,_RSB,1,Rd,Rn,SHIFT_RRX(Rm))

#define RSBS_rri(Rd,Rn,i)               	CC_RSBS_rri(NATIVE_CC_AL,Rd,Rn,i)
#define RSBS_rrr(Rd,Rn,Rm)              	CC_RSBS_rrr(NATIVE_CC_AL,Rd,Rn,Rm)
#define RSBS_rrrLSLi(Rd,Rn,Rm,i)        	CC_RSBS_rrrLSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define RSBS_rrrLSLr(Rd,Rn,Rm,Rs)       	CC_RSBS_rrrLSLr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define RSBS_rrrLSRi(Rd,Rn,Rm,i)        	CC_RSBS_rrrLSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define RSBS_rrrLSRr(Rd,Rn,Rm,Rs)       	CC_RSBS_rrrLSRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define RSBS_rrrASRi(Rd,Rn,Rm,i)        	CC_RSBS_rrrASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define RSBS_rrrASRr(Rd,Rn,Rm,Rs)       	CC_RSBS_rrrASRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define RSBS_rrrRORi(Rd,Rn,Rm,i)        	CC_RSBS_rrrRORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define RSBS_rrrRORr(Rd,Rn,Rm,Rs)       	CC_RSBS_rrrRORr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define RSBS_rrrRRX(Rd,Rn,Rm)           	CC_RSBS_rrrRRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_ADD_rri8(cc,Rd,Rn,i)          	_OP3(cc,_ADD,0,Rd,Rn,UNSHIFT_IMM8(i))
#define CC_ADD_rri8RORi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_ADD,0,Rd,Rn,SHIFT_IMM8_ROR(Rm,i))

#define CC_ADD_rri(cc,Rd,Rn,i)          	_OP3(cc,_ADD,0,Rd,Rn,SHIFT_IMM(i))
#define CC_ADD_rrr(cc,Rd,Rn,Rm)         	_OP3(cc,_ADD,0,Rd,Rn,SHIFT_REG(Rm))
#define CC_ADD_rrrLSLi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_ADD,0,Rd,Rn,SHIFT_LSL_i(Rm,i))
#define CC_ADD_rrrLSLr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_ADD,0,Rd,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_ADD_rrrLSRi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_ADD,0,Rd,Rn,SHIFT_LSR_i(Rm,i))
#define CC_ADD_rrrLSRr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_ADD,0,Rd,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_ADD_rrrASRi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_ADD,0,Rd,Rn,SHIFT_ASR_i(Rm,i))
#define CC_ADD_rrrASRr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_ADD,0,Rd,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_ADD_rrrRORi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_ADD,0,Rd,Rn,SHIFT_ROR_i(Rm,i))
#define CC_ADD_rrrRORr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_ADD,0,Rd,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_ADD_rrrRRX(cc,Rd,Rn,Rm)     		_OP3(cc,_ADD,0,Rd,Rn,SHIFT_RRX(Rm))

#define ADD_rri8(cc,Rd,Rn,i)          		CC_ADD_rri8(NATIVE_CC_AL,Rd,Rn,i)
#define ADD_rri8RORi(cc,Rd,Rn,Rm,i)   		CC_ADD_rri8RORi(NATIVE_CC_AL,Rd,Rn,Rm,i)

#define ADD_rri(Rd,Rn,i)                	CC_ADD_rri(NATIVE_CC_AL,Rd,Rn,i)
#define ADD_rrr(Rd,Rn,Rm)               	CC_ADD_rrr(NATIVE_CC_AL,Rd,Rn,Rm)
#define ADD_rrrLSLi(Rd,Rn,Rm,i)         	CC_ADD_rrrLSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ADD_rrrLSLr(Rd,Rn,Rm,Rs)        	CC_ADD_rrrLSLr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ADD_rrrLSRi(Rd,Rn,Rm,i)         	CC_ADD_rrrLSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ADD_rrrLSRr(Rd,Rn,Rm,Rs)        	CC_ADD_rrrLSRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ADD_rrrASRi(Rd,Rn,Rm,i)         	CC_ADD_rrrASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ADD_rrrASRr(Rd,Rn,Rm,Rs)        	CC_ADD_rrrASRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ADD_rrrRORi(Rd,Rn,Rm,i)         	CC_ADD_rrrRORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ADD_rrrRORr(Rd,Rn,Rm,Rs)        	CC_ADD_rrrRORr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ADD_rrrRRX(Rd,Rn,Rm)            	CC_ADD_rrrRRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_ADDS_rri(cc,Rd,Rn,i)         	_OP3(cc,_ADD,1,Rd,Rn,SHIFT_IMM(i))
#define CC_ADDS_rrr(cc,Rd,Rn,Rm)        	_OP3(cc,_ADD,1,Rd,Rn,SHIFT_REG(Rm))
#define CC_ADDS_rrrLSLi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_ADD,1,Rd,Rn,SHIFT_LSL_i(Rm,i))
#define CC_ADDS_rrrLSLr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_ADD,1,Rd,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_ADDS_rrrLSRi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_ADD,1,Rd,Rn,SHIFT_LSR_i(Rm,i))
#define CC_ADDS_rrrLSRr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_ADD,1,Rd,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_ADDS_rrrASRi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_ADD,1,Rd,Rn,SHIFT_ASR_i(Rm,i))
#define CC_ADDS_rrrASRr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_ADD,1,Rd,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_ADDS_rrrRORi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_ADD,1,Rd,Rn,SHIFT_ROR_i(Rm,i))
#define CC_ADDS_rrrRORr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_ADD,1,Rd,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_ADDS_rrrRRX(cc,Rd,Rn,Rm)    		_OP3(cc,_ADD,1,Rd,Rn,SHIFT_RRX(Rm))

#define ADDS_rri(Rd,Rn,i)               	CC_ADDS_rri(NATIVE_CC_AL,Rd,Rn,i)
#define ADDS_rrr(Rd,Rn,Rm)              	CC_ADDS_rrr(NATIVE_CC_AL,Rd,Rn,Rm)
#define ADDS_rrrLSLi(Rd,Rn,Rm,i)        	CC_ADDS_rrrLSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ADDS_rrrLSLr(Rd,Rn,Rm,Rs)       	CC_ADDS_rrrLSLr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ADDS_rrrLSRi(Rd,Rn,Rm,i)        	CC_ADDS_rrrLSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ADDS_rrrLSRr(Rd,Rn,Rm,Rs)       	CC_ADDS_rrrLSRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ADDS_rrrASRi(Rd,Rn,Rm,i)        	CC_ADDS_rrrASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ADDS_rrrASRr(Rd,Rn,Rm,Rs)       	CC_ADDS_rrrASRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ADDS_rrrRORi(Rd,Rn,Rm,i)        	CC_ADDS_rrrRORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ADDS_rrrRORr(Rd,Rn,Rm,Rs)       	CC_ADDS_rrrRORr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ADDS_rrrRRX(Rd,Rn,Rm)           	CC_ADDS_rrrRRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_ADC_rri(cc,Rd,Rn,i)          	_OP3(cc,_ADC,0,Rd,Rn,SHIFT_IMM(i))
#define CC_ADC_rrr(cc,Rd,Rn,Rm)         	_OP3(cc,_ADC,0,Rd,Rn,SHIFT_REG(Rm))
#define CC_ADC_rrrLSLi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_ADC,0,Rd,Rn,SHIFT_LSL_i(Rm,i))
#define CC_ADC_rrrLSLr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_ADC,0,Rd,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_ADC_rrrLSRi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_ADC,0,Rd,Rn,SHIFT_LSR_i(Rm,i))
#define CC_ADC_rrrLSRr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_ADC,0,Rd,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_ADC_rrrASRi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_ADC,0,Rd,Rn,SHIFT_ASR_i(Rm,i))
#define CC_ADC_rrrASRr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_ADC,0,Rd,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_ADC_rrrRORi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_ADC,0,Rd,Rn,SHIFT_ROR_i(Rm,i))
#define CC_ADC_rrrRORr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_ADC,0,Rd,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_ADC_rrrRRX(cc,Rd,Rn,Rm)     		_OP3(cc,_ADC,0,Rd,Rn,SHIFT_RRX(Rm))

#define ADC_rri(Rd,Rn,i)                	CC_ADC_rri(NATIVE_CC_AL,Rd,Rn,i)
#define ADC_rrr(Rd,Rn,Rm)               	CC_ADC_rrr(NATIVE_CC_AL,Rd,Rn,Rm)
#define ADC_rrrLSLi(Rd,Rn,Rm,i)         	CC_ADC_rrrLSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ADC_rrrLSLr(Rd,Rn,Rm,Rs)        	CC_ADC_rrrLSLr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ADC_rrrLSRi(Rd,Rn,Rm,i)         	CC_ADC_rrrLSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ADC_rrrLSRr(Rd,Rn,Rm,Rs)        	CC_ADC_rrrLSRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ADC_rrrASRi(Rd,Rn,Rm,i)         	CC_ADC_rrrASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ADC_rrrASRr(Rd,Rn,Rm,Rs)        	CC_ADC_rrrASRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ADC_rrrRORi(Rd,Rn,Rm,i)         	CC_ADC_rrrRORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ADC_rrrRORr(Rd,Rn,Rm,Rs)        	CC_ADC_rrrRORr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ADC_rrrRRX(Rd,Rn,Rm)            	CC_ADC_rrrRRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_ADCS_rri(cc,Rd,Rn,i)         	_OP3(cc,_ADC,1,Rd,Rn,SHIFT_IMM(i))
#define CC_ADCS_rrr(cc,Rd,Rn,Rm)        	_OP3(cc,_ADC,1,Rd,Rn,SHIFT_REG(Rm))
#define CC_ADCS_rrrLSLi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_ADC,1,Rd,Rn,SHIFT_LSL_i(Rm,i))
#define CC_ADCS_rrrLSLr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_ADC,1,Rd,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_ADCS_rrrLSRi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_ADC,1,Rd,Rn,SHIFT_LSR_i(Rm,i))
#define CC_ADCS_rrrLSRr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_ADC,1,Rd,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_ADCS_rrrASRi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_ADC,1,Rd,Rn,SHIFT_ASR_i(Rm,i))
#define CC_ADCS_rrrASRr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_ADC,1,Rd,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_ADCS_rrrRORi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_ADC,1,Rd,Rn,SHIFT_ROR_i(Rm,i))
#define CC_ADCS_rrrRORr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_ADC,1,Rd,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_ADCS_rrrRRX(cc,Rd,Rn,Rm)    		_OP3(cc,_ADC,1,Rd,Rn,SHIFT_RRX(Rm))

#define ADCS_rri(Rd,Rn,i)               	CC_ADCS_rri(NATIVE_CC_AL,Rd,Rn,i)
#define ADCS_rrr(Rd,Rn,Rm)              	CC_ADCS_rrr(NATIVE_CC_AL,Rd,Rn,Rm)
#define ADCS_rrrLSLi(Rd,Rn,Rm,i)        	CC_ADCS_rrrLSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ADCS_rrrLSLr(Rd,Rn,Rm,Rs)       	CC_ADCS_rrrLSLr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ADCS_rrrLSRi(Rd,Rn,Rm,i)        	CC_ADCS_rrrLSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ADCS_rrrLSRr(Rd,Rn,Rm,Rs)       	CC_ADCS_rrrLSRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ADCS_rrrASRi(Rd,Rn,Rm,i)        	CC_ADCS_rrrASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ADCS_rrrASRr(Rd,Rn,Rm,Rs)       	CC_ADCS_rrrASRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ADCS_rrrRORi(Rd,Rn,Rm,i)        	CC_ADCS_rrrRORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ADCS_rrrRORr(Rd,Rn,Rm,Rs)       	CC_ADCS_rrrRORr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ADCS_rrrRRX(Rd,Rn,Rm)           	CC_ADCS_rrrRRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_SBC_rri(cc,Rd,Rn,i)          	_OP3(cc,_SBC,0,Rd,Rn,SHIFT_IMM(i))
#define CC_SBC_rrr(cc,Rd,Rn,Rm)         	_OP3(cc,_SBC,0,Rd,Rn,SHIFT_REG(Rm))
#define CC_SBC_rrrLSLi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_SBC,0,Rd,Rn,SHIFT_LSL_i(Rm,i))
#define CC_SBC_rrrLSLr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_SBC,0,Rd,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_SBC_rrrLSRi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_SBC,0,Rd,Rn,SHIFT_LSR_i(Rm,i))
#define CC_SBC_rrrLSRr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_SBC,0,Rd,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_SBC_rrrASRi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_SBC,0,Rd,Rn,SHIFT_ASR_i(Rm,i))
#define CC_SBC_rrrASRr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_SBC,0,Rd,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_SBC_rrrRORi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_SBC,0,Rd,Rn,SHIFT_ROR_i(Rm,i))
#define CC_SBC_rrrRORr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_SBC,0,Rd,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_SBC_rrrRRX(cc,Rd,Rn,Rm)     		_OP3(cc,_SBC,0,Rd,Rn,SHIFT_RRX(Rm))

#define SBC_rri(Rd,Rn,i)                	CC_SBC_rri(NATIVE_CC_AL,Rd,Rn,i)
#define SBC_rrr(Rd,Rn,Rm)               	CC_SBC_rrr(NATIVE_CC_AL,Rd,Rn,Rm)
#define SBC_rrrLSLi(Rd,Rn,Rm,i)         	CC_SBC_rrrLSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define SBC_rrrLSLr(Rd,Rn,Rm,Rs)        	CC_SBC_rrrLSLr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define SBC_rrrLSRi(Rd,Rn,Rm,i)         	CC_SBC_rrrLSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define SBC_rrrLSRr(Rd,Rn,Rm,Rs)        	CC_SBC_rrrLSRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define SBC_rrrASRi(Rd,Rn,Rm,i)         	CC_SBC_rrrASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define SBC_rrrASRr(Rd,Rn,Rm,Rs)        	CC_SBC_rrrASRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define SBC_rrrRORi(Rd,Rn,Rm,i)         	CC_SBC_rrrRORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define SBC_rrrRORr(Rd,Rn,Rm,Rs)        	CC_SBC_rrrRORr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define SBC_rrrRRX(Rd,Rn,Rm)            	CC_SBC_rrrRRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_SBCS_rri(cc,Rd,Rn,i)         	_OP3(cc,_SBC,1,Rd,Rn,SHIFT_IMM(i))
#define CC_SBCS_rrr(cc,Rd,Rn,Rm)        	_OP3(cc,_SBC,1,Rd,Rn,SHIFT_REG(Rm))
#define CC_SBCS_rrrLSLi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_SBC,1,Rd,Rn,SHIFT_LSL_i(Rm,i))
#define CC_SBCS_rrrLSLr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_SBC,1,Rd,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_SBCS_rrrLSRi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_SBC,1,Rd,Rn,SHIFT_LSR_i(Rm,i))
#define CC_SBCS_rrrLSRr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_SBC,1,Rd,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_SBCS_rrrASRi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_SBC,1,Rd,Rn,SHIFT_ASR_i(Rm,i))
#define CC_SBCS_rrrASRr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_SBC,1,Rd,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_SBCS_rrrRORi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_SBC,1,Rd,Rn,SHIFT_ROR_i(Rm,i))
#define CC_SBCS_rrrRORr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_SBC,1,Rd,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_SBCS_rrrRRX(cc,Rd,Rn,Rm)    		_OP3(cc,_SBC,1,Rd,Rn,SHIFT_RRX(Rm))

#define SBCS_rri(Rd,Rn,i)               	CC_SBCS_rri(NATIVE_CC_AL,Rd,Rn,i)
#define SBCS_rrr(Rd,Rn,Rm)              	CC_SBCS_rrr(NATIVE_CC_AL,Rd,Rn,Rm)
#define SBCS_rrrLSLi(Rd,Rn,Rm,i)        	CC_SBCS_rrrLSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define SBCS_rrrLSLr(Rd,Rn,Rm,Rs)       	CC_SBCS_rrrLSLr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define SBCS_rrrLSRi(Rd,Rn,Rm,i)        	CC_SBCS_rrrLSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define SBCS_rrrLSRr(Rd,Rn,Rm,Rs)       	CC_SBCS_rrrLSRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define SBCS_rrrASRi(Rd,Rn,Rm,i)        	CC_SBCS_rrrASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define SBCS_rrrASRr(Rd,Rn,Rm,Rs)       	CC_SBCS_rrrASRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define SBCS_rrrRORi(Rd,Rn,Rm,i)        	CC_SBCS_rrrRORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define SBCS_rrrRORr(Rd,Rn,Rm,Rs)       	CC_SBCS_rrrRORr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define SBCS_rrrRRX(Rd,Rn,Rm)           	CC_SBCS_rrrRRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_RSC_rri(cc,Rd,Rn,i)          	_OP3(cc,_RSC,0,Rd,Rn,SHIFT_IMM(i))
#define CC_RSC_rrr(cc,Rd,Rn,Rm)         	_OP3(cc,_RSC,0,Rd,Rn,SHIFT_REG(Rm))
#define CC_RSC_rrrLSLi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_RSC,0,Rd,Rn,SHIFT_LSL_i(Rm,i))
#define CC_RSC_rrrLSLr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_RSC,0,Rd,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_RSC_rrrLSRi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_RSC,0,Rd,Rn,SHIFT_LSR_i(Rm,i))
#define CC_RSC_rrrLSRr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_RSC,0,Rd,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_RSC_rrrASRi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_RSC,0,Rd,Rn,SHIFT_ASR_i(Rm,i))
#define CC_RSC_rrrASRr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_RSC,0,Rd,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_RSC_rrrRORi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_RSC,0,Rd,Rn,SHIFT_ROR_i(Rm,i))
#define CC_RSC_rrrRORr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_RSC,0,Rd,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_RSC_rrrRRX(cc,Rd,Rn,Rm)     		_OP3(cc,_RSC,0,Rd,Rn,SHIFT_RRX(Rm))

#define RSC_rri(Rd,Rn,i)                	CC_RSC_rri(NATIVE_CC_AL,Rd,Rn,i)
#define RSC_rrr(Rd,Rn,Rm)               	CC_RSC_rrr(NATIVE_CC_AL,Rd,Rn,Rm)
#define RSC_rrrLSLi(Rd,Rn,Rm,i)         	CC_RSC_rrrLSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define RSC_rrrLSLr(Rd,Rn,Rm,Rs)        	CC_RSC_rrrLSLr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define RSC_rrrLSRi(Rd,Rn,Rm,i)         	CC_RSC_rrrLSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define RSC_rrrLSRr(Rd,Rn,Rm,Rs)        	CC_RSC_rrrLSRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define RSC_rrrASRi(Rd,Rn,Rm,i)         	CC_RSC_rrrASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define RSC_rrrASRr(Rd,Rn,Rm,Rs)        	CC_RSC_rrrASRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define RSC_rrrRORi(Rd,Rn,Rm,i)         	CC_RSC_rrrRORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define RSC_rrrRORr(Rd,Rn,Rm,Rs)        	CC_RSC_rrrRORr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define RSC_rrrRRX(Rd,Rn,Rm)            	CC_RSC_rrrRRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_RSCS_rri(cc,Rd,Rn,i)         	_OP3(cc,_RSC,1,Rd,Rn,SHIFT_IMM(i))
#define CC_RSCS_rrr(cc,Rd,Rn,Rm)        	_OP3(cc,_RSC,1,Rd,Rn,SHIFT_REG(Rm))
#define CC_RSCS_rrrLSLi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_RSC,1,Rd,Rn,SHIFT_LSL_i(Rm,i))
#define CC_RSCS_rrrLSLr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_RSC,1,Rd,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_RSCS_rrrLSRi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_RSC,1,Rd,Rn,SHIFT_LSR_i(Rm,i))
#define CC_RSCS_rrrLSRr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_RSC,1,Rd,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_RSCS_rrrASRi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_RSC,1,Rd,Rn,SHIFT_ASR_i(Rm,i))
#define CC_RSCS_rrrASRr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_RSC,1,Rd,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_RSCS_rrrRORi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_RSC,1,Rd,Rn,SHIFT_ROR_i(Rm,i))
#define CC_RSCS_rrrRORr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_RSC,1,Rd,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_RSCS_rrrRRX(cc,Rd,Rn,Rm)    		_OP3(cc,_RSC,1,Rd,Rn,SHIFT_RRX(Rm))

#define RSCS_rri(Rd,Rn,i)               	CC_RSCS_rri(NATIVE_CC_AL,Rd,Rn,i)
#define RSCS_rrr(Rd,Rn,Rm)              	CC_RSCS_rrr(NATIVE_CC_AL,Rd,Rn,Rm)
#define RSCS_rrrLSLi(Rd,Rn,Rm,i)        	CC_RSCS_rrrLSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define RSCS_rrrLSLr(Rd,Rn,Rm,Rs)       	CC_RSCS_rrrLSLr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define RSCS_rrrLSRi(Rd,Rn,Rm,i)        	CC_RSCS_rrrLSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define RSCS_rrrLSRr(Rd,Rn,Rm,Rs)       	CC_RSCS_rrrLSRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define RSCS_rrrASRi(Rd,Rn,Rm,i)        	CC_RSCS_rrrASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define RSCS_rrrASRr(Rd,Rn,Rm,Rs)       	CC_RSCS_rrrASRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define RSCS_rrrRORi(Rd,Rn,Rm,i)        	CC_RSCS_rrrRORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define RSCS_rrrRORr(Rd,Rn,Rm,Rs)       	CC_RSCS_rrrRORr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define RSCS_rrrRRX(Rd,Rn,Rm)           	CC_RSCS_rrrRRX(NATIVE_CC_AL,Rd,Rn,Rm)

/* ORRcc Rd,Rn,#i */
#define CC_ORR_rri8(cc,Rd,Rn,i)          	_OP3(cc,_ORR,0,Rd,Rn,UNSHIFTED_IMM8(i))
/* ORRcc Rd,Rn,#i ROR #s */
#define CC_ORR_rri8RORi(cc,Rd,Rn,i,s)      	_OP3(cc,_ORR,0,Rd,Rn,SHIFT_IMM8_ROR(i,s))

#define CC_ORR_rri(cc,Rd,Rn,i)          	_OP3(cc,_ORR,0,Rd,Rn,SHIFT_IMM(i))
#define CC_ORR_rrr(cc,Rd,Rn,Rm)         	_OP3(cc,_ORR,0,Rd,Rn,SHIFT_REG(Rm))
#define CC_ORR_rrrLSLi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_ORR,0,Rd,Rn,SHIFT_LSL_i(Rm,i))
#define CC_ORR_rrrLSLr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_ORR,0,Rd,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_ORR_rrrLSRi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_ORR,0,Rd,Rn,SHIFT_LSR_i(Rm,i))
#define CC_ORR_rrrLSRr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_ORR,0,Rd,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_ORR_rrrASRi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_ORR,0,Rd,Rn,SHIFT_ASR_i(Rm,i))
#define CC_ORR_rrrASRr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_ORR,0,Rd,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_ORR_rrrRORi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_ORR,0,Rd,Rn,SHIFT_ROR_i(Rm,i))
#define CC_ORR_rrrRORr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_ORR,0,Rd,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_ORR_rrrRRX(cc,Rd,Rn,Rm)     		_OP3(cc,_ORR,0,Rd,Rn,SHIFT_RRX(Rm))

/* ORR Rd,Rn,#i */
#define ORR_rri8(Rd,Rn,i)          			CC_ORR_rri8(NATIVE_CC_AL,Rd,Rn,i)
/* ORR Rd,Rn,#i ROR #s */
#define ORR_rri8RORi(Rd,Rn,i,s)      		CC_ORR_rri8RORi(NATIVE_CC_AL,Rd,Rn,i,s)

#define ORR_rri(Rd,Rn,i)                	CC_ORR_rri(NATIVE_CC_AL,Rd,Rn,i)
#define ORR_rrr(Rd,Rn,Rm)               	CC_ORR_rrr(NATIVE_CC_AL,Rd,Rn,Rm)
#define ORR_rrrLSLi(Rd,Rn,Rm,i)         	CC_ORR_rrrLSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ORR_rrrLSLr(Rd,Rn,Rm,Rs)        	CC_ORR_rrrLSLr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ORR_rrrLSRi(Rd,Rn,Rm,i)         	CC_ORR_rrrLSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ORR_rrrLSRr(Rd,Rn,Rm,Rs)        	CC_ORR_rrrLSRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ORR_rrrASRi(Rd,Rn,Rm,i)         	CC_ORR_rrrASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ORR_rrrASRr(Rd,Rn,Rm,Rs)        	CC_ORR_rrrASRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ORR_rrrRORi(Rd,Rn,Rm,i)         	CC_ORR_rrrRORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ORR_rrrRORr(Rd,Rn,Rm,Rs)        	CC_ORR_rrrRORr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ORR_rrrRRX(Rd,Rn,Rm)            	CC_ORR_rrrRRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_ORRS_rri(cc,Rd,Rn,i)         	_OP3(cc,_ORR,1,Rd,Rn,SHIFT_IMM(i))
#define CC_ORRS_rrr(cc,Rd,Rn,Rm)        	_OP3(cc,_ORR,1,Rd,Rn,SHIFT_REG(Rm))
#define CC_ORRS_rrrLSLi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_ORR,1,Rd,Rn,SHIFT_LSL_i(Rm,i))
#define CC_ORRS_rrrLSLr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_ORR,1,Rd,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_ORRS_rrrLSRi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_ORR,1,Rd,Rn,SHIFT_LSR_i(Rm,i))
#define CC_ORRS_rrrLSRr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_ORR,1,Rd,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_ORRS_rrrASRi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_ORR,1,Rd,Rn,SHIFT_ASR_i(Rm,i))
#define CC_ORRS_rrrASRr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_ORR,1,Rd,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_ORRS_rrrRORi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_ORR,1,Rd,Rn,SHIFT_ROR_i(Rm,i))
#define CC_ORRS_rrrRORr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_ORR,1,Rd,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_ORRS_rrrRRX(cc,Rd,Rn,Rm)  	  	_OP3(cc,_ORR,1,Rd,Rn,SHIFT_RRX(Rm))

#define ORRS_rri(Rd,Rn,i)               	CC_ORRS_rri(NATIVE_CC_AL,Rd,Rn,i)
#define ORRS_rrr(Rd,Rn,Rm)              	CC_ORRS_rrr(NATIVE_CC_AL,Rd,Rn,Rm)
#define ORRS_rrrLSLi(Rd,Rn,Rm,i)        	CC_ORRS_rrrLSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ORRS_rrrLSLr(Rd,Rn,Rm,Rs)       	CC_ORRS_rrrLSLr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ORRS_rrrLSRi(Rd,Rn,Rm,i)        	CC_ORRS_rrrLSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ORRS_rrrLSRr(Rd,Rn,Rm,Rs)       	CC_ORRS_rrrLSRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ORRS_rrrASRi(Rd,Rn,Rm,i)        	CC_ORRS_rrrASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ORRS_rrrASRr(Rd,Rn,Rm,Rs)       	CC_ORRS_rrrASRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ORRS_rrrRORi(Rd,Rn,Rm,i)        	CC_ORRS_rrrRORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define ORRS_rrrRORr(Rd,Rn,Rm,Rs)       	CC_ORRS_rrrRORr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define ORRS_rrrRRX(Rd,Rn,Rm)           	CC_ORRS_rrrRRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_BIC_rri(cc,Rd,Rn,i)          	_OP3(cc,_BIC,0,Rd,Rn,SHIFT_IMM(i))
#define CC_BIC_rrr(cc,Rd,Rn,Rm)         	_OP3(cc,_BIC,0,Rd,Rn,SHIFT_REG(Rm))
#define CC_BIC_rrrLSLi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_BIC,0,Rd,Rn,SHIFT_LSL_i(Rm,i))
#define CC_BIC_rrrLSLr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_BIC,0,Rd,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_BIC_rrrLSRi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_BIC,0,Rd,Rn,SHIFT_LSR_i(Rm,i))
#define CC_BIC_rrrLSRr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_BIC,0,Rd,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_BIC_rrrASRi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_BIC,0,Rd,Rn,SHIFT_ASR_i(Rm,i))
#define CC_BIC_rrrASRr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_BIC,0,Rd,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_BIC_rrrRORi(cc,Rd,Rn,Rm,i)   	_OP3(cc,_BIC,0,Rd,Rn,SHIFT_ROR_i(Rm,i))
#define CC_BIC_rrrRORr(cc,Rd,Rn,Rm,Rs)  	_OP3(cc,_BIC,0,Rd,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_BIC_rrrRRX(cc,Rd,Rn,Rm)     		_OP3(cc,_BIC,0,Rd,Rn,SHIFT_RRX(Rm))

#define BIC_rri(Rd,Rn,i)                	CC_BIC_rri(NATIVE_CC_AL,Rd,Rn,i)
#define BIC_rrr(Rd,Rn,Rm)               	CC_BIC_rrr(NATIVE_CC_AL,Rd,Rn,Rm)
#define BIC_rrrLSLi(Rd,Rn,Rm,i)         	CC_BIC_rrrLSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define BIC_rrrLSLr(Rd,Rn,Rm,Rs)        	CC_BIC_rrrLSLr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define BIC_rrrLSRi(Rd,Rn,Rm,i)         	CC_BIC_rrrLSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define BIC_rrrLSRr(Rd,Rn,Rm,Rs)        	CC_BIC_rrrLSRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define BIC_rrrASRi(Rd,Rn,Rm,i)         	CC_BIC_rrrASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define BIC_rrrASRr(Rd,Rn,Rm,Rs)        	CC_BIC_rrrASRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define BIC_rrrRORi(Rd,Rn,Rm,i)         	CC_BIC_rrrRORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define BIC_rrrRORr(Rd,Rn,Rm,Rs)        	CC_BIC_rrrRORr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define BIC_rrrRRX(Rd,Rn,Rm)            	CC_BIC_rrrRRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_BICS_rri(cc,Rd,Rn,i)         	_OP3(cc,_BIC,1,Rd,Rn,SHIFT_IMM(i))
#define CC_BICS_rrr(cc,Rd,Rn,Rm)        	_OP3(cc,_BIC,1,Rd,Rn,SHIFT_REG(Rm))
#define CC_BICS_rrrLSLi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_BIC,1,Rd,Rn,SHIFT_LSL_i(Rm,i))
#define CC_BICS_rrrLSLr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_BIC,1,Rd,Rn,SHIFT_LSL_r(Rm,Rs))
#define CC_BICS_rrrLSRi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_BIC,1,Rd,Rn,SHIFT_LSR_i(Rm,i))
#define CC_BICS_rrrLSRr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_BIC,1,Rd,Rn,SHIFT_LSR_r(Rm,Rs))
#define CC_BICS_rrrASRi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_BIC,1,Rd,Rn,SHIFT_ASR_i(Rm,i))
#define CC_BICS_rrrASRr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_BIC,1,Rd,Rn,SHIFT_ASR_r(Rm,Rs))
#define CC_BICS_rrrRORi(cc,Rd,Rn,Rm,i)  	_OP3(cc,_BIC,1,Rd,Rn,SHIFT_ROR_i(Rm,i))
#define CC_BICS_rrrRORr(cc,Rd,Rn,Rm,Rs) 	_OP3(cc,_BIC,1,Rd,Rn,SHIFT_ROR_r(Rm,Rs))
#define CC_BICS_rrrRRX(cc,Rd,Rn,Rm)	    	_OP3(cc,_BIC,1,Rd,Rn,SHIFT_RRX(Rm))

#define BICS_rri(Rd,Rn,i)               	CC_BICS_rri(NATIVE_CC_AL,Rd,Rn,i)
#define BICS_rrr(Rd,Rn,Rm)              	CC_BICS_rrr(NATIVE_CC_AL,Rd,Rn,Rm)
#define BICS_rrrLSLi(Rd,Rn,Rm,i)        	CC_BICS_rrrLSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define BICS_rrrLSLr(Rd,Rn,Rm,Rs)       	CC_BICS_rrrLSLr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define BICS_rrrLSRi(Rd,Rn,Rm,i)        	CC_BICS_rrrLSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define BICS_rrrLSRr(Rd,Rn,Rm,Rs)       	CC_BICS_rrrLSRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define BICS_rrrASRi(Rd,Rn,Rm,i)        	CC_BICS_rrrASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define BICS_rrrASRr(Rd,Rn,Rm,Rs)       	CC_BICS_rrrASRr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define BICS_rrrRORi(Rd,Rn,Rm,i)        	CC_BICS_rrrRORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define BICS_rrrRORr(Rd,Rn,Rm,Rs)       	CC_BICS_rrrRORr(NATIVE_CC_AL,Rd,Rn,Rm,Rs)
#define BICS_rrrRRX(Rd,Rn,Rm)           	CC_BICS_rrrRRX(NATIVE_CC_AL,Rd,Rn,Rm)

/* Branch instructions */
#define CC_B_i(cc,i)                        _W(((cc) << 28) | (10 << 24) | (i))
#define CC_BL_i(cc,i)                       _W(((cc) << 28) | (11 << 24) | (i))
#define CC_BLX_r(cc,r)                      _W(((cc) << 28) | (0x12 << 20) | (3 << 4) | (0xfff << 8) | (r))
#define CC_BX_r(cc,r)                       _W(((cc) << 28) | (0x12 << 20) | (1 << 4) | (0xfff << 8) | (r))
#define CC_BXJ_r(cc,r)                      _W(((cc) << 28) | (0x12 << 20) | (2 << 4) | (0xfff << 8) | (r))

#define BEQ_i(i)                            CC_B_i(NATIVE_CC_EQ,i)
#define BNE_i(i)                            CC_B_i(NATIVE_CC_NE,i)
#define BCS_i(i)                            CC_B_i(NATIVE_CC_CS,i)
#define BCC_i(i)                            CC_B_i(NATIVE_CC_CC,i)
#define BMI_i(i)                            CC_B_i(NATIVE_CC_MI,i)
#define BPL_i(i)                            CC_B_i(NATIVE_CC_PL,i)
#define BVS_i(i)                            CC_B_i(NATIVE_CC_VS,i)
#define BVC_i(i)                            CC_B_i(NATIVE_CC_VC,i)
#define BHI_i(i)                            CC_B_i(NATIVE_CC_HI,i)
#define BLS_i(i)                            CC_B_i(NATIVE_CC_LS,i)
#define BGE_i(i)                            CC_B_i(NATIVE_CC_GE,i)
#define BLT_i(i)                            CC_B_i(NATIVE_CC_LT,i)
#define BGT_i(i)                            CC_B_i(NATIVE_CC_GT,i)
#define BLE_i(i)                            CC_B_i(NATIVE_CC_LE,i)
#define B_i(i)                              CC_B_i(NATIVE_CC_AL,i)

#define BL_i(i)                             CC_BL_i(NATIVE_CC_AL,i)
#define BLX_i(i)                            _W((NATIVE_CC_AL << 28) | (10 << 24) | (i))
#define BLX_r(r)                            CC_BLX_r(NATIVE_CC_AL,r)
#define BX_r(r)                             CC_BX_r(NATIVE_CC_AL,r)
#define BXJ_r(r)                            CC_BXJ_r(NATIVE_CC_AL,r)

/* Status register instructions */
#define CC_MRS_CPSR(cc,Rd)					_W(((cc) << 28) | (0x10 << 20) | ((Rd) << 12) | (0xf << 16))
#define MRS_CPSR(Rd)						CC_MRS_CPSR(NATIVE_CC_AL,Rd)
#define CC_MRS_SPSR(cc,Rd)					_W(((cc) << 28) | (0x14 << 20) | ((Rd) << 12) | (0xf << 16))
#define MRS_SPSR(Rd)						CC_MRS_SPSR(NATIVE_CC_AL,Rd)

#define CC_MSR_CPSR_i(cc,i)					_W(((cc) << 28) | (0x32 << 20) | (0x9 << 16) | (0xf << 12) | SHIFT_IMM(i))
#define CC_MSR_CPSR_r(cc,Rm)				_W(((cc) << 28) | (0x12 << 20) | (0x9 << 16) | (0xf << 12) | (Rm))

#define MSR_CPSR_i(i)						CC_MSR_CPSR_i(NATIVE_CC_AL,(i))
#define MSR_CPSR_r(Rm)						CC_MSR_CPSR_r(NATIVE_CC_AL,(Rm))

#define CC_MSR_CPSRf_i(cc,i)				_W(((cc) << 28) | (0x32 << 20) | (0x8 << 16) | (0xf << 12) | SHIFT_IMM(i))
#define CC_MSR_CPSRf_r(cc,Rm)				_W(((cc) << 28) | (0x12 << 20) | (0x8 << 16) | (0xf << 12) | (Rm))

#define MSR_CPSRf_i(i)						CC_MSR_CPSRf_i(NATIVE_CC_AL,(i))
#define MSR_CPSRf_r(Rm)						CC_MSR_CPSRf_r(NATIVE_CC_AL,(Rm))

#define CC_MSR_CPSRc_i(cc,i)				_W(((cc) << 28) | (0x32 << 20) | (0x1 << 16) | (0xf << 12) | SHIFT_IMM(i))
#define CC_MSR_CPSRc_r(cc,Rm)				_W(((cc) << 28) | (0x12 << 20) | (0x1 << 16) | (0xf << 12) | (Rm))

#define MSR_CPSRc_i(i)						CC_MSR_CPSRc_i(NATIVE_CC_AL,(i))
#define MSR_CPSRc_r(Rm)						CC_MSR_CPSRc_r(NATIVE_CC_AL,(Rm))

/* Load Store instructions */

#define CC_PUSH(cc,r)						_W(((cc) << 28) | (0x92d << 16) | (1 << (r)))
#define PUSH(r)								CC_PUSH(NATIVE_CC_AL, r)

#define CC_PUSH_REGS(cc,r)					_W(((cc) << 28) | (0x92d << 16) | (r))
#define PUSH_REGS(r)						CC_PUSH_REGS(NATIVE_CC_AL, r)

#define CC_POP(cc,r)						_W(((cc) << 28) | (0x8bd << 16) | (1 << (r)))
#define POP(r)								CC_POP(NATIVE_CC_AL, r)

#define CC_POP_REGS(cc,r)					_W(((cc) << 28) | (0x8bd << 16) | (r))
#define POP_REGS(r)							CC_POP_REGS(NATIVE_CC_AL, r)

#define CC_LDR_rR(cc,Rd,Rn)					_LS1(cc,1,0,Rd,Rn,ADD_IMM(0))
#define CC_LDR_rRI(cc,Rd,Rn,i)				_LS1(cc,1,0,Rd,Rn,(i) >= 0 ? ADD_IMM(i) : SUB_IMM(-(i)))
#define CC_LDR_rRi(cc,Rd,Rn,i)				_LS1(cc,1,0,Rd,Rn,SUB_IMM(i))
#define CC_LDR_rRR(cc,Rd,Rn,Rm)				_LS1(cc,1,0,Rd,Rn,ADD_REG(Rm))
#define CC_LDR_rRr(cc,Rd,Rn,Rm)				_LS1(cc,1,0,Rd,Rn,SUB_REG(Rm))
#define CC_LDR_rRR_LSLi(cc,Rd,Rn,Rm,i)		_LS1(cc,1,0,Rd,Rn,ADD_LSL(Rm,i))
#define CC_LDR_rRr_LSLi(cc,Rd,Rn,Rm,i)		_LS1(cc,1,0,Rd,Rn,SUB_LSL(Rm,i))
#define CC_LDR_rRR_LSRi(cc,Rd,Rn,Rm,i)		_LS1(cc,1,0,Rd,Rn,ADD_LSR(Rm,i))
#define CC_LDR_rRr_LSRi(cc,Rd,Rn,Rm,i)		_LS1(cc,1,0,Rd,Rn,SUB_LSR(Rm,i))
#define CC_LDR_rRR_ASRi(cc,Rd,Rn,Rm,i)		_LS1(cc,1,0,Rd,Rn,ADD_ASR(Rm,i))
#define CC_LDR_rRr_ASRi(cc,Rd,Rn,Rm,i)		_LS1(cc,1,0,Rd,Rn,SUB_ASR(Rm,i))
#define CC_LDR_rRR_RORi(cc,Rd,Rn,Rm,i)		_LS1(cc,1,0,Rd,Rn,ADD_ROR(Rm,i))
#define CC_LDR_rRr_RORi(cc,Rd,Rn,Rm,i)		_LS1(cc,1,0,Rd,Rn,SUB_ROR(Rm,i))
#define CC_LDR_rRR_RRX(cc,Rd,Rn,Rm)			_LS1(cc,1,0,Rd,Rn,ADD_RRX(Rm))
#define CC_LDR_rRr_RRX(cc,Rd,Rn,Rm)			_LS1(cc,1,0,Rd,Rn,SUB_RRX(Rm))

#define LDR_rR(Rd,Rn)						CC_LDR_rR(NATIVE_CC_AL,Rd,Rn)
#define LDR_rRI(Rd,Rn,i)					CC_LDR_rRI(NATIVE_CC_AL,Rd,Rn,i)
#define LDR_rRi(Rd,Rn,i)					CC_LDR_rRi(NATIVE_CC_AL,Rd,Rn,i)
#define LDR_rRR(Rd,Rn,Rm)					CC_LDR_rRR(NATIVE_CC_AL,Rd,Rn,Rm)
#define LDR_rRr(Rd,Rn,Rm)					CC_LDR_rRr(NATIVE_CC_AL,Rd,Rn,Rm)
#define LDR_rRR_LSLi(Rd,Rn,Rm,i)			CC_LDR_rRR_LSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define LDR_rRr_LSLi(Rd,Rn,Rm,i)			CC_LDR_rRr_LSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define LDR_rRR_LSRi(Rd,Rn,Rm,i)			CC_LDR_rRR_LSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define LDR_rRr_LSRi(Rd,Rn,Rm,i)			CC_LDR_rRr_LSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define LDR_rRR_ASRi(Rd,Rn,Rm,i)			CC_LDR_rRR_ASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define LDR_rRr_ASRi(Rd,Rn,Rm,i)			CC_LDR_rRr_ASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define LDR_rRR_RORi(Rd,Rn,Rm,i)			CC_LDR_rRR_RORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define LDR_rRr_RORi(Rd,Rn,Rm,i)			CC_LDR_rRr_RORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define LDR_rRR_RRX(Rd,Rn,Rm)				CC_LDR_rRR_RRX(NATIVE_CC_AL,Rd,Rn,Rm)
#define LDR_rRr_RRX(Rd,Rn,Rm)				CC_LDR_rRr_RRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_STR_rR(cc,Rd,Rn)					_LS1(cc,0,0,Rd,Rn,ADD_IMM(0))
#define CC_STR_rRI(cc,Rd,Rn,i)				_LS1(cc,0,0,Rd,Rn,ADD_IMM(i))
#define CC_STR_rRi(cc,Rd,Rn,i)				_LS1(cc,0,0,Rd,Rn,SUB_IMM(i))
#define CC_STR_rRR(cc,Rd,Rn,Rm)				_LS1(cc,0,0,Rd,Rn,ADD_REG(Rm))
#define CC_STR_rRr(cc,Rd,Rn,Rm)				_LS1(cc,0,0,Rd,Rn,SUB_REG(Rm))
#define CC_STR_rRR_LSLi(cc,Rd,Rn,Rm,i)		_LS1(cc,0,0,Rd,Rn,ADD_LSL(Rm,i))
#define CC_STR_rRr_LSLi(cc,Rd,Rn,Rm,i)		_LS1(cc,0,0,Rd,Rn,SUB_LSL(Rm,i))
#define CC_STR_rRR_LSRi(cc,Rd,Rn,Rm,i)		_LS1(cc,0,0,Rd,Rn,ADD_LSR(Rm,i))
#define CC_STR_rRr_LSRi(cc,Rd,Rn,Rm,i)		_LS1(cc,0,0,Rd,Rn,SUB_LSR(Rm,i))
#define CC_STR_rRR_ASRi(cc,Rd,Rn,Rm,i)		_LS1(cc,0,0,Rd,Rn,ADD_ASR(Rm,i))
#define CC_STR_rRr_ASRi(cc,Rd,Rn,Rm,i)		_LS1(cc,0,0,Rd,Rn,SUB_ASR(Rm,i))
#define CC_STR_rRR_RORi(cc,Rd,Rn,Rm,i)		_LS1(cc,0,0,Rd,Rn,ADD_ROR(Rm,i))
#define CC_STR_rRr_RORi(cc,Rd,Rn,Rm,i)		_LS1(cc,0,0,Rd,Rn,SUB_ROR(Rm,i))
#define CC_STR_rRR_RRX(cc,Rd,Rn,Rm)			_LS1(cc,0,0,Rd,Rn,ADD_RRX(Rm))
#define CC_STR_rRr_RRX(cc,Rd,Rn,Rm)			_LS1(cc,0,0,Rd,Rn,SUB_RRX(Rm))

#define STR_rR(Rd,Rn)						CC_STR_rR(NATIVE_CC_AL,Rd,Rn)
#define STR_rRI(Rd,Rn,i)					CC_STR_rRI(NATIVE_CC_AL,Rd,Rn,i)
#define STR_rRi(Rd,Rn,i)					CC_STR_rRi(NATIVE_CC_AL,Rd,Rn,i)
#define STR_rRR(Rd,Rn,Rm)					CC_STR_rRR(NATIVE_CC_AL,Rd,Rn,Rm)
#define STR_rRr(Rd,Rn,Rm)					CC_STR_rRr(NATIVE_CC_AL,Rd,Rn,Rm)
#define STR_rRR_LSLi(Rd,Rn,Rm,i)			CC_STR_rRR_LSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define STR_rRr_LSLi(Rd,Rn,Rm,i)			CC_STR_rRr_LSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define STR_rRR_LSRi(Rd,Rn,Rm,i)			CC_STR_rRR_LSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define STR_rRr_LSRi(Rd,Rn,Rm,i)			CC_STR_rRr_LSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define STR_rRR_ASRi(Rd,Rn,Rm,i)			CC_STR_rRR_ASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define STR_rRr_ASRi(Rd,Rn,Rm,i)			CC_STR_rRr_ASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define STR_rRR_RORi(Rd,Rn,Rm,i)			CC_STR_rRR_RORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define STR_rRr_RORi(Rd,Rn,Rm,i)			CC_STR_rRr_RORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define STR_rRR_RRX(Rd,Rn,Rm)				CC_STR_rRR_RRX(NATIVE_CC_AL,Rd,Rn,Rm)
#define STR_rRr_RRX(Rd,Rn,Rm)				CC_STR_rRr_RRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_LDRB_rR(cc,Rd,Rn)				_LS1(cc,1,1,Rd,Rn,ADD_IMM(0))
#define CC_LDRB_rRI(cc,Rd,Rn,i)				_LS1(cc,1,1,Rd,Rn,ADD_IMM(i))
#define CC_LDRB_rRi(cc,Rd,Rn,i)				_LS1(cc,1,1,Rd,Rn,SUB_IMM(i))
#define CC_LDRB_rRR(cc,Rd,Rn,Rm)			_LS1(cc,1,1,Rd,Rn,ADD_REG(Rm))
#define CC_LDRB_rRr(cc,Rd,Rn,Rm)			_LS1(cc,1,1,Rd,Rn,SUB_REG(Rm))
#define CC_LDRB_rRR_LSLi(cc,Rd,Rn,Rm,i)		_LS1(cc,1,1,Rd,Rn,ADD_LSL(Rm,i))
#define CC_LDRB_rRr_LSLi(cc,Rd,Rn,Rm,i)		_LS1(cc,1,1,Rd,Rn,SUB_LSL(Rm,i))
#define CC_LDRB_rRR_LSRi(cc,Rd,Rn,Rm,i)		_LS1(cc,1,1,Rd,Rn,ADD_LSR(Rm,i))
#define CC_LDRB_rRr_LSRi(cc,Rd,Rn,Rm,i)		_LS1(cc,1,1,Rd,Rn,SUB_LSR(Rm,i))
#define CC_LDRB_rRR_ASRi(cc,Rd,Rn,Rm,i)		_LS1(cc,1,1,Rd,Rn,ADD_ASR(Rm,i))
#define CC_LDRB_rRr_ASRi(cc,Rd,Rn,Rm,i)		_LS1(cc,1,1,Rd,Rn,SUB_ASR(Rm,i))
#define CC_LDRB_rRR_RORi(cc,Rd,Rn,Rm,i)		_LS1(cc,1,1,Rd,Rn,ADD_ROR(Rm,i))
#define CC_LDRB_rRr_RORi(cc,Rd,Rn,Rm,i)		_LS1(cc,1,1,Rd,Rn,SUB_ROR(Rm,i))
#define CC_LDRB_rRR_RRX(cc,Rd,Rn,Rm)		_LS1(cc,1,1,Rd,Rn,ADD_RRX(Rm))
#define CC_LDRB_rRr_RRX(cc,Rd,Rn,Rm)		_LS1(cc,1,1,Rd,Rn,SUB_RRX(Rm))

#define LDRB_rR(Rd,Rn)						CC_LDRB_rR(NATIVE_CC_AL,Rd,Rn)
#define LDRB_rRI(Rd,Rn,i)					CC_LDRB_rRI(NATIVE_CC_AL,Rd,Rn,i)
#define LDRB_rRi(Rd,Rn,i)					CC_LDRB_rRi(NATIVE_CC_AL,Rd,Rn,i)
#define LDRB_rRR(Rd,Rn,Rm)					CC_LDRB_rRR(NATIVE_CC_AL,Rd,Rn,Rm)
#define LDRB_rRr(Rd,Rn,Rm)					CC_LDRB_rRr(NATIVE_CC_AL,Rd,Rn,Rm)
#define LDRB_rRR_LSLi(Rd,Rn,Rm,i)			CC_LDRB_rRR_LSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define LDRB_rRr_LSLi(Rd,Rn,Rm,i)			CC_LDRB_rRr_LSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define LDRB_rRR_LSRi(Rd,Rn,Rm,i)			CC_LDRB_rRR_LSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define LDRB_rRr_LSRi(Rd,Rn,Rm,i)			CC_LDRB_rRr_LSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define LDRB_rRR_ASRi(Rd,Rn,Rm,i)			CC_LDRB_rRR_ASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define LDRB_rRr_ASRi(Rd,Rn,Rm,i)			CC_LDRB_rRr_ASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define LDRB_rRR_RORi(Rd,Rn,Rm,i)			CC_LDRB_rRR_RORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define LDRB_rRr_RORi(Rd,Rn,Rm,i)			CC_LDRB_rRr_RORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define LDRB_rRR_RRX(Rd,Rn,Rm)				CC_LDRB_rRR_RRX(NATIVE_CC_AL,Rd,Rn,Rm)
#define LDRB_rRr_RRX(Rd,Rn,Rm)				CC_LDRB_rRr_RRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_STRB_rR(cc,Rd,Rn)				_LS1(cc,0,1,Rd,Rn,ADD_IMM(0))
#define CC_STRB_rRI(cc,Rd,Rn,i)				_LS1(cc,0,1,Rd,Rn,ADD_IMM(i))
#define CC_STRB_rRi(cc,Rd,Rn,i)				_LS1(cc,0,1,Rd,Rn,SUB_IMM(i))
#define CC_STRB_rRR(cc,Rd,Rn,Rm)			_LS1(cc,0,1,Rd,Rn,ADD_REG(Rm))
#define CC_STRB_rRr(cc,Rd,Rn,Rm)			_LS1(cc,0,1,Rd,Rn,SUB_REG(Rm))
#define CC_STRB_rRR_LSLi(cc,Rd,Rn,Rm,i)		_LS1(cc,0,1,Rd,Rn,ADD_LSL(Rm,i))
#define CC_STRB_rRr_LSLi(cc,Rd,Rn,Rm,i)		_LS1(cc,0,1,Rd,Rn,SUB_LSL(Rm,i))
#define CC_STRB_rRR_LSRi(cc,Rd,Rn,Rm,i)		_LS1(cc,0,1,Rd,Rn,ADD_LSR(Rm,i))
#define CC_STRB_rRr_LSRi(cc,Rd,Rn,Rm,i)		_LS1(cc,0,1,Rd,Rn,SUB_LSR(Rm,i))
#define CC_STRB_rRR_ASRi(cc,Rd,Rn,Rm,i)		_LS1(cc,0,1,Rd,Rn,ADD_ASR(Rm,i))
#define CC_STRB_rRr_ASRi(cc,Rd,Rn,Rm,i)		_LS1(cc,0,1,Rd,Rn,SUB_ASR(Rm,i))
#define CC_STRB_rRR_RORi(cc,Rd,Rn,Rm,i)		_LS1(cc,0,1,Rd,Rn,ADD_ROR(Rm,i))
#define CC_STRB_rRr_RORi(cc,Rd,Rn,Rm,i)		_LS1(cc,0,1,Rd,Rn,SUB_ROR(Rm,i))
#define CC_STRB_rRR_RRX(cc,Rd,Rn,Rm)		_LS1(cc,0,1,Rd,Rn,ADD_RRX(Rm))
#define CC_STRB_rRr_RRX(cc,Rd,Rn,Rm)		_LS1(cc,0,1,Rd,Rn,SUB_RRX(Rm))

#define STRB_rR(Rd,Rn)						CC_STRB_rR(NATIVE_CC_AL,Rd,Rn)
#define STRB_rRI(Rd,Rn,i)					CC_STRB_rRI(NATIVE_CC_AL,Rd,Rn,i)
#define STRB_rRi(Rd,Rn,i)					CC_STRB_rRi(NATIVE_CC_AL,Rd,Rn,i)
#define STRB_rRR(Rd,Rn,Rm)					CC_STRB_rRR(NATIVE_CC_AL,Rd,Rn,Rm)
#define STRB_rRr(Rd,Rn,Rm)					CC_STRB_rRr(NATIVE_CC_AL,Rd,Rn,Rm)
#define STRB_rRR_LSLi(Rd,Rn,Rm,i)			CC_STRB_rRR_LSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define STRB_rRr_LSLi(Rd,Rn,Rm,i)			CC_STRB_rRr_LSLi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define STRB_rRR_LSRi(Rd,Rn,Rm,i)			CC_STRB_rRR_LSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define STRB_rRr_LSRi(Rd,Rn,Rm,i)			CC_STRB_rRr_LSRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define STRB_rRR_ASRi(Rd,Rn,Rm,i)			CC_STRB_rRR_ASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define STRB_rRr_ASRi(Rd,Rn,Rm,i)			CC_STRB_rRr_ASRi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define STRB_rRR_RORi(Rd,Rn,Rm,i)			CC_STRB_rRR_RORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define STRB_rRr_RORi(Rd,Rn,Rm,i)			CC_STRB_rRr_RORi(NATIVE_CC_AL,Rd,Rn,Rm,i)
#define STRB_rRR_RRX(Rd,Rn,Rm)				CC_STRB_rRR_RRX(NATIVE_CC_AL,Rd,Rn,Rm)
#define STRB_rRr_RRX(Rd,Rn,Rm)				CC_STRB_rRr_RRX(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_LDRSH_rR(cc,Rd,Rn)                   _LS2(cc,1,1,1,1,Rd,Rn,ADD2_IMM(0))
#define CC_LDRSH_rRI(cc,Rd,Rn,i)                _LS2(cc,1,1,1,1,Rd,Rn,ADD2_IMM(i))
#define CC_LDRSH_rRi(cc,Rd,Rn,i)                _LS2(cc,1,1,1,1,Rd,Rn,SUB2_IMM(i))
#define CC_LDRSH_rRR(cc,Rd,Rn,Rm)               _LS2(cc,1,1,1,1,Rd,Rn,ADD2_REG(Rm))
#define CC_LDRSH_rRr(cc,Rd,Rn,Rm)               _LS2(cc,1,1,1,1,Rd,Rn,SUB2_REG(Rm))

#define LDRSH_rR(Rd,Rn)                         CC_LDRSH_rR(NATIVE_CC_AL,Rd,Rn)
#define LDRSH_rRI(Rd,Rn,i)                      CC_LDRSH_rRI(NATIVE_CC_AL,Rd,Rn,i)
#define LDRSH_rRi(Rd,Rn,i)                      CC_LDRSH_rRi(NATIVE_CC_AL,Rd,Rn,i)
#define LDRSH_rRR(Rd,Rn,Rm)                     CC_LDRSH_rRR(NATIVE_CC_AL,Rd,Rn,Rm)
#define LDRSH_rRr(Rd,Rn,Rm)                     CC_LDRSH_rRr(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_LDRH_rR(cc,Rd,Rn)                    _LS2(cc,1,1,0,1,Rd,Rn,ADD2_IMM(0))
#define CC_LDRH_rRI(cc,Rd,Rn,i)                 _LS2(cc,1,1,0,1,Rd,Rn,(i) >= 0 ? ADD2_IMM(i) : SUB2_IMM(-(i)))
#define CC_LDRH_rRi(cc,Rd,Rn,i)                 _LS2(cc,1,1,0,1,Rd,Rn,SUB2_IMM(i))
#define CC_LDRH_rRR(cc,Rd,Rn,Rm)                _LS2(cc,1,1,0,1,Rd,Rn,ADD2_REG(Rm))
#define CC_LDRH_rRr(cc,Rd,Rn,Rm)                _LS2(cc,1,1,0,1,Rd,Rn,SUB2_REG(Rm))

#define LDRH_rR(Rd,Rn)                          CC_LDRH_rR(NATIVE_CC_AL,Rd,Rn)
#define LDRH_rRI(Rd,Rn,i)                       CC_LDRH_rRI(NATIVE_CC_AL,Rd,Rn,i)
#define LDRH_rRi(Rd,Rn,i)                       CC_LDRH_rRi(NATIVE_CC_AL,Rd,Rn,i)
#define LDRH_rRR(Rd,Rn,Rm)                      CC_LDRH_rRR(NATIVE_CC_AL,Rd,Rn,Rm)
#define LDRH_rRr(Rd,Rn,Rm)                      CC_LDRH_rRr(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_STRD_rR(cc,Rd,Rn)                   _LS2(cc,1,0,1,1,Rd,Rn,ADD2_IMM(0))
#define CC_STRD_rRI(cc,Rd,Rn,i)                _LS2(cc,1,0,1,1,Rd,Rn,ADD2_IMM(i))
#define CC_STRD_rRi(cc,Rd,Rn,i)                _LS2(cc,1,0,1,1,Rd,Rn,SUB2_IMM(i))
#define CC_STRD_rRR(cc,Rd,Rn,Rm)               _LS2(cc,1,0,1,1,Rd,Rn,ADD2_REG(Rm))
#define CC_STRD_rRr(cc,Rd,Rn,Rm)               _LS2(cc,1,0,1,1,Rd,Rn,SUB2_REG(Rm))

#define STRD_rR(Rd,Rn)                         CC_STRD_rR(NATIVE_CC_AL,Rd,Rn)
#define STRD_rRI(Rd,Rn,i)                      CC_STRD_rRI(NATIVE_CC_AL,Rd,Rn,i)
#define STRD_rRi(Rd,Rn,i)                      CC_STRD_rRi(NATIVE_CC_AL,Rd,Rn,i)
#define STRD_rRR(Rd,Rn,Rm)                     CC_STRD_rRR(NATIVE_CC_AL,Rd,Rn,Rm)
#define STRD_rRr(Rd,Rn,Rm)                     CC_STRD_rRr(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_STRH_rR(cc,Rd,Rn)                    _LS2(cc,1,0,0,1,Rd,Rn,ADD2_IMM(0))
#define CC_STRH_rRI(cc,Rd,Rn,i)                 _LS2(cc,1,0,0,1,Rd,Rn,ADD2_IMM(i))
#define CC_STRH_rRi(cc,Rd,Rn,i)                 _LS2(cc,1,0,0,1,Rd,Rn,SUB2_IMM(i))
#define CC_STRH_rRR(cc,Rd,Rn,Rm)                _LS2(cc,1,0,0,1,Rd,Rn,ADD2_REG(Rm))
#define CC_STRH_rRr(cc,Rd,Rn,Rm)                _LS2(cc,1,0,0,1,Rd,Rn,SUB2_REG(Rm))

#define STRH_rR(Rd,Rn)                          CC_STRH_rR(NATIVE_CC_AL,Rd,Rn)
#define STRH_rRI(Rd,Rn,i)                       CC_STRH_rRI(NATIVE_CC_AL,Rd,Rn,i)
#define STRH_rRi(Rd,Rn,i)                       CC_STRH_rRi(NATIVE_CC_AL,Rd,Rn,i)
#define STRH_rRR(Rd,Rn,Rm)                      CC_STRH_rRR(NATIVE_CC_AL,Rd,Rn,Rm)
#define STRH_rRr(Rd,Rn,Rm)                      CC_STRH_rRr(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_LDRSB_rR(cc,Rd,Rn)                   _LS2(cc,1,1,1,0,Rd,Rn,ADD2_IMM(0))
#define CC_LDRSB_rRI(cc,Rd,Rn,i)                _LS2(cc,1,1,1,0,Rd,Rn,ADD2_IMM(i))
#define CC_LDRSB_rRi(cc,Rd,Rn,i)                _LS2(cc,1,1,1,0,Rd,Rn,SUB2_IMM(i))
#define CC_LDRSB_rRR(cc,Rd,Rn,Rm)               _LS2(cc,1,1,1,0,Rd,Rn,ADD2_REG(Rm))
#define CC_LDRSB_rRr(cc,Rd,Rn,Rm)               _LS2(cc,1,1,1,0,Rd,Rn,SUB2_REG(Rm))

#define LDRSB_rR(Rd,Rn)                         CC_LDRSB_rR(NATIVE_CC_AL,Rd,Rn)
#define LDRSB_rRI(Rd,Rn,i)                      CC_LDRSB_rRI(NATIVE_CC_AL,Rd,Rn,i)
#define LDRSB_rRi(Rd,Rn,i)                      CC_LDRSB_rRi(NATIVE_CC_AL,Rd,Rn,i)
#define LDRSB_rRR(Rd,Rn,Rm)                     CC_LDRSB_rRR(NATIVE_CC_AL,Rd,Rn,Rm)
#define LDRSB_rRr(Rd,Rn,Rm)                     CC_LDRSB_rRr(NATIVE_CC_AL,Rd,Rn,Rm)

#define CC_LDRD_rR(cc,Rd,Rn)                   _LS2(cc,1,0,1,0,Rd,Rn,ADD2_IMM(0))
#define CC_LDRD_rRI(cc,Rd,Rn,i)                _LS2(cc,1,0,1,0,Rd,Rn,ADD2_IMM(i))
#define CC_LDRD_rRi(cc,Rd,Rn,i)                _LS2(cc,1,0,1,0,Rd,Rn,SUB2_IMM(i))
#define CC_LDRD_rRR(cc,Rd,Rn,Rm)               _LS2(cc,1,0,1,0,Rd,Rn,ADD2_REG(Rm))
#define CC_LDRD_rRr(cc,Rd,Rn,Rm)               _LS2(cc,1,0,1,0,Rd,Rn,SUB2_REG(Rm))

#define LDRD_rR(Rd,Rn)                         CC_LDRD_rR(NATIVE_CC_AL,Rd,Rn)
#define LDRD_rRI(Rd,Rn,i)                      CC_LDRD_rRI(NATIVE_CC_AL,Rd,Rn,i)
#define LDRD_rRi(Rd,Rn,i)                      CC_LDRD_rRi(NATIVE_CC_AL,Rd,Rn,i)
#define LDRD_rRR(Rd,Rn,Rm)                     CC_LDRD_rRR(NATIVE_CC_AL,Rd,Rn,Rm)
#define LDRD_rRr(Rd,Rn,Rm)                     CC_LDRD_rRr(NATIVE_CC_AL,Rd,Rn,Rm)

/* Multiply */
#define CC_SMULL_rrrr(cc, RdLo, RdHi, Rm, Rs)	_W(((cc) << 28) | (0x0C << 20) | ((RdHi) << 16) | ((RdLo) << 12) | ((Rs) << 8) | (0x9 << 4) | (Rm))
#define SMULL_rrrr(RdLo,RdHi,Rm,Rs)				CC_SMULL_rrrr(NATIVE_CC_AL,RdLo,RdHi,Rm,Rs)
#define CC_SMULLS_rrrr(cc, RdLo, RdHi, Rm, Rs)	_W(((cc) << 28) | (0x0D << 20) | ((RdHi) << 16) | ((RdLo) << 12) | ((Rs) << 8) | (0x9 << 4) | (Rm))
#define SMULLS_rrrr(RdLo,RdHi,Rm,Rs)			CC_SMULLS_rrrr(NATIVE_CC_AL,RdLo,RdHi,Rm,Rs)
#define CC_MUL_rrr(cc, Rd, Rm, Rs)				_W(((cc) << 28) | (0x00 << 20) | ((Rd) << 16) | ((Rs) << 8) | (0x9 << 4) | (Rm))
#define MUL_rrr(Rd, Rm, Rs)						CC_MUL_rrr(NATIVE_CC_AL, Rd, Rm, Rs)
#define CC_MULS_rrr(cc, Rd, Rm, Rs)				_W(((cc) << 28) | (0x01 << 20) | ((Rd) << 16) | ((Rs) << 8) | (0x9 << 4) | (Rm))
#define MULS_rrr(Rd, Rm, Rs)					CC_MULS_rrr(NATIVE_CC_AL, Rd, Rm, Rs)

#define CC_UMULL_rrrr(cc, RdLo, RdHi, Rm, Rs)	_W(((cc) << 28) | (0x08 << 20) | ((RdHi) << 16) | ((RdLo) << 12) | ((Rs) << 8) | (0x9 << 4) | (Rm))
#define UMULL_rrrr(RdLo,RdHi,Rm,Rs)				CC_UMULL_rrrr(NATIVE_CC_AL,RdLo,RdHi,Rm,Rs)
#define CC_UMULLS_rrrr(cc, RdLo, RdHi, Rm, Rs)	_W(((cc) << 28) | (0x09 << 20) | ((RdHi) << 16) | ((RdLo) << 12) | ((Rs) << 8) | (0x9 << 4) | (Rm))
#define UMULLS_rrrr(RdLo,RdHi,Rm,Rs)			CC_UMULLS_rrrr(NATIVE_CC_AL,RdLo,RdHi,Rm,Rs)

/* Others */
#define CC_CLZ_rr(cc,Rd,Rm)					_W(((cc) << 28) | (0x16 << 20) | (0xf << 16) | ((Rd) << 12) | (0xf << 8) | (0x1 << 4) | SHIFT_REG(Rm))
#define CLZ_rr(Rd,Rm)						CC_CLZ_rr(NATIVE_CC_AL,Rd,Rm)

/* Alias */
#define LSL_rri(Rd,Rm,i)                    MOV_rrLSLi(Rd,Rm,i)
#define LSL_rrr(Rd,Rm,Rs)                	MOV_rrLSLr(Rd,Rm,Rs)
#define LSR_rri(Rd,Rm,i)                    MOV_rrLSRi(Rd,Rm,i)
#define LSR_rrr(Rd,Rm,Rs)                	MOV_rrLSRr(Rd,Rm,Rs)
#define ASR_rri(Rd,Rm,i)                    MOV_rrASRi(Rd,Rm,i)
#define ASR_rrr(Rd,Rm,Rs)                	MOV_rrASRr(Rd,Rm,Rs)
#define ROR_rri(Rd,Rm,i)                    MOV_rrRORi(Rd,Rm,i)
#define ROR_rrr(Rd,Rm,Rs)                	MOV_rrRORr(Rd,Rm,Rs)
#define RRX_rr(Rd,Rm)                       MOV_rrRRX(Rd,Rm)
#define LSLS_rri(Rd,Rm,i)                   MOVS_rrLSLi(Rd,Rm,i)
#define LSLS_rrr(Rd,Rm,Rs)                	MOVS_rrLSLr(Rd,Rm,Rs)
#define LSRS_rri(Rd,Rm,i)                   MOVS_rrLSRi(Rd,Rm,i)
#define LSRS_rrr(Rd,Rm,Rs)                	MOVS_rrLSRr(Rd,Rm,Rs)
#define ASRS_rri(Rd,Rm,i)                   MOVS_rrASRi(Rd,Rm,i)
#define ASRS_rrr(Rd,Rm,Rs)                	MOVS_rrASRr(Rd,Rm,Rs)
#define RORS_rri(Rd,Rm,i)                   MOVS_rrRORi(Rd,Rm,i)
#define RORS_rrr(Rd,Rm,Rs)                	MOVS_rrRORr(Rd,Rm,Rs)
#define RRXS_rr(Rd,Rm)                      MOVS_rrRRX(Rd,Rm)

/* ARMV6 ops */
#define CC_SXTB_rr(cc,Rd,Rm)				_W(((cc) << 28) | (0x6a << 20) | (0xf << 16) | ((Rd) << 12) | (0x7 << 4) | SHIFT_REG(Rm))
#define SXTB_rr(Rd,Rm)						CC_SXTB_rr(NATIVE_CC_AL,Rd,Rm)

#define CC_SXTB_rr_ROR8(cc,Rd,Rm)			_W(((cc) << 28) | (0x6a << 20) | (0xf << 16) | ((Rd) << 12) | (1 << 10) | (0x7 << 4) | SHIFT_REG(Rm))
#define SXTB_rr_ROR8(Rd,Rm)					CC_SXTB_rr_ROR8(NATIVE_CC_AL,Rd,Rm)

#define CC_SXTB_rr_ROR16(cc,Rd,Rm)			_W(((cc) << 28) | (0x6a << 20) | (0xf << 16) | ((Rd) << 12) | (2 << 10) | (0x7 << 4) | SHIFT_REG(Rm))
#define SXTB_rr_ROR16(Rd,Rm)				CC_SXTB_rr_ROR16(NATIVE_CC_AL,Rd,Rm)

#define CC_SXTB_rr_ROR24(cc,Rd,Rm)			_W(((cc) << 28) | (0x6a << 20) | (0xf << 16) | ((Rd) << 12) | (3 << 10) | (0x7 << 4) | SHIFT_REG(Rm))
#define SXTB_rr_ROR24(Rd,Rm)				CC_SXTB_rr_ROR24(NATIVE_CC_AL,Rd,Rm)

#define CC_SXTH_rr(cc,Rd,Rm)				_W(((cc) << 28) | (0x6b << 20) | (0xf << 16) | ((Rd) << 12) | (0x7 << 4) | SHIFT_REG(Rm))
#define SXTH_rr(Rd,Rm)						CC_SXTH_rr(NATIVE_CC_AL,Rd,Rm)

#define CC_SXTH_rr_ROR8(cc,Rd,Rm)			_W(((cc) << 28) | (0x6b << 20) | (0xf << 16) | ((Rd) << 12) | (1 << 10) | (0x7 << 4) | SHIFT_REG(Rm))
#define SXTH_rr_ROR8(Rd,Rm)					CC_SXTH_rr_ROR8(NATIVE_CC_AL,Rd,Rm)

#define CC_SXTH_rr_ROR16(cc,Rd,Rm)			_W(((cc) << 28) | (0x6b << 20) | (0xf << 16) | ((Rd) << 12) | (2 << 10) | (0x7 << 4) | SHIFT_REG(Rm))
#define SXTH_rr_ROR16(Rd,Rm)				CC_SXTH_rr_ROR16(NATIVE_CC_AL,Rd,Rm)

#define CC_SXTH_rr_ROR24(cc,Rd,Rm)			_W(((cc) << 28) | (0x6b << 20) | (0xf << 16) | ((Rd) << 12) | (3 << 10) | (0x7 << 4) | SHIFT_REG(Rm))
#define SXTH_rr_ROR24(Rd,Rm)				CC_SXTH_rr_ROR24(NATIVE_CC_AL,Rd,Rm)

#define CC_UXTB_rr(cc,Rd,Rm)				_W(((cc) << 28) | (0x6e << 20) | (0xf << 16) | ((Rd) << 12) | (0x7 << 4) | SHIFT_REG(Rm))
#define UXTB_rr(Rd,Rm)						CC_UXTB_rr(NATIVE_CC_AL,Rd,Rm)

#define CC_UXTB_rr_ROR8(cc,Rd,Rm)			_W(((cc) << 28) | (0x6e << 20) | (0xf << 16) | ((Rd) << 12) | (1 << 10) | (0x7 << 4) | SHIFT_REG(Rm))
#define UXTB_rr_ROR8(Rd,Rm)					CC_UXTB_rr_ROR8(NATIVE_CC_AL,Rd,Rm)

#define CC_UXTB_rr_ROR16(cc,Rd,Rm)			_W(((cc) << 28) | (0x6e << 20) | (0xf << 16) | ((Rd) << 12) | (2 << 10) | (0x7 << 4) | SHIFT_REG(Rm))
#define UXTB_rr_ROR16(Rd,Rm)				CC_UXTB_rr_ROR16(NATIVE_CC_AL,Rd,Rm)

#define CC_UXTB_rr_ROR24(cc,Rd,Rm)			_W(((cc) << 28) | (0x6e << 20) | (0xf << 16) | ((Rd) << 12) | (3 << 10) | (0x7 << 4) | SHIFT_REG(Rm))
#define UXTB_rr_ROR24(Rd,Rm)				CC_UXTB_rr_ROR24(NATIVE_CC_AL,Rd,Rm)

#define CC_UXTH_rr(cc,Rd,Rm)				_W(((cc) << 28) | (0x6f << 20) | (0xf << 16) | ((Rd) << 12) | (0x7 << 4) | SHIFT_REG(Rm))
#define UXTH_rr(Rd,Rm)						CC_UXTH_rr(NATIVE_CC_AL,Rd,Rm)

#define CC_UXTH_rr_ROR8(cc,Rd,Rm)			_W(((cc) << 28) | (0x6f << 20) | (0xf << 16) | ((Rd) << 12) | (1 << 10) | (0x7 << 4) | SHIFT_REG(Rm))
#define UXTH_rr_ROR8(Rd,Rm)					CC_UXTH_rr_ROR8(NATIVE_CC_AL,Rd,Rm)

#define CC_UXTH_rr_ROR16(cc,Rd,Rm)			_W(((cc) << 28) | (0x6f << 20) | (0xf << 16) | ((Rd) << 12) | (2 << 10) | (0x7 << 4) | SHIFT_REG(Rm))
#define UXTH_rr_ROR16(Rd,Rm)				CC_UXTH_rr_ROR16(NATIVE_CC_AL,Rd,Rm)

#define CC_UXTH_rr_ROR24(cc,Rd,Rm)			_W(((cc) << 28) | (0x6f << 20) | (0xf << 16) | ((Rd) << 12) | (3 << 10) | (0x7 << 4) | SHIFT_REG(Rm))
#define UXTH_rr_ROR24(Rd,Rm)				CC_UXTH_rr_ROR24(NATIVE_CC_AL,Rd,Rm)

#define CC_REV_rr(cc,Rd,Rm)					_W(((cc) << 28) | (0x6b << 20) | (0xf << 16) | (0xf << 8) | ((Rd) << 12) | (0x3 << 4) | SHIFT_REG(Rm))
#define REV_rr(Rd,Rm)						CC_REV_rr(NATIVE_CC_AL,Rd,Rm)

#define CC_REV16_rr(cc,Rd,Rm)				_W(((cc) << 28) | (0x6b << 20) | (0xf << 16) | (0xf << 8) | ((Rd) << 12) | (0xB << 4) | SHIFT_REG(Rm))
#define REV16_rr(Rd,Rm)						CC_REV16_rr(NATIVE_CC_AL,Rd,Rm)

#define CC_REVSH_rr(cc,Rd,Rm)				_W(((cc) << 28) | (0x6f << 20) | (0xf << 16) | (0xf << 8) | ((Rd) << 12) | (0xB << 4) | SHIFT_REG(Rm))
#define REVSH_rr(Rd,Rm)						CC_REVSH_rr(NATIVE_CC_AL,Rd,Rm)

#define CC_PKHBT_rrr(cc,Rd,Rn,Rm)			_W(((cc) << 28) | (0x68 << 20) | (Rn << 16) | (Rd << 12) | (0x1 << 4) | (Rm))
#define CC_PKHBT_rrrLSLi(cc,Rd,Rn,Rm,s)		_W(((cc) << 28) | (0x68 << 20) | (Rn << 16) | (Rd << 12) | (0x1 << 4) | SHIFT_PK(Rm, s))
#define PKHBT_rrr(Rd,Rn,Rm)					CC_PKHBT_rrr(NATIVE_CC_AL,Rd,Rn,Rm)
#define PKHBT_rrrLSLi(Rd,Rn,Rm,s)			CC_PKHBT_rrrLSLi(NATIVE_CC_AL,Rd,Rn,Rm,s)

#define CC_PKHTB_rrrASRi(cc,Rd,Rn,Rm,s)		_W(((cc) << 28) | (0x68 << 20) | (Rn << 16) | (Rd << 12) | (0x5 << 4) | SHIFT_PK(Rm, s))
#define PKHTB_rrrASRi(Rd,Rn,Rm,s)			CC_PKHTB_rrrASRi(NATIVE_CC_AL,Rd,Rn,Rm,s)

#endif /* ARM_RTASM_H */
