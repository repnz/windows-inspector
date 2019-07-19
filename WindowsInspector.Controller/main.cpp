#include <iostream>
#include <Windows.h>
#include "InspectorDriver.hpp"
#include "EventFormatter.hpp"
#include <vector>

#define MB 1024 * 1024


int main() {
	std::cout << "Starting the Windows Inspector Controller" << std::endl;
	
	InspectorDriver driver;
	
	std::vector<BYTE> vec(MB);
	BYTE* buffer = &vec[0];

	while (true) {
		SIZE_T readBytes = driver.ReadEvents(buffer, vec.size());

		SIZE_T bufferIndex = 0;
		
		while (bufferIndex < readBytes) {
			auto event = (const EventHeader*)(buffer + bufferIndex);
			EventFormatter::dumpEvent(std::cout, event);
			bufferIndex += event->Size;
		}
	}

	return 0;
} 