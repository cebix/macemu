/*
 *  video_blit.h - Video/graphics emulation, blitters
 *
 *  Basilisk II (C) 1997-2004 Christian Bauer
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
