#pragma once
#include <ntifs.h>


NTSTATUS 
InitializeImageLoadProvider(
    VOID
    );

NTSTATUS 
ReleaseImageLoadProvider(
    VOID
    );
