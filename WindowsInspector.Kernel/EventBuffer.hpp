#pragma once
#include <ntifs.h>
#include "Common.hpp"

NTSTATUS ZeroEventBuffer();

NTSTATUS InitializeEventBuffer(CircularBuffer** CircularBufferAddress);

struct BufferEvent {
    PVOID Memory;
    ULONG NewTail;
};

NTSTATUS AllocateBufferEvent(BufferEvent* Event, ULONG EventSize);

NTSTATUS SendBufferEvent(BufferEvent* Event);

NTSTATUS CancelBufferEvent(BufferEvent* Event);


