/*
 *  ether_windows.cpp - Ethernet device driver
 *
 *  Basilisk II (C) 1997-2005 Christian Bauer
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

#include <process.h>
#include <windowsx.h>
#include <ctype.h>

#include "sysdeps.h"
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
#include "kernel_windows.h"


#define DEBUG 0
#define MONITOR 0

#if DEBUG
#pragma optimize("",off)
#endif

#include "debug.h"


// Options
bool ether_use_permanent = true;
static int16 ether_multi_mode = ETHER_MULTICAST_MAC;

// Global variables
HANDLE ether_th;
unsigned int ether_tid;
HANDLE ether_th1;
HANDLE ether_th2;


// Need to fake a NIC if there is none but the router module is activated.
bool ether_fake = false;

// These are protected by queue_csection
// Controls transfer for read thread to feed thread
static CRITICAL_SECTION queue_csection;
typedef struct _queue_t {
	uint8 *buf;
	int sz;
} queue_t;
#define MAX_QUEUE_ITEMS 1024
static queue_t queue[MAX_QUEUE_ITEMS];
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

static char edevice[512];


// Prototypes
static WINAPI unsigned int ether_thread_feed_int(void *arg);
static WINAPI unsigned int ether_thread_get_packets_nt(void *arg);
static WINAPI unsigned int ether_thread_write_packets(void *arg);
static void init_queue(void);
static void final_queue(void);
static bool allocate_read_packets(void);
static void free_read_packets(void);
static void free_write_packets(void);


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
	char str[256];

	// Initialize NAT-Router
	router_init();

	// Do nothing if no Ethernet device specified
	const char *name = PrefsFindString("ether");
	if (name)
		strcpy(edevice, name);

	bool there_is_a_router = PrefsFindBool("routerenabled");

	if (!name || !*name) {
		if( there_is_a_router ) {
			strcpy( edevice, "None" );
			ether_fake = true;
		} else {
			return false;
		}
	}

	ether_use_permanent = PrefsFindBool("etherpermanentaddress");
	ether_multi_mode = PrefsFindInt32("ethermulticastmode");

	// Open ethernet device
	if(ether_fake) {
		memcpy( ether_addr, router_mac_addr, 6 );
		D(bug("Fake ethernet address (same as router) %02x %02x %02x %02x %02x %02x\r\n", ether_addr[0], ether_addr[1], ether_addr[2], ether_addr[3], ether_addr[4], ether_addr[5]));
	} else {
		fd = PacketOpenAdapter( name, ether_multi_mode );
		if (!fd) {
			sprintf(str, "Could not open ethernet adapter %s.", name);
			WarningAlert(str);
			goto open_error;
		}

		// Get Ethernet address
		if(!PacketGetMAC(fd,ether_addr,ether_use_permanent)) {
			sprintf(str, "Could not get hardware address of device %s. Ethernet is not available.", name);
			WarningAlert(str);
			goto open_error;
		}
		D(bug("Real ethernet address %02x %02x %02x %02x %02x %02x\r\n", ether_addr[0], ether_addr[1], ether_addr[2], ether_addr[3], ether_addr[4], ether_addr[5]));

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
			D(bug("Fake ethernet address %02x %02x %02x %02x %02x %02x\r\n", ether_addr[0], ether_addr[1], ether_addr[2], ether_addr[3], ether_addr[4], ether_addr[5]));
		}
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

	if(pfnInitializeCriticalSectionAndSpinCount) {
		pfnInitializeCriticalSectionAndSpinCount( &fetch_csection, 5000 );
	} else {
		InitializeCriticalSection( &fetch_csection );
	}
	if(pfnInitializeCriticalSectionAndSpinCount) {
		pfnInitializeCriticalSectionAndSpinCount( &queue_csection, 5000 );
	} else {
		InitializeCriticalSection( &queue_csection );
	}
	if(pfnInitializeCriticalSectionAndSpinCount) {
		pfnInitializeCriticalSectionAndSpinCount( &send_csection, 5000 );
	} else {
		InitializeCriticalSection( &send_csection );
	}
	if(pfnInitializeCriticalSectionAndSpinCount) {
		pfnInitializeCriticalSectionAndSpinCount( &wpool_csection, 5000 );
	} else {
		InitializeCriticalSection( &wpool_csection );
	}

	ether_th = (HANDLE)_beginthreadex( 0, 0, ether_thread_feed_int, 0, 0, &ether_tid );
	if (!ether_th) {
		D(bug("Failed to create ethernet thread\r\n"));
		goto open_error;
	}
	thread_active = true;
#if 0
	SetThreadPriority( ether_th, threads[THREAD_ETHER].priority_running );
	SetThreadAffinityMask( ether_th, threads[THREAD_ETHER].affinity_mask );
#endif

	unsigned int dummy;
	ether_th2 = (HANDLE)_beginthreadex( 0, 0, ether_thread_get_packets_nt, 0, 0, &dummy );
#if 0
	SetThreadPriority( ether_th2, threads[THREAD_ETHER].priority_running );
	SetThreadAffinityMask( ether_th2, threads[THREAD_ETHER].affinity_mask );
#endif

	ether_th1 = (HANDLE)_beginthreadex( 0, 0, ether_thread_write_packets, 0, 0, &dummy );
#if 0
	SetThreadPriority( ether_th1, threads[THREAD_ETHER].priority_running );
	SetThreadAffinityMask( ether_th1, threads[THREAD_ETHER].affinity_mask );
#endif

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
	if(!ether_fake) {
		PacketCloseAdapter(fd);
	}
	fd = 0;
	return false;
}


/*
 *  Deinitialization
 */

void ether_exit(void)
{
	D(bug("EtherExit\r\n"));

	// Take them down in a controlled way.
	thread_active = false;

	// _asm int 3

	D(bug("Closing ethernet device %s\r\n",edevice));

	if(!*edevice) return;

	if(int_ack) ReleaseSemaphore(int_ack,1,NULL);
	if(int_sig) ReleaseSemaphore(int_sig,1,NULL);
	if(int_sig2) ReleaseSemaphore(int_sig2,1,NULL);
	if(int_send_now) ReleaseSemaphore(int_send_now,1,NULL);

	D(bug("CancelIO if needed\r\n"));
	if (fd && fd->hFile && pfnCancelIo)
		pfnCancelIo(fd->hFile);

	// Wait max 2 secs to shut down pending io. After that, kill them.
	D(bug("Wait delay\r\n"));
	for( int i=0; i<10; i++ ) {
		if(!thread_active_1 && !thread_active_2 && !thread_active_3) break;
		Sleep(200);
	}

	if(thread_active_1) {
		D(bug("Ether killing ether_th1\r\n"));
		if(ether_th1) TerminateThread(ether_th1,0);
		thread_active_1 = false;
	}
	if(thread_active_2) {
		D(bug("Ether killing ether_th2\r\n"));
		if(ether_th2) TerminateThread(ether_th2,0);
		thread_active_2 = false;
	}
	if(thread_active_3) {
		D(bug("Ether killing thread\r\n"));
		if(ether_th) TerminateThread(ether_th,0);
		thread_active_3 = false;
	}

	ether_th1 = 0;
	ether_th2 = 0;
	ether_th = 0;

	D(bug("Closing semaphores\r\n"));
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
	if(fd) {
		PacketCloseAdapter(fd);
		fd = 0;
	}

	// Remove all protocols
	D(bug("Removing protocols\r\n"));
	NetProtocol *p = prot_list;
	while (p) {
		NetProtocol *next = p->next;
		delete p;
		p = next;
	}
	prot_list = 0;

	D(bug("Deleting sections\r\n"));
	DeleteCriticalSection( &fetch_csection );
	DeleteCriticalSection( &queue_csection );
	DeleteCriticalSection( &send_csection );
	DeleteCriticalSection( &wpool_csection );

	D(bug("Freeing read packets\r\n"));
	free_read_packets();

	D(bug("Freeing write packets\r\n"));
	free_write_packets();

	D(bug("Finalizing queue\r\n"));
	final_queue();

	D(bug("Stopping router\r\n"));
	router_final();

	D(bug("EtherExit done\r\n"));
}


/*
 *  Reset
 */

void ether_reset(void)
{
	D(bug("EtherReset\r\n"));

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

int16 ether_add_multicast(uint32 pb)
{
	D(bug("ether_add_multicast\r\n"));

	// We wouldn't need to do this
	// if(ether_multi_mode != ETHER_MULTICAST_MAC) return noErr;

	if (!ether_fake && !PacketAddMulticast( fd, Mac2HostAddr(pb + eMultiAddr))) {
		D(bug("WARNING: couldn't enable multicast address\r\n"));
		return eMultiErr;
	} else {
		D(bug("ether_add_multicast: noErr\r\n"));
		return noErr;
	}
}


/*
 *  Delete multicast address
 */

int16 ether_del_multicast(uint32 pb)
{
	D(bug("ether_del_multicast\r\n"));

	// We wouldn't need to do this
	// if(ether_multi_mode != ETHER_MULTICAST_MAC) return noErr;

	if (!ether_fake && !PacketDelMulticast( fd, Mac2HostAddr(pb + eMultiAddr))) {
		D(bug("WARNING: couldn't disable multicast address\r\n"));
		return eMultiErr;
	} else
		return noErr;
}


/*
 *  Attach protocol handler
 */

int16 ether_attach_ph(uint16 type, uint32 handler)
{
	D(bug("ether_attach_ph type=0x%x, handler=0x%x\r\n",(int)type,handler));

	// Already attached?
	NetProtocol *p = find_protocol(type);
	if (p != NULL) {
		D(bug("ether_attach_ph: lapProtErr\r\n"));
		return lapProtErr;
	} else {
		// No, create and attach
		p = new NetProtocol;
		p->next = prot_list;
		p->type = type;
		p->handler = handler;
		prot_list = p;
		D(bug("ether_attach_ph: noErr\r\n"));
		return noErr;
	}
}


/*
 *  Detach protocol handler
 */

int16 ether_detach_ph(uint16 type)
{
	D(bug("ether_detach_ph type=%08lx\r\n",(int)type));

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
		sprintf(sm,"%02x", (int)packet[i]);
		strcat( buf, sm );
	}
	strcat( buf, "\r\n" );
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
		D(bug("Freeing write packet %ld\r\n",++i));
		PacketFreePacket(write_packet_pool);
		write_packet_pool = next;
	}
}

void recycle_write_packet( LPPACKET Packet )
{
	EnterCriticalSection( &wpool_csection );
	Packet->next = write_packet_pool;
	write_packet_pool = Packet;
	D(bug("Pool size after recycling = %ld\r\n",get_write_packet_pool_sz()));
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

	D(bug("Pool size after get wr packet = %ld\r\n",get_write_packet_pool_sz()));

	LeaveCriticalSection( &wpool_csection );

	return Packet;
}

static unsigned int ether_thread_write_packets(void *arg)
{
	LPPACKET Packet;

	thread_active_1 = true;

	D(bug("ether_thread_write_packets start\r\n"));

	while(thread_active) {
		// must be alertable, otherwise write completion is never called
		WaitForSingleObjectEx(int_send_now,INFINITE,TRUE);
		while( thread_active && (Packet = get_send_head()) != 0 ) {
			if(m_router_enabled && router_write_packet((uint8 *)Packet->Buffer, Packet->Length)) {
				Packet->bIoComplete = TRUE;
				recycle_write_packet(Packet);
			} else if(ether_fake) {
				Packet->bIoComplete = TRUE;
				recycle_write_packet(Packet);
			} else if(!PacketSendPacket( fd, Packet, FALSE, TRUE )) {
				// already recycled if async
			}
		}
	}

	D(bug("ether_thread_write_packets exit\r\n"));

	thread_active_1 = false;

	return(0);
}

static BOOL write_packet( uint8 *packet, int len )
{
	LPPACKET Packet;

	D(bug("write_packet\r\n"));

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

int16 ether_write(uint32 wds)
{
	D(bug("ether_write\r\n"));

	// Set source address
	uint32 hdr = ReadMacInt32(wds + 2);
	memcpy(Mac2HostAddr(hdr + 6), ether_addr, 6);

	// Copy packet to buffer
	uint8 packet[1514], *p = packet;
	int len = 0;
	for (;;) {
		uint16 w = (uint16)ReadMacInt16(wds);
		if (w == 0)
			break;
		memcpy(p, Mac2HostAddr(ReadMacInt32(wds + 2)), w);
		len += w;
		p += w;
		wds += 6;
	}

	if(len > 1514) {
		D(bug("illegal packet length: %d\r\n",len));
		return eLenErr;
	} else {
#if MONITOR
		bug("Sending Ethernet packet (%d bytes):\n",(int)len);
		dump_packet( packet, len );
#endif
	}

	// Transmit packet
	if (!write_packet(packet, len)) {
		D(bug("WARNING: couldn't transmit packet\r\n"));
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

void enqueue_packet( uint8 *buf, int sz )
{
	EnterCriticalSection( &queue_csection );
	if(queue[queue_inx].sz > 0) {
		D(bug("ethernet queue full, packet dropped\r\n"));
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
		D(bug(" packet received, triggering Ethernet interrupt\r\n"));
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
					D(bug("packet_read_completion discarding own packet.\r\n"));
					dwNumberOfBytesTransfered = 0;

					j = (j+1) & (~(MAX_ECHO-1));
					if(j != echo_count) {
						D(bug("Wow, this fix made some good after all...\r\n"));
					}

					break;
				}
			}
			if(dwNumberOfBytesTransfered) {
				if(!m_router_enabled || !router_read_packet((uint8 *)lpPacket->Buffer, dwNumberOfBytesTransfered)) {
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
			D(bug("allocate_read_packets: out of memory\r\n"));
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

static unsigned int ether_thread_get_packets_nt(void *arg)
{
	static uint8 packet[1514];
	int i, packet_sz = 0;

	thread_active_2 = true;

	D(bug("ether_thread_get_packets_nt start\r\n"));

	// Wait for packets to arrive.
	// Obey the golden rules; keep the reads pending.
	while(thread_active) {

		if(!ether_fake) {
			D(bug("Pending reads\r\n"));
			for( i=0; thread_active && i<PACKET_POOL_COUNT; i++ ) {
				if(packets[i]->free) {
					packets[i]->free = FALSE;
					if(PacketReceivePacket(fd,packets[i],FALSE)) {
						if(packets[i]->bIoComplete) {
							D(bug("Early io completion...\r\n"));
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
			D(bug("Waiting for int_sig2\r\n"));
			// "problem": awakens twice in a row. Fix if you increase the pool size.
			WaitForSingleObjectEx(int_sig2,INFINITE,TRUE);
		}
	}

	D(bug("ether_thread_get_packets_nt exit\r\n"));

	thread_active_2 = false;

	return 0;
}

static unsigned int ether_thread_feed_int(void *arg)
{
	bool looping;

	thread_active_3 = true;

	D(bug("ether_thread_feed_int start\r\n"));

	while(thread_active) {
		D(bug("Waiting for int_sig\r\n"));
		WaitForSingleObject(int_sig,INFINITE);
		// Looping this way to avoid a race condition.
		D(bug("Triggering\r\n"));
		looping = true;
		while(thread_active && looping) {
			trigger_queue();
			// Wait for interrupt acknowledge by EtherInterrupt()
			WaitForSingleObject(int_ack,INFINITE);
			if(thread_active) looping = set_wait_request();
		}
		D(bug("Queue empty.\r\n"));
	}

	D(bug("ether_thread_feed_int exit\r\n"));

	thread_active_3 = false;

	return 0;
}


/*
 *  Ethernet interrupt - activate deferred tasks to call IODone or protocol handlers
 */

void EtherInterrupt(void)
{
	int length;
	static uint8 packet[1514];

	D(bug("EtherIRQ\r\n"));

	// Call protocol handler for received packets
	while( (length = dequeue_packet(packet)) > 0 ) {

		if (length < 14)
			continue;

#if MONITOR
		bug("Receiving Ethernet packet (%d bytes):\n",(int)length);
		dump_packet( packet, length );
#endif

		// Get packet type
		uint16 type = ntohs(*(uint16 *)(packet + 12));

		// Look for protocol
		NetProtocol *prot = find_protocol(type);
		if (prot == NULL)
			continue;
		// break;

		// No default handler
		if (prot->handler == 0)
			continue;
		// break;

		// Copy header to RHA
		memcpy(Mac2HostAddr(ether_data + ed_RHA), packet, 14);
		D(bug(" header %08lx%04lx %08lx%04lx %04lx\r\n", ReadMacInt32(ether_data + ed_RHA), ReadMacInt16(ether_data + ed_RHA + 4), ReadMacInt32(ether_data + ed_RHA + 6), ReadMacInt16(ether_data + ed_RHA + 10), ReadMacInt16(ether_data + ed_RHA + 12)));

		// Call protocol handler
		M68kRegisters r;
		r.d[0] = type;                  // Packet type
		r.d[1] = length - 14;             // Remaining packet length (without header, for ReadPacket)

		r.a[0] = (uint32)packet + 14;         // Pointer to packet (host address, for ReadPacket)
		r.a[3] = ether_data + ed_RHA + 14;        // Pointer behind header in RHA
		r.a[4] = ether_data + ed_ReadPacket;      // Pointer to ReadPacket/ReadRest routines
		D(bug(" calling protocol handler %08lx, type %08lx, length %08lx, data %08lx, rha %08lx, read_packet %08lx\r\n", prot->handler, r.d[0], r.d[1], r.a[0], r.a[3], r.a[4]));
		Execute68k(prot->handler, &r);
	}

	// Acknowledge interrupt to reception thread
	D(bug(" EtherIRQ done\r\n"));
	ReleaseSemaphore(int_ack,1,NULL);
}

#if DEBUG
#pragma optimize("",on)
#endif
