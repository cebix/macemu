/*
 *  rom_patches.cpp - ROM patches
 *
 *  SheepShaver (C) 1997-2008 Christian Bauer and Marc Hellwig
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
 * TODO:
 *  IRQ_NEST must be handled atomically
 *  Don't use r1 in extra routines
 */

#include <string.h>

#include "sysdeps.h"
#include "rom_patches.h"
#include "main.h"
#include "prefs.h"
#include "cpu_emulation.h"
#include "emul_op.h"
#include "xlowmem.h"
#include "sony.h"
#include "disk.h"
#include "cdrom.h"
#include "audio.h"
#include "audio_defs.h"
#include "serial.h"
#include "macos_util.h"
#include "thunks.h"

#define DEBUG 0
#include "debug.h"


// 68k breakpoint address
//#define M68K_BREAK_POINT 0x29e0		// BootMe
//#define M68K_BREAK_POINT 0x2a1e		// Boot block code returned
//#define M68K_BREAK_POINT 0x3150		// CritError
//#define M68K_BREAK_POINT 0x187ce		// Unimplemented trap

// PowerPC breakpoint address
//#define POWERPC_BREAK_POINT 0x36e6c0	// 68k emulator start

#define DISABLE_SCSI 1


// Other ROM addresses
const uint32 CHECK_LOAD_PATCH_SPACE = 0x2fcf00;
const uint32 ZERO_SCRAP_PATCH_SPACE = 0x2fcf80;
const uint32 PUT_SCRAP_PATCH_SPACE = 0x2fcfc0;
const uint32 GET_SCRAP_PATCH_SPACE = 0x2fd100;
const uint32 ADDR_MAP_PATCH_SPACE = 0x2fd140;

// Global variables
int ROMType;				// ROM type
static uint32 sony_offset;	// Offset of .Sony driver resource

// Prototypes
static bool patch_nanokernel_boot(void);
static bool patch_68k_emul(void);
static bool patch_nanokernel(void);
static bool patch_68k(void);


// Decode LZSS data
static void decode_lzss(const uint8 *src, uint8 *dest, int size)
{
	char dict[0x1000];
	int run_mask = 0, dict_idx = 0xfee;
	for (;;) {
		if (run_mask < 0x100) {
			// Start new run
			if (--size < 0)
				break;
			run_mask = *src++ | 0xff00;
		}
		bool bit = run_mask & 1;
		run_mask >>= 1;
		if (bit) {
			// Verbatim copy
			if (--size < 0)
				break;
			int c = *src++;
			dict[dict_idx++] = c;
			*dest++ = c;
			dict_idx &= 0xfff;
		} else {
			// Copy from dictionary
			if (--size < 0)
				break;
			int idx = *src++;
			if (--size < 0)
				break;
			int cnt = *src++;
			idx |= (cnt << 4) & 0xf00;
			cnt = (cnt & 0x0f) + 3;
			while (cnt--) {
				char c = dict[idx++];
				dict[dict_idx++] = c;
				*dest++ = c;
				idx &= 0xfff;
				dict_idx &= 0xfff;
			}
		}
	}
}

// Decode parcels of ROM image (MacOS 9.X and even earlier)
void decode_parcels(const uint8 *src, uint8 *dest, int size)
{
	uint32 parcel_offset = 0x14;
	D(bug("Offset   Type Name\n"));
	while (parcel_offset != 0) {
		const uint32 *parcel_data = (uint32 *)(src + parcel_offset);
		uint32 next_offset = ntohl(parcel_data[0]);
		uint32 parcel_type = ntohl(parcel_data[1]);
		D(bug("%08x %c%c%c%c %s\n", parcel_offset,
			  (parcel_type >> 24) & 0xff, (parcel_type >> 16) & 0xff,
			  (parcel_type >> 8) & 0xff, parcel_type & 0xff, &parcel_data[6]));
		if (parcel_type == FOURCC('r','o','m',' ')) {
			uint32 lzss_offset  = ntohl(parcel_data[2]);
			uint32 lzss_size = ((uintptr)src + next_offset) - ((uintptr)parcel_data + lzss_offset);
			decode_lzss((uint8 *)parcel_data + lzss_offset, dest, lzss_size);
		}
		parcel_offset = next_offset;
	}
}


/*
 *  Decode ROM image, 4 MB plain images or NewWorld images
 */

bool DecodeROM(uint8 *data, uint32 size)
{
	if (size == ROM_SIZE) {
		// Plain ROM image
		memcpy(ROMBaseHost, data, ROM_SIZE);
		return true;
	}
	else if (strncmp((char *)data, "<CHRP-BOOT>", 11) == 0) {
		// CHRP compressed ROM image
		uint32 image_offset, image_size;
		bool decode_info_ok = false;
		
		char *s = strstr((char *)data, "constant lzss-offset");
		if (s != NULL) {
			// Probably a plain LZSS compressed ROM image
			if (sscanf(s - 7, "%06x", &image_offset) == 1) {
				s = strstr((char *)data, "constant lzss-size");
				if (s != NULL && (sscanf(s - 7, "%06x", &image_size) == 1))
					decode_info_ok = true;
			}
		}
		else {
			// Probably a MacOS 9.2.x ROM image
			s = strstr((char *)data, "constant parcels-offset");
			if (s != NULL) {
				if (sscanf(s - 7, "%06x", &image_offset) == 1) {
					s = strstr((char *)data, "constant parcels-size");
					if (s != NULL && (sscanf(s - 7, "%06x", &image_size) == 1))
						decode_info_ok = true;
				}
			}
		}
		
		// No valid information to decode the ROM found?
		if (!decode_info_ok)
			return false;
		
		// Check signature, this could be a parcels-based ROM image
		uint32 rom_signature = ntohl(*(uint32 *)(data + image_offset));
		if (rom_signature == FOURCC('p','r','c','l')) {
			D(bug("Offset of parcels data: %08x\n", image_offset));
			D(bug("Size of parcels data: %08x\n", image_size));
			decode_parcels(data + image_offset, ROMBaseHost, image_size);
		}
		else {
			D(bug("Offset of compressed data: %08x\n", image_offset));
			D(bug("Size of compressed data: %08x\n", image_size));
			decode_lzss(data + image_offset, ROMBaseHost, image_size);
		}
		return true;
	}
	return false;
}


/*
 *  Search ROM for byte string, return ROM offset (or 0)
 */

static uint32 find_rom_data(uint32 start, uint32 end, const uint8 *data, uint32 data_len)
{
	uint32 ofs = start;
	while (ofs < end) {
		if (!memcmp(ROMBaseHost + ofs, data, data_len))
			return ofs;
		ofs++;
	}
	return 0;
}


/*
 *  Search ROM resource by type/ID, return ROM offset of resource data
 */

static uint32 rsrc_ptr = 0;

// id = 4711 means "find any ID"
static uint32 find_rom_resource(uint32 s_type, int16 s_id = 4711, bool cont = false)
{
	uint32 lp = ROMBase + 0x1a;
	uint32 x = ReadMacInt32(lp);
	uint32 header_size = ReadMacInt8(ROMBase + x + 5);

	if (!cont)
		rsrc_ptr = x;
	else if (rsrc_ptr == 0)
		return 0;

	for (;;) {
		lp = ROMBase + rsrc_ptr;
		rsrc_ptr = ReadMacInt32(lp);
		if (rsrc_ptr == 0)
			break;

		rsrc_ptr += header_size;

		lp = ROMBase + rsrc_ptr + 4;
		uint32 data = ReadMacInt32(lp);
		uint32 type = ReadMacInt32(lp + 4);
		int16 id = ReadMacInt16(lp + 8);
		if (type == s_type && (id == s_id || s_id == 4711))
			return data;
	}
	return 0;
}


/*
 *  Search offset of A-Trap routine in ROM
 */

static uint32 find_rom_trap(uint16 trap)
{
	uint32 lp = ROMBase + ReadMacInt32(ROMBase + 0x22);

	if (trap > 0xa800)
		return ReadMacInt32(lp + 4 * (trap & 0x3ff));
	else
		return ReadMacInt32(lp + 4 * ((trap & 0xff) + 0x400));
}


/*
 *  Return target of branch instruction specified at ADDR, or 0 if
 *  there is no such instruction
 */

static uint32 rom_powerpc_branch_target(uint32 addr)
{
	uint32 opcode = ntohl(*(uint32 *)(ROMBaseHost + addr));
	uint32 primop = opcode >> 26;
	uint32 target = 0;

	if (primop == 18) {			// Branch
		target = opcode & 0x3fffffc;
		if (target & 0x2000000)
			target |= 0xfc000000;
		if ((opcode & 2) == 0)
			target += addr;
	}
	else if (primop == 16) {	// Branch Conditional
		target = (int32)(int16)(opcode & 0xfffc);
		if ((opcode & 2) == 0)
			target += addr;
	}
	return target;
}


/*
 *  Search ROM for instruction branching to target address, return 0 if none found
 */

static uint32 find_rom_powerpc_branch(uint32 start, uint32 end, uint32 target)
{
	for (uint32 addr = start; addr < end; addr += 4) {
		if (rom_powerpc_branch_target(addr) == target)
			return addr;
	}
	return 0;
}


/*
 *  Check that requested ROM patch space is really available
 */

static bool check_rom_patch_space(uint32 base, uint32 size)
{
	size = (size + 3) & -4;
	for (int i = 0; i < size; i += 4) {
		uint32 x = ntohl(*(uint32 *)(ROMBaseHost + base + i));
		if (x != 0x6b636b63 && x != 0)
			return false;
	}
	return true;
}


/*
 *  List of audio sifters installed in ROM and System file
 */

struct sift_entry {
	uint32 type;
	int16 id;
};
static sift_entry sifter_list[32];
static int num_sifters;

void AddSifter(uint32 type, int16 id)
{
	if (FindSifter(type, id))
		return;
	D(bug(" adding sifter type %c%c%c%c (%08x), id %d\n", type >> 24, (type >> 16) & 0xff, (type >> 8) & 0xff, type & 0xff, type, id));
	sifter_list[num_sifters].type = type;
	sifter_list[num_sifters].id = id;
	num_sifters++;
}

bool FindSifter(uint32 type, int16 id)
{
	for (int i=0; i<num_sifters; i++) {
		if (sifter_list[i].type == type && sifter_list[i].id == id)
			return true;
	}
	return false;
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

static uint32 long_ptr;

static void SetLongBase(uint32 addr)
{
	long_ptr = addr;
}

static void Long(uint32 value)
{
	WriteMacInt32(long_ptr, value);
	long_ptr += 4;
}

static void gen_ain_driver(uintptr addr)
{
	SetLongBase(addr);

	// .AIn driver header
	Long(0x4d000000); Long(0x00000000);
	Long(0x00200040); Long(0x00600080);
	Long(0x00a0042e); Long(0x41496e00);
	Long(0x00000000); Long(0x00000000);
	Long(0xaafe0700); Long(0x00000000);
	Long(0x00000000); Long(0x00179822);
	Long(0x00010004); Long(NativeTVECT(NATIVE_SERIAL_NOTHING));
	Long(0x00000000); Long(0x00000000);
	Long(0xaafe0700); Long(0x00000000);
	Long(0x00000000); Long(0x00179822);
	Long(0x00010004); Long(NativeTVECT(NATIVE_SERIAL_PRIME_IN));
	Long(0x00000000); Long(0x00000000);
	Long(0xaafe0700); Long(0x00000000);
	Long(0x00000000); Long(0x00179822);
	Long(0x00010004); Long(NativeTVECT(NATIVE_SERIAL_CONTROL));
	Long(0x00000000); Long(0x00000000);
	Long(0xaafe0700); Long(0x00000000);
	Long(0x00000000); Long(0x00179822);
	Long(0x00010004); Long(NativeTVECT(NATIVE_SERIAL_STATUS));
	Long(0x00000000); Long(0x00000000);
	Long(0xaafe0700); Long(0x00000000);
	Long(0x00000000); Long(0x00179822);
	Long(0x00010004); Long(NativeTVECT(NATIVE_SERIAL_NOTHING));
	Long(0x00000000); Long(0x00000000);
};

static void gen_aout_driver(uintptr addr)
{
	SetLongBase(addr);

	// .AOut driver header
	Long(0x4d000000); Long(0x00000000);
	Long(0x00200040); Long(0x00600080);
	Long(0x00a0052e); Long(0x414f7574);
	Long(0x00000000); Long(0x00000000);
	Long(0xaafe0700); Long(0x00000000);
	Long(0x00000000); Long(0x00179822);
	Long(0x00010004); Long(NativeTVECT(NATIVE_SERIAL_OPEN));
	Long(0x00000000); Long(0x00000000);
	Long(0xaafe0700); Long(0x00000000);
	Long(0x00000000); Long(0x00179822);
	Long(0x00010004); Long(NativeTVECT(NATIVE_SERIAL_PRIME_OUT));
	Long(0x00000000); Long(0x00000000);
	Long(0xaafe0700); Long(0x00000000);
	Long(0x00000000); Long(0x00179822);
	Long(0x00010004); Long(NativeTVECT(NATIVE_SERIAL_CONTROL));
	Long(0x00000000); Long(0x00000000);
	Long(0xaafe0700); Long(0x00000000);
	Long(0x00000000); Long(0x00179822);
	Long(0x00010004); Long(NativeTVECT(NATIVE_SERIAL_STATUS));
	Long(0x00000000); Long(0x00000000);
	Long(0xaafe0700); Long(0x00000000);
	Long(0x00000000); Long(0x00179822);
	Long(0x00010004); Long(NativeTVECT(NATIVE_SERIAL_CLOSE));
	Long(0x00000000); Long(0x00000000);
};

static void gen_bin_driver(uintptr addr)
{
	SetLongBase(addr);

	// .BIn driver header
	Long(0x4d000000); Long(0x00000000);
	Long(0x00200040); Long(0x00600080);
	Long(0x00a0042e); Long(0x42496e00);
	Long(0x00000000); Long(0x00000000);
	Long(0xaafe0700); Long(0x00000000);
	Long(0x00000000); Long(0x00179822);
	Long(0x00010004); Long(NativeTVECT(NATIVE_SERIAL_NOTHING));
	Long(0x00000000); Long(0x00000000);
	Long(0xaafe0700); Long(0x00000000);
	Long(0x00000000); Long(0x00179822);
	Long(0x00010004); Long(NativeTVECT(NATIVE_SERIAL_PRIME_IN));
	Long(0x00000000); Long(0x00000000);
	Long(0xaafe0700); Long(0x00000000);
	Long(0x00000000); Long(0x00179822);
	Long(0x00010004); Long(NativeTVECT(NATIVE_SERIAL_CONTROL));
	Long(0x00000000); Long(0x00000000);
	Long(0xaafe0700); Long(0x00000000);
	Long(0x00000000); Long(0x00179822);
	Long(0x00010004); Long(NativeTVECT(NATIVE_SERIAL_STATUS));
	Long(0x00000000); Long(0x00000000);
	Long(0xaafe0700); Long(0x00000000);
	Long(0x00000000); Long(0x00179822);
	Long(0x00010004); Long(NativeTVECT(NATIVE_SERIAL_NOTHING));
	Long(0x00000000); Long(0x00000000);
};

static void gen_bout_driver(uintptr addr)
{
	SetLongBase(addr);

	// .BOut driver header
	Long(0x4d000000); Long(0x00000000);
	Long(0x00200040); Long(0x00600080);
	Long(0x00a0052e); Long(0x424f7574);
	Long(0x00000000); Long(0x00000000);
	Long(0xaafe0700); Long(0x00000000);
	Long(0x00000000); Long(0x00179822);
	Long(0x00010004); Long(NativeTVECT(NATIVE_SERIAL_OPEN));
	Long(0x00000000); Long(0x00000000);
	Long(0xaafe0700); Long(0x00000000);
	Long(0x00000000); Long(0x00179822);
	Long(0x00010004); Long(NativeTVECT(NATIVE_SERIAL_PRIME_OUT));
	Long(0x00000000); Long(0x00000000);
	Long(0xaafe0700); Long(0x00000000);
	Long(0x00000000); Long(0x00179822);
	Long(0x00010004); Long(NativeTVECT(NATIVE_SERIAL_CONTROL));
	Long(0x00000000); Long(0x00000000);
	Long(0xaafe0700); Long(0x00000000);
	Long(0x00000000); Long(0x00179822);
	Long(0x00010004); Long(NativeTVECT(NATIVE_SERIAL_STATUS));
	Long(0x00000000); Long(0x00000000);
	Long(0xaafe0700); Long(0x00000000);
	Long(0x00000000); Long(0x00179822);
	Long(0x00010004); Long(NativeTVECT(NATIVE_SERIAL_CLOSE));
	Long(0x00000000); Long(0x00000000);
};

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
	0x70, 0xff,				//1	moveq	#-1,d0
	0x4c, 0xdf, 0x0f, 0x0e,	//2	movem.l	(sp)+,d1-d3/a0-a3
	0x46, 0xdf,				//	move	(sp)+,sr
	0x4e, 0x75				//	rts
};


/*
 *  Install ROM patches (RAMBase and KernelDataAddr must be set)
 */

bool PatchROM(void)
{
	// Print ROM info
	D(bug("Checksum: %08lx\n", ntohl(*(uint32 *)ROMBaseHost)));
	D(bug("Version: %04x\n", ntohs(*(uint16 *)(ROMBaseHost + 8))));
	D(bug("Sub Version: %04x\n", ntohs(*(uint16 *)(ROMBaseHost + 18))));
	D(bug("Nanokernel ID: %s\n", (char *)ROMBaseHost + 0x30d064));
	D(bug("Resource Map at %08lx\n", ntohl(*(uint32 *)(ROMBaseHost + 26))));
	D(bug("Trap Tables at %08lx\n\n", ntohl(*(uint32 *)(ROMBaseHost + 34))));

	// Detect ROM type
	if (!memcmp(ROMBaseHost + 0x30d064, "Boot TNT", 8))
		ROMType = ROMTYPE_TNT;
	else if (!memcmp(ROMBaseHost + 0x30d064, "Boot Alchemy", 12))
		ROMType = ROMTYPE_ALCHEMY;
	else if (!memcmp(ROMBaseHost + 0x30d064, "Boot Zanzibar", 13))
		ROMType = ROMTYPE_ZANZIBAR;
	else if (!memcmp(ROMBaseHost + 0x30d064, "Boot Gazelle", 12))
		ROMType = ROMTYPE_GAZELLE;
	else if (!memcmp(ROMBaseHost + 0x30d064, "Boot Gossamer", 13))
		ROMType = ROMTYPE_GOSSAMER;
	else if (!memcmp(ROMBaseHost + 0x30d064, "NewWorld", 8))
		ROMType = ROMTYPE_NEWWORLD;
	else
		return false;

	// Check that other ROM addresses point to really free regions
	if (!check_rom_patch_space(CHECK_LOAD_PATCH_SPACE, 0x40))
		return false;
	if (!check_rom_patch_space(ZERO_SCRAP_PATCH_SPACE, 0x40))
		return false;
	if (!check_rom_patch_space(PUT_SCRAP_PATCH_SPACE, 0x40))
		return false;
	if (!check_rom_patch_space(GET_SCRAP_PATCH_SPACE, 0x40))
		return false;
	if (!check_rom_patch_space(ADDR_MAP_PATCH_SPACE - 10 * 4, 0x100))
		return false;

	// Apply patches
	if (!patch_nanokernel_boot()) return false;
	if (!patch_68k_emul()) return false;
	if (!patch_nanokernel()) return false;
	if (!patch_68k()) return false;

#ifdef M68K_BREAK_POINT
	// Install 68k breakpoint
	uint16 *wp = (uint16 *)(ROMBaseHost + M68K_BREAK_POINT);
	*wp++ = htons(M68K_EMUL_BREAK);
	*wp = htons(M68K_EMUL_RETURN);
#endif

#ifdef POWERPC_BREAK_POINT
	// Install PowerPC breakpoint
	uint32 *lp = (uint32 *)(ROMBaseHost + POWERPC_BREAK_POINT);
	*lp = htonl(0);
#endif

	// Copy 68k emulator to 2MB boundary
	memcpy(ROMBaseHost + ROM_SIZE, ROMBaseHost + (ROM_SIZE - 0x100000), 0x100000);
	return true;
}


/*
 *  Nanokernel boot routine patches
 */

static bool patch_nanokernel_boot(void)
{
	uint32 *lp;
	uint32 base, loc;

	// ROM boot structure patches
	lp = (uint32 *)(ROMBaseHost + 0x30d000);
	lp[0x9c >> 2] = htonl(KernelDataAddr);			// LA_InfoRecord
	lp[0xa0 >> 2] = htonl(KernelDataAddr);			// LA_KernelData
	lp[0xa4 >> 2] = htonl(KernelDataAddr + 0x1000);	// LA_EmulatorData
	lp[0xa8 >> 2] = htonl(ROMBase + 0x480000);		// LA_DispatchTable
	lp[0xac >> 2] = htonl(ROMBase + 0x460000);		// LA_EmulatorCode
	lp[0x360 >> 2] = htonl(0);						// Physical RAM base (? on NewWorld ROM, this contains -1)
	lp[0xfd8 >> 2] = htonl(ROMBase + 0x2a);		// 68k reset vector

	// Skip SR/BAT/SDR init
	loc = 0x310000;
	if (ROMType == ROMTYPE_GAZELLE || ROMType == ROMTYPE_GOSSAMER || ROMType == ROMTYPE_NEWWORLD) {
		lp = (uint32 *)(ROMBaseHost + loc);
		*lp++ = htonl(POWERPC_NOP);
		*lp = htonl(0x38000000);
	}
	static const uint8 sr_init_dat[] = {0x35, 0x4a, 0xff, 0xfc, 0x7d, 0x86, 0x50, 0x2e};
	if ((base = find_rom_data(0x3101b0, 0x3105b0, sr_init_dat, sizeof(sr_init_dat))) == 0) return false;
	D(bug("sr_init %08lx\n", base));
	lp = (uint32 *)(ROMBaseHost + loc + 8);
	*lp = htonl(0x48000000 | ((base - loc - 8) & 0x3fffffc));	// b		ROMBase+0x3101b0
	lp = (uint32 *)(ROMBaseHost + base);
	*lp++ = htonl(0x80200000 + XLM_KERNEL_DATA);		// lwz	r1,(pointer to Kernel Data)
	*lp++ = htonl(0x3da0dead);		// lis	r13,0xdead	(start of kernel memory)
	*lp++ = htonl(0x3dc00010);		// lis	r14,0x0010	(size of page table)
	*lp = htonl(0x3de00010);		// lis	r15,0x0010	(size of kernel memory)

	// Don't read PVR
	static const uint8 pvr_read_dat[] = {0x7d, 0x9f, 0x42, 0xa6};
	if ((base = find_rom_data(0x3103b0, 0x3108b0, pvr_read_dat, sizeof(pvr_read_dat))) == 0) return false;
	D(bug("pvr_read %08lx\n", base));
	lp = (uint32 *)(ROMBaseHost + base);
	*lp = htonl(0x81800000 + XLM_PVR);	// lwz	r12,(theoretical PVR)

	// Set CPU specific data (even if ROM doesn't have support for that CPU)
	if (ntohl(lp[6]) != 0x2c0c0001)
		return false;
	uint32 ofs = ntohl(lp[7]) & 0xffff;
	D(bug("ofs %08lx\n", ofs));
	lp[8] = htonl((ntohl(lp[8]) & 0xffff) | 0x48000000);	// beq -> b
	loc = (ntohl(lp[8]) & 0xffff) + (uintptr)(lp+8) - (uintptr)ROMBaseHost;
	D(bug("loc %08lx\n", loc));
	lp = (uint32 *)(ROMBaseHost + ofs + 0x310000);
	switch (PVR >> 16) {
		case 1:		// 601
			lp[0] = htonl(0x1000);		// Page size
			lp[1] = htonl(0x8000);		// Data cache size
			lp[2] = htonl(0x8000);		// Inst cache size
			lp[3] = htonl(0x00200020);	// Coherency block size/Reservation granule size
			lp[4] = htonl(0x00010040);	// Unified caches/Inst cache line size
			lp[5] = htonl(0x00400020);	// Data cache line size/Data cache block size touch
			lp[6] = htonl(0x00200020);	// Inst cache block size/Data cache block size
			lp[7] = htonl(0x00080008);	// Inst cache assoc/Data cache assoc
			lp[8] = htonl(0x01000002);	// TLB total size/TLB assoc
			break;
		case 3:		// 603
			lp[0] = htonl(0x1000);		// Page size
			lp[1] = htonl(0x2000);		// Data cache size
			lp[2] = htonl(0x2000);		// Inst cache size
			lp[3] = htonl(0x00200020);	// Coherency block size/Reservation granule size
			lp[4] = htonl(0x00000020);	// Unified caches/Inst cache line size
			lp[5] = htonl(0x00200020);	// Data cache line size/Data cache block size touch
			lp[6] = htonl(0x00200020);	// Inst cache block size/Data cache block size
			lp[7] = htonl(0x00020002);	// Inst cache assoc/Data cache assoc
			lp[8] = htonl(0x00400002);	// TLB total size/TLB assoc
			break;
		case 4:		// 604
			lp[0] = htonl(0x1000);		// Page size
			lp[1] = htonl(0x4000);		// Data cache size
			lp[2] = htonl(0x4000);		// Inst cache size
			lp[3] = htonl(0x00200020);	// Coherency block size/Reservation granule size
			lp[4] = htonl(0x00000020);	// Unified caches/Inst cache line size
			lp[5] = htonl(0x00200020);	// Data cache line size/Data cache block size touch
			lp[6] = htonl(0x00200020);	// Inst cache block size/Data cache block size
			lp[7] = htonl(0x00040004);	// Inst cache assoc/Data cache assoc
			lp[8] = htonl(0x00800002);	// TLB total size/TLB assoc
			break;
//		case 5:		// 740?
		case 6:		// 603e
		case 7:		// 603ev
			lp[0] = htonl(0x1000);		// Page size
			lp[1] = htonl(0x4000);		// Data cache size
			lp[2] = htonl(0x4000);		// Inst cache size
			lp[3] = htonl(0x00200020);	// Coherency block size/Reservation granule size
			lp[4] = htonl(0x00000020);	// Unified caches/Inst cache line size
			lp[5] = htonl(0x00200020);	// Data cache line size/Data cache block size touch
			lp[6] = htonl(0x00200020);	// Inst cache block size/Data cache block size
			lp[7] = htonl(0x00040004);	// Inst cache assoc/Data cache assoc
			lp[8] = htonl(0x00400002);	// TLB total size/TLB assoc
			break;
		case 8:		// 750, 750FX
		case 0x7000:
			lp[0] = htonl(0x1000);		// Page size
			lp[1] = htonl(0x8000);		// Data cache size
			lp[2] = htonl(0x8000);		// Inst cache size
			lp[3] = htonl(0x00200020);	// Coherency block size/Reservation granule size
			lp[4] = htonl(0x00000020);	// Unified caches/Inst cache line size
			lp[5] = htonl(0x00200020);	// Data cache line size/Data cache block size touch
			lp[6] = htonl(0x00200020);	// Inst cache block size/Data cache block size
			lp[7] = htonl(0x00080008);	// Inst cache assoc/Data cache assoc
			lp[8] = htonl(0x00800002);	// TLB total size/TLB assoc
			break;
		case 9:		// 604e
		case 10:	// 604ev5
			lp[0] = htonl(0x1000);		// Page size
			lp[1] = htonl(0x8000);		// Data cache size
			lp[2] = htonl(0x8000);		// Inst cache size
			lp[3] = htonl(0x00200020);	// Coherency block size/Reservation granule size
			lp[4] = htonl(0x00000020);	// Unified caches/Inst cache line size
			lp[5] = htonl(0x00200020);	// Data cache line size/Data cache block size touch
			lp[6] = htonl(0x00200020);	// Inst cache block size/Data cache block size
			lp[7] = htonl(0x00040004);	// Inst cache assoc/Data cache assoc
			lp[8] = htonl(0x00800002);	// TLB total size/TLB assoc
			break;
//		case 11:	// X704?
		case 12:	// 7400, 7410, 7450, 7455, 7457
		case 0x800c:
		case 0x8000:
		case 0x8001:
		case 0x8002:
			lp[0] = htonl(0x1000);		// Page size
			lp[1] = htonl(0x8000);		// Data cache size
			lp[2] = htonl(0x8000);		// Inst cache size
			lp[3] = htonl(0x00200020);	// Coherency block size/Reservation granule size
			lp[4] = htonl(0x00000020);	// Unified caches/Inst cache line size
			lp[5] = htonl(0x00200020);	// Data cache line size/Data cache block size touch
			lp[6] = htonl(0x00200020);	// Inst cache block size/Data cache block size
			lp[7] = htonl(0x00080008);	// Inst cache assoc/Data cache assoc
			lp[8] = htonl(0x00800002);	// TLB total size/TLB assoc
			break;
		case 13:	// ???
			lp[0] = htonl(0x1000);		// Page size
			lp[1] = htonl(0x8000);		// Data cache size
			lp[2] = htonl(0x8000);		// Inst cache size
			lp[3] = htonl(0x00200020);	// Coherency block size/Reservation granule size
			lp[4] = htonl(0x00000020);	// Unified caches/Inst cache line size
			lp[5] = htonl(0x00200020);	// Data cache line size/Data cache block size touch
			lp[6] = htonl(0x00200020);	// Inst cache block size/Data cache block size
			lp[7] = htonl(0x00080008);	// Inst cache assoc/Data cache assoc
			lp[8] = htonl(0x01000004);	// TLB total size/TLB assoc
			break;
//		case 50:	// 821
//		case 80:	// 860
		case 96:	// ???
			lp[0] = htonl(0x1000);		// Page size
			lp[1] = htonl(0x8000);		// Data cache size
			lp[2] = htonl(0x8000);		// Inst cache size
			lp[3] = htonl(0x00200020);	// Coherency block size/Reservation granule size
			lp[4] = htonl(0x00010020);	// Unified caches/Inst cache line size
			lp[5] = htonl(0x00200020);	// Data cache line size/Data cache block size touch
			lp[6] = htonl(0x00200020);	// Inst cache block size/Data cache block size
			lp[7] = htonl(0x00080008);	// Inst cache assoc/Data cache assoc
			lp[8] = htonl(0x00800004);	// TLB total size/TLB assoc
			break;
		case 0x39:	// 970
			lp[0] = htonl(0x1000);		// Page size
			lp[1] = htonl(0x8000);		// Data cache size
			lp[2] = htonl(0x10000);		// Inst cache size
			lp[3] = htonl(0x00200020);	// Coherency block size/Reservation granule size
			lp[4] = htonl(0x00010020);	// Unified caches/Inst cache line size
			lp[5] = htonl(0x00200020);	// Data cache line size/Data cache block size touch
			lp[6] = htonl(0x00800080);	// Inst cache block size/Data cache block size
			lp[7] = htonl(0x00020002);	// Inst cache assoc/Data cache assoc
			lp[8] = htonl(0x02000004);	// TLB total size/TLB assoc
			break;
		default:
			printf("WARNING: Unknown CPU type\n");
			break;
	}

	// Don't set SPRG3, don't test MQ
	static const uint8 sprg3_mq_dat[] = {0x7d, 0x13, 0x43, 0xa6, 0x3d, 0x00, 0x00, 0x04, 0x7d, 0x00, 0x03, 0xa6, 0x39, 0x00, 0x00, 0x00, 0x7d, 0x00, 0x02, 0xa6};
	if ((base = find_rom_data(loc + 0x20, loc + 0x60, sprg3_mq_dat, sizeof(sprg3_mq_dat))) == 0) return false;
	D(bug("sprg3/mq %08lx\n", base));
	lp = (uint32 *)(ROMBaseHost + base);
	lp[0] = htonl(POWERPC_NOP);
	lp[2] = htonl(POWERPC_NOP);
	lp[4] = htonl(POWERPC_NOP);

	// Don't read MSR
	static const uint8 msr_dat[] = {0x7d, 0xc0, 0x00, 0xa6};
	if ((base = find_rom_data(loc + 0x40, loc + 0x80, msr_dat, sizeof(msr_dat))) == 0) return false;
	D(bug("msr %08lx\n", base));
	lp = (uint32 *)(ROMBaseHost + base);
	*lp = htonl(0x39c00000);		// li	r14,0

	// Don't write to DEC
	lp = (uint32 *)(ROMBaseHost + loc + 0x70);
	*lp++ = htonl(POWERPC_NOP);
	loc = (ntohl(lp[0]) & 0xffff) + (uintptr)lp - (uintptr)ROMBaseHost;
	D(bug("loc %08lx\n", loc));

	// Don't set SPRG3
	static const uint8 sprg3_dat[] = {0x39, 0x21, 0x03, 0x60, 0x7d, 0x33, 0x43, 0xa6, 0x39, 0x01, 0x04, 0x20};
	if ((base = find_rom_data(0x310000, 0x314000, sprg3_dat, sizeof(sprg3_dat))) == 0) return false;
	D(bug("sprg3 %08lx\n", base + 4));
	lp = (uint32 *)(ROMBaseHost + base + 4);
	*lp = htonl(POWERPC_NOP);

	// Don't read PVR
	static const uint8 pvr_read2_dat[] = {0x7e, 0xff, 0x42, 0xa6, 0x56, 0xf7, 0x84, 0x3e};
	if ((base = find_rom_data(0x310000, 0x320000, pvr_read2_dat, sizeof(pvr_read2_dat))) == 0) return false;
	D(bug("pvr_read2 %08lx\n", base));
	lp = (uint32 *)(ROMBaseHost + base);
	*lp = htonl(0x82e00000 + XLM_PVR);		// lwz	r23,(theoretical PVR)
	if ((base = find_rom_data(base + 4, 0x320000, pvr_read2_dat, sizeof(pvr_read2_dat))) != 0) {
		D(bug("pvr_read2 %08lx\n", base));
		lp = (uint32 *)(ROMBaseHost + base);
		*lp = htonl(0x82e00000 + XLM_PVR);	// lwz	r23,(theoretical PVR)
	}
	static const uint8 pvr_read3_dat[] = {0x7e, 0x5f, 0x42, 0xa6, 0x56, 0x52, 0x84, 0x3e};
	if ((base = find_rom_data(0x310000, 0x320000, pvr_read3_dat, sizeof(pvr_read3_dat))) != 0) {
		D(bug("pvr_read3 %08lx\n", base));
		lp = (uint32 *)(ROMBaseHost + base);
		*lp = htonl(0x82400000 + XLM_PVR);	// lwz	r18,(theoretical PVR)
	}
	static const uint8 pvr_read4_dat[] = {0x7d, 0x3f, 0x42, 0xa6, 0x55, 0x29, 0x84, 0x3e};
	if ((base = find_rom_data(0x310000, 0x320000, pvr_read4_dat, sizeof(pvr_read4_dat))) != 0) {
		D(bug("pvr_read4 %08lx\n", base));
		lp = (uint32 *)(ROMBaseHost + base);
		*lp = htonl(0x81200000 + XLM_PVR);	// lzw  r9,(theoritical PVR)
	}

	// Don't read SDR1
	static const uint8 sdr1_read_dat[] = {0x7d, 0x19, 0x02, 0xa6, 0x55, 0x16, 0x81, 0xde};
	if ((base = find_rom_data(0x310000, 0x320000, sdr1_read_dat, sizeof(sdr1_read_dat))) == 0) return false;
	D(bug("sdr1_read %08lx\n", base));
	lp = (uint32 *)(ROMBaseHost + base);
	*lp++ = htonl(0x3d00dead);		// lis	r8,0xdead		(pointer to page table)
	*lp++ = htonl(0x3ec0001f);		// lis	r22,0x001f	(size of page table)
	*lp = htonl(POWERPC_NOP);

	// Don't clear page table, don't invalidate TLB
	static const uint8 pgtb_clear_dat[] = {0x36, 0xd6, 0xff, 0xfc, 0x7e, 0xe8, 0xb1, 0x2e, 0x41, 0x81, 0xff, 0xf8};
	if ((base = find_rom_data(0x310000, 0x320000, pgtb_clear_dat, sizeof(pgtb_clear_dat))) == 0) return false;
	D(bug("pgtb_clear %08lx\n", base + 4));
	lp = (uint32 *)(ROMBaseHost + base + 4);
	*lp = htonl(POWERPC_NOP);
	D(bug("tblie %08lx\n", base + 12));
	lp = (uint32 *)(ROMBaseHost + base + 12);
	*lp = htonl(POWERPC_NOP);

	// Don't create RAM descriptor table
	static const uint8 desc_create_dat[] = {0x97, 0xfd, 0x00, 0x04, 0x3b, 0xff, 0x10, 0x00, 0x4b, 0xff, 0xff, 0xdc};
	if ((base = find_rom_data(0x310000, 0x320000, desc_create_dat, sizeof(desc_create_dat))) == 0) return false;
	D(bug("desc_create %08lx\n", base))
	lp = (uint32 *)(ROMBaseHost + base);
	*lp = htonl(POWERPC_NOP);

	// Don't load SRs and BATs
	static const uint8 sr_load[] = {0x7c, 0x00, 0x04, 0xac, 0x83, 0x9d, 0x00, 0x00, 0x93, 0x81, 0x05, 0xe8};
	if ((loc = find_rom_data(0x310000, 0x320000, sr_load, sizeof(sr_load))) == 0) return false;
	static const uint8 sr_load_caller[] = {0x3e, 0xd6, 0xff, 0xff, 0x41, 0x81, 0xff, 0xdc, 0xb2, 0xc8, 0x00, 0x02};
	if ((base = find_rom_data(0x310000, 0x320000, sr_load_caller, sizeof(sr_load_caller))) == 0) return false;
	if ((base = find_rom_powerpc_branch(base + 12, 0x320000, loc)) == 0) return false;
	D(bug("sr_load %08lx, called from %08lx\n", loc, base));
	lp = (uint32 *)(ROMBaseHost + base);
	*lp = htonl(POWERPC_NOP);

	// Don't mess with SRs
	static const uint8 sr_load2_dat[] = {0x83, 0xa1, 0x05, 0xe8, 0x57, 0x7c, 0x3e, 0x78, 0x7f, 0xbd, 0xe0, 0x2e};
	if ((base = find_rom_data(0x310000, 0x320000, sr_load2_dat, sizeof(sr_load2_dat))) == 0) return false;
	D(bug("sr_load2 %08lx\n", base));
	lp = (uint32 *)(ROMBaseHost + base);
	*lp = htonl(POWERPC_BLR);

	// Don't check performance monitor
	static const uint8 pm_check_dat[] = {0x7e, 0x58, 0xeb, 0xa6, 0x7e, 0x53, 0x90, 0xf8, 0x7e, 0x78, 0xea, 0xa6};
	if ((base = find_rom_data(0x310000, 0x320000, pm_check_dat, sizeof(pm_check_dat))) == 0) return false;
	D(bug("pm_check %08lx\n", base));
	lp = (uint32 *)(ROMBaseHost + base);
	
	static const int spr_check_list[] = {
		952 /* mmcr0 */, 953 /* pmc1 */, 954 /* pmc2 */, 955 /* sia */,
		956 /* mmcr1 */, 957 /* pmc3 */, 958 /* pmc4 */, 959 /* sda */
	};

	for (int i = 0; i < sizeof(spr_check_list)/sizeof(spr_check_list[0]); i++) {
		int spr = spr_check_list[i];
		uint32 mtspr = 0x7e4003a6 | ((spr & 0x1f) << 16) | ((spr & 0x3e0) << 6);
		uint32 mfspr = 0x7e6002a6 | ((spr & 0x1f) << 16) | ((spr & 0x3e0) << 6);
		for (int ofs = 0; ofs < 64; ofs++) {
			if (ntohl(lp[ofs]) == mtspr) {
				if (ntohl(lp[ofs + 2]) != mfspr)
					return false;
				D(bug("  SPR%d %08lx\n", spr, base + 4*ofs));
				lp[ofs] = htonl(POWERPC_NOP);
				lp[ofs + 2] = htonl(POWERPC_NOP);
			}
		}
	}

	// Jump to 68k emulator
	static const uint8 jump68k_dat[] = {0x7d, 0x92, 0x43, 0xa6, 0x7d, 0x5a, 0x03, 0xa6, 0x7d, 0x7b, 0x03, 0xa6};
	if ((loc = find_rom_data(0x310000, 0x320000, jump68k_dat, sizeof(jump68k_dat))) == 0) return false;
	static const uint8 jump68k_caller_dat[] = {0x85, 0x13, 0x00, 0x08, 0x56, 0xbf, 0x50, 0x3e, 0x63, 0xff, 0x0c, 0x00};
	if ((base = find_rom_data(0x310000, 0x320000, jump68k_caller_dat, sizeof(jump68k_caller_dat))) == 0) return false;
	if ((base = find_rom_powerpc_branch(base + 12, 0x320000, loc)) == 0) return false;
	D(bug("jump68k %08lx, called from %08lx\n", loc, base));
	lp = (uint32 *)(ROMBaseHost + base);
	*lp++ = htonl(0x80610634);		// lwz	r3,0x0634(r1)	(pointer to Emulator Data)
	*lp++ = htonl(0x8081119c);		// lwz	r4,0x119c(r1)	(pointer to opcode table)
	*lp++ = htonl(0x80011184);		// lwz	r0,0x1184(r1)	(pointer to emulator init routine)
	*lp++ = htonl(0x7c0903a6);		// mtctr	r0
	*lp = htonl(POWERPC_BCTR);
	return true;
}


/*
 *  68k emulator patches
 */

static bool patch_68k_emul(void)
{
	uint32 *lp;
	uint32 base, loc;

	// Overwrite twi instructions
	static const uint8 twi_dat[] = {0x0f, 0xff, 0x00, 0x00, 0x0f, 0xff, 0x00, 0x01, 0x0f, 0xff, 0x00, 0x02};
	if ((base = find_rom_data(0x36e600, 0x36ea00, twi_dat, sizeof(twi_dat))) == 0) return false;
	D(bug("twi %08lx\n", base));
	lp = (uint32 *)(ROMBaseHost + base);
	*lp++ = htonl(0x48000000 + 0x36f900 - base);		// b 0x36f900 (Emulator start)
	*lp++ = htonl(0x48000000 + 0x36fa00 - base - 4);	// b 0x36fa00 (Mixed mode)
	*lp++ = htonl(0x48000000 + 0x36fb00 - base - 8);	// b 0x36fb00 (Reset/FC1E opcode)
	*lp++ = htonl(0x48000000 + 0x36fc00 - base - 12);	// FE0A opcode
	*lp++ = htonl(POWERPC_ILLEGAL);						// Interrupt
	*lp++ = htonl(0x48000000 + 0x36fd00 - base - 20);	// FE0F opcode
	*lp++ = htonl(POWERPC_ILLEGAL);
	*lp++ = htonl(POWERPC_ILLEGAL);
	*lp++ = htonl(POWERPC_ILLEGAL);
	*lp++ = htonl(POWERPC_ILLEGAL);
	*lp++ = htonl(POWERPC_ILLEGAL);
	*lp++ = htonl(POWERPC_ILLEGAL);
	*lp++ = htonl(POWERPC_ILLEGAL);
	*lp++ = htonl(POWERPC_ILLEGAL);
	*lp++ = htonl(POWERPC_ILLEGAL);
	*lp = htonl(POWERPC_ILLEGAL);

#if EMULATED_PPC
	// Install EMUL_RETURN, EXEC_RETURN, EXEC_NATIVE and EMUL_OP opcodes
	lp = (uint32 *)(ROMBaseHost + 0x380000 + (M68K_EMUL_RETURN << 3));
	*lp++ = htonl(POWERPC_EMUL_OP);
	*lp++ = htonl(0x4bf66e80);							// b	0x366084
	*lp++ = htonl(POWERPC_EMUL_OP | 1);
	*lp++ = htonl(0x4bf66e78);							// b	0x366084
	*lp++ = htonl(POWERPC_EMUL_OP | 2);
	*lp++ = htonl(0x4bf66e70);							// b	0x366084
	for (int i=0; i<OP_MAX; i++) {
		*lp++ = htonl(POWERPC_EMUL_OP | (i + 3));
		*lp++ = htonl(0x4bf66e68 - i*8);				// b	0x366084
	}
#else
	// Install EMUL_RETURN, EXEC_RETURN and EMUL_OP opcodes
	lp = (uint32 *)(ROMBaseHost + 0x380000 + (M68K_EMUL_RETURN << 3));
	*lp++ = htonl(0x80000000 + XLM_EMUL_RETURN_PROC);	// lwz	r0,XLM_EMUL_RETURN_PROC
	*lp++ = htonl(0x4bf705fc);							// b	0x36f800
	*lp++ = htonl(0x80000000 + XLM_EXEC_RETURN_PROC);	// lwz	r0,XLM_EXEC_RETURN_PROC
	*lp++ = htonl(0x4bf705f4);							// b	0x36f800
	*lp++ = htonl(0x00dead00);							// Let SheepShaver crash, since
	*lp++ = htonl(0x00beef00);							// no native opcode is available
	for (int i=0; i<OP_MAX; i++) {
		*lp++ = htonl(0x38a00000 + i);				// li	r5,OP_*
		*lp++ = htonl(0x4bf705ec - i*8);			// b	0x36f808
	}

	// Extra routines for EMUL_RETURN/EXEC_RETURN/EMUL_OP
	lp = (uint32 *)(ROMBaseHost + 0x36f800);
	*lp++ = htonl(0x7c0803a6);						// mtlr	r0
	*lp++ = htonl(0x4e800020);						// blr

	*lp++ = htonl(0x80000000 + XLM_EMUL_OP_PROC);	// lwz	r0,XLM_EMUL_OP_PROC
	*lp++ = htonl(0x7c0803a6);						// mtlr	r0
	*lp = htonl(0x4e800020);						// blr
#endif

	// Extra routine for 68k emulator start
	lp = (uint32 *)(ROMBaseHost + 0x36f900);
	*lp++ = htonl(0x7c2903a6);					// mtctr	r1
	*lp++ = htonl(0x80200000 + XLM_IRQ_NEST);	// lwz		r1,XLM_IRQ_NEST
	*lp++ = htonl(0x38210001);					// addi		r1,r1,1
	*lp++ = htonl(0x90200000 + XLM_IRQ_NEST);	// stw		r1,XLM_IRQ_NEST
	*lp++ = htonl(0x80200000 + XLM_KERNEL_DATA);// lwz		r1,XLM_KERNEL_DATA
	*lp++ = htonl(0x90c10018);					// stw		r6,0x18(r1)
	*lp++ = htonl(0x7cc902a6);					// mfctr	r6
	*lp++ = htonl(0x90c10004);					// stw		r6,$0004(r1)
	*lp++ = htonl(0x80c1065c);					// lwz		r6,$065c(r1)
	*lp++ = htonl(0x90e6013c);					// stw		r7,$013c(r6)
	*lp++ = htonl(0x91060144);					// stw		r8,$0144(r6)
	*lp++ = htonl(0x9126014c);					// stw		r9,$014c(r6)
	*lp++ = htonl(0x91460154);					// stw		r10,$0154(r6)
	*lp++ = htonl(0x9166015c);					// stw		r11,$015c(r6)
	*lp++ = htonl(0x91860164);					// stw		r12,$0164(r6)
	*lp++ = htonl(0x91a6016c);					// stw		r13,$016c(r6)
	*lp++ = htonl(0x7da00026);					// mfcr		r13
	*lp++ = htonl(0x80e10660);					// lwz		r7,$0660(r1)
	*lp++ = htonl(0x7d8802a6);					// mflr		r12
	*lp++ = htonl(0x50e74001);					// rlwimi.	r7,r7,8,$80000000
	*lp++ = htonl(0x814105f0);					// lwz		r10,0x05f0(r1)
	*lp++ = htonl(0x7d4803a6);					// mtlr		r10
	*lp++ = htonl(0x7d8a6378);					// mr		r10,r12
	*lp++ = htonl(0x3d600002);					// lis		r11,0x0002
	*lp++ = htonl(0x616bf072);					// ori		r11,r11,0xf072 (MSR)
	*lp++ = htonl(0x50e7deb4);					// rlwimi	r7,r7,27,$00000020
	*lp = htonl(0x4e800020);					// blr

	// Extra routine for Mixed Mode
	lp = (uint32 *)(ROMBaseHost + 0x36fa00);
	*lp++ = htonl(0x7c2903a6);					// mtctr	r1
	*lp++ = htonl(0x80200000 + XLM_IRQ_NEST);	// lwz		r1,XLM_IRQ_NEST
	*lp++ = htonl(0x38210001);					// addi		r1,r1,1
	*lp++ = htonl(0x90200000 + XLM_IRQ_NEST);	// stw		r1,XLM_IRQ_NEST
	*lp++ = htonl(0x80200000 + XLM_KERNEL_DATA);// lwz		r1,XLM_KERNEL_DATA
	*lp++ = htonl(0x90c10018);					// stw		r6,0x18(r1)
	*lp++ = htonl(0x7cc902a6);					// mfctr	r6
	*lp++ = htonl(0x90c10004);					// stw		r6,$0004(r1)
	*lp++ = htonl(0x80c1065c);					// lwz		r6,$065c(r1)
	*lp++ = htonl(0x90e6013c);					// stw		r7,$013c(r6)
	*lp++ = htonl(0x91060144);					// stw		r8,$0144(r6)
	*lp++ = htonl(0x9126014c);					// stw		r9,$014c(r6)
	*lp++ = htonl(0x91460154);					// stw		r10,$0154(r6)
	*lp++ = htonl(0x9166015c);					// stw		r11,$015c(r6)
	*lp++ = htonl(0x91860164);					// stw		r12,$0164(r6)
	*lp++ = htonl(0x91a6016c);					// stw		r13,$016c(r6)
	*lp++ = htonl(0x7da00026);					// mfcr		r13
	*lp++ = htonl(0x80e10660);					// lwz		r7,$0660(r1)
	*lp++ = htonl(0x7d8802a6);					// mflr		r12
	*lp++ = htonl(0x50e74001);					// rlwimi.	r7,r7,8,$80000000
	*lp++ = htonl(0x814105f4);					// lwz		r10,0x05f4(r1)
	*lp++ = htonl(0x7d4803a6);					// mtlr		r10
	*lp++ = htonl(0x7d8a6378);					// mr		r10,r12
	*lp++ = htonl(0x3d600002);					// lis		r11,0x0002
	*lp++ = htonl(0x616bf072);					// ori		r11,r11,0xf072 (MSR)
	*lp++ = htonl(0x50e7deb4);					// rlwimi	r7,r7,27,$00000020
	*lp = htonl(0x4e800020);					// blr

	// Extra routine for Reset/FC1E opcode
	lp = (uint32 *)(ROMBaseHost + 0x36fb00);
	*lp++ = htonl(0x7c2903a6);					// mtctr	r1
	*lp++ = htonl(0x80200000 + XLM_IRQ_NEST);	// lwz		r1,XLM_IRQ_NEST
	*lp++ = htonl(0x38210001);					// addi		r1,r1,1
	*lp++ = htonl(0x90200000 + XLM_IRQ_NEST);	// stw		r1,XLM_IRQ_NEST
	*lp++ = htonl(0x80200000 + XLM_KERNEL_DATA);// lwz		r1,XLM_KERNEL_DATA
	*lp++ = htonl(0x90c10018);					// stw		r6,0x18(r1)
	*lp++ = htonl(0x7cc902a6);					// mfctr	r6
	*lp++ = htonl(0x90c10004);					// stw		r6,$0004(r1)
	*lp++ = htonl(0x80c1065c);					// lwz		r6,$065c(r1)
	*lp++ = htonl(0x90e6013c);					// stw		r7,$013c(r6)
	*lp++ = htonl(0x91060144);					// stw		r8,$0144(r6)
	*lp++ = htonl(0x9126014c);					// stw		r9,$014c(r6)
	*lp++ = htonl(0x91460154);					// stw		r10,$0154(r6)
	*lp++ = htonl(0x9166015c);					// stw		r11,$015c(r6)
	*lp++ = htonl(0x91860164);					// stw		r12,$0164(r6)
	*lp++ = htonl(0x91a6016c);					// stw		r13,$016c(r6)
	*lp++ = htonl(0x7da00026);					// mfcr		r13
	*lp++ = htonl(0x80e10660);					// lwz		r7,$0660(r1)
	*lp++ = htonl(0x7d8802a6);					// mflr		r12
	*lp++ = htonl(0x50e74001);					// rlwimi.	r7,r7,8,$80000000
	*lp++ = htonl(0x814105f8);					// lwz		r10,0x05f8(r1)
	*lp++ = htonl(0x7d4803a6);					// mtlr		r10
	*lp++ = htonl(0x7d8a6378);					// mr		r10,r12
	*lp++ = htonl(0x3d600002);					// lis		r11,0x0002
	*lp++ = htonl(0x616bf072);					// ori		r11,r11,0xf072 (MSR)
	*lp++ = htonl(0x50e7deb4);					// rlwimi	r7,r7,27,$00000020
	*lp = htonl(0x4e800020);					// blr

	// Extra routine for FE0A opcode (QuickDraw 3D needs this)
	lp = (uint32 *)(ROMBaseHost + 0x36fc00);
	*lp++ = htonl(0x7c2903a6);					// mtctr	r1
	*lp++ = htonl(0x80200000 + XLM_IRQ_NEST);	// lwz		r1,XLM_IRQ_NEST
	*lp++ = htonl(0x38210001);					// addi		r1,r1,1
	*lp++ = htonl(0x90200000 + XLM_IRQ_NEST);	// stw		r1,XLM_IRQ_NEST
	*lp++ = htonl(0x80200000 + XLM_KERNEL_DATA);// lwz		r1,XLM_KERNEL_DATA
	*lp++ = htonl(0x90c10018);					// stw		r6,0x18(r1)
	*lp++ = htonl(0x7cc902a6);					// mfctr	r6
	*lp++ = htonl(0x90c10004);					// stw		r6,$0004(r1)
	*lp++ = htonl(0x80c1065c);					// lwz		r6,$065c(r1)
	*lp++ = htonl(0x90e6013c);					// stw		r7,$013c(r6)
	*lp++ = htonl(0x91060144);					// stw		r8,$0144(r6)
	*lp++ = htonl(0x9126014c);					// stw		r9,$014c(r6)
	*lp++ = htonl(0x91460154);					// stw		r10,$0154(r6)
	*lp++ = htonl(0x9166015c);					// stw		r11,$015c(r6)
	*lp++ = htonl(0x91860164);					// stw		r12,$0164(r6)
	*lp++ = htonl(0x91a6016c);					// stw		r13,$016c(r6)
	*lp++ = htonl(0x7da00026);					// mfcr		r13
	*lp++ = htonl(0x80e10660);					// lwz		r7,$0660(r1)
	*lp++ = htonl(0x7d8802a6);					// mflr		r12
	*lp++ = htonl(0x50e74001);					// rlwimi.	r7,r7,8,$80000000
	*lp++ = htonl(0x814105fc);					// lwz		r10,0x05fc(r1)
	*lp++ = htonl(0x7d4803a6);					// mtlr		r10
	*lp++ = htonl(0x7d8a6378);					// mr		r10,r12
	*lp++ = htonl(0x3d600002);					// lis		r11,0x0002
	*lp++ = htonl(0x616bf072);					// ori		r11,r11,0xf072 (MSR)
	*lp++ = htonl(0x50e7deb4);					// rlwimi	r7,r7,27,$00000020
	*lp = htonl(0x4e800020);					// blr

	// Extra routine for FE0F opcode (power management)
	lp = (uint32 *)(ROMBaseHost + 0x36fd00);
	*lp++ = htonl(0x7c2903a6);					// mtctr	r1
	*lp++ = htonl(0x80200000 + XLM_IRQ_NEST);	// lwz		r1,XLM_IRQ_NEST
	*lp++ = htonl(0x38210001);					// addi		r1,r1,1
	*lp++ = htonl(0x90200000 + XLM_IRQ_NEST);	// stw		r1,XLM_IRQ_NEST
	*lp++ = htonl(0x80200000 + XLM_KERNEL_DATA);// lwz		r1,XLM_KERNEL_DATA
	*lp++ = htonl(0x90c10018);					// stw		r6,0x18(r1)
	*lp++ = htonl(0x7cc902a6);					// mfctr	r6
	*lp++ = htonl(0x90c10004);					// stw		r6,$0004(r1)
	*lp++ = htonl(0x80c1065c);					// lwz		r6,$065c(r1)
	*lp++ = htonl(0x90e6013c);					// stw		r7,$013c(r6)
	*lp++ = htonl(0x91060144);					// stw		r8,$0144(r6)
	*lp++ = htonl(0x9126014c);					// stw		r9,$014c(r6)
	*lp++ = htonl(0x91460154);					// stw		r10,$0154(r6)
	*lp++ = htonl(0x9166015c);					// stw		r11,$015c(r6)
	*lp++ = htonl(0x91860164);					// stw		r12,$0164(r6)
	*lp++ = htonl(0x91a6016c);					// stw		r13,$016c(r6)
	*lp++ = htonl(0x7da00026);					// mfcr		r13
	*lp++ = htonl(0x80e10660);					// lwz		r7,$0660(r1)
	*lp++ = htonl(0x7d8802a6);					// mflr		r12
	*lp++ = htonl(0x50e74001);					// rlwimi.	r7,r7,8,$80000000
	*lp++ = htonl(0x81410604);					// lwz		r10,0x0604(r1)
	*lp++ = htonl(0x7d4803a6);					// mtlr		r10
	*lp++ = htonl(0x7d8a6378);					// mr		r10,r12
	*lp++ = htonl(0x3d600002);					// lis		r11,0x0002
	*lp++ = htonl(0x616bf072);					// ori		r11,r11,0xf072 (MSR)
	*lp++ = htonl(0x50e7deb4);					// rlwimi	r7,r7,27,$00000020
	*lp = htonl(0x4e800020);					// blr

	// Patch DR emulator to jump to right address when an interrupt occurs
	lp = (uint32 *)(ROMBaseHost + 0x370000);
	while (lp < (uint32 *)(ROMBaseHost + 0x380000)) {
		if (ntohl(*lp) == 0x4ca80020)		// bclr		5,8
			goto dr_found;
		lp++;
	}
	D(bug("DR emulator patch location not found\n"));
	return false;
dr_found:
	lp++;
	loc = (uintptr)lp - (uintptr)ROMBaseHost;
	if ((base = rom_powerpc_branch_target(loc)) == 0) base = loc;
	static const uint8 dr_ret_dat[] = {0x80, 0xbf, 0x08, 0x14, 0x53, 0x19, 0x4d, 0xac, 0x7c, 0xa8, 0x03, 0xa6};
	if ((base = find_rom_data(base, 0x380000, dr_ret_dat, sizeof(dr_ret_dat))) == 0) return false;
	D(bug("dr_ret %08lx\n", base));
	if (base != loc) {
		// OldWorld ROMs contain an absolute branch
		D(bug(" patching absolute branch at %08x\n", loc));
		*lp = htonl(0x48000000 + 0xf000 - (loc & 0xffff));				// b	DR_CACHE_BASE+0x1f000
		lp = (uint32 *)(ROMBaseHost + 0x37f000);
		*lp++ = htonl(0x3c000000 + ((ROMBase + base) >> 16));			// lis	r0,xxx
		*lp++ = htonl(0x60000000 + ((ROMBase + base) & 0xffff));		// ori	r0,r0,xxx
		*lp++ = htonl(0x7c0803a6);										// mtlr	r0
		*lp = htonl(POWERPC_BLR);										// blr
	}
	return true;
}


/*
 *  Nanokernel patches
 */

static bool patch_nanokernel(void)
{
	uint32 *lp;
	uint32 base, loc;

	// Patch Mixed Mode trap
	static const uint8 virt2phys_dat[] = {0x7d, 0x1b, 0x43, 0x78, 0x3b, 0xa1, 0x03, 0x20};
	if ((base = find_rom_data(0x313000, 0x314000, virt2phys_dat, sizeof(virt2phys_dat))) == 0) return false;
	D(bug("virt2phys %08lx\n", base + 8));
	lp = (uint32 *)(ROMBaseHost + base + 8);	// Don't translate virtual->physical
	lp[0] = htonl(0x7f7fdb78);					// mr		r31,r27
	lp[2] = htonl(POWERPC_NOP);

	static const uint8 ppc_excp_tbl_dat[] = {0x39, 0x01, 0x04, 0x20, 0x7d, 0x13, 0x43, 0xa6};
	if ((base = find_rom_data(0x313000, 0x314000, ppc_excp_tbl_dat, sizeof(ppc_excp_tbl_dat))) == 0) return false;
	D(bug("ppc_excp_tbl %08lx\n", base));
	lp = (uint32 *)(ROMBaseHost + base);		// Don't activate PPC exception table
	*lp++ = htonl(0x39000000 + MODE_NATIVE);	// li	r8,MODE_NATIVE
	*lp = htonl(0x91000000 + XLM_RUN_MODE);		// stw	r8,XLM_RUN_MODE

	static const uint8 save_fpu_dat[] = {0x7d, 0x00, 0x00, 0xa6, 0x61, 0x08, 0x20, 0x00, 0x7d, 0x00, 0x01, 0x24};
	if ((base = find_rom_data(0x310000, 0x314000, save_fpu_dat, sizeof(save_fpu_dat))) == 0) return false;
	D(bug("save_fpu %08lx\n", base));
	lp = (uint32 *)(ROMBaseHost + base);		// Don't modify MSR to turn on FPU
	if (ntohl(lp[4]) != 0x556b04e2) return false;
	loc = base;
#if 1
	// FIXME: is that really intended?
	*lp++ = htonl(POWERPC_NOP);
	lp++;
	*lp++ = htonl(POWERPC_NOP);
	lp++;
	*lp = htonl(POWERPC_NOP);
#else
	lp[0] = htonl(POWERPC_NOP);
	lp[1] = htonl(POWERPC_NOP);
	lp[2] = htonl(POWERPC_NOP);
	lp[3] = htonl(POWERPC_NOP);
#endif

	static const uint8 save_fpu_caller_dat[] = {0x93, 0xa6, 0x01, 0xec, 0x93, 0xc6, 0x01, 0xf4, 0x93, 0xe6, 0x01, 0xfc, 0x40};
	if ((base = find_rom_data(0x310000, 0x314000, save_fpu_caller_dat, sizeof(save_fpu_caller_dat))) == 0) return false;
	D(bug("save_fpu_caller %08lx\n", base + 12));
	if (rom_powerpc_branch_target(base + 12) != loc) return false;
	lp = (uint32 *)(ROMBaseHost + base + 12);	// Always save FPU state
	*lp = htonl(0x48000000 | (ntohl(*lp) & 0xffff));	// bl	0x00312e88

	static const uint8 mdec_dat[] = {0x7f, 0xf6, 0x02, 0xa6, 0x2c, 0x08, 0x00, 0x00, 0x93, 0xe1, 0x06, 0x68, 0x7d, 0x16, 0x03, 0xa6};
	if ((base = find_rom_data(0x310000, 0x314000, mdec_dat, sizeof(mdec_dat))) == 0) return false;
	D(bug("mdec %08lx\n", base));
	lp = (uint32 *)(ROMBaseHost + base);		// Don't modify DEC
	lp[0] = htonl(0x3be00000);					// li	r31,0
#if 1
	lp[3] = htonl(POWERPC_NOP);
	lp[4] = htonl(POWERPC_NOP);
#else
	lp[3] = htonl(0x39000040);					// li	r8,0x40
	lp[4] = htonl(0x990600e4);					// stb	r8,0xe4(r6)
#endif

	static const uint8 restore_fpu_caller_dat[] = {0x81, 0x06, 0x00, 0xf4, 0x81, 0x46, 0x00, 0xfc, 0x7d, 0x09, 0x03, 0xa6, 0x40};
	if ((base = find_rom_data(0x310000, 0x314000, restore_fpu_caller_dat, sizeof(restore_fpu_caller_dat))) == 0) return false;
	D(bug("restore_fpu_caller %08lx\n", base + 12));
	lp = (uint32 *)(ROMBaseHost + base + 12);	// Always restore FPU state
	*lp = htonl(0x48000000 | (ntohl(*lp) & 0xffff));	// bl	0x00312ddc

	static const uint8 m68k_excp_tbl_dat[] = {0x81, 0x21, 0x06, 0x58, 0x39, 0x01, 0x03, 0x60, 0x7d, 0x13, 0x43, 0xa6};
	if ((base = find_rom_data(0x310000, 0x314000, m68k_excp_tbl_dat, sizeof(m68k_excp_tbl_dat))) == 0) return false;
	D(bug("m68k_excp %08lx\n", base + 4));
	lp = (uint32 *)(ROMBaseHost + base + 4);	// Don't activate 68k exception table
	*lp++ = htonl(0x39000000 + MODE_68K);		// li	r8,MODE_68K
	*lp = htonl(0x91000000 + XLM_RUN_MODE);		// stw	r8,XLM_RUN_MODE

	// Patch 68k emulator trap routine
	static const uint8 restore_fpu_caller2_dat[] = {0x81, 0x86, 0x00, 0x8c, 0x80, 0x66, 0x00, 0x94, 0x80, 0x86, 0x00, 0x9c, 0x40};
	if ((base = find_rom_data(0x310000, 0x314000, restore_fpu_caller2_dat, sizeof(restore_fpu_caller2_dat))) == 0) return false;
	D(bug("restore_fpu_caller2 %08lx\n", base + 12));
	loc = rom_powerpc_branch_target(base + 12);
	lp = (uint32 *)(ROMBaseHost + base + 12);	// Always restore FPU state
	*lp = htonl(0x48000000 | (ntohl(*lp) & 0xffff));	// bl	0x00312dd4

	static const uint8 restore_fpu_dat[] = {0x55, 0x68, 0x04, 0xa5, 0x4c, 0x82, 0x00, 0x20, 0x81, 0x06, 0x00, 0xe4};
	if ((base = find_rom_data(0x310000, 0x314000, restore_fpu_dat, sizeof(restore_fpu_dat))) == 0) return false;
	D(bug("restore_fpu %08lx\n", base));
	if (base != loc) return false;
	lp = (uint32 *)(ROMBaseHost + base + 4);	// Don't modify MSR to turn on FPU
	*lp++ = htonl(POWERPC_NOP);
	lp += 2;
	*lp++ = htonl(POWERPC_NOP);
	lp++;
	*lp++ = htonl(POWERPC_NOP);
	*lp++ = htonl(POWERPC_NOP);
	*lp = htonl(POWERPC_NOP);

	// Disable suspend (FE0F opcode)
	// TODO: really suspend SheepShaver?
	static const uint8 suspend_dat[] = {0x7c, 0x88, 0x68, 0x39, 0x41, 0x9d};
	if ((base = find_rom_data(0x315000, 0x316000, suspend_dat, sizeof(suspend_dat))) == 0) return false;
	D(bug("suspend %08lx\n", base));
	lp = (uint32 *)(ROMBaseHost + base + 4);
	*lp = htonl((ntohl(*lp) & 0xffff) | 0x48000000);	// bgt -> b

	// Patch trap return routine
	static const uint8 trap_return_dat[] = {0x80, 0xc1, 0x00, 0x18, 0x80, 0x21, 0x00, 0x04, 0x4c, 0x00, 0x00, 0x64};
	if ((base = find_rom_data(0x312000, 0x320000, trap_return_dat, sizeof(trap_return_dat))) == 0) return false;
	D(bug("trap_return %08lx\n", base + 8));
	lp = (uint32 *)(ROMBaseHost + base + 8);	// Replace rfi
	*lp = htonl(POWERPC_BCTR);

	while (ntohl(*lp) != 0x7d5a03a6) lp--;
	*lp++ = htonl(0x7d4903a6);					// mtctr	r10
	*lp++ = htonl(0x7daff120);					// mtcr	r13
	*lp = htonl(0x48000000 + ((0x318000 - ((uintptr)lp - (uintptr)ROMBaseHost)) & 0x03fffffc));	// b		ROMBase+0x318000
	uint32 npc = (uintptr)(lp + 1) - (uintptr)ROMBaseHost;

	lp = (uint32 *)(ROMBaseHost + 0x318000);
	*lp++ = htonl(0x81400000 + XLM_IRQ_NEST);	// lwz	r10,XLM_IRQ_NEST
	*lp++ = htonl(0x394affff);					// subi	r10,r10,1
	*lp++ = htonl(0x91400000 + XLM_IRQ_NEST);	// stw	r10,XLM_IRQ_NEST
	*lp = htonl(0x48000000 + ((npc - 0x31800c) & 0x03fffffc));	// b		ROMBase+0x312c2c

	// Patch FEOA opcode, selector 0x0A (virtual->physical page index)
	static const uint8 fe0a_0a_dat[] = {0x55, 0x23, 0xa3, 0x3e, 0x4b};
	if ((base = find_rom_data(0x314000, 0x318000, fe0a_0a_dat, sizeof(fe0a_0a_dat))) == 0) return false;
	loc = rom_powerpc_branch_target(base - 8);
	static const uint8 fe0a_dat[] = {0x7e, 0x04, 0x48, 0x40, 0x81, 0xe1, 0x06, 0xb0, 0x54, 0x88, 0x10, 0x3a, 0x40, 0x90};
	if (find_rom_data(loc, 0x318000, fe0a_dat, sizeof(fe0a_dat)) != loc) return false;
	D(bug("fe0a_0a %08lx\n", base - 8));
	lp = (uint32 *)(ROMBaseHost + base - 8);
	*lp++ = htonl(0x7c832378);					// mr	r3,r4
	*lp++ = htonl(POWERPC_NOP);
	*lp = htonl(POWERPC_NOP);

	// Disable FE0A opcode, selector 0x11 (init page tables?)
	static const uint8 fe0a_11_dat[] = {0x56, 0x07, 0x06, 0x74, 0x2c, 0x07, 0x00, 0x60, 0x40};
	if ((base = find_rom_data(0x314000, 0x318000, fe0a_11_dat, sizeof(fe0a_11_dat))) == 0) return false;
	loc = rom_powerpc_branch_target(base - 4);
	if (find_rom_data(0x314000, 0x318000, fe0a_dat, sizeof(fe0a_dat)) != loc) return false;
	D(bug("fe0a_11 %08lx\n", base - 4));
	lp = (uint32 *)(ROMBaseHost + base - 4);
	*lp++ = htonl(POWERPC_NOP);
	*lp++ = htonl(POWERPC_NOP);
	*lp++ = htonl(POWERPC_NOP);
	*lp = htonl(ntohl(*lp) | 0x02800000);		// bf => ba

	// Patch FE0A opcode to fake a page table entry so that V=P for RAM and ROM
	static const uint8 pg_lookup_dat[] = {0x7e, 0x0f, 0x40, 0x6e, 0x81, 0xc1, 0x06, 0xa4, 0x7e, 0x00, 0x71, 0x20};
	if ((base = find_rom_data(0x310000, 0x320000, pg_lookup_dat, sizeof(pg_lookup_dat))) == 0) return false;
	D(bug("fe0a_pgtb_lookup %08lx\n", base - 12));
	lp = (uint32 *)(ROMBaseHost + base - 12);
	if (ntohl(lp[0]) != 0x81e106b0)				// lwz	r15,$06b0(r1)
		return false;
	lp[0] = htonl(0x54906026);					// slwi	r16,r4,12
	lp[3] = htonl(0x62100121);					// ori	r16,r16,0x121

	// Patch FE0A opcode to not write to kernel memory
	static const uint8 krnl_write_dat[] = {0x38, 0xe0, 0x00, 0x01, 0x7e, 0x10, 0x38, 0x78, 0x92, 0x0f, 0x00, 0x00};
	if ((base = find_rom_data(0x310000, 0x320000, krnl_write_dat, sizeof(krnl_write_dat))) == 0) return false;
	D(bug("fe0a_krnl_write %08lx\n", base));
	lp = (uint32 *)(ROMBaseHost + base);
	lp[2] = htonl(POWERPC_NOP);

/*
	// Disable FE0A/FE06 opcodes
	lp = (uint32 *)(ROMBase + 0x3144ac);
	*lp++ = htonl(POWERPC_NOP);
	*lp += 8;
*/
	return true;
}


/*
 *  68k boot routine patches
 */

static bool patch_68k(void)
{
	uint32 *lp;
	uint16 *wp;
	uint8 *bp;
	uint32 base, loc;

	// Remove 68k RESET instruction
	static const uint8 reset_dat[] = {0x4e, 0x70};
	if ((base = find_rom_data(0xc8, 0x120, reset_dat, sizeof(reset_dat))) == 0) return false;
	D(bug("reset %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp = htons(M68K_NOP);

	// Fake reading PowerMac ID (via Universal)
	static const uint8 powermac_id_dat[] = {0x45, 0xf9, 0x5f, 0xff, 0xff, 0xfc, 0x20, 0x12, 0x72, 0x00};
	if ((base = find_rom_data(0xe000, 0x15000, powermac_id_dat, sizeof(powermac_id_dat))) == 0) return false;
	D(bug("powermac_id %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp++ = htons(0x203c);			// move.l	#id,d0
	*wp++ = htons(0);
//	if (ROMType == ROMTYPE_NEWWORLD)
//		*wp++ = htons(0x3035);		// (PowerMac 9500 ID)
//	else
		*wp++ = htons(0x3020);		// (PowerMac 9500 ID)
	*wp++ = htons(0xb040);			// cmp.w	d0,d0
	*wp = htons(0x4ed6);			// jmp	(a6)

	// Patch UniversalInfo
	if (ROMType == ROMTYPE_NEWWORLD) {
		static const uint8 univ_info_dat[] = {0x3f, 0xff, 0x04, 0x00};
		if ((base = find_rom_data(0x14000, 0x18000, univ_info_dat, sizeof(univ_info_dat))) == 0) return false;
		D(bug("universal_info %08lx\n", base));
		lp = (uint32 *)(ROMBaseHost + base - 0x14);
		lp[0x00 >> 2] = htonl(ADDR_MAP_PATCH_SPACE - (base - 0x14));
		lp[0x10 >> 2] = htonl(0xcc003d11);		// Make it like the PowerMac 9500 UniversalInfo
		lp[0x14 >> 2] = htonl(0x3fff0401);
		lp[0x18 >> 2] = htonl(0x0300001c);
		lp[0x1c >> 2] = htonl(0x000108c4);
		lp[0x24 >> 2] = htonl(0xc301bf26);
		lp[0x28 >> 2] = htonl(0x00000861);
		lp[0x58 >> 2] = htonl(0x30200000);
		lp[0x60 >> 2] = htonl(0x0000003d);
	} else if (ROMType == ROMTYPE_ZANZIBAR) {
		base = 0x12b70;
		lp = (uint32 *)(ROMBaseHost + base - 0x14);
		lp[0x00 >> 2] = htonl(ADDR_MAP_PATCH_SPACE - (base - 0x14));
		lp[0x10 >> 2] = htonl(0xcc003d11);		// Make it like the PowerMac 9500 UniversalInfo
		lp[0x14 >> 2] = htonl(0x3fff0401);
		lp[0x18 >> 2] = htonl(0x0300001c);
		lp[0x1c >> 2] = htonl(0x000108c4);
		lp[0x24 >> 2] = htonl(0xc301bf26);
		lp[0x28 >> 2] = htonl(0x00000861);
		lp[0x58 >> 2] = htonl(0x30200000);
		lp[0x60 >> 2] = htonl(0x0000003d);
	} else if (ROMType == ROMTYPE_GOSSAMER) {
		base = 0x12d20;
		lp = (uint32 *)(ROMBaseHost + base - 0x14);
		lp[0x00 >> 2] = htonl(ADDR_MAP_PATCH_SPACE - (base - 0x14));
		lp[0x10 >> 2] = htonl(0xcc003d11);		// Make it like the PowerMac 9500 UniversalInfo
		lp[0x14 >> 2] = htonl(0x3fff0401);
		lp[0x18 >> 2] = htonl(0x0300001c);
		lp[0x1c >> 2] = htonl(0x000108c4);
		lp[0x24 >> 2] = htonl(0xc301bf26);
		lp[0x28 >> 2] = htonl(0x00000861);
		lp[0x58 >> 2] = htonl(0x30410000);
		lp[0x60 >> 2] = htonl(0x0000003d);
	}

	// Construct AddrMap for NewWorld ROM
	if (ROMType == ROMTYPE_NEWWORLD || ROMType == ROMTYPE_ZANZIBAR || ROMType == ROMTYPE_GOSSAMER) {
		lp = (uint32 *)(ROMBaseHost + ADDR_MAP_PATCH_SPACE);
		memset(lp - 10, 0, 0x128);
		lp[-10] = htonl(0x0300001c);
		lp[-9] = htonl(0x000108c4);
		lp[-4] = htonl(0x00300000);
		lp[-2] = htonl(0x11010000);
		lp[-1] = htonl(0xf8000000);
		lp[0] = htonl(0xffc00000);
		lp[2] = htonl(0xf3016000);
		lp[3] = htonl(0xf3012000);
		lp[4] = htonl(0xf3012000);
		lp[24] = htonl(0xf3018000);
		lp[25] = htonl(0xf3010000);
		lp[34] = htonl(0xf3011000);
		lp[38] = htonl(0xf3015000);
		lp[39] = htonl(0xf3014000);
		lp[43] = htonl(0xf3000000);
		lp[48] = htonl(0xf8000000);
	}

	// Don't initialize VIA (via Universal)
	static const uint8 via_init_dat[] = {0x08, 0x00, 0x00, 0x02, 0x67, 0x00, 0x00, 0x2c, 0x24, 0x68, 0x00, 0x08};
	if ((base = find_rom_data(0xe000, 0x15000, via_init_dat, sizeof(via_init_dat))) == 0) return false;
	D(bug("via_init %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base + 4);
	*wp = htons(0x6000);			// bra

	static const uint8 via_init2_dat[] = {0x24, 0x68, 0x00, 0x08, 0x00, 0x12, 0x00, 0x30, 0x4e, 0x71};
	if ((base = find_rom_data(0xa000, 0x10000, via_init2_dat, sizeof(via_init2_dat))) == 0) return false;
	D(bug("via_init2 %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp = htons(0x4ed6);			// jmp	(a6)

	static const uint8 via_init3_dat[] = {0x22, 0x68, 0x00, 0x08, 0x28, 0x3c, 0x20, 0x00, 0x01, 0x00};
	if ((base = find_rom_data(0xa000, 0x10000, via_init3_dat, sizeof(via_init3_dat))) == 0) return false;
	D(bug("via_init3 %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp = htons(0x4ed6);			// jmp	(a6)

	// Don't RunDiags, get BootGlobs pointer directly
	if (ROMType == ROMTYPE_NEWWORLD) {
		static const uint8 run_diags_dat[] = {0x60, 0xff, 0x00, 0x0c};
		if ((base = find_rom_data(0x110, 0x128, run_diags_dat, sizeof(run_diags_dat))) == 0) return false;
		D(bug("run_diags %08lx\n", base));
		wp = (uint16 *)(ROMBaseHost + base);
		*wp++ = htons(0x4df9);			// lea	xxx,a6
		*wp++ = htons((RAMBase + RAMSize - 0x1c) >> 16);
		*wp = htons((RAMBase + RAMSize - 0x1c) & 0xffff);
	} else {
		static const uint8 run_diags_dat[] = {0x74, 0x00, 0x2f, 0x0e};
		if ((base = find_rom_data(0xd0, 0xf0, run_diags_dat, sizeof(run_diags_dat))) == 0) return false;
		D(bug("run_diags %08lx\n", base));
		wp = (uint16 *)(ROMBaseHost + base - 6);
		*wp++ = htons(0x4df9);			// lea	xxx,a6
		*wp++ = htons((RAMBase + RAMSize - 0x1c) >> 16);
		*wp = htons((RAMBase + RAMSize - 0x1c) & 0xffff);
	}

	// Replace NVRAM routines
	static const uint8 nvram1_dat[] = {0x48, 0xe7, 0x01, 0x0e, 0x24, 0x68, 0x00, 0x08, 0x08, 0x83, 0x00, 0x1f};
	if ((base = find_rom_data(0x7000, 0xc000, nvram1_dat, sizeof(nvram1_dat))) == 0) return false;
	D(bug("nvram1 %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp++ = htons(M68K_EMUL_OP_XPRAM1);
	*wp = htons(M68K_RTS);

	if (ROMType == ROMTYPE_NEWWORLD) {
		static const uint8 nvram2_dat[] = {0x48, 0xe7, 0x1c, 0xe0, 0x4f, 0xef, 0xff, 0xb4};
		if ((base = find_rom_data(0xa000, 0xd000, nvram2_dat, sizeof(nvram2_dat))) == 0) return false;
		D(bug("nvram2 %08lx\n", base));
		wp = (uint16 *)(ROMBaseHost + base);
		*wp++ = htons(M68K_EMUL_OP_XPRAM2);
		*wp = htons(0x4ed3);			// jmp	(a3)

		static const uint8 nvram3_dat[] = {0x48, 0xe7, 0xdc, 0xe0, 0x4f, 0xef, 0xff, 0xb4};
		if ((base = find_rom_data(0xa000, 0xd000, nvram3_dat, sizeof(nvram3_dat))) == 0) return false;
		D(bug("nvram3 %08lx\n", base));
		wp = (uint16 *)(ROMBaseHost + base);
		*wp++ = htons(M68K_EMUL_OP_XPRAM3);
		*wp = htons(0x4ed3);			// jmp	(a3)

		static const uint8 nvram4_dat[] = {0x4e, 0x56, 0xff, 0xa8, 0x48, 0xe7, 0x1f, 0x38, 0x16, 0x2e, 0x00, 0x13};
		if ((base = find_rom_data(0xa000, 0xd000, nvram4_dat, sizeof(nvram4_dat))) == 0) return false;
		D(bug("nvram4 %08lx\n", base));
		wp = (uint16 *)(ROMBaseHost + base + 16);
		*wp++ = htons(0x1a2e);			// move.b	($000f,a6),d5
		*wp++ = htons(0x000f);
		*wp++ = htons(M68K_EMUL_OP_NVRAM3);
		*wp++ = htons(0x4cee);			// movem.l	($ff88,a6),d3-d7/a2-a4
		*wp++ = htons(0x1cf8);
		*wp++ = htons(0xff88);
		*wp++ = htons(0x4e5e);			// unlk	a6
		*wp = htons(M68K_RTS);

		static const uint8 nvram5_dat[] = {0x0c, 0x80, 0x03, 0x00, 0x00, 0x00, 0x66, 0x0a, 0x70, 0x00, 0x21, 0xf8, 0x02, 0x0c, 0x01, 0xe4};
		if ((base = find_rom_data(0xa000, 0xd000, nvram5_dat, sizeof(nvram5_dat))) == 0) return false;
		D(bug("nvram5 %08lx\n", base));
		wp = (uint16 *)(ROMBaseHost + base + 6);
		*wp = htons(M68K_NOP);

		static const uint8 nvram6_dat[] = {0x2f, 0x0a, 0x24, 0x48, 0x4f, 0xef, 0xff, 0xa0, 0x20, 0x0f};
		if ((base = find_rom_data(0x9000, 0xb000, nvram6_dat, sizeof(nvram6_dat))) == 0) return false;
		D(bug("nvram6 %08lx\n", base));
		wp = (uint16 *)(ROMBaseHost + base);
		*wp++ = htons(0x7000);			// moveq	#0,d0
		*wp++ = htons(0x2080);			// move.l	d0,(a0)
		*wp++ = htons(0x4228);			// clr.b	4(a0)
		*wp++ = htons(0x0004);
		*wp = htons(M68K_RTS);

		static const uint8 nvram7_dat[] = {0x42, 0x2a, 0x00, 0x04, 0x4f, 0xef, 0x00, 0x60, 0x24, 0x5f, 0x4e, 0x75, 0x4f, 0xef, 0xff, 0xa0, 0x20, 0x0f};
		base = find_rom_data(0x9000, 0xb000, nvram7_dat, sizeof(nvram7_dat));
		if (base) {
			D(bug("nvram7 %08lx\n", base));
			wp = (uint16 *)(ROMBaseHost + base + 12);
			*wp = htons(M68K_RTS);
		}
	} else {
		static const uint8 nvram2_dat[] = {0x4e, 0xd6, 0x06, 0x41, 0x13, 0x00};
		if ((base = find_rom_data(0x7000, 0xb000, nvram2_dat, sizeof(nvram2_dat))) == 0) return false;
		D(bug("nvram2 %08lx\n", base));
		wp = (uint16 *)(ROMBaseHost + base + 2);
		*wp++ = htons(M68K_EMUL_OP_XPRAM2);
		*wp = htons(0x4ed3);			// jmp	(a3)

		static const uint8 nvram3_dat[] = {0x4e, 0xd3, 0x06, 0x41, 0x13, 0x00};
		if ((base = find_rom_data(0x7000, 0xb000, nvram3_dat, sizeof(nvram3_dat))) == 0) return false;
		D(bug("nvram3 %08lx\n", base));
		wp = (uint16 *)(ROMBaseHost + base + 2);
		*wp++ = htons(M68K_EMUL_OP_XPRAM3);
		*wp = htons(0x4ed3);			// jmp	(a3)

		static const uint32 nvram4_loc[] = {0x582f0, 0xa0a0, 0x7e50, 0xa1d0, 0x538d0, 0};
		wp = (uint16 *)(ROMBaseHost + nvram4_loc[ROMType]);
		*wp++ = htons(0x202f);			// move.l	4(sp),d0
		*wp++ = htons(0x0004);
		*wp++ = htons(M68K_EMUL_OP_NVRAM1);
		if (ROMType == ROMTYPE_ZANZIBAR || ROMType == ROMTYPE_GAZELLE)
			*wp = htons(M68K_RTS);
		else {
			*wp++ = htons(0x1f40);			// move.b	d0,8(sp)
			*wp++ = htons(0x0008);
			*wp++ = htons(0x4e74);			// rtd	#4
			*wp = htons(0x0004);
		}

		static const uint32 nvram5_loc[] = {0x58460, 0xa0f0, 0x7f40, 0xa220, 0x53a20, 0};
		wp = (uint16 *)(ROMBaseHost + nvram5_loc[ROMType]);
		if (ROMType == ROMTYPE_ZANZIBAR || ROMType == ROMTYPE_GAZELLE) {
			*wp++ = htons(0x202f);			// move.l	4(sp),d0
			*wp++ = htons(0x0004);
			*wp++ = htons(0x122f);			// move.b	11(sp),d1
			*wp++ = htons(0x000b);
			*wp++ = htons(M68K_EMUL_OP_NVRAM2);
			*wp = htons(M68K_RTS);
		} else {
			*wp++ = htons(0x202f);			// move.l	6(sp),d0
			*wp++ = htons(0x0006);
			*wp++ = htons(0x122f);			// move.b	4(sp),d1
			*wp++ = htons(0x0004);
			*wp++ = htons(M68K_EMUL_OP_NVRAM2);
			*wp++ = htons(0x4e74);			// rtd	#6
			*wp = htons(0x0006);
		}
	}

	// Fix MemTop/BootGlobs during system startup
	static const uint8 mem_top_dat[] = {0x2c, 0x6c, 0xff, 0xec, 0x2a, 0x4c, 0xdb, 0xec, 0xff, 0xf4};
	if ((base = find_rom_data(0x120, 0x180, mem_top_dat, sizeof(mem_top_dat))) == 0) return false;
	D(bug("mem_top %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp++ = htons(M68K_EMUL_OP_FIX_MEMTOP);
	*wp = htons(M68K_NOP);

	// Don't initialize SCC (via 0x1ac)
	static const uint8 scc_init_caller_dat[] = {0x21, 0xce, 0x01, 0x08, 0x22, 0x78, 0x0d, 0xd8};
	if ((base = find_rom_data(0x180, 0x1f0, scc_init_caller_dat, sizeof(scc_init_caller_dat))) == 0) return false;
	D(bug("scc_init_caller %08lx\n", base + 12));
	wp = (uint16 *)(ROMBaseHost + base + 12);
	loc = ntohs(wp[1]) + ((uintptr)wp - (uintptr)ROMBaseHost) + 2;
	static const uint8 scc_init_dat[] = {0x20, 0x78, 0x01, 0xdc, 0x22, 0x78, 0x01, 0xd8};
	if ((base = find_rom_data(loc, loc + 0x80, scc_init_dat, sizeof(scc_init_dat))) == 0) return false;
	D(bug("scc_init %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp++ = htons(M68K_EMUL_OP_RESET);
	*wp = htons(M68K_RTS);

	// Don't EnableExtCache (via 0x1f6) and don't DisableIntSources(via 0x1fc)
	static const uint8 ext_cache_dat[] = {0x4e, 0x7b, 0x00, 0x02};
	if ((base = find_rom_data(0x1d0, 0x230, ext_cache_dat, sizeof(ext_cache_dat))) == 0) return false;
	D(bug("ext_cache %08lx\n", base));
	loc = ReadMacInt32(ROMBase + base + 6);
	wp = (uint16 *)(ROMBaseHost + loc + base + 6);
	*wp = htons(M68K_RTS);
	loc = ReadMacInt32(ROMBase + base + 12);
	wp = (uint16 *)(ROMBaseHost + loc + base + 12);
	*wp = htons(M68K_RTS);

	// Fake CPU speed test (SetupTimeK)
	static const uint8 timek_dat[] = {0x0c, 0x38, 0x00, 0x04, 0x01, 0x2f, 0x6d, 0x3c};
	if ((base = find_rom_data(0x400, 0x500, timek_dat, sizeof(timek_dat))) == 0) return false;
	D(bug("timek %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp++ = htons(0x31fc);			// move.w	#xxx,TimeDBRA
	*wp++ = htons(100);
	*wp++ = htons(0x0d00);
	*wp++ = htons(0x31fc);			// move.w	#xxx,TimeSCCDBRA
	*wp++ = htons(100);
	*wp++ = htons(0x0d02);
	*wp++ = htons(0x31fc);			// move.w	#xxx,TimeSCSIDBRA
	*wp++ = htons(100);
	*wp++ = htons(0x0b24);
	*wp++ = htons(0x31fc);			// move.w	#xxx,TimeRAMDBRA
	*wp++ = htons(100);
	*wp++ = htons(0x0cea);
	*wp = htons(M68K_RTS);

	// Relocate jump tables ($2000..)
	static const uint8 jump_tab_dat[] = {0x41, 0xfa, 0x00, 0x0e, 0x21, 0xc8, 0x20, 0x10, 0x4e, 0x75};
	if ((base = find_rom_data(0x3000, 0x6000, jump_tab_dat, sizeof(jump_tab_dat))) == 0) return false;
	D(bug("jump_tab %08lx\n", base));
	lp = (uint32 *)(ROMBaseHost + base + 16);
	for (;;) {
		D(bug(" %08lx\n", (uintptr)lp - (uintptr)ROMBaseHost));
		while ((ntohl(*lp) & 0xff000000) == 0xff000000) {
			*lp = htonl((ntohl(*lp) & (ROM_SIZE-1)) + ROMBase);
			lp++;
		}
		while (!ntohl(*lp)) lp++;
		if (ntohl(*lp) != 0x41fa000e)
			break;
		lp += 4;
	}

	// Create SysZone at start of Mac RAM (SetSysAppZone, via 0x22a)
	static const uint8 sys_zone_dat[] = {0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x40, 0x00};
	if ((base = find_rom_data(0x600, 0x900, sys_zone_dat, sizeof(sys_zone_dat))) == 0) return false;
	D(bug("sys_zone %08lx\n", base));
	lp = (uint32 *)(ROMBaseHost + base);
	*lp++ = htonl(RAMBase ? RAMBase : 0x3000);
	*lp = htonl(RAMBase ? RAMBase + 0x1800 : 0x4800);

	// Set boot stack at RAMBase+4MB and fix logical/physical RAM size (CompBootStack)
	// The RAM size fix must be done after InitMemMgr!
	static const uint8 boot_stack_dat[] = {0x08, 0x38, 0x00, 0x06, 0x24, 0x0b};
	if ((base = find_rom_data(0x580, 0x800, boot_stack_dat, sizeof(boot_stack_dat))) == 0) return false;
	D(bug("boot_stack %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp++ = htons(0x207c);			// move.l	#RAMBase+0x3ffffe,a0
	*wp++ = htons((RAMBase + 0x3ffffe) >> 16);
	*wp++ = htons((RAMBase + 0x3ffffe) & 0xffff);
	*wp++ = htons(M68K_EMUL_OP_FIX_MEMSIZE);
	*wp = htons(M68K_RTS);

	// Get PowerPC page size (InitVMemMgr, via 0x240)
	static const uint8 page_size_dat[] = {0x20, 0x30, 0x81, 0xf2, 0x5f, 0xff, 0xef, 0xd8, 0x00, 0x10};
	if ((base = find_rom_data(0xb000, 0x12000, page_size_dat, sizeof(page_size_dat))) == 0) return false;
	D(bug("page_size %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp++ = htons(0x203c);			// move.l	#$1000,d0
	*wp++ = htons(0);
	*wp++ = htons(0x1000);
	*wp++ = htons(M68K_NOP);
	*wp = htons(M68K_NOP);

	// Gestalt PowerPC page size, CPU type, RAM size (InitGestalt, via 0x25c)
	static const uint8 page_size2_dat[] = {0x26, 0x79, 0x5f, 0xff, 0xef, 0xd8, 0x25, 0x6b, 0x00, 0x10, 0x00, 0x1e};
	if ((base = find_rom_data(0x50000, 0x70000, page_size2_dat, sizeof(page_size2_dat))) == 0) return false;
	D(bug("page_size2 %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp++ = htons(0x257c);			// move.l	#$1000,$1e(a2)
	*wp++ = htons(0);
	*wp++ = htons(0x1000);
	*wp++ = htons(0x001e);
	*wp++ = htons(0x157c);			// move.b	#PVR,$1d(a2)
	uint32 cput = (PVR >> 16);
	if (cput == 0x7000)
		cput |= 0x20;
	else if (cput >= 0x8000 && cput <= 0x8002)
		cput |= 0x10;
	cput &= 0xff;
	*wp++ = htons(cput);
	*wp++ = htons(0x001d);
	*wp++ = htons(0x263c);			// move.l	#RAMSize,d3
	*wp++ = htons(RAMSize >> 16);
	*wp++ = htons(RAMSize & 0xffff);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp = htons(M68K_NOP);
	if (ROMType == ROMTYPE_NEWWORLD)
		wp = (uint16 *)(ROMBaseHost + base + 0x4a);
	else
		wp = (uint16 *)(ROMBaseHost + base + 0x28);
	*wp++ = htons(M68K_NOP);
	*wp = htons(M68K_NOP);

	// Gestalt CPU/bus clock speed (InitGestalt, via 0x25c)
	if (ROMType == ROMTYPE_ZANZIBAR) {
		wp = (uint16 *)(ROMBaseHost + 0x5d87a);
		*wp++ = htons(0x203c);			// move.l	#Hz,d0
		*wp++ = htons(BusClockSpeed >> 16);
		*wp++ = htons(BusClockSpeed & 0xffff);
		*wp++ = htons(M68K_NOP);
		*wp = htons(M68K_NOP);
		wp = (uint16 *)(ROMBaseHost + 0x5d888);
		*wp++ = htons(0x203c);			// move.l	#Hz,d0
		*wp++ = htons(CPUClockSpeed >> 16);
		*wp++ = htons(CPUClockSpeed & 0xffff);
		*wp++ = htons(M68K_NOP);
		*wp = htons(M68K_NOP);
	}

	// Don't write to GC interrupt mask register (via 0x262)
	if (ROMType != ROMTYPE_NEWWORLD) {
		static const uint8 gc_mask_dat[] = {0x83, 0xa8, 0x00, 0x24, 0x4e, 0x71};
		if ((base = find_rom_data(0x13000, 0x20000, gc_mask_dat, sizeof(gc_mask_dat))) == 0) return false;
		D(bug("gc_mask %08lx\n", base));
		wp = (uint16 *)(ROMBaseHost + base);
		*wp++ = htons(M68K_NOP);
		*wp = htons(M68K_NOP);
		wp = (uint16 *)(ROMBaseHost + base + 0x40);
		*wp++ = htons(M68K_NOP);
		*wp = htons(M68K_NOP);
		wp = (uint16 *)(ROMBaseHost + base + 0x78);
		*wp++ = htons(M68K_NOP);
		*wp = htons(M68K_NOP);
		wp = (uint16 *)(ROMBaseHost + base + 0x96);
		*wp++ = htons(M68K_NOP);
		*wp = htons(M68K_NOP);

		static const uint8 gc_mask2_dat[] = {0x02, 0xa8, 0x00, 0x00, 0x00, 0x80, 0x00, 0x24};
		if ((base = find_rom_data(0x13000, 0x20000, gc_mask2_dat, sizeof(gc_mask2_dat))) == 0) return false;
		D(bug("gc_mask2 %08lx\n", base));
		wp = (uint16 *)(ROMBaseHost + base);
		if (ROMType == ROMTYPE_GOSSAMER) {
			*wp++ = htons(M68K_NOP);
			*wp++ = htons(M68K_NOP);
			*wp++ = htons(M68K_NOP);
			*wp++ = htons(M68K_NOP);
		}
		for (int i=0; i<5; i++) {
			*wp++ = htons(M68K_NOP);
			*wp++ = htons(M68K_NOP);
			*wp++ = htons(M68K_NOP);
			*wp++ = htons(M68K_NOP);
			wp += 2;
		}
		if (ROMType == ROMTYPE_ZANZIBAR || ROMType == ROMTYPE_GOSSAMER) {
			for (int i=0; i<6; i++) {
				*wp++ = htons(M68K_NOP);
				*wp++ = htons(M68K_NOP);
				*wp++ = htons(M68K_NOP);
				*wp++ = htons(M68K_NOP);
				wp += 2;
			}
		}
	}

	// Don't initialize Cuda (via 0x274)
	static const uint8 cuda_init_dat[] = {0x08, 0xa9, 0x00, 0x04, 0x16, 0x00, 0x4e, 0x71, 0x13, 0x7c, 0x00, 0x84, 0x1c, 0x00, 0x4e, 0x71};
	if ((base = find_rom_data(0xa000, 0x12000, cuda_init_dat, sizeof(cuda_init_dat))) == 0) return false;
	D(bug("cuda_init %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp = htons(M68K_NOP);

	// Patch GetCPUSpeed (via 0x27a) (some ROMs have two of them)
	static const uint8 cpu_speed_dat[] = {0x20, 0x30, 0x81, 0xf2, 0x5f, 0xff, 0xef, 0xd8, 0x00, 0x04, 0x4c, 0x7c};
	if ((base = find_rom_data(0x6000, 0xa000, cpu_speed_dat, sizeof(cpu_speed_dat))) == 0) return false;
	D(bug("cpu_speed %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp++ = htons(0x203c);			// move.l	#(MHz<<16)|MHz,d0
	*wp++ = htons(CPUClockSpeed / 1000000);
	*wp++ = htons(CPUClockSpeed / 1000000);
	*wp = htons(M68K_RTS);
	if ((base = find_rom_data(base, 0xa000, cpu_speed_dat, sizeof(cpu_speed_dat))) != 0) {
		D(bug("cpu_speed2 %08lx\n", base));
		wp = (uint16 *)(ROMBaseHost + base);
		*wp++ = htons(0x203c);			// move.l	#(MHz<<16)|MHz,d0
		*wp++ = htons(CPUClockSpeed / 1000000);
		*wp++ = htons(CPUClockSpeed / 1000000);
		*wp = htons(M68K_RTS);
	}

	// Don't poke VIA in InitTimeMgr (via 0x298)
	static const uint8 time_via_dat[] = {0x40, 0xe7, 0x00, 0x7c, 0x07, 0x00, 0x28, 0x78, 0x01, 0xd4, 0x43, 0xec, 0x10, 0x00};
	if ((base = find_rom_data(0x30000, 0x40000, time_via_dat, sizeof(time_via_dat))) == 0) return false;
	D(bug("time_via %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp++ = htons(0x4cdf);			// movem.l	(sp)+,d0-d5/a0-a4
	*wp++ = htons(0x1f3f);
	*wp = htons(M68K_RTS);

	// Don't read from 0xff800000 (Name Registry, Open Firmware?) (via 0x2a2)
	// Remove this if FE03 works!!
	static const uint8 open_firmware_dat[] = {0x2f, 0x79, 0xff, 0x80, 0x00, 0x00, 0x00, 0xfc};
	if ((base = find_rom_data(0x48000, 0x58000, open_firmware_dat, sizeof(open_firmware_dat))) == 0) return false;
	D(bug("open_firmware %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp++ = htons(0x2f7c);			// move.l		#deadbeef,0xfc(a7)
	*wp++ = htons(0xdead);
	*wp++ = htons(0xbeef);
	*wp = htons(0x00fc);
	wp = (uint16 *)(ROMBaseHost + base + 0x1a);
	*wp++ = htons(M68K_NOP);		// (FE03 opcode, tries to jump to 0xdeadbeef)
	*wp = htons(M68K_NOP);

	// Don't EnableExtCache (via 0x2b2)
	static const uint8 ext_cache2_dat[] = {0x4f, 0xef, 0xff, 0xec, 0x20, 0x4f, 0x10, 0xbc, 0x00, 0x01, 0x11, 0x7c, 0x00, 0x1b};
	if ((base = find_rom_data(0x13000, 0x20000, ext_cache2_dat, sizeof(ext_cache2_dat))) == 0) return false;
	D(bug("ext_cache2 %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp = htons(M68K_RTS);

	// Don't install Time Manager task for 60Hz interrupt (Enable60HzInts, via 0x2b8)
	if (ROMType == ROMTYPE_NEWWORLD || ROMType == ROMTYPE_GOSSAMER) {
		static const uint8 tm_task_dat[] = {0x30, 0x3c, 0x4e, 0x2b, 0xa9, 0xc9};
		if ((base = find_rom_data(0x2a0, 0x320, tm_task_dat, sizeof(tm_task_dat))) == 0) return false;
		D(bug("tm_task %08lx\n", base));
		wp = (uint16 *)(ROMBaseHost + base + 28);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp = htons(M68K_NOP);
	} else {
		static const uint8 tm_task_dat[] = {0x20, 0x3c, 0x73, 0x79, 0x73, 0x61};
		if ((base = find_rom_data(0x280, 0x300, tm_task_dat, sizeof(tm_task_dat))) == 0) return false;
		D(bug("tm_task %08lx\n", base));
		wp = (uint16 *)(ROMBaseHost + base - 6);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp = htons(M68K_NOP);
	}

	// Don't read PVR from 0x5fffef80 in DriverServicesLib (via 0x316)
	if (ROMType != ROMTYPE_NEWWORLD && ROMType != ROMTYPE_GOSSAMER) {
		uint32 dsl_offset = find_rom_resource(FOURCC('n','l','i','b'), -16401);
		if (ROMType == ROMTYPE_ZANZIBAR) {
			static const uint8 dsl_pvr_dat[] = {0x40, 0x82, 0x00, 0x40, 0x38, 0x60, 0xef, 0x80, 0x3c, 0x63, 0x60, 0x00, 0x80, 0x83, 0x00, 0x00, 0x54, 0x84, 0x84, 0x3e};
			if ((base = find_rom_data(dsl_offset, dsl_offset + 0x6000, dsl_pvr_dat, sizeof(dsl_pvr_dat))) == 0) return false;
		} else {
			static const uint8 dsl_pvr_dat[] = {0x3b, 0xc3, 0x00, 0x00, 0x30, 0x84, 0xff, 0xa0, 0x40, 0x82, 0x00, 0x44, 0x80, 0x84, 0xef, 0xe0, 0x54, 0x84, 0x84, 0x3e};
			if ((base = find_rom_data(dsl_offset, dsl_offset + 0x6000, dsl_pvr_dat, sizeof(dsl_pvr_dat))) == 0) return false;
		}
		D(bug("dsl_pvr %08lx\n", base));
		lp = (uint32 *)(ROMBaseHost + base + 12);
		*lp = htonl(0x3c800000 | (PVR >> 16));	// lis	r4,PVR

		// Don't read bus clock from 0x5fffef88 in DriverServicesLib (via 0x316)
		if (ROMType == ROMTYPE_ZANZIBAR) {
			static const uint8 dsl_bus_dat[] = {0x81, 0x07, 0x00, 0x00, 0x39, 0x20, 0x42, 0x40, 0x81, 0x62, 0xff, 0x20};
			if ((base = find_rom_data(dsl_offset, dsl_offset + 0x6000, dsl_bus_dat, sizeof(dsl_bus_dat))) == 0) return false;
			D(bug("dsl_bus %08lx\n", base));
			lp = (uint32 *)(ROMBaseHost + base);
			*lp = htonl(0x81000000 + XLM_BUS_CLOCK);	// lwz	r8,(bus clock speed)
		} else {
			static const uint8 dsl_bus_dat[] = {0x80, 0x83, 0xef, 0xe8, 0x80, 0x62, 0x00, 0x10, 0x7c, 0x04, 0x03, 0x96};
			if ((base = find_rom_data(dsl_offset, dsl_offset + 0x6000, dsl_bus_dat, sizeof(dsl_bus_dat))) == 0) return false;
			D(bug("dsl_bus %08lx\n", base));
			lp = (uint32 *)(ROMBaseHost + base);
			*lp = htonl(0x80800000 + XLM_BUS_CLOCK);	// lwz	r4,(bus clock speed)
		}
	}

	// Don't open InterruptTreeTNT in MotherBoardHAL init in DriverServicesLib init
	if (ROMType == ROMTYPE_ZANZIBAR) {
		lp = (uint32 *)(ROMBaseHost + find_rom_resource(FOURCC('n','l','i','b'), -16408) + 0x16c);
		*lp = htonl(0x38600000);		// li	r3,0
	}

	// Don't read from MacPgm in WipeOutMACPGMINFOProcPtrs (StdCLib)
	if (1) {
		uint32 hpchk_offset = find_rom_resource(FOURCC('n','l','i','b'), 10);
		static const uint8 hpchk_dat[] = {0x80, 0x80, 0x03, 0x16, 0x94, 0x21, 0xff, 0xb0, 0x83, 0xc4, 0x00, 0x04};
		if ((base = find_rom_data(hpchk_offset, hpchk_offset + 0x3000, hpchk_dat, sizeof(hpchk_dat))) == 0) return false;
		D(bug("macpgm %08lx\n", base));
		lp = (uint32 *)(ROMBaseHost + base);
		*lp = htonl(0x80800000 + XLM_ZERO_PAGE);		// lwz	r4,(zero page)
	}

	// Patch Name Registry
	static const uint8 name_reg_dat[] = {0x70, 0xff, 0xab, 0xeb};
	if ((base = find_rom_data(0x300, 0x380, name_reg_dat, sizeof(name_reg_dat))) == 0) return false;
	D(bug("name_reg %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp = htons(M68K_EMUL_OP_NAME_REGISTRY);

#if DISABLE_SCSI
	// Fake SCSI Manager
	// Remove this if SCSI Manager works!!
	static const uint8 scsi_mgr_a_dat[] = {0x4e, 0x56, 0x00, 0x00, 0x20, 0x3c, 0x00, 0x00, 0x04, 0x0c, 0xa7, 0x1e};
	static const uint8 scsi_mgr_b_dat[] = {0x4e, 0x56, 0x00, 0x00, 0x2f, 0x0c, 0x20, 0x3c, 0x00, 0x00, 0x04, 0x0c, 0xa7, 0x1e};
	if ((base = find_rom_data(0x1c000, 0x28000, scsi_mgr_a_dat, sizeof(scsi_mgr_a_dat))) == 0) {
		if ((base = find_rom_data(0x1c000, 0x28000, scsi_mgr_b_dat, sizeof(scsi_mgr_b_dat))) == 0) return false;
	}
	D(bug("scsi_mgr %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp++ = htons(0x21fc);			// move.l	#xxx,0x624	(SCSIAtomic)
	*wp++ = htons((ROMBase + base + 18) >> 16);
	*wp++ = htons((ROMBase + base + 18) & 0xffff);
	*wp++ = htons(0x0624);
	*wp++ = htons(0x21fc);			// move.l	#xxx,0xe54	(SCSIDispatch)
	*wp++ = htons((ROMBase + base + 22) >> 16);
	*wp++ = htons((ROMBase + base + 22) & 0xffff);
	*wp++ = htons(0x0e54);
	*wp++ = htons(M68K_RTS);
	*wp++ = htons(M68K_EMUL_OP_SCSI_ATOMIC);
	*wp++ = htons(M68K_RTS);
	*wp++ = htons(M68K_EMUL_OP_SCSI_DISPATCH);
	*wp = htons(0x4ed0);			// jmp		(a0)
	wp = (uint16 *)(ROMBaseHost + base + 0x20);
	*wp++ = htons(0x7000);			// moveq	#0,d0
	*wp = htons(M68K_RTS);
#endif

#if DISABLE_SCSI
	// Don't access SCSI variables
	// Remove this if SCSI Manager works!!
	if (ROMType == ROMTYPE_NEWWORLD) {
		static const uint8 scsi_var_dat[] = {0x70, 0x01, 0xa0, 0x89, 0x4a, 0x6e, 0xfe, 0xac, 0x4f, 0xef, 0x00, 0x10, 0x66, 0x00};
		if ((base = find_rom_data(0x1f500, 0x1f600, scsi_var_dat, sizeof(scsi_var_dat))) != 0) {
			D(bug("scsi_var %08lx\n", base));
			wp = (uint16 *)(ROMBaseHost + base + 12);
			*wp = htons(0x6000);	// bra
		}

		static const uint8 scsi_var2_dat[] = {0x4e, 0x56, 0xfc, 0x58, 0x48, 0xe7, 0x1f, 0x38};
		if ((base = find_rom_data(0x1f700, 0x1f800, scsi_var2_dat, sizeof(scsi_var2_dat))) != 0) {
			D(bug("scsi_var2 %08lx\n", base));
			wp = (uint16 *)(ROMBaseHost + base);
			*wp++ = htons(0x7000);	// moveq #0,d0
			*wp = htons(M68K_RTS);
		}
	}
	else if (ROMType == ROMTYPE_GOSSAMER) {
		static const uint8 scsi_var_dat[] = {0x70, 0x01, 0xa0, 0x89, 0x4a, 0x6e, 0xfe, 0xac, 0x4f, 0xef, 0x00, 0x10, 0x66, 0x00};
		if ((base = find_rom_data(0x1d700, 0x1d800, scsi_var_dat, sizeof(scsi_var_dat))) != 0) {
			D(bug("scsi_var %08lx\n", base));
			wp = (uint16 *)(ROMBaseHost + base + 12);
			*wp = htons(0x6000);	// bra
		}

		static const uint8 scsi_var2_dat[] = {0x4e, 0x56, 0xfc, 0x5a, 0x48, 0xe7, 0x1f, 0x38};
		if ((base = find_rom_data(0x1d900, 0x1da00, scsi_var2_dat, sizeof(scsi_var2_dat))) != 0) {
			D(bug("scsi_var2 %08lx\n", base));
			wp = (uint16 *)(ROMBaseHost + base);
			*wp++ = htons(0x7000);	// moveq #0,d0
			*wp = htons(M68K_RTS);
		}
	}
#endif

	// Don't wait in ADBInit (via 0x36c)
	static const uint8 adb_init_dat[] = {0x08, 0x2b, 0x00, 0x05, 0x01, 0x5d, 0x66, 0xf8};
	if ((base = find_rom_data(0x31000, 0x3d000, adb_init_dat, sizeof(adb_init_dat))) == 0) return false;
	D(bug("adb_init %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base + 6);
	*wp = htons(M68K_NOP);

	// Modify check in InitResources() so that addresses >0x80000000 work
	static const uint8 init_res_dat[] = {0x4a, 0xb8, 0x0a, 0x50, 0x6e, 0x20};
	if ((base = find_rom_data(0x78000, 0x8c000, init_res_dat, sizeof(init_res_dat))) == 0) return false;
	D(bug("init_res %08lx\n", base));
	bp = (uint8 *)(ROMBaseHost + base + 4);
	*bp = 0x66;

	// Modify vCheckLoad() so that we can patch resources (68k Resource Manager)
	static const uint8 check_load_dat[] = {0x20, 0x78, 0x07, 0xf0, 0x4e, 0xd0};
	if ((base = find_rom_data(0x78000, 0x8c000, check_load_dat, sizeof(check_load_dat))) == 0) return false;
	D(bug("check_load %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp++ = htons(M68K_JMP);
	*wp++ = htons((ROMBase + CHECK_LOAD_PATCH_SPACE) >> 16);
	*wp = htons((ROMBase + CHECK_LOAD_PATCH_SPACE) & 0xffff);
	wp = (uint16 *)(ROMBaseHost + CHECK_LOAD_PATCH_SPACE);
	*wp++ = htons(0x2f03);			// move.l	d3,-(a7)
	*wp++ = htons(0x2078);			// move.l	$07f0,a0
	*wp++ = htons(0x07f0);
	*wp++ = htons(M68K_JSR_A0);
	*wp++ = htons(M68K_EMUL_OP_CHECKLOAD);
	*wp = htons(M68K_RTS);

	// Replace .Sony driver
	sony_offset = find_rom_resource(FOURCC('D','R','V','R'), 4);
	if (ROMType == ROMTYPE_ZANZIBAR || ROMType == ROMTYPE_NEWWORLD)
		sony_offset = find_rom_resource(FOURCC('D','R','V','R'), 4, true);		// First DRVR 4 is .MFMFloppy
	if (sony_offset == 0) {
		sony_offset = find_rom_resource(FOURCC('n','d','r','v'), -20196);		// NewWorld 1.6 has "PCFloppy" ndrv
		if (sony_offset == 0)
			return false;
		lp = (uint32 *)(ROMBaseHost + rsrc_ptr + 8);
		*lp = htonl(FOURCC('D','R','V','R'));
		wp = (uint16 *)(ROMBaseHost + rsrc_ptr + 12);
		*wp = htons(4);
	}
	D(bug("sony_offset %08lx\n", sony_offset));
	memcpy((void *)(ROMBaseHost + sony_offset), sony_driver, sizeof(sony_driver));

	// Install .Disk and .AppleCD drivers
	memcpy((void *)(ROMBaseHost + sony_offset + 0x100), disk_driver, sizeof(disk_driver));
	memcpy((void *)(ROMBaseHost + sony_offset + 0x200), cdrom_driver, sizeof(cdrom_driver));

	// Install serial drivers
	gen_ain_driver( ROMBase + sony_offset + 0x300);
	gen_aout_driver(ROMBase + sony_offset + 0x400);
	gen_bin_driver( ROMBase + sony_offset + 0x500);
	gen_bout_driver(ROMBase + sony_offset + 0x600);

	// Copy icons to ROM
	SonyDiskIconAddr = ROMBase + sony_offset + 0x800;
	memcpy(ROMBaseHost + sony_offset + 0x800, SonyDiskIcon, sizeof(SonyDiskIcon));
	SonyDriveIconAddr = ROMBase + sony_offset + 0xa00;
	memcpy(ROMBaseHost + sony_offset + 0xa00, SonyDriveIcon, sizeof(SonyDriveIcon));
	DiskIconAddr = ROMBase + sony_offset + 0xc00;
	memcpy(ROMBaseHost + sony_offset + 0xc00, DiskIcon, sizeof(DiskIcon));
	CDROMIconAddr = ROMBase + sony_offset + 0xe00;
	memcpy(ROMBaseHost + sony_offset + 0xe00, CDROMIcon, sizeof(CDROMIcon));

	// Patch driver install routine
	static const uint8 drvr_install_dat[] = {0xa7, 0x1e, 0x21, 0xc8, 0x01, 0x1c, 0x4e, 0x75};
	if ((base = find_rom_data(0xb00, 0xd00, drvr_install_dat, sizeof(drvr_install_dat))) == 0) return false;
	D(bug("drvr_install %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base + 8);
	*wp++ = htons(M68K_EMUL_OP_INSTALL_DRIVERS);
	*wp = htons(M68K_RTS);

	// Don't install serial drivers from ROM
	if (ROMType == ROMTYPE_ZANZIBAR || ROMType == ROMTYPE_NEWWORLD || ROMType == ROMTYPE_GOSSAMER) {
		wp = (uint16 *)(ROMBaseHost + find_rom_resource(FOURCC('S','E','R','D'), 0));
		*wp = htons(M68K_RTS);
	} else {
		wp = (uint16 *)(ROMBaseHost + find_rom_resource(FOURCC('s','l','0','5'), 2) + 0xc4);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp++ = htons(M68K_NOP);
		*wp = htons(0x7000);			// moveq	#0,d0
		wp = (uint16 *)(ROMBaseHost + find_rom_resource(FOURCC('s','l','0','5'), 2) + 0x8ee);
		*wp = htons(M68K_NOP);
	}
	uint32 nsrd_offset = find_rom_resource(FOURCC('n','s','r','d'), 1);
	if (nsrd_offset) {
		lp = (uint32 *)(ROMBaseHost + rsrc_ptr + 8);
		*lp = htonl(FOURCC('x','s','r','d'));
	}

	// Replace ADBOp()
	memcpy(ROMBaseHost + find_rom_trap(0xa07c), adbop_patch, sizeof(adbop_patch));

	// Replace Time Manager
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
	*wp = htons(M68K_RTS);
	wp = (uint16 *)(ROMBaseHost + find_rom_trap(0xa093));
	*wp++ = htons(M68K_EMUL_OP_MICROSECONDS);
	*wp = htons(M68K_RTS);

	// Disable Egret Manager
	static const uint8 egret_dat[] = {0x2f, 0x30, 0x81, 0xe2, 0x20, 0x10, 0x00, 0x18};
	if ((base = find_rom_data(0xa000, 0x10000, egret_dat, sizeof(egret_dat))) == 0) return false;
	D(bug("egret %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	*wp++ = htons(0x7000);
	*wp = htons(M68K_RTS);

	// Don't call FE0A opcode in Shutdown Manager
	static const uint8 shutdown_dat[] = {0x40, 0xe7, 0x00, 0x7c, 0x07, 0x00, 0x48, 0xe7, 0x3f, 0x00, 0x2c, 0x00, 0x2e, 0x01};
	if ((base = find_rom_data(0x30000, 0x40000, shutdown_dat, sizeof(shutdown_dat))) == 0) return false;
	D(bug("shutdown %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);
	if (ROMType == ROMTYPE_ZANZIBAR)
		*wp = htons(M68K_RTS);
	else if (ntohs(wp[-4]) == 0x61ff)
		*wp = htons(M68K_RTS);
	else if (ntohs(wp[-2]) == 0x6700)
		wp[-2] = htons(0x6000);	// bra

	// Patch PowerOff()
	wp = (uint16 *)(ROMBaseHost + find_rom_trap(0xa05b));	// PowerOff()
	*wp = htons(M68K_EMUL_RETURN);

	// Patch VIA interrupt handler
	static const uint8 via_int_dat[] = {0x70, 0x7f, 0xc0, 0x29, 0x1a, 0x00, 0xc0, 0x29, 0x1c, 0x00};
	if ((base = find_rom_data(0x13000, 0x1c000, via_int_dat, sizeof(via_int_dat))) == 0) return false;
	D(bug("via_int %08lx\n", base));
	uint32 level1_int = ROMBase + base;
	wp = (uint16 *)(ROMBaseHost + base);	// Level 1 handler
	*wp++ = htons(0x7002);			// moveq	#2,d0 (60Hz interrupt)
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp++ = htons(M68K_NOP);
	*wp = htons(M68K_NOP);

	static const uint8 via_int2_dat[] = {0x13, 0x7c, 0x00, 0x02, 0x1a, 0x00, 0x4e, 0x71, 0x52, 0xb8, 0x01, 0x6a};
	if ((base = find_rom_data(0x10000, 0x18000, via_int2_dat, sizeof(via_int2_dat))) == 0) return false;
	D(bug("via_int2 %08lx\n", base));
	wp = (uint16 *)(ROMBaseHost + base);	// 60Hz handler
	*wp++ = htons(M68K_EMUL_OP_IRQ);
	*wp++ = htons(0x4a80);			// tst.l	d0
	*wp++ = htons(0x6700);			// beq		xxx
	*wp = htons(0xffe8);

	if (ROMType == ROMTYPE_NEWWORLD) {
		static const uint8 via_int3_dat[] = {0x48, 0xe7, 0xf0, 0xf0, 0x76, 0x01, 0x60, 0x26};
		if ((base = find_rom_data(0x15000, 0x19000, via_int3_dat, sizeof(via_int3_dat))) == 0) return false;
		D(bug("via_int3 %08lx\n", base));
		wp = (uint16 *)(ROMBaseHost + base);	// CHRP level 1 handler
		*wp++ = htons(M68K_JMP);
		*wp++ = htons((level1_int - 12) >> 16);
		*wp = htons((level1_int - 12) & 0xffff);
	}

	// Patch ZeroScrap() for clipboard exchange with host OS
	uint32 zero_scrap = find_rom_trap(0xa9fc);	// ZeroScrap()
	wp = (uint16 *)(ROMBaseHost + ZERO_SCRAP_PATCH_SPACE);
	*wp++ = htons(M68K_EMUL_OP_ZERO_SCRAP);
	*wp++ = htons(M68K_JMP);
	*wp++ = htons((ROMBase + zero_scrap) >> 16);
	*wp++ = htons((ROMBase + zero_scrap) & 0xffff);
	base = ROMBase + ReadMacInt32(ROMBase + 0x22);
	WriteMacInt32(base + 4 * (0xa9fc & 0x3ff), ZERO_SCRAP_PATCH_SPACE);

	// Patch PutScrap() for clipboard exchange with host OS
	uint32 put_scrap = find_rom_trap(0xa9fe);	// PutScrap()
	wp = (uint16 *)(ROMBaseHost + PUT_SCRAP_PATCH_SPACE);
	*wp++ = htons(M68K_EMUL_OP_PUT_SCRAP);
	*wp++ = htons(M68K_JMP);
	*wp++ = htons((ROMBase + put_scrap) >> 16);
	*wp++ = htons((ROMBase + put_scrap) & 0xffff);
	base = ROMBase + ReadMacInt32(ROMBase + 0x22);
	WriteMacInt32(base + 4 * (0xa9fe & 0x3ff), PUT_SCRAP_PATCH_SPACE);

	// Patch GetScrap() for clipboard exchange with host OS
	uint32 get_scrap = find_rom_trap(0xa9fd);	// GetScrap()
	wp = (uint16 *)(ROMBaseHost + GET_SCRAP_PATCH_SPACE);
	*wp++ = htons(M68K_EMUL_OP_GET_SCRAP);
	*wp++ = htons(M68K_JMP);
	*wp++ = htons((ROMBase + get_scrap) >> 16);
	*wp++ = htons((ROMBase + get_scrap) & 0xffff);
	base = ROMBase + ReadMacInt32(ROMBase + 0x22);
	WriteMacInt32(base + 4 * (0xa9fd & 0x3ff), GET_SCRAP_PATCH_SPACE);

	// Patch SynchIdleTime()
	if (PrefsFindBool("idlewait")) {
		base = find_rom_trap(0xabf7) + 4;						// SynchIdleTime()
		wp = (uint16 *)(ROMBaseHost + base);
		D(bug("SynchIdleTime at %08lx\n", base));
		if (ntohs(*wp) == 0x2078) {								// movea.l	ExpandMem,a0
			*wp++ = htons(M68K_EMUL_OP_IDLE_TIME);
			*wp = htons(M68K_NOP);
		}
		else if (ntohs(*wp) == 0x70fe)							// moveq	#-2,d0
			*wp++ = htons(M68K_EMUL_OP_IDLE_TIME_2);
		else {
			D(bug("SynchIdleTime patch not installed\n"));
		}
	}

	// Construct list of all sifters used by sound components in ROM
	D(bug("Searching for sound components with type sdev in ROM\n"));
	uint32 thing = find_rom_resource(FOURCC('t','h','n','g'));
	while (thing) {
		thing += ROMBase;
		D(bug(" found %c%c%c%c %c%c%c%c\n", ReadMacInt8(thing), ReadMacInt8(thing + 1), ReadMacInt8(thing + 2), ReadMacInt8(thing + 3), ReadMacInt8(thing + 4), ReadMacInt8(thing + 5), ReadMacInt8(thing + 6), ReadMacInt8(thing + 7)));
		if (ReadMacInt32(thing) == FOURCC('s','d','e','v') && ReadMacInt32(thing + 4) == FOURCC('s','i','n','g')) {
			WriteMacInt32(thing + 4, FOURCC('a','w','g','c'));
			D(bug(" found sdev component at offset %08x in ROM\n", thing));
			AddSifter(ReadMacInt32(thing + componentResType), ReadMacInt16(thing + componentResID));
			if (ReadMacInt32(thing + componentPFCount))
				AddSifter(ReadMacInt32(thing + componentPFResType), ReadMacInt16(thing + componentPFResID));
		}
		thing = find_rom_resource(FOURCC('t','h','n','g'), 4711, true);
	}

	// Patch component code
	D(bug("Patching sifters in ROM\n"));
	for (int i=0; i<num_sifters; i++) {
		if ((thing = find_rom_resource(sifter_list[i].type, sifter_list[i].id)) != 0) {
			D(bug(" patching type %08x, id %d\n", sifter_list[i].type, sifter_list[i].id));
			// Install 68k glue code
			uint16 *wp = (uint16 *)(ROMBaseHost + thing);
			*wp++ = htons(0x4e56); *wp++ = htons(0x0000);	// link a6,#0
			*wp++ = htons(0x48e7); *wp++ = htons(0x8018);	// movem.l d0/a3-a4,-(a7)
			*wp++ = htons(0x266e); *wp++ = htons(0x000c);	// movea.l $c(a6),a3
			*wp++ = htons(0x286e); *wp++ = htons(0x0008);	// movea.l $8(a6),a4
			*wp++ = htons(M68K_EMUL_OP_AUDIO_DISPATCH);
			*wp++ = htons(0x2d40); *wp++ = htons(0x0010);	// move.l d0,$10(a6)
			*wp++ = htons(0x4cdf); *wp++ = htons(0x1801);	// movem.l (a7)+,d0/a3-a4
			*wp++ = htons(0x4e5e);							// unlk a6
			*wp++ = htons(0x4e74); *wp++ = htons(0x0008);	// rtd #8
		}
	}
	return true;
}


/*
 *  Install .Sony, disk and CD-ROM drivers
 */

void InstallDrivers(void)
{
	D(bug("Installing drivers...\n"));
	M68kRegisters r;
	SheepArray<SIZEOF_IOParam> pb_var;
	const uintptr pb = pb_var.addr();

#if DISABLE_SCSI
	// Setup fake SCSI Globals
	r.d[0] = 0x1000;
	Execute68kTrap(0xa71e, &r);		// NewPtrSysClear()
	uint32 scsi_globals = r.a[0];
	D(bug("Fake SCSI globals at %08lx\n", scsi_globals));
	WriteMacInt32(0xc0c, scsi_globals);	// Set SCSIGlobals
#endif

	// Install floppy driver
	if (ROMType == ROMTYPE_NEWWORLD || ROMType == ROMTYPE_GOSSAMER) {

		// Force installation of floppy driver with NewWorld and Gossamer ROMs
		r.a[0] = ROMBase + sony_offset;
		r.d[0] = (uint32)SonyRefNum;
		Execute68kTrap(0xa43d, &r);		// DrvrInstallRsrvMem()
		r.a[0] = ReadMacInt32(ReadMacInt32(0x11c) + ~SonyRefNum * 4);	// Get driver handle from Unit Table
		Execute68kTrap(0xa029, &r);		// HLock()
		uint32 dce = ReadMacInt32(r.a[0]);
		WriteMacInt32(dce + dCtlDriver, ROMBase + sony_offset);
		WriteMacInt16(dce + dCtlFlags, SonyDriverFlags);
	}

	// Open .Sony driver
	SheepString sony_str("\005.Sony");
	WriteMacInt8(pb + ioPermssn, 0);
	WriteMacInt32(pb + ioNamePtr, sony_str.addr());
	r.a[0] = pb;
	Execute68kTrap(0xa000, &r);		// Open()

	// Install disk driver
	r.a[0] = ROMBase + sony_offset + 0x100;
	r.d[0] = (uint32)DiskRefNum;
	Execute68kTrap(0xa43d, &r);		// DrvrInstallRsrvMem()
	r.a[0] = ReadMacInt32(ReadMacInt32(0x11c) + ~DiskRefNum * 4);	// Get driver handle from Unit Table
	Execute68kTrap(0xa029, &r);		// HLock()
	uint32 dce = ReadMacInt32(r.a[0]);
	WriteMacInt32(dce + dCtlDriver, ROMBase + sony_offset + 0x100);
	WriteMacInt16(dce + dCtlFlags, DiskDriverFlags);

	// Open disk driver
	SheepString disk_str("\005.Disk");
	WriteMacInt32(pb + ioNamePtr, disk_str.addr());
	r.a[0] = pb;
	Execute68kTrap(0xa000, &r);		// Open()

	// Install CD-ROM driver unless nocdrom option given
	if (!PrefsFindBool("nocdrom")) {

		// Install CD-ROM driver
		r.a[0] = ROMBase + sony_offset + 0x200;
		r.d[0] = (uint32)CDROMRefNum;
		Execute68kTrap(0xa43d, &r);		// DrvrInstallRsrvMem()
		r.a[0] = ReadMacInt32(ReadMacInt32(0x11c) + ~CDROMRefNum * 4);	// Get driver handle from Unit Table
		Execute68kTrap(0xa029, &r);		// HLock()
		dce = ReadMacInt32(r.a[0]);
		WriteMacInt32(dce + dCtlDriver, ROMBase + sony_offset + 0x200);
		WriteMacInt16(dce + dCtlFlags, CDROMDriverFlags);

		// Open CD-ROM driver
		SheepString apple_cd("\010.AppleCD");
		WriteMacInt32(pb + ioNamePtr, apple_cd.addr());
		r.a[0] = pb;
		Execute68kTrap(0xa000, &r);		// Open()
	}

	// Install serial drivers
	r.a[0] = ROMBase + sony_offset + 0x300;
	r.d[0] = (uint32)-6;
	Execute68kTrap(0xa43d, &r);		// DrvrInstallRsrvMem()
	r.a[0] = ReadMacInt32(ReadMacInt32(0x11c) + ~(-6) * 4);	// Get driver handle from Unit Table
	Execute68kTrap(0xa029, &r);		// HLock()
	dce = ReadMacInt32(r.a[0]);
	WriteMacInt32(dce + dCtlDriver, ROMBase + sony_offset + 0x300);
	WriteMacInt16(dce + dCtlFlags, 0x4d00);

	r.a[0] = ROMBase + sony_offset + 0x400;
	r.d[0] = (uint32)-7;
	Execute68kTrap(0xa43d, &r);		// DrvrInstallRsrvMem()
	r.a[0] = ReadMacInt32(ReadMacInt32(0x11c) + ~(-7) * 4);	// Get driver handle from Unit Table
	Execute68kTrap(0xa029, &r);		// HLock()
	dce = ReadMacInt32(r.a[0]);
	WriteMacInt32(dce + dCtlDriver, ROMBase + sony_offset + 0x400);
	WriteMacInt16(dce + dCtlFlags, 0x4e00);

	r.a[0] = ROMBase + sony_offset + 0x500;
	r.d[0] = (uint32)-8;
	Execute68kTrap(0xa43d, &r);		// DrvrInstallRsrvMem()
	r.a[0] = ReadMacInt32(ReadMacInt32(0x11c) + ~(-8) * 4);	// Get driver handle from Unit Table
	Execute68kTrap(0xa029, &r);		// HLock()
	dce = ReadMacInt32(r.a[0]);
	WriteMacInt32(dce + dCtlDriver, ROMBase + sony_offset + 0x500);
	WriteMacInt16(dce + dCtlFlags, 0x4d00);

	r.a[0] = ROMBase + sony_offset + 0x600;
	r.d[0] = (uint32)-9;
	Execute68kTrap(0xa43d, &r);		// DrvrInstallRsrvMem()
	r.a[0] = ReadMacInt32(ReadMacInt32(0x11c) + ~(-9) * 4);	// Get driver handle from Unit Table
	Execute68kTrap(0xa029, &r);		// HLock()
	dce = ReadMacInt32(r.a[0]);
	WriteMacInt32(dce + dCtlDriver, ROMBase + sony_offset + 0x600);
	WriteMacInt16(dce + dCtlFlags, 0x4e00);
}
