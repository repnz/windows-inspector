#include "Ioctl.h"
#include <WindowsInspector.Kernel/Providers/Providers.h>
#include <WindowsInspector.Kernel/Debug.h>
#include "EventBuffer.h"
#include <WindowsInspector.Shared/Common.h>
#include "ProcessNotifyWrapper.h"


static FAST_MUTEX g_IoctlMutex;

NTSTATUS 
InitializeIoctlHandlers(
    VOID
    )
{
	InitializeProcessNotify();
	ExInitializeFastMutex(&g_IoctlMutex);
    return ZeroEventBuffer();
}

NTSTATUS 
InspectorListen(
    __in PIRP Irp, 
    __in PIO_STACK_LOCATION IoStackLocation
	)
{

	PCIRCULAR_BUFFER* UserBaseAddressPtr = NULL;
	NTSTATUS Status = STATUS_SUCCESS;
	BOOLEAN InitializedEventBuffer = FALSE;
	BOOLEAN InitializedProviders = FALSE;

	ExAcquireFastMutex(&g_IoctlMutex);

	if (g_Listening)
	{
		 Status = STATUS_ALREADY_INITIALIZED;
		 goto cleanup;
	}

    D_INFO("Got IOCTL to listen to events.");

    if (IoStackLocation->Parameters.DeviceIoControl.OutputBufferLength != sizeof(PVOID))
    {
        Status = STATUS_INVALID_BUFFER_SIZE;
		goto cleanup;
    }

    UserBaseAddressPtr = (PCIRCULAR_BUFFER*)Irp->AssociatedIrp.SystemBuffer;
	
    D_INFO("Initializing Event Buffer..");

    Status = InitializeEventBuffer(UserBaseAddressPtr);

    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS("Could not initialize event buffer", Status);
        goto cleanup;
    }
    
    Status = InitializeProviders();

    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS("Could not initialize providers.", Status);
		goto cleanup;
    }

	Status = StartProcessNotify();
	
	if (!NT_SUCCESS(Status))
	{
		D_ERROR_STATUS("Could not start process notifications", Status);
		goto cleanup;
	}

	// Return address to user mode
	Irp->IoStatus.Information = sizeof(ULONG_PTR);

	g_Listening =  TRUE;

cleanup:
	if (!NT_SUCCESS(Status))
	{
		if (InitializedProviders)
		{
			FreeProviders();
		}

		if (InitializedEventBuffer)
		{
			FreeEventBuffer();
		}
	}

	ExReleaseFastMutex(&g_IoctlMutex);

    return Status;
}


NTSTATUS
InspectorStop(
    VOID
    )
{
	NTSTATUS Status = STATUS_SUCCESS;

	ExAcquireFastMutex(&g_IoctlMutex);

	if (g_Listening)
	{
		Status = STATUS_INVALID_STATE_TRANSITION;
		goto cleanup;
	}

	g_Listening = FALSE;
    FreeProviders();
    FreeEventBuffer();
	StopProcessNotify();

cleanup:
	ExReleaseFastMutex(&g_IoctlMutex);
	return Status;
}
