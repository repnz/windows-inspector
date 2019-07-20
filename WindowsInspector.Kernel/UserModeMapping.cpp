#include "UserModeMapping.hpp"
#include "Error.hpp"

NTSTATUS CheckBufferAccess(_In_ PVOID UserModeBuffer, _In_ ULONG Length, _In_ LOCK_OPERATION Operation) {

	__try {
		if (Operation == IoReadAccess) {
			ProbeForRead(UserModeBuffer, Length, sizeof(UCHAR));
		} 
		else {
			ProbeForWrite(UserModeBuffer, Length, sizeof(UCHAR));
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{

		NTSTATUS ntStatus = GetExceptionCode();
		KdPrint((DRIVER_PREFIX "Exception while accessing user mode buffer: 0X%08X\n", ntStatus));

		return ntStatus;
	}

	return STATUS_SUCCESS;
}


NTSTATUS
MapUserModeAddress(
	_In_ PVOID Buffer,
	_In_ ULONG Length,
	_In_ LOCK_OPERATION Operation,
	_Out_ PMDL* OutputMdl,
	_Out_ PCHAR* MappedBuffer
) {

	PMDL mdl = IoAllocateMdl(Buffer, Length, FALSE, TRUE, NULL);

	if (mdl == NULL)
	{
		KdPrint((DRIVER_PREFIX "Failed to allocate MDL for buffer 0x%p\n", Buffer));
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	__try
	{

		MmProbeAndLockPages(mdl, UserMode, Operation);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{

		NTSTATUS ntStatus = GetExceptionCode();
		KdPrint((DRIVER_PREFIX "Exception while locking buffer 0x%p (0x%08X)\n", Buffer, ntStatus));
		IoFreeMdl(mdl);
		return ntStatus;
	}

	PCHAR buffer = (PCHAR)MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority | MdlMappingNoExecute);

	if (!buffer) {
		IoFreeMdl(mdl);
		STATUS_INSUFFICIENT_RESOURCES;
	}

	*OutputMdl = mdl;
	*MappedBuffer = buffer;

	return STATUS_SUCCESS;
}

NTSTATUS UserModeMapping::Create(
	_In_ PVOID UserModeBuffer,
	_In_ ULONG Length, 
	_In_ LOCK_OPERATION IoOperation,
	_Inout_ UserModeMapping* OutputMapping
) {
	if (UserModeBuffer || Length == 0 || OutputMapping == NULL) {
		return STATUS_INVALID_PARAMETER;
	}

	NTSTATUS status = CheckBufferAccess(UserModeBuffer, Length, IoOperation);

	if (!NT_SUCCESS(status)) {
		return status;
	}

	PMDL mdl = IoAllocateMdl(UserModeBuffer, Length, FALSE, TRUE, NULL);

	if (OutputMapping->Mdl == NULL) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	__try
	{

		MmProbeAndLockPages(mdl, UserMode, IoOperation);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		NTSTATUS ntStatus = GetExceptionCode();
		KdPrint((DRIVER_PREFIX "Exception while locking buffer 0x%p: 0x%08X\n", UserModeBuffer, ntStatus));
		IoFreeMdl(mdl);
		return ntStatus;
	}

	PCHAR kernelBuffer = (PCHAR)MmGetSystemAddressForMdlSafe(mdl, NormalPagePriority | MdlMappingNoExecute);

	if (kernelBuffer == NULL) {
		IoFreeMdl(mdl);
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	OutputMapping->Mdl = mdl;
	OutputMapping->Buffer = kernelBuffer;
	OutputMapping->Length = Length;

	return STATUS_SUCCESS;
}


UserModeMapping::~UserModeMapping() {
	if (Mdl != NULL) {
		IoFreeMdl(Mdl);
		MmUnlockPages(Mdl);
	}
}