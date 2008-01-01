/*
 *  sheep_net.h - Net server add-on for SheepShaver and Basilisk II
 *
 *  SheepShaver (C) 1997-2008 Marc Hellwig and Christian Bauer
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

#ifndef SHEEP_NET_H
#define SHEEP_NET_H

// Net buffer dimensions
#define READ_PACKET_COUNT 10
#define WRITE_PACKET_COUNT 10

// Net packet
struct net_packet {
	uint32 cmd;				// Command
	uint32 length;			// Data length
	uint32 card;			// Network card ID
	uint32 reserved;
	uint8 data[1584];
};

// Net buffer (shared area)
struct net_buffer {
	uint8 ether_addr[6];		// Ethernet address
	uint8 filler1[2];
	sem_id read_sem;			// Semaphore for read packets
	uint32 read_ofs;
	uint32 read_packet_size;
	uint32 read_packet_count;
	sem_id write_sem;			// Semaphore for write packets
	uint32 write_ofs;
	uint32 write_packet_size;
	uint32 write_packet_count;
	uint8 filler[24];
	net_packet read[READ_PACKET_COUNT];
	net_packet write[WRITE_PACKET_COUNT];
};

// Packet commands
#define SHEEP_PACKET 0
#define ADD_MULTICAST 1
#define REMOVE_MULTICAST 2
#define ACTIVATE_SHEEP_NET 8
#define DEACTIVATE_SHEEP_NET 9
#define SHUTDOWN_SHEEP_NET 10

#define IN_USE 1

#endif
