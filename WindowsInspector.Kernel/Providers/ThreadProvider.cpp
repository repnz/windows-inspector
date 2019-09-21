#include <WindowsInspector.Kernel/Debug.hpp>
#include <WindowsInspector.Kernel/KernelApi.hpp>
#include <WindowsInspector.Kernel/Common.hpp>
#include <WindowsInspector.Kernel/EventBuffer.hpp>
#include "ThreadProvider.hpp"

void OnThreadNotify(
    _In_ HANDLE ProcessId, 
    _In_ HANDLE ThreadId, 
    _In_ BOOLEAN Create
);

NTSTATUS InitializeThreadProvider()
{
    return PsSetCreateThreadNotifyRoutine(OnThreadNotify);
}

void ReleaseThreadProvider()
{
    PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);
}

NTSTATUS GetThreadWin32StartAddress(
    _In_ ULONG ThreadId, 
    _Out_ PULONG_PTR Win32StartAddress
)
{
    NTSTATUS Status;
    HANDLE CreatingThreadObjectHandle;

    PETHREAD thread;
    Status = PsLookupThreadByThreadId(UlongToHandle(ThreadId), &thread);

    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS_ARGS("Failed to find thread with id %d", Status, ThreadId);
        return Status;
    }

    Status = ObOpenObjectByPointer(
        PsGetCurrentThread(),
        OBJ_KERNEL_HANDLE,
        NULL,
        THREAD_QUERY_INFORMATION,
        *PsThreadType,
        KernelMode,
        &CreatingThreadObjectHandle
    );

    ObDereferenceObject(thread);

    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS("Failed to create a handle to the new thread", Status);
        return Status;
    }

    Status = ZwQueryInformationThread(
        CreatingThreadObjectHandle,
        ThreadQuerySetWin32StartAddress,
        Win32StartAddress,
        sizeof(ULONG_PTR),
        NULL
    );

    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS_ARGS("Cannot query thread start address %d", Status, ThreadId);
    }

    ObCloseHandle(CreatingThreadObjectHandle, KernelMode);
    return Status;
}

void OnThreadCreate(
    _In_ ULONG TargetProcessId, 
    _In_ ULONG TargetThreadId
)
{
    
    ULONG_PTR StartAddress;

    NTSTATUS status = GetThreadWin32StartAddress(TargetThreadId, &StartAddress);

    if (!NT_SUCCESS(status))
    {
        D_ERROR_STATUS_ARGS("Could not find thread start address %d", status, TargetThreadId);
        return;
    }

    ThreadCreateEvent* Event;

    status = AllocateBufferEvent(&Event, sizeof(ThreadCreateEvent));

    if (!NT_SUCCESS(status))
    {
        D_ERROR_STATUS("Could not allocate ThreadCreateEvent", status);
        return;
    }

    Event->Size = sizeof(ThreadCreateEvent);
    Event->Type = EventType::ThreadCreate;
    KeQuerySystemTimePrecise(&Event->Time);

    Event->ProcessId = HandleToUlong(PsGetCurrentProcessId());
    Event->ThreadId = HandleToUlong(PsGetCurrentThreadId());
    Event->NewThreadId = TargetThreadId;
    Event->TargetProcessId = TargetProcessId;

    SendBufferEvent(Event);
}


void OnThreadExit(
    _In_ ULONG ProcessId, 
    _In_ ULONG ThreadId
)
{
    ThreadExitEvent* Event;

    NTSTATUS status = AllocateBufferEvent(&Event, sizeof(ThreadExitEvent));

    if (!NT_SUCCESS(status))
    {
        D_ERROR_STATUS("Could not allocate ThreadExitEvent", status);
        return;
    }

    Event->Size = sizeof(ThreadExitEvent);
    Event->Type = EventType::ThreadExit;
    KeQuerySystemTimePrecise(&Event->Time);
    Event->ThreadId = ThreadId;
    Event->ProcessId = ProcessId;

    SendBufferEvent(Event);
}

void OnThreadNotify(_In_ HANDLE ProcessId, _In_ HANDLE ThreadId, _In_ BOOLEAN Create)
{

    if (Create)
    {
        OnThreadCreate(HandleToUlong(ProcessId), HandleToUlong(ThreadId));
    }
    else
    {
        OnThreadExit(HandleToUlong(ProcessId), HandleToUlong(ThreadId));
    }

}
