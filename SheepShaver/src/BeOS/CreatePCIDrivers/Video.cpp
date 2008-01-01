/*
 *  Video.cpp - SheepShaver video PCI driver stub
 *
 *  SheepShaver (C) 1997-2008 Christian Bauer and Marc Hellwig
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
#include "xlowmem.h"


/*
 *  Driver Description structure
 */

struct DriverDescription {
	uint32 driverDescSignature;
	uint32 driverDescVersion;
	char nameInfoStr[32];
	uint32 version;
	uint32 driverRuntime;
	char driverName[32];
	uint32 driverDescReserved[8];
	uint32 nServices;
	uint32 serviceCategory;
	uint32 serviceType;
	uint32 serviceVersion;
};

#pragma export on
struct DriverDescription TheDriverDescription = {
	'mtej',
	0,
	"\pvideo",
	0x01008000,	// V1.0.0final
	6,			// kDriverIsUnderExpertControl, kDriverIsOpenedUponLoad
	"\pDisplay_Video_Apple_Sheep",
	0, 0, 0, 0, 0, 0, 0, 0,
	1,
	'ndrv',
	'vido',
	0x01000000,	// V1.0.0
};
#pragma export off


// Prototypes for exported functions
extern "C" {
#pragma export on
extern int16 DoDriverIO(void *spaceID, void *commandID, void *commandContents, uint32 commandCode, uint32 commandKind);
#pragma export off
}


/*
 *  Do driver IO
 */

asm int16 DoDriverIO(void *spaceID, void *commandID, void *commandContents, uint32 commandCode, uint32 commandKind)
{
	lwz		r2,XLM_TOC
	lwz		r0,XLM_VIDEO_DOIO
	mtctr	r0
	bctr
}
