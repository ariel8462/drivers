#include <ntifs.h>
#include <ntddk.h>
#include "ProcGuardCommon.h"
#include "ProcGuard.h"
#include "AutoLock.h"

void ProcGuardUnload(PDRIVER_OBJECT DriverObject);
DRIVER_DISPATCH ProcGuardCreateClose;
DRIVER_DISPATCH ProcGuardDeviceControl;
void OnProcessNotify(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo);
void PushItem(LIST_ENTRY* entry);

Globals g_Globals;

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);

	g_Globals.ItemCount = 0;
	g_Globals.Mutex.Init();
	InitializeListHead(&g_Globals.ListHead);

	auto status = STATUS_SUCCESS;
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\ProcGuard");
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\ProcGuard");
	PDEVICE_OBJECT DeviceObject = nullptr;
	do
	{
		status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &DeviceObject);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("[-] Failed to create device object.\n"));
			break;
		}
		status = IoCreateSymbolicLink(&symLink, &devName);
		if (!NT_SUCCESS(status))
		{
			IoDeleteDevice(DeviceObject);
			KdPrint(("[-] Failed to create symbolic link.\n"));
			break;
		}
		status = PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, FALSE);
		if (!NT_SUCCESS(status))
		{
			IoDeleteSymbolicLink(&symLink);
			IoDeleteDevice(DeviceObject);
			KdPrint(("[-] Failed to create process notify routine.\n"));
			break;
		}
	} while (false);

	DriverObject->DriverUnload = ProcGuardUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = ProcGuardCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = ProcGuardCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ProcGuardDeviceControl;

	KdPrint(("[+] Driver loaded successfully!\n"));

	return status;
}

void ProcGuardUnload(PDRIVER_OBJECT DriverObject)
{
	//Removing the notify routine
	PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);

	//Removing Device traces
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\ProcGuard");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);

	while (!IsListEmpty(&g_Globals.ListHead))
	{
		auto entry = RemoveTailList(&g_Globals.ListHead);
		auto currStruct = CONTAINING_RECORD(entry, PrivateProcessData<UNICODE_STRING>, Entry);
		RtlFreeUnicodeString(&currStruct->ProcessPath);
		ExFreePool(currStruct);
	}

	KdPrint(("[+] Unloaded driver successfully!"));
}

NTSTATUS ProcGuardCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS ProcGuardDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	
	auto status = STATUS_SUCCESS;
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	auto size = sizeof(PrivateProcessData<UNICODE_STRING>);
	
	auto info = static_cast<PrivateProcessData<UNICODE_STRING>*>(ExAllocatePoolWithTag(PagedPool, size, DRIVER_TAG));
	if (info == nullptr)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		KdPrint(("[-] Allocation failed.\n"));
		Irp->IoStatus.Status = STATUS_SUCCESS;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return status;
	}

	switch (stack->Parameters.DeviceIoControl.IoControlCode)
	{
	case IOCTL_PROC_GUARD_BLOCK:
	{
		if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(ProcessData))
		{
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}
		auto data = static_cast<ProcessData*>(Irp->AssociatedIrp.SystemBuffer);
		if (data == nullptr)
		{
			status = STATUS_INVALID_PARAMETER;
			KdPrint(("[-] Allocation failed.\n"));
			return status;
		}
		RtlCreateUnicodeString(&info->ProcessPath, data->ProcessPath);
		DbgPrint("[+] Blocking the path - %wZ\n", info->ProcessPath);
		PushItem(&info->Entry);
	}
	default:
	{
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}
	}
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return status;
}

void PushItem(LIST_ENTRY* entry)
{
	AutoLock lock(g_Globals.Mutex);
	if (g_Globals.ItemCount > 1024)
	{
		auto head = RemoveHeadList(&g_Globals.ListHead);
		g_Globals.ItemCount--;
		auto item = CONTAINING_RECORD(head, PrivateProcessData<UNICODE_STRING>, Entry);
		RtlFreeUnicodeString(&item->ProcessPath);
		ExFreePool(item);
	}
	InsertTailList(&g_Globals.ListHead, entry);
	g_Globals.ItemCount++;
}

bool InList(const PPS_CREATE_NOTIFY_INFO& CreateInfo)
{
	AutoLock lock(g_Globals.Mutex);
	auto current = &g_Globals.ListHead;
	current = current->Flink;
	while (current != &g_Globals.ListHead)
	{
		auto currStruct = CONTAINING_RECORD(current, PrivateProcessData<UNICODE_STRING>, Entry);
		DbgPrint("[+] Current process - %wZ\n", CreateInfo->ImageFileName);
		DbgPrint("[+] Blocked process - %wZ\n", currStruct->ProcessPath);
		if (RtlCompareUnicodeString(CreateInfo->ImageFileName, &currStruct->ProcessPath, TRUE) == 0)
		{
			DbgPrint("[+] Found blocked process path - %wZ\n", currStruct->ProcessPath);
			return true;
		}
		current = current->Flink;
	}
	return false;
}

void OnProcessNotify(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo)
{
	UNREFERENCED_PARAMETER(Process);
	UNREFERENCED_PARAMETER(ProcessId);
	
	if (CreateInfo)
	{
		if (CreateInfo->FileOpenNameAvailable && CreateInfo->ImageFileName)
		{
			if (g_Globals.ItemCount > 0 && InList(CreateInfo))
			{
				CreateInfo->CreationStatus = STATUS_ACCESS_DENIED;
				return;
				/*
				AutoLock lock(g_Globals.Mutex);
				auto current = &g_Globals.ListHead;
				current = current->Flink;
				while (current != &g_Globals.ListHead)
				{
					auto currStruct = CONTAINING_RECORD(current, PrivateProcessData<UNICODE_STRING>, Entry);
					DbgPrint("[+] Current process - %wZ\n", CreateInfo->ImageFileName);
					DbgPrint("[+] Blocked process - %wZ\n", currStruct->ProcessPath);
					if (RtlCompareUnicodeString(CreateInfo->ImageFileName, &currStruct->ProcessPath, TRUE) == 0)
					{
						CreateInfo->CreationStatus = STATUS_ACCESS_DENIED;
						break;
					}
					current = current->Flink;
				}
				*/
			}
		}
	}
}