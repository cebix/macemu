/*
 *  gfxaccel.cpp - Generic Native QuickDraw acceleration
 *
 *  SheepShaver (C) 1997-2005 Marc Hellwig and Christian Bauer
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

#include "prefs.h"
#include "video.h"
#include "video_defs.h"

#define DEBUG 0
#include "debug.h"


/*
 *	Utility functions
 */

// Return bytes per pixel for requested depth
static inline int bytes_per_pixel(int depth)
{
	int bpp;
	switch (depth) {
	case 8:
		bpp = 1;
		break;
	case 15: case 16:
		bpp = 2;
		break;
	case 24: case 32:
		bpp = 4;
		break;
	default:
		abort();
	}
	return bpp;
}

// Pass-through dirty areas to redraw functions
static inline void NQD_set_dirty_area(uint32 p)
{
	if (ReadMacInt32(p + acclDestBaseAddr) == screen_base) {
		int16 x = (int16)ReadMacInt16(p + acclDestRect + 2) - (int16)ReadMacInt16(p + acclDestBoundsRect + 2);
		int16 y = (int16)ReadMacInt16(p + acclDestRect + 0) - (int16)ReadMacInt16(p + acclDestBoundsRect + 0);
		int16 w  = (int16)ReadMacInt16(p + acclDestRect + 6) - (int16)ReadMacInt16(p + acclDestRect + 2);
		int16 h = (int16)ReadMacInt16(p + acclDestRect + 4) - (int16)ReadMacInt16(p + acclDestRect + 0);
		video_set_dirty_area(x, y, w, h);
	}
}


/*
 *	Rectangle inversion
 */

template< int bpp >
static inline void do_invrect(uint8 *dest, uint32 length)
{
#define INVERT_1(PTR, OFS) ((uint8  *)(PTR))[OFS] = ~((uint8  *)(PTR))[OFS]
#define INVERT_2(PTR, OFS) ((uint16 *)(PTR))[OFS] = ~((uint16 *)(PTR))[OFS]
#define INVERT_4(PTR, OFS) ((uint32 *)(PTR))[OFS] = ~((uint32 *)(PTR))[OFS]
#define INVERT_8(PTR, OFS) ((uint64 *)(PTR))[OFS] = ~((uint64 *)(PTR))[OFS]

#ifndef UNALIGNED_PROFITABLE
	// Align on 16-bit boundaries
	if (bpp < 16 && (((uintptr)dest) & 1)) {
		INVERT_1(dest, 0);
		dest += 1; length -= 1;
	}

	// Align on 32-bit boundaries
	if (bpp < 32 && (((uintptr)dest) & 2)) {
		INVERT_2(dest, 0);
		dest += 2; length -= 2;
	}
#endif

	// Invert 8-byte words
	if (length >= 8) {
		const int r = (length / 8) % 8;
		dest += r * 8;

		int n = ((length / 8) + 7) / 8;
		switch (r) {
		case 0: do {
				dest += 64;
				INVERT_8(dest, -8);
		case 7: INVERT_8(dest, -7);
		case 6: INVERT_8(dest, -6);
		case 5: INVERT_8(dest, -5);
		case 4: INVERT_8(dest, -4);
		case 3: INVERT_8(dest, -3);
		case 2: INVERT_8(dest, -2);
		case 1: INVERT_8(dest, -1);
				} while (--n > 0);
		}
	}

	// 32-bit cell to invert?
	if (length & 4) {
		INVERT_4(dest, 0);
		if (bpp <= 16)
			dest += 4;
	}

	// 16-bit cell to invert?
	if (bpp <= 16 && (length & 2)) {
		INVERT_2(dest, 0);
		if (bpp <= 8)
			dest += 2;
	}

	// 8-bit cell to invert?
	if (bpp <= 8 && (length & 1))
		INVERT_1(dest, 0);

#undef INVERT_1
#undef INVERT_2
#undef INVERT_4
#undef INVERT_8
}

void NQD_invrect(uint32 p)
{
	D(bug("accl_invrect %08x\n", p));

	// Get inversion parameters
	int16 dest_X = (int16)ReadMacInt16(p + acclDestRect + 2) - (int16)ReadMacInt16(p + acclDestBoundsRect + 2);
	int16 dest_Y = (int16)ReadMacInt16(p + acclDestRect + 0) - (int16)ReadMacInt16(p + acclDestBoundsRect + 0);
	int16 width  = (int16)ReadMacInt16(p + acclDestRect + 6) - (int16)ReadMacInt16(p + acclDestRect + 2);
	int16 height = (int16)ReadMacInt16(p + acclDestRect + 4) - (int16)ReadMacInt16(p + acclDestRect + 0);
	D(bug(" dest X %d, dest Y %d\n", dest_X, dest_Y));
	D(bug(" width %d, height %d, bytes_per_row %d\n", width, height, (int32)ReadMacInt32(p + acclDestRowBytes)));

	//!!?? pen_mode == 14

	// And perform the inversion
	const int bpp = bytes_per_pixel(ReadMacInt32(p + acclDestPixelSize));
	const int dest_row_bytes = (int32)ReadMacInt32(p + acclDestRowBytes);
	uint8 *dest = Mac2HostAddr(ReadMacInt32(p + acclDestBaseAddr) + (dest_Y * dest_row_bytes) + (dest_X * bpp));
	width *= bpp;
	switch (bpp) {
	case 1:
		for (int i = 0; i < height; i++) {
			do_invrect<8>(dest, width);
			dest += dest_row_bytes;
		}
		break;
	case 2:
		for (int i = 0; i < height; i++) {
			do_invrect<16>(dest, width);
			dest += dest_row_bytes;
		}
		break;
	case 4:
		for (int i = 0; i < height; i++) {
			do_invrect<32>(dest, width);
			dest += dest_row_bytes;
		}
		break;
	}
}


/*
 *	Rectangle filling
 */

template< int bpp >
static inline void do_fillrect(uint8 *dest, uint32 color, uint32 length)
{
#define FILL_1(PTR, OFS, VAL) ((uint8  *)(PTR))[OFS] = (VAL)
#define FILL_2(PTR, OFS, VAL) ((uint16 *)(PTR))[OFS] = (VAL)
#define FILL_4(PTR, OFS, VAL) ((uint32 *)(PTR))[OFS] = (VAL)
#define FILL_8(PTR, OFS, VAL) ((uint64 *)(PTR))[OFS] = (VAL)

#ifndef UNALIGNED_PROFITABLE
	// Align on 16-bit boundaries
	if (bpp < 16 && (((uintptr)dest) & 1)) {
		FILL_1(dest, 0, color);
		dest += 1; length -= 1;
	}

	// Align on 32-bit boundaries
	if (bpp < 32 && (((uintptr)dest) & 2)) {
		FILL_2(dest, 0, color);
		dest += 2; length -= 2;
	}
#endif

	// Fill 8-byte words
	if (length >= 8) {
		const uint64 c = (((uint64)color) << 32) | color;
		const int r = (length / 8) % 8;
		dest += r * 8;

		int n = ((length / 8) + 7) / 8;
		switch (r) {
		case 0: do {
				dest += 64;
				FILL_8(dest, -8, c);
		case 7: FILL_8(dest, -7, c);
		case 6: FILL_8(dest, -6, c);
		case 5: FILL_8(dest, -5, c);
		case 4: FILL_8(dest, -4, c);
		case 3: FILL_8(dest, -3, c);
		case 2: FILL_8(dest, -2, c);
		case 1: FILL_8(dest, -1, c);
				} while (--n > 0);
		}
	}

	// 32-bit cell to fill?
	if (length & 4) {
		FILL_4(dest, 0, color);
		if (bpp <= 16)
			dest += 4;
	}

	// 16-bit cell to fill?
	if (bpp <= 16 && (length & 2)) {
		FILL_2(dest, 0, color);
		if (bpp <= 8)
			dest += 2;
	}

	// 8-bit cell to fill?
	if (bpp <= 8 && (length & 1))
		FILL_1(dest, 0, color);

#undef FILL_1
#undef FILL_2
#undef FILL_4
#undef FILL_8
}

void NQD_fillrect(uint32 p)
{
	D(bug("accl_fillrect %08x\n", p));

	// Get filling parameters
	int16 dest_X = (int16)ReadMacInt16(p + acclDestRect + 2) - (int16)ReadMacInt16(p + acclDestBoundsRect + 2);
	int16 dest_Y = (int16)ReadMacInt16(p + acclDestRect + 0) - (int16)ReadMacInt16(p + acclDestBoundsRect + 0);
	int16 width  = (int16)ReadMacInt16(p + acclDestRect + 6) - (int16)ReadMacInt16(p + acclDestRect + 2);
	int16 height = (int16)ReadMacInt16(p + acclDestRect + 4) - (int16)ReadMacInt16(p + acclDestRect + 0);
	uint32 color = htonl(ReadMacInt32(p + acclPenMode) == 8 ? ReadMacInt32(p + acclForePen) : ReadMacInt32(p + acclBackPen));
	D(bug(" dest X %d, dest Y %d\n", dest_X, dest_Y));
	D(bug(" width %d, height %d\n", width, height));
	D(bug(" bytes_per_row %d color %08x\n", (int32)ReadMacInt32(p + acclDestRowBytes), color));

	// And perform the fill
	const int bpp = bytes_per_pixel(ReadMacInt32(p + acclDestPixelSize));
	const int dest_row_bytes = (int32)ReadMacInt32(p + acclDestRowBytes);
	uint8 *dest = Mac2HostAddr(ReadMacInt32(p + acclDestBaseAddr) + (dest_Y * dest_row_bytes) + (dest_X * bpp));
	width *= bpp;
	switch (bpp) {
	case 1:
		for (int i = 0; i < height; i++) {
			memset(dest, color, width);
			dest += dest_row_bytes;
		}
		break;
	case 2:
		for (int i = 0; i < height; i++) {
			do_fillrect<16>(dest, color, width);
			dest += dest_row_bytes;
		}
		break;
	case 4:
		for (int i = 0; i < height; i++) {
			do_fillrect<32>(dest, color, width);
			dest += dest_row_bytes;
		}
		break;
	}
}

bool NQD_fillrect_hook(uint32 p)
{
	D(bug("accl_fillrect_hook %08x\n", p));
	NQD_set_dirty_area(p);

	// Check if we can accelerate this fillrect
	if (ReadMacInt32(p + 0x284) != 0 && ReadMacInt32(p + acclDestPixelSize) >= 8) {
		const int transfer_mode = ReadMacInt32(p + acclTransferMode);
		if (transfer_mode == 8) {
			// Fill
			WriteMacInt32(p + acclDrawProc, NativeTVECT(NATIVE_NQD_FILLRECT));
			return true;
		}
		else if (transfer_mode == 10) {
			// Invert
			WriteMacInt32(p + acclDrawProc, NativeTVECT(NATIVE_NQD_INVRECT));
			return true;
		}
	}
	return false;
}


/*
 *	Isomorphic rectangle blitting
 */

void NQD_bitblt(uint32 p)
{
	D(bug("accl_bitblt %08x\n", p));

	// Get blitting parameters
	int16 src_X  = (int16)ReadMacInt16(p + acclSrcRect + 2) - (int16)ReadMacInt16(p + acclSrcBoundsRect + 2);
	int16 src_Y  = (int16)ReadMacInt16(p + acclSrcRect + 0) - (int16)ReadMacInt16(p + acclSrcBoundsRect + 0);
	int16 dest_X = (int16)ReadMacInt16(p + acclDestRect + 2) - (int16)ReadMacInt16(p + acclDestBoundsRect + 2);
	int16 dest_Y = (int16)ReadMacInt16(p + acclDestRect + 0) - (int16)ReadMacInt16(p + acclDestBoundsRect + 0);
	int16 width  = (int16)ReadMacInt16(p + acclDestRect + 6) - (int16)ReadMacInt16(p + acclDestRect + 2);
	int16 height = (int16)ReadMacInt16(p + acclDestRect + 4) - (int16)ReadMacInt16(p + acclDestRect + 0);
	D(bug(" src addr %08x, dest addr %08x\n", ReadMacInt32(p + acclSrcBaseAddr), ReadMacInt32(p + acclDestBaseAddr)));
	D(bug(" src X %d, src Y %d, dest X %d, dest Y %d\n", src_X, src_Y, dest_X, dest_Y));
	D(bug(" width %d, height %d\n", width, height));

	// And perform the blit
	const int bpp = bytes_per_pixel(ReadMacInt32(p + acclSrcPixelSize));
	width *= bpp;
	if ((int32)ReadMacInt32(p + acclSrcRowBytes) > 0) {
		const int src_row_bytes = (int32)ReadMacInt32(p + acclSrcRowBytes);
		const int dst_row_bytes = (int32)ReadMacInt32(p + acclDestRowBytes);
		uint8 *src = Mac2HostAddr(ReadMacInt32(p + acclSrcBaseAddr) + (src_Y * src_row_bytes) + (src_X * bpp));
		uint8 *dst = Mac2HostAddr(ReadMacInt32(p + acclDestBaseAddr) + (dest_Y * dst_row_bytes) + (dest_X * bpp));
		for (int i = 0; i < height; i++) {
			memmove(dst, src, width);
			src += src_row_bytes;
			dst += dst_row_bytes;
		}
	}
	else {
		const int src_row_bytes = -(int32)ReadMacInt32(p + acclSrcRowBytes);
		const int dst_row_bytes = -(int32)ReadMacInt32(p + acclDestRowBytes);
		uint8 *src = Mac2HostAddr(ReadMacInt32(p + acclSrcBaseAddr) + ((src_Y + height - 1) * src_row_bytes) + (src_X * bpp));
		uint8 *dst = Mac2HostAddr(ReadMacInt32(p + acclDestBaseAddr) + ((dest_Y + height - 1) * dst_row_bytes) + (dest_X * bpp));
		for (int i = height - 1; i >= 0; i--) {
			memmove(dst, src, width);
			src -= src_row_bytes;
			dst -= dst_row_bytes;
		}
	}
}

/*
  BitBlt transfer modes:
  0 : srcCopy
  1 : srcOr
  2 : srcXor
  3 : srcBic
  4 : notSrcCopy
  5 : notSrcOr
  6 : notSrcXor
  7 : notSrcBic
  32 : blend
  33 : addPin
  34 : addOver
  35 : subPin
  36 : transparent
  37 : adMax
  38 : subOver
  39 : adMin
  50 : hilite
*/

bool NQD_bitblt_hook(uint32 p)
{
	D(bug("accl_draw_hook %08x\n", p));
	NQD_set_dirty_area(p);

	// Check if we can accelerate this bitblt
	if (ReadMacInt32(p + 0x018) + ReadMacInt32(p + 0x128) == 0 &&
		ReadMacInt32(p + 0x130) == 0 &&
		ReadMacInt32(p + acclSrcPixelSize) >= 8 &&
		ReadMacInt32(p + acclSrcPixelSize) == ReadMacInt32(p + acclDestPixelSize) &&
		(int32)(ReadMacInt32(p + acclSrcRowBytes) ^ ReadMacInt32(p + acclDestRowBytes)) >= 0 &&	// same sign?
		ReadMacInt32(p + acclTransferMode) == 0 &&												// srcCopy?
		(int32)ReadMacInt32(p + 0x15c) > 0) {

		// Yes, set function pointer
		WriteMacInt32(p + acclDrawProc, NativeTVECT(NATIVE_NQD_BITBLT));
		return true;
	}
	return false;
}

// Unknown hook
bool NQD_unknown_hook(uint32 arg)
{
	D(bug("accl_unknown_hook %08x\n", arg));
	NQD_set_dirty_area(arg);

	return false;
}

// Wait for graphics operation to finish
bool NQD_sync_hook(uint32 arg)
{
	D(bug("accl_sync_hook %08x\n", arg));
	return true;
}


/*
 *	Install Native QuickDraw acceleration hooks
 */

void VideoInstallAccel(void)
{
	// Install acceleration hooks
	if (PrefsFindBool("gfxaccel")) {
		D(bug("Video: Installing acceleration hooks\n"));
		uint32 base;

		SheepVar bitblt_hook_info(sizeof(accl_hook_info));
		base = bitblt_hook_info.addr();
		WriteMacInt32(base + 0, NativeTVECT(NATIVE_NQD_BITBLT_HOOK));
		WriteMacInt32(base + 4, NativeTVECT(NATIVE_NQD_SYNC_HOOK));
		WriteMacInt32(base + 8, ACCL_BITBLT);
		NQDMisc(6, bitblt_hook_info.addr());

		SheepVar fillrect_hook_info(sizeof(accl_hook_info));
		base = fillrect_hook_info.addr();
		WriteMacInt32(base + 0, NativeTVECT(NATIVE_NQD_FILLRECT_HOOK));
		WriteMacInt32(base + 4, NativeTVECT(NATIVE_NQD_SYNC_HOOK));
		WriteMacInt32(base + 8, ACCL_FILLRECT);
		NQDMisc(6, fillrect_hook_info.addr());

		for (int op = 0; op < 8; op++) {
			switch (op) {
			case ACCL_BITBLT:
			case ACCL_FILLRECT:
				continue;
			}
			SheepVar unknown_hook_info(sizeof(accl_hook_info));
			base = unknown_hook_info.addr();
			WriteMacInt32(base + 0, NativeTVECT(NATIVE_NQD_UNKNOWN_HOOK));
			WriteMacInt32(base + 4, NativeTVECT(NATIVE_NQD_SYNC_HOOK));
			WriteMacInt32(base + 8, op);
			NQDMisc(6, unknown_hook_info.addr());
		}
	}
}
