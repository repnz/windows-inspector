#include "Providers.hpp"
#include <WindowsInspector.Kernel/Debug.hpp>
#include <WindowsInspector.Kernel/Providers/ThreadProvider.hpp>
#include <WindowsInspector.Kernel/Providers/ProcessProvider.hpp>
#include <WindowsInspector.Kernel/Providers/ImageLoadProvider.hpp>
#include "RegistryProvider.hpp"

typedef enum _PROVIDER_STATE {
    ProviderStateNotRunning,
    ProviderStateDisabled,
    ProviderStateRunning
} PROVIDER_STATE;

typedef struct _PROVIDER_DESCRIPTOR {
    NTSTATUS(*Start)();
    VOID(*Stop)();
    PCWSTR ProviderName;
    PROVIDER_STATE State;
} PROVIDER_DESCRIPTOR, * PPROVIDER_DESCRIPTOR;


PROVIDER_DESCRIPTOR Providers[] = {
    {
        InitializeProcessProvider,
        ReleaseProcessProvider,
        L"ProcessProvider",
        ProviderStateNotRunning
    },
    {
        InitializeImageLoadProvider,
        ReleaseImageLoadProvider,
        L"ImageLoadProvider",
        ProviderStateNotRunning
    },
    {
        InitializeThreadProvider,
        ReleaseThreadProvider,
        L"ThreadProvider",
        ProviderStateNotRunning
    },
    {
        InitializeRegistryProvider,
        ReleaseRegistryProvider,
        L"RegistryProvider",
        ProviderStateDisabled
    }
};

CONST SIZE_T NumberOfProviders = sizeof(Providers) / sizeof(PROVIDER_DESCRIPTOR);

NTSTATUS 
InitializeProviders()
{
    NTSTATUS status = STATUS_SUCCESS;
    
    for (ULONG i = 0; i < NumberOfProviders; i++)
    {
        if (Providers[i].State == ProviderStateDisabled)
        {
            D_INFO_ARGS("Provider %ws is disabled.", Providers[i].ProviderName);
            continue;
        }
        
        D_INFO_ARGS("Initializing Provider %ws", Providers[i].ProviderName);

        status = Providers[i].Start();

        if (!NT_SUCCESS(status))
        {
            D_ERROR_STATUS_ARGS("Could not initialize provider \"%ws\"", status, Providers[i].ProviderName);
            
            FreeProviders();

            return status;
        }
    }

    return status;
}

VOID
FreeProviders()
{
    for (ULONG i = 0; i < NumberOfProviders; i++)
    {
        if (Providers[i].State == ProviderStateRunning)
        {
            Providers[i].Stop();
        }
    }
    
}
