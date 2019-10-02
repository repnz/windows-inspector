#include <WindowsInspector.Shared/Common.h>
#include <WindowsInspector.Kernel/Debug.h>
#include <WindowsInspector.Kernel/EventBuffer.h>
#include "ProcessProvider.h"
#include <WindowsInspector.Kernel/ProcessNotifyWrapper.h>
#include "Providers.h"

BOOLEAN
ProcessProviderOnProcessNotify(
    __inout PEPROCESS, 
    __in HANDLE ProcessId, 
    __inout_opt PPS_CREATE_NOTIFY_INFO CreateInfo
    );

NTSTATUS 
InitializeProcessProvider(
    VOID
    )
{
    AddProcessNotifyRoutine(&ProcessProviderOnProcessNotify);
	return STATUS_SUCCESS;
}

VOID 
ReleaseProcessProvider(
    VOID
    )
{
	RemoveProcessNotifyRoutine(&ProcessProviderOnProcessNotify);
}

VOID 
ProcessProviderOnProcessStart(
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
    Event->Header.Type = EvtTypeProcessCreate;
    Event->Header.ProcessId = HandleToUlong(CreateInfo->CreatingThreadId.UniqueProcess);
    Event->Header.ThreadId = HandleToUlong(CreateInfo->CreatingThreadId.UniqueThread);
    KeQuerySystemTimePrecise(&Event->Header.Time);

    // ProcessProvider 
    Event->NewProcessId = HandleToUlong(ProcessId);
    Event->ParentProcessId = HandleToUlong(CreateInfo->ParentProcessId);

    Event->CommandLine.Size = CreateInfo->CommandLine->Length;
	Event->CommandLine.Offset = sizeof(PROCESS_CREATE_EVENT);

    RtlCopyMemory(
        EVENT_GET_APPENDIX(Event, Event->CommandLine, PVOID),
        CreateInfo->CommandLine->Buffer,
        CreateInfo->CommandLine->Length
    );

	SendOrCancelBufferEvent(Event);
}

VOID
ProcessProviderOnProcessExit(
    __in HANDLE ProcessId
    )
{
    PPROCESS_EXIT_EVENT Event;
    NTSTATUS Status;
    
    Status = AllocateBufferEvent(&Event, sizeof(PROCESS_EXIT_EVENT));

    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS_ARGS("Failed to allocate memory for ProcessExitInfo: ProcessId=%d", Status, HandleToUlong(ProcessId));
        return;
    }

    // Init Common
    Event->Header.Type = EvtTypeProcessExit;
    Event->Header.ProcessId = HandleToUlong(ProcessId);
    Event->Header.ThreadId = HandleToUlong(PsGetCurrentThreadId());
    KeQuerySystemTimePrecise(&Event->Header.Time);
    
    SendOrCancelBufferEvent(Event);
}

BOOLEAN
ProcessProviderOnProcessNotify(
    __inout PEPROCESS ProcessObject, 
    __in HANDLE ProcessId, 
    __inout_opt PPS_CREATE_NOTIFY_INFO CreateInfo
    )
{
    UNREFERENCED_PARAMETER(ProcessObject);

	if (!g_Listening)
	{
		return TRUE;
	}

    if (CreateInfo)
    {
		ProcessProviderOnProcessStart(ProcessId, CreateInfo);
    }
    else
    {
        ProcessProviderOnProcessExit(ProcessId);
    }

	return TRUE;
}