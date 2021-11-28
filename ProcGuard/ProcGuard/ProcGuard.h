#pragma once

#include "FastMutex.h"

template<typename T>
struct PrivateProcessData
{
	LIST_ENTRY Entry;
	T ProcessPath;
};

struct Globals
{
	LIST_ENTRY ListHead;
	int ItemCount;
	FastMutex Mutex;
};