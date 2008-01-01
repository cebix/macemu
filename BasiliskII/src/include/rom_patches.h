/*
 *  rom_patches.h - ROM patches
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

#ifndef ROM_PATCHES_H
#define ROM_PATCHES_H

// ROM version number, set by CheckROM()
enum {
	ROM_VERSION_64K = 0x0000,		// Original Macintosh (64KB)
	ROM_VERSION_PLUS = 0x0075,		// Mac Plus ROMs (128KB)
	ROM_VERSION_CLASSIC = 0x0276,	// SE/Classic ROMs (256/512KB)
	ROM_VERSION_II = 0x0178,		// Not 32-bit clean Mac II ROMs (256KB)
	ROM_VERSION_32 = 0x067c			// 32-bit clean Mac II ROMs (512KB/1MB)
};

extern uint16 ROMVersion;

// ROM offset of breakpoint, used by PatchROM()
extern uint32 ROMBreakpoint;

// ROM offset of UniversalInfo, set by PatchROM()
extern uint32 UniversalInfo;

// Mac address of PutScrap() patch
extern uint32 PutScrapPatch;

// Mac address of GetScrap() patch
extern uint32 GetScrapPatch;

// Flag: print ROM information in PatchROM()
extern bool PrintROMInfo;

extern bool CheckROM(void);
extern bool PatchROM(void);
extern void InstallDrivers(uint32 pb);
extern void InstallSERD(void);
extern void PatchAfterStartup(void);

#endif
