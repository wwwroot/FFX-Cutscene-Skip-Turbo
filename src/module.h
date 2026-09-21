#pragma once
#include <windows.h>

#pragma pack(push, 1)
typedef struct
{
	unsigned char major;
	unsigned char minor;
	unsigned char step;
} tVersion;
#pragma pack(pop)

extern "C" {
	__declspec(dllexport) const char* FF10HgetName();
	__declspec(dllexport) tVersion FF10HgetVer();
}
