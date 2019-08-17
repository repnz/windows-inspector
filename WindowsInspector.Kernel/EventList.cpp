#include "EventList.hpp"
#include <WindowsInspector.Kernel/Error.hpp>

DataItemList g_EventList;


NTSTATUS InsertIntoList(PLIST_ENTRY listItem, EventType itemType)
{
    UNREFERENCED_PARAMETER(itemType);
    NTSTATUS status = g_EventList.PushItem(listItem);

    if (!NT_SUCCESS(status))
    {
        KdPrint((DRIVER_PREFIX "Cannot insert item of type %d into the list (0x%08X)\n", (ULONG)itemType, status));
        ExFreePool(listItem);
        return status;
    }

    return status;
}