/*
 *  rom_patches.h - ROM patches
 *
 *  SheepShaver (C) 1997-2005 Christian Bauer and Marc Hellwig
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

// ROM types
enum {
	ROMTYPE_TNT,
	ROMTYPE_ALCHEMY,
	ROMTYPE_ZANZIBAR,
	ROMTYPE_GAZELLE,
	ROMTYPE_GOSSAMER,
	ROMTYPE_NEWWORLD
};
extern int ROMType;

extern bool DecodeROM(uint8 *data, uint32 size);
extern bool PatchROM(void);
extern void InstallDrivers(void);

extern void AddSifter(uint32 type, int16 id);
extern bool FindSifter(uint32 type, int16 id);

#endif
