#pragma once
#include <ntifs.h>

NTSTATUS 
InitializeIoctlHandlers(
    VOID
    );

NTSTATUS 
InspectorListen(
    __inout PIRP Irp, 
    __in PIO_STACK_LOCATION IoStackLocation
    );

NTSTATUS
InspectorStop(
    VOID
    );