#include <ntifs.h>
#include "DataItemList.hpp"
#include "Common.hpp"
#include "Mem.hpp"
#include <WindowsInspector.Kernel/KernelApi.hpp>
#include "Error.hpp"
#include <WindowsInspector.Kernel/UserModeMapping.hpp>
#include <WindowsInspector.Kernel/DeviceIoControlCode.hpp>
#include "cpptools.hpp"
#include "EventList.hpp"
#include <WindowsInspector.Kernel/Providers/Providers.hpp>

NTSTATUS DefaultDispatch(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp);

NTSTATUS DeviceIoControlDispatchWrapper(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp);

NTSTATUS DeviceIoControlDispatch(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp);

void DriverUnload(_In_ PDRIVER_OBJECT DriverObject);


extern "C" NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject,_In_ const PUNICODE_STRING RegistryPath) {

	KdPrintMessage("DriverEntry");

	UNREFERENCED_PARAMETER(DriverObject);
	UNREFERENCED_PARAMETER(RegistryPath);

	KdPrint((DRIVER_PREFIX "Initializing Kernel API...\n"));

	if (!KernelApiInitialize()) 
	{
		KdPrintMessage("Could not initialize kernel API");
		return STATUS_INVALID_ADDRESS;
	}

    
    g_EventList.Init(1000);

	PDEVICE_OBJECT deviceObject = nullptr;
	NTSTATUS status = STATUS_SUCCESS;

	// Variables needed for cleaning
	bool linkCreated = false;
    bool providersInitialized = false;

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\WindowsInspector");
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\WindowsInspector");

	do 
	{
		KdPrintMessage("Creating Device...");
		status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &deviceObject);

		if (!NT_SUCCESS(status))
		{
			KdPrintError("Failed to create device", status);
			break;
		}


		KdPrintMessage("Creating Symbolic Link..");

		status = IoCreateSymbolicLink(&symLink, &devName);

		if (!NT_SUCCESS(status))
		{
			KdPrintError("Failed to create symbolic link", status);
			break;
		}

		linkCreated = true;
        
        status = InitializeProviders();

        if (!NT_SUCCESS(status))
        {
            KdPrintError("Failed to initialize providers", status);
            break;
        }

        providersInitialized = true;

		DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControlDispatchWrapper;
		DriverObject->MajorFunction[IRP_MJ_CREATE] = DefaultDispatch;
		DriverObject->MajorFunction[IRP_MJ_CLOSE] = DefaultDispatch;
		DriverObject->DriverUnload = DriverUnload;

	} 
	while (false);

	if (!NT_SUCCESS(status)) 
	{
        if (providersInitialized)
        {
            FreeProviders();
        }

		if (linkCreated) 
		{
			IoDeleteSymbolicLink(&symLink);
		}

		if (deviceObject != nullptr) 
		{
			IoDeleteDevice(deviceObject);
		}
	}

	return status;
}


void DriverUnload(_In_ PDRIVER_OBJECT DriverObject)
{
	// Remove callbacks
	KdPrintMessage("DriverUnload");

    FreeProviders();

	// Remove Device
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\WindowsInspector");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS CompleteIrp(PIRP Irp, NTSTATUS status = STATUS_SUCCESS, ULONG_PTR info = 0) 
{
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp, 0);
	return status;
}

NTSTATUS DefaultDispatch(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	return CompleteIrp(Irp, STATUS_SUCCESS);
}


#define INSPECTOR_GET_EVENTS_FUNCTION_CODE 0x1

NTSTATUS DeviceIoControlDispatchWrapper(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp) 
{
	__try 
	{
		return DeviceIoControlDispatch(DeviceObject, Irp);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) 
	{
		KdPrint((DRIVER_PREFIX "Exception handling IOCTL: (0x%08X)\n", GetExceptionCode()));
		return CompleteIrp(Irp, STATUS_ACCESS_VIOLATION);
	}
}

NTSTATUS GetEventsHandler(_In_ PVOID UserModeBuffer, _In_ ULONG BufferSize, ULONG* BytesRead) 
{

	if (UserModeBuffer == NULL || BufferSize == 0 || BytesRead == NULL)
	{
		return STATUS_INVALID_PARAMETER;
	}

	*BytesRead = 0;

	UserModeMapping mapping;

	NTSTATUS status = UserModeMapping::Create(
		UserModeBuffer,
		BufferSize,
		IoWriteAccess,
		&mapping);

	if (!NT_SUCCESS(status)) 
	{
		return status;
	}

	status = g_EventList.ReadIntoBuffer(mapping.Buffer, mapping.Length, BytesRead);

	if (!NT_SUCCESS(status))
	{
		KdPrintError("Could not read entries into buffer", status);
		return status;
	}

	return status;
}

NTSTATUS DeviceIoControlDispatch(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	NTSTATUS status;

	Irp->IoStatus.Information = 0;

	PIO_STACK_LOCATION iosp = IoGetCurrentIrpStackLocation(Irp);
	const DeviceIoControlCode controlCode(iosp->Parameters.DeviceIoControl.IoControlCode);

	if (controlCode.Method != METHOD_NEITHER || controlCode.DeviceType != FILE_DEVICE_UNKNOWN) 
	{
		return CompleteIrp(Irp, STATUS_INVALID_PARAMETER_1);
	}

	switch (controlCode.Function) 
	{
	case INSPECTOR_GET_EVENTS_FUNCTION_CODE:
	{
		ULONG bytesRead;

		status = GetEventsHandler(
			iosp->Parameters.DeviceIoControl.Type3InputBuffer,
			iosp->Parameters.DeviceIoControl.InputBufferLength,
			&bytesRead
		);

		Irp->IoStatus.Information = bytesRead;
	}
	break;
	default:
		status = STATUS_INVALID_PARAMETER;

	};


	Irp->IoStatus.Status = status;
	IoCompleteRequest(Irp, 0);
	return status;
}
