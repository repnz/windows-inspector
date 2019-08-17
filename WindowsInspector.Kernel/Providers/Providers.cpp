#include "Providers.hpp"
#include <WindowsInspector.Kernel/Error.hpp>

NTSTATUS InitializeProviders()
{
    bool processCallback = false;
    bool threadCallback = false;
    bool imageCallback = false;
    NTSTATUS status;

    do
    {
        KdPrintMessage("Registering Process Callbacks...");

        status = InitializeProcessProvider();

        if (!NT_SUCCESS(status))
        {
            KdPrintError("Failed to register process callback", status);
            break;
        }

        processCallback = true;


        KdPrintMessage("Registering Thread Callbacks...");

        status = InitializeThreadProvider();

        if (!NT_SUCCESS(status))
        {
            KdPrintError("Failed to create thread creation callback", status);
            break;
        }

        threadCallback = true;


        KdPrintMessage("Registering Image Callbacks");

        status = InitializeImageLoadProvider();

        if (!NT_SUCCESS(status))
        {
            KdPrintError("Failed to create image load callback", status);
            break;
        }

        imageCallback = true;

    } while (false);
    
    if (!NT_SUCCESS(status))
    {
        if (imageCallback)
        {
            ReleaseImageLoadProvider();
        }

        if (threadCallback)
        {
            ReleaseThreadProvider();
        }

        if (processCallback)
        {
            ReleaseProcessProvider();
        }
    }

    return status;
}

void FreeProviders()
{
    ReleaseImageLoadProvider();
    ReleaseProcessProvider();
    ReleaseThreadProvider();
}
