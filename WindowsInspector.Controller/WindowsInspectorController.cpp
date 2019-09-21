#include "WindowsInspectorController.h"
#include <Windows.h>
#include <vector>
#include "InspectorDriver.hpp"
#include "EventFormatter.hpp"

static volatile bool Running = true;

void WindowsInspectorController::Listen() 
{
	InspectorDriver driver;
    CircularBuffer* buffer = driver.Listen();
    
	while (Running)
	{
        if (!buffer->Count)
        {
            ResetEvent(buffer->Event);
            WaitForSingleObject(buffer->Event, INFINITE);
        }
        
        LONG count = buffer->Count;
        LONG mem = 0;
        ULONG ReadOffset = buffer->ReadOffset;

        for (LONG i = 0; i < count; i++)
        {
            // Handle event
            const EventHeader* event = *(const EventHeader**)((PBYTE)buffer->BaseAddress + ReadOffset);
            EventFormatter::DumpEvent(std::cout, event);
            mem += event->Size;
            ReadOffset += sizeof(ULONG_PTR);
            ReadOffset %= buffer->BufferSize;
        }       
        
        buffer->ReadOffset = ReadOffset;
        InterlockedAdd(&buffer->Count, -count);
        InterlockedAdd(&buffer->MemoryLeft, mem);
	}
	
	std::cout << "Received a Signal to exit!~" << std::endl;
    driver.Stop();
}

void WindowsInspectorController::Stop()
{
	Running = false;
}
