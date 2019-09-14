#include "ProcessProvider.hpp"
#include <WindowsInspector.Kernel/Common.hpp>
#include <WindowsInspector.Kernel/Debug.hpp>
#include <WindowsInspector.Kernel/EventBuffer.hpp>
#include "ProcessProvider.hpp"

NTSTATUS InitializeProcessProvider();
void ReleaseProcessProvider();

void OnProcessNotify(_Inout_ PEPROCESS, _In_ HANDLE ProcessId, _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo);

NTSTATUS InitializeProcessProvider()
{
    return PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, FALSE);
}

void ReleaseProcessProvider()
{
    PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);
}

void OnProcessStart(_In_ HANDLE ProcessId, _Inout_ PPS_CREATE_NOTIFY_INFO CreateInfo)
{
    if (CreateInfo->CommandLine == NULL)
    {
        D_ERROR("Failed to log ProcessCreateInfo: CommandLine is NULL");
        return;
    }

    BufferEvent event;
    
    NTSTATUS status = AllocateBufferEvent(&event, sizeof(ProcessCreateEvent) + CreateInfo->CommandLine->Length);
    
    if (!NT_SUCCESS(status))
    {
        D_ERROR_STATUS("Failed to allocate memory for ProcessCreateInfo", status);
        return;
    }

    ProcessCreateEvent* info = (ProcessCreateEvent*)event.Memory;

    info->Type = EventType::ProcessCreate;
    info->Size = sizeof(ProcessCreateEvent) + CreateInfo->CommandLine->Length;
    KeQuerySystemTimePrecise(&info->Time);
    info->NewProcessId = HandleToUlong(ProcessId);
    info->ParentProcessId = HandleToUlong(CreateInfo->ParentProcessId);
    info->ProcessId = HandleToUlong(CreateInfo->CreatingThreadId.UniqueProcess);
    info->ThreadId = HandleToUlong(CreateInfo->CreatingThreadId.UniqueThread);
    info->CommandLine.Size = CreateInfo->CommandLine->Length;

    RtlCopyMemory(
        info->GetProcessCommandLine(),
        CreateInfo->CommandLine->Buffer,
        CreateInfo->CommandLine->Length
    );

    SendBufferEvent(&event);
}

void OnProcessExit(_In_ HANDLE ProcessId)
{

    BufferEvent event;
    NTSTATUS status = AllocateBufferEvent(&event, sizeof(ProcessExitEvent*));

    if (!NT_SUCCESS(status))
    {
        D_ERROR("Failed to allocate memory for ProcessExitInfo");
        return;
    }

    ProcessExitEvent* info = (ProcessExitEvent*)event.Memory;
    info->Type = EventType::ProcessExit;
    info->Size = sizeof(ProcessExitEvent);
    KeQuerySystemTimePrecise(&info->Time);
    info->ProcessId = HandleToUlong(ProcessId);

    SendBufferEvent(&event);
}

void OnProcessNotify(_Inout_ PEPROCESS ProcessObject, _In_ HANDLE ProcessId, _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo)
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