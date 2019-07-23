#include "WindowsInspectorController.h"
#include <Windows.h>
#include <vector>
#include "InspectorDriver.hpp"
#include "EventFormatter.hpp"

static volatile bool Running = true;

void WindowsInspectorController::Listen() 
{
	InspectorDriver driver;
	const int MB = 1024 * 1024;
	std::vector<BYTE> vec(MB);
	BYTE* buffer = &vec[0];

	while (Running) 
	{
		SIZE_T readBytes = driver.ReadEvents(buffer, (DWORD)vec.size());
		SIZE_T bufferIndex = 0;

		while (bufferIndex < readBytes) 
		{
			const EventHeader* event = (const EventHeader*)(buffer + bufferIndex);
			EventFormatter::DumpEvent(std::cout, event);
			bufferIndex += event->Size;
		}

		Sleep(200);
	}

	std::cout << "Received a Signal to exit!~" << std::endl;
}

void WindowsInspectorController::Stop()
{
	Running = false;
}
