/*
 *  fpu/fpu_x86.h - Extra Definitions for the X86 assembly FPU core
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
 *
 *  MC68881/68040 fpu emulation
 *  
 *  Original UAE FPU, copyright 1996 Herman ten Brugge
 *  Rewrite for x86, copyright 1999-2000 Lauri Pesonen
 *  New framework, copyright 2000 Gwenole Beauchesne
 *  Adapted for JIT compilation (c) Bernd Meyer, 2000
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

#ifndef FPU_X86_H
#define FPU_X86_H

/* NOTE: this file shall be included from fpu/fpu_x86.cpp */
#undef	PUBLIC
#define PUBLIC	extern

#undef	PRIVATE
#define PRIVATE	static

#undef	FFPU
#define FFPU	/**/

#undef	FPU
#define	FPU		fpu.

// Status word
PRIVATE uae_u32 x86_status_word;
PRIVATE uae_u32 x86_status_word_accrued;

// FPU jump table
typedef void REGPARAM2 ( *fpuop_func )( uae_u32, uae_u32 );
PRIVATE fpuop_func fpufunctbl[65536];

// FPU consistency
PRIVATE uae_u32 checked_sw_atstart;

// FMOVECR constants supported byt x86 FPU
PRIVATE fpu_register const_pi;
PRIVATE fpu_register const_lg2;
PRIVATE fpu_register const_l2e;
PRIVATE fpu_register const_z;
PRIVATE fpu_register const_ln2;
PRIVATE fpu_register const_1;

// FMOVECR constants not not suported by x86 FPU
PRIVATE fpu_register const_e;
PRIVATE fpu_register const_log_10_e;
PRIVATE fpu_register const_ln_10;
PRIVATE fpu_register const_1e1;
PRIVATE fpu_register const_1e2;
PRIVATE fpu_register const_1e4;
PRIVATE fpu_register const_1e8;
PRIVATE fpu_register const_1e16;
PRIVATE fpu_register const_1e32;
PRIVATE fpu_register const_1e64;
PRIVATE fpu_register const_1e128;
PRIVATE fpu_register const_1e256;
PRIVATE fpu_register const_1e512;
PRIVATE fpu_register const_1e1024;
PRIVATE fpu_register const_1e2048;
PRIVATE fpu_register const_1e4096;

// Saved host FPU state
PRIVATE uae_u8 m_fpu_state_original[108]; // 90/94/108

/* -------------------------------------------------------------------------- */
/* --- Methods                                                            --- */
/* -------------------------------------------------------------------------- */

// Debug support functions
PRIVATE void FFPU dump_first_bytes_buf(char *b, uae_u8* buf, uae_s32 actual);
PRIVATE char * FFPU etos(fpu_register const & e) REGPARAM;

// FPU consistency
PRIVATE void FFPU FPU_CONSISTENCY_CHECK_START(void);
PRIVATE void FFPU FPU_CONSISTENCY_CHECK_STOP(const char *name);

// Get special floating-point value class
PRIVATE __inline__ uae_u32 FFPU IS_INFINITY (fpu_register const & f);
PRIVATE __inline__ uae_u32 FFPU IS_NAN (fpu_register const & f);
PRIVATE __inline__ uae_u32 FFPU IS_ZERO (fpu_register const & f);
PRIVATE __inline__ uae_u32 FFPU IS_NEGATIVE (fpu_register const & f);

// Make a special floating-point value
PRIVATE __inline__ void FFPU MAKE_NAN (fpu_register & f);
PRIVATE __inline__ void FFPU MAKE_INF_POSITIVE (fpu_register & f);
PRIVATE __inline__ void FFPU MAKE_INF_NEGATIVE (fpu_register & f);
PRIVATE __inline__ void FFPU MAKE_ZERO_POSITIVE (fpu_register & f);
PRIVATE __inline__ void FFPU MAKE_ZERO_NEGATIVE (fpu_register & f);

// Conversion from extended floating-point values
PRIVATE uae_s32 FFPU extended_to_signed_32 ( fpu_register const & f ) REGPARAM;
PRIVATE uae_s16 FFPU extended_to_signed_16 ( fpu_register const & f ) REGPARAM;
PRIVATE uae_s8 FFPU extended_to_signed_8 ( fpu_register const & f ) REGPARAM;
PRIVATE fpu_double FFPU extended_to_double( fpu_register const & f ) REGPARAM;
PRIVATE uae_u32 FFPU from_single ( fpu_register const & f ) REGPARAM;
PRIVATE void FFPU from_exten ( fpu_register const & f, uae_u32 *wrd1, uae_u32 *wrd2, uae_u32 *wrd3 ) REGPARAM;
PRIVATE void FFPU from_double ( fpu_register const & f, uae_u32 *wrd1, uae_u32 *wrd2 ) REGPARAM;
PRIVATE void FFPU from_pack (fpu_double src, uae_u32 * wrd1, uae_u32 * wrd2, uae_u32 * wrd3) REGPARAM;

// Conversion to extended floating-point values
PRIVATE void FFPU signed_to_extended ( uae_s32 x, fpu_register & f ) REGPARAM;
PRIVATE void FFPU double_to_extended ( double x, fpu_register & f ) REGPARAM;
PRIVATE void FFPU to_single ( uae_u32 src, fpu_register & f ) REGPARAM;
PRIVATE void FFPU to_exten_no_normalize ( uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3, fpu_register & f ) REGPARAM;
PRIVATE void FFPU to_exten ( uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3, fpu_register & f ) REGPARAM;
PRIVATE void FFPU to_double ( uae_u32 wrd1, uae_u32 wrd2, fpu_register & f ) REGPARAM;
PRIVATE fpu_double FFPU to_pack(uae_u32 wrd1, uae_u32 wrd2, uae_u32 wrd3) REGPARAM;

// Atomic floating-point arithmetic operations
PRIVATE void FFPU do_fmove ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fmove_no_status ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fint ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fintrz ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fsqrt ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_ftst ( fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fsinh ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_flognp1 ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fetoxm1 ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_ftanh ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fatan ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fasin ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fatanh ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fetox ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_ftwotox ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_ftentox ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_flogn ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_flog10 ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_flog2 ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_facos ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fcosh ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fsin ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_ftan ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fabs ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fneg ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fcos ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fgetexp ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fgetman ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fdiv ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fmod ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_frem ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fmod_dont_set_cw ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_frem_dont_set_cw ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fadd ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fmul ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fsgldiv ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fscale ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fsglmul ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fsub ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fsincos ( fpu_register & dest_sin, fpu_register & dest_cos, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fcmp ( fpu_register & dest, fpu_register const & src ) REGPARAM;
PRIVATE void FFPU do_fldpi ( fpu_register & dest ) REGPARAM;
PRIVATE void FFPU do_fldlg2 ( fpu_register & dest ) REGPARAM;
PRIVATE void FFPU do_fldl2e ( fpu_register & dest ) REGPARAM;
PRIVATE void FFPU do_fldz ( fpu_register & dest ) REGPARAM;
PRIVATE void FFPU do_fldln2 ( fpu_register & dest ) REGPARAM;
PRIVATE void FFPU do_fld1 ( fpu_register & dest ) REGPARAM;

// Instructions handlers
PRIVATE void REGPARAM2 FFPU fpuop_illg( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmove_2_ea( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_none_2_Dreg( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpiar_2_Dreg( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpsr_2_Dreg( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_2_Dreg( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpsr_fpiar_2_Dreg( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpiar_2_Dreg( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpsr_2_Dreg( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpsr_fpiar_2_Dreg( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Dreg_2_none( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Dreg_2_fpiar( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Dreg_2_fpsr( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Dreg_2_fpsr_fpiar( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Dreg_2_fpcr( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Dreg_2_fpcr_fpiar( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Dreg_2_fpcr_fpsr( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Dreg_2_fpcr_fpsr_fpiar( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_none_2_Areg( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpiar_2_Areg( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpsr_2_Areg( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_2_Areg( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpsr_fpiar_2_Areg( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpiar_2_Areg( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpsr_2_Areg( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpsr_fpiar_2_Areg( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Areg_2_none( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Areg_2_fpiar( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Areg_2_fpsr( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Areg_2_fpsr_fpiar( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Areg_2_fpcr( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Areg_2_fpcr_fpiar( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Areg_2_fpcr_fpsr( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Areg_2_fpcr_fpsr_fpiar( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_none_2_Mem_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpiar_2_Mem_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpsr_2_Mem_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpsr_fpiar_2_Mem_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_2_Mem_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpiar_2_Mem_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpsr_2_Mem_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpsr_fpiar_2_Mem_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_none_2_Mem_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpiar_2_Mem_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpsr_2_Mem_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpsr_fpiar_2_Mem_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_2_Mem_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpiar_2_Mem_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpsr_2_Mem_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpsr_fpiar_2_Mem_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_none_2_Mem( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpiar_2_Mem( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpsr_2_Mem( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpsr_fpiar_2_Mem( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_2_Mem( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpiar_2_Mem( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpsr_2_Mem( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpcr_fpsr_fpiar_2_Mem( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_none_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpiar_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpsr_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpsr_fpiar_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_fpiar_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_fpsr_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_fpsr_fpiar_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_none_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpiar_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpsr_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpsr_fpiar_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_fpiar_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_fpsr_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_fpsr_fpiar_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_none_2_Mem( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpiar_2_Mem( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpsr_2_Mem( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpsr_fpiar_2_Mem( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_2_Mem( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_fpiar_2_Mem( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_fpsr_2_Mem( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpcr_fpsr_fpiar_2_Mem( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_static_pred_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_static_pred_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_static_pred( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_dynamic_pred_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_dynamic_pred_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_dynamic_pred( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_static_postinc_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_static_postinc_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_static_postinc( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_dynamic_postinc_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_dynamic_postinc_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_Mem_2_fpp_dynamic_postinc( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_static_pred_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_static_pred_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_static_pred( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_dynamic_pred_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_dynamic_pred_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_dynamic_pred( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_static_postinc_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_static_postinc_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_static_postinc( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_dynamic_postinc_postincrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_dynamic_postinc_predecrement( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_fmovem_fpp_2_Mem_dynamic_postinc( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fldpi( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fldlg2( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_e( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fldl2e( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_log_10_e( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fldz( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fldln2( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_ln_10( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fld1( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e1( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e2( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e4( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e8( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e16( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e32( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e64( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e128( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e256( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e512( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e1024( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e2048( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_load_const_1e4096( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fmove( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fint( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fsinh( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fintrz( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fsqrt( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_flognp1( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fetoxm1( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_ftanh( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fatan( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fasin( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fatanh( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fsin( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_ftan( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fetox( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_ftwotox( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_ftentox( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_flogn( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_flog10( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_flog2( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fabs( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fcosh( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fneg( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_facos( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fcos( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fgetexp( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fgetman( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fdiv( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fmod( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_frem( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fadd( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fmul( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fsgldiv( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fscale( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fsglmul( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fsub( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fsincos( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_fcmp( uae_u32 opcode, uae_u32 extra );
PRIVATE void REGPARAM2 FFPU fpuop_do_ftst( uae_u32 opcode, uae_u32 extra );

// Get & Put floating-point values
PRIVATE int FFPU get_fp_value (uae_u32 opcode, uae_u32 extra, fpu_register & src) REGPARAM;
PRIVATE int FFPU put_fp_value (fpu_register const & value, uae_u32 opcode, uae_u32 extra) REGPARAM;
PRIVATE int FFPU get_fp_ad(uae_u32 opcode, uae_u32 * ad) REGPARAM;

// Floating-point condition-based instruction handlers
PRIVATE int FFPU fpp_cond(uae_u32 opcode, int condition) REGPARAM;

// Misc functions
PRIVATE void __inline__ FFPU set_host_fpu_control_word ();
PRIVATE void __inline__ FFPU SET_BSUN_ON_NAN ();
PRIVATE void __inline__ FFPU build_ex_status ();
PRIVATE void FFPU do_null_frestore ();
PRIVATE void FFPU build_fpp_opp_lookup_table ();
PRIVATE void FFPU set_constant ( fpu_register & f, char *name, double value, uae_s32 mult );

#endif /* FPU_X86_H */
