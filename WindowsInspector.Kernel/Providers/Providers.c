#include <WindowsInspector.Kernel/Debug.h>
#include "Providers.h"
#include "ThreadProvider.h"
#include "ProcessProvider.h"
#include "ImageLoadProvider.h"
#include "RegistryProvider.h"
#include "ObjectHandleCallbacks.h"

BOOLEAN g_Listening = FALSE;

typedef enum _PROVIDER_STATE {
    ProviderStateNotRunning,
    ProviderStateDisabled,
    ProviderStateRunning
} PROVIDER_STATE;


typedef struct _PROVIDER_DESCRIPTOR {
    NTSTATUS (*Start)();
    VOID (*Stop)();
    PSTR ProviderName;
    PROVIDER_STATE State;
} PROVIDER_DESCRIPTOR, * PPROVIDER_DESCRIPTOR;


PROVIDER_DESCRIPTOR Providers[] = {
    {
        .Start        = InitializeProcessProvider,
        .Stop         = ReleaseProcessProvider,
        .ProviderName = "ProcessProvider",
        .State        = ProviderStateNotRunning
    },
    {
        .Start        = InitializeImageLoadProvider,
        .Stop         = ReleaseImageLoadProvider,
        .ProviderName = "ImageLoadProvider",
        .State        = ProviderStateDisabled
    },
    {
        .Start        = InitializeThreadProvider,
        .Stop         = ReleaseThreadProvider,
        .ProviderName = "ThreadProvider",
        .State        = ProviderStateDisabled
    },
    {
        .Start        = InitializeRegistryProvider,
        .Stop         = ReleaseRegistryProvider,
        .ProviderName = "RegistryProvider",
        .State        = ProviderStateDisabled
    },
	{
		.Start        = InitializeHandleCallbacksProvider,
		.Stop         = ReleaseHandleCallbacksProvider,
		.ProviderName = "HandleCallbacksProvider",
		.State        = ProviderStateDisabled
	}
};

CONST SIZE_T NumberOfProviders = sizeof(Providers) / sizeof(PROVIDER_DESCRIPTOR);

NTSTATUS 
InitializeProviders()
{
    NTSTATUS Status = STATUS_SUCCESS;
    D_INFO("Initializing Providers...");

    for (ULONG i = 0; i < NumberOfProviders; i++)
    {
        if (Providers[i].State == ProviderStateDisabled)
        {
            D_INFO_ARGS("Provider \"%s\" is disabled.", Providers[i].ProviderName);
            continue;
        }
        
        D_INFO_ARGS("Initializing Provider \"%s\"", Providers[i].ProviderName);

        Status = Providers[i].Start();

        if (!NT_SUCCESS(Status))
        {
            D_ERROR_STATUS_ARGS("Could not initialize provider \"%s\"", Status, Providers[i].ProviderName);
            
            FreeProviders();

            return Status;
        }

        Providers[i].State = ProviderStateRunning;
    }

    return Status;
}

VOID
FreeProviders()
{
	g_Listening = FALSE;

    for (ULONG i = 0; i < NumberOfProviders; i++)
    {
        if (Providers[i].State == ProviderStateRunning)
        {
            Providers[i].Stop();
            Providers[i].State = ProviderStateNotRunning;
        }
    }
}
