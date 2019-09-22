#pragma once
#include <ntifs.h>

NTSTATUS 
InitializeThreadProvider();

NTSTATUS
ReleaseThreadProvider();
