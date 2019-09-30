#include "ObjectHandleCallbacks.h"
#include <WindowsInspector.Kernel/EventBuffer.h>
#include <WindowsInspector.Kernel/Debug.h>
#include <WindowsInspector.Kernel/KernelApi.h>

static HANDLE RegistrationHandle = NULL;

static
NTSTATUS
InitializeHandleEvent(
	__inout POBJECT_HANDLE_EVENT* OutputEvent,
	__in ULONG KernelHandle,
	__in PVOID ObjectType,
	__in PVOID Object,
	__in ULONG Operation,
	__in OBJECT_HANDLE_EVENT_TYPE EventType
	)
{
	if (OutputEvent == NULL)
	{
		return STATUS_INVALID_PARAMETER;
	}

	NTSTATUS Status;
	POBJECT_HANDLE_EVENT Event;

	Status = AllocateBufferEvent(&Event, sizeof(OBJECT_HANDLE_EVENT));

	if (!NT_SUCCESS(Status))
	{
		D_ERROR_STATUS("Could not allocate Object Handle Event.", Status);
		return Status;
	}

	// Initialize Common
	Event->Header.ProcessId = HandleToUlong(PsGetCurrentProcessId());
	Event->Header.ThreadId = HandleToUlong(PsGetCurrentThreadId());
	KeQuerySystemTimePrecise(&Event->Header.Time);
	Event->Header.Type = EvtObjectHandleEvent;

	// Initialize Handle Information
	Event->IsKernelHandle = (KernelHandle != 0) ? TRUE : FALSE;
	Event->EventType = EventType;
	Event->OperationType = (Operation == OB_OPERATION_HANDLE_CREATE) ? EvtObCreateHandle : EvtObDuplicateHandle;

	if (ObjectType == *PsProcessType)
	{
		Event->ObjectType = EvtObProcess;
		Event->ObjectId = GetProcessId(Object);
	}
	else
	{
		Event->ObjectType = EvtObThread;
		Event->ObjectId = GetThreadId(Object);
	}


	return STATUS_SUCCESS;
}

static
OB_PREOP_CALLBACK_STATUS 
PreOperationCallback(
	__in PVOID RegistrationContext,
	__inout POB_PRE_OPERATION_INFORMATION OperationInformation
	)
{
	UNREFERENCED_PARAMETER(RegistrationContext);
	
	POBJECT_HANDLE_EVENT Event;
	NTSTATUS Status;

	Status = InitializeHandleEvent(
		&Event,
		OperationInformation->KernelHandle,
		OperationInformation->ObjectType,
		OperationInformation->Object,
		OperationInformation->Operation,
		EvtPre
	);

	if (!NT_SUCCESS(Status))
	{
		return OB_PREOP_SUCCESS;
	}

	if (OperationInformation->Operation == OB_OPERATION_HANDLE_CREATE)
	{
		Event->Access = OperationInformation->Parameters->CreateHandleInformation.OriginalDesiredAccess;
	}
	else
	{
		Event->Access = OperationInformation->Parameters->DuplicateHandleInformation.OriginalDesiredAccess;
		
		CONST PVOID SourceProcess = OperationInformation->Parameters->DuplicateHandleInformation.SourceProcess;
		Event->DuplicateParameters.SourceProcessId = GetProcessId(SourceProcess);

		CONST PVOID TargetProcess = OperationInformation->Parameters->DuplicateHandleInformation.TargetProcess;
		Event->DuplicateParameters.TargetProcessId = GetProcessId(TargetProcess);
	}

	SendOrCancelBufferEvent(Event);

	return OB_PREOP_SUCCESS;
}


static
VOID
PostOperationCallback(
	__in PVOID RegistrationContext,
	__in POB_POST_OPERATION_INFORMATION OperationInformation
	)
{
	UNREFERENCED_PARAMETER(RegistrationContext);

	POBJECT_HANDLE_EVENT Event;
	NTSTATUS Status;

	Status = InitializeHandleEvent(
		&Event, 
		OperationInformation->KernelHandle,
		OperationInformation->ObjectType,
		OperationInformation->Object,
		OperationInformation->Operation,
		EvtPost
		);

	if (!NT_SUCCESS(Status))
	{
		return;
	}

	if (OperationInformation->Operation == OB_OPERATION_HANDLE_CREATE)
	{
		Event->Access = OperationInformation->Parameters->CreateHandleInformation.GrantedAccess;
	}
	else 
	{
		Event->Access = OperationInformation->Parameters->DuplicateHandleInformation.GrantedAccess;
	}

	Event->PostEventParameters.ReturnStatus = OperationInformation->ReturnStatus;

	SendOrCancelBufferEvent(Event);
}


NTSTATUS
InitializeObjectHandleCallbacks(
	VOID
	)
{

	OB_OPERATION_REGISTRATION RegistrationArr[2];

	RegistrationArr[0].ObjectType = PsProcessType;
	RegistrationArr[0].Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
	RegistrationArr[0].PreOperation = PreOperationCallback;
	RegistrationArr[0].PostOperation = PostOperationCallback;

	RegistrationArr[1].ObjectType = PsThreadType;
	RegistrationArr[1].Operations = OB_OPERATION_HANDLE_CREATE | OB_OPERATION_HANDLE_DUPLICATE;
	RegistrationArr[1].PreOperation = PreOperationCallback;
	RegistrationArr[1].PostOperation = PostOperationCallback;

	UNICODE_STRING Altitude = RTL_CONSTANT_STRING(L"1000");

	OB_CALLBACK_REGISTRATION Registration;
	Registration.Version = OB_FLT_REGISTRATION_VERSION;
	Registration.OperationRegistrationCount = sizeof(RegistrationArr) / sizeof(OB_OPERATION_REGISTRATION);
	Registration.RegistrationContext = NULL;
	Registration.Altitude = Altitude;
	Registration.OperationRegistration = RegistrationArr;
	
	return ObRegisterCallbacks(&Registration, &RegistrationHandle);
}


NTSTATUS
ReleaseObjectHandleCallbacks(
	VOID
	)
{
	if (RegistrationHandle == NULL)
	{
		return STATUS_UNSUCCESSFUL;
	}

	ObUnRegisterCallbacks(RegistrationHandle);
	RegistrationHandle = NULL;
	return STATUS_SUCCESS;
}
