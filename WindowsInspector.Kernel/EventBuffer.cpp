#include "EventBuffer.hpp"
#include "Common.hpp"
#include "Debug.hpp"

const SIZE_T BUFFER_SIZE = 20 * 1024 * 1024;

static struct _KernelMappedBuffer {
    PMDL Mdl;
    PVOID KernelMemory;
    PVOID UserModeMemory;
    CircularBuffer* CircularBuffer;
    LONG IsMapped;
    KEVENT EventObject;
    FAST_MUTEX BufferWriterMutex;
    ULONG BufferSize;
} KernelMappedBuffer;

NTSTATUS ZeroEventBuffer()
{
    KernelMappedBuffer = { 0 };

    return STATUS_SUCCESS;
}


NTSTATUS MapUserModeAddressToSystemSpace(_In_ PVOID Buffer, _In_ ULONG Length, _In_ LOCK_OPERATION Operation,
    _Out_ PMDL* OutputMdl, _Out_ PVOID* MappedBuffer) {

    if (OutputMdl == NULL || MappedBuffer == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS status;

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

        status = GetExceptionCode();
        D_ERROR_STATUS_ARGS("Exception while locking buffer 0x%p", status, Buffer);
        IoFreeMdl(mdl);
        return status;
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


NTSTATUS InitializeEventBuffer(CircularBuffer ** CircularBufferAddress)
{
    if (CircularBufferAddress == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (KernelMappedBuffer.IsMapped || BitTestAndSet(&KernelMappedBuffer.IsMapped, 0))
    {
        // Already mapped. 
        return STATUS_ALREADY_INITIALIZED;
    }

    SIZE_T AllocationSize = BUFFER_SIZE;
    PVOID UserBaseAddress = NULL;

    // Allocate pages for buffer
    NTSTATUS status = ZwAllocateVirtualMemory(NtCurrentProcess(), &UserBaseAddress, 0, &AllocationSize,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    if (!NT_SUCCESS(status))
    {
        D_ERROR_STATUS("Could not allocate communication buffer", status);
        KernelMappedBuffer.IsMapped = 0;
        return status;
    }

    PMDL mdl;
    PVOID kernelMemory;

    status = MapUserModeAddressToSystemSpace(UserBaseAddress, (ULONG)AllocationSize, IoWriteAccess, &mdl, &kernelMemory);

    if (!NT_SUCCESS(status))
    {
        D_ERROR_STATUS("Could not map circular buffer address to system space", status);
        KernelMappedBuffer.IsMapped = 0;
        ZwFreeVirtualMemory(NtCurrentProcess(), &UserBaseAddress, NULL, MEM_RELEASE);
        return status;
    }


    CircularBuffer* buffer = (CircularBuffer*)kernelMemory;

    buffer->Count = 0;
    buffer->BaseAddress = (PCHAR)UserBaseAddress + sizeof(CircularBuffer);

    OBJECT_ATTRIBUTES attr;
    InitializeObjectAttributes(&attr, NULL, 0, NULL, NULL);

    status = ZwCreateEvent(&buffer->Event, EVENT_ALL_ACCESS, &attr, SynchronizationEvent, FALSE);

    if (!NT_SUCCESS(status))
    {
        D_ERROR_STATUS("Could not create event for user-kernel communication", status);

        MmUnlockPages(mdl);
        IoFreeMdl(mdl);
        ZwFreeVirtualMemory(NtCurrentProcess(), &UserBaseAddress, NULL, MEM_RELEASE);
        KernelMappedBuffer.IsMapped = 0;
        return status;
    }

    buffer->ResetOffset = BUFFER_SIZE - (1024 * 1024);
    buffer->HeadOffset = 0;
    buffer->TailOffset = 0;


    KernelMappedBuffer.Mdl = mdl;
    KernelMappedBuffer.KernelMemory = (PCHAR)kernelMemory + sizeof(CircularBuffer);
    KernelMappedBuffer.UserModeMemory = UserBaseAddress;
    KernelMappedBuffer.CircularBuffer = (CircularBuffer*)kernelMemory;
    KernelMappedBuffer.BufferSize = (ULONG)AllocationSize;

    ExInitializeFastMutex(&KernelMappedBuffer.BufferWriterMutex);

    *CircularBufferAddress = (CircularBuffer*)UserBaseAddress;
    return STATUS_SUCCESS;
}

NTSTATUS WaitForEvents()
{
    
}

NTSTATUS AllocateBufferEvent(BufferEvent* Event, ULONG EventSize)
{
    if (!KernelMappedBuffer.IsMapped)
    {
        return STATUS_UNSUCCESSFUL;
    }

    if (Event == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    ExAcquireFastMutex(&KernelMappedBuffer.BufferWriterMutex);

    CircularBuffer* buf = KernelMappedBuffer.CircularBuffer;

    ULONG HeadOffset = buf->HeadOffset;
    ULONG TailOffset = buf->TailOffset;

    if (HeadOffset < TailOffset)
    {
        // Head is behind! That means we have to check until the end of entire the buffer;
        if ((KernelMappedBuffer.BufferSize - TailOffset) >= EventSize)
        {
            // Good! we have space for the event.
            goto finish_allocate;
        }
        else
        {
            // Haa.. we can try to reset the TailOffset to the beginning:
            TailOffset = 0;
        }
    }

    if (HeadOffset > TailOffset)
    {
        if ((HeadOffset - TailOffset) >= EventSize)
        {
            // Good! We have space for the event.
            // The HeadOffset may even got further, but we don't care because we still have space
            goto finish_allocate;
        }
        else
        {
            // We have no space for the event! what to do..?
            // We may just drop the event.
            ExReleaseFastMutex(&KernelMappedBuffer.BufferWriterMutex);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    if (HeadOffset == TailOffset)
    {
        // It can be one of two options:
        // 1. Tail is writing very fast and it reached HeadOffset
        // 2. Head is reading very fast and it reached TailOffset
        // We can check how many events there are in the buffer to verify the state:
        if (buf->Count == 0)
        {
            // Yay! we have space. Head is pretty fast:)
            // That will happen alot. Mostly if there is one moment of silence
            goto finish_allocate;
        }
        else
        {
            ExReleaseFastMutex(&KernelMappedBuffer.BufferWriterMutex);
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }


finish_allocate:
    TailOffset += EventSize;
    Event->Memory = (PVOID)((PCHAR)KernelMappedBuffer.KernelMemory + TailOffset);

    if (TailOffset > buf->ResetOffset)
    {
        // We need to set it to zero because it passed the ResetOffset
        TailOffset = 0;
    }

    Event->NewTail = TailOffset;

    // Keep the mutex.
    return STATUS_SUCCESS;

}

NTSTATUS SendBufferEvent(BufferEvent* EventMemory)
{
    if (EventMemory == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    KernelMappedBuffer.CircularBuffer->TailOffset = EventMemory->NewTail;
    InterlockedIncrement(&KernelMappedBuffer.CircularBuffer->Count);
    ExReleaseFastMutex(&KernelMappedBuffer.BufferWriterMutex);
    return STATUS_SUCCESS;
}

NTSTATUS CancelBufferEvent(BufferEvent* Event)
{
    if (Event == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    ExReleaseFastMutex(&KernelMappedBuffer.BufferWriterMutex);
    return STATUS_SUCCESS;
}