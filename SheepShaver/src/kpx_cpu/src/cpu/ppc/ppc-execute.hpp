/*
 *  ppc-execute.hpp - PowerPC semantic action templates
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

#ifndef PPC_EXECUTE_H
#define PPC_EXECUTE_H

// This file is designed to be included from implementation files only.
#ifdef DYNGEN_OPS
#define PPC_CPU							powerpc_dyngen_helper
#define DEFINE_HELPER(NAME, ARGS)		static inline uint32 NAME ARGS
#define RETURN(VAL)						dyngen_barrier(); return (VAL)
#else
#define PPC_CPU							powerpc_cpu
#define DEFINE_HELPER(NAME, ARGS)		inline uint32 powerpc_cpu::NAME ARGS
#define RETURN(VAL)						return (VAL)
#endif


template< bool SB > struct register_value { typedef uint32 type; };
template< > struct register_value< true > { typedef  int32 type; };

/**
 *		Add instruction templates
 **/

template< bool EX, bool CA, bool OE >
DEFINE_HELPER(do_execute_addition, (uint32 RA, uint32 RB))
{
	uint32 RD = RA + RB + (EX ? PPC_CPU::xer().get_ca() : 0);

	const bool _RA = ((int32)RA) < 0;
	const bool _RB = ((int32)RB) < 0;
	const bool _RD = ((int32)RD) < 0;

	if (EX) {
		const bool ca = _RB ^ ((_RB ^ _RA) & (_RA ^ _RD));
		PPC_CPU::xer().set_ca(ca);
	}
	else if (CA) {
		const bool ca = (uint32)RD < (uint32)RA;
		PPC_CPU::xer().set_ca(ca);
	}

	if (OE)
		PPC_CPU::xer().set_ov((_RB ^ _RD) & (_RA ^ _RD));

	RETURN(RD);
}

/**
 *		Subtract instruction templates
 **/

template< bool CA, bool OE >
DEFINE_HELPER(do_execute_subtract, (uint32 RA, uint32 RB))
{
	uint32 RD = RB - RA;

	const bool _RA = ((int32)RA) < 0;
	const bool _RB = ((int32)RB) < 0;
	const bool _RD = ((int32)RD) < 0;

	if (CA)
		PPC_CPU::xer().set_ca((uint32)RD <= (uint32)RB);

	if (OE)
		PPC_CPU::xer().set_ov((_RA ^ _RB) & (_RD ^ _RB));

	RETURN(RD);
}

template< bool OE >
DEFINE_HELPER(do_execute_subtract_extended, (uint32 RA, uint32 RB))
{
	const uint32 RD = ~RA + RB + PPC_CPU::xer().get_ca();

	const bool _RA = ((int32)RA) < 0;
	const bool _RB = ((int32)RB) < 0;
	const bool _RD = ((int32)RD) < 0;

	const bool ca = !_RA ^ ((_RA ^ _RD) & (_RB ^ _RD));
	PPC_CPU::xer().set_ca(ca);

	if (OE)
		PPC_CPU::xer().set_ov((_RA ^ _RB) & (_RD ^ _RB));

	RETURN(RD);
}

/**
 *		Divide instruction templates
 **/

template< bool SB, bool OE >
DEFINE_HELPER(do_execute_divide, (uint32 RA, uint32 RB))
{
	typename register_value<SB>::type a = RA;
	typename register_value<SB>::type b = RB;
	uint32 RD;

	if (b == 0 || (SB && a == 0x80000000 && b == -1)) {
		// Reference manual says result is undefined but it gets all
		// bits set to MSB on a real processor
		RD = SB ? ((int32)RA >> 31) : 0;
		if (OE)
			PPC_CPU::xer().set_ov(1);
	}
	else {
		RD = a / b;
		if (OE)
			PPC_CPU::xer().set_ov(0);
	}

	RETURN(RD);
}

#endif /* PPC_EXECUTE_H */
