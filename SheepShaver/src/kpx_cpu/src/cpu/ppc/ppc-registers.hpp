/*
 *  ppc-registers.hpp - PowerPC registers definition
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

#ifndef PPC_REGISTERS_H
#define PPC_REGISTERS_H

#include "cpu/ppc/ppc-bitfields.hpp"

/**
 *		Condition Register
 **/

class powerpc_cr_register
{
	uint32 cr;
public:
	bool test(int condition) const;
	void set(uint32 v);
	uint32 get() const;
	void clear(int crfd);
	void set(int crfd, uint32 v);
	uint32 get(int crfd) const;
	void set_so(int crfd, bool v);
	void compute(int crfd, int32 v);
};

inline void
powerpc_cr_register::clear(int crfd)
{
	cr &= ~(0xf << (28 - 4 * crfd));
}

inline void
powerpc_cr_register::set(int crfd, uint32 v)
{
	clear(crfd);
	cr |= v << (28 - 4 * crfd);
}

inline uint32
powerpc_cr_register::get(int crfd) const
{
	return (cr >> (28 - 4 * crfd)) & 0xf;
}

inline void
powerpc_cr_register::set_so(int crfd, bool v)
{
	const uint32 m = standalone_CR_SO_field::mask() << (28 - 4 * crfd);
	cr = (cr & ~m) | (v ? m : 0);
}

inline void
powerpc_cr_register::compute(int crfd, int32 v)
{
	const uint32 m = (standalone_CR_LT_field::mask() |
					  standalone_CR_GT_field::mask() |
					  standalone_CR_EQ_field::mask() ) << (28 - 4 * crfd);
	cr = (cr & ~m);
	if (v < 0)
		cr |= standalone_CR_LT_field::mask() << (28 - 4 * crfd);
	else if (v > 0)
		cr |= standalone_CR_GT_field::mask() << (28 - 4 * crfd);
	else
		cr |= standalone_CR_EQ_field::mask() << (28 - 4 * crfd);
}

inline void
powerpc_cr_register::set(uint32 v)
{
	cr = v;
}

inline uint32
powerpc_cr_register::get() const
{
	return cr;
}

inline bool
powerpc_cr_register::test(int condition) const
{
	return (cr << condition) & 0x80000000;
}


/**
 *		XER register (SPR1)
 **/

class powerpc_xer_register
{
	uint8 so;
	uint8 ov;
	uint8 ca;
	uint8 byte_count;
public:
	powerpc_xer_register();
	void set(uint32 xer);
	uint32 get() const;
	void set_so(int v)			{ so = v; }
	int get_so() const			{ return so; }
	void set_ov(int v)			{ ov = v; so |= v; }
	int get_ov() const			{ return ov; }
	void set_ca(int v)			{ ca = v; }
	int get_ca() const			{ return ca; }
	void set_count(int v)		{ byte_count = v; }
	int get_count() const		{ return byte_count; }
};

inline
powerpc_xer_register::powerpc_xer_register()
	: so(0), ov(0), ca(0), byte_count(0)
{ }

inline uint32
powerpc_xer_register::get() const
{
	return (so << 31) | (ov << 30) | (ca << 29) | byte_count;
}

inline void
powerpc_xer_register::set(uint32 xer)
{
	so = XER_SO_field::extract(xer);
	ov = XER_OV_field::extract(xer);
	ca = XER_CA_field::extract(xer);
	byte_count = XER_COUNT_field::extract(xer);
}


/**
 *		Special CPU flags
 **/

#include "cpu/spcflags.hpp"
typedef basic_spcflags powerpc_spcflags;


/**
 *		Floating point register
 **/

union powerpc_fpr {
	uint64 j;
	double d;
};


/**
 *		Vector Status and Control Register
 **/

class powerpc_vscr
{
	uint32 vscr;
public:
	powerpc_vscr() : vscr(0)	{ }
	void set(uint32 v)			{ vscr = v; }
	uint32 get() const			{ return vscr; }
	uint32 get_nj() const		{ return vscr & VSCR_NJ_field::mask(); }
	void set_nj(bool v)			{ VSCR_NJ_field::insert(vscr, v); }
	uint32 get_sat() const		{ return vscr & VSCR_SAT_field::mask(); }
	void set_sat(bool v)		{ VSCR_SAT_field::insert(vscr, v); }
};


/**
 *		Vector register
 **/

union powerpc_vr
{
	uint8	b[16];
	uint16	h[8];
	uint32	w[4];
	uint64	j[2];
	float	f[4];
};


/**
 *		User Environment Architecture (UEA) Register Set
 **/

struct powerpc_registers
{
	enum {
		GPR_BASE	= 0,
		FPR_BASE	= 32,
		CR			= 64,
		FPSCR,
		XER,
		LR,  CTR,
		PC,
		SP			= GPR_BASE + 1
	};

	enum {
		SPR_XER		= 1,
		SPR_LR		= 8,
		SPR_CTR		= 9,
		SPR_SDR1	= 25,
		SPR_PVR		= 287,
		SPR_VRSAVE	= 256,
	};

	static inline int GPR(int r) { return GPR_BASE + r; }
	static inline int FPR(int r) { return FPR_BASE + r; }
	static void interrupt_copy(powerpc_registers &oregs, powerpc_registers const &iregs);
	
	uint32 gpr[32];				// General-Purpose Registers
	powerpc_fpr fpr[32];		// Floating-Point Registers
	powerpc_vr vr[32];			// Vector Registers
	powerpc_cr_register cr;		// Condition Register
	powerpc_xer_register xer;	// XER Register (SPR 1)
	powerpc_vscr vscr;			// Vector Status and Control Register
	uint32 vrsave;				// AltiVec Save Register
	uint32 fpscr;				// Floating-Point Status and Control Register
	uint32 lr;					// Link Register (SPR 8)
	uint32 ctr;					// Count Register (SPR 9)
	uint32 pc;					// Program Counter
	powerpc_spcflags spcflags;	// Special CPU flags
#if KPX_MAX_CPUS == 1
	uint32 reserve_valid;
	uint32 reserve_addr;
#else
	static uint32 reserve_valid;
	static uint32 reserve_addr;
	static uint32 reserve_data;
#endif
};

#endif /* PPC_REGISTERS_H */
