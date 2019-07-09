#include "DataItemList.h"
#include "ScopedLock.h"
#include "Common.h"

void DataItemList::Init(ULONG max)
{
    _max = max;
    InitializeListHead(&_head);
}

NTSTATUS DataItemList::PushItem(PLIST_ENTRY entry)
{
    ScopedLock<FastMutex> scopedLock(_mutex);

    if (_count == _max) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    InsertTailList(&_head, entry);
    _count++;
}

ItemHeader& ToItemHeader(PLIST_ENTRY entry) {
    ListItem<ItemHeader>* Header = CONTAINING_RECORD(entry, ListItem<ItemHeader>, Entry);
    return Header->Item;

}

NTSTATUS DataItemList::ReadIntoBuffer(PVOID buffer, ULONG bufferSize)
{
    ScopedLock<FastMutex> scopedLock(_mutex);

    for (ULONG bufferIndex=0; bufferIndex<bufferSize;){
        PLIST_ENTRY entry = RemoveHeadList(&_head);

        if (entry == &_head) {
            return STATUS_SUCCESS;
        }
        
        ItemHeader& header = ToItemHeader(entry);
        RtlCopyMemory(buffer, &header, header.Size);
        bufferIndex += header.Size;
        ExFreePool(entry);
    } 

    return STATUS_MORE_ENTRIES;
}

ULONG DataItemList::Count() const
{
    return _count;
}

void DataItemList::Free()
{
    ScopedLock<FastMutex> scopedLock(_mutex);
    	
	while (true){
		PLIST_ENTRY entry = RemoveHeadList(&_head);

		if (entry == &_head) {
			break;
		}

		ExFreePool(entry);
	}
}
