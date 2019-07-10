#include "InspectorDriver.h"
#include <exception>

InspectorDriver::InspectorDriver() {
	hDriver = OwnedHandle(CreateFileA(
		"\\\\.\\WindowsInspector",
		GENERIC_ALL,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	));

	if (hDriver.get() == INVALID_HANDLE_VALUE) {
		throw std::exception("Could not create handle to driver object");
	}

}

InspectorDriver::InspectorDriver(OwnedHandle hDriver) : hDriver(std::move(hDriver)){
}

void InspectorDriver::ReadEvents(PVOID buffer, SIZE_T length)
{
}
