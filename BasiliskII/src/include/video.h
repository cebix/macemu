/*
 *  video.h - Video/graphics emulation
 *
 *  Basilisk II (C) 1997-2001 Christian Bauer
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

#ifndef VIDEO_H
#define VIDEO_H

#include <vector>

/*
   Some of the terminology here is completely frelled. In Basilisk II, a
   "video mode" refers to a combination of resolution and color depth, and
   this information is stored in a video_mode structure. In Apple
   documentation, a "mode" historically refers to the color depth only
   (because old Macs had fixed-frequency monitors and could not change the
   resolution). These "modes" are assigned a number (0x80, 0x81, etc.),
   which we here call "Apple mode". When Macs learned how to deal with
   multiscan monitors, Apple introduced another type of "mode", also having
   numbers starting from 0x80 but refrerring to the resolution and/or video
   timing of the display (it's possible to have two modes with the same
   dimension but different refresh rates). We call this a "resolution ID". 
   The combination of "Apple mode" and "ID" corresponds to a Basilisk II
   "video mode". To make the confusion worse, the video driver control call
   that sets the color depth is called "SetMode" while the one that sets
   both depth and resolution is "SwitchMode"...
*/

// Color depth codes
enum video_depth {
	VDEPTH_1BIT,  // 2 colors
	VDEPTH_2BIT,  // 4 colors
	VDEPTH_4BIT,  // 16 colors
	VDEPTH_8BIT,  // 256 colors
	VDEPTH_16BIT, // "Thousands"
	VDEPTH_32BIT  // "Millions"
};

inline uint16 DepthToAppleMode(video_depth depth)
{
	return depth + 0x80;
}

inline video_depth AppleModeToDepth(uint16 mode)
{
	return video_depth(mode - 0x80);
}

inline bool IsDirectMode(video_depth depth)
{
	return depth == VDEPTH_16BIT || depth == VDEPTH_32BIT;
}

inline bool IsDirectMode(uint16 mode)
{
	return IsDirectMode(AppleModeToDepth(mode));
}

// Return the depth code that corresponds to the specified bits-per-pixel value
inline video_depth DepthModeForPixelDepth(int depth)
{
	switch (depth) {
		case 1: return VDEPTH_1BIT;
		case 2: return VDEPTH_2BIT;
		case 4: return VDEPTH_4BIT;
		case 8: return VDEPTH_8BIT;
		case 15: case 16: return VDEPTH_16BIT;
		case 24: case 32: return VDEPTH_32BIT;
		default: return VDEPTH_1BIT;
	}
}

// Return a bytes-per-row value (assuming no padding) for the specified depth and pixel width
inline uint32 TrivialBytesPerRow(uint32 width, video_depth depth)
{
	switch (depth) {
		case VDEPTH_1BIT: return width / 8;
		case VDEPTH_2BIT: return width / 4;
		case VDEPTH_4BIT: return width / 2;
		case VDEPTH_8BIT: return width;
		case VDEPTH_16BIT: return width * 2;
		case VDEPTH_32BIT: return width * 4;
	}
}

/*
   You are not completely free in your selection of depth/resolution
   combinations:
     1) the lowest supported color depth must be available in all
        resolutions
     2) if one resolution provides a certain color depth, it must also
        provide all lower supported depths

   For example, it is possible to have this set of modes:
     640x480 @ 8 bit
     640x480 @ 32 bit
     800x600 @ 8 bit
     800x600 @ 32 bit
     1024x768 @ 8 bit

   But this is not possible (violates rule 1):
     640x480 @ 8 bit
     800x600 @ 8 bit
     1024x768 @ 1 bit

   And neither is this (violates rule 2, 640x480 @ 16 bit is missing):
     640x480 @ 8 bit
     640x480 @ 32 bit
     800x600 @ 8 bit
     800x600 @ 16 bit
     1024x768 @ 8 bit
*/

// Description of a video mode
struct video_mode {
	uint32 x;				// X size of screen (pixels)
	uint32 y;				// Y size of screen (pixels)
	uint32 resolution_id;	// Resolution ID (should be >= 0x80 and uniquely identify the sets of modes with the same X/Y size)
	uint32 bytes_per_row;	// Bytes per row of frame buffer
	video_depth depth;		// Color depth (see definitions above)
};

inline bool IsDirectMode(const video_mode &mode)
{
	return IsDirectMode(mode.depth);
}

// List of all supported video modes
extern std::vector<video_mode> VideoModes;

// Description for one (possibly virtual) monitor
struct monitor_desc {
	uint32 mac_frame_base;	// Mac frame buffer address
	video_mode mode;		// Currently selected video mode description
};

// Description of the main (and currently the only) monitor, set by VideoInit()
extern monitor_desc VideoMonitor;

extern int16 VideoDriverOpen(uint32 pb, uint32 dce);
extern int16 VideoDriverControl(uint32 pb, uint32 dce);
extern int16 VideoDriverStatus(uint32 pb, uint32 dce);

// System specific and internal functions/data
extern bool VideoInit(bool classic);
extern void VideoExit(void);

extern void VideoQuitFullScreen(void);

extern void VideoInterrupt(void);
extern void VideoRefresh(void);

// Called by the video driver to switch the video mode
extern void video_switch_to_mode(const video_mode &mode);

// Called by the video driver to set the color palette (in indexed modes)
// or gamma table (in direct modes)
extern void video_set_palette(uint8 *pal);

#endif
