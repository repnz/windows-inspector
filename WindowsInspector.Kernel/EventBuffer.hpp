#pragma once
#include <ntifs.h>
#include "Common.hpp"

NTSTATUS ZeroEventBuffer();

NTSTATUS InitializeEventBuffer(CircularBuffer** CircularBufferAddress);

NTSTATUS FreeEventBuffer();

NTSTATUS AllocateBufferEvent(PVOID Event, USHORT EventSize);

NTSTATUS SendBufferEvent(EventHeader* Event);

NTSTATUS CancelBufferEvent(EventHeader* Event);


