/*
 *	clip_macosx64.mm - Clipboard handling, MacOS X (Pasteboard Manager) implementation
 *
 *	(C) 2012 Jean-Pierre Stierlin
 *	(C) 2012 Alexei Svitkine
 *	(C) 2012 Charles Srstka
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
#import <Cocoa/Cocoa.h>
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
#define TYPE_STYL FOURCC('s','t','y','l')

static PasteboardRef g_pbref;

// Flag for PutScrap(): the data was put by GetScrap(), don't bounce it back to the MacOS X side
static bool we_put_this_data = false;

static bool should_clear = false;

static CFStringRef const UTF16_TEXT_FLAVOR_NAME = CFSTR("public.utf16-plain-text");
static CFStringRef const TEXT_FLAVOR_NAME = CFSTR("com.apple.traditional-mac-plain-text");
static CFStringRef const STYL_FLAVOR_NAME = CFSTR("net.cebix.basilisk.styl-data");

enum {
	FONT_FACE_PLAIN = 0,
	FONT_FACE_BOLD = 1,
	FONT_FACE_ITALIC = 2,
	FONT_FACE_UNDERLINE = 4,
	FONT_FACE_OUTLINE = 8,
	FONT_FACE_SHADOW = 16,
	FONT_FACE_CONDENSED = 32,
	FONT_FACE_EXTENDED = 64
};

static CFStringRef GetUTIFromFlavor(uint32 type)
{
	switch (type) {
		case TYPE_PICT: return kUTTypePICT;
		case TYPE_TEXT: return TEXT_FLAVOR_NAME;
		//case TYPE_TEXT: return UTF16_TEXT_FLAVOR_NAME;
		case TYPE_STYL: return STYL_FLAVOR_NAME;
		case FOURCC('m','o','o','v'): return kUTTypeQuickTimeMovie;
		case FOURCC('s','n','d',' '): return kUTTypeAudio;
		//case FOURCC('u','t','x','t'): return UTF16_TEXT_FLAVOR_NAME;
		case FOURCC('u','t','1','6'): return UTF16_TEXT_FLAVOR_NAME;
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

static CFDataRef ConvertMacTextEncoding(CFDataRef pbData, int from_host)
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
 * Convert Mac font ID to font name
 */

static NSString *FontNameFromFontID(int16_t fontID)
{
	M68kRegisters r;
	r.d[0] = 256;					// Str255: 255 characters + length byte
	Execute68kTrap(0xa71e, &r);		// NewPtrSysClear()
	uint32_t name_area = r.a[0];

	if (!name_area)
		return nil;

	uint8_t proc[] = {
		0x3f, 0x3c, 0, 0,	// move.w	#fontID,-(sp)
		0x2f, 0x0a,			// move.l	A2,-(sp)
		0xa8, 0xff,			// GetFontName()
		M68K_RTS >> 8, M68K_RTS & 0xff
	};

	r.d[0] = sizeof(proc);
	Execute68kTrap(0xa71e, &r);		// NewPtrSysClear()
	uint32_t proc_area = r.a[0];

	if (proc_area) {
		Host2Mac_memcpy(proc_area, proc, sizeof(proc));
		WriteMacInt16(proc_area + 2, fontID);
		r.a[2] = name_area;
		Execute68k(proc_area, &r);

		r.a[0] = proc_area;
		Execute68kTrap(0xa01f, &r); // DisposePtr
	}

	uint8_t * const namePtr = Mac2HostAddr(name_area);

	CFStringRef name = CFStringCreateWithPascalString(kCFAllocatorDefault, namePtr, kCFStringEncodingMacRoman);

	r.a[0] = name_area;
	Execute68kTrap(0xa01f, &r);			// DisposePtr

	return [(NSString *)name autorelease];
}

/*
 * Convert font name to Mac font ID
 */

static int16_t FontIDFromFontName(NSString *fontName)
{
	M68kRegisters r;
	r.d[0] = 256;					// Str255: 255 characters + length byte
	Execute68kTrap(0xa71e, &r);		// NewPtrSysClear()
	uint32_t name_area = r.a[0];

	if (!name_area)
		return 0;

	uint8_t * const namePtr = Mac2HostAddr(name_area);

	CFStringGetPascalString((CFStringRef)fontName, namePtr, 256, kCFStringEncodingMacRoman);

	uint8_t proc[] = {
		0x2f, 0x0a,			// move.l	A2,-(sp)
		0x2f, 0x0b,			// move.l	A3,-(sp)
		0xa9, 0x00,			// GetFNum()
		M68K_RTS >> 8, M68K_RTS & 0xff,
		0, 0
	};

	r.d[0] = sizeof(proc);
	Execute68kTrap(0xa71e, &r);		// NewPtrSysClear()
	uint32_t proc_area = r.a[0];
	int16_t fontID = 0;

	if (proc_area) {
		Host2Mac_memcpy(proc_area, proc, sizeof(proc));
		r.a[2] = name_area;
		r.a[3] = proc_area + 8;

		Execute68k(proc_area, &r);

		fontID = ReadMacInt16(proc_area + 8);

		r.a[0] = proc_area;
		Execute68kTrap(0xa01f, &r); // DisposePtr
	}

	r.a[0] = name_area;
	Execute68kTrap(0xa01f, &r);			// DisposePtr

	return fontID;
}

/*
 * Convert Mac styl to attributed string
 */

static NSAttributedString *ConvertToAttributedString(NSString *string, NSData *stylData)
{
	NSMutableAttributedString *aStr = [[[NSMutableAttributedString alloc] initWithString:string] autorelease];

	if (aStr == nil)
		return nil;

	const uint8_t *bytes = (const uint8_t *)[stylData bytes];
	NSUInteger length = [stylData length];

	if (length < 2)
		return nil;

	uint16_t elements = CFSwapInt16BigToHost(*(uint16_t *)bytes);
	const NSUInteger elementSize = 20;

	if (length < elements * elementSize)
		return nil;

	CFIndex pointer = 2;

	for (NSUInteger i = 0; i < elements; i++) {
		int32_t startChar = CFSwapInt32BigToHost(*(int32_t *)(bytes + pointer)); pointer += 4;
		int16_t height = CFSwapInt16BigToHost(*(int16_t *)&bytes[pointer]); pointer += 2;
		int16_t ascent = CFSwapInt16BigToHost(*(int16_t *)&bytes[pointer]); pointer += 2;
		int16_t fontID = CFSwapInt16BigToHost(*(int16_t *)&bytes[pointer]); pointer += 2;
		uint8_t face = bytes[pointer]; pointer += 2;
		int16_t size = CFSwapInt16BigToHost(*(int16_t *)&bytes[pointer]); pointer += 2;
		uint16_t red = CFSwapInt16BigToHost(*(int16_t *)&bytes[pointer]); pointer += 2;
		uint16_t green = CFSwapInt16BigToHost(*(int16_t *)&bytes[pointer]); pointer += 2;
		uint16_t blue = CFSwapInt16BigToHost(*(int16_t *)&bytes[pointer]); pointer += 2;

		int32_t nextChar;

		if (i + 1 == elements)
			nextChar = [aStr length];
		else
			nextChar = CFSwapInt32BigToHost(*(int32_t *)(bytes + pointer));

		NSMutableDictionary *attrs = [[NSMutableDictionary alloc] init];
		NSColor *color = [NSColor colorWithDeviceRed:(CGFloat)red / 65535.0 green:(CGFloat)green / 65535.0 blue:(CGFloat)blue / 65535.0 alpha:1.0];
		NSFont *font;

		if (fontID == 0) {			// System font
			CGFloat fontSize = (size == 0) ? [NSFont systemFontSize] : (CGFloat)size;
			font = [NSFont systemFontOfSize:fontSize];
		} else if (fontID == 1) {	// Application font
			font = [NSFont userFontOfSize:(CGFloat)size];
		} else {
			NSString *fontName = FontNameFromFontID(fontID);
			font = [NSFont fontWithName:fontName size:(CGFloat)size];
		}

		if (font == nil)
			font = [NSFont userFontOfSize:(CGFloat)size];

		NSFontManager *fm = [NSFontManager sharedFontManager];

		if (face & FONT_FACE_BOLD)
			font = [fm convertFont:font toHaveTrait:NSBoldFontMask];

		if (face & FONT_FACE_ITALIC)
			font = [fm convertFont:font toHaveTrait:NSItalicFontMask];

		if (face & FONT_FACE_CONDENSED)
			font = [fm convertFont:font toHaveTrait:NSCondensedFontMask];

		if (face & FONT_FACE_EXTENDED)
			font = [fm convertFont:font toHaveTrait:NSExpandedFontMask];

		[attrs setObject:font forKey:NSFontAttributeName];

		if (face & FONT_FACE_UNDERLINE)
			[attrs setObject:[NSNumber numberWithInteger:NSUnderlineStyleSingle] forKey:NSUnderlineStyleAttributeName];

		if (face & FONT_FACE_OUTLINE) {
			[attrs setObject:color forKey:NSStrokeColorAttributeName];
			[attrs setObject:[NSNumber numberWithInteger:3] forKey:NSStrokeWidthAttributeName];
		}

		if (face & FONT_FACE_SHADOW) {
			NSShadow *shadow = [[NSShadow alloc] init];
			NSColor *shadowColor = [NSColor colorWithDeviceRed:(CGFloat)red / 65535.0 green:(CGFloat)green / 65535.0 blue:(CGFloat)blue / 65535.0 alpha:0.5];

			[shadow setShadowColor:shadowColor];
			[shadow setShadowOffset:NSMakeSize(2, -2.0)];

			[attrs setObject:shadow forKey:NSShadowAttributeName];

			[shadow release];
		}

		[attrs setObject:color forKey:NSForegroundColorAttributeName];

		[aStr setAttributes:attrs range:NSMakeRange(startChar, nextChar - startChar)];

		[attrs release];
	}

	return aStr;
}

/*
 * Convert attributed string to TEXT/styl
 */

static NSData *ConvertToMacTEXTAndStyl(NSAttributedString *aStr, NSData **outStylData) {
	// Limitations imposed by the Mac TextEdit system.
	// Something to test would be whether using UTF16 causes TextEdit to choke at 16K characters
	// instead of 32K characters, depending on whether this is a byte limit or a true character limit.
	// If the former, UTF8 might be a better choice for encoding here.
	const NSUInteger charLimit = 32 * 1024;
	const NSUInteger elementLimit = 1601;
	
	if([aStr length] > charLimit) {
		aStr = [aStr attributedSubstringFromRange:NSMakeRange(0, charLimit)];
	}
	
	// See comment in CreateRTFDataFromMacTEXTAndStyl regarding encodings; I hope I've interpreted
	// the existing code correctly in this regard
#if __LITTLE_ENDIAN__
	NSStringEncoding encoding = NSUTF16LittleEndianStringEncoding;
#else
	NSStringEncoding encoding = NSUTF16BigEndianStringEncoding;
#endif
	
	NSData *textData = [[aStr string] dataUsingEncoding:encoding];
	NSMutableData *stylData = [NSMutableData dataWithLength:2]; // number of styles to be filled in at the end
	
	NSUInteger length = [aStr length];
	NSUInteger elements = 0;
	
	NSFontManager *fontManager = [NSFontManager sharedFontManager];
	NSLayoutManager *layoutManager = [[[NSLayoutManager alloc] init] autorelease];
	
	for (NSUInteger index = 0; index < length && elements < elementLimit;) {
		NSRange attrRange;
		NSDictionary *attrs = [aStr attributesAtIndex:index effectiveRange:&attrRange];
		
		NSFont *font = [attrs objectForKey:NSFontAttributeName];
		NSColor *color = [[attrs objectForKey:NSForegroundColorAttributeName] colorUsingColorSpaceName:NSDeviceRGBColorSpace device:nil];
		NSFontTraitMask traits = [fontManager traitsOfFont:font];
		NSNumber *underlineStyle = [attrs objectForKey:NSUnderlineStyleAttributeName];
		NSNumber *strokeWidth = [attrs objectForKey:NSStrokeWidthAttributeName];
		NSShadow *shadow = [attrs objectForKey:NSShadowAttributeName];
		
		int16_t hostFontID = FontIDFromFontName([font familyName]);
		
		if (hostFontID == 0) {
			hostFontID = [font isFixedPitch] ? 4 /* Monaco */ : 1 /* Application font */;
		}
		
		int32_t startChar = CFSwapInt32HostToBig((int32_t)index);
		int16_t height = CFSwapInt16HostToBig((int16_t)rint([layoutManager defaultLineHeightForFont:font]));
		int16_t ascent = CFSwapInt16HostToBig((int16_t)rint([font ascender]));
		int16_t fontID = CFSwapInt16HostToBig(hostFontID);
		uint8_t face = 0;
		int16_t size = CFSwapInt16HostToBig((int16_t)rint([font pointSize]));
		uint16_t red = CFSwapInt16HostToBig((int16_t)rint([color redComponent] * 65535.0));
		uint16_t green = CFSwapInt16HostToBig((int16_t)rint([color greenComponent] * 65535.0));
		uint16_t blue = CFSwapInt16HostToBig((int16_t)rint([color blueComponent] * 65535.0));
		
		if (traits & NSBoldFontMask) {
			face |= FONT_FACE_BOLD;
		}
		
		if (traits & NSItalicFontMask) {
			face |= FONT_FACE_ITALIC;
		}
		
		if (traits & NSCondensedFontMask) {
			face |= FONT_FACE_CONDENSED;
		}
		
		if (traits & NSExpandedFontMask) {
			face |= FONT_FACE_EXTENDED;
		}
		
		if (underlineStyle && [underlineStyle integerValue] != NSUnderlineStyleNone) {
			face |= FONT_FACE_UNDERLINE;
		}
		
		if (strokeWidth && [strokeWidth doubleValue] > 0.0) {
			face |= FONT_FACE_OUTLINE;
		}
		
		if (shadow) {
			face |= FONT_FACE_SHADOW;
		}
		
		[stylData appendBytes:&startChar length:4];
		[stylData appendBytes:&height length:2];
		[stylData appendBytes:&ascent length:2];
		[stylData appendBytes:&fontID length:2];
		[stylData appendBytes:&face length:1];
		[stylData increaseLengthBy:1];
		[stylData appendBytes:&size length:2];
		[stylData appendBytes:&red length:2];
		[stylData appendBytes:&green length:2];
		[stylData appendBytes:&blue length:2];
		
		index += attrRange.length;
		elements++;
	}
	
	uint16_t bigEndianElements = CFSwapInt16HostToBig((uint16_t)elements);
	
	[stylData replaceBytesInRange:NSMakeRange(0, 2) withBytes:&bigEndianElements length:2];
	
	if (outStylData)
		*outStylData = stylData;
	
	textData = (NSData *)ConvertMacTextEncoding((CFDataRef)[textData retain], YES);
	
	return [textData autorelease];
}

/*
 * Convert Mac TEXT/styl to RTF
 */

static CFDataRef CreateRTFDataFromMacTEXTAndStyl(CFDataRef textData, CFDataRef stylData)
{
	// Unfortunately, CF does not seem to have any RTF conversion routines, so do this in Cocoa instead.
	// If we are willing to require OS X 10.6 minimum, we should use the NSPasteboardWriting methods
	// instead of putting the RTF data up ourselves.
	
	NSData *rtfData = nil;
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	// I think this is what's going on? ConvertMacTextEncoding() converts to Unicode in host endian?
	// Maybe UTF8 would be a less ambiguous form to store the pasteboard data in?
	
#if __LITTLE_ENDIAN__
	NSStringEncoding encoding = NSUTF16LittleEndianStringEncoding;
#else
	NSStringEncoding encoding = NSUTF16BigEndianStringEncoding;
#endif
	
	NSMutableString *string = [[[NSMutableString alloc] initWithData:(NSData *)textData encoding:encoding] autorelease];
	
	// fix line endings
	[string replaceOccurrencesOfString:@"\r" withString:@"\n" options:NSLiteralSearch range:NSMakeRange(0, [string length])];
	
	if (string != nil) {
		NSAttributedString *aStr = ConvertToAttributedString(string, (NSData *)stylData);
		
		rtfData = [[aStr RTFFromRange:NSMakeRange(0, [aStr length]) documentAttributes:nil] retain];
	}
	
	[pool drain];
	
	return (CFDataRef)rtfData;
}

/*
 * Convert RTF to Mac TEXT/styl
 */

static CFDataRef CreateMacTEXTAndStylFromRTFData(CFDataRef rtfData, CFDataRef *outStylData)
{
	// No easy way to do this at the CF layer, so use Cocoa.
	// Reading RTF should be backward compatible to the early releases of OS X;
	// if we are willing to require OS X 10.6 or better, we should use NSPasteboardReading
	// to read an NSAttributedString off of the pasteboard, which will give us the ability
	// to potentially read more rich-text formats than RTF.
	
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	
	NSMutableAttributedString *aStr = [[[NSMutableAttributedString alloc] initWithRTF:(NSData *)rtfData documentAttributes:nil] autorelease];
	
	// fix line endings
	[[aStr mutableString] replaceOccurrencesOfString:@"\n" withString:@"\r" options:NSLiteralSearch range:NSMakeRange(0, [[aStr mutableString] length])];
	
	NSData *stylData = nil;
	NSData *textData = ConvertToMacTEXTAndStyl(aStr, &stylData);
	
	[textData retain];
	
	if (outStylData)
		*outStylData = (CFDataRef)[stylData retain];
	
	[pool drain];
	
	return (CFDataRef)textData;
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
 * Copy data from the host pasteboard
 */

static CFDataRef CopyPasteboardDataWithFlavor(CFStringRef flavor)
{
	ItemCount itemCount;
	
	if (PasteboardGetItemCount(g_pbref, &itemCount))
		return NULL;
	
	for (UInt32 itemIndex = 1; itemIndex <= itemCount; itemIndex++) {
		PasteboardItemID itemID;
		CFDataRef pbData;
		
		if (PasteboardGetItemIdentifier(g_pbref, itemIndex, &itemID))
			break;
		
		if (!PasteboardCopyItemFlavorData(g_pbref, itemID, flavor, &pbData)) {
			return pbData;
		}
	}
	
	return NULL;
}

/*
 * Zero Mac clipboard
 */

static void ZeroMacClipboard()
{
	D(bug(stderr, "Zeroing Mac clipboard\n"));
	M68kRegisters r;
	static uint8 proc[] = {
		0x59, 0x8f,					// subq.l	#4,sp
		0xa9, 0xfc,					// ZeroScrap()
		0x58, 0x8f,					// addq.l	#4,sp
		M68K_RTS >> 8, M68K_RTS & 0xff
	};
	r.d[0] = sizeof(proc);
	Execute68kTrap(0xa71e, &r);		// NewPtrSysClear()
	uint32 proc_area = r.a[0];
	
	if (proc_area) {
		Host2Mac_memcpy(proc_area, proc, sizeof(proc));
		Execute68k(proc_area, &r);
		
		r.a[0] = proc_area;
		Execute68kTrap(0xa01f, &r); // DisposePtr
	}
}

/*
 * Write data to Mac clipboard
 */

static void WriteDataToMacClipboard(CFDataRef pbData, uint32 type)
{
	D(bug(stderr, "Writing data %s to Mac clipboard with type '%c%c%c%c'\n", [[(NSData *)pbData description] UTF8String],
				(type >> 24) & 0xff, (type >> 16) & 0xff, (type >> 8) & 0xff, type & 0xff));
	
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
			WriteMacInt32(proc_area +  4, CFDataGetLength(pbData));
			WriteMacInt32(proc_area + 10, type);
			WriteMacInt32(proc_area + 16, scrap_area);
			we_put_this_data = true;
			Execute68k(proc_area, &r);
			
			r.a[0] = proc_area;
			Execute68kTrap(0xa01f, &r); // DisposePtr
		}
		
		r.a[0] = scrap_area;
		Execute68kTrap(0xa01f, &r);			// DisposePtr
	}
}

/*
 *	Mac application reads clipboard
 */

void GetScrap(void **handle, uint32 type, int32 offset)
{
	D(bug("GetScrap handle %p, type %4.4s, offset %d\n", handle, (char *)&type, offset));
	
	CFStringRef typeStr;
	PasteboardSyncFlags syncFlags;
	
	if (!g_pbref)
		return;
	
	syncFlags = PasteboardSynchronize(g_pbref);
	if (syncFlags & kPasteboardModified)
		return;
	
	if (!(typeStr = GetUTIFromFlavor(type)))
		return;
	
	if (type == TYPE_TEXT || type == TYPE_STYL) {
		CFDataRef rtfData = CopyPasteboardDataWithFlavor(kUTTypeRTF);
		
		if (rtfData != NULL) {
			CFDataRef stylData = NULL;
			CFDataRef textData = CreateMacTEXTAndStylFromRTFData(rtfData, &stylData);
			
			ZeroMacClipboard();
			
			if(stylData)
				WriteDataToMacClipboard(stylData, TYPE_STYL);
			
			WriteDataToMacClipboard(textData, TYPE_TEXT);
			
			CFRelease(textData);
			CFRelease(stylData);
			CFRelease(rtfData);
			
			return;
		}
	}
	
	CFDataRef pbData = CopyPasteboardDataWithFlavor(typeStr);
	
	if (pbData) {
		if (type == TYPE_TEXT)
			pbData = ConvertMacTextEncoding(pbData, TRUE);
		
		ZeroMacClipboard();
		WriteDataToMacClipboard(pbData, type);
		
		CFRelease(pbData);
	}
}

/*
 * ZeroScrap() is called before a Mac application writes to the clipboard; clears out the previous contents
 */

void ZeroScrap()
{
	D(bug("ZeroScrap\n"));
	
	we_put_this_data = false;
	
	// Defer clearing the host pasteboard until the Mac tries to put something on it.
	// This prevents us from clearing the pasteboard when ZeroScrap() is called during startup.
	should_clear = true;
}

/*
 *	Mac application wrote to clipboard
 */

void PutScrap(uint32 type, void *scrap, int32 length)
{
	D(bug("PutScrap type %4.4s, data %p, length %ld\n", (char *)&type, scrap, (long)length));
	
	PasteboardSyncFlags syncFlags;
	CFStringRef typeStr;
	
	if (!g_pbref)
		return;
	
	if (!(typeStr = GetUTIFromFlavor(type)))
		return;
	
	if (we_put_this_data) {
		we_put_this_data = false;
		return;
	}
	if (length <= 0)
		return;
	
	if (should_clear) {
		PasteboardClear(g_pbref);
		should_clear = false;
	}
	
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
	
	if (type == TYPE_TEXT || type == TYPE_STYL) {
		CFDataRef textData;
		CFDataRef stylData;

		if (PasteboardCopyItemFlavorData(g_pbref, (PasteboardItemID)1, TEXT_FLAVOR_NAME, &textData) != noErr)
			textData = NULL;

		if (PasteboardCopyItemFlavorData(g_pbref, (PasteboardItemID)1, STYL_FLAVOR_NAME, &stylData) != noErr)
			stylData = NULL;

		if (textData != NULL && stylData != NULL) {
			CFDataRef rtfData = CreateRTFDataFromMacTEXTAndStyl(textData, stylData);

			if (rtfData) {
				PasteboardPutItemFlavor(g_pbref, (PasteboardItemID)1, kUTTypeRTF, rtfData, 0);
				CFRelease(rtfData);
			}
		}

		if (textData)
			CFRelease(textData);

		if (stylData)
			CFRelease(stylData);
	}
}
