#pragma once

#include "FastMutex.h"

#define DRIVER_TAG 'arin'

struct ProcessData
{
	LIST_ENTRY Entry;
	ULONG ProcessID;
};

struct Globals
{
	LIST_ENTRY ListHead;
	FastMutex Mutex;
};
