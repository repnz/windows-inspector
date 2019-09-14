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
    "ThreadExit",
    "RegistryEvent"
};

#define EVENT_TYPE_STR(Type) EventTypeStr[(int)Type]


struct EventHeader
{
    EventType Type;
    USHORT Size;
    LARGE_INTEGER Time;
};

struct AppendedEventBuffer
{
    ULONG Offset;
    ULONG Size;
};

#define EVENT_GET_APPENDIX(Event, Appendix, Type) (Type)(((PUCHAR)Event) + Appendix.Offset)


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
    AppendedEventBuffer CommandLine;

    PWSTR GetProcessCommandLine()
    {
        return EVENT_GET_APPENDIX(this, CommandLine, PWSTR);
    }

    PCWSTR GetProcessCommandLine() const
    {
        return EVENT_GET_APPENDIX(this, CommandLine, PCWSTR);
    }
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
    AppendedEventBuffer ImageFileName;
    

    PWSTR GetImageFileName()
    {
        return EVENT_GET_APPENDIX(this, ImageFileName, PWSTR);
    }

    PCWSTR GetImageFileName() const
    {
        return EVENT_GET_APPENDIX(this, ImageFileName, PCWSTR);
    }
};


enum class RegistryEventType
{
    DeleteKey,
    SetValue,
    RenameKey,
    QueryValue,
    DeleteValue

};

const char* const RegistryEventTypeStr[] =
{
    "DeleteKey",
    "SetValue",
    "RenameKey"
};

#define REG_EVENT_SUB_TYPE_STR(SubType) RegistryEventTypeStr[(int)SubType]


struct RegistryEvent : EventHeader
{
    ULONG Processid;
    ULONG ThreadId;
    RegistryEventType SubType;
    AppendedEventBuffer KeyName;
    AppendedEventBuffer ValueName;
    AppendedEventBuffer ValueData;
    AppendedEventBuffer NewName;

    PWSTR GetKeyName()
    {
        return EVENT_GET_APPENDIX(this, KeyName, PWSTR);
    }

    PCWSTR GetKeyName() const
    {
        return EVENT_GET_APPENDIX(this, KeyName, PCWSTR);
    }

    PWSTR GetValueName()
    {
        return EVENT_GET_APPENDIX(this, ValueName, PWSTR);
    }

    PCWSTR GetValueName() const
    {
        return EVENT_GET_APPENDIX(this, ValueName, PCWSTR);
    }

    PWSTR GetValueData()
    {
        return EVENT_GET_APPENDIX(this, ValueData, PWSTR);
    }

    PCWSTR GetValueData() const
    {
        return EVENT_GET_APPENDIX(this, ValueData, PCWSTR);
    }

    PWSTR GetNewName()
    {
        return EVENT_GET_APPENDIX(this, NewName, PWSTR);
    }

    PCWSTR GetNewName() const
    {
        return EVENT_GET_APPENDIX(this, NewName, PCWSTR);
    }
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


#undef EVENT_GET_APPENDIX