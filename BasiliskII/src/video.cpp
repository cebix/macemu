/*
 *  video.cpp - Video/graphics emulation
 *
 *  Basilisk II (C) 1997-2001 Christian Bauer
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


// List of supported video modes
vector<video_mode> VideoModes;

// Description of the main monitor
monitor_desc VideoMonitor;

// Local variables (per monitor)
struct {
	monitor_desc *desc;			// Pointer to description of monitor handled by this instance of the driver
	uint8 palette[256 * 3];		// Color palette, 256 entries, RGB
	bool luminance_mapping;		// Luminance mapping on/off
	bool interrupts_enabled;	// VBL interrupts on/off
	uint16 current_mode;		// Currently selected depth/resolution
	uint32 current_id;
	uint16 preferred_mode;		// Preferred depth/resolution
	uint32 preferred_id;
} VidLocal;


/*
 *  Check whether specified resolution ID is one of the supported resolutions
 */

static bool has_resolution(uint32 id)
{
	vector<video_mode>::const_iterator i = VideoModes.begin(), end = VideoModes.end();
	while (i != end) {
		if (i->resolution_id == id)
			return true;
		++i;
	}
	return false;
}


/*
 *  Find maximum supported depth for given resolution ID
 */

static video_depth max_depth_of_resolution(uint32 id)
{
	video_depth m = VDEPTH_1BIT;
	vector<video_mode>::const_iterator i = VideoModes.begin(), end = VideoModes.end();
	while (i != end) {
		if (i->depth > m)
			m = i->depth;
		++i;
	}
	return m;
}


/*
 *  Get X/Y size of specified resolution
 */

static void get_size_of_resolution(uint32 id, uint32 &x, uint32 &y)
{
	vector<video_mode>::const_iterator i = VideoModes.begin(), end = VideoModes.end();
	while (i != end) {
		if (i->resolution_id == id) {
			x = i->x;
			y = i->y;
			return;
		}
		++i;
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

		case cscSetMode: {		// Set color depth
			uint16 mode = ReadMacInt16(param + csMode);
			D(bug(" SetMode %04x\n", mode));
			//!! switch mode
			WriteMacInt32(param + csBaseAddr, VidLocal.desc->mac_frame_base);
			//!! VidLocal.current_mode = mode;
			if (mode != VidLocal.current_mode)
				return paramErr;
			else
				return noErr;
		}

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
			//!!
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
			uint32 pat = pattern[VidLocal.desc->mode.depth];
			for (uint32 y=0; y<VidLocal.desc->mode.y; y++) {
				for (uint32 x=0; x<VidLocal.desc->mode.bytes_per_row; x+=4) {
					WriteMacInt32(p + x, pat);
					if (VidLocal.desc->mode.depth == VDEPTH_32BIT)
						pat = ~pat;
				}
				p += VidLocal.desc->mode.bytes_per_row;
				pat = ~pat;
			}
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

		// case cscDirectSetEntries:

		case cscSetDefaultMode: { // Set default color depth
			uint16 mode = ReadMacInt16(param + csMode);
			D(bug(" SetDefaultMode %04x\n", mode));
			VidLocal.preferred_mode = mode;
			return noErr;
		}

		case cscSwitchMode: {	// Switch video mode (depth and resolution)
			uint16 mode = ReadMacInt16(param + csMode);
			uint32 id = ReadMacInt32(param + csData);
			D(bug(" SwitchMode %04x, %08x\n", mode, id));
			//!! switch mode
			WriteMacInt32(param + csBaseAddr, VidLocal.desc->mac_frame_base);
			//!! VidLocal.current_mode = mode;
			//!! VidLocal.current_id = id;
			if (mode != VidLocal.current_mode || id != VidLocal.current_id)
				return paramErr;
			else
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

		// case cscGetEntries:

		case cscGetPageCnt:			// Get number of pages
			D(bug(" GetPageCnt -> 1\n"));
			WriteMacInt16(param + csPage, 1);
			return noErr;

		case cscGetPageBase:		// Get page base address
			D(bug(" GetPageBase -> %08x\n", VidLocal.desc->mac_frame_base));
			WriteMacInt32(param + csBaseAddr, VidLocal.desc->mac_frame_base);
			return noErr;

		case cscGetGray:			// Get luminance mapping flag
			D(bug(" GetGray -> %d\n", VidLocal.luminance_mapping));
			WriteMacInt8(param, VidLocal.luminance_mapping ? 1 : 0);
			return noErr;

		case cscGetInterrupt:		// Get interrupt disable flag
			D(bug(" GetInterrupt -> %d\n", VidLocal.interrupts_enabled));
			WriteMacInt8(param, VidLocal.interrupts_enabled ? 0 : 1);
			return noErr;

		// case cscGetGamma:

		case cscGetDefaultMode:		// Get default color depth
			D(bug(" GetDefaultMode -> %04x\n", VidLocal.preferred_mode));
			WriteMacInt16(param + csMode, VidLocal.preferred_mode);
			return noErr;

		case cscGetCurMode:			// Get current video mode (depth and resolution)
			D(bug(" GetCurMode -> %04x/%08x\n", VidLocal.current_mode, VidLocal.current_id));
			WriteMacInt16(param + csMode, VidLocal.current_mode);
			WriteMacInt32(param + csData, VidLocal.current_id);
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

		case cscGetModeBaseAddress:	// Get frame buffer base address
			D(bug(" GetModeBaseAddress -> %08x\n", VidLocal.desc->mac_frame_base));
			WriteMacInt32(param + csBaseAddr, VidLocal.desc->mac_frame_base);	// Base address of video RAM for the current DisplayModeID and relative bit depth
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
			uint32 flags = 0xb; // mode valid, safe and shown in Monitors panel
			if (id == VidLocal.preferred_id)
				flags |= 4; // default mode
			WriteMacInt32(param + csResolutionFlags, flags);
			return noErr;
		}

		case cscGetVideoParameters: {	// Get information about specified resolution/depth
			uint32 id = ReadMacInt32(param + csDisplayModeID);
			uint16 mode = ReadMacInt16(param + csDepthMode);
			D(bug(" GetVideoParameters %04x/%08x\n", mode, id));

			vector<video_mode>::const_iterator i = VideoModes.begin(), end = VideoModes.end();
			while (i != end) {
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
					WriteMacInt32(vp + vpHRes, 0x00480000);
					WriteMacInt32(vp + vpVRes, 0x00480000);
					uint32 pix_type, pix_size, cmp_count, cmp_size, dev_type;
					switch (i->depth) {
						case VDEPTH_1BIT:
							pix_type = 0; pix_size = 1;
							cmp_count = 1; cmp_size = 1;
							dev_type = 0; // CLUT
							break;
						case VDEPTH_2BIT:
							pix_type = 0; pix_size = 2;
							cmp_count = 1; cmp_size = 2;
							dev_type = 0; // CLUT
							break;
						case VDEPTH_4BIT:
							pix_type = 0; pix_size = 4;
							cmp_count = 1; cmp_size = 4;
							dev_type = 0; // CLUT
							break;
						case VDEPTH_8BIT:
							pix_type = 0; pix_size = 8;
							cmp_count = 1; cmp_size = 8;
							dev_type = 0; // CLUT
							break;
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
					}
					WriteMacInt16(vp + vpPixelType, pix_type);
					WriteMacInt16(vp + vpPixelSize, pix_size);
					WriteMacInt16(vp + vpCmpCount, cmp_count);
					WriteMacInt16(vp + vpCmpSize, cmp_size);
					WriteMacInt32(param + csPageCount, 1);
					WriteMacInt32(param + csDeviceType, dev_type);
					return noErr;
				}
				++i;
			}
			return paramErr; // specified resolution/depth not supported
		}

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

		default:
			printf("WARNING: Unknown VideoDriverStatus(%d)\n", code);
			return statusErr;
	}
}
