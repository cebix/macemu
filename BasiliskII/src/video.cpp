/*
 *  video.cpp - Video/graphics emulation
 *
 *  Basilisk II (C) 1997-2002 Christian Bauer
 *  Portions written by Marc Hellwig
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
 *  SEE ALSO
 *    Inside Macintosh: Devices, chapter 1 "Device Manager"
 *    Designing Cards and Drivers for the Macintosh Family, Second Edition
 *    Designing PCI Cards and Drivers for Power Macintosh Computers
 *    Display Device Driver Guide
 */

#include <stdio.h>

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "main.h"
#include "macos_util.h"
#include "slot_rom.h"
#include "video.h"
#include "video_defs.h"

#define DEBUG 0
#include "debug.h"


// List of supported video modes
vector<video_mode> VideoModes;

// Description of the main monitor
monitor_desc VideoMonitor;

// Depth code -> Apple mode mapping
uint16 apple_mode_for_depth[6];

// Local variables (per monitor)
struct {
	monitor_desc *desc;			// Pointer to description of monitor handled by this instance of the driver
	uint8 palette[256 * 3];		// Color palette, 256 entries, RGB
	bool luminance_mapping;		// Luminance mapping on/off
	bool interrupts_enabled;	// VBL interrupts on/off
	bool dm_present;			// We received a GetVideoParameters call, so the Display Manager seems to be present
	uint32 gamma_table;			// Mac address of gamma table
	int alloc_gamma_table_size;	// Allocated size of gamma table
	uint16 current_mode;		// Currently selected depth/resolution
	uint32 current_id;
	uint16 preferred_mode;		// Preferred depth/resolution
	uint32 preferred_id;
	uint32 slot_param;			// Mac address of Slot Manager parameter block
} VidLocal;


/*
 *  Initialize apple_mode_for_depth[] array from VideoModes list
 */

void video_init_depth_list(void)
{
	uint16 mode = 0x80;
	for (int depth = VDEPTH_1BIT; depth <= VDEPTH_32BIT; depth++) {
		if (video_has_depth(video_depth(depth)))
			apple_mode_for_depth[depth] = mode++;
		else
			apple_mode_for_depth[depth] = 0;
	}
}


/*
 *  Check whether a mode with the specified depth exists
 */

bool video_has_depth(video_depth depth)
{
	vector<video_mode>::const_iterator i = VideoModes.begin(), end = VideoModes.end();
	while (i != end) {
		if (i->depth == depth)
			return true;
		++i;
	}
	return false;
}


/*
 *  Check whether specified resolution ID is one of the supported resolutions
 */

static bool has_resolution(uint32 id)
{
	vector<video_mode>::const_iterator i, end = VideoModes.end();
	for (i = VideoModes.begin(); i != end; ++i) {
		if (i->resolution_id == id)
			return true;
	}
	return false;
}


/*
 *  Find specified mode (depth/resolution) (or VideoModes.end() if not found)
 */

static vector<video_mode>::const_iterator find_mode(uint16 mode, uint32 id)
{
	vector<video_mode>::const_iterator i, end = VideoModes.end();
	for (i = VideoModes.begin(); i != end; ++i) {
		if (i->resolution_id == id && DepthToAppleMode(i->depth) == mode)
			return i;
	}
	return i;
}


/*
 *  Find maximum supported depth for given resolution ID
 */

static video_depth max_depth_of_resolution(uint32 id)
{
	video_depth m = VDEPTH_1BIT;
	vector<video_mode>::const_iterator i, end = VideoModes.end();
	for (i = VideoModes.begin(); i != end; ++i) {
		if (i->depth > m)
			m = i->depth;
	}
	return m;
}


/*
 *  Get X/Y size of specified resolution
 */

static void get_size_of_resolution(uint32 id, uint32 &x, uint32 &y)
{
	vector<video_mode>::const_iterator i, end = VideoModes.end();
	for (i = VideoModes.begin(); i != end; ++i) {
		if (i->resolution_id == id) {
			x = i->x;
			y = i->y;
			return;
		}
	}
	x = y = 0;
}


/*
 *  Get bytes-per-row value for specified resolution/depth
 */

uint32 video_bytes_per_row(video_depth depth, uint32 id)
{
	vector<video_mode>::const_iterator i, end = VideoModes.end();
	for (i = VideoModes.begin(); i != end; ++i) {
		if (i->depth == depth && i->resolution_id == id)
			return i->bytes_per_row;
	}
	uint32 x, y;
	get_size_of_resolution(id, x, y);
	return TrivialBytesPerRow(x, depth);
}


/*
 *  Find palette size for given color depth
 */

static int palette_size(video_depth depth)
{
	switch (depth) {
		case VDEPTH_1BIT: return 2;
		case VDEPTH_2BIT: return 4;
		case VDEPTH_4BIT: return 16;
		case VDEPTH_8BIT: return 256;
		case VDEPTH_16BIT: return 32;
		case VDEPTH_32BIT: return 256;
		default: return 0;
	}
}


/*
 *  Set palette to 50% gray
 */

static void set_gray_palette(void)
{
	for (int i=0; i<256; i++) {
		VidLocal.palette[i * 3 + 0] = 127;
		VidLocal.palette[i * 3 + 1] = 127;
		VidLocal.palette[i * 3 + 2] = 127;
	}
	video_set_palette(VidLocal.palette, 256);
}


/*
 *  Load gamma-corrected black-to-white ramp to palette for direct-color mode
 */

static void load_ramp_palette(void)
{
	// Find tables for gamma correction
	uint8 *red_gamma = NULL, *green_gamma = NULL, *blue_gamma = NULL;
	bool have_gamma = false;
	int data_width = 0;
	if (VidLocal.gamma_table) {
		uint32 table = VidLocal.gamma_table;
		red_gamma = Mac2HostAddr(table + gFormulaData + ReadMacInt16(table + gFormulaSize));
		int chan_cnt = ReadMacInt16(table + gChanCnt);
		if (chan_cnt == 1)
			green_gamma = blue_gamma = red_gamma;
		else {
			int ofs = ReadMacInt16(table + gDataCnt);
			green_gamma = red_gamma + ofs;
			blue_gamma = green_gamma + ofs;
		}
		data_width = ReadMacInt16(table + gDataWidth);
		have_gamma = true;
	}

	int num = (VidLocal.desc->mode.depth == VDEPTH_16BIT ? 32 : 256);
	uint8 *p = VidLocal.palette;
	for (int i=0; i<num; i++) {
		uint8 red = (i * 256 / num), green = red, blue = red;
		if (have_gamma) {
			red = red_gamma[red >> (8 - data_width)];
			green = green_gamma[green >> (8 - data_width)];
			blue = blue_gamma[blue >> (8 - data_width)];
		}
		*p++ = red;
		*p++ = green;
		*p++ = blue;
	}

	video_set_palette(VidLocal.palette, num);
}


/*
 *  Allocate gamma table of specified size
 */

static bool allocate_gamma_table(int size)
{
	M68kRegisters r;

	if (size > VidLocal.alloc_gamma_table_size) {
		if (VidLocal.gamma_table) {
			r.a[0] = VidLocal.gamma_table;
			Execute68kTrap(0xa01f, &r);	// DisposePtr()
			VidLocal.gamma_table = 0;
			VidLocal.alloc_gamma_table_size = 0;
		}
		r.d[0] = size;
		Execute68kTrap(0xa71e, &r);	// NewPtrSysClear()
		if (r.a[0] == 0)
			return false;
		VidLocal.gamma_table = r.a[0];
		VidLocal.alloc_gamma_table_size = size;
	}
	return true;
}


/*
 *  Set gamma table (0 = build linear ramp)
 */

static bool set_gamma_table(uint32 user_table)
{
	if (user_table == 0) { // Build linear ramp, 256 entries

		// Allocate new table, if necessary
		if (!allocate_gamma_table(SIZEOF_GammaTbl + 256))
			return memFullErr;
		uint32 table = VidLocal.gamma_table;

		// Initialize header
		WriteMacInt16(table + gVersion, 0);
		WriteMacInt16(table + gType, 0);
		WriteMacInt16(table + gFormulaSize, 0);
		WriteMacInt16(table + gChanCnt, 1);
		WriteMacInt16(table + gDataCnt, 256);
		WriteMacInt16(table + gDataWidth, 8);

		// Build ramp
		uint32 p = table + gFormulaData;
		for (int i=0; i<256; i++)
			WriteMacInt8(p + i, i);

	} else { // User-supplied gamma table

		// Validate header
		if (ReadMacInt16(user_table + gVersion))
			return paramErr;
		if (ReadMacInt16(user_table + gType))
			return paramErr;
		int chan_cnt = ReadMacInt16(user_table + gChanCnt);
		if (chan_cnt != 1 && chan_cnt != 3)
			return paramErr;
		int data_width = ReadMacInt16(user_table + gDataWidth);
		if (data_width > 8)
			return paramErr;
		int data_cnt = ReadMacInt16(user_table + gDataCnt);
		if (data_cnt != (1 << data_width))
			return paramErr;

		// Allocate new table, if necessary
		int size = SIZEOF_GammaTbl + ReadMacInt16(user_table + gFormulaSize) + chan_cnt * data_cnt;
		if (!allocate_gamma_table(size))
			return memFullErr;
		uint32 table = VidLocal.gamma_table;

		// Copy table
		Mac2Mac_memcpy(table, user_table, size);
	}

	if (IsDirectMode(VidLocal.desc->mode))
		load_ramp_palette();

	return true;
}


/*
 *  Switch video mode
 */

static void switch_mode(const video_mode &mode, uint32 param, uint32 dce)
{
	// Switch mode
	set_gray_palette();
	video_switch_to_mode(mode);

	// Update VidLocal
	VidLocal.current_mode = DepthToAppleMode(mode.depth);
	VidLocal.current_id = mode.resolution_id;

	uint32 frame_base = VidLocal.desc->mac_frame_base;

	M68kRegisters r;
	uint32 sp = VidLocal.slot_param;
	r.a[0] = sp;

	// Find functional sResource for this display
	WriteMacInt8(sp + spSlot, ReadMacInt8(dce + dCtlSlot));
	WriteMacInt8(sp + spID, ReadMacInt8(dce + dCtlSlotId));
	WriteMacInt8(sp + spExtDev, 0);
	r.d[0] = 0x0016;
	Execute68kTrap(0xa06e, &r);	// SRsrcInfo()
	uint32 rsrc = ReadMacInt32(sp + spPointer);

	// Patch minorBase (otherwise rebooting won't work)
	WriteMacInt8(sp + spID, 0x0a); // minorBase
	r.d[0] = 0x0006;
	Execute68kTrap(0xa06e, &r); // SFindStruct()
	uint32 minor_base = ReadMacInt32(sp + spPointer) - ROMBaseMac;
	ROMBaseHost[minor_base + 0] = frame_base >> 24;
	ROMBaseHost[minor_base + 1] = frame_base >> 16;
	ROMBaseHost[minor_base + 2] = frame_base >> 8;
	ROMBaseHost[minor_base + 3] = frame_base;

	// Patch video mode parameter table
	WriteMacInt32(sp + spPointer, rsrc);
	WriteMacInt8(sp + spID, DepthToAppleMode(mode.depth));
	r.d[0] = 0x0006;
	Execute68kTrap(0xa06e, &r); // SFindStruct()
	WriteMacInt8(sp + spID, 0x01);
	r.d[0] = 0x0006;
	Execute68kTrap(0xa06e, &r); // SFindStruct()
	uint32 p = ReadMacInt32(sp + spPointer) - ROMBaseMac;
	ROMBaseHost[p +  8] = mode.bytes_per_row >> 8;
	ROMBaseHost[p +  9] = mode.bytes_per_row;
	ROMBaseHost[p + 14] = mode.y >> 8;
	ROMBaseHost[p + 15] = mode.y;
	ROMBaseHost[p + 16] = mode.x >> 8;
	ROMBaseHost[p + 17] = mode.x;

	// Recalculate slot ROM checksum
	ChecksumSlotROM();

	// Update sResource
	WriteMacInt8(sp + spID, ReadMacInt8(dce + dCtlSlotId));
	r.d[0] = 0x002b;
	Execute68kTrap(0xa06e, &r); // SUpdateSRT()

	// Update frame buffer base in DCE and param block
	WriteMacInt32(dce + dCtlDevBase, frame_base);
	WriteMacInt32(param + csBaseAddr, frame_base);

	// Patch frame buffer base address for MacOS versions <7.6
	if (!VidLocal.dm_present) { // Only do this when no Display Manager seems to be present; otherwise, the screen will not get redrawn
		WriteMacInt32(0x824, frame_base);			// ScrnBase
		WriteMacInt32(0x898, frame_base);			// CrsrBase
		uint32 gdev = ReadMacInt32(0x8a4);			// MainDevice
		gdev = ReadMacInt32(gdev);
		uint32 pmap = ReadMacInt32(gdev + 0x16);	// gdPMap
		pmap = ReadMacInt32(pmap);
		WriteMacInt32(pmap, frame_base);			// baseAddr
	}
}


/*
 *  Driver Open() routine
 */

int16 VideoDriverOpen(uint32 pb, uint32 dce)
{
	D(bug("VideoDriverOpen\n"));

	// This shouldn't happen unless the platform-specific video code is broken
	if (VideoModes.empty())
		fprintf(stderr, "No valid video modes found (broken video driver?)\n");

	// Init local variables
	VidLocal.desc = &VideoMonitor;
	VidLocal.luminance_mapping = false;
	VidLocal.interrupts_enabled = false;
	VidLocal.current_mode = DepthToAppleMode(VidLocal.desc->mode.depth);
	VidLocal.current_id = VidLocal.desc->mode.resolution_id;
	VidLocal.preferred_mode = VidLocal.current_mode;
	VidLocal.preferred_id = VidLocal.current_id;
	VidLocal.dm_present = false;

	// Allocate Slot Manager parameter block in Mac RAM
	M68kRegisters r;
	r.d[0] = SIZEOF_SPBlock;
	Execute68kTrap(0xa71e, &r);	// NewPtrSysClear()
	if (r.a[0] == 0)
		return memFullErr;
	VidLocal.slot_param = r.a[0];
	D(bug("SPBlock at %08x\n", VidLocal.slot_param));

	// Find and set default gamma table
	VidLocal.gamma_table = 0;
	VidLocal.alloc_gamma_table_size = 0;
	set_gamma_table(0);

	// Init color palette (solid gray)
	set_gray_palette();
	return noErr;
}


/*
 *  Driver Control() routine
 */

int16 VideoDriverControl(uint32 pb, uint32 dce)
{
	uint16 code = ReadMacInt16(pb + csCode);
	uint32 param = ReadMacInt32(pb + csParam);
	D(bug("VideoDriverControl %d\n", code));
	switch (code) {

		case cscSetMode: {		// Set color depth
			uint16 mode = ReadMacInt16(param + csMode);
			D(bug(" SetMode %04x\n", mode));

			// Set old base address in case the switch fails
			WriteMacInt32(param + csBaseAddr, VidLocal.desc->mac_frame_base);

			if (ReadMacInt16(param + csPage))
				return paramErr;

			if (mode != VidLocal.current_mode) {
				vector<video_mode>::const_iterator i = find_mode(mode, VidLocal.current_id);
				if (i == VideoModes.end())
					return paramErr;
				switch_mode(*i, param, dce);
			}
			D(bug("  base %08x\n", VidLocal.desc->mac_frame_base));
			return noErr;
		}

		case cscSetEntries:		// Set palette
		case cscDirectSetEntries: {
			D(bug(" (Direct)SetEntries table %08x, count %d, start %d\n", ReadMacInt32(param + csTable), ReadMacInt16(param + csCount), ReadMacInt16(param + csStart)));
			bool is_direct = IsDirectMode(VidLocal.desc->mode);
			if (code == cscSetEntries && is_direct)
				return controlErr;
			if (code == cscDirectSetEntries && !is_direct)
				return controlErr;

			uint32 s_pal = ReadMacInt32(param + csTable);	// Source palette
			uint8 *d_pal;									// Destination palette
			uint16 start = ReadMacInt16(param + csStart);
			uint16 count = ReadMacInt16(param + csCount);
			if (s_pal == 0 || count > 255)
				return paramErr;

			// Find tables for gamma correction
			uint8 *red_gamma = NULL, *green_gamma = NULL, *blue_gamma = NULL;
			bool have_gamma = false;
			int data_width = 0;
			if (VidLocal.gamma_table) {
				uint32 table = VidLocal.gamma_table;
				red_gamma = Mac2HostAddr(table + gFormulaData + ReadMacInt16(table + gFormulaSize));
				int chan_cnt = ReadMacInt16(table + gChanCnt);
				if (chan_cnt == 1)
					green_gamma = blue_gamma = red_gamma;
				else {
					int ofs = ReadMacInt16(table + gDataCnt);
					green_gamma = red_gamma + ofs;
					blue_gamma = green_gamma + ofs;
				}
				data_width = ReadMacInt16(table + gDataWidth);
				have_gamma = true;
			}

			// Convert palette
			if (start == 0xffff) {	// Indexed
				for (uint32 i=0; i<=count; i++) {
					d_pal = VidLocal.palette + (ReadMacInt16(s_pal) & 0xff) * 3;
					uint8 red = (uint16)ReadMacInt16(s_pal + 2) >> 8;
					uint8 green = (uint16)ReadMacInt16(s_pal + 4) >> 8;
					uint8 blue = (uint16)ReadMacInt16(s_pal + 6) >> 8;
					if (VidLocal.luminance_mapping && !is_direct)
						red = green = blue = (red * 0x4ccc + green * 0x970a + blue * 0x1c29) >> 16;
					if (have_gamma) {
						red = red_gamma[red >> (8 - data_width)];
						green = green_gamma[green >> (8 - data_width)];
						blue = blue_gamma[blue >> (8 - data_width)];
					}
					*d_pal++ = red;
					*d_pal++ = green;
					*d_pal++ = blue;
					s_pal += 8;
				}
			} else {				// Sequential
				if (start + count > 255)
					return paramErr;
				d_pal = VidLocal.palette + start * 3;
				for (uint32 i=0; i<=count; i++) {
					uint8 red = (uint16)ReadMacInt16(s_pal + 2) >> 8;
					uint8 green = (uint16)ReadMacInt16(s_pal + 4) >> 8;
					uint8 blue = (uint16)ReadMacInt16(s_pal + 6) >> 8;
					if (VidLocal.luminance_mapping && !is_direct)
						red = green = blue = (red * 0x4ccc + green * 0x970a + blue * 0x1c29) >> 16;
					if (have_gamma) {
						red = red_gamma[red >> (8 - data_width)];
						green = green_gamma[green >> (8 - data_width)];
						blue = blue_gamma[blue >> (8 - data_width)];
					}
					*d_pal++ = red;
					*d_pal++ = green;
					*d_pal++ = blue;
					s_pal += 8;
				}
			}
			video_set_palette(VidLocal.palette, palette_size(VidLocal.desc->mode.depth));
			return noErr;
		}

		case cscSetGamma: {		// Set gamma table
			uint32 user_table = ReadMacInt32(param + csGTable);
			D(bug(" SetGamma %08x\n", user_table));
			return set_gamma_table(user_table) ? noErr : memFullErr;
		}

		case cscGrayPage: {		// Fill page with dithered gray pattern
			D(bug(" GrayPage %d\n", ReadMacInt16(param + csPage)));
			if (ReadMacInt16(param + csPage))
				return paramErr;

			uint32 pattern[6] = {
				0xaaaaaaaa,		// 1 bpp
				0xcccccccc,		// 2 bpp
				0xf0f0f0f0,		// 4 bpp
				0xff00ff00,		// 8 bpp
				0xffff0000,		// 16 bpp
				0xffffffff		// 32 bpp
			};
			uint32 frame_base = VidLocal.desc->mac_frame_base;
			uint32 pat = pattern[VidLocal.desc->mode.depth];
			bool invert = (VidLocal.desc->mode.depth == VDEPTH_32BIT);
			for (uint32 y=0; y<VidLocal.desc->mode.y; y++) {
				for (uint32 x=0; x<VidLocal.desc->mode.bytes_per_row; x+=4) {
					WriteMacInt32(frame_base + x, pat);
					if (invert)
						pat = ~pat;
				}
				frame_base += VidLocal.desc->mode.bytes_per_row;
				pat = ~pat;
			}

			if (IsDirectMode(VidLocal.desc->mode))
				load_ramp_palette();

			return noErr;
		}

		case cscSetGray:		// Enable/disable luminance mapping
			D(bug(" SetGray %02x\n", ReadMacInt8(param + csMode)));
			VidLocal.luminance_mapping = ReadMacInt8(param + csMode);
			return noErr;

		case cscSetInterrupt:	// Enable/disable VBL
			D(bug(" SetInterrupt %02x\n", ReadMacInt8(param + csMode)));
			VidLocal.interrupts_enabled = (ReadMacInt8(param + csMode) == 0);
			return noErr;

		case cscSetDefaultMode: { // Set default color depth
			uint16 mode = ReadMacInt8(param + csMode);
			D(bug(" SetDefaultMode %02x\n", mode));
			VidLocal.preferred_mode = mode;
			return noErr;
		}

		case cscSwitchMode: {	// Switch video mode (depth and resolution)
			uint16 mode = ReadMacInt16(param + csMode);
			uint32 id = ReadMacInt32(param + csData);
			D(bug(" SwitchMode %04x, %08x\n", mode, id));

			// Set old base address in case the switch fails
			WriteMacInt32(param + csBaseAddr, VidLocal.desc->mac_frame_base);

			if (ReadMacInt16(param + csPage))
				return paramErr;

			if (mode != VidLocal.current_mode || id != VidLocal.current_id) {
				vector<video_mode>::const_iterator i = find_mode(mode, id);
				if (i == VideoModes.end())
					return paramErr;
				switch_mode(*i, param, dce);
			}
			D(bug("  base %08x\n", VidLocal.desc->mac_frame_base));
			return noErr;
		}

		case cscSavePreferredConfiguration: {
			uint16 mode = ReadMacInt16(param + csMode);
			uint32 id = ReadMacInt32(param + csData);
			D(bug(" SavePreferredConfiguration %04x, %08x\n", mode, id));
			VidLocal.preferred_mode = mode;
			VidLocal.preferred_id = id;
			return noErr;
		}

		default:
			printf("WARNING: Unknown VideoDriverControl(%d)\n", code);
			return controlErr;
	}
}


/*
 *  Driver Status() routine
 */

int16 VideoDriverStatus(uint32 pb, uint32 dce)
{
	uint16 code = ReadMacInt16(pb + csCode);
	uint32 param = ReadMacInt32(pb + csParam);
	D(bug("VideoDriverStatus %d\n", code));
	switch (code) {

		case cscGetMode:			// Get current color depth
			D(bug(" GetMode -> %04x, base %08x\n", VidLocal.current_mode, VidLocal.desc->mac_frame_base));
			WriteMacInt16(param + csMode, VidLocal.current_mode);
			WriteMacInt16(param + csPage, 0);
			WriteMacInt32(param + csBaseAddr, VidLocal.desc->mac_frame_base);
			return noErr;

		case cscGetEntries: {		// Read palette
			D(bug(" GetEntries table %08x, count %d, start %d\n", ReadMacInt32(param + csTable), ReadMacInt16(param + csCount), ReadMacInt16(param + csStart)));

			uint8 *s_pal;									// Source palette
			uint32 d_pal = ReadMacInt32(param + csTable);	// Destination palette
			uint16 start = ReadMacInt16(param + csStart);
			uint16 count = ReadMacInt16(param + csCount);
			if (d_pal == 0 || count > 255)
				return paramErr;

			if (start == 0xffff) {	// Indexed
				for (uint32 i=0; i<=count; i++) {
					s_pal = VidLocal.palette + (ReadMacInt16(d_pal) & 0xff) * 3;
					uint8 red = *s_pal++;
					uint8 green = *s_pal++;
					uint8 blue = *s_pal++;
					WriteMacInt16(d_pal + 2, red * 0x0101);
					WriteMacInt16(d_pal + 4, green * 0x0101);
					WriteMacInt16(d_pal + 6, blue * 0x0101);
					d_pal += 8;
				}
			} else {				// Sequential
				if (start + count > 255)
					return paramErr;
				s_pal = VidLocal.palette + start * 3;
				for (uint32 i=0; i<=count; i++) {
					uint8 red = *s_pal++;
					uint8 green = *s_pal++;
					uint8 blue = *s_pal++;
					WriteMacInt16(d_pal + 2, red * 0x0101);
					WriteMacInt16(d_pal + 4, green * 0x0101);
					WriteMacInt16(d_pal + 6, blue * 0x0101);
					d_pal += 8;
				}
			}
			return noErr;
		}

		case cscGetPages:			// Get number of pages
			D(bug(" GetPages -> 1\n"));
			WriteMacInt16(param + csPage, 1);
			return noErr;

		case cscGetBaseAddress:		// Get page base address
			D(bug(" GetBaseAddress -> %08x\n", VidLocal.desc->mac_frame_base));
			WriteMacInt32(param + csBaseAddr, VidLocal.desc->mac_frame_base);
			if (ReadMacInt16(param + csPage))
				return paramErr;
			else
				return noErr;

		case cscGetGray:			// Get luminance mapping flag
			D(bug(" GetGray -> %d\n", VidLocal.luminance_mapping));
			WriteMacInt8(param, VidLocal.luminance_mapping ? 1 : 0);
			return noErr;

		case cscGetInterrupt:		// Get interrupt disable flag
			D(bug(" GetInterrupt -> %d\n", VidLocal.interrupts_enabled));
			WriteMacInt8(param, VidLocal.interrupts_enabled ? 0 : 1);
			return noErr;

		case cscGetGamma:
			D(bug(" GetGamma -> %08x\n", VidLocal.gamma_table));
			WriteMacInt32(param + csGTable, VidLocal.gamma_table);
			return noErr;

		case cscGetDefaultMode:		// Get default color depth
			D(bug(" GetDefaultMode -> %02x\n", VidLocal.preferred_mode));
			WriteMacInt8(param + csMode, VidLocal.preferred_mode);
			return noErr;

		case cscGetCurrentMode:		// Get current video mode (depth and resolution)
			D(bug(" GetCurMode -> %04x/%08x, base %08x\n", VidLocal.current_mode, VidLocal.current_id, VidLocal.desc->mac_frame_base));
			WriteMacInt16(param + csMode, VidLocal.current_mode);
			WriteMacInt32(param + csData, VidLocal.current_id);
			WriteMacInt16(param + csPage, 0);
			WriteMacInt32(param + csBaseAddr, VidLocal.desc->mac_frame_base);
			return noErr;

		case cscGetConnection:		// Get monitor information
			D(bug(" GetConnection\n"));
			WriteMacInt16(param + csDisplayType, 8);		// Modeless connection
			WriteMacInt8(param + csConnectTaggedType, 0);
			WriteMacInt8(param + csConnectTaggedData, 0);
			WriteMacInt32(param + csConnectFlags, 0x43);	// All modes valid and safe, non-standard tagging
			WriteMacInt32(param + csDisplayComponent, 0);
			return noErr;

		case cscGetModeTiming: {	// Get video timing for specified resolution
			uint32 id = ReadMacInt32(param + csTimingMode);
			D(bug(" GetModeTiming %08x\n", id));
			if (!has_resolution(id))
				return paramErr;

			WriteMacInt32(param + csTimingFormat, FOURCC('d', 'e', 'c', 'l'));
			WriteMacInt32(param + csTimingData, 0);	// unknown
			uint32 flags = 0xb; // mode valid, safe and shown in Monitors panel
			if (id == VidLocal.preferred_id)
				flags |= 4; // default mode
			WriteMacInt32(param + csTimingFlags, flags);
			return noErr;
		}

		case cscGetModeBaseAddress:	// Get frame buffer base address
			D(bug(" GetModeBaseAddress -> base %08x\n", VidLocal.desc->mac_frame_base));
			WriteMacInt32(param + csBaseAddr, VidLocal.desc->mac_frame_base);
			return noErr;

		case cscGetPreferredConfiguration: // Get default video mode (depth and resolution)
			D(bug(" GetPreferredConfiguration -> %04x/%08x\n", VidLocal.preferred_mode, VidLocal.preferred_id));
			WriteMacInt16(param + csMode, VidLocal.preferred_mode);
			WriteMacInt32(param + csData, VidLocal.preferred_id);
			return noErr;

		case cscGetNextResolution: {	// Called iteratively to obtain a list of all supported resolutions
			uint32 id = ReadMacInt32(param + csPreviousDisplayModeID);
			D(bug(" GetNextResolution %08x\n", id));

			switch (id) {
				case 0:
					// Return current resolution
					id = VidLocal.current_id;
					break;

				case 0xfffffffe:
					// Return first supported resolution
					id = 0x80;
					while (!has_resolution(id))
						id++;
					break;

				default:
					// Get next resolution
					if (!has_resolution(id))
						return paramErr;
					id++;
					while (!has_resolution(id) && id < 0x100)
						id++;
					if (id == 0x100) { // No more resolutions
						WriteMacInt32(param + csRIDisplayModeID, 0xfffffffd);
						return noErr;
					}
					break;
			}

			WriteMacInt32(param + csRIDisplayModeID, id);
			uint32 x, y;
			get_size_of_resolution(id, x, y);
			WriteMacInt32(param + csHorizontalPixels, x);
			WriteMacInt32(param + csVerticalLines, y);
			WriteMacInt32(param + csRefreshRate, 75 << 16);
			WriteMacInt16(param + csMaxDepthMode, DepthToAppleMode(max_depth_of_resolution(id)));
			WriteMacInt32(param + csResolutionFlags, 0);
			return noErr;
		}

		case cscGetVideoParameters: {	// Get information about specified resolution/depth
			uint32 id = ReadMacInt32(param + csDisplayModeID);
			uint16 mode = ReadMacInt16(param + csDepthMode);
			D(bug(" GetVideoParameters %04x/%08x\n", mode, id));
			VidLocal.dm_present = true;	// Display Manager seems to be present

			vector<video_mode>::const_iterator i, end = VideoModes.end();
			for (i = VideoModes.begin(); i != end; ++i) {
				if (DepthToAppleMode(i->depth) == mode && i->resolution_id == id) {
					uint32 vp = ReadMacInt32(param + csVPBlockPtr);
					WriteMacInt32(vp + vpBaseOffset, 0);
					WriteMacInt16(vp + vpRowBytes, i->bytes_per_row);
					WriteMacInt16(vp + vpBounds, 0);
					WriteMacInt16(vp + vpBounds + 2, 0);
					WriteMacInt16(vp + vpBounds + 4, i->y);
					WriteMacInt16(vp + vpBounds + 6, i->x);
					WriteMacInt16(vp + vpVersion, 0);
					WriteMacInt16(vp + vpPackType, 0);
					WriteMacInt32(vp + vpPackSize, 0);
					WriteMacInt32(vp + vpHRes, 0x00480000);	// 72 dpi
					WriteMacInt32(vp + vpVRes, 0x00480000);
					uint32 pix_type, pix_size, cmp_count, cmp_size, dev_type;
					switch (i->depth) {
						case VDEPTH_16BIT:
							pix_type = 0x10; pix_size = 16;
							cmp_count = 3; cmp_size = 5;
							dev_type = 2; // direct
							break;
						case VDEPTH_32BIT:
							pix_type = 0x10; pix_size = 32;
							cmp_count = 3; cmp_size = 8;
							dev_type = 2; // direct
							break;
						default:
							pix_type = 0; pix_size = 1 << i->depth;
							cmp_count = 1; cmp_size = 1 << i->depth;
							dev_type = 0; // CLUT
							break;
					}
					WriteMacInt16(vp + vpPixelType, pix_type);
					WriteMacInt16(vp + vpPixelSize, pix_size);
					WriteMacInt16(vp + vpCmpCount, cmp_count);
					WriteMacInt16(vp + vpCmpSize, cmp_size);
					WriteMacInt32(param + csPageCount, 1);
					WriteMacInt32(param + csDeviceType, dev_type);
					return noErr;
				}
			}
			return paramErr; // specified resolution/depth not supported
		}

		case cscGetMultiConnect: {
			uint32 conn = ReadMacInt32(param + csDisplayCountOrNumber);
			D(bug(" GetMultiConnect %08x\n", conn));
			if (conn == 0xffffffff) {	// Get number of connections
				WriteMacInt32(param + csDisplayCountOrNumber, 1); // Single-headed
				return noErr;
			} else if (conn == 1) {		// Get information about first connection
				WriteMacInt16(param + csConnectInfo + csDisplayType, 8);		// Modeless connection
				WriteMacInt8(param + csConnectInfo + csConnectTaggedType, 0);
				WriteMacInt8(param + csConnectInfo + csConnectTaggedData, 0);
				WriteMacInt32(param + csConnectInfo + csConnectFlags, 0x43);	// All modes valid and safe, non-standard tagging
				WriteMacInt32(param + csConnectInfo + csDisplayComponent, 0);
				return noErr;
			} else
				return paramErr;
		}

		default:
			printf("WARNING: Unknown VideoDriverStatus(%d)\n", code);
			return statusErr;
	}
}
