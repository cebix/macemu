/*
 *  video_blit.h - Video/graphics emulation, blitters
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
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

#ifndef DEFINE_VIDEO_BLITTERS

#ifndef VIDEO_BLIT_H
#define VIDEO_BLIT_H

// Format of the target visual
struct VisualFormat {
	bool	fullscreen;				// Full screen mode?
	int		depth;					// Screen depth
	uint32	Rmask, Gmask, Bmask;	// RGB mask values
	uint32	Rshift, Gshift, Bshift;	// RGB shift values
};

// Prototypes
extern void (*Screen_blit)(uint8 * dest, const uint8 * source, uint32 length);
extern bool Screen_blitter_init(VisualFormat const & visual_format, bool native_byte_order, int mac_depth);
extern uint32 ExpandMap[256];

// Glue for SheepShaver and BasiliskII
#ifdef SHEEPSHAVER
enum {
  VIDEO_DEPTH_1BIT = APPLE_1_BIT,
  VIDEO_DEPTH_2BIT = APPLE_2_BIT,
  VIDEO_DEPTH_4BIT = APPLE_4_BIT,
  VIDEO_DEPTH_8BIT = APPLE_8_BIT,
  VIDEO_DEPTH_16BIT = APPLE_16_BIT,
  VIDEO_DEPTH_32BIT = APPLE_32_BIT
};
#define VIDEO_MODE				VideoInfo
#define VIDEO_MODE_INIT			VideoInfo const & mode = VModes[cur_mode]
#define VIDEO_MODE_INIT_MONITOR	VIDEO_MODE_INIT
#define VIDEO_MODE_ROW_BYTES	mode.viRowBytes
#define VIDEO_MODE_X			mode.viXsize
#define VIDEO_MODE_Y			mode.viYsize
#define VIDEO_MODE_RESOLUTION	mode.viAppleID
#define VIDEO_MODE_DEPTH		mode.viAppleMode
#else
enum {
  VIDEO_DEPTH_1BIT = VDEPTH_1BIT,
  VIDEO_DEPTH_2BIT = VDEPTH_2BIT,
  VIDEO_DEPTH_4BIT = VDEPTH_4BIT,
  VIDEO_DEPTH_8BIT = VDEPTH_8BIT,
  VIDEO_DEPTH_16BIT = VDEPTH_16BIT,
  VIDEO_DEPTH_32BIT = VDEPTH_32BIT
};
#define VIDEO_MODE				video_mode
#define VIDEO_MODE_INIT			video_mode const & mode = drv->mode
#define VIDEO_MODE_INIT_MONITOR	video_mode const & mode = monitor.get_current_mode()
#define VIDEO_MODE_ROW_BYTES	mode.bytes_per_row
#define VIDEO_MODE_X			mode.x
#define VIDEO_MODE_Y			mode.y
#define VIDEO_MODE_RESOLUTION	mode.resolution_id
#define VIDEO_MODE_DEPTH		mode.depth
#endif

#endif /* VIDEO_BLIT_H */

#else

#ifndef FB_DEPTH
# error "Undefined screen depth"
#endif

#if !defined(FB_BLIT_1) && (FB_DEPTH <= 16)
# error "Undefined 16-bit word blit function"
#endif

#if !defined(FB_BLIT_2)
# error "Undefined 32-bit word blit function"
#endif

#if !defined(FB_BLIT_4)
# error "Undefined 64-bit word blit function"
#endif

static void FB_FUNC_NAME(uint8 * dest, const uint8 * source, uint32 length)
{
#define DEREF_WORD_PTR(ptr, ofs) (((uint16 *)(ptr))[(ofs)])
#define DEREF_LONG_PTR(ptr, ofs) (((uint32 *)(ptr))[(ofs)])
#define DEREF_QUAD_PTR(ptr, ofs) (((uint64 *)(ptr))[(ofs)])
	
#ifndef UNALIGNED_PROFITABLE
#if FB_DEPTH <= 8
	// Align source and dest to 16-bit word boundaries
	if (((unsigned long) source) & 1) {
		*dest++ = *source++;
		length -= 1;
	}
#endif
	
#if FB_DEPTH <= 16
	// Align source and dest to 32-bit word boundaries
	if (((unsigned long) source) & 2) {
		FB_BLIT_1(DEREF_WORD_PTR(dest, 0), DEREF_WORD_PTR(source, 0));
		dest += 2; source += 2;
		length -= 2;
	}
#endif
#endif
	
	// Blit 8-byte words
	if (length >= 8) {
		const int remainder = (length / 8) % 8;
		source += remainder * 8;
		dest += remainder * 8;
		
		int n = ((length / 8) + 7) / 8;
		switch (remainder) {
		case 0:	do {
				dest += 64; source += 64;
				FB_BLIT_4(DEREF_QUAD_PTR(dest, -8), DEREF_QUAD_PTR(source, -8));
		case 7: FB_BLIT_4(DEREF_QUAD_PTR(dest, -7), DEREF_QUAD_PTR(source, -7));
		case 6: FB_BLIT_4(DEREF_QUAD_PTR(dest, -6), DEREF_QUAD_PTR(source, -6));
		case 5: FB_BLIT_4(DEREF_QUAD_PTR(dest, -5), DEREF_QUAD_PTR(source, -5));
		case 4: FB_BLIT_4(DEREF_QUAD_PTR(dest, -4), DEREF_QUAD_PTR(source, -4));
		case 3: FB_BLIT_4(DEREF_QUAD_PTR(dest, -3), DEREF_QUAD_PTR(source, -3));
		case 2: FB_BLIT_4(DEREF_QUAD_PTR(dest, -2), DEREF_QUAD_PTR(source, -2));
		case 1: FB_BLIT_4(DEREF_QUAD_PTR(dest, -1), DEREF_QUAD_PTR(source, -1));
				} while (--n > 0);
		}
	}
	
	// There could be one long left to blit
	if (length & 4) {
		FB_BLIT_2(DEREF_LONG_PTR(dest, 0), DEREF_LONG_PTR(source, 0));
#if FB_DEPTH <= 16
		dest += 4;
		source += 4;
#endif
	}
	
#if FB_DEPTH <= 16
	// There could be one word left to blit
	if (length & 2) {
		FB_BLIT_1(DEREF_WORD_PTR(dest, 0), DEREF_WORD_PTR(source, 0));
#if FB_DEPTH <= 8
		dest += 2;
		source += 2;
#endif
	}
#endif
	
#if FB_DEPTH <= 8
	// There could be one byte left to blit
	if (length & 1)
		*dest = *source;
#endif
	
#undef DEREF_QUAD_PTR
#undef DEREF_LONG_PTR
#undef DEREF_WORD_PTR
}

#undef FB_FUNC_NAME

#ifdef FB_BLIT_1
#undef FB_BLIT_1
#endif

#ifdef FB_BLIT_2
#undef FB_BLIT_2
#endif

#ifdef FB_BLIT_4
#undef FB_BLIT_4
#endif

#ifdef FB_DEPTH
#undef FB_DEPTH
#endif

#endif /* DEFINE_VIDEO_BLITTERS */
