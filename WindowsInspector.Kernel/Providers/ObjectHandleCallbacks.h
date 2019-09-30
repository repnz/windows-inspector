#pragma once
#include <ntifs.h>

NTSTATUS
InitializeObjectHandleCallbacks(
	VOID
	);

NTSTATUS
ReleaseObjectHandleCallbacks(
	VOID
	);