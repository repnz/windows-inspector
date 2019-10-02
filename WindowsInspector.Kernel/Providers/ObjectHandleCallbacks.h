#pragma once
#include <ntifs.h>

NTSTATUS
InitializeHandleCallbacksProvider(
	VOID
	);

VOID
ReleaseHandleCallbacksProvider(
	VOID
	);