#pragma once
#include <ntifs.h>	

#define DRIVER_PREFIX "WindowsInspector: "
#define KdPrintMessage(Msg) KdPrint((DRIVER_PREFIX Msg "\n"))
#define KdPrintError(Msg, status) KdPrint((DRIVER_PREFIX Msg "(NTSTATUS: 0x%08X)\n", status))
