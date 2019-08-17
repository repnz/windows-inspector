#pragma once
#include <ntifs.h>

NTSTATUS InitializeThreadProvider();

void ReleaseThreadProvider();
