/*
 *  ether_unix.cpp - Ethernet device driver, Unix specific stuff (Linux and FreeBSD)
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

#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <stdio.h>
#include <map>

#if defined(__FreeBSD__) || defined(sgi) || (defined(__APPLE__) && defined(__MACH__))
#include <net/if.h>
#endif

#if defined(HAVE_LINUX_IF_H) && defined(HAVE_LINUX_IF_TUN_H)
#include <linux/if.h>
#include <linux/if_tun.h>
#endif

#if defined(HAVE_NET_IF_H) && defined(HAVE_NET_IF_TUN_H)
#include <net/if.h>
#include <net/if_tun.h>
#endif

#ifdef HAVE_SLIRP
#include "libslirp.h"
#endif

#include "cpu_emulation.h"
#include "main.h"
#include "macos_util.h"
#include "prefs.h"
#include "user_strings.h"
#include "ether.h"
#include "ether_defs.h"

#ifndef NO_STD_NAMESPACE
using std::map;
#endif

#define DEBUG 0
#include "debug.h"

#define MONITOR 0


// Ethernet device types
enum {
	NET_IF_SHEEPNET,
	NET_IF_ETHERTAP,
	NET_IF_TUNTAP,
	NET_IF_SLIRP
};

// Constants
static const char ETHERCONFIG_FILE_NAME[] = DATADIR "/tunconfig";

// Global variables
static int fd = -1;							// fd of sheep_net device
static pthread_t ether_thread;				// Packet reception thread
static pthread_attr_t ether_thread_attr;	// Packet reception thread attributes
static bool thread_active = false;			// Flag: Packet reception thread installed
static sem_t int_ack;						// Interrupt acknowledge semaphore
static bool udp_tunnel;						// Flag: UDP tunnelling active, fd is the socket descriptor
static int net_if_type = -1;				// Ethernet device type
static char *net_if_name = NULL;			// TUN/TAP device name
static const char *net_if_script = NULL;	// Network config script
static pthread_t slirp_thread;				// Slirp reception thread
static bool slirp_thread_active = false;	// Flag: Slirp reception threadinstalled
static int slirp_output_fd = -1;			// fd of slirp output pipe

// Attached network protocols, maps protocol type to MacOS handler address
static map<uint16, uint32> net_protocols;

// Prototypes
static void *receive_func(void *arg);
static void *slirp_receive_func(void *arg);
static int poll_fd(int fd);


/*
 *  Start packet reception thread
 */

static bool start_thread(void)
{
	if (sem_init(&int_ack, 0, 0) < 0) {
		printf("WARNING: Cannot init semaphore");
		return false;
	}

	Set_pthread_attr(&ether_thread_attr, 1);
	thread_active = (pthread_create(&ether_thread, &ether_thread_attr, receive_func, NULL) == 0);
	if (!thread_active) {
		printf("WARNING: Cannot start Ethernet thread");
		return false;
	}

#ifdef HAVE_SLIRP
	if (net_if_type == NET_IF_SLIRP) {
		slirp_thread_active = (pthread_create(&slirp_thread, NULL, slirp_receive_func, NULL) == 0);
		if (!slirp_thread_active) {
			printf("WARNING: Cannot start slirp reception thread\n");
			return false;
		}
	}
#endif

	return true;
}


/*
 *  Stop packet reception thread
 */

static void stop_thread(void)
{
#ifdef HAVE_SLIRP
	if (slirp_thread_active) {
		pthread_cancel(slirp_thread);
		pthread_join(slirp_thread, NULL);
		slirp_thread_active = false;
	}
#endif

	if (thread_active) {
		pthread_cancel(ether_thread);
		pthread_join(ether_thread, NULL);
		sem_destroy(&int_ack);
		thread_active = false;
	}
}


/*
 *  Execute network script up|down
 */

static bool execute_network_script(const char *action)
{
	if (net_if_script == NULL || net_if_name == NULL)
		return false;

	int pid = fork();
	if (pid >= 0) {
		if (pid == 0) {
			char *args[4];
			args[0] = (char *)net_if_script;
			args[1] = net_if_name;
			args[2] = (char *)action;
			args[3] = NULL;
			execv(net_if_script, args);
			exit(1);
		}
		int status;
		while (waitpid(pid, &status, 0) != pid);
		return WIFEXITED(status) && WEXITSTATUS(status) == 0;
	}

	return false;
}


/*
 *  Initialization
 */

bool ether_init(void)
{
	int nonblock = 1;
	char str[256];

	// Do nothing if no Ethernet device specified
	const char *name = PrefsFindString("ether");
	if (name == NULL)
		return false;

	// Determine Ethernet device type
	net_if_type = -1;
	if (strncmp(name, "tap", 3) == 0)
		net_if_type = NET_IF_ETHERTAP;
#if ENABLE_TUNTAP
	else if (strcmp(name, "tun") == 0)
		net_if_type = NET_IF_TUNTAP;
#endif
#ifdef HAVE_SLIRP
	else if (strcmp(name, "slirp") == 0)
		net_if_type = NET_IF_SLIRP;
#endif
	else
		net_if_type = NET_IF_SHEEPNET;

#ifdef HAVE_SLIRP
	// Initialize slirp library
	if (net_if_type == NET_IF_SLIRP) {
		slirp_init();

		// Open slirp output pipe
		int fds[2];
		if (pipe(fds) < 0)
			return false;
		fd = fds[0];
		slirp_output_fd = fds[1];
	}
#endif

	// Open sheep_net or ethertap or TUN/TAP device
	char dev_name[16];
	switch (net_if_type) {
	case NET_IF_ETHERTAP:
		sprintf(dev_name, "/dev/%s", name);
		break;
	case NET_IF_TUNTAP:
		strcpy(dev_name, "/dev/net/tun");
		break;
	case NET_IF_SHEEPNET:
		strcpy(dev_name, "/dev/sheep_net");
		break;
	}
	if (net_if_type != NET_IF_SLIRP) {
		fd = open(dev_name, O_RDWR);
		if (fd < 0) {
			sprintf(str, GetString(STR_NO_SHEEP_NET_DRIVER_WARN), dev_name, strerror(errno));
			WarningAlert(str);
			goto open_error;
		}
	}

#if ENABLE_TUNTAP
	// Open TUN/TAP interface
	if (net_if_type == NET_IF_TUNTAP) {
		struct ifreq ifr;
		memset(&ifr, 0, sizeof(ifr));
		ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
		strcpy(ifr.ifr_name, "tun%d");
		if (ioctl(fd, TUNSETIFF, (void *) &ifr) != 0) {
			sprintf(str, GetString(STR_SHEEP_NET_ATTACH_WARN), strerror(errno));
			WarningAlert(str);
			goto open_error;
		}

		// Get network config script file path
		net_if_script = PrefsFindString("etherconfig");
		if (net_if_script == NULL)
			net_if_script = ETHERCONFIG_FILE_NAME;

		// Start network script up
		if (net_if_script == NULL) {
			sprintf(str, GetString(STR_TUN_TAP_CONFIG_WARN), "script not found");
			WarningAlert(str);
			goto open_error;
		}
		net_if_name = strdup(ifr.ifr_name);
		if (!execute_network_script("up")) {
			sprintf(str, GetString(STR_TUN_TAP_CONFIG_WARN), "script execute error");
			WarningAlert(str);
			goto open_error;
		}
		D(bug("Connected to host network interface: %s\n", net_if_name));
	}
#endif

#if defined(__linux__)
	// Attach sheep_net to selected Ethernet card
	if (net_if_type == NET_IF_SHEEPNET && ioctl(fd, SIOCSIFLINK, name) < 0) {
		sprintf(str, GetString(STR_SHEEP_NET_ATTACH_WARN), strerror(errno));
		WarningAlert(str);
		goto open_error;
	}
#endif

	// Set nonblocking I/O
	ioctl(fd, FIONBIO, &nonblock);

	// Get Ethernet address
	if (net_if_type == NET_IF_ETHERTAP) {
		pid_t p = getpid();	// If configured for multicast, ethertap requires that the lower 32 bit of the Ethernet address are our PID
		ether_addr[0] = 0xfe;
		ether_addr[1] = 0xfd;
		ether_addr[2] = p >> 24;
		ether_addr[3] = p >> 16;
		ether_addr[4] = p >> 8;
		ether_addr[5] = p;
#ifdef HAVE_SLIRP
	} else if (net_if_type == NET_IF_SLIRP) {
		ether_addr[0] = 0x52;
		ether_addr[1] = 0x54;
		ether_addr[2] = 0x00;
		ether_addr[3] = 0x12;
		ether_addr[4] = 0x34;
		ether_addr[5] = 0x56;
#endif
	} else
		ioctl(fd, SIOCGIFADDR, ether_addr);
	D(bug("Ethernet address %02x %02x %02x %02x %02x %02x\n", ether_addr[0], ether_addr[1], ether_addr[2], ether_addr[3], ether_addr[4], ether_addr[5]));

	// Start packet reception thread
	if (!start_thread())
		goto open_error;

	// Everything OK
	return true;

open_error:
	stop_thread();

	if (fd > 0) {
		close(fd);
		fd = -1;
	}
	if (slirp_output_fd >= 0) {
		close(slirp_output_fd);
		slirp_output_fd = -1;
	}
	return false;
}


/*
 *  Deinitialization
 */

void ether_exit(void)
{
	// Stop reception thread
	if (thread_active) {
		pthread_cancel(ether_thread);
		pthread_join(ether_thread, NULL);
		sem_destroy(&int_ack);
		thread_active = false;
	}

	// Shut down TUN/TAP interface
	if (net_if_type == NET_IF_TUNTAP)
		execute_network_script("down");

	// Free TUN/TAP device name
	if (net_if_name)
		free(net_if_name);

	// Close sheep_net device
	if (fd > 0)
		close(fd);

	// Close slirp output buffer
	if (slirp_output_fd > 0)
		close(slirp_output_fd);
}


/*
 *  Reset
 */

void ether_reset(void)
{
	net_protocols.clear();
}


/*
 *  Add multicast address
 */

int16 ether_add_multicast(uint32 pb)
{
	switch (net_if_type) {
	case NET_IF_ETHERTAP:
	case NET_IF_SHEEPNET:
		if (ioctl(fd, SIOCADDMULTI, Mac2HostAddr(pb + eMultiAddr)) < 0) {
			D(bug("WARNING: Couldn't enable multicast address\n"));
			if (net_if_type == NET_IF_ETHERTAP)
				return noErr;
			else
				return eMultiErr;
		}
	default:
		return noErr;
	}
}


/*
 *  Delete multicast address
 */

int16 ether_del_multicast(uint32 pb)
{
	switch (net_if_type) {
	case NET_IF_ETHERTAP:
	case NET_IF_SHEEPNET:
		if (ioctl(fd, SIOCDELMULTI, Mac2HostAddr(pb + eMultiAddr)) < 0) {
			D(bug("WARNING: Couldn't disable multicast address\n"));
			return eMultiErr;
		}
	default:
		return noErr;
	}
}


/*
 *  Attach protocol handler
 */

int16 ether_attach_ph(uint16 type, uint32 handler)
{
	if (net_protocols.find(type) != net_protocols.end())
		return lapProtErr;
	net_protocols[type] = handler;
	return noErr;
}


/*
 *  Detach protocol handler
 */

int16 ether_detach_ph(uint16 type)
{
	if (net_protocols.erase(type) == 0)
		return lapProtErr;
	return noErr;
}


/*
 *  Transmit raw ethernet packet
 */

int16 ether_write(uint32 wds)
{
	// Copy packet to buffer
	uint8 packet[1516], *p = packet;
	int len = 0;
#if defined(__linux__)
	if (net_if_type == NET_IF_ETHERTAP) {
		*p++ = 0;	// Linux ethertap discards the first 2 bytes
		*p++ = 0;
		len += 2;
	}
#endif
	len += ether_wds_to_buffer(wds, p);

#if MONITOR
	bug("Sending Ethernet packet:\n");
	for (int i=0; i<len; i++) {
		bug("%02x ", packet[i]);
	}
	bug("\n");
#endif

	// Transmit packet
#ifdef HAVE_SLIRP
	if (net_if_type == NET_IF_SLIRP) {
		slirp_input(packet, len);
		return noErr;
	} else
#endif
	if (write(fd, packet, len) < 0) {
		D(bug("WARNING: Couldn't transmit packet\n"));
		return excessCollsns;
	} else
		return noErr;
}


/*
 *  Start UDP packet reception thread
 */

bool ether_start_udp_thread(int socket_fd)
{
	fd = socket_fd;
	udp_tunnel = true;
	return start_thread();
}


/*
 *  Stop UDP packet reception thread
 */

void ether_stop_udp_thread(void)
{
	stop_thread();
	fd = -1;
}


/*
 *  SLIRP output buffer glue
 */

#ifdef HAVE_SLIRP
int slirp_can_output(void)
{
	return 1;
}

void slirp_output(const uint8 *packet, int len)
{
	write(slirp_output_fd, packet, len);
}

void *slirp_receive_func(void *arg)
{
	for (;;) {
		// Wait for packets to arrive
		fd_set rfds, wfds, xfds;
		int nfds;
		struct timeval tv;

		nfds = -1;
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_ZERO(&xfds);
		slirp_select_fill(&nfds, &rfds, &wfds, &xfds);
		tv.tv_sec = 0;
		tv.tv_usec = 16667;
		if (select(nfds + 1, &rfds, &wfds, &xfds, &tv) >= 0)
			slirp_select_poll(&rfds, &wfds, &xfds);
	}
	return NULL;
}
#else
int slirp_can_output(void)
{
	return 0;
}

void slirp_output(const uint8 *packet, int len)
{
}
#endif


/*
 *  Wait for data to arrive
 */

static inline int poll_fd(int fd)
{
#ifdef HAVE_POLL
	struct pollfd pf = {fd, POLLIN, 0};
	return poll(&pf, 1, -1);
#else
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	return select(1, &rfds, NULL, NULL, NULL);
#endif
}


/*
 *  Packet reception thread
 */

static void *receive_func(void *arg)
{
	for (;;) {

		// Wait for packets to arrive
		int res = poll_fd(fd);
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
	EthernetPacket ether_packet;
	uint32 packet = ether_packet.addr();
	ssize_t length;
	for (;;) {

		if (udp_tunnel) {

			// Read packet from socket
			struct sockaddr_in from;
			socklen_t from_len = sizeof(from);
			length = recvfrom(fd, Mac2HostAddr(packet), 1514, 0, (struct sockaddr *)&from, &from_len);
			if (length < 14)
				break;
			ether_udp_read(packet, length, &from);

		} else {

			// Read packet from sheep_net device
#if defined(__linux__)
			length = read(fd, Mac2HostAddr(packet), net_if_type == NET_IF_ETHERTAP ? 1516 : 1514);
#else
			length = read(fd, Mac2HostAddr(packet), 1514);
#endif
			if (length < 14)
				break;

#if MONITOR
			bug("Receiving Ethernet packet:\n");
			for (int i=0; i<length; i++) {
				bug("%02x ", ReadMacInt8(packet + i));
			}
			bug("\n");
#endif

			// Pointer to packet data (Ethernet header)
			uint32 p = packet;
#if defined(__linux__)
			if (net_if_type == NET_IF_ETHERTAP) {
				p += 2;			// Linux ethertap has two random bytes before the packet
				length -= 2;
			}
#endif

			// Get packet type
			uint16 type = ReadMacInt16(p + 12);

			// Look for protocol
			uint16 search_type = (type <= 1500 ? 0 : type);
			if (net_protocols.find(search_type) == net_protocols.end())
				continue;
			uint32 handler = net_protocols[search_type];

			// No default handler
			if (handler == 0)
				continue;

			// Copy header to RHA
			Mac2Mac_memcpy(ether_data + ed_RHA, p, 14);
			D(bug(" header %08x%04x %08x%04x %04x\n", ReadMacInt32(ether_data + ed_RHA), ReadMacInt16(ether_data + ed_RHA + 4), ReadMacInt32(ether_data + ed_RHA + 6), ReadMacInt16(ether_data + ed_RHA + 10), ReadMacInt16(ether_data + ed_RHA + 12)));

			// Call protocol handler
			M68kRegisters r;
			r.d[0] = type;									// Packet type
			r.d[1] = length - 14;							// Remaining packet length (without header, for ReadPacket)
			r.a[0] = p + 14;								// Pointer to packet (Mac address, for ReadPacket)
			r.a[3] = ether_data + ed_RHA + 14;				// Pointer behind header in RHA
			r.a[4] = ether_data + ed_ReadPacket;			// Pointer to ReadPacket/ReadRest routines
			D(bug(" calling protocol handler %08x, type %08x, length %08x, data %08x, rha %08x, read_packet %08x\n", handler, r.d[0], r.d[1], r.a[0], r.a[3], r.a[4]));
			Execute68k(handler, &r);
		}
	}

	// Acknowledge interrupt to reception thread
	D(bug(" EtherIRQ done\n"));
	sem_post(&int_ack);
}
