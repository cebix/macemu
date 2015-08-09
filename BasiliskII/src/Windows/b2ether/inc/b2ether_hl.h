/*
 *  b2ether_hl.h - Win32 ethernet driver high-level interface
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

#ifndef _B2_ETHER_HL_
#define _B2_ETHER_HL_


#ifdef __cplusplus
extern "C" {
#endif


#define	ETH_802_3_ADDRESS_LENGTH 6
#define MAX_LINK_NAME_LENGTH 124

typedef struct _ADAPTER {
  HANDLE     hFile;
  TCHAR      SymbolicLink[MAX_LINK_NAME_LENGTH];
} ADAPTER, *LPADAPTER;

typedef struct _PACKET {
  OVERLAPPED   OverLapped;
  PVOID        Buffer;
  UINT         Length;
  ULONG        BytesReceived;
  BOOL         bIoComplete;
  BOOL         free;
	struct _PACKET *next;
} PACKET, *LPPACKET;



BOOLEAN StartPacketDriver(
	LPCTSTR ServiceName
);

LPADAPTER PacketOpenAdapter(
	LPCTSTR   AdapterName,
	int16		 mode
);

VOID PacketCloseAdapter(
	LPADAPTER   lpAdapter
);

LPPACKET PacketAllocatePacket(
	LPADAPTER   AdapterObject,
	UINT Length
);

VOID PacketFreePacket(
	LPPACKET    lpPacket
);

BOOLEAN PacketSendPacket(
	LPADAPTER   AdapterObject,
	LPPACKET    lpPacket,
	BOOLEAN     Sync,
	BOOLEAN     RecyclingAllowed
);

BOOLEAN PacketGetAddress(
  LPADAPTER  AdapterObject,
  PUCHAR     AddressBuffer,
  PUINT       Length
);

BOOLEAN PacketReceivePacket(
  LPADAPTER   AdapterObject,
  LPPACKET    lpPacket,
  BOOLEAN     Sync
);

BOOLEAN PacketSetFilter( LPADAPTER  AdapterObject, ULONG Filter );
BOOLEAN PacketGetMAC( LPADAPTER AdapterObject, LPBYTE address, BOOL permanent );
BOOLEAN PacketAddMulticast( LPADAPTER AdapterObject, LPBYTE address );
BOOLEAN PacketDelMulticast( LPADAPTER AdapterObject, LPBYTE address );

ULONG PacketGetAdapterNames( LPADAPTER lpAdapter, LPTSTR pStr, PULONG BufferSize );

// callbacks
void recycle_write_packet( LPPACKET Packet );

VOID CALLBACK packet_read_completion(
  DWORD dwErrorCode,
  DWORD dwNumberOfBytesTransfered,
  LPOVERLAPPED lpOverlapped
);


#ifdef __cplusplus
}
#endif


#endif // _B2_ETHER_HL_
