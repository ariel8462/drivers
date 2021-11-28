#include <iostream>
#include <Windows.h>
#include "..\PriorityBooster\PriorityBoosterCommon.h"

int main(int argc, const char* argv[])
{
	if (argc < 3)
	{
		std::cout << "Usage: Booster <threadID> <Priority>" << std::endl;
		return 0;
	}

	HANDLE hDevice = CreateFile(
		L"\\\\.\\PriorityBooster",
		GENERIC_WRITE,
		FILE_SHARE_WRITE,
		nullptr,
		OPEN_EXISTING,
		0,
		nullptr
		);

	if (hDevice == INVALID_HANDLE_VALUE)
	{
		std::cout << "Failed to open device! - " <<  GetLastError() << std::endl;
		return 1;
	}

	ThreadData data;
	data.ThreadID = atoi(argv[1]); // thread ID to change priority of
	data.Priority = atoi(argv[2]); // the priority to change to

	DWORD returned;
	BOOL success = DeviceIoControl(
		hDevice,
		IOCTL_PRIORITY_BOOSTER_SET_PRIORITY,
		&data, sizeof(data), // input buffer and length
		nullptr, 0,			 // output buffer and length
		&returned, nullptr
		);
	
	if (success)
	{
		std::cout << "Priority change succeeded!" << std::endl;
	}
	else
	{
		std::cout << "Priority change failed! - " <<  GetLastError() << std::endl;
		return 1;
	}

	CloseHandle(hDevice);
}