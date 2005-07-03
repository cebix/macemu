/*
 *  ether.h - SheepShaver Ethernet Device Driver
 *
 *  SheepShaver (C) 1997-2005 Marc Hellwig and Christian Bauer
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

struct queue;
struct msgb;
typedef struct queue queue_t;
typedef struct msgb mblk_t;

extern uint8 InitStreamModule(void *theID);
extern void TerminateStreamModule(void);
extern int ether_open(queue_t *rdq, void *dev, int flag, int sflag, void *creds);
extern int ether_close(queue_t *rdq, int flag, void *creds);
extern int ether_wput(queue_t *q, mblk_t *mp);
extern int ether_rsrv(queue_t *q);

// System specific and internal functions/data
extern void EtherInit(void);
extern void EtherExit(void);

extern void EtherIRQ(void);

extern void AO_get_ethernet_address(uint32 addr);
extern void AO_enable_multicast(uint32 addr);
extern void AO_disable_multicast(uint32 addr);
extern void AO_transmit_packet(uint32 mp);

extern mblk_t *allocb(size_t size, int pri);
extern void OTEnterInterrupt(void);
extern void OTLeaveInterrupt(void);

extern void ether_dispatch_packet(uint32 p, uint32 length);
extern void ether_packet_received(mblk_t *mp);

extern bool ether_driver_opened;

// Ethernet packet allocator (optimized for 32-bit platforms in real addressing mode)
class EthernetPacket {
#if SIZEOF_VOID_P == 4 && REAL_ADDRESSING
	uint8 packet[1516];
 public:
	uint32 addr(void) const { return (uint32)packet; }
#else
	uint32 packet;
 public:
	EthernetPacket();
	~EthernetPacket();
	uint32 addr(void) const { return packet; }
#endif
};

// Copy packet data from message block to linear buffer (must hold at
// least 1514 bytes), returns packet length
static inline int ether_msgb_to_buffer(uint32 mp, uint8 *p)
{
	int len = 0;
	while (mp) {
		uint32 size = ReadMacInt32(mp + 16) - ReadMacInt32(mp + 12);
		Mac2Host_memcpy(p, ReadMacInt32(mp + 12), size);
		len += size;
		p += size;
		mp = ReadMacInt32(mp + 8);
	}
	return len;
}

extern int32 num_wput;
extern int32 num_error_acks;
extern int32 num_tx_packets;
extern int32 num_tx_raw_packets;
extern int32 num_tx_normal_packets;
extern int32 num_tx_buffer_full;
extern int32 num_rx_packets;
extern int32 num_ether_irq;
extern int32 num_unitdata_ind;
extern int32 num_rx_fastpath;
extern int32 num_rx_no_mem;
extern int32 num_rx_dropped;
extern int32 num_rx_stream_not_ready;
extern int32 num_rx_no_unitdata_mem;

#endif
