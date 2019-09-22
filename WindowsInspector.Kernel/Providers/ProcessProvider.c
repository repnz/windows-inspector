#include <WindowsInspector.Kernel/Common.h>
#include <WindowsInspector.Kernel/Debug.h>
#include <WindowsInspector.Kernel/EventBuffer.h>
#include "ProcessProvider.hpp"

VOID
OnProcessNotify(
    __inout PEPROCESS, 
    __in HANDLE ProcessId, 
    __inout_opt PPS_CREATE_NOTIFY_INFO CreateInfo
    );

NTSTATUS 
InitializeProcessProvider(
    VOID
    )
{
    return PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, FALSE);
}

NTSTATUS 
ReleaseProcessProvider(
    VOID
    )
{
    PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);
    return STATUS_SUCCESS;
}

VOID 
OnProcessStart(
    __in HANDLE ProcessId,
    __inout PPS_CREATE_NOTIFY_INFO CreateInfo
    )
{
    PPROCESS_CREATE_EVENT Event;
    NTSTATUS Status;

    if (CreateInfo->CommandLine == NULL)
    {
        D_ERROR("Failed to log ProcessCreateInfo: CommandLine is NULL");
        return;
    }
    
    Status = AllocateBufferEvent(&Event, sizeof(PROCESS_CREATE_EVENT) + CreateInfo->CommandLine->Length);
    
    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS_ARGS("Failed to allocate memory for ProcessCreateInfo: ProcessId=%d", 
            Status, HandleToUlong(ProcessId));
        return;
    }

    // Init Common
    Event->Header.Type = EvtProcessCreate;
    Event->Header.ProcessId = HandleToUlong(CreateInfo->CreatingThreadId.UniqueProcess);
    Event->Header.ThreadId = HandleToUlong(CreateInfo->CreatingThreadId.UniqueThread);
    KeQuerySystemTimePrecise(&Event->Header.Time);

    // ProcessProvider 
    Event->NewProcessId = HandleToUlong(ProcessId);
    Event->ParentProcessId = HandleToUlong(CreateInfo->ParentProcessId);
    Event->CommandLine.Size = CreateInfo->CommandLine->Length;

    RtlCopyMemory(
        ProcessCreate_GetCommandLine(Event),
        CreateInfo->CommandLine->Buffer,
        CreateInfo->CommandLine->Length
    );

    SendBufferEvent(Event);
}

VOID
OnProcessExit(
    __in HANDLE ProcessId
    )
{
    PPROCESS_EXIT_EVENT Event;
    NTSTATUS Status;
    
    Status = AllocateBufferEvent(&Event, sizeof(PROCESS_EXIT_EVENT));

    if (!NT_SUCCESS(Status))
    {
        D_ERROR("Failed to allocate memory for ProcessExitInfo: ProcessId=%d");
        return;
    }

    // Init Common
    Event->Header.Type = EvtProcessExit;
    Event->Header.ProcessId = HandleToUlong(ProcessId);
    Event->Header.ThreadId = HandleToUlong(PsGetCurrentThreadId());
    KeQuerySystemTimePrecise(&Event->Header.Time);
    
    SendBufferEvent(Event);
}

VOID
OnProcessNotify(
    __inout PEPROCESS ProcessObject, 
    __in HANDLE ProcessId, 
    __inout_opt PPS_CREATE_NOTIFY_INFO CreateInfo
    )
{
    UNREFERENCED_PARAMETER(ProcessObject);

    if (CreateInfo)
    {
        OnProcessStart(ProcessId, CreateInfo);
    }
    else
    {
        OnProcessExit(ProcessId);
    }
}