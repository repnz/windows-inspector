#include "InspectorDriver.hpp"
#include <stdexcept>

InspectorDriver::InspectorDriver() 
{
	hDriver = OwnedHandle(CreateFileA(
		"\\\\.\\WindowsInspector",
		GENERIC_ALL,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	));

	if (hDriver.get() == INVALID_HANDLE_VALUE) 
	{
		throw std::runtime_error("Could not create handle to driver object");
	}

}

InspectorDriver::InspectorDriver(OwnedHandle hDriver) : hDriver(std::move(hDriver))
{
}

void InspectorDriver::Listen(CircularBuffer** buffer)
{
    DWORD bytesReturned;
    
    if (!DeviceIoControl(
        hDriver.get(),
        INSPECTOR_LISTEN_CTL_CODE,
        buffer,
        sizeof(ULONG_PTR),
        buffer,
        sizeof(ULONG_PTR),
        &bytesReturned,
        NULL))
    {
        throw std::runtime_error("Could not call INSPECTOR_LISTEN ioctl");
    }
}
