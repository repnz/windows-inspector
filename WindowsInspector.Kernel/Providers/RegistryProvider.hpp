#pragma once
#include <ntifs.h>

NTSTATUS InitializeRegistryProvider();

void ReleaseRegistryProvider();
