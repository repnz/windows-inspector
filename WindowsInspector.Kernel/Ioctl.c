#include "Ioctl.h"
#include <WindowsInspector.Kernel/Providers/Providers.h>
#include <WindowsInspector.Kernel/Debug.h>
#include "EventBuffer.h"
#include "Common.h"


NTSTATUS 
InitializeIoctlHandlers(
    VOID
    )
{
    return ZeroEventBuffer();
}

NTSTATUS 
InspectorListen(
    __in PIRP Irp, 
    __in PIO_STACK_LOCATION IoStackLocation)
{
    D_INFO("Got IOCTL to listen to events.");

    PCIRCULAR_BUFFER* UserBaseAddressPtr = NULL;
    NTSTATUS Status = STATUS_SUCCESS;

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength != sizeof(PVOID))
    {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    UserBaseAddressPtr = (PCIRCULAR_BUFFER*)Irp->AssociatedIrp.SystemBuffer;
    
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

NTSTATUS 
InspectorStop(
    VOID
    )
{
    NTSTATUS Status = STATUS_SUCCESS;

    FreeProviders();
    Status = FreeEventBuffer();
    
    return Status;
}
