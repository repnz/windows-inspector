#include "ProcessNotifyWrapper.h"
#include "Debug.h"

#define ROUTINE_COUNT 5

static PWRAPPER_CREATE_PROCESS_NOTIFY_ROUTINE g_ProcessCallbackRoutines[ROUTINE_COUNT];
static ULONG g_CurrentIndex = 0;
static BOOLEAN Running = FALSE;

VOID
OnProcessNotifyWrapper(
	__inout PEPROCESS Process,
	__in HANDLE ProcessId,
	__inout_opt PPS_CREATE_NOTIFY_INFO CreateInfo
);

VOID 
InitializeProcessNotify(
	VOID
	)
{
	RtlSecureZeroMemory(&g_ProcessCallbackRoutines, sizeof(g_ProcessCallbackRoutines));
}

VOID 
AddProcessNotifyRoutine(
	__in PWRAPPER_CREATE_PROCESS_NOTIFY_ROUTINE Routine
	)
{
	if (g_CurrentIndex == ROUTINE_COUNT) 
	{
		D_ERROR_ARGS("Could not add routine. ProcessNotifyWrapper max count is %d", ROUTINE_COUNT);
		return;
	}

	g_ProcessCallbackRoutines[g_CurrentIndex] = Routine;
	g_CurrentIndex++;
}

NTSTATUS 
StartProcessNotify(
	VOID
	)
{
	if (Running)
	{
		return STATUS_SUCCESS;
	}

	return PsSetCreateProcessNotifyRoutineEx(OnProcessNotifyWrapper, FALSE);
}

VOID
StopProcessNotify(
	VOID
	)
{
	if (Running)
	{
		PsSetCreateProcessNotifyRoutineEx(OnProcessNotifyWrapper, TRUE);
		Running = FALSE;
	}
}

VOID
OnProcessNotifyWrapper(
	__inout PEPROCESS Process,
	__in HANDLE ProcessId,
	__inout_opt PPS_CREATE_NOTIFY_INFO CreateInfo
)
{
	for (ULONG i = 0; i < ROUTINE_COUNT; i++) 
	{
		PWRAPPER_CREATE_PROCESS_NOTIFY_ROUTINE Routine = g_ProcessCallbackRoutines[i];

		if (Routine && Routine(Process, ProcessId, CreateInfo))
		{
			StopProcessNotify();
			break;
		}
	}
}

VOID
RemoveProcessNotifyRoutine(
	__in PWRAPPER_CREATE_PROCESS_NOTIFY_ROUTINE RoutineToRemove
	)
{
	for (ULONG i = 0; i < ROUTINE_COUNT; i++)
	{
		PWRAPPER_CREATE_PROCESS_NOTIFY_ROUTINE Routine = g_ProcessCallbackRoutines[i];

		if (Routine == RoutineToRemove)
		{
			g_ProcessCallbackRoutines[i] = NULL;
			break;
		}
	}
}