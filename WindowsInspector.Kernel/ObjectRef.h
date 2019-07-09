#pragma once
#include <ntddk.h>

struct ObjectRef {
	PVOID object;

	ObjectRef(PVOID object) {
		ObReferenceObject(object);
	}

	~ObjectRef() {
		ObDereferenceObject(object);
	}
};