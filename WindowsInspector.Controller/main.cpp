#include <iostream>
#include <Windows.h>
#include "InspectorDriver.hpp"
#include "EventFormatter.hpp"
#include <vector>
#include <string>
#include <exception>
#include <lib/args/args.hxx>
#include <WindowsInspector.Controller\WindowsInspectorController.h>


BOOL WINAPI CtrlHandler(DWORD fdwCtrlType);
void ListenCommand(args::Subparser& parser);

int main(int argc, const char** argv) 
{
	args::ArgumentParser parser("WindowsInspector Controller: Communicate with the driver to control the system");
	args::Group commands(parser, "commands");
	
	// List of different sub-commands
	args::Command listen(parser, "listen", "Listen to windows inspector events", &ListenCommand);
	
	try 
	{
		if (!SetConsoleCtrlHandler(CtrlHandler, TRUE))
		{
			throw std::runtime_error("Could not register a Control-C Handler! " + std::to_string(GetLastError()));
		}

		parser.ParseCLI(argc, argv);
	}
	catch (args::Help)
	{
		std::cout << parser;
	}
	catch (const args::Error& error) 
	{
		std::cerr <<
			"Error parsing arguments: " <<
			error.what() <<
			"\n\n" << 
			parser;
		
	}
	catch (const std::exception& ex) 
	{
		std::cerr << ex.what() << std::endl;
		return -1;
	}

	return 0;
} 


void ListenCommand(args::Subparser& subParser)
{
	subParser.Parse();

	WindowsInspectorController::Listen();
}


BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
	switch (fdwCtrlType)
	{
	case CTRL_C_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_BREAK_EVENT:
		WindowsInspectorController::Stop();
		return TRUE;
	default:
		return FALSE;
	}
}