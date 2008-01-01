/*
 *  router.h - ip router
 *
 *  Basilisk II (C) 1997-2008 Christian Bauer
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

#ifndef _ROUTER_H_
#define _ROUTER_H_

extern bool is_router_shutting_down;
extern CRITICAL_SECTION router_section;

// Increased by one for each ip packet sent to the emulated enet interface.
extern uint16 next_ip_ident_number;

// Used by incoming icmp packets and internal icmp messages. Host byte order.
extern uint32 macos_ip_address;

// The magic constant
extern const uint8 router_mac_addr[6];

// Used by internal icmp messages. Host byte order.
extern uint32 router_ip_address;

// False under NT/Win2k if the user has no admin rights
extern bool raw_sockets_available;



// Interface exposed to ether_windows module.
bool router_init(void);
void router_final(void);

// Both of these return true if the ethernet module should drop the packet.
bool router_write_packet(uint8 *packet, int len);
bool router_read_packet(uint8 *packet, int len);

#endif // _ROUTER_H_
