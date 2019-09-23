#include "Common.h"

PCSTR CONST EventTypeStr[] =
{
    "None",
    "ProcessCreate",
    "ProcessExit",
    "ImageLoad",
    "ThreadCreate",
    "ThreadExit",
    "RegistryEvent"
};


PCSTR CONST RegistryEventTypeStr[] =
{
    "DeleteKey",
    "SetValue",
    "RenameKey",
    "QueryValue",
    "DeleteValue"
};

PCSTR CONST RegistryValueDataTypeStr[] =
{
    "REG_NONE",
    "REG_SZ",
    "REG_EXPAND_SZ",
    "REG_BINARY",
    "REG_DWORD",
    "REG_DWORD_BIG_ENDIAN",
    "REG_LINK",
    "REG_MULTI_SZ",
    "REG_RESOURCE_LIST",
    "REG_FULL_RESOURCE_DESCRIPTOR",
    "REG_RESOURCE_REQUIREMENTS_LIST",
    "REG_QWORD"
};