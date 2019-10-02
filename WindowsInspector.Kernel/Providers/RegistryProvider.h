#pragma once
#include <ntifs.h>

NTSTATUS 
InitializeRegistryProvider(
    VOID
    );

VOID
ReleaseRegistryProvider(
    VOID
    );
