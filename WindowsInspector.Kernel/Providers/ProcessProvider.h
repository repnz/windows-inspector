#pragma once
#include <ntifs.h>

NTSTATUS 
InitializeProcessProvider(
    VOID
    );

VOID
ReleaseProcessProvider(
    VOID
    );
