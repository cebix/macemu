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


NTSTATUS PacketWrite(
  IN PDEVICE_OBJECT DeviceObject,
  IN PIRP Irp
)
{
  POPEN_INSTANCE      Open;
  PIO_STACK_LOCATION IrpSp;
  PNDIS_PACKET       pPacket;

  NDIS_STATUS     Status;

  IF_LOUD(DbgPrint("Packet: SendAdapter\n");)

  IrpSp = IoGetCurrentIrpStackLocation(Irp);

  Open = IrpSp->FileObject->FsContext;

  //  Try to get a packet from our list of free ones
  NdisAllocatePacket( &Status, &pPacket, Open->PacketPool );
  if (Status != NDIS_STATUS_SUCCESS) {
    Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_UNSUCCESSFUL;
  }

  RESERVED(pPacket)->Irp=Irp;

  //  Attach the writes buffer to the packet
  NdisChainBufferAtFront(pPacket,Irp->MdlAddress);

  IoMarkIrpPending(Irp);
  Irp->IoStatus.Status = STATUS_PENDING;

  //  Call the MAC
  NdisSend( &Status, Open->AdapterHandle, pPacket );

  if (Status != NDIS_STATUS_PENDING) {
      PacketSendComplete( Open, pPacket, Status );
  }

  return(STATUS_PENDING);
}


VOID PacketSendComplete(
    IN NDIS_HANDLE   ProtocolBindingContext,
    IN PNDIS_PACKET  pPacket,
    IN NDIS_STATUS   Status
)
{
  PIRP Irp;

  IF_LOUD(DbgPrint("Packet: SendComplete\n");)

  Irp = RESERVED(pPacket)->Irp;

  //  recyle the packet
  NdisReinitializePacket(pPacket);

  //  Put the packet back on the free list
  NdisFreePacket(pPacket);

  Irp->IoStatus.Status = Status;

	// a known bug, but I don't need this information
  Irp->IoStatus.Information = 0;

  IoCompleteRequest(Irp, IO_NO_INCREMENT);
}
