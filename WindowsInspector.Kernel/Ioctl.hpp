#pragma once
#include <ntifs.h>

NTSTATUS InitializeIoctlHandlers();

NTSTATUS InspectorListen(PIRP Irp);