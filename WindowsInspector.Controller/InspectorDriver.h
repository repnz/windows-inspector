#pragma once
#include <WindowsInspector.Shared/Common.h>

typedef struct _INSPECTOR_DRIVER {
    HANDLE DeviceHandle;
} INSPECTOR_DRIVER, *PINSPECTOR_DRIVER;


NTSTATUS
DriverInitialize(
    __out PINSPECTOR_DRIVER Driver
    );

NTSTATUS
DriverListen(
    __in PINSPECTOR_DRIVER Driver,
	__out PCIRCULAR_BUFFER* Buffer
    );

NTSTATUS
DriverStop(
    __in PINSPECTOR_DRIVER Driver
    );


NTSTATUS
DriverRelease(
	__in PINSPECTOR_DRIVER  Driver
	);