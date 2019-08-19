#include <WindowsInspector.Kernel/Debug.hpp>
#include <WindowsInspector.Kernel/KernelApi.hpp>
#include <WindowsInspector.Kernel/Common.hpp>
#include <WindowsInspector.Kernel/EventBuffer.hpp>
#include "ThreadProvider.hpp"

void OnThreadNotify(_In_ HANDLE ProcessId, _In_ HANDLE ThreadId, _In_ BOOLEAN Create);

NTSTATUS InitializeThreadProvider()
{
    return PsSetCreateThreadNotifyRoutine(OnThreadNotify);
}

void ReleaseThreadProvider()
{
    PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);
}

NTSTATUS GetThreadWin32StartAddress(_In_ ULONG ThreadId, _Out_ PULONG_PTR Win32StartAddress)
{
    NTSTATUS status;
    HANDLE CreatingThreadObjectHandle;

    PETHREAD thread;
    status = PsLookupThreadByThreadId(UlongToHandle(ThreadId), &thread);

    if (!NT_SUCCESS(status))
    {
        D_ERROR_STATUS_ARGS("Failed to find thread with id %d", status, ThreadId);
        return status;
    }

    status = ObOpenObjectByPointer(
        PsGetCurrentThread(),
        OBJ_KERNEL_HANDLE,
        NULL,
        THREAD_QUERY_INFORMATION,
        *PsThreadType,
        KernelMode,
        &CreatingThreadObjectHandle
    );

    ObDereferenceObject(thread);

    if (!NT_SUCCESS(status))
    {
        D_ERROR_STATUS("Failed to create a handle to the new thread", status);
        return status;
    }

    status = ZwQueryInformationThread(
        CreatingThreadObjectHandle,
        ThreadQuerySetWin32StartAddress,
        Win32StartAddress,
        sizeof(ULONG_PTR),
        NULL
    );

    if (!NT_SUCCESS(status))
    {
        D_ERROR_STATUS_ARGS("Cannot query thread start address %d", status, ThreadId);
        ObCloseHandle(CreatingThreadObjectHandle, KernelMode);
        return status;
    }

    ObCloseHandle(CreatingThreadObjectHandle, KernelMode);
    return STATUS_SUCCESS;
}

void OnThreadCreate(_In_ ULONG TargetProcessId, _In_ ULONG TargetThreadId)
{
    
    ULONG_PTR StartAddress;

    NTSTATUS status = GetThreadWin32StartAddress(TargetThreadId, &StartAddress);

    if (!NT_SUCCESS(status))
    {
        D_ERROR_STATUS_ARGS("Could not find thread start address %d", status, TargetThreadId);
        return;
    }

    BufferEvent event;

    status = AllocateBufferEvent(&event, sizeof(ThreadCreateEvent));

    if (!NT_SUCCESS(status))
    {
        D_ERROR_STATUS("Could not allocate ThreadCreateEvent", status);
        return;
    }

    ThreadCreateEvent* info = (ThreadCreateEvent*)event.Memory;
    info->Size = sizeof(ThreadCreateEvent);
    info->Type = EventType::ThreadCreate;
    KeQuerySystemTimePrecise(&info->Time);

    info->CreatingProcessId = HandleToUlong(PsGetCurrentProcessId());
    info->CreatingThreadId = HandleToUlong(PsGetCurrentThreadId());
    info->NewThreadId = TargetThreadId;
    info->TargetProcessId = TargetProcessId;

    SendBufferEvent(&event);
}


void OnThreadExit(_In_ ULONG ProcessId, _In_ ULONG ThreadId)
{
    BufferEvent event;

    NTSTATUS status = AllocateBufferEvent(&event, sizeof(ThreadExitEvent));

    if (!NT_SUCCESS(status))
    {
        D_ERROR_STATUS("Could not allocate ThreadExitEvent", status);
        return;
    }

    ThreadExitEvent* info = (ThreadExitEvent*)event.Memory;

    info->Size = sizeof(ThreadExitEvent);
    info->Type = EventType::ThreadExit;
    KeQuerySystemTimePrecise(&info->Time);
    info->ThreadId = ThreadId;
    info->ProcessId = ProcessId;

    SendBufferEvent(&event);
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
