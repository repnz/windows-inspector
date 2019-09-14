#pragma once
#include <ntifs.h>

NTSTATUS InitializeProcessProvider();
void ReleaseProcessProvider();
