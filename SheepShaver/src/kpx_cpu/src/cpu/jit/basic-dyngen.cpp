/*
 *  dyngen-glue.hpp - Glue to QEMU dyngen infrastructure
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

#include "sysdeps.h"
#include "basic-dyngen.hpp"

int __op_param1, __op_param2, __op_param3;
int __op_jmp0, __op_jmp1;

#define DYNGEN_IMPL 1
#define DEFINE_GEN(NAME,RET,ARGS) RET basic_dyngen::NAME ARGS
#include "basic-dyngen-ops.hpp"

basic_dyngen::basic_dyngen(dyngen_cpu_base cpu, int cache_size)
	: parent_cpu(cpu), basic_jit_cache(cache_size)
{
	execute_func = gen_start();
	gen_op_execute();
	gen_end();
	set_code_start(code_ptr());

#if PPC_REENTRANT_JIT
#ifdef SHEEPSHAVER
	extern void init_emul_op_trampolines(basic_dyngen & dg);
	init_emul_op_trampolines(*this);
	set_code_start(code_ptr());
#endif
#endif
}

void
basic_dyngen::gen_invoke(void (*func)())
{
	uintptr funcptr = (uintptr)func;
	if (direct_call_possible(funcptr))
		gen_op_invoke_direct(funcptr);
	else
		gen_op_invoke(funcptr);
}

#define DEFINE_INVOKE(NAME, ARGS, IARGS)		\
void											\
basic_dyngen::gen_invoke_##NAME ARGS			\
{												\
	uintptr funcptr = (uintptr)func;			\
	if (direct_call_possible(funcptr))			\
		gen_op_invoke_direct_##NAME IARGS;		\
	else										\
		gen_op_invoke_##NAME IARGS;				\
}

DEFINE_INVOKE(T0, (void (*func)(uint32)), (funcptr));
DEFINE_INVOKE(T0_T1, (void (*func)(uint32, uint32)), (funcptr));
DEFINE_INVOKE(T0_T1_T2, (void (*func)(uint32, uint32, uint32)), (funcptr));
DEFINE_INVOKE(T0_ret_T0, (uint32 (*func)(uint32)), (funcptr));
DEFINE_INVOKE(im, (void (*func)(uint32), uint32 value), (funcptr, value));
DEFINE_INVOKE(CPU, (void (*func)(dyngen_cpu_base)), (funcptr));
DEFINE_INVOKE(CPU_T0, (void (*func)(dyngen_cpu_base, uint32)), (funcptr));
DEFINE_INVOKE(CPU_im, (void (*func)(dyngen_cpu_base, uint32), uint32 value), (funcptr, value));
DEFINE_INVOKE(CPU_im_im, (void (*func)(dyngen_cpu_base, uint32, uint32), uint32 param1, uint32 param2), (funcptr, param1, param2));
DEFINE_INVOKE(CPU_A0_ret_A0, (void *(*func)(dyngen_cpu_base)), (funcptr));

#undef DEFINE_INVOKE
