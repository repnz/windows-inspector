#include <WindowsInspector.Kernel/Error.hpp>
#include <WindowsInspector.Kernel/KernelApi.hpp>
#include <WindowsInspector.Kernel/DataItemList.hpp>
#include <WindowsInspector.Kernel/Common.hpp>
#include <WindowsInspector.Kernel/EventList.hpp>
#include <WindowsInspector.Kernel/Mem.hpp>
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
        KdPrint((DRIVER_PREFIX "Failed to find thread with id %d (0x%08X)\n", ThreadId, status));
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
        KdPrintError("Failed to create a handle to the new thread", status);
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
        KdPrint((DRIVER_PREFIX "Cannot query thread start address %d (0x%08X)\n", ThreadId, status));
        ZwClose(CreatingThreadObjectHandle);
        return status;
    }

    ZwClose(CreatingThreadObjectHandle);
    return STATUS_SUCCESS;
}

void OnThreadStart(_In_ ULONG TargetProcessId, _In_ ULONG TargetThreadId)
{
    auto* newItem = Mem::Allocate<ListItem<ThreadCreateEvent>>();

    if (newItem == nullptr)
    {
        KdPrintMessage("Could not allocate thread start information");
        return;
    }

    ThreadCreateEvent& info = newItem->Item;
    info.Size = sizeof(ThreadCreateEvent);
    info.Type = EventType::ThreadCreate;
    KeQuerySystemTimePrecise(&info.Time);

    if (!NT_SUCCESS(GetThreadWin32StartAddress(TargetThreadId, &info.StartAddress)))
    {
        ExFreePool(newItem);
        return;
    }

    info.CreatingProcessId = HandleToUlong(PsGetCurrentProcessId());
    info.CreatingThreadId = HandleToUlong(PsGetCurrentThreadId());
    info.NewThreadId = TargetThreadId;
    info.TargetProcessId = TargetProcessId;

    InsertIntoList(&newItem->Entry, EventType::ThreadCreate);
}


void OnThreadExit(_In_ ULONG ProcessId, _In_ ULONG ThreadId)
{
    auto* newItem = Mem::Allocate<ListItem<ThreadExitEvent>>();

    if (newItem == nullptr)
    {
        KdPrintMessage("Could not allocate thread exit information");
        return;
    }

    ThreadExitEvent& info = newItem->Item;

    info.Size = sizeof(ThreadExitEvent);
    info.Type = EventType::ThreadExit;
    KeQuerySystemTimePrecise(&info.Time);
    info.ThreadId = ThreadId;
    info.ProcessId = ProcessId;

    InsertIntoList(&newItem->Entry, EventType::ThreadExit);
}

void OnThreadNotify(_In_ HANDLE ProcessId, _In_ HANDLE ThreadId, _In_ BOOLEAN Create)
{

    if (Create)
    {
        OnThreadStart(HandleToUlong(ProcessId), HandleToUlong(ThreadId));
    }
    else
    {
        OnThreadExit(HandleToUlong(ProcessId), HandleToUlong(ThreadId));
    }

}
