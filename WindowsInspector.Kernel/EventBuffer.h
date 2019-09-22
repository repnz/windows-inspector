#pragma once
#include <ntifs.h>
#include "Common.h"

NTSTATUS 
ZeroEventBuffer(
    VOID
    );

NTSTATUS 
InitializeEventBuffer(
    __out PCIRCULAR_BUFFER* CircularBufferAddress
    );

NTSTATUS 
FreeEventBuffer(
    VOID
    );

NTSTATUS 
AllocateBufferEvent(
    __out PVOID Event, 
    __in USHORT EventSize
    );

NTSTATUS 
SendBufferEvent(
    __in PEVENT_HEADER Event
    );

NTSTATUS 
CancelBufferEvent(
    __in PEVENT_HEADER Event
    );


