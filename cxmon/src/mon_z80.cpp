/*
 *  mon_z80.cpp - Z80 disassembler
 *
 *  cxmon (C) 1997-2007 Christian Bauer, Marc Hellwig
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

#include <stdarg.h>

#include "mon.h"
#include "mon_disass.h"


// Addressing modes
enum AddrMode {
	A_IMPL,
	A_IMM8,		// xx
	A_IMM16,	// xxxx
	A_ABS8,		// (xx)
	A_ABS16,	// (xxxx)
	A_REL,		// relative
	A_A,		// a
	A_HL,		// hl or ix or iy
	A_SP,		// sp
	A_REG1,		// 8-bit register (bits 0..2 of opcode) or (hl)/(ix+d)/(iy+d)
	A_REG1X,	// 8-bit register (bits 0..2 of opcode) or (hl)/(ix+d)/(iy+d), don't substitute h or l on prefixes
	A_REG2,		// 8-bit register (bits 3..5 of opcode) or (hl)/(ix+d)/(iy+d)
	A_REG2X,	// 8-bit register (bits 3..5 of opcode) or (hl)/(ix+d)/(iy+d), don't substitute h or l on prefixes
	A_REG3,		// 16-bit register (bits 4..5 of opcode) bc/de/hl/sp
	A_REG4,		// 16-bit register (bits 4..5 of opcode) bc/de/hl/af
	A_COND,		// condition code (bits 3..5 of opcode)
	A_COND2,	// condition code (bits 3..4 of opcode)
	A_BIT,		// bit number (bits 3..5 of opcode)
	A_BIT_REG1,	// bit number (bits 3..5 of opcode) followed by 8-bit register (bits 0..2 of opcode)
	A_RST,		// restart
	A_BC_IND,	// (bc)
	A_DE_IND,	// (de)
	A_HL_IND,	// (hl) or (ix) or (iy)
	A_XY_IND,	// (ix+d) or (iy+d)
	A_SP_IND,	// (sp)
	A_DE_HL,	// de,hl
	A_AF_AF,	// af,af'
};

// Mnemonics
enum Mnemonic {
	M_ADC, M_ADD, M_AND, M_BIT, M_CALL, M_CCF, M_CP, M_CPD, M_CPDR, M_CPI,
	M_CPIR, M_CPL, M_DAA, M_DEC, M_DI, M_DJNZ, M_EI, M_EX, M_EXX, M_HALT,
	M_IM0, M_IM1, M_IM2, M_IN, M_INC, M_IND, M_INDR, M_INI, M_INIR, M_JP,
	M_JR, M_LD, M_LDD, M_LDDR, M_LDI, M_LDIR, M_NEG, M_NOP, M_OR, M_OTDR,
	M_OTIR, M_OUT, M_OUTD, M_OUTI, M_POP, M_PUSH, M_RES, M_RET, M_RETI,
	M_RETN, M_RL, M_RLA, M_RLC, M_RLCA, M_RLD, M_RR, M_RRA, M_RRC, M_RRCA,
	M_RRD, M_RST, M_SBC, M_SCF, M_SET, M_SL1, M_SLA, M_SRA, M_SRL, M_SUB,
	M_XOR,
	M_ILLEGAL,

	M_MAXIMUM
};

// Chars for each mnemonic
static const char mnem_1[] = "aaabccccccccddddeeehiiiiiiiiijjlllllnnoooooopprrrrrrrrrrrrrrrssssssssx?";
static const char mnem_2[] = "ddniacppppppaeijixxammmnnnnnnprdddddeorttuuuoueeeelllllrrrrrsbcellrruo ";
static const char mnem_3[] = "cddtlf ddiilac n  xl    cddii   ddiigp ditttpssttt accd accdtcft1aalbr ";
static const char mnem_4[] = "    l   r r    z   t012   r r    r r   rr di h  in   a    a            ";

// Mnemonic for each opcode
static const Mnemonic mnemonic[256] = {
	M_NOP , M_LD , M_LD , M_INC    , M_INC , M_DEC    , M_LD  , M_RLCA,	// 00
	M_EX  , M_ADD, M_LD , M_DEC    , M_INC , M_DEC    , M_LD  , M_RRCA,
	M_DJNZ, M_LD , M_LD , M_INC    , M_INC , M_DEC    , M_LD  , M_RLA ,	// 10
	M_JR  , M_ADD, M_LD , M_DEC    , M_INC , M_DEC    , M_LD  , M_RRA ,
	M_JR  , M_LD , M_LD , M_INC    , M_INC , M_DEC    , M_LD  , M_DAA ,	// 20
	M_JR  , M_ADD, M_LD , M_DEC    , M_INC , M_DEC    , M_LD  , M_CPL ,
	M_JR  , M_LD , M_LD , M_INC    , M_INC , M_DEC    , M_LD  , M_SCF ,	// 30
	M_JR  , M_ADD, M_LD , M_DEC    , M_INC , M_DEC    , M_LD  , M_CCF ,
	M_LD  , M_LD , M_LD , M_LD     , M_LD  , M_LD     , M_LD  , M_LD  ,	// 40
	M_LD  , M_LD , M_LD , M_LD     , M_LD  , M_LD     , M_LD  , M_LD  ,
	M_LD  , M_LD , M_LD , M_LD     , M_LD  , M_LD     , M_LD  , M_LD  ,	// 50
	M_LD  , M_LD , M_LD , M_LD     , M_LD  , M_LD     , M_LD  , M_LD  ,
	M_LD  , M_LD , M_LD , M_LD     , M_LD  , M_LD     , M_LD  , M_LD  ,	// 60
	M_LD  , M_LD , M_LD , M_LD     , M_LD  , M_LD     , M_LD  , M_LD  ,
	M_LD  , M_LD , M_LD , M_LD     , M_LD  , M_LD     , M_HALT, M_LD  ,	// 70
	M_LD  , M_LD , M_LD , M_LD     , M_LD  , M_LD     , M_LD  , M_LD  ,
	M_ADD , M_ADD, M_ADD, M_ADD    , M_ADD , M_ADD    , M_ADD , M_ADD ,	// 80
	M_ADC , M_ADC, M_ADC, M_ADC    , M_ADC , M_ADC    , M_ADC , M_ADC ,
	M_SUB , M_SUB, M_SUB, M_SUB    , M_SUB , M_SUB    , M_SUB , M_SUB ,	// 90
	M_SBC , M_SBC, M_SBC, M_SBC    , M_SBC , M_SBC    , M_SBC , M_SBC ,
	M_AND , M_AND, M_AND, M_AND    , M_AND , M_AND    , M_AND , M_AND ,	// a0
	M_XOR , M_XOR, M_XOR, M_XOR    , M_XOR , M_XOR    , M_XOR , M_XOR ,
	M_OR  , M_OR , M_OR , M_OR     , M_OR  , M_OR     , M_OR  , M_OR  ,	// b0
	M_CP  , M_CP , M_CP , M_CP     , M_CP  , M_CP     , M_CP  , M_CP  ,
	M_RET , M_POP, M_JP , M_JP     , M_CALL, M_PUSH   , M_ADD , M_RST ,	// c0
	M_RET , M_RET, M_JP , M_ILLEGAL, M_CALL, M_CALL   , M_ADC , M_RST ,
	M_RET , M_POP, M_JP , M_OUT    , M_CALL, M_PUSH   , M_SUB , M_RST ,	// d0
	M_RET , M_EXX, M_JP , M_IN     , M_CALL, M_ILLEGAL, M_SBC , M_RST ,
	M_RET , M_POP, M_JP , M_EX     , M_CALL, M_PUSH   , M_AND , M_RST ,	// e0
	M_RET , M_JP , M_JP , M_EX     , M_CALL, M_ILLEGAL, M_XOR , M_RST ,
	M_RET , M_POP, M_JP , M_DI     , M_CALL, M_PUSH   , M_OR  , M_RST ,	// f0
	M_RET , M_LD , M_JP , M_EI     , M_CALL, M_ILLEGAL, M_CP  , M_RST
};

// Source/destination addressing modes for each opcode
#define A(d,s) (((A_ ## d) << 8) | (A_ ## s))

static const int adr_mode[256] = {
	A(IMPL,IMPL)  , A(REG3,IMM16) , A(BC_IND,A)  , A(REG3,IMPL) , A(REG2,IMPL) , A(REG2,IMPL) , A(REG2,IMM8) , A(IMPL,IMPL) ,	// 00
	A(AF_AF,IMPL) , A(HL,REG3)    , A(A,BC_IND)  , A(REG3,IMPL) , A(REG2,IMPL) , A(REG2,IMPL) , A(REG2,IMM8) , A(IMPL,IMPL) ,
	A(REL,IMPL)   , A(REG3,IMM16) , A(DE_IND,A)  , A(REG3,IMPL) , A(REG2,IMPL) , A(REG2,IMPL) , A(REG2,IMM8) , A(IMPL,IMPL) ,	// 10
	A(REL,IMPL)   , A(HL,REG3)    , A(A,DE_IND)  , A(REG3,IMPL) , A(REG2,IMPL) , A(REG2,IMPL) , A(REG2,IMM8) , A(IMPL,IMPL) ,
	A(COND2,REL)  , A(REG3,IMM16) , A(ABS16,HL)  , A(REG3,IMPL) , A(REG2,IMPL) , A(REG2,IMPL) , A(REG2,IMM8) , A(IMPL,IMPL) ,	// 20
	A(COND2,REL)  , A(HL,REG3)    , A(HL,ABS16)  , A(REG3,IMPL) , A(REG2,IMPL) , A(REG2,IMPL) , A(REG2,IMM8) , A(IMPL,IMPL) ,
	A(COND2,REL)  , A(REG3,IMM16) , A(ABS16,A)   , A(REG3,IMPL) , A(REG2,IMPL) , A(REG2,IMPL) , A(REG2,IMM8) , A(IMPL,IMPL) ,	// 30
	A(COND2,REL)  , A(HL,REG3)    , A(A,ABS16)   , A(REG3,IMPL) , A(REG2,IMPL) , A(REG2,IMPL) , A(REG2,IMM8) , A(IMPL,IMPL) ,
	A(REG2,REG1)  , A(REG2,REG1)  , A(REG2,REG1) , A(REG2,REG1) , A(REG2,REG1) , A(REG2,REG1) , A(REG2X,REG1), A(REG2,REG1) ,	// 40
	A(REG2,REG1)  , A(REG2,REG1)  , A(REG2,REG1) , A(REG2,REG1) , A(REG2,REG1) , A(REG2,REG1) , A(REG2X,REG1), A(REG2,REG1) ,
	A(REG2,REG1)  , A(REG2,REG1)  , A(REG2,REG1) , A(REG2,REG1) , A(REG2,REG1) , A(REG2,REG1) , A(REG2X,REG1), A(REG2,REG1) ,	// 50
	A(REG2,REG1)  , A(REG2,REG1)  , A(REG2,REG1) , A(REG2,REG1) , A(REG2,REG1) , A(REG2,REG1) , A(REG2X,REG1), A(REG2,REG1) ,
	A(REG2,REG1)  , A(REG2,REG1)  , A(REG2,REG1) , A(REG2,REG1) , A(REG2,REG1) , A(REG2,REG1) , A(REG2X,REG1), A(REG2,REG1) ,	// 60
	A(REG2,REG1)  , A(REG2,REG1)  , A(REG2,REG1) , A(REG2,REG1) , A(REG2,REG1) , A(REG2,REG1) , A(REG2X,REG1), A(REG2,REG1) ,
	A(REG2,REG1X) , A(REG2,REG1X) , A(REG2,REG1X), A(REG2,REG1X), A(REG2,REG1X), A(REG2,REG1X), A(IMPL,IMPL) , A(REG2,REG1X),	// 70
	A(REG2,REG1)  , A(REG2,REG1)  , A(REG2,REG1) , A(REG2,REG1) , A(REG2,REG1) , A(REG2,REG1) , A(REG2X,REG1), A(REG2,REG1) ,
	A(A,REG1)     , A(A,REG1)     , A(A,REG1)    , A(A,REG1)    , A(A,REG1)    , A(A,REG1)    , A(A,REG1)    , A(A,REG1)    ,	// 80
	A(A,REG1)     , A(A,REG1)     , A(A,REG1)    , A(A,REG1)    , A(A,REG1)    , A(A,REG1)    , A(A,REG1)    , A(A,REG1)    ,
	A(REG1,IMPL)  , A(REG1,IMPL)  , A(REG1,IMPL) , A(REG1,IMPL) , A(REG1,IMPL) , A(REG1,IMPL) , A(REG1,IMPL) , A(REG1,IMPL) ,	// 90
	A(A,REG1)     , A(A,REG1)     , A(A,REG1)    , A(A,REG1)    , A(A,REG1)    , A(A,REG1)    , A(A,REG1)    , A(A,REG1)    ,
	A(REG1,IMPL)  , A(REG1,IMPL)  , A(REG1,IMPL) , A(REG1,IMPL) , A(REG1,IMPL) , A(REG1,IMPL) , A(REG1,IMPL) , A(REG1,IMPL) ,	// a0
	A(REG1,IMPL)  , A(REG1,IMPL)  , A(REG1,IMPL) , A(REG1,IMPL) , A(REG1,IMPL) , A(REG1,IMPL) , A(REG1,IMPL) , A(REG1,IMPL) ,
	A(REG1,IMPL)  , A(REG1,IMPL)  , A(REG1,IMPL) , A(REG1,IMPL) , A(REG1,IMPL) , A(REG1,IMPL) , A(REG1,IMPL) , A(REG1,IMPL) ,	// b0
	A(REG1,IMPL)  , A(REG1,IMPL)  , A(REG1,IMPL) , A(REG1,IMPL) , A(REG1,IMPL) , A(REG1,IMPL) , A(REG1,IMPL) , A(REG1,IMPL) ,
	A(COND,IMPL)  , A(REG4,IMPL)  , A(COND,IMM16), A(IMM16,IMPL), A(COND,IMM16), A(REG4,IMPL) , A(A,IMM8)    , A(RST,IMPL)  ,	// c0
	A(COND,IMPL)  , A(IMPL,IMPL)  , A(COND,IMM16), A(IMPL,IMPL) , A(COND,IMM16), A(IMM16,IMPL), A(A,IMM8)    , A(RST,IMPL)  ,
	A(COND,IMPL)  , A(REG4,IMPL)  , A(COND,IMM16), A(ABS8,A)    , A(COND,IMM16), A(REG4,IMPL) , A(IMM8,IMPL) , A(RST,IMPL)  ,	// d0
	A(COND,IMPL)  , A(IMPL,IMPL)  , A(COND,IMM16), A(A,ABS8)    , A(COND,IMM16), A(IMPL,IMPL) , A(A,IMM8)    , A(RST,IMPL)  ,
	A(COND,IMPL)  , A(REG4,IMPL)  , A(COND,IMM16), A(SP_IND,HL) , A(COND,IMM16), A(REG4,IMPL) , A(IMM8,IMPL) , A(RST,IMPL)  ,	// e0
	A(COND,IMPL)  , A(HL_IND,IMPL), A(COND,IMM16), A(DE_HL,IMPL), A(COND,IMM16), A(IMPL,IMPL) , A(IMM8,IMPL) , A(RST,IMPL)  ,
	A(COND,IMPL)  , A(REG4,IMPL)  , A(COND,IMM16), A(IMPL,IMPL) , A(COND,IMM16), A(REG4,IMPL) , A(IMM8,IMPL) , A(RST,IMPL)  ,	// f0
	A(COND,IMPL)  , A(SP,HL)      , A(COND,IMM16), A(IMPL,IMPL) , A(COND,IMM16), A(IMPL,IMPL) , A(IMM8,IMPL) , A(RST,IMPL)
};


/*
 *  sprintf into a "stream"
 */

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


/*
 *  Disassemble one instruction, return number of bytes
 */

static const char *reg_name[] = {"b", "c", "d", "e", "h", "l", "*", "a"};
static const char *reg_name_ix[] = {"b", "c", "d", "e", "hx", "lx", "*", "a"};	// undoc
static const char *reg_name_iy[] = {"b", "c", "d", "e", "hy", "ly", "*", "a"};	// undoc
static const char *reg_name_16[] = {"bc", "de", "hl", "sp"};
static const char *reg_name_16_2[] = {"bc", "de", "hl", "af"};
static const char *cond_name[] = {"nz", "z", "nc", "c", "po", "pe", "p", "m"};

static void operand(SFILE *f, char mode, uint32 &adr, uint8 op, bool ix, bool iy)
{
	switch (mode) {
		case A_IMPL:
			break;

		case A_IMM8:
			mon_sprintf(f, "$%02x", mon_read_byte(adr)); adr++;
			break;

		case A_IMM16:
			mon_sprintf(f, "$%04x", (mon_read_byte(adr + 1) << 8) | mon_read_byte(adr)); adr += 2;
			break;

		case A_ABS8:
			mon_sprintf(f, "($%02x)", mon_read_byte(adr)); adr++;
			break;

		case A_ABS16:
			mon_sprintf(f, "($%04x)", (mon_read_byte(adr + 1) << 8) | mon_read_byte(adr)); adr += 2;
			break;

		case A_REL:
			mon_sprintf(f, "$%04x", (adr + 1 + (int8)mon_read_byte(adr)) & 0xffff); adr++;
			break;

		case A_A:
			mon_sprintf(f, "a");
			break;

		case A_HL:
			mon_sprintf(f, ix ? "ix" : (iy ? "iy" : "hl"));
			break;

		case A_SP:
			mon_sprintf(f, "sp");
			break;

		case A_REG1:
		case A_REG1X: {
			int reg = op & 7;
			if (reg == 6) {
				if (ix || iy) {
					mon_sprintf(f, "(%s+$%02x)", ix ? "ix" : "iy", mon_read_byte(adr)); adr++;
				} else {
					mon_sprintf(f, "(hl)");
				}
			} else if (mode == A_REG1) {
				mon_sprintf(f, "%s", ix ? reg_name_ix[reg] : (iy ? reg_name_iy[reg] : reg_name[reg]));
			} else {
				mon_sprintf(f, "%s", reg_name[reg]);
			}
			break;
		}

		case A_REG2:
		case A_REG2X: {
			int reg = (op >> 3) & 7;
			if (reg == 6) {
				if (ix || iy) {
					mon_sprintf(f, "(%s+$%02x)", ix ? "ix" : "iy", mon_read_byte(adr)); adr++;
				} else {
					mon_sprintf(f, "(hl)");
				}
			} else if (mode == A_REG2) {
				mon_sprintf(f, "%s", ix ? reg_name_ix[reg] : (iy ? reg_name_iy[reg] : reg_name[reg]));
			} else {
				mon_sprintf(f, "%s", reg_name[reg]);
			}
			break;
		}

		case A_REG3: {
			int reg = (op >> 4) & 3;
			if (reg == 2 && (ix || iy)) {
				mon_sprintf(f, ix ? "ix" : "iy");
			} else {
				mon_sprintf(f, reg_name_16[reg]);
			}
			break;
		}

		case A_REG4: {
			int reg = (op >> 4) & 3;
			if (reg == 2 && (ix || iy)) {
				mon_sprintf(f, ix ? "ix" : "iy");
			} else {
				mon_sprintf(f, reg_name_16_2[reg]);
			}
			break;
		}

		case A_COND:
			mon_sprintf(f, cond_name[(op >> 3) & 7]);
			break;

		case A_COND2:
			mon_sprintf(f, cond_name[(op >> 3) & 3]);
			break;

		case A_BIT:
			mon_sprintf(f, "%d", (op >> 3) & 7);
			break;

		case A_BIT_REG1: { // undoc
			int reg = op & 7;
			if (reg == 6) {
				mon_sprintf(f, "%d", (op >> 3) & 7);
			} else {
				mon_sprintf(f, "%d,%s", (op >> 3) & 7, reg_name[reg]);
			}
			break;
		}

		case A_RST:
			mon_sprintf(f, "$%02x", op & 0x38);
			break;

		case A_BC_IND:
			mon_sprintf(f, "(bc)");
			break;

		case A_DE_IND:
			mon_sprintf(f, "(de)");
			break;

		case A_HL_IND:
			mon_sprintf(f, ix ? "(ix)" : (iy ? "(iy)" : "(hl)"));
			break;

		case A_XY_IND: // undoc
			mon_sprintf(f, "(%s+$%02x)", ix ? "ix" : "iy", mon_read_byte(adr)); adr++;
			break;

		case A_SP_IND:
			mon_sprintf(f, "(sp)");
			break;

		case A_DE_HL:
			mon_sprintf(f, "de,hl");
			break;

		case A_AF_AF:
			mon_sprintf(f, "af,af'");
			break;
	}
}

static int print_instr(SFILE *f, Mnemonic mnem, AddrMode dst_mode, AddrMode src_mode, uint32 adr, uint8 op, bool ix, bool iy)
{
	uint32 orig_adr = adr;

	// Print mnemonic
	mon_sprintf(f, "%c%c%c%c ", mnem_1[mnem], mnem_2[mnem], mnem_3[mnem], mnem_4[mnem]);

	// Print destination operand
	operand(f, dst_mode, adr, op, ix, iy);

	// Print source operand
	if (src_mode != A_IMPL)
		mon_sprintf(f, ",");
	operand(f, src_mode, adr, op, ix, iy);

	return adr - orig_adr;
}

static int disass_cb(SFILE *f, uint32 adr, bool ix, bool iy)
{
	int num;

	// Fetch opcode
	uint8 op;
	if (ix || iy) {
		op = mon_read_byte(adr + 1);
		num = 2;
	} else {
		op = mon_read_byte(adr);
		num = 1;
	}

	// Decode mnemonic and addressing modes
	Mnemonic mnem = M_ILLEGAL;
	AddrMode dst_mode = A_IMPL, src_mode = A_IMPL;

	switch (op & 0xc0) {
		case 0x00:
			dst_mode = A_REG1X;
			if ((ix || iy) && ((op & 7) != 6))
				src_mode = A_XY_IND;
			switch ((op >> 3) & 7) {
				case 0: mnem = M_RLC; break;
				case 1: mnem = M_RRC; break;
				case 2: mnem = M_RL; break;
				case 3: mnem = M_RR; break;
				case 4: mnem = M_SLA; break;
				case 5: mnem = M_SRA; break;
				case 6: mnem = M_SL1; break; // undoc
				case 7: mnem = M_SRL; break;
			}
			break;
		case 0x40:
			mnem = M_BIT; dst_mode = A_BIT;
			if (ix || iy)
				src_mode = A_XY_IND;
			else
				src_mode = A_REG1;
			break;
		case 0x80:
			mnem = M_RES;
			if (ix || iy) {
				dst_mode = A_BIT_REG1;
				src_mode = A_XY_IND;
			} else {
				dst_mode = A_BIT;
				src_mode = A_REG1;
			}
			break;
		case 0xc0:
			mnem = M_SET;
			if (ix || iy) {
				dst_mode = A_BIT_REG1;
				src_mode = A_XY_IND;
			} else {
				dst_mode = A_BIT;
				src_mode = A_REG1;
			}
			break;
	}

	// Print instruction
	print_instr(f, mnem, dst_mode, src_mode, adr, op, ix, iy);
	return num;
}

static int disass_ed(SFILE *f, uint32 adr)
{
	// Fetch opcode
	uint8 op = mon_read_byte(adr);

	// Decode mnemonic and addressing modes
	Mnemonic mnem;
	AddrMode dst_mode = A_IMPL, src_mode = A_IMPL;

	switch (op) {
		case 0x40:
		case 0x48:
		case 0x50:
		case 0x58:
		case 0x60:
		case 0x68:
		case 0x78:
			mon_sprintf(f, "in   %s,(c)", reg_name[(op >> 3) & 7]);
			return 1;
		case 0x70:
			mon_sprintf(f, "in   (c)");
			return 1;

		case 0x41:
		case 0x49:
		case 0x51:
		case 0x59:
		case 0x61:
		case 0x69:
		case 0x79:
			mon_sprintf(f, "out  (c),%s", reg_name[(op >> 3) & 7]);
			return 1;
		case 0x71:	// undoc
			mon_sprintf(f, "out  (c),0");
			return 1;

		case 0x42:
		case 0x52:
		case 0x62:
		case 0x72:
			mnem = M_SBC; dst_mode = A_HL; src_mode = A_REG3;
			break;

		case 0x43:
		case 0x53:
		case 0x63:
		case 0x73:
			mnem = M_LD; dst_mode = A_ABS16; src_mode = A_REG3;
			break;

		case 0x4a:
		case 0x5a:
		case 0x6a:
		case 0x7a:
			mnem = M_ADC; dst_mode = A_HL; src_mode = A_REG3;
			break;

		case 0x4b:
		case 0x5b:
		case 0x6b:
		case 0x7b:
			mnem = M_LD; dst_mode = A_REG3; src_mode = A_ABS16;
			break;

		case 0x44:
		case 0x4c:	// undoc
		case 0x54:	// undoc
		case 0x5c:	// undoc
		case 0x64:	// undoc
		case 0x6c:	// undoc
		case 0x74:	// undoc
		case 0x7c:	// undoc
			mnem = M_NEG;
			break;

		case 0x45:
		case 0x55:	// undoc
		case 0x5d:	// undoc
		case 0x65:	// undoc
		case 0x6d:	// undoc
		case 0x75:	// undoc
		case 0x7d:	// undoc
			mnem = M_RETN;
			break;
		case 0x4d: mnem = M_RETI; break;

		case 0x46:
		case 0x4e:	// undoc
		case 0x66:	// undoc
		case 0x6e:	// undoc
			mnem = M_IM0;
			break;
		case 0x56:
		case 0x76:	// undoc
			mnem = M_IM1;
			break;
		case 0x5e:
		case 0x7e:	// undoc
			mnem = M_IM2;
			break;

		case 0x47:
			mon_sprintf(f, "ld   i,a");
			return 1;
		case 0x4f:
			mon_sprintf(f, "ld   r,a");
			return 1;
		case 0x57:
			mon_sprintf(f, "ld   a,i");
			return 1;
		case 0x5f:
			mon_sprintf(f, "ld   a,r");
			return 1;

		case 0x67: mnem = M_RRD; break;
		case 0x6f: mnem = M_RLD; break;

		case 0xa0: mnem = M_LDI; break;
		case 0xa1: mnem = M_CPI; break;
		case 0xa2: mnem = M_INI; break;
		case 0xa3: mnem = M_OUTI; break;
		case 0xa8: mnem = M_LDD; break;
		case 0xa9: mnem = M_CPD; break;
		case 0xaa: mnem = M_IND; break;
		case 0xab: mnem = M_OUTD; break;
		case 0xb0: mnem = M_LDIR; break;
		case 0xb1: mnem = M_CPIR; break;
		case 0xb2: mnem = M_INIR; break;
		case 0xb3: mnem = M_OTIR; break;
		case 0xb8: mnem = M_LDDR; break;
		case 0xb9: mnem = M_CPDR; break;
		case 0xba: mnem = M_INDR; break;
		case 0xbb: mnem = M_OTDR; break;

		default:
			mnem = M_NOP;
			break;
	}

	// Print instruction
	return print_instr(f, mnem, dst_mode, src_mode, adr + 1, op, false, false) + 1;
}

static int disass(SFILE *f, uint32 adr, bool ix, bool iy)
{
	uint8 op = mon_read_byte(adr);
	if (op == 0xcb)
		return disass_cb(f, adr + 1, ix, iy) + 1;
	else
		return print_instr(f, mnemonic[op], AddrMode(adr_mode[op] >> 8), AddrMode(adr_mode[op] & 0xff), adr + 1, op, ix, iy) + 1;
}

int disass_z80(FILE *f, uint32 adr)
{
	int num;
	char buf[64];
	SFILE sfile = {buf, buf};

	switch (mon_read_byte(adr)) {
		case 0xdd:	// ix prefix
			num = disass(&sfile, adr + 1, true, false) + 1;
			break;
		case 0xed:
			num = disass_ed(&sfile, adr + 1) + 1;
			break;
		case 0xfd:	// iy prefix
			num = disass(&sfile, adr + 1, false, true) + 1;
			break;
		default:
			num = disass(&sfile, adr, false, false);
			break;
	}

	for (int i=0; i<4; i++) {
		if (num > i)
			fprintf(f, "%02x ", mon_read_byte(adr + i));
		else
			fprintf(f, "   ");
	}

	fprintf(f, "\t%s\n", buf);
	return num;
}
