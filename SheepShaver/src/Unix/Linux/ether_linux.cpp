/*
 *  ether_linux.cpp - SheepShaver Ethernet Device Driver (DLPI), Linux specific stuff
 *
 *  SheepShaver (C) 1997-2002 Marc Hellwig and Christian Bauer
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

#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <stdio.h>
#include <string.h>

#include "sysdeps.h"
#include "main.h"
#include "macos_util.h"
#include "prefs.h"
#include "user_strings.h"
#include "ether.h"
#include "ether_defs.h"

#define DEBUG 0
#include "debug.h"

#define STATISTICS 0
#define MONITOR 0


// Global variables
static int fd = -1;					// fd of sheep_net device

static pthread_t ether_thread;		// Packet reception thread
static bool thread_active = false;	// Flag: Packet reception thread installed

static sem_t int_ack;				// Interrupt acknowledge semaphore
static uint8 ether_addr[6];			// Our Ethernet address

static bool net_open = false;		// Flag: initialization succeeded, network device open
static bool is_ethertap = false;	// Flag: Ethernet device is ethertap


// Prototypes
static void *receive_func(void *arg);


/*
 *  Initialize ethernet
 */

void EtherInit(void)
{
	int nonblock = 1;
	char str[256];

	// Do nothing if the user disabled the network
	if (PrefsFindBool("nonet"))
		return;

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

	// Attach to selected Ethernet card
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
		WarningAlert("WARNING: Cannot init semaphore");
		goto open_error;
	}
	thread_active = (pthread_create(&ether_thread, NULL, receive_func, NULL) == 0);
	if (!thread_active) {
		WarningAlert("WARNING: Cannot start Ethernet thread");
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
 *  Exit ethernet
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

#if STATISTICS
	// Show statistics
	printf("%ld messages put on write queue\n", num_wput);
	printf("%ld error acks\n", num_error_acks);
	printf("%ld packets transmitted (%ld raw, %ld normal)\n", num_tx_packets, num_tx_raw_packets, num_tx_normal_packets);
	printf("%ld tx packets dropped because buffer full\n", num_tx_buffer_full);
	printf("%ld packets received\n", num_rx_packets);
	printf("%ld packets passed upstream (%ld Fast Path, %ld normal)\n", num_rx_fastpath + num_unitdata_ind, num_rx_fastpath, num_unitdata_ind);
	printf("EtherIRQ called %ld times\n", num_ether_irq);
	printf("%ld rx packets dropped due to low memory\n", num_rx_no_mem);
	printf("%ld rx packets dropped because no stream found\n", num_rx_dropped);
	printf("%ld rx packets dropped because stream not ready\n", num_rx_stream_not_ready);
	printf("%ld rx packets dropped because no memory for unitdata_ind\n", num_rx_no_unitdata_mem);
#endif
}


/*
 *  Get ethernet hardware address
 */

void AO_get_ethernet_address(uint8 *addr)
{
	if (net_open) {
		OTCopy48BitAddress(ether_addr, addr);
	} else {
		addr[0] = 0x12;
		addr[1] = 0x34;
		addr[2] = 0x56;
		addr[3] = 0x78;
		addr[4] = 0x9a;
		addr[5] = 0xbc;
	}
	D(bug("AO_get_ethernet_address: got address %02x%02x%02x%02x%02x%02x\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]));
}


/*
 *  Enable multicast address
 */

void AO_enable_multicast(uint8 *addr)
{
	D(bug("AO_enable_multicast\n"));
	if (net_open) {
		if (ioctl(fd, SIOCADDMULTI, addr) < 0) {
			D(bug("WARNING: couldn't enable multicast address\n"));
		}
	}
}


/*
 *  Disable multicast address
 */

void AO_disable_multicast(uint8 *addr)
{
	D(bug("AO_disable_multicast\n"));
	if (net_open) {
		if (ioctl(fd, SIOCDELMULTI, addr) < 0) {
			D(bug("WARNING: couldn't disable multicast address\n"));
		}
	}
}


/*
 *  Transmit one packet
 */

void AO_transmit_packet(mblk_t *mp)
{
	D(bug("AO_transmit_packet\n"));
	if (net_open) {

		// Copy packet to buffer
		uint8 packet[1516], *p = packet;
		int len = 0;
		if (is_ethertap) {
			*p++ = 0;	// Ethertap discards the first 2 bytes
			*p++ = 0;
			len += 2;
		}
		while (mp) {
			uint32 size = mp->b_wptr - mp->b_rptr;
			memcpy(p, mp->b_rptr, size);
			len += size;
			p += size;
			mp = mp->b_cont;
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
			D(bug("WARNING: couldn't transmit packet\n"));
			num_tx_buffer_full++;
		} else
			num_tx_packets++;
	}
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

		if (ether_driver_opened) {
			// Trigger Ethernet interrupt
			D(bug(" packet received, triggering Ethernet interrupt\n"));
			SetInterruptFlag(INTFLAG_ETHER);
			TriggerInterrupt();

			// Wait for interrupt acknowledge by EtherInterrupt()
			sem_wait(&int_ack);
		} else
			usleep(20000);
	}
	return NULL;
}


/*
 *  Ethernet interrupt
 */

void EtherIRQ(void)
{
	D(bug("EtherIRQ\n"));
	num_ether_irq++;
	OTEnterInterrupt();

	// Send received packets to OpenTransport
	uint8 packet[1516];
	for (;;) {

		if (is_ethertap) {

			// Read packet from ethertap device
			ssize_t size = read(fd, packet, 1516);
			if (size < 14)
				break;

#if MONITOR
			bug("Receiving Ethernet packet:\n");
			for (int i=0; i<size; i++) {
				bug("%02x ", packet[i]);
			}
			bug("\n");
#endif

			// Pointer to packet data (Ethernet header)
			uint8 *p = packet + 2;	// Ethertap has two random bytes before the packet
			size -= 2;

			// Wrap packet in message block
			num_rx_packets++;
			mblk_t *mp;
			if ((mp = allocb(size, 0)) != NULL) {
				D(bug(" packet data at %p\n", mp->b_rptr));
				memcpy(mp->b_rptr, p, size);
				mp->b_wptr += size;
				ether_packet_received(mp);
			} else {
				D(bug("WARNING: Cannot allocate mblk for received packet\n"));
				num_rx_no_mem++;
			}

		} else {

			// Get size of first packet
			int size = 0;
			if (ioctl(fd, FIONREAD, &size) < 0 || size == 0)
				break;

			// Discard packets which are too short
			if (size < 14) {
				uint8 dummy[14];
				read(fd, dummy, size);
				continue;
			}

			// Truncate packets which are too long
			if (size > 1514)
				size = 1514;

			// Read packet and wrap it in message block
			num_rx_packets++;
			mblk_t *mp;
			if ((mp = allocb(size, 0)) != NULL) {
				D(bug(" packet data at %p\n", mp->b_rptr));
				read(fd, mp->b_rptr, 1514);
#if MONITOR
				bug("Receiving Ethernet packet:\n");
				for (int i=0; i<size; i++) {
					bug("%02x ", ((uint8 *)mp->b_rptr)[i]);
				}
				bug("\n");
#endif
				mp->b_wptr += size;
				ether_packet_received(mp);
			} else {
				D(bug("WARNING: Cannot allocate mblk for received packet\n"));
				num_rx_no_mem++;
			}
		}
	}
	OTLeaveInterrupt();

	// Acknowledge interrupt to reception thread
	D(bug(" EtherIRQ done\n"));
	sem_post(&int_ack);
}
