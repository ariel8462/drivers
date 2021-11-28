#pragma once

struct ProcessData
{
	PCWSTR ProcessPath;
};

#define IOCTL_PROC_GUARD_BLOCK CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define DRIVER_TAG 'ar'