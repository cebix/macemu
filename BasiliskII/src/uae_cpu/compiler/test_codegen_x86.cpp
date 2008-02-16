/******************** -*- mode: C; tab-width: 8 -*- ********************
 *
 *	Dumb and Brute Force Run-time assembler verifier for IA-32 and AMD64
 *
 ***********************************************************************/


/***********************************************************************
 *
 *  Copyright 2004-2008 Gwenole Beauchesne
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
 *  STATUS: 26M variations covering unary register based operations,
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

static int verbose = 2;

#define TEST_INST_ALU		1
#define TEST_INST_FPU		1
#define TEST_INST_MMX		1
#define TEST_INST_SSE		1
#if TEST_INST_ALU
#define TEST_INST_ALU_REG	1
#define TEST_INST_ALU_REG_REG	1
#define TEST_INST_ALU_CNT_REG	1
#define TEST_INST_ALU_IMM_REG	1
#define TEST_INST_ALU_MEM_REG	1
#endif
#if TEST_INST_FPU
#define TEST_INST_FPU_UNARY	1
#define TEST_INST_FPU_REG	1
#define TEST_INST_FPU_MEM	1
#endif
#if TEST_INST_MMX
#define TEST_INST_MMX_REG_REG	1
#define TEST_INST_MMX_IMM_REG	1
#define TEST_INST_MMX_MEM_REG	1
#endif
#if TEST_INST_SSE
#define TEST_INST_SSE_REG	1
#define TEST_INST_SSE_REG_REG	1
#define TEST_INST_SSE_MEM_REG	1
#endif

#undef abort
#define abort() do {							\
	fprintf(stderr, "ABORT: %s, line %d\n", __FILE__, __LINE__);	\
	(abort)();							\
} while (0)

#define X86_TARGET_64BIT	1
#define X86_FLAT_REGISTERS	0
#define X86_OPTIMIZE_ALU	1
#define X86_OPTIMIZE_ROTSHI	1
#define X86_RIP_RELATIVE_ADDR	0
#include "compiler/codegen_x86.h"

#if X86_TARGET_64BIT
#define X86_MAX_ALU_REGS	16
#define X86_MAX_SSE_REGS	16
#else
#define X86_MAX_ALU_REGS	8
#define X86_MAX_SSE_REGS	8
#endif
#define X86_MAX_FPU_REGS	8
#define X86_MAX_MMX_REGS	8

#define VALID_REG(r, b, n)	(((unsigned)((r) - X86_##b)) < (n))
#if X86_TARGET_64BIT
#define VALID_REG8(r)		(VALID_REG(r, AL, 16) || VALID_REG(r, AH, 4))
#define VALID_REG64(r)		VALID_REG(r, RAX, X86_MAX_ALU_REGS)
#else
#define VALID_REG8(r)		(VALID_REG(r, AL, 4) || VALID_REG(r, AH, 4))
#define VALID_REG64(r)		(0)
#endif
#define VALID_REG16(r)		VALID_REG(r, AX, X86_MAX_ALU_REGS)
#define VALID_REG32(r)		VALID_REG(r, EAX, X86_MAX_ALU_REGS)

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
    info.mach = X86_TARGET_64BIT ? bfd_mach_x86_64 : bfd_mach_i386_i386;
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

#define MAX_INSNS		1024
#define MAX_INSN_LENGTH		16
#define MAX_INSN_OPERANDS	3

struct insn_t {
    char name[16];
    int n_operands;
    operand_t operands[MAX_INSN_OPERANDS];

    void clear() {
	memset(name, 0, sizeof(name));
	n_operands = 0;
	for (int i = 0; i < MAX_INSN_OPERANDS; i++)
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

static inline char *find_blanks(char *p)
{
    while (*p && !isspace(*p))
	++p;
    return p;
}

static inline char *skip_blanks(char *p)
{
    while (*p && isspace(*p))
	++p;
    return p;
}

static int parse_reg(operand_t *op, int optype, char *buf)
{
    int reg = X86_NOREG;
    int len = 0;
    char *p = buf;
    switch (p[0]) {
    case 'a': case 'A':
	len = 2;
	switch (p[1]) {
	case 'l': case 'L': reg = X86_AL; break;
	case 'h': case 'H': reg = X86_AH; break;
	case 'x': case 'X': reg = X86_AX; break;
	}
	break;
    case 'b': case 'B':
	len = 2;
	switch (p[1]) {
	case 'l': case 'L': reg = X86_BL; break;
	case 'h': case 'H': reg = X86_BH; break;
	case 'x': case 'X': reg = X86_BX; break;
	case 'p': case 'P':
	    switch (p[2]) {
#if X86_TARGET_64BIT
	    case 'l': case 'L': reg = X86_BPL, ++len; break;
#endif
	    default: reg = X86_BP; break;
	    }
	    break;
	}
	break;
    case 'c': case 'C':
	len = 2;
	switch (p[1]) {
	case 'l': case 'L': reg = X86_CL; break;
	case 'h': case 'H': reg = X86_CH; break;
	case 'x': case 'X': reg = X86_CX; break;
	}
	break;
    case 'd': case 'D':
	len = 2;
	switch (p[1]) {
	case 'l': case 'L': reg = X86_DL; break;
	case 'h': case 'H': reg = X86_DH; break;
	case 'x': case 'X': reg = X86_DX; break;
	case 'i': case 'I':
	    switch (p[2]) {
#if X86_TARGET_64BIT
	    case 'l': case 'L': reg = X86_DIL; ++len; break;
#endif
	    default: reg = X86_DI; break;
	    }
	    break;
	}
	break;
    case 's': case 'S':
	len = 2;
	switch (p[2]) {
#if X86_TARGET_64BIT
	case 'l': case 'L':
	    ++len;
	    switch (p[1]) {
	    case 'p': case 'P': reg = X86_SPL; break;
	    case 'i': case 'I': reg = X86_SIL; break;
	    }
	    break;
#endif
	case '(':
	    if ((p[1] == 't' || p[1] == 'T') && isdigit(p[3]) && p[4] == ')')
		len += 3, reg = X86_ST0 + (p[3] - '0');
	    break;
	default:
	    switch (p[1]) {
	    case 't': case 'T': reg = X86_ST0; break;
	    case 'p': case 'P': reg = X86_SP; break;
	    case 'i': case 'I': reg = X86_SI; break;
	    }
	    break;
	}
	break;
    case 'e': case 'E':
	len = 3;
	switch (p[2]) {
	case 'x': case 'X':
	    switch (p[1]) {
	    case 'a': case 'A': reg = X86_EAX; break;
	    case 'b': case 'B': reg = X86_EBX; break;
	    case 'c': case 'C': reg = X86_ECX; break;
	    case 'd': case 'D': reg = X86_EDX; break;
	    }
	    break;
	case 'i': case 'I':
	    switch (p[1]) {
	    case 's': case 'S': reg = X86_ESI; break;
	    case 'd': case 'D': reg = X86_EDI; break;
	    }
	    break;
	case 'p': case 'P':
	    switch (p[1]) {
	    case 'b': case 'B': reg = X86_EBP; break;
	    case 's': case 'S': reg = X86_ESP; break;
	    }
	    break;
	}
	break;
#if X86_TARGET_64BIT
    case 'r': case 'R':
	len = 3;
	switch (p[2]) {
	case 'x': case 'X':
	    switch (p[1]) {
	    case 'a': case 'A': reg = X86_RAX; break;
	    case 'b': case 'B': reg = X86_RBX; break;
	    case 'c': case 'C': reg = X86_RCX; break;
	    case 'd': case 'D': reg = X86_RDX; break;
	    }
	    break;
	case 'i': case 'I':
	    switch (p[1]) {
	    case 's': case 'S': reg = X86_RSI; break;
	    case 'd': case 'D': reg = X86_RDI; break;
	    }
	    break;
	case 'p': case 'P':
	    switch (p[1]) {
	    case 'b': case 'B': reg = X86_RBP; break;
	    case 's': case 'S': reg = X86_RSP; break;
	    }
	    break;
	case 'b': case 'B':
	    switch (p[1]) {
	    case '8': reg = X86_R8B; break;
	    case '9': reg = X86_R9B; break;
	    }
	    break;
	case 'w': case 'W':
	    switch (p[1]) {
	    case '8': reg = X86_R8W; break;
	    case '9': reg = X86_R9W; break;
	    }
	    break;
	case 'd': case 'D':
	    switch (p[1]) {
	    case '8': reg = X86_R8D; break;
	    case '9': reg = X86_R9D; break;
	    }
	    break;
	case '0': case '1': case '2': case '3': case '4': case '5':
	    if (p[1] == '1') {
		const int r = p[2] - '0';
		switch (p[3]) {
		case 'b': case 'B': reg = X86_R10B + r, ++len; break;
		case 'w': case 'W': reg = X86_R10W + r, ++len; break;
		case 'd': case 'D': reg = X86_R10D + r, ++len; break;
		default: reg = X86_R10 + r; break;
		}
	    }
	    break;
	default:
	    switch (p[1]) {
	    case '8': reg = X86_R8, len = 2; break;
	    case '9': reg = X86_R9, len = 2; break;
	    }
	    break;
	}
	break;
#endif
    case 'm': case 'M':
	if ((p[1] == 'm' || p[1] == 'M') && isdigit(p[2]))
	    reg = X86_MM0 + (p[2] - '0'), len = 3;
	break;
    case 'x': case 'X':
	if ((p[1] == 'm' || p[1] == 'M') && (p[2] == 'm' || p[2] == 'M')) {
#if X86_TARGET_64BIT
	    if (p[3] == '1' && isdigit(p[4]))
		reg = X86_XMM10 + (p[4] - '0'), len = 5;
	    else
#endif
	    if (isdigit(p[3]))
		reg = X86_XMM0 + (p[3] - '0'), len = 4;
	}
	break;
    }

    if (len > 0 && reg != X86_NOREG) {
	op->fill(optype, reg);
	return len;
    }

    return X86_NOREG;
}

static unsigned long parse_imm(char *nptr, char **endptr, int base = 0)
{
    errno = 0;
#if X86_TARGET_64BIT
    if (sizeof(unsigned long) != 8) {
	unsigned long long val = strtoull(nptr, endptr, 0);
	if (errno == 0)
	    return val;
	abort();
    }
#endif
    unsigned long val = strtoul(nptr, endptr, 0);
    if (errno == 0)
	return val;
    abort();
    return 0;
}

static int parse_mem(operand_t *op, char *buf)
{
    char *p = buf;

    if (strncmp(buf, "0x", 2) == 0)
	op->disp = parse_imm(buf, &p, 16);

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

#if 0
    printf("BUF: %s\n", buf);
#endif

    if (strncmp(p, "rex64", 5) == 0) {
	char *q = find_blanks(p);
	if (verbose > 1) {
	    char prefix[16];
	    memset(prefix, 0, sizeof(prefix));
	    memcpy(prefix, p, q - p);
	    fprintf(stderr, "Instruction '%s', skip REX prefix '%s'\n", buf, prefix);
	}
	p = skip_blanks(q);
    }

    if (strncmp(p, "rep", 3) == 0) {
	char *q = find_blanks(p);
	if (verbose > 1) {
	    char prefix[16];
	    memset(prefix, 0, sizeof(prefix));
	    memcpy(prefix, p, q - p);
	    fprintf(stderr, "Instruction '%s', skip REP prefix '%s'\n", buf, prefix);
	}
	p = skip_blanks(q);
    }

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
	    ii->operands[n_operands].imm = parse_imm(++p, &p, 0);
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

static unsigned long n_tests, n_failures;
static unsigned long n_all_tests, n_all_failures;

static bool check_unary(insn_t *ii, const char *name)
{
    if (strcasecmp(ii->name, name) != 0) {
	fprintf(stderr, "ERROR: instruction mismatch, expected %s, got %s\n", name, ii->name);
	return false;
    }

    if (ii->n_operands != 0) {
	fprintf(stderr, "ERROR: instruction expected 0 operand, got %d\n", ii->n_operands);
	return false;
    }

    return true;
}

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
	    fprintf(stderr, "r%d\n", reg);
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
	    fprintf(stderr, "r%d\n", srcreg);
	return false;
    }

    if (dstreg != d) {
	fprintf(stderr, "ERROR: instruction expected r%d as destination, got ", d);
	if (dstreg == -1)
	    fprintf(stderr, "nothing\n");
	else
	    fprintf(stderr, "r%d\n", dstreg);
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

static bool do_check_mem(insn_t *ii, uint32 D, int B, int I, int S, int Mpos)
{
    operand_t *mem = &ii->operands[Mpos];
    uint32 d = mem->disp;
    int b = mem->base;
    int i = mem->index;
    int s = mem->scale;

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

    return true;
}

static bool check_mem(insn_t *ii, const char *name, uint32 D, int B, int I, int S)
{
    if (strcasecmp(ii->name, name) != 0) {
	fprintf(stderr, "ERROR: instruction mismatch, expected %s, got %s\n", name, ii->name);
	return false;
    }

    if (ii->n_operands != 1) {
	fprintf(stderr, "ERROR: instruction expected 1 operand, got %d\n", ii->n_operands);
	return false;
    }

    return do_check_mem(ii, D, B, I, S, 0);
}

static bool check_mem_reg(insn_t *ii, const char *name, uint32 D, int B, int I, int S, int R, int Rpos = 1)
{
    if (strcasecmp(ii->name, name) != 0) {
	fprintf(stderr, "ERROR: instruction mismatch, expected %s, got %s\n", name, ii->name);
	return false;
    }

    if (ii->n_operands != 2) {
	fprintf(stderr, "ERROR: instruction expected 2 operands, got %d\n", ii->n_operands);
	return false;
    }

    if (!do_check_mem(ii, D, B, I, S, Rpos ^ 1))
	return false;

    int r = ii->operands[Rpos].reg;

    if (r != R) {
	fprintf(stderr, "ERROR: instruction expected r%d as reg operand, got r%d\n", R, r);
	return false;
    }

    return true;
}

static inline bool check_reg_mem(insn_t *ii, const char *name, uint32 D, int B, int I, int S, int R)
{
    return check_mem_reg(ii, name, D, B, I, S, R, 0);
}

static void show_instruction(const char *buffer, const uint8 *bytes)
{
    if (verbose > 1) {
	if (1) {
	    for (int j = 0; j < MAX_INSN_LENGTH; j++)
		fprintf(stderr, "%02x ", bytes[j]);
	    fprintf(stderr, "| ");
	}
	fprintf(stderr, "%s\n", buffer);
    }
}

static void show_status(unsigned long n_tests)
{
#if 1
  const unsigned long N_STEPS = 100000;
  static const char cursors[] = { '-', '\\', '|', '/' };
  if ((n_tests % N_STEPS) == 0) {
    printf(" %c (%d)\r", cursors[(n_tests/N_STEPS)%sizeof(cursors)], n_tests);
    fflush(stdout);
  }
#else
  const unsigned long N_STEPS = 1000000;
  if ((n_tests % N_STEPS) == 0)
    printf(" ... %d\n", n_tests);
#endif
}

int main(void)
{
    static char buffer[1024];
    static uint8 block[MAX_INSNS * MAX_INSN_LENGTH];
    static char *insns[MAX_INSNS];
    static int modes[MAX_INSNS];
    n_all_tests = n_all_failures = 0;

#if TEST_INST_ALU_REG
    printf("Testing reg forms\n");
    n_tests = n_failures = 0;
    for (int r = 0; r < X86_MAX_ALU_REGS; r++) {
	set_target(block);
	uint8 *b = get_target();
	int i = 0;
#define GEN(INSN, GENOP) do {			\
	insns[i++] = INSN;			\
	GENOP##r(r);				\
} while (0)
#define GEN64(INSN, GENOP) do {			\
	if (X86_TARGET_64BIT)			\
	    GEN(INSN, GENOP);			\
} while (0)
#define GENA(INSN, GENOP) do {			\
	if (VALID_REG8(r))			\
	    GEN(INSN "b", GENOP##B);		\
	GEN(INSN "w", GENOP##W);		\
	GEN(INSN "l", GENOP##L);		\
	GEN64(INSN "q", GENOP##Q);		\
} while (0)
	GENA("not", NOT);
	GENA("neg", NEG);
	GENA("mul", MUL);
	GENA("imul", IMUL);
	GENA("div", DIV);
	GENA("idiv", IDIV);
	GENA("dec", DEC);
	GENA("inc", INC);
	if (X86_TARGET_64BIT) {
	    GEN("callq", CALLs);
	    GEN("jmpq", JMPs);
	    GEN("pushq", PUSHQ);
	    GEN("popq", POPQ);
	}
	else {
	    GEN("calll", CALLs);
	    GEN("jmpl", JMPs);
	    GEN("pushl", PUSHL);
	    GEN("popl", POPL);
	}
	GEN("bswap", BSWAPL);	// FIXME: disass bug? no suffix
	GEN64("bswap", BSWAPQ);	// FIXME: disass bug? no suffix
	if (VALID_REG8(r)) {
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
	}
#undef  GENA
#undef  GEN64
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
		show_instruction(buffer, p);
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
#endif

#if TEST_INST_ALU_REG_REG
    printf("Testing reg,reg forms\n");
    n_tests = n_failures = 0;
    for (int s = 0; s < X86_MAX_ALU_REGS; s++) {
	for (int d = 0; d < X86_MAX_ALU_REGS; d++) {
	    set_target(block);
	    uint8 *b = get_target();
	    int i = 0;
#define GEN(INSN, GENOP) do {			\
	insns[i++] = INSN;			\
	GENOP##rr(s, d);			\
} while (0)
#define GEN64(INSN, GENOP) do {			\
	if (X86_TARGET_64BIT)			\
	    GEN(INSN, GENOP);			\
} while (0)
#define GEN1(INSN, GENOP, OP) do {		\
	insns[i++] = INSN;			\
	GENOP##rr(OP, s, d);			\
} while (0)
#define GENA(INSN, GENOP) do {			\
	if (VALID_REG8(s) && VALID_REG8(d))	\
	    GEN(INSN "b", GENOP##B);		\
	GEN(INSN "w", GENOP##W);		\
	GEN(INSN "l", GENOP##L);		\
	GEN64(INSN "q", GENOP##Q);		\
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
	    GEN64("btq", BTQ);
	    GEN("btcw", BTCW);
	    GEN("btcl", BTCL);
	    GEN64("btcq", BTCQ);
	    GEN("btrw", BTRW);
	    GEN("btrl", BTRL);
	    GEN64("btrq", BTRQ);
	    GEN("btsw", BTSW);
	    GEN("btsl", BTSL);
	    GEN64("btsq", BTSQ);
	    GEN("imulw", IMULW);
	    GEN("imull", IMULL);
	    GEN64("imulq", IMULQ);
	    GEN1("cmove", CMOVW, X86_CC_Z);
	    GEN1("cmove", CMOVL, X86_CC_Z);
	    if (X86_TARGET_64BIT)
		GEN1("cmove", CMOVQ, X86_CC_Z);
	    GENA("test", TEST);
	    GENA("cmpxchg", CMPXCHG);
	    GENA("xadd", XADD);
	    GENA("xchg", XCHG);
	    GEN("bsfw", BSFW);
	    GEN("bsfl", BSFL);
	    GEN64("bsfq", BSFQ);
	    GEN("bsrw", BSRW);
	    GEN("bsrl", BSRL);
	    GEN64("bsrq", BSRQ);
	    if (VALID_REG8(s)) {
		GEN("movsbw", MOVSBW);
		GEN("movsbl", MOVSBL);
		GEN64("movsbq", MOVSBQ);
		GEN("movzbw", MOVZBW);
		GEN("movzbl", MOVZBL);
		GEN64("movzbq", MOVZBQ);
	    }
	    GEN("movswl", MOVSWL);
	    GEN64("movswq", MOVSWQ);
	    GEN("movzwl", MOVZWL);
	    GEN64("movzwq", MOVZWQ);
	    GEN64("movslq", MOVSLQ);
#undef  GENA
#undef  GEN1
#undef  GEN64
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
		    show_instruction(buffer, p);
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
#endif

#if TEST_INST_ALU_CNT_REG
    printf("Testing cl,reg forms\n");
    n_tests = n_failures = 0;
    for (int d = 0; d < X86_MAX_ALU_REGS; d++) {
	set_target(block);
	uint8 *b = get_target();
	int i = 0;
#define GEN(INSN, GENOP) do {		\
	insns[i++] = INSN;		\
	GENOP##rr(X86_CL, d);		\
} while (0)
#define GEN64(INSN, GENOP) do {		\
	if (X86_TARGET_64BIT)		\
	    GEN(INSN, GENOP);		\
} while (0)
#define GENA(INSN, GENOP) do {		\
	if (VALID_REG8(d))		\
	    GEN(INSN "b", GENOP##B);	\
	GEN(INSN "w", GENOP##W);	\
	GEN(INSN "l", GENOP##L);	\
	GEN64(INSN "q", GENOP##Q);	\
} while (0)
	GENA("rol", ROL);
	GENA("ror", ROR);
	GENA("rcl", RCL);
	GENA("rcr", RCR);
	GENA("shl", SHL);
	GENA("shr", SHR);
	GENA("sar", SAR);
#undef  GENA
#undef  GEN64
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
		show_instruction(buffer, p);
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
#endif

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

#if TEST_INST_ALU_IMM_REG
    printf("Testing imm,reg forms\n");
    n_tests = n_failures = 0;
    for (int j = 0; j < n_imm_tab_count; j++) {
	const uint32 value = imm_table[j];
	for (int d = 0; d < X86_MAX_ALU_REGS; d++) {
	    set_target(block);
	    uint8 *b = get_target();
	    int i = 0;
#define GEN(INSN, GENOP) do {			\
		insns[i] = INSN;		\
		modes[i] = -1;			\
		i++; GENOP##ir(value, d);	\
	} while (0)
#define GEN64(INSN, GENOP) do {			\
		if (X86_TARGET_64BIT)		\
		    GEN(INSN, GENOP);		\
	} while (0)
#define GENM(INSN, GENOP, MODE) do {		\
		insns[i] = INSN;		\
		modes[i] = MODE;		\
		i++; GENOP##ir(value, d);	\
	} while (0)
#define GENM64(INSN, GENOP, MODE) do {		\
		if (X86_TARGET_64BIT)		\
		    GENM(INSN, GENOP, MODE);	\
	} while (0)
#define GENA(INSN, GENOP) do {			\
		if (VALID_REG8(d))		\
		GEN(INSN "b", GENOP##B);	\
		GEN(INSN "w", GENOP##W);	\
		GEN(INSN "l", GENOP##L);	\
		GEN64(INSN "q", GENOP##Q);	\
	} while (0)
#define GENAM(INSN, GENOP, MODE) do {		\
		if (VALID_REG8(d))		\
		GENM(INSN "b", GENOP##B, MODE);	\
		GENM(INSN "w", GENOP##W, MODE);	\
		GENM(INSN "l", GENOP##L, MODE);	\
		GENM64(INSN "q", GENOP##Q, MODE);	\
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
	    GENM64("btq", BTQ, 1);
	    GENM("btcw", BTCW, 1);
	    GENM("btcl", BTCL, 1);
	    GENM64("btcq", BTCQ, 1);
	    GENM("btrw", BTRW, 1);
	    GENM("btrl", BTRL, 1);
	    GENM64("btrq", BTRQ, 1);
	    GENM("btsw", BTSW, 1);
	    GENM("btsl", BTSL, 1);
	    GENM64("btsq", BTSQ, 1);
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
#undef GENM64
#undef GENM
#undef GEN64
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
		    show_instruction(buffer, p);
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
#endif

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

#if TEST_INST_ALU_MEM_REG
    printf("Testing mem,reg forms\n");
    n_tests = n_failures = 0;
    for (int d = 0; d < off_table_count; d++) {
	const uint32 D = off_table[d];
	for (int B = -1; B < X86_MAX_ALU_REGS; B++) {
	    for (int I = -1; I < X86_MAX_ALU_REGS; I++) {
		if (I == X86_RSP)
		    continue;
		for (int S = 1; S < 16; S *= 2) {
		    if (I == -1 && S > 1)
			continue;
		    for (int r = 0; r < X86_MAX_ALU_REGS; r++) {
			set_target(block);
			uint8 *b = get_target();
			int i = 0;
#define GEN(INSN, GENOP) do {				\
			insns[i++] = INSN;		\
			GENOP##mr(D, B, I, S, r);	\
			} while (0)
#define GEN64(INSN, GENOP) do {				\
			if (X86_TARGET_64BIT)		\
			    GEN(INSN, GENOP);		\
			} while (0)
#define GENA(INSN, GENOP) do {				\
			if (VALID_REG8(r))		\
			    GEN(INSN "b", GENOP##B);	\
			GEN(INSN "w", GENOP##W);	\
			GEN(INSN "l", GENOP##L);	\
			GEN64(INSN "q", GENOP##Q);	\
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
			GEN64("imulq", IMULQ);
			GEN("bsfw", BSFW);
			GEN("bsfl", BSFL);
			GEN64("bsfq", BSFQ);
			GEN("bsrw", BSRW);
			GEN("bsrl", BSRL);
			GEN64("bsrq", BSRQ);
			GEN("movsbw", MOVSBW);
			GEN("movsbl", MOVSBL);
			GEN64("movsbq", MOVSBQ);
			GEN("movzbw", MOVZBW);
			GEN("movzbl", MOVZBL);
			GEN64("movzbq", MOVZBQ);
			GEN("movswl", MOVSWL);
			GEN64("movswq", MOVSWQ);
			GEN("movzwl", MOVZWL);
			GEN64("movzwq", MOVZWQ);
			GEN64("movslq", MOVSLQ);
#undef  GENA
#undef  GEN64
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
				show_instruction(buffer, p);
				n_failures++;
			    }

			    p += n;
			    i += 1;
			    n_tests++;
			    show_status(n_tests);
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
#endif

#if TEST_INST_FPU_UNARY
    printf("Testing FPU unary forms\n");
    n_tests = n_failures = 0;
    {
	set_target(block);
	uint8 *b = get_target();
	int i = 0;
#define GEN(INSN, GENOP) do {			\
	insns[i++] = INSN;			\
	GENOP();				\
} while (0)
	GEN("f2xm1", F2XM1);
	GEN("fabs", FABS);
	GEN("fchs", FCHS);
	GEN("fcompp", FCOMPP);
	GEN("fcos", FCOS);
	GEN("fdecstp", FDECSTP);
	GEN("fincstp", FINCSTP);
	GEN("fld1", FLD1);
	GEN("fldl2t", FLDL2T);
	GEN("fldl2e", FLDL2E);
	GEN("fldpi", FLDPI);
	GEN("fldlg2", FLDLG2);
	GEN("fldln2", FLDLN2);
	GEN("fldz", FLDZ);
	GEN("fnop", FNOP);
	GEN("fpatan", FPATAN);
	GEN("fprem", FPREM);
	GEN("fprem1", FPREM1);
	GEN("fptan", FPTAN);
	GEN("frndint", FRNDINT);
	GEN("fscale", FSCALE);
	GEN("fsin", FSIN);
	GEN("fsincos", FSINCOS);
	GEN("fsqrt", FSQRT);
	GEN("ftst", FTST);
	GEN("fucompp", FUCOMPP);
	GEN("fxam", FXAM);
	GEN("fxtract", FXTRACT);
	GEN("fyl2x", FYL2X);
	GEN("fyl2xp1", FYL2XP1);
#undef GEN
	int last_insn = i;
	uint8 *e = get_target();

	uint8 *p = b;
	i = 0;
	while (p < e) {
	    int n = disass_x86(buffer, (uintptr)p);
	    insn_t ii;
	    parse_insn(&ii, buffer);

	    if (!check_unary(&ii, insns[i])) {
		show_instruction(buffer, p);
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
#endif

#if TEST_INST_FPU_REG
    printf("Testing FPU reg forms\n");
    n_tests = n_failures = 0;
    for (int r = 0; r < X86_MAX_FPU_REGS; r++) {
	set_target(block);
	uint8 *b = get_target();
	int i = 0;
#define GENr(INSN, GENOP) do {			\
	insns[i] = INSN;			\
	modes[i] = 0;				\
	i++, GENOP##r(r);			\
} while (0)
#define GENr0(INSN, GENOP) do {			\
	insns[i] = INSN;			\
	modes[i] = 1;				\
	i++, GENOP##r0(r);			\
} while (0)
#define GEN0r(INSN, GENOP) do {			\
	insns[i] = INSN;			\
	modes[i] = 2;				\
	i++, GENOP##0r(r);			\
} while (0)
	GENr("fcom", FCOM);
	GENr("fcomp", FCOMP);
	GENr("ffree", FFREE);
	GENr("fxch", FXCH);
	GENr("fst", FST);
	GENr("fstp", FSTP);
	GENr("fucom", FUCOM);
	GENr("fucomp", FUCOMP);
	GENr0("fadd", FADD);
	GENr0("fcmovb", FCMOVB);
	GENr0("fcmove", FCMOVE);
	GENr0("fcmovbe", FCMOVBE);
	GENr0("fcmovu", FCMOVU);
	GENr0("fcmovnb", FCMOVNB);
	GENr0("fcmovne", FCMOVNE);
	GENr0("fcmovnbe", FCMOVNBE);
	GENr0("fcmovnu", FCMOVNU);
	GENr0("fcomi", FCOMI);
	GENr0("fcomip", FCOMIP);
	GENr0("fucomi", FUCOMI);
	GENr0("fucomip", FUCOMIP);
	GENr0("fdiv", FDIV);
	GENr0("fdivr", FDIVR);
	GENr0("fmul", FMUL);
	GENr0("fsub", FSUB);
	GENr0("fsubr", FSUBR);
#undef  GEN0r
#undef  GENr0
#undef  GENr
	int last_insn = i;
	uint8 *e = get_target();

	uint8 *p = b;
	i = 0;
	while (p < e) {
	    int n = disass_x86(buffer, (uintptr)p);
	    insn_t ii;
	    parse_insn(&ii, buffer);

	    switch (modes[i]) {
	    case 0:
		if (!check_reg(&ii, insns[i], r)) {
		    show_instruction(buffer, p);
		    n_failures++;
		}
		break;
	    case 1:
		if (!check_reg_reg(&ii, insns[i], r, 0)) {
		    show_instruction(buffer, p);
		    n_failures++;
		}
		break;
	    case 2:
		if (!check_reg_reg(&ii, insns[i], 0, r)) {
		    show_instruction(buffer, p);
		    n_failures++;
		}
		break;
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
#endif

#if TEST_INST_FPU_MEM
    printf("Testing FPU mem forms\n");
    n_tests = n_failures = 0;
    for (int d = 0; d < off_table_count; d++) {
	const uint32 D = off_table[d];
	for (int B = -1; B < X86_MAX_ALU_REGS; B++) {
	    for (int I = -1; I < X86_MAX_ALU_REGS; I++) {
		if (I == X86_RSP)
		    continue;
		for (int S = 1; S < 16; S *= 2) {
		    if (I == -1 && S > 1)
			continue;
		    set_target(block);
		    uint8 *b = get_target();
		    int i = 0;
#define GEN(INSN, GENOP) do {			\
		    insns[i++] = INSN;		\
		    GENOP##m(D, B, I, S);	\
} while (0)
		    GEN("fadds", FADDS);
		    GEN("faddl", FADDD);
		    GEN("fiadd", FIADDW);
		    GEN("fiaddl", FIADDL);
		    GEN("fbld", FBLD);
		    GEN("fbstp", FBSTP);
		    GEN("fcoms", FCOMS);
		    GEN("fcoml", FCOMD);
		    GEN("fcomps", FCOMPS);
		    GEN("fcompl", FCOMPD);
		    GEN("fdivs", FDIVS);
		    GEN("fdivl", FDIVD);
		    GEN("fidiv", FIDIVW);
		    GEN("fidivl", FIDIVL);
		    GEN("fdivrs", FDIVRS);
		    GEN("fdivrl", FDIVRD);
		    GEN("fidivr", FIDIVRW);
		    GEN("fidivrl", FIDIVRL);
		    GEN("ficom", FICOMW);
		    GEN("ficoml", FICOML);
		    GEN("ficomp", FICOMPW);
		    GEN("ficompl", FICOMPL);
		    GEN("fild", FILDW);
		    GEN("fildl", FILDL);
		    GEN("fildll", FILDQ);
		    GEN("fist", FISTW);
		    GEN("fistl", FISTL);
		    GEN("fistp", FISTPW);
		    GEN("fistpl", FISTPL);
		    GEN("fistpll", FISTPQ);
		    GEN("fisttp", FISTTPW);
		    GEN("fisttpl", FISTTPL);
		    GEN("fisttpll", FISTTPQ);
		    GEN("flds", FLDS);
		    GEN("fldl", FLDD);
		    GEN("fldt", FLDT);
		    GEN("fmuls", FMULS);
		    GEN("fmull", FMULD);
		    GEN("fimul", FIMULW);
		    GEN("fimull", FIMULL);
		    GEN("fsts", FSTS);
		    GEN("fstl", FSTD);
		    GEN("fstps", FSTPS);
		    GEN("fstpl", FSTPD);
		    GEN("fstpt", FSTPT);
		    GEN("fsubs", FSUBS);
		    GEN("fsubl", FSUBD);
		    GEN("fisub", FISUBW);
		    GEN("fisubl", FISUBL);
		    GEN("fsubrs", FSUBRS);
		    GEN("fsubrl", FSUBRD);
		    GEN("fisubr", FISUBRW);
		    GEN("fisubrl", FISUBRL);
#undef  GEN
		    int last_insn = i;
		    uint8 *e = get_target();

		    uint8 *p = b;
		    i = 0;
		    while (p < e) {
			int n = disass_x86(buffer, (uintptr)p);
			insn_t ii;
			parse_insn(&ii, buffer);

			if (!check_mem(&ii, insns[i], D, B, I, S)) {
			    show_instruction(buffer, p);
			    n_failures++;
			}

			p += n;
			i += 1;
			n_tests++;
			show_status(n_tests);
		    }
		    if (i != last_insn)
			abort();
		}
	    }
	}
    }
    printf(" done %ld/%ld\n", n_tests - n_failures, n_tests);
    n_all_tests += n_tests;
    n_all_failures += n_failures;
#endif

#if TEST_INST_MMX_REG_REG
    printf("Testing MMX reg,reg forms\n");
    n_tests = n_failures = 0;
    for (int s = 0; s < X86_MAX_MMX_REGS; s++) {
	for (int d = 0; d < X86_MAX_MMX_REGS; d++) {
	    set_target(block);
	    uint8 *b = get_target();
	    int i = 0;
#define GEN(INSN, GENOP) do {			\
	insns[i++] = INSN;			\
	MMX_##GENOP##rr(s, d);			\
} while (0)
#define GEN64(INSN, GENOP) do {			\
	if (X86_TARGET_64BIT)			\
	    GEN(INSN, GENOP);			\
} while (0)
	    GEN("movq", MOVQ);
	    GEN("packsswb", PACKSSWB);
	    GEN("packssdw", PACKSSDW);
	    GEN("packuswb", PACKUSWB);
	    GEN("paddb", PADDB);
	    GEN("paddw", PADDW);
	    GEN("paddd", PADDD);
	    GEN("paddq", PADDQ);
	    GEN("paddsb", PADDSB);
	    GEN("paddsw", PADDSW);
	    GEN("paddusb", PADDUSB);
	    GEN("paddusw", PADDUSW);
	    GEN("pand", PAND);
	    GEN("pandn", PANDN);
	    GEN("pavgb", PAVGB);
	    GEN("pavgw", PAVGW);
	    GEN("pcmpeqb", PCMPEQB);
	    GEN("pcmpeqw", PCMPEQW);
	    GEN("pcmpeqd", PCMPEQD);
	    GEN("pcmpgtb", PCMPGTB);
	    GEN("pcmpgtw", PCMPGTW);
	    GEN("pcmpgtd", PCMPGTD);
	    GEN("pmaddwd", PMADDWD);
	    GEN("pmaxsw", PMAXSW);
	    GEN("pmaxub", PMAXUB);
	    GEN("pminsw", PMINSW);
	    GEN("pminub", PMINUB);
	    GEN("pmulhuw", PMULHUW);
	    GEN("pmulhw", PMULHW);
	    GEN("pmullw", PMULLW);
	    GEN("pmuludq", PMULUDQ);
	    GEN("por", POR);
	    GEN("psadbw", PSADBW);
	    GEN("psllw", PSLLW);
	    GEN("pslld", PSLLD);
	    GEN("psllq", PSLLQ);
	    GEN("psraw", PSRAW);
	    GEN("psrad", PSRAD);
	    GEN("psrlw", PSRLW);
	    GEN("psrld", PSRLD);
	    GEN("psrlq", PSRLQ);
	    GEN("psubb", PSUBB);
	    GEN("psubw", PSUBW);
	    GEN("psubd", PSUBD);
	    GEN("psubq", PSUBQ);
	    GEN("psubsb", PSUBSB);
	    GEN("psubsw", PSUBSW);
	    GEN("psubusb", PSUBUSB);
	    GEN("psubusw", PSUBUSW);
	    GEN("punpckhbw", PUNPCKHBW);
	    GEN("punpckhwd", PUNPCKHWD);
	    GEN("punpckhdq", PUNPCKHDQ);
	    GEN("punpcklbw", PUNPCKLBW);
	    GEN("punpcklwd", PUNPCKLWD);
	    GEN("punpckldq", PUNPCKLDQ);
	    GEN("pxor", PXOR);
	    GEN("pabsb", PABSB);
	    GEN("pabsw", PABSW);
	    GEN("pabsd", PABSD);
	    GEN("phaddw", PHADDW);
	    GEN("phaddd", PHADDD);
	    GEN("phaddsw", PHADDSW);
	    GEN("phsubw", PHSUBW);
	    GEN("phsubd", PHSUBD);
	    GEN("phsubsw", PHSUBSW);
	    GEN("pmaddubsw", PMADDUBSW);
	    GEN("pmulhrsw", PMULHRSW);
	    GEN("pshufb", PSHUFB);
	    GEN("psignb", PSIGNB);
	    GEN("psignw", PSIGNW);
	    GEN("psignd", PSIGND);
#undef GEN64
#undef GEN
	    int last_insn = i;
	    uint8 *e = get_target();

	    uint8 *p = b;
	    i = 0;
	    while (p < e) {
		int n = disass_x86(buffer, (uintptr)p);
		insn_t ii;
		parse_insn(&ii, buffer);

		if (!check_reg_reg(&ii, insns[i], s, d)) {
		    show_instruction(buffer, p);
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
#endif

    static const uint8 imm8_table[] = {
	0x00, 0x01, 0x02, 0x03,
	0x06, 0x07, 0x08, 0x09,
	0x0e, 0x0f, 0x10, 0x11,
	0x1e, 0x1f, 0x20, 0x21,
	0xfc, 0xfd, 0xfe, 0xff,
    };
    const int n_imm8_tab_count = sizeof(imm8_table)/sizeof(imm8_table[0]);

#if TEST_INST_MMX_IMM_REG
    printf("Testing imm,reg forms\n");
    n_tests = n_failures = 0;
    for (int j = 0; j < n_imm8_tab_count; j++) {
	const uint8 value = imm8_table[j];
	for (int d = 0; d < X86_MAX_MMX_REGS; d++) {
	    set_target(block);
	    uint8 *b = get_target();
	    int i = 0;
#define GEN(INSN, GENOP) do {			\
	    insns[i] = INSN;			\
	    modes[i] = 1;			\
	    i++; MMX_##GENOP##ir(value, d);	\
} while (0)
	    GEN("psllw", PSLLW);
	    GEN("pslld", PSLLD);
	    GEN("psllq", PSLLQ);
	    GEN("psraw", PSRAW);
	    GEN("psrad", PSRAD);
	    GEN("psrlw", PSRLW);
	    GEN("psrld", PSRLD);
	    GEN("psrlq", PSRLQ);
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
		    show_instruction(buffer, p);
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
#endif

#if TEST_INST_MMX_MEM_REG
    printf("Testing MMX mem,reg forms\n");
    n_tests = n_failures = 0;
    for (int d = 0; d < off_table_count; d++) {
	const uint32 D = off_table[d];
	for (int B = -1; B < X86_MAX_ALU_REGS; B++) {
	    for (int I = -1; I < X86_MAX_ALU_REGS; I++) {
		if (I == X86_RSP)
		    continue;
		for (int S = 1; S < 16; S *= 2) {
		    if (I == -1 && S > 1)
			continue;
		    for (int r = 0; r < X86_MAX_MMX_REGS; r++) {
			set_target(block);
			uint8 *b = get_target();
			int i = 0;
#define _GENrm(INSN, GENOP) do {		\
	insns[i] = INSN;			\
	modes[i] = 0;				\
	i++; MMX_##GENOP##rm(r, D, B, I, S);	\
} while (0)
#define _GENmr(INSN, GENOP) do {		\
	insns[i] = INSN;			\
	modes[i] = 1;				\
	i++; MMX_##GENOP##mr(D, B, I, S, r);	\
} while (0)
#define GEN(INSN, GENOP) do {			\
	_GENmr(INSN, GENOP);			\
} while (0)
			_GENmr("movd", MOVD);
			_GENrm("movd", MOVD);
			_GENmr("movq", MOVQ);
			_GENrm("movq", MOVQ);
			GEN("packsswb", PACKSSWB);
			GEN("packssdw", PACKSSDW);
			GEN("packuswb", PACKUSWB);
			GEN("paddb", PADDB);
			GEN("paddw", PADDW);
			GEN("paddd", PADDD);
			GEN("paddq", PADDQ);
			GEN("paddsb", PADDSB);
			GEN("paddsw", PADDSW);
			GEN("paddusb", PADDUSB);
			GEN("paddusw", PADDUSW);
			GEN("pand", PAND);
			GEN("pandn", PANDN);
			GEN("pavgb", PAVGB);
			GEN("pavgw", PAVGW);
			GEN("pcmpeqb", PCMPEQB);
			GEN("pcmpeqw", PCMPEQW);
			GEN("pcmpeqd", PCMPEQD);
			GEN("pcmpgtb", PCMPGTB);
			GEN("pcmpgtw", PCMPGTW);
			GEN("pcmpgtd", PCMPGTD);
			GEN("pmaddwd", PMADDWD);
			GEN("pmaxsw", PMAXSW);
			GEN("pmaxub", PMAXUB);
			GEN("pminsw", PMINSW);
			GEN("pminub", PMINUB);
			GEN("pmulhuw", PMULHUW);
			GEN("pmulhw", PMULHW);
			GEN("pmullw", PMULLW);
			GEN("pmuludq", PMULUDQ);
			GEN("por", POR);
			GEN("psadbw", PSADBW);
			GEN("psllw", PSLLW);
			GEN("pslld", PSLLD);
			GEN("psllq", PSLLQ);
			GEN("psraw", PSRAW);
			GEN("psrad", PSRAD);
			GEN("psrlw", PSRLW);
			GEN("psrld", PSRLD);
			GEN("psrlq", PSRLQ);
			GEN("psubb", PSUBB);
			GEN("psubw", PSUBW);
			GEN("psubd", PSUBD);
			GEN("psubq", PSUBQ);
			GEN("psubsb", PSUBSB);
			GEN("psubsw", PSUBSW);
			GEN("psubusb", PSUBUSB);
			GEN("psubusw", PSUBUSW);
			GEN("punpckhbw", PUNPCKHBW);
			GEN("punpckhwd", PUNPCKHWD);
			GEN("punpckhdq", PUNPCKHDQ);
			GEN("punpcklbw", PUNPCKLBW);
			GEN("punpcklwd", PUNPCKLWD);
			GEN("punpckldq", PUNPCKLDQ);
			GEN("pxor", PXOR);
			GEN("pabsb", PABSB);
			GEN("pabsw", PABSW);
			GEN("pabsd", PABSD);
			GEN("phaddw", PHADDW);
			GEN("phaddd", PHADDD);
			GEN("phaddsw", PHADDSW);
			GEN("phsubw", PHSUBW);
			GEN("phsubd", PHSUBD);
			GEN("phsubsw", PHSUBSW);
			GEN("pmaddubsw", PMADDUBSW);
			GEN("pmulhrsw", PMULHRSW);
			GEN("pshufb", PSHUFB);
			GEN("psignb", PSIGNB);
			GEN("psignw", PSIGNW);
			GEN("psignd", PSIGND);
#undef GEN
#undef _GENmr
#undef _GENrm
			int last_insn = i;
			uint8 *e = get_target();

			uint8 *p = b;
			i = 0;
			while (p < e) {
			    int n = disass_x86(buffer, (uintptr)p);
			    insn_t ii;
			    parse_insn(&ii, buffer);

			    if (!check_mem_reg(&ii, insns[i], D, B, I, S, r, modes[i])) {
				show_instruction(buffer, p);
				n_failures++;
			    }

			    p += n;
			    i += 1;
			    n_tests++;
			    show_status(n_tests);
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
#endif

#if TEST_INST_SSE_REG_REG
    printf("Testing SSE reg,reg forms\n");
    n_tests = n_failures = 0;
    for (int s = 0; s < X86_MAX_SSE_REGS; s++) {
	for (int d = 0; d < X86_MAX_SSE_REGS; d++) {
	    set_target(block);
	    uint8 *b = get_target();
	    int i = 0;
#define GEN(INSN, GENOP) do {			\
	insns[i++] = INSN;			\
	GENOP##rr(s, d);			\
} while (0)
#define GEN64(INSN, GENOP) do {			\
	if (X86_TARGET_64BIT)			\
	    GEN(INSN, GENOP);			\
} while (0)
#define GEN1(INSN, GENOP) do {			\
	GEN(INSN "s", GENOP##S);		\
	GEN(INSN "d", GENOP##D);		\
} while (0)
#define GENA(INSN, GENOP) do {			\
	GEN1(INSN "s", GENOP##S);		\
	GEN1(INSN "p", GENOP##P);		\
} while (0)
#define GENI(INSN, GENOP, IMM) do {		\
	insns[i++] = INSN;			\
	GENOP##rr(IMM, s, d);			\
} while (0)
#define GENI1(INSN, GENOP, IMM) do {		\
	GENI(INSN "s", GENOP##S, IMM);		\
	GENI(INSN "d", GENOP##D, IMM);		\
} while (0)
#define GENIA(INSN, GENOP, IMM) do {		\
	GENI1(INSN "s", GENOP##S, IMM);		\
	GENI1(INSN "p", GENOP##P, IMM);		\
} while (0)
	    GEN1("andp", ANDP);
	    GEN1("andnp", ANDNP);
	    GEN1("orp", ORP);
	    GEN1("xorp", XORP);
	    GENA("add", ADD);
	    GENA("sub", SUB);
	    GENA("mul", MUL);
	    GENA("div", DIV);
	    GEN1("comis", COMIS);
	    GEN1("ucomis", UCOMIS);
	    GENA("min", MIN);
	    GENA("max", MAX);
	    GEN("rcpss", RCPSS);
	    GEN("rcpps", RCPPS);
	    GEN("rsqrtss", RSQRTSS);
	    GEN("rsqrtps", RSQRTPS);
	    GENA("sqrt", SQRT);
	    GENIA("cmpeq", CMP, X86_SSE_CC_EQ);
	    GENIA("cmplt", CMP, X86_SSE_CC_LT);
	    GENIA("cmple", CMP, X86_SSE_CC_LE);
	    GENIA("cmpunord", CMP, X86_SSE_CC_U);
	    GENIA("cmpneq", CMP, X86_SSE_CC_NEQ);
	    GENIA("cmpnlt", CMP, X86_SSE_CC_NLT);
	    GENIA("cmpnle", CMP, X86_SSE_CC_NLE);
	    GENIA("cmpord", CMP, X86_SSE_CC_O);
	    GEN1("movap", MOVAP);
	    GEN("movdqa", MOVDQA);
	    GEN("movdqu", MOVDQU);
	    GEN("movd", MOVDXD);
	    GEN64("movd", MOVQXD);	// FIXME: disass bug? "movq" expected
	    GEN("movd", MOVDXS);
	    GEN64("movd", MOVQXS);	// FIXME: disass bug? "movq" expected
	    GEN("cvtdq2pd", CVTDQ2PD);
	    GEN("cvtdq2ps", CVTDQ2PS);
	    GEN("cvtpd2dq", CVTPD2DQ);
	    GEN("cvtpd2ps", CVTPD2PS);
	    GEN("cvtps2dq", CVTPS2DQ);
	    GEN("cvtps2pd", CVTPS2PD);
	    GEN("cvtsd2si", CVTSD2SIL);
	    GEN64("cvtsd2siq", CVTSD2SIQ);
	    GEN("cvtsd2ss", CVTSD2SS);
	    GEN("cvtsi2sd", CVTSI2SDL);
	    GEN64("cvtsi2sdq", CVTSI2SDQ);
	    GEN("cvtsi2ss", CVTSI2SSL);
	    GEN64("cvtsi2ssq", CVTSI2SSQ);
	    GEN("cvtss2sd", CVTSS2SD);
	    GEN("cvtss2si", CVTSS2SIL);
	    GEN64("cvtss2siq", CVTSS2SIQ);
	    GEN("cvttpd2dq", CVTTPD2DQ);
	    GEN("cvttps2dq", CVTTPS2DQ);
	    GEN("cvttsd2si", CVTTSD2SIL);
	    GEN64("cvttsd2siq", CVTTSD2SIQ);
	    GEN("cvttss2si", CVTTSS2SIL);
	    GEN64("cvttss2siq", CVTTSS2SIQ);
	    if (s < 8) {
		// MMX source register
		GEN("cvtpi2pd", CVTPI2PD);
		GEN("cvtpi2ps", CVTPI2PS);
	    }
	    if (d < 8) {
		// MMX dest register
		GEN("cvtpd2pi", CVTPD2PI);
		GEN("cvtps2pi", CVTPS2PI);
		GEN("cvttpd2pi", CVTTPD2PI);
		GEN("cvttps2pi", CVTTPS2PI);
	    }
#undef  GENIA
#undef  GENI1
#undef  GENI
#undef  GENA
#undef  GEN1
#undef  GEN64
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
		    show_instruction(buffer, p);
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
#endif

#if TEST_INST_SSE_MEM_REG
    printf("Testing SSE mem,reg forms\n");
    n_tests = n_failures = 0;
    for (int d = 0; d < off_table_count; d++) {
	const uint32 D = off_table[d];
	for (int B = -1; B < X86_MAX_ALU_REGS; B++) {
	    for (int I = -1; I < X86_MAX_ALU_REGS; I++) {
		if (I == X86_RSP)
		    continue;
		for (int S = 1; S < 16; S *= 2) {
		    if (I == -1 && S > 1)
			continue;
		    for (int r = 0; r < X86_MAX_SSE_REGS; r++) {
			set_target(block);
			uint8 *b = get_target();
			int i = 0;
#define GEN(INSN, GENOP) do {			\
	insns[i++] = INSN;			\
	GENOP##mr(D, B, I, S, r);		\
} while (0)
#define GEN64(INSN, GENOP) do {			\
	if (X86_TARGET_64BIT)			\
	    GEN(INSN, GENOP);			\
} while (0)
#define GEN1(INSN, GENOP) do {			\
	GEN(INSN "s", GENOP##S);		\
	GEN(INSN "d", GENOP##D);		\
} while (0)
#define GENA(INSN, GENOP) do {			\
	GEN1(INSN "s", GENOP##S);		\
	GEN1(INSN "p", GENOP##P);		\
} while (0)
#define GENI(INSN, GENOP, IMM) do {		\
	insns[i++] = INSN;			\
	GENOP##mr(IMM, D, B, I, S, r);		\
} while (0)
#define GENI1(INSN, GENOP, IMM) do {		\
	GENI(INSN "s", GENOP##S, IMM);		\
	GENI(INSN "d", GENOP##D, IMM);		\
} while (0)
#define GENIA(INSN, GENOP, IMM) do {		\
	GENI1(INSN "s", GENOP##S, IMM);		\
	GENI1(INSN "p", GENOP##P, IMM);		\
} while (0)
			GEN1("andp", ANDP);
			GEN1("andnp", ANDNP);
			GEN1("orp", ORP);
			GEN1("xorp", XORP);
			GENA("add", ADD);
			GENA("sub", SUB);
			GENA("mul", MUL);
			GENA("div", DIV);
			GEN1("comis", COMIS);
			GEN1("ucomis", UCOMIS);
			GENA("min", MIN);
			GENA("max", MAX);
			GEN("rcpss", RCPSS);
			GEN("rcpps", RCPPS);
			GEN("rsqrtss", RSQRTSS);
			GEN("rsqrtps", RSQRTPS);
			GENA("sqrt", SQRT);
			GENIA("cmpeq", CMP, X86_SSE_CC_EQ);
			GENIA("cmplt", CMP, X86_SSE_CC_LT);
			GENIA("cmple", CMP, X86_SSE_CC_LE);
			GENIA("cmpunord", CMP, X86_SSE_CC_U);
			GENIA("cmpneq", CMP, X86_SSE_CC_NEQ);
			GENIA("cmpnlt", CMP, X86_SSE_CC_NLT);
			GENIA("cmpnle", CMP, X86_SSE_CC_NLE);
			GENIA("cmpord", CMP, X86_SSE_CC_O);
			GEN1("movap", MOVAP);
			GEN("movdqa", MOVDQA);
			GEN("movdqu", MOVDQU);
#if 0
			// FIXME: extraneous REX bits generated
			GEN("movd", MOVDXD);
			GEN64("movd", MOVQXD);	// FIXME: disass bug? "movq" expected
#endif
			GEN("cvtdq2pd", CVTDQ2PD);
			GEN("cvtdq2ps", CVTDQ2PS);
			GEN("cvtpd2dq", CVTPD2DQ);
			GEN("cvtpd2ps", CVTPD2PS);
			GEN("cvtps2dq", CVTPS2DQ);
			GEN("cvtps2pd", CVTPS2PD);
			GEN("cvtsd2si", CVTSD2SIL);
			GEN64("cvtsd2siq", CVTSD2SIQ);
			GEN("cvtsd2ss", CVTSD2SS);
			GEN("cvtsi2sd", CVTSI2SDL);
			GEN64("cvtsi2sdq", CVTSI2SDQ);
			GEN("cvtsi2ss", CVTSI2SSL);
			GEN64("cvtsi2ssq", CVTSI2SSQ);
			GEN("cvtss2sd", CVTSS2SD);
			GEN("cvtss2si", CVTSS2SIL);
			GEN64("cvtss2siq", CVTSS2SIQ);
			GEN("cvttpd2dq", CVTTPD2DQ);
			GEN("cvttps2dq", CVTTPS2DQ);
			GEN("cvttsd2si", CVTTSD2SIL);
			GEN64("cvttsd2siq", CVTTSD2SIQ);
			GEN("cvttss2si", CVTTSS2SIL);
			GEN64("cvttss2siq", CVTTSS2SIQ);
			if (r < 8) {
			    // MMX dest register
			    GEN("cvtpd2pi", CVTPD2PI);
			    GEN("cvtps2pi", CVTPS2PI);
			    GEN("cvttpd2pi", CVTTPD2PI);
			    GEN("cvttps2pi", CVTTPS2PI);
			}
#undef  GENIA
#undef  GENI1
#undef  GENI
#undef  GENA
#undef  GEN1
#undef  GEN64
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
				show_instruction(buffer, p);
				n_failures++;
			    }

			    p += n;
			    i += 1;
			    n_tests++;
			    show_status(n_tests);
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
#endif

    printf("\n");
    printf("All %ld tests run, %ld failures\n", n_all_tests, n_all_failures);
}
