#include "EventFormatter.hpp"
#include <iomanip>
#include <sstream>
#include "base64.h"

void DumpCommonInformation(std::ostream& outputStream, const EventHeader& e);
void DumpProcessExitEvent(std::ostream& outputStream, const ProcessExitEvent& e);
void DumpProcessCreateEvent(std::ostream& outputStream, const ProcessCreateEvent& e);
void DumpThreadCreateEvent(std::ostream& outputStream, const ThreadCreateEvent& e);
void DumpThreadExitEvent(std::ostream& outputStream, const ThreadExitEvent& e);
void DumpImageLoadEvent(std::ostream& outputStream, const ImageLoadEvent& e);
void DumpRegistryEvent(std::ostream& outputStream, const RegistryEvent& e);

const int EVENT_NAME_WIDTH = 10;
const int EVENT_TIMESTAMP_WIDTH = 10;

using std::setw; // only for 1
using std::setfill;
using std::hex;
using std::dec;
using std::left;
using std::right;

const std::string columnSeperator = "  ||  ";


void EventFormatter::DumpEvent(std::ostream& outputStream, const EventHeader* event)
{
	switch (event->Type) 
	{
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
    case EventType::RegistryEvent:
        DumpRegistryEvent(outputStream, *(RegistryEvent*)event);
        break;
	default:
		throw std::runtime_error("Error! Buffer Has Problems!");
	}

}

std::string EventFormatter::ToString(const EventHeader* header)
{
	std::stringstream evt;
	DumpEvent(evt, header);
	return evt.str();
}

void DumpTime(std::ostream& outputStream, const LARGE_INTEGER& time)
{
	

	SYSTEMTIME st;
	::FileTimeToSystemTime((FILETIME*)&time, &st);

	char fillChar = outputStream.fill();

	outputStream << setfill('0') <<
		setw(2) << st.wHour   << ":" <<
		setw(2) << st.wMinute << ":" <<
		setw(2) << st.wSecond << ":" <<
		setw(3) << st.wMilliseconds;

	outputStream << setfill(fillChar);
}


void DumpCommonInformation(std::ostream& outputStream, const EventHeader& e)
{

	DumpTime(outputStream, e.Time);
	
    outputStream << columnSeperator << setw(EVENT_NAME_WIDTH) << e.GetEventName() <<
        "ProcessId=" << setw(5) << e.ProcessId << columnSeperator <<
        "ThreadId=" << setw(5) << e.ThreadId << columnSeperator;
}

void DumpProcessExitEvent(std::ostream& outputStream, const ProcessExitEvent& e)
{
	DumpCommonInformation(outputStream, e);
}

void DumpAsAscii(PCWSTR pString, ULONG uSize, std::ostream& outputStream)
{
    for (ULONG i = 0; i < uSize; i++)
    {
        outputStream << (char)pString[i];
    }
}

std::string WideToString(PCWSTR lpString, ULONG uSize) 
{
	
	std::string s(uSize, ' ');

	for (ULONG i = 0; i < uSize; i++)
    {
		s[i] = (char)lpString[i];
	}

	return s;
}

void DumpProcessCreateEvent(std::ostream& outputStream, const ProcessCreateEvent& e)
{
	DumpCommonInformation(outputStream, e);
	
	outputStream <<
		"ParentProcessId="   << setw(5) << e.ParentProcessId << columnSeperator <<
		"NewProcessId="      << setw(5) << e.NewProcessId << columnSeperator <<
		"CommandLine="       << WideToString(e.GetProcessCommandLine(), e.CommandLine.Size/2) << columnSeperator <<
		std::endl;
}

void DumpThreadCreateEvent(std::ostream& outputStream, const ThreadCreateEvent& e)
{
	DumpCommonInformation(outputStream, e);
    
    outputStream << "TargetProcessId=" << setw(5) << e.TargetProcessId << columnSeperator;
    outputStream << "NewThreadId=" << setw(5) << e.NewThreadId << columnSeperator;
    outputStream << "StartAddress=" << "0x" << setfill('0') << hex << setw(16) << e.StartAddress << dec << columnSeperator;
    outputStream << setfill(' ') << std::endl;
}

void DumpThreadExitEvent(std::ostream& outputStream, const ThreadExitEvent& e)
{
	DumpCommonInformation(outputStream, e);
}

void DumpImageLoadEvent(std::ostream& outputStream, const ImageLoadEvent& e)
{
	DumpCommonInformation(outputStream, e);
    const std::streamsize w = outputStream.width();

    outputStream << "LoadAddress=" << setfill('0') << setw(16) << hex << e.LoadAddress << columnSeperator;
    outputStream << "ImageSize=" << e.ImageSize << columnSeperator << dec;
    outputStream << setfill(' ') << "ImageFileName=" << WideToString(e.GetImageFileName(), e.ImageFileName.Size / 2) << columnSeperator;
    outputStream <<  std::endl;
}


void DumpRegistryEvent(std::ostream& outputStream, const RegistryEvent& e)
{
    DumpCommonInformation(outputStream, e);

    outputStream <<
        "RegistryEventType=" << setw(16) << e.GetSubTypeString() << columnSeperator <<
        "KeyName=" << e.GetKeyName() << columnSeperator;

    if (e.ValueName.Offset == 0)
        return;

    outputStream << "ValueName=" << e.GetValueName() << columnSeperator;

    if (e.ValueData.Offset == 0)
        return;

    outputStream << "ValueType=" << e.GetValueTypeName() << columnSeperator;
    outputStream << "ValueData=";

    switch (e.ValueType)
    {
        case REG_DWORD:
        {
            uint32_t ValueData = *(PULONG)e.GetValueData();
            outputStream << "0x" << setfill('0') << setw(8) << hex << ValueData << setfill(' ') << dec;
            break;
        }
        case REG_QWORD:
        {
            uint64_t ValueData = *(uint64_t*)e.GetValueData();
            outputStream << "0x" << setfill('0') << setw(16) << hex << ValueData << setfill(' ') << dec;
            break;
        }
        case REG_SZ:
        {
            PCWSTR ValueData = (PCWSTR)e.GetValueData();
            DumpAsAscii(ValueData, e.ValueData.Size / 2, outputStream);
            break;
        }
        default:
        {
            outputStream << base64_encode((PUCHAR)e.GetValueData(), e.ValueData.Size);
            break;
        }
    }

    outputStream << setfill(' ');
    outputStream << columnSeperator;
    outputStream << std::endl;
}
