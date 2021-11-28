#include <ntddk.h>
#include "RemoteThreadDriver.h"
#include "AutoLock.h"

void DriverUnload(PDRIVER_OBJECT DriverObject);
void OnThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create);
void OnProcessNotify(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo);
bool InList(ULONG pid);
void RemoveItem(ULONG pid);

Globals g_Globals;

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING)
{
	NTSTATUS status = STATUS_SUCCESS;
	g_Globals.Mutex.Init();
	InitializeListHead(&g_Globals.ListHead);

	do
	{
		status = PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, FALSE);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("[-] Failed to create process notify routine.\n"));
			break;
		}
		status = PsSetCreateThreadNotifyRoutine(OnThreadNotify);
		if (!NT_SUCCESS(status))
		{
			KdPrint(("[-] Failed to create thread notify routine.\n"));
			PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);
		}
	} while (false);
	DriverObject->DriverUnload = DriverUnload;
	KdPrint(("[+] Driver loaded successfully!\n"));
	
	return status;
}

void DriverUnload(PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);

	PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);
	PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);
	while (!IsListEmpty(&g_Globals.ListHead))
	{
		auto entry = RemoveTailList(&g_Globals.ListHead);
		auto currStruct = CONTAINING_RECORD(entry, ProcessData, Entry);
		ExFreePool(currStruct);
	}
	KdPrint(("[-] Driver loaded successfully!\n"));
}

void OnThreadNotify(HANDLE ProcessId, HANDLE ThreadId, BOOLEAN Create)
{
	if (Create)
	{
		auto creatorProcessId = PsGetCurrentProcessId();
		if (HandleToULong(creatorProcessId) != HandleToULong(ProcessId) && HandleToUlong(creatorProcessId) != 4 \
			&& !InList(HandleToULong(ProcessId)))
		{
			DbgPrint("[X] Detected remote thread injection - thread %d in process %d\n",
				HandleToULong(ThreadId), HandleToUlong(ProcessId));
			
		}
		else if (InList(HandleToULong(ProcessId)))
		{
			RemoveItem(HandleToULong(ProcessId));
		}
	}
}

bool InList(ULONG pid)
{
	AutoLock lock(g_Globals.Mutex);
	auto current = &g_Globals.ListHead;
	current = current->Flink;
	while (current != &g_Globals.ListHead)
	{
		auto currStruct = CONTAINING_RECORD(current, ProcessData, Entry);
		if (currStruct->ProcessID == pid)
		{
			return true;
		}
		current = current->Flink;
	}
	return false;
}

void RemoveItem(ULONG pid)
{
	AutoLock lock(g_Globals.Mutex);
	auto current = &g_Globals.ListHead;
	current = current->Flink;
	while (current != &g_Globals.ListHead)
	{
		auto currStruct = CONTAINING_RECORD(current, ProcessData, Entry);
		if (currStruct->ProcessID == pid)
		{
			RemoveEntryList(current);
			ExFreePool(currStruct);
			break;
		}
		current = current->Flink;
	}
}

void PushItem(LIST_ENTRY* entry)
{
	AutoLock lock(g_Globals.Mutex);
	InsertHeadList(&g_Globals.ListHead, entry);
}

void OnProcessNotify(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo)
{
	UNREFERENCED_PARAMETER(Process);

	if (CreateInfo)
	{
		auto size = sizeof(ProcessData);
		auto data = static_cast<ProcessData*>(ExAllocatePoolWithTag(PagedPool, size, DRIVER_TAG));
		if (data == nullptr)
		{
			KdPrint(("[-] Allocation failed.\n"));
			return;
		}
		data->ProcessID = HandleToULong(ProcessId);
		PushItem(&data->Entry);
	}
}