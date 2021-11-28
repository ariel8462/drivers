#include <ntifs.h>
#include <ntddk.h>
#include "PriorityBoosterCommon.h"

void PriorityBoosterUnload(_In_ PDRIVER_OBJECT);
NTSTATUS PriorityBoosterCreateClose(_In_ PDEVICE_OBJECT, _In_ PIRP);
NTSTATUS PriorityBoosterDeviceControl(_In_ PDEVICE_OBJECT, _In_ PIRP);

extern "C" NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);

	DriverObject->DriverUnload = PriorityBoosterUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = PriorityBoosterCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = PriorityBoosterCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = PriorityBoosterDeviceControl;

	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\PriorityBooster");

	PDEVICE_OBJECT DeviceObject;
	NTSTATUS status = IoCreateDevice(
		DriverObject,		 // the driver object
		0,					 // no need for extra bytes
		&devName,			 // device name
		FILE_DEVICE_UNKNOWN, // device type
		0,					 // characteristics flags
		FALSE,				 // not exclusive
		&DeviceObject		 // the resuling device object pointer
		);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("Failed to create device object - 0x%08X\n", status));
		return status;
	}

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\PriorityBooster");
	status = IoCreateSymbolicLink(&symLink, &devName);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("Failed to create symbolic link - 0x%08X\n", status));
		IoDeleteDevice(DeviceObject);
		return status;
	}

	return STATUS_SUCCESS;
}


_Use_decl_annotations_
NTSTATUS PriorityBoosterDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	auto stack = IoGetCurrentIrpStackLocation(Irp); // PIO_STACK_LOCATION
	auto status = STATUS_SUCCESS; // NTSTATUS

	switch (stack->Parameters.DeviceIoControl.IoControlCode)
	{
		case IOCTL_PRIORITY_BOOSTER_SET_PRIORITY:
		{
			if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ThreadData)) // error was here, placed '<=' :(
			{
				status = STATUS_BUFFER_TOO_SMALL;
				break;
			}

			auto data = reinterpret_cast<ThreadData*>(stack->Parameters.DeviceIoControl.Type3InputBuffer);

			if (data == nullptr)
			{
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			if (data->Priority < 1 || data->Priority > 31)
			{
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			PETHREAD Thread;
			status = PsLookupThreadByThreadId(ULongToHandle(data->ThreadID), &Thread);

			if (!NT_SUCCESS(status))
			{
				break;
			}

			KeSetPriorityThread(Thread, data->Priority);
			ObDereferenceObject(Thread);
			KdPrint(("Threat Priorty done\n"));
			break;
		}
		default:
		{
			status = STATUS_INVALID_DEVICE_REQUEST;
			break;
		}
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return status;
}


_Use_decl_annotations_
NTSTATUS PriorityBoosterCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}


void PriorityBoosterUnload(_In_ PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\PriorityBooster");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);
}