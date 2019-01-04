/*
 *  ether_windows.cpp - Ethernet device driver
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

#include "sysdeps.h"

#include <process.h>
#include <windowsx.h>
#include <winioctl.h>
#include <ctype.h>

#include "cpu_emulation.h"
#include "main.h"
#include "macos_util.h"
#include "prefs.h"
#include "user_strings.h"
#include "ether.h"
#include "ether_defs.h"
#include "b2ether/multiopt.h"
#include "b2ether/inc/b2ether_hl.h"
#include "ether_windows.h"
#include "router/router.h"
#include "util_windows.h"
#include "libslirp.h"

// Define to let the slirp library determine the right timeout for select()
#define USE_SLIRP_TIMEOUT 1


#define DEBUG 0
#define MONITOR 0

#if DEBUG
#pragma optimize("",off)
#endif

#include "debug.h"


// Ethernet device types
enum {
	NET_IF_B2ETHER,
	NET_IF_ROUTER,
	NET_IF_SLIRP,
	NET_IF_TAP,
	NET_IF_FAKE,
};

// TAP-Win32 constants
#define TAP_VERSION_MIN_MAJOR 7
#define TAP_VERSION_MIN_MINOR 1

#define TAP_CONTROL_CODE(request, method) \
		CTL_CODE (FILE_DEVICE_UNKNOWN, request, method, FILE_ANY_ACCESS)

#define TAP_IOCTL_GET_MAC				TAP_CONTROL_CODE (1, METHOD_BUFFERED)
#define TAP_IOCTL_GET_VERSION			TAP_CONTROL_CODE (2, METHOD_BUFFERED)
#define TAP_IOCTL_GET_MTU				TAP_CONTROL_CODE (3, METHOD_BUFFERED)
#define TAP_IOCTL_GET_INFO				TAP_CONTROL_CODE (4, METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_POINT_TO_POINT	TAP_CONTROL_CODE (5, METHOD_BUFFERED)
#define TAP_IOCTL_SET_MEDIA_STATUS		TAP_CONTROL_CODE (6, METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_DHCP_MASQ		TAP_CONTROL_CODE (7, METHOD_BUFFERED)
#define TAP_IOCTL_GET_LOG_LINE			TAP_CONTROL_CODE (8, METHOD_BUFFERED)
#define TAP_IOCTL_CONFIG_DHCP_SET_OPT	TAP_CONTROL_CODE (9, METHOD_BUFFERED)

#define OLD_TAP_CONTROL_CODE(request, method) \
		CTL_CODE (FILE_DEVICE_PHYSICAL_NETCARD | 8000, request, method, FILE_ANY_ACCESS)

#define OLD_TAP_IOCTL_GET_VERSION		OLD_TAP_CONTROL_CODE (3, METHOD_BUFFERED)

// Options
bool ether_use_permanent = true;
static int16 ether_multi_mode = ETHER_MULTICAST_MAC;

// Global variables
HANDLE ether_th;
unsigned int ether_tid;
HANDLE ether_th1;
HANDLE ether_th2;
static int net_if_type = -1;	// Ethernet device type
#ifdef SHEEPSHAVER
static bool net_open = false;	// Flag: initialization succeeded, network device open
uint8 ether_addr[6];			// Our Ethernet address
#endif

// These are protected by queue_csection
// Controls transfer for read thread to feed thread
static CRITICAL_SECTION queue_csection;
typedef struct _win_queue_t {
	uint8 *buf;
	int sz;
} win_queue_t;
#define MAX_QUEUE_ITEMS 1024
static win_queue_t queue[MAX_QUEUE_ITEMS];
static int queue_head = 0;
static int queue_inx = 0;
static bool wait_request = true;



// Read thread protected packet pool
static CRITICAL_SECTION fetch_csection;
// Some people use pools as large as 64.
#define PACKET_POOL_COUNT 10
static LPPACKET packets[PACKET_POOL_COUNT];
static bool wait_request2 = false;



// Write thread packet queue
static CRITICAL_SECTION send_csection;
static LPPACKET send_queue = 0;


// Write thread free packet pool
static CRITICAL_SECTION wpool_csection;
static LPPACKET write_packet_pool = 0;



// Try to deal with echos. Protected by fetch_csection.
// The code should be moved to the driver. No need to lift
// the echo packets to the application level.
// MAX_ECHO must be a power of two.
#define MAX_ECHO (1<<2)
static int echo_count = 0;
typedef uint8 echo_t[1514];
static echo_t pending_packet[MAX_ECHO];
static int pending_packet_sz[MAX_ECHO];


// List of attached protocols
struct NetProtocol {
	NetProtocol *next;
	uint16 type;
	uint32 handler;
};

static NetProtocol *prot_list = NULL;


static LPADAPTER fd = 0;
static bool thread_active = false;
static bool thread_active_1 = false;
static bool thread_active_2 = false;
static bool thread_active_3 = false;
static HANDLE int_ack = 0;
static HANDLE int_sig = 0;
static HANDLE int_sig2 = 0;
static HANDLE int_send_now = 0;

// Prototypes
static LPADAPTER tap_open_adapter(LPCTSTR dev_name);
static void tap_close_adapter(LPADAPTER fd);
static bool tap_check_version(LPADAPTER fd);
static bool tap_set_status(LPADAPTER fd, ULONG status);
static bool tap_get_mac(LPADAPTER fd, LPBYTE addr);
static bool tap_receive_packet(LPADAPTER fd, LPPACKET lpPacket, BOOLEAN Sync);
static bool tap_send_packet(LPADAPTER fd, LPPACKET lpPacket, BOOLEAN Sync, BOOLEAN recycle);
static unsigned int WINAPI slirp_receive_func(void *arg);
static unsigned int WINAPI ether_thread_feed_int(void *arg);
static unsigned int WINAPI ether_thread_get_packets_nt(void *arg);
static unsigned int WINAPI ether_thread_write_packets(void *arg);
static void init_queue(void);
static void final_queue(void);
static bool allocate_read_packets(void);
static void free_read_packets(void);
static void free_write_packets(void);
static int16 ether_do_add_multicast(uint8 *addr);
static int16 ether_do_del_multicast(uint8 *addr);
static int16 ether_do_write(uint32 arg);
static void ether_do_interrupt(void);


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
 *  Initialization
 */

bool ether_init(void)
{
	TCHAR buf[256];

	// Do nothing if no Ethernet device specified
	const char *name = PrefsFindString("ether");
	if (name == NULL)
		return false;

	ether_multi_mode = PrefsFindInt32("ethermulticastmode");
	ether_use_permanent = PrefsFindBool("etherpermanentaddress");

	// Determine Ethernet device type
	net_if_type = -1;
	if (PrefsFindBool("routerenabled") || strcmp(name, "router") == 0)
		net_if_type = NET_IF_ROUTER;
	else if (strcmp(name, "slirp") == 0)
		net_if_type = NET_IF_SLIRP;
	else if (strcmp(name, "tap") == 0)
		net_if_type = NET_IF_TAP;
	else
		net_if_type = NET_IF_B2ETHER;

	// Initialize NAT-Router
	if (net_if_type == NET_IF_ROUTER) {
		if (!router_init())
			net_if_type = NET_IF_FAKE;
	}

	// Initialize slirp library
	if (net_if_type == NET_IF_SLIRP) {
		if (slirp_init() < 0) {
			WarningAlert(GetString(STR_SLIRP_NO_DNS_FOUND_WARN));
			return false;
		}
	}

	// Open ethernet device
	decltype(tstr(std::declval<const char*>())) dev_name;
	switch (net_if_type) {
	case NET_IF_B2ETHER:
		dev_name = tstr(PrefsFindString("etherguid"));
		if (dev_name == NULL || strcmp(name, "b2ether") != 0)
			dev_name = tstr(name);
		break;
	case NET_IF_TAP:
		dev_name = tstr(PrefsFindString("etherguid"));
		break;
	}
	if (net_if_type == NET_IF_B2ETHER) {
		if (dev_name == NULL) {
			WarningAlert("No ethernet device GUID specified. Ethernet is not available.");
			goto open_error;
		}

		fd = PacketOpenAdapter( dev_name.get(), ether_multi_mode );
		if (!fd) {
			_sntprintf(buf, lengthof(buf), TEXT("Could not open ethernet adapter %s."), dev_name.get());
			WarningAlert(buf);
			goto open_error;
		}

		// Get Ethernet address
		if(!PacketGetMAC(fd,ether_addr,ether_use_permanent)) {
			_sntprintf(buf, lengthof(buf), TEXT("Could not get hardware address of device %s. Ethernet is not available."), dev_name.get());
			WarningAlert(buf);
			goto open_error;
		}
		D(bug("Real ethernet address %02x %02x %02x %02x %02x %02x\n", ether_addr[0], ether_addr[1], ether_addr[2], ether_addr[3], ether_addr[4], ether_addr[5]));

		const char *ether_fake_address;
		ether_fake_address = PrefsFindString("etherfakeaddress");
		if(ether_fake_address && strlen(ether_fake_address) == 12) {
			char sm[10];
			strcpy( sm, "0x00" );
			for( int i=0; i<6; i++ ) {
				sm[2] = ether_fake_address[i*2];
				sm[3] = ether_fake_address[i*2+1];
				ether_addr[i] = (uint8)strtoul(sm,0,0);
			}
			D(bug("Fake ethernet address %02x %02x %02x %02x %02x %02x\n", ether_addr[0], ether_addr[1], ether_addr[2], ether_addr[3], ether_addr[4], ether_addr[5]));
		}
	}
	else if (net_if_type == NET_IF_TAP) {
		if (dev_name == NULL) {
			WarningAlert("No ethernet device GUID specified. Ethernet is not available.");
			goto open_error;
		}

		fd = tap_open_adapter(dev_name.get());
		if (!fd) {
			_sntprintf(buf, lengthof(buf), TEXT("Could not open ethernet adapter %s."), dev_name.get());
			WarningAlert(buf);
			goto open_error;
		}

		if (!tap_check_version(fd)) {
			_sntprintf(buf, lengthof(buf), TEXT("Minimal TAP-Win32 version supported is %d.%d."), TAP_VERSION_MIN_MAJOR, TAP_VERSION_MIN_MINOR);
			WarningAlert(buf);
			goto open_error;
		}

		if (!tap_set_status(fd, true)) {
			WarningAlert("Could not set media status to connected.");
			goto open_error;
		}

		if (!tap_get_mac(fd, ether_addr)) {
			_sntprintf(buf, lengthof(buf), TEXT("Could not get hardware address of device %s. Ethernet is not available."), dev_name.get());
			WarningAlert(buf);
			goto open_error;
		}
		D(bug("Real ethernet address %02x %02x %02x %02x %02x %02x\n", ether_addr[0], ether_addr[1], ether_addr[2], ether_addr[3], ether_addr[4], ether_addr[5]));

		const char *ether_fake_address;
		ether_fake_address = PrefsFindString("etherfakeaddress");
		if (ether_fake_address && strlen(ether_fake_address) == 12) {
			char sm[10];
			strcpy( sm, "0x00" );
			for( int i=0; i<6; i++ ) {
				sm[2] = ether_fake_address[i*2];
				sm[3] = ether_fake_address[i*2+1];
				ether_addr[i] = (uint8)strtoul(sm,0,0);
			}
		}
#if 1
		/*
		  If we bridge the underlying ethernet connection and the TAP
		  device altogether, we have to use a fake address.
		 */
		else {
			ether_addr[0] = 0x52;
			ether_addr[1] = 0x54;
			ether_addr[2] = 0x00;
		}
#endif
		D(bug("Fake ethernet address %02x %02x %02x %02x %02x %02x\n", ether_addr[0], ether_addr[1], ether_addr[2], ether_addr[3], ether_addr[4], ether_addr[5]));
	}
	else if (net_if_type == NET_IF_SLIRP) {
		ether_addr[0] = 0x52;
		ether_addr[1] = 0x54;
		ether_addr[2] = 0x00;
		ether_addr[3] = 0x12;
		ether_addr[4] = 0x34;
		ether_addr[5] = 0x56;
		D(bug("Ethernet address %02x %02x %02x %02x %02x %02x\n", ether_addr[0], ether_addr[1], ether_addr[2], ether_addr[3], ether_addr[4], ether_addr[5]));
	}
	else {
		memcpy( ether_addr, router_mac_addr, 6 );
		D(bug("Fake ethernet address (same as router) %02x %02x %02x %02x %02x %02x\n", ether_addr[0], ether_addr[1], ether_addr[2], ether_addr[3], ether_addr[4], ether_addr[5]));
	}

	// Start packet reception thread
	int_ack = CreateSemaphore( 0, 0, 1, NULL);
	if(!int_ack) {
		WarningAlert("WARNING: Cannot create int_ack semaphore");
		goto open_error;
	}

	// nonsignaled
	int_sig = CreateSemaphore( 0, 0, 1, NULL);
	if(!int_sig) {
		WarningAlert("WARNING: Cannot create int_sig semaphore");
		goto open_error;
	}

	int_sig2 = CreateSemaphore( 0, 0, 1, NULL);
	if(!int_sig2) {
		WarningAlert("WARNING: Cannot create int_sig2 semaphore");
		goto open_error;
	}

	int_send_now = CreateSemaphore( 0, 0, 1, NULL);
	if(!int_send_now) {
		WarningAlert("WARNING: Cannot create int_send_now semaphore");
		goto open_error;
	}

	init_queue();

	if(!allocate_read_packets()) goto open_error;

	// No need to enter wait state if we can avoid it.
	// These all terminate fast.

	InitializeCriticalSectionAndSpinCount( &fetch_csection, 5000 );
	InitializeCriticalSectionAndSpinCount( &queue_csection, 5000 );
	InitializeCriticalSectionAndSpinCount( &send_csection, 5000 );
	InitializeCriticalSectionAndSpinCount( &wpool_csection, 5000 );

	ether_th = (HANDLE)_beginthreadex( 0, 0, ether_thread_feed_int, 0, 0, &ether_tid );
	if (!ether_th) {
		D(bug("Failed to create ethernet thread\n"));
		goto open_error;
	}
	thread_active = true;

	unsigned int dummy;
	unsigned int (WINAPI *receive_func)(void *);
	switch (net_if_type) {
	case NET_IF_SLIRP:
	  receive_func = slirp_receive_func;
	  break;
	default:
	  receive_func = ether_thread_get_packets_nt;
	  break;
	}
	ether_th2 = (HANDLE)_beginthreadex( 0, 0, receive_func, 0, 0, &dummy );
	ether_th1 = (HANDLE)_beginthreadex( 0, 0, ether_thread_write_packets, 0, 0, &dummy );

	// Everything OK
	return true;

 open_error:
	if (thread_active) {
		TerminateThread(ether_th,0);
		ether_th = 0;
		if (int_ack)
			CloseHandle(int_ack);
		int_ack = 0;
		if(int_sig)
			CloseHandle(int_sig);
		int_sig = 0;
		if(int_sig2)
			CloseHandle(int_sig2);
		int_sig2 = 0;
		if(int_send_now)
			CloseHandle(int_send_now);
		int_send_now = 0;
		thread_active = false;
	}
	if (fd) {
		switch (net_if_type) {
		case NET_IF_B2ETHER:
			PacketCloseAdapter(fd);
			break;
		case NET_IF_TAP:
			tap_close_adapter(fd);
			break;
		}
		fd = 0;
	}
	return false;
}


/*
 *  Deinitialization
 */

void ether_exit(void)
{
	D(bug("EtherExit\n"));

	// Stop reception thread
	thread_active = false;

	if(int_ack) ReleaseSemaphore(int_ack,1,NULL);
	if(int_sig) ReleaseSemaphore(int_sig,1,NULL);
	if(int_sig2) ReleaseSemaphore(int_sig2,1,NULL);
	if(int_send_now) ReleaseSemaphore(int_send_now,1,NULL);

	D(bug("CancelIO if needed\n"));
	if (fd && fd->hFile)
		CancelIo(fd->hFile);

	// Wait max 2 secs to shut down pending io. After that, kill them.
	D(bug("Wait delay\n"));
	for( int i=0; i<10; i++ ) {
		if(!thread_active_1 && !thread_active_2 && !thread_active_3) break;
		Sleep(200);
	}

	if(thread_active_1) {
		D(bug("Ether killing ether_th1\n"));
		if(ether_th1) TerminateThread(ether_th1,0);
		thread_active_1 = false;
	}
	if(thread_active_2) {
		D(bug("Ether killing ether_th2\n"));
		if(ether_th2) TerminateThread(ether_th2,0);
		thread_active_2 = false;
	}
	if(thread_active_3) {
		D(bug("Ether killing thread\n"));
		if(ether_th) TerminateThread(ether_th,0);
		thread_active_3 = false;
	}

	ether_th1 = 0;
	ether_th2 = 0;
	ether_th = 0;

	D(bug("Closing semaphores\n"));
	if(int_ack) {
		CloseHandle(int_ack);
		int_ack = 0;
	}
	if(int_sig) {
		CloseHandle(int_sig);
		int_sig = 0;
	}
	if(int_sig2) {
		CloseHandle(int_sig2);
		int_sig2 = 0;
	}
	if(int_send_now) {
		CloseHandle(int_send_now);
		int_send_now = 0;
	}

	// Close ethernet device
	if (fd) {
		switch (net_if_type) {
		case NET_IF_B2ETHER:
			PacketCloseAdapter(fd);
			break;
		case NET_IF_TAP:
			tap_close_adapter(fd);
			break;
		}
		fd = 0;
	}

	// Remove all protocols
	D(bug("Removing protocols\n"));
	NetProtocol *p = prot_list;
	while (p) {
		NetProtocol *next = p->next;
		delete p;
		p = next;
	}
	prot_list = 0;

	D(bug("Deleting sections\n"));
	DeleteCriticalSection( &fetch_csection );
	DeleteCriticalSection( &queue_csection );
	DeleteCriticalSection( &send_csection );
	DeleteCriticalSection( &wpool_csection );

	D(bug("Freeing read packets\n"));
	free_read_packets();

	D(bug("Freeing write packets\n"));
	free_write_packets();

	D(bug("Finalizing queue\n"));
	final_queue();

	if (net_if_type == NET_IF_ROUTER) {
		D(bug("Stopping router\n"));
		router_final();
	}

	D(bug("EtherExit done\n"));
}


/*
 *  Glue around low-level implementation
 */

#ifdef SHEEPSHAVER
// Error codes
enum {
	eMultiErr		= -91,
	eLenErr			= -92,
	lapProtErr		= -94,
	excessCollsns	= -95
};

// Initialize ethernet
void EtherInit(void)
{
	net_open = false;

	// Do nothing if the user disabled the network
	if (PrefsFindBool("nonet"))
		return;

	net_open = ether_init();
}

// Exit ethernet
void EtherExit(void)
{
	ether_exit();
	net_open = false;
}

// Get ethernet hardware address
void AO_get_ethernet_address(uint32 arg)
{
	uint8 *addr = Mac2HostAddr(arg);
	if (net_open)
		OTCopy48BitAddress(ether_addr, addr);
	else {
		addr[0] = 0x12;
		addr[1] = 0x34;
		addr[2] = 0x56;
		addr[3] = 0x78;
		addr[4] = 0x9a;
		addr[5] = 0xbc;
	}
	D(bug("AO_get_ethernet_address: got address %02x%02x%02x%02x%02x%02x\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]));
}

// Add multicast address
void AO_enable_multicast(uint32 addr)
{
	if (net_open)
		ether_do_add_multicast(Mac2HostAddr(addr));
}

// Disable multicast address
void AO_disable_multicast(uint32 addr)
{
	if (net_open)
		ether_do_del_multicast(Mac2HostAddr(addr));
}

// Transmit one packet
void AO_transmit_packet(uint32 mp)
{
	if (net_open) {
		switch (ether_do_write(mp)) {
		case noErr:
			num_tx_packets++;
			break;
		case excessCollsns:
			num_tx_buffer_full++;
			break;
		}
	}
}

// Copy packet data from message block to linear buffer
static inline int ether_arg_to_buffer(uint32 mp, uint8 *p)
{
	return ether_msgb_to_buffer(mp, p);
}

// Ethernet interrupt
void EtherIRQ(void)
{
	D(bug("EtherIRQ\n"));
	num_ether_irq++;

	OTEnterInterrupt();
	ether_do_interrupt();
	OTLeaveInterrupt();

	// Acknowledge interrupt to reception thread
	D(bug(" EtherIRQ done\n"));
	ReleaseSemaphore(int_ack,1,NULL);
}
#else
// Add multicast address
int16 ether_add_multicast(uint32 pb)
{
	return ether_do_add_multicast(Mac2HostAddr(pb + eMultiAddr));
}

// Disable multicast address
int16 ether_del_multicast(uint32 pb)
{
	return ether_do_del_multicast(Mac2HostAddr(pb + eMultiAddr));
}

// Transmit one packet
int16 ether_write(uint32 wds)
{
	return ether_do_write(wds);
}

// Copy packet data from WDS to linear buffer
static inline int ether_arg_to_buffer(uint32 wds, uint8 *p)
{
	return ether_wds_to_buffer(wds, p);
}

// Dispatch packet to protocol handler
static void ether_dispatch_packet(uint32 packet, uint32 length)
{
	// Get packet type
	uint16 type = ReadMacInt16(packet + 12);

	// Look for protocol
	NetProtocol *prot = find_protocol(type);
	if (prot == NULL)
		return;

	// No default handler
	if (prot->handler == 0)
		return;

	// Copy header to RHA
	Mac2Mac_memcpy(ether_data + ed_RHA, packet, 14);
	D(bug(" header %08lx%04lx %08lx%04lx %04lx\n", ReadMacInt32(ether_data + ed_RHA), ReadMacInt16(ether_data + ed_RHA + 4), ReadMacInt32(ether_data + ed_RHA + 6), ReadMacInt16(ether_data + ed_RHA + 10), ReadMacInt16(ether_data + ed_RHA + 12)));

	// Call protocol handler
	M68kRegisters r;
	r.d[0] = type;								// Packet type
	r.d[1] = length - 14;						// Remaining packet length (without header, for ReadPacket)
	r.a[0] = packet + 14;						// Pointer to packet (Mac address, for ReadPacket)
	r.a[3] = ether_data + ed_RHA + 14;			// Pointer behind header in RHA
	r.a[4] = ether_data + ed_ReadPacket;		// Pointer to ReadPacket/ReadRest routines
	D(bug(" calling protocol handler %08lx, type %08lx, length %08lx, data %08lx, rha %08lx, read_packet %08lx\n", prot->handler, r.d[0], r.d[1], r.a[0], r.a[3], r.a[4]));
	Execute68k(prot->handler, &r);
}

// Ethernet interrupt
void EtherInterrupt(void)
{
	D(bug("EtherIRQ\n"));
	ether_do_interrupt();

	// Acknowledge interrupt to reception thread
	D(bug(" EtherIRQ done\n"));
	ReleaseSemaphore(int_ack,1,NULL);
}
#endif


/*
 *  Reset
 */

void ether_reset(void)
{
	D(bug("EtherReset\n"));

	// Remove all protocols
	NetProtocol *p = prot_list;
	while (p) {
		NetProtocol *next = p->next;
		delete p;
		p = next;
	}
	prot_list = NULL;
}


/*
 *  Add multicast address
 */

static int16 ether_do_add_multicast(uint8 *addr)
{
	D(bug("ether_add_multicast\n"));

	// We wouldn't need to do this
	// if(ether_multi_mode != ETHER_MULTICAST_MAC) return noErr;

	switch (net_if_type) {
	case NET_IF_B2ETHER:
		if (!PacketAddMulticast( fd, addr)) {
			D(bug("WARNING: couldn't enable multicast address\n"));
			return eMultiErr;
		}
	default:
		D(bug("ether_add_multicast: noErr\n"));
		return noErr;
	}
}


/*
 *  Delete multicast address
 */

int16 ether_do_del_multicast(uint8 *addr)
{
	D(bug("ether_del_multicast\n"));

	// We wouldn't need to do this
	// if(ether_multi_mode != ETHER_MULTICAST_MAC) return noErr;

	switch (net_if_type) {
	case NET_IF_B2ETHER:
		if (!PacketDelMulticast( fd, addr)) {
			D(bug("WARNING: couldn't disable multicast address\n"));
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
	D(bug("ether_attach_ph type=0x%x, handler=0x%x\n",(int)type,handler));

	// Already attached?
	NetProtocol *p = find_protocol(type);
	if (p != NULL) {
		D(bug("ether_attach_ph: lapProtErr\n"));
		return lapProtErr;
	} else {
		// No, create and attach
		p = new NetProtocol;
		p->next = prot_list;
		p->type = type;
		p->handler = handler;
		prot_list = p;
		D(bug("ether_attach_ph: noErr\n"));
		return noErr;
	}
}


/*
 *  Detach protocol handler
 */

int16 ether_detach_ph(uint16 type)
{
	D(bug("ether_detach_ph type=%08lx\n",(int)type));

	NetProtocol *p = find_protocol(type);
	if (p != NULL) {
		NetProtocol *previous = 0;
		NetProtocol *q = prot_list;
		while(q) {
			if (q == p) {
				if(previous) {
					previous->next = q->next;
				} else {
					prot_list = q->next;
				}
				delete p;
				return noErr;
			}
			previous = q;
			q = q->next;
		}
	}
	return lapProtErr;
}

#if MONITOR
static void dump_packet( uint8 *packet, int length )
{
	char buf[1000], sm[10];

	*buf = 0;

	if(length > 256) length = 256;

	for (int i=0; i<length; i++) {
		sprintf(sm," %02x", (int)packet[i]);
		strcat( buf, sm );
	}
	strcat( buf, "\n" );
	bug(buf);
}
#endif


/*
 *  Transmit raw ethernet packet
 */

static void insert_send_queue( LPPACKET Packet )
{
	EnterCriticalSection( &send_csection );
	Packet->next = 0;
	if(send_queue) {
		LPPACKET p = send_queue;
		// The queue is short. It would be larger overhead to double-link it.
		while(p->next) p = p->next;
		p->next = Packet;
	} else {
		send_queue = Packet;
	}
	LeaveCriticalSection( &send_csection );
}

static LPPACKET get_send_head( void )
{
	LPPACKET Packet = 0;

	EnterCriticalSection( &send_csection );
	if(send_queue) {
		Packet = send_queue;
		send_queue = send_queue->next;
	}
	LeaveCriticalSection( &send_csection );

	return Packet;
}

static int get_write_packet_pool_sz( void )
{
	LPPACKET t = write_packet_pool;
	int sz = 0;

	while(t) {
		t = t->next;
		sz++;
	}
	return(sz);
}

static void free_write_packets( void )
{
	LPPACKET next;
	int i = 0;
	while(write_packet_pool) {
		next = write_packet_pool->next;
		D(bug("Freeing write packet %ld\n",++i));
		PacketFreePacket(write_packet_pool);
		write_packet_pool = next;
	}
}

void recycle_write_packet( LPPACKET Packet )
{
	EnterCriticalSection( &wpool_csection );
	Packet->next = write_packet_pool;
	write_packet_pool = Packet;
	D(bug("Pool size after recycling = %ld\n",get_write_packet_pool_sz()));
	LeaveCriticalSection( &wpool_csection );
}

static LPPACKET get_write_packet( UINT len )
{
	LPPACKET Packet = 0;

	EnterCriticalSection( &wpool_csection );
	if(write_packet_pool) {
		Packet = write_packet_pool;
		write_packet_pool = write_packet_pool->next;
		Packet->OverLapped.Offset = 0;
		Packet->OverLapped.OffsetHigh = 0;
		Packet->Length = len;
		Packet->BytesReceived	= 0;
		Packet->bIoComplete	= FALSE;
		Packet->free = TRUE;
		Packet->next = 0;
		// actually an auto-reset event.
		if(Packet->OverLapped.hEvent) ResetEvent(Packet->OverLapped.hEvent);
	} else {
		Packet = PacketAllocatePacket(fd,len);
	}

	D(bug("Pool size after get wr packet = %ld\n",get_write_packet_pool_sz()));

	LeaveCriticalSection( &wpool_csection );

	return Packet;
}

unsigned int WINAPI ether_thread_write_packets(void *arg)
{
	LPPACKET Packet;

	thread_active_1 = true;

	D(bug("ether_thread_write_packets start\n"));

	while(thread_active) {
		// must be alertable, otherwise write completion is never called
		WaitForSingleObjectEx(int_send_now,INFINITE,TRUE);
		while( thread_active && (Packet = get_send_head()) != 0 ) {
			switch (net_if_type) {
			case NET_IF_ROUTER:
				if(router_write_packet((uint8 *)Packet->Buffer, Packet->Length)) {
					Packet->bIoComplete = TRUE;
					recycle_write_packet(Packet);
				}
				break;
			case NET_IF_FAKE:
				Packet->bIoComplete = TRUE;
				recycle_write_packet(Packet);
				break;
			case NET_IF_B2ETHER:
				if(!PacketSendPacket( fd, Packet, FALSE, TRUE )) {
					// already recycled if async
				}
				break;
			case NET_IF_TAP:
				if (!tap_send_packet(fd, Packet, FALSE, TRUE)) {
					// already recycled if async
				}
				break;
			case NET_IF_SLIRP:
				slirp_input((uint8 *)Packet->Buffer, Packet->Length);
				Packet->bIoComplete = TRUE;
				recycle_write_packet(Packet);
				break;
			}
		}
	}

	D(bug("ether_thread_write_packets exit\n"));

	thread_active_1 = false;

	return(0);
}

static BOOL write_packet( uint8 *packet, int len )
{
	LPPACKET Packet;

	D(bug("write_packet\n"));

	Packet = get_write_packet(len);
	if(Packet) {
		memcpy( Packet->Buffer, packet, len );

		EnterCriticalSection( &fetch_csection );
		pending_packet_sz[echo_count] = min(sizeof(pending_packet),len);
		memcpy( pending_packet[echo_count], packet, pending_packet_sz[echo_count] );
		echo_count = (echo_count+1) & (~(MAX_ECHO-1));
		LeaveCriticalSection( &fetch_csection );

		insert_send_queue( Packet );

		ReleaseSemaphore(int_send_now,1,NULL);
		return(TRUE);
	} else {
		return(FALSE);
	}
}

static int16 ether_do_write(uint32 arg)
{
	D(bug("ether_write\n"));

	// Copy packet to buffer
	uint8 packet[1514], *p = packet;
	int len = ether_arg_to_buffer(arg, p);

	if(len > 1514) {
		D(bug("illegal packet length: %d\n",len));
		return eLenErr;
	} else {
#if MONITOR
		bug("Sending Ethernet packet (%d bytes):\n",(int)len);
		dump_packet( packet, len );
#endif
	}

	// Transmit packet
	if (!write_packet(packet, len)) {
		D(bug("WARNING: couldn't transmit packet\n"));
		return excessCollsns;
	} else {
		// It's up to the protocol drivers to do the error checking. Even if the
		// i/o completion routine returns ok, there can be errors, so there is
		// no point to wait for write completion and try to make some sense of the
		// possible error codes.
		return noErr;
	}
}


static void init_queue(void)
{
	queue_inx = 0;
	queue_head = 0;

	for( int i=0; i<MAX_QUEUE_ITEMS; i++ ) {
		queue[i].buf = (uint8 *)malloc( 1514 );
		queue[i].sz = 0;
	}
}

static void final_queue(void)
{
	for( int i=0; i<MAX_QUEUE_ITEMS; i++ ) {
		if(queue[i].buf) free(queue[i].buf);
	}
}

void enqueue_packet( const uint8 *buf, int sz )
{
	EnterCriticalSection( &queue_csection );
	if(queue[queue_inx].sz > 0) {
		D(bug("ethernet queue full, packet dropped\n"));
	} else {
		if(sz > 1514) sz = 1514;
		queue[queue_inx].sz = sz;
		memcpy( queue[queue_inx].buf, buf, sz );
		queue_inx++;
		if(queue_inx >= MAX_QUEUE_ITEMS) queue_inx = 0;
		if(wait_request) {
			wait_request = false;
			ReleaseSemaphore(int_sig,1,NULL);
		}
	}
	LeaveCriticalSection( &queue_csection );
}

static int dequeue_packet( uint8 *buf )
{
	int sz;

	if(!thread_active) return(0);

	EnterCriticalSection( &queue_csection );
	sz = queue[queue_head].sz;
	if(sz > 0) {
		memcpy( buf, queue[queue_head].buf, sz );
		queue[queue_head].sz = 0;
		queue_head++;
		if(queue_head >= MAX_QUEUE_ITEMS) queue_head = 0;
	}
	LeaveCriticalSection( &queue_csection );
	return(sz);
}

static void trigger_queue(void)
{
	EnterCriticalSection( &queue_csection );
	if( queue[queue_head].sz > 0 ) {
		D(bug(" packet received, triggering Ethernet interrupt\n"));
		SetInterruptFlag(INTFLAG_ETHER);
		TriggerInterrupt();
		// of course can't wait here.
	}
	LeaveCriticalSection( &queue_csection );
}

static bool set_wait_request(void)
{
	bool result;
	EnterCriticalSection( &queue_csection );
	if(queue[queue_head].sz) {
		result = true;
	} else {
		result = false;
		wait_request = true;
	}
	LeaveCriticalSection( &queue_csection );
	return(result);
}


/*
 *  TAP-Win32 glue
 */

static LPADAPTER tap_open_adapter(LPCTSTR dev_name)
{
	fd = (LPADAPTER)GlobalAllocPtr(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(*fd));
	if (fd == NULL)
		return NULL;

	TCHAR dev_path[MAX_PATH];
	_sntprintf(dev_path, lengthof(dev_path),
			 TEXT("\\\\.\\Global\\%s.tap"), dev_name);

	HANDLE handle = CreateFile(
		dev_path,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED,
		NULL);
	if (handle == NULL || handle == INVALID_HANDLE_VALUE)
		return NULL;

	fd->hFile = handle;
	return fd;
}

static void tap_close_adapter(LPADAPTER fd)
{
	if (fd) {
		if (fd->hFile) {
			tap_set_status(fd, false);
			CloseHandle(fd->hFile);
		}
		GlobalFreePtr(fd);
	}
}

static bool tap_check_version(LPADAPTER fd)
{
	ULONG len;
	ULONG info[3] = { 0, };

	if (!DeviceIoControl(fd->hFile, TAP_IOCTL_GET_VERSION,
						 &info, sizeof(info),
						 &info, sizeof(info), &len, NULL)
		&& !DeviceIoControl(fd->hFile, OLD_TAP_IOCTL_GET_VERSION,
							&info, sizeof(info),
							&info, sizeof(info), &len, NULL))
		return false;

	if (info[0] > TAP_VERSION_MIN_MAJOR)
		return true;
	if (info[0] == TAP_VERSION_MIN_MAJOR && info[1] >= TAP_VERSION_MIN_MINOR)
		return true;

	return false;
}

static bool tap_set_status(LPADAPTER fd, ULONG status)
{
	DWORD len = 0;
	return DeviceIoControl(fd->hFile, TAP_IOCTL_SET_MEDIA_STATUS,
						   &status, sizeof (status),
						   &status, sizeof (status), &len, NULL) != FALSE;
}

static bool tap_get_mac(LPADAPTER fd, LPBYTE addr)
{
	DWORD len = 0;
	return DeviceIoControl(fd->hFile, TAP_IOCTL_GET_MAC,
						   addr, 6,
						   addr, 6, &len, NULL) != FALSE;
}

static VOID CALLBACK tap_write_completion(
	DWORD dwErrorCode,
	DWORD dwNumberOfBytesTransfered,
	LPOVERLAPPED lpOverLapped
	)
{
	LPPACKET lpPacket = CONTAINING_RECORD(lpOverLapped, PACKET, OverLapped);

	lpPacket->bIoComplete = TRUE;
	recycle_write_packet(lpPacket);
}

static bool tap_send_packet(
	LPADAPTER fd,
	LPPACKET lpPacket,
	BOOLEAN Sync,
	BOOLEAN RecyclingAllowed)
{
	BOOLEAN Result;

	lpPacket->OverLapped.Offset = 0;
	lpPacket->OverLapped.OffsetHigh = 0;
	lpPacket->bIoComplete = FALSE;

	if (Sync) {
		Result = WriteFile(fd->hFile,
						   lpPacket->Buffer,
						   lpPacket->Length,
						   &lpPacket->BytesReceived,
						   &lpPacket->OverLapped);
		if (Result) {
			GetOverlappedResult(fd->hFile,
								&lpPacket->OverLapped,
								&lpPacket->BytesReceived,
								TRUE);
		}
		lpPacket->bIoComplete = TRUE;
		if (RecyclingAllowed)
			PacketFreePacket(lpPacket);
	}
	else {
		Result = WriteFileEx(fd->hFile,
							 lpPacket->Buffer,
							 lpPacket->Length,
							 &lpPacket->OverLapped,
							 tap_write_completion);

		if (!Result && RecyclingAllowed)
			recycle_write_packet(lpPacket);
	}

	return Result != FALSE;
}

static bool tap_receive_packet(LPADAPTER fd, LPPACKET lpPacket, BOOLEAN Sync)
{
	BOOLEAN Result;

	lpPacket->OverLapped.Offset = 0;
	lpPacket->OverLapped.OffsetHigh = 0;
	lpPacket->bIoComplete = FALSE;

	if (Sync) {
		Result = ReadFile(fd->hFile,
						  lpPacket->Buffer,
						  lpPacket->Length,
						  &lpPacket->BytesReceived,
						  &lpPacket->OverLapped);
		if (Result) {
			Result = GetOverlappedResult(fd->hFile,
										 &lpPacket->OverLapped,
										 &lpPacket->BytesReceived,
										 TRUE);
			if (Result)
				lpPacket->bIoComplete = TRUE;
			else
				lpPacket->free = TRUE;
		}
	}
	else {
		Result = ReadFileEx(fd->hFile,
							lpPacket->Buffer,
							lpPacket->Length,
							&lpPacket->OverLapped,
							packet_read_completion);

		if (!Result)
			lpPacket->BytesReceived = 0;
	}

	return Result != FALSE;
}


/*
 *  SLIRP output buffer glue
 */

int slirp_can_output(void)
{
	return 1;
}

void slirp_output(const uint8 *packet, int len)
{
	enqueue_packet(packet, len);
}

unsigned int WINAPI slirp_receive_func(void *arg)
{
	D(bug("slirp_receive_func\n"));
	thread_active_2 = true;

	while (thread_active) {
		// Wait for packets to arrive
		fd_set rfds, wfds, xfds;
		int nfds, ret, timeout;

		// ... in the output queue
		nfds = -1;
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);
		FD_ZERO(&xfds);
		timeout = slirp_select_fill(&nfds, &rfds, &wfds, &xfds);
#if ! USE_SLIRP_TIMEOUT
		timeout = 10000;
#endif
		if (nfds < 0) {
			/* Windows does not honour the timeout if there is not
			   descriptor to wait for */
			Delay_usec(timeout);
			ret = 0;
		}
		else {
			struct timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = timeout;
			ret = select(0, &rfds, &wfds, &xfds, &tv);
		}
		if (ret >= 0)
			slirp_select_poll(&rfds, &wfds, &xfds);
	}

	D(bug("slirp_receive_func exit\n"));
	thread_active_2 = false;
	return 0;
}

const uint8 ether_broadcast_addr[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
const uint8 appletalk_broadcast_addr[6] = { 0x09, 0x00, 0x07, 0xFF, 0xFF, 0xFF };
const uint8 appletalk_zone_multicast_prefix[5] = { 0x09, 0x00, 0x07, 0x00, 0x00 };

/*
 *  Packet reception threads
 */

VOID CALLBACK packet_read_completion(
	DWORD dwErrorCode,
	DWORD dwNumberOfBytesTransfered,
	LPOVERLAPPED lpOverlapped
	)
{
	EnterCriticalSection( &fetch_csection );

	LPPACKET lpPacket = CONTAINING_RECORD(lpOverlapped,PACKET,OverLapped);

	D(bug("packet_read_completion bytes=%d, error code=%d\n",dwNumberOfBytesTransfered,dwErrorCode));

	if(thread_active && !dwErrorCode) {
		int count = min(dwNumberOfBytesTransfered,1514);
		if(count) {
			int j = echo_count;
			for(int i=MAX_ECHO; i; i--) {
				j--;
				if(j < 0) j = MAX_ECHO-1;
				if(count == pending_packet_sz[j] &&
				   memcmp(pending_packet[j],lpPacket->Buffer,count) == 0)
				{
					D(bug("packet_read_completion discarding own packet.\n"));
					dwNumberOfBytesTransfered = 0;

					j = (j+1) & (~(MAX_ECHO-1));
					if(j != echo_count) {
						D(bug("Wow, this fix made some good after all...\n"));
					}

					break;
				}
			}
			// XXX drop packets that we don't care about
			if (net_if_type == NET_IF_TAP) {
				if (memcmp((LPBYTE)lpPacket->Buffer, ether_addr, 6) != 0 &&
					memcmp((LPBYTE)lpPacket->Buffer, ether_broadcast_addr, 6) != 0 &&
					memcmp((LPBYTE)lpPacket->Buffer, appletalk_broadcast_addr, 6) != 0 &&
					memcmp((LPBYTE)lpPacket->Buffer, appletalk_zone_multicast_prefix, 5) != 0
					) {
					dwNumberOfBytesTransfered = 0;
				}
			}
			if(dwNumberOfBytesTransfered) {
				if(net_if_type != NET_IF_ROUTER || !router_read_packet((uint8 *)lpPacket->Buffer, dwNumberOfBytesTransfered)) {
					enqueue_packet( (LPBYTE)lpPacket->Buffer, dwNumberOfBytesTransfered );
				}
			}
		}
	}

	// actually an auto-reset event.
	if(lpPacket->OverLapped.hEvent) ResetEvent(lpPacket->OverLapped.hEvent);

	lpPacket->free = TRUE;
	lpPacket->bIoComplete = TRUE;

	if(wait_request2) {
		wait_request2 = false;
		ReleaseSemaphore(int_sig2,1,NULL);
	}

	LeaveCriticalSection( &fetch_csection );
}

static BOOL has_no_completed_io(void)
{
	BOOL result = TRUE;

	EnterCriticalSection( &fetch_csection );

	for( int i=0; i<PACKET_POOL_COUNT; i++ ) {
		if(packets[i]->bIoComplete) {
			result = FALSE;
			break;
		}
	}
	if(result) wait_request2 = true;

	LeaveCriticalSection( &fetch_csection );
	return(result);
}

static bool allocate_read_packets(void)
{
	for( int i=0; i<PACKET_POOL_COUNT; i++ ) {
		packets[i] = PacketAllocatePacket(fd,1514);
		if(!packets[i]) {
			D(bug("allocate_read_packets: out of memory\n"));
			return(false);
		}
	}
	return(true);
}

static void free_read_packets(void)
{
	for( int i=0; i<PACKET_POOL_COUNT; i++ ) {
		PacketFreePacket(packets[i]);
	}
}

unsigned int WINAPI ether_thread_get_packets_nt(void *arg)
{
	static uint8 packet[1514];
	int i, packet_sz = 0;

	thread_active_2 = true;

	D(bug("ether_thread_get_packets_nt start\n"));

	// Wait for packets to arrive.
	// Obey the golden rules; keep the reads pending.
	while(thread_active) {

		if(net_if_type == NET_IF_B2ETHER || net_if_type == NET_IF_TAP) {
			D(bug("Pending reads\n"));
			for( i=0; thread_active && i<PACKET_POOL_COUNT; i++ ) {
				if(packets[i]->free) {
					packets[i]->free = FALSE;
					BOOLEAN Result;
					switch (net_if_type) {
					case NET_IF_B2ETHER:
						Result = PacketReceivePacket(fd, packets[i], FALSE);
						break;
					case NET_IF_TAP:
						Result = tap_receive_packet(fd, packets[i], FALSE);
						break;
					}
					if (Result) {
						if(packets[i]->bIoComplete) {
							D(bug("Early io completion...\n"));
							packet_read_completion(
								ERROR_SUCCESS,
								packets[i]->BytesReceived,
								&packets[i]->OverLapped
								);
						}
					} else {
						packets[i]->free = TRUE;
					}
				}
			}
		}

		if(thread_active && has_no_completed_io()) {
			D(bug("Waiting for int_sig2\n"));
			// "problem": awakens twice in a row. Fix if you increase the pool size.
			WaitForSingleObjectEx(int_sig2,INFINITE,TRUE);
		}
	}

	D(bug("ether_thread_get_packets_nt exit\n"));

	thread_active_2 = false;

	return 0;
}

unsigned int WINAPI ether_thread_feed_int(void *arg)
{
	bool looping;

	thread_active_3 = true;

	D(bug("ether_thread_feed_int start\n"));

	while(thread_active) {
		D(bug("Waiting for int_sig\n"));
		WaitForSingleObject(int_sig,INFINITE);
		// Looping this way to avoid a race condition.
		D(bug("Triggering\n"));
		looping = true;
		while(thread_active && looping) {
			trigger_queue();
			// Wait for interrupt acknowledge by EtherInterrupt()
			WaitForSingleObject(int_ack,INFINITE);
			if(thread_active) looping = set_wait_request();
		}
		D(bug("Queue empty.\n"));
	}

	D(bug("ether_thread_feed_int exit\n"));

	thread_active_3 = false;

	return 0;
}


/*
 *  Ethernet interrupt - activate deferred tasks to call IODone or protocol handlers
 */

static void ether_do_interrupt(void)
{
	// Call protocol handler for received packets
	EthernetPacket ether_packet;
	uint32 packet = ether_packet.addr();
	ssize_t length;
	for (;;) {

		// Read packet from Ethernet device
		length = dequeue_packet(Mac2HostAddr(packet));
		if (length < 14)
			break;

#if MONITOR
		bug("Receiving Ethernet packet (%d bytes):\n",(int)length);
		dump_packet( Mac2HostAddr(packet), length );
#endif

		// Dispatch packet
		ether_dispatch_packet(packet, length);
	}
}

#if DEBUG
#pragma optimize("",on)
#endif
