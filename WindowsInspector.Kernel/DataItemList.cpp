#include "DataItemList.hpp"
#include "ScopedLock.hpp"
#include "Common.hpp"

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
	return STATUS_SUCCESS;
}

EventHeader& ToItemHeader(PLIST_ENTRY entry) {
    ListItem<EventHeader>* Header = CONTAINING_RECORD(entry, ListItem<EventHeader>, Entry);
    return Header->Item;

}

NTSTATUS DataItemList::ReadIntoBuffer(PVOID buffer, ULONG bufferSize, ULONG* itemsRead)
{
	*itemsRead = 0;

    ScopedLock<FastMutex> scopedLock(_mutex);

    for (ULONG bufferIndex=0; bufferIndex<bufferSize;){
        PLIST_ENTRY entry = RemoveHeadList(&_head);

        if (entry == &_head) {
            return STATUS_SUCCESS;
        }
        
        EventHeader& header = ToItemHeader(entry);
        RtlCopyMemory(buffer, &header, header.Size);
        bufferIndex += header.Size;
		*itemsRead++;
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
