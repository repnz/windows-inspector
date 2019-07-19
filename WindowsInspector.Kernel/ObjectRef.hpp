#pragma once
#include <ntddk.h>

struct ObjectRef {
	PVOID object;

	ObjectRef(PVOID object) : object(object){
		ObReferenceObject(object);
	}

	~ObjectRef() {
		ObDereferenceObject(object);
	}
};