/*
 *  ether_unix.cpp - Ethernet device driver, Unix specific stuff
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

#include "sysdeps.h"

#include <sys/ioctl.h>
#include <sys/poll.h>
#include <semaphore.h>
#include <errno.h>
#include <stdio.h>

#include "cpu_emulation.h"
#include "main.h"
#include "macos_util.h"
#include "prefs.h"
#include "user_strings.h"
#include "ether.h"
#include "ether_defs.h"

#define DEBUG 0
#include "debug.h"

#define MONITOR 0


// List of attached protocols
struct NetProtocol {
	NetProtocol *next;
	uint16 type;
	uint32 handler;
};

static NetProtocol *prot_list = NULL;


// Global variables
static int fd = -1;							// fd of sheep_net device
static pthread_t ether_thread;				// Packet reception thread
static pthread_attr_t ether_thread_attr;	// Packet reception thread attributes
static bool thread_active = false;			// Flag: Packet reception thread installed
static sem_t int_ack;						// Interrupt acknowledge semaphore
static bool is_ethertap;					// Flag: Ethernet device is ethertap

// Prototypes
static void *receive_func(void *arg);


/*
 *  Find protocol in list
 */

static NetProtocol *find_protocol(uint16 type)
{
	// All 802.2 types are the same
	if (type <= 1500)
		type = 0;

	// Search list (we could use hashing here but there are usually only three
	// handlers installed: 0x0000 for AppleTalk and 0x0800/0x0806 for TCP/IP)
	NetProtocol *p = prot_list;
	while (p) {
		if (p->type == type)
			return p;
		p = p->next;
	}
	return NULL;
}


/*
 *  Remove all protocols
 */

static void remove_all_protocols(void)
{
	NetProtocol *p = prot_list;
	while (p) {
		NetProtocol *next = p->next;
		delete p;
		p = next;
	}
	prot_list = NULL;
}


/*
 *  Initialization
 */

void EtherInit(void)
{
	int nonblock = 1;
	char str[256];

	// Do nothing if no Ethernet device specified
	const char *name = PrefsFindString("ether");
	if (name == NULL)
		return;

	// Is it Ethertap?
	is_ethertap = (strncmp(name, "tap", 3) == 0);

	// Open sheep_net or ethertap device
	char dev_name[16];
	if (is_ethertap)
		sprintf(dev_name, "/dev/%s", name);
	else
		strcpy(dev_name, "/dev/sheep_net");
	fd = open(dev_name, O_RDWR);
	if (fd < 0) {
		sprintf(str, GetString(STR_NO_SHEEP_NET_DRIVER_WARN), dev_name, strerror(errno));
		WarningAlert(str);
		goto open_error;
	}

	// Attach sheep_net to selected Ethernet card
	if (!is_ethertap && ioctl(fd, SIOCSIFLINK, name) < 0) {
		sprintf(str, GetString(STR_SHEEP_NET_ATTACH_WARN), strerror(errno));
		WarningAlert(str);
		goto open_error;
	}

	// Set nonblocking I/O
	ioctl(fd, FIONBIO, &nonblock);

	// Get Ethernet address
	if (is_ethertap) {
		pid_t p = getpid();	// If configured for multicast, ethertap requires that the lower 32 bit of the Ethernet address are our PID
		ether_addr[0] = 0xfe;
		ether_addr[1] = 0xfd;
		ether_addr[2] = p >> 24;
		ether_addr[3] = p >> 16;
		ether_addr[4] = p >> 8;
		ether_addr[5] = p;
	} else
		ioctl(fd, SIOCGIFADDR, ether_addr);
	D(bug("Ethernet address %02x %02x %02x %02x %02x %02x\n", ether_addr[0], ether_addr[1], ether_addr[2], ether_addr[3], ether_addr[4], ether_addr[5]));

	// Start packet reception thread
	if (sem_init(&int_ack, 0, 0) < 0) {
		printf("WARNING: Cannot init semaphore");
		goto open_error;
	}
	pthread_attr_init(&ether_thread_attr);
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING)
	if (geteuid() == 0) {
		pthread_attr_setinheritsched(&ether_thread_attr, PTHREAD_EXPLICIT_SCHED);
		pthread_attr_setschedpolicy(&ether_thread_attr, SCHED_FIFO);
		struct sched_param fifo_param;
		fifo_param.sched_priority = (sched_get_priority_min(SCHED_FIFO) + sched_get_priority_max(SCHED_FIFO)) / 2 + 1;
		pthread_attr_setschedparam(&ether_thread_attr, &fifo_param);
	}
#endif
	thread_active = (pthread_create(&ether_thread, &ether_thread_attr, receive_func, NULL) == 0);
	if (!thread_active) {
		printf("WARNING: Cannot start Ethernet thread");
		goto open_error;
	}

	// Everything OK
	net_open = true;
	return;

open_error:
	if (thread_active) {
		pthread_cancel(ether_thread);
		pthread_join(ether_thread, NULL);
		sem_destroy(&int_ack);
		thread_active = false;
	}
	if (fd > 0) {
		close(fd);
		fd = -1;
	}
}


/*
 *  Deinitialization
 */

void EtherExit(void)
{
	// Stop reception thread
	if (thread_active) {
		pthread_cancel(ether_thread);
		pthread_join(ether_thread, NULL);
		sem_destroy(&int_ack);
		thread_active = false;
	}

	// Close sheep_net device
	if (fd > 0)
		close(fd);

	// Remove all protocols
	remove_all_protocols();
}


/*
 *  Reset
 */

void EtherReset(void)
{
	remove_all_protocols();
}


/*
 *  Add multicast address
 */

int16 ether_add_multicast(uint32 pb)
{
	if (ioctl(fd, SIOCADDMULTI, Mac2HostAddr(pb + eMultiAddr)) < 0) {
		D(bug("WARNING: Couldn't enable multicast address\n"));
		if (is_ethertap)
			return noErr;
		else
			return eMultiErr;
	} else
		return noErr;
}


/*
 *  Delete multicast address
 */

int16 ether_del_multicast(uint32 pb)
{
	if (ioctl(fd, SIOCDELMULTI, Mac2HostAddr(pb + eMultiAddr)) < 0) {
		D(bug("WARNING: Couldn't disable multicast address\n"));
		return eMultiErr;
	} else
		return noErr;
}


/*
 *  Attach protocol handler
 */

int16 ether_attach_ph(uint16 type, uint32 handler)
{
	// Already attached?
	NetProtocol *p = find_protocol(type);
	if (p != NULL)
		return lapProtErr;
	else {
		// No, create and attach
		p = new NetProtocol;
		p->next = prot_list;
		p->type = type;
		p->handler = handler;
		prot_list = p;
		return noErr;
	}
}


/*
 *  Detach protocol handler
 */

int16 ether_detach_ph(uint16 type)
{
	NetProtocol *p = find_protocol(type);
	if (p != NULL) {
		NetProtocol *q = prot_list;
		if (p == q) {
			prot_list = p->next;
			delete p;
			return noErr;
		}
		while (q) {
			if (q->next == p) {
				q->next = p->next;
				delete p;
				return noErr;
			}
			q = q->next;
		}
		return lapProtErr;
	} else
		return lapProtErr;
}


/*
 *  Transmit raw ethernet packet
 */

int16 ether_write(uint32 wds)
{
	// Set source address
	uint32 hdr = ReadMacInt32(wds + 2);
	Host2Mac_memcpy(hdr + 6, ether_addr, 6);

	// Copy packet to buffer
	uint8 packet[1516], *p = packet;
	int len = 0;
	if (is_ethertap) {
		*p++ = 0;	// Ethertap discards the first 2 bytes
		*p++ = 0;
		len += 2;
	}
	for (;;) {
		int w = ReadMacInt16(wds);
		if (w == 0)
			break;
		Mac2Host_memcpy(p, ReadMacInt32(wds + 2), w);
		len += w;
		p += w;
		wds += 6;
	}

#if MONITOR
	bug("Sending Ethernet packet:\n");
	for (int i=0; i<len; i++) {
		bug("%02x ", packet[i]);
	}
	bug("\n");
#endif

	// Transmit packet
	if (write(fd, packet, len) < 0) {
		D(bug("WARNING: Couldn't transmit packet\n"));
		return excessCollsns;
	} else
		return noErr;
}


/*
 *  Packet reception thread
 */

static void *receive_func(void *arg)
{
	for (;;) {

		// Wait for packets to arrive
		struct pollfd pf = {fd, POLLIN, 0};
		int res = poll(&pf, 1, -1);
		if (res <= 0)
			break;

		// Trigger Ethernet interrupt
		D(bug(" packet received, triggering Ethernet interrupt\n"));
		SetInterruptFlag(INTFLAG_ETHER);
		TriggerInterrupt();

		// Wait for interrupt acknowledge by EtherInterrupt()
		sem_wait(&int_ack);
	}
	return NULL;
}


/*
 *  Ethernet interrupt - activate deferred tasks to call IODone or protocol handlers
 */

void EtherInterrupt(void)
{
	D(bug("EtherIRQ\n"));

	// Call protocol handler for received packets
	uint8 packet[1516];
	for (;;) {

		// Read packet from sheep_net device
		ssize_t length = read(fd, packet, is_ethertap ? 1516 : 1514);
		if (length < 14)
			break;

#if MONITOR
		bug("Receiving Ethernet packet:\n");
		for (int i=0; i<length; i++) {
			bug("%02x ", packet[i]);
		}
		bug("\n");
#endif

		// Pointer to packet data (Ethernet header)
		uint8 *p = packet;
		if (is_ethertap) {
			p += 2;			// Ethertap has two random bytes before the packet
			length -= 2;
		}

		// Get packet type
		uint16 type = ntohs(*(uint16 *)(p + 12));

		// Look for protocol
		NetProtocol *prot = find_protocol(type);
		if (prot == NULL)
			continue;

		// No default handler
		if (prot->handler == 0)
			continue;

		// Copy header to RHA
		Host2Mac_memcpy(ether_data + ed_RHA, p, 14);
		D(bug(" header %08lx%04lx %08lx%04lx %04lx\n", ReadMacInt32(ether_data + ed_RHA), ReadMacInt16(ether_data + ed_RHA + 4), ReadMacInt32(ether_data + ed_RHA + 6), ReadMacInt16(ether_data + ed_RHA + 10), ReadMacInt16(ether_data + ed_RHA + 12)));

		// Call protocol handler
		M68kRegisters r;
		r.d[0] = type;									// Packet type
		r.d[1] = length - 14;							// Remaining packet length (without header, for ReadPacket)
		r.a[0] = (uint32)p + 14;						// Pointer to packet (host address, for ReadPacket)
		r.a[3] = ether_data + ed_RHA + 14;				// Pointer behind header in RHA
		r.a[4] = ether_data + ed_ReadPacket;			// Pointer to ReadPacket/ReadRest routines
		D(bug(" calling protocol handler %08lx, type %08lx, length %08lx, data %08lx, rha %08lx, read_packet %08lx\n", prot->handler, r.d[0], r.d[1], r.a[0], r.a[3], r.a[4]));
		Execute68k(prot->handler, &r);
	}

	// Acknowledge interrupt to reception thread
	D(bug(" EtherIRQ done\n"));
	sem_post(&int_ack);
}
