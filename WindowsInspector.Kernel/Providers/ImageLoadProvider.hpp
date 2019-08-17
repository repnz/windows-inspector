#pragma once
#include <ntifs.h>


NTSTATUS InitializeImageLoadProvider();

void ReleaseImageLoadProvider();
