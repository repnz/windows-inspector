#include "FastMutex.hpp"
#include <ntddk.h>

void FastMutex::Init() 
{
    ExInitializeFastMutex(&_mutex);    
}

void FastMutex::Lock() 
{
    ExAcquireFastMutex(&_mutex);
}

void FastMutex::Unlock() 
{
    ExReleaseFastMutex(&_mutex);
}