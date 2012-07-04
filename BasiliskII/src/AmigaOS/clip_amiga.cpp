/*
 *  clip_amiga.cpp - Clipboard handling, AmigaOS implementation
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
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

#include <exec/types.h>
#include <libraries/iffparse.h>
#include <devices/clipboard.h>
#define __USE_SYSBASE
#include <proto/exec.h>
#include <proto/iffparse.h>
#include <inline/exec.h>
#include <inline/iffparse.h>

#include "clip.h"
#include "prefs.h"

#define DEBUG 0
#include "debug.h"


// Global variables
static struct IFFHandle *iffw = NULL;
static struct ClipboardHandle *ch = NULL;
static bool clipboard_open = false;
static bool no_clip_conversion;


// Conversion tables
static const uint8 mac2iso[0x80] = {
	0xc4, 0xc5, 0xc7, 0xc9, 0xd1, 0xd6, 0xdc, 0xe1,
	0xe0, 0xe2, 0xe4, 0xe3, 0xe5, 0xe7, 0xe9, 0xe8,
	0xea, 0xeb, 0xed, 0xec, 0xee, 0xef, 0xf1, 0xf3,
	0xf2, 0xf4, 0xf6, 0xf5, 0xfa, 0xf9, 0xfb, 0xfc,
	0x2b, 0xb0, 0xa2, 0xa3, 0xa7, 0xb7, 0xb6, 0xdf,
	0xae, 0xa9, 0x20, 0xb4, 0xa8, 0x23, 0xc6, 0xd8,
	0x20, 0xb1, 0x3c, 0x3e, 0xa5, 0xb5, 0xf0, 0x53,
	0x50, 0x70, 0x2f, 0xaa, 0xba, 0x4f, 0xe6, 0xf8,
	0xbf, 0xa1, 0xac, 0x2f, 0x66, 0x7e, 0x44, 0xab,
	0xbb, 0x2e, 0x20, 0xc0, 0xc3, 0xd5, 0x4f, 0x6f,
	0x2d, 0x2d, 0x22, 0x22, 0x60, 0x27, 0xf7, 0x20,
	0xff, 0x59, 0x2f, 0xa4, 0x3c, 0x3e, 0x66, 0x66,
	0x23, 0xb7, 0x2c, 0x22, 0x25, 0xc2, 0xca, 0xc1,
	0xcb, 0xc8, 0xcd, 0xce, 0xcf, 0xcc, 0xd3, 0xd4,
	0x20, 0xd2, 0xda, 0xdb, 0xd9, 0x69, 0x5e, 0x7e,
	0xaf, 0x20, 0xb7, 0xb0, 0xb8, 0x22, 0xb8, 0x20
};


/*
 *  Initialization
 */

void ClipInit(void)
{
	no_clip_conversion = PrefsFindBool("noclipconversion");

	// Create clipboard IFF handle
	iffw = AllocIFF();
	if (iffw) {
		ch = OpenClipboard(PRIMARY_CLIP);
		if (ch) {
			iffw->iff_Stream = (ULONG)ch;
			InitIFFasClip(iffw);
			clipboard_open = true;
		}
	}
}


/*
 *  Deinitialization
 */

void ClipExit(void)
{
	if (ch)
		CloseClipboard(ch);
	if (iffw)
		FreeIFF(iffw);
}

/*
 * Mac application zeroes clipboard
 */

void ZeroScrap()
{

}

/*
 *  Mac application reads clipboard
 */

void GetScrap(void **handle, uint32 type, int32 offset)
{
	D(bug("GetScrap handle %p, type %08x, offset %d\n", handle, type, offset));
}


/*
 *  Mac application wrote to clipboard
 */

void PutScrap(uint32 type, void *scrap, int32 length)
{
	D(bug("PutScrap type %08lx, data %08lx, length %ld\n", type, scrap, length));
	if (length <= 0 || !clipboard_open)
		return;

	switch (type) {
		case 'TEXT': {
			D(bug(" clipping TEXT\n"));

			// Open IFF stream
			if (OpenIFF(iffw, IFFF_WRITE))
				break;

			// Convert text from Mac charset to ISO-Latin1
			uint8 *buf = new uint8[length];
			uint8 *p = (uint8 *)scrap;
			uint8 *q = buf;
			for (int i=0; i<length; i++) {
				uint8 c = *p++;
				if (c < 0x80) {
					if (c == 13)	// CR -> LF
						c = 10;
				} else if (!no_clip_conversion)
					c = mac2iso[c & 0x7f];
				*q++ = c;
			}

			// Write text
			if (!PushChunk(iffw, 'FTXT', 'FORM', IFFSIZE_UNKNOWN)) {
				if (!PushChunk(iffw, 0, 'CHRS', IFFSIZE_UNKNOWN)) {
					WriteChunkBytes(iffw, scrap, length);
					PopChunk(iffw);
				}
				PopChunk(iffw);
			}

			// Close IFF stream
			CloseIFF(iffw);
			delete[] buf;
			break;
		}
	}
}
