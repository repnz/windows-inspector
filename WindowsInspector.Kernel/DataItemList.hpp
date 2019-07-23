#pragma once
#include <ntddk.h>
#include "FastMutex.hpp"

template <typename T>
struct ListItem 
{
    LIST_ENTRY Entry;
    T Item;
};

struct DataItemList
{
private:
    LIST_ENTRY _head;
    FastMutex _mutex;
    ULONG _count;
    ULONG _max;
public:
    void Init(ULONG max);

	template <typename T>
	NTSTATUS PushItem(ListItem<T>& item)
	{
		PushItem(&item->Entry);
	}

	template <typename T>
	NTSTATUS PushItem(ListItem<T>* item) 
	{
		return PushItem(&item->Entry);
	}

    NTSTATUS PushItem(PLIST_ENTRY item);

    NTSTATUS ReadIntoBuffer(PVOID buffer, ULONG bufferSize, ULONG* itemsRead);
    
    ULONG Count() const;

    void Free();
};