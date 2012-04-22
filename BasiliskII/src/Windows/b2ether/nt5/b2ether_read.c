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


NTSTATUS PacketRead( IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp )
{
	POPEN_INSTANCE      open;
	PNDIS_PACKET        pPacket;
	NDIS_STATUS         status;
	NTSTATUS            ntStatus;
	PIO_STACK_LOCATION  irpSp;

	// DebugPrint(("Read\n"));

	open = DeviceObject->DeviceExtension;

	IoIncrement(open);

	if(!open->Bound) {
		ntStatus = STATUS_DEVICE_NOT_READY;
		goto ERROR;
	}

	irpSp = IoGetCurrentIrpStackLocation(Irp);

	if (irpSp->Parameters.Read.Length < ETHERNET_HEADER_LENGTH) {
		ntStatus = STATUS_BUFFER_TOO_SMALL;
		goto ERROR;
	}

	NdisAllocatePacket( &status, &pPacket, open->PacketPool );
	if (status != NDIS_STATUS_SUCCESS) {
		// DebugPrint(("Packet: Read- No free packets\n"));
		ntStatus = STATUS_INSUFFICIENT_RESOURCES;
		goto ERROR;
	}

	RESERVED(pPacket)->Irp=Irp;
	RESERVED(pPacket)->pMdl=NULL;
	IoMarkIrpPending(Irp);

	IoSetCancelRoutine(Irp, PacketCancelRoutine);

	ExInterlockedInsertTailList(
			&open->RcvList,
			&RESERVED(pPacket)->ListElement,
			&open->RcvQSpinLock);

	return STATUS_PENDING;

ERROR:
	Irp->IoStatus.Status = ntStatus;
	IoCompleteRequest (Irp, IO_NO_INCREMENT);
	IoDecrement(open);
	return ntStatus;
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
	POPEN_INSTANCE      open;
	PIO_STACK_LOCATION  irpSp;
	PIRP                irp;
	PLIST_ENTRY         packetListEntry;
	PNDIS_PACKET        pPacket;
	ULONG               sizeToTransfer;
	NDIS_STATUS         status;
	UINT                bytesTransfered = 0;
	ULONG               bufferLength;
	PPACKET_RESERVED    reserved;
	PMDL                pMdl;

	// DebugPrint(("ReceiveIndicate\n"));

	open= (POPEN_INSTANCE)ProtocolBindingContext;

	if (HeaderBufferSize > ETHERNET_HEADER_LENGTH) {
			return NDIS_STATUS_SUCCESS;
	}

	//  See if there are any pending read that we can satisfy
	packetListEntry = ExInterlockedRemoveHeadList( &open->RcvList, &open->RcvQSpinLock );

	if (packetListEntry == NULL) {
		// DebugPrint(("No pending read, dropping packets\n"));
		return NDIS_STATUS_NOT_ACCEPTED;
	}

	reserved = CONTAINING_RECORD(packetListEntry,PACKET_RESERVED,ListElement);
	pPacket = CONTAINING_RECORD(reserved,NDIS_PACKET,ProtocolReserved);

	irp = RESERVED(pPacket)->Irp;
	irpSp = IoGetCurrentIrpStackLocation(irp);

	// We don't have to worry about the situation where the IRP is cancelled
	// after we remove it from the queue and before we reset the cancel
	// routine because the cancel routine has been coded to cancel an IRP
	// only if it's in the queue.

	IoSetCancelRoutine(irp, NULL);

	bufferLength = irpSp->Parameters.Read.Length-ETHERNET_HEADER_LENGTH;

		sizeToTransfer = (PacketSize < bufferLength) ? PacketSize : bufferLength;

	NdisMoveMappedMemory(
			MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority),
			HeaderBuffer,
			HeaderBufferSize
			);

	pMdl=IoAllocateMdl(
						MmGetMdlVirtualAddress(irp->MdlAddress),
						MmGetMdlByteCount(irp->MdlAddress),
						FALSE,
						FALSE,
						NULL
						);

	if (pMdl == NULL) {
		// DebugPrint(("Packet: Read-Failed to allocate Mdl\n"));
		status = NDIS_STATUS_RESOURCES;
		goto ERROR;
	}

	IoBuildPartialMdl(
			irp->MdlAddress,
			pMdl,
			((PUCHAR)MmGetMdlVirtualAddress(irp->MdlAddress))+ETHERNET_HEADER_LENGTH,
			0
			);

	pMdl->Next = NULL;

	RESERVED(pPacket)->pMdl=pMdl;

	NdisChainBufferAtFront(pPacket,pMdl);

	NdisTransferData(
			&status,
			open->AdapterHandle,
			MacReceiveContext,
			0,
			sizeToTransfer,
			pPacket,
			&bytesTransfered
	);

	if (status == NDIS_STATUS_PENDING) {
		return NDIS_STATUS_SUCCESS;
	}

ERROR:
	PacketTransferDataComplete( open, pPacket, status, bytesTransfered );
	return NDIS_STATUS_SUCCESS;
}


VOID
PacketTransferDataComplete (
	IN NDIS_HANDLE   ProtocolBindingContext,
	IN PNDIS_PACKET  pPacket,
	IN NDIS_STATUS   Status,
	IN UINT          BytesTransfered
)
{
	PIO_STACK_LOCATION   irpSp;
	POPEN_INSTANCE       open;
	PIRP                 irp;
	PMDL                 pMdl;

	// DebugPrint(("Packet: TransferDataComplete\n"));

	open = (POPEN_INSTANCE)ProtocolBindingContext;
	irp = RESERVED(pPacket)->Irp;
	irpSp = IoGetCurrentIrpStackLocation(irp);
	pMdl = RESERVED(pPacket)->pMdl;


	if(pMdl) IoFreeMdl(pMdl);

	NdisFreePacket(pPacket);

	if(Status == NDIS_STATUS_SUCCESS) {
		irp->IoStatus.Status = STATUS_SUCCESS;
		irp->IoStatus.Information = BytesTransfered+ETHERNET_HEADER_LENGTH;
	} else {
		irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
		irp->IoStatus.Information = 0;
	}

	// DebugPrint(("BytesTransfered:%d\n", irp->IoStatus.Information));

	IoCompleteRequest(irp, IO_NO_INCREMENT);
	IoDecrement(open);
}

VOID PacketReceiveComplete( IN NDIS_HANDLE ProtocolBindingContext )
{
}

INT
PacketReceivePacket(
	IN    NDIS_HANDLE         ProtocolBindingContext,
	IN    PNDIS_PACKET        Packet
)
{
	UINT                bytesTransfered = 0;
	POPEN_INSTANCE      open;
	PIRP                irp;
	PNDIS_PACKET        myPacket;
	PLIST_ENTRY         packetListEntry;
	ULONG               bufferLength;
	PPACKET_RESERVED    reserved;
	PIO_STACK_LOCATION  irpSp;
	PMDL                mdl;
	PVOID               startAddress;
	NTSTATUS           status;

	// DebugPrint(("PacketReceivePacket\n"));

	open = (POPEN_INSTANCE)ProtocolBindingContext;

	packetListEntry = ExInterlockedRemoveHeadList(
											&open->RcvList,
											&open->RcvQSpinLock
											);

	if (packetListEntry == NULL) {
		// DebugPrint(("No pending read, dropping packets\n"));
		return 0;
	}

	reserved = CONTAINING_RECORD(packetListEntry,PACKET_RESERVED,ListElement);
	myPacket = CONTAINING_RECORD(reserved,NDIS_PACKET,ProtocolReserved);

	irp = RESERVED(myPacket)->Irp;
	irpSp = IoGetCurrentIrpStackLocation(irp);

	// We don't have to worry about the situation where the IRP is cancelled
	// after we remove it from the queue and before we reset the cancel
	// routine because the cancel routine has been coded to cancel an IRP
	// only if it's in the queue.

	IoSetCancelRoutine(irp, NULL);

	// Following block of code locks the destination packet
	// MDLs in a safe manner. This is a temporary workaround
	// for NdisCopyFromPacketToPacket that currently doesn't use
	// safe functions to lock pages of MDL. This is required to
	// prevent system from bugchecking under low memory resources.
	//
	{
		PVOID           virtualAddress;
		PNDIS_BUFFER    firstBuffer, nextBuffer;
		ULONG           totalLength;

		NdisQueryPacket(Packet, NULL, NULL, &firstBuffer, &totalLength);
		while( firstBuffer ) {
			NdisQueryBufferSafe( firstBuffer, &virtualAddress, &totalLength, NormalPagePriority );
			if(!virtualAddress) {
				status = STATUS_INSUFFICIENT_RESOURCES;
				goto CleanExit;
			}
			NdisGetNextBuffer(firstBuffer,  &nextBuffer);
			firstBuffer = nextBuffer;
		}
	}

	NdisChainBufferAtFront( myPacket, irp->MdlAddress );
	bufferLength=irpSp->Parameters.Read.Length;
	NdisCopyFromPacketToPacket( myPacket, 0, bufferLength, Packet, 0,  &bytesTransfered );

CleanExit:
	NdisFreePacket(myPacket);
	irp->IoStatus.Status = status;
	irp->IoStatus.Information = bytesTransfered;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	// DebugPrint(("BytesTransfered:%d\n", bytesTransfered));
	IoDecrement(open);
	return 0;
}


VOID
PacketCancelRoutine (
	IN PDEVICE_OBJECT   DeviceObject,
	IN PIRP             Irp
)

{
	POPEN_INSTANCE      open = DeviceObject->DeviceExtension;
	KIRQL               oldIrql;
	PIRP                irpToComplete = NULL;
	PLIST_ENTRY         thisEntry, listHead;
	PIRP                pendingIrp;
	PNDIS_PACKET        myPacket = NULL;
	PPACKET_RESERVED    reserved;
	PMDL                mdl;

	// Don't assume that the IRP being cancelled is in the queue.
	// Only complete the IRP if it IS in the queue.
	//
	// Must acquire the local spinlock before releasing
	// the global cancel spinlock
	//
	// DebugPrint(("PacketCancelRoutine\n"));

	oldIrql = Irp->CancelIrql;

	// One should not intermix KeAcquireSpinLock(AtDpcLevel)
	// and ExInterlocked...List() functions on the same spinlock if the
	// routines that use the lock run at IRQL > DISPATCH_LEVEL.
	// After acquiring the lock using Ke function, if we got interrupted
	// and entered into an ISR and tried to manipulate the list using
	// ExInterlocked...List function with the same lock, we deadlock.
	// In this sample we can safely do that because none of our routines
	// will be called at IRQL > DISPATCH_LEVEL.

	KeAcquireSpinLockAtDpcLevel(&open->RcvQSpinLock);
	IoReleaseCancelSpinLock( KeGetCurrentIrql() );

	listHead = &open->RcvList;
	for( thisEntry = listHead->Flink; thisEntry != listHead; thisEntry = thisEntry->Flink ) {
		reserved=CONTAINING_RECORD(thisEntry,PACKET_RESERVED,ListElement);
		myPacket=CONTAINING_RECORD(reserved,NDIS_PACKET,ProtocolReserved);
		pendingIrp = RESERVED(myPacket)->Irp;
		if (pendingIrp == Irp) {
			RemoveEntryList(thisEntry);
			irpToComplete = pendingIrp;
			break;
		}
	}

	KeReleaseSpinLock(&open->RcvQSpinLock, oldIrql);

	if(irpToComplete) {
		// DebugPrint(("Cancelling IRP\n"));
		// ASSERT(myPacket);

		NdisFreePacket(myPacket);

		irpToComplete->IoStatus.Status = STATUS_CANCELLED;
		irpToComplete->IoStatus.Information = 0;
		IoCompleteRequest(irpToComplete, IO_NO_INCREMENT);
		IoDecrement(open);
	}
}


NTSTATUS PacketCancelReadIrps( IN PDEVICE_OBJECT DeviceObject )
{
	POPEN_INSTANCE      open = DeviceObject->DeviceExtension;
	PLIST_ENTRY         thisEntry;
	PIRP                pendingIrp;
	PNDIS_PACKET        myPacket = NULL;
	PPACKET_RESERVED    reserved;
	PMDL                mdl;

	// DebugPrint(("PacketCancelReadIrps\n"));

	// Walk through the RcvList and cancel all read IRPs.

	while( thisEntry = ExInterlockedRemoveHeadList( &open->RcvList, &open->RcvQSpinLock )) {
		reserved=CONTAINING_RECORD(thisEntry,PACKET_RESERVED,ListElement);
		myPacket=CONTAINING_RECORD(reserved,NDIS_PACKET,ProtocolReserved);

		ASSERT(myPacket);

		pendingIrp = RESERVED(myPacket)->Irp;

		NdisFreePacket(myPacket);

		// DebugPrint(("Cancelled : 0%0x\n", pendingIrp));

		IoSetCancelRoutine(pendingIrp, NULL);

		pendingIrp->IoStatus.Information = 0;
		pendingIrp->IoStatus.Status = STATUS_CANCELLED;
		IoCompleteRequest(pendingIrp, IO_NO_INCREMENT);
		IoDecrement(open);
	}

	return STATUS_SUCCESS;
}
