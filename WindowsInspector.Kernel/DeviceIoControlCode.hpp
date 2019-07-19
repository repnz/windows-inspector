#pragma once

#define FUNCTION_FROM_CTL_CODE(CODE) ((CODE >> 2) & 0b11111111111)
#define ACCESS_FROM_CTL_CODE(CODE) ((CODE >> 14) & 0b11)
#define DEVICE_TYPE_FROM_CTL_CODE(ctrlCode) (((ULONG)(ctrlCode & 0xffff0000)) >> 16)
#define METHOD_FROM_CTL_CODE(ctrlCode) ((ULONG)(ctrlCode & 3))


struct DeviceIoControlCode {
	UCHAR Method : 2;
	USHORT Function : 12;
	UCHAR Access : 2;
	USHORT DeviceType : 16;

	DeviceIoControlCode(ULONG Code) {
		Method = METHOD_FROM_CTL_CODE(Code);
		Function = FUNCTION_FROM_CTL_CODE(Code);
		Access = ACCESS_FROM_CTL_CODE(Code);
		DeviceType = DEVICE_TYPE_FROM_CTL_CODE(Code);
	}


	ULONG ReconstructCode() const {
		return ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method);
	}
};
