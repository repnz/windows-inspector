#include "EventFormatter.hpp"
#include <iomanip>
#include <sstream>

void DumpCommonInformation(std::ostream& outputStream, const EventHeader& e, const std::string& itemName);
void DumpProcessExitEvent(std::ostream& outputStream, const ProcessExitEvent& e);
void DumpProcessCreateEvent(std::ostream& outputStream, const ProcessCreateEvent& e);
void DumpThreadCreateEvent(std::ostream& outputStream, const ThreadCreateEvent& e);
void DumpThreadExitEvent(std::ostream& outputStream, const ThreadExitEvent& e);
void DumpImageLoadEvent(std::ostream& outputStream, const ImageLoadEvent& e);


void EventFormatter::dumpEvent(std::ostream& outputStream, const EventHeader* event) {


	switch (event->Type) {
	case EventType::ProcessExit:
		DumpProcessExitEvent(outputStream, *(ProcessExitEvent*)event);
		break;
	case EventType::ProcessCreate:
		DumpProcessCreateEvent(outputStream, *(ProcessCreateEvent*)event);
		break;
	case EventType::ThreadCreate:
		DumpThreadCreateEvent(outputStream, *(ThreadCreateEvent*)event);
		break;
	case EventType::ThreadExit:
		DumpThreadExitEvent(outputStream, *(ThreadExitEvent*)event);
		break;
	case EventType::ImageLoad:
		DumpImageLoadEvent(outputStream, *(ImageLoadEvent*)event);
		break;
	default:
		throw std::runtime_error("Error! Buffer Has Problems!");
	}

}

std::string EventFormatter::toString(const EventHeader* header)
{
	std::stringstream evt;
	dumpEvent(evt, header);
	return evt.str();
}


const int EVENT_NAME_WIDTH = 10;
const int EVENT_TIMESTAMP_WIDTH = 10;

using std::setw; // only for 1
using std::setfill;
using std::hex;
using std::dec;
using std::left;
using std::right;

const std::string columnSeperator = "  ||  ";

void DumpTime(std::ostream& outputStream, const LARGE_INTEGER& time) {
	

	SYSTEMTIME st;
	::FileTimeToSystemTime((FILETIME*)&time, &st);

	char fillChar = outputStream.fill();

	outputStream << setfill('0') <<
		setw(2) << st.wHour   << ":" <<
		setw(2) << st.wMinute << ":" <<
		setw(2) << st.wSecond << ":" <<
		setw(2) << st.wMilliseconds;

	outputStream << setfill(fillChar);
}

void DumpCommonInformation(std::ostream& outputStream, const EventHeader& e, const std::string& itemName) {

	DumpTime(outputStream, e.Time);
	
	outputStream << columnSeperator << setw(EVENT_NAME_WIDTH) << itemName;
	outputStream << columnSeperator;
}

void DumpProcessExitEvent(std::ostream& outputStream, const ProcessExitEvent& e)
{
	DumpCommonInformation(outputStream, e, "ProcessExit");
	outputStream << "ProcessId=" << e.ProcessId << columnSeperator << std::endl;

}


std::string WideToString(const WCHAR* lpString, ULONG uSize) {
	
	std::string s(uSize, ' ');

	for (ULONG i = 0; i < uSize; i++) {
		s[i] = (char)lpString[i*2];
	}

	return s;
}

void DumpProcessCreateEvent(std::ostream& outputStream, const ProcessCreateEvent& e)
{
	DumpCommonInformation(outputStream, e, "ProcessCreate");
	
	outputStream <<
		"CreatingProcessId=" << setw(5) << e.CreatingProcessId << columnSeperator <<
		"CreatingThreadId=" << setw(5) << e.CreatingThreadId << columnSeperator <<
		"ParentProcessId="   << setw(5) << e.ParentProcessId << columnSeperator <<
		"NewProcessId="      << setw(5) << e.NewProcessId << columnSeperator <<
		"CommandLine="       << WideToString(e.CommandLine, e.CommandLineLength) << columnSeperator <<
		std::endl;
}

void DumpThreadCreateEvent(std::ostream& outputStream, const ThreadCreateEvent& e)
{
	DumpCommonInformation(outputStream, e, "ThreadCreate");
	
	outputStream <<
		"CreatingProcessId=" << setw(5) << e.CreatingProcessId << columnSeperator <<
		"TargetProcessId=" << setw(5) << e.TargetProcessId << columnSeperator <<
		"CreatingThreadId=" << setw(5) << e.CreatingThreadId << columnSeperator <<
		"NewThreadId=" << setw(5) << e.NewThreadId << columnSeperator <<
		"StartAddress=" << "0x" << setfill('0') << setw(16) << hex << e.StartAddress << dec << columnSeperator <<
		std::endl;

	outputStream << setfill('0');

}

void DumpThreadExitEvent(std::ostream& outputStream, const ThreadExitEvent& e)
{
	DumpCommonInformation(outputStream, e, "ThreadExit");

	outputStream <<
		"ProcessId=" << setw(5) << e.ProcessId << columnSeperator <<
		"ThreadId=" << setw(5) << e.ThreadId << columnSeperator <<
		std::endl;
}

void DumpImageLoadEvent(std::ostream& outputStream, const ImageLoadEvent& e)
{
	DumpCommonInformation(outputStream, e, "ImageLoad");
	
	outputStream <<
		"ProcessId=" << setw(5) << e.ProcessId << columnSeperator <<
		"ThreadId=" << setw(5) << e.ThreadId << columnSeperator <<
		"LoadAddress=" << setw(16) << hex << setfill('0') << e.LoadAddress << setfill('0') << columnSeperator <<
		"ImageSize=" << setw(16) << e.ImageSize << columnSeperator << 
		"ImageFileName" << WideToString(e.ImageFileName, e.ImageFileNameLength) << columnSeperator <<
		std::endl;
}
