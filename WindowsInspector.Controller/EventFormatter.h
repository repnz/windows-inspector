#pragma once
#include <WindowsInspector.Shared/Common.h>

VOID
FmtDumpEvent(
    __inout PCHAR Buffer, 
    __in ULONG BufferLength,
    __in PEVENT_HEADER EventHeader
);