#include <WindowsInspector.Kernel/Debug.hpp>
#include <WindowsInspector.Kernel/Common.hpp>
#include <WindowsInspector.Kernel/EventBuffer.hpp>
#include "ImageLoadProvider.hpp"

void OnImageLoadNotify(
    _In_opt_ PUNICODE_STRING FullImageName, 
    _In_ HANDLE ProcessId, 
    _In_ PIMAGE_INFO ImageInfo
);

NTSTATUS InitializeImageLoadProvider()
{
    return PsSetLoadImageNotifyRoutine(OnImageLoadNotify);
}

void ReleaseImageLoadProvider()
{
    PsRemoveLoadImageNotifyRoutine(OnImageLoadNotify);
}

const WCHAR Unknown[] = L"(Unknown)";
ULONG UnknownSize = sizeof(Unknown);

void OnImageLoadNotify(
    _In_opt_ PUNICODE_STRING FullImageName, 
    _In_ HANDLE ProcessId, 
    _In_ PIMAGE_INFO ImageInfo
)
{
    ULONG fullImageNameSize = 0;
    PCWSTR fullImageName;

    if (FullImageName)
    {
        fullImageName = FullImageName->Buffer;
        fullImageNameSize = FullImageName->Length;
    }
    else
    {
        fullImageName = Unknown;
        fullImageNameSize = UnknownSize;
    }
    
    ImageLoadEvent* event;
    NTSTATUS status = AllocateBufferEvent(&event, sizeof(ImageLoadEvent) + (USHORT)fullImageNameSize);

    if (!NT_SUCCESS(status))
    {
        D_ERROR("Could not allocate load image info");
        return;
    }

    event->ImageFileName.Size = fullImageNameSize;
    RtlCopyMemory(event->GetImageFileName(), fullImageName, fullImageNameSize);
    
    event->ImageSize = ImageInfo->ImageSize;
    event->ProcessId = HandleToUlong(ProcessId);
    event->LoadAddress = (ULONG_PTR)ImageInfo->ImageBase;
    event->ThreadId = HandleToUlong(PsGetCurrentThreadId());
    event->Type = EventType::ImageLoad;
    KeQuerySystemTimePrecise(&event->Time);

    SendBufferEvent(event);
}