/*
 *  clip_beos.cpp - Clipboard handling, BeOS implementation
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

#include <AppKit.h>
#include <support/UTF8.h>

#include "clip.h"
#include "prefs.h"

#define DEBUG 1
#include "debug.h"


// Flag: Don't convert clipboard text
static bool no_clip_conversion;


/*
 *  Initialization
 */

void ClipInit(void)
{
	no_clip_conversion = PrefsFindBool("noclipconversion");
}


/*
 *  Deinitialization
 */

void ClipExit(void)
{
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
	if (length <= 0)
		return;

	switch (type) {
		case 'TEXT':
			D(bug(" clipping TEXT\n"));
			if (be_clipboard->Lock()) {
				be_clipboard->Clear();
				BMessage *clipper = be_clipboard->Data(); 
	
				if (no_clip_conversion) {

					// Only convert CR->LF
					char *buf = new char[length];
					for (int i=0; i<length; i++) {
						if (i[(char *)scrap] == 13)
							buf[i] = 10;
						else
							buf[i] = i[(char *)scrap];
					}

					// Add text to Be clipboard
					clipper->AddData("text/plain", B_MIME_TYPE, buf, length); 
					be_clipboard->Commit();
					delete[] buf;

				} else {

					// Convert text from Mac charset to UTF-8
					int32 dest_length = length*3;
					int32 state = 0;
					char *buf = new char[dest_length];
					if (convert_to_utf8(B_MAC_ROMAN_CONVERSION, (char *)scrap, &length, buf, &dest_length, &state) == B_OK) {
						for (int i=0; i<dest_length; i++)
							if (buf[i] == 13)
								buf[i] = 10;
	
						// Add text to Be clipboard
						clipper->AddData("text/plain", B_MIME_TYPE, buf, dest_length); 
						be_clipboard->Commit();
					}
					delete[] buf;
				}
				be_clipboard->Unlock();
			}
			break;
	}
}
