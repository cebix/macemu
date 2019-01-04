/*
 *  compiler/gencomp.c - MC680x0 compilation generator
 *
 *  Based on work Copyright 1995, 1996 Bernd Schmidt
 *  Changes for UAE-JIT Copyright 2000 Bernd Meyer
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include "sysdeps.h"
#include "../readcpu.h"

#define BOOL_TYPE		"int"
#define failure			global_failure=1
#define FAILURE			global_failure=1
#define isjump			global_isjump=1
#define is_const_jump	global_iscjump=1;
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

static int cond_codes_x86[]={-1,-1,7,6,3,2,5,4,-1,-1,9,8,13,12,15,14};

static void comprintf(const char* format, ...)
{
    va_list args;

    va_start(args,format);
    comp_index+=vsprintf(lines+comp_index,format,args);
}

static void com_discard(void)
{
    comp_index=0;
}

static void com_flush(void)
{
    int i;
    for (i=0;i<comp_index;i++)
	putchar(lines[i]);
    com_discard();
}


static FILE *headerfile;
static FILE *stblfile;

static int using_prefetch;
static int using_exception_3;
static unsigned int cpu_level;
static int noflags;

/* For the current opcode, the next lower level that will have different code.
 * Initialized to -1 for each opcode. If it remains unchanged, indicates we
 * are done with that opcode.  */
static int next_cpu_level;

static int *opcode_map;
static int *opcode_next_clev;
static int *opcode_last_postfix;
static unsigned long *counts;

static void 
read_counts (void)
{
    FILE *file;
    unsigned long opcode, count, total;
    char name[20];
    int nr = 0;
    memset (counts, 0, 65536 * sizeof *counts);

    file = fopen ("frequent.68k", "r");
    if (file)
    {
	fscanf (file, "Total: %lu\n", &total);
	while (fscanf (file, "%lx: %lu %s\n", &opcode, &count, name) == 3)
	{
	    opcode_next_clev[nr] = 4;
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
	    opcode_next_clev[nr] = 4;
	    opcode_last_postfix[nr] = -1;
	    opcode_map[nr++] = opcode;
	    counts[opcode] = count;
	}
    }
    if (nr != nr_cpuop_funcs)
	abort ();
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
}

static void 
pop_braces (int to)
{
    while (n_braces > to)
	close_brace ();
}

static int 
bit_size (int size)
{
    switch (size)
    {
     case sz_byte:
	return 8;
     case sz_word:
	return 16;
     case sz_long:
	return 32;
     default:
	abort ();
    }
    return 0;
}

static const char *
bit_mask (int size)
{
    switch (size)
    {
     case sz_byte:
	return "0xff";
     case sz_word:
	return "0xffff";
     case sz_long:
	return "0xffffffff";
     default:
	abort ();
    }
    return 0;
}

static __inline__ void gen_update_next_handler(void)
{
    return; /* Can anything clever be done here? */
}

static void gen_writebyte(char* address, char* source)
{
    comprintf("\twritebyte(%s,%s,scratchie);\n",address,source);
}

static void gen_writeword(char* address, char* source)
{
    comprintf("\twriteword(%s,%s,scratchie);\n",address,source);
}

static void gen_writelong(char* address, char* source)
{
    comprintf("\twritelong(%s,%s,scratchie);\n",address,source);
}

static void gen_readbyte(char* address, char* dest)
{
    comprintf("\treadbyte(%s,%s,scratchie);\n",address,dest);
}

static void gen_readword(char* address, char* dest)
{
    comprintf("\treadword(%s,%s,scratchie);\n",address,dest);
}

static void gen_readlong(char* address, char* dest)
{
    comprintf("\treadlong(%s,%s,scratchie);\n",address,dest);
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
	comprintf("#ifdef HAVE_GET_WORD_UNSWAPPED\n");
	comprintf("\topcode = do_byteswap_16(opcode);\n");
	comprintf("#endif\n");
}

static void 
sync_m68k_pc (void)
{
    comprintf("\t if (m68k_pc_offset>100) sync_m68k_pc();\n"); 
}


/* getv == 1: fetch data; getv != 0: check for odd address. If movem != 0,
 * the calling routine handles Apdi and Aipi modes.
 * gb-- movem == 2 means the same thing but for a MOVE16 instruction */
static void 
genamode (amodes mode, char *reg, wordsizes size, char *name, int getv, int movem)
{
    start_brace ();
    switch (mode)
    {
     case Dreg: /* Do we need to check dodgy here? */
	if (movem)
	    abort ();
	if (getv == 1 || getv==2) { 
	    /* We generate the variable even for getv==2, so we can use
	       it as a destination for MOVE */
	    comprintf ("\tint %s=%s;\n",name,reg);
	}
	return;

     case Areg:
	if (movem)
	    abort ();
	if (getv == 1 || getv==2) {
	    /* see above */
	    comprintf ("\tint %s=dodgy?scratchie++:%s+8;\n",name,reg);
	    if (getv==1) {
		comprintf ("\tif (dodgy) \n");
		comprintf ("\t\tmov_l_rr(%s,%s+8);\n",name, reg);
	    }
	}
	return;

     case Aind:
	comprintf ("\tint %sa=dodgy?scratchie++:%s+8;\n",name,reg);
	comprintf ("\tif (dodgy) \n");
	comprintf ("\t\tmov_l_rr(%sa,%s+8);\n",name, reg);
	break;
     case Aipi:
	comprintf ("\tint %sa=scratchie++;\n",name,reg);
	comprintf ("\tmov_l_rr(%sa,%s+8);\n",name, reg);
	break;
     case Apdi: 
	switch (size)   
	{
	 case sz_byte:
	    if (movem) {
		comprintf ("\tint %sa=dodgy?scratchie++:%s+8;\n",name,reg);
		comprintf ("\tif (dodgy) \n");
		comprintf("\tmov_l_rr(%sa,8+%s);\n",name,reg);
	    }
	    else {
		start_brace();
		comprintf ("\tint %sa=dodgy?scratchie++:%s+8;\n",name,reg);
		comprintf("\tlea_l_brr(%s+8,%s+8,(uae_s32)-areg_byteinc[%s]);\n",reg,reg,reg);
		comprintf ("\tif (dodgy) \n");
		comprintf("\tmov_l_rr(%sa,8+%s);\n",name,reg);
	    }
	    break;
	 case sz_word:
	    if (movem) {
		comprintf ("\tint %sa=dodgy?scratchie++:%s+8;\n",name,reg);
		comprintf ("\tif (dodgy) \n");
		comprintf("\tmov_l_rr(%sa,8+%s);\n",name,reg);
	    }
	    else {
		start_brace();
		comprintf ("\tint %sa=dodgy?scratchie++:%s+8;\n",name,reg);
		comprintf("\tlea_l_brr(%s+8,%s+8,-2);\n",reg,reg);
		comprintf ("\tif (dodgy) \n");
		comprintf("\tmov_l_rr(%sa,8+%s);\n",name,reg);
	    }
	    break;
	 case sz_long:
	    if (movem) {
		comprintf ("\tint %sa=dodgy?scratchie++:%s+8;\n",name,reg);
		comprintf ("\tif (dodgy) \n");
		comprintf("\tmov_l_rr(%sa,8+%s);\n",name,reg);
	    }
	    else {
		start_brace();
		comprintf ("\tint %sa=dodgy?scratchie++:%s+8;\n",name,reg);
		comprintf("\tlea_l_brr(%s+8,%s+8,-4);\n",reg,reg);
		comprintf ("\tif (dodgy) \n");
		comprintf("\tmov_l_rr(%sa,8+%s);\n",name,reg);
	    }
	    break;
	 default:
	    abort ();
	}
	break;
     case Ad16:
	comprintf("\tint %sa=scratchie++;\n",name);
	comprintf("\tmov_l_rr(%sa,8+%s);\n",name,reg);
	comprintf("\tlea_l_brr(%sa,%sa,(uae_s32)(uae_s16)%s);\n",name,name,gen_nextiword());
	break;
     case Ad8r:
	comprintf("\tint %sa=scratchie++;\n",name);
	comprintf("\tcalc_disp_ea_020(%s+8,%s,%sa,scratchie);\n",
		  reg,gen_nextiword(),name);
	break;

     case PC16:
	comprintf("\tint %sa=scratchie++;\n",name);
	comprintf("\tuae_u32 address=start_pc+((char *)comp_pc_p-(char *)start_pc_p)+m68k_pc_offset;\n");
	comprintf ("\tuae_s32 PC16off = (uae_s32)(uae_s16)%s;\n", gen_nextiword ());
	comprintf("\tmov_l_ri(%sa,address+PC16off);\n",name);
	break;

     case PC8r:
	comprintf("\tint pctmp=scratchie++;\n");
	comprintf("\tint %sa=scratchie++;\n",name);
	comprintf("\tuae_u32 address=start_pc+((char *)comp_pc_p-(char *)start_pc_p)+m68k_pc_offset;\n");
	start_brace();
	comprintf("\tmov_l_ri(pctmp,address);\n");

	comprintf("\tcalc_disp_ea_020(pctmp,%s,%sa,scratchie);\n",
		  gen_nextiword(),name);
	break;
     case absw:
	comprintf ("\tint %sa = scratchie++;\n",name);
	comprintf ("\tmov_l_ri(%sa,(uae_s32)(uae_s16)%s);\n", name, gen_nextiword ());
	break;
     case absl:
	comprintf ("\tint %sa = scratchie++;\n",name);
	comprintf ("\tmov_l_ri(%sa,%s); /* absl */\n", name, gen_nextilong ());
	break;
     case imm:
	if (getv != 1)
	    abort ();
	switch (size)
	{
	 case sz_byte:
	    comprintf ("\tint %s = scratchie++;\n",name);
	    comprintf ("\tmov_l_ri(%s,(uae_s32)(uae_s8)%s);\n", name, gen_nextibyte ());
	    break;
	 case sz_word:
	    comprintf ("\tint %s = scratchie++;\n",name);
	    comprintf ("\tmov_l_ri(%s,(uae_s32)(uae_s16)%s);\n", name, gen_nextiword ());
	    break;
	 case sz_long:
	    comprintf ("\tint %s = scratchie++;\n",name);
	    comprintf ("\tmov_l_ri(%s,%s);\n", name, gen_nextilong ());
	    break;
	 default:
	    abort ();
	}
	return;
     case imm0:
	if (getv != 1)
	    abort ();
	comprintf ("\tint %s = scratchie++;\n",name);
	comprintf ("\tmov_l_ri(%s,(uae_s32)(uae_s8)%s);\n", name, gen_nextibyte ());
	return;
     case imm1:
	if (getv != 1)
	    abort ();
	comprintf ("\tint %s = scratchie++;\n",name);
	comprintf ("\tmov_l_ri(%s,(uae_s32)(uae_s16)%s);\n", name, gen_nextiword ());
	return;
     case imm2:
	if (getv != 1)
	    abort ();
	comprintf ("\tint %s = scratchie++;\n",name);
	comprintf ("\tmov_l_ri(%s,%s);\n", name, gen_nextilong ());
	return;
     case immi:
	if (getv != 1)
	    abort ();
	comprintf ("\tint %s = scratchie++;\n",name);
	comprintf ("\tmov_l_ri(%s,%s);\n", name, reg);
	return;
     default:
	abort ();
    }

    /* We get here for all non-reg non-immediate addressing modes to
     * actually fetch the value. */
    if (getv == 1)
    {
	char astring[80];
	sprintf(astring,"%sa",name);
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
	    abort ();
	}
	start_brace ();
	comprintf("\tint %s=scratchie++;\n",name);
	switch (size)
	{
	 case sz_byte:
	    gen_readbyte(astring,name);
	    break;
	 case sz_word:
	    gen_readword(astring,name);
	    break;
	 case sz_long:
	    gen_readlong(astring,name);
	    break;
	 default:
	    abort ();
	}
    }

    /* We now might have to fix up the register for pre-dec or post-inc
     * addressing modes. */
    if (!movem) {
	switch (mode)
	{
	 case Aipi:
	    switch (size)
	    {
	     case sz_byte:
		comprintf("\tlea_l_brr(%s+8,%s+8,areg_byteinc[%s]);\n",reg,reg,reg);
		break;
	     case sz_word:
		comprintf("\tlea_l_brr(%s+8,%s+8,2);\n",reg,reg,reg);
		break;
	     case sz_long:
		comprintf("\tlea_l_brr(%s+8,%s+8,4);\n",reg,reg);
		break;
	     default:
		abort ();
	    }
	    break;
	 case Apdi:
	    break;
	 default:
	    break;
	}
    }
}

static void 
genastore (char *from, amodes mode, char *reg, wordsizes size, char *to)
{
    switch (mode)
    {
     case Dreg:
	switch (size)
	{
	 case sz_byte:
	    comprintf("\tif(%s!=%s)\n",reg,from);
	    comprintf ("\t\tmov_b_rr(%s,%s);\n", reg, from);
	    break;
	 case sz_word:
	    comprintf("\tif(%s!=%s)\n",reg,from);
	    comprintf ("\t\tmov_w_rr(%s,%s);\n", reg, from);
	    break;
	 case sz_long:
	    comprintf("\tif(%s!=%s)\n",reg,from);
	    comprintf ("\t\tmov_l_rr(%s,%s);\n", reg, from);
	    break;
	 default:
	    abort ();
	}
	break;
     case Areg:
	switch (size)
	{
	 case sz_word:
	    comprintf("\tif(%s+8!=%s)\n",reg,from);
	    comprintf ("\t\tmov_w_rr(%s+8,%s);\n", reg, from);
	    break;
	 case sz_long:
	    comprintf("\tif(%s+8!=%s)\n",reg,from);
	    comprintf ("\t\tmov_l_rr(%s+8,%s);\n", reg, from);
	    break;
	 default:
	    abort ();
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
	 sprintf(astring,"%sa",to);
	
	 switch (size)
	 {
	  case sz_byte:
	     insn_n_cycles += 2;
	     gen_writebyte(astring,from);
	     break;
	  case sz_word:
	     insn_n_cycles += 2;
	     gen_writeword(astring,from);
	     break;
	  case sz_long:
	     insn_n_cycles += 4;
	     gen_writelong(astring,from);
	     break;
	  default:
	     abort ();
	 }
     }
     break;
     case imm:
     case imm0:
     case imm1:
     case imm2:
     case immi:
	abort ();
	break;
     default:
	abort ();
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
		genamode (curi->smode, "srcreg", curi->size, "src", 0, 2);
		genamode (curi->dmode, "dstreg", curi->size, "dst", 0, 2);
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

	comprintf("\tint tmp=scratchie;\n");
	comprintf("\tscratchie+=4;\n");
	
	comprintf("\tget_n_addr(src,src,scratchie);\n"
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
}

static void 
genmovemel (uae_u16 opcode)
{
    comprintf ("\tuae_u16 mask = %s;\n", gen_nextiword ());
    comprintf ("\tint native=scratchie++;\n");
    comprintf ("\tint i;\n");
    comprintf ("\tsigned char offset=0;\n");
    genamode (table68k[opcode].dmode, "dstreg", table68k[opcode].size, "src", 2, 1);
    comprintf("\tget_n_addr(srca,native,scratchie);\n");

    comprintf("\tfor (i=0;i<16;i++) {\n"
	      "\t\tif ((mask>>i)&1) {\n");
    switch(table68k[opcode].size) {
     case sz_long: 
	comprintf("\t\t\tmov_l_rR(i,native,offset);\n"
		  "\t\t\tbswap_32(i);\n"
		  "\t\t\toffset+=4;\n");
	break;
     case sz_word: 
	comprintf("\t\t\tmov_w_rR(i,native,offset);\n"
		  "\t\t\tbswap_16(i);\n"
		  "\t\t\tsign_extend_16_rr(i,i);\n"
		  "\t\t\toffset+=2;\n");
	break;
     default: abort();
    }
    comprintf("\t\t}\n"
	      "\t}");
    if (table68k[opcode].dmode == Aipi) {
	comprintf("\t\t\tlea_l_brr(8+dstreg,srca,offset);\n"); 
    }
}


static void 
genmovemle (uae_u16 opcode)
{
    comprintf ("\tuae_u16 mask = %s;\n", gen_nextiword ());
    comprintf ("\tint native=scratchie++;\n");
    comprintf ("\tint i;\n");
    comprintf ("\tint tmp=scratchie++;\n");
    comprintf ("\tsigned char offset=0;\n");
    genamode (table68k[opcode].dmode, "dstreg", table68k[opcode].size, "src", 2, 1);

    comprintf("\tget_n_addr(srca,native,scratchie);\n");

    if (table68k[opcode].dmode!=Apdi) {
	comprintf("\tfor (i=0;i<16;i++) {\n"
		  "\t\tif ((mask>>i)&1) {\n");
	switch(table68k[opcode].size) {
	 case sz_long: 
	    comprintf("\t\t\tmov_l_rr(tmp,i);\n"
		      "\t\t\tbswap_32(tmp);\n"
		      "\t\t\tmov_l_Rr(native,tmp,offset);\n"
		      "\t\t\toffset+=4;\n");
	    break;
	 case sz_word: 
	    comprintf("\t\t\tmov_l_rr(tmp,i);\n"
		      "\t\t\tbswap_16(tmp);\n"
		      "\t\t\tmov_w_Rr(native,tmp,offset);\n"
		      "\t\t\toffset+=2;\n");
	    break;
	 default: abort();
	}
    }
    else {  /* Pre-decrement */
	comprintf("\tfor (i=0;i<16;i++) {\n"
		  "\t\tif ((mask>>i)&1) {\n");
	switch(table68k[opcode].size) {
	 case sz_long: 
	    comprintf("\t\t\toffset-=4;\n"
		      "\t\t\tmov_l_rr(tmp,15-i);\n"
		      "\t\t\tbswap_32(tmp);\n"
		      "\t\t\tmov_l_Rr(native,tmp,offset);\n"
		      );
	    break;
	 case sz_word: 
	    comprintf("\t\t\toffset-=2;\n"
		      "\t\t\tmov_l_rr(tmp,15-i);\n"
		      "\t\t\tbswap_16(tmp);\n"
		      "\t\t\tmov_w_Rr(native,tmp,offset);\n"
		      );
	    break;
	 default: abort();
	}
    }
  

    comprintf("\t\t}\n"
	      "\t}");
    if (table68k[opcode].dmode == Apdi) {
	comprintf("\t\t\tlea_l_brr(8+dstreg,srca,(uae_s32)offset);\n");
    }
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
genflags (flagtypes type, wordsizes size, char *value, char *src, char *dst)
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
		char* op;
		switch(type) {
		 case flag_add: op="add"; break;
		 case flag_sub: op="sub"; break;
		 default: abort();
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
		char* op;
		switch(type) {
		 case flag_or:  op="or"; break;
		 case flag_eor: op="xor"; break;
		 default: abort();
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
		char* op;
		switch(type) {
		 case flag_addx: op="adc"; break;
		 case flag_subx: op="sbb"; break;
		 default: abort();
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

     case flag_and:
     case flag_or:
     case flag_eor:
	comprintf("\tdont_care_flags();\n");
	start_brace();
	{
	    char* op;
	    switch(type) {
	     case flag_and: op="and"; break;
	     case flag_or:  op="or"; break;
	     case flag_eor: op="xor"; break;
	     default: abort();
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
	    char* op;
	    switch(type) {
	     case flag_add: op="add"; break;
	     case flag_sub: op="sub"; break;
	     case flag_cmp: op="cmp"; break;
	     default: abort();
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
	    char* op;
	    switch(type) {
	     case flag_addx: op="adc"; break;
	     case flag_subx: op="sbb"; break;
	     default: abort();
	    }
	    start_brace();
	    comprintf("\tint zero=scratchie++;\n"
		      "\tint one=scratchie++;\n"
		      "\tif (needed_flags&FLAG_Z) {\n"
		      "\tmov_l_ri(zero,0);\n"
		      "\tmov_l_ri(one,-1);\n"
		      "\tmake_flags_live();\n"
		      "\tcmov_l_rr(zero,one,5);\n"
		      "\t}\n");
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
	    comprintf("\tif (needed_flags&FLAG_Z) {\n"
		      "\tcmov_l_rr(zero,one,5);\n"
		      "\tset_zero(zero, one);\n" /* No longer need one */
		      "\tlive_flags();\n"
		      "\t}\n");
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

static void 
force_range_for_rox (const char *var, wordsizes size)
{
    /* Could do a modulo operation here... which one is faster? */
    switch (size)
    {
     case sz_long:
	comprintf ("\tif (%s >= 33) %s -= 33;\n", var, var);
	break;
     case sz_word:
	comprintf ("\tif (%s >= 34) %s -= 34;\n", var, var);
	comprintf ("\tif (%s >= 17) %s -= 17;\n", var, var);
	break;
     case sz_byte:
	comprintf ("\tif (%s >= 36) %s -= 36;\n", var, var);
	comprintf ("\tif (%s >= 18) %s -= 18;\n", var, var);
	comprintf ("\tif (%s >= 9) %s -= 9;\n", var, var);
	break;
    }
}

static const char *
cmask (wordsizes size)
{
    switch (size)
    {
     case sz_byte:
	return "0x80";
     case sz_word:
	return "0x8000";
     case sz_long:
	return "0x80000000";
     default:
	abort ();
    }
}

static int 
source_is_imm1_8 (struct instr *i)
{
    return i->stype == 3;
}

static int  /* returns zero for success, non-zero for failure */
gen_opcode (unsigned long int opcode)
{
    struct instr *curi = table68k + opcode;
    char* ssize=NULL;

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
     default: abort();
    }

    switch (curi->mnemo)
    {
     case i_OR:
     case i_AND:
     case i_EOR:
	genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
	genamode (curi->dmode, "dstreg", curi->size, "dst", 1, 0);
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
	genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
	genamode (curi->dmode, "dstreg", curi->size, "dst", 1, 0);
	genflags (flag_sub, curi->size, "", "src", "dst");
	genastore ("dst", curi->dmode, "dstreg", curi->size, "dst");
	break;
     case i_SUBA:
	genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
	genamode (curi->dmode, "dstreg", sz_long, "dst", 1, 0);
	start_brace();
	comprintf("\tint tmp=scratchie++;\n");
	switch(curi->size) {
	 case sz_byte: comprintf("\tsign_extend_8_rr(tmp,src);\n"); break;
	 case sz_word: comprintf("\tsign_extend_16_rr(tmp,src);\n"); break;
	 case sz_long: comprintf("\ttmp=src;\n"); break;
	 default: abort();
	}
	comprintf("\tsub_l(dst,tmp);\n");
	genastore ("dst", curi->dmode, "dstreg", sz_long, "dst");
	break;
     case i_SUBX:
	isaddx;
	genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
	genamode (curi->dmode, "dstreg", curi->size, "dst", 1, 0);
	genflags (flag_subx, curi->size, "", "src", "dst");
	genastore ("dst", curi->dmode, "dstreg", curi->size, "dst");
	break;
     case i_SBCD:
	failure;
	/* I don't think so! */
	break;
     case i_ADD:
	genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
	genamode (curi->dmode, "dstreg", curi->size, "dst", 1, 0);
	genflags (flag_add, curi->size, "", "src", "dst");
	genastore ("dst", curi->dmode, "dstreg", curi->size, "dst");
	break;
     case i_ADDA:
	genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
	genamode (curi->dmode, "dstreg", sz_long, "dst", 1, 0);
	start_brace();
	comprintf("\tint tmp=scratchie++;\n");
	switch(curi->size) {
	 case sz_byte: comprintf("\tsign_extend_8_rr(tmp,src);\n"); break;
	 case sz_word: comprintf("\tsign_extend_16_rr(tmp,src);\n"); break;
	 case sz_long: comprintf("\ttmp=src;\n"); break;
	 default: abort();
	}
	comprintf("\tadd_l(dst,tmp);\n");
	genastore ("dst", curi->dmode, "dstreg", sz_long, "dst");
	break;
     case i_ADDX:
	isaddx;
	genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
	genamode (curi->dmode, "dstreg", curi->size, "dst", 1, 0);
	start_brace();
	genflags (flag_addx, curi->size, "", "src", "dst");
	genastore ("dst", curi->dmode, "dstreg", curi->size, "dst");
	break;
     case i_ABCD:
	failure;
	/* No BCD maths for me.... */
	break;
     case i_NEG:
	genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
	start_brace ();
	comprintf("\tint dst=scratchie++;\n");
	comprintf("\tmov_l_ri(dst,0);\n");
	genflags (flag_sub, curi->size, "", "src", "dst");
	genastore ("dst", curi->smode, "srcreg", curi->size, "src");
	break;
     case i_NEGX:
    isaddx;
	genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
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
	genamode (curi->smode, "srcreg", curi->size, "src", 2, 0);
	start_brace();
	comprintf("\tint dst=scratchie++;\n");
	comprintf("\tmov_l_ri(dst,0);\n");
	genflags (flag_logical, curi->size, "dst", "", "");
	genastore ("dst", curi->smode, "srcreg", curi->size, "src");
	break;
     case i_NOT:
	genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
	start_brace ();
	comprintf("\tint dst=scratchie++;\n");
	comprintf("\tmov_l_ri(dst,0xffffffff);\n");
	genflags (flag_eor, curi->size, "", "src", "dst");
	genastore ("dst", curi->smode, "srcreg", curi->size, "src");
	break;
     case i_TST:
	genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
	genflags (flag_logical, curi->size, "src", "", "");
	break;
     case i_BCHG:
     case i_BCLR:
     case i_BSET:
     case i_BTST:
/*	failure; /* NEW: from "Ipswitch Town" release */
	genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
	genamode (curi->dmode, "dstreg", curi->size, "dst", 1, 0);
	start_brace();
	comprintf("\tint s=scratchie++;\n"
		  "\tint tmp=scratchie++;\n"
		  "\tmov_l_rr(s,src);\n");
	if (curi->size == sz_byte)
	    comprintf("\tand_l_ri(s,7);\n");
	else
	    comprintf("\tand_l_ri(s,31);\n");

	{
	    char* op;
	    int need_write=1;

	    switch(curi->mnemo) {
	     case i_BCHG: op="btc"; break;
	     case i_BCLR: op="btr"; break;
	     case i_BSET: op="bts"; break;
	     case i_BTST: op="bt"; need_write=0; break;
	    default: abort();
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
	genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
	genamode (curi->dmode, "dstreg", curi->size, "dst", 1, 0);
	start_brace ();
	genflags (flag_cmp, curi->size, "", "src", "dst");
	break;
     case i_CMPA:
	genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
	genamode (curi->dmode, "dstreg", sz_long, "dst", 1, 0);
	start_brace();
	comprintf("\tint tmps=scratchie++;\n");
	switch(curi->size) {
	 case sz_byte: comprintf("\tsign_extend_8_rr(tmps,src);\n"); break;
	 case sz_word: comprintf("\tsign_extend_16_rr(tmps,src);\n"); break;
	 case sz_long: comprintf("tmps=src;\n"); break;
	 default: abort();
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
	switch(curi->dmode) {
	 case Dreg:
	 case Areg:
	    genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
	    genamode (curi->dmode, "dstreg", curi->size, "dst", 2, 0);
	    genflags (flag_mov, curi->size, "", "src", "dst");
	    genastore ("dst", curi->dmode, "dstreg", curi->size, "dst");
	    break;
	 default: /* It goes to memory, not a register */
	    genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
	    genamode (curi->dmode, "dstreg", curi->size, "dst", 2, 0);
	    genflags (flag_logical, curi->size, "src", "", "");
	    genastore ("src", curi->dmode, "dstreg", curi->size, "dst");
	    break;
	}
	break;
     case i_MOVEA:
	genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
	genamode (curi->dmode, "dstreg", curi->size, "dst", 2, 0);

	start_brace();
	comprintf("\tint tmps=scratchie++;\n");
	switch(curi->size) {
	 case sz_word: comprintf("\tsign_extend_16_rr(dst,src);\n"); break;
	 case sz_long: comprintf("\tmov_l_rr(dst,src);\n"); break;
	 default: abort();
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
	genamode (curi->smode, "srcreg", sz_long, "src", 1, 0);
	comprintf("\tdont_care_flags();\n");
	comprintf("\trol_l_ri(src,16);\n");
	genflags (flag_logical, sz_long, "src", "", "");
	genastore ("src", curi->smode, "srcreg", sz_long, "src");
	break;
     case i_EXG:
	genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
	genamode (curi->dmode, "dstreg", curi->size, "dst", 1, 0);
	start_brace();
	comprintf("\tint tmp=scratchie++;\n"
		  "\tmov_l_rr(tmp,src);\n");
	genastore ("dst", curi->smode, "srcreg", curi->size, "src");
	genastore ("tmp", curi->dmode, "dstreg", curi->size, "dst");
	break;
     case i_EXT:
	genamode (curi->smode, "srcreg", sz_long, "src", 1, 0);
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
	    abort ();
	}
	genflags (flag_logical,
		  curi->size == sz_word ? sz_word : sz_long, "dst", "", "");
	genastore ("dst", curi->smode, "srcreg",
		   curi->size == sz_word ? sz_word : sz_long, "src");
	break;
     case i_MVMEL:
	genmovemel ((uae_u16)opcode);
	break;
     case i_MVMLE:
	genmovemle ((uae_u16)opcode);
	break;
     case i_TRAP:
	isjump; 
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
/*	failure; /* NEW: from "Ipswitch Town" release */
	genamode (curi->smode, "srcreg", curi->size, "offs", 1, 0);
	/* offs is constant */
	comprintf("\tadd_l_ri(offs,4);\n");
	start_brace();
	comprintf("\tint newad=scratchie++;\n"
		  "\treadlong(15,newad,scratchie);\n"
		  "\tmov_l_mr((uintptr)&regs.pc,newad);\n"
		  "\tget_n_addr_jmp(newad,PC_P,scratchie);\n"
		  "\tmov_l_mr((uintptr)&regs.pc_oldp,PC_P);\n"
		  "\tm68k_pc_offset=0;\n"
		  "\tadd_l(15,offs);\n");
	gen_update_next_handler();
	isjump; 
	break;
     case i_LINK:
/*	failure; /* NEW: from "Ipswitch Town" release */
	genamode (curi->smode, "srcreg", sz_long, "src", 1, 0);
	genamode (curi->dmode, "dstreg", curi->size, "offs", 1, 0);
	comprintf("\tsub_l_ri(15,4);\n"
		  "\twritelong_clobber(15,src,scratchie);\n"
		  "\tmov_l_rr(src,15);\n");
	if (curi->size==sz_word)
	    comprintf("\tsign_extend_16_rr(offs,offs);\n");
	comprintf("\tadd_l(15,offs);\n");
	genastore ("src", curi->smode, "srcreg", sz_long, "src");
	break;
     case i_UNLK:
/*	failure; /* NEW: from "Ipswitch Town" release */
	genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
	comprintf("\tmov_l_rr(15,src);\n"
		  "\treadlong(15,src,scratchie);\n"
		  "\tadd_l_ri(15,4);\n");
	genastore ("src", curi->smode, "srcreg", curi->size, "src");
	break;
     case i_RTS:
	comprintf("\tint newad=scratchie++;\n"
		  "\treadlong(15,newad,scratchie);\n"
		  "\tmov_l_mr((uintptr)&regs.pc,newad);\n"
		  "\tget_n_addr_jmp(newad,PC_P,scratchie);\n"
		  "\tmov_l_mr((uintptr)&regs.pc_oldp,PC_P);\n"
		  "\tm68k_pc_offset=0;\n"
		  "\tlea_l_brr(15,15,4);\n");
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
	isjump; 
	genamode (curi->smode, "srcreg", curi->size, "src", 0, 0);
	start_brace();
	comprintf("\tuae_u32 retadd=start_pc+((char *)comp_pc_p-(char *)start_pc_p)+m68k_pc_offset;\n");
	comprintf("\tint ret=scratchie++;\n"
		  "\tmov_l_ri(ret,retadd);\n"
		  "\tsub_l_ri(15,4);\n"
		  "\twritelong_clobber(15,ret,scratchie);\n");
	comprintf("\tmov_l_mr((uintptr)&regs.pc,srca);\n"
		  "\tget_n_addr_jmp(srca,PC_P,scratchie);\n"
		  "\tmov_l_mr((uintptr)&regs.pc_oldp,PC_P);\n"
		  "\tm68k_pc_offset=0;\n");
	gen_update_next_handler();
	break;
     case i_JMP:
	isjump;
	genamode (curi->smode, "srcreg", curi->size, "src", 0, 0);
	comprintf("\tmov_l_mr((uintptr)&regs.pc,srca);\n"
		  "\tget_n_addr_jmp(srca,PC_P,scratchie);\n"
		  "\tmov_l_mr((uintptr)&regs.pc_oldp,PC_P);\n"
		  "\tm68k_pc_offset=0;\n");
	gen_update_next_handler();
	break;
     case i_BSR:
	is_const_jump;
	genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
	start_brace();
	comprintf("\tuae_u32 retadd=start_pc+((char *)comp_pc_p-(char *)start_pc_p)+m68k_pc_offset;\n");
	comprintf("\tint ret=scratchie++;\n"
		  "\tmov_l_ri(ret,retadd);\n"
		  "\tsub_l_ri(15,4);\n"
		  "\twritelong_clobber(15,ret,scratchie);\n");
	comprintf("\tadd_l_ri(src,m68k_pc_offset_thisinst+2);\n");
	comprintf("\tm68k_pc_offset=0;\n");
	comprintf("\tadd_l(PC_P,src);\n");

	comprintf("\tcomp_pc_p=(uae_u8*)get_const(PC_P);\n");
	break;
     case i_Bcc:
	genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
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
	    comprintf("\tuae_u32 v1=get_const(PC_P);\n"
		      "\tuae_u32 v2=get_const(src);\n"
		      "\tregister_branch(v1,v2,%d);\n",
		      cond_codes_x86[curi->cc]);
	    comprintf("\tmake_flags_live();\n"); /* Load the flags */
	    isjump; 
	}
	else {
	    is_const_jump; 
	}

	switch(curi->cc) {
	 case 0:  /* Unconditional jump */
	    comprintf("\tmov_l_rr(PC_P,src);\n"); 
	    comprintf("\tcomp_pc_p=(uae_u8*)get_const(PC_P);\n");
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
	 default: abort();
	}
	break;
     case i_LEA:
	genamode (curi->smode, "srcreg", curi->size, "src", 0, 0);
	genamode (curi->dmode, "dstreg", curi->size, "dst", 2, 0);
	genastore ("srca", curi->dmode, "dstreg", curi->size, "dst");
	break;
     case i_PEA:
	if (table68k[opcode].smode==Areg || 
	    table68k[opcode].smode==Aind || 
	    table68k[opcode].smode==Aipi || 
	    table68k[opcode].smode==Apdi || 
	    table68k[opcode].smode==Ad16 || 
	    table68k[opcode].smode==Ad8r) 
	    comprintf("if (srcreg==7) dodgy=1;\n");

	genamode (curi->smode, "srcreg", curi->size, "src", 0, 0);
	genamode (Apdi, "7", sz_long, "dst", 2, 0);
	genastore ("srca", Apdi, "7", sz_long, "dst");
	break;
     case i_DBcc:
	isjump; 
	uses_cmov;
	genamode (curi->smode, "srcreg", curi->size, "src", 1, 0);
	genamode (curi->dmode, "dstreg", curi->size, "offs", 1, 0);

	/* That offs is an immediate, so we can clobber it with abandon */
	switch(curi->size) {
	 case sz_word: comprintf("\tsign_extend_16_rr(offs,offs);\n"); break;
	 default: abort();  /* Seems this only comes in word flavour */
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

	if (curi->size!=sz_word) 
	    abort();


	switch(curi->cc) {
	 case 0: /* This is an elaborate nop? */
	    break;
	 case 1: 
	    comprintf("\tstart_needflags();\n");
	    comprintf("\tsub_w_ri(src,1);\n");
	    comprintf("\t end_needflags();\n");
	    start_brace();
	    comprintf("\tuae_u32 v1=get_const(PC_P);\n");
	    comprintf("\tuae_u32 v2=get_const(offs);\n"
		      "\tregister_branch(v1,v2,3);\n");
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
		      cond_codes_x86[curi->cc]);
	    comprintf("\tcmov_l_rr(src,nsrc,%d);\n",
		      cond_codes_x86[curi->cc]);
	    /* OK, now for cc=true, we have src==nsrc and offs==PC_P, 
	       so whether we move them around doesn't matter. However,
	       if cc=false, we have offs==jump_pc, and src==nsrc-1 */

	    comprintf("\t start_needflags();\n");
	    comprintf("\ttest_w_rr(nsrc,nsrc);\n"); 
	    comprintf("\t end_needflags();\n");
	    comprintf("\tcmov_l_rr(PC_P,offs,5);\n"); 
	    break;
	 default: abort();
	}
	genastore ("src", curi->smode, "srcreg", curi->size, "src");
	gen_update_next_handler();
	break;

     case i_Scc:
/*	failure; /* NEW: from "Ipswitch Town" release */
	genamode (curi->smode, "srcreg", curi->size, "src", 2, 0);
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
		      cond_codes_x86[curi->cc]^1); break;
	 default: abort();
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
/*	failure; /* NEW: from "Ipswitch Town" release */
	comprintf("\tdont_care_flags();\n");
	genamode (curi->smode, "srcreg", sz_word, "src", 1, 0);
	genamode (curi->dmode, "dstreg", sz_word, "dst", 1, 0);
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
/*	failure; /* NEW: from "Ipswitch Town" release */
	comprintf("\tdont_care_flags();\n");
	genamode (curi->smode, "srcreg", sz_word, "src", 1, 0);
	genamode (curi->dmode, "dstreg", sz_word, "dst", 1, 0);
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
    mayfail;
	if (curi->smode==Dreg) {
	  comprintf("if ((uae_u32)srcreg==(uae_u32)dstreg) {\n"
				"  FAIL(1);\n"
				"  return;\n"
				"} \n");
	  start_brace();
	}
	comprintf("\tdont_care_flags();\n");

	genamode (curi->smode, "srcreg", curi->size, "cnt", 1, 0);
	genamode (curi->dmode, "dstreg", curi->size, "data", 1, 0);
	if (curi->smode!=immi) {
/*		failure; /* UNTESTED: NEW: from "Ipswitch Town" release */
	    if (!noflags) {
		uses_cmov;
		start_brace();
		comprintf("\tint highmask;\n"
			  "\tint cdata=scratchie++;\n"
			  "\tint sdata=scratchie++;\n"
			  "\tint tmpcnt=scratchie++;\n");
		comprintf("\tmov_l_rr(sdata,data);\n"
			  "\tmov_l_rr(cdata,data);\n"
			  "\tmov_l_rr(tmpcnt,cnt);\n");
		switch (curi->size) {
		case sz_byte: comprintf("\tshra_b_ri(sdata,7);\n"); break;
		case sz_word: comprintf("\tshra_w_ri(sdata,15);\n"); break;
		case sz_long: comprintf("\tshra_l_ri(sdata,31);\n"); break;
		default: abort();
		}
		/* sdata is now the MSB propagated to all bits for the
		   register of specified size */
		comprintf("\tand_l_ri(tmpcnt,63);\n");
		switch(curi->size) {
		case sz_byte: comprintf("\tshra_b_rr(data,tmpcnt);\n"
					"\thighmask=0x38;\n");
		    break;
		case sz_word: comprintf("\tshra_w_rr(data,tmpcnt);\n"
					"\thighmask=0x30;\n");
		    break;
		case sz_long: comprintf("\tshra_l_rr(data,tmpcnt);\n"
					"\thighmask=0x20;\n");
		    break;
		}
		comprintf("\ttest_l_ri(tmpcnt,highmask);\n");
		switch (curi->size) {
		case sz_byte: comprintf("\tcmov_b_rr(data,sdata,NATIVE_CC_NE);\n"); break;
		case sz_word: comprintf("\tcmov_w_rr(data,sdata,NATIVE_CC_NE);\n"); break;
		case sz_long: comprintf("\tcmov_l_rr(data,sdata,NATIVE_CC_NE);\n"); break;
		}
		
		/* Result of shift is now in data. Now we need to determine
		   the carry by shifting cdata one less */
		/* NOTE: carry bit is cleared if shift count is zero */
		comprintf("\tmov_l_ri(scratchie,0);\n"
			  "\ttest_l_rr(tmpcnt,tmpcnt);\n"
			  "\tcmov_l_rr(sdata,scratchie,NATIVE_CC_EQ);\n"
			  "\tforget_about(scratchie);\n");
		comprintf("\tsub_l_ri(tmpcnt,1);\n");
		switch(curi->size) {
		 case sz_byte: comprintf("\tshra_b_rr(cdata,tmpcnt);\n");break;
		 case sz_word: comprintf("\tshra_w_rr(cdata,tmpcnt);\n");break;
		 case sz_long: comprintf("\tshra_l_rr(cdata,tmpcnt);\n");break;
		 default: abort();
		}
		/* If the shift count was higher than the width, we need
		   to pick up the sign from original data (sdata) */
		/* NOTE: for shift count of zero, the following holds
		   true and cdata contains 0 so that carry bit is cleared */
		comprintf("\ttest_l_ri(tmpcnt,highmask);\n"
			  "\tforget_about(tmpcnt);\n"
			  "\tcmov_l_rr(cdata,sdata,NATIVE_CC_NE);\n");

		/* And create the flags (preserve X flag if shift count is zero) */
		comprintf("\ttest_l_ri(cnt,63);\n"
			  "\tcmov_l_rr(FLAGX,cdata,NATIVE_CC_NE);\n");
		comprintf("\tstart_needflags();\n");
		comprintf("\tif (needed_flags & FLAG_ZNV)\n");
		switch(curi->size) {
		 case sz_byte: comprintf("\t  test_b_rr(data,data);\n"); break;
		 case sz_word: comprintf("\t  test_w_rr(data,data);\n"); break;
		 case sz_long: comprintf("\t  test_l_rr(data,data);\n"); break;
		}
		comprintf("\t bt_l_ri(cdata,0);\n"); /* Set C */
		comprintf("\t live_flags();\n");
		comprintf("\t end_needflags();\n");
		comprintf("if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
		genastore ("data", curi->dmode, "dstreg", curi->size, "data");
	    }
	    else {
		uses_cmov;
		start_brace();
		comprintf("\tint highmask;\n"
			  "\tint width;\n"
			  "\tint highshift=scratchie++;\n");
		switch(curi->size) {
		 case sz_byte: comprintf("\tshra_b_rr(data,cnt);\n"
					 "\thighmask=0x38;\n"
					 "\twidth=8;\n"); 
		 break;
		 case sz_word: comprintf("\tshra_w_rr(data,cnt);\n"
					 "\thighmask=0x30;\n"
					 "\twidth=16;\n"); 
		 break;
		 case sz_long: comprintf("\tshra_l_rr(data,cnt);\n"
					 "\thighmask=0x20;\n"
					 "\twidth=32;\n"); 
		 break;
		 default: abort();
		}
		comprintf("test_l_ri(cnt,highmask);\n"
			  "mov_l_ri(highshift,0);\n"
			  "mov_l_ri(scratchie,width/2);\n"
			  "cmov_l_rr(highshift,scratchie,5);\n");
		/* The x86 masks out bits, so we now make sure that things
		   really get shifted as much as planned */
		switch(curi->size) {
		 case sz_byte: comprintf("\tshra_b_rr(data,highshift);\n");break;
		 case sz_word: comprintf("\tshra_w_rr(data,highshift);\n");break;
		 case sz_long: comprintf("\tshra_l_rr(data,highshift);\n");break;
		 default: abort();
		}
		/* And again */
		switch(curi->size) {
		 case sz_byte: comprintf("\tshra_b_rr(data,highshift);\n");break;
		 case sz_word: comprintf("\tshra_w_rr(data,highshift);\n");break;
		 case sz_long: comprintf("\tshra_l_rr(data,highshift);\n");break;
		 default: abort();
		}
		genastore ("data", curi->dmode, "dstreg", curi->size, "data");
	    }
	}
	else {
	    start_brace();
	    comprintf("\tint tmp=scratchie++;\n"
		      "\tint bp;\n"
		      "\tmov_l_rr(tmp,data);\n");
	    switch(curi->size) {
	     case sz_byte: comprintf("\tshra_b_ri(data,srcreg);\n"
				     "\tbp=srcreg-1;\n"); break;
	     case sz_word: comprintf("\tshra_w_ri(data,srcreg);\n"
				     "\tbp=srcreg-1;\n"); break;
	     case sz_long: comprintf("\tshra_l_ri(data,srcreg);\n"
				     "\tbp=srcreg-1;\n"); break;
	     default: abort();
	    }

	    if (!noflags) {
		comprintf("\tstart_needflags();\n");
		comprintf("\tif (needed_flags & FLAG_ZNV)\n");
		switch(curi->size) {
		 case sz_byte: comprintf("\t  test_b_rr(data,data);\n"); break;
		 case sz_word: comprintf("\t  test_w_rr(data,data);\n"); break;
		 case sz_long: comprintf("\t  test_l_rr(data,data);\n"); break;
		}
		comprintf("\t bt_l_ri(tmp,bp);\n"); /* Set C */
		comprintf("\t live_flags();\n");
		comprintf("\t end_needflags();\n");
		comprintf("\t duplicate_carry();\n");
		comprintf("if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
	    }
	    genastore ("data", curi->dmode, "dstreg", curi->size, "data");
	}
	break;

     case i_ASL:
/*	failure; /* NEW: from "Ipswitch Town" release */
    mayfail;
	if (curi->smode==Dreg) {
	  comprintf("if ((uae_u32)srcreg==(uae_u32)dstreg) {\n"
				"  FAIL(1);\n"
				"  return;\n"
				"} \n");
	  start_brace();
	}
	comprintf("\tdont_care_flags();\n");
	/* Except for the handling of the V flag, this is identical to
	   LSL. The handling of V is, uhm, unpleasant, so if it's needed,
	   let the normal emulation handle it. Shoulders of giants kinda
	   thing ;-) */
	comprintf("if (needed_flags & FLAG_V) {\n"
		  "  FAIL(1);\n"
		  "  return;\n"
		  "} \n");
	
	genamode (curi->smode, "srcreg", curi->size, "cnt", 1, 0);
	genamode (curi->dmode, "dstreg", curi->size, "data", 1, 0);
	if (curi->smode!=immi) {
	    if (!noflags) {
		uses_cmov;
		start_brace();
		comprintf("\tint highmask;\n"
			  "\tint cdata=scratchie++;\n"
			  "\tint tmpcnt=scratchie++;\n");
		comprintf("\tmov_l_rr(tmpcnt,cnt);\n"
			  "\tand_l_ri(tmpcnt,63);\n"
			  "\tmov_l_ri(cdata,0);\n"
			  "\tcmov_l_rr(cdata,data,5);\n");
		/* cdata is now either data (for shift count!=0) or
		   0 (for shift count==0) */
		switch(curi->size) {
		 case sz_byte: comprintf("\tshll_b_rr(data,cnt);\n"
					 "\thighmask=0x38;\n"); 
		 break;
		 case sz_word: comprintf("\tshll_w_rr(data,cnt);\n"
					 "\thighmask=0x30;\n"); 
		 break;
		 case sz_long: comprintf("\tshll_l_rr(data,cnt);\n"
					 "\thighmask=0x20;\n"); 
		 break;
		 default: abort();
		}
		comprintf("test_l_ri(cnt,highmask);\n"
			  "mov_l_ri(scratchie,0);\n"
			  "cmov_l_rr(scratchie,data,4);\n");
		switch(curi->size) {
		 case sz_byte: comprintf("\tmov_b_rr(data,scratchie);\n");break;
		 case sz_word: comprintf("\tmov_w_rr(data,scratchie);\n");break;
		 case sz_long: comprintf("\tmov_l_rr(data,scratchie);\n");break;
		 default: abort();
		}
		/* Result of shift is now in data. Now we need to determine
		   the carry by shifting cdata one less */
		comprintf("\tsub_l_ri(tmpcnt,1);\n");
		switch(curi->size) {
		 case sz_byte: comprintf("\tshll_b_rr(cdata,tmpcnt);\n");break;
		 case sz_word: comprintf("\tshll_w_rr(cdata,tmpcnt);\n");break;
		 case sz_long: comprintf("\tshll_l_rr(cdata,tmpcnt);\n");break;
		 default: abort();
		}
		comprintf("test_l_ri(tmpcnt,highmask);\n"
			  "mov_l_ri(scratchie,0);\n"
			  "cmov_l_rr(cdata,scratchie,5);\n");
		/* And create the flags */
		comprintf("\tstart_needflags();\n");

		comprintf("\tif (needed_flags & FLAG_ZNV)\n");
		switch(curi->size) {
		 case sz_byte: comprintf("\t  test_b_rr(data,data);\n"); 
		    comprintf("\t bt_l_ri(cdata,7);\n"); break;
		 case sz_word: comprintf("\t  test_w_rr(data,data);\n"); 
		    comprintf("\t bt_l_ri(cdata,15);\n"); break;
		 case sz_long: comprintf("\t  test_l_rr(data,data);\n"); 
		    comprintf("\t bt_l_ri(cdata,31);\n"); break;
		}
		comprintf("\t live_flags();\n");
		comprintf("\t end_needflags();\n");
		comprintf("\t duplicate_carry();\n");
		comprintf("if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
		genastore ("data", curi->dmode, "dstreg", curi->size, "data");
	    }
	    else {
		uses_cmov;
		start_brace();
		comprintf("\tint highmask;\n");
		switch(curi->size) {
		 case sz_byte: comprintf("\tshll_b_rr(data,cnt);\n"
					 "\thighmask=0x38;\n"); 
		    break;
		 case sz_word: comprintf("\tshll_w_rr(data,cnt);\n"
					 "\thighmask=0x30;\n"); 
		    break;
		 case sz_long: comprintf("\tshll_l_rr(data,cnt);\n"
					 "\thighmask=0x20;\n"); 
		    break;
		 default: abort();
		}
		comprintf("test_l_ri(cnt,highmask);\n"
			  "mov_l_ri(scratchie,0);\n"
			  "cmov_l_rr(scratchie,data,4);\n");
		switch(curi->size) {
		 case sz_byte: comprintf("\tmov_b_rr(data,scratchie);\n");break;
		 case sz_word: comprintf("\tmov_w_rr(data,scratchie);\n");break;
		 case sz_long: comprintf("\tmov_l_rr(data,scratchie);\n");break;
		 default: abort();
		}
		genastore ("data", curi->dmode, "dstreg", curi->size, "data");
	    }
	}
	else {
	    start_brace();
	    comprintf("\tint tmp=scratchie++;\n"
		      "\tint bp;\n"
		      "\tmov_l_rr(tmp,data);\n");
	    switch(curi->size) {
	     case sz_byte: comprintf("\tshll_b_ri(data,srcreg);\n"
				     "\tbp=8-srcreg;\n"); break;
	     case sz_word: comprintf("\tshll_w_ri(data,srcreg);\n"
				     "\tbp=16-srcreg;\n"); break;
	     case sz_long: comprintf("\tshll_l_ri(data,srcreg);\n"
				     "\tbp=32-srcreg;\n"); break;
	     default: abort();
	    }

	    if (!noflags) {
		comprintf("\tstart_needflags();\n");
		comprintf("\tif (needed_flags & FLAG_ZNV)\n");
		switch(curi->size) {
		 case sz_byte: comprintf("\t  test_b_rr(data,data);\n"); break;
		 case sz_word: comprintf("\t  test_w_rr(data,data);\n"); break;
		 case sz_long: comprintf("\t  test_l_rr(data,data);\n"); break;
		}
		comprintf("\t bt_l_ri(tmp,bp);\n"); /* Set C */
		comprintf("\t live_flags();\n");
		comprintf("\t end_needflags();\n");
		comprintf("\t duplicate_carry();\n");
		comprintf("if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
	    }
	    genastore ("data", curi->dmode, "dstreg", curi->size, "data");
	}
	break;
    
     case i_LSR:
/*	failure; /* NEW: from "Ipswitch Town" release */
    mayfail;
	if (curi->smode==Dreg) {
	  comprintf("if ((uae_u32)srcreg==(uae_u32)dstreg) {\n"
				"  FAIL(1);\n"
				"  return;\n"
				"} \n");
	  start_brace();
	}
	comprintf("\tdont_care_flags();\n");

	genamode (curi->smode, "srcreg", curi->size, "cnt", 1, 0);
	genamode (curi->dmode, "dstreg", curi->size, "data", 1, 0);
	if (curi->smode!=immi) {
	    if (!noflags) {
		uses_cmov;
		start_brace();
		comprintf("\tint highmask;\n"
			  "\tint cdata=scratchie++;\n"
			  "\tint tmpcnt=scratchie++;\n");
		comprintf("\tmov_l_rr(tmpcnt,cnt);\n"
			  "\tand_l_ri(tmpcnt,63);\n"
			  "\tmov_l_ri(cdata,0);\n"
			  "\tcmov_l_rr(cdata,data,NATIVE_CC_NE);\n");
		/* cdata is now either data (for shift count!=0) or
		   0 (for shift count==0) */
		switch(curi->size) {
		 case sz_byte: comprintf("\tshrl_b_rr(data,tmpcnt);\n"
					 "\thighmask=0x38;\n"); 
		 break;
		 case sz_word: comprintf("\tshrl_w_rr(data,tmpcnt);\n"
					 "\thighmask=0x30;\n"); 
		 break;
		 case sz_long: comprintf("\tshrl_l_rr(data,tmpcnt);\n"
					 "\thighmask=0x20;\n"); 
		 break;
		 default: abort();
		}
		comprintf("\ttest_l_ri(tmpcnt,highmask);\n"
			  "\rmov_l_ri(scratchie,0);\n");
		if (curi->size == sz_long)
		    comprintf("\tcmov_l_rr(data,scratchie,NATIVE_CC_NE);\n");
		else {
		    comprintf("\tcmov_l_rr(scratchie,data,NATIVE_CC_EQ);\n");
		    switch(curi->size) {
		    case sz_byte: comprintf("\tmov_b_rr(data,scratchie);\n");break;
		    case sz_word: comprintf("\tmov_w_rr(data,scratchie);\n");break;
		    default: abort();
		    }
		}
		/* Result of shift is now in data. Now we need to determine
		   the carry by shifting cdata one less */
		comprintf("\tsub_l_ri(tmpcnt,1);\n");
		comprintf("\tshrl_l_rr(cdata,tmpcnt);\n");
		comprintf("\ttest_l_ri(tmpcnt,highmask);\n");
		comprintf("\tforget_about(tmpcnt);\n");
		if (curi->size != sz_long) /* scratchie is still live for LSR.L */
		    comprintf("\tmov_l_ri(scratchie,0);\n");
		comprintf("\tcmov_l_rr(cdata,scratchie,NATIVE_CC_NE);\n");
		comprintf("\tforget_about(scratchie);\n");
		/* And create the flags (preserve X flag if shift count is zero) */
		comprintf("\ttest_l_ri(cnt,63);\n"
			  "\tcmov_l_rr(FLAGX,cdata,NATIVE_CC_NE);\n");
		comprintf("\tstart_needflags();\n");
		comprintf("\tif (needed_flags & FLAG_ZNV)\n");
		switch(curi->size) {
		 case sz_byte: comprintf("\t  test_b_rr(data,data);\n"); break;
		 case sz_word: comprintf("\t  test_w_rr(data,data);\n"); break;
		 case sz_long: comprintf("\t  test_l_rr(data,data);\n"); break;
		}
		comprintf("\t bt_l_ri(cdata,0);\n"); /* Set C */
		comprintf("\t live_flags();\n");
		comprintf("\t end_needflags();\n");
		comprintf("if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
		genastore ("data", curi->dmode, "dstreg", curi->size, "data");
	    }
	    else {
		uses_cmov;
		start_brace();
		comprintf("\tint highmask;\n");
		switch(curi->size) {
		 case sz_byte: comprintf("\tshrl_b_rr(data,cnt);\n"
					 "\thighmask=0x38;\n"); 
		    break;
		 case sz_word: comprintf("\tshrl_w_rr(data,cnt);\n"
					 "\thighmask=0x30;\n"); 
		    break;
		 case sz_long: comprintf("\tshrl_l_rr(data,cnt);\n"
					 "\thighmask=0x20;\n"); 
		    break;
		 default: abort();
		}
		comprintf("test_l_ri(cnt,highmask);\n"
			  "mov_l_ri(scratchie,0);\n"
			  "cmov_l_rr(scratchie,data,4);\n");
		switch(curi->size) {
		 case sz_byte: comprintf("\tmov_b_rr(data,scratchie);\n");break;
		 case sz_word: comprintf("\tmov_w_rr(data,scratchie);\n");break;
		 case sz_long: comprintf("\tmov_l_rr(data,scratchie);\n");break;
		 default: abort();
		}
		genastore ("data", curi->dmode, "dstreg", curi->size, "data");
	    }
	}
	else {
	    start_brace();
	    comprintf("\tint tmp=scratchie++;\n"
		      "\tint bp;\n"
		      "\tmov_l_rr(tmp,data);\n");
	    switch(curi->size) {
	     case sz_byte: comprintf("\tshrl_b_ri(data,srcreg);\n"
				     "\tbp=srcreg-1;\n"); break;
	     case sz_word: comprintf("\tshrl_w_ri(data,srcreg);\n"
				     "\tbp=srcreg-1;\n"); break;
	     case sz_long: comprintf("\tshrl_l_ri(data,srcreg);\n"
				     "\tbp=srcreg-1;\n"); break;
	     default: abort();
	    }

	    if (!noflags) {
		comprintf("\tstart_needflags();\n");
		comprintf("\tif (needed_flags & FLAG_ZNV)\n");
		switch(curi->size) {
		 case sz_byte: comprintf("\t  test_b_rr(data,data);\n"); break;
		 case sz_word: comprintf("\t  test_w_rr(data,data);\n"); break;
		 case sz_long: comprintf("\t  test_l_rr(data,data);\n"); break;
		}
		comprintf("\t bt_l_ri(tmp,bp);\n"); /* Set C */
		comprintf("\t live_flags();\n");
		comprintf("\t end_needflags();\n");
		comprintf("\t duplicate_carry();\n");
		comprintf("if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
	    }
	    genastore ("data", curi->dmode, "dstreg", curi->size, "data");
	}
	break;

     case i_LSL:
    mayfail;
	if (curi->smode==Dreg) {
	  comprintf("if ((uae_u32)srcreg==(uae_u32)dstreg) {\n"
				"  FAIL(1);\n"
				"  return;\n"
				"} \n");
	  start_brace();
	}
	comprintf("\tdont_care_flags();\n");

	genamode (curi->smode, "srcreg", curi->size, "cnt", 1, 0);
	genamode (curi->dmode, "dstreg", curi->size, "data", 1, 0);
	if (curi->smode!=immi) {
/*		failure; /* UNTESTED: NEW: from "Ipswitch Town" release */
	    if (!noflags) {
		uses_cmov;
		start_brace();
		comprintf("\tint highmask;\n"
			  "\tint cdata=scratchie++;\n"
			  "\tint tmpcnt=scratchie++;\n");
		comprintf("\tmov_l_rr(tmpcnt,cnt);\n"
			  "\tand_l_ri(tmpcnt,63);\n"
			  "\tmov_l_ri(cdata,0);\n"
			  "\tcmov_l_rr(cdata,data,NATIVE_CC_NE);\n");
		/* cdata is now either data (for shift count!=0) or
		   0 (for shift count==0) */
		switch(curi->size) {
		 case sz_byte: comprintf("\tshll_b_rr(data,tmpcnt);\n"
					 "\thighmask=0x38;\n"); 
		 break;
		 case sz_word: comprintf("\tshll_w_rr(data,tmpcnt);\n"
					 "\thighmask=0x30;\n"); 
		 break;
		 case sz_long: comprintf("\tshll_l_rr(data,tmpcnt);\n"
					 "\thighmask=0x20;\n"); 
		 break;
		 default: abort();
		}
		comprintf("\ttest_l_ri(tmpcnt,highmask);\n"
			  "\tmov_l_ri(scratchie,0);\n");
		if (curi->size == sz_long)
		    comprintf("\tcmov_l_rr(data,scratchie,NATIVE_CC_NE);\n");
		else {
		    comprintf("\tcmov_l_rr(scratchie,data,NATIVE_CC_EQ);\n");
		    switch(curi->size) {
		    case sz_byte: comprintf("\tmov_b_rr(data,scratchie);\n");break;
		    case sz_word: comprintf("\tmov_w_rr(data,scratchie);\n");break;
		    default: abort();
		    }
		}
		/* Result of shift is now in data. Now we need to determine
		   the carry by shifting cdata one less */
		comprintf("\tsub_l_ri(tmpcnt,1);\n");
		comprintf("\tshll_l_rr(cdata,tmpcnt);\n");
		comprintf("\ttest_l_ri(tmpcnt,highmask);\n");
		comprintf("\tforget_about(tmpcnt);\n");
		if (curi->size != sz_long) /* scratchie is still live for LSL.L */
		    comprintf("\tmov_l_ri(scratchie,0);\n");
		comprintf("\tcmov_l_rr(cdata,scratchie,NATIVE_CC_NE);\n");
		comprintf("\tforget_about(scratchie);\n");
		/* And create the flags (preserve X flag if shift count is zero) */
		switch (curi->size) {
		case sz_byte: comprintf("\tshrl_l_ri(cdata,7);\n"); break;
		case sz_word: comprintf("\tshrl_l_ri(cdata,15);\n"); break;
		case sz_long: comprintf("\tshrl_l_ri(cdata,31);\n"); break;
		}
		comprintf("\ttest_l_ri(cnt,63);\n"
			  "\tcmov_l_rr(FLAGX,cdata,NATIVE_CC_NE);\n");
		comprintf("\tstart_needflags();\n");
		comprintf("\tif (needed_flags & FLAG_ZNV)\n");
		switch(curi->size) {
		case sz_byte: comprintf("\t  test_b_rr(data,data);\n"); break;
		case sz_word: comprintf("\t  test_w_rr(data,data);\n"); break;
		case sz_long: comprintf("\t  test_l_rr(data,data);\n"); break;
		}
		comprintf("\t bt_l_ri(cdata,0);\n");
		comprintf("\t live_flags();\n");
		comprintf("\t end_needflags();\n");
		comprintf("if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
		genastore ("data", curi->dmode, "dstreg", curi->size, "data");
	    }
	    else {
		uses_cmov;
		start_brace();
		comprintf("\tint highmask;\n");
		switch(curi->size) {
		 case sz_byte: comprintf("\tshll_b_rr(data,cnt);\n"
					 "\thighmask=0x38;\n"); 
		    break;
		 case sz_word: comprintf("\tshll_w_rr(data,cnt);\n"
					 "\thighmask=0x30;\n"); 
		    break;
		 case sz_long: comprintf("\tshll_l_rr(data,cnt);\n"
					 "\thighmask=0x20;\n"); 
		    break;
		 default: abort();
		}
		comprintf("test_l_ri(cnt,highmask);\n"
			  "mov_l_ri(scratchie,0);\n"
			  "cmov_l_rr(scratchie,data,4);\n");
		switch(curi->size) {
		 case sz_byte: comprintf("\tmov_b_rr(data,scratchie);\n");break;
		 case sz_word: comprintf("\tmov_w_rr(data,scratchie);\n");break;
		 case sz_long: comprintf("\tmov_l_rr(data,scratchie);\n");break;
		 default: abort();
		}
		genastore ("data", curi->dmode, "dstreg", curi->size, "data");
	    }
	}
	else {
	    start_brace();
	    comprintf("\tint tmp=scratchie++;\n"
		      "\tint bp;\n"
		      "\tmov_l_rr(tmp,data);\n");
	    switch(curi->size) {
	     case sz_byte: comprintf("\tshll_b_ri(data,srcreg);\n"
				     "\tbp=8-srcreg;\n"); break;
	     case sz_word: comprintf("\tshll_w_ri(data,srcreg);\n"
				     "\tbp=16-srcreg;\n"); break;
	     case sz_long: comprintf("\tshll_l_ri(data,srcreg);\n"
				     "\tbp=32-srcreg;\n"); break;
	     default: abort();
	    }

	    if (!noflags) {
		comprintf("\tstart_needflags();\n");
		comprintf("\tif (needed_flags & FLAG_ZNV)\n");
		switch(curi->size) {
		 case sz_byte: comprintf("\t  test_b_rr(data,data);\n"); break;
		 case sz_word: comprintf("\t  test_w_rr(data,data);\n"); break;
		 case sz_long: comprintf("\t  test_l_rr(data,data);\n"); break;
		}
		comprintf("\t bt_l_ri(tmp,bp);\n"); /* Set C */
		comprintf("\t live_flags();\n");
		comprintf("\t end_needflags();\n");
		comprintf("\t duplicate_carry();\n");
		comprintf("if (!(needed_flags & FLAG_CZNV)) dont_care_flags();\n");
	    }
	    genastore ("data", curi->dmode, "dstreg", curi->size, "data");
	}
	break;

     case i_ROL:
    mayfail;
	if (curi->smode==Dreg) {
	  comprintf("if ((uae_u32)srcreg==(uae_u32)dstreg) {\n"
				"  FAIL(1);\n"
				"  return;\n"
				"} \n");
	  start_brace();
	}
	comprintf("\tdont_care_flags();\n");
	genamode (curi->smode, "srcreg", curi->size, "cnt", 1, 0);
	genamode (curi->dmode, "dstreg", curi->size, "data", 1, 0);
	start_brace ();

	switch(curi->size) {
	 case sz_long: comprintf("\t rol_l_rr(data,cnt);\n"); break;
	 case sz_word: comprintf("\t rol_w_rr(data,cnt);\n"); break;
	 case sz_byte: comprintf("\t rol_b_rr(data,cnt);\n"); break;
	}
      
	if (!noflags) {
	    comprintf("\tstart_needflags();\n");
	    comprintf("\tif (needed_flags & FLAG_ZNV)\n");
	    switch(curi->size) {
	     case sz_byte: comprintf("\t  test_b_rr(data,data);\n"); break;
	     case sz_word: comprintf("\t  test_w_rr(data,data);\n"); break;
	     case sz_long: comprintf("\t  test_l_rr(data,data);\n"); break;
	    }
	    comprintf("\t bt_l_ri(data,0x00);\n"); /* Set C */
	    comprintf("\t live_flags();\n");
	    comprintf("\t end_needflags();\n");
	}
	genastore ("data", curi->dmode, "dstreg", curi->size, "data");
	break;

     case i_ROR:
    mayfail;
	if (curi->smode==Dreg) {
	  comprintf("if ((uae_u32)srcreg==(uae_u32)dstreg) {\n"
				"  FAIL(1);\n"
				"  return;\n"
				"} \n");
	  start_brace();
	}
	comprintf("\tdont_care_flags();\n");
	genamode (curi->smode, "srcreg", curi->size, "cnt", 1, 0);
	genamode (curi->dmode, "dstreg", curi->size, "data", 1, 0);
	start_brace ();

	switch(curi->size) {
	 case sz_long: comprintf("\t ror_l_rr(data,cnt);\n"); break;
	 case sz_word: comprintf("\t ror_w_rr(data,cnt);\n"); break;
	 case sz_byte: comprintf("\t ror_b_rr(data,cnt);\n"); break;
	}
      
	if (!noflags) {
	    comprintf("\tstart_needflags();\n");
	    comprintf("\tif (needed_flags & FLAG_ZNV)\n");
	    switch(curi->size) {
	     case sz_byte: comprintf("\t  test_b_rr(data,data);\n"); break;
	     case sz_word: comprintf("\t  test_w_rr(data,data);\n"); break;
	     case sz_long: comprintf("\t  test_l_rr(data,data);\n"); break;
	    }
	    switch(curi->size) {
	     case sz_byte: comprintf("\t bt_l_ri(data,0x07);\n"); break;
	     case sz_word: comprintf("\t bt_l_ri(data,0x0f);\n"); break;
	     case sz_long: comprintf("\t bt_l_ri(data,0x1f);\n"); break;
	    }
	    comprintf("\t live_flags();\n");
	    comprintf("\t end_needflags();\n");
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
/*	failure; /* NEW: from "Ipswitch Town" release */
	if (!noflags) {
	    failure;
	    break;
	}
	comprintf("\tuae_u16 extra=%s;\n",gen_nextiword());
	comprintf("\tint r2=(extra>>12)&7;\n"
		  "\tint tmp=scratchie++;\n");
       
	genamode (curi->dmode, "dstreg", curi->size, "dst", 1, 0);
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
	uses_fpu;
#ifdef USE_JIT_FPU
	mayfail;
	comprintf("\tuae_u16 extra=%s;\n",gen_nextiword());
	swap_opcode();
	comprintf("\tcomp_fpp_opp(opcode,extra);\n");
#else
	failure;
#endif
	break;
     case i_FBcc:
	uses_fpu;
#ifdef USE_JIT_FPU
	isjump;
	uses_cmov;
	mayfail;
	swap_opcode();
	comprintf("\tcomp_fbcc_opp(opcode);\n");
#else
	isjump; 
	failure;
#endif
	break;
     case i_FDBcc:
	uses_fpu;
	isjump; 
	failure;
	break;
     case i_FScc:
	uses_fpu;
#ifdef USE_JIT_FPU
	mayfail;
	uses_cmov;
	comprintf("\tuae_u16 extra=%s;\n",gen_nextiword());
	swap_opcode();
	comprintf("\tcomp_fscc_opp(opcode,extra);\n");
#else
	failure;
#endif
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
	genmov16(opcode, curi);
	break;

	case i_EMULOP_RETURN:
	isjump;
	failure;
	break;
	
	case i_EMULOP:
	failure;
	break;
	
     case i_MMUOP:
	isjump; 
	failure;
	break;
     default:
	abort ();
	break;
    }
    comprintf("%s",endstr);
    finish_braces ();
    sync_m68k_pc ();
    if (global_mayfail)
	comprintf("\tif (failure)  m68k_pc_offset=m68k_pc_offset_thisinst;\n");
    return global_failure;
}

static void 
generate_includes (FILE * f)
{
    fprintf (f, "#include \"sysdeps.h\"\n");
	fprintf (f, "#include \"m68k.h\"\n");
    fprintf (f, "#include \"memory.h\"\n");
    fprintf (f, "#include \"readcpu.h\"\n");
    fprintf (f, "#include \"newcpu.h\"\n");
    fprintf (f, "#include \"comptbl.h\"\n");
}

static int postfix;

static void 
generate_one_opcode (int rp, int noflags)
{
    uae_u16 smsk, dmsk;
    const long int opcode = opcode_map[rp];
    const char *opcode_str;
    int aborted=0;
    int have_srcreg=0;
    int have_dstreg=0;

    if (table68k[opcode].mnemo == i_ILLG
	|| table68k[opcode].clev > cpu_level)
	return;

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
	 case 6:
	smsk = 255;
	break;
	 case 7:
	smsk = 3;
	break;
     default:
	abort ();
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
	  
	    comprintf ("#ifdef HAVE_GET_WORD_UNSWAPPED\n");
		
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
		
		comprintf ("#endif\n");
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
	  
	    comprintf ("#ifdef HAVE_GET_WORD_UNSWAPPED\n");
		
	    if (pos < 8 && (dmsk >> (8 - pos)) != 0)
		comprintf ("\tuae_u32 dstreg = ((opcode >> %d) | (opcode << %d)) & %d;\n",
			pos ^ 8, 8 - pos, dmsk);
	    else if (pos != 8)
		comprintf ("\tuae_u32 dstreg = (opcode >> %d) & %d;\n",
			pos ^ 8, dmsk);
	    else
		comprintf ("\tuae_u32 dstreg = opcode & %d;\n", dmsk);
		
		comprintf ("#else\n");
		
	    if (pos)
		comprintf ("\tuae_u32 dstreg = (opcode >> %d) & %d;\n",
			   pos, dmsk);
	    else
		comprintf ("\tuae_u32 dstreg = opcode & %d;\n", dmsk);
		
		comprintf ("#endif\n");
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

    opcode_str = get_instruction_string (opcode);

    aborted=gen_opcode (opcode);
    {
	int flags=0;
	if (global_isjump)	flags|=1;
	if (long_opcode)	flags|=2;
	if (global_cmov)	flags|=4;
	if (global_isaddx)	flags|=8;
	if (global_iscjump)	flags|=16;
	if (global_fpu)		flags|=32;
	
	comprintf ("}\n");
    
	if (aborted) {
	    fprintf (stblfile, "{ NULL, 0x%08x, %ld }, /* %s */\n", flags, opcode, opcode_str);
	    com_discard();
	}
	else {
	    if (noflags) {
		fprintf (stblfile, "{ op_%lx_%d_comp_nf, 0x%08x, %ld }, /* %s */\n", opcode, postfix, flags, opcode, opcode_str);
		fprintf (headerfile, "extern compop_func op_%lx_%d_comp_nf;\n", opcode, postfix);
		printf ("void REGPARAM2 op_%lx_%d_comp_nf(uae_u32 opcode) /* %s */\n{\n", opcode, postfix, opcode_str);
	    }
	    else {
		fprintf (stblfile, "{ op_%lx_%d_comp_ff, 0x%08x, %ld }, /* %s */\n", opcode, postfix, flags, opcode, opcode_str);
		fprintf (headerfile, "extern compop_func op_%lx_%d_comp_ff;\n", opcode, postfix);
		printf ("void REGPARAM2 op_%lx_%d_comp_ff(uae_u32 opcode) /* %s */\n{\n", opcode, postfix, opcode_str);
	    }
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

    using_prefetch = 0;
    using_exception_3 = 0;
    for (i = 0; i < 1; i++) /* We only do one level! */
    {
	cpu_level = 4 - i;
	postfix = i;

	if (noflags)
	    fprintf (stblfile, "struct comptbl op_smalltbl_%d_comp_nf[] = {\n", postfix);
	else
	    fprintf (stblfile, "struct comptbl op_smalltbl_%d_comp_ff[] = {\n", postfix);


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

	rp = 0;
	for (j = 1; j <= 8; ++j)
	{
	    int k = (j * nr_cpuop_funcs) / 8;
	    printf ("#ifdef PART_%d\n", j);
	    for (; rp < k; rp++)
		generate_one_opcode (rp,noflags);
	    printf ("#endif\n\n");
	}

	fprintf (stblfile, "{ 0, 0,65536 }};\n");
    }

}

int 
main (int argc, char **argv)
{
    read_table68k ();
    do_merges ();

    opcode_map = (int *) malloc (sizeof (int) * nr_cpuop_funcs);
    opcode_last_postfix = (int *) malloc (sizeof (int) * nr_cpuop_funcs);
    opcode_next_clev = (int *) malloc (sizeof (int) * nr_cpuop_funcs);
    counts = (unsigned long *) malloc (65536 * sizeof (unsigned long));
    read_counts ();

    /* It would be a lot nicer to put all in one file (we'd also get rid of
     * cputbl.h that way), but cpuopti can't cope.  That could be fixed, but
     * I don't dare to touch the 68k version.  */

    headerfile = fopen ("comptbl.h", "wb");
    stblfile = fopen ("compstbl.cpp", "wb");
    freopen ("compemu.cpp", "wb", stdout);

    generate_includes (stdout);
    generate_includes (stblfile);

    printf("#include \"compiler/compemu.h\"\n");

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

    free(opcode_map);
    free(opcode_last_postfix);
    free(opcode_next_clev);
    free(counts);

    free (table68k);
    fclose (stblfile);
    fclose (headerfile);
    fflush (stdout);
    return 0;
}
