#include "InspectorDriver.h"
#include "ntos.h"
#include <stdio.h>
#include <assert.h>


NTSTATUS
DriverInitialize(
    __out PINSPECTOR_DRIVER Driver
    )
{
    NTSTATUS Status;
    OBJECT_ATTRIBUTES DeviceObjectAttributes;
    IO_STATUS_BLOCK IoStatus;
	UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\??\\WindowsInspector");

	InitializeObjectAttributes(&DeviceObjectAttributes, &DeviceName, 0, NULL, NULL);
	

	Status = NtCreateFile(
        &Driver->DeviceHandle,
        GENERIC_ALL,
        &DeviceObjectAttributes,
        &IoStatus,
        NULL,
		FILE_ATTRIBUTE_NORMAL,
        0,
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
	__in PINSPECTOR_DRIVER Driver,
	__out PCIRCULAR_BUFFER* Buffer
	)
{
	NTSTATUS Status;
	IO_STATUS_BLOCK IoStatus;

	Status = NtDeviceIoControlFile(
		Driver->DeviceHandle,
		NULL,
		NULL,
		NULL,
		&IoStatus,
		INSPECTOR_LISTEN_CTL_CODE,
		NULL,
		0,
		Buffer,
		sizeof(PVOID)
	);

	if (!NT_SUCCESS(Status))
	{
		printf("Could not call INSPECTOR_LISTEN ioctl. (NTSTATUS: 0x%08X)", Status);
		return Status;
	}

	if (IoStatus.Information != sizeof(PVOID))
	{
		printf("Return length is not valid. Length: %lld", IoStatus.Information);
		return Status;
	}

	return Status;
}

NTSTATUS
DriverStop(
	__in PINSPECTOR_DRIVER Driver
	)
{
	NTSTATUS Status;
	IO_STATUS_BLOCK IoStatus;

	Status = NtDeviceIoControlFile(
		Driver->DeviceHandle,
		NULL,
		NULL,
		NULL,
		&IoStatus,
		INSPECTOR_STOP_CTL_CODE,
		NULL,
		0,
		NULL,
		0
	);

	if (!NT_SUCCESS(Status))
	{
		printf("Could not call INSPECTOR_LISTEN ioctl. (NTSTATUS: 0x%08X)", Status);
		return Status;
	}

	return Status;
}

NTSTATUS
DriverRelease(
	__in PINSPECTOR_DRIVER  Driver
	)
{
	assert(Driver->DeviceHandle != NULL);
	return NtClose(Driver->DeviceHandle);
}