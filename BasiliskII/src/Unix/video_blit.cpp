/*
 *  video_blit.cpp - Video/graphics emulation, blitters
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

#include "sysdeps.h"
#include "video.h"

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef ENABLE_VOSF
// Format of the target visual
struct VisualFormat {
	int		depth;					// Screen depth
	uint32	Rmask, Gmask, Bmask;	// RGB mask values
	uint32	Rshift, Gshift, Bshift;	// RGB shift values
};
static VisualFormat visualFormat;

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

#define FB_DEPTH 15
#define FB_FUNC_NAME Blit_BGR555_NBO
#include "video_blit.h"

// Opposite byte order (untested)

#define FB_BLIT_1(dst, src) \
	(dst = (((src) >> 2) & 0x1f00) | (((src) >> 8) & 3) | (((src) << 8) & 0xe000) | (((src) << 2) & 0x7c))

#define FB_BLIT_2(dst, src) \
	(dst = (((src) >> 2) & 0x1f001f00) | (((src) >> 8) & 0x30003) | (((src) << 8) & 0xe000e000) | (((src) << 2) & 0x7c007c))

#define FB_DEPTH 15
#define FB_FUNC_NAME Blit_BGR555_OBO
#include "video_blit.h"

#else

// Native byte order (untested)

#define FB_BLIT_1(dst, src) \
	(dst = (((src) >> 2) & 0x1f) | (((src) >> 8) & 0xe0) | (((src) << 8) & 0x0300) | (((src) << 2) & 0x7c00))

#define FB_BLIT_2(dst, src) \
	(dst = (((src) >> 2) & 0x1f001f) | (((src) >> 8) & 0xe000e0) | (((src) << 8) & 0x03000300) | (((src) << 2) & 0x7c007c00))

#define FB_DEPTH 15
#define FB_FUNC_NAME Blit_BGR555_NBO
#include "video_blit.h"

// Opposite byte order (untested)

#define FB_BLIT_1(dst, src) \
	(dst = (((src) << 6) & 0x1f00) | ((src) & 0xe003) | (((src) >> 6) & 0x7c))

#define FB_BLIT_2(dst, src) \
	(dst = (((src) << 6) & 0x1f001f00) | ((src) & 0xe003e003) | (((src) >> 6) & 0x7c007c))

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

#define FB_DEPTH 16
#define FB_FUNC_NAME Blit_RGB565_NBO
#include "video_blit.h"

// Opposite byte order

#define FB_BLIT_1(dst, src) \
	(dst = ((((src) >> 7) & 0xff) | (((src) << 9) & 0xc000) | (((src) << 8) & 0x1f00)))

#define FB_BLIT_2(dst, src) \
	(dst = ((((src) >> 7) & 0x00ff00ff) | (((src) << 9) & 0xc000c000) | (((src) << 8) & 0x1f001f00)))

#define FB_DEPTH 16
#define FB_FUNC_NAME Blit_RGB565_OBO
#include "video_blit.h"

#else

// Native byte order

#define FB_BLIT_1(dst, src) \
	(dst = (((src) >> 8) & 0x001f) | (((src) << 9) & 0xfe00) | (((src) >> 7) & 0x01c0))
	
// gb-- Disabled because I don't see any improvement
#if 0 && defined(__i386__) && defined(X86_ASSEMBLY)

#define FB_BLIT_2(dst, src) \
	__asm__ (	"movl %0,%%ebx\n\t" \
				"movl %0,%%ebp\n\t" \
				"andl $0x1f001f00,%%ebx\n\t" \
				"andl $0x007f007f,%0\n\t" \
				"andl $0xe000e000,%%ebp\n\t" \
				"shrl $8,%%ebx\n\t" \
				"shrl $7,%%ebp\n\t" \
				"shll $9,%0\n\t" \
				"orl %%ebx,%%ebp\n\t" \
				"orl %%ebp,%0\n\t" \
			: "=r" (dst) : "0" (src) : "ebx", "ebp", "cc" )

#else

#define FB_BLIT_2(dst, src) \
	(dst = (((src) >> 8) & 0x001f001f) | (((src) << 9) & 0xfe00fe00) | (((src) >> 7) & 0x01c001c0))

#endif

#define FB_DEPTH 16
#define FB_FUNC_NAME Blit_RGB565_NBO
#include "video_blit.h"

// Opposite byte order (untested)

#define FB_BLIT_1(dst, src) \
	(dst = (((src) & 0x1f00) | (((src) << 1) & 0xe0fe) | (((src) >> 15) & 1)))

#define FB_BLIT_2(dst, src) \
	(dst = (((src) & 0x1f001f00) | (((src) << 1) & 0xe0fee0fe) | (((src) >> 15) & 0x10001)))

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

#define FB_DEPTH 24
#include "video_blit.h"

/* -------------------------------------------------------------------------- */
/* --- BGR 888                                                            --- */
/* -------------------------------------------------------------------------- */

// Native byte order [BE] (untested)

#ifdef WORDS_BIGENDIAN

#define FB_BLIT_2(dst, src) \
	(dst = (((src) >> 16) & 0xff) | ((src) & 0xff00) | (((src) & 0xff) << 16))

#define FB_FUNC_NAME Blit_BGR888_NBO
#define FB_DEPTH 24
#include "video_blit.h"

#else

// Opposite byte order [LE] (untested)

#define FB_BLIT_2(dst, src) \
	(dst = (((src) >> 16) & 0xff) | ((src) & 0xff0000) | (((src) & 0xff) << 16))

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

#define FB_DEPTH 24
#include "video_blit.h"

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
bool Screen_blitter_init(XVisualInfo * visual_info, bool native_byte_order, video_depth mac_depth)
{
#if REAL_ADDRESSING || DIRECT_ADDRESSING
	if (mac_depth == VDEPTH_1BIT) {

		// 1-bit mode uses a 1-bit X image, so there's no need for special blitting routines
		Screen_blit = Blit_Copy_Raw;

	} else {

		visualFormat.depth = visual_info->depth;
		visualFormat.Rmask = visual_info->red_mask;
		visualFormat.Gmask = visual_info->green_mask;
		visualFormat.Bmask = visual_info->blue_mask;
	
		// Compute RGB shift values
		visualFormat.Rshift = 0;
		for (uint32 Rmask = visualFormat.Rmask; Rmask && ((Rmask & 1) != 1); Rmask >>= 1)
			++visualFormat.Rshift;
		visualFormat.Gshift = 0;
		for (uint32 Gmask = visualFormat.Gmask; Gmask && ((Gmask & 1) != 1); Gmask >>= 1)
			++visualFormat.Gshift;
		visualFormat.Bshift = 0;
		for (uint32 Bmask = visualFormat.Bmask; Bmask && ((Bmask & 1) != 1); Bmask >>= 1)
			++visualFormat.Bshift;
	
		// Search for an adequate blit function
		bool blitter_found = false;
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
#endif /* ENABLE_VOSF */
