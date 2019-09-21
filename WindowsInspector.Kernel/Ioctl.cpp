#include "Ioctl.hpp"
#include <WindowsInspector.Kernel/Providers/Providers.hpp>
#include <WindowsInspector.Kernel/Debug.hpp>
#include "EventBuffer.hpp"
#include "Common.hpp"


NTSTATUS InitializeIoctlHandlers()
{
    return ZeroEventBuffer();
}

NTSTATUS InspectorListen(PIRP Irp, PIO_STACK_LOCATION IoStackLocation)
{
    D_INFO("Got IOCTL to listen to events.");

    CircularBuffer** UserBaseAddressPtr = NULL;
    NTSTATUS Status = STATUS_SUCCESS;

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength != sizeof(PVOID))
    {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    UserBaseAddressPtr = (CircularBuffer**)Irp->AssociatedIrp.SystemBuffer;
    
    D_INFO("Initializing Event Buffer..");

    Status = InitializeEventBuffer(UserBaseAddressPtr);

    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS("Could not initialize event buffer", Status);
        return Status;
    }
    
    // Return address to user mode
    Irp->IoStatus.Information = sizeof(ULONG_PTR);
    
    Status = InitializeProviders();

    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS("Could not initialize providers.", Status);
    }
 
    return Status;
}

NTSTATUS InspectorStop()
{
    NTSTATUS Status = STATUS_SUCCESS;

    FreeProviders();
    Status = FreeEventBuffer();
    
    return Status;
}
