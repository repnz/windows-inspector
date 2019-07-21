#include <iostream>
#include <Windows.h>
#include "InspectorDriver.hpp"
#include "EventFormatter.hpp"
#include <vector>
#include <string>
#include <exception>

#define MB 1024 * 1024

volatile BOOL Running = TRUE;

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType);

void ControllerMain(const std::vector<std::string>& commandLineArguments) {
	std::cout << "Starting the Windows Inspector Controller" << std::endl;
	
	if (!SetConsoleCtrlHandler(CtrlHandler, TRUE)) {
		throw std::runtime_error("Could not register a Control-C Handler! " + std::to_string(GetLastError()));
	}

	InspectorDriver driver;
	
	std::vector<BYTE> vec(MB);
	BYTE* buffer = &vec[0];

	while (Running) {
		SIZE_T readBytes = driver.ReadEvents(buffer, (DWORD)vec.size());
		SIZE_T bufferIndex = 0;

		while (bufferIndex < readBytes) {
			const EventHeader* event = (const EventHeader*)(buffer + bufferIndex);
			EventFormatter::DumpEvent(std::cout, event);
			bufferIndex += event->Size;
		}

		Sleep(200);
	}

	std::cout << "Received a Signal to exit!~" << std::endl;
}

int main(int argc, const char** argv) {
	try {

		std::vector<std::string> commandLineArguments;

		for (int i = 0; i < argc; i++) {
			commandLineArguments.emplace_back(argv[i]);
		}

		ControllerMain(commandLineArguments);
	}
	catch (const std::exception& ex) {
		std::cerr << ex.what() << std::endl;
		return -1;
	}

	return 0;
} 

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
	switch (fdwCtrlType) {
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_BREAK_EVENT:
		Running = FALSE;
		return TRUE;
	default:
		return FALSE;
	}
}