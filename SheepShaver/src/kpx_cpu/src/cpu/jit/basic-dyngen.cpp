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

basic_dyngen::basic_dyngen(dyngen_cpu_base cpu)
	: parent_cpu(cpu)
{
}

bool
basic_dyngen::initialize(void)
{
	if (!jit_codegen::initialize())
		return false;

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

	return true;
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

uint8 *
basic_dyngen::gen_align(int align)
{
	int nbytes = align - (((uintptr)code_ptr()) % align);
	if (nbytes == 0)
		return code_ptr();

#if defined(__i386__) || defined(__x86_64__)
	/* Source: GNU Binutils 2.12.90.0.15 */
	/* Various efficient no-op patterns for aligning code labels.
	   Note: Don't try to assemble the instructions in the comments.
	   0L and 0w are not legal.  */
	static const uint8 f32_1[] =
		{0x90};									/* nop					*/
	static const uint8 f32_2[] =
		{0x89,0xf6};							/* movl %esi,%esi		*/
	static const uint8 f32_3[] =
		{0x8d,0x76,0x00};						/* leal 0(%esi),%esi	*/
	static const uint8 f32_4[] =
		{0x8d,0x74,0x26,0x00};					/* leal 0(%esi,1),%esi	*/
	static const uint8 f32_5[] =
		{0x90,									/* nop					*/
		 0x8d,0x74,0x26,0x00};					/* leal 0(%esi,1),%esi	*/
	static const uint8 f32_6[] =
		{0x8d,0xb6,0x00,0x00,0x00,0x00};		/* leal 0L(%esi),%esi	*/
	static const uint8 f32_7[] =
		{0x8d,0xb4,0x26,0x00,0x00,0x00,0x00};	/* leal 0L(%esi,1),%esi */
	static const uint8 f32_8[] =
		{0x90,									/* nop					*/
		 0x8d,0xb4,0x26,0x00,0x00,0x00,0x00};	/* leal 0L(%esi,1),%esi */
	static const uint8 f32_9[] =
		{0x89,0xf6,								/* movl %esi,%esi		*/
		 0x8d,0xbc,0x27,0x00,0x00,0x00,0x00};	/* leal 0L(%edi,1),%edi */
	static const uint8 f32_10[] =
		{0x8d,0x76,0x00,						/* leal 0(%esi),%esi	*/
		 0x8d,0xbc,0x27,0x00,0x00,0x00,0x00};	/* leal 0L(%edi,1),%edi */
	static const uint8 f32_11[] =
		{0x8d,0x74,0x26,0x00,					/* leal 0(%esi,1),%esi	*/
		 0x8d,0xbc,0x27,0x00,0x00,0x00,0x00};	/* leal 0L(%edi,1),%edi */
	static const uint8 f32_12[] =
		{0x8d,0xb6,0x00,0x00,0x00,0x00,			/* leal 0L(%esi),%esi	*/
		 0x8d,0xbf,0x00,0x00,0x00,0x00};		/* leal 0L(%edi),%edi	*/
	static const uint8 f32_13[] =
		{0x8d,0xb6,0x00,0x00,0x00,0x00,			/* leal 0L(%esi),%esi	*/
		 0x8d,0xbc,0x27,0x00,0x00,0x00,0x00};	/* leal 0L(%edi,1),%edi */
	static const uint8 f32_14[] =
		{0x8d,0xb4,0x26,0x00,0x00,0x00,0x00,	/* leal 0L(%esi,1),%esi */
		 0x8d,0xbc,0x27,0x00,0x00,0x00,0x00};	/* leal 0L(%edi,1),%edi */
	static const uint8 f32_15[] =
		{0xeb,0x0d,0x90,0x90,0x90,0x90,0x90,	/* jmp .+15; lotsa nops	*/
		 0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90};
	static const uint8 f32_16[] =
		{0xeb,0x0d,0x90,0x90,0x90,0x90,0x90,	/* jmp .+15; lotsa nops	*/
		 0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90};
	static const uint8 *const f32_patt[] = {
		f32_1, f32_2, f32_3, f32_4, f32_5, f32_6, f32_7, f32_8,
		f32_9, f32_10, f32_11, f32_12, f32_13, f32_14, f32_15
	};
	static const uint8 prefixes[4] = { 0x66, 0x66, 0x66, 0x66 };

#if defined(__x86_64__)
	/* The recommended way to pad 64bit code is to use NOPs preceded by
	   maximally four 0x66 prefixes.  Balance the size of nops.  */
	int i;
	int nnops = (nbytes + 3) / 4;
	int len = nbytes / nnops;
	int remains = nbytes - nnops * len;

	for (i = 0; i < remains; i++) {
		emit_block(prefixes, len);
		emit_8(0x90); // NOP
	}
	for (; i < nnops; i++) {
		emit_block(prefixes, len - 1);
		emit_8(0x90); // NOP
	}
#else
	int nloops = nbytes / 16;
	while (nloops-- > 0)
		emit_block(f32_16, sizeof(f32_16));

	nbytes %= 16;
	if (nbytes)
		emit_block(f32_patt[nbytes - 1], nbytes);
#endif
#else
#warning "FIXME: tune for your target"
	for (int i = 0; i < nbytes; i++)
		emit_8(0);
#endif
	return code_ptr();
}
