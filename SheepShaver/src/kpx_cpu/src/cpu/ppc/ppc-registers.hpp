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

#ifdef PPC_HAVE_SPLIT_CR
class powerpc_crf_register
{
	union {
		struct {
			uint8 lt;
			uint8 gt;
			uint8 eq;
			uint8 so;
		} parts;
		uint32 value;
	};
#ifdef PPC_LAZY_CC_UPDATE
	bool lazy_mode;
	int32 cc_dest;
#endif
public:
	powerpc_crf_register()		{ clear(); }
	void set_so(bool v)			{ parts.so = v; }
	bool is_overflow() const	{ return parts.so; }
	bool is_less() const;
	bool is_greater() const;
	bool is_zero() const;
	void clear();
	bool test(int condition) const;
	void set(uint32 v);
	uint32 get() const;
	void compute(int32 v);
};

inline void
powerpc_crf_register::clear()
{
	value = 0;
#ifdef PPC_LAZY_CC_UPDATE
	lazy_mode = false;
	cc_dest = 0;
#endif
}

inline bool
powerpc_crf_register::is_less() const
{
#ifdef PPC_LAZY_CC_UPDATE
	if (lazy_mode)
		return cc_dest < 0;
#endif
	return parts.lt;
}

inline bool
powerpc_crf_register::is_greater() const
{
#ifdef PPC_LAZY_CC_UPDATE
	if (lazy_mode)
		return cc_dest > 0;
#endif
	return parts.gt;
}

inline bool
powerpc_crf_register::is_zero() const
{
#ifdef PPC_LAZY_CC_UPDATE
	if (lazy_mode)
		return cc_dest == 0;
#endif
	return parts.eq;
}

inline bool
powerpc_crf_register::test(int condition) const
{
	switch (condition) {
	case 0: return is_less();
	case 1: return is_greater();
	case 2: return is_zero();
	case 3: return is_overflow();
	}
	abort();
	return false;
}

inline void
powerpc_crf_register::set(uint32 v)
{
	parts.so = standalone_CR_SO_field::extract(v);
#ifdef PPC_LAZY_CC_UPDATE
	const uint32 rd = v & (standalone_CR_LT_field::mask() |
						   standalone_CR_GT_field::mask() |
						   standalone_CR_EQ_field::mask());
	lazy_mode = false;
	if (rd == standalone_CR_LT_field::mask()) {
		lazy_mode = true;
		cc_dest = -1;
		return;
	}
	if (rd == standalone_CR_GT_field::mask()) {
		lazy_mode = true;
		cc_dest = 1;
		return;
	}
	if (rd == standalone_CR_EQ_field::mask()) {
		lazy_mode = true;
		cc_dest = 0;
		return;
	}
#endif
	parts.lt = standalone_CR_LT_field::extract(v);
	parts.gt = standalone_CR_GT_field::extract(v);
	parts.eq = standalone_CR_EQ_field::extract(v);
}

inline uint32
powerpc_crf_register::get() const
{
	uint32 value = parts.so;
#ifdef PPC_LAZY_CC_UPDATE
	if (lazy_mode) {
		if ((int32)cc_dest < 0)
			value |= standalone_CR_LT_field::mask();
		else if ((int32)cc_dest > 0)
			value |= standalone_CR_GT_field::mask();
		else
			value |= standalone_CR_EQ_field::mask();
		return value;
	}
#endif
	return (parts.lt << 3) | (parts.gt << 2) | (parts.eq << 1) | value;
}

inline void
powerpc_crf_register::compute(int32 v)
{
#ifdef PPC_LAZY_CC_UPDATE
	lazy_mode = true;
	cc_dest = v;
#else
	if (v < 0)
		parts.lt = 1, parts.gt = 0, parts.eq = 0;
	else if (v > 0)
		parts.lt = 0, parts.gt = 1, parts.eq = 0;
	else
		parts.lt = 0, parts.gt = 0, parts.eq = 1;
#endif
}
#endif

class powerpc_cr_register
{
#ifdef PPC_HAVE_SPLIT_CR
	powerpc_crf_register crf[8];
#else
	uint32 cr;
#endif
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
#ifdef PPC_HAVE_SPLIT_CR
	crf[crfd].clear();
#else
	cr &= ~(0xf << (28 - 4 * crfd));
#endif
}

inline void
powerpc_cr_register::set(int crfd, uint32 v)
{
#ifdef PPC_HAVE_SPLIT_CR
	crf[crfd].set(v);
#else
	clear(crfd);
	cr |= v << (28 - 4 * crfd);
#endif
}

inline uint32
powerpc_cr_register::get(int crfd) const
{
#ifdef PPC_HAVE_SPLIT_CR
	return crf[crfd].get();
#else
	return (cr >> (28 - 4 * crfd)) & 0xf;
#endif
}

inline void
powerpc_cr_register::set_so(int crfd, bool v)
{
#ifdef PPC_HAVE_SPLIT_CR
	crf[crfd].set_so(v);
#else
	const uint32 m = standalone_CR_SO_field::mask() << (28 - 4 * crfd);
	cr = (cr & ~m) | (v ? m : 0);
#endif
}

inline void
powerpc_cr_register::compute(int crfd, int32 v)
{
#ifdef PPC_HAVE_SPLIT_CR
	crf[crfd].compute(v);
#else
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
#endif
}

inline void
powerpc_cr_register::set(uint32 v)
{
#ifdef PPC_HAVE_SPLIT_CR
	crf[0].set(CR_field<0>::extract(v));
	crf[1].set(CR_field<1>::extract(v));
	crf[2].set(CR_field<2>::extract(v));
	crf[3].set(CR_field<3>::extract(v));
	crf[4].set(CR_field<4>::extract(v));
	crf[5].set(CR_field<5>::extract(v));
	crf[6].set(CR_field<6>::extract(v));
	crf[7].set(CR_field<7>::extract(v));
#else
	cr = v;
#endif
}

inline uint32
powerpc_cr_register::get() const
{
#ifdef PPC_HAVE_SPLIT_CR
	uint32 cr = crf[0].get();
	for (int i = 1; i < 8; i++)
		cr = (cr << 4) | crf[i].get();
#endif
	return cr;
}

inline bool
powerpc_cr_register::test(int condition) const
{
#ifdef PPC_HAVE_SPLIT_CR
	return crf[condition / 4].test(condition % 4);
#else
	return (cr << condition) & 0x80000000;
#endif
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
 *		VEA Register Set
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
		TBL, TBU,
		PC,
		SP			= GPR_BASE + 1
	};
	
	static inline int GPR(int r) { return GPR_BASE + r; }
	static inline int FPR(int r) { return FPR_BASE + r; }
	
	uint32	gpr[32];			// General-Purpose Registers
	double	fpr[32];			// Floating-Point Registers
	powerpc_cr_register cr;		// Condition Register
	uint32	fpscr;				// Floating-Point Status and Control Register
	powerpc_xer_register xer;	// XER Register (SPR 1)
	uint32	lr;					// Link Register (SPR 8)
	uint32	ctr;				// Count Register (SPR 9)
	uint32	pc;					// Program Counter
	uint32	tbl, tbu;			// Time Base
	static uint32 reserve_valid;
	static uint32 reserve_addr;
	static uint32 reserve_data;
};

#endif /* PPC_REGISTERS_H */
