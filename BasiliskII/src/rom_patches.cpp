/*
 *  rom_patches.cpp - ROM patches
 *
 *  Basilisk II (C) Christian Bauer
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

#include <string.h>

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "main.h"
#include "emul_op.h"
#include "macos_util.h"
#include "slot_rom.h"
#include "sony.h"
#include "disk.h"
#include "cdrom.h"
#include "video.h"
#include "extfs.h"
#include "prefs.h"

#if ENABLE_MON
#include "mon.h"
#endif

#include "rom_patches.h"

#define DEBUG 0
#include "debug.h"


// Global variables
uint32 UniversalInfo;		// ROM offset of UniversalInfo
uint32 PutScrapPatch = 0;	// Mac address of PutScrap() patch
uint32 GetScrapPatch = 0;	// Mac address of GetScrap() patch
uint32 ROMBreakpoint = 0;	// ROM offset of breakpoint (0 = disabled, 0x2310 = CritError)
bool PrintROMInfo = false;	// Flag: print ROM information in PatchROM()
bool PatchHWBases = true;	// Flag: patch hardware base addresses

static uint32 sony_offset;		// ROM offset of .Sony driver
static uint32 serd_offset;		// ROM offset of SERD resource (serial drivers)
static uint32 microseconds_offset;	// ROM offset of Microseconds() replacement routine
static uint32 debugutil_offset;		// ROM offset of DebugUtil() replacement routine

// Prototypes
uint16 ROMVersion;

/*
 *  Macros used to extract one of the 16-bit words from a 32-bit word value
 */

#define HiWord(X) (((X) >> 16) & 0xffff)
#define LoWord(X) ((X) & 0xffff)


/*
 *  Search ROM for byte string, return ROM offset (or 0)
 */

static uint32 find_rom_data(uint32 start, uint32 end, const uint8 *data, uint32 data_len)
{
	uint32 ofs = start;
	while (ofs < end) {
		if (!memcmp((void *)(ROMBaseHost + ofs), data, data_len))
			return ofs;
		ofs++;
	}
	return 0;
}


/*
 *  Search ROM resource by type/ID, return ROM offset of resource data
 */

static uint32 rsrc_ptr = 0;

static uint32 find_rom_resource(uint32 s_type, int16 s_id, bool cont = false)
{
	uint32 lp = ROMBaseMac + ReadMacInt32(ROMBaseMac + 0x1a);
	uint32 x = ReadMacInt32(lp);

	if (!cont)
		rsrc_ptr = x;
	else
		rsrc_ptr = ReadMacInt32(ROMBaseMac + rsrc_ptr + 8);

	for (;;) {
		lp = ROMBaseMac + rsrc_ptr;
		uint32 data = ReadMacInt32(lp + 12);
		uint32 type = ReadMacInt32(lp + 16);
		int16 id = ReadMacInt16(lp + 20);

		if (type == s_type && id == s_id)
			return data;

		rsrc_ptr = ReadMacInt32(lp + 8);
		if (!rsrc_ptr)
			break;
	}
	return 0;
}


/*
 *  Search offset of A-Trap routine in ROM
 */

static uint32 find_rom_trap(uint16 trap)
{
	uint8 *bp = (uint8 *)(ROMBaseHost + ReadMacInt32(ROMBaseMac + 0x22));
	uint16 rom_trap = 0xa800;
	uint32 ofs = 0;

again:
	for (int i=0; i<0x400; i++) {
		bool unimplemented = false;
		uint8 b = *bp++;
		if (b == 0x80)			// Unimplemented trap
			unimplemented = true;
		else if (b == 0xff) {	// Absolute address
			ofs = (bp[0] << 24) | (bp[1] << 16) | (bp[2] << 8) | bp[3];
			bp += 4;
		} else if (b & 0x80) {	// 1 byte offset
			int16 add = (b & 0x7f) << 1;
			if (!add)
				return 0;
			ofs += add;
		} else {				// 2 byte offset
			int16 add = ((b << 8) | *bp++) << 1;
			if (!add)
				return 0;
			ofs += add;
		}
		if (rom_trap == trap)
			return unimplemented ? 0 : ofs;
		rom_trap++;
	}
	rom_trap = 0xa000;
	goto again;
}


/*
 *  Print ROM information to stream,
 */

static void list_rom_resources(void)
{
	printf("ROM Resources:\n");
	printf("Offset\t Type\tID\tSize\tName\n");
	printf("------------------------------------------------\n");

	uint32 lp = ROMBaseMac + ReadMacInt32(ROMBaseMac + 0x1a);
	uint32 rsrc_ptr = ReadMacInt32(lp);

	for (;;) {
		lp = ROMBaseMac + rsrc_ptr;
		uint32 data = ReadMacInt32(lp + 12);

		char name[32];
		int name_len = ReadMacInt8(lp + 23), i;
		for (i=0; i<name_len; i++)
			name[i] = ReadMacInt8(lp + 24 + i);
		name[i] = 0;

		printf("%08x %c%c%c%c\t%d\t%d\t%s\n", data, ReadMacInt8(lp + 16), ReadMacInt8(lp + 17), ReadMacInt8(lp + 18), ReadMacInt8(lp + 19), ReadMacInt16(lp + 20), ReadMacInt32(ROMBaseMac + data - 8), name);

		rsrc_ptr = ReadMacInt32(lp + 8);
		if (!rsrc_ptr)
			break;
	}
	printf("\n");
}

// Mapping of Model IDs to Model names
struct mac_desc {
	const char *name;
	int32 id;
};

static mac_desc MacDesc[] = {
	{"Classic"				, 1},
	{"Mac XL"				, 2},
	{"Mac 512KE"			, 3},
	{"Mac Plus"				, 4},
	{"Mac SE"				, 5},
	{"Mac II"				, 6},
	{"Mac IIx"				, 7},
	{"Mac IIcx"				, 8},
	{"Mac SE/030"			, 9},
	{"Mac Portable"			, 10},
	{"Mac IIci"				, 11},
	{"Mac IIfx"				, 13},
	{"Mac Classic"			, 17},
	{"Mac IIsi"				, 18},
	{"Mac LC"				, 19},
	{"Quadra 900"			, 20},
	{"PowerBook 170"		, 21},
	{"Quadra 700"			, 22},
	{"Classic II"			, 23},
	{"PowerBook 100"		, 24},
	{"PowerBook 140"		, 25},
	{"Quadra 950"			, 26},
	{"Mac LCIII/Performa 450", 27},
	{"PowerBook Duo 210"	, 29},
	{"Centris 650"			, 30},
	{"PowerBook Duo 230"	, 32},
	{"PowerBook 180"		, 33},
	{"PowerBook 160"		, 34},
	{"Quadra 800"			, 35},
	{"Quadra 650"			, 36},
	{"Mac LCII"				, 37},
	{"PowerBook Duo 250"	, 38},
	{"Mac IIvi"				, 44},
	{"Mac IIvm/Performa 600", 45},
	{"Mac IIvx"				, 48},
	{"Color Classic/Performa 250", 49},
	{"PowerBook 165c"		, 50},
	{"Centris 610"			, 52},
	{"Quadra 610"			, 53},
	{"PowerBook 145"		, 54},
	{"Mac LC520"			, 56},
	{"Quadra/Centris 660AV"	, 60},
	{"Performa 46x"			, 62},
	{"PowerBook 180c"		, 71},
	{"PowerBook 520/520c/540/540c", 72},
	{"PowerBook Duo 270c"	, 77},
	{"Quadra 840AV"			, 78},
	{"Performa 550"			, 80},
	{"PowerBook 165"		, 84},
	{"PowerBook 190"		, 85},
	{"Mac TV"				, 88},
	{"Mac LC475/Performa 47x", 89},
	{"Mac LC575"			, 92},
	{"Quadra 605"			, 94},
	{"Quadra 630"			, 98},
	{"Mac LC580"			, 99},
	{"PowerBook Duo 280"	, 102},
	{"PowerBook Duo 280c"	, 103},
	{"PowerBook 150"		, 115},
	{"unknown", -1}
};

static void print_universal_info(uint32 info)
{
	uint8 id = ReadMacInt8(info + 18);
	uint16 hwcfg = ReadMacInt16(info + 16);
	uint16 rom85 = ReadMacInt16(info + 20);

	// Find model name
	const char *name = "unknown";
	for (int i=0; MacDesc[i].id >= 0; i++)
		if (MacDesc[i].id == id + 6) {
			name = MacDesc[i].name;
			break;
		}

	printf("%08x %02x\t%04x\t%04x\t%s\n", info - ROMBaseMac, id, hwcfg, rom85, name);
}

static void list_universal_infos(void)
{
	uint32 ofs = 0x3000;
	for (int i=0; i<0x2000; i+=2, ofs+=2)
		if (ReadMacInt32(ROMBaseMac + ofs) == 0xdc000505) {
			ofs -= 16;
			uint32 q;
			for (q=ofs; q > 0 && ReadMacInt32(ROMBaseMac + q) != ofs - q; q-=4) ;
			if (q > 0) {
				printf("Universal Table at %08x:\n", q);
				printf("Offset\t ID\tHWCfg\tROM85\tModel\n");
				printf("------------------------------------------------\n");
				while ((ofs = ReadMacInt32(ROMBaseMac + q))) {
					print_universal_info(ROMBaseMac + ofs + q);
					q += 4;
				}
			}
			break;
		}
	printf("\n");
}

static void print_rom_info(void)
{
	printf("\nROM Info:\n");
	printf("Checksum    : %08x\n", ReadMacInt32(ROMBaseMac));
	printf("Version     : %04x\n", ROMVersion);
	printf("Sub Version : %04x\n", ReadMacInt16(ROMBaseMac + 18));
	printf("Resource Map: %08x\n", ReadMacInt32(ROMBaseMac + 26));
	printf("Trap Tables : %08x\n\n", ReadMacInt32(ROMBaseMac + 34));
	if (ROMVersion == ROM_VERSION_32) {
		list_rom_resources();
		list_universal_infos();
	}
}


/*
 *  Driver stubs
 */

static const uint8 sony_driver[] = {	// Replacement for .Sony driver
	// Driver header
	SonyDriverFlags >> 8, SonyDriverFlags & 0xff, 0, 0, 0, 0, 0, 0,
	0x00, 0x18,							// Open() offset
	0x00, 0x1c,							// Prime() offset
	0x00, 0x20,							// Control() offset
	0x00, 0x2c,							// Status() offset
	0x00, 0x52,							// Close() offset
	0x05, 0x2e, 0x53, 0x6f, 0x6e, 0x79,	// ".Sony"

	// Open()
	M68K_EMUL_OP_SONY_OPEN >> 8, M68K_EMUL_OP_SONY_OPEN & 0xff,
	0x4e, 0x75,							//  rts

	// Prime()
	M68K_EMUL_OP_SONY_PRIME >> 8, M68K_EMUL_OP_SONY_PRIME & 0xff,
	0x60, 0x0e,							//  bra		IOReturn

	// Control()
	M68K_EMUL_OP_SONY_CONTROL >> 8, M68K_EMUL_OP_SONY_CONTROL & 0xff,
	0x0c, 0x68, 0x00, 0x01, 0x00, 0x1a,	//  cmp.w	#1,$1a(a0)
	0x66, 0x04,							//  bne		IOReturn
	0x4e, 0x75,							//  rts

	// Status()
	M68K_EMUL_OP_SONY_STATUS >> 8, M68K_EMUL_OP_SONY_STATUS & 0xff,

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
	0x70, 0xe8,							//  moveq	#-24,d0
	0x4e, 0x75							//  rts
};

static const uint8 disk_driver[] = {	// Generic disk driver
	// Driver header
	DiskDriverFlags >> 8, DiskDriverFlags & 0xff, 0, 0, 0, 0, 0, 0,
	0x00, 0x18,							// Open() offset
	0x00, 0x1c,							// Prime() offset
	0x00, 0x20,							// Control() offset
	0x00, 0x2c,							// Status() offset
	0x00, 0x52,							// Close() offset
	0x05, 0x2e, 0x44, 0x69, 0x73, 0x6b,	// ".Disk"

	// Open()
	M68K_EMUL_OP_DISK_OPEN >> 8, M68K_EMUL_OP_DISK_OPEN & 0xff,
	0x4e, 0x75,							//  rts

	// Prime()
	M68K_EMUL_OP_DISK_PRIME >> 8, M68K_EMUL_OP_DISK_PRIME & 0xff,
	0x60, 0x0e,							//  bra		IOReturn

	// Control()
	M68K_EMUL_OP_DISK_CONTROL >> 8, M68K_EMUL_OP_DISK_CONTROL & 0xff,
	0x0c, 0x68, 0x00, 0x01, 0x00, 0x1a,	//  cmp.w	#1,$1a(a0)
	0x66, 0x04,							//  bne		IOReturn
	0x4e, 0x75,							//  rts

	// Status()
	M68K_EMUL_OP_DISK_STATUS >> 8, M68K_EMUL_OP_DISK_STATUS & 0xff,

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
	0x70, 0xe8,							//  moveq	#-24,d0
	0x4e, 0x75							//  rts
};

static const uint8 cdrom_driver[] = {	// CD-ROM driver
	// Driver header
	CDROMDriverFlags >> 8, CDROMDriverFlags & 0xff, 0, 0, 0, 0, 0, 0,
	0x00, 0x1c,							// Open() offset
	0x00, 0x20,							// Prime() offset
	0x00, 0x24,							// Control() offset
	0x00, 0x30,							// Status() offset
	0x00, 0x56,							// Close() offset
	0x08, 0x2e, 0x41, 0x70, 0x70, 0x6c, 0x65, 0x43, 0x44, 0x00,	// ".AppleCD"

	// Open()
	M68K_EMUL_OP_CDROM_OPEN >> 8, M68K_EMUL_OP_CDROM_OPEN & 0xff,
	0x4e, 0x75,							//  rts

	// Prime()
	M68K_EMUL_OP_CDROM_PRIME >> 8, M68K_EMUL_OP_CDROM_PRIME & 0xff,
	0x60, 0x0e,							//  bra		IOReturn

	// Control()
	M68K_EMUL_OP_CDROM_CONTROL >> 8, M68K_EMUL_OP_CDROM_CONTROL & 0xff,
	0x0c, 0x68, 0x00, 0x01, 0x00, 0x1a,	//  cmp.w	#1,$1a(a0)
	0x66, 0x04,							//  bne		IOReturn
	0x4e, 0x75,							//  rts

	// Status()
	M68K_EMUL_OP_CDROM_STATUS >> 8, M68K_EMUL_OP_CDROM_STATUS & 0xff,

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
	0x70, 0xe8,							//  moveq	#-24,d0
	0x4e, 0x75							//  rts
};

static const uint8 ain_driver[] = {	// .AIn driver header
	// Driver header
	0x4d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x18,							// Open() offset
	0x00, 0x1e,							// Prime() offset
	0x00, 0x24,							// Control() offset
	0x00, 0x32,							// Status() offset
	0x00, 0x38,							// Close() offset
	0x04, 0x2e, 0x41, 0x49, 0x6e, 0x09,	// ".AIn",9

	// Open()
	0x70, 0x00,							//  moveq	#0,d0
	M68K_EMUL_OP_SERIAL_OPEN >> 8, M68K_EMUL_OP_SERIAL_OPEN & 0xff,
	0x4e, 0x75,							//	rts

	// Prime()
	0x70, 0x00,							//  moveq	#0,d0
	M68K_EMUL_OP_SERIAL_PRIME >> 8, M68K_EMUL_OP_SERIAL_PRIME & 0xff,
	0x60, 0x1a,							//	bra		IOReturn

	// Control()
	0x70, 0x00,							//  moveq	#0,d0
	M68K_EMUL_OP_SERIAL_CONTROL >> 8, M68K_EMUL_OP_SERIAL_CONTROL & 0xff,
	0x0c, 0x68, 0x00, 0x01, 0x00, 0x1a,	//	cmp.w	#1,$1a(a0)
	0x66, 0x0e,							//	bne		IOReturn
	0x4e, 0x75,							//	rts

	// Status()
	0x70, 0x00,							//  moveq	#0,d0
	M68K_EMUL_OP_SERIAL_STATUS >> 8, M68K_EMUL_OP_SERIAL_STATUS & 0xff,
	0x60, 0x06,							//  bra IOReturn

	// Close()
	0x70, 0x00,							//  moveq	#0,d0
	M68K_EMUL_OP_SERIAL_CLOSE >> 8, M68K_EMUL_OP_SERIAL_CLOSE & 0xff,
	0x4e, 0x75,							//	rts

	// IOReturn
	0x32, 0x28, 0x00, 0x06,				//	move.w	6(a0),d1
	0x08, 0x01, 0x00, 0x09,				//	btst	#9,d1
	0x67, 0x0c,							//	beq		1
	0x4a, 0x40,							//	tst.w	d0
	0x6f, 0x02,							//	ble		2
	0x42, 0x40,							//	clr.w	d0
	0x31, 0x40, 0x00, 0x10,				//2	move.w	d0,$10(a0)
	0x4e, 0x75,							//	rts
	0x4a, 0x40,							//1	tst.w	d0
	0x6f, 0x04,							//	ble		3
	0x42, 0x40,							//	clr.w	d0
	0x4e, 0x75,							//	rts
	0x2f, 0x38, 0x08, 0xfc,				//3	move.l	$8fc,-(a7)
	0x4e, 0x75,							//	rts
};

static const uint8 aout_driver[] = {	// .AOut driver header
	// Driver header
	0x4e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x1a,							// Open() offset
	0x00, 0x20,							// Prime() offset
	0x00, 0x26,							// Control() offset
	0x00, 0x34,							// Status() offset
	0x00, 0x3a,							// Close() offset
	0x05, 0x2e, 0x41, 0x4f, 0x75, 0x74, 0x09, 0x00,		// ".AOut",9

	// Open()
	0x70, 0x01,							//  moveq	#1,d0
	M68K_EMUL_OP_SERIAL_OPEN >> 8, M68K_EMUL_OP_SERIAL_OPEN & 0xff,
	0x4e, 0x75,							//	rts

	// Prime()
	0x70, 0x01,							//  moveq	#1,d0
	M68K_EMUL_OP_SERIAL_PRIME >> 8, M68K_EMUL_OP_SERIAL_PRIME & 0xff,
	0x60, 0x1a,							//	bra		IOReturn

	// Control()
	0x70, 0x01,							//  moveq	#1,d0
	M68K_EMUL_OP_SERIAL_CONTROL >> 8, M68K_EMUL_OP_SERIAL_CONTROL & 0xff,
	0x0c, 0x68, 0x00, 0x01, 0x00, 0x1a,	//	cmp.w	#1,$1a(a0)
	0x66, 0x0e,							//	bne		IOReturn
	0x4e, 0x75,							//	rts

	// Status()
	0x70, 0x01,							//  moveq	#1,d0
	M68K_EMUL_OP_SERIAL_STATUS >> 8, M68K_EMUL_OP_SERIAL_STATUS & 0xff,
	0x60, 0x06,							//  bra IOReturn

	// Close()
	0x70, 0x01,							//  moveq	#1,d0
	M68K_EMUL_OP_SERIAL_CLOSE >> 8, M68K_EMUL_OP_SERIAL_CLOSE & 0xff,
	0x4e, 0x75,							//	rts

	// IOReturn
	0x32, 0x28, 0x00, 0x06,				//	move.w	6(a0),d1
	0x08, 0x01, 0x00, 0x09,				//	btst	#9,d1
	0x67, 0x0c,							//	beq		1
	0x4a, 0x40,							//	tst.w	d0
	0x6f, 0x02,							//	ble		2
	0x42, 0x40,							//	clr.w	d0
	0x31, 0x40, 0x00, 0x10,				//2	move.w	d0,$10(a0)
	0x4e, 0x75,							//	rts
	0x4a, 0x40,							//1	tst.w	d0
	0x6f, 0x04,							//	ble		3
	0x42, 0x40,							//	clr.w	d0
	0x4e, 0x75,							//	rts
	0x2f, 0x38, 0x08, 0xfc,				//3	move.l	$8fc,-(a7)
	0x4e, 0x75,							//	rts
};

static const uint8 bin_driver[] = {	// .BIn driver header
	// Driver header
	0x4d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x18,							// Open() offset
	0x00, 0x1e,							// Prime() offset
	0x00, 0x24,							// Control() offset
	0x00, 0x32,							// Status() offset
	0x00, 0x38,							// Close() offset
	0x04, 0x2e, 0x42, 0x49, 0x6e, 0x09,	// ".BIn",9

	// Open()
	0x70, 0x02,							//  moveq	#2,d0
	M68K_EMUL_OP_SERIAL_OPEN >> 8, M68K_EMUL_OP_SERIAL_OPEN & 0xff,
	0x4e, 0x75,							//	rts

	// Prime()
	0x70, 0x02,							//  moveq	#2,d0
	M68K_EMUL_OP_SERIAL_PRIME >> 8, M68K_EMUL_OP_SERIAL_PRIME & 0xff,
	0x60, 0x1a,							//	bra		IOReturn

	// Control()
	0x70, 0x02,							//  moveq	#2,d0
	M68K_EMUL_OP_SERIAL_CONTROL >> 8, M68K_EMUL_OP_SERIAL_CONTROL & 0xff,
	0x0c, 0x68, 0x00, 0x01, 0x00, 0x1a,	//	cmp.w	#1,$1a(a0)
	0x66, 0x0e,							//	bne		IOReturn
	0x4e, 0x75,							//	rts

	// Status()
	0x70, 0x02,							//  moveq	#2,d0
	M68K_EMUL_OP_SERIAL_STATUS >> 8, M68K_EMUL_OP_SERIAL_STATUS & 0xff,
	0x60, 0x06,							//  bra IOReturn

	// Close()
	0x70, 0x02,							//  moveq	#2,d0
	M68K_EMUL_OP_SERIAL_CLOSE >> 8, M68K_EMUL_OP_SERIAL_CLOSE & 0xff,
	0x4e, 0x75,							//	rts

	// IOReturn
	0x32, 0x28, 0x00, 0x06,				//	move.w	6(a0),d1
	0x08, 0x01, 0x00, 0x09,				//	btst	#9,d1
	0x67, 0x0c,							//	beq		1
	0x4a, 0x40,							//	tst.w	d0
	0x6f, 0x02,							//	ble		2
	0x42, 0x40,							//	clr.w	d0
	0x31, 0x40, 0x00, 0x10,				//2	move.w	d0,$10(a0)
	0x4e, 0x75,							//	rts
	0x4a, 0x40,							//1	tst.w	d0
	0x6f, 0x04,							//	ble		3
	0x42, 0x40,							//	clr.w	d0
	0x4e, 0x75,							//	rts
	0x2f, 0x38, 0x08, 0xfc,				//3	move.l	$8fc,-(a7)
	0x4e, 0x75,							//	rts
};

static const uint8 bout_driver[] = {	// .BOut driver header
	// Driver header
	0x4e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x1a,							// Open() offset
	0x00, 0x20,							// Prime() offset
	0x00, 0x26,							// Control() offset
	0x00, 0x34,							// Status() offset
	0x00, 0x3a,							// Close() offset
	0x05, 0x2e, 0x42, 0x4f, 0x75, 0x74, 0x09, 0x00,		// ".BOut",9

	// Open()
	0x70, 0x03,							//  moveq	#3,d0
	M68K_EMUL_OP_SERIAL_OPEN >> 8, M68K_EMUL_OP_SERIAL_OPEN & 0xff,
	0x4e, 0x75,							//	rts

	// Prime()
	0x70, 0x03,							//  moveq	#3,d0
	M68K_EMUL_OP_SERIAL_PRIME >> 8, M68K_EMUL_OP_SERIAL_PRIME & 0xff,
	0x60, 0x1a,							//	bra		IOReturn

	// Control()
	0x70, 0x03,							//  moveq	#3,d0
	M68K_EMUL_OP_SERIAL_CONTROL >> 8, M68K_EMUL_OP_SERIAL_CONTROL & 0xff,
	0x0c, 0x68, 0x00, 0x01, 0x00, 0x1a,	//	cmp.w	#1,$1a(a0)
	0x66, 0x0e,							//	bne		IOReturn
	0x4e, 0x75,							//	rts

	// Status()
	0x70, 0x03,							//  moveq	#3,d0
	M68K_EMUL_OP_SERIAL_STATUS >> 8, M68K_EMUL_OP_SERIAL_STATUS & 0xff,
	0x60, 0x06,							//  bra IOReturn

	// Close()
	0x70, 0x03,							//  moveq	#3,d0
	M68K_EMUL_OP_SERIAL_CLOSE >> 8, M68K_EMUL_OP_SERIAL_CLOSE & 0xff,
	0x4e, 0x75,							//	rts

	// IOReturn
	0x32, 0x28, 0x00, 0x06,				//	move.w	6(a0),d1
	0x08, 0x01, 0x00, 0x09,				//	btst	#9,d1
	0x67, 0x0c,							//	beq		1
	0x4a, 0x40,							//	tst.w	d0
	0x6f, 0x02,							//	ble		2
	0x42, 0x40,							//	clr.w	d0
	0x31, 0x40, 0x00, 0x10,				//2	move.w	d0,$10(a0)
	0x4e, 0x75,							//	rts
	0x4a, 0x40,							//1	tst.w	d0
	0x6f, 0x04,							//	ble		3
	0x42, 0x40,							//	clr.w	d0
	0x4e, 0x75,							//	rts
	0x2f, 0x38, 0x08, 0xfc,				//3	move.l	$8fc,-(a7)
	0x4e, 0x75,							//	rts
};


/*
 *  ADBOp() patch
 */

static const uint8 adbop_patch[] = {	// Call ADBOp() completion procedure
										// The completion procedure may call ADBOp() again!
	0x40, 0xe7,				//	move	sr,-(sp)
	0x00, 0x7c, 0x07, 0x00,	//	ori		#$0700,sr
	M68K_EMUL_OP_ADBOP >> 8, M68K_EMUL_OP_ADBOP & 0xff,
	0x48, 0xe7, 0x70, 0xf0,	//	movem.l	d1-d3/a0-a3,-(sp)
	0x26, 0x48,				//	move.l	a0,a3
	0x4a, 0xab, 0x00, 0x04,	//	tst.l	4(a3)
	0x67, 0x00, 0x00, 0x18,	//	beq		1
	0x20, 0x53,				//	move.l	(a3),a0
	0x22, 0x6b, 0x00, 0x04,	//	move.l	4(a3),a1
	0x24, 0x6b, 0x00, 0x08,	//	move.l	8(a3),a2
	0x26, 0x78, 0x0c, 0xf8,	//	move.l	$cf8,a3
	0x4e, 0x91,				//	jsr		(a1)
	0x70, 0x00,				//	moveq	#0,d0
	0x60, 0x00, 0x00, 0x04,	//	bra		2
//	0x70, 0xff,				//1	moveq	#-1,d0
	0x70, 0x00,				//1	moveq	#0,d0	copy from BasiliskII-build142
	0x4c, 0xdf, 0x0f, 0x0e,	//2	movem.l	(sp)+,d1-d3/a0-a3
	0x46, 0xdf,				//	move	(sp)+,sr
	0x4e, 0x75				//	rts
};


/*
 *  Install .Sony, disk and CD-ROM drivers
 */

void InstallDrivers(uint32 pb)
{
	D(bug("InstallDrivers, pb %08x\n", pb));
	M68kRegisters r;

	// Install Microseconds() replacement routine
	r.a[0] = ROMBaseMac + microseconds_offset;
	r.d[0] = 0xa093;
	Execute68kTrap(0xa247, &r);		// SetOSTrapAddress()

	// Install DebugUtil() replacement routine
	r.a[0] = ROMBaseMac + debugutil_offset;
	r.d[0] = 0xa08d;
	Execute68kTrap(0xa247, &r);		// SetOSTrapAddress()

	// Install disk driver
	r.a[0] = ROMBaseMac + sony_offset + 0x100;
	r.d[0] = (uint32)DiskRefNum;
	Execute68kTrap(0xa43d, &r);		// DrvrInstallRsrvMem()
	r.a[0] = ReadMacInt32(ReadMacInt32(0x11c) + ~DiskRefNum * 4);	// Get driver handle from Unit Table
	Execute68kTrap(0xa029, &r);		// HLock()
	uint32 dce = ReadMacInt32(r.a[0]);
	WriteMacInt32(dce + dCtlDriver, ROMBaseMac + sony_offset + 0x100);
	WriteMacInt16(dce + dCtlFlags, DiskDriverFlags);

	// Open disk driver
	WriteMacInt32(pb + ioNamePtr, ROMBaseMac + sony_offset + 0x112);
	r.a[0] = pb;
	Execute68kTrap(0xa000, &r);		// Open()

	// Install CD-ROM driver unless nocdrom option given
	if (!PrefsFindBool("nocdrom")) {

		// Install CD-ROM driver
		r.a[0] = ROMBaseMac + sony_offset + 0x200;
		r.d[0] = (uint32)CDROMRefNum;
		Execute68kTrap(0xa43d, &r);		// DrvrInstallRsrvMem()
		r.a[0] = ReadMacInt32(ReadMacInt32(0x11c) + ~CDROMRefNum * 4);	// Get driver handle from Unit Table
		Execute68kTrap(0xa029, &r);		// HLock()
		dce = ReadMacInt32(r.a[0]);
		WriteMacInt32(dce + dCtlDriver, ROMBaseMac + sony_offset + 0x200);
		WriteMacInt16(dce + dCtlFlags, CDROMDriverFlags);

		// Open CD-ROM driver
		WriteMacInt32(pb + ioNamePtr, ROMBaseMac + sony_offset + 0x212);
		r.a[0] = pb;
		Execute68kTrap(0xa000, &r);		// Open()
	}
}


/*
 *  Install serial drivers
 */

void InstallSERD(void)
{
	D(bug("InstallSERD\n"));

	// All drivers are inside the SERD resource
	M68kRegisters r;

	// Install .AIn driver
	r.d[0] = (uint32)-6;
	r.a[0] = ROMBaseMac + serd_offset + 0x100;
	Execute68kTrap(0xa53d, &r);	// DrvrInstallRsrvMem()
	Execute68kTrap(0xa029, &r);	// HLock()
	uint32 drvr_ptr = ReadMacInt32(r.a[0]);
	WriteMacInt32(drvr_ptr + dCtlDriver, ROMBaseMac + serd_offset + 0x100);			// Pointer to driver header
	WriteMacInt16(drvr_ptr + dCtlFlags, (ain_driver[0] << 8) + ain_driver[1]);		// Driver flags
	WriteMacInt16(drvr_ptr + dCtlQHdr + qFlags, 9);									// Version number

	// Install .AOut driver
	r.d[0] = (uint32)-7;
	r.a[0] = ROMBaseMac + serd_offset + 0x200;
	Execute68kTrap(0xa53d, &r);	// DrvrInstallRsrvMem()
	Execute68kTrap(0xa029, &r);	// HLock()
	drvr_ptr = ReadMacInt32(r.a[0]);
	WriteMacInt32(drvr_ptr + dCtlDriver, ROMBaseMac + serd_offset + 0x200);			// Pointer to driver header
	WriteMacInt16(drvr_ptr + dCtlFlags, (aout_driver[0] << 8) + aout_driver[1]);	// Driver flags
	WriteMacInt16(drvr_ptr + dCtlQHdr + qFlags, 9);									// Version number

	// Install .BIn driver
	r.d[0] = (uint32)-8;
	r.a[0] = ROMBaseMac + serd_offset + 0x300;
	Execute68kTrap(0xa53d, &r);	// DrvrInstallRsrvMem()
	Execute68kTrap(0xa029, &r);	// HLock()
	drvr_ptr = ReadMacInt32(r.a[0]);
	WriteMacInt32(drvr_ptr + dCtlDriver, ROMBaseMac + serd_offset + 0x300);			// Pointer to driver header
	WriteMacInt16(drvr_ptr + dCtlFlags, (bin_driver[0] << 8) + bin_driver[1]);		// Driver flags
	WriteMacInt16(drvr_ptr + dCtlQHdr + qFlags, 9);									// Version number

	// Install .BOut driver
	r.d[0] = (uint32)-9;
	r.a[0] = ROMBaseMac + serd_offset + 0x400;
	Execute68kTrap(0xa53d, &r);	// DrvrInstallRsrvMem()
	Execute68kTrap(0xa029, &r);	// HLock()
	drvr_ptr = ReadMacInt32(r.a[0]);
	WriteMacInt32(drvr_ptr + dCtlDriver, ROMBaseMac + serd_offset + 0x400);			// Pointer to driver header
	WriteMacInt16(drvr_ptr + dCtlFlags, (bout_driver[0] << 8) + bout_driver[1]);	// Driver flags
	WriteMacInt16(drvr_ptr + dCtlQHdr + qFlags, 9);									// Version number
}


/*
 *  Install patches after MacOS startup
 */

void PatchAfterStartup(void)
{
#if SUPPORTS_EXTFS
	// Install external file system
	InstallExtFS();
#endif
}


/*
 *  Check ROM version, returns false if ROM version is not supported
 */

bool CheckROM(void)
{
	// Read version
	ROMVersion = ntohs(*(uint16 *)(ROMBaseHost + 8));

#if REAL_ADDRESSING || DIRECT_ADDRESSING
	// Real and direct addressing modes require a 32-bit clean ROM
	return ROMVersion == ROM_VERSION_32;
#else
	// Virtual addressing mode works with 32-bit clean Mac II ROMs and Classic ROMs
	return (ROMVersion == ROM_VERSION_CLASSIC) || (ROMVersion == ROM_VERSION_32);
#endif
}


/*
 *  Install ROM patches, returns false if ROM version is not supported
 */

// ROM patches for Mac Classic/SE ROMs (version $0276)
static bool patch_rom_classic(void)
{
	uint16 *wp;
	uint32 base;

	// Don't jump into debugger (VIA line)
	wp = (uint16 *)(ROMBaseHost + 0x1c40);
	*wp = htons(0x601e);

	// Don't complain about incorrect ROM checksum
	wp = (uint16 *)(ROMBaseHost + 0x1c6c);
	*wp = htons(0x7c00);

	// Don't initialize IWM
	wp = (uint16 *)(ROMBaseHost + 0x50);
	*wp++ = htons(M68K_NOP);
	*wp = htons(M68K_NOP);

	// Skip startup sound
	wp = (uint16 *)(ROMBaseHost + 0x6a);
	*wp++ = htons(M68K_NOP);
	*wp = htons(M68K_NOP);

	// Don't loop in ADB init
	wp = (uint16 *)(ROMBaseHost + 0x3364);
	*wp = htons(M68K_NOP);

	// Patch ClkNoMem
	wp = (uint16 *)(ROMBaseHost + 0xa2c0);
	*wp++ = htons(M68K_EMUL_OP_CLKNOMEM);
	*wp = htons(0x4ed5);			// jmp	(a5)

	// Skip main memory test (not that it wouldn't pass, but it's faster that way)
	wp = (uint16 *)(ROMBaseHost + 0x11e);
	*wp++ = htons(M68K_NOP);
	*wp = htons(M68K_NOP);

	// Don't open .Sound driver but install our own drivers
	wp = (uint16 *)(ROMBaseHost + 0x36caa);
	*wp++ = htons(M68K_EMUL_OP_INSTALL_DRIVERS);
	*wp = htons(0x4e75); //rts

#if 1
	// Don't look for SCSI devices
	wp = (uint16 *)(ROMBaseHost + 0xd5a);
	*wp = htons(0x601e);
#endif

	// Replace .Sony driver
	sony_offset = 0x34680;
	D(bug("sony %08lx\n", sony_offset));
	memcpy(ROMBaseHost + sony_offset, sony_driver, sizeof(sony_driver));

	// Install .Disk and .AppleCD drivers
	memcpy(ROMBaseHost + sony_offset + 0x100, disk_driver, sizeof(disk_driver));
	memcpy(ROMBaseHost + sony_offset + 0x200, cdrom_driver, sizeof(cdrom_driver));

	// Copy icons to ROM
	SonyDiskIconAddr = ROMBaseMac + sony_offset + 0x400;
	memcpy(ROMBaseHost + sony_offset + 0x400, SonyDiskIcon, sizeof(SonyDiskIcon));
	SonyDriveIconAddr = ROMBaseMac + sony_offset + 0x600;
	memcpy(ROMBaseHost + sony_offset + 0x600, SonyDriveIcon, sizeof(SonyDriveIcon));
	DiskIconAddr = ROMBaseMac + sony_offset + 0x800;
	memcpy(ROMBaseHost + sony_offset + 0x800, DiskIcon, sizeof(DiskIcon));
	CDROMIconAddr = ROMBaseMac + sony_offset + 0xa00;
	memcpy(ROMBaseHost + sony_offset + 0xa00, CDROMIcon, sizeof(CDROMIcon));

	// Install SERD patch and serial drivers
	serd_offset = 0x31bae;
	D(bug("serd %08lx\n", serd_offset));
	wp = (uint16 *)(ROMBaseHost + serd_offset + 12);
	*wp++ = htons(M68K_EMUL_OP_SERD);
	*wp = htons(M68K_RTS);
	memcpy(ROMBaseHost + serd_offset + 0x100, ain_driver, sizeof(ain_driver));
	memcpy(ROMBaseHost + serd_offset + 0x200, aout_driver, sizeof(aout_driver));
	memcpy(ROMBaseHost + serd_offset + 0x300, bin_driver, sizeof(bin_driver));
	memcpy(ROMBaseHost + serd_offset + 0x400, bout_driver, sizeof(bout_driver));

	// Replace ADBOp()
	memcpy(ROMBaseHost + 0x3880, adbop_patch, sizeof(adbop_patch));

	// Replace Time Manager
	wp = (uint16 *)(ROMBaseHost + 0x1a95c);
	*wp++ = htons(M68K_EMUL_OP_INSTIME);
	*wp = htons(M68K_RTS);
	wp = (uint16 *)(ROMBaseHost + 0x1a96a);
	*wp++ = htons(0x40e7);		// move	sr,-(sp)
	*wp++ = htons(0x007c);		// ori	#$0700,sr
	*wp++ = htons(0x0700);
	*wp++ = htons(M68K_EMUL_OP_RMVTIME);
	*wp++ = htons(0x46df);		// move	(sp)+,sr
	*wp = htons(M68K_RTS);
	wp = (uint16 *)(ROMBaseHost + 0x1a984);
	*wp++ = htons(0x40e7);		// move	sr,-(sp)
	*wp++ = htons(0x007c);		// ori	#$0700,sr
	*wp++ = htons(0x0700);
	*wp++ = htons(M68K_EMUL_OP_PRIMETIME);
	*wp++ = htons(0x46df);		// move	(sp)+,sr
	*wp++ = htons(M68K_RTS);
	microseconds_offset = (uint8 *)wp - ROMBaseHost;
	*wp++ = htons(M68K_EMUL_OP_MICROSECONDS);
	*wp++ = htons(M68K_RTS);

	// Replace DebugUtil
	debugutil_offset = (uint8 *)wp - ROMBaseHost;
	*wp++ = htons(M68K_EMUL_OP_DEBUGUTIL);
	*wp = htons(M68K_RTS);

	// Replace SCSIDispatch()
	wp = (uint16 *)(ROMBaseHost + 0x1a206);
	*wp++ = htons(M68K_EMUL_OP_SCSI_DISPATCH);
	*wp++ = htons(0x2e49);		// move.l	a1,a7
	*wp = htons(M68K_JMP_A0);

	// Modify vCheckLoad() so we can patch resources
	wp = (uint16 *)(ROMBaseHost + 0xe740);
	*wp++ = htons(M68K_JMP);
	*wp++ = htons((ROMBaseMac + sony_offset + 0x300) >> 16);
	*wp = htons((ROMBaseMac + sony_offset + 0x300) & 0xffff);
	wp = (uint16 *)(ROMBaseHost + sony_offset + 0x300);
	*wp++ = htons(0x2f03);		// move.l	d3,-(sp) (save type)
	*wp++ = htons(0x2078);		// move.l	$07f0,a0
	*wp++ = htons(0x07f0);
	*wp++ = htons(M68K_JSR_A0);
	*wp++ = htons(0x221f);		// move.l	(sp)+,d1 (restore type)
	*wp++ = htons(M68K_EMUL_OP_CHECKLOAD);
	*wp = htons(M68K_RTS);

	// Install PutScrap() patch for clipboard data exchange (the patch is activated by EMUL_OP_INSTALL_DRIVERS)
	PutScrapPatch = ROMBaseMac + sony_offset + 0xc00;
	base = ROMBaseMac + 0x12794;
	wp = (uint16 *)(ROMBaseHost + sony_offset + 0xc00);
	*wp++ = htons(M68K_EMUL_OP_PUT_SCRAP);
	*wp++ = htons(M68K_JMP);
	*wp++ = htons(base >> 16);
	*wp = htons(base & 0xffff);

#if 0
	// Boot from internal EDisk
	wp = (uint16 *)(ROMBaseHost + 0x3f83c);
	*wp = htons(M68K_NOP);
#endif

	// Patch VIA interrupt handler
	wp = (uint16 *)(ROMBaseHost + 0x2b3a);	// Level 1 handler
	*wp++ = htons(0x5888);		// addq.l	#4,a0
	*wp++ = htons(0x5888);		// addq.l	#4,a0
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp = htons(M68K_NOP);

	wp = (uint16 *)(ROMBaseHost + 0x2be4);	// 60Hz handler (handles everything)
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_EMUL_OP_IRQ);
	*wp++ = htons(0x4a80);		// tst.l	d0
	*wp = htons(0x67f4);		// beq		0x402be2
	return true;
}

// ROM patches for 32-bit clean Mac-II ROMs (version $067c)
static bool patch_rom_32(void)
{
	uint16 *wp;
	uint8 *bp;
	uint32 base;

	// Find UniversalInfo
	static const uint8 universal_dat[] = {0xdc, 0x00, 0x05, 0x05, 0x3f, 0xff, 0x01, 0x00};
	if ((base = find_rom_data(0x3400, 0x3c00, universal_dat, sizeof(universal_dat))) == 0) return false;
	UniversalInfo = base - 0x10;
	D(bug("universal %08lx\n", UniversalInfo));

	// Patch UniversalInfo (disable NuBus slots)
	bp = ROMBaseHost + UniversalInfo + ReadMacInt32(ROMBaseMac + UniversalInfo + 12);	// nuBusInfoPtr
	bp[0] = 0x03;
	for (int i=1; i<16; i++)
		bp[i] = 0x08;

	// Set model ID from preferences
	bp = ROMBaseHost + UniversalInfo + 18;		// productKind
	*bp = PrefsFindInt32("modelid");

#if !ROM_IS_WRITE_PROTECTED
#if defined(USE_SCRATCHMEM_SUBTERFUGE)
	// Set hardware base addresses to scratch memory area
	if (PatchHWBases) {
		extern uint8 *ScratchMem;
		const uint32 ScratchMemBase = Host2MacAddr(ScratchMem);
		
		D(bug("LMGlob\tOfs/4\tBase\n"));
		base = ROMBaseMac + UniversalInfo + ReadMacInt32(ROMBaseMac + UniversalInfo); // decoderInfoPtr
		wp = (uint16 *)(ROMBaseHost + 0x94a);
		while (*wp != 0xffff) {
			int16 ofs = ntohs(*wp++);			// offset in decoderInfo (/4)
			int16 lmg = ntohs(*wp++);			// address of LowMem global
			D(bug("0x%04x\t%d\t0x%08x\n", lmg, ofs, ReadMacInt32(base + ofs*4)));
			
			// Fake address only if this is not the ASC base
			if (lmg != 0xcc0)
				WriteMacInt32(base + ofs*4, ScratchMemBase);
		}
	}
#else
#error System specific handling for writable ROM is required here
#endif
#endif

	// Make FPU optional
	if (FPUType == 0) {
		bp = ROMBaseHost + UniversalInfo + 22;	// defaultRSRCs
		*bp = 4;	// FPU optional
	}

	// Install special reset opcode and jump (skip hardware detection and tests)
	wp = (uint16 *)(ROMBaseHost + 0x8c);
	*wp++ = htons(M68K_EMUL_OP_RESET);
	*wp++ = htons(M68K_JMP);
	*wp++ = htons((ROMBaseMac + 0xba) >> 16);
	*wp = htons((ROMBaseMac + 0xba) & 0xffff);

	// Don't GetHardwareInfo
	wp = (uint16 *)(ROMBaseHost + 0xc2);
	*wp++ = htons(M68K_NOP);
	*wp = htons(M68K_NOP);

	// Don't init VIAs
	wp = (uint16 *)(ROMBaseHost + 0xc6);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp = htons(M68K_NOP);

	// Fake CPU type test
	wp = (uint16 *)(ROMBaseHost + 0x7c0);
	*wp++ = htons(0x7e00 + CPUType);
	*wp = htons(M68K_RTS);

	// Don't clear end of BootGlobs upto end of RAM (address xxxx0000)
	static const uint8 clear_globs_dat[] = {0x42, 0x9a, 0x36, 0x0a, 0x66, 0xfa};
	base = find_rom_data(0xa00, 0xb00, clear_globs_dat, sizeof(clear_globs_dat));
	D(bug("clear_globs %08lx\n", base));
	if (base) {		// ROM15/20/22/23/26/27/32
		wp = (uint16 *)(ROMBaseHost + base + 2);
		*wp++ = htons(M68K_NOP);
		*wp = htons(M68K_NOP);
	}

	// Patch InitMMU (no MMU present, don't choke on unknown CPU types)
	if (ROMSize <= 0x80000) {
		static const uint8 init_mmu_dat[] = {0x0c, 0x47, 0x00, 0x03, 0x62, 0x00, 0xfe};
		if ((base = find_rom_data(0x4000, 0x50000, init_mmu_dat, sizeof(init_mmu_dat))) == 0) return false;
	} else {
		static const uint8 init_mmu_dat[] = {0x0c, 0x47, 0x00, 0x04, 0x62, 0x00, 0xfd};
		if ((base = find_rom_data(0x80000, 0x90000, init_mmu_dat, sizeof(init_mmu_dat))) == 0) return false;
	}
	D(bug("init_mmu %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	wp++;
	*wp++ = htons(0x7000);			// moveq #0,d0
	*wp = htons(M68K_NOP);

	// Patch InitMMU (no RBV present)
	static const uint8 init_mmu2_dat[] = {0x08, 0x06, 0x00, 0x0d, 0x67};
	if (ROMSize <= 0x80000) {
		base = find_rom_data(0x4000, 0x50000, init_mmu2_dat, sizeof(init_mmu2_dat));
	} else {
		base = find_rom_data(0x80000, 0x90000, init_mmu2_dat, sizeof(init_mmu2_dat));
	}
	D(bug("init_mmu2 %08lx\n", base));
	if (base) {		// ROM11/10/13/26
		bp = (uint8 *)(ROMBaseHost + base + 4);
		*bp = 0x60;						// bra
	}

	// Patch InitMMU (don't init MMU)
	static const uint8 init_mmu3_dat[] = {0x0c, 0x2e, 0x00, 0x01, 0xff, 0xe6, 0x66, 0x0c, 0x4c, 0xed, 0x03, 0x87, 0xff, 0xe8};
	if (ROMSize <= 0x80000) {
		if ((base = find_rom_data(0x4000, 0x50000, init_mmu3_dat, sizeof(init_mmu3_dat))) == 0) return false;
	} else {
		if ((base = find_rom_data(0x80000, 0x90000, init_mmu3_dat, sizeof(init_mmu3_dat))) == 0) return false;
	}
	D(bug("init_mmu3 %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base + 6);
	*wp = htons(M68K_NOP);

	// Replace XPRAM routines
	static const uint8 read_xpram_dat[] = {0x26, 0x4e, 0x41, 0xf9, 0x50, 0xf0, 0x00, 0x00, 0x08, 0x90, 0x00, 0x02};
	base = find_rom_data(0x40000, 0x50000, read_xpram_dat, sizeof(read_xpram_dat));
	D(bug("read_xpram %08lx\n", base));
	if (base) {			// ROM10
		wp = (uint16 *)(ROMBaseHost + base);
		*wp++ = htons(M68K_EMUL_OP_READ_XPRAM);
		*wp = htons(0x4ed6);		// jmp	(a6)
	}
	static const uint8 read_xpram2_dat[] = {0x26, 0x4e, 0x08, 0x92, 0x00, 0x02, 0xea, 0x59, 0x02, 0x01, 0x00, 0x07, 0x00, 0x01, 0x00, 0xb8};
	base = find_rom_data(0x40000, 0x50000, read_xpram2_dat, sizeof(read_xpram2_dat));
	D(bug("read_xpram2 %08lx\n", base));
	if (base) {			// ROM11
		wp = (uint16 *)(ROMBaseHost + base);
		*wp++ = htons(M68K_EMUL_OP_READ_XPRAM);
		*wp = htons(0x4ed6);		// jmp	(a6)
	}
	if (ROMSize > 0x80000) {
		static const uint8 read_xpram3_dat[] = {0x48, 0xe7, 0xe0, 0x60, 0x02, 0x01, 0x00, 0x70, 0x0c, 0x01, 0x00, 0x20};
		base = find_rom_data(0x80000, 0x90000, read_xpram3_dat, sizeof(read_xpram3_dat));
		D(bug("read_xpram3 %08lx\n", base));
		if (base) {		// ROM15
			wp = (uint16 *)(ROMBaseHost + base);
			*wp++ = htons(M68K_EMUL_OP_READ_XPRAM2);
			*wp = htons(M68K_RTS);
		}
	}

	// Patch ClkNoMem
	base = find_rom_trap(0xa053);
	wp = (uint16 *)(ROMBaseHost + base);
	if (ntohs(*wp) == 0x4ed5) {	// ROM23/26/27/32
		static const uint8 clk_no_mem_dat[] = {0x40, 0xc2, 0x00, 0x7c, 0x07, 0x00, 0x48, 0x42};
		if ((base = find_rom_data(0xb0000, 0xb8000, clk_no_mem_dat, sizeof(clk_no_mem_dat))) == 0) return false;
	}
	D(bug("clk_no_mem %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp++ = htons(M68K_EMUL_OP_CLKNOMEM);
	*wp = htons(0x4ed5);			// jmp	(a5)

	// Patch BootGlobs
	wp = (uint16 *)(ROMBaseHost + 0x10e);
	*wp++ = htons(M68K_EMUL_OP_PATCH_BOOT_GLOBS);
	*wp = htons(M68K_NOP);

	// Don't init SCC
	static const uint8 init_scc_dat[] = {0x08, 0x38, 0x00, 0x01, 0x0d, 0xd1, 0x67, 0x04};
	if ((base = find_rom_data(0xa00, 0xa80, init_scc_dat, sizeof(init_scc_dat))) == 0) return false;
	D(bug("init_scc %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp = htons(M68K_RTS);

	// Don't access 0x50f1a101
	wp = (uint16 *)(ROMBaseHost + 0x4232);
	if (ntohs(wp[1]) == 0x50f1 && ntohs(wp[2]) == 0xa101) {	// ROM32
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp = htons(M68K_NOP);
	}

	// Don't init IWM
	wp = (uint16 *)(ROMBaseHost + 0x9c0);
	*wp = htons(M68K_RTS);

	// Don't init SCSI
	wp = (uint16 *)(ROMBaseHost + 0x9a0);
	*wp = htons(M68K_RTS);

	// Don't init ASC
	static const uint8 init_asc_dat[] = {0x26, 0x68, 0x00, 0x30, 0x12, 0x00, 0xeb, 0x01};
	base = find_rom_data(0x4000, 0x5000, init_asc_dat, sizeof(init_asc_dat));
	D(bug("init_asc %08lx\n", base));
	if (base) {		// ROM15/22/23/26/27/32
		wp = (uint16 *)(ROMBaseHost + base);
		*wp = htons(0x4ed6);		// jmp	(a6)
	}

	// Don't EnableExtCache
	wp = (uint16 *)(ROMBaseHost + 0x190);
	*wp++ = htons(M68K_NOP);
	*wp = htons(M68K_NOP);

	// Don't DisableIntSources
	wp = (uint16 *)(ROMBaseHost + 0x9f4c);
	*wp = htons(M68K_RTS);

	// Fake CPU speed test (SetupTimeK)
	// *** increased jl : MacsBug uses TimeDBRA for kbd repeat timing
	wp = (uint16 *)(ROMBaseHost + 0x800);
	*wp++ = htons(0x31fc);			// move.w	#xxx,TimeDBRA
	*wp++ = htons(10000);
	*wp++ = htons(0x0d00);
	*wp++ = htons(0x31fc);			// move.w	#xxx,TimeSCCDBRA
	*wp++ = htons(10000);
	*wp++ = htons(0x0d02);
	*wp++ = htons(0x31fc);			// move.w	#xxx,TimeSCSIDBRA
	*wp++ = htons(10000);
	*wp++ = htons(0x0b24);
	*wp++ = htons(0x31fc);			// move.w	#xxx,TimeRAMDBRA
	*wp++ = htons(10000);
	*wp++ = htons(0x0cea);
	*wp = htons(M68K_RTS);

#if REAL_ADDRESSING
	// Move system zone to start of Mac RAM
	wp = (uint16 *)(ROMBaseHost + 0x50a);
	*wp++ = htons(HiWord(RAMBaseMac + 0x2000));
	*wp++ = htons(LoWord(RAMBaseMac + 0x2000));
	*wp++ = htons(HiWord(RAMBaseMac + 0x3800));
	*wp = htons(LoWord(RAMBaseMac + 0x3800));
#endif

#if !ROM_IS_WRITE_PROTECTED
#if defined(USE_SCRATCHMEM_SUBTERFUGE)
	// Set fake handle at 0x0000 to scratch memory area (so broken Mac programs won't write into Mac ROM)
	extern uint8 *ScratchMem;
	const uint32 ScratchMemBase = Host2MacAddr(ScratchMem);
	wp = (uint16 *)(ROMBaseHost + 0xccaa);
	*wp++ = htons(0x203c);			// move.l	#ScratchMem,d0
	*wp++ = htons(ScratchMemBase >> 16);
	*wp = htons(ScratchMemBase);
#else
#error System specific handling for writable ROM is required here
#endif
#endif

#if REAL_ADDRESSING && defined(AMIGA)
	// Don't overwrite SysBase under AmigaOS
	wp = (uint16 *)(ROMBaseHost + 0xccb4);
	*wp++ = htons(M68K_NOP);
	*wp = htons(M68K_NOP);
#endif
	
#if REAL_ADDRESSING && !defined(AMIGA)
	// gb-- Temporary hack to get rid of crashes in Speedometer
	wp = (uint16 *)(ROMBaseHost + 0xdba2);
	if (ntohs(*wp) == 0x662c)		// bne.b	#$2c
		*wp = htons(0x602c);		// bra.b	#$2c
#endif
	
	// Don't write to VIA in InitTimeMgr
	wp = (uint16 *)(ROMBaseHost + 0xb0e2);
	*wp++ = htons(0x4cdf);			// movem.l	(sp)+,d0-d5/a0-a4
	*wp++ = htons(0x1f3f);
	*wp = htons(M68K_RTS);

	// Don't read ModelID from 0x5ffffffc
	static const uint8 model_id_dat[] = {0x20, 0x7c, 0x5f, 0xff, 0xff, 0xfc, 0x72, 0x07, 0xc2, 0x90};
	base = find_rom_data(0x40000, 0x50000, model_id_dat, sizeof(model_id_dat));
	D(bug("model_id %08lx\n", base));
	if (base) {		// ROM20
		wp = (uint16 *)(ROMBaseHost + base + 8);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp = htons(M68K_NOP);
	}

	// Don't read ModelID from 0x5ffffffc
	static const uint8 model_id2_dat[] = {0x45, 0xf9, 0x5f, 0xff, 0xff, 0xfc, 0x20, 0x12};
	base = find_rom_data(0x4000, 0x5000, model_id2_dat, sizeof(model_id2_dat));
	D(bug("model_id2 %08lx\n", base));
	if (base) {		// ROM27/32
		wp = (uint16 *)(ROMBaseHost + base + 6);
		*wp++ = htons(0x7000);	// moveq	#0,d0
		*wp++ = htons(0xb040);	// cmp.w	d0,d0
		*wp = htons(0x4ed6);	// jmp		(a6)
	}

	// Install slot ROM
	if (!InstallSlotROM())
		return false;

	// Don't probe NuBus slots
	static const uint8 nubus_dat[] = {0x45, 0xfa, 0x00, 0x0a, 0x42, 0xa7, 0x10, 0x11};
	base = find_rom_data(0x5000, 0x6000, nubus_dat, sizeof(nubus_dat));
	D(bug("nubus %08lx\n", base));
	if (base) {		// ROM10/11
		wp = (uint16 *)(ROMBaseHost + base + 6);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp = htons(M68K_NOP);
	}

	// Don't EnableOneSecInts
	static const uint8 lea_dat[] = {0x41, 0xf9};
	if ((base = find_rom_data(0x226, 0x22a, lea_dat, sizeof(lea_dat))) == 0) return false;
	D(bug("enable_one_sec_ints %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp = htons(M68K_NOP);

	// Don't EnableParityPatch/Enable60HzInts
	if ((base = find_rom_data(0x230, 0x234, lea_dat, sizeof(lea_dat))) == 0) {
		wp = (uint16 *)(ROMBaseHost + 0x230);
		if (ntohs(*wp) == 0x6100)	// ROM11
			base = 0x230;
		else
			return false;
	}
	D(bug("enable_60hz_ints %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp = htons(M68K_NOP);

	// Compute boot stack pointer and fix logical/physical RAM size (CompBootStack) (must be done after InitMemMgr!)
	wp = (uint16 *)(ROMBaseHost + 0x490);
	*wp++ = htons(0x2038);	// move.l	$10c,d0
	*wp++ = htons(0x010c);
	*wp++ = htons(0xd0b8);	// add.l	$2a6,d0
	*wp++ = htons(0x02a6);
	*wp++ = htons(0xe288);	// lsr.l	#1,d0
	*wp++ = htons(0x0880);	// bclr		#0,d0
	*wp++ = htons(0x0000);
	*wp++ = htons(0x0440);	// subi.w	#$400,d0
	*wp++ = htons(0x0400);
	*wp++ = htons(0x2040);	// move.l	d0,a0
	*wp++ = htons(M68K_EMUL_OP_FIX_MEMSIZE);
	*wp++ = htons(M68K_RTS);

	static const uint8 fix_memsize2_dat[] = {0x22, 0x30, 0x81, 0xe2, 0x0d, 0xdc, 0xff, 0xba, 0xd2, 0xb0, 0x81, 0xe2, 0x0d, 0xdc, 0xff, 0xec, 0x21, 0xc1, 0x1e, 0xf8};
	base = find_rom_data(0x4c000, 0x4c080, fix_memsize2_dat, sizeof(fix_memsize2_dat));
	D(bug("fix_memsize2 %08lx\n", base));
	if (base) {		// ROM15/22/23/26/27/32
		wp = (uint16 *)(ROMBaseHost + base + 16);
		*wp++ = htons(M68K_NOP);
		*wp = htons(M68K_NOP);
	}

	// Don't open .Sound driver but install our own drivers
	wp = (uint16 *)(ROMBaseHost + 0x1142);
	*wp = htons(M68K_EMUL_OP_INSTALL_DRIVERS);

	// Don't access SonyVars
	wp = (uint16 *)(ROMBaseHost + 0x1144);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	wp += 2;
	*wp = htons(M68K_NOP);

	// Don't write to VIA in InitADB
	wp = (uint16 *)(ROMBaseHost + 0xa8a8);
	if (*wp == 0) {		// ROM22/23/26/27/32
		wp = (uint16 *)(ROMBaseHost + 0xb2c6a);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp = htons(M68K_NOP);
		wp = (uint16 *)(ROMBaseHost + 0xb2d2e);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		wp += 2;
		*wp++ = htons(M68K_NOP);
		*wp = htons(M68K_NOP);
	} else {
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp = htons(M68K_NOP);
		wp = (uint16 *)(ROMBaseHost + 0xa662);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		wp += 2;
		*wp++ = htons(M68K_NOP);
		*wp = htons(M68K_NOP);
	}

	// Don't EnableSlotInts
	if ((base = find_rom_data(0x2ee, 0x2f2, lea_dat, sizeof(lea_dat))) == 0) return false;
	D(bug("enable_slot_ints %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp = htons(M68K_NOP);

	// Don't mangle frame buffer base (GetDevBase)
	wp = (uint16 *)(ROMBaseHost + 0x5b78);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(0x2401);		// move.l	d1,d2
	*wp = htons(0x605e);		// bra		0x40805bde

	// Really don't mangle frame buffer base
	if (ROMSize > 0x80000) {
		static const uint8 frame_base_dat[] = {0x22, 0x78, 0x0d, 0xd8, 0xd3, 0xe9, 0x00, 0x08};
		base = find_rom_data(0x8c000, 0x8d000, frame_base_dat, sizeof(frame_base_dat));
		D(bug("frame_base %08lx\n", base));
		if (base) {		// ROM22/23/26/27/32
			wp = (uint16 *)(ROMBaseHost + base);
			*wp++ = htons(0x2401);	// move.l	d1,d2
			*wp = htons(M68K_RTS);
		}
	}

	// Don't write to VIA2
	static const uint8 via2_dat[] = {0x20, 0x78, 0x0c, 0xec, 0x11, 0x7c, 0x00, 0x90};
	if ((base = find_rom_data(0xa000, 0xa400, via2_dat, sizeof(via2_dat))) == 0) return false;
	D(bug("via2 %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base + 4);
	*wp = htons(M68K_RTS);

	// Don't write to VIA2, even on ROM20
	static const uint8 via2b_dat[] = {0x20, 0x78, 0x0c, 0xec, 0x11, 0x7c, 0x00, 0x90, 0x00, 0x13, 0x4e, 0x75};
	base = find_rom_data(0x40000, 0x44000, via2b_dat, sizeof(via2b_dat));
	D(bug("via2b %08lx\n", base));
	if (base) {		// ROM19/20
		wp = (uint16 *)(ROMBaseHost + base + 4);
		*wp = htons(M68K_RTS);
	}

	// Don't use PTEST instruction on 68040/060
	if (ROMSize > 0x80000) {

		// BlockMove()
		static const uint8 bmove_dat[] = {0x20, 0x5f, 0x22, 0x5f, 0x0c, 0x38, 0x00, 0x04, 0x01, 0x2f};
		base = find_rom_data(0x87000, 0x87800, bmove_dat, sizeof(bmove_dat));
		D(bug("block_move %08lx\n", base));
		if (base) {		// ROM15/22/23/26/27/32
			wp = (uint16 *)(ROMBaseHost + base + 4);
			*wp++ = htons(M68K_EMUL_OP_BLOCK_MOVE);
			*wp++ = htons(0x7000);
			*wp = htons(M68K_RTS);
		}

		// SANE
		static const uint8 ptest2_dat[] = {0x0c, 0x38, 0x00, 0x04, 0x01, 0x2f, 0x6d, 0x54, 0x48, 0xe7, 0xf8, 0x60};
		base = find_rom_data(0, ROMSize, ptest2_dat, sizeof(ptest2_dat));
		D(bug("ptest2 %08lx\n", base));
		if (base) {		// ROM15/20/22/23/26/27/32
			wp = (uint16 *)(ROMBaseHost + base + 8);
			*wp++ = htons(M68K_NOP);
			*wp++ = htons(0xf4f8);		// cpusha	dc/ic
			*wp++ = htons(M68K_NOP);
			*wp++ = htons(0x7000);		// moveq	#0,d0
			*wp = htons(M68K_RTS);
		}
	}

	// Don't set MemoryDispatch() to unimplemented trap
	static const uint8 memdisp_dat[] = {0x30, 0x3c, 0xa8, 0x9f, 0xa7, 0x46, 0x30, 0x3c, 0xa0, 0x5c, 0xa2, 0x47};
	base = find_rom_data(0x4f100, 0x4f180, memdisp_dat, sizeof(memdisp_dat));
	D(bug("memdisp %08lx\n", base));
	if (base) {	// ROM15/22/23/26/27/32
		wp = (uint16 *)(ROMBaseHost + base + 10);
		*wp = htons(M68K_NOP);
	}

	// Patch .EDisk driver (don't scan for EDisks in the area ROMBase..0xe00000)
	uint32 edisk_offset = find_rom_resource(FOURCC('D','R','V','R'), 51);
	if (edisk_offset) {
		static const uint8 edisk_dat[] = {0xd5, 0xfc, 0x00, 0x01, 0x00, 0x00, 0xb5, 0xfc, 0x00, 0xe0, 0x00, 0x00};
		base = find_rom_data(edisk_offset, edisk_offset + 0x10000, edisk_dat, sizeof(edisk_dat));
		D(bug("edisk %08lx\n", base));
		if (base) {
			wp = (uint16 *)(ROMBaseHost + base + 8);
			*wp++ = 0;
			*wp = 0;
		}
	}

	// Replace .Sony driver
	sony_offset = find_rom_resource(FOURCC('D','R','V','R'), 4);
	D(bug("sony %08lx\n", sony_offset));
	memcpy(ROMBaseHost + sony_offset, sony_driver, sizeof(sony_driver));

	// Install .Disk and .AppleCD drivers
	memcpy(ROMBaseHost + sony_offset + 0x100, disk_driver, sizeof(disk_driver));
	memcpy(ROMBaseHost + sony_offset + 0x200, cdrom_driver, sizeof(cdrom_driver));

	// Copy icons to ROM
	SonyDiskIconAddr = ROMBaseMac + sony_offset + 0x400;
	memcpy(ROMBaseHost + sony_offset + 0x400, SonyDiskIcon, sizeof(SonyDiskIcon));
	SonyDriveIconAddr = ROMBaseMac + sony_offset + 0x600;
	memcpy(ROMBaseHost + sony_offset + 0x600, SonyDriveIcon, sizeof(SonyDriveIcon));
	DiskIconAddr = ROMBaseMac + sony_offset + 0x800;
	memcpy(ROMBaseHost + sony_offset + 0x800, DiskIcon, sizeof(DiskIcon));
	CDROMIconAddr = ROMBaseMac + sony_offset + 0xa00;
	memcpy(ROMBaseHost + sony_offset + 0xa00, CDROMIcon, sizeof(CDROMIcon));

	// Install SERD patch and serial drivers
	serd_offset = find_rom_resource(FOURCC('S','E','R','D'), 0);
	D(bug("serd %08lx\n", serd_offset));
	wp = (uint16 *)(ROMBaseHost + serd_offset + 12);
	*wp++ = htons(M68K_EMUL_OP_SERD);
	*wp = htons(M68K_RTS);
	memcpy(ROMBaseHost + serd_offset + 0x100, ain_driver, sizeof(ain_driver));
	memcpy(ROMBaseHost + serd_offset + 0x200, aout_driver, sizeof(aout_driver));
	memcpy(ROMBaseHost + serd_offset + 0x300, bin_driver, sizeof(bin_driver));
	memcpy(ROMBaseHost + serd_offset + 0x400, bout_driver, sizeof(bout_driver));

	// Replace ADBOp()
	memcpy(ROMBaseHost + find_rom_trap(0xa07c), adbop_patch, sizeof(adbop_patch));

	// Replace Time Manager (the Microseconds patch is activated in InstallDrivers())
	wp = (uint16 *)(ROMBaseHost + find_rom_trap(0xa058));
	*wp++ = htons(M68K_EMUL_OP_INSTIME);
	*wp = htons(M68K_RTS);
	wp = (uint16 *)(ROMBaseHost + find_rom_trap(0xa059));
	*wp++ = htons(0x40e7);		// move	sr,-(sp)
	*wp++ = htons(0x007c);		// ori	#$0700,sr
	*wp++ = htons(0x0700);
	*wp++ = htons(M68K_EMUL_OP_RMVTIME);
	*wp++ = htons(0x46df);		// move	(sp)+,sr
	*wp = htons(M68K_RTS);
	wp = (uint16 *)(ROMBaseHost + find_rom_trap(0xa05a));
	*wp++ = htons(0x40e7);		// move	sr,-(sp)
	*wp++ = htons(0x007c);		// ori	#$0700,sr
	*wp++ = htons(0x0700);
	*wp++ = htons(M68K_EMUL_OP_PRIMETIME);
	*wp++ = htons(0x46df);		// move	(sp)+,sr
	*wp++ = htons(M68K_RTS);
	microseconds_offset = (uint8 *)wp - ROMBaseHost;
	*wp++ = htons(M68K_EMUL_OP_MICROSECONDS);
	*wp++ = htons(M68K_RTS);

	// Replace DebugUtil
	debugutil_offset = (uint8 *)wp - ROMBaseHost;
	*wp++ = htons(M68K_EMUL_OP_DEBUGUTIL);
	*wp = htons(M68K_RTS);

	// Replace SCSIDispatch()
	wp = (uint16 *)(ROMBaseHost + find_rom_trap(0xa815));
	*wp++ = htons(M68K_EMUL_OP_SCSI_DISPATCH);
	*wp++ = htons(0x2e49);		// move.l	a1,a7
	*wp = htons(M68K_JMP_A0);

	// Modify vCheckLoad() so we can patch resources
	wp = (uint16 *)(ROMBaseHost + 0x1b8f4);
	*wp++ = htons(M68K_JMP);
	*wp++ = htons((ROMBaseMac + sony_offset + 0x300) >> 16);
	*wp = htons((ROMBaseMac + sony_offset + 0x300) & 0xffff);
	wp = (uint16 *)(ROMBaseHost + sony_offset + 0x300);
	*wp++ = htons(0x2f03);		// move.l	d3,-(sp) (save type)
	*wp++ = htons(0x2078);		// move.l	$07f0,a0
	*wp++ = htons(0x07f0);
	*wp++ = htons(M68K_JSR_A0);
	*wp++ = htons(0x221f);		// move.l	(sp)+,d1 (restore type)
	*wp++ = htons(M68K_EMUL_OP_CHECKLOAD);
	*wp = htons(M68K_RTS);

	// Patch PowerOff()
	wp = (uint16 *)(ROMBaseHost + find_rom_trap(0xa05b));	// PowerOff()
	*wp = htons(M68K_EMUL_OP_SHUTDOWN);

	// Install PutScrap() patch for clipboard data exchange (the patch is activated by EMUL_OP_INSTALL_DRIVERS)
	PutScrapPatch = ROMBaseMac + sony_offset + 0xc00;
	base = ROMBaseMac + find_rom_trap(0xa9fe);
	wp = (uint16 *)(ROMBaseHost + sony_offset + 0xc00);
	*wp++ = htons(M68K_EMUL_OP_PUT_SCRAP);
	*wp++ = htons(M68K_JMP);
	*wp++ = htons(base >> 16);
	*wp = htons(base & 0xffff);

	// Install GetScrap() patch for clipboard data exchange (the patch is activated by EMUL_OP_INSTALL_DRIVERS)
	GetScrapPatch = ROMBaseMac + sony_offset + 0xd00;
	base = ROMBaseMac + find_rom_trap(0xa9fd);
	wp = (uint16 *)(ROMBaseHost + sony_offset + 0xd00);
	*wp++ = htons(M68K_EMUL_OP_GET_SCRAP);
	*wp++ = htons(M68K_JMP);
	*wp++ = htons(base >> 16);
	*wp = htons(base & 0xffff);

	// Look for double PACK 4 resources
	if ((base = find_rom_resource(FOURCC('P','A','C','K'), 4)) == 0) return false;
	if ((base = find_rom_resource(FOURCC('P','A','C','K'), 4, true)) == 0 && FPUType == 0)
		printf("WARNING: This ROM seems to require an FPU\n");

	// Patch VIA interrupt handler
	wp = (uint16 *)(ROMBaseHost + 0x9bc4);	// Level 1 handler
	*wp++ = htons(0x7002);		// moveq	#2,d0 (always 60Hz interrupt)
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp = htons(M68K_NOP);

	wp = (uint16 *)(ROMBaseHost + 0xa296);	// 60Hz handler (handles everything)
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_EMUL_OP_IRQ);
	*wp++ = htons(0x4a80);		// tst.l	d0
	*wp = htons(0x67f4);		// beq		0x4080a294
	return true;
}

bool PatchROM(void)
{
	// Print some information about the ROM
	if (PrintROMInfo)
		print_rom_info();

	// Patch ROM depending on version
	switch (ROMVersion) {
		case ROM_VERSION_CLASSIC:
			if (!patch_rom_classic())
				return false;
			break;
		case ROM_VERSION_32:
			if (!patch_rom_32())
				return false;
			break;
		default:
			return false;
	}

	// Install breakpoint
	if (ROMBreakpoint) {
#if ENABLE_MON
		mon_add_break_point(ROMBaseMac + ROMBreakpoint);
		printf("ROM start address at %08x\n", ROMBaseMac);
		printf("Set ROM break point at %08x\n", ROMBaseMac + ROMBreakpoint);
#else
		uint16 *wp = (uint16 *)(ROMBaseHost + ROMBreakpoint);
		*wp = htons(M68K_EMUL_BREAK);
#endif
	}

	// Clear caches as we loaded and patched code
	FlushCodeCache(ROMBaseHost, ROMSize);
	return true;
}
