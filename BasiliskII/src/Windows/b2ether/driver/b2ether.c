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
#include "ntddpack.h"
#include "b2ether.h"

#undef DBG
#define DBG 0
#include "debug.h"

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    );


NTSTATUS
PacketReadRegistry(
    IN  PWSTR              *MacDriverName,
    IN  PWSTR              *PacketDriverName,
    IN  PUNICODE_STRING     RegistryPath
    );


NTSTATUS
PacketCreateSymbolicLink(
    IN  PUNICODE_STRING  DeviceName,
    IN  BOOLEAN          Create
    );

NTSTATUS
PacketQueryRegistryRoutine(
    IN PWSTR     ValueName,
    IN ULONG     ValueType,
    IN PVOID     ValueData,
    IN ULONG     ValueLength,
    IN PVOID     Context,
    IN PVOID     EntryContext
    );


#if DBG
ULONG PacketDebugFlag = PACKET_DEBUG_LOUD;
#endif


PDEVICE_EXTENSION GlobalDeviceExtension;


NTSTATUS DriverEntry(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegistryPath
)
{
  NDIS_PROTOCOL_CHARACTERISTICS  ProtocolChar;

  UNICODE_STRING MacDriverName;
  UNICODE_STRING UnicodeDeviceName;

  PDEVICE_OBJECT DeviceObject = NULL;
  PDEVICE_EXTENSION DeviceExtension = NULL;

  NTSTATUS Status = STATUS_SUCCESS;
  NTSTATUS ErrorCode = STATUS_SUCCESS;
  NDIS_STRING ProtoName = NDIS_STRING_CONST("PacketDriver");

  ULONG          DevicesCreated=0;

  PWSTR          BindString;
  PWSTR          ExportString;

  PWSTR          BindStringSave;
  PWSTR          ExportStringSave;

  NDIS_HANDLE    NdisProtocolHandle;

  IF_LOUD(DbgPrint("\n\nPacket: DriverEntry\n");)

  RtlZeroMemory(&ProtocolChar,sizeof(NDIS_PROTOCOL_CHARACTERISTICS));

  ProtocolChar.MajorNdisVersion            = 3;
  ProtocolChar.MinorNdisVersion            = 0;
  ProtocolChar.Reserved                    = 0;
  ProtocolChar.OpenAdapterCompleteHandler  = PacketOpenAdapterComplete;
  ProtocolChar.CloseAdapterCompleteHandler = PacketCloseAdapterComplete;
  ProtocolChar.SendCompleteHandler         = PacketSendComplete;
  ProtocolChar.TransferDataCompleteHandler = PacketTransferDataComplete;
  ProtocolChar.ResetCompleteHandler        = PacketResetComplete;
  ProtocolChar.RequestCompleteHandler      = PacketRequestComplete;
  ProtocolChar.ReceiveHandler              = PacketReceiveIndicate;
  ProtocolChar.ReceiveCompleteHandler      = PacketReceiveComplete;
  ProtocolChar.StatusHandler               = PacketStatus;
  ProtocolChar.StatusCompleteHandler       = PacketStatusComplete;
  ProtocolChar.Name                        = ProtoName;

  NdisRegisterProtocol(
      &Status,
      &NdisProtocolHandle,
      &ProtocolChar,
      sizeof(NDIS_PROTOCOL_CHARACTERISTICS));

  if (Status != NDIS_STATUS_SUCCESS) {
      IF_LOUD(DbgPrint("Packet: Failed to register protocol with NDIS\n");)
      return Status;
  }

  //
  // Set up the device driver entry points.
  //

  DriverObject->MajorFunction[IRP_MJ_CREATE] = PacketOpen;
  DriverObject->MajorFunction[IRP_MJ_CLOSE]  = PacketClose;
  DriverObject->MajorFunction[IRP_MJ_READ]   = PacketRead;
  DriverObject->MajorFunction[IRP_MJ_WRITE]  = PacketWrite;
  DriverObject->MajorFunction[IRP_MJ_CLEANUP]  = PacketCleanup;
  DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]  = PacketIoControl;

  DriverObject->DriverUnload = PacketUnload;


  //
  //  Get the name of the Packet driver and the name of the MAC driver
  //  to bind to from the registry
  //

  Status=PacketReadRegistry(
             &BindString,
             &ExportString,
             RegistryPath
             );

  if (Status != STATUS_SUCCESS) {
      IF_LOUD(DbgPrint("Perf: Failed to read registry\n");)
      goto RegistryError;
  }

  BindStringSave   = BindString;
  ExportStringSave = ExportString;

  //  create a device object for each entry
  while (*BindString!= UNICODE_NULL && *ExportString!= UNICODE_NULL) {

      //  Create a counted unicode string for both null terminated strings
      RtlInitUnicodeString(
          &MacDriverName,
          BindString
          );

      RtlInitUnicodeString(
          &UnicodeDeviceName,
          ExportString
          );

      //  Advance to the next string of the MULTI_SZ string
      BindString   += (MacDriverName.Length+sizeof(UNICODE_NULL))/sizeof(WCHAR);

      ExportString += (UnicodeDeviceName.Length+sizeof(UNICODE_NULL))/sizeof(WCHAR);

      IF_LOUD(DbgPrint("Packet: DeviceName=%ws  MacName=%ws\n",UnicodeDeviceName.Buffer,MacDriverName.Buffer);)

      //  Create the device object
      Status = IoCreateDevice(
                  DriverObject,
                  sizeof(DEVICE_EXTENSION),
                  &UnicodeDeviceName,
                  FILE_DEVICE_PROTOCOL,
                  0,
                  FALSE,
                  &DeviceObject
                  );

      if (Status != STATUS_SUCCESS) {
          IF_LOUD(DbgPrint("Perf: IoCreateDevice() failed:\n");)
          break;
      }

      DevicesCreated++;

      DeviceObject->Flags |= DO_DIRECT_IO;
      DeviceExtension  =  (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
      DeviceExtension->DeviceObject = DeviceObject;

      //  Save the the name of the MAC driver to open in the Device Extension
      DeviceExtension->AdapterName=MacDriverName;

      if (DevicesCreated == 1) {
          DeviceExtension->BindString   = BindStringSave;
          DeviceExtension->ExportString = ExportStringSave;
      }

      DeviceExtension->NdisProtocolHandle=NdisProtocolHandle;
  }

  if (DevicesCreated > 0) {
      return STATUS_SUCCESS;
  }

  ExFreePool(BindStringSave);
  ExFreePool(ExportStringSave);

RegistryError:

  NdisDeregisterProtocol(
      &Status,
      NdisProtocolHandle
      );

  Status=STATUS_UNSUCCESSFUL;

  return(Status);
}


VOID PacketUnload( IN PDRIVER_OBJECT DriverObject )
{
  PDEVICE_OBJECT     DeviceObject;
  PDEVICE_OBJECT     OldDeviceObject;
  PDEVICE_EXTENSION  DeviceExtension;

  NDIS_HANDLE        NdisProtocolHandle;
  NDIS_STATUS        Status;

  IF_LOUD(DbgPrint("Packet: Unload\n");)

  DeviceObject = DriverObject->DeviceObject;

  while (DeviceObject != NULL) {
      DeviceExtension = DeviceObject->DeviceExtension;
      NdisProtocolHandle = DeviceExtension->NdisProtocolHandle;
      if (DeviceExtension->BindString != NULL) {
          ExFreePool(DeviceExtension->BindString);
      }
      if (DeviceExtension->ExportString != NULL) {
          ExFreePool(DeviceExtension->ExportString);
      }
      OldDeviceObject=DeviceObject;
      DeviceObject=DeviceObject->NextDevice;
      IoDeleteDevice(OldDeviceObject);
  }

  NdisDeregisterProtocol( &Status, NdisProtocolHandle );
}


NTSTATUS PacketIoControl( IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp )
{
  POPEN_INSTANCE      Open;
  PIO_STACK_LOCATION  IrpSp;
  PLIST_ENTRY         RequestListEntry;
  PINTERNAL_REQUEST   pRequest;
  ULONG               FunctionCode;
  NDIS_STATUS     Status;

  IF_LOUD(DbgPrint("Packet: IoControl\n");)

  IrpSp = IoGetCurrentIrpStackLocation(Irp);

  FunctionCode=IrpSp->Parameters.DeviceIoControl.IoControlCode;

  Open=IrpSp->FileObject->FsContext;

  RequestListEntry=ExInterlockedRemoveHeadList(&Open->RequestList,&Open->RequestSpinLock);
  if (RequestListEntry == NULL) {
    Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_UNSUCCESSFUL;
  }

  pRequest=CONTAINING_RECORD(RequestListEntry,INTERNAL_REQUEST,ListElement);
  pRequest->Irp=Irp;

  IoMarkIrpPending(Irp);
  Irp->IoStatus.Status = STATUS_PENDING;

  IF_LOUD(DbgPrint("Packet: Function code is %08lx  buff size=%08lx  %08lx\n",FunctionCode,IrpSp->Parameters.DeviceIoControl.InputBufferLength,IrpSp->Parameters.DeviceIoControl.OutputBufferLength);)

  if (FunctionCode == IOCTL_PROTOCOL_RESET) {
      IF_LOUD(DbgPrint("Packet: IoControl - Reset request\n");)
      ExInterlockedInsertTailList(
              &Open->ResetIrpList,
              &Irp->Tail.Overlay.ListEntry,
              &Open->RequestSpinLock);
      NdisReset( &Status, Open->AdapterHandle );

      if (Status != NDIS_STATUS_PENDING) {
          IF_LOUD(DbgPrint("Packet: IoControl - ResetComplte being called\n");)
          PacketResetComplete( Open, Status );
      }

  } else {
      //  See if it is an Ndis request
      PPACKET_OID_DATA    OidData=Irp->AssociatedIrp.SystemBuffer;

      if (((FunctionCode == IOCTL_PROTOCOL_SET_OID) || (FunctionCode == IOCTL_PROTOCOL_QUERY_OID))
          &&
          (IrpSp->Parameters.DeviceIoControl.InputBufferLength == IrpSp->Parameters.DeviceIoControl.OutputBufferLength)
          &&
          (IrpSp->Parameters.DeviceIoControl.InputBufferLength >= sizeof(PACKET_OID_DATA))
          &&
          (IrpSp->Parameters.DeviceIoControl.InputBufferLength >= sizeof(PACKET_OID_DATA)-1+OidData->Length)) {

          IF_LOUD(DbgPrint("Packet: IoControl: Request: Oid=%08lx, Length=%08lx\n",OidData->Oid,OidData->Length);)

          if (FunctionCode == IOCTL_PROTOCOL_SET_OID) {
              pRequest->Request.RequestType=NdisRequestSetInformation;
              pRequest->Request.DATA.SET_INFORMATION.Oid=OidData->Oid;
              pRequest->Request.DATA.SET_INFORMATION.InformationBuffer=OidData->Data;
              pRequest->Request.DATA.SET_INFORMATION.InformationBufferLength=OidData->Length;
          } else {
              pRequest->Request.RequestType=NdisRequestQueryInformation;
              pRequest->Request.DATA.QUERY_INFORMATION.Oid=OidData->Oid;
              pRequest->Request.DATA.QUERY_INFORMATION.InformationBuffer=OidData->Data;
              pRequest->Request.DATA.QUERY_INFORMATION.InformationBufferLength=OidData->Length;
          }
          NdisRequest(
              &Status,
              Open->AdapterHandle,
              &pRequest->Request
              );
      } else { // buffer too small
          Status=NDIS_STATUS_FAILURE;
          pRequest->Request.DATA.SET_INFORMATION.BytesRead=0;
          pRequest->Request.DATA.QUERY_INFORMATION.BytesWritten=0;
      }

      if (Status != NDIS_STATUS_PENDING) {
          IF_LOUD(DbgPrint("Packet: Calling RequestCompleteHandler\n");)
          PacketRequestComplete(
              Open,
              &pRequest->Request,
              Status
              );
      }
  }
  return(STATUS_PENDING);
}


VOID PacketRequestComplete(
  IN NDIS_HANDLE   ProtocolBindingContext,
  IN PNDIS_REQUEST NdisRequest,
  IN NDIS_STATUS   Status
)
{
  POPEN_INSTANCE      Open;
  PIO_STACK_LOCATION  IrpSp;
  PIRP                Irp;
  PINTERNAL_REQUEST   pRequest;
  UINT                FunctionCode;

  PPACKET_OID_DATA    OidData;

  IF_LOUD(DbgPrint("Packet: RequestComplete\n");)

  Open= (POPEN_INSTANCE)ProtocolBindingContext;

  pRequest=CONTAINING_RECORD(NdisRequest,INTERNAL_REQUEST,Request);
  Irp=pRequest->Irp;

  IrpSp = IoGetCurrentIrpStackLocation(Irp);

  FunctionCode=IrpSp->Parameters.DeviceIoControl.IoControlCode;

  OidData=Irp->AssociatedIrp.SystemBuffer;

  if (FunctionCode == IOCTL_PROTOCOL_SET_OID) {
      OidData->Length=pRequest->Request.DATA.SET_INFORMATION.BytesRead;
  } else {
      if (FunctionCode == IOCTL_PROTOCOL_QUERY_OID) {
          OidData->Length=pRequest->Request.DATA.QUERY_INFORMATION.BytesWritten;
      }
  }

  Irp->IoStatus.Information=IrpSp->Parameters.DeviceIoControl.InputBufferLength;

  ExInterlockedInsertTailList(
      &Open->RequestList,
      &pRequest->ListElement,
      &Open->RequestSpinLock);

  Irp->IoStatus.Status = Status;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
}


VOID PacketStatus(
  IN NDIS_HANDLE   ProtocolBindingContext,
  IN NDIS_STATUS   Status,
  IN PVOID         StatusBuffer,
  IN UINT          StatusBufferSize
)
{
  IF_LOUD(DbgPrint("Packet: Status Indication\n");)
}


VOID PacketStatusComplete( IN NDIS_HANDLE  ProtocolBindingContext )
{
  IF_LOUD(DbgPrint("Packet: StatusIndicationComplete\n");)
}


#if 0
NTSTATUS PacketCreateSymbolicLink(
  IN  PUNICODE_STRING  DeviceName,
  IN  BOOLEAN          Create
)
{
    UNICODE_STRING UnicodeDosDeviceName;
    NTSTATUS       Status;

    if (DeviceName->Length < sizeof(L"\\Device\\")) {
        return STATUS_UNSUCCESSFUL;
    }

    RtlInitUnicodeString(&UnicodeDosDeviceName,NULL);
    UnicodeDosDeviceName.MaximumLength=DeviceName->Length+sizeof(L"\\DosDevices")+sizeof(UNICODE_NULL);
    UnicodeDosDeviceName.Buffer=ExAllocatePool(
                                    NonPagedPool,
                                    UnicodeDosDeviceName.MaximumLength
                                    );
    if (UnicodeDosDeviceName.Buffer != NULL) {
        RtlZeroMemory( UnicodeDosDeviceName.Buffer, UnicodeDosDeviceName.MaximumLength );
        RtlAppendUnicodeToString( &UnicodeDosDeviceName, L"\\DosDevices\\" );
        RtlAppendUnicodeToString( &UnicodeDosDeviceName, (DeviceName->Buffer+(sizeof("\\Device"))) );
        IF_LOUD(DbgPrint("Packet: DosDeviceName is %ws\n",UnicodeDosDeviceName.Buffer);)
        if (Create) {
            Status=IoCreateSymbolicLink(&UnicodeDosDeviceName,DeviceName);
        } else {
            Status=IoDeleteSymbolicLink(&UnicodeDosDeviceName);
        }
        ExFreePool(UnicodeDosDeviceName.Buffer);
    }
    return Status;
}
#endif


NTSTATUS PacketReadRegistry(
	IN  PWSTR              *MacDriverName,
	IN  PWSTR              *PacketDriverName,
	IN  PUNICODE_STRING     RegistryPath
)
{
  NTSTATUS   Status;
  RTL_QUERY_REGISTRY_TABLE ParamTable[5];
  PWSTR      Bind       = L"Bind";        // LAURI: \Device\W30NT1
  PWSTR      Export     = L"Export";      // \Device\appletalk\W30NT1\0\0
  PWSTR      Parameters = L"Parameters";
  PWSTR      Linkage    = L"Linkage";
  PWCHAR     Path;

  Path=ExAllocatePool( PagedPool, RegistryPath->Length+sizeof(WCHAR) );

  if (!Path) return STATUS_INSUFFICIENT_RESOURCES;

  RtlZeroMemory( Path, RegistryPath->Length+sizeof(WCHAR) );
  RtlCopyMemory( Path, RegistryPath->Buffer, RegistryPath->Length );

  IF_LOUD(DbgPrint("Packet: Reg path is %ws\n",RegistryPath->Buffer);)

  RtlZeroMemory( ParamTable, sizeof(ParamTable) );

  //  change to the parmeters key
  ParamTable[0].QueryRoutine = NULL;
  ParamTable[0].Flags = RTL_QUERY_REGISTRY_SUBKEY;
  ParamTable[0].Name = Parameters;

  //  change to the linkage key
  ParamTable[1].QueryRoutine = NULL;
  ParamTable[1].Flags = RTL_QUERY_REGISTRY_SUBKEY;
  ParamTable[1].Name = Linkage;

  //  Get the name of the mac driver we should bind to
  ParamTable[2].QueryRoutine = PacketQueryRegistryRoutine;
  ParamTable[2].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;

  ParamTable[2].Name = Bind;
  ParamTable[2].EntryContext = (PVOID)MacDriverName;
  ParamTable[2].DefaultType = REG_MULTI_SZ;

  //  Get the name that we should use for the driver object
  ParamTable[3].QueryRoutine = PacketQueryRegistryRoutine;
  ParamTable[3].Flags = RTL_QUERY_REGISTRY_REQUIRED | RTL_QUERY_REGISTRY_NOEXPAND;

  ParamTable[3].Name = Export;
  ParamTable[3].EntryContext = (PVOID)PacketDriverName;
  ParamTable[3].DefaultType = REG_MULTI_SZ;

  Status=RtlQueryRegistryValues(
             RTL_REGISTRY_ABSOLUTE,
             Path,
             ParamTable,
             NULL,
             NULL
             );

  ExFreePool(Path);

  return Status;
}


NTSTATUS PacketQueryRegistryRoutine(
    IN PWSTR     ValueName,
    IN ULONG     ValueType,
    IN PVOID     ValueData,
    IN ULONG     ValueLength,
    IN PVOID     Context,
    IN PVOID     EntryContext
    )

{
  PUCHAR Buffer;

  IF_LOUD(DbgPrint("Perf: QueryRegistryRoutine\n");)

  if (ValueType != REG_MULTI_SZ) {
      return STATUS_OBJECT_NAME_NOT_FOUND;
  }

  Buffer=ExAllocatePool(NonPagedPool,ValueLength);
  if(!Buffer) return STATUS_INSUFFICIENT_RESOURCES;

  RtlCopyMemory( Buffer, ValueData, ValueLength );

  *((PUCHAR *)EntryContext)=Buffer;

  return STATUS_SUCCESS;
}
