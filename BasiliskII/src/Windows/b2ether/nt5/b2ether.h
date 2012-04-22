/*
 *  b2ether driver -- derived from DDK packet driver sample
 *
 *  Basilisk II (C) 1997-1999 Christian Bauer
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

#ifndef _B2ETHER_H_
#define _B2ETHER_H_

#undef  ExAllocatePool
#define ExAllocatePool(a,b) ExAllocatePoolWithTag(a, b, 'te2B')

#if DBG
#define DebugPrint(_x_) \
                DbgPrint("B2ETHER: ");\
                DbgPrint _x_;
#else
#define DebugPrint(_x_)
#endif

#define NT_DEVICE_NAME L"\\Device\\B2ether"
#define DOS_DEVICE_NAME L"\\DosDevices\\B2ether"

typedef struct _GLOBAL {
	PDRIVER_OBJECT DriverObject;
	NDIS_HANDLE    NdisProtocolHandle;
	UNICODE_STRING RegistryPath;
	LIST_ENTRY  AdapterList;
	KSPIN_LOCK  GlobalLock;
	PDEVICE_OBJECT  ControlDeviceObject;
} GLOBAL, *PGLOBAL;

GLOBAL Globals;

typedef struct _INTERNAL_REQUEST {
  PIRP           Irp;
  NDIS_REQUEST   Request;
} INTERNAL_REQUEST, *PINTERNAL_REQUEST;

typedef struct _OPEN_INSTANCE {
	PDEVICE_OBJECT      DeviceObject;
	ULONG               IrpCount;
	NDIS_STRING         AdapterName;
	NDIS_STRING         SymbolicLink;
	NDIS_HANDLE         AdapterHandle;
	NDIS_HANDLE         PacketPool;
	KSPIN_LOCK          RcvQSpinLock;
	LIST_ENTRY          RcvList;
	NDIS_MEDIUM         Medium;
	KSPIN_LOCK          ResetQueueLock;
	LIST_ENTRY          ResetIrpList;
	NDIS_STATUS         Status;
	NDIS_EVENT          Event;
	NDIS_EVENT          CleanupEvent;
	LIST_ENTRY          AdapterListEntry;
	BOOLEAN             Bound;
	CHAR                Filler[3];
} OPEN_INSTANCE, *POPEN_INSTANCE;

typedef struct _PACKET_RESERVED {
  LIST_ENTRY     ListElement;
  PIRP           Irp;
  PMDL           pMdl;
}  PACKET_RESERVED, *PPACKET_RESERVED;


#define  ETHERNET_HEADER_LENGTH   14
#define RESERVED(_p) ((PPACKET_RESERVED)((_p)->ProtocolReserved))
#define  TRANSMIT_PACKETS    16


NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
PacketCancelReadIrps(
    IN PDEVICE_OBJECT DeviceObject
);

NTSTATUS
PacketCleanup(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
PacketBindAdapter(
    OUT PNDIS_STATUS            Status,
    IN  NDIS_HANDLE             BindContext,
    IN  PNDIS_STRING            DeviceName,
    IN  PVOID                   SystemSpecific1,
    IN  PVOID                   SystemSpecific2
    );
VOID
PacketUnbindAdapter(
    OUT PNDIS_STATUS        Status,
    IN  NDIS_HANDLE            ProtocolBindingContext,
    IN  NDIS_HANDLE            UnbindContext
    );


VOID
PacketOpenAdapterComplete(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN NDIS_STATUS  Status,
    IN NDIS_STATUS  OpenErrorStatus
    );

VOID
PacketCloseAdapterComplete(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN NDIS_STATUS  Status
    );


NDIS_STATUS
PacketReceiveIndicate(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_HANDLE MacReceiveContext,
    IN PVOID HeaderBuffer,
    IN UINT HeaderBufferSize,
    IN PVOID LookAheadBuffer,
    IN UINT LookaheadBufferSize,
    IN UINT PacketSize
    );

VOID
PacketReceiveComplete(
    IN NDIS_HANDLE  ProtocolBindingContext
    );


VOID
PacketRequestComplete(
    IN NDIS_HANDLE   ProtocolBindingContext,
    IN PNDIS_REQUEST pRequest,
    IN NDIS_STATUS   Status
    );

VOID
PacketSendComplete(
    IN NDIS_HANDLE   ProtocolBindingContext,
    IN PNDIS_PACKET  pPacket,
    IN NDIS_STATUS   Status
    );


VOID
PacketResetComplete(
    IN NDIS_HANDLE  ProtocolBindingContext,
    IN NDIS_STATUS  Status
    );


VOID
PacketStatus(
    IN NDIS_HANDLE   ProtocolBindingContext,
    IN NDIS_STATUS   Status,
    IN PVOID         StatusBuffer,
    IN UINT          StatusBufferSize
    );


VOID
PacketStatusComplete(
    IN NDIS_HANDLE  ProtocolBindingContext
    );

VOID
PacketTransferDataComplete(
    IN NDIS_HANDLE ProtocolBindingContext,
    IN PNDIS_PACKET Packet,
    IN NDIS_STATUS Status,
    IN UINT BytesTransferred
    );


NTSTATUS
PacketShutdown(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
PacketUnload(
    IN PDRIVER_OBJECT DriverObject
    );



NTSTATUS
PacketOpen(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
PacketClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
PacketWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
PacketRead(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
PacketIoControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

VOID
PacketCancelRoutine (
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp
    );

INT
PacketReceivePacket(
    IN    NDIS_HANDLE            ProtocolBindingContext,
    IN    PNDIS_PACKET        Packet
    );

NTSTATUS
PacketGetAdapterList(
    IN  PVOID              Buffer,
    IN  ULONG              Length,
    IN  OUT PULONG          DataLength
    );

NDIS_STATUS
PacketPNPHandler(
    IN    NDIS_HANDLE        ProtocolBindingContext,
    IN    PNET_PNP_EVENT    pNetPnPEvent
    );


VOID
IoIncrement (
    IN  OUT POPEN_INSTANCE  Open
    );

VOID
IoDecrement (
    IN  OUT POPEN_INSTANCE  Open
    );

#endif //_B2ETHER_H_
