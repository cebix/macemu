/*
 *  sony.h - Replacement .Sony driver (floppy drives)
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
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

#ifndef SONY_H
#define SONY_H

const int SonyRefNum = -5;				// RefNum of driver
const uint16 SonyDriverFlags = 0x6f00;	// Driver flags

extern const uint8 SonyDiskIcon[258];	// Icon data (copied to ROM by PatchROM())
extern const uint8 SonyDriveIcon[258];

extern uint32 SonyDiskIconAddr;			// Icon addresses (Mac address space, set by PatchROM())
extern uint32 SonyDriveIconAddr;

extern void SonyInit(void);
extern void SonyExit(void);

extern void SonyInterrupt(void);

extern bool SonyMountVolume(void *fh);

extern int16 SonyOpen(uint32 pb, uint32 dce);
extern int16 SonyPrime(uint32 pb, uint32 dce);
extern int16 SonyControl(uint32 pb, uint32 dce);
extern int16 SonyStatus(uint32 pb, uint32 dce);

#endif
