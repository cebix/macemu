/*
 *  ether.cpp - Ethernet device driver
 *
 *  Basilisk II (C) 1997-1999 Christian Bauer
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

/*
 *  SEE ALSO
 *    Inside Macintosh: Devices, chapter 1 "Device Manager"
 *    Inside Macintosh: Networking, chapter 11 "Ethernet, Token Ring, and FDDI"
 *    Inside AppleTalk, chapter 3 "Ethernet and TokenTalk Link Access Protocols"
 */

#include <string.h>

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "main.h"
#include "macos_util.h"
#include "emul_op.h"
#include "ether.h"
#include "ether_defs.h"

#define DEBUG 0
#include "debug.h"


// Global variables
uint8 ether_addr[6];	// Ethernet address (set by EtherInit())
bool net_open = false;	// Flag: initialization succeeded, network device open (set by EtherInit())

uint32 ether_data = 0;	// Mac address of driver data in MacOS RAM


/*
 *  Driver Open() routine
 */

int16 EtherOpen(uint32 pb, uint32 dce)
{
	D(bug("EtherOpen\n"));

	// Allocate driver data
	M68kRegisters r;
	r.d[0] = SIZEOF_etherdata;
	Execute68kTrap(0xa71e, &r);		// NewPtrSysClear()
	if (r.a[0] == 0)
		return openErr;
	ether_data = r.a[0];
	D(bug(" data %08lx\n", ether_data));

	WriteMacInt16(ether_data + ed_DeferredTask + qType, dtQType);
	WriteMacInt32(ether_data + ed_DeferredTask + dtAddr, ether_data + ed_Code);
	WriteMacInt32(ether_data + ed_DeferredTask + dtParam, ether_data + ed_Result);
															// Deferred function for signalling that packet write is complete (pointer to mydtResult in a1)
	WriteMacInt16(ether_data + ed_Code, 0x2019);			//  move.l	(a1)+,d0	(result)
	WriteMacInt16(ether_data + ed_Code + 2, 0x2251);		//  move.l	(a1),a1		(dce)
	WriteMacInt32(ether_data + ed_Code + 4, 0x207808fc);	//  move.l	JIODone,a0
	WriteMacInt16(ether_data + ed_Code + 8, 0x4ed0);		//  jmp		(a0)

	WriteMacInt32(ether_data + ed_DCE, dce);
															// ReadPacket/ReadRest routines
	WriteMacInt16(ether_data + ed_ReadPacket, 0x6010);		//	bra		2
	WriteMacInt16(ether_data + ed_ReadPacket + 2, 0x3003);	//  move.w	d3,d0
	WriteMacInt16(ether_data + ed_ReadPacket + 4, 0x9041);	//  sub.w	d1,d0
	WriteMacInt16(ether_data + ed_ReadPacket + 6, 0x4a43);	//  tst.w	d3
	WriteMacInt16(ether_data + ed_ReadPacket + 8, 0x6702);	//  beq		1
	WriteMacInt16(ether_data + ed_ReadPacket + 10, M68K_EMUL_OP_ETHER_READ_PACKET);
	WriteMacInt16(ether_data + ed_ReadPacket + 12, 0x3600);	//1 move.w	d0,d3
	WriteMacInt16(ether_data + ed_ReadPacket + 14, 0x7000);	//  moveq	#0,d0
	WriteMacInt16(ether_data + ed_ReadPacket + 16, 0x4e75);	//  rts
	WriteMacInt16(ether_data + ed_ReadPacket + 18, M68K_EMUL_OP_ETHER_READ_PACKET);	//2
	WriteMacInt16(ether_data + ed_ReadPacket + 20, 0x4a43);	//  tst.w	d3
	WriteMacInt16(ether_data + ed_ReadPacket + 22, 0x4e75);	//  rts
	return 0;
}


/*
 *  Driver Control() routine
 */

int16 EtherControl(uint32 pb, uint32 dce)
{
	uint16 code = ReadMacInt16(pb + csCode);
	D(bug("EtherControl %d\n", code));
	switch (code) {
		case 1:		// KillIO
			return -1;

		case kENetAddMulti:		// Add multicast address
			D(bug("AddMulti %08lx%04x\n", ReadMacInt32(pb + eMultiAddr), ReadMacInt16(pb + eMultiAddr + 4)));
			if (net_open)
				return ether_add_multicast(pb);
			else
				return noErr;

		case kENetDelMulti:		// Delete multicast address
			D(bug("DelMulti %08lx%04x\n", ReadMacInt32(pb + eMultiAddr), ReadMacInt16(pb + eMultiAddr + 4)));
			if (net_open)
				return ether_del_multicast(pb);
			else
				return noErr;

		case kENetAttachPH:		// Attach protocol handler
			D(bug("AttachPH prot %04x, handler %08lx\n", ReadMacInt16(pb + eProtType), ReadMacInt32(pb + ePointer)));
			if (net_open)
				return ether_attach_ph(ReadMacInt16(pb + eProtType), ReadMacInt32(pb + ePointer));
			else
				return noErr;

		case kENetDetachPH:		// Detach protocol handler
			D(bug("DetachPH prot %04x\n", ReadMacInt16(pb + eProtType)));
			if (net_open)
				return ether_detach_ph(ReadMacInt16(pb + eProtType));
			else
				return noErr;

		case kENetWrite:		// Transmit raw Ethernet packet
			D(bug("EtherWrite\n"));
			if (ReadMacInt16(ReadMacInt32(pb + ePointer)) < 14)
				return eLenErr;	// Header incomplete
			if (net_open)
				return ether_write(ReadMacInt32(pb + ePointer));
			else
				return noErr;

		case kENetGetInfo: {	// Get device information/statistics
			D(bug("GetInfo buf %08lx, size %d\n", ReadMacInt32(pb + ePointer), ReadMacInt16(pb + eBuffSize)));

			// Collect info (only ethernet address)
			uint8 buf[18];
			memset(buf, 0, 18);
			memcpy(buf, ether_addr, 6);

			// Transfer info to supplied buffer
			int16 size = ReadMacInt16(pb + eBuffSize);
			if (size > 18)
				size = 18;
			WriteMacInt16(pb + eDataSize, size);	// Number of bytes actually written
			Host2Mac_memcpy(ReadMacInt32(pb + ePointer), buf, size);
			return noErr;
		}

		case kENetSetGeneral:	// Set general mode (always in general mode)
			D(bug("SetGeneral\n"));
			return noErr;

		default:
			printf("WARNING: Unknown EtherControl(%d)\n", code);
			return controlErr;
	}
}


/*
 *  Ethernet ReadPacket routine
 */

void EtherReadPacket(uint8 **src, uint32 &dest, uint32 &len, uint32 &remaining)
{
	D(bug("EtherReadPacket src %08lx, dest %08lx, len %08lx, remaining %08lx\n", *src, dest, len, remaining));
	uint32 todo = len > remaining ? remaining : len;
	Host2Mac_memcpy(dest, *src, todo);
	*src += todo;
	dest += todo;
	len -= todo;
	remaining -= todo;
}
