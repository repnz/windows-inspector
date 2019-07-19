#pragma once
#include <iostream>
#include <WindowsInspector.Kernel/Common.hpp>

class EventFormatter {
public:
	static void dumpEvent(std::ostream& outputStream, const EventHeader* header);
	static std::string toString(const EventHeader* header);
};