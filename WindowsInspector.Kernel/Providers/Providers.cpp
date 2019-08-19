#include "Providers.hpp"
#include <WindowsInspector.Kernel/Debug.hpp>

NTSTATUS InitializeProviders()
{
    bool processCallback = false;
    bool threadCallback = false;
    bool imageCallback = false;

    NTSTATUS status;

    do
    {
        D_INFO("Registering Process Callbacks...");

        status = InitializeProcessProvider();

        if (!NT_SUCCESS(status))
        {
            D_ERROR_STATUS("Failed to register process callback", status);
            break;
        }

        processCallback = true;


        D_INFO("Registering Thread Callbacks...");

        status = InitializeThreadProvider();

        if (!NT_SUCCESS(status))
        {
            D_ERROR_STATUS("Failed to create thread creation callback", status);
            break;
        }

        threadCallback = true;

        D_INFO("Registering Image Callbacks");

        status = InitializeImageLoadProvider();

        if (!NT_SUCCESS(status))
        {
            D_ERROR_STATUS("Failed to create image load callback", status);
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
