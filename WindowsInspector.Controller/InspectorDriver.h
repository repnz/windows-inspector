#pragma once
#include <Windows.h>
#include <memory>

struct HandleDeleter {
	void operator()(HANDLE value) { 
		if (value != NULL && value != INVALID_HANDLE_VALUE) {
			CloseHandle(value);
		}
	}
};

using OwnedHandle = std::unique_ptr<std::remove_pointer<HANDLE>::type, HandleDeleter>;

class InspectorDriver {
	OwnedHandle hDriver;
public:

	InspectorDriver();
	InspectorDriver(OwnedHandle hDriver);


	void ReadEvents(PVOID buffer, SIZE_T length);
};