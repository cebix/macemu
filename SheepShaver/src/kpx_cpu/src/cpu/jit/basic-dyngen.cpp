/*
 *  dyngen-glue.hpp - Glue to QEMU dyngen infrastructure
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

#include "sysdeps.h"
#include "basic-dyngen.hpp"

int __op_param1, __op_param2, __op_param3;
int __op_jmp0, __op_jmp1;

#define DYNGEN_IMPL 1
#define DEFINE_GEN(NAME,ARGS) void basic_dyngen::NAME ARGS
#include "basic-dyngen-ops.hpp"

basic_dyngen::basic_dyngen(dyngen_cpu_base cpu, int cache_size)
	: parent_cpu(cpu), basic_jit_cache(cache_size)
{
	execute_func = gen_start();
	gen_op_execute();
	gen_end();
	set_code_start(code_ptr());
}

void
basic_dyngen::gen_invoke(void (*func)())
{
	if (direct_call_possible((uintptr)func))
		gen_op_invoke_direct((uintptr)func);
	else {
		gen_op_mov_ad_A0_im((uintptr)func);
		gen_op_invoke();
	}
}

void
basic_dyngen::gen_invoke_T0(void (*func)(uint32))
{
	if (direct_call_possible((uintptr)func))
		gen_op_invoke_direct_T0((uintptr)func);
	else {
		gen_op_mov_ad_A0_im((uintptr)func);
		gen_op_invoke_T0();
	}
}

void
basic_dyngen::gen_invoke_im(void (*func)(uint32), uint32 value)
{
	if (direct_call_possible((uintptr)func))
		gen_op_invoke_direct_im((uintptr)func, value);
	else {
		gen_op_mov_ad_A0_im((uintptr)func);
		gen_op_invoke_im(value);
	}
}

void
basic_dyngen::gen_invoke_CPU(void (*func)(dyngen_cpu_base))
{
	if (direct_call_possible((uintptr)func))
		gen_op_invoke_direct_CPU((uintptr)func);
	else {
		gen_op_mov_ad_A0_im((uintptr)func);
		gen_op_invoke_CPU();
	}
}

void
basic_dyngen::gen_invoke_CPU_T0(void (*func)(dyngen_cpu_base, uint32))
{
	if (direct_call_possible((uintptr)func))
		gen_op_invoke_direct_CPU_T0((uintptr)func);
	else {
		gen_op_mov_ad_A0_im((uintptr)func);
		gen_op_invoke_CPU_T0();
	}
}

void
basic_dyngen::gen_invoke_CPU_im(void (*func)(dyngen_cpu_base, uint32), uint32 value)
{
	if (direct_call_possible((uintptr)func))
		gen_op_invoke_direct_CPU_im((uintptr)func, value);
	else {
		gen_op_mov_ad_A0_im((uintptr)func);
		gen_op_invoke_CPU_im(value);
	}
}
