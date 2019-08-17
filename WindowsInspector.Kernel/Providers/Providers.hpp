#pragma once
#include <WindowsInspector.Kernel/Providers/ThreadProvider.hpp>
#include <WindowsInspector.Kernel/Providers/ProcessProvider.hpp>
#include <WindowsInspector.Kernel/Providers/ImageLoadProvider.hpp>


NTSTATUS InitializeProviders();

void FreeProviders();
