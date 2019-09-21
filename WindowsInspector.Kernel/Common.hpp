#pragma once

#ifdef KERNEL_DRIVER
#include <ntifs.h>
#else
#include <windows.h>
#include <winioctl.h>
#endif


#define INSPECTOR_LISTEN_CTL_CODE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define INSPECTOR_STOP_CTL_CODE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x2, METHOD_NEITHER, FILE_ANY_ACCESS)

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


struct EventHeader
{
    EventType Type;
    USHORT Size;
    LARGE_INTEGER Time;
    ULONG ProcessId;
    ULONG ThreadId;

    PCSTR GetEventName() const
    {
        return EventTypeStr[(int)Type];
    }
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
	ULONG ParentProcessId;
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
    ULONG NewThreadId;
    ULONG TargetProcessId;
	ULONG_PTR StartAddress;
};

struct ThreadExitEvent : EventHeader
{
};

struct ImageLoadEvent : EventHeader 
{
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
    "RenameKey",
    "QueryValue",
    "DeleteValue"
};

#define REG_EVENT_SUB_TYPE_STR(SubType) RegistryEventTypeStr[(int)SubType]

const char* const RegistryValueDataTypeStr[] =
{
    "REG_NONE",
    "REG_SZ",
    "REG_EXPAND_SZ",
    "REG_BINARY",
    "REG_DWORD",
    "REG_DWORD_BIG_ENDIAN",
    "REG_LINK",
    "REG_MULTI_SZ",
    "REG_RESOURCE_LIST",
    "REG_FULL_RESOURCE_DESCRIPTOR",
    "REG_RESOURCE_REQUIREMENTS_LIST",
    "REG_QWORD"
};

#define REG_VALUE_TYPE_STR(ValueType) RegistryValueDataTypeStr[(int)ValueType];

struct RegistryEvent : EventHeader
{
    RegistryEventType SubType;
    AppendedEventBuffer KeyName;
    AppendedEventBuffer ValueName;
    AppendedEventBuffer ValueData;
    ULONG ValueType;
    AppendedEventBuffer NewName;
    
    FORCEINLINE PCSTR GetValueTypeName() const
    {
        return REG_VALUE_TYPE_STR(ValueType);
    }

    FORCEINLINE PCSTR GetSubTypeString() const
    {
        return REG_EVENT_SUB_TYPE_STR(SubType);
    }

    FORCEINLINE PWSTR GetKeyName()
    {
        return EVENT_GET_APPENDIX(this, KeyName, PWSTR);
    }

    FORCEINLINE PCWSTR GetKeyName() const
    {
        return EVENT_GET_APPENDIX(this, KeyName, PCWSTR);
    }

    FORCEINLINE PWSTR GetValueName()
    {
        return EVENT_GET_APPENDIX(this, ValueName, PWSTR);
    }

    FORCEINLINE PCWSTR GetValueName() const
    {
        return EVENT_GET_APPENDIX(this, ValueName, PCWSTR);
    }

    FORCEINLINE PVOID GetValueData() const
    {
        return EVENT_GET_APPENDIX(this, ValueData, PVOID);
    }

    FORCEINLINE PWSTR GetNewName()
    {
        return EVENT_GET_APPENDIX(this, NewName, PWSTR);
    }

    FORCEINLINE PCWSTR GetNewName() const
    {
        return EVENT_GET_APPENDIX(this, NewName, PCWSTR);
    }
};

struct CircularBuffer 
{
    PVOID BaseAddress;
    ULONG BufferSize;

    // This member might not be synchronized with TailOffset / HeadOffset
    // Use it with caution
    LONG Count;

    // These members are synchronized carefully between user and kernel mode
    ULONG WriteOffset;
    ULONG ReadOffset;
    LONG MemoryLeft;

    HANDLE Event;
};


#undef EVENT_GET_APPENDIX