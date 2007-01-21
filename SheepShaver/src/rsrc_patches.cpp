/*
 *  rsrc_patches.cpp - Resource patches
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sysdeps.h"
#include "rsrc_patches.h"
#include "cpu_emulation.h"
#include "emul_op.h"
#include "xlowmem.h"
#include "macos_util.h"
#include "rom_patches.h"
#include "main.h"
#include "audio.h"
#include "audio_defs.h"
#include "thunks.h"

#define DEBUG 0
#include "debug.h"


// Sound input driver
static const uint8 sound_input_driver[] = {	// .AppleSoundInput driver header
	// Driver header
	0x4d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x24,			// Open() offset
	0x00, 0x28,			// Prime() offset
	0x00, 0x2c,			// Control() offset
	0x00, 0x38,			// Status() offset
	0x00, 0x5e,			// Close() offset
	0x10, 0x2e, 0x41, 0x70, 0x70, 0x6c, 0x65, 0x53, 0x6f, 0x75, 0x6e, 0x64, 0x49, 0x6e, 0x70, 0x75, 0x74, 0x00,	// ".AppleSoundInput"

	// Open()
	M68K_EMUL_OP_SOUNDIN_OPEN >> 8, M68K_EMUL_OP_SOUNDIN_OPEN & 0xff,
	0x4e, 0x75,							//	rts

	// Prime()
	M68K_EMUL_OP_SOUNDIN_PRIME >> 8, M68K_EMUL_OP_SOUNDIN_PRIME & 0xff,
	0x60, 0x0e,							//	bra		IOReturn

	// Control()
	M68K_EMUL_OP_SOUNDIN_CONTROL >> 8, M68K_EMUL_OP_SOUNDIN_CONTROL & 0xff,
	0x0c, 0x68, 0x00, 0x01, 0x00, 0x1a,	//	cmp.w	#1,$1a(a0)
	0x66, 0x04,							//	bne		IOReturn
	0x4e, 0x75,							//	rts

	// Status()
	M68K_EMUL_OP_SOUNDIN_STATUS >> 8, M68K_EMUL_OP_SOUNDIN_STATUS & 0xff,

	// IOReturn
	0x32, 0x28, 0x00, 0x06,				//  move.w	6(a0),d1
	0x08, 0x01, 0x00, 0x09,				//  btst		#9,d1
	0x67, 0x0c,							//  beq		1
	0x4a, 0x40,							//  tst.w	d0
	0x6f, 0x02,							//  ble		2
	0x42, 0x40,							//  clr.w	d0
	0x31, 0x40, 0x00, 0x10,				//2 move.w	d0,$10(a0)
	0x4e, 0x75,							//  rts
	0x4a, 0x40,							//1 tst.w	d0
	0x6f, 0x04,							//  ble		3
	0x42, 0x40,							//  clr.w	d0
	0x4e, 0x75,							//  rts
	0x2f, 0x38, 0x08, 0xfc,				//3 move.l	$8fc,-(sp)
	0x4e, 0x75,							//  rts

	// Close()
	M68K_EMUL_OP_SOUNDIN_CLOSE >> 8, M68K_EMUL_OP_SOUNDIN_CLOSE & 0xff,
	0x4e, 0x75,							//	rts
};


/*
 *  Search resource for byte string, return offset (or 0)
 */

static uint32 find_rsrc_data(const uint8 *rsrc, uint32 max, const uint8 *search, uint32 search_len, uint32 ofs = 0)
{
	while (ofs < max - search_len) {
		if (!memcmp(rsrc + ofs, search, search_len))
			return ofs;
		ofs++;
	}
	return 0;
}


/*
 *  Resource patches via vCheckLoad
 */

// 680x0 code pattern matching helper
#define PM(N, V) (p[N] == htons(V))

void CheckLoad(uint32 type, int16 id, uint16 *p, uint32 size)
{
	uint16 *p16;
	uint32 base;
	D(bug("vCheckLoad %c%c%c%c (%08x) ID %d, data %p, size %d\n", type >> 24, (type >> 16) & 0xff, (type >> 8) & 0xff, type & 0xff, type, id, p, size));

	// Don't modify resources in ROM
	if ((uintptr)p >= (uintptr)ROMBaseHost && (uintptr)p <= (uintptr)(ROMBaseHost + ROM_SIZE))
		return;

	if (type == FOURCC('b','o','o','t') && id == 3) {
		D(bug("boot 3 found\n"));
		size >>= 1;
		while (size--) {
			if (PM(0,0x51c9) && PM(2,0x2e49)) {
				// Set boot stack pointer (7.5.2, 7.5.3, 7.5.5, 7.6, 7.6.1, 8.0, 8.1, 8.5, 8.6, 9.0)
				p[2] = htons(M68K_EMUL_OP_FIX_BOOTSTACK);
				D(bug(" patch 1 applied\n"));
			} else if (PM(0,0x4267) && PM(1,0x3f01) && PM(2,0x3f2a) && PM(3,0x0006) && PM(4,0x6100)) {
				// Check when ntrb 17 is installed (for native Resource Manager patch) (7.5.3, 7.5.5)
				p[7] = htons(M68K_EMUL_OP_NTRB_17_PATCH3);
				D(bug(" patch 2 applied\n"));
			} else if (PM(0,0x3f2a) && PM(1,0x0006) && PM(2,0x3f2a) && PM(3,0x0002) && PM(4,0x6100)) {
				// Check when ntrb 17 is installed (for native Resource Manager patch) (7.6, 7.6.1, 8.0, 8.1)
				p[7] = htons(M68K_EMUL_OP_NTRB_17_PATCH);
				D(bug(" patch 3 applied\n"));
			} else if (PM(0,0x3f2a) && PM(1,0x0006) && PM(2,0x3f2a) && PM(3,0x0002) && PM(4,0x61ff) && PM(8,0x245f)) {
				// Check when ntrb 17 is installed (for native Resource Manager patch) (8.5, 8.6)
				p[8] = htons(M68K_EMUL_OP_NTRB_17_PATCH);
				D(bug(" patch 4 applied\n"));
			} else if (PM(0,0x3f2a) && PM(1,0x0006) && PM(2,0x3f2a) && PM(3,0x0002) && PM(4,0x61ff) && PM(7,0x301f)) {
				// Check when ntrb 17 is installed (for native Resource Manager patch) (9.0)
				p[7] = htons(M68K_EMUL_OP_NTRB_17_PATCH4);
				p[8] = htons(ntohs(p[8]) & 0xf0ff); // bra
				D(bug(" patch 5 applied\n"));
			} else if (PM(0,0x0c39) && PM(1,0x0001) && PM(2,0xf800) && PM(3,0x0008) && PM(4,0x6f00)) {
				// Don't read from 0xf8000008 (8.5 with Zanzibar ROM, 8.6, 9.0)
				p[0] = htons(M68K_NOP);
				p[1] = htons(M68K_NOP);
				p[2] = htons(M68K_NOP);
				p[3] = htons(M68K_NOP);
				p[4] = htons(0x6000);	// bra
				D(bug(" patch 6 applied\n"));
			} else if (PM(0,0x2f3c) && PM(1,0x6b72) && PM(2,0x6e6c) && PM(3,0x4267) && PM(4,0xa9a0) && PM(5,0x265f) && PM(6,0x200b) && PM(7,0x6700)) {
				// Don't replace nanokernel ("krnl" resource) (8.6, 9.0)
				p[0] = htons(M68K_NOP);
				p[1] = htons(M68K_NOP);
				p[2] = htons(M68K_NOP);
				p[3] = htons(M68K_NOP);
				p[4] = htons(M68K_NOP);
				p[7] = htons(0x6000);	// bra
				D(bug(" patch 7 applied\n"));
			} else if (PM(0,0xa8fe) && PM(1,0x3038) && PM(2,0x017a) && PM(3,0x0c40) && PM(4,0x8805) && PM(5,0x6710)) {
				// No SCSI (calls via 0x205c jump vector which is not initialized in NewWorld ROM 1.6) (8.6)
				if (ROMType == ROMTYPE_NEWWORLD) {
					p[5] = htons(0x6010);	// bra
					D(bug(" patch 8 applied\n"));
				}
			} else if (PM(0,0x2f3c) && PM(1,0x7665) && PM(2,0x7273) && PM(3,0x3f3c) && PM(4,0x0001) && PM(10,0x2041) && PM(11,0x2248) && PM(12,0x2050) && PM(20,0x7066) && PM(21,0xa9c9)) {
				// Check when vers 1 is installed (for safe abort if MacOS < 8.1 is used with a NewWorld ROM)
				p[10] = htons(M68K_EMUL_OP_CHECK_SYSV);
				p[11] = htons(0x4a81);	// tst.l	d1
				p[12] = htons(0x670e);	// beq.s	<SysError #dsOldSystem>
				D(bug(" patch 9 applied\n"));
			}
			p++;
		}

	} else if (type == FOURCC('g','n','l','d') && id == 0) {
		D(bug("gnld 0 found\n"));

		// Patch native Resource Manager after ntrbs are installed (7.5.2)
		static const uint8 dat[] = {0x4e, 0xba, 0x00, 0x9e, 0x3e, 0x00, 0x50, 0x4f, 0x67, 0x04};
		base = find_rsrc_data((uint8 *)p, size, dat, sizeof(dat));
		if (base) {
			p16 = (uint16 *)((uintptr)p + base + 6);
			*p16 = htons(M68K_EMUL_OP_NTRB_17_PATCH2);
			D(bug(" patch 1 applied\n"));
		}

	} else if (type == FOURCC('p','t','c','h') && id == 156) {
		D(bug("ptch 156 found\n"));
		size >>= 1;
		while (size--) {
			if (PM(0,0x4e56) && PM(1,0xfffa) && PM(2,0x48e7) && PM(3,0x1f18) && PM(4,0x7800) && PM(5,0x267c) && PM(6,0x6900) && PM(7,0x0000)) {
				// Don't call FE0A opcode (9.0)
				p[0] = htons(0x7000);		// moveq #0,d0
				p[1] = htons(M68K_RTS);
				D(bug(" patch 1 applied\n"));
				break;
			}
			p++;
		}

	} else if (type == FOURCC('p','t','c','h') && id == 420) {
		D(bug("ptch 420 found\n"));
		size >>= 1;
		while (size--) {
			if (PM(0,0xa030) && PM(1,0x5240) && PM(2,0x303c) && PM(3,0x0100) && PM(4,0xc06e) && PM(5,0xfef6)) {
				// Disable VM (7.5.2, 7.5.3, 7.5.5, 7.6, 7.6.1)
				p[1] = htons(M68K_NOP);
				p[2] = htons(M68K_NOP);
				p[3] = htons(M68K_NOP);
				p[4] = htons(M68K_NOP);
				p[5] = htons(M68K_NOP);
				p[6] = htons(M68K_NOP);
				p[7] = htons(M68K_NOP);
				p[8] = htons(M68K_NOP);
				p[9] = htons(M68K_NOP);
				p[10] = htons(M68K_NOP);
				p[11] = htons(M68K_NOP);
				D(bug(" patch 1 applied\n"));
				break;
			} else if (PM(0,0xa030) && PM(1,0x5240) && PM(2,0x7000) && PM(3,0x302e) && PM(4,0xfef6) && PM(5,0x323c) && PM(6,0x0100)) {
				// Disable VM (8.0, 8.1)
				p[8] = htons(M68K_NOP);
				p[15] = htons(M68K_NOP);
				D(bug(" patch 2 applied\n"));
				break;
			} else if (PM(0,0xa030) && PM(1,0x5240) && PM(2,0x7000) && PM(3,0x302e) && PM(4,0xfecc) && PM(5,0x323c) && PM(6,0x0100)) {
				// Disable VM (8.5, 8.6, 9.0)
				p[8] = htons(M68K_NOP);
				p[15] = htons(M68K_NOP);
				D(bug(" patch 3 applied\n"));
				break;
			}
			p++;
		}

	} else if (type == FOURCC('g','p','c','h') && id == 16) {
		D(bug("gpch 16 found\n"));
		size >>= 1;
		while (size--) {
			if (PM(0,0x6700) && PM(13,0x7013) && PM(14,0xfe0a)) {
				// Don't call FE0A in Shutdown Manager (7.6.1, 8.0, 8.1, 8.5)
				p[0] = htons(0x6000);
				D(bug(" patch 1 applied\n"));
				break;
			}
			p++;
		}

	} else if (type == FOURCC('g','p','c','h') && id == 650) {
		D(bug("gpch 650 found\n"));
		size >>= 1;
		while (size--) {
			if (PM(0,0x6600) && PM(1,0x001a) && PM(2,0x2278) && PM(3,0x0134)) {
				// We don't have SonyVars (7.5.2)
				p[0] = htons(0x6000);
				D(bug(" patch 1 applied\n"));
			} else if (PM(0,0x6618) && PM(1,0x2278) && PM(2,0x0134)) {
				// We don't have SonyVars (7.5.3)
				p[-6] = htons(M68K_NOP);
				p[-3] = htons(M68K_NOP);
				p[0] = htons(0x6018);
				D(bug(" patch 2 applied\n"));
			} else if (PM(0,0x6660) && PM(1,0x2278) && PM(2,0x0134)) {
				// We don't have SonyVars (7.5.3 Revision 2.2)
				p[-6] = htons(M68K_NOP);
				p[-3] = htons(M68K_NOP);
				p[0] = htons(0x6060);
				D(bug(" patch 3 applied\n"));
			} else if (PM(0,0x666e) && PM(1,0x2278) && PM(2,0x0134)) {
				// We don't have SonyVars (7.5.5)
				p[-6] = htons(M68K_NOP);
				p[-3] = htons(M68K_NOP);
				p[0] = htons(0x606e);
				D(bug(" patch 4 applied\n"));
			} else if (PM(0,0x6400) && PM(1,0x011c) && PM(2,0x2278) && PM(3,0x0134)) {
				// We don't have SonyVars (7.6.1, 8.0, 8.1, 8.5, 8.6, 9.0)
				p[0] = htons(0x6000);
				D(bug(" patch 5 applied\n"));
			} else if (PM(0,0x6400) && PM(1,0x00e6) && PM(2,0x2278) && PM(3,0x0134)) {
				// We don't have SonyVars (7.6)
				p[0] = htons(0x6000);
				D(bug(" patch 6 applied\n"));
			}
			p++;
		}

	} else if (type == FOURCC('g','p','c','h') && id == 655) {
		D(bug("gpch 655 found\n"));
		size >>= 1;
		while (size--) {
			if (PM(0,0x83a8) && PM(1,0x0024) && PM(2,0x4e71)) {
				// Don't write to GC interrupt mask (7.6, 7.6.1, 8.0, 8.1 with Zanzibar ROM)
				p[0] = htons(M68K_NOP);
				p[1] = htons(M68K_NOP);
				D(bug(" patch 1 applied\n"));
			} else if (PM(0,0x207c) && PM(1,0xf300) && PM(2,0x0034)) {
				// Don't read PowerMac ID (7.6, 7.6.1, 8.0, 8.1 with Zanzibar ROM)
				p[0] = htons(0x303c);		// move.w #id,d0
				p[1] = htons(0x3020);
				p[2] = htons(M68K_RTS);
				D(bug(" patch 2 applied\n"));
			} else if (PM(0,0x13fc) && PM(1,0x0081) && PM(2,0xf130) && PM(3,0xa030)) {
				// Don't write to hardware (7.6, 7.6.1, 8.0, 8.1 with Zanzibar ROM)
				p[0] = htons(M68K_NOP);
				p[1] = htons(M68K_NOP);
				p[2] = htons(M68K_NOP);
				p[3] = htons(M68K_NOP);
				D(bug(" patch 3 applied\n"));
			} else if (PM(0,0x4e56) && PM(1,0x0000) && PM(2,0x227c) && PM(3,0xf800) && PM(4,0x0000)) {
				// OpenFirmare? (7.6.1, 8.0, 8.1 with Zanzibar ROM)
				p[0] = htons(M68K_RTS);
				D(bug(" patch 4 applied\n"));
			} else if (PM(0,0x4e56) && PM(1,0xfffc) && PM(2,0x48e7) && PM(3,0x0300) && PM(4,0x598f) && PM(5,0x2eb8) && PM(6,0x01dc)) {
				// Don't write to SCC (7.6.1, 8.0, 8.1 with Zanzibar ROM)
				p[0] = htons(M68K_RTS);
				D(bug(" patch 5 applied\n"));
			} else if (PM(0,0x4e56) && PM(1,0x0000) && PM(2,0x227c) && PM(3,0xf300) && PM(4,0x0034)) {
				// Don't write to GC (7.6.1, 8.0, 8.1 with Zanzibar ROM)
				p[0] = htons(M68K_RTS);
				D(bug(" patch 6 applied\n"));
			} else if (PM(0,0x40e7) && PM(1,0x007c) && PM(2,0x0700) && PM(3,0x48e7) && PM(4,0x00c0) && PM(5,0x2078) && PM(6,0x0dd8) && PM(7,0xd1e8) && PM(8,0x0044) && PM(9,0x8005) && PM(11,0x93c8) && PM(12,0x2149) && PM(13,0x0024)) {
				// Don't replace NVRAM routines (7.6, 7.6.1, 8.0, 8.1 with Zanzibar ROM)
				p[0] = htons(M68K_RTS);
				D(bug(" patch 7 applied\n"));
			} else if (PM(0,0x207c) && PM(1,0x50f1) && PM(2,0xa101) && (PM(3,0x08d0) || PM(3,0x0890))) {
				// Don't write to 0x50f1a101 (8.1 with Zanzibar ROM)
				p[3] = htons(M68K_NOP);
				p[4] = htons(M68K_NOP);
				D(bug(" patch 8 applied\n"));
			}
			p++;
		}

	} else if (type == FOURCC('g','p','c','h') && id == 750) {
		D(bug("gpch 750 found\n"));
		size >>= 1;
		while (size--) {
			if (PM(0,0xf301) && PM(1,0x9100) && PM(2,0x0c11) && PM(3,0x0044)) {
				// Don't read from 0xf3019100 (MACE ENET) (7.6, 7.6.1, 8.0, 8.1)
				p[2] = htons(M68K_NOP);
				p[3] = htons(M68K_NOP);
				p[4] = htons(0x6026);
				D(bug(" patch 1 applied\n"));
			} else if (PM(0,0x41e8) && PM(1,0x0374) && PM(2,0xfc1e)) {
				// Don't call FC1E opcode (7.6, 7.6.1, 8.0, 8.1, 8.5, 8.6)
				p[2] = htons(M68K_NOP);
				D(bug(" patch 2 applied\n"));
			} else if (PM(0,0x700a) && PM(1,0xfe0a)) {
				// Don't call FE0A opcode (7.6, 7.6.1, 8.0, 8.1, 8.5, 8.6, 9.0)
				p[1] = htons(0x2008);	// move.l a0,d0
				D(bug(" patch 3 applied\n"));
			} else if (PM(0,0x6c00) && PM(1,0x016a) && PM(2,0x2278) && PM(3,0x0134)) {
				// We don't have SonyVars (8.6)
				p[-4] = htons(0x21fc);  // move.l $40810000,($0000)
				p[-3] = htons(0x4081);
				p[-2] = htons(0x0000);
				p[-1] = htons(0x0000);
				p[0] = htons(0x6000);
				D(bug(" patch 4 applied\n"));
			}
			p++;
		}

	} else if (type == FOURCC('g','p','c','h') && id == 999) {
		D(bug("gpch 999 found\n"));
		size >>= 1;
		while (size--) {
			if (PM(0,0xf301) && PM(1,0x9100) && PM(2,0x0c11) && PM(3,0x0044)) {
				// Don't read from 0xf3019100 (MACE ENET) (8.5, 8.6)
				p[2] = htons(M68K_NOP);
				p[3] = htons(M68K_NOP);
				p[4] = htons(0x6026);
				D(bug(" patch 1 applied\n"));
			}
			p++;
		}

	} else if (type == FOURCC('g','p','c','h') && id == 3000) {
		D(bug("gpch 3000 found\n"));
		size >>= 1;
		while (size--) {
			if (PM(0,0xf301) && PM(1,0x9100) && PM(2,0x0c11) && PM(3,0x0044)) {
				// Don't read from 0xf3019100 (MACE ENET) (8.1 with NewWorld ROM)
				p[2] = htons(M68K_NOP);
				p[3] = htons(M68K_NOP);
				p[4] = htons(0x6026);
				D(bug(" patch 1 applied\n"));
			}
			p++;
		}

	} else if (type == FOURCC('l','t','l','k') && id == 0) {
		D(bug("ltlk 0 found\n"));
#if 1
		size >>= 1;
		while (size--) {
			if (PM(0,0xc2fc) && PM(1,0x0fa0) && PM(2,0x82c5)) {
				// Prevent division by 0 in speed test (7.5.2, 7.5.3, 7.5.5, 7.6, 7.6.1, 8.0, 8.1)
				p[2] = htons(0x7200);
				WriteMacInt32(0x1d8, 0x2c00);
				WriteMacInt32(0x1dc, 0x2c00);
				D(bug(" patch 1 applied\n"));
			} else if (PM(0,0x1418) && PM(1,0x84c1)) {
				// Prevent division by 0 (7.5.2, 7.5.3, 7.5.5, 7.6, 7.6.1, 8.0, 8.1)
				p[1] = htons(0x7400);
				D(bug(" patch 2 applied\n"));
			} else if (PM(0,0x2678) && PM(1,0x01dc) && PM(2,0x3018) && PM(3,0x6708) && PM(4,0x1680) && PM(5,0xe058) && PM(6,0x1680)) {
				// Don't write to SCC (7.5.2, 7.5.3, 7.5.5, 7.6, 7.6.1, 8.0, 8.1)
				p[4] = htons(M68K_NOP);
				p[6] = htons(M68K_NOP);
				D(bug(" patch 3 applied\n"));
			} else if (PM(0,0x2278) && PM(1,0x01dc) && PM(2,0x12bc) && PM(3,0x0006) && PM(4,0x4e71) && PM(5,0x1292)) {
				// Don't write to SCC (7.5.2, 7.5.3, 7.5.5, 7.6, 7.6.1, 8.0, 8.1)
				p[2] = htons(M68K_NOP);
				p[3] = htons(M68K_NOP);
				p[5] = htons(M68K_NOP);
				D(bug(" patch 4 applied\n"));
			} else if (PM(0,0x2278) && PM(1,0x01dc) && PM(2,0x12bc) && PM(3,0x0003) && PM(4,0x4e71) && PM(5,0x1281)) {
				// Don't write to SCC (7.5.2, 7.5.3, 7.5.5, 7.6, 7.6.1, 8.0, 8.1)
				p[2] = htons(M68K_NOP);
				p[3] = htons(M68K_NOP);
				p[5] = htons(M68K_NOP);
				D(bug(" patch 5 applied\n"));
			} else if (PM(0,0x0811) && PM(1,0x0000) && PM(2,0x51c8) && PM(3,0xfffa)) {
				// Don't test SCC (7.5.2, 7.5.3, 7.5.5, 7.6, 7.6.1, 8.0, 8.1)
				p[0] = htons(M68K_NOP);
				p[1] = htons(M68K_NOP);
				D(bug(" patch 6 applied\n"));
			} else if (PM(0,0x4a2a) && PM(1,0x063e) && PM(2,0x66fa)) {
				// Don't wait for SCC (7.5.2, 7.5.3, 7.5.5)
				p[2] = htons(M68K_NOP);
				D(bug(" patch 7 applied\n"));
			} else if (PM(0,0x4a2a) && PM(1,0x03a6) && PM(2,0x66fa)) {
				// Don't wait for SCC (7.6, 7.6.1, 8.0, 8.1)
				p[2] = htons(M68K_NOP);
				D(bug(" patch 8 applied\n"));
			}
			p++;
		}
#else
		// Disable LocalTalk
		p[0] = htons(M68K_JMP_A0);
		p[1] = htons(0x7000);		// moveq #0,d0
		p[2] = htons(M68K_RTS);
		D(bug(" patch 1 applied\n"));
#endif

	} else if (type == FOURCC('n','s','r','d') && id == 1) {
		D(bug("nsrd 1 found\n"));
		if (p[(0x378 + 0x460) >> 1] == htons(0x7c08) && p[(0x37a + 0x460) >> 1] == htons(0x02a6)) {
			// Don't overwrite our serial drivers (7.5.3 Revision 2.2)
			p[(0x378 + 0x460) >> 1] = htons(0x4e80);		// blr
			p[(0x37a + 0x460) >> 1] = htons(0x0020);
			D(bug(" patch 1 applied\n"));
		} else if (p[(0x378 + 0x570) >> 1] == htons(0x7c08) && p[(0x37a + 0x570) >> 1] == htons(0x02a6)) {
			// Don't overwrite our serial drivers (8.0, 8.1)
			p[(0x378 + 0x570) >> 1] = htons(0x4e80);		// blr
			p[(0x37a + 0x570) >> 1] = htons(0x0020);
			D(bug(" patch 2 applied\n"));
		} else if (p[(0x378 + 0x6c0) >> 1] == htons(0x7c08) && p[(0x37a + 0x6c0) >> 1] == htons(0x02a6)) {
			// Don't overwrite our serial drivers (8.5, 8.6)
			p[(0x378 + 0x6c0) >> 1] = htons(0x4e80);		// blr
			p[(0x37a + 0x6c0) >> 1] = htons(0x0020);
			D(bug(" patch 3 applied\n"));
		} else if (p[(0x374 + 0x510) >> 1] == htons(0x7c08) && p[(0x376 + 0x510) >> 1] == htons(0x02a6)) {
			// Don't overwrite our serial drivers (9.0)
			p[(0x374 + 0x510) >> 1] = htons(0x4e80);		// blr
			p[(0x376 + 0x510) >> 1] = htons(0x0020);
			D(bug(" patch 4 applied\n"));
		}

	} else if (type == FOURCC('c','i','t','t') && id == 45) {
		D(bug("citt 45 found\n"));
		size >>= 1;
		while (size--) {
			if (PM(0,0x203c) && PM(1,0x0100) && PM(2,0x0000) && PM(3,0xc0ae) && PM(4,0xfffc)) {
				// Don't replace SCSI Manager (8.1, 8.5, 8.6, 9.0)
				p[5] = htons((ntohs(p[5]) & 0xff) | 0x6000);		// beq
				D(bug(" patch 1 applied\n"));
				break;
			}
			p++;
		}

	} else if (type == FOURCC('t','h','n','g')) {
		// Collect info about used audio sifters
		uint32 thing = Host2MacAddr((uint8 *)p);
		uint32 c_type = ReadMacInt32(thing);
		uint32 sub_type = ReadMacInt32(thing + 4);
		if (c_type == FOURCC('s','d','e','v') && sub_type == FOURCC('s','i','n','g')) {
			WriteMacInt32(thing + 4, FOURCC('a','w','g','c'));
			D(bug("thng %d, type %c%c%c%c (%08x), sub type %c%c%c%c (%08x), data %p\n", id, c_type >> 24, (c_type >> 16) & 0xff, (c_type >> 8) & 0xff, c_type & 0xff, c_type, sub_type >> 24, (sub_type >> 16) & 0xff, (sub_type >> 8) & 0xff, sub_type & 0xff, sub_type, p));
			AddSifter(ReadMacInt32(thing + componentResType), ReadMacInt16(thing + componentResID));
			if (ReadMacInt32(thing + componentPFCount))
				AddSifter(ReadMacInt32(thing + componentPFResType), ReadMacInt16(thing + componentPFResID));
		}

	} else if (type == FOURCC('s','i','f','t') || type == FOURCC('n','i','f','t')) {
		// Patch audio sifters
		if (FindSifter(type, id)) {
			D(bug("sifter found\n"));
			p[0] = htons(0x4e56); p[1] = htons(0x0000);		// link a6,#0
			p[2] = htons(0x48e7); p[3] = htons(0x8018);		// movem.l d0/a3-a4,-(a7)
			p[4] = htons(0x266e); p[5] = htons(0x000c);		// movea.l $c(a6),a3
			p[6] = htons(0x286e); p[7] = htons(0x0008);		// movea.l $8(a6),a4
			p[8] = htons(M68K_EMUL_OP_AUDIO_DISPATCH);
			p[9] = htons(0x2d40); p[10] = htons(0x0010);	// move.l d0,$10(a6)
			p[11] = htons(0x4cdf); p[12] = htons(0x1801);	// movem.l (a7)+,d0/a3-a4
			p[13] = htons(0x4e5e);							// unlk a6
			p[14] = htons(0x4e74); p[15] = htons(0x0008);	// rtd #8
			D(bug(" patch applied\n"));
		}

	} else if (type == FOURCC('D','R','V','R') && (id == -16501 || id == -16500)) {
		D(bug("DRVR -16501/-16500 found\n"));
		// Install sound input driver
		memcpy(p, sound_input_driver, sizeof(sound_input_driver));
		D(bug(" patch 1 applied\n"));

	} else if (type == FOURCC('I','N','I','T') && id == 1 && size == (2416 >> 1)) {
		D(bug("INIT 1 (size 2416) found\n"));
		size >>= 1;
		while (size--) {
			if (PM(0,0x247c) && PM(1,0xf301) && PM(2,0x9000)) {
				// Prevent "MacOS Licensing Extension" from accessing hardware (7.6)
				p[22] = htons(0x6028);
				D(bug(" patch 1 applied\n"));
				break;
			}
			p++;
		}

	} else if (type == FOURCC('s','c','o','d') && id == -16465) {
		D(bug("scod -16465 found\n"));

		// Don't crash in Process Manager on reset/shutdown (8.6, 9.0)
		static const uint8 dat[] = {0x4e, 0x56, 0x00, 0x00, 0x48, 0xe7, 0x03, 0x18, 0x2c, 0x2e, 0x00, 0x10};
		base = find_rsrc_data((uint8 *)p, size, dat, sizeof(dat));
		if (base) {
			p16 = (uint16 *)((uintptr)p + base);
			p16[0] = htons(0x7000);	// moveq #0,d0
			p16[1] = htons(M68K_RTS);
			D(bug(" patch 1 applied\n"));
		}

	} else if (type == FOURCC('N','O','b','j') && id == 100) {
		D(bug("NObj 100 found\n"));

		// Don't access VIA registers in MacBench 5.0
		static const uint8 dat1[] = {0x7c, 0x08, 0x02, 0xa6, 0xbf, 0x01, 0xff, 0xe0, 0x90, 0x01, 0x00, 0x08};
		base = find_rsrc_data((uint8 *)p, size, dat1, sizeof(dat1));
		if (base) {
			p[(base + 0x00) >> 1] = htons(0x3860);		// li r3,0
			p[(base + 0x02) >> 1] = htons(0x0000);
			p[(base + 0x04) >> 1] = htons(0x4e80);		// blr
			p[(base + 0x06) >> 1] = htons(0x0020);
			D(bug(" patch 1 applied\n"));
		}
		static const uint8 dat2[] = {0x7c, 0x6c, 0x1b, 0x78, 0x7c, 0x8b, 0x23, 0x78, 0x38, 0xc0, 0x3f, 0xfd};
		base = find_rsrc_data((uint8 *)p, size, dat2, sizeof(dat2));
		if (base) {
			p[(base + 0x00) >> 1] = htons(0x3860);		// li r3,0
			p[(base + 0x02) >> 1] = htons(0x0000);
			p[(base + 0x04) >> 1] = htons(0x4e80);		// blr
			p[(base + 0x06) >> 1] = htons(0x0020);
			D(bug(" patch 2 applied\n"));
		}

	} else if (type == FOURCC('C','O','D','E') && id == 27 && size == 25024) {
		D(bug("CODE 27 found [Apple Personal Diagnostics]\n"));

		// Don't access FCBs directly in Apple Personal Diagnostics (MacOS 9)
		// FIXME: this should not be called in the first place, use UTResolveFCB?
		static const uint8 dat[] = {0x2d, 0x78, 0x03, 0x4e, 0xff, 0xf8, 0x20, 0x6e, 0xff, 0xf8};
		base = find_rsrc_data((uint8 *)p, size, dat, sizeof(dat));
		if (base
			&& ReadMacInt16(0x3f6) == 4 /* FSFCBLen */
			&& p[(base + 0x1a) >> 1] == htons(0x605e)
			&& p[(base + 0x80) >> 1] == htons(0x7000))
		{
			p[(base + 0x1a) >> 1] = htons(0x6064);
			D(bug(" patch1 applied\n"));
		}

	} else if (type == FOURCC('i','n','f','n') && (id == 129 || id == 200)) {
		D(bug("infn %d found\n", id));
		size >>= 1;
		while (size--) {
			if (PM(0,0x203c) && PM(1,0xf800) && PM(2,0x0000) && PM(4,0x2040) && PM(5,0x1028) && PM(6,0x0090)) {
				// Don't read from 0xf8000090 during MacOS (8.5, 9.0) installation
				p[0] = htons(M68K_NOP);
				p[1] = htons(M68K_NOP);
				p[2] = htons(M68K_NOP);
				p[3] = htons(M68K_NOP);
				p[4] = htons(M68K_NOP);
				p[5] = htons(M68K_NOP);
				p[6] = htons(0x7000);	// moveq #0,d0
				D(bug(" patch 1 applied\n"));
				break;
			}
			p++;
		}

	}
}


/*
 *  Resource patches via GetNamedResource() and Get1NamedResource()
 */

void CheckLoad(uint32 type, const char *name, uint8 *p, uint32 size)
{
	uint16 *p16;
	uint32 base;
	D(bug("vCheckLoad %c%c%c%c (%08x) name \"%*s\", data %p, size %d\n", type >> 24, (type >> 16) & 0xff, (type >> 8) & 0xff, type & 0xff, type, name[0], &name[1], p, size));

	// Don't modify resources in ROM
	if ((uintptr)p >= (uintptr)ROMBaseHost && (uintptr)p <= (uintptr)(ROMBaseHost + ROM_SIZE))
		return;

	if (type == FOURCC('D','R','V','R') && strncmp(&name[1], ".AFPTranslator", name[0]) == 0) {
		D(bug(" DRVR .AFPTranslator found\n"));
		
		// Don't access ROM85 as it it was a pointer to a ROM version number (8.0, 8.1)
		static const uint8 dat[] = {0x3a, 0x2e, 0x00, 0x0a, 0x55, 0x4f, 0x3e, 0xb8, 0x02, 0x8e, 0x30, 0x1f, 0x48, 0xc0, 0x24, 0x40, 0x20, 0x40};
		base = find_rsrc_data(p, size, dat, sizeof(dat));
		if (base) {
			p16 = (uint16 *)(p + base + 4);
			*p16++ = htons(0x303c);		// move.l	#ROM85,%d0
			*p16++ = htons(0x028e);
			*p16++ = htons(M68K_NOP);
			*p16++ = htons(M68K_NOP);
			D(bug("  patch 1 applied\n"));
		}
	}
}


/*
 *  Native Resource Manager patches
 */

#ifdef __BEOS__
static
#else
extern "C"
#endif
void check_load_invoc(uint32 type, int16 id, uint32 h)
{
	if (h == 0)
		return;
	uint32 p = ReadMacInt32(h);
	if (p == 0)
		return;
	uint32 size = ReadMacInt32(p - 2 * 4) & 0xffffff;

	CheckLoad(type, id, (uint16 *)Mac2HostAddr(p), size);
}

#ifdef __BEOS__
static
#else
extern "C"
#endif
void named_check_load_invoc(uint32 type, uint32 name, uint32 h)
{
	if (h == 0)
		return;
	uint32 p = ReadMacInt32(h);
	if (p == 0)
		return;
	uint32 size = ReadMacInt32(p - 2 * 4) & 0xffffff;

	CheckLoad(type, (char *)Mac2HostAddr(name), Mac2HostAddr(p), size);
}

#ifdef __BEOS__
static asm void **get_resource(register uint32 type, register int16 id)
{
	// Create stack frame
	mflr	r0
	stw		r0,8(r1)
	stwu	r1,-(56+12)(r1)

	// Save type/ID
	stw		r3,56(r1)
	stw		r4,56+4(r1)

	// Call old routine
	lwz		r0,XLM_GET_RESOURCE
	lwz		r2,XLM_RES_LIB_TOC
	mtctr	r0
	bctrl
	lwz		r2,XLM_TOC		// Get TOC
	stw		r3,56+8(r1)		// Save handle

	// Call CheckLoad
	lwz		r3,56(r1)
	lwz		r4,56+4(r1)
	lwz		r5,56+8(r1)
	bl		check_load_invoc
	lwz		r3,56+8(r1)		// Restore handle

	// Return to caller
	lwz		r0,56+12+8(r1)
	mtlr	r0
	addi	r1,r1,56+12
	blr
}

static asm void **get_1_resource(register uint32 type, register int16 id)
{
	// Create stack frame
	mflr	r0
	stw		r0,8(r1)
	stwu	r1,-(56+12)(r1)

	// Save type/ID
	stw		r3,56(r1)
	stw		r4,56+4(r1)

	// Call old routine
	lwz		r0,XLM_GET_1_RESOURCE
	lwz		r2,XLM_RES_LIB_TOC
	mtctr	r0
	bctrl
	lwz		r2,XLM_TOC		// Get TOC
	stw		r3,56+8(r1)		// Save handle

	// Call CheckLoad
	lwz		r3,56(r1)
	lwz		r4,56+4(r1)
	lwz		r5,56+8(r1)
	bl		check_load_invoc
	lwz		r3,56+8(r1)		// Restore handle

	// Return to caller
	lwz		r0,56+12+8(r1)
	mtlr	r0
	addi	r1,r1,56+12
	blr
}

static asm void **get_ind_resource(register uint32 type, register int16 index)
{
	// Create stack frame
	mflr	r0
	stw		r0,8(r1)
	stwu	r1,-(56+12)(r1)

	// Save type/index
	stw		r3,56(r1)
	stw		r4,56+4(r1)

	// Call old routine
	lwz		r0,XLM_GET_IND_RESOURCE
	lwz		r2,XLM_RES_LIB_TOC
	mtctr	r0
	bctrl
	lwz		r2,XLM_TOC		// Get TOC
	stw		r3,56+8(r1)		// Save handle

	// Call CheckLoad
	lwz		r3,56(r1)
	lwz		r4,56+4(r1)
	lwz		r5,56+8(r1)
	bl		check_load_invoc
	lwz		r3,56+8(r1)		// Restore handle

	// Return to caller
	lwz		r0,56+12+8(r1)
	mtlr	r0
	addi	r1,r1,56+12
	blr
}

static asm void **get_1_ind_resource(register uint32 type, register int16 index)
{
	// Create stack frame
	mflr	r0
	stw		r0,8(r1)
	stwu	r1,-(56+12)(r1)

	// Save type/index
	stw		r3,56(r1)
	stw		r4,56+4(r1)

	// Call old routine
	lwz		r0,XLM_GET_1_IND_RESOURCE
	lwz		r2,XLM_RES_LIB_TOC
	mtctr	r0
	bctrl
	lwz		r2,XLM_TOC		// Get TOC
	stw		r3,56+8(r1)		// Save handle

	// Call CheckLoad
	lwz		r3,56(r1)
	lwz		r4,56+4(r1)
	lwz		r5,56+8(r1)
	bl		check_load_invoc
	lwz		r3,56+8(r1)		// Restore handle

	// Return to caller
	lwz		r0,56+12+8(r1)
	mtlr	r0
	addi	r1,r1,56+12
	blr
}

static asm void **r_get_resource(register uint32 type, register int16 id)
{
	// Create stack frame
	mflr	r0
	stw		r0,8(r1)
	stwu	r1,-(56+12)(r1)

	// Save type/ID
	stw		r3,56(r1)
	stw		r4,56+4(r1)

	// Call old routine
	lwz		r0,XLM_R_GET_RESOURCE
	lwz		r2,XLM_RES_LIB_TOC
	mtctr	r0
	bctrl
	lwz		r2,XLM_TOC		// Get TOC
	stw		r3,56+8(r1)		// Save handle

	// Call CheckLoad
	lwz		r3,56(r1)
	lwz		r4,56+4(r1)
	lwz		r5,56+8(r1)
	bl		check_load_invoc
	lwz		r3,56+8(r1)		// Restore handle

	// Return to caller
	lwz		r0,56+12+8(r1)
	mtlr	r0
	addi	r1,r1,56+12
	blr
}

static asm void **get_named_resource(register uint32 type, register uint32 name)
{
	// Create stack frame
	mflr	r0
	stw		r0,8(r1)
	stwu	r1,-(56+12)(r1)

	// Save type/ID
	stw		r3,56(r1)
	stw		r4,56+4(r1)

	// Call old routine
	lwz		r0,XLM_GET_NAMED_RESOURCE
	lwz		r2,XLM_RES_LIB_TOC
	mtctr	r0
	bctrl
	lwz		r2,XLM_TOC		// Get TOC
	stw		r3,56+8(r1)		// Save handle

	// Call CheckLoad
	lwz		r3,56(r1)
	lwz		r4,56+4(r1)
	lwz		r5,56+8(r1)
	bl		named_check_load_invoc
	lwz		r3,56+8(r1)		// Restore handle

	// Return to caller
	lwz		r0,56+12+8(r1)
	mtlr	r0
	addi	r1,r1,56+12
	blr
}

static asm void **get_1_named_resource(register uint32 type, register uint32 name)
{
	// Create stack frame
	mflr	r0
	stw		r0,8(r1)
	stwu	r1,-(56+12)(r1)

	// Save type/ID
	stw		r3,56(r1)
	stw		r4,56+4(r1)

	// Call old routine
	lwz		r0,XLM_GET_1_NAMED_RESOURCE
	lwz		r2,XLM_RES_LIB_TOC
	mtctr	r0
	bctrl
	lwz		r2,XLM_TOC		// Get TOC
	stw		r3,56+8(r1)		// Save handle

	// Call CheckLoad
	lwz		r3,56(r1)
	lwz		r4,56+4(r1)
	lwz		r5,56+8(r1)
	bl		named_check_load_invoc
	lwz		r3,56+8(r1)		// Restore handle

	// Return to caller
	lwz		r0,56+12+8(r1)
	mtlr	r0
	addi	r1,r1,56+12
	blr
}
#else
// Routines in asm_linux.S
extern "C" void get_resource(void);
extern "C" void get_1_resource(void);
extern "C" void get_ind_resource(void);
extern "C" void get_1_ind_resource(void);
extern "C" void r_get_resource(void);
extern "C" void get_named_resource(void);
extern "C" void get_1_named_resource(void);
#endif

void PatchNativeResourceManager(void)
{
	D(bug("PatchNativeResourceManager\n"));

	// Patch native GetResource()
	uint32 upp = ReadMacInt32(0x1480);
	if ((upp & 0xffc00000) == ROM_BASE)
		return;
	uint32 tvec = ReadMacInt32(upp + 5 * 4);
	D(bug(" GetResource() entry %08x, TOC %08x\n", ReadMacInt32(tvec), ReadMacInt32(tvec + 4)));
	WriteMacInt32(XLM_RES_LIB_TOC, ReadMacInt32(tvec + 4));
	WriteMacInt32(XLM_GET_RESOURCE, ReadMacInt32(tvec));
#if EMULATED_PPC
	WriteMacInt32(tvec, NativeFunction(NATIVE_GET_RESOURCE));
#else
#ifdef __BEOS__
	uint32 *tvec2 = (uint32 *)get_resource;
	WriteMacInt32(tvec, tvec2[0]);
	WriteMacInt32(tvec + 4, tvec2[1]);
#else
	WriteMacInt32(tvec, (uint32)get_resource);
#endif
#endif

	// Patch native Get1Resource()
	upp = ReadMacInt32(0x0e7c);
	tvec = ReadMacInt32(upp + 5 * 4);
	D(bug(" Get1Resource() entry %08x, TOC %08x\n", ReadMacInt32(tvec), ReadMacInt32(tvec + 4)));
	WriteMacInt32(XLM_GET_1_RESOURCE, ReadMacInt32(tvec));
#if EMULATED_PPC
	WriteMacInt32(tvec, NativeFunction(NATIVE_GET_1_RESOURCE));
#else
#ifdef __BEOS__
	tvec2 = (uint32 *)get_1_resource;
	WriteMacInt32(tvec, tvec2[0]);
	WriteMacInt32(tvec + 4, tvec2[1]);
#else
	WriteMacInt32(tvec, (uint32)get_1_resource);
#endif
#endif

	// Patch native GetIndResource()
	upp = ReadMacInt32(0x1474);
	tvec = ReadMacInt32(upp + 5 * 4);
	D(bug(" GetIndResource() entry %08x, TOC %08x\n", ReadMacInt32(tvec), ReadMacInt32(tvec + 4)));
	WriteMacInt32(XLM_GET_IND_RESOURCE, ReadMacInt32(tvec));
#if EMULATED_PPC
	WriteMacInt32(tvec, NativeFunction(NATIVE_GET_IND_RESOURCE));
#else
#ifdef __BEOS__
	tvec2 = (uint32 *)get_ind_resource;
	WriteMacInt32(tvec, tvec2[0]);
	WriteMacInt32(tvec + 4, tvec2[1]);
#else
	WriteMacInt32(tvec, (uint32)get_ind_resource);
#endif
#endif

	// Patch native Get1IndResource()
	upp = ReadMacInt32(0x0e38);
	tvec = ReadMacInt32(upp + 5 * 4);
	D(bug(" Get1IndResource() entry %08x, TOC %08x\n", ReadMacInt32(tvec), ReadMacInt32(tvec + 4)));
	WriteMacInt32(XLM_GET_1_IND_RESOURCE, ReadMacInt32(tvec));
#if EMULATED_PPC
	WriteMacInt32(tvec, NativeFunction(NATIVE_GET_1_IND_RESOURCE));
#else
#ifdef __BEOS__
	tvec2 = (uint32 *)get_1_ind_resource;
	WriteMacInt32(tvec, tvec2[0]);
	WriteMacInt32(tvec + 4, tvec2[1]);
#else
	WriteMacInt32(tvec, (uint32)get_1_ind_resource);
#endif
#endif

	// Patch native RGetResource()
	upp = ReadMacInt32(0x0e30);
	tvec = ReadMacInt32(upp + 5 * 4);
	D(bug(" RGetResource() entry %08x, TOC %08x\n", ReadMacInt32(tvec), ReadMacInt32(tvec + 4)));
	WriteMacInt32(XLM_R_GET_RESOURCE, ReadMacInt32(tvec));
#if EMULATED_PPC
	WriteMacInt32(tvec, NativeFunction(NATIVE_R_GET_RESOURCE));
#else
#ifdef __BEOS__
	tvec2 = (uint32 *)r_get_resource;
	WriteMacInt32(tvec, tvec2[0]);
	WriteMacInt32(tvec + 4, tvec2[1]);
#else
	WriteMacInt32(tvec, (uint32)r_get_resource);
#endif
#endif

	// Patch native GetNamedResource()
	upp = ReadMacInt32(0x1484);
	tvec = ReadMacInt32(upp + 5 * 4);
	D(bug(" GetNamedResource() entry %08x, TOC %08x\n", ReadMacInt32(tvec), ReadMacInt32(tvec + 4)));
	WriteMacInt32(XLM_GET_NAMED_RESOURCE, ReadMacInt32(tvec));
#if EMULATED_PPC
	WriteMacInt32(tvec, NativeFunction(NATIVE_GET_NAMED_RESOURCE));
#else
#ifdef __BEOS__
	tvec2 = (uint32 *)get_named_resource;
	WriteMacInt32(tvec, tvec2[0]);
	WriteMacInt32(tvec + 4, tvec2[1]);
#else
	WriteMacInt32(tvec, (uint32)get_named_resource);
#endif
#endif

	// Patch native Get1NamedResource()
	upp = ReadMacInt32(0x0e80);
	tvec = ReadMacInt32(upp + 5 * 4);
	D(bug(" Get1NamedResource() entry %08x, TOC %08x\n", ReadMacInt32(tvec), ReadMacInt32(tvec + 4)));
	WriteMacInt32(XLM_GET_1_NAMED_RESOURCE, ReadMacInt32(tvec));
#if EMULATED_PPC
	WriteMacInt32(tvec, NativeFunction(NATIVE_GET_1_NAMED_RESOURCE));
#else
#ifdef __BEOS__
	tvec2 = (uint32 *)get_1_named_resource;
	WriteMacInt32(tvec, tvec2[0]);
	WriteMacInt32(tvec + 4, tvec2[1]);
#else
	WriteMacInt32(tvec, (uint32)get_1_named_resource);
#endif
#endif
}
