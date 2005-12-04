/*
 *  ether_defs.h - Definitions for DLPI Ethernet Driver
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

#ifndef ETHER_DEFS_H
#define ETHER_DEFS_H


#if __MWERKS__ && __POWERPC__
#define PRAGMA_ALIGN_SUPPORTED 1
#define PACKED__
#elif defined __GNUC__
#define PACKED__ __attribute__ ((packed))
#elif defined __sgi
#define PRAGMA_PACK_SUPPORTED 1
#define PACKED__
#else
#error "Packed attribute or pragma shall be supported"
#endif


/*
 *  Macros
 */

// Get pointer to the read queue, assumes 'q' is a write queue ptr
#define RD(q) (&q[-1])

// Get pointer to the write queue, assumes 'q' is a read queue ptr
#define WR(q) (&q[1])

#define OTCompare48BitAddresses(p1, p2)														\
	(*(const uint32*)((const uint8*)(p1)) == *(const uint32*)((const uint8*)(p2)) &&		\
	 *(const uint16*)(((const uint8*)(p1))+4) == *(const uint16*)(((const uint8*)(p2))+4) )

#define OTCopy48BitAddress(p1, p2)												\
	(*(uint32*)((uint8*)(p2)) = *(const uint32*)((const uint8*)(p1)),			\
	 *(uint16*)(((uint8*)(p2))+4) = *(const uint16*)(((const uint8*)(p1))+4) )

#define OTClear48BitAddress(p1)													\
	(*(uint32*)((uint8*)(p1)) = 0,												\
	 *(uint16*)(((uint8*)(p1))+4) = 0 )

#define OTCompare8022SNAP(p1, p2)														\
	(*(const uint32*)((const uint8*)(p1)) == *(const uint32*)((const uint8*)(p2)) &&	\
	 *(((const uint8*)(p1))+4) == *(((const uint8*)(p2))+4) )

#define OTCopy8022SNAP(p1, p2)												\
	(*(uint32*)((uint8*)(p2)) = *(const uint32*)((const uint8*)(p1)),		\
	 *(((uint8*)(p2))+4) = *(((const uint8*)(p1))+4) )

#define OTIs48BitBroadcastAddress(p1)					\
	(*(uint32*)((uint8*)(p1)) == 0xffffffff &&			\
	 *(uint16*)(((uint8*)(p1))+4) == 0xffff )

#define OTSet48BitBroadcastAddress(p1)					\
	(*(uint32*)((uint8*)(p1)) = 0xffffffff,				\
	 *(uint16*)(((uint8*)(p1))+4) = 0xffff )

#define OTIs48BitZeroAddress(p1)				\
	(*(uint32*)((uint8*)(p1)) == 0 && 			\
	 *(uint16*)(((uint8*)(p1))+4) == 0 )


/*
 *  Constants
 */

enum {
	// Address and packet lengths
	kEnetPhysicalAddressLength = 6,
	k8022SAPLength = 1,
	k8022DLSAPLength = 2,
	k8022SNAPLength = 5,
	kMaxBoundAddrLength = 6 + 2 + 5, // addr/SAP/SNAP
	kEnetAndSAPAddressLength = kEnetPhysicalAddressLength + k8022DLSAPLength,
	kEnetPacketHeaderLength = (2 * kEnetPhysicalAddressLength) + k8022DLSAPLength,
	k8022BasicHeaderLength = 3, // SSAP/DSAP/Control
	k8022SNAPHeaderLength = k8022SNAPLength + k8022BasicHeaderLength,
	kMinDIXSAP = 1501,
	kEnetTSDU = 1514,

	// Special addresses
	kSNAPSAP = 0xaa,
	kMax8022SAP = 0xfe,
	k8022GlobalSAP = 0xff,
	kIPXSAP = 0xff,

	// DLPI interface states
	DL_UNBOUND = 0,

	// Message types
	M_DATA = 0,
	M_PROTO = 1,
	M_IOCTL = 14,
	M_IOCACK = 129,
	M_IOCNAK = 130,
	M_PCPROTO = 131, // priority message
	M_FLUSH = 134,
		FLUSHDATA = 0,
		FLUSHALL = 1,
		FLUSHR = 1,
		FLUSHW = 2,
		FLUSHRW = 3,

	// DLPI primitives
	DL_INFO_REQ = 0,
	DL_BIND_REQ = 1,
		DL_PEER_BIND = 1,
		DL_HIERARCHICAL_BIND = 2,
	DL_UNBIND_REQ = 2,
	DL_INFO_ACK = 3,
	DL_BIND_ACK = 4,
	DL_ERROR_ACK = 5,
	DL_OK_ACK = 6,
	DL_UNITDATA_REQ = 7,
	DL_UNITDATA_IND = 8,
	DL_UDERROR_IND = 9,
	DL_SUBS_UNBIND_REQ = 21,
	DL_SUBS_BIND_REQ = 27,
	DL_SUBS_BIND_ACK = 28,
	DL_ENABMULTI_REQ = 29,
	DL_DISABMULTI_REQ = 30,
	DL_PHYS_ADDR_REQ = 49,
	DL_PHYS_ADDR_ACK = 50,
		DL_FACT_PHYS_ADDR = 1,
		DL_CURR_PHYS_ADDR = 2,

	// DLPI states
	DL_IDLE = 3,

	// DLPI error codes
	DL_BADADDR = 1,			// improper address format
	DL_OUTSTATE = 3,		// improper state
	DL_SYSERR = 4,			// UNIX system error
	DL_UNSUPPORTED = 7,		// service unsupported
	DL_BADPRIM = 9,			// primitive unknown
	DL_NOTSUPPORTED = 18,	// primitive not implemented
	DL_TOOMANY = 19,		// limit exceeded

	// errnos
	MAC_ENXIO = 6,
	MAC_ENOMEM = 12,
	MAC_EINVAL = 22,

	// Various DLPI constants
	DL_CLDLS = 2, // connectionless data link service
	DL_STYLE1 = 0x500,
	DL_VERSION_2 = 2,
	DL_CSMACD = 0,
	DL_ETHER = 4,
	DL_UNKNOWN = -1,

	// ioctl() codes
	I_OTSetFramingType = (('O' << 8) | 2),
		kOTGetFramingValue = -1,
		kOTFramingEthernet = 1,
		kOTFramingEthernetIPX = 2,
		kOTFramingEthernet8023 = 4,
		kOTFraming8022 = 8,
	I_OTSetRawMode = (('O' << 8) | 3),
	DL_IOC_HDR_INFO = (('l' << 8) | 10),

	// Buffer allocation priority
	BPRI_LO = 1,
	BPRI_HI = 3,

	// Misc constants
	kEnetModuleID = 7101
};

enum EAddrType {
	keaStandardAddress = 0,
	keaMulticast,
	keaBroadcast,
	keaBadAddress
};


/*
 *  Data member wrappers
 */

// Forward declarations
struct datab;
struct msgb;
struct queue;
struct multicast_node;
struct DLPIStream;

// Optimize for 32-bit big endian targets
#if defined(WORDS_BIGENDIAN) && (SIZEOF_VOID_P == 4)

// Predefined member types
typedef int8			nw_int8;
typedef int16			nw_int16;
typedef int32			nw_int32;
typedef uint8			nw_uint8;
typedef uint16			nw_uint16;
typedef uint32			nw_uint32;
typedef int				nw_bool;
typedef uint8 *			nw_uint8_p;
typedef void *			nw_void_p;
typedef datab *			nw_datab_p;
typedef msgb *			nw_msgb_p;
typedef queue *			nw_queue_p;
typedef multicast_node *nw_multicast_node_p;
typedef DLPIStream *	nw_DLPIStream_p;

#else

// Big-endian memory accessor
template< int nbytes >
struct nw_memory_helper;

template<>
struct nw_memory_helper<1> {
	static inline uint8 load(void *ptr) { return *((uint8 *)ptr); }
	static inline void store(void *ptr, uint8 val) { *((uint8 *)ptr) = val; }
};

template<>
struct nw_memory_helper<2> {
	static inline uint16 load(void *ptr) { return ntohs(*((uint16 *)ptr)); }
	static inline void  store(void *ptr, uint16 val) { *((uint16 *)ptr) = htons(val); }
};

template<>
struct nw_memory_helper<4> {
	static inline uint32 load(void *ptr) { return ntohl(*((uint32 *)ptr)); }
	static inline void  store(void *ptr, uint32 val) { *((uint32 *)ptr) = htonl(val); }
};

// Scalar data member wrapper (specialise for pointer member types?)
template< class type, class public_type >
class nw_scalar_member_helper {
	uint8 _pad[sizeof(type)];
public:
	operator public_type () const {
		return (public_type)(uintptr)nw_memory_helper<sizeof(type)>::load((void *)this);
	}
	public_type operator -> () const {
		return this->operator public_type ();
	}
	nw_scalar_member_helper<type, public_type> & operator = (public_type val) {
		nw_memory_helper<sizeof(type)>::store((void *)this, (type)(uintptr)val);
		return *this;
	}
	nw_scalar_member_helper<type, public_type> & operator += (int val) {
		*this = *this + val;
		return *this;
	}
	nw_scalar_member_helper<type, public_type> & operator -= (int val) {
		*this = *this - val;
		return *this;
	}
	nw_scalar_member_helper<type, public_type> & operator &= (int val) {
		*this = *this & val;
		return *this;
	}
	nw_scalar_member_helper<type, public_type> & operator |= (int val) {
		*this = *this | val;
		return *this;
	}
};

// Predefined member types
typedef nw_scalar_member_helper<uint8, int8>			nw_int8;
typedef nw_scalar_member_helper<uint16, int16>			nw_int16;
typedef nw_scalar_member_helper<uint32, int32>			nw_int32;
typedef nw_scalar_member_helper<uint8, uint8>			nw_uint8;
typedef nw_scalar_member_helper<uint16, uint16>			nw_uint16;
typedef nw_scalar_member_helper<uint32, uint32>			nw_uint32;
typedef nw_scalar_member_helper<int, bool>				nw_bool;
typedef nw_scalar_member_helper<uint32, uint8 *>		nw_uint8_p;
typedef nw_scalar_member_helper<uint32, void *>			nw_void_p;
typedef nw_scalar_member_helper<uint32, datab *>		nw_datab_p;
typedef nw_scalar_member_helper<uint32, msgb *>			nw_msgb_p;
typedef nw_scalar_member_helper<uint32, queue *>		nw_queue_p;
typedef nw_scalar_member_helper<uint32, multicast_node *> nw_multicast_node_p;
typedef nw_scalar_member_helper<uint32, DLPIStream *>	nw_DLPIStream_p;

#endif


/*
 *  Structures
 */

// Data block
struct datab {
	nw_datab_p db_freep;
	nw_uint8_p db_base;
	nw_uint8_p db_lim;
	nw_uint8   db_ref;
	nw_uint8   db_type;
	// ...
};

// Message block
struct msgb {
	nw_msgb_p  b_next;
	nw_msgb_p  b_prev;
	nw_msgb_p  b_cont;
	nw_uint8_p b_rptr;
	nw_uint8_p b_wptr;
	nw_datab_p b_datap;
	// ...
};

// Queue (full structure required because of size)
struct queue {
	nw_void_p q_qinfo;
	nw_msgb_p q_first;
	nw_msgb_p q_last;
	nw_queue_p q_next;
	nw_queue_p q_link;
	nw_DLPIStream_p q_ptr;
	nw_uint32 q_count;
	nw_int32 q_minpsz;
	nw_int32 q_maxpsz;
	nw_uint32 q_hiwat;
	nw_uint32 q_lowat;
	nw_void_p q_bandp;
	nw_uint16 q_flag;
	nw_uint8 q_nband;
	uint8 _q_pad1[1];
	nw_void_p q_osx;
	nw_queue_p q_ffcp;
	nw_queue_p q_bfcp;
};
typedef struct queue queue_t;

// M_IOCTL parameters
struct iocblk {
	nw_int32 ioc_cmd;
	nw_void_p ioc_cr;
	nw_uint32 ioc_id;
	nw_uint32 ioc_count;
	nw_int32 ioc_error;
	nw_int32 ioc_rval;
	int32 _ioc_filler[4];
};

// Priority specification
struct dl_priority_t {
	nw_int32 dl_min, dl_max;
};

// DPLI primitives
struct dl_info_req_t {
	nw_uint32 dl_primitive; // DL_INFO_REQ
};

struct dl_info_ack_t {
	nw_uint32 dl_primitive; // DL_INFO_ACK
    nw_uint32 dl_max_sdu;
    nw_uint32 dl_min_sdu;
    nw_uint32 dl_addr_length;
    nw_uint32 dl_mac_type;
    nw_uint32 dl_reserved;
    nw_uint32 dl_current_state;
    nw_int32 dl_sap_length;
    nw_uint32 dl_service_mode;
    nw_uint32 dl_qos_length;
    nw_uint32 dl_qos_offset;
    nw_uint32 dl_qos_range_length;
    nw_uint32 dl_qos_range_offset;
    nw_uint32 dl_provider_style;
    nw_uint32 dl_addr_offset;
    nw_uint32 dl_version;
    nw_uint32 dl_brdcst_addr_length;
    nw_uint32 dl_brdcst_addr_offset;
    nw_uint32 dl_growth;
};

struct dl_bind_req_t {
	nw_uint32 dl_primitive; // DL_BIND_REQ
	nw_uint32 dl_sap;
	nw_uint32 dl_max_conind;
	nw_uint16 dl_service_mode;
	nw_uint16 dl_conn_mgmt;
	nw_uint32 dl_xidtest_flg;
};

struct dl_bind_ack_t {
	nw_uint32 dl_primitive; // DL_BIND_ACK
	nw_uint32 dl_sap;
	nw_uint32 dl_addr_length;
	nw_uint32 dl_addr_offset;
	nw_uint32 dl_max_conind;
	nw_uint32 dl_xidtest_flg;
};

struct dl_error_ack_t {
	nw_uint32 dl_primitive; // DL_ERROR_ACK
	nw_uint32 dl_error_primitive;
	nw_uint32 dl_errno;
	nw_uint32 dl_unix_errno;
};

struct dl_ok_ack_t {
	nw_uint32 dl_primitive; // DL_ERROR_ACK
	nw_uint32 dl_correct_primitive;
};

struct dl_unitdata_req_t {
	nw_uint32 dl_primitive; // DL_UNITDATA_REQ
	nw_uint32 dl_dest_addr_length;
	nw_uint32 dl_dest_addr_offset;
	dl_priority_t   dl_priority;
};

struct dl_unitdata_ind_t {
	nw_uint32 dl_primitive; // DL_UNITDATA_IND
	nw_uint32 dl_dest_addr_length;
	nw_uint32 dl_dest_addr_offset;
	nw_uint32 dl_src_addr_length;
	nw_uint32 dl_src_addr_offset;
	nw_uint32 dl_group_address;
};

struct dl_uderror_ind_t {
	nw_uint32 dl_primitive; // DL_UDERROR_IND
	nw_uint32 dl_dest_addr_length;
	nw_uint32 dl_dest_addr_offset;
	nw_uint32 dl_unix_errno;
	nw_uint32 dl_errno;
};

struct dl_subs_bind_req_t {
	nw_uint32 dl_primitive; // DL_SUBS_BIND_REQ
	nw_uint32 dl_subs_sap_offset;
	nw_uint32 dl_subs_sap_length;
	nw_uint32 dl_subs_bind_class;
};

struct dl_subs_bind_ack_t {
	nw_uint32 dl_primitive; // DL_SUBS_BIND_ACK
	nw_uint32 dl_subs_sap_offset;
	nw_uint32 dl_subs_sap_length;
};

struct dl_subs_unbind_req_t {
	nw_uint32 dl_primitive; // DL_SUBS_UNBIND_REQ
	nw_uint32 dl_subs_sap_offset;
	nw_uint32 dl_subs_sap_length;
};

struct dl_enabmulti_req_t {
	nw_uint32 dl_primitive; // DL_ENABMULTI_REQ
	nw_uint32 dl_addr_length;
	nw_uint32 dl_addr_offset;
};

struct dl_disabmulti_req_t {
	nw_uint32 dl_primitive; // DL_DISABMULTI_REQ
	nw_uint32 dl_addr_length;
	nw_uint32 dl_addr_offset;
};

struct dl_phys_addr_req_t {
	nw_uint32 dl_primitive; // DL_PHYS_ADDR_REQ
	nw_uint32 dl_addr_type;
};

struct dl_phys_addr_ack_t {
	nw_uint32 dl_primitive; // DL_PHYS_ADDR_ACK
	nw_uint32 dl_addr_length;
	nw_uint32 dl_addr_offset;
};

// Parameters for I_OTSetRawMode/kOTSetRecvMode ioctl()
struct dl_recv_control_t {
	nw_uint32 dl_primitive;
	nw_uint32 dl_flags;
	nw_uint32 dl_truncation_length;
};

union DL_primitives {
	nw_uint32 dl_primitive;
	dl_info_req_t info_req;
	dl_info_ack_t info_ack;
	dl_bind_req_t bind_req;
	dl_bind_ack_t bind_ack;
	dl_error_ack_t error_ack;
	dl_ok_ack_t ok_ack;
	dl_unitdata_req_t unitdata_req;
	dl_unitdata_ind_t unitdata_ind;
	dl_uderror_ind_t uderror_ind;
	dl_subs_bind_req_t subs_bind_req;
	dl_subs_bind_ack_t subs_bind_ack;
	dl_subs_unbind_req_t subs_unbind_req;
	dl_enabmulti_req_t enabmulti_req;
	dl_disabmulti_req_t disabmulti_req;
	dl_phys_addr_req_t phys_addr_req;
	dl_phys_addr_ack_t phys_addr_ack;
};

#ifdef PRAGMA_ALIGN_SUPPORTED
#pragma options align=mac68k
#endif

#ifdef PRAGMA_PACK_SUPPORTED
#pragma pack(1)
#endif

// Packet headers
struct EnetPacketHeader {
	uint8 fDestAddr[6];
	uint8 fSourceAddr[6];
	nw_uint16 fProto;
} PACKED__;

struct T8022Header {
	uint8 fDSAP;
	uint8 fSSAP;
	uint8 fCtrl;
} PACKED__;

struct T8022SNAPHeader {
	uint8 fDSAP;
	uint8 fSSAP;
	uint8 fCtrl;
	uint8 fSNAP[k8022SNAPLength];
} PACKED__;

struct T8022FullPacketHeader {
	EnetPacketHeader fEnetPart;
	T8022SNAPHeader f8022Part;
} PACKED__;

struct T8022AddressStruct {
	uint8 fHWAddr[6];
	nw_uint16 fSAP;
	uint8 fSNAP[k8022SNAPLength];
} PACKED__;

#ifdef PRAGMA_PACK_SUPPORTED
#pragma pack(0)
#endif

#ifdef PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif

#endif
