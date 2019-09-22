#include "RegistryProvider.h"
#include <WindowsInspector.Kernel/Common.h>
#include <WindowsInspector.Kernel/DriverObject.h>
#include <WindowsInspector.Kernel/Debug.h>
#include <WindowsInspector.Kernel/EventBuffer.h>

NTSTATUS 
RegistryCallback(
    __in PVOID CallbackContext, 
    __in PVOID Argument1, 
    __in PVOID Argument2
    );

static LARGE_INTEGER g_RegistryCookie;

NTSTATUS 
InitializeRegistryProvider(
    VOID
)
{
    NTSTATUS Status;

    UNICODE_STRING Altitude = RTL_CONSTANT_STRING(L"7657.124");
    Status = CmRegisterCallbackEx(RegistryCallback, &Altitude, g_DriverObject, NULL, &g_RegistryCookie, NULL);
    
    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS("Could not register to registry callback: CmRegistryCallbackEx Failed", Status);
        return Status;
    }

    return Status;
}

NTSTATUS 
ReleaseRegistryProvider(
    VOID
)
{
    return CmUnRegisterCallback(g_RegistryCookie);
}

static
NTSTATUS
SendRegistryEvent(
    __in PVOID KeyObject,
    __in REGISTRY_EVENT_TYPE EventSubType,
    __in_opt PCUNICODE_STRING ValueName,
    __in_opt PVOID Data,
    __in ULONG DataSize,
    __in_opt PCUNICODE_STRING NewName,
    __in_opt ULONG ValueType
)
{
    
    LARGE_INTEGER Time;
    KeQuerySystemTimePrecise(&Time);

    NTSTATUS Status;
    HANDLE KeyHandle = NULL;
    ULONG KeyLength = 0;
    USHORT RegistryEventLength = sizeof(REGISTRY_EVENT);
    PREGISTRY_EVENT Event;

    if (KeyObject == NULL)
    {
        Status = STATUS_INVALID_PARAMETER;
        goto cleanup;
    }
    
    //
    // Calculate the length of the registry event
    //
    Status = ObOpenObjectByPointer(
        KeyObject, 
        OBJ_KERNEL_HANDLE, 
        0, 
        KEY_QUERY_VALUE,
        *CmKeyObjectType,
        KernelMode,
        &KeyHandle
    );

    if (!NT_SUCCESS(Status))
    {
        D_ERROR("Cannot open handle to KeyObject");
        goto cleanup;
    }

    Status = ZwQueryKey(KeyHandle, KeyNameInformation, NULL, 0, &KeyLength);


    if (!NT_SUCCESS(Status))
    {
        D_ERROR("ZwQueryKey: Cannot Query The Length Of The Key Name");
        goto cleanup;
    }

    RegistryEventLength += (USHORT)KeyLength;
    
    
    if (ValueName != NULL)
    {
        RegistryEventLength += ValueName->Length;
    }

    if (DataSize)
    {
        RegistryEventLength += (USHORT)DataSize;
    }

    if (NewName != NULL)
    {
        RegistryEventLength += NewName->Length;
    }

    //
    // Allocate and initialize event
    //
    Status = AllocateBufferEvent(&Event, RegistryEventLength);

    if (!NT_SUCCESS(Status))
    {
        D_ERROR_ARGS("AllocateBufferEvent Failed: Cannot allocate %d bytes for RegistryEvent", 
            RegistryEventLength);
        goto cleanup;
    }

    // Initialize Key Name

    Event->KeyName.Offset = KeyLength;
    Event->KeyName.Size = sizeof(REGISTRY_EVENT);

    Status = ZwQueryKey(KeyHandle, KeyNameInformation, RegistryEvent_GetKeyName(Event), KeyLength, NULL);

    if (!NT_SUCCESS(Status))
    {
        D_ERROR("ZwQueryKey failed: Cannot query key name");
        goto cleanup;
    }
    
    // Initialize Value Name

    if (ValueName != NULL)
    {
        Event->ValueName.Offset = Event->KeyName.Offset + Event->KeyName.Size;
        Event->ValueName.Size = ValueName->Length;
        RtlCopyMemory(RegistryEvent_GetValueName(Event), ValueName->Buffer, ValueName->Length);
    }
    else
    {
        Event->ValueName.Offset = 0;
        Event->ValueName.Size = 0;
    }

    // Initialize Data

    if (Data != NULL)
    {
        Event->ValueData.Offset = Event->ValueName.Offset + Event->ValueName.Size;
        Event->ValueData.Size = DataSize;
        RtlCopyMemory(RegistryEvent_GetValueData(Event), Data, DataSize);
    }
    else
    {
        Event->ValueData.Offset = 0;
        Event->ValueData.Size = 0;
    }

    // New Name in case of a rename
    if (NewName != NULL)
    {
        Event->NewName.Offset = Event->ValueData.Offset + Event->ValueData.Size;
        Event->NewName.Size = NewName->Length;
        RtlCopyMemory(RegistryEvent_GetNewName(Event), NewName->Buffer, NewName->Length);
    }
    else 
    {
        Event->NewName.Offset = 0;
        Event->NewName.Size = 0;
    }

    Event->Header.Type = EvtRegistryEvent;
    Event->Header.ProcessId = HandleToUlong(PsGetCurrentProcessId());
    Event->Header.ThreadId = HandleToUlong(PsGetCurrentThreadId());
    Event->Header.Time = Time;

    Event->ValueType = ValueType;
    Event->SubType = EventSubType;

    SendBufferEvent(Event);

cleanup:
    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS_ARGS("Could not send RegistryEvent of type '%s'", Status, REG_EVENT_SUB_TYPE_STR(EventSubType));
    }

    if (KeyHandle != NULL)
    {
        ObCloseHandle(KeyHandle, KernelMode);
    }

    return Status;
}

FORCEINLINE 
NTSTATUS
DeleteKeyCallback(
    __in PREG_DELETE_KEY_INFORMATION Information
    )
{
    if (
        Information == NULL ||
        Information->Object == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }


    return SendRegistryEvent(
        Information->Object,
        RegEvtDeleteKey,
        NULL,
        NULL,
        0,
        NULL,
        0
    );
}

FORCEINLINE
NTSTATUS
SetValueKeyCallback(
    __in PREG_SET_VALUE_KEY_INFORMATION Information
    )
{
    if (
        Information == NULL ||
        Information->Object == NULL ||
        Information->ValueName == NULL ||
        Information->Data == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }


    return SendRegistryEvent(
        Information->Object,
        RegEvtSetValue,
        Information->ValueName,
        Information->Data,
        Information->DataSize,
        NULL,
        Information->Type
    );
}

FORCEINLINE
NTSTATUS
RenameKeyCallback(
    __in PREG_RENAME_KEY_INFORMATION Information
    )
{
    if (
        Information == NULL ||
        Information->Object == NULL ||
        Information->NewName == NULL
        )
    {
        return STATUS_INVALID_PARAMETER;
    }


    return SendRegistryEvent(
        Information->Object,
        RegEvtRenameKey,
        NULL,
        NULL,
        0,
        Information->NewName,
        0
    );
}

FORCEINLINE
NTSTATUS
QueryValueKeyCallback(
    _In_ PREG_QUERY_VALUE_KEY_INFORMATION Information
)
{
    if (
        Information == NULL ||
        Information->Object == NULL ||
        Information->ValueName == NULL
        )
    {
        return STATUS_INVALID_PARAMETER;
    }


    return SendRegistryEvent(
        Information->Object,
        RegEvtQueryValue,
        Information->ValueName,
        NULL,
        0,
        NULL,
        0
    );
}


FORCEINLINE
NTSTATUS
DeleteValueKeyCallback(
    __in PREG_DELETE_VALUE_KEY_INFORMATION Information
    )
{
    if (
        Information == NULL ||
        Information->Object == NULL ||
        Information->ValueName == NULL
        )
    {
        return STATUS_INVALID_PARAMETER;
    }


    return SendRegistryEvent(
        Information->Object,
        RegEvtDeleteValue,
        Information->ValueName,
        NULL,
        0,
        NULL,
        0
    );
}

NTSTATUS 
RegistryCallback(
    __in PVOID CallbackContext, 
    __in PVOID Argument1,
    __in PVOID Argument2
    )
{
    UNREFERENCED_PARAMETER(CallbackContext);

    ULONG* ArgumentUlong = (ULONG*)&Argument1;
    REG_NOTIFY_CLASS TypeClass = (REG_NOTIFY_CLASS)*ArgumentUlong;

    switch (TypeClass)
    {
        case RegNtPreDeleteKey:
        {
            DeleteKeyCallback((PREG_DELETE_KEY_INFORMATION)(Argument2));
            break;
        }
        case RegNtPreSetValueKey:
        {
            SetValueKeyCallback((PREG_SET_VALUE_KEY_INFORMATION)(Argument2));
            break;
        }
        case RegNtPreRenameKey:
        {
            RenameKeyCallback((PREG_RENAME_KEY_INFORMATION)(Argument2));
            break;
        }
        case RegNtPreDeleteValueKey:
        {
            DeleteValueKeyCallback((PREG_DELETE_VALUE_KEY_INFORMATION)(Argument2));
        }
        case RegNtPreQueryValueKey:
        {
            QueryValueKeyCallback((PREG_QUERY_VALUE_KEY_INFORMATION)(Argument2));
            break;
        }

        default:
            break;
    }

    return STATUS_SUCCESS;
}