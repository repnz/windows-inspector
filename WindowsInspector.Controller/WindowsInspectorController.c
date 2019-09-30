#include "WindowsInspectorController.h"
#include "InspectorDriver.h"
#include "EventFormatter.h"
#include <ntstatus.h>
#include "ntos.h"
#include <stdio.h>

static volatile BOOLEAN Running = TRUE;
#define EVENT_STRING_BUFFER_SIZE (1024*1024)


static 
VOID
WindowsInspectorListenLoop(
	__in PCIRCULAR_BUFFER Buffer,
	__in PSTR EventStringBuffer
	)
{
	NTSTATUS Status;

	while (Running)
	{
		if (!Buffer->Count)
		{
			ResetEvent(Buffer->Event);
			WaitForSingleObject(Buffer->Event, INFINITE);
		}

		LONG Count = Buffer->Count;
		LONG ReadMem = 0;
		ULONG ReadOffset = Buffer->ReadOffset;
		ULONG EventLength;

		for (LONG i = 0; i < Count; i++)
		{
			// Handle event
			PEVENT_HEADER Event = *(PEVENT_HEADER*)((PBYTE)Buffer->BaseAddress + ReadOffset);
			Status = FmtDumpEvent(EventStringBuffer, &EventLength, EVENT_STRING_BUFFER_SIZE, Event);

			if (!NT_SUCCESS(Status))
			{
				printf("Could not handle event 0x%p: 0x%08x", Event, Status);
			}
			else
			{
				printf("%.*s\n", EventLength, EventStringBuffer);
			}

			ReadMem += Event->Size;
			ReadOffset += sizeof(ULONG_PTR);
			ReadOffset %= Buffer->BufferSize;
		}

		Buffer->ReadOffset = ReadOffset;
		InterlockedAdd(&Buffer->Count, -Count);
		InterlockedAdd(&Buffer->MemoryLeft, ReadMem);
	}

	printf("Received a Signal to exit!~\n");
}

NTSTATUS
WindowsInspectorListen(
	VOID
	) 
{
	NTSTATUS Status = STATUS_SUCCESS;
	NTSTATUS ReleaseStatus;
	INSPECTOR_DRIVER Driver;
	PCIRCULAR_BUFFER Buffer = NULL;
	PSTR EventStringBuffer = NULL;

	BOOLEAN DriverInitialized = FALSE;
	BOOLEAN DriverListening = FALSE;
	BOOLEAN EventStringAllocated = FALSE;

	Status = DriverInitialize(&Driver);

	if (!NT_SUCCESS(Status))
	{
		printf("DriverInitialize failed. (0x%08X)\n", Status);
		goto cleanup;
	}

	DriverInitialized = TRUE;
    
	Status = DriverListen(&Driver, &Buffer);

	if (!NT_SUCCESS(Status))
	{
		printf("DriverListen Failed. (0x%08X)\n", Status);
		goto cleanup;
	}
    
	DriverListening = TRUE;

	EventStringBuffer = HeapAlloc(NULL, 0, EVENT_STRING_BUFFER_SIZE);

	if (EventStringBuffer == NULL)
	{
		printf("Could not allocate memory for string buffer (0x%08X)\n", GetLastError());
		Status = STATUS_INSUFFICIENT_RESOURCES;
		goto cleanup;
	}

	EventStringAllocated = TRUE;

	WindowsInspectorListenLoop(Buffer, EventStringBuffer);

cleanup:
	if (EventStringAllocated)
	{
		HeapFree(NULL, 0, EventStringBuffer);
	}

	if (DriverListening)
	{
		ReleaseStatus = DriverStop(&Driver);

		if (!NT_SUCCESS(ReleaseStatus))
		{
			printf("DriverStop failed. (0x%08X)\n", ReleaseStatus);
		}
	}

	if (DriverInitialized)
	{
		ReleaseStatus = DriverRelease(&Driver);

		if (!NT_SUCCESS(ReleaseStatus))
		{
			printf("DriverRelease failed. (0x%08X)\n", ReleaseStatus);
		}
	}
	
	return Status;
}

VOID
WindowsInspectorStop(
	VOID
	)
{
	Running = FALSE;
}
