#include "Ioctl.hpp"
#include <WindowsInspector.Kernel/Providers/Providers.hpp>
#include <WindowsInspector.Kernel/Debug.hpp>
#include "Common.hpp"

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


void InitializeIoctlHandlers()
{
    KernelMappedBuffer = { 0 };
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

NTSTATUS CancelEvent(BufferEvent* Event)
{
    if (Event == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    ExReleaseFastMutex(&KernelMappedBuffer.BufferWriterMutex);
    return STATUS_SUCCESS;
}

NTSTATUS InspectorListen(PIRP Irp)
{
    if (BitTestAndSet(&KernelMappedBuffer.IsMapped, 0)) 
    {
        // Already mapped. 
        return STATUS_ALREADY_INITIALIZED;
    }
    
    SIZE_T AllocationSize = BUFFER_SIZE;
    PVOID UserBaseAddress = NULL;

    // Allocate pages for buffer
    NTSTATUS status = ZwAllocateVirtualMemory(
        NtCurrentProcess(), // ProcessHandle
        &UserBaseAddress, // BaseAddress
        0, // ZeroBits
        &AllocationSize, // RegionSize 
        MEM_RESERVE | MEM_COMMIT, // AllocationType
        PAGE_READWRITE // Protect
    );

    if (!NT_SUCCESS(status))
    {
        
        D_ERROR_STATUS("Could not allocate communication buffer", status);
        KernelMappedBuffer.IsMapped = 0;
        return status;
    }

    PMDL mdl = IoAllocateMdl(UserBaseAddress, (ULONG)AllocationSize, FALSE, FALSE, NULL);
    
    if (mdl == NULL)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        ZwFreeVirtualMemory(NtCurrentProcess(), &UserBaseAddress, NULL, MEM_RELEASE);
        D_ERROR("Could not allocate MDL for buffer");
        KernelMappedBuffer.IsMapped = 0;
        return status;
    }

    __try
    {
        MmProbeAndLockPages(mdl, KernelMode, IoWriteAccess);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        IoFreeMdl(mdl);
        ZwFreeVirtualMemory(NtCurrentProcess(), &UserBaseAddress, NULL, MEM_RELEASE);
        D_ERROR("Could not lock pages for buffer memory");
        KernelMappedBuffer.IsMapped = 0;
        return STATUS_UNSUCCESSFUL;
    }


    PVOID KernelMemory = MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority | MdlMappingNoExecute);

    if (KernelMemory == NULL)
    {
        MmUnlockPages(mdl);
        IoFreeMdl(mdl);
        ZwFreeVirtualMemory(NtCurrentProcess(), &UserBaseAddress, NULL, MEM_RELEASE);
        KernelMappedBuffer.IsMapped = 0;
        return STATUS_INSUFFICIENT_RESOURCES;
    }


    CircularBuffer* buffer = (CircularBuffer*)KernelMemory;

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

    buffer->ResetOffset = BUFFER_SIZE - (1024*1024);
    buffer->HeadOffset = 0;
    buffer->TailOffset = 0;


    KernelMappedBuffer.Mdl = mdl;
    KernelMappedBuffer.KernelMemory = KernelMemory;
    KernelMappedBuffer.UserModeMemory = UserBaseAddress;
    KernelMappedBuffer.CircularBuffer = (CircularBuffer*)KernelMemory;
    KernelMappedBuffer.BufferSize = (ULONG)AllocationSize;

    ExInitializeFastMutex(&KernelMappedBuffer.BufferWriterMutex);

    // Return address to user mode
    *((CircularBuffer**)Irp->AssociatedIrp.SystemBuffer) = (CircularBuffer*)UserBaseAddress;
    Irp->IoStatus.Information = sizeof(ULONG_PTR);
    return STATUS_SUCCESS;
}