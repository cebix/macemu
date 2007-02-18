/*
 *  clip_macosx.cpp - Clipboard handling, MacOS X (Carbon) implementation
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
#include <Carbon/Carbon.h>

#include "clip.h"
#include "main.h"
#include "cpu_emulation.h"
#include "emul_op.h"

#define DEBUG 0
#include "debug.h"


// Flag for PutScrap(): the data was put by GetScrap(), don't bounce it back to the MacOS X side
static bool we_put_this_data = false;


static void SwapScrapData(uint32 type, void *data, int32 length, int from_host) {
#if BYTE_ORDER != BIG_ENDIAN
	if (type == kScrapFlavorTypeTextStyle) {
		uint16 *sdata = (uint16 *) data;
		// the first short stores the number of runs
		uint16 runs = sdata[0];
		sdata[0] = htons(sdata[0]);
		if (from_host)
			runs = sdata[0];
		sdata++;
		// loop through each run
		for (int i = 0; i < runs; i++) {
			struct style_data {
				uint32 offset;
				uint16 line_height;
				uint16 line_ascent;
				uint16 font_family;
				uint16 character_style; // not swapped
				uint16 point_size;
				uint16 red;
				uint16 green;
				uint16 blue;
			} *style = (struct style_data *) (sdata + i*10);
			style->offset = htonl(style->offset);
			style->line_height = htons(style->line_height);
			style->line_ascent = htons(style->line_ascent);
			style->font_family = htons(style->font_family);
			style->point_size = htons(style->point_size);
			style->red = htons(style->red);
			style->green = htons(style->green);
			style->blue = htons(style->blue);
		}
	} else {
		// add byteswapping code for other flavor types here ...
	}
#endif
}


/*
 *  Initialization
 */

void ClipInit(void)
{
}


/*
 *  Deinitialization
 */

void ClipExit(void)
{
}


/*
 *  Mac application reads clipboard
 */

void GetScrap(void **handle, uint32 type, int32 offset)
{
	D(bug("GetScrap handle %p, type %08x, offset %d\n", handle, type, offset));
	ScrapRef theScrap;

	if (GetCurrentScrap(&theScrap) != noErr) {
		D(bug(" could not open scrap\n"));
		return;
	}

	Size byteCount;
	if (GetScrapFlavorSize(theScrap, type, &byteCount) == noErr) {

		// Allocate space for new scrap in MacOS side
		M68kRegisters r;
		r.d[0] = byteCount;
		Execute68kTrap(0xa71e, &r);				// NewPtrSysClear()
		uint32 scrap_area = r.a[0];

		// Get the native clipboard data
		if (scrap_area) {
			uint8 * const data = Mac2HostAddr(scrap_area);
			if (GetScrapFlavorData(theScrap, type, &byteCount, data) == noErr) {
				SwapScrapData(type, data, byteCount, FALSE);
				// Add new data to clipboard
				static uint8 proc[] = {
					0x59, 0x8f,					// subq.l	#4,sp
					0xa9, 0xfc,					// ZeroScrap()
					0x2f, 0x3c, 0, 0, 0, 0,		// move.l	#length,-(sp)
					0x2f, 0x3c, 0, 0, 0, 0,		// move.l	#type,-(sp)
					0x2f, 0x3c, 0, 0, 0, 0,		// move.l	#outbuf,-(sp)
					0xa9, 0xfe,					// PutScrap()
					0x58, 0x8f,					// addq.l	#4,sp
					M68K_RTS >> 8, M68K_RTS
				};
				r.d[0] = sizeof(proc);
				Execute68kTrap(0xa71e, &r);		// NewPtrSysClear()
				uint32 proc_area = r.a[0];

				if (proc_area) {
					Host2Mac_memcpy(proc_area, proc, sizeof(proc));
					WriteMacInt32(proc_area +  6, byteCount);
					WriteMacInt32(proc_area + 12, type);
					WriteMacInt32(proc_area + 18, scrap_area);
					we_put_this_data = true;
					Execute68k(proc_area, &r);

					r.a[0] = proc_area;
					Execute68kTrap(0xa01f, &r);	// DisposePtr
				}
			}

			r.a[0] = scrap_area;
			Execute68kTrap(0xa01f, &r);			// DisposePtr
		}
	}
}


/*
 *  Mac application wrote to clipboard
 */

void PutScrap(uint32 type, void *scrap, int32 length)
{
	D(bug("PutScrap type %08lx, data %08lx, length %ld\n", type, scrap, length));
	ScrapRef theScrap;

	if (we_put_this_data) {
		we_put_this_data = false;
		return;
	}
	if (length <= 0)
		return;

	ClearCurrentScrap();
	if (GetCurrentScrap(&theScrap) != noErr) {
		D(bug(" could not open scrap\n"));
		return;
	}

	SwapScrapData(type, scrap, length, TRUE);
	if (PutScrapFlavor(theScrap, type, kScrapFlavorMaskNone, length, scrap) != noErr) {
		D(bug(" could not put to scrap\n"));
		return;
	}
}
