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

// Color depth codes
enum video_depth {
	VDEPTH_1BIT,  // 2 colors
	VDEPTH_2BIT,  // 4 colors
	VDEPTH_4BIT,  // 16 colors
	VDEPTH_8BIT,  // 256 colors
	VDEPTH_16BIT, // "Thousands"
	VDEPTH_32BIT  // "Millions"
};

inline bool IsDirectMode(video_depth depth)
{
	return depth == VDEPTH_16BIT || depth == VDEPTH_32BIT;
}

inline uint16 DepthToAppleMode(video_depth depth)
{
	return depth + 0x80;
}

inline video_depth AppleModeToDepth(uint16 mode)
{
	return video_depth(mode - 0x80);
}

// Description of one video mode
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
extern vector<video_mode> VideoModes;

// Description for one (possibly virtual) monitor
struct monitor_desc {
	uint32 mac_frame_base;	// Mac frame buffer address
	video_mode mode;		// Currently selected video mode description
};

extern monitor_desc VideoMonitor;	// Description of the main monitor, set by VideoInit()

extern int16 VideoDriverOpen(uint32 pb, uint32 dce);
extern int16 VideoDriverControl(uint32 pb, uint32 dce);
extern int16 VideoDriverStatus(uint32 pb, uint32 dce);

// System specific and internal functions/data
extern bool VideoInit(bool classic);
extern void VideoExit(void);

extern void VideoQuitFullScreen(void);

extern void VideoInterrupt(void);
extern void VideoRefresh(void);

extern void video_set_palette(uint8 *pal);

#endif
