/*
 * UAE - The Un*x Amiga Emulator
 *
 * MC68000 emulation
 *
 * (c) 1995 Bernd Schmidt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sysdeps.h"

#include "cpu_emulation.h"
#include "main.h"
#include "emul_op.h"

extern int intlev(void);	// From baisilisk_glue.cpp

#include "m68k.h"
#include "memory.h"
#include "readcpu.h"
#include "newcpu.h"
#include "compiler/compemu.h"
#include "fpu/fpu.h"

#if defined(ENABLE_EXCLUSIVE_SPCFLAGS) && !defined(HAVE_HARDWARE_LOCKS)
B2_mutex *spcflags_lock = NULL;
#endif

bool quit_program = false;
struct flag_struct regflags;

/* Opcode of faulting instruction */
uae_u16 last_op_for_exception_3;
/* PC at fault time */
uaecptr last_addr_for_exception_3;
/* Address that generated the exception */
uaecptr last_fault_for_exception_3;

int areg_byteinc[] = { 1,1,1,1,1,1,1,2 };
int imm8_table[] = { 8,1,2,3,4,5,6,7 };

int movem_index1[256];
int movem_index2[256];
int movem_next[256];

cpuop_func *cpufunctbl[65536];

#if FLIGHT_RECORDER
struct rec_step {
	uae_u32 pc;
#if FLIGHT_RECORDER >= 2
	uae_u32 d[8];
	uae_u32 a[8];
#endif
};

const int LOG_SIZE = 32768;
static rec_step log[LOG_SIZE];
static int log_ptr = -1; // First time initialization

static const char *log_filename(void)
{
	const char *name = getenv("M68K_LOG_FILE");
	return name ? name : "log.68k";
}

void m68k_record_step(uaecptr pc)
{
#if FLIGHT_RECORDER >= 2
	/* XXX: if LSB is set, we are recording from generated code and we
	   don't support registers recording yet.  */
	if ((pc & 1) == 0) {
		for (int i = 0; i < 8; i++) {
			log[log_ptr].d[i] = m68k_dreg(regs, i);
			log[log_ptr].a[i] = m68k_areg(regs, i);
		}
	}
#endif
	log[log_ptr].pc = pc;
	log_ptr = (log_ptr + 1) % LOG_SIZE;
}

static void dump_log(void)
{
	FILE *f = fopen(log_filename(), "w");
	if (f == NULL)
		return;
	for (int i = 0; i < LOG_SIZE; i++) {
		int j = (i + log_ptr) % LOG_SIZE;
		uae_u32 pc = log[j].pc & ~1;
		fprintf(f, "pc %08x", pc);
#if FLIGHT_RECORDER >= 2
		fprintf(f, "\n");
		if ((log[j].pc & 1) == 0) {
			fprintf(f, "d0 %08x d1 %08x d2 %08x d3 %08x\n", log[j].d[0], log[j].d[1], log[j].d[2], log[j].d[3]);
			fprintf(f, "d4 %08x d5 %08x d6 %08x d7 %08x\n", log[j].d[4], log[j].d[5], log[j].d[6], log[j].d[7]);
			fprintf(f, "a0 %08x a1 %08x a2 %08x a3 %08x\n", log[j].a[0], log[j].a[1], log[j].a[2], log[j].a[3]);
			fprintf(f, "a4 %08x a5 %08x a6 %08x a7 %08x\n", log[j].a[4], log[j].a[5], log[j].a[6], log[j].a[7]);
		}
#else
		fprintf(f, " | ");
#endif
#if ENABLE_MON
		disass_68k(f, pc);
#endif
	}
	fclose(f);
}
#endif

#if ENABLE_MON
static void dump_regs(void)
{
	m68k_dumpstate(NULL);
}
#endif

#define COUNT_INSTRS 0

#if COUNT_INSTRS
static unsigned long int instrcount[65536];
static uae_u16 opcodenums[65536];

static int compfn (const void *el1, const void *el2)
{
	return instrcount[*(const uae_u16 *)el1] < instrcount[*(const uae_u16 *)el2];
}

static char *icountfilename (void)
{
	char *name = getenv ("INSNCOUNT");
	if (name)
		return name;
	return COUNT_INSTRS == 2 ? "frequent.68k" : "insncount";
}

void dump_counts (void)
{
	FILE *f = fopen (icountfilename (), "w");
	unsigned long int total;
	int i;

	write_log ("Writing instruction count file...\n");
	for (i = 0; i < 65536; i++) {
		opcodenums[i] = i;
		total += instrcount[i];
	}
	qsort (opcodenums, 65536, sizeof(uae_u16), compfn);

	fprintf (f, "Total: %lu\n", total);
	for (i=0; i < 65536; i++) {
		unsigned long int cnt = instrcount[opcodenums[i]];
		struct instr *dp;
		struct mnemolookup *lookup;
		if (!cnt)
			break;
		dp = table68k + opcodenums[i];
		for (lookup = lookuptab;lookup->mnemo != dp->mnemo; lookup++)
			;
		fprintf (f, "%04x: %lu %s\n", opcodenums[i], cnt, lookup->name);
	}
	fclose (f);
}
#else
void dump_counts (void)
{
}
#endif

int broken_in;

static __inline__ unsigned int cft_map (unsigned int f)
{
#ifndef HAVE_GET_WORD_UNSWAPPED
	return f;
#else
	return ((f >> 8) & 255) | ((f & 255) << 8);
#endif
}

void REGPARAM2 op_illg_1 (uae_u32 opcode) REGPARAM;

void REGPARAM2 op_illg_1 (uae_u32 opcode)
{
	op_illg (cft_map (opcode));
}

static void build_cpufunctbl (void)
{
	int i;
	unsigned long opcode;
	unsigned int cpu_level = 0;		// 68000 (default)
	if (CPUType == 4)
		cpu_level = 4;		// 68040 with FPU
	else {
		if (FPUType)
			cpu_level = 3;	// 68020 with FPU
		else if (CPUType >= 2)
			cpu_level = 2;	// 68020
		else if (CPUType == 1)
			cpu_level = 1;
	}
	struct cputbl *tbl = (
				cpu_level == 4 ? op_smalltbl_0_ff
				: cpu_level == 3 ? op_smalltbl_1_ff
				: cpu_level == 2 ? op_smalltbl_2_ff
				: cpu_level == 1 ? op_smalltbl_3_ff
				: op_smalltbl_4_ff);

	for (opcode = 0; opcode < 65536; opcode++)
		cpufunctbl[cft_map (opcode)] = op_illg_1;
	for (i = 0; tbl[i].handler != NULL; i++) {
		if (! tbl[i].specific)
			cpufunctbl[cft_map (tbl[i].opcode)] = tbl[i].handler;
	}
	for (opcode = 0; opcode < 65536; opcode++) {
		cpuop_func *f;

		if (table68k[opcode].mnemo == i_ILLG || table68k[opcode].clev > cpu_level)
			continue;

		if (table68k[opcode].handler != -1) {
			f = cpufunctbl[cft_map (table68k[opcode].handler)];
			if (f == op_illg_1)
				abort();
			cpufunctbl[cft_map (opcode)] = f;
		}
	}
	for (i = 0; tbl[i].handler != NULL; i++) {
		if (tbl[i].specific)
			cpufunctbl[cft_map (tbl[i].opcode)] = tbl[i].handler;
	}
}

void init_m68k (void)
{
	int i;

	for (i = 0 ; i < 256 ; i++) {
		int j;
		for (j = 0 ; j < 8 ; j++) {
			if (i & (1 << j)) break;
		}
		movem_index1[i] = j;
		movem_index2[i] = 7-j;
		movem_next[i] = i & (~(1 << j));
	}
#if COUNT_INSTRS
	{
		FILE *f = fopen (icountfilename (), "r");
		memset (instrcount, 0, sizeof instrcount);
		if (f) {
			uae_u32 opcode, count, total;
			char name[20];
			write_log ("Reading instruction count file...\n");
			fscanf (f, "Total: %lu\n", &total);
			while (fscanf (f, "%lx: %lu %s\n", &opcode, &count, name) == 3) {
				instrcount[opcode] = count;
			}
			fclose(f);
		}
	}
#endif
	read_table68k ();
	do_merges ();

	build_cpufunctbl ();

#if defined(ENABLE_EXCLUSIVE_SPCFLAGS) && !defined(HAVE_HARDWARE_LOCKS)
	spcflags_lock = B2_create_mutex();
#endif
	fpu_init(CPUType == 4);
}

void exit_m68k (void)
{
	fpu_exit ();
#if defined(ENABLE_EXCLUSIVE_SPCFLAGS) && !defined(HAVE_HARDWARE_LOCKS)
	B2_delete_mutex(spcflags_lock);
#endif
}

struct regstruct regs, lastint_regs;
static struct regstruct regs_backup[16];
static int backup_pointer = 0;
static long int m68kpc_offset;
int lastint_no;

#if REAL_ADDRESSING || DIRECT_ADDRESSING
#define get_ibyte_1(o) get_byte(get_virtual_address(regs.pc_p) + (o) + 1)
#define get_iword_1(o) get_word(get_virtual_address(regs.pc_p) + (o))
#define get_ilong_1(o) get_long(get_virtual_address(regs.pc_p) + (o))
#else
#define get_ibyte_1(o) get_byte(regs.pc + (regs.pc_p - regs.pc_oldp) + (o) + 1)
#define get_iword_1(o) get_word(regs.pc + (regs.pc_p - regs.pc_oldp) + (o))
#define get_ilong_1(o) get_long(regs.pc + (regs.pc_p - regs.pc_oldp) + (o))
#endif

uae_s32 ShowEA (int reg, amodes mode, wordsizes size, char *buf)
{
	uae_u16 dp;
	uae_s8 disp8;
	uae_s16 disp16;
	int r;
	uae_u32 dispreg;
	uaecptr addr;
	uae_s32 offset = 0;
	char buffer[80];

	switch (mode){
	case Dreg:
		sprintf (buffer,"D%d", reg);
		break;
	case Areg:
		sprintf (buffer,"A%d", reg);
		break;
	case Aind:
		sprintf (buffer,"(A%d)", reg);
		break;
	case Aipi:
		sprintf (buffer,"(A%d)+", reg);
		break;
	case Apdi:
		sprintf (buffer,"-(A%d)", reg);
		break;
	case Ad16:
		disp16 = get_iword_1 (m68kpc_offset); m68kpc_offset += 2;
		addr = m68k_areg(regs,reg) + (uae_s16)disp16;
		sprintf (buffer,"(A%d,$%04x) == $%08lx", reg, disp16 & 0xffff,
				 (unsigned long)addr);
		break;
	case Ad8r:
		dp = get_iword_1 (m68kpc_offset); m68kpc_offset += 2;
		disp8 = dp & 0xFF;
		r = (dp & 0x7000) >> 12;
		dispreg = dp & 0x8000 ? m68k_areg(regs,r) : m68k_dreg(regs,r);
		if (!(dp & 0x800)) dispreg = (uae_s32)(uae_s16)(dispreg);
		dispreg <<= (dp >> 9) & 3;

		if (dp & 0x100) {
			uae_s32 outer = 0, disp = 0;
			uae_s32 base = m68k_areg(regs,reg);
			char name[10];
			sprintf (name,"A%d, ",reg);
			if (dp & 0x80) { base = 0; name[0] = 0; }
			if (dp & 0x40) dispreg = 0;
			if ((dp & 0x30) == 0x20) { disp = (uae_s32)(uae_s16)get_iword_1 (m68kpc_offset); m68kpc_offset += 2; }
			if ((dp & 0x30) == 0x30) { disp = get_ilong_1 (m68kpc_offset); m68kpc_offset += 4; }
			base += disp;

			if ((dp & 0x3) == 0x2) { outer = (uae_s32)(uae_s16)get_iword_1 (m68kpc_offset); m68kpc_offset += 2; }
			if ((dp & 0x3) == 0x3) { outer = get_ilong_1 (m68kpc_offset); m68kpc_offset += 4; }

			if (!(dp & 4)) base += dispreg;
			if (dp & 3) base = get_long (base);
			if (dp & 4) base += dispreg;

			addr = base + outer;
			sprintf (buffer,"(%s%c%d.%c*%d+%d)+%d == $%08lx", name,
					 dp & 0x8000 ? 'A' : 'D', (int)r, dp & 0x800 ? 'L' : 'W',
					 1 << ((dp >> 9) & 3),
					 disp,outer,
					 (unsigned long)addr);
		} else {
			addr = m68k_areg(regs,reg) + (uae_s32)((uae_s8)disp8) + dispreg;
			sprintf (buffer,"(A%d, %c%d.%c*%d, $%02x) == $%08lx", reg,
					 dp & 0x8000 ? 'A' : 'D', (int)r, dp & 0x800 ? 'L' : 'W',
					 1 << ((dp >> 9) & 3), disp8,
					 (unsigned long)addr);
		}
		break;
	case PC16:
		addr = m68k_getpc () + m68kpc_offset;
		disp16 = get_iword_1 (m68kpc_offset); m68kpc_offset += 2;
		addr += (uae_s16)disp16;
		sprintf (buffer,"(PC,$%04x) == $%08lx", disp16 & 0xffff,(unsigned long)addr);
		break;
	case PC8r:
		addr = m68k_getpc () + m68kpc_offset;
		dp = get_iword_1 (m68kpc_offset); m68kpc_offset += 2;
		disp8 = dp & 0xFF;
		r = (dp & 0x7000) >> 12;
		dispreg = dp & 0x8000 ? m68k_areg(regs,r) : m68k_dreg(regs,r);
		if (!(dp & 0x800)) dispreg = (uae_s32)(uae_s16)(dispreg);
		dispreg <<= (dp >> 9) & 3;

		if (dp & 0x100) {
			uae_s32 outer = 0,disp = 0;
			uae_s32 base = addr;
			char name[10];
			sprintf (name,"PC, ");
			if (dp & 0x80) { base = 0; name[0] = 0; }
			if (dp & 0x40) dispreg = 0;
			if ((dp & 0x30) == 0x20) { disp = (uae_s32)(uae_s16)get_iword_1 (m68kpc_offset); m68kpc_offset += 2; }
			if ((dp & 0x30) == 0x30) { disp = get_ilong_1 (m68kpc_offset); m68kpc_offset += 4; }
			base += disp;

			if ((dp & 0x3) == 0x2) { outer = (uae_s32)(uae_s16)get_iword_1 (m68kpc_offset); m68kpc_offset += 2; }
			if ((dp & 0x3) == 0x3) { outer = get_ilong_1 (m68kpc_offset); m68kpc_offset += 4; }

			if (!(dp & 4)) base += dispreg;
			if (dp & 3) base = get_long (base);
			if (dp & 4) base += dispreg;

			addr = base + outer;
			sprintf (buffer,"(%s%c%d.%c*%d+%d)+%d == $%08lx", name,
					 dp & 0x8000 ? 'A' : 'D', (int)r, dp & 0x800 ? 'L' : 'W',
					 1 << ((dp >> 9) & 3),
					 disp,outer,
					 (unsigned long)addr);
		} else {
			addr += (uae_s32)((uae_s8)disp8) + dispreg;
			sprintf (buffer,"(PC, %c%d.%c*%d, $%02x) == $%08lx", dp & 0x8000 ? 'A' : 'D',
					 (int)r, dp & 0x800 ? 'L' : 'W',  1 << ((dp >> 9) & 3),
					 disp8, (unsigned long)addr);
		}
		break;
	case absw:
		sprintf (buffer,"$%08lx", (unsigned long)(uae_s32)(uae_s16)get_iword_1 (m68kpc_offset));
		m68kpc_offset += 2;
		break;
	case absl:
		sprintf (buffer,"$%08lx", (unsigned long)get_ilong_1 (m68kpc_offset));
		m68kpc_offset += 4;
		break;
	case imm:
		switch (size){
		case sz_byte:
			sprintf (buffer,"#$%02x", (unsigned int)(get_iword_1 (m68kpc_offset) & 0xff));
			m68kpc_offset += 2;
			break;
		case sz_word:
			sprintf (buffer,"#$%04x", (unsigned int)(get_iword_1 (m68kpc_offset) & 0xffff));
			m68kpc_offset += 2;
			break;
		case sz_long:
			sprintf (buffer,"#$%08lx", (unsigned long)(get_ilong_1 (m68kpc_offset)));
			m68kpc_offset += 4;
			break;
		default:
			break;
		}
		break;
	case imm0:
		offset = (uae_s32)(uae_s8)get_iword_1 (m68kpc_offset);
		m68kpc_offset += 2;
		sprintf (buffer,"#$%02x", (unsigned int)(offset & 0xff));
		break;
	case imm1:
		offset = (uae_s32)(uae_s16)get_iword_1 (m68kpc_offset);
		m68kpc_offset += 2;
		sprintf (buffer,"#$%04x", (unsigned int)(offset & 0xffff));
		break;
	case imm2:
		offset = (uae_s32)get_ilong_1 (m68kpc_offset);
		m68kpc_offset += 4;
		sprintf (buffer,"#$%08lx", (unsigned long)offset);
		break;
	case immi:
		offset = (uae_s32)(uae_s8)(reg & 0xff);
		sprintf (buffer,"#$%08lx", (unsigned long)offset);
		break;
	default:
		break;
	}
	if (buf == 0)
		printf ("%s", buffer);
	else
		strcat (buf, buffer);
	return offset;
}

/* The plan is that this will take over the job of exception 3 handling -
 * the CPU emulation functions will just do a longjmp to m68k_go whenever
 * they hit an odd address. */
static int verify_ea (int reg, amodes mode, wordsizes size, uae_u32 *val)
{
	uae_u16 dp;
	uae_s8 disp8;
	uae_s16 disp16;
	int r;
	uae_u32 dispreg;
	uaecptr addr;
	uae_s32 offset = 0;

	switch (mode){
	case Dreg:
		*val = m68k_dreg (regs, reg);
		return 1;
	case Areg:
		*val = m68k_areg (regs, reg);
		return 1;

	case Aind:
	case Aipi:
		addr = m68k_areg (regs, reg);
		break;
	case Apdi:
		addr = m68k_areg (regs, reg);
		break;
	case Ad16:
		disp16 = get_iword_1 (m68kpc_offset); m68kpc_offset += 2;
		addr = m68k_areg(regs,reg) + (uae_s16)disp16;
		break;
	case Ad8r:
		addr = m68k_areg (regs, reg);
d8r_common:
		dp = get_iword_1 (m68kpc_offset); m68kpc_offset += 2;
		disp8 = dp & 0xFF;
		r = (dp & 0x7000) >> 12;
		dispreg = dp & 0x8000 ? m68k_areg(regs,r) : m68k_dreg(regs,r);
		if (!(dp & 0x800)) dispreg = (uae_s32)(uae_s16)(dispreg);
		dispreg <<= (dp >> 9) & 3;

		if (dp & 0x100) {
			uae_s32 outer = 0, disp = 0;
			uae_s32 base = addr;
			if (dp & 0x80) base = 0;
			if (dp & 0x40) dispreg = 0;
			if ((dp & 0x30) == 0x20) { disp = (uae_s32)(uae_s16)get_iword_1 (m68kpc_offset); m68kpc_offset += 2; }
			if ((dp & 0x30) == 0x30) { disp = get_ilong_1 (m68kpc_offset); m68kpc_offset += 4; }
			base += disp;

			if ((dp & 0x3) == 0x2) { outer = (uae_s32)(uae_s16)get_iword_1 (m68kpc_offset); m68kpc_offset += 2; }
			if ((dp & 0x3) == 0x3) { outer = get_ilong_1 (m68kpc_offset); m68kpc_offset += 4; }

			if (!(dp & 4)) base += dispreg;
			if (dp & 3) base = get_long (base);
			if (dp & 4) base += dispreg;

			addr = base + outer;
		} else {
			addr += (uae_s32)((uae_s8)disp8) + dispreg;
		}
		break;
	case PC16:
		addr = m68k_getpc () + m68kpc_offset;
		disp16 = get_iword_1 (m68kpc_offset); m68kpc_offset += 2;
		addr += (uae_s16)disp16;
		break;
	case PC8r:
		addr = m68k_getpc () + m68kpc_offset;
		goto d8r_common;
	case absw:
		addr = (uae_s32)(uae_s16)get_iword_1 (m68kpc_offset);
		m68kpc_offset += 2;
		break;
	case absl:
		addr = get_ilong_1 (m68kpc_offset);
		m68kpc_offset += 4;
		break;
	case imm:
		switch (size){
		case sz_byte:
			*val = get_iword_1 (m68kpc_offset) & 0xff;
			m68kpc_offset += 2;
			break;
		case sz_word:
			*val = get_iword_1 (m68kpc_offset) & 0xffff;
			m68kpc_offset += 2;
			break;
		case sz_long:
			*val = get_ilong_1 (m68kpc_offset);
			m68kpc_offset += 4;
			break;
		default:
			break;
		}
		return 1;
	case imm0:
		*val = (uae_s32)(uae_s8)get_iword_1 (m68kpc_offset);
		m68kpc_offset += 2;
		return 1;
	case imm1:
		*val = (uae_s32)(uae_s16)get_iword_1 (m68kpc_offset);
		m68kpc_offset += 2;
		return 1;
	case imm2:
		*val = get_ilong_1 (m68kpc_offset);
		m68kpc_offset += 4;
		return 1;
	case immi:
		*val = (uae_s32)(uae_s8)(reg & 0xff);
		return 1;
	default:
		addr = 0;
		break;
	}
	if ((addr & 1) == 0)
		return 1;

	last_addr_for_exception_3 = m68k_getpc () + m68kpc_offset;
	last_fault_for_exception_3 = addr;
	return 0;
}

uae_u32 get_disp_ea_020 (uae_u32 base, uae_u32 dp)
{
	int reg = (dp >> 12) & 15;
	uae_s32 regd = regs.regs[reg];
	if ((dp & 0x800) == 0)
		regd = (uae_s32)(uae_s16)regd;
	regd <<= (dp >> 9) & 3;
	if (dp & 0x100) {
		uae_s32 outer = 0;
		if (dp & 0x80) base = 0;
		if (dp & 0x40) regd = 0;

		if ((dp & 0x30) == 0x20) base += (uae_s32)(uae_s16)next_iword();
		if ((dp & 0x30) == 0x30) base += next_ilong();

		if ((dp & 0x3) == 0x2) outer = (uae_s32)(uae_s16)next_iword();
		if ((dp & 0x3) == 0x3) outer = next_ilong();

		if ((dp & 0x4) == 0) base += regd;
		if (dp & 0x3) base = get_long (base);
		if (dp & 0x4) base += regd;

		return base + outer;
	} else {
		return base + (uae_s32)((uae_s8)dp) + regd;
	}
}

uae_u32 get_disp_ea_000 (uae_u32 base, uae_u32 dp)
{
	int reg = (dp >> 12) & 15;
	uae_s32 regd = regs.regs[reg];
#if 1
	if ((dp & 0x800) == 0)
		regd = (uae_s32)(uae_s16)regd;
	return base + (uae_s8)dp + regd;
#else
	/* Branch-free code... benchmark this again now that
	 * things are no longer inline.  */
	uae_s32 regd16;
	uae_u32 mask;
	mask = ((dp & 0x800) >> 11) - 1;
	regd16 = (uae_s32)(uae_s16)regd;
	regd16 &= mask;
	mask = ~mask;
	base += (uae_s8)dp;
	regd &= mask;
	regd |= regd16;
	return base + regd;
#endif
}

void MakeSR (void)
{
#if 0
	assert((regs.t1 & 1) == regs.t1);
	assert((regs.t0 & 1) == regs.t0);
	assert((regs.s & 1) == regs.s);
	assert((regs.m & 1) == regs.m);
	assert((XFLG & 1) == XFLG);
	assert((NFLG & 1) == NFLG);
	assert((ZFLG & 1) == ZFLG);
	assert((VFLG & 1) == VFLG);
	assert((CFLG & 1) == CFLG);
#endif
	regs.sr = ((regs.t1 << 15) | (regs.t0 << 14)
			   | (regs.s << 13) | (regs.m << 12) | (regs.intmask << 8)
			   | (GET_XFLG << 4) | (GET_NFLG << 3) | (GET_ZFLG << 2) | (GET_VFLG << 1)
			   | GET_CFLG);
}

void MakeFromSR (void)
{
	int oldm = regs.m;
	int olds = regs.s;

	regs.t1 = (regs.sr >> 15) & 1;
	regs.t0 = (regs.sr >> 14) & 1;
	regs.s = (regs.sr >> 13) & 1;
	regs.m = (regs.sr >> 12) & 1;
	regs.intmask = (regs.sr >> 8) & 7;
	SET_XFLG ((regs.sr >> 4) & 1);
	SET_NFLG ((regs.sr >> 3) & 1);
	SET_ZFLG ((regs.sr >> 2) & 1);
	SET_VFLG ((regs.sr >> 1) & 1);
	SET_CFLG (regs.sr & 1);
	if (CPUType >= 2) {
		if (olds != regs.s) {
			if (olds) {
				if (oldm)
					regs.msp = m68k_areg(regs, 7);
				else
					regs.isp = m68k_areg(regs, 7);
				m68k_areg(regs, 7) = regs.usp;
			} else {
				regs.usp = m68k_areg(regs, 7);
				m68k_areg(regs, 7) = regs.m ? regs.msp : regs.isp;
			}
		} else if (olds && oldm != regs.m) {
			if (oldm) {
				regs.msp = m68k_areg(regs, 7);
				m68k_areg(regs, 7) = regs.isp;
			} else {
				regs.isp = m68k_areg(regs, 7);
				m68k_areg(regs, 7) = regs.msp;
			}
		}
	} else {
		if (olds != regs.s) {
			if (olds) {
				regs.isp = m68k_areg(regs, 7);
				m68k_areg(regs, 7) = regs.usp;
			} else {
				regs.usp = m68k_areg(regs, 7);
				m68k_areg(regs, 7) = regs.isp;
			}
		}
	}

	SPCFLAGS_SET( SPCFLAG_INT );
	if (regs.t1 || regs.t0)
		SPCFLAGS_SET( SPCFLAG_TRACE );
	else
		/* Keep SPCFLAG_DOTRACE, we still want a trace exception for
	   SR-modifying instructions (including STOP).  */
		SPCFLAGS_CLEAR( SPCFLAG_TRACE );
}

void Exception(int nr, uaecptr oldpc)
{
	uae_u32 currpc = m68k_getpc ();
	MakeSR();
	if (!regs.s) {
		regs.usp = m68k_areg(regs, 7);
		if (CPUType >= 2)
			m68k_areg(regs, 7) = regs.m ? regs.msp : regs.isp;
		else
			m68k_areg(regs, 7) = regs.isp;
		regs.s = 1;
	}
	if (CPUType > 0) {
		if (nr == 2 || nr == 3) {
			int i;
			/* @@@ this is probably wrong (?) */
			for (i = 0 ; i < 12 ; i++) {
				m68k_areg(regs, 7) -= 2;
				put_word (m68k_areg(regs, 7), 0);
			}
			m68k_areg(regs, 7) -= 2;
			put_word (m68k_areg(regs, 7), 0xa000 + nr * 4);
		} else if (nr ==5 || nr == 6 || nr == 7 || nr == 9) {
			m68k_areg(regs, 7) -= 4;
			put_long (m68k_areg(regs, 7), oldpc);
			m68k_areg(regs, 7) -= 2;
			put_word (m68k_areg(regs, 7), 0x2000 + nr * 4);
		} else if (regs.m && nr >= 24 && nr < 32) {
			m68k_areg(regs, 7) -= 2;
			put_word (m68k_areg(regs, 7), nr * 4);
			m68k_areg(regs, 7) -= 4;
			put_long (m68k_areg(regs, 7), currpc);
			m68k_areg(regs, 7) -= 2;
			put_word (m68k_areg(regs, 7), regs.sr);
			regs.sr |= (1 << 13);
			regs.msp = m68k_areg(regs, 7);
			m68k_areg(regs, 7) = regs.isp;
			m68k_areg(regs, 7) -= 2;
			put_word (m68k_areg(regs, 7), 0x1000 + nr * 4);
		} else {
			m68k_areg(regs, 7) -= 2;
			put_word (m68k_areg(regs, 7), nr * 4);
		}
	} else {
		if (nr == 2 || nr == 3) {
			m68k_areg(regs, 7) -= 12;
			/* ??????? */
			if (nr == 3) {
				put_long (m68k_areg(regs, 7), last_fault_for_exception_3);
				put_word (m68k_areg(regs, 7)+4, last_op_for_exception_3);
				put_long (m68k_areg(regs, 7)+8, last_addr_for_exception_3);
			}
			write_log ("Exception!\n");
			goto kludge_me_do;
		}
	}
	m68k_areg(regs, 7) -= 4;
	put_long (m68k_areg(regs, 7), currpc);
kludge_me_do:
	m68k_areg(regs, 7) -= 2;
	put_word (m68k_areg(regs, 7), regs.sr);
	m68k_setpc (get_long (regs.vbr + 4*nr));
	SPCFLAGS_SET( SPCFLAG_JIT_END_COMPILE );
	fill_prefetch_0 ();
	regs.t1 = regs.t0 = regs.m = 0;
	SPCFLAGS_CLEAR( SPCFLAG_TRACE | SPCFLAG_DOTRACE );
}

static void Interrupt(int nr)
{
	assert(nr < 8 && nr >= 0);
	lastint_regs = regs;
	lastint_no = nr;
	Exception(nr+24, 0);

	regs.intmask = nr;
	SPCFLAGS_SET( SPCFLAG_INT );
}

static int caar, cacr, tc, itt0, itt1, dtt0, dtt1, mmusr, urp, srp;

static int movec_illg (int regno)
{
	switch (CPUType) {
	case 1:
		if ((regno & 0x7ff) <= 1)
			return 0;
		break;
	case 2:
	case 3:
		if ((regno & 0x7ff) <= 2)
			return 0;
		if (regno == 0x803 || regno == 0x804)
			return 0;
		break;
	case 4:
		if ((regno & 0x7ff) <= 7) {
			if (regno != 0x802)
				return 0;
		}
		break;
	}
	return 1;
}

int m68k_move2c (int regno, uae_u32 *regp)
{
	if (movec_illg (regno)) {
		op_illg (0x4E7B);
		return 0;
	} else {
		switch (regno) {
		case 0: regs.sfc = *regp & 7; break;
		case 1: regs.dfc = *regp & 7; break;
		case 2:
			cacr = *regp & (CPUType < 4 ? 0x3 : 0x80008000);
#if USE_JIT
			if (CPUType < 4) {
				set_cache_state(cacr&1);
				if (*regp & 0x08)
					flush_icache(1);
			}
			else {
				set_cache_state(cacr&0x8000);
			}
#endif
			break;
		case 3: tc = *regp & 0xc000; break;
		case 4: itt0 = *regp & 0xffffe364; break;
		case 5: itt1 = *regp & 0xffffe364; break;
		case 6: dtt0 = *regp & 0xffffe364; break;
		case 7: dtt1 = *regp & 0xffffe364; break;
		case 0x800: regs.usp = *regp; break;
		case 0x801: regs.vbr = *regp; break;
		case 0x802: caar = *regp &0xfc; break;
		case 0x803: regs.msp = *regp; if (regs.m == 1) m68k_areg(regs, 7) = regs.msp; break;
		case 0x804: regs.isp = *regp; if (regs.m == 0) m68k_areg(regs, 7) = regs.isp; break;
		case 0x805: mmusr = *regp; break;
		case 0x806: urp = *regp; break;
		case 0x807: srp = *regp; break;
		default:
			op_illg (0x4E7B);
			return 0;
		}
	}
	return 1;
}

int m68k_movec2 (int regno, uae_u32 *regp)
{
	if (movec_illg (regno))
	{
		op_illg (0x4E7A);
		return 0;
	} else {
		switch (regno) {
		case 0: *regp = regs.sfc; break;
		case 1: *regp = regs.dfc; break;
		case 2: *regp = cacr; break;
		case 3: *regp = tc; break;
		case 4: *regp = itt0; break;
		case 5: *regp = itt1; break;
		case 6: *regp = dtt0; break;
		case 7: *regp = dtt1; break;
		case 0x800: *regp = regs.usp; break;
		case 0x801: *regp = regs.vbr; break;
		case 0x802: *regp = caar; break;
		case 0x803: *regp = regs.m == 1 ? m68k_areg(regs, 7) : regs.msp; break;
		case 0x804: *regp = regs.m == 0 ? m68k_areg(regs, 7) : regs.isp; break;
		case 0x805: *regp = mmusr; break;
		case 0x806: *regp = urp; break;
		case 0x807: *regp = srp; break;
		default:
			op_illg (0x4E7A);
			return 0;
		}
	}
	return 1;
}

#if !defined(uae_s64)
static __inline__ int
div_unsigned(uae_u32 src_hi, uae_u32 src_lo, uae_u32 div, uae_u32 *quot, uae_u32 *rem)
{
	uae_u32 q = 0, cbit = 0;
	int i;

	if (div <= src_hi) {
		return 1;
	}
	for (i = 0 ; i < 32 ; i++) {
		cbit = src_hi & 0x80000000ul;
		src_hi <<= 1;
		if (src_lo & 0x80000000ul) src_hi++;
		src_lo <<= 1;
		q = q << 1;
		if (cbit || div <= src_hi) {
			q |= 1;
			src_hi -= div;
		}
	}
	*quot = q;
	*rem = src_hi;
	return 0;
}
#endif

void m68k_divl (uae_u32 opcode, uae_u32 src, uae_u16 extra, uaecptr oldpc)
{
#if defined(uae_s64)
	if (src == 0) {
		Exception (5, oldpc);
		return;
	}
	if (extra & 0x800) {
		/* signed variant */
		uae_s64 a = (uae_s64)(uae_s32)m68k_dreg(regs, (extra >> 12) & 7);
		uae_s64 quot, rem;

		if (extra & 0x400) {
			a &= 0xffffffffu;
			a |= (uae_s64)m68k_dreg(regs, extra & 7) << 32;
		}
		rem = a % (uae_s64)(uae_s32)src;
		quot = a / (uae_s64)(uae_s32)src;
		if ((quot & UVAL64(0xffffffff80000000)) != 0
				&& (quot & UVAL64(0xffffffff80000000)) != UVAL64(0xffffffff80000000))
		{
			SET_VFLG (1);
			SET_NFLG (1);
			SET_CFLG (0);
		} else {
			if (((uae_s32)rem < 0) != ((uae_s64)a < 0)) rem = -rem;
			SET_VFLG (0);
			SET_CFLG (0);
			SET_ZFLG (((uae_s32)quot) == 0);
			SET_NFLG (((uae_s32)quot) < 0);
			m68k_dreg(regs, extra & 7) = (uae_u32)rem;
			m68k_dreg(regs, (extra >> 12) & 7) = (uae_u32)quot;
		}
	} else {
		/* unsigned */
		uae_u64 a = (uae_u64)(uae_u32)m68k_dreg(regs, (extra >> 12) & 7);
		uae_u64 quot, rem;

		if (extra & 0x400) {
			a &= 0xffffffffu;
			a |= (uae_u64)m68k_dreg(regs, extra & 7) << 32;
		}
		rem = a % (uae_u64)src;
		quot = a / (uae_u64)src;
		if (quot > 0xffffffffu) {
			SET_VFLG (1);
			SET_NFLG (1);
			SET_CFLG (0);
		} else {
			SET_VFLG (0);
			SET_CFLG (0);
			SET_ZFLG (((uae_s32)quot) == 0);
			SET_NFLG (((uae_s32)quot) < 0);
			m68k_dreg(regs, extra & 7) = (uae_u32)rem;
			m68k_dreg(regs, (extra >> 12) & 7) = (uae_u32)quot;
		}
	}
#else
	if (src == 0) {
		Exception (5, oldpc);
		return;
	}
	if (extra & 0x800) {
		/* signed variant */
		uae_s32 lo = (uae_s32)m68k_dreg(regs, (extra >> 12) & 7);
		uae_s32 hi = lo < 0 ? -1 : 0;
		uae_s32 save_high;
		uae_u32 quot, rem;
		uae_u32 sign;

		if (extra & 0x400) {
			hi = (uae_s32)m68k_dreg(regs, extra & 7);
		}
		save_high = hi;
		sign = (hi ^ src);
		if (hi < 0) {
			hi = ~hi;
			lo = -lo;
			if (lo == 0) hi++;
		}
		if ((uae_s32)src < 0) src = -src;
		if (div_unsigned(hi, lo, src, &quot, &rem) ||
				(sign & 0x80000000) ? quot > 0x80000000 : quot > 0x7fffffff) {
			SET_VFLG (1);
			SET_NFLG (1);
			SET_CFLG (0);
		} else {
			if (sign & 0x80000000) quot = -quot;
			if (((uae_s32)rem < 0) != (save_high < 0)) rem = -rem;
			SET_VFLG (0);
			SET_CFLG (0);
			SET_ZFLG (((uae_s32)quot) == 0);
			SET_NFLG (((uae_s32)quot) < 0);
			m68k_dreg(regs, extra & 7) = rem;
			m68k_dreg(regs, (extra >> 12) & 7) = quot;
		}
	} else {
		/* unsigned */
		uae_u32 lo = (uae_u32)m68k_dreg(regs, (extra >> 12) & 7);
		uae_u32 hi = 0;
		uae_u32 quot, rem;

		if (extra & 0x400) {
			hi = (uae_u32)m68k_dreg(regs, extra & 7);
		}
		if (div_unsigned(hi, lo, src, &quot, &rem)) {
			SET_VFLG (1);
			SET_NFLG (1);
			SET_CFLG (0);
		} else {
			SET_VFLG (0);
			SET_CFLG (0);
			SET_ZFLG (((uae_s32)quot) == 0);
			SET_NFLG (((uae_s32)quot) < 0);
			m68k_dreg(regs, extra & 7) = rem;
			m68k_dreg(regs, (extra >> 12) & 7) = quot;
		}
	}
#endif
}

#if !defined(uae_s64)
static __inline__ void
mul_unsigned(uae_u32 src1, uae_u32 src2, uae_u32 *dst_hi, uae_u32 *dst_lo)
{
	uae_u32 r0 = (src1 & 0xffff) * (src2 & 0xffff);
	uae_u32 r1 = ((src1 >> 16) & 0xffff) * (src2 & 0xffff);
	uae_u32 r2 = (src1 & 0xffff) * ((src2 >> 16) & 0xffff);
	uae_u32 r3 = ((src1 >> 16) & 0xffff) * ((src2 >> 16) & 0xffff);
	uae_u32 lo;

	lo = r0 + ((r1 << 16) & 0xffff0000ul);
	if (lo < r0) r3++;
	r0 = lo;
	lo = r0 + ((r2 << 16) & 0xffff0000ul);
	if (lo < r0) r3++;
	r3 += ((r1 >> 16) & 0xffff) + ((r2 >> 16) & 0xffff);
	*dst_lo = lo;
	*dst_hi = r3;
}
#endif

void m68k_mull (uae_u32 opcode, uae_u32 src, uae_u16 extra)
{
#if defined(uae_s64)
	if (extra & 0x800) {
		/* signed variant */
		uae_s64 a = (uae_s64)(uae_s32)m68k_dreg(regs, (extra >> 12) & 7);

		a *= (uae_s64)(uae_s32)src;
		SET_VFLG (0);
		SET_CFLG (0);
		SET_ZFLG (a == 0);
		SET_NFLG (a < 0);
		if (extra & 0x400)
			m68k_dreg(regs, extra & 7) = a >> 32;
		else if ((a & UVAL64(0xffffffff80000000)) != 0
				 && (a & UVAL64(0xffffffff80000000)) != UVAL64(0xffffffff80000000))
		{
			SET_VFLG (1);
		}
		m68k_dreg(regs, (extra >> 12) & 7) = (uae_u32)a;
	} else {
		/* unsigned */
		uae_u64 a = (uae_u64)(uae_u32)m68k_dreg(regs, (extra >> 12) & 7);

		a *= (uae_u64)src;
		SET_VFLG (0);
		SET_CFLG (0);
		SET_ZFLG (a == 0);
		SET_NFLG (((uae_s64)a) < 0);
		if (extra & 0x400)
			m68k_dreg(regs, extra & 7) = a >> 32;
		else if ((a & UVAL64(0xffffffff00000000)) != 0) {
			SET_VFLG (1);
		}
		m68k_dreg(regs, (extra >> 12) & 7) = (uae_u32)a;
	}
#else
	if (extra & 0x800) {
		/* signed variant */
		uae_s32 src1,src2;
		uae_u32 dst_lo,dst_hi;
		uae_u32 sign;

		src1 = (uae_s32)src;
		src2 = (uae_s32)m68k_dreg(regs, (extra >> 12) & 7);
		sign = (src1 ^ src2);
		if (src1 < 0) src1 = -src1;
		if (src2 < 0) src2 = -src2;
		mul_unsigned((uae_u32)src1,(uae_u32)src2,&dst_hi,&dst_lo);
		if (sign & 0x80000000) {
			dst_hi = ~dst_hi;
			dst_lo = -dst_lo;
			if (dst_lo == 0) dst_hi++;
		}
		SET_VFLG (0);
		SET_CFLG (0);
		SET_ZFLG (dst_hi == 0 && dst_lo == 0);
		SET_NFLG (((uae_s32)dst_hi) < 0);
		if (extra & 0x400)
			m68k_dreg(regs, extra & 7) = dst_hi;
		else if ((dst_hi != 0 || (dst_lo & 0x80000000) != 0)
				 && ((dst_hi & 0xffffffff) != 0xffffffff
					 || (dst_lo & 0x80000000) != 0x80000000))
		{
			SET_VFLG (1);
		}
		m68k_dreg(regs, (extra >> 12) & 7) = dst_lo;
	} else {
		/* unsigned */
		uae_u32 dst_lo,dst_hi;

		mul_unsigned(src,(uae_u32)m68k_dreg(regs, (extra >> 12) & 7),&dst_hi,&dst_lo);

		SET_VFLG (0);
		SET_CFLG (0);
		SET_ZFLG (dst_hi == 0 && dst_lo == 0);
		SET_NFLG (((uae_s32)dst_hi) < 0);
		if (extra & 0x400)
			m68k_dreg(regs, extra & 7) = dst_hi;
		else if (dst_hi != 0) {
			SET_VFLG (1);
		}
		m68k_dreg(regs, (extra >> 12) & 7) = dst_lo;
	}
#endif
}
static const char* ccnames[] =
{ "T ","F ","HI","LS","CC","CS","NE","EQ",
  "VC","VS","PL","MI","GE","LT","GT","LE" };

// If value is greater than zero, this means we are still processing an EmulOp
// because the counter is incremented only in m68k_execute(), i.e. interpretive
// execution only
static int m68k_execute_depth = 0;

void m68k_reset (void)
{
	m68k_areg (regs, 7) = 0x2000;
	m68k_setpc (ROMBaseMac + 0x2a);
	fill_prefetch_0 ();
	regs.s = 1;
	regs.m = 0;
	regs.stopped = 0;
	regs.t1 = 0;
	regs.t0 = 0;
	SET_ZFLG (0);
	SET_XFLG (0);
	SET_CFLG (0);
	SET_VFLG (0);
	SET_NFLG (0);
	SPCFLAGS_INIT( 0 );
	regs.intmask = 7;
	regs.vbr = regs.sfc = regs.dfc = 0;
	fpu_reset();

#if FLIGHT_RECORDER
	log_ptr = 0;
	memset(log, 0, sizeof(log));
#endif

#if ENABLE_MON
	static bool first_time = true;
	if (first_time) {
		first_time = false;
		mon_add_command("regs", dump_regs, "regs                    Dump m68k emulator registers\n");
#if FLIGHT_RECORDER
		// Install "log" command in mon
		mon_add_command("log", dump_log, "log                      Dump m68k emulation log\n");
#endif
	}
#endif
}

void m68k_emulop_return(void)
{
	SPCFLAGS_SET( SPCFLAG_BRK );
	quit_program = true;
}

void m68k_emulop(uae_u32 opcode)
{
	struct M68kRegisters r;
	int i;

	for (i=0; i<8; i++) {
		r.d[i] = m68k_dreg(regs, i);
		r.a[i] = m68k_areg(regs, i);
	}
	MakeSR();
	r.sr = regs.sr;
	EmulOp(opcode, &r);
	for (i=0; i<8; i++) {
		m68k_dreg(regs, i) = r.d[i];
		m68k_areg(regs, i) = r.a[i];
	}
	regs.sr = r.sr;
	MakeFromSR();
}

void REGPARAM2 op_illg (uae_u32 opcode)
{
	uaecptr pc = m68k_getpc ();

	if ((opcode & 0xF000) == 0xA000) {
		Exception(0xA,0);
		return;
	}

	if ((opcode & 0xF000) == 0xF000) {
		Exception(0xB,0);
		return;
	}

	write_log ("Illegal instruction: %04x at %08x\n", opcode, pc);
#if USE_JIT && JIT_DEBUG
	compiler_dumpstate();
#endif

	Exception (4,0);
	return;
}

void mmu_op(uae_u32 opcode, uae_u16 extra)
{
	if ((opcode & 0xFE0) == 0x0500) {
		/* PFLUSH */
		mmusr = 0;
	} else if ((opcode & 0x0FD8) == 0x548) {
		/* PTEST */
	} else
		op_illg (opcode);
}

static int n_insns = 0, n_spcinsns = 0;

static uaecptr last_trace_ad = 0;

static void do_trace (void)
{
	if (regs.t0 && CPUType >= 2) {
		uae_u16 opcode;
		/* should also include TRAP, CHK, SR modification FPcc */
		/* probably never used so why bother */
		/* We can afford this to be inefficient... */
		m68k_setpc (m68k_getpc ());
		fill_prefetch_0 ();
		opcode = get_word(m68k_getpc());
		if (opcode == 0x4e72            /* RTE */
				|| opcode == 0x4e74                 /* RTD */
				|| opcode == 0x4e75                 /* RTS */
				|| opcode == 0x4e77                 /* RTR */
				|| opcode == 0x4e76                 /* TRAPV */
				|| (opcode & 0xffc0) == 0x4e80      /* JSR */
				|| (opcode & 0xffc0) == 0x4ec0      /* JMP */
				|| (opcode & 0xff00) == 0x6100  /* BSR */
				|| ((opcode & 0xf000) == 0x6000     /* Bcc */
					&& cctrue((opcode >> 8) & 0xf))
				|| ((opcode & 0xf0f0) == 0x5050 /* DBcc */
					&& !cctrue((opcode >> 8) & 0xf)
					&& (uae_s16)m68k_dreg(regs, opcode & 7) != 0))
		{
			last_trace_ad = m68k_getpc ();
			SPCFLAGS_CLEAR( SPCFLAG_TRACE );
			SPCFLAGS_SET( SPCFLAG_DOTRACE );
		}
	} else if (regs.t1) {
		last_trace_ad = m68k_getpc ();
		SPCFLAGS_CLEAR( SPCFLAG_TRACE );
		SPCFLAGS_SET( SPCFLAG_DOTRACE );
	}
}

int m68k_do_specialties (void)
{
#if USE_JIT
	// Block was compiled
	SPCFLAGS_CLEAR( SPCFLAG_JIT_END_COMPILE );

	// Retain the request to get out of compiled code until
	// we reached the toplevel execution, i.e. the one that
	// can compile then run compiled code. This also means
	// we processed all (nested) EmulOps
	if ((m68k_execute_depth == 0) && SPCFLAGS_TEST( SPCFLAG_JIT_EXEC_RETURN ))
		SPCFLAGS_CLEAR( SPCFLAG_JIT_EXEC_RETURN );
#endif

	if (SPCFLAGS_TEST( SPCFLAG_DOTRACE )) {
		Exception (9,last_trace_ad);
	}
	while (SPCFLAGS_TEST( SPCFLAG_STOP )) {
		if (SPCFLAGS_TEST( SPCFLAG_INT | SPCFLAG_DOINT )){
			SPCFLAGS_CLEAR( SPCFLAG_INT | SPCFLAG_DOINT );
			int intr = intlev ();
			if (intr != -1 && intr > regs.intmask) {
				Interrupt (intr);
				regs.stopped = 0;
				SPCFLAGS_CLEAR( SPCFLAG_STOP );
			}
		}
	}
	if (SPCFLAGS_TEST( SPCFLAG_TRACE ))
		do_trace ();

	if (SPCFLAGS_TEST( SPCFLAG_DOINT )) {
		SPCFLAGS_CLEAR( SPCFLAG_DOINT );
		int intr = intlev ();
		if (intr != -1 && intr > regs.intmask) {
			Interrupt (intr);
			regs.stopped = 0;
		}
	}
	if (SPCFLAGS_TEST( SPCFLAG_INT )) {
		SPCFLAGS_CLEAR( SPCFLAG_INT );
		SPCFLAGS_SET( SPCFLAG_DOINT );
	}
	if (SPCFLAGS_TEST( SPCFLAG_BRK )) {
		SPCFLAGS_CLEAR( SPCFLAG_BRK );
		return 1;
	}
	return 0;
}

void m68k_do_execute (void)
{
	for (;;) {
		uae_u32 opcode = GET_OPCODE;
#if FLIGHT_RECORDER
		m68k_record_step(m68k_getpc());
#endif
		(*cpufunctbl[opcode])(opcode);
		cpu_check_ticks();
		if (SPCFLAGS_TEST(SPCFLAG_ALL_BUT_EXEC_RETURN)) {
			if (m68k_do_specialties())
				return;
		}
	}
}

void m68k_execute (void)
{
#if USE_JIT
	++m68k_execute_depth;
#endif
	for (;;) {
		if (quit_program)
			break;
		m68k_do_execute();
	}
#if USE_JIT
	--m68k_execute_depth;
#endif
}

static void m68k_verify (uaecptr addr, uaecptr *nextpc)
{
	uae_u32 opcode, val;
	struct instr *dp;

	opcode = get_iword_1(0);
	last_op_for_exception_3 = opcode;
	m68kpc_offset = 2;

	if (cpufunctbl[cft_map (opcode)] == op_illg_1) {
		opcode = 0x4AFC;
	}
	dp = table68k + opcode;

	if (dp->suse) {
		if (!verify_ea (dp->sreg, (amodes)dp->smode, (wordsizes)dp->size, &val)) {
			Exception (3, 0);
			return;
		}
	}
	if (dp->duse) {
		if (!verify_ea (dp->dreg, (amodes)dp->dmode, (wordsizes)dp->size, &val)) {
			Exception (3, 0);
			return;
		}
	}
}

void m68k_disasm (uaecptr addr, uaecptr *nextpc, int cnt)
{
	uaecptr newpc = 0;
	m68kpc_offset = addr - m68k_getpc ();
	while (cnt-- > 0) {
		char instrname[20],*ccpt;
		int opwords;
		uae_u32 opcode;
		struct mnemolookup *lookup;
		struct instr *dp;
		printf ("%08lx: ", m68k_getpc () + m68kpc_offset);
		for (opwords = 0; opwords < 5; opwords++){
			printf ("%04x ", get_iword_1 (m68kpc_offset + opwords*2));
		}
		opcode = get_iword_1 (m68kpc_offset);
		m68kpc_offset += 2;
		if (cpufunctbl[cft_map (opcode)] == op_illg_1) {
			opcode = 0x4AFC;
		}
		dp = table68k + opcode;
		for (lookup = lookuptab;lookup->mnemo != dp->mnemo; lookup++)
			;

		strcpy (instrname, lookup->name);
		ccpt = strstr (instrname, "cc");
		if (ccpt != 0) {
			strncpy (ccpt, ccnames[dp->cc], 2);
		}
		printf ("%s", instrname);
		switch (dp->size){
		case sz_byte: printf (".B "); break;
		case sz_word: printf (".W "); break;
		case sz_long: printf (".L "); break;
		default: printf ("   "); break;
		}

		if (dp->suse) {
			newpc = m68k_getpc () + m68kpc_offset;
			newpc += ShowEA (dp->sreg, (amodes)dp->smode, (wordsizes)dp->size, 0);
		}
		if (dp->suse && dp->duse)
			printf (",");
		if (dp->duse) {
			newpc = m68k_getpc () + m68kpc_offset;
			newpc += ShowEA (dp->dreg, (amodes)dp->dmode, (wordsizes)dp->size, 0);
		}
		if (ccpt != 0) {
			if (cctrue(dp->cc))
				printf (" == %08x (TRUE)", newpc);
			else
				printf (" == %08x (FALSE)", newpc);
		} else if ((opcode & 0xff00) == 0x6100) /* BSR */
			printf (" == %08x", newpc);
		printf ("\n");
	}
	if (nextpc)
		*nextpc = m68k_getpc () + m68kpc_offset;
}

void m68k_dumpstate (uaecptr *nextpc)
{
	int i;
	for (i = 0; i < 8; i++){
		printf ("D%d: %08x ", i, m68k_dreg(regs, i));
		if ((i & 3) == 3) printf ("\n");
	}
	for (i = 0; i < 8; i++){
		printf ("A%d: %08x ", i, m68k_areg(regs, i));
		if ((i & 3) == 3) printf ("\n");
	}
	if (regs.s == 0) regs.usp = m68k_areg(regs, 7);
	if (regs.s && regs.m) regs.msp = m68k_areg(regs, 7);
	if (regs.s && regs.m == 0) regs.isp = m68k_areg(regs, 7);
	printf ("USP=%08x ISP=%08x MSP=%08x VBR=%08x\n",
			regs.usp,regs.isp,regs.msp,regs.vbr);
	printf ("T=%d%d S=%d M=%d X=%ld N=%ld Z=%ld V=%ld C=%ld IMASK=%d\n",
			regs.t1, regs.t0, regs.s, regs.m,
			GET_XFLG, GET_NFLG, GET_ZFLG, GET_VFLG, GET_CFLG, regs.intmask);

	fpu_dump_registers();
	fpu_dump_flags();

	m68k_disasm(m68k_getpc (), nextpc, 1);
	if (nextpc)
		printf ("next PC: %08x\n", *nextpc);
}
