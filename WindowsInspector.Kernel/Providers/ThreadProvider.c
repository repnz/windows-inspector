#include <WindowsInspector.Kernel/Debug.h>
#include <WindowsInspector.Kernel/KernelApi.h>
#include <WindowsInspector.Shared/Common.h>
#include <WindowsInspector.Kernel/EventBuffer.h>
#include "ThreadProvider.h"
#include "Providers.h"

VOID
OnThreadNotify(
    __in HANDLE ProcessId, 
    __in HANDLE ThreadId,
    __in BOOLEAN Create
);

NTSTATUS 
InitializeThreadProvider(
    VOID
    )
{
    return PsSetCreateThreadNotifyRoutine(OnThreadNotify);
}

VOID
ReleaseThreadProvider(
    VOID
    )
{
    PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);
}

NTSTATUS 
GetThreadWin32StartAddress(
    __in ULONG ThreadId, 
    __out PULONG_PTR Win32StartAddress
    )
{
    NTSTATUS Status;
    HANDLE CreatingThreadObjectHandle = NULL;
    PETHREAD Thread = NULL;


    Status = PsLookupThreadByThreadId(UlongToHandle(ThreadId), &Thread);

    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS_ARGS("Failed to find thread with id %d", Status, ThreadId);
        goto cleanup;
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

    ObDereferenceObject(Thread);

    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS("Failed to create a handle to the new thread", Status);
        goto cleanup;
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
		goto cleanup;
    }

cleanup:
	
	if (CreatingThreadObjectHandle)
	{
		ObCloseHandle(CreatingThreadObjectHandle, KernelMode);
	}

    return Status;
}

VOID
OnThreadCreate(
    __in ULONG TargetProcessId, 
    __in ULONG TargetThreadId
    )
{
    
    ULONG_PTR StartAddress;
    PTHREAD_CREATE_EVENT Event;
    NTSTATUS Status;

    Status = GetThreadWin32StartAddress(TargetThreadId, &StartAddress);

    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS_ARGS("Could not find thread start address %d", Status, TargetThreadId);
        return;
    }

    Status = AllocateBufferEvent(&Event, sizeof(THREAD_CREATE_EVENT));

    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS("Could not allocate ThreadCreateEvent", Status);
        return;
    }

    
    Event->Header.Type = EvtTypeThreadCreate;
    KeQuerySystemTimePrecise(&Event->Header.Time);

    Event->Header.ProcessId = HandleToUlong(PsGetCurrentProcessId());
    Event->Header.ThreadId = HandleToUlong(PsGetCurrentThreadId());
    Event->NewThreadId = TargetThreadId;
    Event->TargetProcessId = TargetProcessId;
	Event->StartAddress = StartAddress;
	
	SendOrCancelBufferEvent(Event);
}


VOID
OnThreadExit(
    __in ULONG ProcessId, 
    __in ULONG ThreadId
    )
{
    PTHREAD_EXIT_EVENT Event;
    NTSTATUS Status;

	Status = AllocateBufferEvent(&Event, sizeof(THREAD_EXIT_EVENT));

    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS("Could not allocate ThreadExitEvent", Status);
        return;
    }

    Event->Header.Type = EvtTypeThreadExit;
    KeQuerySystemTimePrecise(&Event->Header.Time);
    Event->Header.ThreadId = ThreadId;
    Event->Header.ProcessId = ProcessId;

	SendOrCancelBufferEvent(Event);
}

VOID 
OnThreadNotify(
    __in HANDLE ProcessId, 
    __in HANDLE ThreadId, 
    __in BOOLEAN Create
    )
{
	if (!g_Listening)
	{
		return;
	}

    if (Create)
    {
        OnThreadCreate(HandleToUlong(ProcessId), HandleToUlong(ThreadId));
    }
    else
    {
        OnThreadExit(HandleToUlong(ProcessId), HandleToUlong(ThreadId));
    }

}
