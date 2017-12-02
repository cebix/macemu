/*
 *  mon_ppc.cpp - PowerPC disassembler
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


// Instruction fields
static int primop, exop, vxop, ra, rb, rc, rd;
static unsigned short imm;


// Codes for trap instructions
static const char *to_code[32] = {
	NULL, "lgt", "llt", NULL, "eq", "lge", "lle", NULL,
	"gt", NULL, NULL, NULL, "ge", NULL, NULL, NULL,
	"lt", NULL, NULL, NULL, "le", NULL, NULL, NULL,
	"ne", NULL, NULL, NULL, NULL, NULL, NULL, "" 
};


// Macros for instruction forms
#define iform(s, t) fprintf(f, "%s\t$%08x\n", s, t)

#define bform(s, t) \
	if (rd == 0) \
		fprintf(f, "bdnzf%s\t%d,$%08x\n", s, ra, t); \
	else if (rd == 2) \
		fprintf(f, "bdzf%s\t%d,$%08x\n", s, ra, t); \
	else if (rd == 4) { \
		if (ra == 0) \
			fprintf(f, "bge%s\t$%08x\n", s, t); \
		else if (ra == 1) \
			fprintf(f, "ble%s\t$%08x\n", s, t); \
		else if (ra == 2) \
			fprintf(f, "bne%s\t$%08x\n", s, t); \
		else \
			fprintf(f, "bf%s\t%d,$%08x\n", s, ra, t); \
	} else if (rd == 8) \
		fprintf(f, "bdnzt%s\t%d,$%08x\n", s, ra, t); \
	else if (rd == 10) \
		fprintf(f, "bdzt%s\t%d,$%08x\n", s, ra, t); \
	else if (rd == 12) { \
		if (ra == 0) \
			fprintf(f, "blt%s\t$%08x\n", s, t); \
		else if (ra == 1) \
			fprintf(f, "bgt%s\t$%08x\n", s, t); \
		else if (ra == 2) \
			fprintf(f, "beq%s\t$%08x\n", s, t); \
		else \
			fprintf(f, "bt%s\t%d,$%08x\n", s, ra, t); \
	} else if (rd == 16) \
		fprintf(f, "bdnz%s\t$%08x\n", s, t); \
	else if (rd == 18) \
		fprintf(f, "bdz%s\t$%08x\n", s, t); \
	else \
		fprintf(f, "bc%s\t%d,%d,$%08x\n", s, rd, ra, t)

#define scform(s) fprintf(f, s "\n")

#define dform_ls(s) \
	if (ra == 0) \
		fprintf(f, "%s\tr%d,$%08x\n", s, rd, short(imm)); \
	else \
		fprintf(f, "%s\tr%d,$%04x(r%d)\n", s, rd, short(imm), ra)
#define dform_fls(s) \
	if (ra == 0) \
		fprintf(f, "%s\tfr%d,$%08x\n", s, rd, short(imm)); \
	else \
		fprintf(f, "%s\tfr%d,$%04x(r%d)\n", s, rd, short(imm), ra)
#define dform_simm(s) fprintf(f, "%s\tr%d,r%d,$%04x\n", s, rd, ra, short(imm))
#define dform_simmn(s) fprintf(f, "%s\tr%d,r%d,$%04x\n", s, rd, ra, (-imm) & 0xffff)
#define dform_uimm(s) fprintf(f, "%s\tr%d,r%d,$%04x\n", s, ra, rd, imm)
#define dform_crs(s) fprintf(f, "%s\tcrf%d,r%d,$%04x\n", s, rd >> 2, ra, short(imm))
#define dform_cru(s) fprintf(f, "%s\tcrf%d,r%d,$%04x\n", s, rd >> 2, ra, imm)
#define dform_to(s) fprintf(f, "%s\t%d,r%d,$%04x\n", s, rd, ra, short(imm))

#define dsform(s) fprintf(f, "%s\tr%d,$%04x(r%d)\n", s, rd, short(imm & 0xfffc), ra)

#define xform_vls(s) \
	if (ra == 0) \
		fprintf(f, "%s\tv%d,r%d\n", s, rd, rb); \
	else \
		fprintf(f, "%s\tv%d,r%d,r%d\n", s, rd, ra, rb)
#define xform_ls(s) \
	if (ra == 0) \
		fprintf(f, "%s\tr%d,r%d\n", s, rd, rb); \
	else \
		fprintf(f, "%s\tr%d,r%d,r%d\n", s, rd, ra, rb)
#define xform_fls(s) \
	if (ra == 0) \
		fprintf(f, "%s\tfr%d,r%d\n", s, rd, rb); \
	else \
		fprintf(f, "%s\tfr%d,r%d,r%d\n", s, rd, ra, rb)
#define xform_lsswi(s) \
	if (ra == 0) \
		fprintf(f, "%s\tr%d,%d\n", s, rd, rb ? rb : 32); \
	else \
		fprintf(f, "%s\tr%d,r%d,%d\n", s, rd, ra, rb ? rb : 32)
#define xform_db(s) fprintf(f, "%s\tr%d,r%d\n", s, rd, rb)
#define xform_d(s) fprintf(f, "%s\tr%d\n", s, rd)
#define xform_fsr(s) fprintf(f, s "\tr%d,%d\n", rd, ra & 0xf)
#define xform_sabc(s) fprintf(f, "%s%s\tr%d,r%d,r%d\n", s, w & 1 ? "." : "", ra, rd, rb)
#define xform_sac(s) fprintf(f, "%s%s\tr%d,r%d\n", s, w & 1 ? "." : "", ra, rd)
#define xform_tsr(s) fprintf(f, s "\t%d,r%d\n", ra & 0xf, rd)
#define xform_sash(s) fprintf(f, s "%s\tr%d,r%d,%d\n", w & 1 ? "." : "", ra, rd, rb)
#define xform_crlab(s) fprintf(f, "%s\tcrf%d,r%d,r%d\n", s, rd >> 2, ra, rb)
#define xform_fcrab(s) fprintf(f, "%s\tcrf%d,fr%d,fr%d\n", s, rd >> 2, ra, rb)
#define xform_crcr(s) fprintf(f, "%s\tcrf%d,crf%d\n", s, rd >> 2, ra >> 2)
#define xform_cr(s) fprintf(f, "%s\tcrf%d\n", s, rd >> 2)
#define xform_cri(s) fprintf(f, s "%s\tcrf%d,%d\n", w & 1 ? "." : "", rd >> 2, rb >> 1)
#define xform_to(s) fprintf(f, "%s\t%d,r%d,r%d\n", s, rd, ra, rb)
#define xform_fdb(s) fprintf(f, "%s%s\tfr%d,fr%d\n", s, w & 1 ? "." : "", rd, rb)
#define xform_fd(s) fprintf(f, "%s%s\tfr%d\n", s, w & 1 ? "." : "", rd)
#define xform_crb(s) fprintf(f, "%s%s\tcrb%d\n", s, w & 1 ? "." : "", rd)
#define xform_ab(s) fprintf(f, "%s\tr%d,r%d\n", s, ra, rb)
#define xform_b(s) fprintf(f, "%s\tr%d\n", s, rb)
#define xform(s) fprintf(f, s "\n")

#define xlform_b(s) fprintf(f, "%s\t%d,%d\n", s, rd, ra)
#define xlform_cr(s) fprintf(f, "%s\tcrb%d,crb%d,crb%d\n", s, rd, ra, rb)
#define xlform_crcr(s) fprintf(f, "%s\tcrf%d,crf%d\n", s, rd >> 2, ra >> 2)
#define xlform(s) fprintf(f, s "\n")

#define xfxform_fspr(s) fprintf(f, "%s\tr%d,SPR%d\n", s, rd, ra | (rb << 5))
#define xfxform_crm(s) fprintf(f, s "\t$%02x,r%d\n", w >> 12 & 0xff, rd)
#define xfxform_tspr(s) fprintf(f, "%s\tSPR%d,r%d\n", s, ra | (rb << 5), rd)
#define xfxform_tb(s) fprintf(f, "%s\tr%d\n", s, rd)

#define xflform(s) fprintf(f, s "%s\t$%02x,fr%d\n", w & 1 ? "." : "", w >> 17 & 0xff, rb)

#define xsform(s) fprintf(f, s "%s\tr%d,r%d,%d\n", w & 1 ? "." : "", ra, rd, rb | (w & 2 ? 32 : 0))

#define xoform_dab(s) fprintf(f, "%s%s\tr%d,r%d,r%d\n", s, w & 1 ? "." : "", rd, ra, rb)
#define xoform_da(s) fprintf(f, "%s%s\tr%d,r%d\n", s, w & 1 ? "." : "", rd, ra)

#define aform_dab(s) fprintf(f, "%s%s\tfr%d,fr%d,fr%d\n", s, w & 1 ? "." : "", rd, ra, rb)
#define aform_db(s) fprintf(f, "%s%s\tfr%d,fr%d\n", s, w & 1 ? "." : "", rd, rb)
#define aform_dac(s) fprintf(f, "%s%s\tfr%d,fr%d,fr%d\n", s, w & 1 ? "." : "", rd, ra, rc)
#define aform_dacb(s) fprintf(f, "%s%s\tfr%d,fr%d,fr%d,fr%d\n", s, w & 1 ? "." : "", rd, ra, rc, rb)

#define mform(s) fprintf(f, "%s%s\tr%d,r%d,r%d,$%08x\n", s, w & 1 ? "." : "", ra, rd, rb, mbme2mask(w >> 6 & 31, w >>1 & 31))
#define mform_i(s) fprintf(f, "%s%s\tr%d,r%d,%d,$%08x\n", s, w & 1 ? "." : "", ra, rd, rb, mbme2mask(w >> 6 & 31, w >>1 & 31))

#define mdform(s) fprintf(f, "%s%s\tr%d,r%d,%d,%d\n", s, w & 1 ? "." : "", ra, rd, rb | (w & 2 ? 32 : 0), rc | (w & 32 ? 32 : 0))

#define mdsform(s) fprintf(f, "%s%s\tr%d,r%d,r%d,%d\n", s, w & 1 ? "." : "", ra, rd, rb, rc | (w & 32 ? 32 : 0))

#define va_form(s) fprintf(f, "%s\tv%d,v%d,v%d,v%d\n", s, rd, ra, rb, rc)
#define vx_form(s) fprintf(f, "%s\tv%d,v%d,v%d\n", s, rd, ra, rb)
#define vxr_form(s) fprintf(f, "%s%s\tv%d,v%d,v%d\n", s, w & (1 << 10) ? "." : "", rd, ra, rb)
#define vxi_ra_form(s) fprintf(f, "%s\tv%d,v%d,%d\n", s, rd, rb, ra)
#define vx_raz_form(s) \
	if (ra == 0) \
		fprintf(f, "%s\tv%d,v%d\n", s, rd, rb); \
	else \
		fprintf(f, "?\n")
#define vxi_ras_rbz_form(s) \
	if (rb == 0) \
		fprintf(f, "%s\tv%d,%d\n", s, rd, ra - (ra & 0x10 ? 0x20 : 0)); \
	else \
		fprintf(f, "?\n")

// Prototypes
static void disass4(FILE *f, unsigned int adr, unsigned int w);
static void disass19(FILE *f, unsigned int adr, unsigned int w);
static void disass31(FILE *f, unsigned int adr, unsigned int w);
static void disass59(FILE *f, unsigned int adr, unsigned int w);
static void disass63(FILE *f, unsigned int adr, unsigned int w);
static unsigned int mbme2mask(int mb, int me);
static const char *get_spr(int reg);


/*
 *  Disassemble one instruction
 */

void disass_ppc(FILE *f, unsigned int adr, unsigned int w)
{
	// Divide instruction into fields
	primop = w >> 26;
	rd = w >> 21 & 0x1f;
	ra = w >> 16 & 0x1f;
	rb = w >> 11 & 0x1f;
	rc = w >> 6 & 0x1f;
	exop = w >> 1 & 0x3ff;
	vxop = w & 0x7ff;
	imm = w & 0xffff;

	// Decode primary opcode
	switch (primop) {
		case 2:		// 64 bit
			if (to_code[rd] != NULL)
				fprintf(f, "td%si\tr%d,$%04x\n", to_code[rd], ra, short(imm));
			else
				dform_to("tdi");
			break;

		case 3:
			if (to_code[rd] != NULL)
				fprintf(f, "tw%si\tr%d,$%04x\n", to_code[rd], ra, short(imm));
			else
				dform_to("twi");
			break;

		case 4: // AltiVec
			disass4(f, adr, w);
			break;

		case 7: dform_simm("mulli"); break;
		case 8: dform_simm("subfic"); break;

		case 10:
			if (rd & 1)
				dform_cru("cmpldi");	// 64 bit
			else
				dform_cru("cmplwi");
			break;

		case 11:
			if (rd & 1)
				dform_crs("cmpdi");		// 64 bit
			else
				dform_crs("cmpwi");
			break;

		case 12:
			if (imm < 0x8000)
				dform_simm("addic");
			else
				dform_simmn("subic");
			break;

		case 13:
			if (imm < 0x8000)
				dform_simm("addic.");
			else
				dform_simmn("subic.");
			break;

		case 14:
			if (ra == 0)
				fprintf(f, "li\tr%d,$%04x\n", rd, short(imm));
			else
				if (imm < 0x8000)
					dform_simm("addi");
				else
					dform_simmn("subi");
			break;

		case 15:
			if (ra == 0)
				fprintf(f, "lis\tr%d,$%04x\n", rd, imm);
			else
				if (imm < 0x8000)
					dform_simm("addis");
				else
					dform_simmn("subis");
			break;

		case 16: {
			int target = short(imm & 0xfffc);
			const char *form;
			if (w & 1)
				if (w & 2)
					form = "la";
				else {
					form = "l";
					target += adr;
				}
			else
				if (w & 2)
					form = "a";
				else {
					form = "";
					target += adr;
				}
			bform(form, target);
			break;
		}

		case 17:
			if (w & 2)
				scform("sc");
			else
				fprintf(f, "?\n");
			break;

		case 18: {
			int target = w & 0x03fffffc;
			if (target & 0x02000000)
				target |= 0xfc000000;
			if (w & 1)
				if (w & 2)
					iform("bla", target);
				else
					iform("bl", adr + target);
			else
				if (w & 2)
					iform("ba", target);
				else
					iform("b", adr + target);
			break;
		}

		case 19: disass19(f, adr, w); break;
		case 20: mform_i("rlwimi"); break;
		case 21: mform_i("rlwinm"); break;
		case 23: mform("rlwnm"); break;

		case 24:
			if (rd == 0 && ra == 0 && imm == 0)
				fprintf(f, "nop\n");
			else
				dform_uimm("ori");
			break;

		case 25: dform_uimm("oris"); break;
		case 26: dform_uimm("xori"); break;
		case 27: dform_uimm("xoris"); break;
		case 28: dform_uimm("andi."); break;
		case 29: dform_uimm("andis."); break;

		case 30:	// 64 bit
			switch (w >> 1 & 0xf) {
				case 0: case 1: mdform("rldicl"); break;
				case 2: case 3: mdform("rldicr"); break;
				case 4: case 5: mdform("rldic"); break;
				case 6: case 7: mdform("rldimi"); break;
				case 8: mdsform("rldcl"); break;
				case 9: mdsform("rldcr"); break;
				default:
					fprintf(f, "?\n");
					break;
			};
			break;

		case 31: disass31(f, adr, w); break;
		case 32: dform_ls("lwz"); break;
		case 33: dform_ls("lwzu"); break;
		case 34: dform_ls("lbz"); break;
		case 35: dform_ls("lbzu"); break;
		case 36: dform_ls("stw"); break;
		case 37: dform_ls("stwu"); break;
		case 38: dform_ls("stb"); break;
		case 39: dform_ls("stbu"); break;
		case 40: dform_ls("lhz"); break;
		case 41: dform_ls("lhzu"); break;
		case 42: dform_ls("lha"); break;
		case 43: dform_ls("lhau"); break;
		case 44: dform_ls("sth"); break;
		case 45: dform_ls("sthu"); break;
		case 46: dform_ls("lmw"); break;
		case 47: dform_ls("stmw"); break;
		case 48: dform_fls("lfs"); break;
		case 49: dform_fls("lfsu"); break;
		case 50: dform_fls("lfd"); break;
		case 51: dform_fls("lfdu"); break;
		case 52: dform_fls("stfs"); break;
		case 53: dform_fls("stfsu"); break;
		case 54: dform_fls("stfd"); break;
		case 55: dform_fls("stfdu"); break;

		case 58:	// 64 bit
			switch (w & 3) {
				case 0: dsform("ld"); break;
				case 1: dsform("ldu"); break;
				case 2: dsform("lwa"); break;
				default:
					fprintf(f, "?\n");
					break;
			}
			break;

		case 59: disass59(f, adr, w); break;

		case 62:	// 64 bit
			switch (w & 3) {
				case 0: dsform("std"); break;
				case 1: dsform("stdu"); break;
				default:
					fprintf(f, "?\n");
					break;
			}
			break;

		case 63: disass63(f, adr, w); break;

		default:
			if (!w)
				fprintf(f, "illegal\n");
			else
				fprintf(f, "?\n");
			break;
	}
}


/*
 *  Disassemble instruction with primary opcode = 4 (VX-Form)
 */

static void disass4(FILE *f, unsigned int adr, unsigned int w)
{
	switch (vxop) {
	case 1540:
		if (ra == 0 && rb == 0)
			fprintf(f, "mfvscr\tv%d\n", rd);
		else
			fprintf(f, "?\n");
		break;
	case 1604:
		if (rd == 0 && ra == 0)
			fprintf(f, "mtvscr\tv%d\n", rb);
		else
			fprintf(f, "?\n");
		break;
	case 384: vx_form("vaddcuw"); break;
	case 10: vx_form("vaddfp"); break;
	case 768: vx_form("vaddsbs"); break;
	case 832: vx_form("vaddshs"); break;
	case 896: vx_form("vaddsws"); break;
	case 0: vx_form("vaddubm"); break;
	case 512: vx_form("vaddubs"); break;
	case 64: vx_form("vadduhm"); break;
	case 576: vx_form("vadduhs"); break;
	case 128: vx_form("vadduwm"); break;
	case 640: vx_form("vadduws"); break;
	case 1028: vx_form("vand"); break;
	case 1092: vx_form("vandc"); break;
	case 1282: vx_form("vavgsb"); break;
	case 1346: vx_form("vavgsh"); break;
	case 1410: vx_form("vavgsw"); break;
	case 1026: vx_form("vavgub"); break;
	case 1090: vx_form("vavguh"); break;
	case 1154: vx_form("vavguw"); break;
	case 842: vxi_ra_form("vcfsx"); break;
	case 778: vxi_ra_form("vcfux"); break;
	case 966: case 966+1024: vxr_form("vcmpbfp"); break;
	case 198: case 198+1024: vxr_form("vcmpeqfp"); break;
	case 6: case 6+1024: vxr_form("vcmpequb"); break;
	case 70: case 70+1024: vxr_form("vcmpequh"); break;
	case 134: case 134+1024: vxr_form("vcmpequw"); break;
	case 454: case 454+1024: vxr_form("vcmpgefp"); break;
	case 710: case 710+1024: vxr_form("vcmpgtfp"); break;
	case 774: case 774+1024: vxr_form("vcmpgtsb"); break;
	case 838: case 838+1024: vxr_form("vcmpgtsh"); break;
	case 902: case 902+1024: vxr_form("vcmpgtsw"); break;
	case 518: case 518+1024: vxr_form("vcmpgtub"); break;
	case 582: case 582+1024: vxr_form("vcmpgtuh"); break;
	case 646: case 646+1024: vxr_form("vcmpgtuw"); break;
	case 970: vxi_ra_form("vctsxs"); break;
	case 906: vxi_ra_form("vctuxs"); break;
	case 394: vx_raz_form("vexptefp"); break;
	case 458: vx_raz_form("vlogefp"); break;
	case 1034: vx_form("vmaxfp"); break;
	case 258: vx_form("vmaxsb"); break;
	case 322: vx_form("vmaxsh"); break;
	case 386: vx_form("vmaxsw"); break;
	case 2: vx_form("vmaxub"); break;
	case 66: vx_form("vmaxuh"); break;
	case 130: vx_form("vmaxuw"); break;
	case 1098: vx_form("vminfp"); break;
	case 770: vx_form("vminsb"); break;
	case 834: vx_form("vminsh"); break;
	case 898: vx_form("vminsw"); break;
	case 514: vx_form("vminub"); break;
	case 578: vx_form("vminuh"); break;
	case 642: vx_form("vminuw"); break;
	case 12: vx_form("vmrghb"); break;
	case 76: vx_form("vmrghh"); break;
	case 140: vx_form("vmrghw"); break;
	case 268: vx_form("vmrglb"); break;
	case 332: vx_form("vmrglh"); break;
	case 396: vx_form("vmrglw"); break;
	case 776: vx_form("vmulesb"); break;
	case 840: vx_form("vmulesh"); break;
	case 520: vx_form("vmuleub"); break;
	case 584: vx_form("vmuleuh"); break;
	case 264: vx_form("vmulosb"); break;
	case 328: vx_form("vmulosh"); break;
	case 8: vx_form("vmuloub"); break;
	case 72: vx_form("vmulouh"); break;
	case 1284: vx_form("vnor"); break;
	case 1156: vx_form("vor"); break;
	case 782: vx_form("vpkpx"); break;
	case 398: vx_form("vpkshss"); break;
	case 270: vx_form("vpkshus"); break;
	case 462: vx_form("vpkswss"); break;
	case 334: vx_form("vpkswus"); break;
	case 14: vx_form("vpkuhum"); break;
	case 142: vx_form("vpkuhus"); break;
	case 78: vx_form("vpkuwum"); break;
	case 206: vx_form("vpkuwus"); break;
	case 266: vx_raz_form("vrefp"); break;
	case 714: vx_raz_form("vrfim"); break;
	case 522: vx_raz_form("vrfin"); break;
	case 650: vx_raz_form("vrfip"); break;
	case 586: vx_raz_form("vrfiz"); break;
	case 4: vx_form("vrlb"); break;
	case 68: vx_form("vrlh"); break;
	case 132: vx_form("vrlw"); break;
	case 330: vx_raz_form("vrsqrtefp"); break;
	case 452: vx_form("vsl"); break;
	case 260: vx_form("vslb"); break;
	case 324: vx_form("vslh"); break;
	case 1036: vx_form("vslo"); break;
	case 388: vx_form("vslw"); break;
	case 524: vxi_ra_form("vspltb"); break;
	case 588: vxi_ra_form("vsplth"); break;
	case 780: vxi_ras_rbz_form("vspltisb"); break;
	case 844: vxi_ras_rbz_form("vspltish"); break;
	case 908: vxi_ras_rbz_form("vspltisw"); break;
	case 652: vxi_ra_form("vspltw"); break;
	case 708: vx_form("vsr"); break;
	case 772: vx_form("vsrab"); break;
	case 836: vx_form("vsrah"); break;
	case 900: vx_form("vsraw"); break;
	case 516: vx_form("vsrb"); break;
	case 580: vx_form("vsrh"); break;
	case 1100: vx_form("vsro"); break;
	case 644: vx_form("vsrw"); break;
	case 1408: vx_form("vsubcuw"); break;
	case 74: vx_form("vsubfp"); break;
	case 1792: vx_form("vsubsbs"); break;
	case 1856: vx_form("vsubshs"); break;
	case 1920: vx_form("vsubsws"); break;
	case 1024: vx_form("vsububm"); break;
	case 1536: vx_form("vsububs"); break;
	case 1088: vx_form("vsubuhm"); break;
	case 1600: vx_form("vsubuhs"); break;
	case 1152: vx_form("vsubuwm"); break;
	case 1664: vx_form("vsubuws"); break;
	case 1928: vx_form("vsumsws"); break;
	case 1672: vx_form("vsum2sws"); break;
	case 1800: vx_form("vsum4sbs"); break;
	case 1608: vx_form("vsum4shs"); break;
	case 1544: vx_form("vsum4ubs"); break;
	case 846: vx_raz_form("vupkhpx"); break;
	case 526: vx_raz_form("vupkhsb"); break;
	case 590: vx_raz_form("vupkhsh"); break;
	case 974: vx_raz_form("vupklpx"); break;
	case 654: vx_raz_form("vupklsb"); break;
	case 718: vx_raz_form("vupklsh"); break;
	case 1220: vx_form("vxor"); break;
	default:
		if ((vxop & 0x43f) == 44) {		// vsldoi vD,vA,vB,SHB
			fprintf(f, "vsldoi\tv%d,v%d,v%d,%d\n", rd, ra, rb, rc & 15);
			break;
		}
		switch (vxop & 0x3f) {			// VA-form, must come last
		case 46: va_form("vmaddfp"); break;
		case 32: va_form("vmhaddshs"); break;
		case 33: va_form("vmhraddshs"); break;
		case 34: va_form("vmladduhm"); break;
		case 37: va_form("vmsummbm"); break;
		case 40: va_form("vmsumshm"); break;
		case 41: va_form("vmsumshs"); break;
		case 36: va_form("vmsumubm"); break;
		case 38: va_form("vmsumuhm"); break;
		case 39: va_form("vmsumuhs"); break;
		case 47: va_form("vnmsubfp"); break;
		case 43: va_form("vperm"); break;
		case 42: va_form("vsel"); break;
		default: fprintf(f, "?\n"); break;
		}
		break;
	}
}


/*
 *  Disassemble instruction with primary opcode = 19 (XL-Form)
 */

static void disass19(FILE *f, unsigned int adr, unsigned int w)
{
	switch (exop) {
		case 0: xlform_crcr("mcrf"); break;

		case 16:
			if (w & 1)
				if (rd == 20)
					fprintf(f, "blrl\n");
				else
					xlform_b("bclrl");
			else
				if (rd == 20)
					fprintf(f, "blr\n");
				else
					xlform_b("bclr");
			break;

		case 33:
			if (ra == rb)
				fprintf(f, "crnot\tcrb%d,crb%d\n", rd, ra);
			else
				xlform_cr("crnor");
			break;

		case 50: xlform("rfi"); break;
		case 129: xlform_cr("crandc"); break;
		case 150: xlform("isync"); break;

		case 193:
			if (ra == rd && rb == rd)
				fprintf(f, "crclr\tcrb%d\n", rd);
			else
				xlform_cr("crxor");
			break;

		case 225: xlform_cr("crnand"); break;
		case 257: xlform_cr("crand"); break;

		case 289:
			if (ra == rd && rb == rd)
				fprintf(f, "crset\tcrb%d\n", rd);
			else
				xlform_cr("creqv");
			break;

		case 417: xlform_cr("crorc"); break;

		case 449:
			if (ra == rb)
				fprintf(f, "crmove\tcrb%d,crb%d\n", rd, ra);
			else
				xlform_cr("cror");
			break;

		case 528:
			if (w & 1)
				if (rd == 20)
					fprintf(f, "bctrl\n");
				else
					xlform_b("bcctrl");
			else
				if (rd == 20)
					fprintf(f, "bctr\n");
				else
					xlform_b("bcctr");
			break;

		default:
			fprintf(f, "?\n");
			break;
	}
}


/*
 *  Disassemble instruction with primary opcode = 31 (X-Form/XO-Form/XFX-Form/XS-Form)
 */

static void disass31(FILE *f, unsigned int adr, unsigned int w)
{
	switch (exop) {
		case 0:
			if (rd & 1)
				xform_crlab("cmpd");	// 64 bit
			else
				xform_crlab("cmpw");
			break;

		case 4:
			if (rd == 31 && ra == 0 && rb == 0)
				xform("trap");
			else if (to_code[rd] != NULL)
				fprintf(f, "tw%s\tr%d,r%d\n", to_code[rd], ra, rb);
			else
				xform_to("tw");
			break;

		case 6: xform_vls("lvsl"); break;
		case 7: xform_vls("lvebx"); break;
		case 8: xoform_dab("subfc"); break;
		case 8+512: xoform_dab("subfco"); break;
		case 9: case 9+512: xoform_dab("mulhdu"); break; // 64 bit
		case 10: xoform_dab("addc"); break;
		case 10+512: xoform_dab("addco"); break;
		case 11: case 11+512: xoform_dab("mulhwu"); break;
		case 19: xform_d("mfcr"); break;
		case 20: xform_ls("lwarx"); break;
		case 21: xform_ls("ldx"); break; // 64 bit
		case 23: xform_ls("lwzx"); break;
		case 24: xform_sabc("slw"); break;
		case 26: xform_sac("cntlzw"); break;
		case 27: xform_sabc("sld"); break; // 64 bit
		case 28: xform_sabc("and"); break;

		case 32:
			if (rd & 1)
				xform_crlab("cmpld");	// 64 bit
			else
				xform_crlab("cmplw");
			break;

		case 38: xform_vls("lvsr"); break;
		case 39: xform_vls("lvehx"); break;
		case 40: xoform_dab("subf"); break;
		case 40+512: xoform_dab("subfo"); break;
		case 53: xform_ls("ldux"); break; // 64 bit
		case 54: xform_ab("dcbst"); break;
		case 55: xform_ls("lwzux"); break;
		case 58: xform_sac("cntlzd"); break; // 64 bit
		case 60: xform_sabc("andc"); break;

		case 68:	// 64 bit
			if (to_code[rd] != NULL)
				fprintf(f, "td%s\tr%d,r%d\n", to_code[rd], ra, rb);
			else
				xform_to("td");
			break;

		case 71: xform_vls("lvewx"); break;
		case 73: case 73+512: xoform_dab("mulhd"); break; // 64 bit
		case 75: case 75+512: xoform_dab("mulhw"); break;
		case 83: xform_d("mfmsr"); break;
		case 84: xform_ls("ldarx"); break; // 64 bit
		case 86: xform_ab("dcbf"); break;
		case 87: xform_ls("lbzx"); break;
		case 103: xform_vls("lvx"); break;
		case 104: xoform_da("neg"); break;
		case 104+512: xoform_da("nego"); break;
		case 119: xform_ls("lbzux"); break;

		case 124:
			if (rd == rb)
				fprintf(f, "not%s\tr%d,r%d\n", w & 1 ? "." : "", ra, rd);
			else
				xform_sabc("nor");
			break;

		case 135: xform_vls("stvebx"); break;
		case 136: xoform_dab("subfe"); break;
		case 136+512: xoform_dab("subfeo"); break;
		case 138: xoform_dab("adde"); break;
		case 138+512: xoform_dab("addeo"); break;
		case 144: xfxform_crm("mtcrf"); break;
		case 146: xform_d("mtmsr"); break;
		case 149: xform_ls("stdx"); break; // 64 bit

		case 150:
			if (w & 1)
				xform_ls("stwcx.");
			else
				fprintf(f, "?\n");
			break;

		case 151: xform_ls("stwx"); break;
		case 167: xform_vls("stvehx"); break;
		case 181: xform_ls("stdux"); break; // 64 bit
		case 183: xform_ls("stwux"); break;
		case 199: xform_vls("stvewx"); break;
		case 200: xoform_da("subfze"); break;
		case 200+512: xoform_da("subfzeo"); break;
		case 202: xoform_da("addze"); break;
		case 202+512: xoform_da("addzeo"); break;
		case 210: xform_tsr("mtsr"); break;

		case 214:	// 64 bit
			if (w & 1)
				xform_ls("stdcx");
			else
				fprintf(f, "?\n");
			break;

		case 215: xform_ls("stbx"); break;
		case 231: xform_vls("stvx"); break;
		case 232: xoform_da("subfme"); break;
		case 232+512: xoform_da("subfmeo"); break;
		case 233: xoform_dab("mulld"); break; // 64 bit
		case 233+512: xoform_dab("mulldo"); break; // 64 bit
		case 234: xoform_da("addme"); break;
		case 234+512: xoform_da("addmeo"); break;
		case 235: xoform_dab("mullw"); break;
		case 235+512: xoform_dab("mullwo"); break;
		case 242: xform_db("mtsrin"); break;
		case 246: xform_ab("dcbtst"); break;
		case 247: xform_ls("stbux"); break;
		case 266: xoform_dab("add"); break;
		case 266+512: xoform_dab("addo"); break;
		case 278: xform_ab("dcbt"); break;
		case 279: xform_ls("lhzx"); break;
		case 284: xform_sabc("eqv"); break;
		case 306: xform_b("tlbie"); break;
		case 310: xform_ls("eciwx"); break;
		case 311: xform_ls("lhzux"); break;
		case 316: xform_sabc("xor"); break;

		case 339:
			if ((ra | (rb << 5)) == 1)
				fprintf(f, "mfxer\tr%d\n", rd);
			else if ((ra | (rb << 5)) == 8)
				fprintf(f, "mflr\tr%d\n", rd);
			else if ((ra | (rb << 5)) == 9)
				fprintf(f, "mfctr\tr%d\n", rd);
			else if ((ra | (rb << 5)) == 256)
				fprintf(f, "mfvrsave\tr%d\n", rd);
			else {
				const char *spr = get_spr(ra | (rb << 5));
				if (spr)
					fprintf(f, "mfspr\tr%d,%s\n", rd, spr);
				else
					xfxform_fspr("mfspr");
			}
			break;

		case 341: xform_ls("lwax"); break; // 64 bit
		case 343: xform_ls("lhax"); break;
		case 359: xform_vls("lvxl"); break;
		case 370: xform("tlbia"); break;

		case 822: // AltiVec
			if ((rd & 0xc) == 0 && ra == 0 && rb == 0 && (w & 1) == 0) {
				if (rd & 0x10)
					fprintf(f, "dssall\n");
				else
					fprintf(f, "dss\t%d\n", rd & 3);
			}
			else
				fprintf(f, "?\n");
			break;

		case 342: // AltiVec
			if ((rd & 0xc) == 0 && (w & 1) == 0)
				fprintf(f, "dst%s\tr%d,r%d,%d\n", rd & 0x10 ? "t" : "", ra, rb, rd & 3);
			else
				fprintf(f, "?\n");
			break;

		case 374: // AltiVec
			if ((rd & 0xc) == 0 && (w & 1) == 0)
				fprintf(f, "dstst%s\tr%d,r%d,%d\n", rd & 0x10 ? "t" : "", ra, rb, rd & 3);
			else
				fprintf(f, "?\n");
			break;

		case 371:
			if ((ra | (rb << 5)) == 268)
				xfxform_tb("mftb");
			else if ((ra | (rb << 5)) == 269)
				xfxform_tb("mftbu");
			else
				fprintf(f, "?\n");
			break;

		case 373: xform_ls("lwaux"); break; // 64 bit
		case 375: xform_ls("lhaux"); break;
		case 407: xform_ls("sthx"); break;
		case 412: xform_sabc("orc"); break;
		case 434: xform_b("slbie"); break; // 64 bit
		case 438: xform_ls("ecowx"); break;
		case 439: xform_ls("sthux"); break;

		case 444:
			if (rd == rb)
				fprintf(f, "mr%s\tr%d,r%d\n", w & 1 ? "." : "", ra, rd);
			else
				xform_sabc("or");
			break;

		case 457: xoform_dab("divdu"); break; // 64 bit
		case 457+512: xoform_dab("divduo"); break; // 64 bit
		case 459: xoform_dab("divwu"); break;
		case 459+512: xoform_dab("divwuo"); break;

		case 467:
			if ((ra | (rb << 5)) == 1)
				fprintf(f, "mtxer\tr%d\n", rd);
			else if ((ra | (rb << 5)) == 8)
				fprintf(f, "mtlr\tr%d\n", rd);
			else if ((ra | (rb << 5)) == 9)
				fprintf(f, "mtctr\tr%d\n", rd);
			else if ((ra | (rb << 5)) == 256)
				fprintf(f, "mtvrsave\tr%d\n", rd);
			else {
				const char *spr = get_spr(ra | (rb << 5));
				if (spr)
					fprintf(f, "mtspr\t%s,r%d\n", spr, rd);
				else
					xfxform_tspr("mtspr");
			}
			break;

		case 470: xform_ab("dcbi"); break;
		case 476: xform_sabc("nand"); break;
		case 487: xform_vls("stvxl"); break;
		case 489: xoform_dab("divd"); break; // 64 bit
		case 489+512: xoform_dab("divdo"); break; // 64 bit
		case 491: xoform_dab("divw"); break;
		case 491+512: xoform_dab("divwo"); break;
		case 498: xform("slbia"); break; // 64 bit
		case 512: xform_cr("mcrxr"); break;
		case 533: xform_ls("lswx"); break;
		case 534: xform_ls("lwbrx"); break;
		case 535: xform_fls("lfsx"); break;
		case 536: xform_sabc("srw"); break;
		case 539: xform_sabc("srd"); break; // 64 bit
		case 566: xform("tlbsync"); break;
		case 567: xform_fls("lfsux"); break;
		case 595: xform_fsr("mfsr"); break;
		case 597: xform_lsswi("lswi"); break;
		case 598: xform("sync"); break;
		case 599: xform_fls("lfdx"); break;
		case 631: xform_fls("lfdux"); break;
		case 659: xform_db("mfsrin"); break;
		case 661: xform_ls("stswx"); break;
		case 662: xform_ls("stwbrx"); break;
		case 663: xform_fls("stfsx"); break;
		case 695: xform_fls("stfsux"); break;
		case 725: xform_lsswi("stswi"); break;
		case 727: xform_fls("stfdx"); break;
		case 758: xform_ab("dcba"); break;
		case 759: xform_fls("stfdux"); break;
		case 790: xform_ls("lhbrx"); break;
		case 792: xform_sabc("sraw"); break;
		case 794: xform_sabc("srad"); break; // 64 bit
		case 824: xform_sash("srawi"); break;
		case 826: case 827: xsform("sradi"); break; // 64 bit
		case 854: xform("eieio"); break;
		case 918: xform_ls("sthbrx"); break;
		case 922: xform_sac("extsh"); break;
		case 954: xform_sac("extsb"); break;
		case 978: fprintf(f, "tlbld\tr%d\n", rb);	break; // 603
		case 982: xform_ab("icbi"); break;
		case 983: xform_fls("stfiwx"); break;
		case 986: xform_sac("extsw"); break; // 64 bit
		case 1010: fprintf(f, "tlbli\tr%d\n", rb);	break; // 603
		case 1014: xform_ab("dcbz"); break;

		default:
			fprintf(f, "?\n");
			break;
	}
}


/*
 *  Disassemble instruction with primary opcode = 59 (A-Form)
 */

static void disass59(FILE *f, unsigned int adr, unsigned int w)
{
	switch (exop & 0x1f) {
		case 18: aform_dab("fdivs"); break;
		case 20: aform_dab("fsubs"); break;
		case 21: aform_dab("fadds"); break;
		case 22: aform_db("fsqrts"); break;
		case 24: aform_db("fres"); break;
		case 25: aform_dac("fmuls"); break;
		case 28: aform_dacb("fmsubs"); break;
		case 29: aform_dacb("fmadds"); break;
		case 30: aform_dacb("fnmsubs"); break;
		case 31: aform_dacb("fnmadds"); break;

		default:
			fprintf(f, "?\n");
			break;
	}
}


/*
 *  Disassemble instruction with primary opcode = 63 (A-Form/X-Form/XFL-Form)
 */

static void disass63(FILE *f, unsigned int adr, unsigned int w)
{
	if (exop & 0x10)
		switch (exop & 0x1f) {
			case 18: aform_dab("fdiv"); break;
			case 20: aform_dab("fsub"); break;
			case 21: aform_dab("fadd"); break;
			case 22: aform_db("fsqrt"); break;
			case 23: aform_dacb("fsel"); break;
			case 25: aform_dac("fmul"); break;
			case 26: aform_db("frsqrte"); break;
			case 28: aform_dacb("fmsub"); break;
			case 29: aform_dacb("fmadd"); break;
			case 30: aform_dacb("fnmsub"); break;
			case 31: aform_dacb("fnmadd"); break;

			default:
				fprintf(f, "?\n");
				break;
		}
	else
		switch (exop) {
			case 0: xform_fcrab("fcmpu"); break;
			case 12: xform_fdb("frsp"); break;
			case 14: xform_fdb("fctiw"); break;
			case 15: xform_fdb("fctiwz"); break;
			case 32: xform_fcrab("fcmpo"); break;
			case 38: xform_crb("mtfsb1"); break;
			case 40: xform_fdb("fneg"); break;
			case 64: xform_crcr("mcrfs"); break;
			case 70: xform_crb("mtfsb0"); break;
			case 72: xform_fdb("fmr"); break;
			case 134: xform_cri("mtfsfi"); break;
			case 136: xform_fdb("fnabs"); break;
			case 264: xform_fdb("fabs"); break;
			case 583: xform_fd("mffs"); break;
			case 711: xflform("mtfsf"); break;
			case 814: xform_fdb("fctid"); break; // 64 bit
			case 815: xform_fdb("fctidz"); break; // 64 bit
			case 846: xform_fdb("fcfid"); break; // 64 bit

			default:
				fprintf(f, "?\n");
				break;
		}
}


/*
 *  Convert mask begin/end to mask
 */

static unsigned int mbme2mask(int mb, int me)
{
	unsigned int m = 0;
	int i;

	if (mb <= me)
		for (i=mb; i<=me; i++)
			m |= 1 << (31-i);
	else {
		for (i=0; i<=me; i++)
			m |= 1 << (31-i);
		for (i=mb; i<=31; i++)
			m |= 1 << (31-i);
	}
	return m;
}


/*
 *  Convert SPR number to register name
 */

const char *get_spr(int reg)
{
	switch (reg) {
		case 1: return "xer";
		case 8: return "lr";
		case 9: return "ctr";
		case 18: return "dsisr";
		case 19: return "dar";
		case 22: return "dec";
		case 25: return "sdr1";
		case 26: return "srr0";
		case 27: return "srr1";
		case 272: return "sprg0";
		case 273: return "sprg1";
		case 274: return "sprg2";
		case 275: return "sprg3";
		case 280: return "asr";		// 64 bit
		case 282: return "ear";
		case 284: return "tbl";
		case 285: return "tbu";
		case 287: return "pvr";
		case 528: return "ibat0u";
		case 529: return "ibat0l";
		case 530: return "ibat1u";
		case 531: return "ibat1l";
		case 532: return "ibat2u";
		case 533: return "ibat2l";
		case 534: return "ibat3u";
		case 535: return "ibat3l";
		case 536: return "dbat0u";
		case 537: return "dbat0l";
		case 538: return "dbat1u";
		case 539: return "dbat1l";
		case 540: return "dbat2u";
		case 541: return "dbat2l";
		case 542: return "dbat3u";
		case 543: return "dbat3l";
		case 1013: return "dabr";

		case 0: return "mq";		// 601
		case 4: return "rtcu";		// 601
		case 5: return "rtcl";		// 601
		case 20: return "rtcu";		// 601
		case 21: return "rtcl";		// 601
		case 952: return "mmcr0";	// 604
		case 953: return "pmc1";	// 604
		case 954: return "pmc2";	// 604
		case 955: return "sia";		// 604
		case 956: return "mmcr1";	// 604e
		case 957: return "pmc3";	// 604e
		case 958: return "pmc4";	// 604e
		case 959: return "sda";		// 604
		case 976: return "dmiss";	// 603
		case 977: return "dcmp";	// 603
		case 978: return "hash1";	// 603
		case 979: return "hash2";	// 603
		case 980: return "imiss";	// 603
		case 981: return "icmp";	// 603
		case 982: return "rpa";		// 603
		case 1008: return "hid0";	// 601/603/604
		case 1009: return "hid1";	// 601/603/604e
		case 1010: return "iabr";	// 601/603
		case 1023: return "pir";	// 601/604

		case 256: return "vrsave";	// AltiVec

		default: return NULL;
	}
}
