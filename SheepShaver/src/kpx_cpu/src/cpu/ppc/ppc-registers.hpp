/*
 *  ppc-registers.hpp - PowerPC registers definition
 *
 *  Kheperix (C) 2003 Gwenole Beauchesne
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
	bool so;
	bool ov;
	bool ca;
	uint32 byte_count;
public:
	powerpc_xer_register();
	void set(uint32 xer);
	uint32 get() const;
	void set_so(bool v)			{ so = v; }
	bool get_so() const			{ return so; }
	void set_ov(bool v)			{ ov = v; if (v) so = true; }
	bool get_ov() const			{ return ov; }
	void set_ca(bool v)			{ ca = v; }
	bool get_ca() const			{ return ca; }
	void set_count(uint32 v)	{ byte_count = v; }
	uint32 get_count() const	{ return byte_count; }
};

inline
powerpc_xer_register::powerpc_xer_register()
	: so(false), ov(false), ca(false), byte_count(0)
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
	
	static inline int GPR(int r) { return GPR_BASE + r; }
	static inline int FPR(int r) { return FPR_BASE + r; }
	
	uint32	gpr[32];			// General-Purpose Registers
	powerpc_fpr fpr[32];		// Floating-Point Registers
	powerpc_cr_register cr;		// Condition Register
	uint32	fpscr;				// Floating-Point Status and Control Register
	powerpc_xer_register xer;	// XER Register (SPR 1)
	uint32	lr;					// Link Register (SPR 8)
	uint32	ctr;				// Count Register (SPR 9)
	uint32	pc;					// Program Counter
	powerpc_spcflags spcflags;	// Special CPU flags
	static uint32 reserve_valid;
	static uint32 reserve_addr;
	static uint32 reserve_data;
};

#endif /* PPC_REGISTERS_H */
