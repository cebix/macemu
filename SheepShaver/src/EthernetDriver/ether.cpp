/*
 *  ether.cpp - SheepShaver Ethernet Device Driver (DLPI)
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

/*
 * TODO
 * - 802.2 TEST/XID
 * - MIB statistics
 */

#include <string.h>

#include "sysdeps.h"
#include "cpu_emulation.h"
#include "ether.h"
#include "ether_defs.h"
#include "macos_util.h"

#define DEBUG 0
#include "debug.h"

// Packet types
enum {
	kPktDIX				= 0,
	kPkt8022SAP			= 1,
	kPkt8022GroupSAP	= 2,
	kPkt8022SNAP		= 3,
	kPktIPX				= 4,
	kPktUnknown			= 5
};


/*
 *  Stream private data structure
 */

static const int kGroupSAPMapSize = 128/32;	// Number of 32-bit values we need for 128 bits	
static const int kGSshift = 6;
static const int kGSmask = 0x1F;

struct multicast_node {
	nw_multicast_node_p next;
	uint8 addr[kEnetPhysicalAddressLength];
};

struct DLPIStream {
	void SetGroupSAP(uint8 sap) 
	{
		group_sap[sap >> kGSshift] |= (1L << ((sap >> 1) & kGSmask));
	}

	void ClearGroupSAP(uint8 sap)
	{
		group_sap[sap >> kGSshift] &= ~(1L << ((sap >> 1) & kGSmask));
	}

	void ClearAllGroupSAPs(void)
	{
		for (int i=0; i<kGroupSAPMapSize; i++)
			group_sap[i] = 0;
	}

	bool TestGroupSAP(uint8 sap) 
	{
		return group_sap[sap >> kGSshift] & (1L << ((sap >> 1) & kGSmask));
	}

	void AddMulticast(uint8 *addr)
	{
		multicast_node *n = (multicast_node *)Mac2HostAddr(Mac_sysalloc(sizeof(multicast_node)));
		memcpy(n->addr, addr, kEnetPhysicalAddressLength);
		n->next = multicast_list;
		multicast_list = n;
	}

	void RemoveMulticast(uint8 *addr)
	{
		multicast_node *p = multicast_list;
		while (p) {
			if (memcmp(addr, p->addr, kEnetPhysicalAddressLength) == 0)
				goto found;
			p = p->next;
		}
		return;
	found:
		multicast_node *q = (multicast_node *)&multicast_list;
		while (q) {
			if (q->next == p) {
				q->next = p->next;
				Mac_sysfree(Host2MacAddr((uint8 *)p));
				return;
			}
			q = q->next;
		}
	}

	uint8 *IsMulticastRegistered(uint8 *addr)
	{
		multicast_node *n = multicast_list;
		while (n) {
			if (memcmp(addr, n->addr, kEnetPhysicalAddressLength) == 0)
				return n->addr;
			n = n->next;
		}
		return NULL;
	}

	nw_uint32 minor_num;					// Minor device number of this stream
	nw_uint32 dlpi_state;					// DLPI state of this stream
	nw_uint32 flags;						// Flags
	nw_uint16 dlsap;						// SAP bound to this stream
	nw_bool framing_8022;					// Using 802.2 framing? This is only used to report the MAC type for DL_INFO_ACK and can be set with an ioctl() call
	nw_queue_p rdq;							// Read queue for this stream
	nw_uint32 group_sap[kGroupSAPMapSize];	// Map of bound group SAPs
	uint8 snap[k8022SNAPLength];			// SNAP bound to this stream
	nw_multicast_node_p multicast_list;		// List of enabled multicast addresses
};

// Hack to make DLPIStream list initialization early to NULL (do we really need this?)
struct DLPIStreamInit {
	DLPIStreamInit(nw_DLPIStream_p *dlpi_stream_p) { *dlpi_stream_p = NULL; }
};

// Stream flags
enum {
	kSnapStream				= 0x00000001,
	kAcceptMulticasts		= 0x00000002,
	kAcceptAll8022Packets	= 0x00000004,
	kFastPathMode			= 0x00000008
};

// List of opened streams (used internally by OpenTransport)
static nw_DLPIStream_p dlpi_stream_list;
static DLPIStreamInit dlpi_stream_init(&dlpi_stream_list);

// Are we open?
bool ether_driver_opened = false;

// Our ethernet hardware address
static uint8 hardware_address[6] = {0, 0, 0, 0, 0, 0};

// Statistics
int32 num_wput = 0;
int32 num_error_acks = 0;
int32 num_tx_packets = 0;
int32 num_tx_raw_packets = 0;
int32 num_tx_normal_packets = 0;
int32 num_tx_buffer_full = 0;
int32 num_rx_packets = 0;
int32 num_ether_irq = 0;
int32 num_unitdata_ind = 0;
int32 num_rx_fastpath = 0;
int32 num_rx_no_mem = 0;
int32 num_rx_dropped = 0;
int32 num_rx_stream_not_ready = 0;
int32 num_rx_no_unitdata_mem = 0;


// Function pointers of imported functions
typedef mblk_t *(*allocb_ptr)(size_t size, int pri);
static uint32 allocb_tvect = 0;
mblk_t *allocb(size_t arg1, int arg2)
{
	return (mblk_t *)Mac2HostAddr((uint32)CallMacOS2(allocb_ptr, allocb_tvect, arg1, arg2));
}
typedef void (*freeb_ptr)(mblk_t *);
static uint32 freeb_tvect = 0;
static inline void freeb(mblk_t *arg1)
{
	CallMacOS1(freeb_ptr, freeb_tvect, arg1);
}
typedef int16 (*freemsg_ptr)(mblk_t *);
static uint32 freemsg_tvect = 0;
static inline int16 freemsg(mblk_t *arg1)
{
	return (int16)CallMacOS1(freemsg_ptr, freemsg_tvect, arg1);
}
typedef mblk_t *(*copyb_ptr)(mblk_t *);
static uint32 copyb_tvect = 0;
static inline mblk_t *copyb(mblk_t *arg1)
{
	return (mblk_t *)Mac2HostAddr((uint32)CallMacOS1(copyb_ptr, copyb_tvect, arg1));
}
typedef mblk_t *(*dupmsg_ptr)(mblk_t *);
static uint32 dupmsg_tvect = 0;
static inline mblk_t *dupmsg(mblk_t *arg1)
{
	return (mblk_t *)Mac2HostAddr((uint32)CallMacOS1(dupmsg_ptr, dupmsg_tvect, arg1));
}
typedef mblk_t *(*getq_ptr)(queue_t *);
static uint32 getq_tvect = 0;
static inline mblk_t *getq(queue_t *arg1)
{
	return (mblk_t *)Mac2HostAddr((uint32)CallMacOS1(getq_ptr, getq_tvect, arg1));
}
typedef int (*putq_ptr)(queue_t *, mblk_t *);
static uint32 putq_tvect = 0;
static inline int putq(queue_t *arg1, mblk_t *arg2)
{
	return (int)CallMacOS2(putq_ptr, putq_tvect, arg1, arg2);
}
typedef int (*putnext_ptr)(queue_t *,  mblk_t *);
static uint32 putnext_tvect = 0;
static inline int putnext(queue_t *arg1, mblk_t *arg2)
{
	return (int)CallMacOS2(putnext_ptr, putnext_tvect, arg1, arg2);
}
typedef int (*putnextctl1_ptr)(queue_t *, int type, int c);
static uint32 putnextctl1_tvect = 0;
static inline int putnextctl1(queue_t *arg1, int arg2, int arg3)
{
	return (int)CallMacOS3(putnextctl1_ptr, putnextctl1_tvect, arg1, arg2, arg3);
}
typedef int (*canputnext_ptr)(queue_t *);
static uint32 canputnext_tvect = 0;
static inline int canputnext(queue_t *arg1)
{
	return (int)CallMacOS1(canputnext_ptr, canputnext_tvect, arg1);
}
typedef int (*qreply_ptr)(queue_t *, mblk_t *);
static uint32 qreply_tvect = 0;
static inline int qreply(queue_t *arg1, mblk_t *arg2)
{
	return (int)CallMacOS2(qreply_ptr, qreply_tvect, arg1, arg2);
}
typedef void (*flushq_ptr)(queue_t *, int flag);
static uint32 flushq_tvect = 0;
static inline void flushq(queue_t *arg1, int arg2)
{
	CallMacOS2(flushq_ptr, flushq_tvect, arg1, arg2);
}
typedef int (*msgdsize_ptr)(const mblk_t *);
static uint32 msgdsize_tvect = 0;
static inline int msgdsize(const mblk_t *arg1)
{
	return (int)CallMacOS1(msgdsize_ptr, msgdsize_tvect, arg1);
}
typedef void (*otenterint_ptr)(void);
static uint32 otenterint_tvect = 0;
void OTEnterInterrupt(void)
{
	CallMacOS(otenterint_ptr, otenterint_tvect);
}
typedef void (*otleaveint_ptr)(void);
static uint32 otleaveint_tvect = 0;
void OTLeaveInterrupt(void)
{
	CallMacOS(otleaveint_ptr, otleaveint_tvect);
}
typedef int (*mi_open_comm_ptr)(DLPIStream **mi_opp_orig, size_t size, queue_t *q, void *dev, int flag, int sflag, void *credp);
static uint32 mi_open_comm_tvect = 0;
static inline int mi_open_comm(DLPIStream **arg1, size_t arg2, queue_t *arg3, void *arg4, int arg5, int arg6, void *arg7)
{
	return (int)CallMacOS7(mi_open_comm_ptr, mi_open_comm_tvect, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}
typedef int (*mi_close_comm_ptr)(DLPIStream **mi_opp_orig, queue_t *q);
static uint32 mi_close_comm_tvect = 0;
static inline int mi_close_comm(DLPIStream **arg1, queue_t *arg2)
{
	return (int)CallMacOS2(mi_close_comm_ptr, mi_close_comm_tvect, arg1, arg2);
}
typedef DLPIStream *(*mi_next_ptr_ptr)(DLPIStream *);
static uint32 mi_next_ptr_tvect = 0;
static inline DLPIStream *mi_next_ptr(DLPIStream *arg1)
{
	return (DLPIStream *)Mac2HostAddr((uint32)CallMacOS1(mi_next_ptr_ptr, mi_next_ptr_tvect, arg1));
}
#ifdef USE_ETHER_FULL_DRIVER
typedef void (*ether_dispatch_packet_ptr)(uint32 p, uint32 size);
static uint32 ether_dispatch_packet_tvect = 0;
#endif

// Prototypes
static void ether_ioctl(DLPIStream *the_stream, queue_t* q, mblk_t* mp);
static void ether_flush(queue_t* q, mblk_t* mp);
static mblk_t *build_tx_packet_header(DLPIStream *the_stream, mblk_t *mp, bool fast_path);
static void transmit_packet(mblk_t *mp);
static void DLPI_error_ack(DLPIStream *the_stream, queue_t *q, mblk_t *ack_mp, uint32 prim, uint32 err, uint32 uerr);
static void DLPI_ok_ack(DLPIStream *the_stream, queue_t *q, mblk_t *ack_mp, uint32 prim);
static void DLPI_info(DLPIStream *the_stream, queue_t *q, mblk_t *mp);
static void DLPI_phys_addr(DLPIStream *the_stream, queue_t *q, mblk_t *mp);
static void DLPI_bind(DLPIStream *the_stream, queue_t *q, mblk_t *mp);
static void DLPI_unbind(DLPIStream *the_stream, queue_t *q, mblk_t *mp);
static void DLPI_subs_bind(DLPIStream *the_stream, queue_t *q, mblk_t *mp);
static void DLPI_subs_unbind(DLPIStream *the_stream, queue_t *q, mblk_t *mp);
static void DLPI_enable_multi(DLPIStream *the_stream, queue_t *q, mblk_t *mp);
static void DLPI_disable_multi(DLPIStream *the_stream, queue_t *q, mblk_t *mp);
static void DLPI_unit_data(DLPIStream *the_stream, queue_t *q, mblk_t *mp);


/*
 *  Initialize ethernet stream module
 */

static uint8 InitStreamModuleImpl(void *theID)
{
	D(bug("InitStreamModule\n"));

	// Don't re-open if already open
	if (ether_driver_opened)
		return true;
	ether_driver_opened = false;

	// Import functions from OTKernelLib
	allocb_tvect = FindLibSymbol("\013OTKernelLib", "\006allocb");
	D(bug("allocb TVECT at %08lx\n", allocb_tvect));
	if (allocb_tvect == 0)
		return false;
	freeb_tvect = FindLibSymbol("\013OTKernelLib", "\005freeb");
	D(bug("freeb TVECT at %08lx\n", freeb_tvect));
	if (freeb_tvect == 0)
		return false;
	freemsg_tvect = FindLibSymbol("\013OTKernelLib", "\007freemsg");
	D(bug("freemsg TVECT at %08lx\n", freemsg_tvect));
	if (freemsg_tvect == 0)
		return false;
	copyb_tvect = FindLibSymbol("\013OTKernelLib", "\005copyb");
	D(bug("copyb TVECT at %08lx\n", copyb_tvect));
	if (copyb_tvect == 0)
		return false;
	dupmsg_tvect = FindLibSymbol("\013OTKernelLib", "\006dupmsg");
	D(bug("dupmsg TVECT at %08lx\n", dupmsg_tvect));
	if (dupmsg_tvect == 0)
		return false;
	getq_tvect = FindLibSymbol("\013OTKernelLib", "\004getq");
	D(bug("getq TVECT at %08lx\n", getq_tvect));
	if (getq_tvect == 0)
		return false;
	putq_tvect = FindLibSymbol("\013OTKernelLib", "\004putq");
	D(bug("putq TVECT at %08lx\n", putq_tvect));
	if (putq_tvect == 0)
		return false;
	putnext_tvect = FindLibSymbol("\013OTKernelLib", "\007putnext");
	D(bug("putnext TVECT at %08lx\n", putnext_tvect));
	if (putnext_tvect == 0)
		return false;
	putnextctl1_tvect = FindLibSymbol("\013OTKernelLib", "\013putnextctl1");
	D(bug("putnextctl1 TVECT at %08lx\n", putnextctl1_tvect));
	if (putnextctl1_tvect == 0)
		return false;
	canputnext_tvect = FindLibSymbol("\013OTKernelLib", "\012canputnext");
	D(bug("canputnext TVECT at %08lx\n", canputnext_tvect));
	if (canputnext_tvect == 0)
		return false;
	qreply_tvect = FindLibSymbol("\013OTKernelLib", "\006qreply");
	D(bug("qreply TVECT at %08lx\n", qreply_tvect));
	if (qreply_tvect == 0)
		return false;
	flushq_tvect = FindLibSymbol("\013OTKernelLib", "\006flushq");
	D(bug("flushq TVECT at %08lx\n", flushq_tvect));
	if (flushq_tvect == 0)
		return false;
	msgdsize_tvect = FindLibSymbol("\013OTKernelLib", "\010msgdsize");
	D(bug("msgdsize TVECT at %08lx\n", msgdsize_tvect));
	if (msgdsize_tvect == 0)
		return false;
	otenterint_tvect = FindLibSymbol("\017OTKernelUtilLib", "\020OTEnterInterrupt");
	D(bug("OTEnterInterrupt TVECT at %08lx\n", otenterint_tvect));
	if (otenterint_tvect == 0)
		return false;
	otleaveint_tvect = FindLibSymbol("\017OTKernelUtilLib", "\020OTLeaveInterrupt");
	D(bug("OTLeaveInterrupt TVECT at %08lx\n", otleaveint_tvect));
	if (otleaveint_tvect == 0)
		return false;
	mi_open_comm_tvect = FindLibSymbol("\013OTKernelLib", "\014mi_open_comm");
	D(bug("mi_open_comm TVECT at %08lx\n", mi_open_comm_tvect));
	if (mi_open_comm_tvect == 0)
		return false;
	mi_close_comm_tvect = FindLibSymbol("\013OTKernelLib", "\015mi_close_comm");
	D(bug("mi_close_comm TVECT at %08lx\n", mi_close_comm_tvect));
	if (mi_close_comm_tvect == 0)
		return false;
	mi_next_ptr_tvect = FindLibSymbol("\013OTKernelLib", "\013mi_next_ptr");
	D(bug("mi_next_ptr TVECT at %08lx\n", mi_next_ptr_tvect));
	if (mi_next_ptr_tvect == 0)
		return false;

#ifndef USE_ETHER_FULL_DRIVER
	// Initialize stream list (which might be leftover)
	dlpi_stream_list = NULL;

	// Ask add-on for ethernet hardware address
	AO_get_ethernet_address(Host2MacAddr(hardware_address));
#endif

	// Yes, we're open
	ether_driver_opened = true;
	return true;
}

uint8 InitStreamModule(void *theID)
{
	// Common initialization code
	bool net_open = InitStreamModuleImpl(theID);

	// Call InitStreamModule() in native side
#ifdef BUILD_ETHER_FULL_DRIVER
	extern bool NativeInitStreamModule(void *);
	if (!NativeInitStreamModule((void *)ether_dispatch_packet))
		net_open = false;
#endif

	// Import functions from the Ethernet driver
#ifdef USE_ETHER_FULL_DRIVER
	ether_dispatch_packet_tvect = (uintptr)theID;
	D(bug("ether_dispatch_packet TVECT at %08lx\n", ether_dispatch_packet_tvect));
	if (ether_dispatch_packet_tvect == 0)
		net_open = false;
#endif

	return net_open;
}


/*
 *  Terminate ethernet stream module
 */

static void TerminateStreamModuleImpl(void)
{
	D(bug("TerminateStreamModule\n"));

#ifndef USE_ETHER_FULL_DRIVER
	// This happens sometimes. I don't know why.
	if (dlpi_stream_list != NULL)
		printf("FATAL: TerminateStreamModule() called, but streams still open\n");
#endif

	// Sorry, we're closed
	ether_driver_opened = false;
}

void TerminateStreamModule(void)
{
	// Common termination code
	TerminateStreamModuleImpl();

	// Call TerminateStreamModule() in native side
#ifdef BUILD_ETHER_FULL_DRIVER
	extern void NativeTerminateStreamModule(void);
	NativeTerminateStreamModule();
#endif
}


/*
 *  Open new stream
 */

int ether_open(queue_t *rdq, void *dev, int flag, int sflag, void *creds)
{
	D(bug("ether_open(%p,%p,%d,%d,%p)\n", rdq, dev, flag, sflag, creds));

	// Return if driver was closed
	if (!ether_driver_opened) {
		printf("FATAL: ether_open(): Ethernet driver not opened\n");
		return MAC_ENXIO;
	}

	// If we're being reopened, just return
	if (rdq->q_ptr != NULL)
		return 0;

	// Allocate DLPIStream structure
	int err = mi_open_comm((DLPIStream **)&dlpi_stream_list, sizeof(DLPIStream), rdq, dev, flag, sflag, creds);
	if (err)
		return err;
	DLPIStream *the_stream = (DLPIStream *)rdq->q_ptr;
	the_stream->rdq = rdq;
	the_stream->dlpi_state = DL_UNBOUND;
	the_stream->flags = 0;
	the_stream->dlsap = 0;
	the_stream->framing_8022 = false;
	the_stream->multicast_list = NULL;
	return 0;
}


/*
 *  Close stream
 */

int ether_close(queue_t *rdq, int flag, void *creds)
{
	D(bug("ether_close(%p,%d,%p)\n", rdq, flag, creds));

	// Return if driver was closed
	if (!ether_driver_opened) {
		printf("FATAL: ether_close(): Ethernet driver not opened\n");
		return MAC_ENXIO;
	}

	// Get stream
	DLPIStream *the_stream = (DLPIStream *)rdq->q_ptr;

	// Don't close if never opened
	if (the_stream == NULL)
		return 0;

	// Disable all registered multicast addresses
	while (the_stream->multicast_list) {
		AO_disable_multicast(Host2MacAddr(the_stream->multicast_list->addr));
		the_stream->RemoveMulticast(the_stream->multicast_list->addr);
	}
	the_stream->multicast_list = NULL;

	// Delete the DLPIStream
	return mi_close_comm((DLPIStream **)&dlpi_stream_list, rdq);
}


/*
 *  Put something on the write queue
 */

int ether_wput(queue_t *q, mblk_t *mp)
{
	D(bug("ether_wput(%p,%p)\n", q, mp));

	// Return if driver was closed
	if (!ether_driver_opened) {
		printf("FATAL: ether_wput(): Ethernet driver not opened\n");
		return MAC_ENXIO;
	}

	// Get stream
	DLPIStream *the_stream = (DLPIStream *)q->q_ptr;
	if (the_stream == NULL)
		return MAC_ENXIO;

	D(bug(" db_type %d\n", (int)mp->b_datap->db_type));
	switch (mp->b_datap->db_type) {

		case M_DATA:
			// Transmit raw packet
			D(bug(" raw packet\n"));
			num_tx_raw_packets++;
			transmit_packet(mp);
			break;

		case M_PROTO:
		case M_PCPROTO: {
			union DL_primitives *dlp = (union DL_primitives *)(void *)mp->b_rptr;
			uint32 prim = dlp->dl_primitive;
			D(bug(" dl_primitive %d\n", prim));
			switch (prim) {
				case DL_UNITDATA_REQ:
					// Transmit normal packet
					num_tx_normal_packets++;
					DLPI_unit_data(the_stream, q, mp);
					break;

				case DL_INFO_REQ:
					DLPI_info(the_stream, q, mp);
					break;
	
				case DL_PHYS_ADDR_REQ:
					DLPI_phys_addr(the_stream, q, mp);
					break;
	
				case DL_BIND_REQ:
					DLPI_bind(the_stream, q, mp);
					break;
	
				case DL_UNBIND_REQ:
					DLPI_unbind(the_stream, q, mp);
					break;
	
				case DL_SUBS_BIND_REQ:
					DLPI_subs_bind(the_stream, q, mp);
					break;
	
				case DL_SUBS_UNBIND_REQ:
					DLPI_subs_unbind(the_stream, q, mp);
					break;
	
				case DL_ENABMULTI_REQ:
					DLPI_enable_multi(the_stream, q, mp);
					break;
	
				case DL_DISABMULTI_REQ:
					DLPI_disable_multi(the_stream, q, mp);
					break;
	
				default:
					D(bug("WARNING: ether_wsrv(): Unknown primitive\n"));
					DLPI_error_ack(the_stream, q, mp, prim, DL_NOTSUPPORTED, 0);
					break;
			}
			break;
		}

		case M_IOCTL:
			ether_ioctl(the_stream, q, mp);
			break;

		case M_FLUSH:
			ether_flush(q, mp);
			break;

		default:
			D(bug("WARNING: ether_wput(): Unknown message type\n"));
			freemsg(mp);
			break;
	}
	num_wput++;
	return 0;
}


/*
 *  Dequeue and process messages from the read queue
 */

int ether_rsrv(queue_t *q)
{
	mblk_t *mp;
	while ((mp = getq(q)) != NULL) {
		if (canputnext(q))
			putnext(q, mp);
		else {
			freemsg(mp);
			flushq(q, FLUSHDATA);
			break;
		}
	}
	return 0;
}


/*
 *  Handle ioctl calls
 */

static void ether_ioctl(DLPIStream *the_stream, queue_t *q, mblk_t *mp)
{
	struct iocblk *ioc = (struct iocblk *)(void *)mp->b_rptr;
	D(bug(" ether_ioctl(%p,%p) cmd %d\n", q, mp, (int)ioc->ioc_cmd));

	switch (ioc->ioc_cmd) {

		case I_OTSetFramingType: {	// Toggles what the general info primitive returns for dl_mac_type in dl_info_ack_t structure
			mblk_t *info_mp = mp->b_cont;
			if (info_mp == NULL || ((info_mp->b_wptr - info_mp->b_rptr) != sizeof(uint32))) {
				ioc->ioc_error = MAC_EINVAL;
				goto ioctl_error;
			}
			uint32 framing_type = ntohl(*(uint32 *)(void *)info_mp->b_rptr);
			D(bug("  I_OTSetFramingType type %d\n", framing_type));
			if (framing_type != kOTGetFramingValue)
				the_stream->framing_8022 = (framing_type == kOTFraming8022);
			mp->b_cont = NULL;
			freemsg(info_mp);
			if (the_stream->framing_8022)
				ioc->ioc_rval = kOTFraming8022;
			else
				ioc->ioc_rval = kOTFramingEthernet;
			goto ioctl_ok;
		}

		case DL_IOC_HDR_INFO: {		// Special Mentat call, for fast transmits
			D(bug("  DL_IOC_HDR_INFO\n"));
			mblk_t *info_mp = mp->b_cont;

			// Copy DL_UNITDATA_REQ block
			mblk_t *unitdata_mp = copyb(info_mp);
			if (unitdata_mp == NULL) {
				ioc->ioc_error = MAC_ENOMEM;
				goto ioctl_error;
			}
			unitdata_mp->b_datap->db_type = M_PROTO;

			// Construct header (converts DL_UNITDATA_REQ -> M_DATA)
			mblk_t *header_mp = build_tx_packet_header(the_stream, unitdata_mp, true);
			
			if (header_mp == NULL) {
				// Could not allocate a message block large enough
				ioc->ioc_error = MAC_ENOMEM;
				goto ioctl_error;
			}

			// Attach header block at the end
			mp->b_cont->b_cont = header_mp;
			the_stream->flags |= kFastPathMode;
			goto ioctl_ok;
		}

		case I_OTSetRawMode: {
			mblk_t *info_mp = mp->b_cont;
			dl_recv_control_t *dlrc;
			if (info_mp == NULL || ((info_mp->b_wptr - info_mp->b_rptr) != sizeof(dlrc->dl_primitive))) {
				ioc->ioc_error = MAC_EINVAL;
				goto ioctl_error;
			}
			dlrc = (dl_recv_control_t *)(void *)info_mp->b_rptr;
			D(bug("  I_OTSetRawMode primitive %d\n", (int)dlrc->dl_primitive));
			ioc->ioc_error = MAC_EINVAL;
			goto ioctl_error;
		}

		default:
			D(bug("WARNING: Unknown ether_ioctl() call\n"));
			ioc->ioc_error = MAC_EINVAL;
			goto ioctl_error;
	}

ioctl_ok:
	ioc->ioc_count = 0;
	for (mblk_t *mp1 = mp; (mp1 = mp1->b_cont) != NULL;)
		ioc->ioc_count += mp1->b_wptr - mp1->b_rptr;
	ioc->ioc_error = 0;
	mp->b_datap->db_type = M_IOCACK;
	qreply(q, mp);
	return;

ioctl_error:
	mp->b_datap->db_type = M_IOCNAK;
	qreply(q, mp);
	return;
}


/*
 *  Flush call, send it up to the read side of the stream
 */

static void ether_flush(queue_t* q, mblk_t* mp)
{
	D(bug(" ether_flush(%p,%p)\n", q, mp));

	uint8 *rptr = mp->b_rptr;
	if (*rptr & FLUSHW)
		flushq(q, FLUSHALL);
	if (*rptr & FLUSHR) {
		flushq(RD(q), FLUSHALL);
		*rptr &= ~FLUSHW;
		qreply(q, mp);
	} else
		freemsg(mp);
}


/*
 *  Classify packet into the different types of protocols
 */

static uint16 classify_packet_type(uint16 primarySAP, uint16 secondarySAP)
{
	if (primarySAP >= kMinDIXSAP) 		
		return kPktDIX;

	if ((primarySAP == kIPXSAP) && (secondarySAP == kIPXSAP))
		return kPktIPX;

	if (primarySAP == kSNAPSAP)
		return kPkt8022SNAP;

	if (primarySAP <= k8022GlobalSAP)
		return kPkt8022SAP;

	return kPktUnknown;
}


/*
 *  Check if the address is a multicast, broadcast or standard address
 */

static int32 get_address_type(uint8 *addr)
{
	if (addr[0] & 1) { 	// Multicast/broadcast flag
		if (OTIs48BitBroadcastAddress(addr))
			return keaBroadcast;
		else
			return keaMulticast;
	} else
		return keaStandardAddress;
}


/*
 *  Reuse a message block, make room for more data
 */

static mblk_t *reuse_message_block(mblk_t *mp, uint16 needed_size)
{
	mblk_t *nmp;

	if ((mp->b_datap->db_ref == 1) && ((mp->b_datap->db_lim - mp->b_datap->db_base) >= needed_size)) {
		mp->b_datap->db_type = M_DATA;
		mp->b_rptr = mp->b_datap->db_base;
		mp->b_wptr = mp->b_datap->db_base + needed_size;
	} else {
		nmp = mp->b_cont; 	// Grab the M_DATA blocks
		mp->b_cont = NULL; 	// Detach the M_(PC)PROTO
		freemsg(mp);		// Free the M_(PC)PROTO
		mp = nmp;			// Point to the M_DATA blocks
	
		// Try to get space on the first M_DATA block
		if (mp && (mp->b_datap->db_ref == 1) && ((mp->b_rptr - mp->b_datap->db_base) >= needed_size))
			mp->b_rptr -= needed_size;
		else {
			// Try to allocate a new message
			if ((nmp = allocb(needed_size, BPRI_HI)) == NULL) {
				// Could not get a new message block so lets forget about the message altogether
				freemsg(mp); 			// Free the original M_DATA portion of the message
				mp = NULL; 				// Indicates the reuse failed
			} else {
				nmp->b_cont = mp;		// Attach the new message block as the head
				nmp->b_wptr += needed_size;
				mp = nmp; 				
			} 
		}
	}

	return mp;
}


/*
 *  Built header for packet to be transmitted (convert DL_UNITDATA_REQ -> M_DATA)
 *  The passed-in message has the header info in the first message block and the data
 *  in the following blocks
 */

static mblk_t *build_tx_packet_header(DLPIStream *the_stream, mblk_t *mp, bool fast_path)
{
	// Only handle unit_data requests
	dl_unitdata_req_t *req = (dl_unitdata_req_t *)(void *)mp->b_rptr;
	if (req->dl_primitive != DL_UNITDATA_REQ) {
		freemsg(mp);
		return NULL;
	}

	// Extract destination address and its length
	uint8 *destAddrOrig = ((uint8 *)req) + req->dl_dest_addr_offset;
	uint32 destAddrLen = req->dl_dest_addr_length;
	uint8 ctrl = 0x03;

	// Extract DLSAP
	uint16 dlsap;
	switch (destAddrLen) {
		case kEnetPhysicalAddressLength:	
			dlsap = the_stream->dlsap;
			break;
		case kEnetAndSAPAddressLength:	
			dlsap = ntohs(*(uint16 *)(destAddrOrig + kEnetPhysicalAddressLength));
			break;
		case kEnetPhysicalAddressLength + k8022DLSAPLength + k8022SNAPLength:	// SNAP SAP
			dlsap = ntohs(*(uint16 *)(destAddrOrig + kEnetPhysicalAddressLength));
			break;
		default:
			dlsap = the_stream->dlsap;
			break;
	}

	// Extract data size (excluding header info) and packet type
	uint16 datasize = msgdsize(mp);
	uint16 packetType = classify_packet_type(the_stream->dlsap, dlsap);

	// Calculate header size and protocol type/size field
	uint16 hdrsize, proto;
	switch (packetType) {
		case kPktDIX:
			hdrsize = kEnetPacketHeaderLength; 
			proto = dlsap;
			break;
		case kPkt8022SAP:
			hdrsize = kEnetPacketHeaderLength + k8022BasicHeaderLength;
			if (fast_path)
				proto = 0;
			else
				proto = datasize + k8022BasicHeaderLength;
			break;
		case kPkt8022SNAP:
			hdrsize = kEnetPacketHeaderLength + k8022SNAPHeaderLength;
			if (fast_path)
				proto = 0;		
			else
				proto = datasize + k8022SNAPHeaderLength;
			break;
		case kPktIPX:
			hdrsize = kEnetPacketHeaderLength;
			if (fast_path)
				proto = 0;
			else
				proto = datasize;		
			break;
		default:
			hdrsize = kEnetPacketHeaderLength; 
			proto = dlsap;
			break;
	}

	// We need to copy the dest address info in the message before we can reuse it
	uint8 destAddrCopy[kMaxBoundAddrLength];
	memcpy(destAddrCopy, destAddrOrig, destAddrLen);

	// Resize header info in message block
	if ((mp = reuse_message_block(mp, hdrsize)) == NULL)
		return NULL;
	struct T8022FullPacketHeader *packetHeader = (struct T8022FullPacketHeader *)(void *)mp->b_rptr;

	// Set protocol type/size field
	packetHeader->fEnetPart.fProto = proto;

	// Set destination ethernet address
	OTCopy48BitAddress(destAddrCopy, packetHeader->fEnetPart.fDestAddr);

	// Set other header fields
	switch (packetType) {
		case kPkt8022SAP:
			packetHeader->f8022Part.fDSAP = (uint8)dlsap;
			packetHeader->f8022Part.fSSAP = (uint8)the_stream->dlsap;
			packetHeader->f8022Part.fCtrl = ctrl;
			break;
		case kPkt8022SNAP: {
			uint8 *snapStart;
			packetHeader->f8022Part.fDSAP = (uint8)dlsap;
			packetHeader->f8022Part.fSSAP = (uint8)the_stream->dlsap;
			packetHeader->f8022Part.fCtrl = ctrl;
			if (destAddrLen >= kEnetAndSAPAddressLength + k8022SNAPLength)
				snapStart = destAddrCopy + kEnetAndSAPAddressLength;
			else
				snapStart = the_stream->snap;
			OTCopy8022SNAP(snapStart, packetHeader->f8022Part.fSNAP);
			break;
		}
	}

	// Return updated message
	return mp;
}


/*
 *  Transmit packet
 */

static void transmit_packet(mblk_t *mp)
{
	EnetPacketHeader *enetHeader = (EnetPacketHeader *)(void *)mp->b_rptr;

	// Fill in length in 802.3 packets
	if (enetHeader->fProto == 0)
		enetHeader->fProto = msgdsize(mp) - sizeof(EnetPacketHeader);

	// Fill in ethernet source address
	OTCopy48BitAddress(hardware_address, enetHeader->fSourceAddr); 

	// Tell add-on to transmit packet
	AO_transmit_packet(Host2MacAddr((uint8 *)mp));
	freemsg(mp);
}


/*
 *  Handle incoming packet (one stream), construct DL_UNITDATA_IND message
 */

static void handle_received_packet(DLPIStream *the_stream, mblk_t *mp, uint16 packet_type, int32 dest_addr_type)
{
	// Find address and header length
	uint32 addr_len;
	uint32 header_len;
	switch (packet_type) {
		case kPkt8022SAP:
			addr_len = kEnetAndSAPAddressLength;
			header_len = kEnetPacketHeaderLength + k8022BasicHeaderLength;
			break;
		case kPkt8022SNAP:
			addr_len = kEnetAndSAPAddressLength + k8022SNAPLength;
			header_len = kEnetPacketHeaderLength + k8022SNAPHeaderLength;
			break;
		default:	// DIX and IPX
			addr_len = kEnetAndSAPAddressLength;
			header_len = kEnetPacketHeaderLength;
			break;
	}

	// In Fast Path mode, don't send DL_UNITDATA_IND messages for unicast packets
	if ((the_stream->flags & kFastPathMode) && dest_addr_type == keaStandardAddress) {
		mp->b_rptr += header_len;
		num_rx_fastpath++;
		putq(the_stream->rdq, mp);
		return;
	}

	// Allocate the dl_unitdata_ind_t message
	mblk_t *nmp;
	if ((nmp = allocb(sizeof(dl_unitdata_ind_t) + 2*addr_len, BPRI_HI)) == NULL) {
		freemsg(mp);
		num_rx_no_unitdata_mem++;
		return;
	}

	// Set message type
	nmp->b_datap->db_type = M_PROTO;
	dl_unitdata_ind_t *ind = (dl_unitdata_ind_t*)(void *)nmp->b_rptr;
	ind->dl_primitive = DL_UNITDATA_IND;
	nmp->b_wptr += (sizeof(dl_unitdata_ind_t) + 2*addr_len);

	// Link M_DATA block
	nmp->b_cont = mp;

	// Set address fields
	ind->dl_dest_addr_length = addr_len;
	ind->dl_dest_addr_offset = sizeof(dl_unitdata_ind_t);
	ind->dl_src_addr_length = addr_len;
	ind->dl_src_addr_offset = sizeof(dl_unitdata_ind_t) + addr_len;

	// Set address type
	ind->dl_group_address = dest_addr_type;

	// Set address fields
	T8022FullPacketHeader *packetHeader = (T8022FullPacketHeader *)(void *)mp->b_rptr;
	T8022AddressStruct *destAddr = ((T8022AddressStruct*)(nmp->b_rptr + ind->dl_dest_addr_offset));
	T8022AddressStruct *srcAddr = ((T8022AddressStruct*)(nmp->b_rptr + ind->dl_src_addr_offset));

	OTCopy48BitAddress(packetHeader->fEnetPart.fDestAddr, destAddr->fHWAddr);
	OTCopy48BitAddress(packetHeader->fEnetPart.fSourceAddr, srcAddr->fHWAddr);

	destAddr->fSAP = packetHeader->f8022Part.fDSAP;
	srcAddr->fSAP = packetHeader->f8022Part.fSSAP;

	if (packet_type == kPkt8022SNAP) {
		OTCopy8022SNAP(packetHeader->f8022Part.fSNAP, destAddr->fSNAP);
		OTCopy8022SNAP(packetHeader->f8022Part.fSNAP, srcAddr->fSNAP);
	}

	// "Hide" the ethernet and protocol header(s)
	mp->b_rptr += header_len;

	// Pass message up the stream
	num_unitdata_ind++;
	putq(the_stream->rdq, nmp);
	return;
}


/*
 *  Packet received, distribute it to the streams that want it
 */

void ether_packet_received(mblk_t *mp)
{
	// Extract address and types
	EnetPacketHeader *pkt = (EnetPacketHeader *)(void *)mp->b_rptr;
	T8022FullPacketHeader *fullpkt = (T8022FullPacketHeader *)pkt;
	uint16 sourceSAP, destSAP;
	destSAP = fullpkt->fEnetPart.fProto;
	if (destSAP >= kMinDIXSAP) {
		// Classic ethernet
		sourceSAP = destSAP;
	} else {
		destSAP = fullpkt->f8022Part.fDSAP;
		sourceSAP = fullpkt->f8022Part.fSSAP;
	}
	uint16 packetType = classify_packet_type(sourceSAP, destSAP);
	int32 destAddressType = get_address_type(pkt->fDestAddr);

	// Look which streams want it
	DLPIStream *the_stream, *found_stream = NULL;
	uint16 found_packetType = 0;
	int32 found_destAddressType = 0;
	for (the_stream = dlpi_stream_list; the_stream != NULL; the_stream = mi_next_ptr(the_stream)) {

		// Don't send to unbound streams
		if (the_stream->dlpi_state == DL_UNBOUND)
			continue;

		// Does this stream want all 802.2 packets?
		if ((the_stream->flags & kAcceptAll8022Packets) && (destSAP <= 0xff))
			goto type_found;

		// No, check SAP/SNAP
		if (destSAP == the_stream->dlsap) {
			if (the_stream->flags & kSnapStream) {
				// Check SNAPs if necessary
				uint8 sum = fullpkt->f8022Part.fSNAP[0] ^ the_stream->snap[0];
				sum |= fullpkt->f8022Part.fSNAP[1] ^ the_stream->snap[1];
				sum |= fullpkt->f8022Part.fSNAP[2] ^ the_stream->snap[2];
				sum |= fullpkt->f8022Part.fSNAP[3] ^ the_stream->snap[3];
				sum |= fullpkt->f8022Part.fSNAP[4] ^ the_stream->snap[4];
				if (sum == 0)
					goto type_found;
			} else {
				// No SNAP, found a match since saps match 
				goto type_found;
			}
		} else {
			// Check for an 802.3 Group/Global (odd) 
			if (((packetType == kPkt8022SAP) || (packetType == kPkt8022SNAP)) && (destSAP & 1) && the_stream->TestGroupSAP(destSAP))
				goto type_found;
		}

		// No stream for this SAP/SNAP found
		continue;

type_found:
		// If it's a multicast packet, it must be in the stream's multicast list
		if ((destAddressType == keaMulticast) && (the_stream->flags & kAcceptMulticasts) && (!the_stream->IsMulticastRegistered(pkt->fDestAddr)))
			continue;

		// Send packet to stream
		// found_stream keeps a pointer to the previously found stream, so that only the last
		// stream gets the original message, the other ones get duplicates
		if (found_stream)
			handle_received_packet(found_stream, dupmsg(mp), found_packetType, found_destAddressType);
		found_stream = the_stream;
		found_packetType = packetType;
		found_destAddressType = destAddressType;
	}

	// Send original message to last found stream
	if (found_stream)
		handle_received_packet(found_stream, mp, found_packetType, found_destAddressType);
	else {
		freemsg(mp);	// Nobody wants it *snief*
		num_rx_dropped++;
	}
}

void ether_dispatch_packet(uint32 p, uint32 size)
{
#ifdef USE_ETHER_FULL_DRIVER
	// Call handler from the Ethernet driver
	D(bug("ether_dispatch_packet\n"));
	D(bug(" packet data at %p, %d bytes\n", p, size));
	CallMacOS2(ether_dispatch_packet_ptr, ether_dispatch_packet_tvect, p, size);
#else
	// Wrap packet in message block
	num_rx_packets++;
	mblk_t *mp;
	if ((mp = allocb(size, 0)) != NULL) {
		D(bug(" packet data at %p\n", (void *)mp->b_rptr));
		Mac2Host_memcpy(mp->b_rptr, p, size);
		mp->b_wptr += size;
		ether_packet_received(mp);
	} else {
		D(bug("WARNING: Cannot allocate mblk for received packet\n"));
		num_rx_no_mem++;
	}
#endif
}


/*
 *  Build and send an error acknowledge
 */

static void DLPI_error_ack(DLPIStream *the_stream, queue_t *q, mblk_t *ack_mp, uint32 prim, uint32 err, uint32 uerr)
{
	D(bug("  DLPI_error_ack(%p,%p) prim %d, err %d, uerr %d\n", the_stream, ack_mp, prim, err, uerr));
	num_error_acks++;

	if (ack_mp != NULL)
		freemsg(ack_mp);
	if ((ack_mp = allocb(sizeof(dl_error_ack_t), BPRI_HI)) == NULL)
		return;

	ack_mp->b_datap->db_type = M_PCPROTO;
	dl_error_ack_t *errp = (dl_error_ack_t *)(void *)ack_mp->b_wptr;
	errp->dl_primitive = DL_ERROR_ACK;
	errp->dl_error_primitive = prim;
	errp->dl_errno = err;
	errp->dl_unix_errno = uerr;
	ack_mp->b_wptr += sizeof(dl_error_ack_t);
	qreply(q, ack_mp);
}


/*
 *  Build and send an OK acknowledge
 */

static void DLPI_ok_ack(DLPIStream *the_stream, queue_t *q, mblk_t *ack_mp, uint32 prim)
{
	if (ack_mp->b_datap->db_ref != 1) {
		// Message already in use, create a new one
		freemsg(ack_mp);
		if ((ack_mp = allocb(sizeof(dl_error_ack_t), BPRI_HI)) == NULL)
			return;
	} else {
		// Message free
		if (ack_mp->b_cont != NULL) {
			freemsg(ack_mp->b_cont);
			ack_mp->b_cont = NULL;
		}
	}

	ack_mp->b_datap->db_type = M_PCPROTO;
	dl_ok_ack_t *ackp = (dl_ok_ack_t *)(void *)ack_mp->b_rptr;
	ackp->dl_primitive = DL_OK_ACK;
	ackp->dl_correct_primitive = prim;
	ack_mp->b_wptr = ack_mp->b_rptr + sizeof(dl_ok_ack_t);
	qreply(q, ack_mp);
}


/*
 *  Handle DL_INFO_REQ (report general information)
 */

static void DLPI_info(DLPIStream *the_stream, queue_t *q, mblk_t *mp)
{
	D(bug("  DLPI_info(%p)\n", the_stream));
	uint32 saplen = 0;
	uint32 addrlen = kEnetPhysicalAddressLength;
	uint32 bcastlen = kEnetPhysicalAddressLength;
	uint32 hdrlen = kEnetPacketHeaderLength;

	// Calculate header length
	if (the_stream->dlpi_state != DL_UNBOUND) {
		saplen = (the_stream->flags & kSnapStream) ? k8022DLSAPLength+k8022SNAPLength : k8022DLSAPLength;
		if (the_stream->dlsap == kSNAPSAP) 	
			hdrlen = kEnetPacketHeaderLength + k8022SNAPHeaderLength;	// SNAP address 
		else if ((the_stream->dlsap <= kMax8022SAP) || (the_stream->dlsap == kIPXSAP))
			hdrlen = kEnetPacketHeaderLength + k8022BasicHeaderLength;	// SAP or IPX 
		else								
			hdrlen = kEnetPacketHeaderLength;							// Basic Ethernet
	}

	// Allocate message block for reply
	mblk_t *ack_mp;
	if ((ack_mp = allocb(sizeof(dl_info_ack_t) + addrlen + saplen + bcastlen, BPRI_LO)) == NULL) {
		DLPI_error_ack(the_stream, q, mp, DL_INFO_REQ, DL_SYSERR, MAC_ENOMEM);
		return;
	}

	// Set up message type
	ack_mp->b_datap->db_type = M_PCPROTO;
	dl_info_ack_t *ackp = (dl_info_ack_t *)(void *)ack_mp->b_rptr;
	ackp->dl_primitive = DL_INFO_ACK;

	// Info/version fields
	ackp->dl_service_mode = DL_CLDLS;
	ackp->dl_provider_style = DL_STYLE1;
	ackp->dl_version = DL_VERSION_2;
	ackp->dl_current_state = the_stream->dlpi_state;
	ackp->dl_mac_type = the_stream->framing_8022 ? DL_CSMACD : DL_ETHER;
	ackp->dl_reserved = 0;
	ackp->dl_qos_length = 0;
	ackp->dl_qos_offset = (uint32)DL_UNKNOWN;
	ackp->dl_qos_range_length = 0;
	ackp->dl_qos_range_offset = (uint32)DL_UNKNOWN;
	ackp->dl_growth = 0;
	ackp->dl_min_sdu = 1;
	ackp->dl_max_sdu = kEnetTSDU - hdrlen;

	// Address fields
	ackp->dl_sap_length = -saplen;		// Negative to indicate sap follows physical address
	ackp->dl_addr_length = addrlen + saplen;
	ackp->dl_addr_offset = sizeof(dl_info_ack_t);
	T8022AddressStruct *boundAddr = ((T8022AddressStruct *)(ack_mp->b_rptr + ackp->dl_addr_offset));
	OTCopy48BitAddress(hardware_address, boundAddr->fHWAddr);
	if (saplen) {
		boundAddr->fSAP = the_stream->dlsap;
		if (the_stream->flags & kSnapStream) 
			OTCopy8022SNAP(the_stream->snap, boundAddr->fSNAP);
	}
	ackp->dl_brdcst_addr_length = bcastlen;
	ackp->dl_brdcst_addr_offset = sizeof(dl_info_ack_t) + addrlen + saplen;
	OTSet48BitBroadcastAddress(ack_mp->b_rptr + ackp->dl_brdcst_addr_offset);	

	// Advance write pointer
	ack_mp->b_wptr += sizeof(dl_info_ack_t) + addrlen + saplen + bcastlen;

	// Free request
	freemsg(mp);

	// Send reply
	qreply(q, ack_mp);
	return;
}


/*
 *  Handle DL_PHYS_ADDR_REQ (report physical address)
 */

static void DLPI_phys_addr(DLPIStream *the_stream, queue_t *q, mblk_t *mp)
{
	D(bug("  DLPI_phys_addr(%p,%p)\n", the_stream, mp));
	dl_phys_addr_req_t *req = (dl_phys_addr_req_t *)(void *)mp->b_rptr;

	// Allocate message block for reply
	mblk_t *ack_mp;
	if ((ack_mp = allocb(sizeof(dl_phys_addr_ack_t) + kEnetPhysicalAddressLength, BPRI_HI)) == NULL) {
		DLPI_error_ack(the_stream, q, mp, DL_PHYS_ADDR_REQ, DL_SYSERR, MAC_ENOMEM);
		return;
	}

	// Set up message type
	ack_mp->b_datap->db_type = M_PCPROTO;
	dl_phys_addr_ack_t *ackp = (dl_phys_addr_ack_t *)(void *)ack_mp->b_wptr;
	ackp->dl_primitive = DL_PHYS_ADDR_ACK;

	// Fill in address
	ackp->dl_addr_length = kEnetPhysicalAddressLength;
	ackp->dl_addr_offset = sizeof(dl_phys_addr_ack_t);
	ack_mp->b_wptr += sizeof(dl_phys_addr_ack_t) + kEnetPhysicalAddressLength;
	if (req->dl_addr_type == DL_CURR_PHYS_ADDR || req->dl_addr_type == DL_FACT_PHYS_ADDR)
		OTCopy48BitAddress(hardware_address, ack_mp->b_rptr + ackp->dl_addr_offset);
	else {
		DLPI_error_ack(the_stream, q, mp, DL_PHYS_ADDR_REQ, DL_BADPRIM, 0);
		return;
	}

	// Free request
	freemsg(mp);

	// Send reply
	qreply(q, ack_mp);
	return;
}


/*
 *  Handle DL_BIND_REQ (bind a stream)
 */

static void DLPI_bind(DLPIStream *the_stream, queue_t *q, mblk_t *mp)
{
	dl_bind_req_t *req = (dl_bind_req_t *)(void *)mp->b_rptr;
	uint32 sap = req->dl_sap;
	D(bug("  DLPI_bind(%p,%p) SAP %04x\n", the_stream, mp, sap));

	// Stream must be unbound
	if (the_stream->dlpi_state != DL_UNBOUND) {
		DLPI_error_ack(the_stream, q, mp, DL_BIND_REQ, DL_OUTSTATE, 0);
		return;
	}

	// We only support connectionless data link services
	if (req->dl_service_mode != DL_CLDLS || req->dl_max_conind != 0) {
		DLPI_error_ack(the_stream, q, mp, DL_BIND_REQ, DL_UNSUPPORTED, 0);
		return;
	}

	// Don't bind to 802.2 group saps, can't check 802.2 global sap (0xFF)
	// because it looks like IPX
	if ((sap <= kMax8022SAP) && (sap & 1)) {
		DLPI_error_ack(the_stream, q, mp, DL_BIND_REQ, DL_BADADDR, 0);
		return;
	}

	if (classify_packet_type(sap, sap) == kPktUnknown) {
		DLPI_error_ack(the_stream, q, mp, DL_BIND_REQ, DL_BADADDR, 0);
		return;
	}

	// Allocate message block for reply
	mblk_t *ack_mp;
	if ((ack_mp = allocb(sizeof(dl_bind_ack_t) + kEnetAndSAPAddressLength, BPRI_HI)) == NULL) {
		DLPI_error_ack(the_stream, q, mp, DL_BIND_REQ, DL_SYSERR, MAC_ENOMEM);
		return;
	}

	// Set up message type
	ack_mp->b_datap->db_type = M_PCPROTO;
	dl_bind_ack_t *ackp = (dl_bind_ack_t *)(void *)ack_mp->b_rptr;
	ackp->dl_primitive = DL_BIND_ACK;

	// Fill in other fields
	ackp->dl_sap = sap;
	ackp->dl_addr_length = kEnetAndSAPAddressLength;		
	ackp->dl_addr_offset = sizeof(dl_bind_ack_t);
	ackp->dl_max_conind = 0;
	ackp->dl_xidtest_flg = 0;

	T8022AddressStruct *addrInfo = (T8022AddressStruct *)(ack_mp->b_rptr + sizeof(dl_bind_ack_t));
	OTCopy48BitAddress(hardware_address, addrInfo->fHWAddr);
	addrInfo->fSAP = sap;

	// Must move b_wptr past the address info data
	ack_mp->b_wptr = ack_mp->b_rptr + sizeof(dl_bind_ack_t) + kEnetAndSAPAddressLength;

	// Set group SAP if necessary
	the_stream->ClearAllGroupSAPs();
	if (sap <= kMax8022SAP)
		the_stream->SetGroupSAP(k8022GlobalSAP);	

	// The stream is now bound and idle
	the_stream->dlpi_state = DL_IDLE;
	the_stream->dlsap = sap;
	the_stream->flags &= ~kSnapStream;

	// Free request
	freemsg(mp);

	// Send reply
	qreply(q, ack_mp);
	return;
}


/*
 *  Handle DL_UNBIND_REQ (unbind a stream)
 */

static void DLPI_unbind(DLPIStream *the_stream, queue_t *q, mblk_t *mp)
{
	D(bug("  DLPI_unbind(%p,%p)\n", the_stream, mp));

	// Stream must be bound and idle
	if (the_stream->dlpi_state != DL_IDLE) {
		DLPI_error_ack(the_stream, q, mp, DL_UNBIND_REQ, DL_OUTSTATE, 0);
		return;
	}

	// Stream is now unbound
	the_stream->dlpi_state = DL_UNBOUND;
	the_stream->dlsap = 0;				

	// Flush all pending outbound messages
	flushq(q, FLUSHDATA);

	// Flush all inbound messages pending on the stream
	flushq(RD(q), FLUSHDATA);
	putnextctl1(RD(q), M_FLUSH, FLUSHRW);

	// Send reply
	DLPI_ok_ack(the_stream, q, mp, DL_UNBIND_REQ);
	return;
}


/*
 *  Handle DL_SUBS_BIND_REQ (register 802.2 SAP group addresses and SNAPs)
 */

static void DLPI_subs_bind(DLPIStream *the_stream, queue_t *q, mblk_t *mp)
{
	dl_subs_bind_req_t *req = (dl_subs_bind_req_t *)(void *)mp->b_rptr;
	uint8 *sap = ((uint8 *)req) + req->dl_subs_sap_offset;
	int32 length = req->dl_subs_sap_length;
	uint16 theSap = ntohs(*((uint16 *)sap));
	int32 error = 0;
	D(bug("  DLPI_subs_bind(%p,%p) SAP %02x%02x%02x%02x%02x\n", the_stream, mp, sap[0], sap[1], sap[2], sap[3], sap[4]));

	// Stream must be idle
	if (the_stream->dlpi_state != DL_IDLE) {
		DLPI_error_ack(the_stream, q, mp, DL_SUBS_BIND_REQ, DL_OUTSTATE, 0);
		return;
	}

	// Check if address is valid
	switch (req->dl_subs_bind_class) {
		case DL_PEER_BIND:			// Bind a group address
			if (the_stream->dlsap <= kMax8022SAP) {
				if ((theSap & 1) && (length == sizeof(theSap)))
					the_stream->SetGroupSAP(theSap);
				else
					if (theSap == 0x0000)	// special case to receive all 802.2 packets
						the_stream->flags |= kAcceptAll8022Packets;
					else
						error = DL_BADADDR;
			} else
				error = DL_UNSUPPORTED;
			break;	

		case DL_HIERARCHICAL_BIND:	// Bind an additional SNAP
			if (the_stream->dlsap == kSNAPSAP) {
				if (the_stream->flags & kSnapStream) 
					error = DL_TOOMANY;	// only one SNAP binding allowed 
				else {
					OTCopy8022SNAP(sap, the_stream->snap);
					the_stream->flags |= kSnapStream;
				}
			} else
				error = DL_BADADDR;
			break;

		default:
			error = DL_UNSUPPORTED;
			break;
	}
	if (error) {
		DLPI_error_ack(the_stream, q, mp, DL_SUBS_BIND_REQ, error, 0);
		return;
	}

	// Allocate message block for reply
	mblk_t *ack_mp;
	if ((ack_mp = allocb(sizeof(dl_subs_bind_ack_t) + length, BPRI_HI)) == NULL) {
		DLPI_error_ack(the_stream, q, mp, DL_SUBS_BIND_REQ, DL_SYSERR, MAC_ENOMEM);
		return;
	}

	// Set up message type
	ack_mp->b_datap->db_type = M_PCPROTO;
	dl_subs_bind_ack_t *ackp = (dl_subs_bind_ack_t *)(void *)ack_mp->b_wptr;
	memset(ackp, 0, sizeof(dl_subs_bind_ack_t) + length);
	ackp->dl_primitive = DL_SUBS_BIND_ACK;

	// Fill in other fields
	ackp->dl_subs_sap_length = length;
	ackp->dl_subs_sap_offset = length ? sizeof(dl_subs_bind_ack_t) : 0;
	ack_mp->b_wptr += sizeof(dl_subs_bind_ack_t);
	if (length)
		memcpy(ack_mp->b_wptr, sap, length);
	ack_mp->b_wptr += length;

	// Free request
	freemsg(mp);

	// Send reply
	qreply(q, ack_mp);
	return;
}


/*
 *  Handle DL_SUBS_UNBIND_REQ (unregister 802.2 SAP group addresses and snaps)
 */

static void DLPI_subs_unbind(DLPIStream *the_stream, queue_t *q, mblk_t *mp)
{
	dl_subs_unbind_req_t *req = (dl_subs_unbind_req_t *)(void *)mp->b_rptr;
	uint8 *sap = ((uint8 *)req) + req->dl_subs_sap_offset;
	int32 length = req->dl_subs_sap_length;
	int32 error = 0;
	D(bug("  DLPI_subs_unbind(%p,%p) SAP %02x%02x%02x%02x%02x\n", the_stream, mp, sap[0], sap[1], sap[2], sap[3], sap[4]));

	// Stream must be idle
	if (the_stream->dlpi_state != DL_IDLE) {
		DLPI_error_ack(the_stream, q, mp, DL_SUBS_UNBIND_REQ, DL_OUTSTATE, 0);
		return;
	}

	// Check if we are unbinding from an address we are bound to
	if (length == k8022SAPLength) {
		if ((*sap & 1) && (*sap != kIPXSAP)) {
			if (the_stream->dlsap <= kMax8022SAP)
				the_stream->ClearGroupSAP(*sap);
			else
				error = DL_UNSUPPORTED;
		} else
			error = DL_BADADDR;
	} else if (length == k8022SNAPLength) {
		if (the_stream->dlsap == kSNAPSAP) {
			if (the_stream->flags & kSnapStream) {
				if (memcmp(the_stream->snap, sap, length) != 0) 
					error = DL_BADADDR;
			} else
				error = DL_BADADDR;
		} else
			error = DL_UNSUPPORTED;
	}
	if (error) {
		DLPI_error_ack(the_stream, q, mp, DL_SUBS_UNBIND_REQ, error, 0);
		return;
	}

	// Stream is no longer bound to SNAP
	the_stream->flags &= ~kSnapStream;

	// Send reply
	DLPI_ok_ack(the_stream, q, mp, DL_SUBS_UNBIND_REQ);
	return;
}


/*
 *  Handles DL_ENABMULTI_REQ (enable multicast address)
 */

static void DLPI_enable_multi(DLPIStream *the_stream, queue_t *q, mblk_t *mp)
{
	dl_enabmulti_req_t*	req = (dl_enabmulti_req_t*)(void *)mp->b_rptr;
	uint8 *reqaddr = (uint8 *)(mp->b_rptr + req->dl_addr_offset);
	D(bug("  DLPI_enable_multi(%p,%p) addr %02x%02x%02x%02x%02x%02x\n", the_stream, mp, reqaddr[0], reqaddr[1], reqaddr[2], reqaddr[3], reqaddr[4], reqaddr[5]));

	// Address must be a multicast address
	if (get_address_type(reqaddr) != keaMulticast) {
		DLPI_error_ack(the_stream, q, mp, DL_ENABMULTI_REQ, DL_BADADDR, 0);
		return;
	}

	// Address already in multicast list?
	if (the_stream->IsMulticastRegistered(reqaddr)) {
		DLPI_error_ack(the_stream, q, mp, DL_ENABMULTI_REQ, DL_BADADDR, 0);
		return;
	}

	// Tell add-on to enable multicast address
	AO_enable_multicast(Host2MacAddr((uint8 *)reqaddr));

	// Add new address to multicast list
	uint8 *addr = Mac2HostAddr(Mac_sysalloc(kEnetPhysicalAddressLength));
	OTCopy48BitAddress(reqaddr, addr);
	the_stream->AddMulticast(addr);

	// On receive now check multicast packets
	the_stream->flags |= kAcceptMulticasts;

	// Send reply
	DLPI_ok_ack(the_stream, q, mp, DL_ENABMULTI_REQ);
	return;
}


/*
 *  Handles DL_DISABMULTI_REQ (disable multicast address)
 */

static void DLPI_disable_multi(DLPIStream *the_stream, queue_t *q, mblk_t *mp)
{
	dl_disabmulti_req_t *req = (dl_disabmulti_req_t*)(void *)mp->b_rptr;
	uint8 *reqaddr = (uint8 *)(mp->b_rptr + req->dl_addr_offset);
	D(bug("  DLPI_disable_multi(%p,%p) addr %02x%02x%02x%02x%02x%02x\n", the_stream, mp, reqaddr[0], reqaddr[1], reqaddr[2], reqaddr[3], reqaddr[4], reqaddr[5]));

	// Address must be a multicast address
	if (get_address_type(reqaddr) != keaMulticast) {
		DLPI_error_ack(the_stream, q, mp, DL_DISABMULTI_REQ, DL_BADADDR, 0);
		return;
	}

	// Find address in multicast list
	uint8 *addr = the_stream->IsMulticastRegistered(reqaddr);
	if (addr == NULL) {
		DLPI_error_ack(the_stream, q, mp, DL_DISABMULTI_REQ, DL_BADADDR, 0);
		return;
	}

	// Found, then remove
	the_stream->RemoveMulticast(addr);
	Mac_sysfree(Host2MacAddr(addr));

	// Tell add-on to disable multicast address
	AO_disable_multicast(Host2MacAddr((uint8 *)reqaddr));
	
	// No longer check multicast packets if no multicast addresses are registered
	if (the_stream->multicast_list == NULL)
		the_stream->flags &= ~kAcceptMulticasts;
			
	// Send reply
	DLPI_ok_ack(the_stream, q, mp, DL_DISABMULTI_REQ);
	return;
}


/*
 *  Handle DL_UNITDATA_REQ (transmit packet)
 */

static void DLPI_unit_data(DLPIStream *the_stream, queue_t *q, mblk_t *mp)
{
	D(bug("  DLPI_unit_data(%p,%p)\n", the_stream, mp));
	dl_unitdata_req_t *req = (dl_unitdata_req_t *)(void *)mp->b_rptr;

	// Stream must be idle
	if (the_stream->dlpi_state != DL_IDLE) {

		// Not idle, send error response
		dl_uderror_ind_t *errp;
		mblk_t *bp;

		int i = sizeof(dl_uderror_ind_t) + req->dl_dest_addr_length;
		if ((bp = allocb(i, BPRI_HI)) == NULL) {
			freemsg(mp);
			return;
		}
		bp->b_datap->db_type = M_PROTO;
		errp = (dl_uderror_ind_t *)(void *)bp->b_wptr;
		errp->dl_primitive = DL_UDERROR_IND;
		errp->dl_errno = DL_OUTSTATE;
		errp->dl_unix_errno = 0;
		errp->dl_dest_addr_length = req->dl_dest_addr_length;
		errp->dl_dest_addr_offset = sizeof(dl_uderror_ind_t);
		bp->b_wptr += sizeof(dl_uderror_ind_t);
		memcpy((uint8 *)bp->b_wptr, ((uint8 *)req) + req->dl_dest_addr_offset, req->dl_dest_addr_length);
		bp->b_wptr += req->dl_dest_addr_length;
		qreply(q, bp);

		freemsg(mp);
		return;
	}

	// Build packet header and transmit packet
	if ((mp = build_tx_packet_header(the_stream, mp, false)) != NULL)
		transmit_packet(mp);
}


/*
 *  Ethernet packet allocator
 */

#if SIZEOF_VOID_P != 4 || REAL_ADDRESSING == 0
static uint32 ether_packet = 0;			// Ethernet packet (cached allocation)
static uint32 n_ether_packets = 0;		// Number of ethernet packets allocated so far (should be at most 1)

EthernetPacket::EthernetPacket()
{
	++n_ether_packets;
	if (ether_packet && n_ether_packets == 1)
		packet = ether_packet;
	else {
		packet = Mac_sysalloc(1516);
		assert(packet != 0);
		Mac_memset(packet, 0, 1516);
		if (ether_packet == 0)
			ether_packet = packet;
	}
}

EthernetPacket::~EthernetPacket()
{
	--n_ether_packets;
	if (packet != ether_packet)
		Mac_sysfree(packet);
	if (n_ether_packets > 0) {
		bug("WARNING: Nested allocation of ethernet packets!\n");
	}
}
#endif
