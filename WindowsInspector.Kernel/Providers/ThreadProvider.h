#pragma once
#include <ntifs.h>

NTSTATUS 
InitializeThreadProvider(
	VOID
	);

VOID
ReleaseThreadProvider(
	VOID
	);
