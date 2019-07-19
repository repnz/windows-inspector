#pragma once
#include <WindowsInspector.Kernel/KernelApi.hpp>

ZwQueryInformationThreadFunc ZwQueryInformationThread;

bool KernelApiInitialize()
{
	UNICODE_STRING ZwQueryInformationThreadString = RTL_CONSTANT_STRING(L"ZwQueryInformationThread");

	ZwQueryInformationThread = (ZwQueryInformationThreadFunc)MmGetSystemRoutineAddress(&ZwQueryInformationThreadString);

	if (ZwQueryInformationThread == NULL) {
		return false;
	}


	return true;
}