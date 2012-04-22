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

NTSTATUS PacketOpen( IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp )
{
	POPEN_INSTANCE      open;
	NTSTATUS            status = STATUS_SUCCESS;

	// DebugPrint(("OpenAdapter\n"));

	if(DeviceObject == Globals.ControlDeviceObject) {
		Irp->IoStatus.Status = status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return status;
	}

	open = DeviceObject->DeviceExtension;

	// DebugPrint(("AdapterName :%ws\n", open->AdapterName.Buffer));

	IoIncrement(open);

	if(!open->Bound) {
		status = STATUS_DEVICE_NOT_READY;
	}

	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = status;
	IoCompleteRequest (Irp, IO_NO_INCREMENT);
	IoDecrement(open);
	return status;
}


NTSTATUS PacketClose( IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp )
{
	POPEN_INSTANCE      open;
	NTSTATUS            status = STATUS_SUCCESS;

	// DebugPrint(("CloseAdapter \n"));

	if(DeviceObject == Globals.ControlDeviceObject) {
		Irp->IoStatus.Status = status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return status;
	}

	open = DeviceObject->DeviceExtension;
	IoIncrement(open);
	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = status;
	IoCompleteRequest (Irp, IO_NO_INCREMENT);
	IoDecrement(open);
	return status;
}


NTSTATUS PacketCleanup( IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp )
{
	POPEN_INSTANCE      open;
	NTSTATUS            status = STATUS_SUCCESS;

	// DebugPrint(("Packet: Cleanup\n"));

	if(DeviceObject == Globals.ControlDeviceObject) {
		Irp->IoStatus.Status = status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return status;
	}

	open = DeviceObject->DeviceExtension;

	IoIncrement(open);

	PacketCancelReadIrps(DeviceObject);

	// Since the current implementation of NDIS doesn't
	// allow us to cancel requests pending at the
	// minport, we must wait here until they complete.

	IoDecrement(open);

	NdisWaitEvent(&open->CleanupEvent, 0);

	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = status;
	IoCompleteRequest (Irp, IO_NO_INCREMENT);
	return status;
}


VOID
PacketResetComplete(
	IN NDIS_HANDLE  ProtocolBindingContext,
	IN NDIS_STATUS  Status
)
{
	POPEN_INSTANCE      open;
	PIRP                irp;

	PLIST_ENTRY         resetListEntry;

	// DebugPrint(("PacketResetComplte\n"));

	open= (POPEN_INSTANCE)ProtocolBindingContext;

	resetListEntry=ExInterlockedRemoveHeadList(
										 &open->ResetIrpList,
										 &open->ResetQueueLock
										 );

#if DBG
	if (resetListEntry == NULL) {
			DbgBreakPoint();
			return;
	}
#endif

	irp=CONTAINING_RECORD(resetListEntry,IRP,Tail.Overlay.ListEntry);

	if(Status == NDIS_STATUS_SUCCESS) {
			irp->IoStatus.Status = STATUS_SUCCESS;
	} else {
			irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
	}

	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	IoDecrement(open);

	// DebugPrint(("PacketResetComplte exit\n"));
}
