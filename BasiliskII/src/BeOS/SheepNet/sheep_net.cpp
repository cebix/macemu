/*
 *  sheep_net.cpp - Net server add-on for SheepShaver and Basilisk II
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

#include <KernelKit.h>
#include <SupportKit.h>
#include <add-ons/net_server/NetDevice.h>
#include <add-ons/net_server/NetProtocol.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "sheep_net.h"

#define DEBUG 0

#if DEBUG==1
#define bug pprintf
#elif DEBUG==2
#define bug kprintf
#endif

#if DEBUG
#define D(x) (x)
#else
#define D(x) ;
#endif

static int pprintf(const char* format, ...)
{
	port_id PortNum;
	int len,Ret;
	char Buffer[1024];
	va_list ap;
	
	if ((PortNum = find_port("PortLogger")) == B_NAME_NOT_FOUND)
		return(PortNum);
	for (len=0; len<1024; len++)
		Buffer[len]='\0'; 
	va_start(ap, format);
	vsprintf(Buffer, format, ap);
	Ret = write_port(PortNum, 0, Buffer, strlen(Buffer));
	return(Ret);
}


// Constants
#define NETDUMP_PRIO 1		// Default is 0

const uint32 buffer_size = (sizeof(net_buffer) / B_PAGE_SIZE + 1) * B_PAGE_SIZE;


// SheepNet add-on object
class SheepNetAddOn : public BNetProtocol, BPacketHandler {
public:
	void AddDevice(BNetDevice *dev, const char *name);
	bool PacketReceived(BNetPacket *buf, BNetDevice *dev);
};


// Global variables
static bool shutdown_now = false; 
static bool	active = false;

static thread_id write_thread;			// Packet writer
static sem_id write_sem;				// Semaphore to trigger packet writing
static BNetDevice *EtherCard = NULL;	// The Ethernet card we are attached to
static area_id buffer_area;				// Packet buffer area
static net_buffer *net_buffer_ptr;		// Pointer to packet buffer

static uint32 rd_pos;					// Current read position in packet buffer
static uint32 wr_pos;					// Current write position in packet buffer


/*
 *  Clear packet buffer
 */

static void clear(void)
{
	int i;
	for (i=0;i<READ_PACKET_COUNT;i++) {
		net_buffer_ptr->read[i].cmd = 0;
		net_buffer_ptr->read[i].length = 0;
		net_buffer_ptr->read[i].card = 0;
		net_buffer_ptr->read[i].reserved = 0;
	}
	for (i=0;i<WRITE_PACKET_COUNT;i++) {
		net_buffer_ptr->write[i].cmd = 0;
		net_buffer_ptr->write[i].length = 0;
		net_buffer_ptr->write[i].card = 0;
		net_buffer_ptr->write[i].reserved = 0;
	}
	rd_pos = wr_pos = 0;
}


/*
 *  Packet writer thread
 */

static status_t write_packet_func(void *arg)
{
	while (!shutdown_now) {

		// Read and execute command
		net_packet *p = &net_buffer_ptr->write[wr_pos];
		while (p->cmd & IN_USE) {
			D(bug("wp: %d\n", wr_pos));
			switch (p->cmd >> 8) {

				case ACTIVATE_SHEEP_NET:
					D(bug("activate sheep-net\n"));
					active = false;
					clear();
					active = true;
					goto next;

				case DEACTIVATE_SHEEP_NET:
					D(bug("deactivate sheep-net\n"));
					active = false;
					clear();
					goto next;

				case SHUTDOWN_SHEEP_NET:
					D(bug("shutdown sheep-net\n"));
					active = false;
					clear();
					shutdown_now = true;
					goto next;

				case ADD_MULTICAST: {
					const char *data = (const char *)p->data;
					D(bug("add multicast %02x %02x %02x %02x %02x %02x\n", data[0], data[1], data[2], data[3], data[4], data[5]));
					if (active) {
						status_t result;
						if ((result = EtherCard->AddMulticastAddress(data)) != B_OK) {
							// !! handle error !! error while creating multicast address
							D(bug("error while creating multicast address %d\n", result));
						}
					}
					break;
				}

				case REMOVE_MULTICAST: {
					const char *data = (const char *)p->data;
					D(bug("remove multicast %02x %02x %02x %02x %02x %02x\n", data[0], data[1], data[2], data[3], data[4], data[5]));
					if (active) {
						status_t result;
						if ((result = EtherCard->RemoveMulticastAddress(data)) != B_OK) {
							// !! handle error !! error while removing multicast address
							D(bug("error while removing multicast address %d\n", result));
						}
					}
					break;
				}

				case SHEEP_PACKET: {
					uint32 length = p->length;
					// D(bug("sheep packet %d\n", length));
					if (active) {
						BStandardPacket *packet = new BStandardPacket(length);
						packet->Write(0, (const char *)p->data, length);
						EtherCard->SendPacket(packet);
					}
					break;
				}

				default:
					D(bug("error: unknown port packet type\n"));
					break;
			}
			p->cmd = 0;	// Free packet					
			wr_pos = (wr_pos + 1) % WRITE_PACKET_COUNT;
			p = &net_buffer_ptr->write[wr_pos];
		}

		// Wait for next packet
next:	acquire_sem_etc(write_sem, 1, B_TIMEOUT, 25000);
	}
	return 0;
}


/*
 *  Init the net add-on
 */

static void init_addon()
{
	int i;
	D(bug("init sheep-net\n"));

	// Create packet buffer
	if ((buffer_area = create_area("packet buffer", (void **)&net_buffer_ptr, B_ANY_ADDRESS, buffer_size, B_FULL_LOCK, B_READ_AREA | B_WRITE_AREA)) < B_NO_ERROR) {
		D(bug("FATAL ERROR: can't create shared area\n"));
		return;
	}	

	// Init packet buffer
	clear();
	EtherCard->Address((char *)net_buffer_ptr->ether_addr);
	net_buffer_ptr->read_sem = -1;
	net_buffer_ptr->read_ofs = (uint32)(net_buffer_ptr->read) - (uint32)net_buffer_ptr;
	net_buffer_ptr->read_packet_size = sizeof(net_packet);
	net_buffer_ptr->read_packet_count = READ_PACKET_COUNT;
	if ((write_sem = create_sem(0, "ether write")) < B_NO_ERROR) {
		D(bug("FATAL ERROR: can't create semaphore\n"));
		return;
	}
	net_buffer_ptr->write_sem = write_sem;
	net_buffer_ptr->write_ofs = (uint32)(net_buffer_ptr->write) - (uint32)net_buffer_ptr;
	net_buffer_ptr->write_packet_size = sizeof(net_packet);
	net_buffer_ptr->write_packet_count = WRITE_PACKET_COUNT;

	// Start packet writer thread
	write_thread = spawn_thread(write_packet_func, "sheep_net ether write", B_URGENT_DISPLAY_PRIORITY, NULL);
	resume_thread(write_thread);
}


/*
 *  Add-on attached to Ethernet card
 */

void SheepNetAddOn::AddDevice(BNetDevice *dev, const char *name)
{
	if (dev->Type() != B_ETHER_NET_DEVICE)
		return;
	if (EtherCard != NULL) {
		// !! handle error !! support for multiple ethernet cards ...
		D(bug("error: SheepShaver doesn't support multiple Ethernetcards !\n"));
		return;
	}
	EtherCard = dev;
	init_addon();
	register_packet_handler(this, dev, NETDUMP_PRIO);
}


/*
 *  Ethernet packet received
 */

bool SheepNetAddOn::PacketReceived(BNetPacket *pkt, BNetDevice *dev)
{
	if (shutdown_now) {
		unregister_packet_handler(this, dev);
		return false;
	}
//	D(bug("read_packet_func %d\n", pkt->Size()));
	if (active) {
		D(bug("rp: %d\n", rd_pos));
		net_packet *p = &net_buffer_ptr->read[rd_pos];
		if (p->cmd & IN_USE) {
			D(bug("error: full read buffer ... lost packet\n"));
		} else {
			memcpy(p->data, pkt->Data(), pkt->Size());
			p->length = pkt->Size();
			p->cmd = IN_USE | (SHEEP_PACKET << 8);
			rd_pos = (rd_pos + 1) % READ_PACKET_COUNT;
			release_sem(net_buffer_ptr->read_sem);
		}
	}
	//D(bug("%02x %02x %02x %02x %02x %02x", (uchar) (pkt->Data())[0],(uchar) (pkt->Data())[1],(uchar) (pkt->Data())[2],(uchar) (pkt->Data())[3],(uchar) (pkt->Data())[4],(uchar) (pkt->Data())[5]));
	return false;
}

#pragma export on
extern "C" BNetProtocol *open_protocol(const char *device)
{
	SheepNetAddOn *dev = new SheepNetAddOn;
	return dev;
}
#pragma export off
