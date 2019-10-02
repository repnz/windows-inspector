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

NTSTATUS
MapUserModeAddressToSystemSpace(
	__in PVOID Buffer,
	__in ULONG Length,
	__in LOCK_OPERATION Operation,
	__out PMDL* OutputMdl,
	__out PVOID* MappedBuffer
)
{

	NTSTATUS Status = STATUS_SUCCESS;
	PMDL Mdl = NULL;
	PVOID SystemSpaceMemory = NULL;

	BOOLEAN MdlAllocated = FALSE;
	BOOLEAN MdlLocked = FALSE;

	if (OutputMdl == NULL || MappedBuffer == NULL)
	{
		Status = STATUS_INVALID_PARAMETER;
		goto cleanup;
	}

	Mdl = IoAllocateMdl(Buffer, Length, FALSE, FALSE, NULL);

	if (Mdl == NULL)
	{
		D_ERROR_ARGS("Failed to allocate MDL for buffer 0x%p", Buffer);
		Status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanup;
		
	}

	MdlAllocated = TRUE;

	__try
	{
		MmProbeAndLockPages(Mdl, UserMode, Operation);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		Status = GetExceptionCode();
		D_ERROR_STATUS_ARGS("Exception while locking buffer 0x%p", Status, Buffer);
		goto cleanup;
	}

	MdlLocked = TRUE;

	SystemSpaceMemory = MmGetSystemAddressForMdlSafe(Mdl, NormalPagePriority | MdlMappingNoExecute);

	if (!SystemSpaceMemory)
	{
		D_ERROR_ARGS("Could not call MmGetSystemAddressForMdlSafe() with buffer: 0x%p", Buffer);
		Status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanup;
	}
	
cleanup:
	if (!NT_SUCCESS(Status))
	{
		if (MdlLocked)
		{
			MmUnlockPages(Mdl);
		}

		if (MdlAllocated)
		{
			IoFreeMdl(Mdl);
		}
	}
	else
	{
		*OutputMdl = Mdl;
		*MappedBuffer = SystemSpaceMemory;
	}

	return Status;
}