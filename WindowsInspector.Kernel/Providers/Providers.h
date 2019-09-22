#pragma once
#include <ntifs.h>

NTSTATUS 
InitializeProviders(
    VOID
    );

VOID 
FreeProviders(
    VOID
    );
