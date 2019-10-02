#pragma once
#include <ntifs.h>

typedef
BOOLEAN
(*PWRAPPER_CREATE_PROCESS_NOTIFY_ROUTINE)(
	__inout PEPROCESS Process,
	__in HANDLE ProcessId,
	__inout_opt PPS_CREATE_NOTIFY_INFO CreateInfo
	);

VOID
InitializeProcessNotify(
	VOID
);

VOID
AddProcessNotifyRoutine(
	__in PWRAPPER_CREATE_PROCESS_NOTIFY_ROUTINE Routine
);

VOID
RemoveProcessNotifyRoutine(
	__in PWRAPPER_CREATE_PROCESS_NOTIFY_ROUTINE RoutineToRemove
);

NTSTATUS
StartProcessNotify(
	VOID
	);

VOID
StopProcessNotify(
	VOID
);