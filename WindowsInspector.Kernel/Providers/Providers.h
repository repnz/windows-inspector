#pragma once
#include <ntifs.h>

NTSTATUS 
InitializeProviders(
    VOID
    );

NTSTATUS
FreeProviders(
    VOID
    );
