/*
 *  video.cpp - Video/graphics emulation
 *
 *  Basilisk II (C) 1997-2000 Christian Bauer
 *  Portions (C) 1997-1999 Marc Hellwig
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
 */

#include <stdio.h>

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "main.h"
#include "macos_util.h"
#include "video.h"
#include "video_defs.h"

#define DEBUG 0
#include "debug.h"


// Description of the main monitor
video_desc VideoMonitor;

// Local variables (per monitor)
struct {
	video_desc *desc;			// Pointer to monitor description
	uint8 palette[256 * 3];		// Color palette, 256 entries, RGB
	bool luminance_mapping;		// Luminance mapping on/off
	bool interrupts_enabled;	// VBL interrupts on/off
} VidLocal;


/*
 *  Driver Open() routine
 */

int16 VideoDriverOpen(uint32 pb, uint32 dce)
{
	D(bug("VideoDriverOpen\n"));

	// Init local variables
	VidLocal.desc = &VideoMonitor;
	VidLocal.luminance_mapping = false;
	VidLocal.interrupts_enabled = false;

	// Init color palette (solid gray)
	if (!IsDirectMode(VidLocal.desc->mode)) {
		for (int i=0; i<256; i++) {
			VidLocal.palette[i * 3 + 0] = 127;
			VidLocal.palette[i * 3 + 1] = 127;
			VidLocal.palette[i * 3 + 2] = 127;
		}
		video_set_palette(VidLocal.palette);
	}
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

		case cscSetMode:		// Set color depth
			D(bug(" SetMode %04x\n", ReadMacInt16(param + csMode)));
			WriteMacInt32(param + csBaseAddr, VidLocal.desc->mac_frame_base);
			return noErr;

		case cscSetEntries: {	// Set palette
			D(bug(" SetEntries table %08lx, count %d, start %d\n", ReadMacInt32(param + csTable), ReadMacInt16(param + csCount), ReadMacInt16(param + csStart)));
			if (IsDirectMode(VidLocal.desc->mode))
				return controlErr;

			uint32 s_pal = ReadMacInt32(param + csTable);	// Source palette
			uint8 *d_pal;									// Destination palette
			uint16 count = ReadMacInt16(param + csCount);
			if (!s_pal || count > 255)
				return paramErr;

			if (ReadMacInt16(param + csStart) == 0xffff) {	// Indexed
				for (uint32 i=0; i<=count; i++) {
					d_pal = VidLocal.palette + ReadMacInt16(s_pal) * 3;
					uint8 red = (uint16)ReadMacInt16(s_pal + 2) >> 8;
					uint8 green = (uint16)ReadMacInt16(s_pal + 4) >> 8;
					uint8 blue = (uint16)ReadMacInt16(s_pal + 6) >> 8;
					if (VidLocal.luminance_mapping)
						red = green = blue = (red * 0x4ccc + green * 0x970a + blue * 0x1c29) >> 16;
					*d_pal++ = red;
					*d_pal++ = green;
					*d_pal++ = blue;
					s_pal += 8;
				}
			} else {										// Sequential
				d_pal = VidLocal.palette + ReadMacInt16(param + csStart) * 3;
				for (uint32 i=0; i<=count; i++) {
					uint8 red = (uint16)ReadMacInt16(s_pal + 2) >> 8;
					uint8 green = (uint16)ReadMacInt16(s_pal + 4) >> 8;
					uint8 blue = (uint16)ReadMacInt16(s_pal + 6) >> 8;
					if (VidLocal.luminance_mapping)
						red = green = blue = (red * 0x4ccc + green * 0x970a + blue * 0x1c29) >> 16;
					*d_pal++ = red;
					*d_pal++ = green;
					*d_pal++ = blue;
					s_pal += 8;
				}
			}
			video_set_palette(VidLocal.palette);
			return noErr;
		}

		case cscSetGamma:		// Set gamma table
			D(bug(" SetGamma\n"));
			return noErr;

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
			uint32 p = VidLocal.desc->mac_frame_base;
			uint32 pat = pattern[VidLocal.desc->mode];
			for (uint32 y=0; y<VidLocal.desc->y; y++) {
				uint32 p2 = p;
				for (uint32 x=0; x<VidLocal.desc->bytes_per_row / 4; x++) {
					WriteMacInt32(p2, pat);
					p2 += 4;
					if (VidLocal.desc->mode == VMODE_32BIT)
						pat = ~pat;
				}
				p += VidLocal.desc->bytes_per_row;
				pat = ~pat;
			}
			return noErr;
		}

		case cscSetGray:		// Enable/disable luminance mapping
			D(bug(" SetGray %02x\n", ReadMacInt8(param + csMode)));
			VidLocal.luminance_mapping = ReadMacInt8(param + csMode);
			return noErr;

		case cscSwitchMode:		// Switch video mode
			D(bug(" SwitchMode %04x, %08lx\n", ReadMacInt16(param + csMode), ReadMacInt32(param + csData)));
			WriteMacInt32(param + csBaseAddr, VidLocal.desc->mac_frame_base);
			return noErr;

		case cscSetInterrupt:	// Enable/disable VBL
			D(bug(" SetInterrupt %02x\n", ReadMacInt8(param + csMode)));
			VidLocal.interrupts_enabled = (ReadMacInt8(param + csMode) == 0);
			return noErr;

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

		case cscGetPageCnt:			// Get number of pages
			D(bug(" GetPageCnt\n"));
			WriteMacInt16(param + csPage, 1);
			return noErr;

		case cscGetPageBase:		// Get page base address
			D(bug(" GetPageBase\n"));
			WriteMacInt32(param + csBaseAddr, VidLocal.desc->mac_frame_base);
			return noErr;

		case cscGetGray:			// Get luminance mapping flag
			D(bug(" GetGray\n"));
			WriteMacInt8(param + csMode, VidLocal.luminance_mapping ? 1 : 0);
			return noErr;

		case cscGetInterrupt:		// Get interrupt disable flag
			D(bug(" GetInterrupt\n"));
			WriteMacInt8(param + csMode, VidLocal.interrupts_enabled ? 0 : 1);
			return noErr;

		case cscGetDefaultMode:		// Get default video mode
			D(bug(" GetDefaultMode\n"));
			WriteMacInt8(param + csMode, 0x80);
			return noErr;

		case cscGetCurMode:			// Get current video mode
			D(bug(" GetCurMode\n"));
			WriteMacInt16(param + csMode, 0x80);
			WriteMacInt32(param + csData, 0x80);
			WriteMacInt16(param + csPage, 0);
			WriteMacInt32(param + csBaseAddr, VidLocal.desc->mac_frame_base);
			return noErr;

		case cscGetConnection:		// Get monitor information
			D(bug(" GetConnection\n"));
			WriteMacInt16(param + csDisplayType, 6);		// 21" Multiscan
			WriteMacInt8(param + csConnectTaggedType, 6);
			WriteMacInt8(param + csConnectTaggedData, 0x23);
			WriteMacInt32(param + csConnectFlags, 0x03);	// All modes valid and safe
			WriteMacInt32(param + csDisplayComponent, 0);
			return noErr;

		case cscGetModeTiming:		// Get video timing for mode
			D(bug(" GetModeTiming mode %08lx\n", ReadMacInt32(param + csTimingMode)));
			WriteMacInt32(param + csTimingFormat, 'decl');
			WriteMacInt32(param + csTimingData, 220);		// 21" Multiscan
			WriteMacInt32(param + csTimingFlags, 0x0f);		// Mode valid, safe, default and shown in Monitors panel
			return noErr;

		case cscGetModeBaseAddress:	// Get frame buffer base address
			D(bug(" GetModeBaseAddress\n"));
			WriteMacInt32(param + csBaseAddr, VidLocal.desc->mac_frame_base);
			return noErr;

		default:
			printf("WARNING: Unknown VideoDriverStatus(%d)\n", code);
			return statusErr;
	}
}
