#pragma once
#include <ntddk.h>

struct Mem 
{
	static const ULONG Tag = 'znpr';

	template <typename T>
	static T* Allocate(SIZE_T Appendix = 0, POOL_TYPE pool = NonPagedPool)
	{
		SIZE_T AllocationSize = sizeof(T) + Appendix;

		PVOID Memory = ExAllocatePoolWithTag(pool, AllocationSize, Tag);

		if (Memory == NULL) 
		{
			return NULL;
		}

		RtlSecureZeroMemory(Memory, AllocationSize);
		return (T*)Memory;
	}
};