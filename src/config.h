#pragma once
#include <windows.h>
#include <string>

enum ControlMode
{
	CONTROL_TOGGLE = 0,
	CONTROL_HOLD = 1
};

struct ModConfig
{
	float speedMultiplier;
	bool skipCutscenesAtSpeed;
	bool skipIntro;
	ControlMode controlMode;
	int keyboardHotkey;
	std::string keyboardHotkeyName;
	bool enableGamepad;
	WORD gamepadButtonMask;
	std::string gamepadComboName;
	bool showOSD;
	bool autoDisableInBattle;
	bool debugMode;
	bool logToFile;

	ModConfig()
		: speedMultiplier(8.0f)
		, skipCutscenesAtSpeed(true)
		, skipIntro(true)
		, controlMode(CONTROL_TOGGLE)
		, keyboardHotkey('R')
		, keyboardHotkeyName("R")
		, enableGamepad(true)
		, gamepadButtonMask(0x0020 | 0x4000) // BACK + X
		, gamepadComboName("SELECT+X")
		, showOSD(true)
		, autoDisableInBattle(true)
		, debugMode(true)
		, logToFile(true)
	{}
};

extern ModConfig g_Config;

bool LoadConfig(const std::wstring& iniPath);
int ParseVirtualKey(const std::string& keyStr);
WORD ParseGamepadButtons(const std::string& comboStr);
void LogMessage(const char* fmt, ...);
