#pragma once
#include <ntifs.h>
#include <WindowsInspector.Shared/Common.h>

NTSTATUS 
ZeroEventBuffer(
    VOID
    );

NTSTATUS 
InitializeEventBuffer(
    __out PCIRCULAR_BUFFER* CircularBufferAddress
    );

VOID 
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
    __in PVOID Event
    );

NTSTATUS 
CancelBufferEvent(
    __in PVOID Event
    );

NTSTATUS
SendOrCancelBufferEvent(
	__in PVOID Event
	);