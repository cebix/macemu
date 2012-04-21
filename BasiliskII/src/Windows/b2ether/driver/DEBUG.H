/*
 *  b2ether driver -- derived from DDK packet driver sample
 *
 *  Basilisk II (C) 1997-1999 Christian Bauer
 *
 *  Windows platform specific code copyright (C) Lauri Pesonen
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

#if DBG

#define IF_PACKETDEBUG(f) if (PacketDebugFlag & (f))
extern ULONG PacketDebugFlag;

#define PACKET_DEBUG_LOUD               0x00000001  // debugging info
#define PACKET_DEBUG_VERY_LOUD          0x00000002  // excessive debugging info

#define PACKET_DEBUG_INIT               0x00000100  // init debugging info

#define IF_LOUD(A) IF_PACKETDEBUG( PACKET_DEBUG_LOUD ) { A }
#define IF_VERY_LOUD(A) IF_PACKETDEBUG( PACKET_DEBUG_VERY_LOUD ) { A }
#define IF_INIT_LOUD(A) IF_PACKETDEBUG( PACKET_DEBUG_INIT ) { A }

#else

#define IF_LOUD(A)
#define IF_VERY_LOUD(A)
#define IF_INIT_LOUD(A)

#endif
