/*
 *  video_blit.cpp - Video/graphics emulation, blitters
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

#include "sysdeps.h"
#include "video.h"
#include "video_blit.h"

#include <stdio.h>
#include <stdlib.h>

// Format of the target visual
static VisualFormat visualFormat;

// This holds the pixels values of the palette colors for 8->16/32-bit expansion
uint32 ExpandMap[256];

// Mark video_blit.h for specialization
#define DEFINE_VIDEO_BLITTERS 1

/* -------------------------------------------------------------------------- */
/* --- Raw Copy / No conversion required                                  --- */
/* -------------------------------------------------------------------------- */

static void Blit_Copy_Raw(uint8 * dest, const uint8 * source, uint32 length)
{
	// This function is likely to be inlined and/or highly optimized
	memcpy(dest, source, length);
}

/* -------------------------------------------------------------------------- */
/* --- RGB 555                                                            --- */
/* -------------------------------------------------------------------------- */

#ifdef WORDS_BIGENDIAN
# define FB_FUNC_NAME Blit_RGB555_OBO
#else
# define FB_FUNC_NAME Blit_RGB555_NBO
#endif

#define FB_BLIT_1(dst, src) \
	(dst = (((src) >> 8) & 0xff) | (((src) & 0xff) << 8))
	
#define FB_BLIT_2(dst, src) \
	(dst = (((src) >> 8) & 0x00ff00ff) | (((src) & 0x00ff00ff) << 8))

#define FB_BLIT_4(dst, src) \
	(dst =	(((src) >> 8) & UVAL64(0x00ff00ff00ff00ff)) | \
			(((src) & UVAL64(0x00ff00ff00ff00ff)) << 8))

#define	FB_DEPTH 15
#include "video_blit.h"

/* -------------------------------------------------------------------------- */
/* --- BGR 555                                                            --- */
/* -------------------------------------------------------------------------- */

#ifdef WORDS_BIGENDIAN

// Native byte order

#define FB_BLIT_1(dst, src) \
	(dst = (((src) >> 10) & 0x001f) | ((src) & 0x03e0) | (((src) << 10) & 0x7c00))

#define FB_BLIT_2(dst, src) \
	(dst = (((src) >> 10) & 0x001f001f) | ((src) & 0x03e003e0) | (((src) << 10) & 0x7c007c00))

#define FB_BLIT_4(dst, src) \
	(dst =	(((src) >> 10) & UVAL64(0x001f001f001f001f)) | \
			( (src)        & UVAL64(0x03e003e003e003e0)) | \
			(((src) << 10) & UVAL64(0x7c007c007c007c00)))

#define FB_DEPTH 15
#define FB_FUNC_NAME Blit_BGR555_NBO
#include "video_blit.h"

// Opposite byte order (untested)

#define FB_BLIT_1(dst, src) \
	(dst = (((src) >> 2) & 0x1f00) | (((src) >> 8) & 3) | (((src) << 8) & 0xe000) | (((src) << 2) & 0x7c))

#define FB_BLIT_2(dst, src) \
	(dst = (((src) >> 2) & 0x1f001f00) | (((src) >> 8) & 0x30003) | (((src) << 8) & 0xe000e000) | (((src) << 2) & 0x7c007c))

#define FB_BLIT_4(dst, src) \
	(dst =	(((src) >> 2) & UVAL64(0x1f001f001f001f00)) | \
			(((src) >> 8) & UVAL64(0x0003000300030003)) | \
			(((src) << 8) & UVAL64(0xe000e000e000e000)) | \
			(((src) << 2) & UVAL64(0x007c007c007c007c)))

#define FB_DEPTH 15
#define FB_FUNC_NAME Blit_BGR555_OBO
#include "video_blit.h"

#else

// Native byte order (untested)

#define FB_BLIT_1(dst, src) \
	(dst = (((src) >> 2) & 0x1f) | (((src) >> 8) & 0xe0) | (((src) << 8) & 0x0300) | (((src) << 2) & 0x7c00))

#define FB_BLIT_2(dst, src) \
	(dst = (((src) >> 2) & 0x1f001f) | (((src) >> 8) & 0xe000e0) | (((src) << 8) & 0x03000300) | (((src) << 2) & 0x7c007c00))

#define FB_BLIT_4(dst, src) \
	(dst =	(((src) >> 2) & UVAL64(0x001f001f001f001f)) | \
			(((src) >> 8) & UVAL64(0x00e000e000e000e0)) | \
			(((src) << 8) & UVAL64(0x0300030003000300)) | \
			(((src) << 2) & UVAL64(0x7c007c007c007c00)))

#define FB_DEPTH 15
#define FB_FUNC_NAME Blit_BGR555_NBO
#include "video_blit.h"

// Opposite byte order (untested)

#define FB_BLIT_1(dst, src) \
	(dst = (((src) << 6) & 0x1f00) | ((src) & 0xe003) | (((src) >> 6) & 0x7c))

#define FB_BLIT_2(dst, src) \
	(dst = (((src) << 6) & 0x1f001f00) | ((src) & 0xe003e003) | (((src) >> 6) & 0x7c007c))

#define FB_BLIT_4(dst, src) \
	(dst =	(((src) << 6) & UVAL64(0x1f001f001f001f00)) | \
			( (src)       & UVAL64(0xe003e003e003e003)) | \
			(((src) >> 6) & UVAL64(0x007c007c007c007c)))

#define FB_DEPTH 15
#define FB_FUNC_NAME Blit_BGR555_OBO
#include "video_blit.h"

#endif

/* -------------------------------------------------------------------------- */
/* --- RGB 565                                                            --- */
/* -------------------------------------------------------------------------- */

#ifdef WORDS_BIGENDIAN

// Native byte order

#define FB_BLIT_1(dst, src) \
	(dst = (((src) & 0x1f) | (((src) << 1) & 0xffc0)))

#define FB_BLIT_2(dst, src) \
	(dst = (((src) & 0x001f001f) | (((src) << 1) & 0xffc0ffc0)))

#define FB_BLIT_4(dst, src) \
	(dst =	(((src)       & UVAL64(0x001f001f001f001f)) | \
			(((src) << 1) & UVAL64(0xffc0ffc0ffc0ffc0))))

#define FB_DEPTH 16
#define FB_FUNC_NAME Blit_RGB565_NBO
#include "video_blit.h"

// Opposite byte order

#define FB_BLIT_1(dst, src) \
	(dst = ((((src) >> 7) & 0xff) | (((src) << 9) & 0xc000) | (((src) << 8) & 0x1f00)))

#define FB_BLIT_2(dst, src) \
	(dst = ((((src) >> 7) & 0x00ff00ff) | (((src) << 9) & 0xc000c000) | (((src) << 8) & 0x1f001f00)))

#define FB_BLIT_4(dst, src) \
	(dst =	(((src) >> 7) & UVAL64(0x00ff00ff00ff00ff)) | \
			(((src) << 9) & UVAL64(0xc000c000c000c000)) | \
			(((src) << 8) & UVAL64(0x1f001f001f001f00)))

#define FB_DEPTH 16
#define FB_FUNC_NAME Blit_RGB565_OBO
#include "video_blit.h"

#else

// Native byte order

#define FB_BLIT_1(dst, src) \
	(dst = (((src) >> 8) & 0x001f) | (((src) << 9) & 0xfe00) | (((src) >> 7) & 0x01c0))
	
#define FB_BLIT_2(dst, src) \
	(dst = (((src) >> 8) & 0x001f001f) | (((src) << 9) & 0xfe00fe00) | (((src) >> 7) & 0x01c001c0))

#define FB_BLIT_4(dst, src) \
	(dst =	(((src) >> 8) & UVAL64(0x001f001f001f001f)) | \
			(((src) << 9) & UVAL64(0xfe00fe00fe00fe00)) | \
			(((src) >> 7) & UVAL64(0x01c001c001c001c0)))

#define FB_DEPTH 16
#define FB_FUNC_NAME Blit_RGB565_NBO
#include "video_blit.h"

// Opposite byte order (untested)

#define FB_BLIT_1(dst, src) \
	(dst = (((src) & 0x1f00) | (((src) << 1) & 0xe0fe) | (((src) >> 15) & 1)))

#define FB_BLIT_2(dst, src) \
	(dst = (((src) & 0x1f001f00) | (((src) << 1) & 0xe0fee0fe) | (((src) >> 15) & 0x10001)))

#define FB_BLIT_4(dst, src) \
	(dst =	(((src)        & UVAL64(0x1f001f001f001f00)) | \
			(((src) <<  1) & UVAL64(0xe0fee0fee0fee0fe)) | \
			(((src) >> 15) & UVAL64(0x0001000100010001))))

#define FB_DEPTH 16
#define FB_FUNC_NAME Blit_RGB565_OBO
#include "video_blit.h"

#endif

/* -------------------------------------------------------------------------- */
/* --- RGB 888                                                            --- */
/* -------------------------------------------------------------------------- */

#ifdef WORDS_BIGENDIAN
# define FB_FUNC_NAME Blit_RGB888_OBO
#else
# define FB_FUNC_NAME Blit_RGB888_NBO
#endif

#define FB_BLIT_2(dst, src) \
	(dst = (((src) >> 24) & 0xff) | (((src) >> 8) & 0xff00) | (((src) & 0xff00) << 8) | (((src) & 0xff) << 24))

#define FB_BLIT_4(dst, src) \
	(dst =	(((src) >> 24) & UVAL64(0x000000ff000000ff)) | \
			(((src) >>  8) & UVAL64(0x0000ff000000ff00)) | \
			(((src) & UVAL64(0x0000ff000000ff00)) <<  8) | \
			(((src) & UVAL64(0x000000ff000000ff)) << 24))

#define FB_DEPTH 24
#include "video_blit.h"

/* -------------------------------------------------------------------------- */
/* --- BGR 888                                                            --- */
/* -------------------------------------------------------------------------- */

// Native byte order [BE] (untested)

#ifdef WORDS_BIGENDIAN

#define FB_BLIT_2(dst, src) \
	(dst = (((src) >> 16) & 0xff) | ((src) & 0xff00) | (((src) & 0xff) << 16))

#define FB_BLIT_4(dst, src) \
	(dst =	(((src) >> 16) & UVAL64(0x000000ff000000ff)) | \
			( (src)        & UVAL64(0x0000ff000000ff00)) | \
			(((src) & UVAL64(0x000000ff000000ff)) << 16))

#define FB_FUNC_NAME Blit_BGR888_NBO
#define FB_DEPTH 24
#include "video_blit.h"

#else

// Opposite byte order [LE] (untested)

#define FB_BLIT_2(dst, src) \
	(dst = (((src) >> 16) & 0xff) | ((src) & 0xff0000) | (((src) & 0xff) << 16))

#define FB_BLIT_4(dst, src) \
	(dst =	(((src) >> 16) & UVAL64(0x000000ff000000ff)) | \
			( (src)        & UVAL64(0x00ff000000ff0000)) | \
			(((src) & UVAL64(0x000000ff000000ff)) << 16))

#define FB_FUNC_NAME Blit_BGR888_OBO
#define FB_DEPTH 24
#include "video_blit.h"

#endif

// Opposite byte order [BE] (untested) / Native byte order [LE] (untested)

#ifdef WORDS_BIGENDIAN
# define FB_FUNC_NAME Blit_BGR888_OBO
#else
# define FB_FUNC_NAME Blit_BGR888_NBO
#endif

#define FB_BLIT_2(dst, src) \
	(dst = ((src) & 0xff00ff) | (((src) & 0xff00) << 16))

#define FB_BLIT_4(dst, src) \
	(dst = ((src) & UVAL64(0x00ff00ff00ff00ff)) | (((src) & UVAL64(0x0000ff000000ff00)) << 16))

#define FB_DEPTH 24
#include "video_blit.h"

/* -------------------------------------------------------------------------- */
/* --- 1/2/4-bit indexed to 8-bit mode conversion                         --- */
/* -------------------------------------------------------------------------- */

static void Blit_Expand_1_To_8(uint8 * dest, const uint8 * p, uint32 length)
{
	uint8 *q = (uint8 *)dest;
	for (uint32 i=0; i<length; i++) {
		uint8 c = *p++;
		*q++ = c >> 7;
		*q++ = (c >> 6) & 1;
		*q++ = (c >> 5) & 1;
		*q++ = (c >> 4) & 1;
		*q++ = (c >> 3) & 1;
		*q++ = (c >> 2) & 1;
		*q++ = (c >> 1) & 1;
		*q++ = c & 1;
	}
}

static void Blit_Expand_2_To_8(uint8 * dest, const uint8 * p, uint32 length)
{
	uint8 *q = (uint8 *)dest;
	for (uint32 i=0; i<length; i++) {
		uint8 c = *p++;
		*q++ = c >> 6;
		*q++ = (c >> 4) & 3;
		*q++ = (c >> 2) & 3;
		*q++ = c & 3;
	}
}

static void Blit_Expand_4_To_8(uint8 * dest, const uint8 * p, uint32 length)
{
	uint8 *q = (uint8 *)dest;
	for (uint32 i=0; i<length; i++) {
		uint8 c = *p++;
		*q++ = c >> 4;
		*q++ = c & 0x0f;
	}
}

/* -------------------------------------------------------------------------- */
/* --- 1/2/4/8-bit indexed to 16-bit mode color expansion                 --- */
/* -------------------------------------------------------------------------- */

static void Blit_Expand_1_To_16(uint8 * dest, const uint8 * p, uint32 length)
{
	uint16 *q = (uint16 *)dest;
	for (uint32 i=0; i<length; i++) {
		uint8 c = *p++;
		*q++ = -(c >> 7);
		*q++ = -((c >> 6) & 1);
		*q++ = -((c >> 5) & 1);
		*q++ = -((c >> 4) & 1);
		*q++ = -((c >> 3) & 1);
		*q++ = -((c >> 2) & 1);
		*q++ = -((c >> 1) & 1);
		*q++ = -(c & 1);
	}
}

static void Blit_Expand_2_To_16(uint8 * dest, const uint8 * p, uint32 length)
{
	uint16 *q = (uint16 *)dest;
	for (uint32 i=0; i<length; i++) {
		uint8 c = *p++;
		*q++ = ExpandMap[c >> 6];
		*q++ = ExpandMap[c >> 4];
		*q++ = ExpandMap[c >> 2];
		*q++ = ExpandMap[c];
	}
}

static void Blit_Expand_4_To_16(uint8 * dest, const uint8 * p, uint32 length)
{
	uint16 *q = (uint16 *)dest;
	for (uint32 i=0; i<length; i++) {
		uint8 c = *p++;
		*q++ = ExpandMap[c >> 4];
		*q++ = ExpandMap[c];
	}
}

static void Blit_Expand_8_To_16(uint8 * dest, const uint8 * p, uint32 length)
{
	uint16 *q = (uint16 *)dest;
	for (uint32 i=0; i<length; i++)
		*q++ = ExpandMap[*p++];
}

/* -------------------------------------------------------------------------- */
/* --- 1/2/4/8-bit indexed to 32-bit mode color expansion                 --- */
/* -------------------------------------------------------------------------- */

static void Blit_Expand_1_To_32(uint8 * dest, const uint8 * p, uint32 length)
{
	uint32 *q = (uint32 *)dest;
	for (uint32 i=0; i<length; i++) {
		uint8 c = *p++;
		*q++ = -(c >> 7);
		*q++ = -((c >> 6) & 1);
		*q++ = -((c >> 5) & 1);
		*q++ = -((c >> 4) & 1);
		*q++ = -((c >> 3) & 1);
		*q++ = -((c >> 2) & 1);
		*q++ = -((c >> 1) & 1);
		*q++ = -(c & 1);
	}
}

static void Blit_Expand_2_To_32(uint8 * dest, const uint8 * p, uint32 length)
{
	uint32 *q = (uint32 *)dest;
	for (uint32 i=0; i<length; i++) {
		uint8 c = *p++;
		*q++ = ExpandMap[c >> 6];
		*q++ = ExpandMap[c >> 4];
		*q++ = ExpandMap[c >> 2];
		*q++ = ExpandMap[c];
	}
}

static void Blit_Expand_4_To_32(uint8 * dest, const uint8 * p, uint32 length)
{
	uint32 *q = (uint32 *)dest;
	for (uint32 i=0; i<length; i++) {
		uint8 c = *p++;
		*q++ = ExpandMap[c >> 4];
		*q++ = ExpandMap[c];
	}
}

static void Blit_Expand_8_To_32(uint8 * dest, const uint8 * p, uint32 length)
{
	uint32 *q = (uint32 *)dest;
	for (uint32 i=0; i<length; i++)
		*q++ = ExpandMap[*p++];
}

/* -------------------------------------------------------------------------- */
/* --- Blitters to the host frame buffer, or XImage buffer                --- */
/* -------------------------------------------------------------------------- */

// Function used to update the hosst frame buffer (DGA), or an XImage buffer (WIN)
// --> Shall be initialized only through the Screen_blitter_init() function
typedef void (*Screen_blit_func)(uint8 * dest, const uint8 * source, uint32 length);
Screen_blit_func Screen_blit = 0;

// Structure used to match the adequate framebuffer update function
struct Screen_blit_func_info {
	int					depth;			// Screen depth
	uint32				Rmask;			// Red mask
	uint32				Gmask;			// Green mask
	uint32				Bmask;			// Blue mask
	Screen_blit_func	handler_nbo;	// Update function (native byte order)
	Screen_blit_func	handler_obo;	// Update function (opposite byte order)
};

// Table of visual formats supported and their respective handler
static Screen_blit_func_info Screen_blitters[] = {
#ifdef WORDS_BIGENDIAN
	{  1, 0x000000, 0x000000, 0x000000, Blit_Copy_Raw	, Blit_Copy_Raw		},	// NT
	{  8, 0x000000, 0x000000, 0x000000, Blit_Copy_Raw	, Blit_Copy_Raw		},	// OK (NBO)
	{ 15, 0x007c00, 0x0003e0, 0x00001f, Blit_Copy_Raw	, Blit_RGB555_OBO	},	// OK (OBO)
	{ 15, 0x00001f, 0x0003e0, 0x007c00, Blit_BGR555_NBO	, Blit_BGR555_OBO	},	// NT
	{ 16, 0x007c00, 0x0003e0, 0x00001f, Blit_Copy_Raw	, Blit_RGB555_OBO	},	// OK (OBO)
	{ 16, 0x00f800, 0x0007e0, 0x00001f, Blit_RGB565_NBO	, Blit_RGB565_OBO	},	// OK (OBO)
	{ 24, 0xff0000, 0x00ff00, 0x0000ff, Blit_Copy_Raw	, Blit_RGB888_OBO	},	// OK (OBO)
	{ 24, 0x0000ff, 0x00ff00, 0xff0000, Blit_BGR888_NBO	, Blit_BGR888_OBO	},	// NT
	{ 32, 0xff0000, 0x00ff00, 0x0000ff, Blit_Copy_Raw	, Blit_RGB888_OBO	},	// OK
	{ 32, 0x0000ff, 0x00ff00, 0xff0000, Blit_BGR888_NBO	, Blit_BGR888_OBO	}	// OK
#else
	{  1, 0x000000, 0x000000, 0x000000, Blit_Copy_Raw	, Blit_Copy_Raw		},	// NT
	{  8, 0x000000, 0x000000, 0x000000, Blit_Copy_Raw	, Blit_Copy_Raw		},	// OK (NBO)
	{ 15, 0x007c00, 0x0003e0, 0x00001f, Blit_RGB555_NBO	, Blit_Copy_Raw		},	// OK (NBO)
	{ 15, 0x00001f, 0x0003e0, 0x007c00, Blit_BGR555_NBO	, Blit_BGR555_OBO	},	// NT
	{ 16, 0x007c00, 0x0003e0, 0x00001f, Blit_RGB555_NBO	, Blit_Copy_Raw		},	// OK (NBO)
	{ 16, 0x00f800, 0x0007e0, 0x00001f, Blit_RGB565_NBO	, Blit_RGB565_OBO	},	// OK (NBO)
	{ 24, 0xff0000, 0x00ff00, 0x0000ff, Blit_RGB888_NBO	, Blit_Copy_Raw		},	// OK (NBO)
	{ 24, 0x0000ff, 0x00ff00, 0xff0000, Blit_BGR888_NBO	, Blit_BGR888_OBO	},	// NT
	{ 32, 0xff0000, 0x00ff00, 0x0000ff, Blit_RGB888_NBO	, Blit_Copy_Raw		},	// OK (NBO)
	{ 32, 0x0000ff, 0x00ff00, 0xff0000, Blit_BGR888_NBO	, Blit_BGR888_OBO	}	// NT
#endif
};

// Initialize the framebuffer update function
// Returns FALSE, if the function was to be reduced to a simple memcpy()
// --> In that case, VOSF is not necessary
bool Screen_blitter_init(VisualFormat const & visual_format, bool native_byte_order, int mac_depth)
{
#if USE_SDL_VIDEO
	const bool use_sdl_video = true;
#else
	const bool use_sdl_video = false;
#endif
#if REAL_ADDRESSING || DIRECT_ADDRESSING
	if (mac_depth == 1 && !use_sdl_video && !visual_format.fullscreen) {

		// Windowed 1-bit mode uses a 1-bit X image, so there's no need for special blitting routines
		Screen_blit = Blit_Copy_Raw;

	} else {

		// Compute RGB shift values
		visualFormat = visual_format;
		visualFormat.Rshift = 0;
		for (uint32 Rmask = visualFormat.Rmask; Rmask && ((Rmask & 1) != 1); Rmask >>= 1)
			++visualFormat.Rshift;
		visualFormat.Gshift = 0;
		for (uint32 Gmask = visualFormat.Gmask; Gmask && ((Gmask & 1) != 1); Gmask >>= 1)
			++visualFormat.Gshift;
		visualFormat.Bshift = 0;
		for (uint32 Bmask = visualFormat.Bmask; Bmask && ((Bmask & 1) != 1); Bmask >>= 1)
			++visualFormat.Bshift;

		// 1/2/4/8-bit mode on 8/16/32-bit screen?
		Screen_blit = NULL;
		switch (visualFormat.depth) {
		case 8:
			switch (mac_depth) {
			case 1: Screen_blit = Blit_Expand_1_To_8; break;
			case 2: Screen_blit = Blit_Expand_2_To_8; break;
			case 4: Screen_blit = Blit_Expand_4_To_8; break;
			}
			break;
		case 15:
		case 16:
			switch (mac_depth) {
			case 1: Screen_blit = Blit_Expand_1_To_16; break;
			case 2: Screen_blit = Blit_Expand_2_To_16; break;
			case 4: Screen_blit = Blit_Expand_4_To_16; break;
			case 8: Screen_blit = Blit_Expand_8_To_16; break;
			}
			break;
		case 24:
		case 32:
			switch (mac_depth) {
			case 1: Screen_blit = Blit_Expand_1_To_32; break;
			case 2: Screen_blit = Blit_Expand_2_To_32; break;
			case 4: Screen_blit = Blit_Expand_4_To_32; break;
			case 8: Screen_blit = Blit_Expand_8_To_32; break;
			}
			break;
		}
		bool blitter_found = (Screen_blit != NULL);
	
		// Search for an adequate blit function
		const int blitters_count = sizeof(Screen_blitters)/sizeof(Screen_blitters[0]);
		for (int i = 0; !blitter_found && (i < blitters_count); i++) {
			if	(	(visualFormat.depth == Screen_blitters[i].depth)
				&&	(visualFormat.Rmask == Screen_blitters[i].Rmask)
				&&	(visualFormat.Gmask == Screen_blitters[i].Gmask)
				&&	(visualFormat.Bmask == Screen_blitters[i].Bmask)
				)
			{
				blitter_found = true;
				Screen_blit = native_byte_order
							? Screen_blitters[i].handler_nbo
							: Screen_blitters[i].handler_obo
							;
			}
		}
	
		// No appropriate blitter found, dump RGB mask values and abort()
		if (!blitter_found) {
			fprintf(stderr, "### No appropriate blitter found\n");
			fprintf(stderr, "\tR/G/B mask values  : 0x%06x, 0x%06x, 0x%06x (depth = %d)\n",
				visualFormat.Rmask, visualFormat.Gmask, visualFormat.Bmask, visualFormat.depth);
			fprintf(stderr, "\tR/G/B shift values : %d/%d/%d\n",
				visualFormat.Rshift, visualFormat.Gshift, visualFormat.Bshift);
			abort();
		}
	}
#else
	// The UAE memory handlers will blit correctly
	// --> no need for specialised blitters here
	Screen_blit = Blit_Copy_Raw;
#endif
	
	// If the blitter simply reduces to a copy, we don't need VOSF in DGA mode
	// --> In that case, we return FALSE
	return (Screen_blit != Blit_Copy_Raw);
}
