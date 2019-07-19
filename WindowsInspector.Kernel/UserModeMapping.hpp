#pragma once
#include <ntddk.h>

struct IoBuffer {
	PCHAR Buffer;
	ULONG Length;

	IoBuffer(PCHAR buffer, ULONG length) : Buffer(buffer), Length(length) {}

};


struct UserModeMapping {
	PMDL Mdl;
	PVOID Buffer;
	SIZE_T Length;


	UserModeMapping(PVOID buffer, SIZE_T length, PMDL mdl) :
		Mdl(mdl), Buffer(buffer), Length(length) {
	}

	UserModeMapping()
		: Mdl(NULL), Buffer(NULL), Length(0) {
	}

	UserModeMapping(UserModeMapping&& list) :
		Mdl(list.Mdl), Buffer(list.Buffer), Length(list.Length){
		
		list.Mdl = NULL;
		list.Buffer = NULL;
		list.Length = 0;
	}

	IoBuffer GetIoBuffer() {
		return IoBuffer { (PCHAR) Buffer, Length };
	}

	UserModeMapping(const UserModeMapping&) = delete;
	UserModeMapping& operator=(const UserModeMapping&) = delete;

	bool IsValid() const {
		return Buffer != NULL && Length != 0 && Mdl != NULL;
	}

	static NTSTATUS Create(
		_In_ PVOID UserModeBuffer, 
		_In_ SIZE_T Length, 
		_In_ LOCK_OPERATION IoOperation,
		_Inout_ UserModeMapping* OutputMapping
	);

	~UserModeMapping();
};