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

#include "ntddk.h"
#include "ndis.h"
#include "b2ether.h"


NTSTATUS PacketWrite( IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp )
{
	POPEN_INSTANCE      open;
	PNDIS_PACKET        pPacket;
	NDIS_STATUS         Status;

	// DebugPrint(("SendAdapter\n"));

	open = DeviceObject->DeviceExtension;

	IoIncrement(open);

	if(!open->Bound) {
		Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
		IoDecrement(open);
		return STATUS_UNSUCCESSFUL;
	}

	NdisAllocatePacket( &Status, &pPacket, open->PacketPool );

	if (Status != NDIS_STATUS_SUCCESS) {
		Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		IoCompleteRequest (Irp, IO_NO_INCREMENT);
		IoDecrement(open);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RESERVED(pPacket)->Irp=Irp;

	NdisChainBufferAtFront(pPacket,Irp->MdlAddress);

	// Important: Since we have marked the IRP pending, we must return
	// STATUS_PENDING even we happen to complete the IRP synchronously.

	IoMarkIrpPending(Irp);

	NdisSend( &Status, open->AdapterHandle, pPacket );

	if (Status != NDIS_STATUS_PENDING) {
		PacketSendComplete( open, pPacket, Status );
	}

	return STATUS_PENDING;
}

VOID
PacketSendComplete(
	IN NDIS_HANDLE   ProtocolBindingContext,
	IN PNDIS_PACKET  pPacket,
	IN NDIS_STATUS   Status
)
{
	PIRP                irp;
	PIO_STACK_LOCATION  irpSp;

	// DebugPrint(("Packet: SendComplete :%x\n", Status));

	irp = RESERVED(pPacket)->Irp;
	irpSp = IoGetCurrentIrpStackLocation(irp);

	NdisFreePacket(pPacket);

	if(Status == NDIS_STATUS_SUCCESS) {
		irp->IoStatus.Information = irpSp->Parameters.Write.Length;
		irp->IoStatus.Status = STATUS_SUCCESS;
	} else {
		irp->IoStatus.Information = 0;
		irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
	}

	IoCompleteRequest(irp, IO_NO_INCREMENT);
	IoDecrement((POPEN_INSTANCE)ProtocolBindingContext);
}
