#pragma once

#ifdef KERNEL_DRIVER
#include <ntifs.h>
#else
#include <windows.h>
#include <winioctl.h>
#endif


#define INSPECTOR_LISTEN_CTL_CODE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1, METHOD_BUFFERED, FILE_ANY_ACCESS)


enum class EventType : short
{
    None,
    ProcessCreate,
    ProcessExit,
	ImageLoad,
	ThreadCreate,
	ThreadExit,
    RegistryEvent
};

const char* const EventTypeStr[] = 
{
    "None",
    "ProcessCreate",
    "ProcessExit",
    "ImageLoad",
    "ThreadCreate",
    "ThreadExit"
};

#define EVENT_TYPE_STR(Type) EventTypeStr[(int)Type]

struct EventHeader
{
    EventType Type;
    USHORT Size;
    LARGE_INTEGER Time;
};

struct ProcessExitEvent : EventHeader 
{
    ULONG ProcessId;
};

struct ProcessCreateEvent : EventHeader
{
    ULONG NewProcessId;
    ULONG CreatingProcessId;
	ULONG ParentProcessId;
	ULONG CreatingThreadId;
    USHORT CommandLineLength;
	WCHAR CommandLine[1];
};

struct ThreadCreateEvent : EventHeader 
{
	ULONG CreatingProcessId;
	ULONG CreatingThreadId;
    ULONG NewThreadId;
    ULONG TargetProcessId;
	ULONG_PTR StartAddress;
};

struct ThreadExitEvent : EventHeader
{
	ULONG ThreadId;
	ULONG ProcessId;
};

struct ImageLoadEvent : EventHeader 
{
    ULONG ProcessId;
	ULONG ThreadId;
    ULONG_PTR LoadAddress;
    ULONG_PTR ImageSize;
	ULONG ImageFileNameLength;
    WCHAR ImageFileName[1];
};

enum class RegistryEventType
{
    Write,

};

struct RegistryEvent : EventHeader
{
    ULONG Processid;
    ULONG ThreadId;
    RegistryEventType SubType;
    ULONG KeyNameLength;
    WCHAR KeyNameOffset;
};

struct CircularBuffer 
{
    PVOID BaseAddress;

    // This member might not be synchronized with TailOffset / HeadOffset
    // Use it with caution
    LONG Count;
    ULONG HeadOffset;
    ULONG TailOffset;
    ULONG ResetOffset;
    HANDLE Event;
};