#include "ProcessProvider.hpp"
#include <WindowsInspector.Kernel/Common.hpp>
#include <WindowsInspector.Kernel/DataItemList.hpp>
#include <WindowsInspector.Kernel/EventList.hpp>
#include <WindowsInspector.Kernel/Mem.hpp>
#include <WindowsInspector.Kernel/Error.hpp>
#include "ProcessProvider.hpp"

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
        KdPrintMessage("Failed to log ProcessCreateInfo: CommandLine is NULL");
        return;
    }

    auto* newItem = Mem::Allocate<ListItem<ProcessCreateEvent>>(CreateInfo->CommandLine->Length);

    if (newItem == NULL)
    {
        KdPrintMessage("Failed to allocate memory for ProcessCreateInfo");
        return;
    }

    ProcessCreateEvent& info = newItem->Item;
    info.Type = EventType::ProcessCreate;
    info.Size = sizeof(ProcessCreateEvent) + CreateInfo->CommandLine->Length;
    KeQuerySystemTimePrecise(&info.Time);
    info.NewProcessId = HandleToUlong(ProcessId);
    info.ParentProcessId = HandleToUlong(CreateInfo->ParentProcessId);
    info.CreatingProcessId = HandleToUlong(CreateInfo->CreatingThreadId.UniqueProcess);
    info.CreatingThreadId = HandleToUlong(CreateInfo->CreatingThreadId.UniqueThread);
    info.CommandLineLength = CreateInfo->CommandLine->Length / 2;

    RtlCopyMemory(
        info.CommandLine,
        CreateInfo->CommandLine->Buffer,
        CreateInfo->CommandLine->Length
    );

    InsertIntoList(&newItem->Entry, info.Type);
}

void OnProcessExit(_In_ HANDLE ProcessId)
{
    auto* newItem = Mem::Allocate<ListItem<ProcessExitEvent>>();

    if (newItem == NULL)
    {
        KdPrintMessage("Failed to allocate memory for ProcessExitInfo");
        return;
    }

    ProcessExitEvent& info = newItem->Item;
    info.Type = EventType::ProcessExit;
    info.Size = sizeof(ProcessExitEvent);
    KeQuerySystemTimePrecise(&info.Time);
    info.ProcessId = HandleToUlong(ProcessId);

    InsertIntoList(&newItem->Entry, EventType::ProcessExit);
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