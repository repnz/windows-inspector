#include "RegistryProvider.hpp"
#include <WindowsInspector.Kernel/Common.hpp>
#include <WindowsInspector.Kernel/DriverObject.hpp>
#include <WindowsInspector.Kernel/Debug.hpp>
#include <WindowsInspector.Kernel/EventBuffer.hpp>

NTSTATUS RegistryCallback(PVOID CallbackContext, PVOID Argument1, PVOID Argument2);

static LARGE_INTEGER g_RegistryCookie;

NTSTATUS InitializeRegistryProvider()
{
    UNICODE_STRING altitude = RTL_CONSTANT_STRING(L"7657.124");
    
    NTSTATUS status = CmRegisterCallbackEx(RegistryCallback, &altitude, g_DriverObject, NULL, &g_RegistryCookie, NULL);
    
    if (!NT_SUCCESS(status))
    {
        D_ERROR_STATUS("Could not registry to registry callback: CmRegistryCallbackEx Failed", status);
        return status;
    }

    return status;
}

void ReleaseRegistryProvider()
{
    CmUnRegisterCallback(g_RegistryCookie);
}

static 
VOID
InitializeCommonRegistryEvent(
    _Out_ RegistryEvent* event, 
    _In_ RegistryEventType eventType
)
{
    event->Processid = HandleToUlong(PsGetCurrentProcessId());
    event->ThreadId = HandleToUlong(PsGetCurrentThreadId());
    KeQuerySystemTimePrecise(&event->Time);
    event->SubType = eventType;
    event->Type = EventType::RegistryEvent;
}

NTSTATUS RegistryDeleteKeyCallback(RegistryEvent* SourceEvent, PREG_DELETE_KEY_INFORMATION DeleteKeyInformation) 
{
    NTSTATUS Status;
    HANDLE RegistryKeyHandle = NULL;
    BufferEvent BufferEvent;

    if (DeleteKeyInformation == NULL || DeleteKeyInformation->Object == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    Status = ObOpenObjectByPointer(DeleteKeyInformation->Object, OBJ_KERNEL_HANDLE, 0, KEY_QUERY_VALUE,
        *CmKeyObjectType,
        KernelMode,
        &RegistryKeyHandle
        );


    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS_ARGS("Could not create handle to registry object: 0x%p", Status, DeleteKeyInformation->Object);
        goto cleanup;
    }

    ULONG KeyLength;

    Status = ZwQueryKey(RegistryKeyHandle, KeyNameInformation, NULL, 0, &KeyLength);

    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS("Could not query key name of deleted key.", Status);
        goto cleanup;
    }


    SourceEvent->Size = sizeof(RegistryEvent) + KeyLength;

    Status = AllocateBufferEvent(&BufferEvent, SourceEvent->Size);

    
cleanup:
    if (RegistryKeyHandle != NULL)
    {
        ZwClose(RegistryKeyHandle);
    }

    return Status;
}

NTSTATUS RegistryCallback(PVOID CallbackContext, PVOID Argument1, PVOID Argument2)
{
    UNREFERENCED_PARAMETER(CallbackContext);

    REG_NOTIFY_CLASS type = (REG_NOTIFY_CLASS)Argument1;
    RegistryEvent event;
    InitializeCommonRegistryEvent(&event);
    NTSTATUS status;
    BufferEvent BufferEvent;

    switch (type)
    {
        case RegNtPreDeleteKey:
        {
            auto deleteKeyInformation = (PREG_DELETE_KEY_INFORMATION)(Argument2);
            PVOID RegistryKeyObject = deleteKeyInformation->Object;

        }

    }

}