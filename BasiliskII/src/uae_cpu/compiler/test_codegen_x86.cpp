/******************** -*- mode: C; tab-width: 8 -*- ********************
 *
 *	Dumb and Brute Force Run-time assembler verifier for IA-32 and AMD64
 *
 ***********************************************************************/


/***********************************************************************
 *
 *  Copyright 2004 Gwenole Beauchesne
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
 *
 ***********************************************************************/

/*
 *  STATUS: 5.5M variations covering unary register based operations,
 *  reg/reg operations, imm/reg operations.
 *
 *  TODO:
 *  - Rewrite to use internal BFD/opcodes format instead of string compares
 *  - Add reg/mem, imm/mem variations
 */

#define _BSD_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "sysdeps.h"

#undef abort
#define abort() do {							\
	fprintf(stderr, "ABORT: %s, line %d\n", __FILE__, __LINE__);	\
	(abort)();							\
} while (0)

#define X86_TARGET_64BIT	1
#define X86_FLAT_REGISTERS	0
#define X86_OPTIMIZE_ALU	1
#define X86_OPTIMIZE_ROTSHI	1
#include "compiler/codegen_x86.h"

#define x86_emit_byte(B)	emit_byte(B)
#define x86_emit_word(W)	emit_word(W)
#define x86_emit_long(L)	emit_long(L)
#define x86_emit_quad(Q)	emit_quad(Q)
#define x86_get_target()	get_target()
#define x86_emit_failure(MSG)	jit_fail(MSG, __FILE__, __LINE__, __FUNCTION__)

static void jit_fail(const char *msg, const char *file, int line, const char *function)
{
    fprintf(stderr, "JIT failure in function %s from file %s at line %d: %s\n",
	    function, file, line, msg);
    abort();
}

static uint8 *target;

static inline void emit_byte(uint8 x)
{
    *target++ = x;
}

static inline void emit_word(uint16 x)
{
    *((uint16 *)target) = x;
    target += 2;
}

static inline void emit_long(uint32 x)
{
    *((uint32 *)target) = x;
    target += 4;
}

static inline void emit_quad(uint64 x)
{
    *((uint64 *)target) = x;
    target += 8;
}

static inline void set_target(uint8 *t)
{
    target = t;
}

static inline uint8 *get_target(void)
{
    return target;
}

static uint32 mon_read_byte(uintptr addr)
{
    uint8 *m = (uint8 *)addr;
    return (uint32)(*m);
}

extern "C" {
#include "disass/dis-asm.h"

int buffer_read_memory(bfd_vma from, bfd_byte *to, unsigned int length, struct disassemble_info *info)
{
    while (length--)
	*to++ = mon_read_byte(from++);
    return 0;
}

void perror_memory(int status, bfd_vma memaddr, struct disassemble_info *info)
{
    info->fprintf_func(info->stream, "Unknown error %d\n", status);
}

void generic_print_address(bfd_vma addr, struct disassemble_info *info)
{
    if (addr >= UVAL64(0x100000000))
	info->fprintf_func(info->stream, "$%08x%08x", (uint32)(addr >> 32), (uint32)addr);
    else
	info->fprintf_func(info->stream, "$%08x", (uint32)addr);
}

int generic_symbol_at_address(bfd_vma addr, struct disassemble_info *info)
{
    return 0;
}
}

struct SFILE {
    char *buffer;
    char *current;
};

static int mon_sprintf(SFILE *f, const char *format, ...)
{
    int n;
    va_list args;
    va_start(args, format);
    vsprintf(f->current, format, args);
    f->current += n = strlen(f->current);
    va_end(args);
    return n;
}

static int disass_x86(char *buf, uintptr adr)
{
    disassemble_info info;
    SFILE sfile;
    sfile.buffer = buf;
    sfile.current = buf;
    INIT_DISASSEMBLE_INFO(info, (FILE *)&sfile, (fprintf_ftype)mon_sprintf);
    info.mach = bfd_mach_x86_64;
    info.disassembler_options = "suffix";
    return print_insn_i386(adr, &info);
}

enum {
    op_disp,
    op_reg,
    op_base,
    op_index,
    op_scale,
    op_imm,
};
struct operand_t {
    int32 disp;
    int8 reg;
    int8 base;
    int8 index;
    int8 scale;
    int64 imm;

    void clear() {
	disp = imm = 0;
	reg = base = index = -1;
	scale = 1;
    }

    void fill(int optype, int value) {
	switch (optype) {
	case op_disp:	disp = value;	break;
	case op_reg:	reg = value;	break;
	case op_base:	base = value;	break;
	case op_index:	index = value;	break;
	case op_scale:	scale = value;	break;
	case op_imm:	imm = value;	break;
	default:	abort();
	}
    }
};

struct insn_t {
    char name[16];
    int n_operands;
#define MAX_OPERANDS 3
    operand_t operands[MAX_OPERANDS];

    void clear() {
	memset(name, 0, sizeof(name));
	n_operands = 0;
	for (int i = 0; i < MAX_OPERANDS; i++)
	    operands[i].clear();
    }

    void pretty_print() {
	printf("%s, %d operands\n", name, n_operands);
	for (int i = 0; i < n_operands; i++) {
	    operand_t *op = &operands[i];
	    if (op->reg != -1)
		printf(" reg r%d\n", op->reg);
	    else {
		printf(" mem 0x%08x(", op->disp);
		if (op->base != -1)
		    printf("r%d", op->base);
		printf(",");
		if (op->index != -1)
		    printf("r%d", op->index);
		printf(",");
		if (op->base != -1 || op->index != -1)
		    printf("%d", op->scale);
		printf(")\n");
	    }
	}
    }
};

static const struct {
    const char *name;
    int reg;
}
regnames[] = {
#define _(REG) { #REG, X86_##REG }

    _(AL), _(CL), _(DL), _(BL),
    _(AH), _(CH), _(DH), _(BH),
    _(SPL), _(BPL), _(SIL), _(DIL),
    _(R8B), _(R9B), _(R10B), _(R11B), _(R12B), _(R13B), _(R14B), _(R15B),

    _(AX), _(CX), _(DX), _(BX), _(SP), _(BP), _(SI), _(DI),
    _(R8W), _(R9W), _(R10W), _(R11W), _(R12W), _(R13W), _(R14W), _(R15W),

    _(EAX), _(ECX), _(EDX), _(EBX), _(ESP), _(EBP), _(ESI), _(EDI),
    _(R8D), _(R9D), _(R10D), _(R11D), _(R12D), _(R13D), _(R14D), _(R15D),

    _(RAX), _(RCX), _(RDX), _(RBX), _(RSP), _(RBP), _(RSI), _(RDI),
    _(R8), _(R9), _(R10), _(R11), _(R12), _(R13), _(R14), _(R15),

    { NULL, -1 }
#undef _
};

static int parse_reg(operand_t *op, int optype, char *buf)
{
    for (int i = 0; regnames[i].name; i++) {
	int len = strlen(regnames[i].name);
	if (strncasecmp(regnames[i].name, buf, len) == 0) {
	    op->fill(optype, regnames[i].reg);
	    return len;
	}
    }
    return 0;
}

static int parse_mem(operand_t *op, char *buf)
{
    char *p = buf;

    if (strncmp(buf, "0x", 2) == 0) {
	unsigned long val = strtoul(buf, &p, 16);
	if (val == 0 && errno == EINVAL)
	    abort();
	op->disp = val;
    }

    if (*p == '(') {
	p++;

	if (*p == '%') {
	    p++;

	    int n = parse_reg(op, op_base, p);
	    if (n <= 0)
		return -3;
	    p += n;
	}

	if (*p == ',') {
	    p++;

	    if (*p == '%') {
		int n = parse_reg(op, op_index, ++p);
		if (n <= 0)
		    return -4;
		p += n;

		if (*p != ',')
		    return -5;
		p++;

		goto do_parse_scale;
	    }
	    else if (isdigit(*p)) {
	    do_parse_scale:
		long val = strtol(p, &p, 10);
		if (val == 0 && errno == EINVAL)
		    abort();
		op->scale = val;
	    }
	}

	if (*p != ')')
	    return -6;
	p++;
    }

    return p - buf;
}

static void parse_insn(insn_t *ii, char *buf)
{
    char *p = buf;
    ii->clear();

    for (int i = 0; !isspace(*p); i++)
	ii->name[i] = *p++;

    while (*p && isspace(*p))
	p++;
    if (*p == '\0')
	return;

    int n_operands = 0;
    int optype = op_reg;
    bool done = false;
    while (!done) {
	int n;
	switch (*p) {
	case '%':
	    n = parse_reg(&ii->operands[n_operands], optype, ++p);
	    if (n <= 0) {
		fprintf(stderr, "parse_reg(%s) error %d\n", p, n);
		abort();
	    }
	    p += n;
	    break;
	case '0': case '(':
	    n = parse_mem(&ii->operands[n_operands], p);
	    if (n <= 0) {
		fprintf(stderr, "parse_mem(%s) error %d\n", p, n);
		abort();
	    }
	    p += n;
	    break;
	case '$': {
	    unsigned long val = strtoul(++p, &p, 16);
	    if (val == 0 && errno == EINVAL)
		abort();
	    ii->operands[n_operands].imm = val;
	    break;
	}
	case '*':
	    p++;
	    break;
	case ',':
	    n_operands++;
	    p++;
	    break;
	case ' ': case '\t':
	    p++;
	    break;
	case '\0':
	    done = true;
	    break;
	default:
	    fprintf(stderr, "parse error> %s\n", p);
	    abort();
	}
    }
    ii->n_operands = n_operands + 1;
}

static long n_tests, n_failures;
static long n_all_tests, n_all_failures;

static bool check_reg(insn_t *ii, const char *name, int r)
{
    if (strcasecmp(ii->name, name) != 0) {
	fprintf(stderr, "ERROR: instruction mismatch, expected %s, got %s\n", name, ii->name);
	return false;
    }

    if (ii->n_operands != 1) {
	fprintf(stderr, "ERROR: instruction expected 1 operand, got %d\n", ii->n_operands);
	return false;
    }

    int reg = ii->operands[0].reg;

    if (reg != r) {
	fprintf(stderr, "ERROR: instruction expected r%d as source, got ", r);
	if (reg == -1)
	    fprintf(stderr, "nothing\n");
	else
	    fprintf(stderr, "%d\n", reg);
	return false;
    }

    return true;
}

static bool check_reg_reg(insn_t *ii, const char *name, int s, int d)
{
    if (strcasecmp(ii->name, name) != 0) {
	fprintf(stderr, "ERROR: instruction mismatch, expected %s, got %s\n", name, ii->name);
	return false;
    }

    if (ii->n_operands != 2) {
	fprintf(stderr, "ERROR: instruction expected 2 operands, got %d\n", ii->n_operands);
	return false;
    }

    int srcreg = ii->operands[0].reg;
    int dstreg = ii->operands[1].reg;

    if (srcreg != s) {
	fprintf(stderr, "ERROR: instruction expected r%d as source, got ", s);
	if (srcreg == -1)
	    fprintf(stderr, "nothing\n");
	else
	    fprintf(stderr, "%d\n", srcreg);
	return false;
    }

    if (dstreg != d) {
	fprintf(stderr, "ERROR: instruction expected r%d as destination, got ", d);
	if (dstreg == -1)
	    fprintf(stderr, "nothing\n");
	else
	    fprintf(stderr, "%d\n", dstreg);
	return false;
    }

    return true;
}

static bool check_imm_reg(insn_t *ii, const char *name, uint32 v, int d, int mode = -1)
{
    if (strcasecmp(ii->name, name) != 0) {
	fprintf(stderr, "ERROR: instruction mismatch, expected %s, got %s\n", name, ii->name);
	return false;
    }

    if (ii->n_operands != 2) {
	fprintf(stderr, "ERROR: instruction expected 2 operands, got %d\n", ii->n_operands);
	return false;
    }

    uint32 imm = ii->operands[0].imm;
    int dstreg = ii->operands[1].reg;

    if (mode == -1) {
	char suffix = name[strlen(name) - 1];
	switch (suffix) {
	case 'b': mode = 1; break;
	case 'w': mode = 2; break;
	case 'l': mode = 4; break;
	case 'q': mode = 8; break;
	}
    }
    switch (mode) {
    case 1: v &= 0xff; break;
    case 2: v &= 0xffff; break;
    }

    if (imm != v) {
	fprintf(stderr, "ERROR: instruction expected 0x%08x as immediate, got ", v);
	if (imm == -1)
	    fprintf(stderr, "nothing\n");
	else
	    fprintf(stderr, "0x%08x\n", imm);
	return false;
    }

    if (dstreg != d) {
	fprintf(stderr, "ERROR: instruction expected r%d as destination, got ", d);
	if (dstreg == -1)
	    fprintf(stderr, "nothing\n");
	else
	    fprintf(stderr, "%d\n", dstreg);
	return false;
    }

    return true;
}

static bool check_mem_reg(insn_t *ii, const char *name, uint32 D, int B, int I, int S, int R)
{
    if (strcasecmp(ii->name, name) != 0) {
	fprintf(stderr, "ERROR: instruction mismatch, expected %s, got %s\n", name, ii->name);
	return false;
    }

    if (ii->n_operands != 2) {
	fprintf(stderr, "ERROR: instruction expected 2 operands, got %d\n", ii->n_operands);
	return false;
    }

    operand_t *mem = &ii->operands[0];
    operand_t *reg = &ii->operands[1];

    uint32 d = mem->disp;
    int b = mem->base;
    int i = mem->index;
    int s = mem->scale;
    int r = reg->reg;

    if (d != D) {
	fprintf(stderr, "ERROR: instruction expected 0x%08x as displacement, got 0x%08x\n", D, d);
	return false;
    }

    if (b != B) {
	fprintf(stderr, "ERROR: instruction expected r%d as base, got r%d\n", B, b);
	return false;
    }

    if (i != I) {
	fprintf(stderr, "ERROR: instruction expected r%d as index, got r%d\n", I, i);
	return false;
    }

    if (s != S) {
	fprintf(stderr, "ERROR: instruction expected %d as scale factor, got %d\n", S, s);
	return false;
    }

    if (r != R) {
	fprintf(stderr, "ERROR: instruction expected r%d as reg operand, got r%d\n", R, r);
	return false;
    }

    return true;
}

static int verbose = 2;

int main(void)
{
    static char buffer[1024];
#define MAX_INSN_LENGTH 16
#define MAX_INSNS 1024
    static uint8 block[MAX_INSNS * MAX_INSN_LENGTH];
    static char *insns[MAX_INSNS];
    static int modes[MAX_INSNS];
    n_all_tests = n_all_failures = 0;

    printf("Testing reg forms\n");
    n_tests = n_failures = 0;
    for (int r = 0; r < 16; r++) {
	set_target(block);
	uint8 *b = get_target();
	int i = 0;
#define GEN(INSN, GENOP) do {			\
	insns[i++] = INSN;			\
	GENOP##r(r);				\
} while (0)
#define GENA(INSN, GENOP) do {			\
	GEN(INSN "b", GENOP##B);		\
	GEN(INSN "w", GENOP##W);		\
	GEN(INSN "l", GENOP##L);		\
	GEN(INSN "q", GENOP##Q);		\
} while (0)
	GENA("not", NOT);
	GENA("neg", NEG);
	GENA("mul", MUL);
	GENA("imul", IMUL);
	GENA("div", DIV);
	GENA("idiv", IDIV);
	GENA("dec", DEC);
	GENA("inc", INC);
	GEN("callq", CALLs);
	GEN("jmpq", JMPs);
	GEN("pushl", PUSHQ);	// FIXME: disass bug? wrong suffix
	GEN("popl", POPQ);		// FIXME: disass bug? wrong suffix
	GEN("bswap", BSWAPL);	// FIXME: disass bug? no suffix
	GEN("bswap", BSWAPQ);	// FIXME: disass bug? no suffix
	GEN("seto", SETO);
	GEN("setno", SETNO);
	GEN("setb", SETB);
	GEN("setae", SETAE);
	GEN("sete", SETE);
	GEN("setne", SETNE);
	GEN("setbe", SETBE);
	GEN("seta", SETA);
	GEN("sets", SETS);
	GEN("setns", SETNS);
	GEN("setp", SETP);
	GEN("setnp", SETNP);
	GEN("setl", SETL);
	GEN("setge", SETGE);
	GEN("setle", SETLE);
	GEN("setg", SETG);
#undef  GENA
#undef  GEN
	int last_insn = i;
	uint8 *e = get_target();

	uint8 *p = b;
	i = 0;
	while (p < e) {
	    int n = disass_x86(buffer, (uintptr)p);
	    insn_t ii;
	    parse_insn(&ii, buffer);

	    if (!check_reg(&ii, insns[i], r)) {
		if (verbose > 1)
		    fprintf(stderr, "%s\n", buffer);
		n_failures++;
	    }

	    p += n;
	    i += 1;
	    n_tests++;
	}
	if (i != last_insn)
	    abort();
    }
    printf(" done %ld/%ld\n", n_tests - n_failures, n_tests);
    n_all_tests += n_tests;
    n_all_failures += n_failures;

    printf("Testing reg,reg forms\n");
    n_tests = n_failures = 0;
    for (int s = 0; s < 16; s++) {
	for (int d = 0; d < 16; d++) {
	    set_target(block);
	    uint8 *b = get_target();
	    int i = 0;
#define GEN(INSN, GENOP) do {			\
	insns[i++] = INSN;			\
	GENOP##rr(s, d);			\
} while (0)
#define GEN1(INSN, GENOP, OP) do {		\
	insns[i++] = INSN;			\
	GENOP##rr(OP, s, d);			\
} while (0)
#define GENA(INSN, GENOP) do {			\
	GEN(INSN "b", GENOP##B);		\
	GEN(INSN "w", GENOP##W);		\
	GEN(INSN "l", GENOP##L);		\
	GEN(INSN "q", GENOP##Q);		\
} while (0)
	    GENA("adc", ADC);
	    GENA("add", ADD);
	    GENA("and", AND);
	    GENA("cmp", CMP);
	    GENA("or",  OR);
	    GENA("sbb", SBB);
	    GENA("sub", SUB);
	    GENA("xor", XOR);
	    GENA("mov", MOV);
	    GEN("btw", BTW);
	    GEN("btl", BTL);
	    GEN("btq", BTQ);
	    GEN("btcw", BTCW);
	    GEN("btcl", BTCL);
	    GEN("btcq", BTCQ);
	    GEN("btrw", BTRW);
	    GEN("btrl", BTRL);
	    GEN("btrq", BTRQ);
	    GEN("btsw", BTSW);
	    GEN("btsl", BTSL);
	    GEN("btsq", BTSQ);
	    GEN("imulw", IMULW);
	    GEN("imull", IMULL);
	    GEN("imulq", IMULQ);
	    GEN1("cmove", CMOVW, X86_CC_Z);
	    GEN1("cmove", CMOVL, X86_CC_Z);
	    GEN1("cmove", CMOVQ, X86_CC_Z);
	    GENA("test", TEST);
	    GENA("cmpxchg", CMPXCHG);
	    GENA("xadd", XADD);
	    GENA("xchg", XCHG);
	    GEN("bsfw", BSFW);
	    GEN("bsfl", BSFL);
	    GEN("bsfq", BSFQ);
	    GEN("bsrw", BSRW);
	    GEN("bsrl", BSRL);
	    GEN("bsrq", BSRQ);
	    GEN("movsbw", MOVSBW);
	    GEN("movsbl", MOVSBL);
	    GEN("movsbq", MOVSBQ);
	    GEN("movzbw", MOVZBW);
	    GEN("movzbl", MOVZBL);
	    GEN("movzbq", MOVZBQ);
	    GEN("movswl", MOVSWL);
	    GEN("movswq", MOVSWQ);
	    GEN("movzwl", MOVZWL);
	    GEN("movzwq", MOVZWQ);
	    GEN("movslq", MOVSLQ);
#undef  GENA
#undef  GEN1
#undef  GEN
	    int last_insn = i;
	    uint8 *e = get_target();

	    uint8 *p = b;
	    i = 0;
	    while (p < e) {
		int n = disass_x86(buffer, (uintptr)p);
		insn_t ii;
		parse_insn(&ii, buffer);

		if (!check_reg_reg(&ii, insns[i], s, d)) {
		    if (verbose > 1)
			fprintf(stderr, "%s\n", buffer);
		    n_failures++;
		}

		p += n;
		i += 1;
		n_tests++;
	    }
	    if (i != last_insn)
		abort();
	}
    }
    printf(" done %ld/%ld\n", n_tests - n_failures, n_tests);
    n_all_tests += n_tests;
    n_all_failures += n_failures;

    printf("Testing cl,reg forms\n");
    n_tests = n_failures = 0;
    for (int d = 0; d < 16; d++) {
	set_target(block);
	uint8 *b = get_target();
	int i = 0;
#define GEN(INSN, GENOP) do {		\
	insns[i++] = INSN;		\
	GENOP##rr(X86_CL, d);		\
} while (0)
#define GENA(INSN, GENOP) do {		\
	GEN(INSN "b", GENOP##B);	\
	GEN(INSN "w", GENOP##W);	\
	GEN(INSN "l", GENOP##L);	\
	GEN(INSN "q", GENOP##Q);	\
} while (0)
	GENA("rol", ROL);
	GENA("ror", ROR);
	GENA("rcl", RCL);
	GENA("rcr", RCR);
	GENA("shl", SHL);
	GENA("shr", SHR);
	GENA("sar", SAR);
#undef  GENA
#undef  GEN
	int last_insn = i;
	uint8 *e = get_target();

	uint8 *p = b;
	i = 0;
	while (p < e) {
	    int n = disass_x86(buffer, (uintptr)p);
	    insn_t ii;
	    parse_insn(&ii, buffer);

	    if (!check_reg_reg(&ii, insns[i], X86_CL, d)) {
		if (verbose > 1)
		    fprintf(stderr, "%s\n", buffer);
		n_failures++;
	    }

	    p += n;
	    i += 1;
	    n_tests++;
	}
	if (i != last_insn)
	    abort();
    }
    printf(" done %ld/%ld\n", n_tests - n_failures, n_tests);
    n_all_tests += n_tests;
    n_all_failures += n_failures;

    printf("Testing imm,reg forms\n");
    static const uint32 imm_table[] = {
	0x00000000, 0x00000001, 0x00000002, 0x00000004,
	0x00000008, 0x00000010, 0x00000020, 0x00000040,
	0x00000080, 0x000000fe, 0x000000ff, 0x00000100,
	0x00000101, 0x00000102, 0xfffffffe, 0xffffffff,
	0x00000000, 0x10000000, 0x20000000, 0x30000000,
	0x40000000, 0x50000000, 0x60000000, 0x70000000,
	0x80000000, 0x90000000, 0xa0000000, 0xb0000000,
	0xc0000000, 0xd0000000, 0xe0000000, 0xf0000000,
	0xfffffffd, 0xfffffffe, 0xffffffff, 0x00000001,
	0x00000002, 0x00000003, 0x11111111, 0x22222222,
	0x33333333, 0x44444444, 0x55555555, 0x66666666,
	0x77777777, 0x88888888, 0x99999999, 0xaaaaaaaa,
	0xbbbbbbbb, 0xcccccccc, 0xdddddddd, 0xeeeeeeee,
    };
    const int n_imm_tab_count = sizeof(imm_table)/sizeof(imm_table[0]);
    n_tests = n_failures = 0;
    for (int j = 0; j < n_imm_tab_count; j++) {
	const uint32 value = imm_table[j];
	for (int d = 0; d < 16; d++) {
	    set_target(block);
	    uint8 *b = get_target();
	    int i = 0;
#define GEN(INSN, GENOP) do {			\
		insns[i] = INSN;		\
		modes[i] = -1;			\
		i++; GENOP##ir(value, d);	\
	} while (0)
#define GENM(INSN, GENOP, MODE) do {		\
		insns[i] = INSN;		\
		modes[i] = MODE;		\
		i++; GENOP##ir(value, d);	\
	} while (0)
#define GENA(INSN, GENOP) do {			\
		GEN(INSN "b", GENOP##B);	\
		GEN(INSN "w", GENOP##W);	\
		GEN(INSN "l", GENOP##L);	\
		GEN(INSN "q", GENOP##Q);	\
	} while (0)
#define GENAM(INSN, GENOP, MODE) do {		\
		GENM(INSN "b", GENOP##B, MODE);	\
		GENM(INSN "w", GENOP##W, MODE);	\
		GENM(INSN "l", GENOP##L, MODE);	\
		GENM(INSN "q", GENOP##Q, MODE);	\
	} while (0)
	    GENA("adc", ADC);
	    GENA("add", ADD);
	    GENA("and", AND);
	    GENA("cmp", CMP);
	    GENA("or",  OR);
	    GENA("sbb", SBB);
	    GENA("sub", SUB);
	    GENA("xor", XOR);
	    GENA("mov", MOV);
	    GENM("btw", BTW, 1);
	    GENM("btl", BTL, 1);
	    GENM("btq", BTQ, 1);
	    GENM("btcw", BTCW, 1);
	    GENM("btcl", BTCL, 1);
	    GENM("btcq", BTCQ, 1);
	    GENM("btrw", BTRW, 1);
	    GENM("btrl", BTRL, 1);
	    GENM("btrq", BTRQ, 1);
	    GENM("btsw", BTSW, 1);
	    GENM("btsl", BTSL, 1);
	    GENM("btsq", BTSQ, 1);
	    if (value != 1) {
		GENAM("rol", ROL, 1);
		GENAM("ror", ROR, 1);
		GENAM("rcl", RCL, 1);
		GENAM("rcr", RCR, 1);
		GENAM("shl", SHL, 1);
		GENAM("shr", SHR, 1);
		GENAM("sar", SAR, 1);
	    }
	    GENA("test", TEST);
#undef GENAM
#undef GENA
#undef GENM
#undef GEN
	    int last_insn = i;
	    uint8 *e = get_target();

	    uint8 *p = b;
	    i = 0;
	    while (p < e) {
		int n = disass_x86(buffer, (uintptr)p);
		insn_t ii;
		parse_insn(&ii, buffer);

		if (!check_imm_reg(&ii, insns[i], value, d, modes[i])) {
		    if (verbose > 1)
			fprintf(stderr, "%s\n", buffer);
		    n_failures++;
		}

		p += n;
		i += 1;
		n_tests++;
	    }
	    if (i != last_insn)
		abort();
	}
    }
    printf(" done %ld/%ld\n", n_tests - n_failures, n_tests);
    n_all_tests += n_tests;
    n_all_failures += n_failures;

    printf("Testing mem,reg forms\n");
    n_tests = n_failures = 0;
    static const uint32 off_table[] = {
	0x00000000,
	0x00000001,
	0x00000040,
	0x00000080,
	0x000000ff,
	0x00000100,
	0xfffffffe,
	0xffffffff,
    };
    const int off_table_count = sizeof(off_table) / sizeof(off_table[0]);
    for (int d = 0; d < off_table_count; d++) {
	const uint32 D = off_table[d];
	for (int B = -1; B < 16; B++) {
	    for (int I = -1; I < 16; I++) {
		if (I == X86_RSP)
		    continue;
		for (int S = 1; S < 8; S *= 2) {
		    if (I == -1)
			continue;
		    for (int r = 0; r < 16; r++) {
			set_target(block);
			uint8 *b = get_target();
			int i = 0;
#define GEN(INSN, GENOP) do {				\
			insns[i++] = INSN;		\
			GENOP##mr(D, B, I, S, r);	\
			} while (0)
#define GENA(INSN, GENOP) do {				\
			GEN(INSN "b", GENOP##B);	\
			GEN(INSN "w", GENOP##W);	\
			GEN(INSN "l", GENOP##L);	\
			GEN(INSN "q", GENOP##Q);	\
			} while (0)
			GENA("adc", ADC);
			GENA("add", ADD);
			GENA("and", AND);
			GENA("cmp", CMP);
			GENA("or",  OR);
			GENA("sbb", SBB);
			GENA("sub", SUB);
			GENA("xor", XOR);
			GENA("mov", MOV);
			GEN("imulw", IMULW);
			GEN("imull", IMULL);
			GEN("imulq", IMULQ);
			GEN("bsfw", BSFW);
			GEN("bsfl", BSFL);
			GEN("bsfq", BSFQ);
			GEN("bsrw", BSRW);
			GEN("bsrl", BSRL);
			GEN("bsrq", BSRQ);
			GEN("movsbw", MOVSBW);
			GEN("movsbl", MOVSBL);
			GEN("movsbq", MOVSBQ);
			GEN("movzbw", MOVZBW);
			GEN("movzbl", MOVZBL);
			GEN("movzbq", MOVZBQ);
			GEN("movswl", MOVSWL);
			GEN("movswq", MOVSWQ);
			GEN("movzwl", MOVZWL);
			GEN("movzwq", MOVZWQ);
			GEN("movslq", MOVSLQ);
#undef  GENA
#undef  GEN
			int last_insn = i;
			uint8 *e = get_target();

			uint8 *p = b;
			i = 0;
			while (p < e) {
			    int n = disass_x86(buffer, (uintptr)p);
			    insn_t ii;
			    parse_insn(&ii, buffer);

			    if (!check_mem_reg(&ii, insns[i], D, B, I, S, r)) {
				if (verbose > 1)
				    fprintf(stderr, "%s\n", buffer);
				n_failures++;
			    }

			    p += n;
			    i += 1;
			    n_tests++;
			}
			if (i != last_insn)
			    abort();
		    }
		}
	    }
	}
    }
    printf(" done %ld/%ld\n", n_tests - n_failures, n_tests);
    n_all_tests += n_tests;
    n_all_failures += n_failures;

    printf("\n");
    printf("All %ld tests run, %ld failures\n", n_all_tests, n_all_failures);
}
