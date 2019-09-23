#include <WindowsInspector.Kernel/Debug.h>
#include <WindowsInspector.Shared/Common.h>
#include <WindowsInspector.Kernel/EventBuffer.h>
#include "ImageLoadProvider.h"

VOID
OnImageLoadNotify(
    __in_opt PUNICODE_STRING FullImageName, 
    __in HANDLE ProcessId, 
    __in PIMAGE_INFO ImageInfo
    );

NTSTATUS 
InitializeImageLoadProvider(
    VOID
    )
{
    return PsSetLoadImageNotifyRoutine(OnImageLoadNotify);
}

NTSTATUS 
ReleaseImageLoadProvider(
    VOID
    )
{
    PsRemoveLoadImageNotifyRoutine(OnImageLoadNotify);
    return STATUS_SUCCESS;
}

CONST WCHAR Unknown[] = L"(Unknown)";
CONST USHORT UnknownSize = sizeof(Unknown);

VOID
OnImageLoadNotify(
    __in_opt PUNICODE_STRING FullImageName, 
    __in HANDLE ProcessId, 
    __in PIMAGE_INFO ImageInfo
)
{
    NTSTATUS Status;
    USHORT FixedFullImageSize = 0;
    PCWSTR FixedFullImageName;
    PIMAGE_LOAD_EVENT Event;

    //
    // Handle cases where FullImageName is NULL
    //
    if (FullImageName)
    {
        FixedFullImageName = FullImageName->Buffer;
        FixedFullImageSize = FullImageName->Length;
    }
    else
    {
        FixedFullImageName = Unknown;
        FixedFullImageSize = UnknownSize;
    }
    
    Status = AllocateBufferEvent(&Event, sizeof(IMAGE_LOAD_EVENT) + FixedFullImageSize);

    if (!NT_SUCCESS(Status))
    {
        D_ERROR_STATUS_ARGS("Could not allocate load image info for image \"%ws\"", Status, FixedFullImageName);
        return;
    }

    Event->ImageFileName.Size = FixedFullImageSize;
    RtlCopyMemory(ImageLoad_GetImageName(Event), FixedFullImageName, FixedFullImageSize);
    
    // Common Data
    Event->Header.Type = EvtImageLoad;
    Event->Header.ProcessId = HandleToUlong(ProcessId);
    Event->Header.ThreadId = HandleToUlong(PsGetCurrentThreadId());
    KeQuerySystemTimePrecise(&Event->Header.Time);

    // Image Load Data
    Event->ImageSize = ImageInfo->ImageSize;
    Event->LoadAddress = ImageInfo->ImageBase;

    SendBufferEvent(Event);
}