#pragma once
#include <ntifs.h>
#include <ntddk.h>

struct ObjectHandle {
	HANDLE Handle;

	ObjectHandle(HANDLE Handle) : Handle(Handle) {
	}

	~ObjectHandle() {
		ZwClose(Handle);
	}
};