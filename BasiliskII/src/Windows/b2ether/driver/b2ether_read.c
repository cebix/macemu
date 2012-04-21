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

#include "stdarg.h"
#include "ntddk.h"
#include "ntiologc.h"
#include "ndis.h"
#include "b2ether.h"

#undef DBG
#define DBG 0
#include "debug.h"


NTSTATUS PacketRead(
  IN PDEVICE_OBJECT DeviceObject,
  IN PIRP Irp
)
{
  POPEN_INSTANCE      Open;
  PIO_STACK_LOCATION  IrpSp;
  PNDIS_PACKET        pPacket;
  PMDL                pMdl;
  NDIS_STATUS         Status;

  IF_LOUD(DbgPrint("Packet: Read\n");)

  IrpSp = IoGetCurrentIrpStackLocation(Irp);

  Open = IrpSp->FileObject->FsContext;

  if (IrpSp->Parameters.Read.Length < ETHERNET_HEADER_LENGTH) {
      Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
		  Irp->IoStatus.Information = 0;
		  IoCompleteRequest(Irp, IO_NO_INCREMENT);
      return STATUS_UNSUCCESSFUL;
  }

  pMdl=IoAllocateMdl(
            MmGetMdlVirtualAddress(Irp->MdlAddress),
            MmGetMdlByteCount(Irp->MdlAddress),
            FALSE,
            FALSE,
            NULL
            );

  if (!pMdl) {
      IF_LOUD(DbgPrint("Packet: Read-Failed to allocate Mdl\n");)
      Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
		  Irp->IoStatus.Information = 0;
		  IoCompleteRequest(Irp, IO_NO_INCREMENT);
      return STATUS_UNSUCCESSFUL;
  }

  IoBuildPartialMdl(
      Irp->MdlAddress,
      pMdl,
      ((PUCHAR)MmGetMdlVirtualAddress(Irp->MdlAddress))+ETHERNET_HEADER_LENGTH,
      0
      );
  pMdl->Next = NULL;

  //
  //  Try to get a packet from our list of free ones
  //
  NdisAllocatePacket( &Status, &pPacket, Open->PacketPool );

  if (Status != NDIS_STATUS_SUCCESS) {
      IF_LOUD(DbgPrint("Packet: Read- No free packets\n");)
      IoFreeMdl(pMdl);
      Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
		  Irp->IoStatus.Information = 0;
		  IoCompleteRequest(Irp, IO_NO_INCREMENT);
      return STATUS_UNSUCCESSFUL;
  }

  //
  //  Get a pointer to the packet itself
  //
  RESERVED(pPacket)->Irp = Irp;
  RESERVED(pPacket)->pMdl = pMdl;

  IoMarkIrpPending(Irp);
  Irp->IoStatus.Status = STATUS_PENDING;

  //
  //  Attach our new MDL to the packet
  //
  NdisChainBufferAtFront(pPacket,pMdl);


	//
	//  Put this packet in a list of pending reads.
	//  The receive indication handler will attemp to remove packets
	//  from this list for use in transfer data calls
	//
	ExInterlockedInsertTailList(
			&Open->RcvList,
			&RESERVED(pPacket)->ListElement,
			&Open->RcvQSpinLock);

  return(STATUS_PENDING);
}


NDIS_STATUS
PacketReceiveIndicate (
    IN NDIS_HANDLE ProtocolBindingContext,
    IN NDIS_HANDLE MacReceiveContext,
    IN PVOID       HeaderBuffer,
    IN UINT        HeaderBufferSize,
    IN PVOID       LookAheadBuffer,
    IN UINT        LookaheadBufferSize,
    IN UINT        PacketSize
    )

{
  POPEN_INSTANCE      Open;
  PIO_STACK_LOCATION  IrpSp;
  PIRP                Irp;
  PLIST_ENTRY         PacketListEntry;
  PNDIS_PACKET        pPacket;
  ULONG               SizeToTransfer;
  NDIS_STATUS         Status;
  UINT                BytesTransfered;
  ULONG               BufferLength;
  PPACKET_RESERVED    Reserved;

  IF_LOUD(DbgPrint("Packet: ReceiveIndicate\n");)

  Open = (POPEN_INSTANCE)ProtocolBindingContext;

  if (HeaderBufferSize > ETHERNET_HEADER_LENGTH) {
    return NDIS_STATUS_NOT_ACCEPTED; // NDIS_STATUS_SUCCESS;
  }

  //  See if there are any pending read that we can satisfy
  PacketListEntry=ExInterlockedRemoveHeadList(
                      &Open->RcvList,
                      &Open->RcvQSpinLock
                      );

  if (PacketListEntry == NULL) {
	  IF_LOUD(DbgPrint("Packet: ReceiveIndicate dropped a packet\n");)
    return NDIS_STATUS_NOT_ACCEPTED;
  }

  Reserved=CONTAINING_RECORD(PacketListEntry,PACKET_RESERVED,ListElement);
  pPacket=CONTAINING_RECORD(Reserved,NDIS_PACKET,ProtocolReserved);

  Irp=RESERVED(pPacket)->Irp;
  IrpSp = IoGetCurrentIrpStackLocation(Irp);

  //  This is the length of our partial MDL
  BufferLength = IrpSp->Parameters.Read.Length-ETHERNET_HEADER_LENGTH;

  SizeToTransfer = (PacketSize < BufferLength) ? PacketSize : BufferLength;

  //  copy the ethernet header into the actual readbuffer
  NdisMoveMappedMemory(
      MmGetSystemAddressForMdl(Irp->MdlAddress),
      HeaderBuffer,
      HeaderBufferSize
      );

  //  Call the Mac to transfer the packet
  NdisTransferData(
      &Status,
      Open->AdapterHandle,
      MacReceiveContext,
      0,
      SizeToTransfer,
      pPacket,
      &BytesTransfered);

  if (Status != NDIS_STATUS_PENDING) {
      PacketTransferDataComplete( Open, pPacket, Status, BytesTransfered );
  }

  return NDIS_STATUS_SUCCESS;
}

VOID PacketTransferDataComplete (
    IN NDIS_HANDLE   ProtocolBindingContext,
    IN PNDIS_PACKET  pPacket,
    IN NDIS_STATUS   Status,
    IN UINT          BytesTransfered
)
{
  PIO_STACK_LOCATION   IrpSp;
  POPEN_INSTANCE       Open;
  PIRP                 Irp;
  PMDL                 pMdl;

  IF_LOUD(DbgPrint("Packet: TransferDataComplete\n");)

  Open = (POPEN_INSTANCE)ProtocolBindingContext;
  Irp = RESERVED(pPacket)->Irp;
  IrpSp = IoGetCurrentIrpStackLocation(Irp);
  pMdl = RESERVED(pPacket)->pMdl;

  //  Free the MDL that we allocated
  IoFreeMdl(pMdl);

  //  recycle the packet
  NdisReinitializePacket(pPacket);

  //  Put the packet on the free queue
  NdisFreePacket(pPacket);

  Irp->IoStatus.Status = Status;
  Irp->IoStatus.Information = BytesTransfered+ETHERNET_HEADER_LENGTH;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
}


VOID PacketReceiveComplete( IN NDIS_HANDLE  ProtocolBindingContext )
{
}
