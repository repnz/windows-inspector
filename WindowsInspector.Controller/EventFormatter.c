#include "EventFormatter.h"
#include <ntstatus.h>
#include "ntos.h"
#include <stdio.h>
#include <WindowsInspector.Shared/MemStream.h>
#include <WindowsInspector.Shared/base64.h>

#define EVENT_NAME_WIDTH = 10;
#define EVENT_TIMESTAMP_WIDTH = 10;
#define COLUMN_SEPERATOR "  ||  "
	

NTSTATUS 
DumpTime(
	__in const PLARGE_INTEGER Time,
	__inout mem_stream* Stream
	)
{
	SYSTEMTIME st;
	stream_error rc;

	if (!FileTimeToSystemTime((FILETIME*)Time, &st))
	{
		printf("FileTimeToSystemTime failed. Error: 0x%08X\n", GetLastError());
		return STATUS_UNSUCCESSFUL;
	}
	
	rc = mem_stream_printf(Stream, NULL, "%02d:%02d:%02d:%03d", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
	
	if (!STRM_IS_SUCCESS(rc))
	{
		return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
}


NTSTATUS
DumpCommonInformation(
	__in PEVENT_HEADER Event,
	__inout mem_stream* Stream
	)
{
	NTSTATUS Status;

	Status = DumpTime(&Event->Time, Stream);

	if (!NT_SUCCESS(Status))
	{
		printf("DumpTime failed. (0x%08x)\n", Status);
		return Status;
	}

	if (!STRM_IS_SUCCESS(mem_stream_printf(Stream, NULL, "%10s", Event_GetEventName(Event))))
	{
		return STATUS_UNSUCCESSFUL;
	}
	if (!STRM_IS_SUCCESS(mem_stream_printf(Stream, NULL, "ProcessId=%05d " COLUMN_SEPERATOR, Event->ProcessId)))
	{
		return STATUS_UNSUCCESSFUL;
	}

	if (!STRM_IS_SUCCESS(mem_stream_printf(Stream, NULL, "ThreadId=%05d" COLUMN_SEPERATOR, Event->ThreadId)))
	{
		return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
}

NTSTATUS 
DumpProcessExitEvent(
	__in PPROCESS_EXIT_EVENT Event, 
	__inout mem_stream* Stream
	)
{
	return DumpCommonInformation(&Event->Header, Stream);
}


NTSTATUS 
DumpProcessCreateEvent(
	__in PPROCESS_CREATE_EVENT Event, 
	__inout mem_stream* Stream
	)
{
	DumpCommonInformation(&Event->Header, Stream);
	
	if (!STRM_IS_SUCCESS(mem_stream_printf(Stream, NULL, "ParentProcessId=%05d" COLUMN_SEPERATOR, Event->ParentProcessId)))
	{
		return STATUS_UNSUCCESSFUL;
	}

	if (!STRM_IS_SUCCESS(mem_stream_printf(Stream, NULL, "NewProcessId=%05d" COLUMN_SEPERATOR, Event->NewProcessId)))
	{
		return STATUS_UNSUCCESSFUL;
	}

	PCWSTR CommandLine = ProcessCreate_GetCommandLine(Event);
	
	if (!STRM_IS_SUCCESS(mem_stream_printf(Stream, NULL, "CommandLine=%S" COLUMN_SEPERATOR, CommandLine)))
	{
		return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
}

NTSTATUS
DumpThreadCreateEvent(
	__in PTHREAD_CREATE_EVENT Event, 
	__inout mem_stream* Stream
	)
{
	NTSTATUS Status = DumpCommonInformation(&Event->Header, Stream);

	if (!NT_SUCCESS(Status))
	{
		return Status;
	}
    
	if (!STRM_IS_SUCCESS(mem_stream_printf(Stream, NULL, "TargetProcessId=%05d" COLUMN_SEPERATOR, Event->TargetProcessId)))
	{
		return STATUS_UNSUCCESSFUL;
	}

	if (!STRM_IS_SUCCESS(mem_stream_printf(Stream, NULL, "NewThreadId=%05d" COLUMN_SEPERATOR, Event->NewThreadId)))
	{
		return STATUS_UNSUCCESSFUL;
	}

	if (!STRM_IS_SUCCESS(mem_stream_printf(Stream, NULL, "StartAddress=0x%016X" COLUMN_SEPERATOR, Event->NewThreadId)))
	{
		return STATUS_UNSUCCESSFUL;
	}

	if (!STRM_IS_SUCCESS((mem_stream_printf(Stream, NULL, "StartAddress=0x%016X" COLUMN_SEPERATOR, Event->StartAddress))))
	{
		return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
}

NTSTATUS 
DumpThreadExitEvent(
	__in PTHREAD_EXIT_EVENT Event, 
	__inout mem_stream* Stream
	)
{
	return DumpCommonInformation(&Event->Header, Stream);
}

NTSTATUS 
DumpImageLoadEvent(
	__in PIMAGE_LOAD_EVENT Event,
	__inout mem_stream* Stream
	)
{
	NTSTATUS Status = DumpCommonInformation(&Event->Header, Stream);

	if (!NT_SUCCESS(Status))
	{
		return Status;
	}

	PWSTR ImageFileName = ImageLoad_GetImageName(Event);
	
	if (!STRM_IS_SUCCESS(mem_stream_printf(Stream, NULL, "FileName=%S" COLUMN_SEPERATOR, ImageFileName)))
	{
		return STATUS_UNSUCCESSFUL;
	}

	if (!STRM_IS_SUCCESS(mem_stream_printf(Stream, NULL, "LoadAddress=0x%016X" COLUMN_SEPERATOR, Event->LoadAddress)))
	{
		return STATUS_UNSUCCESSFUL;
	}

	if (!STRM_IS_SUCCESS(mem_stream_printf(Stream, NULL, "ImageSize=0x%016X" COLUMN_SEPERATOR, Event->ImageSize)))
	{
		return STATUS_UNSUCCESSFUL;
	}

	return STATUS_SUCCESS;
}

#define BASE64_BUFFER_SIZE (1024*1024)


NTSTATUS 
DumpRegistryEvent(
	__in PREGISTRY_EVENT Event,
	__inout mem_stream* Stream
	)
{
	static PCHAR Base64Buffer = NULL;

	if (Base64Buffer == NULL)
	{
		Base64Buffer = HeapAlloc(NULL, 0, BASE64_BUFFER_SIZE);
	}

	NTSTATUS Status = DumpCommonInformation(&Event->Header, Stream);

	if (!NT_SUCCESS(Status))
	{
		return Status;
	}

	PCSTR SubType = RegistryEvent_GetSubTypeString(Event);

	if (!STRM_IS_SUCCESS(mem_stream_printf(Stream, NULL, "RegistryEventType=%s" COLUMN_SEPERATOR, SubType)))
	{
		return STATUS_UNSUCCESSFUL;
	}

	PWSTR KeyName = RegistryEvent_GetKeyName(Event);

	if (!STRM_IS_SUCCESS(mem_stream_printf(Stream, NULL, "RegistryEventType=%10S" COLUMN_SEPERATOR, KeyName)))
	{
		return STATUS_UNSUCCESSFUL;
	}

    if (Event->ValueName.Offset == 0)
        return STATUS_SUCCESS;

    PCWSTR ValueName = RegistryEvent_GetValueName(Event);

	if (!STRM_IS_SUCCESS(mem_stream_printf(Stream, NULL, "ValueName=%S" COLUMN_SEPERATOR, ValueName)))
	{
		return STATUS_UNSUCCESSFUL;
	}

    if (Event->ValueData.Offset == 0)
        return STATUS_SUCCESS;

	PCSTR ValueTypeName = RegistryEvent_GetValueTypeName(Event);

	if (!STRM_IS_SUCCESS(mem_stream_printf(Stream, NULL, "ValueType=%s" COLUMN_SEPERATOR, ValueTypeName)))
	{
		return STATUS_UNSUCCESSFUL;
	}

	if (!STRM_IS_SUCCESS(mem_stream_printf(Stream, NULL, "ValueData=")))
	{
		return STATUS_UNSUCCESSFUL;
	}
	
    switch (Event->ValueType)
    {
        case REG_DWORD:
        {
            uint32_t ValueData = *(uint32_t*)RegistryEvent_GetValueData(Event);
			
			if (!STRM_IS_SUCCESS(mem_stream_printf(Stream, NULL, "%u" COLUMN_SEPERATOR, ValueData)))
			{
				return STATUS_UNSUCCESSFUL;
			}

            break;
        }
        case REG_QWORD:
        {
			uint64_t ValueData = *(uint64_t*)RegistryEvent_GetValueData(Event);

			if (!STRM_IS_SUCCESS(mem_stream_printf(Stream, NULL, "%lu" COLUMN_SEPERATOR, ValueData)))
			{
				return STATUS_UNSUCCESSFUL;
			}

            break;
        }
        case REG_SZ:
        {
			PCWSTR ValueData = (PCWSTR)RegistryEvent_GetValueData(Event);

			if (!STRM_IS_SUCCESS(mem_stream_printf(Stream, NULL, "%S" COLUMN_SEPERATOR, ValueData)))
			{
				return STATUS_UNSUCCESSFUL;
			}

            break;
        }
        default:
        {
			PUCHAR ValueData = (PUCHAR)RegistryEvent_GetValueData(Event);
			unsigned int outputLength = base64_encode(ValueData, Event->ValueData.Size, Base64Buffer, BASE64_BUFFER_SIZE);
			
			if (!outputLength)
			{
				return STATUS_UNSUCCESSFUL;
			}
			
			ValueData[outputLength] = 0;

			if (!STRM_IS_SUCCESS(mem_stream_printf(Stream, NULL, "%s" COLUMN_SEPERATOR, Base64Buffer)))
			{
				return STATUS_UNSUCCESSFUL;
			}

            break;
        }
    }

    return STATUS_SUCCESS;
}



NTSTATUS
FmtDumpEvent(
	__inout PCHAR Buffer,
	__in ULONG BufferLength,
	__in PEVENT_HEADER Event
	)
{
	mem_stream Stream;

	if (STRM_IS_SUCCESS(mem_stream_init(&Stream, Buffer, BufferLength)))
	{
		return STATUS_UNSUCCESSFUL;
	}

	switch (Event->Type)
	{
	case EvtProcessExit:
		return DumpProcessExitEvent((PPROCESS_EXIT_EVENT)Event, &Stream);
	case EvtProcessCreate:
		return DumpProcessCreateEvent((PPROCESS_CREATE_EVENT)Event, &Stream);
	case EvtThreadCreate:
		return DumpThreadCreateEvent((PTHREAD_CREATE_EVENT)Event, &Stream);
	case EvtThreadExit:
		return DumpThreadExitEvent((PTHREAD_EXIT_EVENT)Event, &Stream);
	case EvtImageLoad:
		return DumpImageLoadEvent((PIMAGE_LOAD_EVENT)Event, &Stream);
	case EvtRegistryEvent:
		return DumpRegistryEvent((PREGISTRY_EVENT)Event, &Stream);
	default:
		printf("Error! Buffer Has Problems! Unknown Event Type: %d\n", Event->Type);
		return STATUS_UNSUCCESSFUL;
	}
}