/*
 *  sheep_net.h - SheepShaver net server add-on
 *
 *  SheepShaver (C) 1997-1999 Mar"c Hellwig and Christian Bauer
 *  All rights reserved.
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
