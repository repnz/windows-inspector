#include "InspectorDriver.h"
#include "ntos.h"
#include <stdio.h>

#define DEVICE_NAME "\\\\.\\WindowsInspector"

NTSTATUS
DriverInitialize(
    __out PINSPECTOR_DRIVER Driver
    )
{
    NTSTATUS Status;
    OBJECT_ATTRIBUTES DeviceObjectAttributes;
    PIO_STATUS_BLOCK IoStatus;

	InitializeObjectAttributes(&DeviceObjectAttributes, DEVICE_NAME, NULL, NULL, NULL);
	

	Status = NtCreateFile(
        Driver,
        GENERIC_ALL,
        &DeviceObjectAttributes,
        &IoStatus,
        NULL,
        0,
        FILE_ATTRIBUTE_NORMAL,
        FILE_OPEN,
        FILE_NON_DIRECTORY_FILE,
        NULL,
        0
    );

    if (!NT_SUCCESS(Status))
    {
		printf("Could not create device handle. Status: 0x%08X\n", Status);
		return Status;
    }

	return Status;
}

NTSTATUS
DriverListen(
	__in PINSPECTOR_DRIVER Driver
	)
{
	NTSTATUS Status;
    PCIRCULAR_BUFFER Buffer;
	PIO_STATUS_BLOCK IoStatus;

	Status = NtDeviceIoControlFile(
		Driver->DeviceHandle,
		NULL,
		NULL,
		NULL,
		&IoStatus,
		INSPECTOR_LISTEN_CTL_CODE,
		NULL,
		0,
		&Buffer,
		sizeof(Buffer)
	);

	if (!NT_SUCCESS(Status))
	{
		printf("Could not call INSPECTOR_LISTEN ioctl. (NTSTATUS: 0x%08X)", Status);
		return Status;
	}

	if (IoStatus->Information != sizeof(Buffer))
	{
		printf("Return length is not valid. Length: %d", IoStatus->Information);
		return Status;
	}

	return Status;
}

NTSTATUS
DriverStop(
	__in PINSPECTOR_DRIVER Driver,
	)
{
    if (!DeviceIoControl(
        hDriver.get(),
        INSPECTOR_STOP_CTL_CODE,
        NULL,
        0,
        NULL,
        0,
        &bytesReturned,
        NULL
    ))
    {
        throw std::runtime_error("Could not call INSPECTOR_STOP iocl");
    }
}

NTSTATUS InitializeDriver(PINSPECTOR_DRIVER Driver)
{
    return NTSTATUS();
}

NTSTATUS DriverListen(PINSPECTOR_DRIVER Driver, PCIRCULAR_BUFFER Buffer)
{
    return NTSTATUS();
}

NTSTATUS DriverStop(PINSPECTOR_DRIVER Driver)
{
    return NTSTATUS();
}
