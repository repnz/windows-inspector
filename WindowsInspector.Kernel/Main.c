#include <ntifs.h>
#include <WindowsInspector.Kernel/KernelApi.h>
#include <WindowsInspector.Kernel/Debug.h>
#include <WindowsInspector.Kernel/Ioctl.h>
#include <WindowsInspector.Shared/Common.h>
#include <WindowsInspector.Kernel/DriverObject.h>

NTSTATUS 
DefaultDispatch(
    __in PDEVICE_OBJECT DeviceObject, 
    __inout PIRP Irp
    );

NTSTATUS 
DeviceIoControlDispatch(
    __in PDEVICE_OBJECT DeviceObject, 
    __inout PIRP Irp
    );

VOID 
DriverUnload(
    __in PDRIVER_OBJECT DriverObject
    );

NTSTATUS 
DriverEntry(
    __in PDRIVER_OBJECT DriverObject,
    __in PUNICODE_STRING RegistryPath
    ) 
{

	UNREFERENCED_PARAMETER(DriverObject);
	UNREFERENCED_PARAMETER(RegistryPath);
    
    D_INFO("Driver Is Starting!");
    D_INFO("Initializing Kernel API...");

	if (!NT_SUCCESS(KernelApiInitialize()))
	{
		D_ERROR("Could not initialize kernel API");
		return STATUS_NOT_SUPPORTED;
	}
    
	NTSTATUS Status = STATUS_SUCCESS;

	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\WindowsInspector");
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\WindowsInspector");

	D_INFO("Creating Device...");

    InitializeIoctlHandlers();
    
    // ExclusiveAccess: TRUE
	Status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &g_DeviceObject);

	if (!NT_SUCCESS(Status))
	{
		D_ERROR_STATUS("Failed to create device", Status);
        return Status;
	}
     
	D_INFO("Creating Symbolic Link..");

	Status = IoCreateSymbolicLink(&symLink, &devName);

	if (!NT_SUCCESS(Status))
	{
		D_ERROR_STATUS("Failed to create symbolic link", Status);
        IoDeleteDevice(g_DeviceObject);
        return Status;
	}

	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControlDispatch;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DefaultDispatch;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DefaultDispatch;
	DriverObject->DriverUnload = DriverUnload;

    D_INFO("Completed DriverEntry Successfully");

	return Status;
}

VOID 
DriverUnload(
    __in PDRIVER_OBJECT DriverObject
    )
{
	// Remove callbacks
	D_INFO("DriverUnload");

	// Remove Device
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\WindowsInspector");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);
}


NTSTATUS 
DefaultDispatch(
    __in PDEVICE_OBJECT DeviceObject, 
    __inout PIRP Irp
    )
{
	UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Information = 0;
    Irp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(Irp, 0);
    return STATUS_SUCCESS;
}

NTSTATUS 
DeviceIoControlDispatch(
    __in PDEVICE_OBJECT DeviceObject, 
    __inout PIRP Irp
    )
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
    else if (IoControlCode == INSPECTOR_STOP_CTL_CODE)
    {
        Status = InspectorStop();
    }
    else
    {
        Status = STATUS_NOT_SUPPORTED;
    }
   
	Irp->IoStatus.Status = Status;
	IoCompleteRequest(Irp, 0);
	return Status;
}