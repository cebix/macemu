/*
 *  compiler/gencomp.c - MC680x0 compilation generator
 *
 *  Based on work Copyright 1995, 1996 Bernd Schmidt
 *  Changes for UAE-JIT Copyright 2000 Bernd Meyer
 *
 *  Adaptation for ARAnyM/ARM, copyright 2001-2014
 *    Milan Jurik, Jens Heitmann
 * 
 *  Adaptation for Basilisk II and improvements, copyright 2000-2005
 *    Gwenole Beauchesne
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
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

#define CC_FOR_BUILD 1
// #include "sysconfig.h"
#define WINUAE_ARANYM

#include "sysdeps.h"
#include "readcpu.h"

#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#undef abort

#ifdef UAE
/*
#define DISABLE_I_OR_AND_EOR
#define DISABLE_I_SUB
#define DISABLE_I_SUBA
#define DISABLE_I_SUBX
#define DISABLE_I_ADD
#define DISABLE_I_ADDA
#define DISABLE_I_ADDX
#define DISABLE_I_NEG
#define DISABLE_I_NEGX
#define DISABLE_I_CLR
#define DISABLE_I_NOT
#define DISABLE_I_TST
#define DISABLE_I_BCHG_BCLR_BSET_BTST
#define DISABLE_I_CMPM_CMP
#define DISABLE_I_CMPA
#define DISABLE_I_MOVE
#define DISABLE_I_MOVEA
#define DISABLE_I_SWAP
#define DISABLE_I_EXG
#define DISABLE_I_EXT
#define DISABLE_I_MVEL
#define DISABLE_I_MVMLE
#define DISABLE_I_RTD
#define DISABLE_I_LINK
#define DISABLE_I_UNLK
#define DISABLE_I_RTS
#define DISABLE_I_JSR
#define DISABLE_I_JMP
#define DISABLE_I_BSR
#define DISABLE_I_BCC
#define DISABLE_I_LEA
#define DISABLE_I_PEA
#define DISABLE_I_DBCC
#define DISABLE_I_SCC
#define DISABLE_I_MULU
#define DISABLE_I_MULS
#define DISABLE_I_ASR
#define DISABLE_I_ASL
#define DISABLE_I_LSR
#define DISABLE_I_LSL
#define DISABLE_I_ROL
#define DISABLE_I_ROR
#define DISABLE_I_MULL
#define DISABLE_I_FPP
#define DISABLE_I_FBCC
#define DISABLE_I_FSCC
#define DISABLE_I_MOVE16
*/

#endif /* UAE */

#ifdef UAE
#define JIT_PATH "jit/"
#ifdef FSUAE
#define GEN_PATH "gen/"
#else
#define GEN_PATH "jit/"
#endif
#define RETURN "return 0;"
#define RETTYPE "uae_u32"
#define NEXT_CPU_LEVEL 5
#else
#define JIT_PATH "compiler/"
#define GEN_PATH ""
#define RETURN "return;"
#define RETTYPE "void"
#define NEXT_CPU_LEVEL 4
#endif

#define BOOL_TYPE		"int"
#define failure			global_failure=1
#define FAILURE			global_failure=1
#define isjump			global_isjump=1
#define is_const_jump	global_iscjump=1
#define isaddx			global_isaddx=1
#define uses_cmov		global_cmov=1
#define mayfail			global_mayfail=1
#define uses_fpu		global_fpu=1

int hack_opcode;

static int global_failure;
static int global_isjump;
static int global_iscjump;
static int global_isaddx;
static int global_cmov;
static int long_opcode;
static int global_mayfail;
static int global_fpu;

static char endstr[1000];
static char lines[100000];
static int comp_index=0;

#include "flags_x86.h"

#ifndef __attribute__
#  ifndef __GNUC__
#    define __attribute__(x)
#  endif
#endif

#define GENA_GETV_NO_FETCH	0
#define GENA_GETV_FETCH		1
#define GENA_GETV_FETCH_ALIGN 2
#define GENA_MOVEM_DO_INC	0
#define GENA_MOVEM_NO_INC	1
#define GENA_MOVEM_MOVE16	2


static int cond_codes[]={-1,-1,
		NATIVE_CC_HI,NATIVE_CC_LS,
		NATIVE_CC_CC,NATIVE_CC_CS,
		NATIVE_CC_NE,NATIVE_CC_EQ,
		-1,-1,
		NATIVE_CC_PL,NATIVE_CC_MI,
		NATIVE_CC_GE,NATIVE_CC_LT,
		NATIVE_CC_GT,NATIVE_CC_LE
		};

__attribute__((format(printf, 1, 2)))
static void comprintf(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	comp_index += vsprintf(lines + comp_index, format, args);
	va_end(args);
}

static void com_discard(void)
{
	comp_index = 0;
}

static void com_flush(void)
{
	int i;
	for (i = 0; i < comp_index; i++)
		putchar(lines[i]);
	com_discard();
}


static FILE *headerfile;
static FILE *stblfile;

static int using_prefetch;
static int using_exception_3;
static int cpu_level;
static int noflags;

/* For the current opcode, the next lower level that will have different code.
 * Initialized to -1 for each opcode. If it remains unchanged, indicates we
 * are done with that opcode.  */
static int next_cpu_level;

static int *opcode_map;
static int *opcode_next_clev;
static int *opcode_last_postfix;
static unsigned long *counts;

static void read_counts(void)
{
    FILE *file;
    unsigned long opcode, count, total;
    char name[20];
    int nr = 0;
    memset (counts, 0, 65536 * sizeof *counts);

    file = fopen ("frequent.68k", "r");
    if (file)
    {
	if (fscanf (file, "Total: %lu\n", &total) != 1) {
		assert(0);
	}
	while (fscanf (file, "%lx: %lu %s\n", &opcode, &count, name) == 3)
	{
	    opcode_next_clev[nr] = NEXT_CPU_LEVEL;
	    opcode_last_postfix[nr] = -1;
	    opcode_map[nr++] = opcode;
	    counts[opcode] = count;
	}
	fclose (file);
    }
    if (nr == nr_cpuop_funcs)
	return;
    for (opcode = 0; opcode < 0x10000; opcode++)
    {
	if (table68k[opcode].handler == -1 && table68k[opcode].mnemo != i_ILLG
	    && counts[opcode] == 0)
	{
	    opcode_next_clev[nr] = NEXT_CPU_LEVEL;
	    opcode_last_postfix[nr] = -1;
	    opcode_map[nr++] = opcode;
	    counts[opcode] = count;
	}
    }
    assert (nr == nr_cpuop_funcs);
}

static int n_braces = 0;
static int insn_n_cycles;

static void
start_brace (void)
{
    n_braces++;
    comprintf ("{");
}

static void
close_brace (void)
{
    assert (n_braces > 0);
    n_braces--;
    comprintf ("}");
}

static void
finish_braces (void)
{
    while (n_braces > 0)
	close_brace ();
    comprintf ("\n");
}

static inline void gen_update_next_handler(void)
{
    return; /* Can anything clever be done here? */
}

static void gen_writebyte(const char *address, const char *source)
{
	comprintf("\twritebyte(%s, %s, scratchie);\n", address, source);
}

static void gen_writeword(const char *address, const char *source)
{
	comprintf("\twriteword(%s, %s, scratchie);\n", address, source);
}

static void gen_writelong(const char *address, const char *source)
{
	comprintf("\twritelong(%s, %s, scratchie);\n", address, source);
}

static void gen_readbyte(const char *address, const char* dest)
{
	comprintf("\treadbyte(%s, %s, scratchie);\n", address, dest);
}

static void gen_readword(const char *address, const char *dest)
{
	comprintf("\treadword(%s,%s,scratchie);\n", address, dest);
}

static void gen_readlong(const char *address, const char *dest)
{
	comprintf("\treadlong(%s, %s, scratchie);\n", address, dest);
}



static const char *
gen_nextilong (void)
{
    static char buffer[80];

    sprintf (buffer, "comp_get_ilong((m68k_pc_offset+=4)-4)");
    insn_n_cycles += 4;

    long_opcode=1;
    return buffer;
}

static const char *
gen_nextiword (void)
{
    static char buffer[80];

    sprintf (buffer, "comp_get_iword((m68k_pc_offset+=2)-2)");
    insn_n_cycles+=2;

    long_opcode=1;
    return buffer;
}

static const char *
gen_nextibyte (void)
{
    static char buffer[80];

    sprintf (buffer, "comp_get_ibyte((m68k_pc_offset+=2)-2)");
    insn_n_cycles += 2;

    long_opcode=1;
    return buffer;
}


static void
swap_opcode (void)
{
#ifdef UAE
	/* no-op */
#else
	comprintf("#ifdef USE_JIT_FPU\n");
	comprintf("#if defined(HAVE_GET_WORD_UNSWAPPED) && !defined(FULLMMU)\n");
	comprintf("\topcode = do_byteswap_16(opcode);\n");
	comprintf("#endif\n");
	comprintf("#endif\n");
#endif
}

static void
sync_m68k_pc (void)
{
	comprintf("    if (m68k_pc_offset > SYNC_PC_OFFSET)\n        sync_m68k_pc();\n");
}


static void gen_set_fault_pc(void)
{
	start_brace();
	comprintf("\tsync_m68k_pc();\n");
	comprintf("\tuae_u32 retadd=start_pc+((char *)comp_pc_p-(char *)start_pc_p)+m68k_pc_offset;\n");
	comprintf("\tint ret=scratchie++;\n"
		  "\tmov_l_ri(ret,retadd);\n"
		  "\tmov_l_mr((uintptr)&regs.fault_pc,ret);\n");
}


static void make_sr(void)
{
	start_brace();
	comprintf("\tint sr = scratchie++;\n");
	comprintf("\tint tmp = scratchie++;\n");
	comprintf("\tcompemu_make_sr(sr, tmp);\n");
}


static void disasm_this_inst(void)
{
	comprintf("\tdisasm_this_inst = true;\n");
}


/* getv == 1: fetch data; getv != 0: check for odd address. If movem != 0,
 * the calling routine handles Apdi and Aipi modes.
 * gb-- movem == 2 means the same thing but for a MOVE16 instruction */
static void genamode(amodes mode, const char *reg, wordsizes size, const char *name, int getv, int movem)
{
	start_brace();
	switch (mode)
	{
	case Dreg: /* Do we need to check dodgy here? */
		assert (movem == GENA_MOVEM_DO_INC);
		if (getv == GENA_GETV_FETCH || getv == GENA_GETV_FETCH_ALIGN)
		{
			/* We generate the variable even for getv==2, so we can use
			 it as a destination for MOVE */
			comprintf("\tint %s = %s;\n", name, reg);
		}
		return;

	case Areg:
		assert (movem == GENA_MOVEM_DO_INC);
		if (getv == GENA_GETV_FETCH || getv == GENA_GETV_FETCH_ALIGN)
		{
			/* see above */
			comprintf("\tint %s = dodgy ? scratchie++ : %s + 8;\n", name, reg);
			if (getv == GENA_GETV_FETCH)
			{
				comprintf("\tif (dodgy) \n");
				comprintf("\t\tmov_l_rr(%s, %s + 8);\n", name, reg);
			}
		}
		return;

	case Aind:
		comprintf("\tint %sa = dodgy ? scratchie++ : %s + 8;\n", name, reg);
		comprintf("\tif (dodgy)\n");
		comprintf("\t\tmov_l_rr(%sa, %s + 8);\n", name, reg);
		break;
	case Aipi:
		comprintf("\tint %sa = scratchie++;\n", name);
		comprintf("\tmov_l_rr(%sa, %s + 8);\n", name, reg);
		break;
	case Apdi:
		switch (size)
		{
		case sz_byte:
			if (movem != GENA_MOVEM_DO_INC)
			{
				comprintf("\tint %sa = dodgy ? scratchie++ : %s + 8;\n", name, reg);
				comprintf("\tif (dodgy)\n");
				comprintf("\t\tmov_l_rr(%sa, 8 + %s);\n", name, reg);
			} else
			{
				start_brace();
				comprintf("\tint %sa = dodgy ? scratchie++ : %s + 8;\n", name, reg);
				comprintf("\tlea_l_brr(%s + 8, %s + 8, (uae_s32)-areg_byteinc[%s]);\n", reg, reg, reg);
				comprintf("\tif (dodgy)\n");
				comprintf("\t\tmov_l_rr(%sa, 8 + %s);\n", name, reg);
			}
			break;
		case sz_word:
			if (movem != GENA_MOVEM_DO_INC)
			{
				comprintf("\tint %sa=dodgy?scratchie++:%s+8;\n", name, reg);
				comprintf("\tif (dodgy) \n");
				comprintf("\tmov_l_rr(%sa,8+%s);\n", name, reg);
			} else
			{
				start_brace();
				comprintf("\tint %sa = dodgy ? scratchie++ : %s + 8;\n", name, reg);
				comprintf("\tlea_l_brr(%s + 8, %s + 8, -2);\n", reg, reg);
				comprintf("\tif (dodgy)\n");
				comprintf("\t\tmov_l_rr(%sa, 8 + %s);\n", name, reg);
			}
			break;
		case sz_long:
			if (movem != GENA_MOVEM_DO_INC)
			{
				comprintf("\tint %sa = dodgy ? scratchie++ : %s + 8;\n", name, reg);
				comprintf("\tif (dodgy)\n");
				comprintf("\t\tmov_l_rr(%sa, 8 + %s);\n", name, reg);
			} else
			{
				start_brace();
				comprintf("\tint %sa = dodgy ? scratchie++ : %s + 8;\n", name, reg);
				comprintf("\tlea_l_brr(%s + 8, %s + 8, -4);\n", reg, reg);
				comprintf("\tif (dodgy)\n");
				comprintf("\t\tmov_l_rr(%sa, 8 + %s);\n", name, reg);
			}
			break;
		default:
			assert(0);
			break;
		}
		break;
	case Ad16:
		comprintf("\tint %sa = scratchie++;\n", name);
		comprintf("\tmov_l_rr(%sa, 8 + %s);\n", name, reg);
		comprintf("\tlea_l_brr(%sa, %sa, (uae_s32)(uae_s16)%s);\n", name, name, gen_nextiword());
		break;
	case Ad8r:
		comprintf("\tint %sa = scratchie++;\n", name);
		comprintf("\tcalc_disp_ea_020(%s + 8, %s, %sa, scratchie);\n", reg, gen_nextiword(), name);
		break;

	case PC16:
		comprintf("\tint %sa = scratchie++;\n", name);
		comprintf("\tuae_u32 address = start_pc + ((char *)comp_pc_p - (char *)start_pc_p) + m68k_pc_offset;\n");
		comprintf("\tuae_s32 PC16off = (uae_s32)(uae_s16)%s;\n", gen_nextiword());
		comprintf("\tmov_l_ri(%sa, address + PC16off);\n", name);
		break;

	case PC8r:
		comprintf("\tint pctmp = scratchie++;\n");
		comprintf("\tint %sa = scratchie++;\n", name);
		comprintf("\tuae_u32 address = start_pc + ((char *)comp_pc_p - (char *)start_pc_p) + m68k_pc_offset;\n");
		start_brace();
		comprintf("\tmov_l_ri(pctmp,address);\n");

		comprintf("\tcalc_disp_ea_020(pctmp, %s, %sa, scratchie);\n", gen_nextiword(), name);
		break;
	case absw:
		comprintf("\tint %sa = scratchie++;\n", name);
		comprintf("\tmov_l_ri(%sa, (uae_s32)(uae_s16)%s);\n", name, gen_nextiword());
		break;
	case absl:
		comprintf("\tint %sa = scratchie++;\n", name);
		comprintf("\tmov_l_ri(%sa, %s); /* absl */\n", name, gen_nextilong());
		break;
	case imm:
		assert (getv == GENA_GETV_FETCH);
		switch (size)
		{
		case sz_byte:
			comprintf("\tint %s = scratchie++;\n", name);
			comprintf("\tmov_l_ri(%s, (uae_s32)(uae_s8)%s);\n", name, gen_nextibyte());
			break;
		case sz_word:
			comprintf("\tint %s = scratchie++;\n", name);
			comprintf("\tmov_l_ri(%s, (uae_s32)(uae_s16)%s);\n", name, gen_nextiword());
			break;
		case sz_long:
			comprintf("\tint %s = scratchie++;\n", name);
			comprintf("\tmov_l_ri(%s, %s);\n", name, gen_nextilong());
			break;
		default:
			assert(0);
			break;
		}
		return;
	case imm0:
		assert (getv == GENA_GETV_FETCH);
		comprintf("\tint %s = scratchie++;\n", name);
		comprintf("\tmov_l_ri(%s, (uae_s32)(uae_s8)%s);\n", name, gen_nextibyte());
		return;
	case imm1:
		assert (getv == GENA_GETV_FETCH);
		comprintf("\tint %s = scratchie++;\n", name);
		comprintf("\tmov_l_ri(%s, (uae_s32)(uae_s16)%s);\n", name, gen_nextiword());
		return;
	case imm2:
		assert (getv == GENA_GETV_FETCH);
		comprintf("\tint %s = scratchie++;\n", name);
		comprintf("\tmov_l_ri(%s, %s);\n", name, gen_nextilong());
		return;
	case immi:
		assert (getv == GENA_GETV_FETCH);
		comprintf("\tint %s = scratchie++;\n", name);
		comprintf("\tmov_l_ri(%s, %s);\n", name, reg);
		return;
	default:
		assert(0);
		break;
	}

	/* We get here for all non-reg non-immediate addressing modes to
	 * actually fetch the value. */
	if (getv == GENA_GETV_FETCH)
	{
		char astring[80];
		sprintf(astring, "%sa", name);
		switch (size)
		{
		case sz_byte:
			insn_n_cycles += 2;
			break;
		case sz_word:
			insn_n_cycles += 2;
			break;
		case sz_long:
			insn_n_cycles += 4;
			break;
		default:
			assert(0);
			break;
		}
		start_brace();
		comprintf("\tint %s = scratchie++;\n", name);
		switch (size)
		{
		case sz_byte:
			gen_readbyte(astring, name);
			break;
		case sz_word:
			gen_readword(astring, name);
			break;
		case sz_long:
			gen_readlong(astring, name);
			break;
		default:
			assert(0);
			break;
		}
	}

	/* We now might have to fix up the register for pre-dec or post-inc
	 * addressing modes. */
	if (movem == GENA_MOVEM_DO_INC)
	{
		switch (mode)
		{
		case Aipi:
			switch (size)
			{
			case sz_byte:
				comprintf("\tlea_l_brr(%s + 8,%s + 8, areg_byteinc[%s]);\n", reg, reg, reg);
				break;
			case sz_word:
				comprintf("\tlea_l_brr(%s + 8, %s + 8, 2);\n", reg, reg);
				break;
			case sz_long:
				comprintf("\tlea_l_brr(%s + 8, %s + 8, 4);\n", reg, reg);
				break;
			default:
				assert(0);
				break;
			}
			break;
		case Apdi:
			break;
		default:
			break;
		}
	}
}

static void genastore(const char *from, amodes mode, const char *reg, wordsizes size, const char *to)
{
	switch (mode)
	{
	case Dreg:
		switch (size)
		{
		case sz_byte:
			comprintf("\tif(%s != %s)\n", reg, from);
			comprintf("\t\tmov_b_rr(%s, %s);\n", reg, from);
			break;
		case sz_word:
			comprintf("\tif(%s != %s)\n", reg, from);
			comprintf("\t\tmov_w_rr(%s, %s);\n", reg, from);
			break;
		case sz_long:
			comprintf("\tif(%s != %s)\n", reg, from);
			comprintf("\t\tmov_l_rr(%s, %s);\n", reg, from);
			break;
		default:
			assert(0);
			break;
		}
		break;
	case Areg:
		switch (size)
		{
		case sz_word:
			comprintf("\tif(%s + 8 != %s)\n", reg, from);
			comprintf("\t\tmov_w_rr(%s + 8, %s);\n", reg, from);
			break;
		case sz_long:
			comprintf("\tif(%s + 8 != %s)\n", reg, from);
			comprintf("\t\tmov_l_rr(%s + 8, %s);\n", reg, from);
			break;
		default:
			assert(0);
			break;
		}
		break;

	case Apdi:
	case absw:
	case PC16:
	case PC8r:
	case Ad16:
	case Ad8r:
	case Aipi:
	case Aind:
	case absl:
	{
		char astring[80];
		sprintf(astring, "%sa", to);

		switch (size)
		{
		case sz_byte:
			insn_n_cycles += 2;
			gen_writebyte(astring, from);
			break;
		case sz_word:
			insn_n_cycles += 2;
			gen_writeword(astring, from);
			break;
		case sz_long:
			insn_n_cycles += 4;
			gen_writelong(astring, from);
			break;
		default:
			assert(0);
			break;
		}
	}
		break;
	case imm:
	case imm0:
	case imm1:
	case imm2:
	case immi:
		assert(0);
		break;
	default:
		assert(0);
		break;
	}
}

static void genmov16(uae_u32 opcode, struct instr *curi)
{
	comprintf("\tint src=scratchie++;\n");
	comprintf("\tint dst=scratchie++;\n");

	if ((opcode & 0xfff8) == 0xf620) {
		/* MOVE16 (Ax)+,(Ay)+ */
		comprintf("\tuae_u16 dstreg=((%s)>>12)&0x07;\n", gen_nextiword());
		comprintf("\tmov_l_rr(src,8+srcreg);\n");
		comprintf("\tmov_l_rr(dst,8+dstreg);\n");
	}
	else {
		/* Other variants */
		genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_NO_FETCH, GENA_MOVEM_MOVE16);
		genamode (curi->dmode, "dstreg", curi->size, "dst", GENA_GETV_NO_FETCH, GENA_MOVEM_MOVE16);
		comprintf("\tmov_l_rr(src,srca);\n");
		comprintf("\tmov_l_rr(dst,dsta);\n");
	}

	/* Align on 16-byte boundaries */
	comprintf("\tand_l_ri(src,~15);\n");
	comprintf("\tand_l_ri(dst,~15);\n");

	if ((opcode & 0xfff8) == 0xf620) {
		comprintf("\tif (srcreg != dstreg)\n");
		comprintf("\tadd_l_ri(srcreg+8,16);\n");
		comprintf("\tadd_l_ri(dstreg+8,16);\n");
	}
	else if ((opcode & 0xfff8) == 0xf600)
		comprintf("\tadd_l_ri(srcreg+8,16);\n");
	else if ((opcode & 0xfff8) == 0xf608)
		comprintf("\tadd_l_ri(dstreg+8,16);\n");

#ifdef UAE
	comprintf("\tif (special_mem) {\n");
	comprintf("\t\tint tmp=scratchie;\n");
	comprintf("\tscratchie+=4;\n"
		  "\treadlong(src,tmp,scratchie);\n"
		  "\twritelong_clobber(dst,tmp,scratchie);\n"
		  "\tadd_l_ri(src,4);\n"
		  "\tadd_l_ri(dst,4);\n"
		  "\treadlong(src,tmp,scratchie);\n"
		  "\twritelong_clobber(dst,tmp,scratchie);\n"
		  "\tadd_l_ri(src,4);\n"
		  "\tadd_l_ri(dst,4);\n"
		  "\treadlong(src,tmp,scratchie);\n"
		  "\twritelong_clobber(dst,tmp,scratchie);\n"
		  "\tadd_l_ri(src,4);\n"
		  "\tadd_l_ri(dst,4);\n"
		  "\treadlong(src,tmp,scratchie);\n"
		  "\twritelong_clobber(dst,tmp,scratchie);\n");
	comprintf("\t} else\n");
#endif
	start_brace();
	comprintf("\tint tmp=scratchie;\n");
	comprintf("\tscratchie+=4;\n"
		"\tget_n_addr(src,src,scratchie);\n"
		"\tget_n_addr(dst,dst,scratchie);\n"
		"\tmov_l_rR(tmp+0,src,0);\n"
		"\tmov_l_rR(tmp+1,src,4);\n"
		"\tmov_l_rR(tmp+2,src,8);\n"
		"\tmov_l_rR(tmp+3,src,12);\n"
		"\tmov_l_Rr(dst,tmp+0,0);\n"
		"\tforget_about(tmp+0);\n"
		"\tmov_l_Rr(dst,tmp+1,4);\n"
		"\tforget_about(tmp+1);\n"
		"\tmov_l_Rr(dst,tmp+2,8);\n"
		"\tforget_about(tmp+2);\n"
		"\tmov_l_Rr(dst,tmp+3,12);\n");
	close_brace();
}

static void
genmovemel (uae_u16 opcode)
{
    comprintf ("\tuae_u16 mask = %s;\n", gen_nextiword ());
    comprintf ("\tint native=scratchie++;\n");
    comprintf ("\tint i;\n");
    comprintf ("\tsigned char offset=0;\n");
    genamode (table68k[opcode].dmode, "dstreg", table68k[opcode].size, "src", GENA_GETV_FETCH_ALIGN, GENA_MOVEM_NO_INC);
#ifdef UAE
	if (table68k[opcode].size == sz_long)
	    comprintf("\tif (1 && !special_mem) {\n");
	else
	    comprintf("\tif (1 && !special_mem) {\n");
#endif

    /* Fast but unsafe...  */
    comprintf("\tget_n_addr(srca,native,scratchie);\n");

    comprintf("\tfor (i=0;i<16;i++) {\n"
	      "\t\tif ((mask>>i)&1) {\n");
    switch(table68k[opcode].size) {
     case sz_long:
	comprintf("\t\t\tmov_l_rR(i,native,offset);\n"
		  "\t\t\tmid_bswap_32(i);\n"
		  "\t\t\toffset+=4;\n");
	break;
     case sz_word:
	comprintf("\t\t\tmov_w_rR(i,native,offset);\n"
		  "\t\t\tmid_bswap_16(i);\n"
		  "\t\t\tsign_extend_16_rr(i,i);\n"
		  "\t\t\toffset+=2;\n");
	break;
     default: assert(0);
    }
    comprintf("\t\t}\n"
	      "\t}");
    if (table68k[opcode].dmode == Aipi) {
	comprintf("\t\t\tlea_l_brr(8+dstreg,srca,offset);\n");
    }
    /* End fast but unsafe.   */

#ifdef UAE
    comprintf("\t} else {\n");

    comprintf ("\t\tint tmp=scratchie++;\n");

    comprintf("\t\tmov_l_rr(tmp,srca);\n");
    comprintf("\t\tfor (i=0;i<16;i++) {\n"
	      "\t\t\tif ((mask>>i)&1) {\n");
    switch(table68k[opcode].size) {
    case sz_long:
	comprintf("\t\t\t\treadlong(tmp,i,scratchie);\n"
		  "\t\t\t\tadd_l_ri(tmp,4);\n");
	break;
    case sz_word:
	comprintf("\t\t\t\treadword(tmp,i,scratchie);\n"
		  "\t\t\t\tadd_l_ri(tmp,2);\n");
	break;
    default: assert(0);
    }

    comprintf("\t\t\t}\n"
	      "\t\t}\n");
    if (table68k[opcode].dmode == Aipi) {
	comprintf("\t\tmov_l_rr(8+dstreg,tmp);\n");
    }
    comprintf("\t}\n");
#endif

}


static void
genmovemle (uae_u16 opcode)
{
    comprintf ("\tuae_u16 mask = %s;\n", gen_nextiword ());
    comprintf ("\tint native=scratchie++;\n");
    comprintf ("\tint i;\n");
    comprintf ("\tint tmp=scratchie++;\n");
    comprintf ("\tsigned char offset=0;\n");
    genamode (table68k[opcode].dmode, "dstreg", table68k[opcode].size, "src", GENA_GETV_FETCH_ALIGN, GENA_MOVEM_NO_INC);

#ifdef UAE
    /* *Sigh* Some clever geek realized that the fastest way to copy a
       buffer from main memory to the gfx card is by using movmle. Good
       on her, but unfortunately, gfx mem isn't "real" mem, and thus that
       act of cleverness means that movmle must pay attention to special_mem,
       or Genetic Species is a rather boring-looking game ;-) */
	if (table68k[opcode].size == sz_long)
	    comprintf("\tif (1 && !special_mem) {\n");
	else
	    comprintf("\tif (1 && !special_mem) {\n");
#endif
    comprintf("\tget_n_addr(srca,native,scratchie);\n");

    if (table68k[opcode].dmode!=Apdi) {
	comprintf("\tfor (i=0;i<16;i++) {\n"
		  "\t\tif ((mask>>i)&1) {\n");
	switch(table68k[opcode].size) {
	 case sz_long:
	    comprintf("\t\t\tmov_l_rr(tmp,i);\n"
		      "\t\t\tmid_bswap_32(tmp);\n"
		      "\t\t\tmov_l_Rr(native,tmp,offset);\n"
		      "\t\t\toffset+=4;\n");
	    break;
	 case sz_word:
	    comprintf("\t\t\tmov_l_rr(tmp,i);\n"
		      "\t\t\tmid_bswap_16(tmp);\n"
		      "\t\t\tmov_w_Rr(native,tmp,offset);\n"
		      "\t\t\toffset+=2;\n");
	    break;
	 default: assert(0);
	}
    }
    else {  /* Pre-decrement */
	comprintf("\tfor (i=0;i<16;i++) {\n"
		  "\t\tif ((mask>>i)&1) {\n");
	switch(table68k[opcode].size) {
	 case sz_long:
	    comprintf("\t\t\toffset-=4;\n"
		      "\t\t\tmov_l_rr(tmp,15-i);\n"
		      "\t\t\tmid_bswap_32(tmp);\n"
		      "\t\t\tmov_l_Rr(native,tmp,offset);\n"
		      );
	    break;
	 case sz_word:
	    comprintf("\t\t\toffset-=2;\n"
		      "\t\t\tmov_l_rr(tmp,15-i);\n"
		      "\t\t\tmid_bswap_16(tmp);\n"
		      "\t\t\tmov_w_Rr(native,tmp,offset);\n"
		      );
	    break;
	 default: assert(0);
	}
    }


    comprintf("\t\t}\n"
	      "\t}");
    if (table68k[opcode].dmode == Apdi) {
	comprintf("\t\t\tlea_l_brr(8+dstreg,srca,(uae_s32)offset);\n");
    }
#ifdef UAE
    comprintf("\t} else {\n");

    if (table68k[opcode].dmode!=Apdi) {
	comprintf("\tmov_l_rr(tmp,srca);\n");
	comprintf("\tfor (i=0;i<16;i++) {\n"
		  "\t\tif ((mask>>i)&1) {\n");
	switch(table68k[opcode].size) {
	 case sz_long:
	    comprintf("\t\t\twritelong(tmp,i,scratchie);\n"
		      "\t\t\tadd_l_ri(tmp,4);\n");
	    break;
	 case sz_word:
	    comprintf("\t\t\twriteword(tmp,i,scratchie);\n"
		      "\t\t\tadd_l_ri(tmp,2);\n");
	    break;
	 default: assert(0);
	}
    }
    else {  /* Pre-decrement */
	comprintf("\tfor (i=0;i<16;i++) {\n"
		  "\t\tif ((mask>>i)&1) {\n");
	switch(table68k[opcode].size) {
	 case sz_long:
	    comprintf("\t\t\tsub_l_ri(srca,4);\n"
		      "\t\t\twritelong(srca,15-i,scratchie);\n");
	    break;
	 case sz_word:
	    comprintf("\t\t\tsub_l_ri(srca,2);\n"
		      "\t\t\twriteword(srca,15-i,scratchie);\n");
	    break;
	 default: assert(0);
	}
    }


    comprintf("\t\t}\n"
	      "\t}");
    if (table68k[opcode].dmode == Apdi) {
	comprintf("\t\t\tmov_l_rr(8+dstreg,srca);\n");
    }
    comprintf("\t}\n");
#endif
}


static void
duplicate_carry (void)
{
    comprintf ("\tif (needed_flags&FLAG_X) duplicate_carry();\n");
}

typedef enum
{
    flag_logical_noclobber, flag_logical, flag_add, flag_sub, flag_cmp,
    flag_addx, flag_subx, flag_zn, flag_av, flag_sv, flag_and, flag_or,
    flag_eor, flag_mov
}
flagtypes;


static void
genflags (flagtypes type, wordsizes size, const char *value, const char *src, const char *dst)
{
    if (noflags) {
	switch(type) {
	 case flag_cmp:
	    comprintf("\tdont_care_flags();\n");
	    comprintf("/* Weird --- CMP with noflags ;-) */\n");
	    return;
	 case flag_add:
	 case flag_sub:
	    comprintf("\tdont_care_flags();\n");
	    {
		const char* op;
		switch(type) {
		 case flag_add: op="add"; break;
		 case flag_sub: op="sub"; break;
		 default: assert(0);
		}
		switch (size)
		{
		 case sz_byte:
		    comprintf("\t%s_b(%s,%s);\n",op,dst,src);
		    break;
		 case sz_word:
		    comprintf("\t%s_w(%s,%s);\n",op,dst,src);
		    break;
		 case sz_long:
		    comprintf("\t%s_l(%s,%s);\n",op,dst,src);
		    break;
		}
		return;
	    }
	    break;

	 case flag_and:
	    comprintf("\tdont_care_flags();\n");
	    switch (size)
	    {
	     case sz_byte:
		comprintf("if (kill_rodent(dst)) {\n");
		comprintf("\tzero_extend_8_rr(scratchie,%s);\n",src);
		comprintf("\tor_l_ri(scratchie,0xffffff00);\n");
		comprintf("\tand_l(%s,scratchie);\n",dst);
		comprintf("\tforget_about(scratchie);\n");
		comprintf("\t} else \n"
			  "\tand_b(%s,%s);\n",dst,src);
		break;
	     case sz_word:
		comprintf("if (kill_rodent(dst)) {\n");
		comprintf("\tzero_extend_16_rr(scratchie,%s);\n",src);
		comprintf("\tor_l_ri(scratchie,0xffff0000);\n");
		comprintf("\tand_l(%s,scratchie);\n",dst);
		comprintf("\tforget_about(scratchie);\n");
		comprintf("\t} else \n"
			  "\tand_w(%s,%s);\n",dst,src);
		break;
	     case sz_long:
		comprintf("\tand_l(%s,%s);\n",dst,src);
		break;
	    }
	    return;

	 case flag_mov:
	    comprintf("\tdont_care_flags();\n");
	    switch (size)
	    {
	     case sz_byte:
		comprintf("if (kill_rodent(dst)) {\n");
		comprintf("\tzero_extend_8_rr(scratchie,%s);\n",src);
		comprintf("\tand_l_ri(%s,0xffffff00);\n",dst);
		comprintf("\tor_l(%s,scratchie);\n",dst);
		comprintf("\tforget_about(scratchie);\n");
		comprintf("\t} else \n"
			  "\tmov_b_rr(%s,%s);\n",dst,src);
		break;
	     case sz_word:
		comprintf("if (kill_rodent(dst)) {\n");
		comprintf("\tzero_extend_16_rr(scratchie,%s);\n",src);
		comprintf("\tand_l_ri(%s,0xffff0000);\n",dst);
		comprintf("\tor_l(%s,scratchie);\n",dst);
		comprintf("\tforget_about(scratchie);\n");
		comprintf("\t} else \n"
			  "\tmov_w_rr(%s,%s);\n",dst,src);
		break;
	     case sz_long:
		comprintf("\tmov_l_rr(%s,%s);\n",dst,src);
		break;
	    }
	    return;

	 case flag_or:
	 case flag_eor:
	    comprintf("\tdont_care_flags();\n");
	    start_brace();
	    {
		const char* op;
		switch(type) {
		 case flag_or:  op="or"; break;
		 case flag_eor: op="xor"; break;
		 default: assert(0);
		}
		switch (size)
		{
		 case sz_byte:
		    comprintf("if (kill_rodent(dst)) {\n");
		    comprintf("\tzero_extend_8_rr(scratchie,%s);\n",src);
		    comprintf("\t%s_l(%s,scratchie);\n",op,dst);
		    comprintf("\tforget_about(scratchie);\n");
		    comprintf("\t} else \n"
			      "\t%s_b(%s,%s);\n",op,dst,src);
		    break;
		 case sz_word:
		    comprintf("if (kill_rodent(dst)) {\n");
		    comprintf("\tzero_extend_16_rr(scratchie,%s);\n",src);
		    comprintf("\t%s_l(%s,scratchie);\n",op,dst);
		    comprintf("\tforget_about(scratchie);\n");
		    comprintf("\t} else \n"
			      "\t%s_w(%s,%s);\n",op,dst,src);
		    break;
		 case sz_long:
		    comprintf("\t%s_l(%s,%s);\n",op,dst,src);
		    break;
		}
		close_brace();
		return;
	    }


	 case flag_addx:
	 case flag_subx:
	    comprintf("\tdont_care_flags();\n");
	    {
		const char* op;
		switch(type) {
		 case flag_addx: op="adc"; break;
		 case flag_subx: op="sbb"; break;
		 default: assert(0);
		}
		comprintf("\trestore_carry();\n"); /* Reload the X flag into C */
		switch (size)
		{
		 case sz_byte:
		    comprintf("\t%s_b(%s,%s);\n",op,dst,src);
		    break;
		 case sz_word:
		    comprintf("\t%s_w(%s,%s);\n",op,dst,src);
		    break;
		 case sz_long:
		    comprintf("\t%s_l(%s,%s);\n",op,dst,src);
		    break;
		}
		return;
	    }
	    break;
	 default: return;
	}
    }

    /* Need the flags, but possibly not all of them */
    switch (type)
    {
     case flag_logical_noclobber:
	failure;
	/* fall through */

     case flag_and:
     case flag_or:
     case flag_eor:
	comprintf("\tdont_care_flags();\n");
	start_brace();
	{
	    const char* op;
	    switch(type) {
	     case flag_and: op="and"; break;
	     case flag_or:  op="or"; break;
	     case flag_eor: op="xor"; break;
	     default: assert(0);
	    }
	    switch (size)
	    {
	     case sz_byte:
		comprintf("\tstart_needflags();\n"
			  "\t%s_b(%s,%s);\n",op,dst,src);
		break;
	     case sz_word:
		comprintf("\tstart_needflags();\n"
			  "\t%s_w(%s,%s);\n",op,dst,src);
		break;
	     case sz_long:
		comprintf("\tstart_needflags();\n"
			  "\t%s_l(%s,%s);\n",op,dst,src);
		break;
	    }
	    comprintf("\tlive_flags();\n");
	    comprintf("\tend_needflags();\n");
	    close_brace();
	    return;
	}

     case flag_mov:
	comprintf("\tdont_care_flags();\n");
	start_brace();
	{
	    switch (size)
	    {
	     case sz_byte:
		comprintf("\tif (%s!=%s) {\n",src,dst);
		comprintf("\tmov_b_ri(%s,0);\n"
			  "\tstart_needflags();\n",dst);
		comprintf("\tor_b(%s,%s);\n",dst,src);
		comprintf("\t} else {\n");
		comprintf("\tmov_b_rr(%s,%s);\n",dst,src);
		comprintf("\ttest_b_rr(%s,%s);\n",dst,dst);
		comprintf("\t}\n");
		break;
	     case sz_word:
		comprintf("\tif (%s!=%s) {\n",src,dst);
		comprintf("\tmov_w_ri(%s,0);\n"
			  "\tstart_needflags();\n",dst);
		comprintf("\tor_w(%s,%s);\n",dst,src);
		comprintf("\t} else {\n");
		comprintf("\tmov_w_rr(%s,%s);\n",dst,src);
		comprintf("\ttest_w_rr(%s,%s);\n",dst,dst);
		comprintf("\t}\n");
		break;
	     case sz_long:
		comprintf("\tif (%s!=%s) {\n",src,dst);
		comprintf("\tmov_l_ri(%s,0);\n"
			  "\tstart_needflags();\n",dst);
		comprintf("\tor_l(%s,%s);\n",dst,src);
		comprintf("\t} else {\n");
		comprintf("\tmov_l_rr(%s,%s);\n",dst,src);
		comprintf("\ttest_l_rr(%s,%s);\n",dst,dst);
		comprintf("\t}\n");
		break;
	    }
	    comprintf("\tlive_flags();\n");
	    comprintf("\tend_needflags();\n");
	    close_brace();
	    return;
	}

     case flag_logical:
	comprintf("\tdont_care_flags();\n");
	start_brace();
	switch (size)
	{
	 case sz_byte:
	    comprintf("\tstart_needflags();\n"
		      "\ttest_b_rr(%s,%s);\n",value,value);
	    break;
	 case sz_word:
	    comprintf("\tstart_needflags();\n"
		      "\ttest_w_rr(%s,%s);\n",value,value);
	    break;
	 case sz_long:
	    comprintf("\tstart_needflags();\n"
		      "\ttest_l_rr(%s,%s);\n",value,value);
	    break;
	}
	comprintf("\tlive_flags();\n");
	comprintf("\tend_needflags();\n");
	close_brace();
	return;


     case flag_add:
     case flag_sub:
     case flag_cmp:
	comprintf("\tdont_care_flags();\n");
	{
	    const char* op;
	    switch(type) {
	     case flag_add: op="add"; break;
	     case flag_sub: op="sub"; break;
	     case flag_cmp: op="cmp"; break;
	     default: assert(0);
	    }
	    switch (size)
	    {
	     case sz_byte:
		comprintf("\tstart_needflags();\n"
			  "\t%s_b(%s,%s);\n",op,dst,src);
		break;
	     case sz_word:
		comprintf("\tstart_needflags();\n"
			  "\t%s_w(%s,%s);\n",op,dst,src);
		break;
	     case sz_long:
		comprintf("\tstart_needflags();\n"
			  "\t%s_l(%s,%s);\n",op,dst,src);
		break;
	    }
	    comprintf("\tlive_flags();\n");
	    comprintf("\tend_needflags();\n");
	    if (type!=flag_cmp) {
		duplicate_carry();
	    }
	    comprintf("if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");

	    return;
	}

     case flag_addx:
     case flag_subx:
		 uses_cmov;
	comprintf("\tdont_care_flags();\n");
	{
	    const char* op;
	    switch(type) {
	     case flag_addx: op="adc"; break;
	     case flag_subx: op="sbb"; break;
	     default: assert(0);
	    }
	    start_brace();
	    comprintf("\tint zero=scratchie++;\n"
		      "\tint one=scratchie++;\n"
		      "\tif (needed_flags&FLAG_Z) {\n"
		      "\tmov_l_ri(zero,0);\n"
		      "\tmov_l_ri(one,-1);\n"
		      "\tmake_flags_live();\n"
		      "\tcmov_l_rr(zero,one,%d);\n"
		      "\t}\n",NATIVE_CC_NE);
	    comprintf("\trestore_carry();\n"); /* Reload the X flag into C */
	    switch (size)
	    {
	     case sz_byte:
		comprintf("\tstart_needflags();\n"
			  "\t%s_b(%s,%s);\n",op,dst,src);
		break;
	     case sz_word:
		comprintf("\tstart_needflags();\n"
			  "\t%s_w(%s,%s);\n",op,dst,src);
		break;
	     case sz_long:
		comprintf("\tstart_needflags();\n"
			  "\t%s_l(%s,%s);\n",op,dst,src);
		break;
	    }
	    comprintf("\tlive_flags();\n");
	    comprintf("\tif (needed_flags&FLAG_Z) {\n");
	    comprintf("\tcmov_l_rr(zero,one,%d);\n", NATIVE_CC_NE);
	    comprintf("\tset_zero(zero, one);\n"); /* No longer need one */
	    comprintf("\tlive_flags();\n");
	    comprintf("\t}\n");
	    comprintf("\tend_needflags();\n");
	    duplicate_carry();
	    comprintf("if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
	    return;
	}
     default:
	failure;
	break;
    }
}

static int  /* returns zero for success, non-zero for failure */
gen_opcode (unsigned int opcode)
{
    struct instr *curi = table68k + opcode;
    const char* ssize=NULL;

    insn_n_cycles = 2;
    global_failure=0;
    long_opcode=0;
    global_isjump=0;
    global_iscjump=0;
    global_isaddx=0;
    global_cmov=0;
	global_fpu=0;
    global_mayfail=0;
    hack_opcode=opcode;
    endstr[0]=0;

    start_brace ();
    comprintf("\tuae_u8 scratchie=S1;\n");
    switch (curi->plev)
    {
     case 0:			/* not privileged */
	break;
     case 1:			/* unprivileged only on 68000 */
	if (cpu_level == 0)
	    break;
	if (next_cpu_level < 0)
	    next_cpu_level = 0;

	/* fall through */
     case 2:			/* priviledged */
	failure;   /* Easy ones first */
	break;
     case 3:			/* privileged if size == word */
	if (curi->size == sz_byte)
	    break;
	failure;
	break;
    }
    switch (curi->size) {
     case sz_byte: ssize="b"; break;
     case sz_word: ssize="w"; break;
     case sz_long: ssize="l"; break;
     default: assert(0);
    }
    (void)ssize;

    switch (curi->mnemo)
    {
     case i_OR:
     case i_AND:
     case i_EOR:
#ifdef DISABLE_I_OR_AND_EOR
    failure;
#endif
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", curi->size, "dst", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	switch(curi->mnemo) {
	 case i_OR: genflags (flag_or, curi->size, "", "src", "dst"); break;
	 case i_AND: genflags (flag_and, curi->size, "", "src", "dst"); break;
	 case i_EOR: genflags (flag_eor, curi->size, "", "src", "dst"); break;
	}
	genastore ("dst", curi->dmode, "dstreg", curi->size, "dst");
	break;

     case i_ORSR:
     case i_EORSR:
	failure;
	isjump;
	break;

     case i_ANDSR:
	failure;
	isjump;
	break;

     case i_SUB:
#ifdef DISABLE_I_SUB
    failure;
#endif
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", curi->size, "dst", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genflags (flag_sub, curi->size, "", "src", "dst");
	genastore ("dst", curi->dmode, "dstreg", curi->size, "dst");
	break;

     case i_SUBA:
#ifdef DISABLE_I_SUBA
    failure;
#endif
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", sz_long, "dst", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	start_brace();
	comprintf("\tint tmp=scratchie++;\n");
	switch(curi->size) {
	 case sz_byte: comprintf("\tsign_extend_8_rr(tmp,src);\n"); break;
	 case sz_word: comprintf("\tsign_extend_16_rr(tmp,src);\n"); break;
	 case sz_long: comprintf("\ttmp=src;\n"); break;
	 default: assert(0);
	}
	comprintf("\tsub_l(dst,tmp);\n");
	genastore ("dst", curi->dmode, "dstreg", sz_long, "dst");
	break;

     case i_SUBX:
#ifdef DISABLE_I_SUBX
    failure;
#endif
	isaddx;
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", curi->size, "dst", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genflags (flag_subx, curi->size, "", "src", "dst");
	genastore ("dst", curi->dmode, "dstreg", curi->size, "dst");
	break;

     case i_SBCD:
	failure;
	/* I don't think so! */
	break;

     case i_ADD:
#ifdef DISABLE_I_ADD
    failure;
#endif
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", curi->size, "dst", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genflags (flag_add, curi->size, "", "src", "dst");
	genastore ("dst", curi->dmode, "dstreg", curi->size, "dst");
	break;

     case i_ADDA:
#ifdef DISABLE_I_ADDA
	failure;
#endif
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", sz_long, "dst", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	start_brace();
	comprintf("\tint tmp=scratchie++;\n");
	switch(curi->size) {
	 case sz_byte: comprintf("\tsign_extend_8_rr(tmp,src);\n"); break;
	 case sz_word: comprintf("\tsign_extend_16_rr(tmp,src);\n"); break;
	 case sz_long: comprintf("\ttmp=src;\n"); break;
	 default: assert(0);
	}
	comprintf("\tadd_l(dst,tmp);\n");
	genastore ("dst", curi->dmode, "dstreg", sz_long, "dst");
	break;

     case i_ADDX:
#ifdef DISABLE_I_ADDX
    failure;
#endif
	isaddx;
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", curi->size, "dst", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	start_brace();
	genflags (flag_addx, curi->size, "", "src", "dst");
	genastore ("dst", curi->dmode, "dstreg", curi->size, "dst");
	break;

     case i_ABCD:
	failure;
	/* No BCD maths for me.... */
	break;

     case i_NEG:
#ifdef DISABLE_I_NEG
    failure;
#endif
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	start_brace ();
	comprintf("\tint dst=scratchie++;\n");
	comprintf("\tmov_l_ri(dst,0);\n");
	genflags (flag_sub, curi->size, "", "src", "dst");
	genastore ("dst", curi->smode, "srcreg", curi->size, "src");
	break;

     case i_NEGX:
#ifdef DISABLE_I_NEGX
    failure;
#endif
	isaddx;
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	start_brace ();
	comprintf("\tint dst=scratchie++;\n");
	comprintf("\tmov_l_ri(dst,0);\n");
	genflags (flag_subx, curi->size, "", "src", "dst");
	genastore ("dst", curi->smode, "srcreg", curi->size, "src");
	break;

     case i_NBCD:
	failure;
	/* Nope! */
	break;

     case i_CLR:
#ifdef DISABLE_I_CLR
    failure;
#endif
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH_ALIGN, GENA_MOVEM_DO_INC);
	start_brace();
	comprintf("\tint dst=scratchie++;\n");
	comprintf("\tmov_l_ri(dst,0);\n");
	genflags (flag_logical, curi->size, "dst", "", "");
	genastore ("dst", curi->smode, "srcreg", curi->size, "src");
	break;

     case i_NOT:
#ifdef DISABLE_I_NOT
    failure;
#endif
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	start_brace ();
	comprintf("\tint dst=scratchie++;\n");
	comprintf("\tmov_l_ri(dst,0xffffffff);\n");
	genflags (flag_eor, curi->size, "", "src", "dst");
	genastore ("dst", curi->smode, "srcreg", curi->size, "src");
	break;

     case i_TST:
#ifdef DISABLE_I_TST
    failure;
#endif
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genflags (flag_logical, curi->size, "src", "", "");
	break;
     case i_BCHG:
     case i_BCLR:
     case i_BSET:
     case i_BTST:
#ifdef DISABLE_I_BCHG_BCLR_BSET_BTST
    failure;
#endif
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", curi->size, "dst", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	start_brace();
	comprintf("\tint s=scratchie++;\n"
		  "\tint tmp=scratchie++;\n"
		  "\tmov_l_rr(s,src);\n");
	if (curi->size == sz_byte)
	    comprintf("\tand_l_ri(s,7);\n");
	else
	    comprintf("\tand_l_ri(s,31);\n");

	{
	    const char* op;
	    int need_write=1;

	    switch(curi->mnemo) {
	     case i_BCHG: op="btc"; break;
	     case i_BCLR: op="btr"; break;
	     case i_BSET: op="bts"; break;
	     case i_BTST: op="bt"; need_write=0; break;
	    default: op=""; assert(0);
	    }
	    comprintf("\t%s_l_rr(dst,s);\n"  /* Answer now in C */
				  "\tsbb_l(s,s);\n" /* s is 0 if bit was 0, -1 otherwise */
				  "\tmake_flags_live();\n" /* Get the flags back */
				  "\tdont_care_flags();\n",op);
		if (!noflags) {
		  comprintf("\tstart_needflags();\n"
					"\tset_zero(s,tmp);\n"
					"\tlive_flags();\n"
					"\tend_needflags();\n");
		}
	    if (need_write)
		genastore ("dst", curi->dmode, "dstreg", curi->size, "dst");
	}
	break;

     case i_CMPM:
     case i_CMP:
#ifdef DISABLE_I_CMPM_CMP
    failure;
#endif
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", curi->size, "dst", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	start_brace ();
	genflags (flag_cmp, curi->size, "", "src", "dst");
	break;

     case i_CMPA:
#ifdef DISABLE_I_CMPA
    failure;
#endif
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", sz_long, "dst", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	start_brace();
	comprintf("\tint tmps=scratchie++;\n");
	switch(curi->size) {
	 case sz_byte: comprintf("\tsign_extend_8_rr(tmps,src);\n"); break;
	 case sz_word: comprintf("\tsign_extend_16_rr(tmps,src);\n"); break;
	 case sz_long: comprintf("tmps=src;\n"); break;
	 default: assert(0);
	}
	genflags (flag_cmp, sz_long, "", "tmps", "dst");
	break;
	/* The next two are coded a little unconventional, but they are doing
	 * weird things... */

     case i_MVPRM:
	isjump;
	failure;
	break;

     case i_MVPMR:
	isjump;
	failure;
	break;

     case i_MOVE:
#ifdef DISABLE_I_MOVE
    failure;
#endif
	switch(curi->dmode) {
	 case Dreg:
	 case Areg:
	    genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	    genamode (curi->dmode, "dstreg", curi->size, "dst", GENA_GETV_FETCH_ALIGN, GENA_MOVEM_DO_INC);
	    genflags (flag_mov, curi->size, "", "src", "dst");
	    genastore ("dst", curi->dmode, "dstreg", curi->size, "dst");
	    break;
	 default: /* It goes to memory, not a register */
	    genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	    genamode (curi->dmode, "dstreg", curi->size, "dst", GENA_GETV_FETCH_ALIGN, GENA_MOVEM_DO_INC);
	    genflags (flag_logical, curi->size, "src", "", "");
	    genastore ("src", curi->dmode, "dstreg", curi->size, "dst");
	    break;
	}
	break;

     case i_MOVEA:
#ifdef DISABLE_I_MOVEA
    failure;
#endif
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", curi->size, "dst", GENA_GETV_FETCH_ALIGN, GENA_MOVEM_DO_INC);

	start_brace();
	comprintf("\tint tmps=scratchie++;\n");
	switch(curi->size) {
	 case sz_word: comprintf("\tsign_extend_16_rr(dst,src);\n"); break;
	 case sz_long: comprintf("\tmov_l_rr(dst,src);\n"); break;
	 default: assert(0);
	}
	genastore ("dst", curi->dmode, "dstreg", sz_long, "dst");
	break;

     case i_MVSR2:
	isjump;
	failure;
	break;

     case i_MV2SR:
	isjump;
	failure;
	break;

     case i_SWAP:
#ifdef DISABLE_I_SWAP
    failure;
#endif
	genamode (curi->smode, "srcreg", sz_long, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	comprintf("\tdont_care_flags();\n");
	comprintf("\trol_l_ri(src,16);\n");
	genflags (flag_logical, sz_long, "src", "", "");
	genastore ("src", curi->smode, "srcreg", sz_long, "src");
	break;

     case i_EXG:
#ifdef DISABLE_I_EXG
    failure;
#endif
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", curi->size, "dst", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	start_brace();
	comprintf("\tint tmp=scratchie++;\n"
		  "\tmov_l_rr(tmp,src);\n");
	genastore ("dst", curi->smode, "srcreg", curi->size, "src");
	genastore ("tmp", curi->dmode, "dstreg", curi->size, "dst");
	break;

	 case i_EXT:
#ifdef DISABLE_I_EXT
	failure;
#endif
	genamode (curi->smode, "srcreg", sz_long, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	comprintf("\tdont_care_flags();\n");
	start_brace ();
	switch (curi->size)
	{
	 case sz_byte:
	    comprintf ("\tint dst = src;\n"
		       "\tsign_extend_8_rr(src,src);\n");
	    break;
	 case sz_word:
	    comprintf ("\tint dst = scratchie++;\n"
		       "\tsign_extend_8_rr(dst,src);\n");
	    break;
	 case sz_long:
	    comprintf ("\tint dst = src;\n"
		       "\tsign_extend_16_rr(src,src);\n");
	    break;
	 default:
	    assert(0);
	}
	genflags (flag_logical,
		  curi->size == sz_word ? sz_word : sz_long, "dst", "", "");
	genastore ("dst", curi->smode, "srcreg",
		   curi->size == sz_word ? sz_word : sz_long, "src");
	break;

	 case i_MVMEL:
#ifdef DISABLE_I_MVEL
	failure;
#endif
	genmovemel (opcode);
	break;

     case i_MVMLE:
#ifdef DISABLE_I_MVMLE
    failure;
#endif
	genmovemle (opcode);
	break;

	 case i_TRAP:
#ifdef DISABLE_I_TRAP
	failure;
#endif
	isjump;
	mayfail;
	start_brace();
	comprintf("    int trapno = srcreg + 32;\n");
	gen_set_fault_pc();
	make_sr();
	comprintf("    compemu_enter_super(sr);\n");
	comprintf("    compemu_exc_make_frame(0, sr, ret, trapno, scratchie);\n");
	comprintf("    forget_about(ret);\n");
	/* m68k_setpc (get_long (regs.vbr + 4*nr)); */
	start_brace();
	comprintf("    int srca = scratchie++;\n");
	comprintf("    mov_l_rm(srca, (uintptr)&regs.vbr);\n");
	comprintf("    mov_l_brR(srca, srca, MEMBaseDiff + trapno * 4); mid_bswap_32(srca);\n");
	comprintf("    mov_l_mr((uintptr)&regs.pc, srca);\n");
	comprintf("    get_n_addr_jmp(srca, PC_P, scratchie);\n");
	comprintf("    mov_l_mr((uintptr)&regs.pc_oldp, PC_P);\n");
	gen_update_next_handler();
	disasm_this_inst(); /* for debugging only */
	/*
	 * this currently deactivates this feature, since it does not work yet
	 */
	failure;
	break;

     case i_MVR2USP:
	isjump;
	failure;
	break;

     case i_MVUSP2R:
	isjump;
	failure;
	break;

     case i_RESET:
	isjump;
	failure;
	break;

     case i_NOP:
	break;

     case i_STOP:
	isjump;
	failure;
	break;

     case i_RTE:
	isjump;
	failure;
	break;

     case i_RTD:
#ifdef DISABLE_I_RTD
    failure;
#endif
	genamode (curi->smode, "srcreg", curi->size, "offs", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	/* offs is constant */
	comprintf("\tadd_l_ri(offs,4);\n");
	start_brace();
	comprintf("\tint newad=scratchie++;\n"
		  "\treadlong(SP_REG,newad,scratchie);\n"
		  "\tmov_l_mr((uintptr)&regs.pc,newad);\n"
		  "\tget_n_addr_jmp(newad,PC_P,scratchie);\n"
		  "\tmov_l_mr((uintptr)&regs.pc_oldp,PC_P);\n"
		  "\tm68k_pc_offset=0;\n"
		  "\tadd_l(SP_REG,offs);\n");
	gen_update_next_handler();
	isjump;
	break;

     case i_LINK:
#ifdef DISABLE_I_LINK
    failure;
#endif
	genamode (curi->smode, "srcreg", sz_long, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", curi->size, "offs", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	comprintf("\tsub_l_ri(SP_REG,4);\n"
		  "\twritelong_clobber(SP_REG,src,scratchie);\n"
		  "\tmov_l_rr(src,SP_REG);\n");
	if (curi->size==sz_word)
	    comprintf("\tsign_extend_16_rr(offs,offs);\n");
	comprintf("\tadd_l(SP_REG,offs);\n");
	genastore ("src", curi->smode, "srcreg", sz_long, "src");
	break;

     case i_UNLK:
#ifdef DISABLE_I_UNLK
    failure;
#endif
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	comprintf("\tmov_l_rr(SP_REG,src);\n"
		  "\treadlong(SP_REG,src,scratchie);\n"
		  "\tadd_l_ri(SP_REG,4);\n");
	genastore ("src", curi->smode, "srcreg", curi->size, "src");
	break;

	 case i_RTS:
#ifdef DISABLE_I_RTS
	failure;
#endif
	comprintf("\tint newad=scratchie++;\n"
		  "\treadlong(SP_REG,newad,scratchie);\n"
		  "\tmov_l_mr((uintptr)&regs.pc,newad);\n"
		  "\tget_n_addr_jmp(newad,PC_P,scratchie);\n"
		  "\tmov_l_mr((uintptr)&regs.pc_oldp,PC_P);\n"
		  "\tm68k_pc_offset=0;\n"
		  "\tlea_l_brr(SP_REG,SP_REG,4);\n");
	gen_update_next_handler();
	isjump;
	break;

     case i_TRAPV:
	isjump;
	failure;
	break;

     case i_RTR:
	isjump;
	failure;
	break;

     case i_JSR:
#ifdef DISABLE_I_JSR
    failure;
#endif
	isjump;
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_NO_FETCH, GENA_MOVEM_DO_INC);
	start_brace();
	comprintf("\tuae_u32 retadd=start_pc+((char *)comp_pc_p-(char *)start_pc_p)+m68k_pc_offset;\n");
	comprintf("\tint ret=scratchie++;\n"
		  "\tmov_l_ri(ret,retadd);\n"
		  "\tsub_l_ri(SP_REG,4);\n"
		  "\twritelong_clobber(SP_REG,ret,scratchie);\n");
	comprintf("\tmov_l_mr((uintptr)&regs.pc,srca);\n"
		  "\tget_n_addr_jmp(srca,PC_P,scratchie);\n"
		  "\tmov_l_mr((uintptr)&regs.pc_oldp,PC_P);\n"
		  "\tm68k_pc_offset=0;\n");
	gen_update_next_handler();
	break;

     case i_JMP:
#ifdef DISABLE_I_JMP
    failure;
#endif
	isjump;
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_NO_FETCH, GENA_MOVEM_DO_INC);
	comprintf("\tmov_l_mr((uintptr)&regs.pc,srca);\n"
		  "\tget_n_addr_jmp(srca,PC_P,scratchie);\n"
		  "\tmov_l_mr((uintptr)&regs.pc_oldp,PC_P);\n"
		  "\tm68k_pc_offset=0;\n");
	gen_update_next_handler();
	break;

     case i_BSR:
#ifdef DISABLE_I_BSR
    failure;
#endif
	is_const_jump;
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	start_brace();
	comprintf("\tuae_u32 retadd=start_pc+((char *)comp_pc_p-(char *)start_pc_p)+m68k_pc_offset;\n");
	comprintf("\tint ret=scratchie++;\n"
		  "\tmov_l_ri(ret,retadd);\n"
		  "\tsub_l_ri(SP_REG,4);\n"
		  "\twritelong_clobber(SP_REG,ret,scratchie);\n");
	comprintf("\tadd_l_ri(src,m68k_pc_offset_thisinst+2);\n");
	comprintf("\tm68k_pc_offset=0;\n");
	comprintf("\tadd_l(PC_P,src);\n");

	comprintf("\tcomp_pc_p=(uae_u8*)(uintptr)get_const(PC_P);\n");
	gen_update_next_handler();
	break;

     case i_Bcc:
#ifdef DISABLE_I_BCC
    failure;
#endif
	comprintf("\tuae_u32 v,v1,v2;\n");
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	/* That source is an immediate, so we can clobber it with abandon */
	switch(curi->size) {
	 case sz_byte: comprintf("\tsign_extend_8_rr(src,src);\n"); break;
	 case sz_word: comprintf("\tsign_extend_16_rr(src,src);\n"); break;
	 case sz_long: break;
	}
	comprintf("\tsub_l_ri(src,m68k_pc_offset-m68k_pc_offset_thisinst-2);\n");
	/* Leave the following as "add" --- it will allow it to be optimized
	   away due to src being a constant ;-) */
	comprintf("\tadd_l_ri(src,(uintptr)comp_pc_p);\n");
	comprintf("\tmov_l_ri(PC_P,(uintptr)comp_pc_p);\n");
	/* Now they are both constant. Might as well fold in m68k_pc_offset */
	comprintf("\tadd_l_ri(src,m68k_pc_offset);\n");
	comprintf("\tadd_l_ri(PC_P,m68k_pc_offset);\n");
	comprintf("\tm68k_pc_offset=0;\n");

	if (curi->cc>=2) {
	    comprintf("\tv1=get_const(PC_P);\n"
		      "\tv2=get_const(src);\n"
		      "\tregister_branch(v1,v2,%d);\n",
		      cond_codes[curi->cc]);
	    comprintf("\tmake_flags_live();\n"); /* Load the flags */
	    isjump;
	}
	else {
	    is_const_jump;
	}

	switch(curi->cc) {
	 case 0:  /* Unconditional jump */
	    comprintf("\tmov_l_rr(PC_P,src);\n");
	    comprintf("\tcomp_pc_p=(uae_u8*)(uintptr)get_const(PC_P);\n");
	    break;
	 case 1: break; /* This is silly! */
	 case 8: failure; break;  /* Work out details! FIXME */
	 case 9: failure; break;  /* Not critical, though! */

	 case 2:
	 case 3:
	 case 4:
	 case 5:
	 case 6:
	 case 7:
	 case 10:
	 case 11:
	 case 12:
	 case 13:
	 case 14:
	 case 15:
	    break;
	 default: assert(0);
	}
	break;

     case i_LEA:
#ifdef DISABLE_I_LEA
    failure;
#endif
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_NO_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", curi->size, "dst", GENA_GETV_FETCH_ALIGN, GENA_MOVEM_DO_INC);
	genastore ("srca", curi->dmode, "dstreg", curi->size, "dst");
	break;

     case i_PEA:
#ifdef DISABLE_I_PEA
    failure;
#endif
	if (table68k[opcode].smode==Areg ||
	    table68k[opcode].smode==Aind ||
	    table68k[opcode].smode==Aipi ||
	    table68k[opcode].smode==Apdi ||
	    table68k[opcode].smode==Ad16 ||
	    table68k[opcode].smode==Ad8r)
	    comprintf("if (srcreg==7) dodgy=1;\n");

	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_NO_FETCH, GENA_MOVEM_DO_INC);
	genamode (Apdi, "7", sz_long, "dst", GENA_GETV_FETCH_ALIGN, GENA_MOVEM_DO_INC);
	genastore ("srca", Apdi, "7", sz_long, "dst");
	break;

     case i_DBcc:
#ifdef DISABLE_I_DBCC
    failure;
#endif
	isjump;
	uses_cmov;
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", curi->size, "offs", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);

	/* That offs is an immediate, so we can clobber it with abandon */
	switch(curi->size) {
	 case sz_word: comprintf("\tsign_extend_16_rr(offs,offs);\n"); break;
	 default: assert(0);  /* Seems this only comes in word flavour */
	}
	comprintf("\tsub_l_ri(offs,m68k_pc_offset-m68k_pc_offset_thisinst-2);\n");
	comprintf("\tadd_l_ri(offs,(uintptr)comp_pc_p);\n"); /* New PC,
								once the
								offset_68k is
								* also added */
	/* Let's fold in the m68k_pc_offset at this point */
	comprintf("\tadd_l_ri(offs,m68k_pc_offset);\n");
	comprintf("\tadd_l_ri(PC_P,m68k_pc_offset);\n");
	comprintf("\tm68k_pc_offset=0;\n");

	start_brace();
	comprintf("\tint nsrc=scratchie++;\n");

	if (curi->cc>=2) {
	    comprintf("\tmake_flags_live();\n"); /* Load the flags */
	}

	assert (curi->size==sz_word);

	switch(curi->cc) {
	 case 0: /* This is an elaborate nop? */
	    break;
	 case 1:
	    comprintf("\tstart_needflags();\n");
	    comprintf("\tsub_w_ri(src,1);\n");
	    comprintf("\tend_needflags();\n");
	    start_brace();
	    comprintf("\tuae_u32 v2,v;\n"
		      "\tuae_u32 v1=get_const(PC_P);\n");
	    comprintf("\tv2=get_const(offs);\n"
		      "\tregister_branch(v1,v2,%d);\n", NATIVE_CC_CC);
	    break;

	 case 8: failure; break;  /* Work out details! FIXME */
	 case 9: failure; break;  /* Not critical, though! */

	 case 2:
	 case 3:
	 case 4:
	 case 5:
	 case 6:
	 case 7:
	 case 10:
	 case 11:
	 case 12:
	 case 13:
	 case 14:
	 case 15:
	    comprintf("\tmov_l_rr(nsrc,src);\n");
	    comprintf("\tlea_l_brr(scratchie,src,(uae_s32)-1);\n"
		      "\tmov_w_rr(src,scratchie);\n");
	    comprintf("\tcmov_l_rr(offs,PC_P,%d);\n",
		      cond_codes[curi->cc]);
	    comprintf("\tcmov_l_rr(src,nsrc,%d);\n",
		      cond_codes[curi->cc]);
	    /* OK, now for cc=true, we have src==nsrc and offs==PC_P,
	       so whether we move them around doesn't matter. However,
	       if cc=false, we have offs==jump_pc, and src==nsrc-1 */

	    comprintf("\tstart_needflags();\n");
	    comprintf("\ttest_w_rr(nsrc,nsrc);\n");
	    comprintf("\tend_needflags();\n");
	    comprintf("\tcmov_l_rr(PC_P,offs,%d);\n", NATIVE_CC_NE);
	    break;
	 default: assert(0);
	}
	genastore ("src", curi->smode, "srcreg", curi->size, "src");
	gen_update_next_handler();
	break;

     case i_Scc:
#ifdef DISABLE_I_SCC
    failure;
#endif
	genamode (curi->smode, "srcreg", curi->size, "src", GENA_GETV_FETCH_ALIGN, GENA_MOVEM_DO_INC);
	start_brace ();
	comprintf ("\tint val = scratchie++;\n");

	/* We set val to 0 if we really should use 255, and to 1 for real 0 */
	switch(curi->cc) {
	 case 0:  /* Unconditional set */
	    comprintf("\tmov_l_ri(val,0);\n");
	    break;
	 case 1:
	    /* Unconditional not-set */
	    comprintf("\tmov_l_ri(val,1);\n");
	    break;
	 case 8: failure; break;  /* Work out details! FIXME */
	 case 9: failure; break;  /* Not critical, though! */

	 case 2:
	 case 3:
	 case 4:
	 case 5:
	 case 6:
	 case 7:
	 case 10:
	 case 11:
	 case 12:
	 case 13:
	 case 14:
	 case 15:
	    comprintf("\tmake_flags_live();\n"); /* Load the flags */
	    /* All condition codes can be inverted by changing the LSB */
	    comprintf("\tsetcc(val,%d);\n",
		      cond_codes[curi->cc]^1); break;
	 default: assert(0);
	}
	comprintf("\tsub_b_ri(val,1);\n");
	genastore ("val", curi->smode, "srcreg", curi->size, "src");
	break;

	 case i_DIVU:
	isjump;
	failure;
	break;

     case i_DIVS:
	isjump;
	failure;
	break;

	 case i_MULU:
#ifdef DISABLE_I_MULU
	failure;
#endif
	comprintf("\tdont_care_flags();\n");
	genamode (curi->smode, "srcreg", sz_word, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", sz_word, "dst", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	/* To do 16x16 unsigned multiplication, we actually use
	   32x32 signed, and zero-extend the registers first.
	   That solves the problem of MUL needing dedicated registers
	   on the x86 */
	comprintf("\tzero_extend_16_rr(scratchie,src);\n"
		  "\tzero_extend_16_rr(dst,dst);\n"
		  "\timul_32_32(dst,scratchie);\n");
	genflags (flag_logical, sz_long, "dst", "", "");
	genastore ("dst", curi->dmode, "dstreg", sz_long, "dst");
	break;

     case i_MULS:
#ifdef DISABLE_I_MULS
    failure;
#endif
	comprintf("\tdont_care_flags();\n");
	genamode (curi->smode, "srcreg", sz_word, "src", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", sz_word, "dst", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	comprintf("\tsign_extend_16_rr(scratchie,src);\n"
		  "\tsign_extend_16_rr(dst,dst);\n"
		  "\timul_32_32(dst,scratchie);\n");
	genflags (flag_logical, sz_long, "dst", "", "");
	genastore ("dst", curi->dmode, "dstreg", sz_long, "dst");
	break;

	 case i_CHK:
	isjump;
	failure;
	break;

     case i_CHK2:
	isjump;
	failure;
	break;

     case i_ASR:
#ifdef DISABLE_I_ASR
    failure;
#endif
	mayfail;
	if (curi->smode==Dreg) {
	    comprintf(
	    	"    if ((uae_u32)srcreg==(uae_u32)dstreg) {\n"
			"        FAIL(1);\n"
			"        " RETURN "\n"
			"    }\n");
	    start_brace();
	}
	comprintf("\tdont_care_flags();\n");

	genamode (curi->smode, "srcreg", curi->size, "cnt", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", curi->size, "data", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);

	start_brace();
	if (!noflags)
		comprintf("\tstart_needflags();\n");
	if (curi->smode!=immi) {
		uses_cmov;
		start_brace();
		comprintf("\tint zero = scratchie++;\n");
		comprintf("\tint tmpcnt = scratchie++;\n");
		comprintf("\tint minus1 = scratchie++;\n");
		comprintf("\tint cdata = minus1;\n");
		comprintf("\tmov_l_rr(tmpcnt,cnt);\n");
		comprintf("\tand_l_ri(tmpcnt,63);\n");
		comprintf("\tmov_l_ri(zero, 0);\n");
		comprintf("\tmov_l_ri(minus1, -1);\n");
		switch(curi->size) {
		 case sz_byte:
		 	comprintf("\ttest_b_rr(data,data);\n");
		 	comprintf("\tcmov_l_rr(zero, minus1, NATIVE_CC_MI);\n");
		 	comprintf("\ttest_l_ri(tmpcnt, 0x38);\n");
			comprintf("\tmov_l_rr(cdata,data);\n");
		 	comprintf("\tcmov_l_rr(cdata, zero, NATIVE_CC_NE);\n");
		 	comprintf("\tshra_b_rr(cdata,tmpcnt);\n");
			comprintf("\tmov_b_rr(data,cdata);\n");
		 	break;
		 case sz_word:
		 	comprintf("\ttest_w_rr(data,data);\n");
		 	comprintf("\tcmov_l_rr(zero, minus1, NATIVE_CC_MI);\n");
		 	comprintf("\ttest_l_ri(tmpcnt, 0x30);\n");
			comprintf("\tmov_l_rr(cdata,data);\n");
		 	comprintf("\tcmov_l_rr(cdata, zero, NATIVE_CC_NE);\n");
		 	comprintf("\tshra_w_rr(cdata,tmpcnt);\n");
			comprintf("\tmov_w_rr(data,cdata);\n");
		 	break;
		 case sz_long:
		 	comprintf("\ttest_l_rr(data,data);\n");
		 	comprintf("\tcmov_l_rr(zero, minus1, NATIVE_CC_MI);\n");
		 	comprintf("\ttest_l_ri(tmpcnt, 0x20);\n");
			comprintf("\tmov_l_rr(cdata,data);\n");
		 	comprintf("\tcmov_l_rr(cdata, zero, NATIVE_CC_NE);\n");
		 	comprintf("\tshra_l_rr(cdata,tmpcnt);\n");
			comprintf("\tmov_l_rr(data,cdata);\n");
		 	break;
		 default: assert(0);
		}
		/* Result of shift is now in data. */
	}
	else {
	    switch(curi->size) {
	     case sz_byte: comprintf("\tshra_b_ri(data,srcreg);\n"); break;
	     case sz_word: comprintf("\tshra_w_ri(data,srcreg);\n"); break;
	     case sz_long: comprintf("\tshra_l_ri(data,srcreg);\n"); break;
	     default: assert(0);
	    }
	}
	/* And create the flags */
	if (!noflags) {
		comprintf("\tlive_flags();\n");
		comprintf("\tend_needflags();\n");
		if (curi->smode!=immi)
			comprintf("\tsetcc_for_cntzero(tmpcnt, data, %d);\n", curi->size == sz_byte ? 1 : curi->size == sz_word ? 2 : 4);
		else
			comprintf("\tduplicate_carry();\n");
		comprintf("if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
	}
	genastore ("data", curi->dmode, "dstreg", curi->size, "data");
	break;

     case i_ASL:
#ifdef DISABLE_I_ASL
    failure;
#endif
	mayfail;
	if (curi->smode==Dreg) {
	    comprintf(
	    	"    if ((uae_u32)srcreg==(uae_u32)dstreg) {\n"
			"        FAIL(1);\n"
			"        " RETURN "\n"
			"    }\n");
	    start_brace();
	}
	comprintf("\tdont_care_flags();\n");
	/* Except for the handling of the V flag, this is identical to
	   LSL. The handling of V is, uhm, unpleasant, so if it's needed,
	   let the normal emulation handle it. Shoulders of giants kinda
	   thing ;-) */
	comprintf(
		"    if (needed_flags & FLAG_V) {\n"
		"        FAIL(1);\n"
		"        " RETURN "\n"
		"    }\n");

	genamode (curi->smode, "srcreg", curi->size, "cnt", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", curi->size, "data", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);

	start_brace();
	if (!noflags)
		comprintf("\tstart_needflags();\n");
	if (curi->smode!=immi) {
		uses_cmov;
		start_brace();
		comprintf("\tint cdata = scratchie++;\n");
		comprintf("\tint tmpcnt=scratchie++;\n");
		comprintf("\tmov_l_rr(tmpcnt,cnt);\n");
		comprintf("\tand_l_ri(tmpcnt,63);\n");
		comprintf("\tmov_l_ri(cdata, 0);\n");
		switch(curi->size) {
		 case sz_byte:
		 	comprintf("\ttest_l_ri(tmpcnt, 0x38);\n");
		 	comprintf("\tcmov_l_rr(cdata, data, NATIVE_CC_EQ);\n");
		 	comprintf("\tshll_b_rr(cdata,tmpcnt);\n");
			comprintf("\tmov_b_rr(data, cdata);\n");
		 	break;
		 case sz_word:
		 	comprintf("\ttest_l_ri(tmpcnt, 0x30);\n");
		 	comprintf("\tcmov_l_rr(cdata, data, NATIVE_CC_EQ);\n");
		 	comprintf("\tshll_w_rr(cdata,tmpcnt);\n");
			comprintf("\tmov_w_rr(data, cdata);\n");
		 	break;
		 case sz_long:
		 	comprintf("\ttest_l_ri(tmpcnt, 0x20);\n");
		 	comprintf("\tcmov_l_rr(cdata, data, NATIVE_CC_EQ);\n");
		 	comprintf("\tshll_l_rr(cdata,tmpcnt);\n");
			comprintf("\tmov_l_rr(data, cdata);\n");
		 	break;
		 default: assert(0);
		}
		/* Result of shift is now in data. */
	}
	else {
	    switch(curi->size) {
	     case sz_byte: comprintf("\tshll_b_ri(data,srcreg);\n"); break;
	     case sz_word: comprintf("\tshll_w_ri(data,srcreg);\n"); break;
	     case sz_long: comprintf("\tshll_l_ri(data,srcreg);\n"); break;
	     default: assert(0);
	    }
	}
	/* And create the flags */
	if (!noflags) {
		comprintf("\tlive_flags();\n");
		comprintf("\tend_needflags();\n");
		if (curi->smode!=immi)
			comprintf("\tsetcc_for_cntzero(tmpcnt, data, %d);\n", curi->size == sz_byte ? 1 : curi->size == sz_word ? 2 : 4);
		else
			comprintf("\tduplicate_carry();\n");
		comprintf("if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
	}
	genastore ("data", curi->dmode, "dstreg", curi->size, "data");
	break;

	 case i_LSR:
#ifdef DISABLE_I_LSR
	failure;
#endif
	mayfail;
	if (curi->smode==Dreg) {
	    comprintf(
	    	"    if ((uae_u32)srcreg==(uae_u32)dstreg) {\n"
			"        FAIL(1);\n"
			"        " RETURN "\n"
			"    }\n");
	    start_brace();
	}
	comprintf("\tdont_care_flags();\n");

	genamode (curi->smode, "srcreg", curi->size, "cnt", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", curi->size, "data", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);

	start_brace();
	if (!noflags)
		comprintf("\tstart_needflags();\n");
	if (curi->smode!=immi) {
		uses_cmov;
		start_brace();
		comprintf("\tint cdata = scratchie++;\n");
		comprintf("\tint tmpcnt=scratchie++;\n");
		comprintf("\tmov_l_rr(tmpcnt,cnt);\n");
		comprintf("\tand_l_ri(tmpcnt,63);\n");
		comprintf("\tmov_l_ri(cdata, 0);\n");
		switch(curi->size) {
		 case sz_byte:
		 	comprintf("\ttest_l_ri(tmpcnt, 0x38);\n");
		 	comprintf("\tcmov_l_rr(cdata, data, NATIVE_CC_EQ);\n");
		 	comprintf("\tshrl_b_rr(cdata,tmpcnt);\n");
			comprintf("\tmov_b_rr(data, cdata);\n");
		 	break;
		 case sz_word:
		 	comprintf("\ttest_l_ri(tmpcnt, 0x30);\n");
		 	comprintf("\tcmov_l_rr(cdata, data, NATIVE_CC_EQ);\n");
		 	comprintf("\tshrl_w_rr(cdata,tmpcnt);\n");
			comprintf("\tmov_w_rr(data, cdata);\n");
		 	break;
		 case sz_long:
		 	comprintf("\ttest_l_ri(tmpcnt, 0x20);\n");
		 	comprintf("\tcmov_l_rr(cdata, data, NATIVE_CC_EQ);\n");
		 	comprintf("\tshrl_l_rr(cdata, tmpcnt);\n");
			comprintf("\tmov_l_rr(data, cdata);\n");
		 	break;
		 default: assert(0);
		}
		/* Result of shift is now in data. */
	}
	else {
	    switch(curi->size) {
	     case sz_byte: comprintf("\tshrl_b_ri(data,srcreg);\n"); break;
	     case sz_word: comprintf("\tshrl_w_ri(data,srcreg);\n"); break;
	     case sz_long: comprintf("\tshrl_l_ri(data,srcreg);\n"); break;
	     default: assert(0);
	    }
	}
	/* And create the flags */
	if (!noflags) {
		comprintf("\tlive_flags();\n");
		comprintf("\tend_needflags();\n");
		if (curi->smode!=immi)
			comprintf("\tsetcc_for_cntzero(tmpcnt, data, %d);\n", curi->size == sz_byte ? 1 : curi->size == sz_word ? 2 : 4);
		else
			comprintf("\tduplicate_carry();\n");
		comprintf("if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
	}
	genastore ("data", curi->dmode, "dstreg", curi->size, "data");
	break;

	 case i_LSL:
#ifdef DISABLE_I_LSL
	failure;
#endif
	mayfail;
	if (curi->smode==Dreg) {
		comprintf(
			"    if ((uae_u32)srcreg==(uae_u32)dstreg) {\n"
			"        FAIL(1);\n"
			"        " RETURN "\n"
			"    }\n");
		start_brace();
	}
	comprintf("\tdont_care_flags();\n");

	genamode (curi->smode, "srcreg", curi->size, "cnt", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", curi->size, "data", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);

	start_brace();
	if (!noflags)
		comprintf("\tstart_needflags();\n");
	if (curi->smode!=immi) {
		uses_cmov;
		start_brace();
		comprintf("\tint cdata = scratchie++;\n");
		comprintf("\tint tmpcnt = scratchie++;\n");
		comprintf("\tmov_l_rr(tmpcnt,cnt);\n");
		comprintf("\tand_l_ri(tmpcnt,63);\n");
		comprintf("\tmov_l_ri(cdata, 0);\n");
		switch(curi->size) {
		 case sz_byte:
		 	comprintf("\ttest_l_ri(tmpcnt, 0x38);\n");
		 	comprintf("\tcmov_l_rr(cdata, data, NATIVE_CC_EQ);\n");
		 	comprintf("\tshll_b_rr(cdata,tmpcnt);\n");
			comprintf("\tmov_b_rr(data, cdata);\n");
		 	break;
		 case sz_word:
		 	comprintf("\ttest_l_ri(tmpcnt, 0x30);\n");
		 	comprintf("\tcmov_l_rr(cdata, data, NATIVE_CC_EQ);\n");
		 	comprintf("\tshll_w_rr(cdata,tmpcnt);\n");
			comprintf("\tmov_w_rr(data, cdata);\n");
		 	break;
		 case sz_long:
		 	comprintf("\ttest_l_ri(tmpcnt, 0x20);\n");
		 	comprintf("\tcmov_l_rr(cdata, data, NATIVE_CC_EQ);\n");
		 	comprintf("\tshll_l_rr(cdata,tmpcnt);\n");
			comprintf("\tmov_l_rr(data, cdata);\n");
		 	break;
		 default: assert(0);
		}
		/* Result of shift is now in data. */
	}
	else {
	    switch(curi->size) {
	     case sz_byte: comprintf("\tshll_b_ri(data,srcreg);\n"); break;
	     case sz_word: comprintf("\tshll_w_ri(data,srcreg);\n"); break;
	     case sz_long: comprintf("\tshll_l_ri(data,srcreg);\n"); break;
	     default: assert(0);
	    }
	}
	/* And create the flags */
	if (!noflags) {
		comprintf("\tlive_flags();\n");
		comprintf("\tend_needflags();\n");
		if (curi->smode!=immi)
			comprintf("\tsetcc_for_cntzero(tmpcnt, data, %d);\n", curi->size == sz_byte ? 1 : curi->size == sz_word ? 2 : 4);
		else
			comprintf("\tduplicate_carry();\n");
		comprintf("if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
	}
	genastore ("data", curi->dmode, "dstreg", curi->size, "data");
	break;

	 case i_ROL:
#ifdef DISABLE_I_ROL
	failure;
#endif
	mayfail;
	if (curi->smode==Dreg) {
	    comprintf(
	    	"    if ((uae_u32)srcreg==(uae_u32)dstreg) {\n"
			"        FAIL(1);\n"
			"        " RETURN "\n"
			"    }\n");
	    start_brace();
	}
	comprintf("\tdont_care_flags();\n");
	genamode (curi->smode, "srcreg", curi->size, "cnt", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", curi->size, "data", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	start_brace ();

	switch(curi->size) {
	 case sz_long: comprintf("\trol_l_rr(data,cnt);\n"); break;
	 case sz_word: comprintf("\trol_w_rr(data,cnt);\n"); break;
	 case sz_byte: comprintf("\trol_b_rr(data,cnt);\n"); break;
	}

	if (!noflags) {
	    comprintf("\tstart_needflags();\n");
	    /*
	     * x86 ROL instruction does not set ZF/SF, so we need extra checks here
	     */
	    comprintf("\tif (needed_flags & FLAG_ZNV)\n");
	    switch(curi->size) {
	     case sz_byte: comprintf("\t    test_b_rr(data,data);\n"); break;
	     case sz_word: comprintf("\t    test_w_rr(data,data);\n"); break;
	     case sz_long: comprintf("\t    test_l_rr(data,data);\n"); break;
	    }
	    comprintf("\tbt_l_ri(data,0x00);\n"); /* Set C */
	    comprintf("\tlive_flags();\n");
	    comprintf("\tend_needflags();\n");
	}
	genastore ("data", curi->dmode, "dstreg", curi->size, "data");
	break;

     case i_ROR:
#ifdef DISABLE_I_ROR
    failure;
#endif
	mayfail;
	if (curi->smode==Dreg) {
	    comprintf(
	    	"    if ((uae_u32)srcreg==(uae_u32)dstreg) {\n"
			"        FAIL(1);\n"
			"        " RETURN "\n"
			"    }\n");
	    start_brace();
	}
	comprintf("\tdont_care_flags();\n");
	genamode (curi->smode, "srcreg", curi->size, "cnt", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	genamode (curi->dmode, "dstreg", curi->size, "data", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	start_brace ();

	switch(curi->size) {
	 case sz_long: comprintf("\tror_l_rr(data,cnt);\n"); break;
	 case sz_word: comprintf("\tror_w_rr(data,cnt);\n"); break;
	 case sz_byte: comprintf("\tror_b_rr(data,cnt);\n"); break;
	}

	if (!noflags) {
	    comprintf("\tstart_needflags();\n");
	    /*
	     * x86 ROR instruction does not set ZF/SF, so we need extra checks here
	     */
	    comprintf("\tif (needed_flags & FLAG_ZNV)\n");
	    switch(curi->size) {
	     case sz_byte: comprintf("\t    test_b_rr(data,data);\n"); break;
	     case sz_word: comprintf("\t    test_w_rr(data,data);\n"); break;
	     case sz_long: comprintf("\t    test_l_rr(data,data);\n"); break;
	    }
	    switch(curi->size) {
	     case sz_byte: comprintf("\tbt_l_ri(data,0x07);\n"); break;
	     case sz_word: comprintf("\tbt_l_ri(data,0x0f);\n"); break;
	     case sz_long: comprintf("\tbt_l_ri(data,0x1f);\n"); break;
	    }
	    comprintf("\tlive_flags();\n");
	    comprintf("\tend_needflags();\n");
	}
	genastore ("data", curi->dmode, "dstreg", curi->size, "data");
	break;

     case i_ROXL:
	failure;
	break;

     case i_ROXR:
	failure;
	break;

     case i_ASRW:
	failure;
	break;

     case i_ASLW:
	failure;
	break;

     case i_LSRW:
	failure;
	break;

     case i_LSLW:
	failure;
	break;

     case i_ROLW:
	failure;
	break;

     case i_RORW:
	failure;
	break;

     case i_ROXLW:
	failure;
	break;

     case i_ROXRW:
	failure;
	break;

     case i_MOVEC2:
	isjump;
	failure;
	break;

     case i_MOVE2C:
	isjump;
	failure;
	break;

     case i_CAS:
	failure;
	break;

     case i_CAS2:
	failure;
	break;

     case i_MOVES:		/* ignore DFC and SFC because we have no MMU */
	isjump;
	failure;
	break;

     case i_BKPT:		/* only needed for hardware emulators */
	isjump;
	failure;
	break;

     case i_CALLM:		/* not present in 68030 */
	isjump;
	failure;
	break;

     case i_RTM:		/* not present in 68030 */
	isjump;
	failure;
	break;

     case i_TRAPcc:
	isjump;
	failure;
	break;

     case i_DIVL:
	isjump;
	failure;
	break;

     case i_MULL:
#ifdef DISABLE_I_MULL
    failure;
#endif
	if (!noflags) {
	    failure;
	    break;
	}
	comprintf("\tuae_u16 extra=%s;\n",gen_nextiword());
	comprintf("\tint r2=(extra>>12)&7;\n"
		  "\tint tmp=scratchie++;\n");

	genamode (curi->dmode, "dstreg", curi->size, "dst", GENA_GETV_FETCH, GENA_MOVEM_DO_INC);
	/* The two operands are in dst and r2 */
	comprintf("\tif (extra&0x0400) {\n" /* Need full 64 bit result */
		  "\tint r3=(extra&7);\n"
		  "\tmov_l_rr(r3,dst);\n"); /* operands now in r3 and r2 */
	comprintf("\tif (extra&0x0800) { \n" /* signed */
		  "\t\timul_64_32(r2,r3);\n"
		  "\t} else { \n"
		  "\t\tmul_64_32(r2,r3);\n"
		  "\t} \n");
	/* The result is in r2/tmp, with r2 holding the lower 32 bits */
	comprintf("\t} else {\n");  /* Only want 32 bit result */
	/* operands in dst and r2, result foes into r2 */
	/* shouldn't matter whether it's signed or unsigned?!? */
	comprintf("\timul_32_32(r2,dst);\n"
		  "\t}\n");
	break;

     case i_BFTST:
     case i_BFEXTU:
     case i_BFCHG:
     case i_BFEXTS:
     case i_BFCLR:
     case i_BFFFO:
     case i_BFSET:
     case i_BFINS:
	failure;
	break;

     case i_PACK:
	failure;
	break;

     case i_UNPK:
	failure;
	break;

	 case i_TAS:
	failure;
	break;

     case i_FPP:
#ifdef DISABLE_I_FPP
    failure;
#endif
	uses_fpu;
	mayfail;
	comprintf("#ifdef USE_JIT_FPU\n");
	comprintf("\tuae_u16 extra=%s;\n",gen_nextiword());
	swap_opcode();
	comprintf("\tcomp_fpp_opp(opcode,extra);\n");
	comprintf("#else\n");
	comprintf("\tfailure = 1;\n");
	comprintf("#endif\n");
	break;

     case i_FBcc:
#ifdef DISABLE_I_FBCC
    failure;
#endif
	uses_fpu;
	isjump;
	uses_cmov;
	mayfail;
	comprintf("#ifdef USE_JIT_FPU\n");
	swap_opcode();
	comprintf("\tcomp_fbcc_opp(opcode);\n");
	comprintf("#else\n");
	comprintf("\tfailure = 1;\n");
	comprintf("#endif\n");
	break;

     case i_FDBcc:
	uses_fpu;
	isjump;
	failure;
	break;

     case i_FScc:
#ifdef DISABLE_I_FSCC
    failure;
#endif
	uses_fpu;
	mayfail;
	uses_cmov;
	comprintf("#ifdef USE_JIT_FPU\n");
	comprintf("\tuae_u16 extra=%s;\n",gen_nextiword());
	swap_opcode();
	comprintf("\tcomp_fscc_opp(opcode,extra);\n");
	comprintf("#else\n");
	comprintf("\tfailure = 1;\n");
	comprintf("#endif\n");
	break;

     case i_FTRAPcc:
	uses_fpu;
	isjump;
	failure;
	break;

     case i_FSAVE:
	uses_fpu;
	failure;
	break;

     case i_FRESTORE:
	uses_fpu;
	failure;
	break;

     case i_CINVL:
     case i_CINVP:
     case i_CINVA:
	isjump;  /* Not really, but it's probably a good idea to stop
		    translating at this point */
	failure;
	comprintf ("\tflush_icache();\n");  /* Differentiate a bit more? */
	break;

     case i_CPUSHL:
     case i_CPUSHP:
     case i_CPUSHA:
	isjump;  /* Not really, but it's probably a good idea to stop
		    translating at this point */
	failure;
	break;

     case i_MOVE16:
#ifdef DISABLE_I_MOVE16
    failure;
#endif
	genmov16(opcode,curi);
	break;

#ifdef UAE
     case i_MMUOP030:
     case i_PFLUSHN:
     case i_PFLUSH:
     case i_PFLUSHAN:
     case i_PFLUSHA:
     case i_PLPAR:
     case i_PLPAW:
     case i_PTESTR:
     case i_PTESTW:
     case i_LPSTOP:
	isjump;
	failure;
	break;
#endif

#ifdef WINUAE_ARANYM
     case i_EMULOP_RETURN:
	isjump;
	failure;
	break;
	
     case i_EMULOP:
	failure;
	break;

 //     case i_NATFEAT_ID:
 //     case i_NATFEAT_CALL:
	// failure;
	// break;

     case i_MMUOP:
	isjump; 
	failure;
	break;
#endif

	 default:
		assert(0);
	break;
    }
    comprintf("%s",endstr);
    finish_braces ();
    sync_m68k_pc ();
    if (global_mayfail)
	comprintf("    if (failure)\n        m68k_pc_offset = m68k_pc_offset_thisinst;\n");
    return global_failure;
}

static void
generate_includes (FILE * f)
{
	// fprintf (f, "#include \"sysconfig.h\"\n");
	fprintf (f, "#if defined(JIT)\n");
	fprintf (f, "#include \"sysdeps.h\"\n");
#ifdef UAE
	fprintf (f, "#include \"options.h\"\n");
	fprintf (f, "#include \"uae/memory.h\"\n");
#else
	fprintf (f, "#include \"m68k.h\"\n");
	fprintf (f, "#include \"memory.h\"\n");
#endif
	fprintf (f, "#include \"readcpu.h\"\n");
	fprintf (f, "#include \"newcpu.h\"\n");
	fprintf (f, "#include \"comptbl.h\"\n");
	fprintf (f, "#include \"debug.h\"\n");
}

static int postfix;


static char *decodeEA (amodes mode, wordsizes size)
{
	static char buffer[80];

	buffer[0] = 0;
	switch (mode){
	case Dreg:
		strcpy (buffer,"Dn");
		break;
	case Areg:
		strcpy (buffer,"An");
		break;
	case Aind:
		strcpy (buffer,"(An)");
		break;
	case Aipi:
		strcpy (buffer,"(An)+");
		break;
	case Apdi:
		strcpy (buffer,"-(An)");
		break;
	case Ad16:
		strcpy (buffer,"(d16,An)");
		break;
	case Ad8r:
		strcpy (buffer,"(d8,An,Xn)");
		break;
	case PC16:
		strcpy (buffer,"(d16,PC)");
		break;
	case PC8r:
		strcpy (buffer,"(d8,PC,Xn)");
		break;
	case absw:
		strcpy (buffer,"(xxx).W");
		break;
	case absl:
		strcpy (buffer,"(xxx).L");
		break;
	case imm:
		switch (size){
		case sz_byte:
			strcpy (buffer,"#<data>.B");
			break;
		case sz_word:
			strcpy (buffer,"#<data>.W");
			break;
		case sz_long:
			strcpy (buffer,"#<data>.L");
			break;
		default:
			break;
		}
		break;
	case imm0:
		strcpy (buffer,"#<data>.B");
		break;
	case imm1:
		strcpy (buffer,"#<data>.W");
		break;
	case imm2:
		strcpy (buffer,"#<data>.L");
		break;
	case immi:
		strcpy (buffer,"#<data>");
		break;

	default:
		break;
	}
	return buffer;
}

static char *outopcode (const char *name, int opcode)
{
	static char out[100];
	struct instr *ins;

	ins = &table68k[opcode];
	strcpy (out, name);
	if (ins->smode == immi)
		strcat (out, "Q");
	if (ins->size == sz_byte)
		strcat (out,".B");
	if (ins->size == sz_word)
		strcat (out,".W");
	if (ins->size == sz_long)
		strcat (out,".L");
	strcat (out," ");
	if (ins->suse)
		strcat (out, decodeEA (ins->smode, ins->size));
	if (ins->duse) {
		if (ins->suse) strcat (out,",");
		strcat (out, decodeEA (ins->dmode, ins->size));
	}
	return out;
}


static void
generate_one_opcode (int rp, int noflags)
{
    int i;
    uae_u16 smsk, dmsk;
    unsigned int opcode = opcode_map[rp];
    int aborted=0;
    int have_srcreg=0;
    int have_dstreg=0;
	const char *name;

    if (table68k[opcode].mnemo == i_ILLG
	|| table68k[opcode].clev > cpu_level)
	return;

    for (i = 0; lookuptab[i].name[0]; i++)
    {
	if (table68k[opcode].mnemo == lookuptab[i].mnemo)
	    break;
    }

    if (table68k[opcode].handler != -1)
	return;

    switch (table68k[opcode].stype)
    {
    case 0:
	smsk = 7;
	break;
    case 1:
	smsk = 255;
	break;
    case 2:
	smsk = 15;
	break;
    case 3:
	smsk = 7;
	break;
    case 4:
	smsk = 7;
	break;
    case 5:
	smsk = 63;
	break;
#ifndef UAE
    case 6:
	smsk = 255;
	break;
#endif
	case 7:
	smsk = 3;
	break;
    default:
    smsk = 0;
	assert(0);
    }
    dmsk = 7;

    next_cpu_level = -1;
    if (table68k[opcode].suse
	&& table68k[opcode].smode != imm && table68k[opcode].smode != imm0
	&& table68k[opcode].smode != imm1 && table68k[opcode].smode != imm2
	&& table68k[opcode].smode != absw && table68k[opcode].smode != absl
	&& table68k[opcode].smode != PC8r && table68k[opcode].smode != PC16)
    {
	have_srcreg=1;
	if (table68k[opcode].spos == -1)
	{
	    if (((int) table68k[opcode].sreg) >= 128)
		comprintf ("\tuae_s32 srcreg = (uae_s32)(uae_s8)%d;\n", (int) table68k[opcode].sreg);
	    else
		comprintf ("\tuae_s32 srcreg = %d;\n", (int) table68k[opcode].sreg);
	}
	else
	{
	    char source[100];
	    int pos = table68k[opcode].spos;

#ifndef UAE
	    comprintf ("#if defined(HAVE_GET_WORD_UNSWAPPED) && !defined(FULLMMU)\n");
		
	    if (pos < 8 && (smsk >> (8 - pos)) != 0)
		sprintf (source, "(((opcode >> %d) | (opcode << %d)) & %d)",
			pos ^ 8, 8 - pos, dmsk);
	    else if (pos != 8)
		sprintf (source, "((opcode >> %d) & %d)", pos ^ 8, smsk);
	    else
		sprintf (source, "(opcode & %d)", smsk);
		
	    if (table68k[opcode].stype == 3)
		comprintf ("\tuae_u32 srcreg = imm8_table[%s];\n", source);
	    else if (table68k[opcode].stype == 1)
		comprintf ("\tuae_u32 srcreg = (uae_s32)(uae_s8)%s;\n", source);
	    else
		comprintf ("\tuae_u32 srcreg = %s;\n", source);
		
	    comprintf ("#else\n");
#endif
		
	    if (pos)
		sprintf (source, "((opcode >> %d) & %d)", pos, smsk);
	    else
		sprintf (source, "(opcode & %d)", smsk);

	    if (table68k[opcode].stype == 3)
		comprintf ("\tuae_s32 srcreg = imm8_table[%s];\n", source);
	    else if (table68k[opcode].stype == 1)
		comprintf ("\tuae_s32 srcreg = (uae_s32)(uae_s8)%s;\n", source);
	    else
		comprintf ("\tuae_s32 srcreg = %s;\n", source);

#ifndef UAE
		comprintf ("#endif\n");
#endif
	}
    }
    if (table68k[opcode].duse
	/* Yes, the dmode can be imm, in case of LINK or DBcc */
	&& table68k[opcode].dmode != imm && table68k[opcode].dmode != imm0
	&& table68k[opcode].dmode != imm1 && table68k[opcode].dmode != imm2
	&& table68k[opcode].dmode != absw && table68k[opcode].dmode != absl)
    {
	have_dstreg=1;
	if (table68k[opcode].dpos == -1)
	{
	    if (((int) table68k[opcode].dreg) >= 128)
		comprintf ("\tuae_s32 dstreg = (uae_s32)(uae_s8)%d;\n", (int) table68k[opcode].dreg);
	    else
		comprintf ("\tuae_s32 dstreg = %d;\n", (int) table68k[opcode].dreg);
	}
	else
	{
	    int pos = table68k[opcode].dpos;

#ifndef UAE
	    comprintf ("#if defined(HAVE_GET_WORD_UNSWAPPED) && !defined(FULLMMU)\n");
		
	    if (pos < 8 && (dmsk >> (8 - pos)) != 0)
		comprintf ("\tuae_u32 dstreg = ((opcode >> %d) | (opcode << %d)) & %d;\n",
			pos ^ 8, 8 - pos, dmsk);
	    else if (pos != 8)
		comprintf ("\tuae_u32 dstreg = (opcode >> %d) & %d;\n",
			pos ^ 8, dmsk);
	    else
			comprintf ("\tuae_u32 dstreg = opcode & %d;\n", dmsk);
		
		comprintf ("#else\n");
#endif

	    if (pos)
		comprintf ("\tuae_u32 dstreg = (opcode >> %d) & %d;\n",
			   pos, dmsk);
	    else
			comprintf ("\tuae_u32 dstreg = opcode & %d;\n", dmsk);

#ifndef UAE
		comprintf ("#endif\n");
#endif
	}
    }

    if (have_srcreg && have_dstreg &&
	(table68k[opcode].dmode==Areg ||
	 table68k[opcode].dmode==Aind ||
	 table68k[opcode].dmode==Aipi ||
	 table68k[opcode].dmode==Apdi ||
	 table68k[opcode].dmode==Ad16 ||
	 table68k[opcode].dmode==Ad8r) &&
	(table68k[opcode].smode==Areg ||
	 table68k[opcode].smode==Aind ||
	 table68k[opcode].smode==Aipi ||
	 table68k[opcode].smode==Apdi ||
	 table68k[opcode].smode==Ad16 ||
	 table68k[opcode].smode==Ad8r)
	) {
	comprintf("\tuae_u32 dodgy=(srcreg==(uae_s32)dstreg);\n");
    }
    else {
	comprintf("\tuae_u32 dodgy=0;\n");
    }
    comprintf("\tuae_u32 m68k_pc_offset_thisinst=m68k_pc_offset;\n");
    comprintf("\tm68k_pc_offset+=2;\n");

    aborted=gen_opcode (opcode);
    {
	char flags[64 * 6];
	*flags = '\0';
	if (global_isjump)	strcat(flags, "COMP_OPCODE_ISJUMP|");
	if (long_opcode)	strcat(flags, "COMP_OPCODE_LONG_OPCODE|");
	if (global_cmov)	strcat(flags, "COMP_OPCODE_CMOV|");
	if (global_isaddx)	strcat(flags, "COMP_OPCODE_ISADDX|");
	if (global_iscjump)	strcat(flags, "COMP_OPCODE_ISCJUMP|");
	if (global_fpu)		strcat(flags, "COMP_OPCODE_USES_FPU|");
	if (*flags)
		flags[strlen(flags) - 1] = '\0';
	else
		strcpy(flags, "0");

#ifdef UAE /* RETTYPE != void */
	comprintf ("return 0;\n");
#endif
	comprintf ("}\n");

	name = lookuptab[i].name;
	if (aborted) {
	    fprintf (stblfile, "{ NULL, %u, %s }, /* %s */\n", opcode, flags, name);
	    com_discard();
	} else {
		const char *tbl = noflags ? "nf" : "ff";
		fprintf (stblfile, "{ op_%x_%d_comp_%s, %u, %s }, /* %s */\n", opcode, postfix, tbl, opcode, flags, name);
		fprintf (headerfile, "extern compop_func op_%x_%d_comp_%s;\n", opcode, postfix, tbl);
		printf ("/* %s */\n", outopcode (name, opcode));
		printf (RETTYPE " REGPARAM2 op_%x_%d_comp_%s(uae_u32 opcode) /* %s */\n{\n", opcode, postfix, tbl, name);
	    com_flush();
	}
    }
    opcode_next_clev[rp] = next_cpu_level;
    opcode_last_postfix[rp] = postfix;
}

static void
generate_func (int noflags)
{
    int i, j, rp;
	const char *tbl = noflags ? "nf" : "ff";

    using_prefetch = 0;
    using_exception_3 = 0;
    for (i = 0; i < 1; i++) /* We only do one level! */
    {
	cpu_level = NEXT_CPU_LEVEL - i;
	postfix = i;

	fprintf (stblfile, "const struct comptbl op_smalltbl_%d_comp_%s[] = {\n", postfix, tbl);

	/* sam: this is for people with low memory (eg. me :)) */
	printf ("\n"
		 "#if !defined(PART_1) && !defined(PART_2) && "
		 "!defined(PART_3) && !defined(PART_4) && "
		 "!defined(PART_5) && !defined(PART_6) && "
		 "!defined(PART_7) && !defined(PART_8)"
		 "\n"
		 "#define PART_1 1\n"
		 "#define PART_2 1\n"
		 "#define PART_3 1\n"
		 "#define PART_4 1\n"
		 "#define PART_5 1\n"
		 "#define PART_6 1\n"
		 "#define PART_7 1\n"
		 "#define PART_8 1\n"
		 "#endif\n\n");
#ifdef UAE
	printf ("extern void comp_fpp_opp();\n"
		"extern void comp_fscc_opp();\n"
		"extern void comp_fbcc_opp();\n\n");
#endif

	rp = 0;
	for (j = 1; j <= 8; ++j)
	{
	    int k = (j * nr_cpuop_funcs) / 8;
	    printf ("#ifdef PART_%d\n", j);
	    for (; rp < k; rp++)
		generate_one_opcode (rp,noflags);
	    printf ("#endif\n\n");
	}

	fprintf (stblfile, "{ 0, 65536, 0 }};\n");
    }

}

#if (defined(OS_cygwin) || defined(OS_mingw)) && defined(EXTENDED_SIGSEGV)
void cygwin_mingw_abort()
{
#undef abort
	abort();
}
#endif

#if defined(FSUAE) && defined (WINDOWS)
#include "windows.h"
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#else
int main(void)
#endif
{
    init_table68k ();

    opcode_map = (int *) malloc (sizeof (int) * nr_cpuop_funcs);
    opcode_last_postfix = (int *) malloc (sizeof (int) * nr_cpuop_funcs);
    opcode_next_clev = (int *) malloc (sizeof (int) * nr_cpuop_funcs);
    counts = (unsigned long *) malloc (65536 * sizeof (unsigned long));
    read_counts ();

    /* It would be a lot nicer to put all in one file (we'd also get rid of
     * cputbl.h that way), but cpuopti can't cope.  That could be fixed, but
     * I don't dare to touch the 68k version.  */

	headerfile = fopen (GEN_PATH "comptbl.h", "wb");
	fprintf (headerfile, ""
		"extern const struct comptbl op_smalltbl_0_comp_nf[];\n"
		"extern const struct comptbl op_smalltbl_0_comp_ff[];\n"
		"");

	stblfile = fopen (GEN_PATH "compstbl.cpp", "wb");
	if (freopen (GEN_PATH "compemu.cpp", "wb", stdout) == NULL) {
		abort();
	}

    generate_includes (stdout);
    generate_includes (stblfile);

    printf("#include \"" JIT_PATH "compemu.h\"\n");
    printf("#include \"" JIT_PATH "flags_x86.h\"\n");

    noflags=0;
    generate_func (noflags);

	free(opcode_map);
	free(opcode_last_postfix);
	free(opcode_next_clev);
	free(counts);

    opcode_map = (int *) malloc (sizeof (int) * nr_cpuop_funcs);
    opcode_last_postfix = (int *) malloc (sizeof (int) * nr_cpuop_funcs);
    opcode_next_clev = (int *) malloc (sizeof (int) * nr_cpuop_funcs);
    counts = (unsigned long *) malloc (65536 * sizeof (unsigned long));
    read_counts ();
    noflags=1;
    generate_func (noflags);

    printf ("#endif\n");
    fprintf (stblfile, "#endif\n");

	free(opcode_map);
	free(opcode_last_postfix);
	free(opcode_next_clev);
	free(counts);

    free (table68k);
	fclose (stblfile);
	fclose (headerfile);
	(void)disasm_this_inst;
    return 0;
}

#ifdef UAE
void write_log (const TCHAR *format,...)
{
}
#endif
