/*
 *  clip_windows.cpp - Clipboard handling, Windows implementation
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

#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "macos_util.h"
#include "clip.h"
#include "prefs.h"
#include "cpu_emulation.h"
#include "main.h"
#include "emul_op.h"

#define DEBUG 0
#include "debug.h"

#ifndef NO_STD_NAMESPACE
using std::vector;
#endif


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

static const uint8 iso2mac[0x80] = {
	0xad, 0xb0, 0xe2, 0xc4, 0xe3, 0xc9, 0xa0, 0xe0,
	0xf6, 0xe4, 0xde, 0xdc, 0xce, 0xb2, 0xb3, 0xb6,
	0xb7, 0xd4, 0xd5, 0xd2, 0xd3, 0xa5, 0xd0, 0xd1,
	0xf7, 0xaa, 0xdf, 0xdd, 0xcf, 0xba, 0xfd, 0xd9,
	0xca, 0xc1, 0xa2, 0xa3, 0xdb, 0xb4, 0xbd, 0xa4,
	0xac, 0xa9, 0xbb, 0xc7, 0xc2, 0xf0, 0xa8, 0xf8,
	0xa1, 0xb1, 0xc3, 0xc5, 0xab, 0xb5, 0xa6, 0xe1,
	0xfc, 0xc6, 0xbc, 0xc8, 0xf9, 0xda, 0xd7, 0xc0,
	0xcb, 0xe7, 0xe5, 0xcc, 0x80, 0x81, 0xae, 0x82,
	0xe9, 0x83, 0xe6, 0xe8, 0xed, 0xea, 0xeb, 0xec,
	0xf5, 0x84, 0xf1, 0xee, 0xef, 0xcd, 0x85, 0xfb,
	0xaf, 0xf4, 0xf2, 0xf3, 0x86, 0xfa, 0xb8, 0xa7,
	0x88, 0x87, 0x89, 0x8b, 0x8a, 0x8c, 0xbe, 0x8d,
	0x8f, 0x8e, 0x90, 0x91, 0x93, 0x92, 0x94, 0x95,
	0xfe, 0x96, 0x98, 0x97, 0x99, 0x9b, 0x9a, 0xd6,
	0xbf, 0x9d, 0x9c, 0x9e, 0x9f, 0xff, 0xb9, 0xd8
};

// Flag: Don't convert clipboard text
static bool no_clip_conversion;

// Flag for PutScrap(): the data was put by GetScrap(), don't bounce it back to the Windows  side
static bool we_put_this_data = false;

// Define a byte array (rewrite if it's a bottleneck)
struct ByteArray : public vector<uint8> {
	uint8 *data() { return &(*this)[0]; }
};

// Prototypes
static void do_putscrap(uint32 type, void *scrap, int32 length);
static void do_getscrap(void **handle, uint32 type, int32 offset);

// From main_windows.cpp
extern HWND GetMainWindowHandle(void);


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
 *  Mac application wrote to clipboard
 */

void PutScrap(uint32 type, void *scrap, int32 length)
{
	D(bug("PutScrap type %08lx, data %p, length %ld\n", type, scrap, length));
	if (we_put_this_data) {
		we_put_this_data = false;
		return;
	}
	if (length <= 0)
		return;

	do_putscrap(type, scrap, length);
}

static void do_putscrap(uint32 type, void *scrap, int32 length)
{
	ByteArray clip_data;
	UINT uFormat = 0;

	switch (type) {
	case FOURCC('T','E','X','T'): {
		D(bug(" clipping TEXT\n"));

		// Convert text from Mac charset to ISO-Latin1
		uint8 *p = (uint8 *)scrap;
		for (int i=0; i<length; i++) {
			uint8 c = *p++;
			if (c < 0x80) {
				if (c == 13) {	// CR -> CR/LF
					clip_data.push_back(c);
					c = 10;
				}
			} else if (!no_clip_conversion)
				c = mac2iso[c & 0x7f];
			clip_data.push_back(c);
		}
		clip_data.push_back(0);
		uFormat = CF_TEXT;
		break;
	}
	}
	if (uFormat != CF_TEXT)				// 'TEXT' only
		return;

	// Transfer data to the native clipboard
	HWND hMainWindow = GetMainWindowHandle();
	if (!hMainWindow ||!OpenClipboard(hMainWindow))
		return;
	EmptyClipboard();
	HANDLE hData = GlobalAlloc(GMEM_DDESHARE, clip_data.size());
	if (hData) {
		uint8 *data = (uint8 *)GlobalLock(hData);
		memcpy(data, clip_data.data(), clip_data.size());
		GlobalUnlock(hData);
		if (!SetClipboardData(uFormat, hData))
			GlobalFree(hData);
	}
	CloseClipboard();
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

	do_getscrap(handle, type, offset);
}

static void do_getscrap(void **handle, uint32 type, int32 offset)
{
	// Get appropriate format for requested data
	UINT uFormat = 0;
	switch (type) {
	case FOURCC('T','E','X','T'):
		uFormat = CF_TEXT;
		break;
	}
	if (uFormat != CF_TEXT)				// 'TEXT' only
		return;

	// Get the native clipboard data
	HWND hMainWindow = GetMainWindowHandle();
	if (!hMainWindow || !OpenClipboard(hMainWindow))
		return;
	HANDLE hData = GetClipboardData(uFormat);
	if (hData) {
		uint8 *data = (uint8 *)GlobalLock(hData);
		if (data) {
			uint32 length = GlobalSize(hData);
			if (length) {
				int32 out_length = 0;

				// Allocate space for new scrap in MacOS side
				M68kRegisters r;
				r.d[0] = length;
				Execute68kTrap(0xa71e, &r);	// NewPtrSysClear()
				uint32 scrap_area = r.a[0];

				if (scrap_area) {
					switch (type) {
					case FOURCC('T','E','X','T'):
						D(bug(" clipping TEXT\n"));

						// Convert text from ISO-Latin1 to Mac charset
						uint8 *p = Mac2HostAddr(scrap_area);
						for (int i = 0; i < length; i++) {
							uint8 c = data[i];
							if (c < 0x80) {
								if (c == 0)
									break;
								if (c == 13 && i < length - 1 && data[i + 1] == 10) {	// CR/LF -> CR
									c = 13;
									i++;
								}
							} else if (!no_clip_conversion)
								c = iso2mac[c & 0x7f];
							*p++ = c;
							out_length++;
						}
						break;
					}

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
					uint32 proc_area = Host2MacAddr(proc);
					WriteMacInt32(proc_area +  6, out_length);
					WriteMacInt32(proc_area + 12, type);
					WriteMacInt32(proc_area + 18, scrap_area);
					we_put_this_data = true;
					Execute68k(proc_area, &r);

					// We are done with scratch memory
					r.a[0] = scrap_area;
					Execute68kTrap(0xa01f, &r);		// DisposePtr
				}
			}
			GlobalUnlock(hData);
		}
	}
	CloseClipboard();
}
