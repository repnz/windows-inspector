#pragma once

#ifdef KERNEL_DRIVER
#include <ntifs.h>
#else
#include <windows.h>
#include <winioctl.h>
#endif


#define INSPECTOR_LISTEN_CTL_CODE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x1, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define INSPECTOR_STOP_CTL_CODE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x2, METHOD_NEITHER, FILE_ANY_ACCESS)

typedef enum _EVT_TYPE {
    EvtNone,
    EvtProcessCreate,
    EvtProcessExit,
    EvtImageLoad,
    EvtThreadCreate,
    EvtThreadExit,
    EvtRegistryEvent
} EVT_TYPE, *PEVT_TYPE;

PCSTR CONST EventTypeStr[] = 
{
    "None",
    "ProcessCreate",
    "ProcessExit",
    "ImageLoad",
    "ThreadCreate",
    "ThreadExit",
    "RegistryEvent"
};


typedef struct _EVENT_HEADER {
    EVENT_TYPE Type;
    USHORT Size;
    LARGE_INTEGER Time;
    ULONG ProcessId;
    ULONG ThreadId;
} EVENT_HEADER, *PEVENT_HEADER;

FORCEINLINE 
PCSTR 
Event_GetEventName(
    __in PEVENT_HEADER Event
    )
{
    return EventTypeStr[(int)Event->Type];
}

typedef struct _APPENDIX_BUFFER {
    ULONG Offset;
    ULONG Size;
} APPENDIX_BUFFER, *PAPPENDIX_BUFFER;

#define EVENT_GET_APPENDIX(Event, Appendix, Type) (Type)(((PUCHAR)Event) + Appendix.Offset)


typedef struct _PROCESS_EXIT_EVENT {
    EVENT_HEADER Header;
} PROCESS_EXIT_EVENT, * PPROCESS_EXIT_EVENT;

typedef struct _PROCESS_CREATE_EVENT {
    EVENT_HEADER Header;
    ULONG NewProcessId;
    ULONG ParentProcessId;
    APPENDIX_BUFFER CommandLine;
} PROCESS_CREATE_EVENT, * PPROCESS_CREATE_EVENT;


FORCEINLINE
PWSTR 
ProcessCreate_GetCommandLine(
    __in PPROCESS_CREATE_EVENT Event
    )
{
    return EVENT_GET_APPENDIX(Event, Event->CommandLine, PWSTR);
}

typedef struct _THREAD_CREATE_EVENT {
    EVENT_HEADER Header;
    ULONG NewThreadId;
    ULONG TargetProcessId;
    ULONG_PTR StartAddress;
} THREAD_CREATE_EVENT, * PTHREAD_CREATE_EVENT;

typedef struct _THREAD_EXIT_EVENT {
    EVENT_HEADER Header;
} THREAD_EXIT_EVENT, * PTHREAD_EXIT_EVENT;


typedef struct _IMAGE_LOAD_EVENT {
    EVENT_HEADER Header;
    PVOID LoadAddress;
    ULONG_PTR ImageSize;
    APPENDIX_BUFFER ImageFileName;
} IMAGE_LOAD_EVENT, *PIMAGE_LOAD_EVENT;

FORCEINLINE
PWSTR 
ImageLoad_GetImageName(
    __in PIMAGE_LOAD_EVENT Event
    )
{
    return EVENT_GET_APPENDIX(Event, Event->ImageFileName, PWSTR);
}

typedef enum _REGISTRY_EVENT_TYPE {
    RegEvtDeleteKey,
    RegEvtSetValue,
    RegEvtRenameKey,
    RegEvtQueryValue,
    RegEvtDeleteValue
} REGISTRY_EVENT_TYPE, *PREGISTRY_EVENT_TYPE;

PCSTR CONST RegistryEventTypeStr[] =
{
    "DeleteKey",
    "SetValue",
    "RenameKey",
    "QueryValue",
    "DeleteValue"
};

#define REG_EVENT_SUB_TYPE_STR(SubType) RegistryEventTypeStr[(int)SubType]

PCSTR CONST RegistryValueDataTypeStr[] =
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

typedef struct _REGISTRY_EVENT {
    EVENT_HEADER Header;
    REGISTRY_EVENT_TYPE SubType;
    APPENDIX_BUFFER KeyName;
    APPENDIX_BUFFER ValueName;
    APPENDIX_BUFFER ValueData;
    ULONG ValueType;
    APPENDIX_BUFFER NewName;
} REGISTRY_EVENT, * PREGISTRY_EVENT;


FORCEINLINE 
PCSTR 
RegistryEvent_GetValueTypeName(
    __in PREGISTRY_EVENT Event
    )
{
    return REG_VALUE_TYPE_STR(Event->ValueType);
}

FORCEINLINE 
PCSTR 
RegistryEvent_GetSubTypeString(
    __in PREGISTRY_EVENT Event
    ) 
{
    return REG_EVENT_SUB_TYPE_STR(Event->SubType);
}

FORCEINLINE 
PWSTR 
RegistryEvent_GetKeyName(
    __in PREGISTRY_EVENT Event
    )
{
    return EVENT_GET_APPENDIX(Event, Event->KeyName, PWSTR);
}


FORCEINLINE 
PWSTR 
RegistryEvent_GetValueName(
    __in PREGISTRY_EVENT Event
    )
{
    return EVENT_GET_APPENDIX(Event, Event->ValueName, PWSTR);
}

FORCEINLINE 
PWSTR 
RegistryEvent_GetNewName(
    __in PREGISTRY_EVENT Event
    )
{
    return EVENT_GET_APPENDIX(Event, Event->NewName, PWSTR);
}


typedef struct _CIRCULAR_BUFFER {
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
} CIRCULAR_BUFFER, *PCIRCULAR_BUFFER;


#undef EVENT_GET_APPENDIX