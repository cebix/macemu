/*
 *  disk.h - Generic disk driver
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

#ifndef DISK_H
#define DISK_H

const int DiskRefNum = -63;				// RefNum of driver
const uint16 DiskDriverFlags = 0x6f04;	// Driver flags

extern const uint8 DiskIcon[258];		// Icon data (copied to ROM by PatchROM())

extern uint32 DiskIconAddr;				// Icon address (Mac address space, set by PatchROM())

extern void DiskInit(void);
extern void DiskExit(void);

extern void DiskInterrupt(void);

extern bool DiskMountVolume(void *fh);

extern int16 DiskOpen(uint32 pb, uint32 dce);
extern int16 DiskPrime(uint32 pb, uint32 dce);
extern int16 DiskControl(uint32 pb, uint32 dce);
extern int16 DiskStatus(uint32 pb, uint32 dce);

#endif
