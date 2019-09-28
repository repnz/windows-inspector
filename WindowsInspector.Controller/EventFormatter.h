#pragma once
#include <WindowsInspector.Shared/Common.h>


NTSTATUS
FmtDumpEvent(
    __inout PCHAR Buffer, 
    __in ULONG BufferLength,
    __in PEVENT_HEADER EventHeader
);