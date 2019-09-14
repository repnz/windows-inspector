#include <ntifs.h>
#include <WindowsInspector.Kernel/KernelApi.hpp>
#include <WindowsInspector.Kernel/Debug.hpp>
#include <WindowsInspector.Kernel/Ioctl.hpp>
#include <WindowsInspector.Kernel/Common.hpp>
#include <WindowsInspector.Kernel/DriverObject.hpp>

NTSTATUS DefaultDispatch(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp);

NTSTATUS DeviceIoControlDispatch(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp);

void DriverUnload(_In_ PDRIVER_OBJECT DriverObject);

extern "C" NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject,_In_ PUNICODE_STRING RegistryPath) {

	UNREFERENCED_PARAMETER(DriverObject);
	UNREFERENCED_PARAMETER(RegistryPath);
    
    D_INFO("DriverEntry");
    D_INFO("Initializing Kernel API...");

	if (!KernelApiInitialize()) 
	{
		D_ERROR("Could not initialize kernel API");
		return STATUS_INVALID_ADDRESS;
	}
    
	NTSTATUS status = STATUS_SUCCESS;

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\WindowsInspector");
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\WindowsInspector");

	D_INFO("Creating Device...");

    InitializeIoctlHandlers();
    
    // ExclusiveAccess: TRUE
	status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &g_DeviceObject);

	if (!NT_SUCCESS(status))
	{
		D_ERROR_STATUS("Failed to create device", status);
        return status;
	}
     
	D_INFO("Creating Symbolic Link..");

	status = IoCreateSymbolicLink(&symLink, &devName);

	if (!NT_SUCCESS(status))
	{
		D_ERROR_STATUS("Failed to create symbolic link", status);
        IoDeleteDevice(g_DeviceObject);
        return status;
	}

	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControlDispatch;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DefaultDispatch;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DefaultDispatch;
	DriverObject->DriverUnload = DriverUnload;

    D_INFO("Completed DriverEntry Successfully");

	return status;
}

void DriverUnload(_In_ PDRIVER_OBJECT DriverObject)
{
	// Remove callbacks
	D_INFO("DriverUnload");

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

NTSTATUS DeviceIoControlDispatch(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	NTSTATUS Status;

	Irp->IoStatus.Information = 0;

	PIO_STACK_LOCATION iosp = IoGetCurrentIrpStackLocation(Irp);

    ULONG IoControlCode = iosp->Parameters.DeviceIoControl.IoControlCode;

    if (IoControlCode == INSPECTOR_LISTEN_CTL_CODE)
    {
        Status = InspectorListen(Irp, iosp);
    }
    else
    {
        Status = STATUS_NOT_SUPPORTED;
    }

	Irp->IoStatus.Status = Status;
	IoCompleteRequest(Irp, 0);
	return Status;
}