/*
 *  xpram_amiga.cpp - XPRAM handling, AmigaOS specific stuff
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

#include <exec/types.h>
#define __USE_SYSBASE
#include <proto/dos.h>
#include <inline/dos.h>

#include "sysdeps.h"
#include "xpram.h"


// XPRAM file name
#if POWERPC_ROM
static char XPRAM_FILE_NAME[] = "ENV:SheepShaver_NVRAM";
static char XPRAM_FILE_NAME_ARC[] = "ENVARC:SheepShaver_NVRAM";
#else
static char XPRAM_FILE_NAME[] = "ENV:BasiliskII_XPRAM";
static char XPRAM_FILE_NAME_ARC[] = "ENVARC:BasiliskII_XPRAM";
#endif


/*
 *  Load XPRAM from settings file
 */

void LoadXPRAM(void)
{
	BPTR fh;
	if ((fh = Open(XPRAM_FILE_NAME, MODE_OLDFILE)) != NULL) {
		Read(fh, XPRAM, XPRAM_SIZE);
		Close(fh);
	}
}


/*
 *  Save XPRAM to settings file
 */

void SaveXPRAM(void)
{
	BPTR fh;
	if ((fh = Open(XPRAM_FILE_NAME, MODE_NEWFILE)) != NULL) {
		Write(fh, XPRAM, XPRAM_SIZE);
		Close(fh);
	}
	if ((fh = Open(XPRAM_FILE_NAME_ARC, MODE_NEWFILE)) != NULL) {
		Write(fh, XPRAM, XPRAM_SIZE);
		Close(fh);
	}
}


/*
 *  Delete PRAM file
 */

void ZapPRAM(void)
{
	DeleteFile(XPRAM_FILE_NAME);
	DeleteFile(XPRAM_FILE_NAME_ARC);
}
