/*
 *  ppc-blockinfo.hpp - PowerPC basic block information
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

#ifndef PPC_BLOCKINFO_H
#define PPC_BLOCKINFO_H

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

	decode_info *		di;
};

#endif /* PPC_BLOCKINFO_H */
