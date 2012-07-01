/*
 *	clip_macosx64.mm - Clipboard handling, MacOS X (Pasteboard Manager) implementation
 *
 *	(C) 2012 Jean-Pierre Stierlin
 *	(C) 2012 Alexei Svitkine
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "sysdeps.h"
#define _UINT64
#include <ApplicationServices/ApplicationServices.h>

#include "clip.h"
#include "main.h"
#include "cpu_emulation.h"
#include "emul_op.h"

#define DEBUG 0
#include "debug.h"

#ifndef FOURCC
#define FOURCC(a,b,c,d) (((uint32)(a) << 24) | ((uint32)(b) << 16) | ((uint32)(c) << 8) | (uint32)(d))
#endif

#define TYPE_PICT FOURCC('P','I','C','T')
#define TYPE_TEXT FOURCC('T','E','X','T')

static PasteboardRef g_pbref;

// Flag for PutScrap(): the data was put by GetScrap(), don't bounce it back to the MacOS X side
static bool we_put_this_data = false;

static CFStringRef GetUTIFromFlavor(uint32 type)
{
	switch (type) {
		case TYPE_PICT: return kUTTypePICT;
		case TYPE_TEXT: return CFSTR("com.apple.traditional-mac-plain-text");
		//case TYPE_TEXT: return CFSTR("public.utf16-plain-text");
		//case FOURCC('s','t','y','l'): return CFSTR("????");
		case FOURCC('m','o','o','v'): return kUTTypeQuickTimeMovie;
		case FOURCC('s','n','d',' '): return kUTTypeAudio;
		//case FOURCC('u','t','x','t'): return CFSTR("public.utf16-plain-text");
		case FOURCC('u','t','1','6'): return CFSTR("public.utf16-plain-text");
		//case FOURCC('u','s','t','l'): return CFSTR("????");
		case FOURCC('i','c','n','s'): return kUTTypeAppleICNS;
		default: return NULL;
	}
}

/*
 *	Get current system script encoding on Mac
 */

#define smMacSysScript 18
#define smMacRegionCode 40

static int GetMacScriptManagerVariable(uint16 id)
{
	int ret = -1;
	M68kRegisters r;
	static uint8 proc[] = {
		0x59, 0x4f,							// subq.w	 #4,sp
		0x3f, 0x3c, 0x00, 0x00,				// move.w	 #id,-(sp)
		0x2f, 0x3c, 0x84, 0x02, 0x00, 0x08, // move.l	 #-2080243704,-(sp)
		0xa8, 0xb5,							// ScriptUtil()
		0x20, 0x1f,							// move.l	 (a7)+,d0
		M68K_RTS >> 8, M68K_RTS & 0xff
	};
	r.d[0] = sizeof(proc);
	Execute68kTrap(0xa71e, &r);		// NewPtrSysClear()
	uint32 proc_area = r.a[0];
	if (proc_area) {
		Host2Mac_memcpy(proc_area, proc, sizeof(proc));
		WriteMacInt16(proc_area +  4, id);
		Execute68k(proc_area, &r);
		ret = r.d[0];
		r.a[0] = proc_area;
		Execute68kTrap(0xa01f, &r); // DisposePtr
	}
	return ret;
}

/*
 *	Convert utf-16 from/to system script encoding on Mac
 */

CFDataRef ConvertMacTextEncoding(CFDataRef pbData, int from_host)
{
	static TextEncoding g_textEncodingHint = kTextEncodingUnknown;
	static UnicodeMapping uMapping;
	static UnicodeToTextInfo utInfo;
	static TextToUnicodeInfo tuInfo;
	static int ready;

	// should we check this only once ?
	if (g_textEncodingHint == kTextEncodingUnknown) {
		int script = GetMacScriptManagerVariable(smMacSysScript);
		int region = GetMacScriptManagerVariable(smMacRegionCode);
		if (UpgradeScriptInfoToTextEncoding(script, kTextLanguageDontCare, region, NULL, &g_textEncodingHint))
			g_textEncodingHint = kTextEncodingMacRoman;
		uMapping.unicodeEncoding = CreateTextEncoding(kTextEncodingUnicodeV2_0, kTextEncodingDefaultVariant, kUnicode16BitFormat);
		uMapping.otherEncoding	 = GetTextEncodingBase(g_textEncodingHint);
		uMapping.mappingVersion	 = kUnicodeUseLatestMapping;
		ready = !CreateUnicodeToTextInfo(&uMapping, &utInfo) && !CreateTextToUnicodeInfo(&uMapping, &tuInfo);
	}
	if (!ready)
		return pbData;

	ByteCount byteCount = CFDataGetLength(pbData);
	ByteCount outBytesLength = byteCount * 2;
	LogicalAddress outBytesPtr = malloc(byteCount * 2);
	if (!outBytesPtr)
		return pbData;
	
	ByteCount outBytesConverted;
	OSStatus err;
	if (from_host) {
		err = ConvertFromUnicodeToText(utInfo, byteCount, (UniChar *)CFDataGetBytePtr(pbData),
									   kUnicodeLooseMappingsMask,
									   0, NULL, 0, NULL,
									   outBytesLength,
									   &outBytesConverted, &outBytesLength, outBytesPtr);
	} else {
		err = ConvertFromTextToUnicode(tuInfo, byteCount, CFDataGetBytePtr(pbData),
									   kUnicodeLooseMappingsMask,
									   0, NULL, 0, NULL,
									   outBytesLength,
									   &outBytesConverted, &outBytesLength, (UniChar *)outBytesPtr);
	}

	if (err == noErr && outBytesConverted == byteCount) {
		CFDataRef pbDataConverted = CFDataCreate(kCFAllocatorDefault, (UInt8*)outBytesPtr, outBytesLength);
		if (pbDataConverted) {
			CFRelease(pbData);
			pbData = pbDataConverted;
		}
	}
	free(outBytesPtr);

	return pbData;
}

/*
 *	Initialization
 */

void ClipInit(void)
{
	OSStatus err = PasteboardCreate(kPasteboardClipboard, &g_pbref);
	if (err) {
		D(bug("could not create Pasteboard\n"));
		g_pbref = NULL;
	}
}


/*
 *	Deinitialization
 */

void ClipExit(void)
{
	if (g_pbref) {
		CFRelease(g_pbref);
		g_pbref = NULL;
	}
}


/*
 *	Mac application reads clipboard
 */

void GetScrap(void **handle, uint32 type, int32 offset)
{
	D(bug("GetScrap handle %p, type %4.4s, offset %d\n", handle, &type, offset));

	CFStringRef typeStr;
	PasteboardSyncFlags syncFlags;
	ItemCount itemCount;

	if (!g_pbref)
		return;

	syncFlags = PasteboardSynchronize(g_pbref);
	if (syncFlags & kPasteboardModified)
		return;

	if (PasteboardGetItemCount(g_pbref, &itemCount))
		return;
	
	if (!(typeStr = GetUTIFromFlavor(type)))
		return;

	for (UInt32 itemIndex = 1; itemIndex <= itemCount; itemIndex++) {
		PasteboardItemID itemID;
		CFDataRef pbData;

		if (PasteboardGetItemIdentifier(g_pbref, itemIndex, &itemID))
			break;
	
		if (!PasteboardCopyItemFlavorData(g_pbref, itemID, typeStr, &pbData)) {
			if (type == TYPE_TEXT)
				pbData = ConvertMacTextEncoding(pbData, TRUE);
			if (pbData) {
				// Allocate space for new scrap in MacOS side
				M68kRegisters r;
				r.d[0] = CFDataGetLength(pbData);
				Execute68kTrap(0xa71e, &r);				// NewPtrSysClear()
				uint32 scrap_area = r.a[0];
	
				// Get the native clipboard data
				if (scrap_area) {
					uint8 * const data = Mac2HostAddr(scrap_area);
	
					memcpy(data, CFDataGetBytePtr(pbData), CFDataGetLength(pbData));
	
					// Add new data to clipboard
					static uint8 proc[] = {
						0x59, 0x8f,					// subq.l	#4,sp
						0xa9, 0xfc,					// ZeroScrap()
						0x2f, 0x3c, 0, 0, 0, 0,		// move.l	#length,-(sp)
						0x2f, 0x3c, 0, 0, 0, 0,		// move.l	#type,-(sp)
						0x2f, 0x3c, 0, 0, 0, 0,		// move.l	#outbuf,-(sp)
						0xa9, 0xfe,					// PutScrap()
						0x58, 0x8f,					// addq.l	#4,sp
						M68K_RTS >> 8, M68K_RTS & 0xff
					};
					r.d[0] = sizeof(proc);
					Execute68kTrap(0xa71e, &r);		// NewPtrSysClear()
					uint32 proc_area = r.a[0];
			
					if (proc_area) {
						Host2Mac_memcpy(proc_area, proc, sizeof(proc));
						WriteMacInt32(proc_area +  6, CFDataGetLength(pbData));
						WriteMacInt32(proc_area + 12, type);
						WriteMacInt32(proc_area + 18, scrap_area);
						we_put_this_data = true;
						Execute68k(proc_area, &r);
			
						r.a[0] = proc_area;
						Execute68kTrap(0xa01f, &r); // DisposePtr
					}
			
					r.a[0] = scrap_area;
					Execute68kTrap(0xa01f, &r);			// DisposePtr
				}
			}
	
			CFRelease(pbData);
			break;
		}
	}
}


/*
 *	Mac application wrote to clipboard
 */

void PutScrap(uint32 type, void *scrap, int32 length)
{
	static bool clear = true;
	D(bug("PutScrap type %4.4s, data %08lx, length %ld\n", &type, scrap, length));

	PasteboardSyncFlags syncFlags;
	CFStringRef typeStr;

	if (!g_pbref)
		return;

	if (!(typeStr = GetUTIFromFlavor(type)))
		return;

	if (we_put_this_data) {
		we_put_this_data = false;
		clear = true;
		return;
	}
	if (length <= 0)
		return;

	if (clear && PasteboardClear(g_pbref))
		return;
	syncFlags = PasteboardSynchronize(g_pbref);
	if ((syncFlags & kPasteboardModified) || !(syncFlags & kPasteboardClientIsOwner))
		return;

	CFDataRef pbData = CFDataCreate(kCFAllocatorDefault, (UInt8*)scrap, length);
	if (!pbData)
		return;

	if (type == TYPE_TEXT)
		pbData = ConvertMacTextEncoding(pbData, FALSE);

	if (pbData) {
		PasteboardPutItemFlavor(g_pbref, (PasteboardItemID)1, typeStr, pbData, 0);
		CFRelease(pbData);
	}
}
