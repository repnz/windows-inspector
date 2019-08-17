#pragma once
#include <ntifs.h>
#include "DataItemList.hpp"
#include "Common.hpp"

extern DataItemList g_EventList;

NTSTATUS InsertIntoList(PLIST_ENTRY listItem, EventType itemType);

