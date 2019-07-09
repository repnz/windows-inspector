#pragma once
#include <Ntddk.h>

#define THREAD_QUERY_INFORMATION 0x40

NTSYSCALLAPI
NTSTATUS
NTAPI
ZwQueryInformationThread(
	_In_ HANDLE ThreadHandle,
	_In_ THREADINFOCLASS ThreadInformationClass,
	_Out_writes_bytes_(ThreadInformationLength) PVOID ThreadInformation,
	_In_ ULONG ThreadInformationLength,
	_Out_opt_ PULONG ReturnLength
);