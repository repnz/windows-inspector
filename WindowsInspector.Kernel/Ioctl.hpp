#pragma once
#include <ntifs.h>

NTSTATUS InitializeIoctlHandlers();

NTSTATUS InspectorListen(PIRP Irp, PIO_STACK_LOCATION IoStackLocation);