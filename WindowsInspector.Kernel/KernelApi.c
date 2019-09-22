#pragma once
#include <WindowsInspector.Kernel/KernelApi.h>
#include "Debug.h"

ZwQueryInformationThreadFunc ZwQueryInformationThread;

NTSTATUS 
KernelApiInitialize(
    VOID
    )
{
	UNICODE_STRING ZwQueryInformationThreadString = RTL_CONSTANT_STRING(L"ZwQueryInformationThread");

	ZwQueryInformationThread = (ZwQueryInformationThreadFunc)MmGetSystemRoutineAddress(&ZwQueryInformationThreadString);

	if (ZwQueryInformationThread == NULL)
	{
        return STATUS_NOT_FOUND;
	}

    return STATUS_SUCCESS;
}

NTSTATUS 
GetCurrentProcessHandle(
    __out PHANDLE ProcessHandle
    )
{
    if (ProcessHandle == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    PEPROCESS ProcessObject = PsGetCurrentProcess();
    NTSTATUS Status = ObOpenObjectByPointer(
        ProcessObject,
        OBJ_KERNEL_HANDLE,
        NULL,
        PROCESS_ALL_ACCESS,
        *PsProcessType,
        KernelMode,
        ProcessHandle
    );

    ObDereferenceObject(ProcessObject);
    return Status;
}

NTSTATUS
GetProcessHandleById(
    __in ULONG ProcessId,
    __out PHANDLE ProcessHandle
    )
{
    OBJECT_ATTRIBUTES attr;
    InitializeObjectAttributes(&attr, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);

    CLIENT_ID ClientId;
    ClientId.UniqueProcess = UlongToHandle(ProcessId);
    ClientId.UniqueThread = NULL;

    return ZwOpenProcess(ProcessHandle, PROCESS_ALL_ACCESS, &attr, &ClientId);
}