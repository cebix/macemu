/*
 *  video.h - Video/graphics emulation
 *
 *  Basilisk II (C) 1997-1999 Christian Bauer
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

// Description for one (possibly virtual) monitor
enum {
	VMODE_1BIT,
	VMODE_2BIT,
	VMODE_4BIT,
	VMODE_8BIT,
	VMODE_16BIT,
	VMODE_32BIT
};

#define IsDirectMode(x) ((x) == VMODE_16BIT || (x) == VMODE_32BIT)

struct video_desc {
	uint32 mac_frame_base;	// Mac frame buffer address
	uint32 bytes_per_row;	// Bytes per row
	uint32 x;				// X size of screen (pixels)
	uint32 y;				// Y size of screen (pixels)
	int mode;				// Video mode
};

extern struct video_desc VideoMonitor;	// Description of the main monitor, set by VideoInit()

extern int16 VideoDriverOpen(uint32 pb, uint32 dce);
extern int16 VideoDriverControl(uint32 pb, uint32 dce);
extern int16 VideoDriverStatus(uint32 pb, uint32 dce);

// System specific and internal functions/data
extern bool VideoInit(bool classic);
extern void VideoExit(void);

extern void VideoQuitFullScreen(void);

extern void VideoInterrupt(void);

extern void video_set_palette(uint8 *pal);

#endif
