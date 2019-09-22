#pragma once
#include <ntifs.h>

NTSTATUS 
InitializeProcessProvider(
    VOID
    );

NTSTATUS
ReleaseProcessProvider(
    VOID
    );
