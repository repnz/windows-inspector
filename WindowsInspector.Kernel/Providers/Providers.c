#include <WindowsInspector.Kernel/Debug.h>
#include "Providers.h"
#include "ThreadProvider.h"
#include "ProcessProvider.h"
#include "ImageLoadProvider.h"
#include "RegistryProvider.h"

typedef enum _PROVIDER_STATE {
    ProviderStateNotRunning,
    ProviderStateDisabled,
    ProviderStateRunning
} PROVIDER_STATE;


typedef struct _PROVIDER_DESCRIPTOR {
    NTSTATUS(*Start)();
    NTSTATUS(*Stop)();
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
        ProviderStateDisabled
    },
    {
        InitializeThreadProvider,
        ReleaseThreadProvider,
        L"ThreadProvider",
        ProviderStateDisabled
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
    NTSTATUS Status = STATUS_SUCCESS;
    
    for (ULONG i = 0; i < NumberOfProviders; i++)
    {
        if (Providers[i].State == ProviderStateDisabled)
        {
            D_INFO_ARGS("Provider \"%ws\" is disabled.", Providers[i].ProviderName);
            continue;
        }
        
        D_INFO_ARGS("Initializing Provider \"%ws\"", Providers[i].ProviderName);

        Status = Providers[i].Start();

        if (!NT_SUCCESS(Status))
        {
            D_ERROR_STATUS_ARGS("Could not initialize provider \"%ws\"", Status, Providers[i].ProviderName);
            
            FreeProviders();

            return Status;
        }

        Providers[i].State = ProviderStateRunning;
    }

    return Status;
}

NTSTATUS
FreeProviders()
{
    NTSTATUS Status;

    for (ULONG i = 0; i < NumberOfProviders; i++)
    {
        if (Providers[i].State == ProviderStateRunning)
        {
            Status = Providers[i].Stop();

            if (!NT_SUCCESS(Status))
            {
                D_ERROR_STATUS_ARGS("Could not free provider %ws.", Status, Providers[i].ProviderName);
                return Status;
            }
            
            Providers[i].State = ProviderStateNotRunning;
        }
    }

    return STATUS_SUCCESS;
    
}
