/*
 *  emul_ppc.cpp - PowerPC processor emulation
 *
 *  SheepShaver (C) 1997-2005 Christian Bauer and Marc Hellwig
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

/*
TODO:
addme
addmeo
addze
addzeo
dcbst
dcbt
dcbtst
divwuo
fabs
fadd
fadds
fcmpo
fcmpu
fctiw
fctiwz
fdiv
fdivs
fmadd
fmadds
fmr
fmsub
fmsubs
fmul
fmuls
fnabs
fneg
fnmadd
fnmadds
fnmsub
fnmsubs
fres
frsp
frsqrte
fsel
fsqrt
fsqrts
fsub
fsubs
lfdu
lfdux
lfdx
lfs
lfsu
lfsux
lfsx
lhbrx
lwbrx
mcrfs
mcrxr
mtfsb0
mtfsb1
mtfsfi
mulhwu
mullwo
nego
sc
stfdu
stfdux
stfdx
stfs
stfsu
stfsux
stfsx
sthbrx
stwbrx
subfo
subfme
subfmeo
subfze
subfzeo
tw
twi

CHECK:
crxor
creqv
 */

#include <stdio.h>
#include <signal.h>

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "main.h"
#include "xlowmem.h"
#include "emul_op.h"

#if ENABLE_MON
#include "mon.h"
#include "mon_disass.h"
#endif

#define DEBUG 1
#include "debug.h"

#define FLIGHT_RECORDER 1


// PowerPC user mode registers
uint32 r[32];
double fr[32];
uint32 lr, ctr;
uint32 cr, xer;
uint32 fpscr;
uint32 pc;

// Convert 8-bit field mask (e.g. mtcrf) to bit mask
static uint32 field2mask[256];


/*
 *  Flight recorder
 */

#if FLIGHT_RECORDER
struct rec_step {
	uint32 r[32];
	double fr[32];
	uint32 lr, ctr;
	uint32 cr, xer;
	uint32 fpscr;
	uint32 pc;
	uint32 opcode;
};

const int LOG_SIZE = 8192;
static rec_step log[LOG_SIZE];
static int log_ptr = 0;

static void record_step(uint32 opcode)
{
	for (int i=0; i<32; i++) {
		log[log_ptr].r[i] = r[i];
		log[log_ptr].fr[i] = fr[i];
	}
	log[log_ptr].lr = lr;
	log[log_ptr].ctr = ctr;
	log[log_ptr].cr = cr;
	log[log_ptr].xer = xer;
	log[log_ptr].fpscr = fpscr;
	log[log_ptr].pc = pc;
	log[log_ptr].opcode = opcode;
	log_ptr++;
	if (log_ptr == LOG_SIZE)
		log_ptr = 0;
}

static void dump_log(void)
{
	FILE *f = fopen("log", "w");
	if (f == NULL)
		return;
	for (int i=0; i<LOG_SIZE; i++) {
		int j = (i + log_ptr) % LOG_SIZE;
		fprintf(f, "pc %08x lr %08x ctr %08x cr %08x xer %08x ", log[j].pc, log[j].lr, log[j].ctr, log[j].cr, log[j].xer);
		fprintf(f, "r0 %08x r1 %08x r2 %08x r3 %08x ", log[j].r[0], log[j].r[1], log[j].r[2], log[j].r[3]);
		fprintf(f, "r4 %08x r5 %08x r6 %08x r7 %08x ", log[j].r[4], log[j].r[5], log[j].r[6], log[j].r[7]);
		fprintf(f, "r8 %08x r9 %08x r10 %08x r11 %08x ", log[j].r[8], log[j].r[9], log[j].r[10], log[j].r[11]);
		fprintf(f, "r12 %08x r13 %08x r14 %08x r15 %08x ", log[j].r[12], log[j].r[13], log[j].r[14], log[j].r[15]);
		fprintf(f, "r16 %08x r17 %08x r18 %08x r19 %08x ", log[j].r[16], log[j].r[17], log[j].r[18], log[j].r[19]);
		fprintf(f, "r20 %08x r21 %08x r22 %08x r23 %08x ", log[j].r[20], log[j].r[21], log[j].r[22], log[j].r[23]);
		fprintf(f, "r24 %08x r25 %08x r26 %08x r27 %08x ", log[j].r[24], log[j].r[25], log[j].r[26], log[j].r[27]);
		fprintf(f, "r28 %08x r29 %08x r30 %08x r31 %08x\n", log[j].r[28], log[j].r[29], log[j].r[30], log[j].r[31]);
#if ENABLE_MON
		disass_ppc(f, log[j].pc, log[j].opcode);
#endif
	}
	fclose(f);
}
#endif


/*
 *  Dump PPC registers
 */

static void dump(void)
{
	// Dump registers
	printf(" r0 %08x   r1 %08x   r2 %08x   r3 %08x\n", r[0], r[1], r[2], r[3]);
	printf(" r4 %08x   r5 %08x   r6 %08x   r7 %08x\n", r[4], r[5], r[6], r[7]);
	printf(" r8 %08x   r9 %08x  r10 %08x  r11 %08x\n", r[8], r[9], r[10], r[11]);
	printf("r12 %08x  r13 %08x  r14 %08x  r15 %08x\n", r[12], r[13], r[14], r[15]);
	printf("r16 %08x  r17 %08x  r18 %08x  r19 %08x\n", r[16], r[17], r[18], r[19]);
	printf("r20 %08x  r21 %08x  r22 %08x  r23 %08x\n", r[20], r[21], r[22], r[23]);
	printf("r24 %08x  r25 %08x  r26 %08x  r27 %08x\n", r[24], r[25], r[26], r[27]);
	printf("r28 %08x  r29 %08x  r30 %08x  r31 %08x\n", r[28], r[29], r[30], r[31]);
	printf(" lr %08x  ctr %08x   cr %08x  xer %08x\n", lr, ctr, cr, xer);
	printf(" pc %08x fpscr %08x\n", pc, fpscr);

	// Start up mon in real-mode
#if ENABLE_MON
	char *arg[4] = {"mon", "-m", "-r", NULL};
	mon(3, arg);
#endif
	QuitEmulator();
}


/*
 *  Record result in CR0
 */

static void record(uint32 val)
{
	uint32 crf = 0;
	if (val == 0)
		crf |= 0x20000000;
	else if (val & 0x80000000)
		crf |= 0x80000000;
	else
		crf |= 0x40000000;
	if (xer & 0x80000000)
		crf |= 0x10000000;
	cr = (cr & 0x0fffffff) | crf;
}


/*
 *  Record result in CR1
 */

static inline void record1(void)
{
	cr = (cr & 0xf0ffffff) | ((fpscr >> 4) & 0x0f000000);
}


/*
 *  Convert mask begin/end to mask
 */

static uint32 mbme2mask(uint32 op)
{
	uint32 mb = (op >> 6) & 0x1f;
	uint32 me = (op >> 1) & 0x1f;
	uint32 m = 0;
	uint32 i;

	if (mb <= me)
		for (i=mb; i<=me; i++)
			m |= 0x80000000 >> i;
	else {
		for (i=0; i<=me; i++)
			m |= 0x80000000 >> i;
		for (i=mb; i<=31; i++)
			m |= 0x80000000 >> i;
	}
	return m;
}


/*
 *  Emulate instruction with primary opcode = 19
 */

static void emul19(uint32 op)
{
	uint32 exop = (op >> 1) & 0x3ff;
	uint32 rd = (op >> 21) & 0x1f;
	uint32 ra = (op >> 16) & 0x1f;
	switch (exop) {

		case 0: {	// mcrf
			uint32 crfd = 0x1c - (rd & 0x1c);
			uint32 crfa = 0x1c - (ra & 0x1c);
			uint32 crf = (cr >> crfa) & 0xf;
			cr = (cr & ~(0xf << crfd)) | (crf << crfd);
			break;
		}

		case 16: {	// bclr
			uint32 oldpc = pc;
			if (!(rd & 4)) {
				ctr--;
				if (rd & 2) {
					if (ctr)
						goto blr_nobranch;
				} else {
					if (!ctr)
						goto blr_nobranch;
				}
			}
			if (!(rd & 0x10)) {
				if (rd & 8) {
					if (!(cr & (0x80000000 >> ra)))
						goto blr_nobranch;
				} else {
					if (cr & (0x80000000 >> ra))
						goto blr_nobranch;
				}
			}
			pc = lr & 0xfffffffc;
blr_nobranch:
			if (op & 1)
				lr = oldpc;
			break;
		}

		case 33:	// crnor
			if ((cr & (0x80000000 >> ra)) || ((cr & (0x80000000 >> ((op >> 11) & 0x1f)))))
				cr &= ~(0x80000000 >> rd);
			else
				cr |= 0x80000000 >> rd;
			break;

		case 129:	// crandc
			if ((cr & (0x80000000 >> ra)) && !((cr & (0x80000000 >> ((op >> 11) & 0x1f)))))
				cr |= 0x80000000 >> rd;
			else
				cr &= ~(0x80000000 >> rd);
			break;

		case 150:	// isync
			break;

		case 193: {	// crxor
			uint32 mask = 0x80000000 >> rd;
			cr = (((((cr >> (31 - ra)) ^ (cr >> (31 - ((op >> 11) & 0x1f)))) & 1) << (31 - rd)) & mask) | (cr & ~mask);
			break;
		}

		case 225:	// crnand
			if ((cr & (0x80000000 >> ra)) && ((cr & (0x80000000 >> ((op >> 11) & 0x1f)))))
				cr &= ~(0x80000000 >> rd);
			else
				cr |= 0x80000000 >> rd;
			break;

		case 257:	// crand
			if ((cr & (0x80000000 >> ra)) && ((cr & (0x80000000 >> ((op >> 11) & 0x1f)))))
				cr |= 0x80000000 >> rd;
			else
				cr &= ~(0x80000000 >> rd);
			break;

		case 289: {	// creqv
			uint32 mask = 0x80000000 >> rd;
			cr = (((~((cr >> (31 - ra)) ^ (cr >> (31 - ((op >> 11) & 0x1f)))) & 1) << (31 - rd)) & mask) | (cr & ~mask);
			break;
		}

		case 417:	// crorc
			if ((cr & (0x80000000 >> ra)) || !((cr & (0x80000000 >> ((op >> 11) & 0x1f)))))
				cr |= 0x80000000 >> rd;
			else
				cr &= ~(0x80000000 >> rd);
			break;

		case 449:	// cror
			if ((cr & (0x80000000 >> ra)) || ((cr & (0x80000000 >> ((op >> 11) & 0x1f)))))
				cr |= 0x80000000 >> rd;
			else
				cr &= ~(0x80000000 >> rd);
			break;

		case 528: {	// bcctr
			if (op & 1)
				lr = pc;
			if (!(rd & 4)) {
				ctr--;
				if (rd & 2) {
					if (ctr)
						goto bctr_nobranch;
				} else {
					if (!ctr)
						goto bctr_nobranch;
				}
			}
			if (!(rd & 0x10)) {
				if (rd & 8) {
					if (!(cr & (0x80000000 >> ra)))
						goto bctr_nobranch;
				} else {
					if (cr & (0x80000000 >> ra))
						goto bctr_nobranch;
				}
			}
			pc = ctr & 0xfffffffc;
bctr_nobranch:
			break;
		}

		default:
			printf("Illegal 19 opcode %08x (exop %d) at %08x\n", op, exop, pc-4);
			dump();
			break;
	}
}


/*
 *  Emulate instruction with primary opcode = 31
 */

static void emul31(uint32 op)
{
	uint32 exop = (op >> 1) & 0x3ff;
	uint32 rd = (op >> 21) & 0x1f;
	uint32 ra = (op >> 16) & 0x1f;
	uint32 rb = (op >> 11) & 0x1f;
	switch (exop) {

		case 0:	{	// cmpw
			uint32 crfd = 0x1c - (rd & 0x1c);
			uint8 crf = 0;
			if (r[ra] == r[rb])
				crf |= 2;
			else if ((int32)r[ra] < (int32)r[rb])
				crf |= 8;
			else
				crf |= 4;
			if (xer & 0x80000000)
				crf |= 1;
			cr = (cr & ~(0xf << crfd)) | (crf << crfd);
			break;
		}

		case 8: {	// subfc
			uint64 tmp = (uint64)r[rb] - (uint64)r[ra];
			r[rd] = tmp;
			if (tmp & 0x100000000LL)
				xer &= ~0x20000000;
			else
				xer |= 0x20000000;
			if (op & 1)
				record(r[rd]);
			break;
		}

		case 10: {	// addc
			uint64 tmp = (uint64)r[ra] + (uint64)r[rb];
			r[rd] = tmp;
			if (tmp & 0x100000000LL)
				xer |= 0x20000000;
			else
				xer &= ~0x20000000;
			if (op & 1)
				record(r[rd]);
			break;
		}

		case 19:	// mfcr
			r[rd] = cr;
			break;

		case 20:	// lwarx
			r[rd] = ReadMacInt32(r[rb] + (ra ? r[ra] : 0));
			//!! set reservation bit
			break;

		case 23:	// lwzx
			r[rd] = ReadMacInt32(r[rb] + (ra ? r[ra] : 0));
			break;

		case 24:	// slw
			r[ra] = r[rd] << (r[rb] & 0x3f);
			if (op & 1)
				record(r[ra]);
			break;

		case 26: {	// cntlzw
			uint32 mask = 0x80000000;
			for (int i=0; i<32; i++, mask>>=1) {
				if (r[rd] & mask) {
					r[ra] = i;
					goto cntlzw_done;
				}
			}
			r[ra] = 32;
cntlzw_done:if (op & 1)
				record(r[ra]);
			break;
		}

		case 28:	// and
			r[ra] = r[rd] & r[rb];
			if (op & 1)
				record(r[ra]);
			break;

		case 32: {	// cmplw
			uint32 crfd = 0x1c - (rd & 0x1c);
			uint8 crf = 0;
			if (r[ra] == r[rb])
				crf |= 2;
			else if (r[ra] < r[rb])
				crf |= 8;
			else
				crf |= 4;
			if (xer & 0x80000000)
				crf |= 1;
			cr = (cr & ~(0xf << crfd)) | (crf << crfd);
			break;
		}

		case 40:	// subf
			r[rd] = r[rb] - r[ra];
			if (op & 1)
				record(r[rd]);
			break;

		case 55:	// lwzux
			r[ra] += r[rb];
			r[rd] = ReadMacInt32(r[ra]);
			break;

		case 60:	// andc
			r[ra] = r[rd] & ~r[rb];
			if (op & 1)
				record(r[ra]);
			break;

		case 75:	// mulhw
			r[rd] = ((int64)(int32)r[ra] * (int32)r[rb]) >> 32;
			if (op & 1)
				record(r[rd]);
			break;

		case 86:	// dcbf
			break;

		case 87:	// lbzx
			r[rd] = ReadMacInt8(r[rb] + (ra ? r[ra] : 0));
			break;

		case 104:	// neg
			if (r[ra] == 0x80000000)
				r[rd] = 0x80000000;
			else
				r[rd] = -(int32)r[ra];
			if (op & 1)
				record(r[rd]);
			break;

		case 119:	// lbzux
			r[ra] += r[rb];
			r[rd] = ReadMacInt8(r[ra]);
			break;

		case 124:	// nor
			r[ra] = ~(r[rd] | r[rb]);
			if (op & 1)
				record(r[ra]);
			break;

		case 136: {	// subfe
			uint64 tmp = (uint64)r[rb] - (uint64)r[ra];
			if (!(xer & 0x20000000))
				tmp--;
			r[rd] = tmp;
			if (tmp & 0x100000000LL)
				xer &= ~0x20000000;
			else
				xer |= 0x20000000;
			if (op & 1)
				record(r[rd]);
			break;
		}

		case 138: {	// adde
			uint64 tmp = (uint64)r[ra] + (uint64)r[rb];
			if (xer & 0x20000000)
				tmp++;
			r[rd] = tmp;
			if (tmp & 0x100000000LL)
				xer |= 0x20000000;
			else
				xer &= ~0x20000000;
			if (op & 1)
				record(r[rd]);
			break;
		}

		case 144: {	// mtcrf
			uint32 mask = field2mask[(op >> 12) & 0xff];
			cr = (r[rd] & mask) | (cr & ~mask);
			break;
		}

		case 150:	// stwcx
			//!! check reserved bit
			WriteMacInt32(r[rb] + (ra ? r[ra] : 0), r[rd]);
			record(0);
			break;

		case 151:	// stwx
			WriteMacInt32(r[rb] + (ra ? r[ra] : 0), r[rd]);
			break;

		case 183:	// stwux
			r[ra] += r[rb];
			WriteMacInt32(r[ra], r[rd]);
			break;

		case 215:	// stbx
			WriteMacInt8(r[rb] + (ra ? r[ra] : 0), r[rd]);
			break;

		case 235:	// mullw
			r[rd] = (int32)r[ra] * (int32)r[rb];
			if (op & 1)
				record(r[rd]);
			break;

		case 247:	// stbux
			r[ra] += r[rb];
			WriteMacInt8(r[ra], r[rd]);
			break;

		case 266:	// add
			r[rd] = r[ra] + r[rb];
			if (op & 1)
				record(r[rd]);
			break;

		case 279:	// lhzx
			r[rd] = ReadMacInt16(r[rb] + (ra ? r[ra] : 0));
			break;

		case 284:	// eqv
			r[ra] = ~(r[rd] ^ r[rb]);
			if (op & 1)
				record(r[ra]);
			break;

		case 311:	// lhzux
			r[ra] += r[rb];
			r[rd] = ReadMacInt16(r[ra]);
			break;

		case 316:	// xor
			r[ra] = r[rd] ^ r[rb];
			if (op & 1)
				record(r[ra]);
			break;

		case 339: {	// mfspr
			uint32 spr = ra | (rb << 5);
			switch (spr) {
				case 1: r[rd] = xer; break;
				case 8: r[rd] = lr; break;
				case 9: r[rd] = ctr; break;
				default:
					printf("Illegal mfspr opcode %08x at %08x\n", op, pc-4);
					dump();
			}
			break;
		}

		case 343:	// lhax
			r[rd] = (int32)(int16)ReadMacInt16(r[rb] + (ra ? r[ra] : 0));
			break;

		case 371:	// mftb
			r[rd] = 0;	//!!
			break;

		case 375:	// lhaux
			r[ra] += r[rb];
			r[rd] = (int32)(int16)ReadMacInt16(r[ra]);
			break;

		case 407:	// sthx
			WriteMacInt16(r[rb] + (ra ? r[ra] : 0), r[rd]);
			break;

		case 412:	// orc
			r[ra] = r[rd] | ~r[rb];
			if (op & 1)
				record(r[ra]);
			break;

		case 439:	// sthux
			r[ra] += r[rb];
			WriteMacInt16(r[ra], r[rd]);
			break;

		case 444:	// or
			r[ra] = r[rd] | r[rb];
			if (op & 1)
				record(r[ra]);
			break;

		case 459:	// divwu
			if (r[rb])
				r[rd] = r[ra] / r[rb];
			if (op & 1)
				record(r[rd]);
			break;

		case 467: {	// mtspr
			uint32 spr = ra | (rb << 5);
			switch (spr) {
				case 1: xer = r[rd] & 0xe000007f; break;
				case 8: lr = r[rd]; break;
				case 9: ctr = r[rd]; break;
				default:
					printf("Illegal mtspr opcode %08x at %08x\n", op, pc-4);
					dump();
			}
			break;
		}

		case 476:	// nand
			r[ra] = ~(r[rd] & r[rb]);
			if (op & 1)
				record(r[ra]);
			break;

		case 491:	// divw
			if (r[rb])
				r[rd] = (int32)r[ra] / (int32)r[rb];
			if (op & 1)
				record(r[rd]);
			break;

		case 520: {	// subfco
			uint64 tmp = (uint64)r[rb] - (uint64)r[ra];
			uint32 ov = (r[ra] ^ r[rb]) & ((uint32)tmp ^ r[rb]);
			r[rd] = tmp;
			if (tmp & 0x100000000LL)
				xer &= ~0x20000000;
			else
				xer |= 0x20000000;
			if (ov & 0x80000000)
				xer |= 0xc0000000;
			else
				xer &= ~0x40000000;
			if (op & 1)
				record(r[rd]);
			break;
		}

		case 522: {	// addco
			uint64 tmp = (uint64)r[ra] + (uint64)r[rb];
			uint32 ov = (r[ra] ^ (uint32)tmp) & (r[rb] ^ (uint32)tmp);
			r[rd] = tmp;
			if (tmp & 0x100000000LL)
				xer |= 0x20000000;
			else
				xer &= ~0x20000000;
			if (ov & 0x80000000)
				xer |= 0xc0000000;
			else
				xer &= ~0x40000000;
			if (op & 1)
				record(r[rd]);
			break;
		}

		case 533: {	// lswx
			uint32 addr = r[rb] + (ra ? r[ra] : 0);
			int nb = xer & 0x7f;
			int reg = rd;
			for (int i=0; i<nb; i++) {
				switch (i & 3) {
					case 0:
						r[reg] = ReadMacInt8(addr + i) << 24;
						break;
					case 1:
						r[reg] = (r[reg] & 0xff00ffff) | (ReadMacInt8(addr + i) << 16);
						break;
					case 2:
						r[reg] = (r[reg] & 0xffff00ff) | (ReadMacInt8(addr + i) << 8);
						break;
					case 3:
						r[reg] = (r[reg] & 0xffffff00) | ReadMacInt8(addr + i);
						reg = (reg + 1) & 0x1f;
						break;
				}
			}
			break;
		}

		case 536:	// srw
			r[ra] = r[rd] >> (r[rb] & 0x3f);
			if (op & 1)
				record(r[ra]);
			break;

		case 597: {	// lswi
			uint32 addr = ra ? r[ra] : 0;
			int nb = rb ? rb : 32;
			int reg = rd;
			for (int i=0; i<nb; i++) {
				switch (i & 3) {
					case 0:
						r[reg] = ReadMacInt8(addr + i) << 24;
						break;
					case 1:
						r[reg] = (r[reg] & 0xff00ffff) | (ReadMacInt8(addr + i) << 16);
						break;
					case 2:
						r[reg] = (r[reg] & 0xffff00ff) | (ReadMacInt8(addr + i) << 8);
						break;
					case 3:
						r[reg] = (r[reg] & 0xffffff00) | ReadMacInt8(addr + i);
						reg = (reg + 1) & 0x1f;
						break;
				}
			}
			break;
		}

		case 598:	// sync
			break;

		case 648: {	// subfeo
			uint64 tmp = (uint64)r[rb] - (uint64)r[ra];
			if (!(xer & 0x20000000))
				tmp--;
			uint32 ov = (r[ra] ^ r[rb]) & ((uint32)tmp ^ r[rb]);
			r[rd] = tmp;
			if (tmp & 0x100000000LL)
				xer &= ~0x20000000;
			else
				xer |= 0x20000000;
			if (ov & 0x80000000)
				xer |= 0xc0000000;
			else
				xer &= ~0x40000000;
			if (op & 1)
				record(r[rd]);
			break;
		}

		case 650: {	// addeo
			uint64 tmp = (uint64)r[ra] + (uint64)r[rb];
			if (xer & 0x20000000)
				tmp++;
			uint32 ov = (r[ra] ^ (uint32)tmp) & (r[rb] ^ (uint32)tmp);
			r[rd] = tmp;
			if (tmp & 0x100000000LL)
				xer |= 0x20000000;
			else
				xer &= ~0x20000000;
			if (ov & 0x80000000)
				xer |= 0xc0000000;
			else
				xer &= ~0x40000000;
			if (op & 1)
				record(r[rd]);
			break;
		}

		case 661: {	// stswx
			uint32 addr = r[rb] + (ra ? r[ra] : 0);
			int nb = xer & 0x7f;
			int reg = rd;
			int shift = 24;
			for (int i=0; i<nb; i++) {
				WriteMacInt8(addr + i, (r[reg] >> shift));
				shift -= 8;
				if ((i & 3) == 3) {
					shift = 24;
					reg = (reg + 1) & 0x1f;
				}
			}
			break;
		}

		case 725: {	// stswi
			uint32 addr = ra ? r[ra] : 0;
			int nb = rb ? rb : 32;
			int reg = rd;
			int shift = 24;
			for (int i=0; i<nb; i++) {
				WriteMacInt8(addr + i, (r[reg] >> shift));
				shift -= 8;
				if ((i & 3) == 3) {
					shift = 24;
					reg = (reg + 1) & 0x1f;
				}
			}
			break;
		}

		case 778: {	// addo
			uint32 tmp = r[ra] + r[rb];
			uint32 ov = (r[ra] ^ tmp) & (r[rb] ^ tmp);
			r[rd] = tmp;
			if (ov & 0x80000000)
				xer |= 0xc0000000;
			else
				xer &= ~0x40000000;
			if (op & 1)
				record(r[rd]);
			break;
		}

		case 792: {	// sraw
			uint32 sh = r[rb] & 0x3f;
			uint32 mask = ~(0xffffffff << sh);
			if ((r[rd] & 0x80000000) && (r[rd] & mask))
				xer |= 0x20000000;
			else
				xer &= ~0x20000000;
			r[ra] = (int32)r[rd] >> sh;
			if (op & 1)
				record(r[ra]);
			break;
		}

		case 824: {	// srawi
			uint32 mask = ~(0xffffffff << rb);
			if ((r[rd] & 0x80000000) && (r[rd] & mask))
				xer |= 0x20000000;
			else
				xer &= ~0x20000000;
			r[ra] = (int32)r[rd] >> rb;
			if (op & 1)
				record(r[ra]);
			break;
		}

		case 854:	// eieio
			break;

		case 922:	// extsh
			r[ra] = (int32)(int16)r[rd];
			if (op & 1)
				record(r[ra]);
			break;

		case 954:	// extsb
			r[ra] = (int32)(int8)r[rd];
			if (op & 1)
				record(r[ra]);
			break;

		case 982:	// icbi
			break;

		case 1003:	// divwo
			if (r[rb] == 0 || (r[ra] == 0x80000000 && r[rb] == 0xffffffff))
				xer |= 0xc0000000;
			else {
				r[rd] = (int32)r[ra] / (int32)r[rb];
				xer &= ~0x40000000;
			}
			if (op & 1)
				record(r[rd]);
			break;

#if 0
		case 1014:	// dcbz
			memset(r[rb] + (ra ? r[ra] : 0), 0, 32);
			break;
#endif

		default:
			printf("Illegal 31 opcode %08x (exop %d) at %08x\n", op, exop, pc-4);
			dump();
			break;
	}
}


/*
 *  Emulate instruction with primary opcode = 59
 */

static void emul59(uint32 op)
{
	uint32 exop = (op >> 1) & 0x3ff;
	switch (exop) {
		default:
			printf("Illegal 59 opcode %08x (exop %d) at %08x\n", op, exop, pc-4);
			dump();
			break;
	}
}


/*
 *  Emulate instruction with primary opcode = 63
 */

static void emul63(uint32 op)
{
	uint32 exop = (op >> 1) & 0x3ff;
	uint32 rd = (op >> 21) & 0x1f;
	uint32 ra = (op >> 16) & 0x1f;
	uint32 rb = (op >> 11) & 0x1f;
	switch (exop) {

		case 583:	// mffs
			fr[rd] = (double)(uint64)fpscr;
			if (op & 1)
				record1();
			break;

		case 711:	// mtfsf
			//!!
			if (op & 1)
				record1();
			break;

		default:
			printf("Illegal 63 opcode %08x (exop %d) at %08x\n", op, exop, pc-4);
			dump();
			break;
	}
}


/*
 *  Emulation loop
 */

void emul_ppc(uint32 start)
{
	pc = start;
//uint32 old_val = 0;
	for (;;) {
//uint32 val = ReadMacInt32(0x68fff778);
//if (val != old_val) {
//	printf("insn at %08lx changed %08lx->%08lx\n", pc-4, old_val, val);
//	old_val = val;
//}
		uint32 op = ReadMacInt32(pc);
#if FLIGHT_RECORDER
		record_step(op);
#endif
//		printf("%08lx at %08lx\n", op, pc);
		uint32 primop = op >> 26;
		pc += 4;
		switch (primop) {

			case 6:		// SheepShaver extensions
				printf("Extended opcode %08x at %08x (68k pc %08x)\n", op, pc-4, r[24]);
				switch (op & 0x3f) {
					case 0:		// EMUL_RETURN
						QuitEmulator();
						break;

					case 1:		// EXEC_RETURN
						//!!
						dump();
						break;

					default: {	// EMUL_OP
						M68kRegisters r68;
						WriteMacInt32(XLM_68K_R25, r[25]);
						WriteMacInt32(XLM_RUN_MODE, MODE_EMUL_OP);
						for (int i=0; i<8; i++)
							r68.d[i] = r[8 + i];
						for (int i=0; i<7; i++)
							r68.a[i] = r[16 + i];
						r68.a[7] = r[1];
						EmulOp(&r68, r[24], (op & 0x3f) - 2);
						for (int i=0; i<8; i++)
							r[8 + i] = r68.d[i];
						for (int i=0; i<7; i++)
							r[16 + i] = r68.a[i];
						r[1] = r68.a[7];
						WriteMacInt32(XLM_RUN_MODE, MODE_68K);
						break;
					}
				}
				break;

			case 7: {	// mulli
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				r[rd] = (int32)r[ra] * (int32)(int16)(op & 0xffff);
				break;
			}

			case 8: {	// subfic
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				uint64 tmp = (uint32)(int32)(int16)(op & 0xffff) - (uint64)r[ra];
				r[rd] = tmp;
				if (tmp & 0x100000000LL)
					xer &= ~0x20000000;
				else
					xer |= 0x20000000;
				break;
			}

			case 10: {	// cmpli
				uint32 crfd = 0x1c - ((op >> 21) & 0x1c);
				uint32 ra = (op >> 16) & 0x1f;
				uint32 val = (op & 0xffff);
				uint8 crf = 0;
				if (r[ra] == val)
					crf |= 2;
				else if (r[ra] < val)
					crf |= 8;
				else
					crf |= 4;
				if (xer & 0x80000000)
					crf |= 1;
				cr = (cr & ~(0xf << crfd)) | (crf << crfd);
				break;
			}

			case 11: {	// cmpi
				uint32 crfd = 0x1c - ((op >> 21) & 0x1c);
				uint32 ra = (op >> 16) & 0x1f;
				int32 val = (int32)(int16)(op & 0xffff);
				uint8 crf = 0;
				if ((int32)r[ra] == val)
					crf |= 2;
				else if ((int32)r[ra] < val)
					crf |= 8;
				else
					crf |= 4;
				if (xer & 0x80000000)
					crf |= 1;
				cr = (cr & ~(0xf << crfd)) | (crf << crfd);
				break;
			}

			case 12: {	// addic
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				uint64 tmp = (uint64)r[ra] + (uint32)(int32)(int16)(op & 0xffff);
				r[rd] = tmp;
				if (tmp & 0x100000000LL)
					xer |= 0x20000000;
				else
					xer &= ~0x20000000;
				break;
			}

			case 13: {	// addic.
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				uint64 tmp = (uint64)r[ra] + (uint32)(int32)(int16)(op & 0xffff);
				r[rd] = tmp;
				if (tmp & 0x100000000LL)
					xer |= 0x20000000;
				else
					xer &= ~0x20000000;
				record(r[rd]);
				break;
			}

			case 14: {	// addi
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				r[rd] = (ra ? r[ra] : 0) + (int32)(int16)(op & 0xffff);
				break;
			}

			case 15: {	// addis
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				r[rd] = (ra ? r[ra] : 0) + (op << 16);
				break;
			}

			case 16: {	// bc
				uint32 bo = (op >> 21) & 0x1f;
				uint32 bi = (op >> 16) & 0x1f;
				if (op & 1)
					lr = pc;
				if (!(bo & 4)) {
					ctr--;
					if (bo & 2) {
						if (ctr)
							goto bc_nobranch;
					} else {
						if (!ctr)
							goto bc_nobranch;
					}
				}
				if (!(bo & 0x10)) {
					if (bo & 8) {
						if (!(cr & (0x80000000 >> bi)))
							goto bc_nobranch;
					} else {
						if (cr & (0x80000000 >> bi))
							goto bc_nobranch;
					}
				}
				if (op & 2)
					pc = (int32)(int16)(op & 0xfffc);
				else
					pc += (int32)(int16)(op & 0xfffc) - 4;
bc_nobranch:
				break;
			}

			case 18: {	// b
				int32 target = op & 0x03fffffc;
				if (target & 0x02000000)
					target |= 0xfc000000;
				if (op & 1)
					lr = pc;
				if (op & 2)
					pc = target;
				else
					pc += target - 4;
				break;
			}

			case 19:
				emul19(op);
				break;

			case 20: {	// rlwimi
				uint32 rs = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				uint32 sh = (op >> 11) & 0x1f;
				uint32 mask = mbme2mask(op);
				r[ra] = (((r[rs] << sh) | (r[rs] >> (32-sh))) & mask) | (r[ra] & ~mask);
				if (op & 1)
					record(r[ra]);
				break;
			}

			case 21: {	// rlwinm
				uint32 rs = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				uint32 sh = (op >> 11) & 0x1f;
				r[ra] = ((r[rs] << sh) | (r[rs] >> (32-sh))) & mbme2mask(op);
				if (op & 1)
					record(r[ra]);
				break;
			}

			case 23: {	// rlwnm
				uint32 rs = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				uint32 sh = r[(op >> 11) & 0x1f] & 0x1f;
				r[ra] = ((r[rs] << sh) | (r[rs] >> (32-sh))) & mbme2mask(op);
				if (op & 1)
					record(r[ra]);
				break;
			}

			case 24: {	// ori
				uint32 rs = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				r[ra] = r[rs] | (op & 0xffff);
				break;
			}

			case 25: {	// oris
				uint32 rs = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				r[ra] = r[rs] | (op << 16);
				break;
			}

			case 26: {	// xori
				uint32 rs = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				r[ra] = r[rs] ^ (op & 0xffff);
				break;
			}

			case 27: {	// xoris
				uint32 rs = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				r[ra] = r[rs] ^ (op << 16);
				break;
			}

			case 28: {	// andi.
				uint32 rs = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				r[ra] = r[rs] & (op & 0xffff);
				record(r[ra]);
				break;
			}

			case 29: {	// andis.
				uint32 rs = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				r[ra] = r[rs] & (op << 16);
				record(r[ra]);
				break;
			}

			case 31:
				emul31(op);
				break;

			case 32: {	// lwz
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				r[rd] = ReadMacInt32(int16(op & 0xffff) + (ra ? r[ra] : 0));
				break;
			}

			case 33: {	// lwzu
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				r[ra] += int16(op & 0xffff);
				r[rd] = ReadMacInt32(r[ra]);
				break;
			}

			case 34: {	// lbz
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				r[rd] = ReadMacInt8(int16(op & 0xffff) + (ra ? r[ra] : 0));
				break;
			}

			case 35: {	// lbzu
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				r[ra] += int16(op & 0xffff);
				r[rd] = ReadMacInt8(r[ra]);
				break;
			}

			case 36: {	// stw
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				WriteMacInt32(int16(op & 0xffff) + (ra ? r[ra] : 0), r[rd]);
				break;
			}

			case 37: {	// stwu
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				r[ra] += int16(op & 0xffff);
				WriteMacInt32(r[ra], r[rd]);
				break;
			}

			case 38: {	// stb
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				WriteMacInt8(int16(op & 0xffff) + (ra ? r[ra] : 0), r[rd]);
				break;
			}

			case 39: {	// stbu
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				r[ra] += int16(op & 0xffff);
				WriteMacInt8(r[ra], r[rd]);
				break;
			}

			case 40: {	// lhz
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				r[rd] = ReadMacInt16(int16(op & 0xffff) + (ra ? r[ra] : 0));
				break;
			}

			case 41: {	// lhzu
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				r[ra] += int16(op & 0xffff);
				r[rd] = ReadMacInt16(r[ra]);
				break;
			}

			case 42: {	// lha
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				r[rd] = (int32)(int16)ReadMacInt16(int16(op & 0xffff) + (ra ? r[ra] : 0));
				break;
			}

			case 43: {	// lhau
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				r[ra] += int16(op & 0xffff);
				r[rd] = (int32)(int16)ReadMacInt16(r[ra]);
				break;
			}

			case 44: {	// sth
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				WriteMacInt16(int16(op & 0xffff) + (ra ? r[ra] : 0), r[rd]);
				break;
			}

			case 45: {	// sthu
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				r[ra] += int16(op & 0xffff);
				WriteMacInt16(r[ra], r[rd]);
				break;
			}

			case 46: {	// lmw
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				uint32 addr = int16(op & 0xffff) + (ra ? r[ra] : 0);
				while (rd <= 31) {
					r[rd] = ReadMacInt32(addr);
					rd++;
					addr += 4;
				}
				break;
			}

			case 47: {	// stmw
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				uint32 addr = int16(op & 0xffff) + (ra ? r[ra] : 0);
				while (rd <= 31) {
					WriteMacInt32(addr, r[rd]);
					rd++;
					addr += 4;
				}
				break;
			}

			case 50: {	// lfd
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				fr[rd] = (double)ReadMacInt64(int16(op & 0xffff) + (ra ? r[ra] : 0));
				break;
			}

			case 54: {	// stfd
				uint32 rd = (op >> 21) & 0x1f;
				uint32 ra = (op >> 16) & 0x1f;
				WriteMacInt64(int16(op & 0xffff) + (ra ? r[ra] : 0), (uint64)fr[rd]);
				break;
			}

			case 59:
				emul59(op);
				break;

			case 63:
				emul63(op);
				break;

			default:
				printf("Illegal opcode %08x at %08x\n", op, pc-4);
				dump();
				break;
		}
	}
}


static struct sigaction sigsegv_action;

static void sigsegv_handler(int sig)
{
	printf("SIGSEGV\n");
	dump();
}

void init_emul_ppc(void)
{
	// Init field2mask
	for (int i=0; i<256; i++) {
		uint32 mask = 0;
		if (i & 0x01) mask |= 0x0000000f;
		if (i & 0x02) mask |= 0x000000f0;
		if (i & 0x04) mask |= 0x00000f00;
		if (i & 0x08) mask |= 0x0000f000;
		if (i & 0x10) mask |= 0x000f0000;
		if (i & 0x20) mask |= 0x00f00000;
		if (i & 0x40) mask |= 0x0f000000;
		if (i & 0x80) mask |= 0xf0000000;
		field2mask[i] = mask;
	}

	// Init registers
	for (int i=0; i<32; i++) {
		r[i] = 0;
		fr[i] = 0.0;
	}
	lr = ctr = 0;
	cr = xer = 0;
	fpscr = 0;

	r[3] = ROM_BASE + 0x30d000;

	// Install SIGSEGV handler
	sigemptyset(&sigsegv_action.sa_mask);
	sigsegv_action.sa_handler = (__sighandler_t)sigsegv_handler;
	sigsegv_action.sa_flags = 0;
	sigsegv_action.sa_restorer = NULL;
	sigaction(SIGSEGV, &sigsegv_action, NULL);

#if FLIGHT_RECORDER && ENABLE_MON
	// Install "log" command in mon
	mon_add_command("log", dump_log, "log                      Dump PowerPC emulation log\n");
#endif
}


/*
 *  Execute 68k subroutine (must be ended with EXEC_RETURN)
 *  This must only be called by the emul_thread when in EMUL_OP mode
 *  r->a[7] is unused, the routine runs on the caller's stack
 */

void Execute68k(uint32 pc, M68kRegisters *r)
{
	printf("ERROR: Execute68k() unimplemented\n");
	QuitEmulator();
}


/*
 *  Execute 68k A-Trap from EMUL_OP routine
 *  r->a[7] is unused, the routine runs on the caller's stack
 */

void Execute68kTrap(uint16 trap, M68kRegisters *r)
{
	printf("ERROR: Execute68kTrap() unimplemented\n");
	QuitEmulator();
}


/*
 *  Call MacOS PPC code
 */

uint32 call_macos(uint32 tvect)
{
	printf("ERROR: call_macos() unimplemented\n");
	QuitEmulator();
	return 0;
}

uint32 call_macos1(uint32 tvect, uint32 arg1)
{
	printf("ERROR: call_macos1() unimplemented\n");
	QuitEmulator();
	return 0;
}

uint32 call_macos2(uint32 tvect, uint32 arg1, uint32 arg2)
{
	printf("ERROR: call_macos2() unimplemented\n");
	QuitEmulator();
	return 0;
}

uint32 call_macos3(uint32 tvect, uint32 arg1, uint32 arg2, uint32 arg3)
{
	printf("ERROR: call_macos3() unimplemented\n");
	QuitEmulator();
	return 0;
}

uint32 call_macos4(uint32 tvect, uint32 arg1, uint32 arg2, uint32 arg3, uint32 arg4)
{
	printf("ERROR: call_macos4() unimplemented\n");
	QuitEmulator();
	return 0;
}

uint32 call_macos5(uint32 tvect, uint32 arg1, uint32 arg2, uint32 arg3, uint32 arg4, uint32 arg5)
{
	printf("ERROR: call_macos5() unimplemented\n");
	QuitEmulator();
	return 0;
}

uint32 call_macos6(uint32 tvect, uint32 arg1, uint32 arg2, uint32 arg3, uint32 arg4, uint32 arg5, uint32 arg6)
{
	printf("ERROR: call_macos6() unimplemented\n");
	QuitEmulator();
	return 0;
}

uint32 call_macos7(uint32 tvect, uint32 arg1, uint32 arg2, uint32 arg3, uint32 arg4, uint32 arg5, uint32 arg6, uint32 arg7)
{
	printf("ERROR: call_macos7() unimplemented\n");
	QuitEmulator();
	return 0;
}


/*
 *  Atomic operations
 */

extern int atomic_add(int *var, int v)
{
	int ret = *var;
	*var += v;
	return ret;
}

extern int atomic_and(int *var, int v)
{
	int ret = *var;
	*var &= v;
	return ret;
}

extern int atomic_or(int *var, int v)
{
	int ret = *var;
	*var |= v;
	return ret;
}


extern "C" void get_resource(void);
extern "C" void get_1_resource(void);
extern "C" void get_ind_resource(void);
extern "C" void get_1_ind_resource(void);
extern "C" void r_get_resource(void);

void get_resource(void)
{
	printf("ERROR: get_resource() unimplemented\n");
	QuitEmulator();
}

void get_1_resource(void)
{
	printf("ERROR: get_1_resource() unimplemented\n");
	QuitEmulator();
}

void get_ind_resource(void)
{
	printf("ERROR: get_ind_resource() unimplemented\n");
	QuitEmulator();
}

void get_1_ind_resource(void)
{
	printf("ERROR: get_1_ind_resource() unimplemented\n");
	QuitEmulator();
}

void r_get_resource(void)
{
	printf("ERROR: r_get_resource() unimplemented\n");
	QuitEmulator();
}
