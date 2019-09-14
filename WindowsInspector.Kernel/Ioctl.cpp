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
    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength != sizeof(PVOID))
    {
        return STATUS_INVALID_BUFFER_SIZE;
    }

    CircularBuffer** UserBaseAddressPtr = (CircularBuffer**)Irp->AssociatedIrp.SystemBuffer;
    
    NTSTATUS status = InitializeEventBuffer(UserBaseAddressPtr);

    if (!NT_SUCCESS(status))
    {
        D_ERROR_STATUS("Could not allocate event buffer", status);
        return status;
    }
    else
    {
        // Return address to user mode
        Irp->IoStatus.Information = sizeof(ULONG_PTR);
    }

    InitializeProviders();

    return STATUS_SUCCESS;
}