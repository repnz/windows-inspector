#include <ntifs.h>
#include <ntddk.h>
#include "DataItemList.hpp"
#include "Common.hpp"
#include "Mem.hpp"
#include "ObjectRef.hpp"
#include <WindowsInspector.Kernel/KernelApi.hpp>
#include "Error.hpp"
#include <WindowsInspector.Kernel/UserModeMapping.hpp>
#include <WindowsInspector.Kernel/DeviceIoControlCode.hpp>
#include "cpptools.hpp"

#define PROCESS_CALLBACK TRUE
//#define THREAD_CALLBACK  TRUE
//#define IMAGE_CALLBACK   TRUE

void OnProcessNotify(_Inout_ PEPROCESS, _In_ HANDLE ProcessId, _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo);
void OnThreadNotify(_In_ HANDLE ProcessId, _In_ HANDLE ThreadId, _In_ BOOLEAN Create);
void OnImageLoadNotify(_In_opt_ PUNICODE_STRING FullImageName, _In_ HANDLE ProcessId, _In_ PIMAGE_INFO ImageInfo);

DataItemList list;

NTSTATUS DefaultDispatch(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp);

NTSTATUS DeviceIoControlDispatchWrapper(
	_In_ PDEVICE_OBJECT DeviceObject, 
	_Inout_ PIRP Irp
);

NTSTATUS DeviceIoControlDispatch(
	_In_ PDEVICE_OBJECT DeviceObject,
	_Inout_ PIRP Irp
);

void DriverUnload(PDRIVER_OBJECT DriverObject);

extern "C" NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
) {
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);
    
	if (!KernelApiInitialize())
    list.Init(500);

    PDEVICE_OBJECT deviceObject = nullptr;
    NTSTATUS status = STATUS_SUCCESS;

    // Variables needed for cleaning
    bool linkCreated = false;
    bool processCallback = false;
    bool threadCallback = false;
    bool imageCallback = false;

    UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\WindowsInspector");
    UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\WindowsInspector");

    do {

        status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &deviceObject);

        if (!NT_SUCCESS(status)) {
            KdPrint(("Failed to create device (0x%08X)\n", status));
            break;
        }
        deviceObject->Flags = DO_DIRECT_IO;
        
        status = IoCreateSymbolicLink(&symLink, &devName);

        if (!NT_SUCCESS(status)) {
            KdPrint((DRIVER_PREFIX  "Failed to create symbolic link (0x%08X)\n", status));
            break;
        }
        
#ifdef PROCESS_CALLBACK
        status = PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, FALSE);

        if (!NT_SUCCESS(status)) {
            KdPrint((DRIVER_PREFIX "Failed to register process callback (0x%08X)\n", status));
            break;
        }

        processCallback = true;
#endif

#ifdef THREAD_CALLBACK
        status = PsSetCreateThreadNotifyRoutine(OnThreadNotify);

        if (!NT_SUCCESS(status)) {
            KdPrint((DRIVER_PREFIX "Failed to create thread creation callback (0x%08X)\n", status));
            break;
        }

        threadCallback = true;
#endif

#ifdef IMAGE_CALLBACK
        status = PsSetLoadImageNotifyRoutine(OnImageLoadNotify);

        if (!NT_SUCCESS(status)) {
            KdPrint((DRIVER_PREFIX "Failed to create image load callback (0x%08X)", status));
            break;
        }
#endif

		DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControlDispatch;
        DriverObject->MajorFunction[IRP_MJ_CREATE] = DefaultDispatch;
        DriverObject->MajorFunction[IRP_MJ_CLOSE] = DefaultDispatch;
        DriverObject->DriverUnload = DriverUnload;
    } while (false);
    
    if (!NT_SUCCESS(status)) {
        if (imageCallback) {
            PsRemoveLoadImageNotifyRoutine(OnImageLoadNotify);
        }

        if (threadCallback) {
            PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);
        }

        if (processCallback) {
            PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);
        }

        if (linkCreated) {
            IoDeleteSymbolicLink(&symLink);
        }

        if (deviceObject != nullptr) {
            IoDeleteDevice(deviceObject);
        }
    }
    
    return status;
}


void DriverUnload(PDRIVER_OBJECT DriverObject) {
	// Remove callbacks

#ifdef IMAGE_CALLBACK
	PsRemoveLoadImageNotifyRoutine(OnImageLoadNotify);
#endif

#ifdef THREAD_CALLBACK
	PsRemoveCreateThreadNotifyRoutine(OnThreadNotify);
#endif

#ifdef PROCESS_CALLBACK
	PsSetCreateProcessNotifyRoutineEx(OnProcessNotify, TRUE);
#endif

	// Remove Device
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\??\\WindowsInspector");
	IoDeleteSymbolicLink(&symLink);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS CompleteIrp(PIRP Irp, NTSTATUS status = STATUS_SUCCESS, ULONG_PTR info = 0) {
    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = info;
    IoCompleteRequest(Irp, 0);
    return status;
}

NTSTATUS DefaultDispatch(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
) {
	UNREFERENCED_PARAMETER(DeviceObject);

    return CompleteIrp(Irp, STATUS_SUCCESS);
}


#define INSPECTOR_GET_EVENTS_FUNCTION_CODE 0x1

NTSTATUS DeviceIoControlDispatchWrapper(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp) {
	__try {
		return DeviceIoControlDispatch(DeviceObject, Irp);
	} 
	__except (EXCEPTION_EXECUTE_HANDLER) {
		KdPrint(("Exception handling IOCTL: (0x%08X)\n", GetExceptionCode()));
		return CompleteIrp(Irp, STATUS_ACCESS_VIOLATION);
	}
}

NTSTATUS GetEventsHandler(_In_ PVOID UserModeBuffer, _In_ ULONG BufferSize, ULONG* BytesRead) {
	if (BufferSize == NULL || BufferSize == 0 || BytesRead == NULL) {
		return STATUS_INVALID_PARAMETER;
	}

	*BytesRead = 0;

	UserModeMapping mapping;

	NTSTATUS status = UserModeMapping::Create(
		UserModeBuffer,
		BufferSize,
		IoWriteAccess,
		&mapping);

	if (!NT_SUCCESS(status)) {
		return status;
	}

	status = list.ReadIntoBuffer(mapping.Buffer, mapping.Length, BytesRead);

	if (!NT_SUCCESS(status)) {
		return status;
	}

	return status;
}

NTSTATUS DeviceIoControlDispatch(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp)
{	
	UNREFERENCED_PARAMETER(DeviceObject);

	NTSTATUS status;

	PIO_STACK_LOCATION iosp = IoGetCurrentIrpStackLocation(Irp);
	DeviceIoControlCode controlCode(iosp->Parameters.DeviceIoControl.IoControlCode);

	if (controlCode.Method != METHOD_NEITHER || controlCode.DeviceType != FILE_DEVICE_UNKNOWN) {
		return CompleteIrp(Irp, STATUS_INVALID_PARAMETER_1);
	}

	switch (controlCode.Function) {
	case INSPECTOR_GET_EVENTS_FUNCTION_CODE:
	{
		status = GetEventsHandler(
			Irp->UserBuffer,
			iosp->Parameters.DeviceIoControl.InputBufferLength,
			&Irp->IoStatus.Information
		);
	}
	break;
	default:
		status = STATUS_INVALID_PARAMETER;

	};

	return CompleteIrp(Irp, status);
}


NTSTATUS InsertIntoList(PLIST_ENTRY listItem, EventType itemType) {
	UNREFERENCED_PARAMETER(itemType);
	NTSTATUS status = list.PushItem(listItem);

	if (!NT_SUCCESS(status)) {
		KdPrint(("Cannot insert item of type %d into the list (0x%08X)\n", (ULONG)itemType, status));
		ExFreePool(listItem);
		return status;
	}

	return status;
}


void OnProcessStart(_In_ HANDLE ProcessId, _Inout_ PPS_CREATE_NOTIFY_INFO CreateInfo) {
	if (CreateInfo->CommandLine == NULL) {
		KdPrint((DRIVER_PREFIX "Failed to log ProcessCreateInfo: CommandLine is NULL"));
		return;
	}
	
	auto* newItem = Mem::Allocate<ListItem<ProcessCreateEvent>>(CreateInfo->CommandLine->Length);

	if (newItem == NULL) {
		KdPrint((DRIVER_PREFIX "Failed to allocate memory for ProcessCreateInfo"));
		return;
	}

	ProcessCreateEvent& info = newItem->Item;
	info.Type = EventType::ProcessCreate;
	info.Size = sizeof(ProcessCreateEvent) + CreateInfo->CommandLine->Length;
	KeQuerySystemTimePrecise(&info.Time);
	info.NewProcessId = HandleToUlong(ProcessId);
	info.ParentProcessId = HandleToUlong(CreateInfo->ParentProcessId);
	info.CreatingProcessId = HandleToUlong(PsGetCurrentProcessId());
	info.CommandLineLength = CreateInfo->CommandLine->Length / 2;
	
	RtlCopyMemory(
		info.CommandLine,
		CreateInfo->CommandLine->Buffer, 
		CreateInfo->CommandLine->Length
	);

	InsertIntoList(&newItem->Entry, info.Type);
}

void OnProcessExit( _In_ HANDLE ProcessId) {
	auto* newItem = Mem::Allocate<ListItem<ProcessExitEvent>>();

	if (newItem == NULL) {
		KdPrint((DRIVER_PREFIX "Failed to allocate memory for ProcessExitInfo"));
		return;
	}

	ProcessExitEvent& info = newItem->Item;
	info.Type = EventType::ProcessExit;
	info.Size = sizeof(ProcessExitEvent);
	KeQuerySystemTimePrecise(&info.Time);
	info.ProcessId = HandleToUlong(ProcessId);
	
	InsertIntoList(&newItem->Entry, EventType::ProcessExit);
}

void OnProcessNotify(_Inout_ PEPROCESS ProcessObject, _In_ HANDLE ProcessId, _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo) {
	UNREFERENCED_PARAMETER(ProcessObject);

    if (CreateInfo) {
		OnProcessStart(ProcessId, CreateInfo);
    }
    else {
		OnProcessExit(ProcessId);
    }
}

NTSTATUS GetThreadWin32StartAddress(_In_ ULONG ThreadId, _Out_ PULONG Win32StartAddress) {
	NTSTATUS status;
	HANDLE CreatingThreadObjectHandle;
	
	PETHREAD thread;
	status = PsLookupThreadByThreadId(UlongToHandle(ThreadId), &thread);

	if (!NT_SUCCESS(status)) {
		KdPrint(("Failed to find thread with id %d (0x%08X)\n", ThreadId, status));
		return status;
	}

	status = ObOpenObjectByPointer(
		PsGetCurrentThread(),
		OBJ_KERNEL_HANDLE,
		NULL,
		THREAD_QUERY_INFORMATION,
		*PsThreadType,
		KernelMode,
		&CreatingThreadObjectHandle
	);

	ObDereferenceObject(thread);

	if (!NT_SUCCESS(status)) {
		KdPrintError("Failed to create a handle to the new thread (0x%08X)\n", status);
		return status;
	}

	status = ZwQueryInformationThread(
		CreatingThreadObjectHandle,
		ThreadQuerySetWin32StartAddress,
		Win32StartAddress,
		sizeof(ULONG),
		NULL
	);

	if (!NT_SUCCESS(status)) {
		KdPrint(("Cannot query thread start address %d (0x%08X)\n", ThreadId, status));
		ZwClose(CreatingThreadObjectHandle);
		return status;
	}

	ZwClose(CreatingThreadObjectHandle);
	return STATUS_SUCCESS;
}

void OnThreadStart(_In_ ULONG TargetProcessId, _In_ ULONG TargetThreadId) {
	auto* newItem = Mem::Allocate<ListItem<ThreadCreateEvent>>();

	if (newItem == nullptr) {
		KdPrint((DRIVER_PREFIX "Could not allocate thread start information"));
		return;
	}

	ThreadCreateEvent& info = newItem->Item;
	info.Size = sizeof(ThreadCreateEvent);
	info.Type = EventType::ThreadCreate;
	KeQuerySystemTimePrecise(&info.Time);

	if (!NT_SUCCESS(GetThreadWin32StartAddress(TargetThreadId, &info.StartAddress))) {
		ExFreePool(newItem);
		return;
	}
	
	info.CreatingProcessId = HandleToUlong(PsGetCurrentProcessId());
	info.CreatingThreadId = HandleToUlong(PsGetCurrentThreadId());
	info.NewThreadId = TargetThreadId;
	info.TargetProcessId = TargetProcessId;

	InsertIntoList(&newItem->Entry, EventType::ThreadCreate);
}


void OnThreadExit(_In_ ULONG ProcessId, _In_ ULONG ThreadId) {
	auto* newItem = Mem::Allocate<ListItem<ThreadExitEvent>>();

	if (newItem == nullptr) {
		KdPrint((DRIVER_PREFIX "Could not allocate thread exit information"));
		return;
	}

	ThreadExitEvent& info = newItem->Item;

	info.Size = sizeof(ThreadExitEvent);
	info.Type = EventType::ThreadExit;
	KeQuerySystemTimePrecise(&info.Time);
	info.ThreadId = ThreadId;
	info.ProcessId = ProcessId;

	InsertIntoList(&newItem->Entry, EventType::ThreadExit);
}

void OnThreadNotify(_In_ HANDLE ProcessId, _In_ HANDLE ThreadId, _In_ BOOLEAN Create) {

	if (Create) {
		OnThreadStart(HandleToUlong(ProcessId), HandleToUlong(ThreadId));
	}
    else {
		OnThreadExit(HandleToUlong(ProcessId), HandleToUlong(ThreadId));
    }

}

const WCHAR Unknown[] = L"(Unknown)";
SIZE_T UnknownSize = sizeof(Unknown);

void OnImageLoadNotify(_In_opt_ PUNICODE_STRING FullImageName, _In_ HANDLE ProcessId, _In_ PIMAGE_INFO ImageInfo) {
	ULONG fullImageNameLen = 0;
	PCWSTR fullImageName;

	if (FullImageName) {
		fullImageName = FullImageName->Buffer;
		fullImageNameLen = FullImageName->Length;
	}
	else {
		fullImageName = Unknown;
		fullImageNameLen = UnknownSize;
	}

	auto* newItem = Mem::Allocate<ListItem<ImageLoadEvent>>(fullImageNameLen);

	if (newItem == NULL) {
		KdPrint((DRIVER_PREFIX "Could not allocate load image info"));
		return;
	}

	ImageLoadEvent& info = newItem->Item;
	
	RtlCopyMemory(info.ImageFileName, fullImageName, fullImageNameLen);
	info.ImageFileNameLength = fullImageNameLen / 2;
	info.ImageSize = ImageInfo->ImageSize;
	info.ProcessId = HandleToUlong(ProcessId);
	info.LoadAddress = (ULONG)ImageInfo->ImageBase;
	info.ThreadId = HandleToUlong(PsGetCurrentThreadId());
	info.Size = sizeof(ImageLoadEvent) + (USHORT)fullImageNameLen;
	info.Type = EventType::ImageLoad;
	KeQuerySystemTimePrecise(&info.Time);

	InsertIntoList(&newItem->Entry, EventType::ImageLoad);
	
}
