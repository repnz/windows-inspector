#pragma once
#include <iostream>
#include <WindowsInspector.Kernel/Common.hpp>

class EventFormatter {
public:
	static void DumpEvent(std::ostream& outputStream, const EventHeader* header);
	static std::string ToString(const EventHeader* header);
};