/*
 *  mon_6502.cpp - 6502 disassembler
 *
 *  cxmon (C) 1997-2004 Christian Bauer, Marc Hellwig
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

#include "mon.h"
#include "mon_disass.h"


// Addressing modes
enum AddrMode {
	A_IMPL,
	A_ACCU,		// A
	A_IMM,		// #zz
	A_REL,		// Branches
	A_ZERO,		// zz
	A_ZEROX,	// zz,x
	A_ZEROY,	// zz,y
	A_ABS,		// zzzz
	A_ABSX,		// zzzz,x
	A_ABSY,		// zzzz,y
	A_IND,		// (zzzz)
	A_INDX,		// (zz,x)
	A_INDY		// (zz),y
};

// Mnemonics
enum Mnemonic {
	M_ADC, M_AND, M_ASL, M_BCC, M_BCS, M_BEQ, M_BIT, M_BMI, M_BNE, M_BPL,
	M_BRK, M_BVC, M_BVS, M_CLC, M_CLD, M_CLI, M_CLV, M_CMP, M_CPX, M_CPY,
	M_DEC, M_DEX, M_DEY, M_EOR, M_INC, M_INX, M_INY, M_JMP, M_JSR, M_LDA,
	M_LDX, M_LDY, M_LSR, M_NOP, M_ORA, M_PHA, M_PHP, M_PLA, M_PLP, M_ROL,
	M_ROR, M_RTI, M_RTS, M_SBC, M_SEC, M_SED, M_SEI, M_STA, M_STX, M_STY,
	M_TAX, M_TAY, M_TSX, M_TXA, M_TXS, M_TYA,

	M_ILLEGAL,  // Undocumented opcodes start here

	M_IANC, M_IANE, M_IARR, M_IASR, M_IDCP, M_IISB, M_IJAM, M_INOP, M_ILAS,
	M_ILAX, M_ILXA, M_IRLA, M_IRRA, M_ISAX, M_ISBC, M_ISBX, M_ISHA, M_ISHS,
	M_ISHX, M_ISHY, M_ISLO, M_ISRE,

	M_MAXIMUM  // Highest element
};

// Mnemonic for each opcode
static const Mnemonic mnemonic[256] = {
	M_BRK , M_ORA , M_IJAM, M_ISLO, M_INOP, M_ORA, M_ASL , M_ISLO,	// 00
	M_PHP , M_ORA , M_ASL , M_IANC, M_INOP, M_ORA, M_ASL , M_ISLO,
	M_BPL , M_ORA , M_IJAM, M_ISLO, M_INOP, M_ORA, M_ASL , M_ISLO,	// 10
	M_CLC , M_ORA , M_INOP, M_ISLO, M_INOP, M_ORA, M_ASL , M_ISLO,
	M_JSR , M_AND , M_IJAM, M_IRLA, M_BIT , M_AND, M_ROL , M_IRLA,	// 20
	M_PLP , M_AND , M_ROL , M_IANC, M_BIT , M_AND, M_ROL , M_IRLA,
	M_BMI , M_AND , M_IJAM, M_IRLA, M_INOP, M_AND, M_ROL , M_IRLA,	// 30
	M_SEC , M_AND , M_INOP, M_IRLA, M_INOP, M_AND, M_ROL , M_IRLA,
	M_RTI , M_EOR , M_IJAM, M_ISRE, M_INOP, M_EOR, M_LSR , M_ISRE,	// 40
	M_PHA , M_EOR , M_LSR , M_IASR, M_JMP , M_EOR, M_LSR , M_ISRE,
	M_BVC , M_EOR , M_IJAM, M_ISRE, M_INOP, M_EOR, M_LSR , M_ISRE,	// 50
	M_CLI , M_EOR , M_INOP, M_ISRE, M_INOP, M_EOR, M_LSR , M_ISRE,
	M_RTS , M_ADC , M_IJAM, M_IRRA, M_INOP, M_ADC, M_ROR , M_IRRA,	// 60
	M_PLA , M_ADC , M_ROR , M_IARR, M_JMP , M_ADC, M_ROR , M_IRRA,
	M_BVS , M_ADC , M_IJAM, M_IRRA, M_INOP, M_ADC, M_ROR , M_IRRA,	// 70
	M_SEI , M_ADC , M_INOP, M_IRRA, M_INOP, M_ADC, M_ROR , M_IRRA,
	M_INOP, M_STA , M_INOP, M_ISAX, M_STY , M_STA, M_STX , M_ISAX,	// 80
	M_DEY , M_INOP, M_TXA , M_IANE, M_STY , M_STA, M_STX , M_ISAX,
	M_BCC , M_STA , M_IJAM, M_ISHA, M_STY , M_STA, M_STX , M_ISAX,	// 90
	M_TYA , M_STA , M_TXS , M_ISHS, M_ISHY, M_STA, M_ISHX, M_ISHA,
	M_LDY , M_LDA , M_LDX , M_ILAX, M_LDY , M_LDA, M_LDX , M_ILAX,	// a0
	M_TAY , M_LDA , M_TAX , M_ILXA, M_LDY , M_LDA, M_LDX , M_ILAX,
	M_BCS , M_LDA , M_IJAM, M_ILAX, M_LDY , M_LDA, M_LDX , M_ILAX,	// b0
	M_CLV , M_LDA , M_TSX , M_ILAS, M_LDY , M_LDA, M_LDX , M_ILAX,
	M_CPY , M_CMP , M_INOP, M_IDCP, M_CPY , M_CMP, M_DEC , M_IDCP,	// c0
	M_INY , M_CMP , M_DEX , M_ISBX, M_CPY , M_CMP, M_DEC , M_IDCP,
	M_BNE , M_CMP , M_IJAM, M_IDCP, M_INOP, M_CMP, M_DEC , M_IDCP,	// d0
	M_CLD , M_CMP , M_INOP, M_IDCP, M_INOP, M_CMP, M_DEC , M_IDCP,
	M_CPX , M_SBC , M_INOP, M_IISB, M_CPX , M_SBC, M_INC , M_IISB,	// e0
	M_INX , M_SBC , M_NOP , M_ISBC, M_CPX , M_SBC, M_INC , M_IISB,
	M_BEQ , M_SBC , M_IJAM, M_IISB, M_INOP, M_SBC, M_INC , M_IISB,	// f0
	M_SED , M_SBC , M_INOP, M_IISB, M_INOP, M_SBC, M_INC , M_IISB
};

// Addressing mode for each opcode
static const AddrMode adr_mode[256] = {
	A_IMPL, A_INDX, A_IMPL, A_INDX, A_ZERO , A_ZERO , A_ZERO , A_ZERO,	// 00
	A_IMPL, A_IMM , A_ACCU, A_IMM , A_ABS  , A_ABS  , A_ABS  , A_ABS,
	A_REL , A_INDY, A_IMPL, A_INDY, A_ZEROX, A_ZEROX, A_ZEROX, A_ZEROX,	// 10
	A_IMPL, A_ABSY, A_IMPL, A_ABSY, A_ABSX , A_ABSX , A_ABSX , A_ABSX,
	A_ABS , A_INDX, A_IMPL, A_INDX, A_ZERO , A_ZERO , A_ZERO , A_ZERO,	// 20
	A_IMPL, A_IMM , A_ACCU, A_IMM , A_ABS  , A_ABS  , A_ABS  , A_ABS,
	A_REL , A_INDY, A_IMPL, A_INDY, A_ZEROX, A_ZEROX, A_ZEROX, A_ZEROX,	// 30
	A_IMPL, A_ABSY, A_IMPL, A_ABSY, A_ABSX , A_ABSX , A_ABSX , A_ABSX,
	A_IMPL, A_INDX, A_IMPL, A_INDX, A_ZERO , A_ZERO , A_ZERO , A_ZERO,	// 40
	A_IMPL, A_IMM , A_ACCU, A_IMM , A_ABS  , A_ABS  , A_ABS  , A_ABS,
	A_REL , A_INDY, A_IMPL, A_INDY, A_ZEROX, A_ZEROX, A_ZEROX, A_ZEROX,	// 50
	A_IMPL, A_ABSY, A_IMPL, A_ABSY, A_ABSX , A_ABSX , A_ABSX , A_ABSX,
	A_IMPL, A_INDX, A_IMPL, A_INDX, A_ZERO , A_ZERO , A_ZERO , A_ZERO,	// 60
	A_IMPL, A_IMM , A_ACCU, A_IMM , A_IND  , A_ABS  , A_ABS  , A_ABS,
	A_REL , A_INDY, A_IMPL, A_INDY, A_ZEROX, A_ZEROX, A_ZEROX, A_ZEROX,	// 70
	A_IMPL, A_ABSY, A_IMPL, A_ABSY, A_ABSX , A_ABSX , A_ABSX , A_ABSX,
	A_IMM , A_INDX, A_IMM , A_INDX, A_ZERO , A_ZERO , A_ZERO , A_ZERO,	// 80
	A_IMPL, A_IMM , A_IMPL, A_IMM , A_ABS  , A_ABS  , A_ABS  , A_ABS,
	A_REL , A_INDY, A_IMPL, A_INDY, A_ZEROX, A_ZEROX, A_ZEROY, A_ZEROY,	// 90
	A_IMPL, A_ABSY, A_IMPL, A_ABSY, A_ABSX , A_ABSX , A_ABSY , A_ABSY,
	A_IMM , A_INDX, A_IMM , A_INDX, A_ZERO , A_ZERO , A_ZERO , A_ZERO,	// a0
	A_IMPL, A_IMM , A_IMPL, A_IMM , A_ABS  , A_ABS  , A_ABS  , A_ABS,
	A_REL , A_INDY, A_IMPL, A_INDY, A_ZEROX, A_ZEROX, A_ZEROY, A_ZEROY,	// b0
	A_IMPL, A_ABSY, A_IMPL, A_ABSY, A_ABSX , A_ABSX , A_ABSY , A_ABSY,
	A_IMM , A_INDX, A_IMM , A_INDX, A_ZERO , A_ZERO , A_ZERO , A_ZERO,	// c0
	A_IMPL, A_IMM , A_IMPL, A_IMM , A_ABS  , A_ABS  , A_ABS  , A_ABS,
	A_REL , A_INDY, A_IMPL, A_INDY, A_ZEROX, A_ZEROX, A_ZEROX, A_ZEROX,	// d0
	A_IMPL, A_ABSY, A_IMPL, A_ABSY, A_ABSX , A_ABSX , A_ABSX , A_ABSX,
	A_IMM , A_INDX, A_IMM , A_INDX, A_ZERO , A_ZERO , A_ZERO , A_ZERO,	// e0
	A_IMPL, A_IMM , A_IMPL, A_IMM , A_ABS  , A_ABS  , A_ABS  , A_ABS,
	A_REL , A_INDY, A_IMPL, A_INDY, A_ZEROX, A_ZEROX, A_ZEROX, A_ZEROX,	// f0
	A_IMPL, A_ABSY, A_IMPL, A_ABSY, A_ABSX , A_ABSX , A_ABSX , A_ABSX
};

// Chars for each mnemonic
static const char mnem_1[] = "aaabbbbbbbbbbcccccccdddeiiijjllllnopppprrrrssssssstttttt?aaaadijnlllrrsssssssss";
static const char mnem_2[] = "dnscceimnprvvllllmppeeeonnnmsdddsorhhlloottbeeetttaasxxy?nnrscsaoaaxlrabbhhhhlr";
static const char mnem_3[] = "cdlcsqtielkcscdivpxycxyrcxypraxyrpaapaplrisccdiaxyxyxasa?cerrpbmpsxaaaxcxasxyoe";

// Instruction length for each addressing mode
static const int adr_length[] = {1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 2, 2};


/*
 *  Disassemble one instruction, return number of bytes
 */

int disass_6502(FILE *f, uint32 adr, uint8 op, uint8 lo, uint8 hi)
{
	AddrMode mode = adr_mode[op];
	Mnemonic mnem = mnemonic[op];

	// Display instruction bytes in hex
	switch (adr_length[mode]) {
		case 1:
			fprintf(f, "%02x\t\t", op);
			break;

		case 2:
			fprintf(f, "%02x %02x\t\t", op, lo);
			break;

		case 3:
			fprintf(f, "%02x %02x %02x\t", op, lo, hi);
			break;
	}

	// Tag undocumented opcodes with an asterisk
	if (mnem > M_ILLEGAL)
		fputc('*', f);
	else
		fputc(' ', f);

	// Print mnemonic
	fprintf(f, "%c%c%c ", mnem_1[mnem], mnem_2[mnem], mnem_3[mnem]);

	// Print argument
	switch (mode) {
		case A_IMPL:
			break;

		case A_ACCU:
			fprintf(f, "a");
			break;

		case A_IMM:
			fprintf(f, "#$%02x", lo);
			break;

		case A_REL:
			fprintf(f, "$%04x", (adr + 2) + (int8)lo);
			break;

		case A_ZERO:
			fprintf(f, "$%02x", lo);
			break;

		case A_ZEROX:
			fprintf(f, "$%02x,x", lo);
			break;

		case A_ZEROY:
			fprintf(f, "$%02x,y", lo);
			break;

		case A_ABS:
			fprintf(f, "$%04x", (hi << 8) | lo);
			break;

		case A_ABSX:
			fprintf(f, "$%04x,x", (hi << 8) | lo);
			break;

		case A_ABSY:
			fprintf(f, "$%04x,y", (hi << 8) | lo);
			break;

		case A_IND:
			fprintf(f, "($%04x)", (hi << 8) | lo);
			break;

		case A_INDX:
			fprintf(f, "($%02x,x)", lo);
			break;

		case A_INDY:
			fprintf(f, "($%02x),y", lo);
			break;
	}

	fputc('\n', f);
	return adr_length[mode];
}
