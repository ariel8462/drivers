#pragma once

#include <ntddk.h>

template<typename T>
class AutoLock
{
public:
	AutoLock(T& lock) : _lock(lock)
	{
		_lock.Lock();
	}

	~AutoLock()
	{
		_lock.Unlock();
	}

private:
	T& _lock;
};
