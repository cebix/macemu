/*
 *  ppc-blockinfo.hpp - PowerPC basic block information
 *
 *  Kheperix (C) 2003-2004 Gwenole Beauchesne
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

#ifndef PPC_BLOCKINFO_H
#define PPC_BLOCKINFO_H

#include "cpu/jit/jit-config.hpp"
#include "nvmemfun.hpp"
#include "basic-blockinfo.hpp"

class powerpc_cpu;

struct powerpc_block_info
	: public basic_block_info
{
	typedef nv_mem_fun1_t< void, powerpc_cpu, uint32 > execute_fn;

	struct decode_info
	{
		execute_fn		execute;
		uint32			opcode;
	};

#if PPC_DECODE_CACHE
	decode_info *		di;
#endif
#if PPC_ENABLE_JIT
	uint8 *				entry_point;
#if DYNGEN_DIRECT_BLOCK_CHAINING
	uint8 *				jmp_addr[2];	// Address of target native branch offset to patch
	uint32				jmp_pc[2];		// Target jump addresses in emulated address space
#endif
#endif
	uintptr				min_pc, max_pc;

	bool intersect(uintptr start, uintptr end);
};

inline bool
powerpc_block_info::intersect(uintptr start, uintptr end)
{
	return (min_pc >= start && min_pc < end) || (max_pc >= start && max_pc < end);
}

#endif /* PPC_BLOCKINFO_H */
