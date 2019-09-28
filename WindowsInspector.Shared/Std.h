#pragma once
#ifdef KERNEL_DRIVER
#include <ntifs.h>
#else
#include <Windows.h>
#include <ntstatus.h>
#include <WindowsInspector.Controller/ntos.h>
#include <winioctl.h>
#endif
