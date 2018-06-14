/*
 *  packet32.cpp
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
#include "main.h"
#include "util_windows.h"
#include <windowsx.h>
#include <winioctl.h>
#include "cpu_emulation.h"

// VC6 does not have this, Platform SDK has.
// In case of errors, try to comment out, the needed
// definitions are below (#ifndef _NTDDNDIS_)

// Most people don't have the Platform SDK, so I take this one out.
// #include <ntddndis.h>

#include "inc/ntddpack.h"

#include "ether.h"
#include "ether_defs.h"
#include "b2ether/multiopt.h"
#include "b2ether/inc/b2ether_hl.h"



#ifndef _NTDDNDIS_
#define NDIS_PACKET_TYPE_DIRECTED					0x00000001
#define NDIS_PACKET_TYPE_MULTICAST				0x00000002
#define NDIS_PACKET_TYPE_ALL_MULTICAST		0x00000004
#define NDIS_PACKET_TYPE_BROADCAST				0x00000008
#define NDIS_PACKET_TYPE_SOURCE_ROUTING		0x00000010
#define NDIS_PACKET_TYPE_PROMISCUOUS			0x00000020

#define OID_802_3_PERMANENT_ADDRESS				0x01010101
#define OID_802_3_CURRENT_ADDRESS					0x01010102
#define OID_802_3_MULTICAST_LIST					0x01010103

#define OID_GEN_CURRENT_PACKET_FILTER			0x0001010E
#endif

#define DEBUG_PACKETS 0
#define DEBUG 0
#include "debug.h"

#ifdef __cplusplus
extern "C" {
#endif

#if DEBUG
#pragma optimize("",off)
#endif

#define MAX_MULTICAST 100
#define MAX_MULTICAST_SZ (20*ETH_802_3_ADDRESS_LENGTH)

static ULONG packet_filter = 0;


LPADAPTER PacketOpenAdapter( LPCTSTR AdapterName, int16 mode )
{
  LPADAPTER  lpAdapter;
  BOOLEAN    Result = TRUE;

  D(bug("Packet32: PacketOpenAdapter\n"));

  // May fail if user is not an Administrator.
  StartPacketDriver( TEXT("B2ether") );

  lpAdapter = (LPADAPTER)GlobalAllocPtr( GMEM_MOVEABLE|GMEM_ZEROINIT, sizeof(ADAPTER) );
  if (lpAdapter==NULL) {
      D(bug("Packet32: PacketOpenAdapter GlobalAlloc Failed\n"));
      return NULL;
  }

	TCHAR device_name[256];
	_sntprintf(lpAdapter->SymbolicLink, lengthof(lpAdapter->SymbolicLink), TEXT("\\\\.\\B2ether_%s"), AdapterName );
	_sntprintf(device_name, lengthof(device_name), TEXT("\\Device\\B2ether_%s"), AdapterName );

	// Work around one subtle NT4 bug.
	DefineDosDevice(
			DDD_REMOVE_DEFINITION,
			&lpAdapter->SymbolicLink[4],
			NULL
	);
	DefineDosDevice(
			DDD_RAW_TARGET_PATH,
			&lpAdapter->SymbolicLink[4],
			device_name
	);

	packet_filter = NDIS_PACKET_TYPE_DIRECTED |
									NDIS_PACKET_TYPE_MULTICAST |
									NDIS_PACKET_TYPE_BROADCAST;

	if(mode == ETHER_MULTICAST_ALL) packet_filter |= NDIS_PACKET_TYPE_ALL_MULTICAST;
	if(mode == ETHER_MULTICAST_PROMISCUOUS) packet_filter |= NDIS_PACKET_TYPE_PROMISCUOUS;

  if (Result) {
    lpAdapter->hFile = CreateFile(lpAdapter->SymbolicLink,
                         GENERIC_WRITE | GENERIC_READ,
                         0,
                         NULL,
												 // (os == VER_PLATFORM_WIN32_NT) ? CREATE_ALWAYS : OPEN_EXISTING,
												 OPEN_EXISTING,
                         FILE_FLAG_OVERLAPPED,
                         0
                         );
    if (lpAdapter->hFile != INVALID_HANDLE_VALUE) {
			if(*AdapterName && _tcscmp(AdapterName,TEXT("<None>")) != 0) {
				PacketSetFilter( lpAdapter, packet_filter );
			}
      return lpAdapter;
    }
  }
  D(bug("Packet32: PacketOpenAdapter Could not open adapter\n"));
  GlobalFreePtr( lpAdapter );
  return NULL;
}

VOID PacketCloseAdapter( LPADAPTER lpAdapter )
{
  D(bug("Packet32: PacketCloseAdapter\n"));

	if(lpAdapter) {
		if(lpAdapter->hFile) {
			CloseHandle(lpAdapter->hFile);
		}
		GlobalFreePtr(lpAdapter);
	}
}

LPPACKET PacketAllocatePacket( LPADAPTER AdapterObject, UINT Length )
{
  LPPACKET lpPacket;

  lpPacket = (LPPACKET)GlobalAllocPtr( GMEM_MOVEABLE|GMEM_ZEROINIT, sizeof(PACKET) );
  if(lpPacket==NULL) {
      D(bug("Packet32: PacketAllocatePacket: GlobalAlloc Failed\n"));
      return NULL;
  }

	lpPacket->OverLapped.hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
	if(!lpPacket->OverLapped.hEvent) {
			D(bug("Packet32: PacketAllocatePacket: CreateEvent Failed\n"));
			GlobalFreePtr(lpPacket);
			return NULL;
	}

	lpPacket->Buffer = GlobalAllocPtr(GMEM_MOVEABLE,2048); // 1514
	if(!lpPacket->Buffer) {
      D(bug("Packet32: PacketAllocatePacket: GlobalAllocPtr Failed\n"));
		  if(lpPacket->OverLapped.hEvent) CloseHandle(lpPacket->OverLapped.hEvent);
      GlobalFreePtr(lpPacket);
      return NULL;
	}

  lpPacket->OverLapped.Offset = 0;
  lpPacket->OverLapped.OffsetHigh = 0;
	lpPacket->Length = Length;
	lpPacket->BytesReceived	= 0;
	lpPacket->bIoComplete	= FALSE;
	lpPacket->free = TRUE;

  return lpPacket;
}

VOID PacketFreePacket( LPPACKET lpPacket )
{
	if(lpPacket) {
		if(lpPacket->Buffer) GlobalFreePtr(lpPacket->Buffer);
		if(lpPacket->OverLapped.hEvent) CloseHandle(lpPacket->OverLapped.hEvent);
		GlobalFreePtr(lpPacket);
	}
}

BOOLEAN PacketDeviceIoControl(
	LPADAPTER	lpAdapterObject,
	LPPACKET	lpPacket,
	ULONG			ulIoctl,
	BOOLEAN		bSync
)
{
	BOOLEAN Result;

	lpPacket->OverLapped.Offset		= 0;
	lpPacket->OverLapped.OffsetHigh	= 0;
	lpPacket->BytesReceived		= 0;

	if ( !ResetEvent( lpPacket->OverLapped.hEvent ) )  {
		lpPacket->bIoComplete = FALSE;
		D(bug( "Packet32: PacketDeviceIoControl failed to reset event\r\n", GetLastError() ));
		return FALSE;
	}

	Result = DeviceIoControl(
		lpAdapterObject->hFile,
		ulIoctl,
		lpPacket->Buffer,
		lpPacket->Length,
		lpPacket->Buffer,
		lpPacket->Length,
		&(lpPacket->BytesReceived),
		&(lpPacket->OverLapped) );

	if( !Result && bSync ) {
		if (GetLastError() == ERROR_IO_PENDING) {
			Result = GetOverlappedResult(	lpAdapterObject->hFile,
											&(lpPacket->OverLapped),
											&(lpPacket->BytesReceived),
											TRUE );
		} else {
			D(bug( "Packet32: unsupported API call returned error 0x%x\r\n", GetLastError() ));
		}
	}
	lpPacket->bIoComplete = Result;
	return Result;
}

VOID CALLBACK PacketSendCompletionRoutine(
  DWORD dwErrorCode,
  DWORD dwNumberOfBytesTransfered,
  LPOVERLAPPED lpOverlapped
)
{
  LPPACKET lpPacket = CONTAINING_RECORD(lpOverlapped,PACKET,OverLapped);

#if DEBUG_PACKETS
  D(bug("PacketSendCompletionRoutine %d\n",dwNumberOfBytesTransfered));
#endif

	lpPacket->bIoComplete = TRUE;
	// lpPacket->free = TRUE;
	// PacketFreePacket(lpPacket);
	recycle_write_packet(lpPacket);
}

BOOLEAN PacketSendPacket(
  LPADAPTER   AdapterObject,
  LPPACKET    lpPacket,
  BOOLEAN     Sync,
	BOOLEAN     RecyclingAllowed
)
{
  BOOLEAN  Result;

#if DEBUG_PACKETS
  D(bug("Packet32: PacketSendPacket bytes=%d, sync=%d\n",lpPacket->Length,Sync));
#endif

	lpPacket->OverLapped.Offset = 0;
	lpPacket->OverLapped.OffsetHigh = 0;
	lpPacket->bIoComplete = FALSE;

	if(Sync) {
		Result = WriteFile(
							AdapterObject->hFile,
							lpPacket->Buffer,
							lpPacket->Length,
							&lpPacket->BytesReceived,
							&lpPacket->OverLapped
							);
		if(Result) {
			Result = GetOverlappedResult(
									AdapterObject->hFile,
									&lpPacket->OverLapped,
									&lpPacket->BytesReceived,
									TRUE
									);
		} else {
			D(bug("Packet32: PacketSendPacket WriteFile failed, err=%d\n",(int)GetLastError()));
		}
		lpPacket->bIoComplete = TRUE;
		if(RecyclingAllowed) PacketFreePacket(lpPacket);
#if DEBUG_PACKETS
		D(bug("Packet32: PacketSendPacket result=%d, bytes=%d\n",(int)Result,(int)lpPacket->BytesReceived));
#endif
	} else {
		// don't care about the result
		Result = WriteFileEx(
			AdapterObject->hFile,
			lpPacket->Buffer,
			lpPacket->Length,
			&lpPacket->OverLapped,
			PacketSendCompletionRoutine
		);
#if DEBUG_PACKETS
		D(bug("Packet32: PacketSendPacket result=%d\n",(int)Result));
#endif
		if(!Result && RecyclingAllowed)	{
			recycle_write_packet(lpPacket);
		}
	}

  return Result;
}

BOOLEAN PacketReceivePacket(
  LPADAPTER   AdapterObject,
  LPPACKET    lpPacket,
  BOOLEAN     Sync
)
{
  BOOLEAN      Result;

	lpPacket->OverLapped.Offset=0;
	lpPacket->OverLapped.OffsetHigh=0;
	lpPacket->bIoComplete = FALSE;

#if DEBUG_PACKETS
	D(bug("Packet32: PacketReceivePacket\n"));
#endif

	if (Sync) {
		Result = ReadFile(
							AdapterObject->hFile,
							lpPacket->Buffer,
							lpPacket->Length,
							&lpPacket->BytesReceived,
							&lpPacket->OverLapped
							);
		if(Result) {
			Result = GetOverlappedResult(
									AdapterObject->hFile,
									&lpPacket->OverLapped,
									&lpPacket->BytesReceived,
									TRUE
									);
			if(Result)
				lpPacket->bIoComplete = TRUE;
			else
				lpPacket->free = TRUE;
		}
	} else {
		Result = ReadFileEx(
							AdapterObject->hFile,
							lpPacket->Buffer,
							lpPacket->Length,
							&lpPacket->OverLapped,
							packet_read_completion
							);
	}

	if(!Result) lpPacket->BytesReceived = 0;

#if DEBUG_PACKETS
  D(bug("Packet32: PacketReceivePacket got %d bytes, result=%d\n",lpPacket->BytesReceived,(int)Result));
#endif

  return Result;
}

BOOLEAN PacketRequest(
	LPADAPTER	lpAdapterObject,
	LPPACKET	lpPacket,
	BOOLEAN		bSet
)
{
	BOOLEAN	Result = FALSE;

	Result = PacketDeviceIoControl(
				lpAdapterObject,
				lpPacket,
				(ULONG) ((bSet) ? IOCTL_PROTOCOL_SET_OID : IOCTL_PROTOCOL_QUERY_OID),
				TRUE );

	if ( lpPacket->BytesReceived == 0 ) {
		D(bug( "Packet32: Ndis returned error to OID\r\n"));
		Result = FALSE;
	}
	return Result;
}

LPPACKET PacketQueryOid(
	LPADAPTER	lpAdapter,
	ULONG		ulOid,
	ULONG		ulLength
)
{
	ULONG		ioctl;
	LPPACKET lpPacket;

#define pOidData ((PPACKET_OID_DATA)(lpPacket->Buffer))

	lpPacket = PacketAllocatePacket( lpAdapter, sizeof(PACKET_OID_DATA)-1+ulLength );

	if( lpPacket ) {
		ioctl = IOCTL_PROTOCOL_QUERY_OID;
		pOidData->Oid    = ulOid;
		pOidData->Length = ulLength;

		if (PacketRequest( lpAdapter, lpPacket, FALSE )) {
			return lpPacket;
		}
		PacketFreePacket( lpPacket );
	}

#undef pOidData

	return 0;
}

BOOLEAN PacketGetMAC( LPADAPTER AdapterObject, LPBYTE address, BOOL permanent )
{
	BOOLEAN    Status;
	LPPACKET lpPacket;

	lpPacket = PacketQueryOid(
			AdapterObject,
			permanent ? OID_802_3_PERMANENT_ADDRESS : OID_802_3_CURRENT_ADDRESS,
			ETH_802_3_ADDRESS_LENGTH
	);
	if(lpPacket) {
		memcpy( address,
				((BYTE *)(lpPacket->Buffer)) + sizeof(PACKET_OID_DATA) - 1,
				ETH_802_3_ADDRESS_LENGTH );
		PacketFreePacket( lpPacket );
		Status = TRUE;
	} else {
		Status = FALSE;
	}

	return Status;
}

// There are other ways to do this.

BOOLEAN PacketAddMulticast( LPADAPTER AdapterObject, LPBYTE address )
{
	BOOLEAN Status = FALSE;
	LPBYTE		p;
	int				 i, count;
	LPPACKET lpPacket;

	D(bug("PacketAddMulticast\n"));

	/*
	if(packet_filter & (NDIS_PACKET_TYPE_ALL_MULTICAST|NDIS_PACKET_TYPE_PROMISCUOUS)) {
		D(bug("PacketAddMulticast: already listening for all multicast\n"));
		return TRUE;
	}
	*/

	lpPacket = PacketQueryOid( AdapterObject, OID_802_3_MULTICAST_LIST, MAX_MULTICAST_SZ );
#define OidData	((PPACKET_OID_DATA)(lpPacket->Buffer))

	if(lpPacket) {
		count = OidData->Length / ETH_802_3_ADDRESS_LENGTH;

		D(bug("PacketAddMulticast: %d old addresses\n",count));

		p = (LPBYTE)OidData->Data;

		for( i=0; i<count; i++ ) {
			if(memcmp(p,address,ETH_802_3_ADDRESS_LENGTH) == 0) {
				// This multicast is already defined -- error or not?
				Status = TRUE;
				D(bug("PacketAddMulticast: address already defined\n"));
				break;
			}
			p += ETH_802_3_ADDRESS_LENGTH;
		}
		if(i == count) {
			if(i >= MAX_MULTICAST) {
				D(bug("PacketAddMulticast: too many addresses\n"));
				Status = FALSE;
			} else {
				D(bug("PacketAddMulticast: adding a new address\n"));

				// ULONG IoCtlBufferLength = (sizeof(PACKET_OID_DATA)+ETH_802_3_ADDRESS_LENGTH*1-1);
				ULONG IoCtlBufferLength = (sizeof(PACKET_OID_DATA)+ETH_802_3_ADDRESS_LENGTH*(count+1)-1);

				LPPACKET lpPacket2 = PacketAllocatePacket( AdapterObject, IoCtlBufferLength );
#define OidData2	((PPACKET_OID_DATA)(lpPacket2->Buffer))
				if ( lpPacket2 ) {
					OidData2->Oid = OID_802_3_MULTICAST_LIST;

					// OidData2->Length = ETH_802_3_ADDRESS_LENGTH*1;
					// memcpy( OidData2->Data, address, ETH_802_3_ADDRESS_LENGTH );

					memcpy( OidData2->Data, OidData->Data, ETH_802_3_ADDRESS_LENGTH*count );
					memcpy( OidData2->Data+ETH_802_3_ADDRESS_LENGTH*count, address, ETH_802_3_ADDRESS_LENGTH );
					OidData2->Length = ETH_802_3_ADDRESS_LENGTH*(count+1);

					Status = PacketRequest( AdapterObject, lpPacket2, TRUE );
					PacketFreePacket( lpPacket2 );
				}
#undef OidData2
			}
		}
		PacketFreePacket( lpPacket );
	}

	#undef OidData

	// return Status;
	return TRUE;
}

// It seems that the last multicast address is never deleted. Why?
// Don't know the reason, but luckily this is not fatal.
// Hard to examine return codes. See NE2000 sources, always returns ok.

BOOLEAN PacketDelMulticast( LPADAPTER AdapterObject, LPBYTE address )
{
	BOOLEAN Status = FALSE;
	LPBYTE		 p;
	int				 i, count;
	LPPACKET lpPacket, lpPacket2;

	D(bug("PacketDelMulticast\n"));

	if(packet_filter & (NDIS_PACKET_TYPE_ALL_MULTICAST|NDIS_PACKET_TYPE_PROMISCUOUS)) {
		D(bug("PacketDelMulticast: already listening for all multicast\n"));
		return TRUE;
	}

	lpPacket = PacketQueryOid( AdapterObject, OID_802_3_MULTICAST_LIST, MAX_MULTICAST_SZ );
#define OidData	((PPACKET_OID_DATA)(lpPacket->Buffer))

	if(lpPacket) {
		count = OidData->Length / ETH_802_3_ADDRESS_LENGTH;

		D(bug("PacketDelMulticast: %d old addresses\n",count));

		Status = FALSE;

		p = (LPBYTE)OidData->Data;

		for( i=0; i<count; i++ ) {
			int tail_len;
			if(memcmp(p,address,ETH_802_3_ADDRESS_LENGTH) == 0) {
				D(bug("PacketDelMulticast: address found, deleting\n"));
				ULONG IoCtlBufferLength = (sizeof(PACKET_OID_DATA)+ETH_802_3_ADDRESS_LENGTH*(count-1)-1);
				lpPacket2 = PacketAllocatePacket( AdapterObject, IoCtlBufferLength );
#define OidData2	((PPACKET_OID_DATA)(lpPacket2->Buffer))
				if ( lpPacket2 ) {
					OidData2->Oid = OID_802_3_MULTICAST_LIST;
					OidData2->Length = ETH_802_3_ADDRESS_LENGTH*(count-1);
					tail_len = ETH_802_3_ADDRESS_LENGTH * (count-i-1);
					if(tail_len) memmove( p, p+ETH_802_3_ADDRESS_LENGTH, tail_len );
					if(OidData2->Length) memcpy( OidData2->Data, OidData->Data, OidData2->Length );
					if(count == 1) memset( OidData2->Data, 0, ETH_802_3_ADDRESS_LENGTH ); // eh...
					Status = PacketRequest( AdapterObject, lpPacket2, TRUE );
					PacketFreePacket( lpPacket2 );
					D(bug("PacketDelMulticast: PacketRequest returned status 0x%X, last error = 0x%X\n",Status,GetLastError()));
				}
				break;
#undef OidData2
			}
			p += ETH_802_3_ADDRESS_LENGTH;
		}
		if( i == count ) {
			D(bug("PacketDelMulticast: cannot delete, was not defined\n"));
		}
		PacketFreePacket( lpPacket );
#undef OidData
	}

	// return Status;
	return TRUE;
}

BOOLEAN PacketSetFilter( LPADAPTER AdapterObject, ULONG Filter )
{
	BOOLEAN    Status;
	ULONG		IoCtlBufferLength = (sizeof(PACKET_OID_DATA)+sizeof(ULONG)-1);
	LPPACKET	lpPacket;

	lpPacket = PacketAllocatePacket( AdapterObject, IoCtlBufferLength );
#define lpOidData	((PPACKET_OID_DATA)(lpPacket->Buffer))

	if ( lpPacket ) {
		lpOidData->Oid = OID_GEN_CURRENT_PACKET_FILTER;
		lpOidData->Length = sizeof(ULONG);
		*((PULONG)lpOidData->Data) = Filter;
		Status = PacketRequest( AdapterObject, lpPacket, TRUE );
		PacketFreePacket( lpPacket );
	} else {
		Status = FALSE;
	}

#undef lpOidData

	return Status;
}

BOOLEAN StartPacketDriver( LPCTSTR ServiceName )
{
  BOOLEAN Status = FALSE;

  SC_HANDLE  SCManagerHandle;
  SC_HANDLE  SCServiceHandle;

  SCManagerHandle = OpenSCManager(
                    NULL,
                    NULL,
                    SC_MANAGER_ALL_ACCESS);

  if(SCManagerHandle == NULL) {
    D(bug("Could not open Service Control Manager\r\n"));
  } else {
    SCServiceHandle = OpenService(SCManagerHandle,ServiceName,SERVICE_START);
    if (SCServiceHandle == NULL) {
      D(bug(TEXT("Could not open service %s\r\n"),ServiceName));
    } else {
			Status = StartService( SCServiceHandle, 0, NULL );
			if(!Status) {
				if (GetLastError()==ERROR_SERVICE_ALREADY_RUNNING) {
					Status = TRUE;
				}
			}
			BOOL waiting = TRUE;
			// loop until the service is fully started.
      while (waiting) {
		    SERVICE_STATUS ServiceStatus;
        if (QueryServiceStatus(SCServiceHandle, &ServiceStatus)) {
					switch(ServiceStatus.dwCurrentState) {
						case SERVICE_RUNNING:
							waiting = FALSE;
							Status = TRUE;
							break;
						case SERVICE_START_PENDING:
							Sleep(500);
							break;
						default:
							waiting = FALSE;
							break;
					}
				} else {
					waiting = FALSE;
				}
		  }
		  CloseServiceHandle(SCServiceHandle);
		}
		CloseServiceHandle(SCManagerHandle);
  }
  return Status;
}

ULONG PacketGetAdapterNames( LPADAPTER lpAdapter, LPTSTR pStr, PULONG BufferSize )
{
  LONG Status;

	HKEY hKey;
	DWORD RegType;

	Status = RegOpenKey(
			HKEY_LOCAL_MACHINE,
			TEXT("SYSTEM\\CurrentControlSet\\Services\\B2Ether\\Linkage"),
			&hKey
	);
	if( Status == ERROR_SUCCESS ) {
		Status = RegQueryValueEx(
								hKey,
								TEXT("Export"),
								NULL,
								&RegType,
								(LPBYTE)pStr,
								BufferSize
								);
		RegCloseKey(hKey);
	}

  return Status;
}

#ifdef __cplusplus
}
#endif

#if DEBUG
#pragma optimize("",on)
#endif
