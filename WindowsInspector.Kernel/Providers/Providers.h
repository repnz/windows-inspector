#pragma once
#include <ntifs.h>

extern BOOLEAN g_Listening;

NTSTATUS 
InitializeProviders(
    VOID
    );

VOID
FreeProviders(
    VOID
    );

VOID StartProviderEvents(
	VOID
	);

VOID StopProvidersEvents(
	VOID
	);