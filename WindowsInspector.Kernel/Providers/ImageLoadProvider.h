#pragma once
#include <ntifs.h>


NTSTATUS 
InitializeImageLoadProvider(
    VOID
    );

VOID 
ReleaseImageLoadProvider(
    VOID
    );
