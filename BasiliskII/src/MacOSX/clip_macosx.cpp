/*
 *  clip_macosx.cpp - Clipboard handling, MacOS X (Carbon) implementation
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

	  // Get the native clipboard data
	  uint8 *data = new uint8[byteCount];
	  if (GetScrapFlavorData(theScrap, type, &byteCount, data) == noErr) {
		  M68kRegisters r;

		  // Add new data to clipboard
		  static uint16 proc[] = {
			  0x598f,				// subq.l		#4,sp
			  0xa9fc,				// ZeroScrap()
			  0x2f3c, 0, 0,			// move.l		#length,-(sp)
			  0x2f3c, 0, 0,			// move.l		#type,-(sp)
			  0x2f3c, 0, 0,			// move.l		#outbuf,-(sp)
			  0xa9fe,				// PutScrap()
			  0x588f,				// addq.l		#4,sp
			  M68K_RTS
		  };
		  uint32 proc_area = (uint32)proc;
		  WriteMacInt32(proc_area +  6, byteCount);
		  WriteMacInt32(proc_area + 12, type);
		  WriteMacInt32(proc_area + 18, (uint32)data);
		  we_put_this_data = true;
		  Execute68k(proc_area, &r);
	  }

	  delete[] data;
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

	if (PutScrapFlavor(theScrap, type, kScrapFlavorMaskNone, length, scrap) != noErr) {
		D(bug(" could not put to scrap\n"));
		return;
	}
}
