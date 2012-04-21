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

//#include "stdarg.h"
#include "ntddk.h"
#include "ntiologc.h"
#include "ndis.h"
#include "b2ether.h"

#undef DBG
#define DBG 0
#include "debug.h"

static UINT            Medium;
static NDIS_MEDIUM     MediumArray=NdisMedium802_3;

NTSTATUS PacketOpen( IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp )
/*
    This is the dispatch routine for create/open and close requests.
    These requests complete successfully.
*/
{
  PDEVICE_EXTENSION DeviceExtension;

  POPEN_INSTANCE    Open;

  PIO_STACK_LOCATION  IrpSp;

  NDIS_STATUS     Status;
  NDIS_STATUS     ErrorStatus;

  UINT            i;

  IF_LOUD(DbgPrint("Packet: OpenAdapter\n");)

  DeviceExtension = DeviceObject->DeviceExtension;

  IrpSp = IoGetCurrentIrpStackLocation(Irp);

  Open = ExAllocatePool(NonPagedPool,sizeof(OPEN_INSTANCE));
  if (Open==NULL) {
      Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		  Irp->IoStatus.Information = 0;
		  IoCompleteRequest(Irp, IO_NO_INCREMENT);
      return STATUS_INSUFFICIENT_RESOURCES;
  }

  RtlZeroMemory( Open, sizeof(OPEN_INSTANCE) );

  //  Save or open here
  IrpSp->FileObject->FsContext = Open;
  Open->DeviceExtension = DeviceExtension;
  Open->OpenCloseIrp = Irp;

  //  Allocate a packet pool for our xmit and receive packets
  NdisAllocatePacketPool(
      &Status,
      &Open->PacketPool,
      TRANSMIT_PACKETS,
      sizeof(PACKET_RESERVED));

  if (Status != NDIS_STATUS_SUCCESS) {
      IF_LOUD(DbgPrint("Packet: Failed to allocate packet pool\n");)
      ExFreePool(Open);
      Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		  Irp->IoStatus.Information = 0;
		  IoCompleteRequest(Irp, IO_NO_INCREMENT);
      return STATUS_INSUFFICIENT_RESOURCES;
  }

  //  list to hold irp's want to reset the adapter
  InitializeListHead(&Open->ResetIrpList);

  //  Initialize list for holding pending read requests
  KeInitializeSpinLock(&Open->RcvQSpinLock);
  InitializeListHead(&Open->RcvList);

  //  Initialize the request list
  KeInitializeSpinLock(&Open->RequestSpinLock);
  InitializeListHead(&Open->RequestList);

  //  link up the request stored in our open block
  for ( i=0; i<MAX_REQUESTS; i++ ) {
      ExInterlockedInsertTailList(
          &Open->RequestList,
          &Open->Requests[i].ListElement,
          &Open->RequestSpinLock);

  }

  IoMarkIrpPending(Irp);
  Irp->IoStatus.Status = STATUS_PENDING;

  //  Try to open the MAC
  NdisOpenAdapter(
      &Status,
      &ErrorStatus,
      &Open->AdapterHandle,
      &Medium,
      &MediumArray,
      1,
      DeviceExtension->NdisProtocolHandle,
      Open,
      &DeviceExtension->AdapterName,
      0,
      NULL);

  if (Status != NDIS_STATUS_PENDING) {
    PacketOpenAdapterComplete( Open, Status, NDIS_STATUS_SUCCESS );
  }
  return(STATUS_PENDING);
}


VOID PacketOpenAdapterComplete(
  IN NDIS_HANDLE  ProtocolBindingContext,
  IN NDIS_STATUS  Status,
  IN NDIS_STATUS  OpenErrorStatus
)
{
  PIRP              Irp;
  POPEN_INSTANCE    Open;

  IF_LOUD(DbgPrint("Packet: OpenAdapterComplete\n");)

  Open = (POPEN_INSTANCE)ProtocolBindingContext;

  Irp = Open->OpenCloseIrp;

  if (Status != NDIS_STATUS_SUCCESS) {
      IF_LOUD(DbgPrint("Packet: OpenAdapterComplete-FAILURE\n");)
      NdisFreePacketPool(Open->PacketPool);
      ExFreePool(Open);
  }

  Irp->IoStatus.Status = Status;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);

  return;
}


NTSTATUS PacketClose( IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp )
{
  POPEN_INSTANCE    Open;
  NDIS_STATUS     Status;
  PIO_STACK_LOCATION  IrpSp;

  IF_LOUD(DbgPrint("Packet: CloseAdapter\n");)

  IrpSp = IoGetCurrentIrpStackLocation(Irp);
  Open = IrpSp->FileObject->FsContext;
  Open->OpenCloseIrp =Irp;

  IoMarkIrpPending(Irp);
  Irp->IoStatus.Status = STATUS_PENDING;

  NdisCloseAdapter( &Status, Open->AdapterHandle );

  if (Status != NDIS_STATUS_PENDING) {
      PacketCloseAdapterComplete( Open, Status );
  }

  return(STATUS_PENDING);
}

VOID PacketCloseAdapterComplete(
  IN NDIS_HANDLE  ProtocolBindingContext,
  IN NDIS_STATUS  Status
)
{
  POPEN_INSTANCE    Open;
  PIRP              Irp;

  IF_LOUD(DbgPrint("Packet: CloseAdapterComplete\n");)

  Open = (POPEN_INSTANCE)ProtocolBindingContext;
  Irp = Open->OpenCloseIrp;

  NdisFreePacketPool(Open->PacketPool);
  ExFreePool(Open);

  Irp->IoStatus.Status = STATUS_SUCCESS;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
}


NTSTATUS PacketCleanup(
  IN PDEVICE_OBJECT DeviceObject,
  IN PIRP FlushIrp
)
{
  POPEN_INSTANCE      Open;
  PIO_STACK_LOCATION  IrpSp;
  PLIST_ENTRY         PacketListEntry;
  PNDIS_PACKET        pPacket;
  NDIS_STATUS         Status;

  IF_LOUD(DbgPrint("Packet: Cleanup\n");)

  IrpSp = IoGetCurrentIrpStackLocation(FlushIrp);

  Open=IrpSp->FileObject->FsContext;

  IoMarkIrpPending(FlushIrp);
  FlushIrp->IoStatus.Status = STATUS_PENDING;

  //
  //  The open instance of the device is about to close
  //  We need to complete all pending Irp's
  //  First we complete any pending read requests
  //
  while ((PacketListEntry=ExInterlockedRemoveHeadList(
                              &Open->RcvList,
                              &Open->RcvQSpinLock
                              )) != NULL) {

      IF_LOUD(DbgPrint("Packet: CleanUp - Completeing read\n");)

      pPacket=CONTAINING_RECORD(PacketListEntry,NDIS_PACKET,ProtocolReserved);

      //  complete normally
      PacketTransferDataComplete(
          Open,
          pPacket,
          NDIS_STATUS_SUCCESS,
          0
          );
  }

  // IoMarkIrpPending(FlushIrp);
  // FlushIrp->IoStatus.Status = STATUS_PENDING;

  //  We now place the Irp on the Reset list
  ExInterlockedInsertTailList(
          &Open->ResetIrpList,
          &FlushIrp->Tail.Overlay.ListEntry,
          &Open->RequestSpinLock);

  //  Now reset the adapter, the mac driver will complete any
  //  pending requests we have made to it.
  NdisReset( &Status, Open->AdapterHandle );

  if (Status != NDIS_STATUS_PENDING) {
      IF_LOUD(DbgPrint("Packet: Cleanup - ResetComplte being called\n");)
      PacketResetComplete( Open, Status );
  }

  return(STATUS_PENDING);
}


VOID PacketResetComplete(
  IN NDIS_HANDLE  ProtocolBindingContext,
  IN NDIS_STATUS  Status
)
{
  POPEN_INSTANCE      Open;
  PIRP                Irp;
  PLIST_ENTRY         ResetListEntry;

  IF_LOUD(DbgPrint("Packet: PacketResetComplte\n");)

  Open = (POPEN_INSTANCE)ProtocolBindingContext;

  //  remove the reset IRP from the list
  ResetListEntry=ExInterlockedRemoveHeadList(
                     &Open->ResetIrpList,
                     &Open->RequestSpinLock
                     );

#if DBG
  if (ResetListEntry == NULL) {
      DbgBreakPoint();
      return;
  }
#endif

  Irp = CONTAINING_RECORD(ResetListEntry,IRP,Tail.Overlay.ListEntry);
  Irp->IoStatus.Status = STATUS_SUCCESS;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);

  IF_LOUD(DbgPrint("Packet: PacketResetComplte exit\n");)
}
