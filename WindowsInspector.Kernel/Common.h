#pragma once
#include <ntddk.h>

enum class ItemType : short {
    None,
    ProcessCreate,
    ProcessExit,
	ImageLoad,
	ThreadStart,
	ThreadExit
};

struct ItemHeader {
    ItemType Type;
    USHORT Size;
    LARGE_INTEGER Time;
};

struct ProcessExitInfo : ItemHeader {
    ULONG ProcessId;
};

struct ProcessCreateInfo : ItemHeader {
    ULONG NewProcessId;
    ULONG CreatingProcessId;
	ULONG ParentProcessId;
    USHORT CommandLineLength;
	USHORT CommandLine[1];
};

struct ThreadCreateInfo : ItemHeader {
	ULONG CreatingProcessId;
	ULONG CreatingThreadId;
    ULONG NewThreadId;
    ULONG TargetProcessId;
	ULONG StartAddress;
};

struct ThreadExitInfo : ItemHeader {
	ULONG ThreadId;
	ULONG ProcessId;
};

struct ImageLoadInfo : ItemHeader {
    ULONG ProcessId;
	ULONG ThreadId;
    ULONG LoadAddress;
    ULONG_PTR ImageSize;
	ULONG ImageFileNameLength;
    WCHAR ImageFileName[1];
};