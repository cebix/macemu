/*
 *  router_types.h - ip router
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

#ifndef _ROUTER_TYPES_H_
#define _ROUTER_TYPES_H_

#pragma pack(push, 1)


// --------------------------- MAC ---------------------------
typedef struct {
	uint8 dest[6];
	uint8 src[6];
	uint16 type;
} mac_t;

enum {
	mac_type_llc_ipx_limit	= 0x05DC, // <= mac_type_llc_ipx_limit -->> 802.3 MAC frame
	mac_type_ip4						= 0x0800,
	mac_type_arp						= 0x0806,
	mac_type_rarp						= 0x8035,
	mac_type_ip6						= 0x86DD,
	mac_type_loopback				= 0x9000
};

// --------------------------- ARP ---------------------------
typedef struct {
	mac_t mac;
	uint16 htype;
	uint16 ptype;
	uint8 halen;
	uint8 palen;
	uint16 opcode;
	uint8 srch[6];	// size for ethernet
	uint8 srcp[4];	// size for ip
	uint8 dsth[6];	// size for ethernet
	uint8 dstp[4];	// size for ip
} arp_t;

enum {
	arp_request = 1,
	arp_reply = 2
};
enum {
	arp_hwtype_enet = 1
};

// --------------------------- IP4 ---------------------------
typedef struct {
	mac_t mac;
	uint8 header_len:4;
	uint8 version:4;
	uint8 tos;
	uint16 total_len;
	uint16 ident;
	uint16 flags_n_frag_offset; // foffset 0..11, flags 12..15
	uint8 ttl;
	uint8 proto;
	uint16 checksum;
	uint32 src;
	uint32 dest;
	// ip options, size = 4 * header_len - 20
} ip_t;

// Protocol STD numbers
enum {
	ip_proto_icmp		= IPPROTO_ICMP,
	ip_proto_tcp		= IPPROTO_TCP,
	ip_proto_udp		= IPPROTO_UDP
};

// --------------------------- ICMP ---------------------------
typedef struct {
	ip_t ip;
	uint8 type;
	uint8 code;
	uint16 checksum;
	// data
} icmp_t;

enum {
	icmp_Echo_reply	= 0,
	icmp_Destination_unreachable	= 3,
	icmp_Source_quench	= 4,
	icmp_Redirect	= 5,
	icmp_Echo	= 8,
	icmp_Router_advertisement	= 9,
	icmp_Router_solicitation	= 10,
	icmp_Time_exceeded	= 11,
	icmp_Parameter_problem	= 12,
	icmp_Time_Stamp_request	= 13,
	icmp_Time_Stamp_reply	= 14,
	icmp_Information_request_obsolete	= 15,
	icmp_Information_reply_obsolete	= 16,
	icmp_Address_mask_request	= 17,
	icmp_Address_mask_reply	= 18,
	icmp_Traceroute	= 30,
	icmp_Datagram_conversion_error	= 31,
	icmp_Mobile_host_redirect	= 32,
	icmp_IPv6_Where_Are_You	= 33,
	icmp_IPv6_I_Am_Here	= 34,
	icmp_Mobile_registration_request	= 35,
	icmp_Mobile_registration_reply	= 36,
	icmp_Domain_name_request	= 37,
	icmp_Domain_name_reply	= 38,
	icmp_SKIP	= 39,
	icmp_Photuris	= 40
};

// --------------------------- TCP ---------------------------
typedef struct {
	ip_t ip;
	uint16 src_port;
	uint16 dest_port;
	uint32 seq;
	uint32 ack;
	uint8 header_len;	// note: some reserved bits
	uint8 flags;			// note: some reserved bits
	uint16 window;
	uint16 checksum;
	uint16 urgent_ptr;
	// options + padding: size = dataoffset*4-20
	// data
} tcp_t;

enum {
	tcp_flags_URG = 0x20,	// The urgent pointer field is significant in this segment.
	tcp_flags_ACK = 0x10,	// The acknowledgment field is significant in this segment.
	tcp_flags_PSH = 0x08,	// Push function.
	tcp_flags_RST = 0x04,	// Resets the connection.
	tcp_flags_SYN = 0x02,	// Synchronizes the sequence numbers.
	tcp_flags_FIN = 0x01	// No more data from sender.
};

enum {
	tcp_state_closed,
	tcp_state_listen,
	tcp_state_syn_sent,
	tcp_state_syn_rcvd,
	tcp_state_established,
	tcp_state_close_wait,
	tcp_state_last_ack,
	tcp_state_finwait_1,
	tcp_state_finwait_2,
	tcp_state_closing,
	tcp_state_time_wait
};

// --------------------------- UDP ---------------------------
typedef struct {
	ip_t ip;
	uint16 src_port;
	uint16 dest_port;
	uint16 msg_len;
	uint16 checksum;
	// data
} udp_t;

typedef struct {
	uint16 src_lo, src_hi;
	uint16 dest_lo, dest_hi;
	uint16 proto;
	uint16 msg_len;
} pseudo_ip_t;

#pragma pack(pop)

#endif // _ROUTER_TYPES_H_
