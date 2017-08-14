/*
 * pict.c - convert an image to PICT.
 *
 * Currently creates a bitmap PICT resource; vector graphics are not preserved.
 *
 * By Charles Srstka.
 *
 * Public Domain. Do with it as you wish.
 *
 */

/*
 * PICT format:
 *
 * Size: 2 bytes
 * Bounding Rect: 8 bytes
 *
 * This is followed by a series of opcodes.
 * Each opcode is 1 byte long for a Version 1 PICT, or 2 bytes long for a Version 2 PICT.
 * The ones we currently care about are:
 *
 * 0x0011	VersionOp: begins PICT version 2
 * 0x02ff	Version: identifies PICT version 2
 * 0x0c00	HeaderOp: followed by 24 header bytes
 * 				4 bytes: 0xFFFFFFFF for regular v2 PICT or 0xFFFE0000 for extended v2 PICT
 *				16 bytes: fixed-point bounding rectangle (or resolution, if an extended v2 PICT)
 *				4 bytes: reserved
 *
 * 0x0001	Clip: set clipping region: followed by variable-sized region
 * 0x001e	DefHilite: set default highlight color
 * 0x009b	DirectBitsRgn: bitmap data
 * 				pixMap:		50 bytes (PixMap)
 *				srcRect:	8 bytes (Rect)
 *				dstRect:	8 bytes (Rect)
 *				mode:		2 bytes (Mode)
 *				maskRgn:	variable (Region)
 *				pixData:	variable
 * 0x00ff	End of File
 */

/*
 * PixMap format:
 *
 * baseAddr:	Ptr			(4 bytes)
 * rowBytes:	Integer		(2 bytes)
 * bounds:		Rect		(8 bytes)
 * pmVersion:	Integer		(2 bytes)
 * packType:	Integer		(2 bytes)
 * packSize:	LongInt		(4 bytes)
 * hRes:		Fixed		(4 bytes)
 * vRes:		Fixed		(4 bytes)
 * pixelType:	Integer		(2 bytes)
 * pixelSize:	Integer		(2 bytes)
 * cmpCount:	Integer		(2 bytes)
 * cmpSize:		Integer		(2 bytes)
 * planeBytes:	LongInt		(4 bytes)
 * pmTable:		CTabHandle	(4 bytes)
 * pmReserved:	LongInt		(4 bytes)
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>

static ssize_t CompressUsingRLE(uint8_t *row, uint16_t uncmpLength, uint8_t *outBuf, size_t bufSize)
{
	int byteCountLength = 1 + (uncmpLength > 250);

	uint16_t cmpCursor = byteCountLength;
	uint16_t cursor = 0;

	// enough to output the data uncompressed if we have to, plus the length bytes
	size_t maxSize = byteCountLength + uncmpLength + (uncmpLength + 126) / 127;

	int outOfRoom = 0;
	uint16_t cmpLength;

	if (row == NULL || outBuf == NULL || bufSize == 0)
		return maxSize;

	while (cursor < uncmpLength) {
		uint8_t byte = row[cursor++];
		uint8_t nextByte;

		if (cursor < uncmpLength && (nextByte = row[cursor]) == byte) {
			int8_t matches = 1;

			while (++cursor < uncmpLength && matches < 127 && row[cursor] == byte) {
				matches++;
			}

			if(cmpCursor + 2 > bufSize) {
				outOfRoom = 1;
				break;
			}

			outBuf[cmpCursor++] = -matches;
			outBuf[cmpCursor++] = byte;
		} else {
			uint8_t literals = 0;
			uint8_t i;

			while (cursor + literals + 1 < uncmpLength && literals < 127 && nextByte != row[cursor + literals + 1]) {
				nextByte = row[cursor + literals + 1];
				literals++;
			}

			if(cmpCursor + 2 + literals > bufSize) {
				outOfRoom = 1;
				break;
			}

			outBuf[cmpCursor++] = literals;
			outBuf[cmpCursor++] = byte;

			for (i = 0; i < literals; i++) {
				outBuf[cmpCursor++] = row[cursor++];
			}
		}
	}

	if(outOfRoom) {
		// Trying to compress this just made it larger; just output the data uncompressed instead

		if(bufSize < maxSize) {
			// sorry folks, don't have enough buffer
			return -1;
		}

		cursor = 0;
		cmpCursor = byteCountLength;

		while (cursor < uncmpLength) {
			uint8_t bytesToCopy = uncmpLength - cursor > 128 ? 128 : uncmpLength - cursor;

			outBuf[cmpCursor++] = bytesToCopy - 1;
			memcpy(outBuf + cmpCursor, row + cursor, bytesToCopy); cmpCursor += bytesToCopy; cursor += bytesToCopy;
		}

		cmpLength = cmpCursor - 1;
	}

	cmpLength = cmpCursor - byteCountLength;

	if (byteCountLength == 2) {
		outBuf[0] = cmpLength >> 8;
		outBuf[1] = cmpLength & 0xff;
	} else {
		outBuf[0] = cmpLength;
	}

	return cmpCursor;
}

ssize_t ConvertRGBAToPICT(uint8_t *buf, unsigned long bufSize, uint8_t *rgbaPixels, uint16_t width, uint16_t height)
{
	unsigned long initialSize = (10	/* size + rect */ +
								 6	/* initial opcodes */ +
								 24	/* header */ +
								 12	/* clip region */ +
								 2	/* DefHilite */ +
								 70	/* DirectBitsRgn - pixData - maskRgn */ +
								 10	/* maskRgn */);

#define RECT_SIZE		8
#define REGION_SIZE		10
#define FIXED_RECT_SIZE	16

	char rect[RECT_SIZE] = {0, 0, 0, 0, height >> 8, height & 0xff, width >> 8, width & 0xff };
	char region[REGION_SIZE] = { 0x00, 0x0a, rect[0], rect[1], rect[2], rect[3], rect[4], rect[5], rect[6], rect[7] };
	char fixedRect[FIXED_RECT_SIZE] = { 0, 0, 0, 0, 0, 0, 0, 0, width >> 8, width & 0xff, 0, 0, height >> 8, height & 0xff, 0, 0 };

	uint32_t hDPI = htonl(0x00480000); // 72 dpi
	uint32_t vDPI = hDPI;

	uint16_t bytesPerPixel = 4; // RGBA
	uint16_t bytesPerRow = width * bytesPerPixel;

	unsigned long cursor = 2; // size bytes filled in at the end

	ssize_t cmpBufSize = CompressUsingRLE(NULL, bytesPerRow, NULL, 0);
	uint8_t cmpBuf[cmpBufSize];

	uint16_t i;

	if (buf == NULL || bufSize == 0) {
		// Give an upper bound for the buffer size.

		return initialSize + height * cmpBufSize + 2;
	}

	if (bufSize < initialSize) {
		return -1;
	}

	memcpy(buf + cursor, rect, RECT_SIZE); cursor += RECT_SIZE;

	buf[cursor++] = 0x00; buf[cursor++] = 0x11; buf[cursor++] = 0x02; buf[cursor++] = 0xff;

	buf[cursor++] = 0x0c; buf[cursor++] = 0x00;
	buf[cursor++] = 0xff; buf[cursor++] = 0xff; buf[cursor++] = 0xff; buf[cursor++] = 0xff;
	memcpy(buf + cursor, fixedRect, FIXED_RECT_SIZE); cursor += FIXED_RECT_SIZE;
	memset(buf + cursor, '\0', 4); cursor += 4;

	buf[cursor++] = 0x00; buf[cursor++] = 0x1e;

	buf[cursor++] = 0x00; buf[cursor++] = 0x01;
	memcpy(buf + cursor, region, REGION_SIZE); cursor += REGION_SIZE;

	buf[cursor++] = 0x00; buf[cursor++] = 0x9b;
	memset(buf + cursor, '\0', 4); cursor += 4; // I think this pointer isn't used
	buf[cursor++] = (bytesPerRow >> 8) | 0x80; buf[cursor++] = bytesPerRow & 0xff; // rowBytes
	memcpy(buf + cursor, rect, RECT_SIZE); cursor += RECT_SIZE; //bounds
	buf[cursor++] = 0x00; buf[cursor++] = 0x00; // pmVersion
	buf[cursor++] = 0x00; buf[cursor++] = 0x04; // packType
	buf[cursor++] = 0x00; buf[cursor++] = 0x00; buf[cursor++] = 0x00; buf[cursor++] = 0x00; // packSize is always 0
	memcpy(buf + cursor, &hDPI, 4); cursor += 4; // hRes
	memcpy(buf + cursor, &vDPI, 4); cursor += 4; // vRes
	buf[cursor++] = 0x00; buf[cursor++] = 0x10; // pixelType; direct device
	buf[cursor++] = 0x00; buf[cursor++] = 0x20; // pixelSize; 32 bits per pixel
	buf[cursor++] = bytesPerPixel >> 8; buf[cursor++] = bytesPerPixel & 0xff; // components per pixel
	buf[cursor++] = 0x00; buf[cursor++] = 0x08; // 8 bits per component
	memset(buf + cursor, '\0', 4); cursor += 4; // planeBytes isn't used
	memset(buf + cursor, '\0', 4); cursor += 4; // don't think we need pmTable
	memset(buf + cursor, '\0', 4); cursor += 4; // reserved

	memcpy(buf + cursor, rect, RECT_SIZE); cursor += RECT_SIZE;
	memcpy(buf + cursor, rect, RECT_SIZE); cursor += RECT_SIZE;
	buf[cursor++] = 0x00; buf[cursor++] = 0x00; // no transfer mode
	memcpy(buf + cursor, region, REGION_SIZE); cursor += REGION_SIZE;

	for (i = 0; i < height; i++) {
		uint8_t row[bytesPerRow];
		ssize_t cmpLength;
		uint16_t j;

		for (j = 0; j < width; j++) {
			row[j] = rgbaPixels[i * bytesPerRow + j * bytesPerPixel + 3];
			row[width + j] = rgbaPixels[i * bytesPerRow + j * bytesPerPixel];
			row[width * 2 + j] = rgbaPixels[i * bytesPerRow + j * bytesPerPixel + 1];
			row[width * 3 + j] = rgbaPixels[i * bytesPerRow + j * bytesPerPixel + 2];
		}

		cmpLength = CompressUsingRLE(row, bytesPerRow, cmpBuf, cmpBufSize);

		if (cmpLength < 0 || cursor + cmpLength > bufSize)
			return -1;

		memcpy(buf + cursor, cmpBuf, cmpLength); cursor += cmpLength;
	}

	// Fun fact: forgetting to put 0x00ff at the end of a PICT picture causes the entire
	// Classic Mac OS to crash when it tries to read it! Don't ask me how I learned this.
	if (cursor + 2 > bufSize)
		return -1;

	buf[cursor++] = 0x00; buf[cursor++] = 0xff;

	if(cursor > UINT16_MAX) {
		buf[0] = buf[1] = 0xff;
	} else {
		buf[0] = cursor >> 8;
		buf[1] = cursor & 0xff;
	}

	return cursor;
}
