#pragma once
#include <windows.h>
#include <string>

// Initializes OSD overlay subsystem
void InitOSD();

// Updates OSD display state (active = true shows ">> TURBO 8.0x", active = false hides/fades)
void UpdateOSD(bool active, float speed);

// Closes and cleans up OSD window and resources
void ShutdownOSD();
