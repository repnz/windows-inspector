#include "EventBuffer.h"
#include "Common.h"
#include "Debug.h"
#include "KernelApi.h"

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
    
    union _KernelModeCircularBuffer {
        // Kernel addresses base pointers
        PVOID KernelModeBase;
        PCIRCULAR_BUFFER CircularBuffer;  
    } KernelModeCircularBuffer;

    ULONG ClientProcessId;
    PVOID UserModeBase;
    LONG IsMapped;
    KEVENT EventObject;
    SIZE_T BufferSize;
    LONG LastPadding;
    FAST_MUTEX HeapWriterMutex;
    FAST_MUTEX PointerWriterMutex;
    ULONG HeapOffset;

} g_Sync;


NTSTATUS 
ZeroEventBuffer(
    VOID
    )
{
    g_Sync = { 0 };
    
    return STATUS_SUCCESS;
}

NTSTATUS 
FreeEventBuffer(
    VOID
    )
{
    NTSTATUS Status = STATUS_SUCCESS;
    HANDLE ProcessHandle = NULL;

    if (!g_Sync.IsMapped)
    {
        return STATUS_SUCCESS;
    }

    MmUnlockPages(g_Sync.Mdl);
    IoFreeMdl(g_Sync.Mdl);
    
    Status = GetProcessHandleById(g_Sync.ClientProcessId, &ProcessHandle);
    
    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS_ARGS("Could not get a handle to process %d to free allocation", Status, g_Sync.ClientProcessId);
        return Status;
    }

    Status = ZwFreeVirtualMemory(ProcessHandle, &g_Sync.UserModeBase, NULL, MEM_RELEASE);

    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS("ZwFreeVirtualMemory failed to free allocation.", Status);
    }

    ObCloseHandle(ProcessHandle, KernelMode);
    return Status;

}

NTSTATUS 
MapUserModeAddressToSystemSpace(
    __in PVOID Buffer, 
    __in ULONG Length, 
    __in LOCK_OPERATION Operation,
    __out PMDL* OutputMdl, 
    __out PVOID* MappedBuffer
    ) 
{

    if (OutputMdl == NULL || MappedBuffer == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS Status;

    PMDL mdl = IoAllocateMdl(Buffer, Length, FALSE, FALSE, NULL);

    if (mdl == NULL)
    {
        D_ERROR_ARGS("Failed to allocate MDL for buffer 0x%p", Buffer);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    __try
    {

        MmProbeAndLockPages(mdl, UserMode, Operation);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {

        Status = GetExceptionCode();
        D_ERROR_STATUS_ARGS("Exception while locking buffer 0x%p", Status, Buffer);
        IoFreeMdl(mdl);
        return Status;
    }

    PVOID buffer = MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority | MdlMappingNoExecute);

    if (!buffer)
    {
        D_ERROR_ARGS("Could not call MmGetSystemAddressForMdlSafe() with buffer: 0x%p", Buffer);
        MmUnlockPages(mdl);
        IoFreeMdl(mdl);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    *OutputMdl = mdl;
    *MappedBuffer = buffer;

    return STATUS_SUCCESS;
}


NTSTATUS 
InitializeEventBuffer(
    __out PCIRCULAR_BUFFER* CircularBufferAddress
    )
{
    NTSTATUS Status = STATUS_SUCCESS;
    BOOLEAN AllocatedUserModeMemory = FALSE;
    BOOLEAN MappedUserModeMemory = FALSE;
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

    AllocatedUserModeMemory = TRUE;
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

    MappedUserModeMemory = TRUE;

    g_Sync.CircularBuffer->Count = 0;
    g_Sync.CircularBuffer->BaseAddress = (PCHAR)g_Sync.UserModeBase + POINTER_BUFFER_OFFSET;

    D_INFO("Creating NotificationEvent...");

    OBJECT_ATTRIBUTES attr;
    InitializeObjectAttributes(&attr, NULL, 0, NULL, NULL);

    Status = ZwCreateEvent(&g_Sync.CircularBuffer->Event, EVENT_ALL_ACCESS, &attr, NotificationEvent, FALSE);

    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS("Could not create event for user-kernel communication", Status);
        goto cleanup;
    }

    g_Sync.CircularBuffer->WriteOffset = 0;
    g_Sync.CircularBuffer->ReadOffset = 0;
    g_Sync.CircularBuffer->BufferSize = POINTER_BUFFER_SIZE;

    D_INFO("Initializing Mutex Objects..");

    ExInitializeFastMutex(&g_Sync.HeapWriterMutex);
    ExInitializeFastMutex(&g_Sync.PointerWriterMutex);

    g_Sync.IsMapped = TRUE;

cleanup:
    if (!NT_SUCCESS(Status))
    {   
        if (MappedUserModeMemory)
        {
            MmUnlockPages(g_Sync.Mdl);
            IoFreeMdl(g_Sync.Mdl);
        }

        if (AllocatedUserModeMemory)
        {
            ZwFreeVirtualMemory(NtCurrentProcess(), &g_Sync.UserModeBase, NULL, MEM_RELEASE);
        }

        g_Sync = { 0 };
        
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
        InterlockedAdd(&g_Sync.CircularBuffer->MemoryLeft, g_Sync.LastPadding - Left);
        g_Sync.LastPadding = Left;
        g_Sync.HeapOffset = 0;
    }
    
    *EventPtr = (PEVENT_HEADER)((PUCHAR)g_Sync.KernelModeBase + g_Sync.HeapOffset);
    (*EventPtr)->Size = EventSize;
    
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
    __in PEVENT_HEADER Event
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

    KeSetEvent(&g_Sync.EventObject, 0, FALSE);

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
    __in PEVENT_HEADER Event
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

    InterlockedAdd(&g_Sync.CircularBuffer->MemoryLeft, Event->Size);
    return STATUS_SUCCESS;
}