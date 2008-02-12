/******************** -*- mode: C; tab-width: 8 -*- ********************
 *
 *	Run-time assembler for IA-32 and AMD64
 *
 ***********************************************************************/


/***********************************************************************
 *
 *  This file is derived from CCG.
 *
 *  Copyright 1999, 2000, 2001, 2002, 2003 Ian Piumarta
 *
 *  Adaptations and enhancements for AMD64 support, Copyright 2003-2008
 *    Gwenole Beauchesne
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
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
 ***********************************************************************/

#ifndef X86_RTASM_H
#define X86_RTASM_H

/* NOTES
 *
 *	o Best viewed on a 1024x768 screen with fixed-6x10 font ;-)
 *
 * TODO
 *
 *	o Fix FIXMEs
 *	o i387 FPU instructions
 *	o SSE instructions
 *	o Optimize for cases where register numbers are not integral constants
 */

/* --- Configuration ------------------------------------------------------- */

/* Define to settle a "flat" register set, i.e. different regno for
   each size variant.  */
#ifndef X86_FLAT_REGISTERS
#define X86_FLAT_REGISTERS	1
#endif

/* Define to generate x86-64 code.  */
#ifndef X86_TARGET_64BIT
#define X86_TARGET_64BIT	0
#endif

/* Define to optimize ALU instructions.  */
#ifndef X86_OPTIMIZE_ALU
#define X86_OPTIMIZE_ALU	1
#endif

/* Define to optimize rotate/shift instructions.  */
#ifndef X86_OPTIMIZE_ROTSHI
#define X86_OPTIMIZE_ROTSHI	1
#endif

/* Define to optimize absolute addresses for RIP relative addressing.  */
#ifndef X86_RIP_RELATIVE_ADDR
#define X86_RIP_RELATIVE_ADDR	1
#endif 


/* --- Macros -------------------------------------------------------------- */

/* Functions used to emit code.
 *
 *	x86_emit_byte(B)
 *	x86_emit_word(W)
 *	x86_emit_long(L)
 */

/* Get pointer to current code
 *
 *	x86_get_target()
 */

/* Abort assembler, fatal failure.
 *
 *	x86_emit_failure(MSG)
 */

#define x86_emit_failure0(MSG) (x86_emit_failure(MSG),0)


/* --- Register set -------------------------------------------------------- */

enum {
  X86_RIP         = -2,
#if X86_FLAT_REGISTERS
  X86_NOREG       = 0,
  X86_Reg8L_Base  = 0x10,
  X86_Reg8H_Base  = 0x20,
  X86_Reg16_Base  = 0x30,
  X86_Reg32_Base  = 0x40,
  X86_Reg64_Base  = 0x50,
  X86_RegMMX_Base = 0x60,
  X86_RegXMM_Base = 0x70,
#else
  X86_NOREG       = -1,
  X86_Reg8L_Base  = 0,
  X86_Reg8H_Base  = 16,
  X86_Reg16_Base  = 0,
  X86_Reg32_Base  = 0,
  X86_Reg64_Base  = 0,
  X86_RegMMX_Base = 0,
  X86_RegXMM_Base = 0,
#endif
};

enum {
  X86_AL  = X86_Reg8L_Base,
  X86_CL,   X86_DL,   X86_BL,
  X86_SPL,  X86_BPL,  X86_SIL,  X86_DIL,
  X86_R8B,  X86_R9B,  X86_R10B, X86_R11B,
  X86_R12B, X86_R13B, X86_R14B, X86_R15B,
  X86_AH  = X86_Reg8H_Base + 4,
  X86_CH,   X86_DH,   X86_BH
};

enum {
  X86_AX  = X86_Reg16_Base,
  X86_CX,   X86_DX,   X86_BX,
  X86_SP,   X86_BP,   X86_SI,   X86_DI,
  X86_R8W,  X86_R9W,  X86_R10W, X86_R11W,
  X86_R12W, X86_R13W, X86_R14W, X86_R15W
};

enum {
  X86_EAX = X86_Reg32_Base,
  X86_ECX,  X86_EDX,  X86_EBX,
  X86_ESP,  X86_EBP,  X86_ESI,  X86_EDI,
  X86_R8D,  X86_R9D,  X86_R10D, X86_R11D,
  X86_R12D, X86_R13D, X86_R14D, X86_R15D
};

enum {
  X86_RAX = X86_Reg64_Base,
  X86_RCX,  X86_RDX,  X86_RBX,
  X86_RSP,  X86_RBP,  X86_RSI,  X86_RDI,
  X86_R8,   X86_R9,   X86_R10,  X86_R11,
  X86_R12,  X86_R13,  X86_R14,  X86_R15
};

enum {
  X86_MM0 = X86_RegMMX_Base,
  X86_MM1,  X86_MM2,  X86_MM3,
  X86_MM4,  X86_MM5,  X86_MM6,  X86_MM7,
};

enum {
  X86_XMM0 = X86_RegXMM_Base,
  X86_XMM1,  X86_XMM2,  X86_XMM3,
  X86_XMM4,  X86_XMM5,  X86_XMM6,  X86_XMM7,
  X86_XMM8,  X86_XMM9,  X86_XMM10, X86_XMM11,
  X86_XMM12, X86_XMM13, X86_XMM14, X86_XMM15
};

/* Register control and access
 *
 *	_r0P(R)	Null register?
 *	_rIP(R)	RIP register?
 *	_rXP(R)	Extended register?
 *
 *	_rC(R)	Class of register (only valid if X86_FLAT_REGISTERS)
 *	_rR(R)	Full register number
 *	_rN(R)	Short register number for encoding
 *
 *	_r1(R)	8-bit register ID
 *	_r2(R)	16-bit register ID
 *	_r4(R)	32-bit register ID
 *	_r8(R)	64-bit register ID
 *	_rM(R)	MMX register ID
 *	_rX(R)	XMM register ID
 *	_rA(R)	Address register ID used for EA calculation
 */

#define _r0P(R)		((int)(R) == (int)X86_NOREG)
#define _rIP(R)		(X86_TARGET_64BIT ? ((int)(R) == (int)X86_RIP) : 0)

#if X86_FLAT_REGISTERS
#define _rC(R)		((R) & 0xf0)
#define _rR(R)		((R) & 0x0f)
#define _rN(R)		((R) & 0x07)
#define _rXP(R)		((R) > 0 && _rR(R) > 7)
#else
#define _rN(R)		((R) & 0x07)
#define _rR(R)		(int(R))
#define _rXP(R)		(_rR(R) > 7 && _rR(R) < 16)
#endif

#if !defined(_ASM_SAFETY) || ! X86_FLAT_REGISTERS
#define _r1(R)		_rN(R)
#define _r2(R)		_rN(R)
#define _r4(R)		_rN(R)
#define _r8(R)		_rN(R)
#define _rA(R)		_rN(R)
#define _rM(R)		_rN(R)
#define _rX(R)		_rN(R)
#else
#define _r1(R)		( ((_rC(R) & (X86_Reg8L_Base | X86_Reg8H_Base)) != 0)	? _rN(R) : x86_emit_failure0( "8-bit register required"))
#define _r2(R)		( (_rC(R) == X86_Reg16_Base)				? _rN(R) : x86_emit_failure0("16-bit register required"))
#define _r4(R)		( (_rC(R) == X86_Reg32_Base)				? _rN(R) : x86_emit_failure0("32-bit register required"))
#define _r8(R)		( (_rC(R) == X86_Reg64_Base)				? _rN(R) : x86_emit_failure0("64-bit register required"))
#define _rA(R)		( X86_TARGET_64BIT ? \
			( (_rC(R) == X86_Reg64_Base)				? _rN(R) : x86_emit_failure0("not a valid 64-bit base/index expression")) : \
			( (_rC(R) == X86_Reg32_Base)				? _rN(R) : x86_emit_failure0("not a valid 32-bit base/index expression")) )
#define _rM(R)		( (_rC(R) == X86_RegMMX_Base)				? _rN(R) : x86_emit_failure0("MMX register required"))
#define _rX(R)		( (_rC(R) == X86_RegXMM_Base)				? _rN(R) : x86_emit_failure0("SSE register required"))
#endif

#define _rSP()		(X86_TARGET_64BIT ? (int)X86_RSP : (int)X86_ESP)
#define _r1e8lP(R)	(int(R) >= X86_SPL && int(R) <= X86_DIL)
#define _rbpP(R)	(_rR(R) == _rR(X86_RBP))
#define _rspP(R)	(_rR(R) == _rR(X86_RSP))
#define _rbp13P(R)	(_rN(R) == _rN(X86_RBP))
#define _rsp12P(R)	(_rN(R) == _rN(X86_RSP))


/* ========================================================================= */
/* --- UTILITY ------------------------------------------------------------- */
/* ========================================================================= */

typedef signed char	_sc;
typedef unsigned char	_uc;
typedef signed short	_ss;
typedef unsigned short	_us;
typedef signed int	_sl;
typedef unsigned int	_ul;

#define _UC(X)		((_uc  )(unsigned long)(X))
#define _US(X)		((_us  )(unsigned long)(X))
#define _SL(X)		((_sl  )(unsigned long)(X))
#define _UL(X)		((_ul  )(unsigned long)(X))

#define _PUC(X)		((_uc *)(X))
#define _PUS(X)		((_us *)(X))
#define _PSL(X)		((_sl *)(X))
#define _PUL(X)		((_ul *)(X))

#define _B(B)		x86_emit_byte((B))
#define _W(W)		x86_emit_word((W))
#define _L(L)		x86_emit_long((L))
#define _Q(Q)		x86_emit_quad((Q))

#define _MASK(N)	((unsigned)((1<<(N)))-1)
#define _siP(N,I)	(!((((unsigned)(I))^(((unsigned)(I))<<1))&~_MASK(N)))
#define _uiP(N,I)	(!(((unsigned)(I))&~_MASK(N)))
#define _suiP(N,I)	(_siP(N,I) | _uiP(N,I))

#ifndef _ASM_SAFETY
#define _ck_s(W,I)	(_UL(I) & _MASK(W))
#define _ck_u(W,I)    	(_UL(I) & _MASK(W))
#define _ck_su(W,I)    	(_UL(I) & _MASK(W))
#define _ck_d(W,I)    	(_UL(I) & _MASK(W))
#else
#define _ck_s(W,I)	(_siP(W,I) ? (_UL(I) & _MASK(W)) : x86_emit_failure0(  "signed integer `"#I"' too large for "#W"-bit field"))
#define _ck_u(W,I)    	(_uiP(W,I) ? (_UL(I) & _MASK(W)) : x86_emit_failure0("unsigned integer `"#I"' too large for "#W"-bit field"))
#define _ck_su(W,I)    	(_suiP(W,I) ? (_UL(I) & _MASK(W)) : x86_emit_failure0(        "integer `"#I"' too large for "#W"-bit field"))
#define _ck_d(W,I)    	(_siP(W,I) ? (_UL(I) & _MASK(W)) : x86_emit_failure0(    "displacement `"#I"' too large for "#W"-bit field"))
#endif

#define _s0P(I)		((I)==0)
#define _s8P(I)		_siP(8,I)
#define _s16P(I)	_siP(16,I)
#define _u8P(I)		_uiP(8,I)
#define _u16P(I)	_uiP(16,I)

#define _su8(I)		_ck_su(8,I)
#define _su16(I)	_ck_su(16,I)

#define _s1(I)          _ck_s( 1,I)
#define _s2(I)          _ck_s( 2,I)
#define _s3(I)          _ck_s( 3,I)
#define _s4(I)          _ck_s( 4,I)
#define _s5(I)          _ck_s( 5,I)
#define _s6(I)          _ck_s( 6,I)
#define _s7(I)          _ck_s( 7,I)
#define _s8(I)          _ck_s( 8,I)
#define _s9(I)          _ck_s( 9,I)
#define _s10(I)         _ck_s(10,I)
#define _s11(I)         _ck_s(11,I)
#define _s12(I)         _ck_s(12,I)
#define _s13(I)         _ck_s(13,I)
#define _s14(I)         _ck_s(14,I)
#define _s15(I)         _ck_s(15,I)
#define _s16(I)         _ck_s(16,I)
#define _s17(I)         _ck_s(17,I)
#define _s18(I)         _ck_s(18,I)
#define _s19(I)         _ck_s(19,I)
#define _s20(I)         _ck_s(20,I)
#define _s21(I)         _ck_s(21,I)
#define _s22(I)         _ck_s(22,I)
#define _s23(I)         _ck_s(23,I)
#define _s24(I)         _ck_s(24,I)
#define _s25(I)         _ck_s(25,I)
#define _s26(I)         _ck_s(26,I)
#define _s27(I)         _ck_s(27,I)
#define _s28(I)         _ck_s(28,I)
#define _s29(I)         _ck_s(29,I)
#define _s30(I)         _ck_s(30,I)
#define _s31(I)         _ck_s(31,I)
#define _u1(I)          _ck_u( 1,I)
#define _u2(I)          _ck_u( 2,I)
#define _u3(I)          _ck_u( 3,I)
#define _u4(I)          _ck_u( 4,I)
#define _u5(I)          _ck_u( 5,I)
#define _u6(I)          _ck_u( 6,I)
#define _u7(I)          _ck_u( 7,I)
#define _u8(I)          _ck_u( 8,I)
#define _u9(I)          _ck_u( 9,I)
#define _u10(I)         _ck_u(10,I)
#define _u11(I)         _ck_u(11,I)
#define _u12(I)         _ck_u(12,I)
#define _u13(I)         _ck_u(13,I)
#define _u14(I)         _ck_u(14,I)
#define _u15(I)         _ck_u(15,I)
#define _u16(I)         _ck_u(16,I)
#define _u17(I)         _ck_u(17,I)
#define _u18(I)         _ck_u(18,I)
#define _u19(I)         _ck_u(19,I)
#define _u20(I)         _ck_u(20,I)
#define _u21(I)         _ck_u(21,I)
#define _u22(I)         _ck_u(22,I)
#define _u23(I)         _ck_u(23,I)
#define _u24(I)         _ck_u(24,I)
#define _u25(I)         _ck_u(25,I)
#define _u26(I)         _ck_u(26,I)
#define _u27(I)         _ck_u(27,I)
#define _u28(I)         _ck_u(28,I)
#define _u29(I)         _ck_u(29,I)
#define _u30(I)         _ck_u(30,I)
#define _u31(I)         _ck_u(31,I)

/* ========================================================================= */
/* --- ASSEMBLER ----------------------------------------------------------- */
/* ========================================================================= */

#define _b00		0
#define _b01		1
#define _b10		2
#define _b11		3

#define _b000		0
#define _b001		1
#define _b010		2
#define _b011		3
#define _b100		4
#define _b101		5
#define _b110		6
#define _b111		7

#define _OFF4(D)	(_UL(D) - _UL(x86_get_target()))
#define _CKD8(D)	_ck_d(8, ((_uc) _OFF4(D)) )

#define _D8(D)		(_B(0), ((*(_PUC(x86_get_target())-1))= _CKD8(D)))
#define _D32(D)		(_L(0), ((*(_PUL(x86_get_target())-1))= _OFF4(D)))

#ifndef _ASM_SAFETY
# define _M(M)		(M)
# define _r(R)		(R)
# define _m(M)		(M)
# define _s(S)		(S)
# define _i(I)		(I)
# define _b(B)		(B)
#else
# define _M(M)		(((M)>3) ? x86_emit_failure0("internal error: mod = " #M) : (M))
# define _r(R)		(((R)>7) ? x86_emit_failure0("internal error: reg = " #R) : (R))
# define _m(M)		(((M)>7) ? x86_emit_failure0("internal error: r/m = " #M) : (M))
# define _s(S)		(((S)>3) ? x86_emit_failure0("internal error: memory scale = " #S) : (S))
# define _i(I)		(((I)>7) ? x86_emit_failure0("internal error: memory index = " #I) : (I))
# define _b(B)		(((B)>7) ? x86_emit_failure0("internal error: memory base = "  #B) : (B))
#endif

#define _Mrm(Md,R,M)	_B((_M(Md)<<6)|(_r(R)<<3)|_m(M))
#define _SIB(Sc,I, B)	_B((_s(Sc)<<6)|(_i(I)<<3)|_b(B))

#define _SCL(S)		((((S)==1) ? _b00 : \
			 (((S)==2) ? _b01 : \
			 (((S)==4) ? _b10 : \
			 (((S)==8) ? _b11 : x86_emit_failure0("illegal scale: " #S))))))


/* --- Memory subformats - urgh! ------------------------------------------- */

/* _r_D() is RIP addressing mode if X86_TARGET_64BIT, use _r_DSIB() instead */
#define _r_D(	R, D	  )	(_Mrm(_b00,_rN(R),_b101 )		             ,_L((_sl)(D)))
#define _r_DSIB(R, D      )	(_Mrm(_b00,_rN(R),_b100 ),_SIB(_SCL(1),_b100 ,_b101 ),_L((_sl)(D)))
#define _r_0B(	R,   B    )	(_Mrm(_b00,_rN(R),_rA(B))			                   )
#define _r_0BIS(R,   B,I,S)	(_Mrm(_b00,_rN(R),_b100 ),_SIB(_SCL(S),_rA(I),_rA(B))              )
#define _r_1B(	R, D,B    )	(_Mrm(_b01,_rN(R),_rA(B))		             ,_B((_sc)(D)))
#define _r_1BIS(R, D,B,I,S)	(_Mrm(_b01,_rN(R),_b100 ),_SIB(_SCL(S),_rA(I),_rA(B)),_B((_sc)(D)))
#define _r_4B(	R, D,B    )	(_Mrm(_b10,_rN(R),_rA(B))		             ,_L((_sl)(D)))
#define _r_4IS( R, D,I,S)	(_Mrm(_b00,_rN(R),_b100 ),_SIB(_SCL(S),_rA(I),_b101 ),_L((_sl)(D)))
#define _r_4BIS(R, D,B,I,S)	(_Mrm(_b10,_rN(R),_b100 ),_SIB(_SCL(S),_rA(I),_rA(B)),_L((_sl)(D)))

#define _r_DB(  R, D,B    )	((_s0P(D) && (!_rbp13P(B)) ? _r_0B  (R,  B    ) : (_s8P(D) ? _r_1B(  R,D,B    ) : _r_4B(  R,D,B    ))))
#define _r_DBIS(R, D,B,I,S)	((_s0P(D) && (!_rbp13P(B)) ? _r_0BIS(R,  B,I,S) : (_s8P(D) ? _r_1BIS(R,D,B,I,S) : _r_4BIS(R,D,B,I,S))))

/* Use RIP-addressing in 64-bit mode, if possible */
#define _x86_RIP_addressing_possible(D,O)	(X86_RIP_RELATIVE_ADDR && \
						((uintptr)x86_get_target() + 4 + (O) - (D) <= 0xffffffff))

#define _r_X(   R, D,B,I,S,O)	(_r0P(I) ? (_r0P(B)    ? (!X86_TARGET_64BIT ? _r_D(R,D) : \
					                 (_x86_RIP_addressing_possible(D, O) ? \
				                          _r_D(R, (D) - ((uintptr)x86_get_target() + 4 + (O))) : \
				                          _r_DSIB(R,D))) : \
				           (_rIP(B)    ? _r_D   (R,D                )   : \
				           (_rsp12P(B) ? _r_DBIS(R,D,_rSP(),_rSP(),1)   : \
						         _r_DB  (R,D,     B       ))))  : \
				 (_r0P(B)	       ? _r_4IS (R,D,	         I,S)   : \
				 (!_rspP(I)            ? _r_DBIS(R,D,     B,     I,S)   : \
						         x86_emit_failure("illegal index register: %esp"))))


/* --- Instruction formats ------------------------------------------------- */

#define _m32only(X)		(! X86_TARGET_64BIT ? X : x86_emit_failure("invalid instruction in 64-bit mode"))
#define _m64only(X)		(  X86_TARGET_64BIT ? X : x86_emit_failure("invalid instruction in 32-bit mode"))
#define _m64(X)			(  X86_TARGET_64BIT ? X : ((void)0)						)

/*	 _format						     Opcd	  ModR/M dN(rB,rI,Sc)	  imm... */

#define	 _d16()					   (		  _B(0x66	)				  )
#define	  _O(	     OP				)  (		  _B(  OP	)				  )
#define	  _Or(	     OP,R			)  (		  _B( (OP)|_r(R))				  )
#define	 _OO(	     OP				)  ( _B((OP)>>8), _B(( (OP)      )&0xff)			  )
#define	 _OOr(	     OP,R			)  ( _B((OP)>>8), _B(( (OP)|_r(R))&0xff)			  )
#define	  _Os(	     OP,B			)  (	_s8P(B) ? _B(((OP)|_b10)) : _B(OP)			  )
#define	    _sW(			     W	)  (				       _s8P(W) ? _B(W):_W(W)	  )
#define	    _sL(			     L	)  (				       _s8P(L) ? _B(L):_L(L)	  )
#define	    _sWO(			     W	)  (				       _s8P(W) ?    1 :   2	  )
#define	    _sLO(			     L	)  (				       _s8P(L) ?    1 :   4	  )
#define	  _O_B(	     OP			    ,B	)  (	    _O	    (  OP  )			      ,_B(B)	  )
#define	  _O_W(	     OP			    ,W	)  (	    _O	    (  OP  )			      ,_W(W)	  )
#define	  _O_L(	     OP			    ,L	)  (	    _O	    (  OP  )			      ,_L(L)	  )
#define	  _OO_L(     OP			    ,L	)  (	   _OO	    (  OP  )			      ,_L(L)	  )
#define	  _O_D8(     OP			    ,D	)  (	    _O	    (  OP  )			     ,_D8(D)	  )
#define	  _O_D32(     OP		    ,D	)  (	    _O	    (  OP  )			     ,_D32(D)	  )
#define	 _OO_D32(     OP		    ,D	)  (	   _OO	    (  OP  )			     ,_D32(D)	  )
#define	  _Os_sW(    OP			    ,W	)  (	    _Os	    (  OP,W)			     ,_sW(W)	  )
#define	  _Os_sL(    OP			    ,L	)  (	    _Os	    (  OP,L)			     ,_sL(L)	  )
#define	  _O_W_B(    OP			    ,W,B)  (	    _O	    (  OP  )			      ,_W(W),_B(B))
#define	  _Or_B(     OP,R		    ,B	)  (	    _Or	    (  OP,R)			      ,_B(B)	  )
#define	  _Or_W(     OP,R		    ,W	)  (	    _Or	    (  OP,R)			      ,_W(W)	  )
#define	  _Or_L(     OP,R		    ,L	)  (	    _Or	    (  OP,R)			      ,_L(L)	  )
#define	  _Or_Q(     OP,R		    ,Q	)  (	    _Or	    (  OP,R)			      ,_Q(Q)	  )
#define	  _O_Mrm(    OP	 ,MO,R,M		)  (	    _O	    (  OP  ),_Mrm(MO,R,M	    )		  )
#define	 _OO_Mrm(    OP	 ,MO,R,M		)  (	   _OO	    (  OP  ),_Mrm(MO,R,M	    )		  )
#define	  _O_Mrm_B(  OP	 ,MO,R,M	    ,B	)  (	    _O	    (  OP  ),_Mrm(MO,R,M	    ) ,_B(B)	  )
#define	  _O_Mrm_W(  OP	 ,MO,R,M	    ,W	)  (	    _O	    (  OP  ),_Mrm(MO,R,M	    ) ,_W(W)	  )
#define	  _O_Mrm_L(  OP	 ,MO,R,M	    ,L	)  (	    _O	    (  OP  ),_Mrm(MO,R,M	    ) ,_L(L)	  )
#define	 _OO_Mrm_B(  OP	 ,MO,R,M	    ,B	)  (	   _OO	    (  OP  ),_Mrm(MO,R,M	    ) ,_B(B)	  )
#define	  _Os_Mrm_sW(OP	 ,MO,R,M	    ,W	)  (	    _Os	    (  OP,W),_Mrm(MO,R,M	    ),_sW(W)	  )
#define	  _Os_Mrm_sL(OP	 ,MO,R,M	    ,L	)  (	    _Os	    (  OP,L),_Mrm(MO,R,M	    ),_sL(L)	  )
#define	  _O_r_X(    OP	    ,R	,MD,MB,MI,MS	)  (	    _O	    (  OP  ),_r_X(   R	,MD,MB,MI,MS,0)		  )
#define	 _OO_r_X(    OP	    ,R	,MD,MB,MI,MS	)  (	   _OO	    (  OP  ),_r_X(   R	,MD,MB,MI,MS,0)		  )
#define	  _O_r_X_B(  OP	    ,R	,MD,MB,MI,MS,B	)  (	    _O	    (  OP  ),_r_X(   R	,MD,MB,MI,MS,1) ,_B(B)	  )
#define	  _O_r_X_W(  OP	    ,R	,MD,MB,MI,MS,W	)  (	    _O	    (  OP  ),_r_X(   R	,MD,MB,MI,MS,2) ,_W(W)	  )
#define	  _O_r_X_L(  OP	    ,R	,MD,MB,MI,MS,L	)  (	    _O	    (  OP  ),_r_X(   R	,MD,MB,MI,MS,4) ,_L(L)	  )
#define	 _OO_r_X_B(  OP	    ,R	,MD,MB,MI,MS,B	)  (	   _OO	    (  OP  ),_r_X(   R	,MD,MB,MI,MS,1) ,_B(B)	  )
#define	  _Os_r_X_sW(OP	    ,R	,MD,MB,MI,MS,W	)  (	    _Os	    (  OP,W),_r_X(   R	,MD,MB,MI,MS,_sWO(W)),_sW(W))
#define	  _Os_r_X_sL(OP	    ,R	,MD,MB,MI,MS,L	)  (	    _Os	    (  OP,L),_r_X(   R	,MD,MB,MI,MS,_sLO(L)),_sL(L))
#define	  _O_X_B(    OP		,MD,MB,MI,MS,B	)  (	    _O_r_X_B(  OP	    ,0	,MD,MB,MI,MS	 ,B)	  )
#define	  _O_X_W(    OP		,MD,MB,MI,MS,W	)  (	    _O_r_X_W(  OP	    ,0	,MD,MB,MI,MS	 ,W)	  )
#define	  _O_X_L(    OP		,MD,MB,MI,MS,L	)  (	    _O_r_X_L(  OP	    ,0	,MD,MB,MI,MS	 ,L)	  )


/* --- REX prefixes -------------------------------------------------------- */

#define _VOID()			((void)0)
#define _BIT(X)			(!!(X))
#define _d64(W,R,X,B)		(_B(0x40|(W)<<3|(R)<<2|(X)<<1|(B)))

#define __REXwrxb(L,W,R,X,B)	((W|R|X|B) || (L) ? _d64(W,R,X,B) : _VOID())
#define __REXwrx_(L,W,R,X,MR)	(__REXwrxb(L,W,R,X,_BIT(_rIP(MR)?0:_rXP(MR))))
#define __REXw_x_(L,W,R,X,MR)	(__REXwrx_(L,W,_BIT(_rXP(R)),X,MR))
#define __REX_reg(RR)		(__REXwrxb(0,0,0,00,_BIT(_rXP(RR))))
#define __REX_mem(MB,MI)	(__REXwrxb(0,0,0,_BIT(_rXP(MI)),_BIT(_rXP(MB))))

// FIXME: can't mix new (SPL,BPL,SIL,DIL) with (AH,BH,CH,DH)
#define _REXBrr(RR,MR)		_m64(__REXw_x_(_r1e8lP(RR)||_r1e8lP(MR),0,RR,0,MR))
#define _REXBmr(MB,MI,RD)	_m64(__REXw_x_(_r1e8lP(RD)||_r1e8lP(MB),0,RD,_BIT(_rXP(MI)),MB))
#define _REXBrm(RS,MB,MI)	_REXBmr(MB,MI,RS)

#define _REXBLrr(RR,MR)		_m64(__REXw_x_(_r1e8lP(MR),0,RR,0,MR))
#define _REXLrr(RR,MR)		_m64(__REXw_x_(0,0,RR,0,MR))
#define _REXLmr(MB,MI,RD)	_m64(__REXw_x_(0,0,RD,_BIT(_rXP(MI)),MB))
#define _REXLrm(RS,MB,MI)	_REXLmr(MB,MI,RS)
#define _REXLr(RR)		_m64(__REX_reg(RR))
#define _REXLm(MB,MI)		_m64(__REX_mem(MB,MI))

#define _REXQrr(RR,MR)		_m64only(__REXw_x_(0,1,RR,0,MR))
#define _REXQmr(MB,MI,RD)	_m64only(__REXw_x_(0,1,RD,_BIT(_rXP(MI)),MB))
#define _REXQrm(RS,MB,MI)	_REXQmr(MB,MI,RS)
#define _REXQr(RR)		_m64only(__REX_reg(RR))
#define _REXQm(MB,MI)		_m64only(__REX_mem(MB,MI))


/* ========================================================================= */
/* --- Fully-qualified intrinsic instructions ------------------------------ */
/* ========================================================================= */

/*	OPCODE	+ i	= immediate operand
 *		+ r	= register operand
 *		+ m	= memory operand (disp,base,index,scale)
 *		+ sr/sm	= a star preceding a register or memory
 *		+ 0	= top of stack register (for FPU instructions)
 *
 *	NOTE in x86-64 mode: a memory operand with only a valid
 *	displacement value will lead to the expect absolute mode. If
 *	RIP addressing is necessary, X86_RIP shall be used as the base
 *	register argument.
 */

/* --- ALU instructions ---------------------------------------------------- */

enum {
  X86_ADD = 0,
  X86_OR  = 1,
  X86_ADC = 2,
  X86_SBB = 3,
  X86_AND = 4,
  X86_SUB = 5,
  X86_XOR = 6,
  X86_CMP = 7,
};

/*									_format		Opcd		,Mod ,r	    ,m		,mem=dsp+sib	,imm... */

#define _ALUBrr(OP,RS, RD)		(_REXBrr(RS, RD),		_O_Mrm		(((OP) << 3)	,_b11,_r1(RS),_r1(RD)				))
#define _ALUBmr(OP, MD, MB, MI, MS, RD)	(_REXBmr(MB, MI, RD),		_O_r_X		(((OP) << 3) + 2     ,_r1(RD)		,MD,MB,MI,MS		))
#define _ALUBrm(OP, RS, MD, MB, MI, MS)	(_REXBrm(RS, MB, MI),		_O_r_X		(((OP) << 3)	     ,_r1(RS)		,MD,MB,MI,MS		))
#define _ALUBir(OP, IM, RD)		(X86_OPTIMIZE_ALU && ((RD) == X86_AL) ? \
					(_REXBrr(0, RD),		_O_B		(((OP) << 3) + 4					,_su8(IM))) : \
					(_REXBrr(0, RD),		_O_Mrm_B	(0x80		,_b11,OP     ,_r1(RD)			,_su8(IM))) )
#define _ALUBim(OP, IM, MD, MB, MI, MS)	(_REXBrm(0, MB, MI),		_O_r_X_B	(0x80		     ,OP		,MD,MB,MI,MS	,_su8(IM)))

#define _ALUWrr(OP, RS, RD)		(_d16(), _REXLrr(RS, RD),	_O_Mrm		(((OP) << 3) + 1,_b11,_r2(RS),_r2(RD)				))
#define _ALUWmr(OP, MD, MB, MI, MS, RD)	(_d16(), _REXLmr(MB, MI, RD),	_O_r_X		(((OP) << 3) + 3     ,_r2(RD)		,MD,MB,MI,MS		))
#define _ALUWrm(OP, RS, MD, MB, MI, MS)	(_d16(), _REXLrm(RS, MB, MI),	_O_r_X		(((OP) << 3) + 1     ,_r2(RS)		,MD,MB,MI,MS		))
#define _ALUWir(OP, IM, RD)		(X86_OPTIMIZE_ALU && ((RD) == X86_AX) ? \
					(_d16(), _REXLrr(0, RD),	_O_W		(((OP) << 3) + 5					,_su16(IM))) : \
					(_d16(), _REXLrr(0, RD),	_Os_Mrm_sW	(0x81		,_b11,OP     ,_r2(RD)			,_su16(IM))) )
#define _ALUWim(OP, IM, MD, MB, MI, MS)	(_d16(), _REXLrm(0, MB, MI),	_Os_r_X_sW	(0x81		     ,OP		,MD,MB,MI,MS	,_su16(IM)))

#define _ALULrr(OP, RS, RD)		(_REXLrr(RS, RD),		_O_Mrm		(((OP) << 3) + 1,_b11,_r4(RS),_r4(RD)				))
#define _ALULmr(OP, MD, MB, MI, MS, RD)	(_REXLmr(MB, MI, RD),		_O_r_X		(((OP) << 3) + 3     ,_r4(RD)		,MD,MB,MI,MS		))
#define _ALULrm(OP, RS, MD, MB, MI, MS)	(_REXLrm(RS, MB, MI),		_O_r_X		(((OP) << 3) + 1     ,_r4(RS)		,MD,MB,MI,MS		))
#define _ALULir(OP, IM, RD)		(X86_OPTIMIZE_ALU && ((RD) == X86_EAX) ? \
					(_REXLrr(0, RD),		_O_L		(((OP) << 3) + 5					,IM	)) : \
					(_REXLrr(0, RD),		_Os_Mrm_sL	(0x81		,_b11,OP     ,_r4(RD)			,IM	)) )
#define _ALULim(OP, IM, MD, MB, MI, MS)	(_REXLrm(0, MB, MI),		_Os_r_X_sL	(0x81		     ,OP		,MD,MB,MI,MS	,IM	))

#define _ALUQrr(OP, RS, RD)		(_REXQrr(RS, RD),		_O_Mrm		(((OP) << 3) + 1,_b11,_r8(RS),_r8(RD)				))
#define _ALUQmr(OP, MD, MB, MI, MS, RD)	(_REXQmr(MB, MI, RD),		_O_r_X		(((OP) << 3) + 3     ,_r8(RD)		,MD,MB,MI,MS		))
#define _ALUQrm(OP, RS, MD, MB, MI, MS)	(_REXQrm(RS, MB, MI),		_O_r_X		(((OP) << 3) + 1     ,_r8(RS)		,MD,MB,MI,MS		))
#define _ALUQir(OP, IM, RD)		(X86_OPTIMIZE_ALU && ((RD) == X86_RAX) ? \
					(_REXQrr(0, RD),		_O_L		(((OP) << 3) + 5					,IM	)) : \
					(_REXQrr(0, RD),		_Os_Mrm_sL	(0x81		,_b11,OP     ,_r8(RD)			,IM	)) )
#define _ALUQim(OP, IM, MD, MB, MI, MS)	(_REXQrm(0, MB, MI),		_Os_r_X_sL	(0x81		     ,OP		,MD,MB,MI,MS	,IM	))

#define ADCBrr(RS, RD)			_ALUBrr(X86_ADC, RS, RD)
#define ADCBmr(MD, MB, MI, MS, RD)	_ALUBmr(X86_ADC, MD, MB, MI, MS, RD)
#define ADCBrm(RS, MD, MB, MI, MS)	_ALUBrm(X86_ADC, RS, MD, MB, MI, MS)
#define ADCBir(IM, RD)			_ALUBir(X86_ADC, IM, RD)
#define ADCBim(IM, MD, MB, MI, MS)	_ALUBim(X86_ADC, IM, MD, MB, MI, MS)

#define ADCWrr(RS, RD)			_ALUWrr(X86_ADC, RS, RD)
#define ADCWmr(MD, MB, MI, MS, RD)	_ALUWmr(X86_ADC, MD, MB, MI, MS, RD)
#define ADCWrm(RS, MD, MB, MI, MS)	_ALUWrm(X86_ADC, RS, MD, MB, MI, MS)
#define ADCWir(IM, RD)			_ALUWir(X86_ADC, IM, RD)
#define ADCWim(IM, MD, MB, MI, MS)	_ALUWim(X86_ADC, IM, MD, MB, MI, MS)

#define ADCLrr(RS, RD)			_ALULrr(X86_ADC, RS, RD)
#define ADCLmr(MD, MB, MI, MS, RD)	_ALULmr(X86_ADC, MD, MB, MI, MS, RD)
#define ADCLrm(RS, MD, MB, MI, MS)	_ALULrm(X86_ADC, RS, MD, MB, MI, MS)
#define ADCLir(IM, RD)			_ALULir(X86_ADC, IM, RD)
#define ADCLim(IM, MD, MB, MI, MS)	_ALULim(X86_ADC, IM, MD, MB, MI, MS)

#define ADCQrr(RS, RD)			_ALUQrr(X86_ADC, RS, RD)
#define ADCQmr(MD, MB, MI, MS, RD)	_ALUQmr(X86_ADC, MD, MB, MI, MS, RD)
#define ADCQrm(RS, MD, MB, MI, MS)	_ALUQrm(X86_ADC, RS, MD, MB, MI, MS)
#define ADCQir(IM, RD)			_ALUQir(X86_ADC, IM, RD)
#define ADCQim(IM, MD, MB, MI, MS)	_ALUQim(X86_ADC, IM, MD, MB, MI, MS)

#define ADDBrr(RS, RD)			_ALUBrr(X86_ADD, RS, RD)
#define ADDBmr(MD, MB, MI, MS, RD)	_ALUBmr(X86_ADD, MD, MB, MI, MS, RD)
#define ADDBrm(RS, MD, MB, MI, MS)	_ALUBrm(X86_ADD, RS, MD, MB, MI, MS)
#define ADDBir(IM, RD)			_ALUBir(X86_ADD, IM, RD)
#define ADDBim(IM, MD, MB, MI, MS)	_ALUBim(X86_ADD, IM, MD, MB, MI, MS)

#define ADDWrr(RS, RD)			_ALUWrr(X86_ADD, RS, RD)
#define ADDWmr(MD, MB, MI, MS, RD)	_ALUWmr(X86_ADD, MD, MB, MI, MS, RD)
#define ADDWrm(RS, MD, MB, MI, MS)	_ALUWrm(X86_ADD, RS, MD, MB, MI, MS)
#define ADDWir(IM, RD)			_ALUWir(X86_ADD, IM, RD)
#define ADDWim(IM, MD, MB, MI, MS)	_ALUWim(X86_ADD, IM, MD, MB, MI, MS)

#define ADDLrr(RS, RD)			_ALULrr(X86_ADD, RS, RD)
#define ADDLmr(MD, MB, MI, MS, RD)	_ALULmr(X86_ADD, MD, MB, MI, MS, RD)
#define ADDLrm(RS, MD, MB, MI, MS)	_ALULrm(X86_ADD, RS, MD, MB, MI, MS)
#define ADDLir(IM, RD)			_ALULir(X86_ADD, IM, RD)
#define ADDLim(IM, MD, MB, MI, MS)	_ALULim(X86_ADD, IM, MD, MB, MI, MS)

#define ADDQrr(RS, RD)			_ALUQrr(X86_ADD, RS, RD)
#define ADDQmr(MD, MB, MI, MS, RD)	_ALUQmr(X86_ADD, MD, MB, MI, MS, RD)
#define ADDQrm(RS, MD, MB, MI, MS)	_ALUQrm(X86_ADD, RS, MD, MB, MI, MS)
#define ADDQir(IM, RD)			_ALUQir(X86_ADD, IM, RD)
#define ADDQim(IM, MD, MB, MI, MS)	_ALUQim(X86_ADD, IM, MD, MB, MI, MS)

#define ANDBrr(RS, RD)			_ALUBrr(X86_AND, RS, RD)
#define ANDBmr(MD, MB, MI, MS, RD)	_ALUBmr(X86_AND, MD, MB, MI, MS, RD)
#define ANDBrm(RS, MD, MB, MI, MS)	_ALUBrm(X86_AND, RS, MD, MB, MI, MS)
#define ANDBir(IM, RD)			_ALUBir(X86_AND, IM, RD)
#define ANDBim(IM, MD, MB, MI, MS)	_ALUBim(X86_AND, IM, MD, MB, MI, MS)

#define ANDWrr(RS, RD)			_ALUWrr(X86_AND, RS, RD)
#define ANDWmr(MD, MB, MI, MS, RD)	_ALUWmr(X86_AND, MD, MB, MI, MS, RD)
#define ANDWrm(RS, MD, MB, MI, MS)	_ALUWrm(X86_AND, RS, MD, MB, MI, MS)
#define ANDWir(IM, RD)			_ALUWir(X86_AND, IM, RD)
#define ANDWim(IM, MD, MB, MI, MS)	_ALUWim(X86_AND, IM, MD, MB, MI, MS)

#define ANDLrr(RS, RD)			_ALULrr(X86_AND, RS, RD)
#define ANDLmr(MD, MB, MI, MS, RD)	_ALULmr(X86_AND, MD, MB, MI, MS, RD)
#define ANDLrm(RS, MD, MB, MI, MS)	_ALULrm(X86_AND, RS, MD, MB, MI, MS)
#define ANDLir(IM, RD)			_ALULir(X86_AND, IM, RD)
#define ANDLim(IM, MD, MB, MI, MS)	_ALULim(X86_AND, IM, MD, MB, MI, MS)

#define ANDQrr(RS, RD)			_ALUQrr(X86_AND, RS, RD)
#define ANDQmr(MD, MB, MI, MS, RD)	_ALUQmr(X86_AND, MD, MB, MI, MS, RD)
#define ANDQrm(RS, MD, MB, MI, MS)	_ALUQrm(X86_AND, RS, MD, MB, MI, MS)
#define ANDQir(IM, RD)			_ALUQir(X86_AND, IM, RD)
#define ANDQim(IM, MD, MB, MI, MS)	_ALUQim(X86_AND, IM, MD, MB, MI, MS)

#define CMPBrr(RS, RD)			_ALUBrr(X86_CMP, RS, RD)
#define CMPBmr(MD, MB, MI, MS, RD)	_ALUBmr(X86_CMP, MD, MB, MI, MS, RD)
#define CMPBrm(RS, MD, MB, MI, MS)	_ALUBrm(X86_CMP, RS, MD, MB, MI, MS)
#define CMPBir(IM, RD)			_ALUBir(X86_CMP, IM, RD)
#define CMPBim(IM, MD, MB, MI, MS)	_ALUBim(X86_CMP, IM, MD, MB, MI, MS)

#define CMPWrr(RS, RD)			_ALUWrr(X86_CMP, RS, RD)
#define CMPWmr(MD, MB, MI, MS, RD)	_ALUWmr(X86_CMP, MD, MB, MI, MS, RD)
#define CMPWrm(RS, MD, MB, MI, MS)	_ALUWrm(X86_CMP, RS, MD, MB, MI, MS)
#define CMPWir(IM, RD)			_ALUWir(X86_CMP, IM, RD)
#define CMPWim(IM, MD, MB, MI, MS)	_ALUWim(X86_CMP, IM, MD, MB, MI, MS)

#define CMPLrr(RS, RD)			_ALULrr(X86_CMP, RS, RD)
#define CMPLmr(MD, MB, MI, MS, RD)	_ALULmr(X86_CMP, MD, MB, MI, MS, RD)
#define CMPLrm(RS, MD, MB, MI, MS)	_ALULrm(X86_CMP, RS, MD, MB, MI, MS)
#define CMPLir(IM, RD)			_ALULir(X86_CMP, IM, RD)
#define CMPLim(IM, MD, MB, MI, MS)	_ALULim(X86_CMP, IM, MD, MB, MI, MS)

#define CMPQrr(RS, RD)			_ALUQrr(X86_CMP, RS, RD)
#define CMPQmr(MD, MB, MI, MS, RD)	_ALUQmr(X86_CMP, MD, MB, MI, MS, RD)
#define CMPQrm(RS, MD, MB, MI, MS)	_ALUQrm(X86_CMP, RS, MD, MB, MI, MS)
#define CMPQir(IM, RD)			_ALUQir(X86_CMP, IM, RD)
#define CMPQim(IM, MD, MB, MI, MS)	_ALUQim(X86_CMP, IM, MD, MB, MI, MS)

#define ORBrr(RS, RD)			_ALUBrr(X86_OR, RS, RD)
#define ORBmr(MD, MB, MI, MS, RD)	_ALUBmr(X86_OR, MD, MB, MI, MS, RD)
#define ORBrm(RS, MD, MB, MI, MS)	_ALUBrm(X86_OR, RS, MD, MB, MI, MS)
#define ORBir(IM, RD)			_ALUBir(X86_OR, IM, RD)
#define ORBim(IM, MD, MB, MI, MS)	_ALUBim(X86_OR, IM, MD, MB, MI, MS)

#define ORWrr(RS, RD)			_ALUWrr(X86_OR, RS, RD)
#define ORWmr(MD, MB, MI, MS, RD)	_ALUWmr(X86_OR, MD, MB, MI, MS, RD)
#define ORWrm(RS, MD, MB, MI, MS)	_ALUWrm(X86_OR, RS, MD, MB, MI, MS)
#define ORWir(IM, RD)			_ALUWir(X86_OR, IM, RD)
#define ORWim(IM, MD, MB, MI, MS)	_ALUWim(X86_OR, IM, MD, MB, MI, MS)

#define ORLrr(RS, RD)			_ALULrr(X86_OR, RS, RD)
#define ORLmr(MD, MB, MI, MS, RD)	_ALULmr(X86_OR, MD, MB, MI, MS, RD)
#define ORLrm(RS, MD, MB, MI, MS)	_ALULrm(X86_OR, RS, MD, MB, MI, MS)
#define ORLir(IM, RD)			_ALULir(X86_OR, IM, RD)
#define ORLim(IM, MD, MB, MI, MS)	_ALULim(X86_OR, IM, MD, MB, MI, MS)

#define ORQrr(RS, RD)			_ALUQrr(X86_OR, RS, RD)
#define ORQmr(MD, MB, MI, MS, RD)	_ALUQmr(X86_OR, MD, MB, MI, MS, RD)
#define ORQrm(RS, MD, MB, MI, MS)	_ALUQrm(X86_OR, RS, MD, MB, MI, MS)
#define ORQir(IM, RD)			_ALUQir(X86_OR, IM, RD)
#define ORQim(IM, MD, MB, MI, MS)	_ALUQim(X86_OR, IM, MD, MB, MI, MS)

#define SBBBrr(RS, RD)			_ALUBrr(X86_SBB, RS, RD)
#define SBBBmr(MD, MB, MI, MS, RD)	_ALUBmr(X86_SBB, MD, MB, MI, MS, RD)
#define SBBBrm(RS, MD, MB, MI, MS)	_ALUBrm(X86_SBB, RS, MD, MB, MI, MS)
#define SBBBir(IM, RD)			_ALUBir(X86_SBB, IM, RD)
#define SBBBim(IM, MD, MB, MI, MS)	_ALUBim(X86_SBB, IM, MD, MB, MI, MS)

#define SBBWrr(RS, RD)			_ALUWrr(X86_SBB, RS, RD)
#define SBBWmr(MD, MB, MI, MS, RD)	_ALUWmr(X86_SBB, MD, MB, MI, MS, RD)
#define SBBWrm(RS, MD, MB, MI, MS)	_ALUWrm(X86_SBB, RS, MD, MB, MI, MS)
#define SBBWir(IM, RD)			_ALUWir(X86_SBB, IM, RD)
#define SBBWim(IM, MD, MB, MI, MS)	_ALUWim(X86_SBB, IM, MD, MB, MI, MS)

#define SBBLrr(RS, RD)			_ALULrr(X86_SBB, RS, RD)
#define SBBLmr(MD, MB, MI, MS, RD)	_ALULmr(X86_SBB, MD, MB, MI, MS, RD)
#define SBBLrm(RS, MD, MB, MI, MS)	_ALULrm(X86_SBB, RS, MD, MB, MI, MS)
#define SBBLir(IM, RD)			_ALULir(X86_SBB, IM, RD)
#define SBBLim(IM, MD, MB, MI, MS)	_ALULim(X86_SBB, IM, MD, MB, MI, MS)

#define SBBQrr(RS, RD)			_ALUQrr(X86_SBB, RS, RD)
#define SBBQmr(MD, MB, MI, MS, RD)	_ALUQmr(X86_SBB, MD, MB, MI, MS, RD)
#define SBBQrm(RS, MD, MB, MI, MS)	_ALUQrm(X86_SBB, RS, MD, MB, MI, MS)
#define SBBQir(IM, RD)			_ALUQir(X86_SBB, IM, RD)
#define SBBQim(IM, MD, MB, MI, MS)	_ALUQim(X86_SBB, IM, MD, MB, MI, MS)

#define SUBBrr(RS, RD)			_ALUBrr(X86_SUB, RS, RD)
#define SUBBmr(MD, MB, MI, MS, RD)	_ALUBmr(X86_SUB, MD, MB, MI, MS, RD)
#define SUBBrm(RS, MD, MB, MI, MS)	_ALUBrm(X86_SUB, RS, MD, MB, MI, MS)
#define SUBBir(IM, RD)			_ALUBir(X86_SUB, IM, RD)
#define SUBBim(IM, MD, MB, MI, MS)	_ALUBim(X86_SUB, IM, MD, MB, MI, MS)

#define SUBWrr(RS, RD)			_ALUWrr(X86_SUB, RS, RD)
#define SUBWmr(MD, MB, MI, MS, RD)	_ALUWmr(X86_SUB, MD, MB, MI, MS, RD)
#define SUBWrm(RS, MD, MB, MI, MS)	_ALUWrm(X86_SUB, RS, MD, MB, MI, MS)
#define SUBWir(IM, RD)			_ALUWir(X86_SUB, IM, RD)
#define SUBWim(IM, MD, MB, MI, MS)	_ALUWim(X86_SUB, IM, MD, MB, MI, MS)

#define SUBLrr(RS, RD)			_ALULrr(X86_SUB, RS, RD)
#define SUBLmr(MD, MB, MI, MS, RD)	_ALULmr(X86_SUB, MD, MB, MI, MS, RD)
#define SUBLrm(RS, MD, MB, MI, MS)	_ALULrm(X86_SUB, RS, MD, MB, MI, MS)
#define SUBLir(IM, RD)			_ALULir(X86_SUB, IM, RD)
#define SUBLim(IM, MD, MB, MI, MS)	_ALULim(X86_SUB, IM, MD, MB, MI, MS)

#define SUBQrr(RS, RD)			_ALUQrr(X86_SUB, RS, RD)
#define SUBQmr(MD, MB, MI, MS, RD)	_ALUQmr(X86_SUB, MD, MB, MI, MS, RD)
#define SUBQrm(RS, MD, MB, MI, MS)	_ALUQrm(X86_SUB, RS, MD, MB, MI, MS)
#define SUBQir(IM, RD)			_ALUQir(X86_SUB, IM, RD)
#define SUBQim(IM, MD, MB, MI, MS)	_ALUQim(X86_SUB, IM, MD, MB, MI, MS)

#define XORBrr(RS, RD)			_ALUBrr(X86_XOR, RS, RD)
#define XORBmr(MD, MB, MI, MS, RD)	_ALUBmr(X86_XOR, MD, MB, MI, MS, RD)
#define XORBrm(RS, MD, MB, MI, MS)	_ALUBrm(X86_XOR, RS, MD, MB, MI, MS)
#define XORBir(IM, RD)			_ALUBir(X86_XOR, IM, RD)
#define XORBim(IM, MD, MB, MI, MS)	_ALUBim(X86_XOR, IM, MD, MB, MI, MS)

#define XORWrr(RS, RD)			_ALUWrr(X86_XOR, RS, RD)
#define XORWmr(MD, MB, MI, MS, RD)	_ALUWmr(X86_XOR, MD, MB, MI, MS, RD)
#define XORWrm(RS, MD, MB, MI, MS)	_ALUWrm(X86_XOR, RS, MD, MB, MI, MS)
#define XORWir(IM, RD)			_ALUWir(X86_XOR, IM, RD)
#define XORWim(IM, MD, MB, MI, MS)	_ALUWim(X86_XOR, IM, MD, MB, MI, MS)

#define XORLrr(RS, RD)			_ALULrr(X86_XOR, RS, RD)
#define XORLmr(MD, MB, MI, MS, RD)	_ALULmr(X86_XOR, MD, MB, MI, MS, RD)
#define XORLrm(RS, MD, MB, MI, MS)	_ALULrm(X86_XOR, RS, MD, MB, MI, MS)
#define XORLir(IM, RD)			_ALULir(X86_XOR, IM, RD)
#define XORLim(IM, MD, MB, MI, MS)	_ALULim(X86_XOR, IM, MD, MB, MI, MS)

#define XORQrr(RS, RD)			_ALUQrr(X86_XOR, RS, RD)
#define XORQmr(MD, MB, MI, MS, RD)	_ALUQmr(X86_XOR, MD, MB, MI, MS, RD)
#define XORQrm(RS, MD, MB, MI, MS)	_ALUQrm(X86_XOR, RS, MD, MB, MI, MS)
#define XORQir(IM, RD)			_ALUQir(X86_XOR, IM, RD)
#define XORQim(IM, MD, MB, MI, MS)	_ALUQim(X86_XOR, IM, MD, MB, MI, MS)


/* --- Shift/Rotate instructions ------------------------------------------- */

enum {
  X86_ROL = 0,
  X86_ROR = 1,
  X86_RCL = 2,
  X86_RCR = 3,
  X86_SHL = 4,
  X86_SHR = 5,
  X86_SAR = 7,
};

/*									_format		Opcd		,Mod ,r	    ,m		,mem=dsp+sib	,imm... */

#define _ROTSHIBir(OP,IM,RD)		(X86_OPTIMIZE_ROTSHI && ((IM) == 1) ? \
					(_REXBrr(0, RD),		_O_Mrm		(0xd0		,_b11,OP,_r1(RD)				)) : \
					(_REXBrr(0, RD),		_O_Mrm_B	(0xc0		,_b11,OP,_r1(RD)			,_u8(IM))) )
#define _ROTSHIBim(OP,IM,MD,MB,MI,MS)	(X86_OPTIMIZE_ROTSHI && ((IM) == 1) ? \
					(_REXBrm(0, MB, MI),		_O_r_X		(0xd0		     ,OP		,MD,MB,MI,MS		)) : \
					(_REXBrm(0, MB, MI),		_O_r_X_B	(0xc0		     ,OP		,MD,MB,MI,MS	,_u8(IM))) )
#define _ROTSHIBrr(OP,RS,RD)		(((RS) == X86_CL) ? \
					(_REXBrr(RS, RD),		_O_Mrm		(0xd2		,_b11,OP,_r1(RD)				)) : \
									x86_emit_failure("source register must be CL"					)  )
#define _ROTSHIBrm(OP,RS,MD,MB,MI,MS)	(((RS) == X86_CL) ? \
					(_REXBrm(RS, MB, MI),		_O_r_X		(0xd2		     ,OP		,MD,MB,MI,MS		)) : \
									x86_emit_failure("source register must be CL"					)  )

#define _ROTSHIWir(OP,IM,RD)		(X86_OPTIMIZE_ROTSHI && ((IM) == 1) ? \
					(_d16(), _REXLrr(0, RD),	_O_Mrm		(0xd1		,_b11,OP,_r2(RD)				)) : \
					(_d16(), _REXLrr(0, RD),	_O_Mrm_B	(0xc1		,_b11,OP,_r2(RD)			,_u8(IM))) )
#define _ROTSHIWim(OP,IM,MD,MB,MI,MS)	(X86_OPTIMIZE_ROTSHI && ((IM) == 1) ? \
					(_d16(), _REXLrm(0, MB, MI),	_O_r_X		(0xd1		     ,OP		,MD,MB,MI,MS		)) : \
					(_d16(), _REXLrm(0, MB, MI),	_O_r_X_B	(0xc1		     ,OP		,MD,MB,MI,MS	,_u8(IM))) )
#define _ROTSHIWrr(OP,RS,RD)		(((RS) == X86_CL) ? \
					(_d16(), _REXLrr(RS, RD),	_O_Mrm		(0xd3		,_b11,OP,_r2(RD)				)) : \
									x86_emit_failure("source register must be CL"					)  )
#define _ROTSHIWrm(OP,RS,MD,MB,MI,MS)	(((RS) == X86_CL) ? \
					(_d16(), _REXLrm(RS, MB, MI),	_O_r_X		(0xd3		     ,OP		,MD,MB,MI,MS		)) : \
									x86_emit_failure("source register must be CL"					)  )

#define _ROTSHILir(OP,IM,RD)		(X86_OPTIMIZE_ROTSHI && ((IM) == 1) ? \
					(_REXLrr(0, RD),		_O_Mrm		(0xd1		,_b11,OP,_r4(RD)				)) : \
					(_REXLrr(0, RD),		_O_Mrm_B	(0xc1		,_b11,OP,_r4(RD)			,_u8(IM))) )
#define _ROTSHILim(OP,IM,MD,MB,MI,MS)	(X86_OPTIMIZE_ROTSHI && ((IM) == 1) ? \
					(_REXLrm(0, MB, MI),		_O_r_X		(0xd1		     ,OP		,MD,MB,MI,MS		)) : \
					(_REXLrm(0, MB, MI),		_O_r_X_B	(0xc1		     ,OP		,MD,MB,MI,MS	,_u8(IM))) )
#define _ROTSHILrr(OP,RS,RD)		(((RS) == X86_CL) ? \
					(_REXLrr(RS, RD),		_O_Mrm		(0xd3		,_b11,OP,_r4(RD)				)) : \
									x86_emit_failure("source register must be CL"					)  )
#define _ROTSHILrm(OP,RS,MD,MB,MI,MS)	(((RS) == X86_CL) ? \
					(_REXLrm(RS, MB, MI),		_O_r_X		(0xd3		     ,OP		,MD,MB,MI,MS		)) : \
									x86_emit_failure("source register must be CL"					)  )

#define _ROTSHIQir(OP,IM,RD)		(X86_OPTIMIZE_ROTSHI && ((IM) == 1) ? \
					(_REXQrr(0, RD),		_O_Mrm		(0xd1		,_b11,OP,_r8(RD)				)) : \
					(_REXQrr(0, RD),		_O_Mrm_B	(0xc1		,_b11,OP,_r8(RD)			,_u8(IM))) )
#define _ROTSHIQim(OP,IM,MD,MB,MI,MS)	(X86_OPTIMIZE_ROTSHI && ((IM) == 1) ? \
					(_REXQrm(0, MB, MI),		_O_r_X		(0xd1		     ,OP		,MD,MB,MI,MS		)) : \
					(_REXQrm(0, MB, MI),		_O_r_X_B	(0xc1		     ,OP		,MD,MB,MI,MS	,_u8(IM))) )
#define _ROTSHIQrr(OP,RS,RD)		(((RS) == X86_CL) ? \
					(_REXQrr(RS, RD),		_O_Mrm		(0xd3		,_b11,OP,_r8(RD)				)) : \
									x86_emit_failure("source register must be CL"					)  )
#define _ROTSHIQrm(OP,RS,MD,MB,MI,MS)	(((RS) == X86_CL) ? \
					(_REXQrm(RS, MB, MI),		_O_r_X		(0xd3		     ,OP		,MD,MB,MI,MS		)) : \
									x86_emit_failure("source register must be CL"					)  )

#define ROLBir(IM, RD)			_ROTSHIBir(X86_ROL, IM, RD)
#define ROLBim(IM, MD, MB, MI, MS)	_ROTSHIBim(X86_ROL, IM, MD, MB, MI, MS)
#define ROLBrr(RS, RD)			_ROTSHIBrr(X86_ROL, RS, RD)
#define ROLBrm(RS, MD, MB, MI, MS)	_ROTSHIBrm(X86_ROL, RS, MD, MB, MI, MS)

#define ROLWir(IM, RD)			_ROTSHIWir(X86_ROL, IM, RD)
#define ROLWim(IM, MD, MB, MI, MS)	_ROTSHIWim(X86_ROL, IM, MD, MB, MI, MS)
#define ROLWrr(RS, RD)			_ROTSHIWrr(X86_ROL, RS, RD)
#define ROLWrm(RS, MD, MB, MI, MS)	_ROTSHIWrm(X86_ROL, RS, MD, MB, MI, MS)

#define ROLLir(IM, RD)			_ROTSHILir(X86_ROL, IM, RD)
#define ROLLim(IM, MD, MB, MI, MS)	_ROTSHILim(X86_ROL, IM, MD, MB, MI, MS)
#define ROLLrr(RS, RD)			_ROTSHILrr(X86_ROL, RS, RD)
#define ROLLrm(RS, MD, MB, MI, MS)	_ROTSHILrm(X86_ROL, RS, MD, MB, MI, MS)

#define ROLQir(IM, RD)			_ROTSHIQir(X86_ROL, IM, RD)
#define ROLQim(IM, MD, MB, MI, MS)	_ROTSHIQim(X86_ROL, IM, MD, MB, MI, MS)
#define ROLQrr(RS, RD)			_ROTSHIQrr(X86_ROL, RS, RD)
#define ROLQrm(RS, MD, MB, MI, MS)	_ROTSHIQrm(X86_ROL, RS, MD, MB, MI, MS)

#define RORBir(IM, RD)			_ROTSHIBir(X86_ROR, IM, RD)
#define RORBim(IM, MD, MB, MI, MS)	_ROTSHIBim(X86_ROR, IM, MD, MB, MI, MS)
#define RORBrr(RS, RD)			_ROTSHIBrr(X86_ROR, RS, RD)
#define RORBrm(RS, MD, MB, MI, MS)	_ROTSHIBrm(X86_ROR, RS, MD, MB, MI, MS)

#define RORWir(IM, RD)			_ROTSHIWir(X86_ROR, IM, RD)
#define RORWim(IM, MD, MB, MI, MS)	_ROTSHIWim(X86_ROR, IM, MD, MB, MI, MS)
#define RORWrr(RS, RD)			_ROTSHIWrr(X86_ROR, RS, RD)
#define RORWrm(RS, MD, MB, MI, MS)	_ROTSHIWrm(X86_ROR, RS, MD, MB, MI, MS)

#define RORLir(IM, RD)			_ROTSHILir(X86_ROR, IM, RD)
#define RORLim(IM, MD, MB, MI, MS)	_ROTSHILim(X86_ROR, IM, MD, MB, MI, MS)
#define RORLrr(RS, RD)			_ROTSHILrr(X86_ROR, RS, RD)
#define RORLrm(RS, MD, MB, MI, MS)	_ROTSHILrm(X86_ROR, RS, MD, MB, MI, MS)

#define RORQir(IM, RD)			_ROTSHIQir(X86_ROR, IM, RD)
#define RORQim(IM, MD, MB, MI, MS)	_ROTSHIQim(X86_ROR, IM, MD, MB, MI, MS)
#define RORQrr(RS, RD)			_ROTSHIQrr(X86_ROR, RS, RD)
#define RORQrm(RS, MD, MB, MI, MS)	_ROTSHIQrm(X86_ROR, RS, MD, MB, MI, MS)

#define RCLBir(IM, RD)			_ROTSHIBir(X86_RCL, IM, RD)
#define RCLBim(IM, MD, MB, MI, MS)	_ROTSHIBim(X86_RCL, IM, MD, MB, MI, MS)
#define RCLBrr(RS, RD)			_ROTSHIBrr(X86_RCL, RS, RD)
#define RCLBrm(RS, MD, MB, MI, MS)	_ROTSHIBrm(X86_RCL, RS, MD, MB, MI, MS)

#define RCLWir(IM, RD)			_ROTSHIWir(X86_RCL, IM, RD)
#define RCLWim(IM, MD, MB, MI, MS)	_ROTSHIWim(X86_RCL, IM, MD, MB, MI, MS)
#define RCLWrr(RS, RD)			_ROTSHIWrr(X86_RCL, RS, RD)
#define RCLWrm(RS, MD, MB, MI, MS)	_ROTSHIWrm(X86_RCL, RS, MD, MB, MI, MS)

#define RCLLir(IM, RD)			_ROTSHILir(X86_RCL, IM, RD)
#define RCLLim(IM, MD, MB, MI, MS)	_ROTSHILim(X86_RCL, IM, MD, MB, MI, MS)
#define RCLLrr(RS, RD)			_ROTSHILrr(X86_RCL, RS, RD)
#define RCLLrm(RS, MD, MB, MI, MS)	_ROTSHILrm(X86_RCL, RS, MD, MB, MI, MS)

#define RCLQir(IM, RD)			_ROTSHIQir(X86_RCL, IM, RD)
#define RCLQim(IM, MD, MB, MI, MS)	_ROTSHIQim(X86_RCL, IM, MD, MB, MI, MS)
#define RCLQrr(RS, RD)			_ROTSHIQrr(X86_RCL, RS, RD)
#define RCLQrm(RS, MD, MB, MI, MS)	_ROTSHIQrm(X86_RCL, RS, MD, MB, MI, MS)

#define RCRBir(IM, RD)			_ROTSHIBir(X86_RCR, IM, RD)
#define RCRBim(IM, MD, MB, MI, MS)	_ROTSHIBim(X86_RCR, IM, MD, MB, MI, MS)
#define RCRBrr(RS, RD)			_ROTSHIBrr(X86_RCR, RS, RD)
#define RCRBrm(RS, MD, MB, MI, MS)	_ROTSHIBrm(X86_RCR, RS, MD, MB, MI, MS)

#define RCRWir(IM, RD)			_ROTSHIWir(X86_RCR, IM, RD)
#define RCRWim(IM, MD, MB, MI, MS)	_ROTSHIWim(X86_RCR, IM, MD, MB, MI, MS)
#define RCRWrr(RS, RD)			_ROTSHIWrr(X86_RCR, RS, RD)
#define RCRWrm(RS, MD, MB, MI, MS)	_ROTSHIWrm(X86_RCR, RS, MD, MB, MI, MS)

#define RCRLir(IM, RD)			_ROTSHILir(X86_RCR, IM, RD)
#define RCRLim(IM, MD, MB, MI, MS)	_ROTSHILim(X86_RCR, IM, MD, MB, MI, MS)
#define RCRLrr(RS, RD)			_ROTSHILrr(X86_RCR, RS, RD)
#define RCRLrm(RS, MD, MB, MI, MS)	_ROTSHILrm(X86_RCR, RS, MD, MB, MI, MS)

#define RCRQir(IM, RD)			_ROTSHIQir(X86_RCR, IM, RD)
#define RCRQim(IM, MD, MB, MI, MS)	_ROTSHIQim(X86_RCR, IM, MD, MB, MI, MS)
#define RCRQrr(RS, RD)			_ROTSHIQrr(X86_RCR, RS, RD)
#define RCRQrm(RS, MD, MB, MI, MS)	_ROTSHIQrm(X86_RCR, RS, MD, MB, MI, MS)

#define SHLBir(IM, RD)			_ROTSHIBir(X86_SHL, IM, RD)
#define SHLBim(IM, MD, MB, MI, MS)	_ROTSHIBim(X86_SHL, IM, MD, MB, MI, MS)
#define SHLBrr(RS, RD)			_ROTSHIBrr(X86_SHL, RS, RD)
#define SHLBrm(RS, MD, MB, MI, MS)	_ROTSHIBrm(X86_SHL, RS, MD, MB, MI, MS)

#define SHLWir(IM, RD)			_ROTSHIWir(X86_SHL, IM, RD)
#define SHLWim(IM, MD, MB, MI, MS)	_ROTSHIWim(X86_SHL, IM, MD, MB, MI, MS)
#define SHLWrr(RS, RD)			_ROTSHIWrr(X86_SHL, RS, RD)
#define SHLWrm(RS, MD, MB, MI, MS)	_ROTSHIWrm(X86_SHL, RS, MD, MB, MI, MS)

#define SHLLir(IM, RD)			_ROTSHILir(X86_SHL, IM, RD)
#define SHLLim(IM, MD, MB, MI, MS)	_ROTSHILim(X86_SHL, IM, MD, MB, MI, MS)
#define SHLLrr(RS, RD)			_ROTSHILrr(X86_SHL, RS, RD)
#define SHLLrm(RS, MD, MB, MI, MS)	_ROTSHILrm(X86_SHL, RS, MD, MB, MI, MS)

#define SHLQir(IM, RD)			_ROTSHIQir(X86_SHL, IM, RD)
#define SHLQim(IM, MD, MB, MI, MS)	_ROTSHIQim(X86_SHL, IM, MD, MB, MI, MS)
#define SHLQrr(RS, RD)			_ROTSHIQrr(X86_SHL, RS, RD)
#define SHLQrm(RS, MD, MB, MI, MS)	_ROTSHIQrm(X86_SHL, RS, MD, MB, MI, MS)

#define SHRBir(IM, RD)			_ROTSHIBir(X86_SHR, IM, RD)
#define SHRBim(IM, MD, MB, MI, MS)	_ROTSHIBim(X86_SHR, IM, MD, MB, MI, MS)
#define SHRBrr(RS, RD)			_ROTSHIBrr(X86_SHR, RS, RD)
#define SHRBrm(RS, MD, MB, MI, MS)	_ROTSHIBrm(X86_SHR, RS, MD, MB, MI, MS)

#define SHRWir(IM, RD)			_ROTSHIWir(X86_SHR, IM, RD)
#define SHRWim(IM, MD, MB, MI, MS)	_ROTSHIWim(X86_SHR, IM, MD, MB, MI, MS)
#define SHRWrr(RS, RD)			_ROTSHIWrr(X86_SHR, RS, RD)
#define SHRWrm(RS, MD, MB, MI, MS)	_ROTSHIWrm(X86_SHR, RS, MD, MB, MI, MS)

#define SHRLir(IM, RD)			_ROTSHILir(X86_SHR, IM, RD)
#define SHRLim(IM, MD, MB, MI, MS)	_ROTSHILim(X86_SHR, IM, MD, MB, MI, MS)
#define SHRLrr(RS, RD)			_ROTSHILrr(X86_SHR, RS, RD)
#define SHRLrm(RS, MD, MB, MI, MS)	_ROTSHILrm(X86_SHR, RS, MD, MB, MI, MS)

#define SHRQir(IM, RD)			_ROTSHIQir(X86_SHR, IM, RD)
#define SHRQim(IM, MD, MB, MI, MS)	_ROTSHIQim(X86_SHR, IM, MD, MB, MI, MS)
#define SHRQrr(RS, RD)			_ROTSHIQrr(X86_SHR, RS, RD)
#define SHRQrm(RS, MD, MB, MI, MS)	_ROTSHIQrm(X86_SHR, RS, MD, MB, MI, MS)

#define SALBir				SHLBir
#define SALBim				SHLBim
#define SALBrr				SHLBrr
#define SALBrm				SHLBrm

#define SALWir				SHLWir
#define SALWim				SHLWim
#define SALWrr				SHLWrr
#define SALWrm				SHLWrm

#define SALLir				SHLLir
#define SALLim				SHLLim
#define SALLrr				SHLLrr
#define SALLrm				SHLLrm

#define SALQir				SHLQir
#define SALQim				SHLQim
#define SALQrr				SHLQrr
#define SALQrm				SHLQrm

#define SARBir(IM, RD)			_ROTSHIBir(X86_SAR, IM, RD)
#define SARBim(IM, MD, MB, MI, MS)	_ROTSHIBim(X86_SAR, IM, MD, MB, MI, MS)
#define SARBrr(RS, RD)			_ROTSHIBrr(X86_SAR, RS, RD)
#define SARBrm(RS, MD, MB, MI, MS)	_ROTSHIBrm(X86_SAR, RS, MD, MB, MI, MS)

#define SARWir(IM, RD)			_ROTSHIWir(X86_SAR, IM, RD)
#define SARWim(IM, MD, MB, MI, MS)	_ROTSHIWim(X86_SAR, IM, MD, MB, MI, MS)
#define SARWrr(RS, RD)			_ROTSHIWrr(X86_SAR, RS, RD)
#define SARWrm(RS, MD, MB, MI, MS)	_ROTSHIWrm(X86_SAR, RS, MD, MB, MI, MS)

#define SARLir(IM, RD)			_ROTSHILir(X86_SAR, IM, RD)
#define SARLim(IM, MD, MB, MI, MS)	_ROTSHILim(X86_SAR, IM, MD, MB, MI, MS)
#define SARLrr(RS, RD)			_ROTSHILrr(X86_SAR, RS, RD)
#define SARLrm(RS, MD, MB, MI, MS)	_ROTSHILrm(X86_SAR, RS, MD, MB, MI, MS)

#define SARQir(IM, RD)			_ROTSHIQir(X86_SAR, IM, RD)
#define SARQim(IM, MD, MB, MI, MS)	_ROTSHIQim(X86_SAR, IM, MD, MB, MI, MS)
#define SARQrr(RS, RD)			_ROTSHIQrr(X86_SAR, RS, RD)
#define SARQrm(RS, MD, MB, MI, MS)	_ROTSHIQrm(X86_SAR, RS, MD, MB, MI, MS)


/* --- Bit test instructions ----------------------------------------------- */

enum {
  X86_BT  = 4,
  X86_BTS = 5,
  X86_BTR = 6,
  X86_BTC = 7,
};

/*									_format		Opcd		 ,Mod ,r      ,m	,mem=dsp+sib	,imm... */

#define _BTWir(OP, IM, RD)		(_d16(), _REXLrr(0, RD),	_OO_Mrm_B	(0x0fba		 ,_b11,OP     ,_r2(RD)			,_u8(IM)))
#define _BTWim(OP, IM, MD, MB, MI, MS)	(_d16(), _REXLrm(0, MB, MI),	_OO_r_X_B	(0x0fba		      ,OP		,MD,MB,MI,MS	,_u8(IM)))
#define _BTWrr(OP, RS, RD)		(_d16(), _REXLrr(RS, RD),	_OO_Mrm		(0x0f83|((OP)<<3),_b11,_r2(RS),_r2(RD)				))
#define _BTWrm(OP, RS, MD, MB, MI, MS)	(_d16(), _REXLrm(RS, MB, MI),	_OO_r_X		(0x0f83|((OP)<<3)     ,_r2(RS)		,MD,MB,MI,MS		))

#define _BTLir(OP, IM, RD)		(_REXLrr(0, RD),		_OO_Mrm_B	(0x0fba		 ,_b11,OP     ,_r4(RD)			,_u8(IM)))
#define _BTLim(OP, IM, MD, MB, MI, MS)	(_REXLrm(0, MB, MI),		_OO_r_X_B	(0x0fba		      ,OP		,MD,MB,MI,MS	,_u8(IM)))
#define _BTLrr(OP, RS, RD)		(_REXLrr(RS, RD),		_OO_Mrm		(0x0f83|((OP)<<3),_b11,_r4(RS),_r4(RD)				))
#define _BTLrm(OP, RS, MD, MB, MI, MS)	(_REXLrm(RS, MB, MI),		_OO_r_X		(0x0f83|((OP)<<3)     ,_r4(RS)		,MD,MB,MI,MS		))

#define _BTQir(OP, IM, RD)		(_REXQrr(0, RD),		_OO_Mrm_B	(0x0fba		 ,_b11,OP     ,_r8(RD)			,_u8(IM)))
#define _BTQim(OP, IM, MD, MB, MI, MS)	(_REXQrm(0, MB, MI),		_OO_r_X_B	(0x0fba		      ,OP		,MD,MB,MI,MS	,_u8(IM)))
#define _BTQrr(OP, RS, RD)		(_REXQrr(RS, RD),		_OO_Mrm		(0x0f83|((OP)<<3),_b11,_r8(RS),_r8(RD)				))
#define _BTQrm(OP, RS, MD, MB, MI, MS)	(_REXQrm(RS, MB, MI),		_OO_r_X		(0x0f83|((OP)<<3)     ,_r8(RS)		,MD,MB,MI,MS		))

#define BTWir(IM, RD)			_BTWir(X86_BT, IM, RD)
#define BTWim(IM, MD, MB, MI, MS)	_BTWim(X86_BT, IM, MD, MB, MI, MS)
#define BTWrr(RS, RD)			_BTWrr(X86_BT, RS, RD)
#define BTWrm(RS, MD, MB, MI, MS)	_BTWrm(X86_BT, RS, MD, MB, MI, MS)

#define BTLir(IM, RD)			_BTLir(X86_BT, IM, RD)
#define BTLim(IM, MD, MB, MI, MS)	_BTLim(X86_BT, IM, MD, MB, MI, MS)
#define BTLrr(RS, RD)			_BTLrr(X86_BT, RS, RD)
#define BTLrm(RS, MD, MB, MI, MS)	_BTLrm(X86_BT, RS, MD, MB, MI, MS)

#define BTQir(IM, RD)			_BTQir(X86_BT, IM, RD)
#define BTQim(IM, MD, MB, MI, MS)	_BTQim(X86_BT, IM, MD, MB, MI, MS)
#define BTQrr(RS, RD)			_BTQrr(X86_BT, RS, RD)
#define BTQrm(RS, MD, MB, MI, MS)	_BTQrm(X86_BT, RS, MD, MB, MI, MS)

#define BTCWir(IM, RD)			_BTWir(X86_BTC, IM, RD)
#define BTCWim(IM, MD, MB, MI, MS)	_BTWim(X86_BTC, IM, MD, MB, MI, MS)
#define BTCWrr(RS, RD)			_BTWrr(X86_BTC, RS, RD)
#define BTCWrm(RS, MD, MB, MI, MS)	_BTWrm(X86_BTC, RS, MD, MB, MI, MS)

#define BTCLir(IM, RD)			_BTLir(X86_BTC, IM, RD)
#define BTCLim(IM, MD, MB, MI, MS)	_BTLim(X86_BTC, IM, MD, MB, MI, MS)
#define BTCLrr(RS, RD)			_BTLrr(X86_BTC, RS, RD)
#define BTCLrm(RS, MD, MB, MI, MS)	_BTLrm(X86_BTC, RS, MD, MB, MI, MS)

#define BTCQir(IM, RD)			_BTQir(X86_BTC, IM, RD)
#define BTCQim(IM, MD, MB, MI, MS)	_BTQim(X86_BTC, IM, MD, MB, MI, MS)
#define BTCQrr(RS, RD)			_BTQrr(X86_BTC, RS, RD)
#define BTCQrm(RS, MD, MB, MI, MS)	_BTQrm(X86_BTC, RS, MD, MB, MI, MS)

#define BTRWir(IM, RD)			_BTWir(X86_BTR, IM, RD)
#define BTRWim(IM, MD, MB, MI, MS)	_BTWim(X86_BTR, IM, MD, MB, MI, MS)
#define BTRWrr(RS, RD)			_BTWrr(X86_BTR, RS, RD)
#define BTRWrm(RS, MD, MB, MI, MS)	_BTWrm(X86_BTR, RS, MD, MB, MI, MS)

#define BTRLir(IM, RD)			_BTLir(X86_BTR, IM, RD)
#define BTRLim(IM, MD, MB, MI, MS)	_BTLim(X86_BTR, IM, MD, MB, MI, MS)
#define BTRLrr(RS, RD)			_BTLrr(X86_BTR, RS, RD)
#define BTRLrm(RS, MD, MB, MI, MS)	_BTLrm(X86_BTR, RS, MD, MB, MI, MS)

#define BTRQir(IM, RD)			_BTQir(X86_BTR, IM, RD)
#define BTRQim(IM, MD, MB, MI, MS)	_BTQim(X86_BTR, IM, MD, MB, MI, MS)
#define BTRQrr(RS, RD)			_BTQrr(X86_BTR, RS, RD)
#define BTRQrm(RS, MD, MB, MI, MS)	_BTQrm(X86_BTR, RS, MD, MB, MI, MS)

#define BTSWir(IM, RD)			_BTWir(X86_BTS, IM, RD)
#define BTSWim(IM, MD, MB, MI, MS)	_BTWim(X86_BTS, IM, MD, MB, MI, MS)
#define BTSWrr(RS, RD)			_BTWrr(X86_BTS, RS, RD)
#define BTSWrm(RS, MD, MB, MI, MS)	_BTWrm(X86_BTS, RS, MD, MB, MI, MS)

#define BTSLir(IM, RD)			_BTLir(X86_BTS, IM, RD)
#define BTSLim(IM, MD, MB, MI, MS)	_BTLim(X86_BTS, IM, MD, MB, MI, MS)
#define BTSLrr(RS, RD)			_BTLrr(X86_BTS, RS, RD)
#define BTSLrm(RS, MD, MB, MI, MS)	_BTLrm(X86_BTS, RS, MD, MB, MI, MS)

#define BTSQir(IM, RD)			_BTQir(X86_BTS, IM, RD)
#define BTSQim(IM, MD, MB, MI, MS)	_BTQim(X86_BTS, IM, MD, MB, MI, MS)
#define BTSQrr(RS, RD)			_BTQrr(X86_BTS, RS, RD)
#define BTSQrm(RS, MD, MB, MI, MS)	_BTQrm(X86_BTS, RS, MD, MB, MI, MS)


/* --- Move instructions --------------------------------------------------- */

/*									_format		Opcd		,Mod ,r	    ,m		,mem=dsp+sib	,imm... */

#define MOVBrr(RS, RD)			(_REXBrr(RS, RD),		_O_Mrm		(0x88		,_b11,_r1(RS),_r1(RD)				))
#define MOVBmr(MD, MB, MI, MS, RD)	(_REXBmr(MB, MI, RD),		_O_r_X		(0x8a		     ,_r1(RD)		,MD,MB,MI,MS		))
#define MOVBrm(RS, MD, MB, MI, MS)	(_REXBrm(RS, MB, MI),		_O_r_X		(0x88		     ,_r1(RS)		,MD,MB,MI,MS		))
#define MOVBir(IM,  R)			(_REXBrr(0, R),			_Or_B		(0xb0,_r1(R)						,_su8(IM)))
#define MOVBim(IM, MD, MB, MI, MS)	(_REXBrm(0, MB, MI),		_O_X_B		(0xc6					,MD,MB,MI,MS	,_su8(IM)))

#define MOVWrr(RS, RD)			(_d16(), _REXLrr(RS, RD),	_O_Mrm		(0x89		,_b11,_r2(RS),_r2(RD)				))
#define MOVWmr(MD, MB, MI, MS, RD)	(_d16(), _REXLmr(MB, MI, RD),	_O_r_X		(0x8b		     ,_r2(RD)		,MD,MB,MI,MS		))
#define MOVWrm(RS, MD, MB, MI, MS)	(_d16(), _REXLrm(RS, MB, MI),	_O_r_X		(0x89		     ,_r2(RS)		,MD,MB,MI,MS		))
#define MOVWir(IM,  R)			(_d16(), _REXLrr(0, R),		_Or_W		(0xb8,_r2(R)						,_su16(IM)))
#define MOVWim(IM, MD, MB, MI, MS)	(_d16(), _REXLrm(0, MB, MI),	_O_X_W		(0xc7					,MD,MB,MI,MS	,_su16(IM)))

#define MOVLrr(RS, RD)			(_REXLrr(RS, RD),		_O_Mrm		(0x89		,_b11,_r4(RS),_r4(RD)				))
#define MOVLmr(MD, MB, MI, MS, RD)	(_REXLmr(MB, MI, RD),		_O_r_X		(0x8b		     ,_r4(RD)		,MD,MB,MI,MS		))
#define MOVLrm(RS, MD, MB, MI, MS)	(_REXLrm(RS, MB, MI),		_O_r_X		(0x89		     ,_r4(RS)		,MD,MB,MI,MS		))
#define MOVLir(IM,  R)			(_REXLrr(0, R),			_Or_L		(0xb8,_r4(R)						,IM	))
#define MOVLim(IM, MD, MB, MI, MS)	(_REXLrm(0, MB, MI),		_O_X_L		(0xc7					,MD,MB,MI,MS	,IM	))

#define MOVQrr(RS, RD)			(_REXQrr(RS, RD),		_O_Mrm		(0x89		,_b11,_r8(RS),_r8(RD)				))
#define MOVQmr(MD, MB, MI, MS, RD)	(_REXQmr(MB, MI, RD),		_O_r_X		(0x8b		     ,_r8(RD)		,MD,MB,MI,MS		))
#define MOVQrm(RS, MD, MB, MI, MS)	(_REXQrm(RS, MB, MI),		_O_r_X		(0x89		     ,_r8(RS)		,MD,MB,MI,MS		))
#define MOVQir(IM,  R)			(_REXQrr(0, R),			_Or_Q		(0xb8,_r8(R)						,IM	))
#define MOVQim(IM, MD, MB, MI, MS)	(_REXQrm(0, MB, MI),		_O_X_L		(0xc7					,MD,MB,MI,MS	,IM	))


/* --- Unary and Multiply/Divide instructions ------------------------------ */

enum {
  X86_NOT  = 2,
  X86_NEG  = 3,
  X86_MUL  = 4,
  X86_IMUL = 5,
  X86_DIV  = 6,
  X86_IDIV = 7,
};

/*									_format		Opcd		,Mod ,r	    ,m		,mem=dsp+sib	,imm... */

#define _UNARYBr(OP, RS)		(_REXBrr(0, RS),		_O_Mrm		(0xf6		,_b11,OP    ,_r1(RS)				))
#define _UNARYBm(OP, MD, MB, MI, MS)	(_REXBrm(0, MB, MI),		_O_r_X		(0xf6		     ,OP		,MD,MB,MI,MS		))
#define _UNARYWr(OP, RS)		(_d16(), _REXLrr(0, RS),	_O_Mrm		(0xf7		,_b11,OP    ,_r2(RS)				))
#define _UNARYWm(OP, MD, MB, MI, MS)	(_d16(), _REXLmr(MB, MI, 0),	_O_r_X		(0xf7		     ,OP		,MD,MB,MI,MS		))
#define _UNARYLr(OP, RS)		(_REXLrr(0, RS),		_O_Mrm		(0xf7		,_b11,OP    ,_r4(RS)				))
#define _UNARYLm(OP, MD, MB, MI, MS)	(_REXLmr(MB, MI, 0),		_O_r_X		(0xf7		     ,OP		,MD,MB,MI,MS		))
#define _UNARYQr(OP, RS)		(_REXQrr(0, RS),		_O_Mrm		(0xf7		,_b11,OP    ,_r8(RS)				))
#define _UNARYQm(OP, MD, MB, MI, MS)	(_REXQmr(MB, MI, 0),		_O_r_X		(0xf7		     ,OP		,MD,MB,MI,MS		))

#define NOTBr(RS)			_UNARYBr(X86_NOT, RS)
#define NOTBm(MD, MB, MI, MS)		_UNARYBm(X86_NOT, MD, MB, MI, MS)
#define NOTWr(RS)			_UNARYWr(X86_NOT, RS)
#define NOTWm(MD, MB, MI, MS)		_UNARYWm(X86_NOT, MD, MB, MI, MS)
#define NOTLr(RS)			_UNARYLr(X86_NOT, RS)
#define NOTLm(MD, MB, MI, MS)		_UNARYLm(X86_NOT, MD, MB, MI, MS)
#define NOTQr(RS)			_UNARYQr(X86_NOT, RS)
#define NOTQm(MD, MB, MI, MS)		_UNARYQm(X86_NOT, MD, MB, MI, MS)

#define NEGBr(RS)			_UNARYBr(X86_NEG, RS)
#define NEGBm(MD, MB, MI, MS)		_UNARYBm(X86_NEG, MD, MB, MI, MS)
#define NEGWr(RS)			_UNARYWr(X86_NEG, RS)
#define NEGWm(MD, MB, MI, MS)		_UNARYWm(X86_NEG, MD, MB, MI, MS)
#define NEGLr(RS)			_UNARYLr(X86_NEG, RS)
#define NEGLm(MD, MB, MI, MS)		_UNARYLm(X86_NEG, MD, MB, MI, MS)
#define NEGQr(RS)			_UNARYQr(X86_NEG, RS)
#define NEGQm(MD, MB, MI, MS)		_UNARYQm(X86_NEG, MD, MB, MI, MS)

#define MULBr(RS)			_UNARYBr(X86_MUL, RS)
#define MULBm(MD, MB, MI, MS)		_UNARYBm(X86_MUL, MD, MB, MI, MS)
#define MULWr(RS)			_UNARYWr(X86_MUL, RS)
#define MULWm(MD, MB, MI, MS)		_UNARYWm(X86_MUL, MD, MB, MI, MS)
#define MULLr(RS)			_UNARYLr(X86_MUL, RS)
#define MULLm(MD, MB, MI, MS)		_UNARYLm(X86_MUL, MD, MB, MI, MS)
#define MULQr(RS)			_UNARYQr(X86_MUL, RS)
#define MULQm(MD, MB, MI, MS)		_UNARYQm(X86_MUL, MD, MB, MI, MS)

#define IMULBr(RS)			_UNARYBr(X86_IMUL, RS)
#define IMULBm(MD, MB, MI, MS)		_UNARYBm(X86_IMUL, MD, MB, MI, MS)
#define IMULWr(RS)			_UNARYWr(X86_IMUL, RS)
#define IMULWm(MD, MB, MI, MS)		_UNARYWm(X86_IMUL, MD, MB, MI, MS)
#define IMULLr(RS)			_UNARYLr(X86_IMUL, RS)
#define IMULLm(MD, MB, MI, MS)		_UNARYLm(X86_IMUL, MD, MB, MI, MS)
#define IMULQr(RS)			_UNARYQr(X86_IMUL, RS)
#define IMULQm(MD, MB, MI, MS)		_UNARYQm(X86_IMUL, MD, MB, MI, MS)

#define DIVBr(RS)			_UNARYBr(X86_DIV, RS)
#define DIVBm(MD, MB, MI, MS)		_UNARYBm(X86_DIV, MD, MB, MI, MS)
#define DIVWr(RS)			_UNARYWr(X86_DIV, RS)
#define DIVWm(MD, MB, MI, MS)		_UNARYWm(X86_DIV, MD, MB, MI, MS)
#define DIVLr(RS)			_UNARYLr(X86_DIV, RS)
#define DIVLm(MD, MB, MI, MS)		_UNARYLm(X86_DIV, MD, MB, MI, MS)
#define DIVQr(RS)			_UNARYQr(X86_DIV, RS)
#define DIVQm(MD, MB, MI, MS)		_UNARYQm(X86_DIV, MD, MB, MI, MS)

#define IDIVBr(RS)			_UNARYBr(X86_IDIV, RS)
#define IDIVBm(MD, MB, MI, MS)		_UNARYBm(X86_IDIV, MD, MB, MI, MS)
#define IDIVWr(RS)			_UNARYWr(X86_IDIV, RS)
#define IDIVWm(MD, MB, MI, MS)		_UNARYWm(X86_IDIV, MD, MB, MI, MS)
#define IDIVLr(RS)			_UNARYLr(X86_IDIV, RS)
#define IDIVLm(MD, MB, MI, MS)		_UNARYLm(X86_IDIV, MD, MB, MI, MS)
#define IDIVQr(RS)			_UNARYQr(X86_IDIV, RS)
#define IDIVQm(MD, MB, MI, MS)		_UNARYQm(X86_IDIV, MD, MB, MI, MS)

/*									_format		Opcd		,Mod ,r	    ,m		,mem=dsp+sib	,imm... */

#define IMULWrr(RS, RD)			(_d16(), _REXLrr(RD, RS),	_OO_Mrm		(0x0faf		,_b11,_r2(RD),_r2(RS)				))
#define IMULWmr(MD, MB, MI, MS, RD)	(_d16(), _REXLmr(MB, MI, RD),	_OO_r_X		(0x0faf		     ,_r2(RD)		,MD,MB,MI,MS		))

#define IMULWirr(IM,RS,RD)		(_d16(), _REXLrr(RS, RD),	_Os_Mrm_sW	(0x69		,_b11,_r2(RS),_r2(RD)			,_su16(IM)	))
#define IMULWimr(IM,MD,MB,MI,MS,RD)	(_d16(), _REXLmr(MB, MI, RD),	_Os_r_X_sW	(0x69		     ,_r2(RD)		,MD,MB,MI,MS	,_su16(IM)	))

#define IMULLir(IM, RD)			(_REXLrr(0, RD),		_Os_Mrm_sL	(0x69		,_b11,_r4(RD),_r4(RD)			,IM	))
#define IMULLrr(RS, RD)			(_REXLrr(RD, RS),		_OO_Mrm		(0x0faf		,_b11,_r4(RD),_r4(RS)				))
#define IMULLmr(MD, MB, MI, MS, RD)	(_REXLmr(MB, MI, RD),		_OO_r_X		(0x0faf		     ,_r4(RD)		,MD,MB,MI,MS		))

#define IMULQir(IM, RD)			(_REXQrr(0, RD),		_Os_Mrm_sL	(0x69		,_b11,_r8(RD),_r8(RD)			,IM	))
#define IMULQrr(RS, RD)			(_REXQrr(RD, RS),		_OO_Mrm		(0x0faf		,_b11,_r8(RD),_r8(RS)				))
#define IMULQmr(MD, MB, MI, MS, RD)	(_REXQmr(MB, MI, RD),		_OO_r_X		(0x0faf		     ,_r8(RD)		,MD,MB,MI,MS		))

#define IMULLirr(IM,RS,RD)		(_REXLrr(RS, RD),		_Os_Mrm_sL	(0x69		,_b11,_r4(RS),_r4(RD)			,IM	))
#define IMULLimr(IM,MD,MB,MI,MS,RD)	(_REXLmr(MB, MI, RD),		_Os_r_X_sL	(0x69		     ,_r4(RD)		,MD,MB,MI,MS	,IM	))

#define IMULQirr(IM,RS,RD)		(_REXQrr(RS, RD),		_Os_Mrm_sL	(0x69		,_b11,_r8(RS),_r8(RD)			,IM	))
#define IMULQimr(IM,MD,MB,MI,MS,RD)	(_REXQmr(MB, MI, RD),		_Os_r_X_sL	(0x69		     ,_r8(RD)		,MD,MB,MI,MS	,IM	))


/* --- Control Flow related instructions ----------------------------------- */

enum {
  X86_CC_O   = 0x0,
  X86_CC_NO  = 0x1,
  X86_CC_NAE = 0x2,
  X86_CC_B   = 0x2,
  X86_CC_C   = 0x2,
  X86_CC_AE  = 0x3,
  X86_CC_NB  = 0x3,
  X86_CC_NC  = 0x3,
  X86_CC_E   = 0x4,
  X86_CC_Z   = 0x4,
  X86_CC_NE  = 0x5,
  X86_CC_NZ  = 0x5,
  X86_CC_BE  = 0x6,
  X86_CC_NA  = 0x6,
  X86_CC_A   = 0x7,
  X86_CC_NBE = 0x7,
  X86_CC_S   = 0x8,
  X86_CC_NS  = 0x9,
  X86_CC_P   = 0xa,
  X86_CC_PE  = 0xa,
  X86_CC_NP  = 0xb,
  X86_CC_PO  = 0xb,
  X86_CC_L   = 0xc,
  X86_CC_NGE = 0xc,
  X86_CC_GE  = 0xd,
  X86_CC_NL  = 0xd,
  X86_CC_LE  = 0xe,
  X86_CC_NG  = 0xe,
  X86_CC_G   = 0xf,
  X86_CC_NLE = 0xf,
};

/*									_format		Opcd		,Mod ,r	    ,m		,mem=dsp+sib	,imm... */

// FIXME: no prefix is availble to encode a 32-bit operand size in 64-bit mode
#define CALLm(M)							_O_D32		(0xe8					,(int)(M)		)
#define _CALLLsr(R)			(_REXLrr(0, R),			_O_Mrm		(0xff		,_b11,_b010,_r4(R)				))
#define _CALLQsr(R)			(_REXLrr(0, R),			_O_Mrm		(0xff		,_b11,_b010,_r8(R)				))
#define CALLsr(R)			( X86_TARGET_64BIT ? _CALLQsr(R) : _CALLLsr(R))
#define CALLsm(D,B,I,S)			(_REXLrm(0, B, I),		_O_r_X		(0xff		     ,_b010		,(int)(D),B,I,S		))

// FIXME: no prefix is availble to encode a 32-bit operand size in 64-bit mode
#define JMPSm(M)							_O_D8		(0xeb					,(int)(M)		)
#define JMPm(M)								_O_D32		(0xe9					,(int)(M)		)
#define _JMPLsr(R)			(_REXLrr(0, R),			_O_Mrm		(0xff		,_b11,_b100,_r4(R)				))
#define _JMPQsr(R)			(_REXLrr(0, R),			_O_Mrm		(0xff		,_b11,_b100,_r8(R)				))
#define JMPsr(R)			( X86_TARGET_64BIT ? _JMPQsr(R) : _JMPLsr(R))
#define JMPsm(D,B,I,S)			(_REXLrm(0, B, I),		_O_r_X		(0xff		     ,_b100		,(int)(D),B,I,S		))

/*									_format		Opcd		,Mod ,r	    ,m		,mem=dsp+sib	,imm... */
#define JCCSii(CC, D)							_O_B		(0x70|(CC)				,(_sc)(int)(D)		)
#define JCCSim(CC, D)							_O_D8		(0x70|(CC)				,(int)(D)		)
#define JOSm(D)				JCCSim(X86_CC_O,   D)
#define JNOSm(D)			JCCSim(X86_CC_NO,  D)
#define JBSm(D)				JCCSim(X86_CC_B,   D)
#define JNAESm(D)			JCCSim(X86_CC_NAE, D)
#define JNBSm(D)			JCCSim(X86_CC_NB,  D)
#define JAESm(D)			JCCSim(X86_CC_AE,  D)
#define JESm(D)				JCCSim(X86_CC_E,   D)
#define JZSm(D)				JCCSim(X86_CC_Z,   D)
#define JNESm(D)			JCCSim(X86_CC_NE,  D)
#define JNZSm(D)			JCCSim(X86_CC_NZ,  D)
#define JBESm(D)			JCCSim(X86_CC_BE,  D)
#define JNASm(D)			JCCSim(X86_CC_NA,  D)
#define JNBESm(D)			JCCSim(X86_CC_NBE, D)
#define JASm(D)				JCCSim(X86_CC_A,   D)
#define JSSm(D)				JCCSim(X86_CC_S,   D)
#define JNSSm(D)			JCCSim(X86_CC_NS,  D)
#define JPSm(D)				JCCSim(X86_CC_P,   D)
#define JPESm(D)			JCCSim(X86_CC_PE,  D)
#define JNPSm(D)			JCCSim(X86_CC_NP,  D)
#define JPOSm(D)			JCCSim(X86_CC_PO,  D)
#define JLSm(D)				JCCSim(X86_CC_L,   D)
#define JNGESm(D)			JCCSim(X86_CC_NGE, D)
#define JNLSm(D)			JCCSim(X86_CC_NL,  D)
#define JGESm(D)			JCCSim(X86_CC_GE,  D)
#define JLESm(D)			JCCSim(X86_CC_LE,  D)
#define JNGSm(D)			JCCSim(X86_CC_NG,  D)
#define JNLESm(D)			JCCSim(X86_CC_NLE, D)
#define JGSm(D)				JCCSim(X86_CC_G,   D)

/*									_format		Opcd		,Mod ,r	    ,m		,mem=dsp+sib	,imm... */
#define JCCii(CC, D)							_OO_L		(0x0f80|(CC)				,(int)(D)		)
#define JCCim(CC, D)							_OO_D32		(0x0f80|(CC)				,(int)(D)		)
#define JOm(D)				JCCim(X86_CC_O,   D)
#define JNOm(D)				JCCim(X86_CC_NO,  D)
#define JBm(D)				JCCim(X86_CC_B,   D)
#define JNAEm(D)			JCCim(X86_CC_NAE, D)
#define JNBm(D)				JCCim(X86_CC_NB,  D)
#define JAEm(D)				JCCim(X86_CC_AE,  D)
#define JEm(D)				JCCim(X86_CC_E,   D)
#define JZm(D)				JCCim(X86_CC_Z,   D)
#define JNEm(D)				JCCim(X86_CC_NE,  D)
#define JNZm(D)				JCCim(X86_CC_NZ,  D)
#define JBEm(D)				JCCim(X86_CC_BE,  D)
#define JNAm(D)				JCCim(X86_CC_NA,  D)
#define JNBEm(D)			JCCim(X86_CC_NBE, D)
#define JAm(D)				JCCim(X86_CC_A,   D)
#define JSm(D)				JCCim(X86_CC_S,   D)
#define JNSm(D)				JCCim(X86_CC_NS,  D)
#define JPm(D)				JCCim(X86_CC_P,   D)
#define JPEm(D)				JCCim(X86_CC_PE,  D)
#define JNPm(D)				JCCim(X86_CC_NP,  D)
#define JPOm(D)				JCCim(X86_CC_PO,  D)
#define JLm(D)				JCCim(X86_CC_L,   D)
#define JNGEm(D)			JCCim(X86_CC_NGE, D)
#define JNLm(D)				JCCim(X86_CC_NL,  D)
#define JGEm(D)				JCCim(X86_CC_GE,  D)
#define JLEm(D)				JCCim(X86_CC_LE,  D)
#define JNGm(D)				JCCim(X86_CC_NG,  D)
#define JNLEm(D)			JCCim(X86_CC_NLE, D)
#define JGm(D)				JCCim(X86_CC_G,   D)

/*									_format		Opcd		,Mod ,r	    ,m		,mem=dsp+sib	,imm... */
#define SETCCir(CC, RD)			(_REXBrr(0, RD),		_OO_Mrm		(0x0f90|(CC)	,_b11,_b000,_r1(RD)				))
#define SETOr(RD)			SETCCir(X86_CC_O,   RD)
#define SETNOr(RD)			SETCCir(X86_CC_NO,  RD)
#define SETBr(RD)			SETCCir(X86_CC_B,   RD)
#define SETNAEr(RD)			SETCCir(X86_CC_NAE, RD)
#define SETNBr(RD)			SETCCir(X86_CC_NB,  RD)
#define SETAEr(RD)			SETCCir(X86_CC_AE,  RD)
#define SETEr(RD)			SETCCir(X86_CC_E,   RD)
#define SETZr(RD)			SETCCir(X86_CC_Z,   RD)
#define SETNEr(RD)			SETCCir(X86_CC_NE,  RD)
#define SETNZr(RD)			SETCCir(X86_CC_NZ,  RD)
#define SETBEr(RD)			SETCCir(X86_CC_BE,  RD)
#define SETNAr(RD)			SETCCir(X86_CC_NA,  RD)
#define SETNBEr(RD)			SETCCir(X86_CC_NBE, RD)
#define SETAr(RD)			SETCCir(X86_CC_A,   RD)
#define SETSr(RD)			SETCCir(X86_CC_S,   RD)
#define SETNSr(RD)			SETCCir(X86_CC_NS,  RD)
#define SETPr(RD)			SETCCir(X86_CC_P,   RD)
#define SETPEr(RD)			SETCCir(X86_CC_PE,  RD)
#define SETNPr(RD)			SETCCir(X86_CC_NP,  RD)
#define SETPOr(RD)			SETCCir(X86_CC_PO,  RD)
#define SETLr(RD)			SETCCir(X86_CC_L,   RD)
#define SETNGEr(RD)			SETCCir(X86_CC_NGE, RD)
#define SETNLr(RD)			SETCCir(X86_CC_NL,  RD)
#define SETGEr(RD)			SETCCir(X86_CC_GE,  RD)
#define SETLEr(RD)			SETCCir(X86_CC_LE,  RD)
#define SETNGr(RD)			SETCCir(X86_CC_NG,  RD)
#define SETNLEr(RD)			SETCCir(X86_CC_NLE, RD)
#define SETGr(RD)			SETCCir(X86_CC_G,   RD)

/*									_format		Opcd		,Mod ,r	    ,m		,mem=dsp+sib	,imm... */
#define SETCCim(CC,MD,MB,MI,MS)		(_REXBrm(0, MB, MI),		_OO_r_X		(0x0f90|(CC)	     ,_b000		,MD,MB,MI,MS		))
#define SETOm(D, B, I, S)		SETCCim(X86_CC_O,   D, B, I, S)
#define SETNOm(D, B, I, S)		SETCCim(X86_CC_NO,  D, B, I, S)
#define SETBm(D, B, I, S)		SETCCim(X86_CC_B,   D, B, I, S)
#define SETNAEm(D, B, I, S)		SETCCim(X86_CC_NAE, D, B, I, S)
#define SETNBm(D, B, I, S)		SETCCim(X86_CC_NB,  D, B, I, S)
#define SETAEm(D, B, I, S)		SETCCim(X86_CC_AE,  D, B, I, S)
#define SETEm(D, B, I, S)		SETCCim(X86_CC_E,   D, B, I, S)
#define SETZm(D, B, I, S)		SETCCim(X86_CC_Z,   D, B, I, S)
#define SETNEm(D, B, I, S)		SETCCim(X86_CC_NE,  D, B, I, S)
#define SETNZm(D, B, I, S)		SETCCim(X86_CC_NZ,  D, B, I, S)
#define SETBEm(D, B, I, S)		SETCCim(X86_CC_BE,  D, B, I, S)
#define SETNAm(D, B, I, S)		SETCCim(X86_CC_NA,  D, B, I, S)
#define SETNBEm(D, B, I, S)		SETCCim(X86_CC_NBE, D, B, I, S)
#define SETAm(D, B, I, S)		SETCCim(X86_CC_A,   D, B, I, S)
#define SETSm(D, B, I, S)		SETCCim(X86_CC_S,   D, B, I, S)
#define SETNSm(D, B, I, S)		SETCCim(X86_CC_NS,  D, B, I, S)
#define SETPm(D, B, I, S)		SETCCim(X86_CC_P,   D, B, I, S)
#define SETPEm(D, B, I, S)		SETCCim(X86_CC_PE,  D, B, I, S)
#define SETNPm(D, B, I, S)		SETCCim(X86_CC_NP,  D, B, I, S)
#define SETPOm(D, B, I, S)		SETCCim(X86_CC_PO,  D, B, I, S)
#define SETLm(D, B, I, S)		SETCCim(X86_CC_L,   D, B, I, S)
#define SETNGEm(D, B, I, S)		SETCCim(X86_CC_NGE, D, B, I, S)
#define SETNLm(D, B, I, S)		SETCCim(X86_CC_NL,  D, B, I, S)
#define SETGEm(D, B, I, S)		SETCCim(X86_CC_GE,  D, B, I, S)
#define SETLEm(D, B, I, S)		SETCCim(X86_CC_LE,  D, B, I, S)
#define SETNGm(D, B, I, S)		SETCCim(X86_CC_NG,  D, B, I, S)
#define SETNLEm(D, B, I, S)		SETCCim(X86_CC_NLE, D, B, I, S)
#define SETGm(D, B, I, S)		SETCCim(X86_CC_G,   D, B, I, S)

/*									_format		Opcd		,Mod ,r	     ,m		,mem=dsp+sib	,imm... */
#define CMOVWrr(CC,RS,RD)		(_d16(), _REXLrr(RD, RS),	_OO_Mrm		(0x0f40|(CC)	,_b11,_r2(RD),_r2(RS)				))
#define CMOVWmr(CC,MD,MB,MI,MS,RD)	(_d16(), _REXLmr(MB, MI, RD),	_OO_r_X		(0x0f40|(CC)	     ,_r2(RD)		,MD,MB,MI,MS		))
#define CMOVLrr(CC,RS,RD)		(_REXLrr(RD, RS),		_OO_Mrm		(0x0f40|(CC)	,_b11,_r4(RD),_r4(RS)				))
#define CMOVLmr(CC,MD,MB,MI,MS,RD)	(_REXLmr(MB, MI, RD),		_OO_r_X		(0x0f40|(CC)	     ,_r4(RD)		,MD,MB,MI,MS		))
#define CMOVQrr(CC,RS,RD)		(_REXQrr(RD, RS),		_OO_Mrm		(0x0f40|(CC)	,_b11,_r8(RD),_r8(RS)				))
#define CMOVQmr(CC,MD,MB,MI,MS,RD)	(_REXQmr(MB, MI, RD),		_OO_r_X		(0x0f40|(CC)	     ,_r8(RD)		,MD,MB,MI,MS		))


/* --- Push/Pop instructions ----------------------------------------------- */

/*									_format		Opcd		,Mod ,r	    ,m		,mem=dsp+sib	,imm... */

#define POPWr(RD)			_m32only((_d16(),		_Or		(0x58,_r2(RD)							)))
#define POPWm(MD, MB, MI, MS)		_m32only((_d16(),		_O_r_X		(0x8f		     ,_b000		,MD,MB,MI,MS		)))

#define POPLr(RD)			_m32only(			_Or		(0x58,_r4(RD)							))
#define POPLm(MD, MB, MI, MS)		_m32only(			_O_r_X		(0x8f		     ,_b000		,MD,MB,MI,MS		))

#define POPQr(RD)			_m64only((_REXQr(RD),		_Or		(0x58,_r8(RD)							)))
#define POPQm(MD, MB, MI, MS)		_m64only((_REXQm(MB, MI), 	_O_r_X		(0x8f		     ,_b000		,MD,MB,MI,MS		)))

#define PUSHWr(RS)			_m32only((_d16(),		_Or		(0x50,_r2(RS)							)))
#define PUSHWm(MD, MB, MI, MS)		_m32only((_d16(),		_O_r_X		(0xff,		     ,_b110		,MD,MB,MI,MS		)))
#define PUSHWi(IM)			_m32only((_d16(),		_Os_sW		(0x68							,IM	)))

#define PUSHLr(RS)			_m32only(			_Or		(0x50,_r4(RS)							))
#define PUSHLm(MD, MB, MI, MS)		_m32only(			_O_r_X		(0xff		     ,_b110		,MD,MB,MI,MS		))
#define PUSHLi(IM)			_m32only(			_Os_sL		(0x68							,IM	))

#define PUSHQr(RS)			_m64only((_REXQr(RS),		_Or		(0x50,_r8(RS)							)))
#define PUSHQm(MD, MB, MI, MS)		_m64only((_REXQm(MB, MI),	_O_r_X		(0xff		     ,_b110		,MD,MB,MI,MS		)))
#define PUSHQi(IM)			_m64only(			_Os_sL		(0x68							,IM	))

#define POPA()				(_d16(),			_O		(0x61								))
#define POPAD()								_O		(0x61								)

#define PUSHA()				(_d16(),			_O		(0x60								))
#define PUSHAD()							_O		(0x60								)

#define POPF()								_O		(0x9d								)
#define PUSHF()								_O		(0x9c								)


/* --- Test instructions --------------------------------------------------- */

/*									_format		Opcd		,Mod ,r	    ,m		,mem=dsp+sib	,imm... */

#define TESTBrr(RS, RD)			(_REXBrr(RS, RD),		_O_Mrm		(0x84		,_b11,_r1(RS),_r1(RD)				))
#define TESTBrm(RS, MD, MB, MI, MS)	(_REXBrm(RS, MB, MI),		_O_r_X		(0x84		     ,_r1(RS)		,MD,MB,MI,MS		))
#define TESTBir(IM, RD)			(X86_OPTIMIZE_ALU && ((RD) == X86_AL) ? \
					(_REXBrr(0, RD),		_O_B		(0xa8							,_u8(IM))) : \
					(_REXBrr(0, RD),		_O_Mrm_B	(0xf6		,_b11,_b000  ,_r1(RD)			,_u8(IM))) )
#define TESTBim(IM, MD, MB, MI, MS)	(_REXBrm(0, MB, MI),		_O_r_X_B	(0xf6		     ,_b000		,MD,MB,MI,MS	,_u8(IM)))

#define TESTWrr(RS, RD)			(_d16(), _REXLrr(RS, RD),	_O_Mrm		(0x85		,_b11,_r2(RS),_r2(RD)				))
#define TESTWrm(RS, MD, MB, MI, MS)	(_d16(), _REXLrm(RS, MB, MI),	_O_r_X		(0x85		     ,_r2(RS)		,MD,MB,MI,MS		))
#define TESTWir(IM, RD)			(X86_OPTIMIZE_ALU && ((RD) == X86_AX) ? \
					(_d16(), _REXLrr(0, RD),	_O_W		(0xa9							,_u16(IM))) : \
					(_d16(), _REXLrr(0, RD),	_O_Mrm_W	(0xf7		,_b11,_b000  ,_r2(RD)			,_u16(IM))) )
#define TESTWim(IM, MD, MB, MI, MS)	(_d16(), _REXLrm(0, MB, MI),	_O_r_X_W	(0xf7		     ,_b000		,MD,MB,MI,MS	,_u16(IM)))

#define TESTLrr(RS, RD)			(_REXLrr(RS, RD),		_O_Mrm		(0x85		,_b11,_r4(RS),_r4(RD)				))
#define TESTLrm(RS, MD, MB, MI, MS)	(_REXLrm(RS, MB, MI),		_O_r_X		(0x85		     ,_r4(RS)		,MD,MB,MI,MS		))
#define TESTLir(IM, RD)			(X86_OPTIMIZE_ALU && ((RD) == X86_EAX) ? \
					(_REXLrr(0, RD),		_O_L		(0xa9							,IM	)) : \
					(_REXLrr(0, RD),		_O_Mrm_L	(0xf7		,_b11,_b000  ,_r4(RD)			,IM	)) )
#define TESTLim(IM, MD, MB, MI, MS)	(_REXLrm(0, MB, MI),		_O_r_X_L	(0xf7		     ,_b000		,MD,MB,MI,MS	,IM	))

#define TESTQrr(RS, RD)			(_REXQrr(RS, RD),		_O_Mrm		(0x85		,_b11,_r8(RS),_r8(RD)				))
#define TESTQrm(RS, MD, MB, MI, MS)	(_REXQrm(RS, MB, MI),		_O_r_X		(0x85		     ,_r8(RS)		,MD,MB,MI,MS		))
#define TESTQir(IM, RD)			(X86_OPTIMIZE_ALU && ((RD) == X86_RAX) ? \
					(_REXQrr(0, RD),		_O_L		(0xa9							,IM	)) : \
					(_REXQrr(0, RD),		_O_Mrm_L	(0xf7		,_b11,_b000  ,_r8(RD)			,IM	)) )
#define TESTQim(IM, MD, MB, MI, MS)	(_REXQrm(0, MB, MI),		_O_r_X_L	(0xf7		     ,_b000		,MD,MB,MI,MS	,IM	))


/* --- Exchange instructions ----------------------------------------------- */

/*									_format		Opcd		,Mod ,r	    ,m		,mem=dsp+sib	,imm... */

#define CMPXCHGBrr(RS, RD)		(_REXBrr(RS, RD),		_OO_Mrm		(0x0fb0		,_b11,_r1(RS),_r1(RD)				))
#define CMPXCHGBrm(RS, MD, MB, MI, MS)	(_REXBrm(RS, MB, MI),		_OO_r_X		(0x0fb0		     ,_r1(RS)		,MD,MB,MI,MS		))

#define CMPXCHGWrr(RS, RD)		(_d16(), _REXLrr(RS, RD),	_OO_Mrm		(0x0fb1		,_b11,_r2(RS),_r2(RD)				))
#define CMPXCHGWrm(RS, MD, MB, MI, MS)	(_d16(), _REXLrm(RS, MB, MI),	_OO_r_X		(0x0fb1		     ,_r2(RS)		,MD,MB,MI,MS		))

#define CMPXCHGLrr(RS, RD)		(_REXLrr(RS, RD),		_OO_Mrm		(0x0fb1		,_b11,_r4(RS),_r4(RD)				))
#define CMPXCHGLrm(RS, MD, MB, MI, MS)	(_REXLrm(RS, MB, MI),		_OO_r_X		(0x0fb1		     ,_r4(RS)		,MD,MB,MI,MS		))

#define CMPXCHGQrr(RS, RD)		(_REXQrr(RS, RD),		_OO_Mrm		(0x0fb1		,_b11,_r8(RS),_r8(RD)				))
#define CMPXCHGQrm(RS, MD, MB, MI, MS)	(_REXQrm(RS, MB, MI),		_OO_r_X		(0x0fb1		     ,_r8(RS)		,MD,MB,MI,MS		))

#define XADDBrr(RS, RD)			(_REXBrr(RS, RD),		_OO_Mrm		(0x0fc0		,_b11,_r1(RS),_r1(RD)				))
#define XADDBrm(RS, MD, MB, MI, MS)	(_REXBrm(RS, MB, MI),		_OO_r_X		(0x0fc0		     ,_r1(RS)		,MD,MB,MI,MS		))

#define XADDWrr(RS, RD)			(_d16(), _REXLrr(RS, RD),	_OO_Mrm		(0x0fc1		,_b11,_r2(RS),_r2(RD)				))
#define XADDWrm(RS, MD, MB, MI, MS)	(_d16(), _REXLrm(RS, MB, MI),	_OO_r_X		(0x0fc1		     ,_r2(RS)		,MD,MB,MI,MS		))

#define XADDLrr(RS, RD)			(_REXLrr(RS, RD),		_OO_Mrm		(0x0fc1		,_b11,_r4(RS),_r4(RD)				))
#define XADDLrm(RS, MD, MB, MI, MS)	(_REXLrm(RS, MB, MI),		_OO_r_X		(0x0fc1		     ,_r4(RS)		,MD,MB,MI,MS		))

#define XADDQrr(RS, RD)			(_REXQrr(RS, RD),		_OO_Mrm		(0x0fc1		,_b11,_r8(RS),_r8(RD)				))
#define XADDQrm(RS, MD, MB, MI, MS)	(_REXQrm(RS, MB, MI),		_OO_r_X		(0x0fc1		     ,_r8(RS)		,MD,MB,MI,MS		))

#define XCHGBrr(RS, RD)			(_REXBrr(RS, RD),		_O_Mrm		(0x86		,_b11,_r1(RS),_r1(RD)				))
#define XCHGBrm(RS, MD, MB, MI, MS)	(_REXBrm(RS, MB, MI),		_O_r_X		(0x86		     ,_r1(RS)		,MD,MB,MI,MS		))

#define XCHGWrr(RS, RD)			(_d16(), _REXLrr(RS, RD),	_O_Mrm		(0x87		,_b11,_r2(RS),_r2(RD)				))
#define XCHGWrm(RS, MD, MB, MI, MS)	(_d16(), _REXLrm(RS, MB, MI),	_O_r_X		(0x87		     ,_r2(RS)		,MD,MB,MI,MS		))

#define XCHGLrr(RS, RD)			(_REXLrr(RS, RD),		_O_Mrm		(0x87		,_b11,_r4(RS),_r4(RD)				))
#define XCHGLrm(RS, MD, MB, MI, MS)	(_REXLrm(RS, MB, MI),		_O_r_X		(0x87		     ,_r4(RS)		,MD,MB,MI,MS		))

#define XCHGQrr(RS, RD)			(_REXQrr(RS, RD),		_O_Mrm		(0x87		,_b11,_r8(RS),_r8(RD)				))
#define XCHGQrm(RS, MD, MB, MI, MS)	(_REXQrm(RS, MB, MI),		_O_r_X		(0x87		     ,_r8(RS)		,MD,MB,MI,MS		))


/* --- Increment/Decrement instructions ------------------------------------ */

/*									_format		Opcd		,Mod ,r	    ,m		,mem=dsp+sib	,imm... */

#define DECBm(MD, MB, MI, MS)		(_REXBrm(0, MB, MI),		_O_r_X		(0xfe		     ,_b001		,MD,MB,MI,MS		))
#define DECBr(RD)			(_REXBrr(0, RD),		_O_Mrm		(0xfe		,_b11,_b001  ,_r1(RD)				))

#define DECWm(MD, MB, MI, MS)		(_d16(), _REXLrm(0, MB, MI),	_O_r_X		(0xff		     ,_b001		,MD,MB,MI,MS		))
#define DECWr(RD)			(! X86_TARGET_64BIT ? (_d16(),	_Or		(0x48,_r2(RD)							)) : \
					(_d16(), _REXLrr(0, RD),	_O_Mrm		(0xff		,_b11,_b001  ,_r2(RD)				)))

#define DECLm(MD, MB, MI, MS)		(_REXLrm(0, MB, MI),		_O_r_X		(0xff		     ,_b001		,MD,MB,MI,MS		))
#define DECLr(RD)			(! X86_TARGET_64BIT ?	 	_Or		(0x48,_r4(RD)							) : \
					(_REXLrr(0, RD),		_O_Mrm		(0xff		,_b11,_b001  ,_r4(RD)				)))

#define DECQm(MD, MB, MI, MS)		(_REXQrm(0, MB, MI),		_O_r_X		(0xff		     ,_b001		,MD,MB,MI,MS		))
#define DECQr(RD)			(_REXQrr(0, RD),		_O_Mrm		(0xff		,_b11,_b001  ,_r8(RD)				))

#define INCBm(MD, MB, MI, MS)		(_REXBrm(0, MB, MI),		_O_r_X		(0xfe		     ,_b000		,MD,MB,MI,MS		))
#define INCBr(RD)			(_REXBrr(0, RD),		_O_Mrm		(0xfe		,_b11,_b000  ,_r1(RD)				))

#define INCWm(MD, MB, MI, MS)		(_d16(), _REXLrm(0, MB, MI),	_O_r_X		(0xff		     ,_b000		,MD,MB,MI,MS		))
#define INCWr(RD)			(! X86_TARGET_64BIT ? (_d16(),	_Or		(0x40,_r2(RD)							)) : \
					(_d16(), _REXLrr(0, RD),	_O_Mrm		(0xff		,_b11,_b000  ,_r2(RD)				)) )

#define INCLm(MD, MB, MI, MS)		(_REXLrm(0, MB, MI),		_O_r_X		(0xff		     ,_b000		,MD,MB,MI,MS		))
#define INCLr(RD)			(! X86_TARGET_64BIT ? 		_Or		(0x40,_r4(RD)							) : \
					(_REXLrr(0, RD),		_O_Mrm		(0xff		,_b11,_b000  ,_r4(RD)				)))

#define INCQm(MD, MB, MI, MS)		(_REXQrm(0, MB, MI),		_O_r_X		(0xff		     ,_b000		,MD,MB,MI,MS		))
#define INCQr(RD)			(_REXQrr(0, RD),		_O_Mrm		(0xff		,_b11,_b000  ,_r8(RD)				))


/* --- Misc instructions --------------------------------------------------- */

/*									_format		Opcd		,Mod ,r	    ,m		,mem=dsp+sib	,imm... */

#define BSFWrr(RS, RD)			(_d16(), _REXLrr(RD, RS),	_OO_Mrm		(0x0fbc		,_b11,_r2(RD),_r2(RS)				))
#define BSFWmr(MD, MB, MI, MS, RD)	(_d16(), _REXLmr(MB, MI, RD),	_OO_r_X		(0x0fbc		     ,_r2(RD)		,MD,MB,MI,MS		))
#define BSRWrr(RS, RD)			(_d16(), _REXLrr(RD, RS),	_OO_Mrm		(0x0fbd		,_b11,_r2(RD),_r2(RS)				))
#define BSRWmr(MD, MB, MI, MS, RD)	(_d16(), _REXLmr(MB, MI, RD),	_OO_r_X		(0x0fbd		     ,_r2(RD)		,MD,MB,MI,MS		))

#define BSFLrr(RS, RD)			(_REXLrr(RD, RS),		_OO_Mrm		(0x0fbc		,_b11,_r4(RD),_r4(RS)				))
#define BSFLmr(MD, MB, MI, MS, RD)	(_REXLmr(MB, MI, RD),		_OO_r_X		(0x0fbc		     ,_r4(RD)		,MD,MB,MI,MS		))
#define BSRLrr(RS, RD)			(_REXLrr(RD, RS),		_OO_Mrm		(0x0fbd		,_b11,_r4(RD),_r4(RS)				))
#define BSRLmr(MD, MB, MI, MS, RD)	(_REXLmr(MB, MI, RD),		_OO_r_X		(0x0fbd		     ,_r4(RD)		,MD,MB,MI,MS		))

#define BSFQrr(RS, RD)			(_REXQrr(RD, RS),		_OO_Mrm		(0x0fbc		,_b11,_r8(RD),_r8(RS)				))
#define BSFQmr(MD, MB, MI, MS, RD)	(_REXQmr(MB, MI, RD),		_OO_r_X		(0x0fbc		     ,_r8(RD)		,MD,MB,MI,MS		))
#define BSRQrr(RS, RD)			(_REXQrr(RD, RS),		_OO_Mrm		(0x0fbd		,_b11,_r8(RD),_r8(RS)				))
#define BSRQmr(MD, MB, MI, MS, RD)	(_REXQmr(MB, MI, RD),		_OO_r_X		(0x0fbd		     ,_r8(RD)		,MD,MB,MI,MS		))

/*									_format		Opcd		,Mod ,r	    ,m		,mem=dsp+sib	,imm... */

#define MOVSBWrr(RS, RD)		(_d16(), _REXBLrr(RD, RS),	_OO_Mrm		(0x0fbe		,_b11,_r2(RD),_r1(RS)				))
#define MOVSBWmr(MD, MB, MI, MS, RD)	(_d16(), _REXLmr(MB, MI, RD),	_OO_r_X		(0x0fbe		     ,_r2(RD)		,MD,MB,MI,MS		))
#define MOVZBWrr(RS, RD)		(_d16(), _REXBLrr(RD, RS),	_OO_Mrm		(0x0fb6		,_b11,_r2(RD),_r1(RS)				))
#define MOVZBWmr(MD, MB, MI, MS, RD)	(_d16(), _REXLmr(MB, MI, RD),	_OO_r_X		(0x0fb6		     ,_r2(RD)		,MD,MB,MI,MS		))

#define MOVSBLrr(RS, RD)		(_REXBLrr(RD, RS),		_OO_Mrm		(0x0fbe		,_b11,_r4(RD),_r1(RS)				))
#define MOVSBLmr(MD, MB, MI, MS, RD)	(_REXLmr(MB, MI, RD),		_OO_r_X		(0x0fbe		     ,_r4(RD)		,MD,MB,MI,MS		))
#define MOVZBLrr(RS, RD)		(_REXBLrr(RD, RS),		_OO_Mrm		(0x0fb6		,_b11,_r4(RD),_r1(RS)				))
#define MOVZBLmr(MD, MB, MI, MS, RD)	(_REXLmr(MB, MI, RD),		_OO_r_X		(0x0fb6		     ,_r4(RD)		,MD,MB,MI,MS		))

#define MOVSBQrr(RS, RD)		(_REXQrr(RD, RS),		_OO_Mrm		(0x0fbe		,_b11,_r8(RD),_r1(RS)				))
#define MOVSBQmr(MD, MB, MI, MS, RD)	(_REXQmr(MB, MI, RD),		_OO_r_X		(0x0fbe		     ,_r8(RD)		,MD,MB,MI,MS		))
#define MOVZBQrr(RS, RD)		(_REXQrr(RD, RS),		_OO_Mrm		(0x0fb6		,_b11,_r8(RD),_r1(RS)				))
#define MOVZBQmr(MD, MB, MI, MS, RD)	(_REXQmr(MB, MI, RD),		_OO_r_X		(0x0fb6		     ,_r8(RD)		,MD,MB,MI,MS		))

#define MOVSWLrr(RS, RD)		(_REXLrr(RD, RS),		_OO_Mrm		(0x0fbf		,_b11,_r4(RD),_r2(RS)				))
#define MOVSWLmr(MD, MB, MI, MS, RD)	(_REXLmr(MB, MI, RD),		_OO_r_X		(0x0fbf		     ,_r4(RD)		,MD,MB,MI,MS		))
#define MOVZWLrr(RS, RD)		(_REXLrr(RD, RS),		_OO_Mrm		(0x0fb7		,_b11,_r4(RD),_r2(RS)				))
#define MOVZWLmr(MD, MB, MI, MS, RD)	(_REXLmr(MB, MI, RD),		_OO_r_X		(0x0fb7		     ,_r4(RD)		,MD,MB,MI,MS		))

#define MOVSWQrr(RS, RD)		_m64only((_REXQrr(RD, RS),	_OO_Mrm		(0x0fbf		,_b11,_r8(RD),_r2(RS)				)))
#define MOVSWQmr(MD, MB, MI, MS, RD)	_m64only((_REXQmr(MB, MI, RD),	_OO_r_X		(0x0fbf		     ,_r8(RD)		,MD,MB,MI,MS		)))
#define MOVZWQrr(RS, RD)		_m64only((_REXQrr(RD, RS),	_OO_Mrm		(0x0fb7		,_b11,_r8(RD),_r2(RS)				)))
#define MOVZWQmr(MD, MB, MI, MS, RD)	_m64only((_REXQmr(MB, MI, RD),	_OO_r_X		(0x0fb7		     ,_r8(RD)		,MD,MB,MI,MS		)))

#define MOVSLQrr(RS, RD)		_m64only((_REXQrr(RD, RS),	_O_Mrm		(0x63		,_b11,_r8(RD),_r4(RS)				)))
#define MOVSLQmr(MD, MB, MI, MS, RD)	_m64only((_REXQmr(MB, MI, RD),	_O_r_X		(0x63		     ,_r8(RD)		,MD,MB,MI,MS		)))

/*									_format		Opcd		,Mod ,r	    ,m		,mem=dsp+sib	,imm... */

#define LEALmr(MD, MB, MI, MS, RD)	(_REXLmr(MB, MI, RD),		_O_r_X		(0x8d		     ,_r4(RD)		,MD,MB,MI,MS		))
#define LEAQmr(MD, MB, MI, MS, RD)	(_REXQmr(MB, MI, RD),		_O_r_X		(0x8d		     ,_r4(RD)		,MD,MB,MI,MS		))

#define BSWAPLr(R)			(_REXLrr(0, R),			_OOr		(0x0fc8,_r4(R)							))
#define BSWAPQr(R)			(_REXQrr(0, R),			_OOr		(0x0fc8,_r8(R)							))

#define CLC()								_O		(0xf8								)
#define STC()								_O		(0xf9								)
#define CMC()								_O		(0xf5								)

#define CLD()								_O		(0xfc								)
#define STD()								_O		(0xfd								)

#define CBTW()				(_d16(),			_O		(0x98								))
#define CWTL()								_O		(0x98								)
#define CLTQ()				_m64only(_REXQrr(0, 0),		_O		(0x98								))

#define CBW				CBTW
#define CWDE				CWTL
#define CDQE				CLTQ

#define CWTD()				(_d16(),			_O		(0x99								))
#define CLTD()								_O		(0x99								)
#define CQTO()				_m64only(_REXQrr(0, 0),		_O		(0x99								))

#define CWD				CWTD
#define CDQ				CLTD
#define CQO				CQTO

#define LAHF()								_O		(0x9f								)
#define SAHF()								_O		(0x9e								)

/*									_format		Opcd		,Mod ,r	    ,m		,mem=dsp+sib	,imm... */

#define CPUID()								_OO		(0x0fa2								)
#define RDTSC()								_OO		(0xff31								)

#define ENTERii(W, B)							_O_W_B		(0xc8						  ,_su16(W),_su8(B))

#define LEAVE()								_O		(0xc9								)
#define RET()								_O		(0xc3								)
#define RETi(IM)							_O_W		(0xc2							,_su16(IM))

#define NOP()								_O		(0x90								)


/* --- Media 64-bit instructions ------------------------------------------- */

enum {
  X86_MMX_PABSB		= 0x1c, // 2P
  X86_MMX_PABSW		= 0x1d, // 2P
  X86_MMX_PABSD		= 0x1e, // 2P
  X86_MMX_PACKSSWB	= 0x63,
  X86_MMX_PACKSSDW	= 0x6b,
  X86_MMX_PACKUSWB	= 0x67,
  X86_MMX_PADDB		= 0xfc,
  X86_MMX_PADDW		= 0xfd,
  X86_MMX_PADDD		= 0xfe,
  X86_MMX_PADDQ		= 0xd4,
  X86_MMX_PADDSB	= 0xec,
  X86_MMX_PADDSW	= 0xed,
  X86_MMX_PADDUSB	= 0xdc,
  X86_MMX_PADDUSW	= 0xdd,
  X86_MMX_PAND		= 0xdb,
  X86_MMX_PANDN		= 0xdf,
  X86_MMX_PAVGB		= 0xe0,
  X86_MMX_PAVGW		= 0xe3,
  X86_MMX_PCMPEQB	= 0x74,
  X86_MMX_PCMPEQW	= 0x75,
  X86_MMX_PCMPEQD	= 0x76,
  X86_MMX_PCMPGTB	= 0x64,
  X86_MMX_PCMPGTW	= 0x65,
  X86_MMX_PCMPGTD	= 0x66,
  X86_MMX_PEXTRW	= 0xc5, // 64, /r ib
  X86_MMX_PHADDW	= 0x01, // 2P
  X86_MMX_PHADDD	= 0x02, // 2P
  X86_MMX_PHADDSW	= 0x03, // 2P
  X86_MMX_PHSUBW	= 0x05, // 2P
  X86_MMX_PHSUBD	= 0x06, // 2P
  X86_MMX_PHSUBSW	= 0x07, // 2P
  X86_MMX_PINSRW	= 0xc4, // 64, /r ib
  X86_MMX_PMADDUBSW	= 0x04, // 2P
  X86_MMX_PMADDWD	= 0xf5,
  X86_MMX_PMAXSW	= 0xee,
  X86_MMX_PMAXUB	= 0xde,
  X86_MMX_PMINSW	= 0xea,
  X86_MMX_PMINUB	= 0xda,
  X86_MMX_PMOVMSKB	= 0xd7, // 64
  X86_MMX_PMULHRSW	= 0x0b, // 2P
  X86_MMX_PMULHUW	= 0xe4,
  X86_MMX_PMULHW	= 0xe5,
  X86_MMX_PMULLW	= 0xd5,
  X86_MMX_PMULUDQ	= 0xf4,
  X86_MMX_POR		= 0xeb,
  X86_MMX_PSADBW	= 0xf6,
  X86_MMX_PSHUFB	= 0x00, // 2P
  X86_MMX_PSHUFW	= 0x70, // /r ib
  X86_MMX_PSIGNB	= 0x08, // 2P
  X86_MMX_PSIGNW	= 0x09, // 2P
  X86_MMX_PSIGND	= 0x0a, // 2P
  X86_MMX_PSLLW		= 0xf1,
  X86_MMX_PSLLWi	= 0x71, // /6 ib
  X86_MMX_PSLLD		= 0xf2,
  X86_MMX_PSLLDi	= 0x72, // /6 ib
  X86_MMX_PSLLQ		= 0xf3,
  X86_MMX_PSLLQi	= 0x73, // /6 ib
  X86_MMX_PSRAW		= 0xe1,
  X86_MMX_PSRAWi	= 0x71, // /4 ib
  X86_MMX_PSRAD		= 0xe2,
  X86_MMX_PSRADi	= 0x72, // /4 ib
  X86_MMX_PSRLW		= 0xd1,
  X86_MMX_PSRLWi	= 0x71, // /2 ib
  X86_MMX_PSRLD		= 0xd2,
  X86_MMX_PSRLDi	= 0x72, // /2 ib
  X86_MMX_PSRLQ		= 0xd3,
  X86_MMX_PSRLQi	= 0x73, // /2 ib
  X86_MMX_PSUBB		= 0xf8,
  X86_MMX_PSUBW		= 0xf9,
  X86_MMX_PSUBD		= 0xfa,
  X86_MMX_PSUBQ		= 0xfb,
  X86_MMX_PSUBSB	= 0xe8,
  X86_MMX_PSUBSW	= 0xe9,
  X86_MMX_PSUBUSB	= 0xd8,
  X86_MMX_PSUBUSW	= 0xd9,
  X86_MMX_PUNPCKHBW	= 0x68,
  X86_MMX_PUNPCKHWD	= 0x69,
  X86_MMX_PUNPCKHDQ	= 0x6a,
  X86_MMX_PUNPCKLBW	= 0x60,
  X86_MMX_PUNPCKLWD	= 0x61,
  X86_MMX_PUNPCKLDQ	= 0x62,
  X86_MMX_PXOR		= 0xef,
};

#define __MMXLrr(OP,RS,RSA,RD,RDA)		(_REXLrr(RD, RS),	_OO_Mrm		(0x0f00|(OP)	,_b11,RDA(RD),RSA(RS)				))
#define __MMXLmr(OP,MD,MB,MI,MS,RD,RDA)		(_REXLmr(MB, MI, RD),	_OO_r_X		(0x0f00|(OP)	     ,RDA(RD)		,MD,MB,MI,MS		))
#define __MMXLrm(OP,RS,RSA,MD,MB,MI,MS)		(_REXLrm(RS, MB, MI),	_OO_r_X		(0x0f00|(OP)	     ,RSA(RS)		,MD,MB,MI,MS		))
#define __MMXLirr(OP,IM,RS,RSA,RD,RDA)		(_REXLrr(RD, RS),	_OO_Mrm_B	(0x0f00|(OP)	,_b11,RDA(RD),RSA(RS)			,_u8(IM)))
#define __MMXLimr(OP,IM,MD,MB,MI,MS,RD,RDA)	(_REXLmr(MB, MI, RS),	_OO_r_X_B	(0x0f00|(OP)	     ,RDA(RD)		,MD,MB,MI,MS	,_u8(IM)))
#define __MMXQrr(OP,RS,RSA,RD,RDA)		(_REXQrr(RD, RS),	_OO_Mrm		(0x0f00|(OP)	,_b11,RDA(RD),RSA(RS)				))
#define __MMXQmr(OP,MD,MB,MI,MS,RD,RDA)		(_REXQmr(MB, MI, RD),	_OO_r_X		(0x0f00|(OP)	     ,RDA(RD)		,MD,MB,MI,MS		))
#define __MMXQrm(OP,RS,RSA,MD,MB,MI,MS)		(_REXQrm(RS, MB, MI),	_OO_r_X		(0x0f00|(OP)	     ,RSA(RS)		,MD,MB,MI,MS		))
#define __MMXQirr(OP,IM,RS,RSA,RD,RDA)		(_REXQrr(RD, RS),	_OO_Mrm_B	(0x0f00|(OP)	,_b11,RDA(RD),RSA(RS)			,_u8(IM)))
#define __MMXQimr(OP,IM,MD,MB,MI,MS,RD,RDA)	(_REXQmr(MB, MI, RS),	_OO_r_X_B	(0x0f00|(OP)	     ,RDA(RD)		,MD,MB,MI,MS	,_u8(IM)))
#define __MMX1Lrr(PX,OP,RS,RSA,RD,RDA)		(_REXLrr(RD, RS),	_B(0x0f),_OO_Mrm(((PX)<<8)|(OP)	,_b11,RDA(RD),RSA(RS)				))
#define __MMX1Lmr(PX,OP,MD,MB,MI,MS,RD,RDA)	(_REXLmr(MB, MI, RD),	_B(0x0f),_OO_r_X(((PX)<<8)|(OP)	     ,RDA(RD)		,MD,MB,MI,MS		))
#define __MMX1Lrm(PX,OP,RS,RSA,MD,MB,MI,MS)	(_REXLrm(RS, MB, MI),	_B(0x0f),_OO_r_X(((PX)<<8)|(OP)	     ,RSA(RS)		,MD,MB,MI,MS		))

#define _MMXLrr(OP,RS,RD)		__MMXLrr(OP,RS,_rM,RD,_rM)
#define _MMXLmr(OP,MD,MB,MI,MS,RD)	__MMXLmr(OP,MD,MB,MI,MS,RD,_rM)
#define _MMXLrm(OP,RS,MD,MB,MI,MS)	__MMXLrm(OP,RS,_rM,MD,MB,MI,MS)
#define _MMXQrr(OP,RS,RD)		__MMXQrr(OP,RS,_rM,RD,_rM)
#define _MMXQmr(OP,MD,MB,MI,MS,RD)	__MMXQmr(OP,MD,MB,MI,MS,RD,_rM)
#define _MMXQrm(OP,RS,MD,MB,MI,MS)	__MMXQrm(OP,RS,_rM,MD,MB,MI,MS)
#define _2P_MMXLrr(OP,RS,RD)		__MMX1Lrr(0x38, OP,RS,_rM,RD,_rM)
#define _2P_MMXLmr(OP,MD,MB,MI,MS,RD)	__MMX1Lmr(0x38, OP,MD,MB,MI,MS,RD,_rM)
#define _2P_MMXLrm(OP,RS,MD,MB,MI,MS)	__MMX1Lrm(0x38, OP,RS,_rM,MD,MB,MI,MS)

#define MMX_MOVDMDrr(RS, RD)		__MMXLrr(0x6e, RS,_r4, RD,_rM)
#define MMX_MOVQMDrr(RS, RD)		__MMXQrr(0x6e, RS,_r8, RD,_rM)
#define MMX_MOVDMSrr(RS, RD)		__MMXLrr(0x7e, RD,_r4, RS,_rM)
#define MMX_MOVQMSrr(RS, RD)		__MMXQrr(0x7e, RD,_r8, RS,_rM)

#define MMX_MOVDmr(MD, MB, MI, MS, RD)	_MMXLmr(0x6e, MD, MB, MI, MS, RD)
#define MMX_MOVDrm(RS, MD, MB, MI, MS)	_MMXLrm(0x7e, RS, MD, MB, MI, MS)
#define MMX_MOVQrr(RS, RD)		_MMXLrr(0x6f, RS, RD)
#define MMX_MOVQmr(MD, MB, MI, MS, RD)	_MMXLmr(0x6f, MD, MB, MI, MS, RD)
#define MMX_MOVQrm(RS, MD, MB, MI, MS)	_MMXLrm(0x7f, RS, MD, MB, MI, MS)

// Original MMX instructions
#define MMX_PACKSSWBrr(RS, RD)		_MMXLrr(X86_MMX_PACKSSWB,RS,RD)
#define MMX_PACKSSWBmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PACKSSWB, MD, MB, MI, MS, RD)
#define MMX_PACKSSDWrr(RS, RD)		_MMXLrr(X86_MMX_PACKSSDW,RS,RD)
#define MMX_PACKSSDWmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PACKSSDW, MD, MB, MI, MS, RD)
#define MMX_PACKUSWBrr(RS, RD)		_MMXLrr(X86_MMX_PACKUSWB,RS,RD)
#define MMX_PACKUSWBmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PACKUSWB, MD, MB, MI, MS, RD)
#define MMX_PADDBrr(RS, RD)		_MMXLrr(X86_MMX_PADDB,RS,RD)
#define MMX_PADDBmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PADDB, MD, MB, MI, MS, RD)
#define MMX_PADDWrr(RS, RD)		_MMXLrr(X86_MMX_PADDW,RS,RD)
#define MMX_PADDWmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PADDW, MD, MB, MI, MS, RD)
#define MMX_PADDDrr(RS, RD)		_MMXLrr(X86_MMX_PADDD,RS,RD)
#define MMX_PADDDmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PADDD, MD, MB, MI, MS, RD)
#define MMX_PADDQrr(RS, RD)		_MMXLrr(X86_MMX_PADDQ,RS,RD)
#define MMX_PADDQmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PADDQ, MD, MB, MI, MS, RD)
#define MMX_PADDSBrr(RS, RD)		_MMXLrr(X86_MMX_PADDSB,RS,RD)
#define MMX_PADDSBmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PADDSB, MD, MB, MI, MS, RD)
#define MMX_PADDSWrr(RS, RD)		_MMXLrr(X86_MMX_PADDSW,RS,RD)
#define MMX_PADDSWmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PADDSW, MD, MB, MI, MS, RD)
#define MMX_PADDUSBrr(RS, RD)		_MMXLrr(X86_MMX_PADDUSB,RS,RD)
#define MMX_PADDUSBmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PADDUSB, MD, MB, MI, MS, RD)
#define MMX_PADDUSWrr(RS, RD)		_MMXLrr(X86_MMX_PADDUSW,RS,RD)
#define MMX_PADDUSWmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PADDUSW, MD, MB, MI, MS, RD)
#define MMX_PANDrr(RS, RD)		_MMXLrr(X86_MMX_PAND,RS,RD)
#define MMX_PANDmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PAND, MD, MB, MI, MS, RD)
#define MMX_PANDNrr(RS, RD)		_MMXLrr(X86_MMX_PANDN,RS,RD)
#define MMX_PANDNmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PANDN, MD, MB, MI, MS, RD)
#define MMX_PAVGBrr(RS, RD)		_MMXLrr(X86_MMX_PAVGB,RS,RD)
#define MMX_PAVGBmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PAVGB, MD, MB, MI, MS, RD)
#define MMX_PAVGWrr(RS, RD)		_MMXLrr(X86_MMX_PAVGW,RS,RD)
#define MMX_PAVGWmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PAVGW, MD, MB, MI, MS, RD)
#define MMX_PCMPEQBrr(RS, RD)		_MMXLrr(X86_MMX_PCMPEQB,RS,RD)
#define MMX_PCMPEQBmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PCMPEQB, MD, MB, MI, MS, RD)
#define MMX_PCMPEQWrr(RS, RD)		_MMXLrr(X86_MMX_PCMPEQW,RS,RD)
#define MMX_PCMPEQWmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PCMPEQW, MD, MB, MI, MS, RD)
#define MMX_PCMPEQDrr(RS, RD)		_MMXLrr(X86_MMX_PCMPEQD,RS,RD)
#define MMX_PCMPEQDmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PCMPEQD, MD, MB, MI, MS, RD)
#define MMX_PCMPGTBrr(RS, RD)		_MMXLrr(X86_MMX_PCMPGTB,RS,RD)
#define MMX_PCMPGTBmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PCMPGTB, MD, MB, MI, MS, RD)
#define MMX_PCMPGTWrr(RS, RD)		_MMXLrr(X86_MMX_PCMPGTW,RS,RD)
#define MMX_PCMPGTWmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PCMPGTW, MD, MB, MI, MS, RD)
#define MMX_PCMPGTDrr(RS, RD)		_MMXLrr(X86_MMX_PCMPGTD,RS,RD)
#define MMX_PCMPGTDmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PCMPGTD, MD, MB, MI, MS, RD)
#define MMX_PMADDWDrr(RS, RD)		_MMXLrr(X86_MMX_PMADDWD,RS,RD)
#define MMX_PMADDWDmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PMADDWD, MD, MB, MI, MS, RD)
#define MMX_PMAXSWrr(RS, RD)		_MMXLrr(X86_MMX_PMAXSW,RS,RD)
#define MMX_PMAXSWmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PMAXSW, MD, MB, MI, MS, RD)
#define MMX_PMAXUBrr(RS, RD)		_MMXLrr(X86_MMX_PMAXUB,RS,RD)
#define MMX_PMAXUBmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PMAXUB, MD, MB, MI, MS, RD)
#define MMX_PMINSWrr(RS, RD)		_MMXLrr(X86_MMX_PMINSW,RS,RD)
#define MMX_PMINSWmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PMINSW, MD, MB, MI, MS, RD)
#define MMX_PMINUBrr(RS, RD)		_MMXLrr(X86_MMX_PMINUB,RS,RD)
#define MMX_PMINUBmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PMINUB, MD, MB, MI, MS, RD)
#define MMX_PMULHUWrr(RS, RD)		_MMXLrr(X86_MMX_PMULHUW,RS,RD)
#define MMX_PMULHUWmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PMULHUW, MD, MB, MI, MS, RD)
#define MMX_PMULHWrr(RS, RD)		_MMXLrr(X86_MMX_PMULHW,RS,RD)
#define MMX_PMULHWmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PMULHW, MD, MB, MI, MS, RD)
#define MMX_PMULLWrr(RS, RD)		_MMXLrr(X86_MMX_PMULLW,RS,RD)
#define MMX_PMULLWmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PMULLW, MD, MB, MI, MS, RD)
#define MMX_PMULUDQrr(RS, RD)		_MMXLrr(X86_MMX_PMULUDQ,RS,RD)
#define MMX_PMULUDQmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PMULUDQ, MD, MB, MI, MS, RD)
#define MMX_PORrr(RS, RD)		_MMXLrr(X86_MMX_POR,RS,RD)
#define MMX_PORmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_POR, MD, MB, MI, MS, RD)
#define MMX_PSADBWrr(RS, RD)		_MMXLrr(X86_MMX_PSADBW,RS,RD)
#define MMX_PSADBWmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PSADBW, MD, MB, MI, MS, RD)
#define MMX_PSLLWir(IM, RD)		__MMXLirr(X86_MMX_PSLLWi, IM, RD,_rM, _b110,_rN)
#define MMX_PSLLWrr(RS, RD)		_MMXLrr(X86_MMX_PSLLW,RS,RD)
#define MMX_PSLLWmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PSLLW, MD, MB, MI, MS, RD)
#define MMX_PSLLDir(IM, RD)		__MMXLirr(X86_MMX_PSLLDi, IM, RD,_rM, _b110,_rN)
#define MMX_PSLLDrr(RS, RD)		_MMXLrr(X86_MMX_PSLLD,RS,RD)
#define MMX_PSLLDmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PSLLD, MD, MB, MI, MS, RD)
#define MMX_PSLLQir(IM, RD)		__MMXLirr(X86_MMX_PSLLQi, IM, RD,_rM, _b110,_rN)
#define MMX_PSLLQrr(RS, RD)		_MMXLrr(X86_MMX_PSLLQ,RS,RD)
#define MMX_PSLLQmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PSLLQ, MD, MB, MI, MS, RD)
#define MMX_PSRAWir(IM, RD)		__MMXLirr(X86_MMX_PSRAWi, IM, RD,_rM, _b100,_rN)
#define MMX_PSRAWrr(RS, RD)		_MMXLrr(X86_MMX_PSRAW,RS,RD)
#define MMX_PSRAWmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PSRAW, MD, MB, MI, MS, RD)
#define MMX_PSRADir(IM, RD)		__MMXLirr(X86_MMX_PSRADi, IM, RD,_rM, _b100,_rN)
#define MMX_PSRADrr(RS, RD)		_MMXLrr(X86_MMX_PSRAD,RS,RD)
#define MMX_PSRADmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PSRAD, MD, MB, MI, MS, RD)
#define MMX_PSRLWir(IM, RD)		__MMXLirr(X86_MMX_PSRLWi, IM, RD,_rM, _b010,_rN)
#define MMX_PSRLWrr(RS, RD)		_MMXLrr(X86_MMX_PSRLW,RS,RD)
#define MMX_PSRLWmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PSRLW, MD, MB, MI, MS, RD)
#define MMX_PSRLDir(IM, RD)		__MMXLirr(X86_MMX_PSRLDi, IM, RD,_rM, _b010,_rN)
#define MMX_PSRLDrr(RS, RD)		_MMXLrr(X86_MMX_PSRLD,RS,RD)
#define MMX_PSRLDmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PSRLD, MD, MB, MI, MS, RD)
#define MMX_PSRLQir(IM, RD)		__MMXLirr(X86_MMX_PSRLQi, IM, RD,_rM, _b010,_rN)
#define MMX_PSRLQrr(RS, RD)		_MMXLrr(X86_MMX_PSRLQ,RS,RD)
#define MMX_PSRLQmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PSRLQ, MD, MB, MI, MS, RD)
#define MMX_PSUBBrr(RS, RD)		_MMXLrr(X86_MMX_PSUBB,RS,RD)
#define MMX_PSUBBmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PSUBB, MD, MB, MI, MS, RD)
#define MMX_PSUBWrr(RS, RD)		_MMXLrr(X86_MMX_PSUBW,RS,RD)
#define MMX_PSUBWmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PSUBW, MD, MB, MI, MS, RD)
#define MMX_PSUBDrr(RS, RD)		_MMXLrr(X86_MMX_PSUBD,RS,RD)
#define MMX_PSUBDmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PSUBD, MD, MB, MI, MS, RD)
#define MMX_PSUBQrr(RS, RD)		_MMXLrr(X86_MMX_PSUBQ,RS,RD)
#define MMX_PSUBQmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PSUBQ, MD, MB, MI, MS, RD)
#define MMX_PSUBSBrr(RS, RD)		_MMXLrr(X86_MMX_PSUBSB,RS,RD)
#define MMX_PSUBSBmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PSUBSB, MD, MB, MI, MS, RD)
#define MMX_PSUBSWrr(RS, RD)		_MMXLrr(X86_MMX_PSUBSW,RS,RD)
#define MMX_PSUBSWmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PSUBSW, MD, MB, MI, MS, RD)
#define MMX_PSUBUSBrr(RS, RD)		_MMXLrr(X86_MMX_PSUBUSB,RS,RD)
#define MMX_PSUBUSBmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PSUBUSB, MD, MB, MI, MS, RD)
#define MMX_PSUBUSWrr(RS, RD)		_MMXLrr(X86_MMX_PSUBUSW,RS,RD)
#define MMX_PSUBUSWmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PSUBUSW, MD, MB, MI, MS, RD)
#define MMX_PUNPCKHBWrr(RS, RD)		_MMXLrr(X86_MMX_PUNPCKHBW,RS,RD)
#define MMX_PUNPCKHBWmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PUNPCKHBW, MD, MB, MI, MS, RD)
#define MMX_PUNPCKHWDrr(RS, RD)		_MMXLrr(X86_MMX_PUNPCKHWD,RS,RD)
#define MMX_PUNPCKHWDmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PUNPCKHWD, MD, MB, MI, MS, RD)
#define MMX_PUNPCKHDQrr(RS, RD)		_MMXLrr(X86_MMX_PUNPCKHDQ,RS,RD)
#define MMX_PUNPCKHDQmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PUNPCKHDQ, MD, MB, MI, MS, RD)
#define MMX_PUNPCKLBWrr(RS, RD)		_MMXLrr(X86_MMX_PUNPCKLBW,RS,RD)
#define MMX_PUNPCKLBWmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PUNPCKLBW, MD, MB, MI, MS, RD)
#define MMX_PUNPCKLWDrr(RS, RD)		_MMXLrr(X86_MMX_PUNPCKLWD,RS,RD)
#define MMX_PUNPCKLWDmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PUNPCKLWD, MD, MB, MI, MS, RD)
#define MMX_PUNPCKLDQrr(RS, RD)		_MMXLrr(X86_MMX_PUNPCKLDQ,RS,RD)
#define MMX_PUNPCKLDQmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PUNPCKLDQ, MD, MB, MI, MS, RD)
#define MMX_PXORrr(RS, RD)		_MMXLrr(X86_MMX_PXOR,RS,RD)
#define MMX_PXORmr(MD,MB,MI,MS,RD)	_MMXLmr(X86_MMX_PXOR, MD, MB, MI, MS, RD)

#define MMX_PSHUFWirr(IM, RS, RD)		__MMXLirr(X86_MMX_PSHUFW, IM, RS,_rM, RD,_rM)
#define MMX_PSHUFWimr(IM, MD, MB, MI, MS, RD)	__MMXLimr(X86_MMX_PSHUFW, IM, MD, MB, MI, MS, RD,_rM)
#define MMX_PEXTRWLirr(IM, RS, RD)		__MMXLirr(X86_MMX_PEXTRW, IM, RS,_rM, RD,_r4)
#define MMX_PEXTRWQirr(IM, RS, RD)		__MMXQirr(X86_MMX_PEXTRW, IM, RS,_rM, RD,_r8)
#define MMX_PINSRWLirr(IM, RS, RD)		__MMXLirr(X86_MMX_PINSRW, IM, RS,_r4, RD,_rM)
#define MMX_PINSRWLimr(IM, MD, MB, MI, MS, RD)	__MMXLimr(X86_MMX_PINSRW, IM, MD, MB, MI, MS, RD,_r4)
#define MMX_PINSRWQirr(IM, RS, RD)		__MMXQirr(X86_MMX_PINSRW, IM, RS,_r4, RD,_rM)
#define MMX_PINSRWQimr(IM, MD, MB, MI, MS, RD)	__MMXQimr(X86_MMX_PINSRW, IM, MD, MB, MI, MS, RD,_r8)

// Additionnal MMX instructions, brought by SSSE3 ISA
#define MMX_PABSBrr(RS, RD)		_2P_MMXLrr(X86_MMX_PABSB,RS,RD)
#define MMX_PABSBmr(MD,MB,MI,MS,RD)	_2P_MMXLmr(X86_MMX_PABSB, MD, MB, MI, MS, RD)
#define MMX_PABSWrr(RS, RD)		_2P_MMXLrr(X86_MMX_PABSW,RS,RD)
#define MMX_PABSWmr(MD,MB,MI,MS,RD)	_2P_MMXLmr(X86_MMX_PABSW, MD, MB, MI, MS, RD)
#define MMX_PABSDrr(RS, RD)		_2P_MMXLrr(X86_MMX_PABSD,RS,RD)
#define MMX_PABSDmr(MD,MB,MI,MS,RD)	_2P_MMXLmr(X86_MMX_PABSD, MD, MB, MI, MS, RD)
#define MMX_PHADDWrr(RS, RD)		_2P_MMXLrr(X86_MMX_PHADDW,RS,RD)
#define MMX_PHADDWmr(MD,MB,MI,MS,RD)	_2P_MMXLmr(X86_MMX_PHADDW, MD, MB, MI, MS, RD)
#define MMX_PHADDDrr(RS, RD)		_2P_MMXLrr(X86_MMX_PHADDD,RS,RD)
#define MMX_PHADDDmr(MD,MB,MI,MS,RD)	_2P_MMXLmr(X86_MMX_PHADDD, MD, MB, MI, MS, RD)
#define MMX_PHADDSWrr(RS, RD)		_2P_MMXLrr(X86_MMX_PHADDSW,RS,RD)
#define MMX_PHADDSWmr(MD,MB,MI,MS,RD)	_2P_MMXLmr(X86_MMX_PHADDSW, MD, MB, MI, MS, RD)
#define MMX_PHSUBWrr(RS, RD)		_2P_MMXLrr(X86_MMX_PHSUBW,RS,RD)
#define MMX_PHSUBWmr(MD,MB,MI,MS,RD)	_2P_MMXLmr(X86_MMX_PHSUBW, MD, MB, MI, MS, RD)
#define MMX_PHSUBDrr(RS, RD)		_2P_MMXLrr(X86_MMX_PHSUBD,RS,RD)
#define MMX_PHSUBDmr(MD,MB,MI,MS,RD)	_2P_MMXLmr(X86_MMX_PHSUBD, MD, MB, MI, MS, RD)
#define MMX_PHSUBSWrr(RS, RD)		_2P_MMXLrr(X86_MMX_PHSUBSW,RS,RD)
#define MMX_PHSUBSWmr(MD,MB,MI,MS,RD)	_2P_MMXLmr(X86_MMX_PHSUBSW, MD, MB, MI, MS, RD)
#define MMX_PMADDUBSWrr(RS, RD)		_2P_MMXLrr(X86_MMX_PMADDUBSW,RS,RD)
#define MMX_PMADDUBSWmr(MD,MB,MI,MS,RD)	_2P_MMXLmr(X86_MMX_PMADDUBSW, MD, MB, MI, MS, RD)
#define MMX_PMULHRSWrr(RS, RD)		_2P_MMXLrr(X86_MMX_PMULHRSW,RS,RD)
#define MMX_PMULHRSWmr(MD,MB,MI,MS,RD)	_2P_MMXLmr(X86_MMX_PMULHRSW, MD, MB, MI, MS, RD)
#define MMX_PSHUFBrr(RS, RD)		_2P_MMXLrr(X86_MMX_PSHUFB,RS,RD)
#define MMX_PSHUFBmr(MD,MB,MI,MS,RD)	_2P_MMXLmr(X86_MMX_PSHUFB, MD, MB, MI, MS, RD)
#define MMX_PSIGNBrr(RS, RD)		_2P_MMXLrr(X86_MMX_PSIGNB,RS,RD)
#define MMX_PSIGNBmr(MD,MB,MI,MS,RD)	_2P_MMXLmr(X86_MMX_PSIGNB, MD, MB, MI, MS, RD)
#define MMX_PSIGNWrr(RS, RD)		_2P_MMXLrr(X86_MMX_PSIGNW,RS,RD)
#define MMX_PSIGNWmr(MD,MB,MI,MS,RD)	_2P_MMXLmr(X86_MMX_PSIGNW, MD, MB, MI, MS, RD)
#define MMX_PSIGNDrr(RS, RD)		_2P_MMXLrr(X86_MMX_PSIGND,RS,RD)
#define MMX_PSIGNDmr(MD,MB,MI,MS,RD)	_2P_MMXLmr(X86_MMX_PSIGND, MD, MB, MI, MS, RD)

#define EMMS()								_OO		(0x0f77								)


/* --- Media 128-bit instructions ------------------------------------------ */

enum {
  X86_SSE_CC_EQ		= 0,
  X86_SSE_CC_LT		= 1,
  X86_SSE_CC_GT		= 1,
  X86_SSE_CC_LE		= 2,
  X86_SSE_CC_GE		= 2,
  X86_SSE_CC_U		= 3,
  X86_SSE_CC_NEQ	= 4,
  X86_SSE_CC_NLT	= 5,
  X86_SSE_CC_NGT	= 5,
  X86_SSE_CC_NLE	= 6,
  X86_SSE_CC_NGE	= 6,
  X86_SSE_CC_O		= 7
};

enum {
  X86_SSE_UCOMI  = 0x2e,
  X86_SSE_COMI   = 0x2f,
  X86_SSE_CMP		= 0xc2,
  X86_SSE_SQRT   = 0x51,
  X86_SSE_RSQRT  = 0x52,
  X86_SSE_RCP    = 0x53,
  X86_SSE_AND    = 0x54,
  X86_SSE_ANDN   = 0x55,
  X86_SSE_OR     = 0x56,
  X86_SSE_XOR    = 0x57,
  X86_SSE_ADD    = 0x58,
  X86_SSE_MUL    = 0x59,
  X86_SSE_SUB    = 0x5c,
  X86_SSE_MIN    = 0x5d,
  X86_SSE_DIV    = 0x5e,
  X86_SSE_MAX    = 0x5f,
  X86_SSE_CVTDQ2PD	= 0xe6,
  X86_SSE_CVTDQ2PS	= 0x5b,
  X86_SSE_CVTPD2DQ	= 0xe6,
  X86_SSE_CVTPD2PI	= 0x2d,
  X86_SSE_CVTPD2PS	= 0x5a,
  X86_SSE_CVTPI2PD	= 0x2a,
  X86_SSE_CVTPI2PS	= 0x2a,
  X86_SSE_CVTPS2DQ	= 0x5b,
  X86_SSE_CVTPS2PD	= 0x5a,
  X86_SSE_CVTPS2PI	= 0x2d,
  X86_SSE_CVTSD2SI	= 0x2d,
  X86_SSE_CVTSD2SS	= 0x5a,
  X86_SSE_CVTSI2SD	= 0x2a,
  X86_SSE_CVTSI2SS	= 0x2a,
  X86_SSE_CVTSS2SD	= 0x5a,
  X86_SSE_CVTSS2SI	= 0x2d,
  X86_SSE_CVTTPD2PI	= 0x2c,
  X86_SSE_CVTTPD2DQ	= 0xe6,
  X86_SSE_CVTTPS2DQ	= 0x5b,
  X86_SSE_CVTTPS2PI	= 0x2c,
  X86_SSE_CVTTSD2SI	= 0x2c,
  X86_SSE_CVTTSS2SI	= 0x2c,
  X86_SSE_MOVMSK	= 0x50,
  X86_SSE_PACKSSDW	= 0x6b,
  X86_SSE_PACKSSWB	= 0x63,
  X86_SSE_PACKUSWB	= 0x67,
  X86_SSE_PADDB		= 0xfc,
  X86_SSE_PADDD		= 0xfe,
  X86_SSE_PADDQ		= 0xd4,
  X86_SSE_PADDSB	= 0xec,
  X86_SSE_PADDSW	= 0xed,
  X86_SSE_PADDUSB	= 0xdc,
  X86_SSE_PADDUSW	= 0xdd,
  X86_SSE_PADDW		= 0xfd,
  X86_SSE_PAND		= 0xdb,
  X86_SSE_PANDN		= 0xdf,
  X86_SSE_PAVGB		= 0xe0,
  X86_SSE_PAVGW		= 0xe3,
  X86_SSE_PCMPEQB	= 0x74,
  X86_SSE_PCMPEQD	= 0x76,
  X86_SSE_PCMPEQW	= 0x75,
  X86_SSE_PCMPGTB	= 0x64,
  X86_SSE_PCMPGTD	= 0x66,
  X86_SSE_PCMPGTW	= 0x65,
  X86_SSE_PMADDWD	= 0xf5,
  X86_SSE_PMAXSW	= 0xee,
  X86_SSE_PMAXUB	= 0xde,
  X86_SSE_PMINSW	= 0xea,
  X86_SSE_PMINUB	= 0xda,
  X86_SSE_PMOVMSKB	= 0xd7,
  X86_SSE_PMULHUW	= 0xe4,
  X86_SSE_PMULHW	= 0xe5,
  X86_SSE_PMULLW	= 0xd5,
  X86_SSE_PMULUDQ	= 0xf4,
  X86_SSE_POR		= 0xeb,
  X86_SSE_PSADBW	= 0xf6,
  X86_SSE_PSLLD		= 0xf2,
  X86_SSE_PSLLQ		= 0xf3,
  X86_SSE_PSLLW		= 0xf1,
  X86_SSE_PSRAD		= 0xe2,
  X86_SSE_PSRAW		= 0xe1,
  X86_SSE_PSRLD		= 0xd2,
  X86_SSE_PSRLQ		= 0xd3,
  X86_SSE_PSRLW		= 0xd1,
  X86_SSE_PSUBB		= 0xf8,
  X86_SSE_PSUBD		= 0xfa,
  X86_SSE_PSUBQ		= 0xfb,
  X86_SSE_PSUBSB	= 0xe8,
  X86_SSE_PSUBSW	= 0xe9,
  X86_SSE_PSUBUSB	= 0xd8,
  X86_SSE_PSUBUSW	= 0xd9,
  X86_SSE_PSUBW		= 0xf9,
  X86_SSE_PUNPCKHBW	= 0x68,
  X86_SSE_PUNPCKHDQ	= 0x6a,
  X86_SSE_PUNPCKHQDQ	= 0x6d,
  X86_SSE_PUNPCKHWD	= 0x69,
  X86_SSE_PUNPCKLBW	= 0x60,
  X86_SSE_PUNPCKLDQ	= 0x62,
  X86_SSE_PUNPCKLQDQ	= 0x6c,
  X86_SSE_PUNPCKLWD	= 0x61,
  X86_SSE_PXOR		= 0xef,
  X86_SSSE3_PSHUFB	= 0x00,
};

/*									_format		Opcd		,Mod ,r	     ,m		,mem=dsp+sib	,imm... */

#define _SSSE3Lrr(OP1,OP2,RS,RSA,RD,RDA)	(_B(0x66), _REXLrr(RD,RD),	_B(0x0f), _OO_Mrm	(((OP1)<<8)|(OP2)	,_b11,RDA(RD),RSA(RS)				))
#define _SSSE3Lmr(OP1,OP2,MD,MB,MI,MS,RD,RDA)	(_B(0x66), _REXLmr(MB, MI, RD),	_B(0x0f), _OO_r_X	(((OP1)<<8)|(OP2)	     ,RDA(RD)		,MD,MB,MI,MS		))
#define _SSSE3Lirr(OP1,OP2,IM,RS,RD)		(_B(0x66), _REXLrr(RD, RS),	_B(0x0f), _OO_Mrm_B	(((OP1)<<8)|(OP2)	,_b11,_rX(RD),_rX(RS)			,_u8(IM)))
#define _SSSE3Limr(OP1,OP2,IM,MD,MB,MI,MS,RD)	(_B(0x66), _REXLmr(MB, MI, RD),	_B(0x0f), _OO_r_X_B	(((OP1)<<8)|(OP2)	     ,_rX(RD)		,MD,MB,MI,MS	,_u8(IM)))

#define __SSELir(OP,MO,IM,RD)		(_REXLrr(0, RD),		_OO_Mrm_B	(0x0f00|(OP)	,_b11,MO     ,_rX(RD)			,_u8(IM)))
#define __SSELim(OP,MO,IM,MD,MB,MI,MS)	(_REXLrm(0, MB, MI),		_OO_r_X_B	(0x0f00|(OP)	     ,MO		,MD,MB,MI,MS	,_u8(IM)))
#define __SSELrr(OP,RS,RSA,RD,RDA)	(_REXLrr(RD, RS),		_OO_Mrm		(0x0f00|(OP)	,_b11,RDA(RD),RSA(RS)				))
#define __SSELmr(OP,MD,MB,MI,MS,RD,RDA)	(_REXLmr(MB, MI, RD),		_OO_r_X		(0x0f00|(OP)	     ,RDA(RD)		,MD,MB,MI,MS		))
#define __SSELrm(OP,RS,RSA,MD,MB,MI,MS)	(_REXLrm(RS, MB, MI),		_OO_r_X		(0x0f00|(OP)	     ,RSA(RS)		,MD,MB,MI,MS		))
#define __SSELirr(OP,IM,RS,RD)		(_REXLrr(RD, RS),		_OO_Mrm_B	(0x0f00|(OP)	,_b11,_rX(RD),_rX(RS)			,_u8(IM)))
#define __SSELimr(OP,IM,MD,MB,MI,MS,RD)	(_REXLmr(MB, MI, RD),		_OO_r_X_B	(0x0f00|(OP)	     ,_rX(RD)		,MD,MB,MI,MS	,_u8(IM)))

#define __SSEQrr(OP,RS,RSA,RD,RDA)	(_REXQrr(RD, RS),		_OO_Mrm		(0x0f00|(OP)	,_b11,RDA(RD),RSA(RS)				))
#define __SSEQmr(OP,MD,MB,MI,MS,RD,RDA)	(_REXQmr(MB, MI, RD),		_OO_r_X		(0x0f00|(OP)	     ,RDA(RD)		,MD,MB,MI,MS		))
#define __SSEQrm(OP,RS,RSA,MD,MB,MI,MS)	(_REXQrm(RS, MB, MI),		_OO_r_X		(0x0f00|(OP)	     ,RSA(RS)		,MD,MB,MI,MS		))

#define _SSELrr(PX,OP,RS,RSA,RD,RDA)					(_B(PX), __SSELrr(OP, RS, RSA, RD, RDA))
#define _SSELmr(PX,OP,MD,MB,MI,MS,RD,RDA)				(_B(PX), __SSELmr(OP, MD, MB, MI, MS, RD, RDA))
#define _SSELrm(PX,OP,RS,RSA,MD,MB,MI,MS)				(_B(PX), __SSELrm(OP, RS, RSA, MD, MB, MI, MS))
#define _SSELir(PX,OP,MO,IM,RD)						(_B(PX), __SSELir(OP, MO, IM, RD))
#define _SSELim(PX,OP,MO,IM,MD,MB,MI,MS)				(_B(PX), __SSELim(OP, MO, IM, MD, MB, MI, MS))
#define _SSELirr(PX,OP,IM,RS,RD)					(_B(PX), __SSELirr(OP, IM, RS, RD))
#define _SSELimr(PX,OP,IM,MD,MB,MI,MS,RD)				(_B(PX), __SSELimr(OP, IM, MD, MB, MI, MS, RD))

#define _SSEQrr(PX,OP,RS,RSA,RD,RDA)					(_B(PX), __SSEQrr(OP, RS, RSA, RD, RDA))
#define _SSEQmr(PX,OP,MD,MB,MI,MS,RD,RDA)				(_B(PX), __SSEQmr(OP, MD, MB, MI, MS, RD, RDA))
#define _SSEQrm(PX,OP,RS,RSA,MD,MB,MI,MS)				(_B(PX), __SSEQrm(OP, RS, RSA, MD, MB, MI, MS))

#define _SSEPSrr(OP,RS,RD)		__SSELrr(      OP, RS,_rX, RD,_rX)
#define _SSEPSmr(OP,MD,MB,MI,MS,RD)	__SSELmr(      OP, MD, MB, MI, MS, RD,_rX)
#define _SSEPSrm(OP,RS,MD,MB,MI,MS)	__SSELrm(      OP, RS,_rX, MD, MB, MI, MS)
#define _SSEPSirr(OP,IM,RS,RD)		__SSELirr(     OP, IM, RS, RD)
#define _SSEPSimr(OP,IM,MD,MB,MI,MS,RD)	__SSELimr(     OP, IM, MD, MB, MI, MS, RD)

#define _SSEPDrr(OP,RS,RD)		 _SSELrr(0x66, OP, RS,_rX, RD,_rX)
#define _SSEPDmr(OP,MD,MB,MI,MS,RD)	 _SSELmr(0x66, OP, MD, MB, MI, MS, RD,_rX)
#define _SSEPDrm(OP,RS,MD,MB,MI,MS)	 _SSELrm(0x66, OP, RS,_rX, MD, MB, MI, MS)
#define _SSEPDirr(OP,IM,RS,RD)		 _SSELirr(0x66, OP, IM, RS, RD)
#define _SSEPDimr(OP,IM,MD,MB,MI,MS,RD)	 _SSELimr(0x66, OP, IM, MD, MB, MI, MS, RD)

#define _SSESSrr(OP,RS,RD)		 _SSELrr(0xf3, OP, RS,_rX, RD,_rX)
#define _SSESSmr(OP,MD,MB,MI,MS,RD)	 _SSELmr(0xf3, OP, MD, MB, MI, MS, RD,_rX)
#define _SSESSrm(OP,RS,MD,MB,MI,MS)	 _SSELrm(0xf3, OP, RS,_rX, MD, MB, MI, MS)
#define _SSESSirr(OP,IM,RS,RD)		 _SSELirr(0xf3, OP, IM, RS, RD)
#define _SSESSimr(OP,IM,MD,MB,MI,MS,RD)	 _SSELimr(0xf3, OP, IM, MD, MB, MI, MS, RD)

#define _SSESDrr(OP,RS,RD)		 _SSELrr(0xf2, OP, RS,_rX, RD,_rX)
#define _SSESDmr(OP,MD,MB,MI,MS,RD)	 _SSELmr(0xf2, OP, MD, MB, MI, MS, RD,_rX)
#define _SSESDrm(OP,RS,MD,MB,MI,MS)	 _SSELrm(0xf2, OP, RS,_rX, MD, MB, MI, MS)
#define _SSESDirr(OP,IM,RS,RD)		 _SSELirr(0xf2, OP, IM, RS, RD)
#define _SSESDimr(OP,IM,MD,MB,MI,MS,RD)	 _SSELimr(0xf2, OP, IM, MD, MB, MI, MS, RD)

#define ADDPSrr(RS, RD)			_SSEPSrr(X86_SSE_ADD, RS, RD)
#define ADDPSmr(MD, MB, MI, MS, RD)	_SSEPSmr(X86_SSE_ADD, MD, MB, MI, MS, RD)
#define ADDPDrr(RS, RD)			_SSEPDrr(X86_SSE_ADD, RS, RD)
#define ADDPDmr(MD, MB, MI, MS, RD)	_SSEPDmr(X86_SSE_ADD, MD, MB, MI, MS, RD)

#define ADDSSrr(RS, RD)			_SSESSrr(X86_SSE_ADD, RS, RD)
#define ADDSSmr(MD, MB, MI, MS, RD)	_SSESSmr(X86_SSE_ADD, MD, MB, MI, MS, RD)
#define ADDSDrr(RS, RD)			_SSESDrr(X86_SSE_ADD, RS, RD)
#define ADDSDmr(MD, MB, MI, MS, RD)	_SSESDmr(X86_SSE_ADD, MD, MB, MI, MS, RD)

#define ANDNPSrr(RS, RD)		_SSEPSrr(X86_SSE_ANDN, RS, RD)
#define ANDNPSmr(MD, MB, MI, MS, RD)	_SSEPSmr(X86_SSE_ANDN, MD, MB, MI, MS, RD)
#define ANDNPDrr(RS, RD)		_SSEPDrr(X86_SSE_ANDN, RS, RD)
#define ANDNPDmr(MD, MB, MI, MS, RD)	_SSEPDmr(X86_SSE_ANDN, MD, MB, MI, MS, RD)

#define ANDPSrr(RS, RD)			_SSEPSrr(X86_SSE_AND, RS, RD)
#define ANDPSmr(MD, MB, MI, MS, RD)	_SSEPSmr(X86_SSE_AND, MD, MB, MI, MS, RD)
#define ANDPDrr(RS, RD)			_SSEPDrr(X86_SSE_AND, RS, RD)
#define ANDPDmr(MD, MB, MI, MS, RD)	_SSEPDmr(X86_SSE_AND, MD, MB, MI, MS, RD)

#define CMPPSrr(IM, RS, RD)		_SSEPSirr(X86_SSE_CMP, IM, RS, RD)
#define CMPPSmr(IM, MD, MB, MI, MS, RD)	_SSEPSimr(X86_SSE_CMP, IM, MD, MB, MI, MS, RD)
#define CMPPDrr(IM, RS, RD)		_SSEPDirr(X86_SSE_CMP, IM, RS, RD)
#define CMPPDmr(IM, MD, MB, MI, MS, RD)	_SSEPDimr(X86_SSE_CMP, IM, MD, MB, MI, MS, RD)

#define CMPSSrr(IM, RS, RD)		_SSESSirr(X86_SSE_CMP, IM, RS, RD)
#define CMPSSmr(IM, MD, MB, MI, MS, RD)	_SSESSimr(X86_SSE_CMP, IM, MD, MB, MI, MS, RD)
#define CMPSDrr(IM, RS, RD)		_SSESDirr(X86_SSE_CMP, IM, RS, RD)
#define CMPSDmr(IM, MD, MB, MI, MS, RD)	_SSESDimr(X86_SSE_CMP, IM, MD, MB, MI, MS, RD)

#define DIVPSrr(RS, RD)			_SSEPSrr(X86_SSE_DIV, RS, RD)
#define DIVPSmr(MD, MB, MI, MS, RD)	_SSEPSmr(X86_SSE_DIV, MD, MB, MI, MS, RD)
#define DIVPDrr(RS, RD)			_SSEPDrr(X86_SSE_DIV, RS, RD)
#define DIVPDmr(MD, MB, MI, MS, RD)	_SSEPDmr(X86_SSE_DIV, MD, MB, MI, MS, RD)

#define DIVSSrr(RS, RD)			_SSESSrr(X86_SSE_DIV, RS, RD)
#define DIVSSmr(MD, MB, MI, MS, RD)	_SSESSmr(X86_SSE_DIV, MD, MB, MI, MS, RD)
#define DIVSDrr(RS, RD)			_SSESDrr(X86_SSE_DIV, RS, RD)
#define DIVSDmr(MD, MB, MI, MS, RD)	_SSESDmr(X86_SSE_DIV, MD, MB, MI, MS, RD)

#define MAXPSrr(RS, RD)			_SSEPSrr(X86_SSE_MAX, RS, RD)
#define MAXPSmr(MD, MB, MI, MS, RD)	_SSEPSmr(X86_SSE_MAX, MD, MB, MI, MS, RD)
#define MAXPDrr(RS, RD)			_SSEPDrr(X86_SSE_MAX, RS, RD)
#define MAXPDmr(MD, MB, MI, MS, RD)	_SSEPDmr(X86_SSE_MAX, MD, MB, MI, MS, RD)

#define MAXSSrr(RS, RD)			_SSESSrr(X86_SSE_MAX, RS, RD)
#define MAXSSmr(MD, MB, MI, MS, RD)	_SSESSmr(X86_SSE_MAX, MD, MB, MI, MS, RD)
#define MAXSDrr(RS, RD)			_SSESDrr(X86_SSE_MAX, RS, RD)
#define MAXSDmr(MD, MB, MI, MS, RD)	_SSESDmr(X86_SSE_MAX, MD, MB, MI, MS, RD)

#define MINPSrr(RS, RD)			_SSEPSrr(X86_SSE_MIN, RS, RD)
#define MINPSmr(MD, MB, MI, MS, RD)	_SSEPSmr(X86_SSE_MIN, MD, MB, MI, MS, RD)
#define MINPDrr(RS, RD)			_SSEPDrr(X86_SSE_MIN, RS, RD)
#define MINPDmr(MD, MB, MI, MS, RD)	_SSEPDmr(X86_SSE_MIN, MD, MB, MI, MS, RD)

#define MINSSrr(RS, RD)			_SSESSrr(X86_SSE_MIN, RS, RD)
#define MINSSmr(MD, MB, MI, MS, RD)	_SSESSmr(X86_SSE_MIN, MD, MB, MI, MS, RD)
#define MINSDrr(RS, RD)			_SSESDrr(X86_SSE_MIN, RS, RD)
#define MINSDmr(MD, MB, MI, MS, RD)	_SSESDmr(X86_SSE_MIN, MD, MB, MI, MS, RD)

#define MULPSrr(RS, RD)			_SSEPSrr(X86_SSE_MUL, RS, RD)
#define MULPSmr(MD, MB, MI, MS, RD)	_SSEPSmr(X86_SSE_MUL, MD, MB, MI, MS, RD)
#define MULPDrr(RS, RD)			_SSEPDrr(X86_SSE_MUL, RS, RD)
#define MULPDmr(MD, MB, MI, MS, RD)	_SSEPDmr(X86_SSE_MUL, MD, MB, MI, MS, RD)

#define MULSSrr(RS, RD)			_SSESSrr(X86_SSE_MUL, RS, RD)
#define MULSSmr(MD, MB, MI, MS, RD)	_SSESSmr(X86_SSE_MUL, MD, MB, MI, MS, RD)
#define MULSDrr(RS, RD)			_SSESDrr(X86_SSE_MUL, RS, RD)
#define MULSDmr(MD, MB, MI, MS, RD)	_SSESDmr(X86_SSE_MUL, MD, MB, MI, MS, RD)

#define ORPSrr(RS, RD)			_SSEPSrr(X86_SSE_OR, RS, RD)
#define ORPSmr(MD, MB, MI, MS, RD)	_SSEPSmr(X86_SSE_OR, MD, MB, MI, MS, RD)
#define ORPDrr(RS, RD)			_SSEPDrr(X86_SSE_OR, RS, RD)
#define ORPDmr(MD, MB, MI, MS, RD)	_SSEPDmr(X86_SSE_OR, MD, MB, MI, MS, RD)

#define RCPPSrr(RS, RD)			_SSEPSrr(X86_SSE_RCP, RS, RD)
#define RCPPSmr(MD, MB, MI, MS, RD)	_SSEPSmr(X86_SSE_RCP, MD, MB, MI, MS, RD)
#define RCPSSrr(RS, RD)			_SSESSrr(X86_SSE_RCP, RS, RD)
#define RCPSSmr(MD, MB, MI, MS, RD)	_SSESSmr(X86_SSE_RCP, MD, MB, MI, MS, RD)

#define RSQRTPSrr(RS, RD)		_SSEPSrr(X86_SSE_RSQRT, RS, RD)
#define RSQRTPSmr(MD, MB, MI, MS, RD)	_SSEPSmr(X86_SSE_RSQRT, MD, MB, MI, MS, RD)
#define RSQRTSSrr(RS, RD)		_SSESSrr(X86_SSE_RSQRT, RS, RD)
#define RSQRTSSmr(MD, MB, MI, MS, RD)	_SSESSmr(X86_SSE_RSQRT, MD, MB, MI, MS, RD)

#define SQRTPSrr(RS, RD)		_SSEPSrr(X86_SSE_SQRT, RS, RD)
#define SQRTPSmr(MD, MB, MI, MS, RD)	_SSEPSmr(X86_SSE_SQRT, MD, MB, MI, MS, RD)
#define SQRTPDrr(RS, RD)		_SSEPDrr(X86_SSE_SQRT, RS, RD)
#define SQRTPDmr(MD, MB, MI, MS, RD)	_SSEPDmr(X86_SSE_SQRT, MD, MB, MI, MS, RD)

#define SQRTSSrr(RS, RD)		_SSESSrr(X86_SSE_SQRT, RS, RD)
#define SQRTSSmr(MD, MB, MI, MS, RD)	_SSESSmr(X86_SSE_SQRT, MD, MB, MI, MS, RD)
#define SQRTSDrr(RS, RD)		_SSESDrr(X86_SSE_SQRT, RS, RD)
#define SQRTSDmr(MD, MB, MI, MS, RD)	_SSESDmr(X86_SSE_SQRT, MD, MB, MI, MS, RD)

#define SUBPSrr(RS, RD)			_SSEPSrr(X86_SSE_SUB, RS, RD)
#define SUBPSmr(MD, MB, MI, MS, RD)	_SSEPSmr(X86_SSE_SUB, MD, MB, MI, MS, RD)
#define SUBPDrr(RS, RD)			_SSEPDrr(X86_SSE_SUB, RS, RD)
#define SUBPDmr(MD, MB, MI, MS, RD)	_SSEPDmr(X86_SSE_SUB, MD, MB, MI, MS, RD)

#define SUBSSrr(RS, RD)			_SSESSrr(X86_SSE_SUB, RS, RD)
#define SUBSSmr(MD, MB, MI, MS, RD)	_SSESSmr(X86_SSE_SUB, MD, MB, MI, MS, RD)
#define SUBSDrr(RS, RD)			_SSESDrr(X86_SSE_SUB, RS, RD)
#define SUBSDmr(MD, MB, MI, MS, RD)	_SSESDmr(X86_SSE_SUB, MD, MB, MI, MS, RD)

#define XORPSrr(RS, RD)			_SSEPSrr(X86_SSE_XOR, RS, RD)
#define XORPSmr(MD, MB, MI, MS, RD)	_SSEPSmr(X86_SSE_XOR, MD, MB, MI, MS, RD)
#define XORPDrr(RS, RD)			_SSEPDrr(X86_SSE_XOR, RS, RD)
#define XORPDmr(MD, MB, MI, MS, RD)	_SSEPDmr(X86_SSE_XOR, MD, MB, MI, MS, RD)

#define COMISSrr(RS, RD)		_SSEPSrr(X86_SSE_COMI, RS, RD)
#define COMISSmr(MD, MB, MI, MS, RD)	_SSEPSmr(X86_SSE_COMI, MD, MB, MI, MS, RD)
#define COMISDrr(RS, RD)		_SSEPDrr(X86_SSE_COMI, RS, RD)
#define COMISDmr(MD, MB, MI, MS, RD)	_SSEPDmr(X86_SSE_COMI, MD, MB, MI, MS, RD)

#define UCOMISSrr(RS, RD)		_SSEPSrr(X86_SSE_UCOMI, RS, RD)
#define UCOMISSmr(MD, MB, MI, MS, RD)	_SSEPSmr(X86_SSE_UCOMI, MD, MB, MI, MS, RD)
#define UCOMISDrr(RS, RD)		_SSEPDrr(X86_SSE_UCOMI, RS, RD)
#define UCOMISDmr(MD, MB, MI, MS, RD)	_SSEPDmr(X86_SSE_UCOMI, MD, MB, MI, MS, RD)

#define MOVAPSrr(RS, RD)		_SSEPSrr(0x28, RS, RD)
#define MOVAPSmr(MD, MB, MI, MS, RD)	_SSEPSmr(0x28, MD, MB, MI, MS, RD)
#define MOVAPSrm(RS, MD, MB, MI, MS)	_SSEPSrm(0x29, RS, MD, MB, MI, MS)

#define MOVAPDrr(RS, RD)		_SSEPDrr(0x28, RS, RD)
#define MOVAPDmr(MD, MB, MI, MS, RD)	_SSEPDmr(0x28, MD, MB, MI, MS, RD)
#define MOVAPDrm(RS, MD, MB, MI, MS)	_SSEPDrm(0x29, RS, MD, MB, MI, MS)

#define CVTDQ2PDrr(RS, RD)		 _SSELrr(0xf3, X86_SSE_CVTDQ2PD, RS,_rX, RD,_rX)
#define CVTDQ2PDmr(MD, MB, MI, MS, RD)	 _SSELmr(0xf3, X86_SSE_CVTDQ2PD, MD, MB, MI, MS, RD,_rX)
#define CVTDQ2PSrr(RS, RD)		__SSELrr(      X86_SSE_CVTDQ2PS, RS,_rX, RD,_rX)
#define CVTDQ2PSmr(MD, MB, MI, MS, RD)	__SSELmr(      X86_SSE_CVTDQ2PS, MD, MB, MI, MS, RD,_rX)
#define CVTPD2DQrr(RS, RD)		 _SSELrr(0xf2, X86_SSE_CVTPD2DQ, RS,_rX, RD,_rX)
#define CVTPD2DQmr(MD, MB, MI, MS, RD)	 _SSELmr(0xf2, X86_SSE_CVTPD2DQ, MD, MB, MI, MS, RD,_rX)
#define CVTPD2PIrr(RS, RD)		 _SSELrr(0x66, X86_SSE_CVTPD2PI, RS,_rX, RD,_rM)
#define CVTPD2PImr(MD, MB, MI, MS, RD)	 _SSELmr(0x66, X86_SSE_CVTPD2PI, MD, MB, MI, MS, RD,_rM)
#define CVTPD2PSrr(RS, RD)		 _SSELrr(0x66, X86_SSE_CVTPD2PS, RS,_rX, RD,_rX)
#define CVTPD2PSmr(MD, MB, MI, MS, RD)	 _SSELmr(0x66, X86_SSE_CVTPD2PS, MD, MB, MI, MS, RD,_rX)
#define CVTPI2PDrr(RS, RD)		 _SSELrr(0x66, X86_SSE_CVTPI2PD, RS,_rM, RD,_rX)
#define CVTPI2PDmr(MD, MB, MI, MS, RD)	 _SSELmr(0x66, X86_SSE_CVTPI2PD, MD, MB, MI, MS, RD,_rX)
#define CVTPI2PSrr(RS, RD)		__SSELrr(      X86_SSE_CVTPI2PS, RS,_rM, RD,_rX)
#define CVTPI2PSmr(MD, MB, MI, MS, RD)	__SSELmr(      X86_SSE_CVTPI2PS, MD, MB, MI, MS, RD,_rX)
#define CVTPS2DQrr(RS, RD)		 _SSELrr(0x66, X86_SSE_CVTPS2DQ, RS,_rX, RD,_rX)
#define CVTPS2DQmr(MD, MB, MI, MS, RD)	 _SSELmr(0x66, X86_SSE_CVTPS2DQ, MD, MB, MI, MS, RD,_rX)
#define CVTPS2PDrr(RS, RD)		__SSELrr(      X86_SSE_CVTPS2PD, RS,_rX, RD,_rX)
#define CVTPS2PDmr(MD, MB, MI, MS, RD)	__SSELmr(      X86_SSE_CVTPS2PD, MD, MB, MI, MS, RD,_rX)
#define CVTPS2PIrr(RS, RD)		__SSELrr(      X86_SSE_CVTPS2PI, RS,_rX, RD,_rM)
#define CVTPS2PImr(MD, MB, MI, MS, RD)	__SSELmr(      X86_SSE_CVTPS2PI, MD, MB, MI, MS, RD,_rM)
#define CVTSD2SILrr(RS, RD)		 _SSELrr(0xf2, X86_SSE_CVTSD2SI, RS,_rX, RD,_r4)
#define CVTSD2SILmr(MD, MB, MI, MS, RD)	 _SSELmr(0xf2, X86_SSE_CVTSD2SI, MD, MB, MI, MS, RD,_r4)
#define CVTSD2SIQrr(RS, RD)		 _SSEQrr(0xf2, X86_SSE_CVTSD2SI, RS,_rX, RD,_r8)
#define CVTSD2SIQmr(MD, MB, MI, MS, RD)	 _SSEQmr(0xf2, X86_SSE_CVTSD2SI, MD, MB, MI, MS, RD,_r8)
#define CVTSD2SSrr(RS, RD)		 _SSELrr(0xf2, X86_SSE_CVTSD2SS, RS,_rX, RD,_rX)
#define CVTSD2SSmr(MD, MB, MI, MS, RD)	 _SSELmr(0xf2, X86_SSE_CVTSD2SS, MD, MB, MI, MS, RD,_rX)
#define CVTSI2SDLrr(RS, RD)		 _SSELrr(0xf2, X86_SSE_CVTSI2SD, RS,_r4, RD,_rX)
#define CVTSI2SDLmr(MD, MB, MI, MS, RD)	 _SSELmr(0xf2, X86_SSE_CVTSI2SD, MD, MB, MI, MS, RD,_rX)
#define CVTSI2SDQrr(RS, RD)		 _SSEQrr(0xf2, X86_SSE_CVTSI2SD, RS,_r8, RD,_rX)
#define CVTSI2SDQmr(MD, MB, MI, MS, RD)	 _SSEQmr(0xf2, X86_SSE_CVTSI2SD, MD, MB, MI, MS, RD,_rX)
#define CVTSI2SSLrr(RS, RD)		 _SSELrr(0xf3, X86_SSE_CVTSI2SS, RS,_r4, RD,_rX)
#define CVTSI2SSLmr(MD, MB, MI, MS, RD)	 _SSELmr(0xf3, X86_SSE_CVTSI2SS, MD, MB, MI, MS, RD,_rX)
#define CVTSI2SSQrr(RS, RD)		 _SSEQrr(0xf3, X86_SSE_CVTSI2SS, RS,_r8, RD,_rX)
#define CVTSI2SSQmr(MD, MB, MI, MS, RD)	 _SSEQmr(0xf3, X86_SSE_CVTSI2SS, MD, MB, MI, MS, RD,_rX)
#define CVTSS2SDrr(RS, RD)		 _SSELrr(0xf3, X86_SSE_CVTSS2SD, RS,_rX, RD,_rX)
#define CVTSS2SDmr(MD, MB, MI, MS, RD)	 _SSELmr(0xf3, X86_SSE_CVTSS2SD, MD, MB, MI, MS, RD,_rX)
#define CVTSS2SILrr(RS, RD)		 _SSELrr(0xf3, X86_SSE_CVTSS2SI, RS,_rX, RD,_r4)
#define CVTSS2SILmr(MD, MB, MI, MS, RD)	 _SSELmr(0xf3, X86_SSE_CVTSS2SI, MD, MB, MI, MS, RD,_r4)
#define CVTSS2SIQrr(RS, RD)		 _SSEQrr(0xf3, X86_SSE_CVTSS2SI, RS,_rX, RD,_r8)
#define CVTSS2SIQmr(MD, MB, MI, MS, RD)	 _SSEQmr(0xf3, X86_SSE_CVTSS2SI, MD, MB, MI, MS, RD,_r8)
#define CVTTPD2PIrr(RS, RD)		 _SSELrr(0x66, X86_SSE_CVTTPD2PI, RS,_rX, RD,_rM)
#define CVTTPD2PImr(MD, MB, MI, MS, RD)	 _SSELmr(0x66, X86_SSE_CVTTPD2PI, MD, MB, MI, MS, RD,_rM)
#define CVTTPD2DQrr(RS, RD)		 _SSELrr(0x66, X86_SSE_CVTTPD2DQ, RS,_rX, RD,_rX)
#define CVTTPD2DQmr(MD, MB, MI, MS, RD)	 _SSELmr(0x66, X86_SSE_CVTTPD2DQ, MD, MB, MI, MS, RD,_rX)
#define CVTTPS2DQrr(RS, RD)		 _SSELrr(0xf3, X86_SSE_CVTTPS2DQ, RS,_rX, RD,_rX)
#define CVTTPS2DQmr(MD, MB, MI, MS, RD)	 _SSELmr(0xf3, X86_SSE_CVTTPS2DQ, MD, MB, MI, MS, RD,_rX)
#define CVTTPS2PIrr(RS, RD)		__SSELrr(      X86_SSE_CVTTPS2PI, RS,_rX, RD,_rM)
#define CVTTPS2PImr(MD, MB, MI, MS, RD)	__SSELmr(      X86_SSE_CVTTPS2PI, MD, MB, MI, MS, RD,_rM)
#define CVTTSD2SILrr(RS, RD)		 _SSELrr(0xf2, X86_SSE_CVTTSD2SI, RS,_rX, RD,_r4)
#define CVTTSD2SILmr(MD, MB, MI, MS, RD) _SSELmr(0xf2, X86_SSE_CVTTSD2SI, MD, MB, MI, MS, RD,_r4)
#define CVTTSD2SIQrr(RS, RD)		 _SSEQrr(0xf2, X86_SSE_CVTTSD2SI, RS,_rX, RD,_r8)
#define CVTTSD2SIQmr(MD, MB, MI, MS, RD) _SSEQmr(0xf2, X86_SSE_CVTTSD2SI, MD, MB, MI, MS, RD,_r8)
#define CVTTSS2SILrr(RS, RD)		 _SSELrr(0xf3, X86_SSE_CVTTSS2SI, RS,_rX, RD,_r4)
#define CVTTSS2SILmr(MD, MB, MI, MS, RD) _SSELmr(0xf3, X86_SSE_CVTTSS2SI, MD, MB, MI, MS, RD,_r4)
#define CVTTSS2SIQrr(RS, RD)		 _SSEQrr(0xf3, X86_SSE_CVTTSS2SI, RS,_rX, RD,_r8)
#define CVTTSS2SIQmr(MD, MB, MI, MS, RD) _SSEQmr(0xf3, X86_SSE_CVTTSS2SI, MD, MB, MI, MS, RD,_r8)

#define MOVDXDrr(RS, RD)		 _SSELrr(0x66, 0x6e, RS,_r4, RD,_rX)
#define MOVDXDmr(MD, MB, MI, MS, RD)	 _SSELmr(0x66, 0x6e, MD, MB, MI, MS, RD,_rX)
#define MOVQXDrr(RS, RD)		 _SSEQrr(0x66, 0x6e, RS,_r8, RD,_rX)
#define MOVQXDmr(MD, MB, MI, MS, RD)	 _SSEQmr(0x66, 0x6e, MD, MB, MI, MS, RD,_rX)

#define MOVDXSrr(RS, RD)		 _SSELrr(0x66, 0x7e, RD,_r4, RS,_rX)
#define MOVDXSrm(RS, MD, MB, MI, MS)	 _SSELrm(0x66, 0x7e, RS,_rX, MD, MB, MI, MS)
#define MOVQXSrr(RS, RD)		 _SSEQrr(0x66, 0x7e, RD,_r8, RS,_rX)
#define MOVQXSrm(RS, MD, MB, MI, MS)	 _SSEQrm(0x66, 0x7e, RS,_rX, MD, MB, MI, MS)

#define MOVDLMrr(RS, RD)		__SSELrr(      0x6e, RS,_r4, RD,_rM)
#define MOVDLMmr(MD, MB, MI, MS, RD)	__SSELmr(      0x6e, MD, MB, MI, MS, RD,_rM)
#define MOVDQMrr(RS, RD)		__SSEQrr(      0x6e, RS,_r8, RD,_rM)
#define MOVDQMmr(MD, MB, MI, MS, RD)	__SSEQmr(      0x6e, MD, MB, MI, MS, RD,_rM)

#define MOVDMLrr(RS, RD)		__SSELrr(      0x7e, RS,_rM, RD,_r4)
#define MOVDMLrm(RS, MD, MB, MI, MS)	__SSELrm(      0x7e, RS,_rM, MD, MB, MI, MS)
#define MOVDMQrr(RS, RD)		__SSEQrr(      0x7e, RS,_rM, RD,_r8)
#define MOVDMQrm(RS, MD, MB, MI, MS)	__SSEQrm(      0x7e, RS,_rM, MD, MB, MI, MS)

#define MOVDQ2Qrr(RS, RD)		 _SSELrr(0xf2, 0xd6, RS,_rX, RD,_rM)
#define MOVMSKPSrr(RS, RD)		__SSELrr(      0x50, RS,_rX, RD,_r4)
#define MOVMSKPDrr(RS, RD)		 _SSELrr(0x66, 0x50, RS,_rX, RD,_r4)

#define MOVHLPSrr(RS, RD)		__SSELrr(      0x12, RS,_rX, RD,_rX)
#define MOVLHPSrr(RS, RD)		__SSELrr(      0x16, RS,_rX, RD,_rX)

#define MOVDQArr(RS, RD)		 _SSELrr(0x66, 0x6f, RS,_rX, RD,_rX)
#define MOVDQAmr(MD, MB, MI, MS, RD)	 _SSELmr(0x66, 0x6f, MD, MB, MI, MS, RD,_rX)
#define MOVDQArm(RS, MD, MB, MI, MS)	 _SSELrm(0x66, 0x7f, RS,_rX, MD, MB, MI, MS)

#define MOVDQUrr(RS, RD)		 _SSELrr(0xf3, 0x6f, RS,_rX, RD,_rX)
#define MOVDQUmr(MD, MB, MI, MS, RD)	 _SSELmr(0xf3, 0x6f, MD, MB, MI, MS, RD,_rX)
#define MOVDQUrm(RS, MD, MB, MI, MS)	 _SSELrm(0xf3, 0x7f, RS,_rX, MD, MB, MI, MS)

#define MOVHPDmr(MD, MB, MI, MS, RD)	 _SSELmr(0x66, 0x16, MD, MB, MI, MS, RD,_rX)
#define MOVHPDrm(RS, MD, MB, MI, MS)	 _SSELrm(0x66, 0x17, RS,_rX, MD, MB, MI, MS)
#define MOVHPSmr(MD, MB, MI, MS, RD)	__SSELmr(      0x16, MD, MB, MI, MS, RD,_rX)
#define MOVHPSrm(RS, MD, MB, MI, MS)	__SSELrm(      0x17, RS,_rX, MD, MB, MI, MS)

#define MOVLPDmr(MD, MB, MI, MS, RD)	 _SSELmr(0x66, 0x12, MD, MB, MI, MS, RD,_rX)
#define MOVLPDrm(RS, MD, MB, MI, MS)	 _SSELrm(0x66, 0x13, RS,_rX, MD, MB, MI, MS)
#define MOVLPSmr(MD, MB, MI, MS, RD)	__SSELmr(      0x12, MD, MB, MI, MS, RD,_rX)
#define MOVLPSrm(RS, MD, MB, MI, MS)	__SSELrm(      0x13, RS,_rX, MD, MB, MI, MS)


/* --- FLoating-Point instructions ----------------------------------------- */

#define _ESCmi(D,B,I,S,OP)	(_REXLrm(0,B,I), _O_r_X(0xd8|(OP & 7), (OP >> 3), D,B,I,S))

#define FLDr(R)			_OOr(0xd9c0,_rN(R))
#define FLDLm(D,B,I,S)		_ESCmi(D,B,I,S,005)
#define FLDSm(D,B,I,S)		_ESCmi(D,B,I,S,001)
#define FLDTm(D,B,I,S)		_ESCmi(D,B,I,S,053)

#define FSTr(R)			_OOr(0xddd0,_rN(R))
#define FSTSm(D,B,I,S)		_ESCmi(D,B,I,S,021)
#define FSTLm(D,B,I,S)		_ESCmi(D,B,I,S,025)

#define FSTPr(R)		_OOr(0xddd8,_rN(R))
#define FSTPSm(D,B,I,S)		_ESCmi(D,B,I,S,031)
#define FSTPLm(D,B,I,S)		_ESCmi(D,B,I,S,035)
#define FSTPTm(D,B,I,S)		_ESCmi(D,B,I,S,073)

#define FADDr0(R)		_OOr(0xd8c0,_rN(R))
#define FADD0r(R)		_OOr(0xdcc0,_rN(R))
#define FADDP0r(R)		_OOr(0xdec0,_rN(R))
#define FADDSm(D,B,I,S)		_ESCmi(D,B,I,S,000)
#define FADDLm(D,B,I,S)		_ESCmi(D,B,I,S,004)

#define FSUBSm(D,B,I,S)		_ESCmi(D,B,I,S,040)
#define FSUBLm(D,B,I,S)		_ESCmi(D,B,I,S,044)
#define FSUBr0(R)		_OOr(0xd8e0,_rN(R))
#define FSUB0r(R)		_OOr(0xdce8,_rN(R))
#define FSUBP0r(R)		_OOr(0xdee8,_rN(R))

#define FSUBRr0(R)		_OOr(0xd8e8,_rN(R))
#define FSUBR0r(R)		_OOr(0xdce0,_rN(R))
#define FSUBRP0r(R)		_OOr(0xdee0,_rN(R))
#define FSUBRSm(D,B,I,S)	_ESCmi(D,B,I,S,050)
#define FSUBRLm(D,B,I,S)	_ESCmi(D,B,I,S,054)

#define FMULr0(R)		_OOr(0xd8c8,_rN(R))
#define FMUL0r(R)		_OOr(0xdcc8,_rN(R))
#define FMULP0r(R)		_OOr(0xdec8,_rN(R))
#define FMULSm(D,B,I,S)		_ESCmi(D,B,I,S,010)
#define FMULLm(D,B,I,S)		_ESCmi(D,B,I,S,014)

#define FDIVr0(R)		_OOr(0xd8f0,_rN(R))
#define FDIV0r(R)		_OOr(0xdcf8,_rN(R))
#define FDIVP0r(R)		_OOr(0xdef8,_rN(R))
#define FDIVSm(D,B,I,S)		_ESCmi(D,B,I,S,060)
#define FDIVLm(D,B,I,S)		_ESCmi(D,B,I,S,064)

#define FDIVRr0(R)		_OOr(0xd8f8,_rN(R))
#define FDIVR0r(R)		_OOr(0xdcf0,_rN(R))
#define FDIVRP0r(R)		_OOr(0xdef0,_rN(R))
#define FDIVRSm(D,B,I,S)	_ESCmi(D,B,I,S,070)
#define FDIVRLm(D,B,I,S)	_ESCmi(D,B,I,S,074)

#define FCMOVBr0(R)		_OOr(0xdac0,_rN(R))
#define FCMOVBEr0(R)		_OOr(0xdad0,_rN(R))
#define FCMOVEr0(R)		_OOr(0xdac8,_rN(R))
#define FCMOVNBr0(R)		_OOr(0xdbc0,_rN(R))
#define FCMOVNBEr0(R)		_OOr(0xdbd0,_rN(R))
#define FCMOVNEr0(R)		_OOr(0xdbc8,_rN(R))
#define FCMOVNUr0(R)		_OOr(0xdbd8,_rN(R))
#define FCMOVUr0(R)		_OOr(0xdad8,_rN(R))
#define FCOMIr0(R)		_OOr(0xdbf0,_rN(R))
#define FCOMIPr0(R)		_OOr(0xdff0,_rN(R))

#define FCOMr(R)		_OOr(0xd8d0,_rN(R))
#define FCOMSm(D,B,I,S)		_ESCmi(D,B,I,S,020)
#define FCOMLm(D,B,I,S)		_ESCmi(D,B,I,S,024)

#define FCOMPr(R)		_OOr(0xd8d8,_rN(R))
#define FCOMPSm(D,B,I,S)	_ESCmi(D,B,I,S,030)
#define FCOMPLm(D,B,I,S)	_ESCmi(D,B,I,S,034)

#define FUCOMIr0(R)		_OOr(0xdbe8,_rN(R))
#define FUCOMIPr0(R)		_OOr(0xdfe8,_rN(R))
#define FUCOMPr(R)		_OOr(0xdde8,_rN(R))
#define FUCOMr(R)		_OOr(0xdde0,_rN(R))

#define FIADDLm(D,B,I,S)	_ESCmi(D,B,I,S,002)
#define FICOMLm(D,B,I,S)	_ESCmi(D,B,I,S,022)
#define FICOMPLm(D,B,I,S)	_ESCmi(D,B,I,S,032)
#define FIDIVLm(D,B,I,S)	_ESCmi(D,B,I,S,062)
#define FIDIVRLm(D,B,I,S)	_ESCmi(D,B,I,S,072)
#define FILDLm(D,B,I,S)		_ESCmi(D,B,I,S,003)
#define FILDQm(D,B,I,S)		_ESCmi(D,B,I,S,057)
#define FIMULLm(D,B,I,S)	_ESCmi(D,B,I,S,012)
#define FISTLm(D,B,I,S)		_ESCmi(D,B,I,S,023)
#define FISTPLm(D,B,I,S)	_ESCmi(D,B,I,S,033)
#define FISTPQm(D,B,I,S)	_ESCmi(D,B,I,S,077)
#define FISUBLm(D,B,I,S)	_ESCmi(D,B,I,S,042)
#define FISUBRLm(D,B,I,S)	_ESCmi(D,B,I,S,052)

#define FREEr(R)		_OOr(0xddc0,_rN(R))
#define FXCHr(R)		_OOr(0xd9c8,_rN(R))

#endif /* X86_RTASM_H */
