#pragma once
#include <ntifs.h>

NTSTATUS InitializeProviders();

void FreeProviders();
