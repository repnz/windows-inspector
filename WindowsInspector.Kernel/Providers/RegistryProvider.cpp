#include "RegistryProvider.hpp"
#include <WindowsInspector.Kernel/Common.hpp>
#include <WindowsInspector.Kernel/DriverObject.hpp>
#include <WindowsInspector.Kernel/Debug.hpp>
#include <WindowsInspector.Kernel/EventBuffer.hpp>

NTSTATUS RegistryCallback(PVOID CallbackContext, PVOID Argument1, PVOID Argument2);

static LARGE_INTEGER g_RegistryCookie;

NTSTATUS 
InitializeRegistryProvider()
{
    UNICODE_STRING Altitude = RTL_CONSTANT_STRING(L"7657.124");
    
    NTSTATUS Status = CmRegisterCallbackEx(RegistryCallback, &Altitude, g_DriverObject, NULL, &g_RegistryCookie, NULL);
    
    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS("Could not registry to registry callback: CmRegistryCallbackEx Failed", Status);
        return Status;
    }

    return Status;
}

VOID 
ReleaseRegistryProvider()
{
    CmUnRegisterCallback(g_RegistryCookie);
}

static
NTSTATUS
SendRegistryEvent(
    _In_ PVOID KeyObject,
    _In_ RegistryEventType EventSubType,
    _In_opt_ PCUNICODE_STRING ValueName,
    _In_opt_ PVOID Data,
    _In_ ULONG DataSize,
    _In_opt_ PCUNICODE_STRING NewName
)
{
    LARGE_INTEGER Time;
    KeQuerySystemTimePrecise(&Time);

    NTSTATUS Status;
    BufferEvent BufferEvent;
    HANDLE KeyHandle = NULL;
    ULONG KeyLength = 0;
    ULONG RegistryEventLength = sizeof(RegistryEvent);

    if (KeyObject == NULL)
    {
        Status = STATUS_INVALID_PARAMETER;
        goto cleanup;
    }
    
    //
    // Calculate the length of the registry event
    //
    Status = ObOpenObjectByPointer(KeyObject, OBJ_KERNEL_HANDLE, 0, KEY_QUERY_VALUE,
        *CmKeyObjectType,
        KernelMode,
        &KeyHandle
    );


    if (!NT_SUCCESS(Status))
    {
        goto cleanup;
    }

    Status = ZwQueryKey(KeyHandle, KeyNameInformation, NULL, 0, &KeyLength);


    if (!NT_SUCCESS(Status))
    {
        goto cleanup;
    }

    RegistryEventLength += KeyLength;
    
    
    if (ValueName != NULL)
    {
        RegistryEventLength += ValueName->Length;
    }

    if (DataSize)
    {
        RegistryEventLength += DataSize;
    }

    if (NewName != NULL)
    {
        RegistryEventLength += NewName->Length;
    }

    //
    // Allocate and initialize event
    //
    Status = AllocateBufferEvent(&BufferEvent, RegistryEventLength);

    if (!NT_SUCCESS(Status))
    {
        goto cleanup;
    }

    auto Event = (RegistryEvent*)BufferEvent.Memory;

    // Initialize Key Name

    Event->KeyName.Offset = KeyLength;
    Event->KeyName.Size = sizeof(RegistryEvent);

    Status = ZwQueryKey(KeyHandle, KeyNameInformation, Event->GetKeyName(), KeyLength, NULL);

    if (!NT_SUCCESS(Status))
    {
        goto cleanup;
    }
    
    // Initialize Value Name

    if (ValueName != NULL)
    {
        Event->ValueName.Offset = Event->KeyName.Offset + Event->KeyName.Size;
        Event->ValueName.Size = ValueName->Length;
        RtlCopyMemory(Event->GetValueName(), ValueName->Buffer, ValueName->Length);
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
        RtlCopyMemory(Event->GetValueData(), Data, DataSize);
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
        RtlCopyMemory(Event->GetNewName(), NewName->Buffer, NewName->Length);
    }
    else 
    {
        Event->NewName.Offset = 0;
        Event->NewName.Size = 0;
    }

    Event->Processid = HandleToUlong(PsGetCurrentProcessId());
    Event->ThreadId = HandleToUlong(PsGetCurrentThreadId());
    Event->Time = Time;
    Event->SubType = EventSubType;
    Event->Type = EventType::RegistryEvent;

    SendBufferEvent(&BufferEvent);

cleanup:
    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS_ARGS("Could not send RegistryEvent of type %s", Status, REG_EVENT_SUB_TYPE_STR(EventSubType));
    }

    if (KeyHandle != NULL)
    {
        ObCloseHandle(KeyHandle, KernelMode);
    }

    return Status;
}

FORCEINLINE NTSTATUS
DeleteKeyCallback(
    _In_ PREG_DELETE_KEY_INFORMATION Information
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
        RegistryEventType::DeleteKey,
        NULL,
        NULL,
        0,
        NULL
    );
}

FORCEINLINE
NTSTATUS
SetValueKeyCallback(
    _In_ PREG_SET_VALUE_KEY_INFORMATION Information
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
        RegistryEventType::SetValue,
        Information->ValueName,
        Information->Data,
        Information->DataSize,
        NULL
    );
}

FORCEINLINE
NTSTATUS
RenameKeyCallback(
    _In_ PREG_RENAME_KEY_INFORMATION Information
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
        RegistryEventType::RenameKey,
        NULL,
        NULL,
        0,
        Information->NewName
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
        RegistryEventType::QueryValue,
        Information->ValueName,
        NULL,
        0,
        NULL
    );
}


FORCEINLINE
NTSTATUS
DeleteValueKeyCallback(
    _In_ PREG_DELETE_VALUE_KEY_INFORMATION Information
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
        RegistryEventType::DeleteValue,
        Information->ValueName,
        NULL,
        0,
        NULL
    );
}

NTSTATUS 
RegistryCallback(
    _In_ PVOID CallbackContext, 
    _In_ PVOID Argument1, 
    _In_ PVOID Argument2
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