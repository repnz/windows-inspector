#pragma once

#ifdef KERNEL_DRIVER
#include <ntddk.h>
#else
#include <windows.h>
#include <winioctl.h>
#define IM_ORI_DAMARI
#endif

#define INSPECTOR_GET_EVENTS_CTL_CODE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1, METHOD_NEITHER, FILE_ANY_ACCESS)


enum class EventType : short {
    None,
    ProcessCreate,
    ProcessExit,
	ImageLoad,
	ThreadCreate,
	ThreadExit
};

struct EventHeader {
    EventType Type;
    USHORT Size;
    LARGE_INTEGER Time;
};

struct ProcessExitEvent : EventHeader {
    ULONG ProcessId;
};

struct ProcessCreateEvent : EventHeader {
    ULONG NewProcessId;
    ULONG CreatingProcessId;
	ULONG ParentProcessId;
	ULONG CreatingThreadId;
    USHORT CommandLineLength;
	WCHAR CommandLine[1];
};

struct ThreadCreateEvent : EventHeader {
	ULONG CreatingProcessId;
	ULONG CreatingThreadId;
    ULONG NewThreadId;
    ULONG TargetProcessId;
	ULONG_PTR StartAddress;
};

struct ThreadExitEvent : EventHeader {
	ULONG ThreadId;
	ULONG ProcessId;
};

struct ImageLoadEvent : EventHeader {
    ULONG ProcessId;
	ULONG ThreadId;
    ULONG_PTR LoadAddress;
    ULONG_PTR ImageSize;
	ULONG ImageFileNameLength;
    WCHAR ImageFileName[1];
};