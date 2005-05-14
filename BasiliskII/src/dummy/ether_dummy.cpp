/*
 *  ether_dummy.cpp - Ethernet device driver, dummy implementation
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
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

#if SUPPORTS_UDP_TUNNEL
#include <netinet/in.h>
#include <sys/socket.h>
#endif

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


// Global variables
#if SUPPORTS_UDP_TUNNEL
static int fd = -1;						// UDP tunnel socket fd
static bool udp_tunnel_active = false;
#endif


/*
 *  Initialization
 */

bool ether_init(void)
{
	return true;
}


/*
 *  Deinitialization
 */

void ether_exit(void)
{
}


/*
 *  Reset
 */

void ether_reset(void)
{
}


/*
 *  Add multicast address
 */

int16 ether_add_multicast(uint32 pb)
{
	return noErr;
}


/*
 *  Delete multicast address
 */

int16 ether_del_multicast(uint32 pb)
{
	return noErr;
}


/*
 *  Attach protocol handler
 */

int16 ether_attach_ph(uint16 type, uint32 handler)
{
	return noErr;
}


/*
 *  Detach protocol handler
 */

int16 ether_detach_ph(uint16 type)
{
	return noErr;
}


/*
 *  Transmit raw ethernet packet
 */

int16 ether_write(uint32 wds)
{
	// Set source address
	uint32 hdr = ReadMacInt32(wds + 2);
	memcpy(Mac2HostAddr(hdr + 6), ether_addr, 6);
	return noErr;
}


/*
 *  Start UDP packet reception thread
 */

bool ether_start_udp_thread(int socket_fd)
{
#if SUPPORTS_UDP_TUNNEL
	fd = socket_fd;
	udp_tunnel_active = true;
	return true;
#else
	return false;
#endif
}


/*
 *  Stop UDP packet reception thread
 */

void ether_stop_udp_thread(void)
{
#if SUPPORTS_UDP_TUNNEL
	udp_tunnel_active = false;
#endif
}


/*
 *  Ethernet interrupt - activate deferred tasks to call IODone or protocol handlers
 */

void EtherInterrupt(void)
{
#if SUPPORTS_UDP_TUNNEL
	if (udp_tunnel_active) {
		EthernetPacket ether_packet;
		uint32 packet = ether_packet.addr();
		ssize_t length;

		// Read packets from socket and hand to ether_udp_read() for processing
		while (true) {
			struct sockaddr_in from;
			socklen_t from_len = sizeof(from);
			length = recvfrom(fd, Mac2HostAddr(packet), 1514, 0, (struct sockaddr *)&from, &from_len);
			if (length < 14)
				break;
			ether_udp_read(packet, length, &from);
		}
	}
#endif
}
