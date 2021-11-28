#include <iostream>
#include <Windows.h>
#include "../ProcGuard/ProcGuardCommon.h"

int main()
{
    auto hDevice = CreateFile(L"\\\\.\\ProcGuard", GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hDevice == INVALID_HANDLE_VALUE)
	{
		std::cout << GetLastError() << std::endl;
		return 1;
	}
	ProcessData data;
	PCWSTR procPath = L"\\??\\C:\\Windows\\System32\\notepad.exe";
	data.ProcessPath = procPath;
	
	DWORD returned;
	bool success = DeviceIoControl(
		hDevice, IOCTL_PROC_GUARD_BLOCK,
		&data, sizeof(data),
		nullptr, 0,
		&returned, 0
		);
	if (success)
	{
		std::cout << "Process path blocked successfully" << std::endl;
	}
	else
	{
		std::cout << "Process path blocking failed - " << GetLastError() << std::endl;
	}
	CloseHandle(hDevice);
	
	return 0;
}

