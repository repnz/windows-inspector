#include "EventBuffer.h"
#include <WindowsInspector.Shared/Common.h>
#include "Debug.h"
#include "KernelApi.h"
#include "ProcessNotifyWrapper.h"

// 20 mb
#define BUFFER_SIZE (20 * 1024 * 1024)

// 78 kb
#define POINTER_BUFFER_SIZE (10 * 1000 * sizeof(PVOID))

// 20891520 bytes 
#define HEAP_SIZE (BUFFER_SIZE - sizeof(CIRCULAR_BUFFER) - POINTER_BUFFER_SIZE)

// Offsets into the buffer
#define POINTER_BUFFER_OFFSET sizeof(CIRCULAR_BUFFER)
#define HEAP_OFFSET (POINTER_BUFFER_OFFSET + POINTER_BUFFER_SIZE)


static struct {
    PMDL Mdl;
    union {
        PVOID KernelModeBase;
        PCIRCULAR_BUFFER CircularBuffer;
    };
    ULONG ClientProcessId;
	HANDLE ClientProcessHandle;
    PVOID UserModeBase;
    LONG IsMapped;
    PKEVENT EventObject;
    SIZE_T BufferSize;
    LONG LastPadding;
    FAST_MUTEX HeapWriterMutex;
    FAST_MUTEX PointerWriterMutex;
    ULONG HeapOffset;

} g_Sync;


//
// Functions to manage the user mode event object
//
static
NTSTATUS
CreateUserModeNotificationEvent(
	VOID
);

static
VOID
FreeUserModeNotificationEvent(
	VOID
);



//
// Functions to manage the user mode buffer and mapping to system space
//
static
NTSTATUS
AllocateBufferMemory(
	VOID
);


static
VOID
FreeBufferMemory(
	VOID
);



static
BOOLEAN
EventBufferOnProcessNotify(
	__inout PEPROCESS Process,
	__in HANDLE ProcessId,
	__inout_opt PPS_CREATE_NOTIFY_INFO CreateInfo
	)
{
	UNREFERENCED_PARAMETER(Process);

	// Ignore process creation
	if (CreateInfo)
		return TRUE;

	if (HandleToUlong(ProcessId) == g_Sync.ClientProcessId) 
	{	
		FreeEventBuffer();
		return FALSE;
	}

	return TRUE;
}

NTSTATUS
InitializeEventBuffer(
	__out PCIRCULAR_BUFFER* CircularBufferAddress
)
{
	NTSTATUS Status = STATUS_SUCCESS;
	g_Sync.BufferSize = BUFFER_SIZE;

	if (CircularBufferAddress == NULL)
	{
		return STATUS_INVALID_PARAMETER;
	}

	if (g_Sync.IsMapped)
	{
		// Already mapped. 
		return STATUS_ALREADY_INITIALIZED;
	}

	g_Sync.ClientProcessId = HandleToUlong(PsGetCurrentProcessId());
	Status = GetProcessHandleById(g_Sync.ClientProcessId, &g_Sync.ClientProcessHandle);

	if (!NT_SUCCESS(Status))
	{
		D_ERROR_STATUS_ARGS("GetProcessHandleById failed: Could not get a handle to process %d", Status, g_Sync.ClientProcessId);
		return Status;
	}

	Status = AllocateBufferMemory();

	if (!NT_SUCCESS(Status))
	{
		D_ERROR_STATUS("Could not allocate buffer event", Status);
		goto cleanup;
	}

	g_Sync.CircularBuffer->Count = 0;
	g_Sync.CircularBuffer->BaseAddress = (PCHAR)g_Sync.UserModeBase + POINTER_BUFFER_OFFSET;

	Status = CreateUserModeNotificationEvent();

	if (!NT_SUCCESS(Status))
	{
		D_ERROR_STATUS("Could not create user mode notification event", Status);
		goto cleanup;
	}

	g_Sync.CircularBuffer->WriteOffset = 0;
	g_Sync.CircularBuffer->ReadOffset = 0;
	g_Sync.CircularBuffer->BufferSize = POINTER_BUFFER_SIZE;
	g_Sync.CircularBuffer->MemoryLeft = HEAP_SIZE;

	D_INFO("Initializing Mutex Objects..");

	ExInitializeFastMutex(&g_Sync.HeapWriterMutex);
	ExInitializeFastMutex(&g_Sync.PointerWriterMutex);

	AddProcessNotifyRoutine(&EventBufferOnProcessNotify);

	g_Sync.IsMapped = TRUE;

cleanup:
	if (!NT_SUCCESS(Status))
	{
		FreeEventBuffer();
	}
	else
	{
		//
		// Return the base address to the caller
		//
		*CircularBufferAddress = (PCIRCULAR_BUFFER)g_Sync.UserModeBase;
	}

	return Status;
}

VOID
FreeEventBuffer(
	VOID
)
{
	if (!g_Sync.IsMapped)
	{
		return;
	}

	FreeBufferMemory();
	FreeUserModeNotificationEvent();

	if (g_Sync.ClientProcessHandle) 
	{
		ObCloseHandle(g_Sync.ClientProcessHandle, KernelMode);
	}

}

NTSTATUS
ZeroEventBuffer(
	VOID
)
{
	RtlZeroMemory(&g_Sync, sizeof(g_Sync));

	return STATUS_SUCCESS;
}


static
NTSTATUS
CreateUserModeNotificationEvent(
	VOID
	)
{
	D_INFO("Creating NotificationEvent...");

	OBJECT_ATTRIBUTES ObjectAttributes;
	NTSTATUS Status;
	InitializeObjectAttributes(&ObjectAttributes, NULL, 0, NULL, NULL);

	Status = ZwCreateEvent(&g_Sync.CircularBuffer->Event, EVENT_ALL_ACCESS, &ObjectAttributes, NotificationEvent, FALSE);

	if (!NT_SUCCESS(Status))
	{
		D_ERROR_STATUS("Could not create event for user-kernel communication", Status);
		goto cleanup;
	}

	Status = ObReferenceObjectByHandle(
		g_Sync.CircularBuffer->Event,
		EVENT_ALL_ACCESS,
		*ExEventObjectType,
		KernelMode,
		&g_Sync.EventObject,
		NULL
	);

	if (!NT_SUCCESS(Status))
	{
		D_ERROR_STATUS("ObReferenceObjectByHandle failed. Cannot get event object from handle", Status);
		goto cleanup;
	}

cleanup:
	if (!NT_SUCCESS(Status))
	{
		FreeUserModeNotificationEvent();
	}

	return Status;
}


static
VOID
FreeUserModeNotificationEvent(
	VOID
)
{
	if (g_Sync.CircularBuffer && g_Sync.CircularBuffer->Event)
	{
		ObCloseHandle(g_Sync.CircularBuffer->Event, UserMode);
		g_Sync.CircularBuffer->Event = NULL;
	}

	if (g_Sync.EventObject)
	{
		ObDereferenceObject(g_Sync.EventObject);
		g_Sync.EventObject = NULL;
	}
}


static
NTSTATUS
AllocateBufferMemory(
	VOID
	)
{
	NTSTATUS Status;

	D_INFO("ZwAllocateVirtualMemory: Allocating event buffer...");

	// Allocate pages for buffer
	Status = ZwAllocateVirtualMemory(
		NtCurrentProcess(),
		&g_Sync.UserModeBase,
		0,
		&g_Sync.BufferSize,
		MEM_RESERVE | MEM_COMMIT,
		PAGE_READWRITE
	);

	if (!NT_SUCCESS(Status))
	{
		D_ERROR_STATUS("Could not allocate communication buffer", Status);
		goto cleanup;
	}

	D_INFO_ARGS("Event buffer allocated! UserModeBase=0x%p", g_Sync.UserModeBase);
	D_INFO("Mapping EventBuffer to system space...");

	Status = MapUserModeAddressToSystemSpace(
		g_Sync.UserModeBase,
		BUFFER_SIZE,
		IoWriteAccess,
		&g_Sync.Mdl,
		&g_Sync.KernelModeBase
	);

	if (!NT_SUCCESS(Status))
	{
		D_ERROR_STATUS("Could not map circular buffer address to system space", Status);
		goto cleanup;
	}

	D_INFO_ARGS("EventBuffer mapped to system space at 0x%p", g_Sync.KernelModeBase);

cleanup:
	if (!NT_SUCCESS(Status))
	{
		FreeBufferMemory();
	}

	return Status;
}

static
VOID
FreeBufferMemory(
	VOID
)
{
	if (g_Sync.Mdl)
	{
		UnlockFreeMdl(g_Sync.Mdl);
		g_Sync.Mdl = NULL;
	}

	if (g_Sync.UserModeBase)
	{
		ZwFreeVirtualMemory(g_Sync.ClientProcessHandle, &g_Sync.UserModeBase, NULL, MEM_RELEASE);
		g_Sync.UserModeBase = NULL;
	}
}

NTSTATUS 
AllocateBufferEvent(
    __out PVOID Event, 
    __in USHORT EventSize
    )
{
    
    NTSTATUS Status = STATUS_SUCCESS;
    BOOLEAN MutexAcquired = FALSE;

    if (!g_Sync.IsMapped)
    {
        Status = STATUS_UNSUCCESSFUL;
        goto cleanup;
    }

    if (Event == NULL)
    {
        Status = STATUS_INVALID_PARAMETER;
        goto cleanup;
    }

    PEVENT_HEADER* EventPtr = (PEVENT_HEADER*)Event;

    ExAcquireFastMutex(&g_Sync.HeapWriterMutex);
    MutexAcquired = TRUE;
    
    if (g_Sync.CircularBuffer->MemoryLeft < EventSize)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto cleanup;
    }
    
    LONG Left = HEAP_SIZE - g_Sync.HeapOffset;

    if (Left < EventSize)
    {
		// Release Padding Memory
        InterlockedAdd(&g_Sync.CircularBuffer->MemoryLeft, g_Sync.LastPadding - Left);
        g_Sync.LastPadding = Left;
        g_Sync.HeapOffset = 0;
    }
    
    *EventPtr = (PEVENT_HEADER)((PUCHAR)g_Sync.KernelModeBase + g_Sync.HeapOffset);
    (*EventPtr)->Size = EventSize;
    RtlZeroMemory(*EventPtr, EventSize);
    g_Sync.HeapOffset += EventSize;

cleanup:
    if (MutexAcquired)
    {
        ExReleaseFastMutex(&g_Sync.HeapWriterMutex);
    }

    
    return Status;
}

NTSTATUS 
SendBufferEvent(
    __in PVOID Event
    )
{
    if (!g_Sync.IsMapped)
    {
        return STATUS_UNSUCCESSFUL;
    }

    if (Event == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }
    
    NTSTATUS Status = STATUS_SUCCESS;

    ExAcquireFastMutex(&g_Sync.PointerWriterMutex);

    ULONG WriteOffset = g_Sync.CircularBuffer->WriteOffset;
    ULONG ReadOffset = g_Sync.CircularBuffer->ReadOffset;
    
    
    // We have to check if it's empty
    // It can be one of two options:
    // 1. Tail is writing very fast and it reached HeadOffset
    // 2. Head is reading very fast and it reached TailOffset
    // We can check how many events there are in the buffer to verify the state:

    if (WriteOffset == ReadOffset && g_Sync.CircularBuffer->Count > 0)
    {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto cleanup;
    }

    KeSetEvent(g_Sync.EventObject, 0, FALSE);

    ULONG EventHeapOffset = (ULONG)((PUCHAR)Event - (PUCHAR)g_Sync.KernelModeBase);
       
    
    PVOID* QueuePointer = (PVOID*)((PCHAR)g_Sync.KernelModeBase + POINTER_BUFFER_OFFSET + WriteOffset);
    
    *QueuePointer = (PUCHAR)g_Sync.UserModeBase + HEAP_OFFSET + EventHeapOffset;

    // Point to the next pointer to write
    WriteOffset += sizeof(PVOID);

    if (WriteOffset == POINTER_BUFFER_SIZE)
    {
        WriteOffset = 0;
    }

    g_Sync.CircularBuffer->WriteOffset = WriteOffset;

cleanup:
    
    if (NT_SUCCESS(Status))
    {
        InterlockedIncrement(&g_Sync.CircularBuffer->Count);
    }
    
    ExReleaseFastMutex(&g_Sync.PointerWriterMutex);
    return Status;
}

NTSTATUS 
CancelBufferEvent(
    __in PVOID Event
    )
{
    PEVENT_HEADER EventObj = (PEVENT_HEADER)Event;

    if (!g_Sync.IsMapped)
    {
        return STATUS_UNSUCCESSFUL;
    }
    if (Event == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    InterlockedAdd(&g_Sync.CircularBuffer->MemoryLeft, EventObj->Size);
    return STATUS_SUCCESS;
}

NTSTATUS 
SendOrCancelBufferEvent(
	__in PVOID Event
	)
{
	NTSTATUS Status = SendBufferEvent(Event);

	if (!NT_SUCCESS(Status))
	{
		PCSTR EventTypeStr = Event_GetEventName((PEVENT_HEADER)Event);
		D_ERROR_STATUS_ARGS("Could not send buffer event of type '%s'", Status, EventTypeStr);
		CancelBufferEvent(Event);
	}

	return Status;
}