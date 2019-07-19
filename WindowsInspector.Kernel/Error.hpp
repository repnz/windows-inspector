#pragma once
#include <ntddk.h>

#define DRIVER_PREFIX "WindowsInspector: "
#define KdPrintMessage(Msg) KdPrint((DRIVER_PREFIX Msg))
#define KdPrintError(Msg, status) KdPrint((DRIVER_PREFIX Msg, status))
