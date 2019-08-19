#pragma once
#include <ntifs.h>

void InitializeIoctlHandlers();


struct BufferEvent {
    PVOID Memory;
    ULONG NewTail;
};

NTSTATUS AllocateBufferEvent(BufferEvent* Event, ULONG EventSize);

NTSTATUS SendBufferEvent(BufferEvent* Event);

NTSTATUS InspectorListen(PIRP Irp);