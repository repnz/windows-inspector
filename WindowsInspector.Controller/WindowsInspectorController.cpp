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
            // This should almost never happen
            WaitForSingleObject(buffer->Event, INFINITE);
        }
        
        LONG count = buffer->Count;

        for (LONG i = 0; i < count; i++)
        {
            // Handle event
            const EventHeader* event = (const EventHeader*)((PBYTE)buffer->BaseAddress + buffer->HeadOffset);
            EventFormatter::DumpEvent(std::cout, event);
        
            // Increment HeadOffset
            ULONG HeadOffset = buffer->HeadOffset + event->Size;

            if (HeadOffset > buffer->ResetOffset)
            {
                buffer->HeadOffset = 0;
            }
            else 
            {
                buffer->HeadOffset = HeadOffset;
            }
        }       

        InterlockedAdd(&buffer->Count, -count);
	}
	
	std::cout << "Received a Signal to exit!~" << std::endl;
}

void WindowsInspectorController::Stop()
{
	Running = false;
    driver.Stop();
}
