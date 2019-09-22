#pragma once
#include <ntifs.h>

NTSTATUS 
InitializeRegistryProvider(
    VOID
    );

NTSTATUS
ReleaseRegistryProvider(
    VOID
    );
