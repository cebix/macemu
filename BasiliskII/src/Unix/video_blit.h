/*
 *  video_blit.h - Video/graphics emulation, blitters
 *
 *  Basilisk II (C) 1997-2000 Christian Bauer
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

#ifndef FB_BLIT_1
# error "Undefined 16-bit word blit function"
#endif

#ifndef FB_BLIT_2
# error "Undefined 32-bit word blit function"
#endif

#ifndef FB_DEPTH
# error "Undefined screen depth"
#endif

static void FB_FUNC_NAME(uint8 * dest, const uint8 * source, uint32 length)
{
	union {
	uint8 *  bp;
	uint16 * wp;
	uint32 * lp;
	} s, d;
	
	s.bp = (uint8 *)source;
	d.bp = dest;
	
#if FB_DEPTH <= 8
	// Align source and dest to 16-bit word boundaries
	if (FB_DEPTH <= 8 && ((unsigned long) source) & 1) {
		*d.bp++ = *s.bp++;
		length -= 1;
	}
#endif
	
#if FB_DEPTH <= 8
	if (length >= 2) {
#endif
	
#if FB_DEPTH <= 16
	// Align source and dest to 32-bit word boundaries
	if (((unsigned long) source) & 2) {
		const uint16 val = *s.wp++;
		FB_BLIT_1(*d.wp++, val);
		length -= 2;
	}
#endif
	
	// Blit 4-byte words
	if (length >= 4) {
		const int remainder = (length / 4) % 8;
		s.lp += remainder;
		d.lp += remainder;
		
		int n = ((length / 4) + 7) / 8;
		switch (remainder) {
		case 0:	do {
				s.lp += 8; d.lp += 8;
				FB_BLIT_2(d.lp[-8], s.lp[-8]);
		case 7: FB_BLIT_2(d.lp[-7], s.lp[-7]);
		case 6: FB_BLIT_2(d.lp[-6], s.lp[-6]);
		case 5: FB_BLIT_2(d.lp[-5], s.lp[-5]);
		case 4: FB_BLIT_2(d.lp[-4], s.lp[-4]);
		case 3: FB_BLIT_2(d.lp[-3], s.lp[-3]);
		case 2: FB_BLIT_2(d.lp[-2], s.lp[-2]);
		case 1: FB_BLIT_2(d.lp[-1], s.lp[-1]);
				} while (--n > 0);
		}
	}
	
#if FB_DEPTH <= 16
	// There might remain at least one word to blit
	if (length & 2) {
		FB_BLIT_1(*d.wp, *s.wp);
#if FB_DEPTH <= 8
		d.wp++;
		s.wp++;
#endif
	}
#endif
	
#if FB_DEPTH <= 8
	}
	
	// There might remain one byte to blit
	if (length & 1)
		*d.bp = *s.bp;
#endif
}

#undef FB_FUNC_NAME

#ifdef FB_BLIT_1
#undef FB_BLIT_1
#endif

#ifdef FB_BLIT_2
#undef FB_BLIT_2
#endif

#ifdef FB_DEPTH
#undef FB_DEPTH
#endif
