/*
 *  ether.h - Ethernet device driver
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

#ifndef ETHER_H
#define ETHER_H

extern int16 EtherOpen(uint32 pb, uint32 dce);
extern int16 EtherControl(uint32 pb, uint32 dce);
extern void EtherReadPacket(uint8 **src, uint32 &dest, uint32 &len, uint32 &remaining);

// System specific and internal functions/data
extern void EtherInit(void);
extern void EtherExit(void);
extern void EtherReset(void);
extern void EtherInterrupt(void);

extern int16 ether_add_multicast(uint32 pb);
extern int16 ether_del_multicast(uint32 pb);
extern int16 ether_attach_ph(uint16 type, uint32 handler);
extern int16 ether_detach_ph(uint16 type);
extern int16 ether_write(uint32 wds);

extern uint8 ether_addr[6];	// Ethernet address (set by EtherInit())
extern bool net_open;		// Flag: initialization succeeded, network device open (set by EtherInit())

// Ethernet driver data in MacOS RAM
enum {
	ed_DeferredTask = 0,	// Deferred Task struct
	ed_Code = 20,			// DT code is stored here
	ed_Result = 30,			// Result for DT
	ed_DCE = 34,			// DCE for DT (must come directly behind ed_Result)
	ed_RHA = 38,			// Read header area
	ed_ReadPacket = 52,		// ReadPacket/ReadRest routines
	SIZEOF_etherdata = 76
};

extern uint32 ether_data;	// Mac address of driver data in MacOS RAM

#endif
