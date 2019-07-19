#include "InspectorDriver.hpp"
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

SIZE_T InspectorDriver::ReadEvents(PVOID buffer, SIZE_T length)
{
	
	DWORD bytesReturned;

	if (!DeviceIoControl(
		hDriver.get(),
		INSPECTOR_GET_EVENTS_CTL_CODE,
		buffer,
		length,
		NULL,
		0,
		&bytesReturned,
		NULL)) {
		throw std::exception("Some error communicating with the driver");
	}

	return bytesReturned;
}
