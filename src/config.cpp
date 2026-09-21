#include "config.h"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstdio>

ModConfig g_Config;
static std::wstring g_LogPath = L"";

static std::string ToUpper(std::string str)
{
	std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return (char)::toupper(c); });
	return str;
}

static std::string Trim(const std::string& str)
{
	size_t first = str.find_first_not_of(" \t\r\n");
	if (first == std::string::npos) return "";
	size_t last = str.find_last_not_of(" \t\r\n");
	return str.substr(first, (last - first + 1));
}

int ParseVirtualKey(const std::string& keyStrRaw)
{
	std::string key = ToUpper(Trim(keyStrRaw));
	if (key.empty()) return VK_F1;

	if (key == "F1") return VK_F1;
	if (key == "F2") return VK_F2;
	if (key == "F3") return VK_F3;
	if (key == "F4") return VK_F4;
	if (key == "F5") return VK_F5;
	if (key == "F6") return VK_F6;
	if (key == "F7") return VK_F7;
	if (key == "F8") return VK_F8;
	if (key == "F9") return VK_F9;
	if (key == "F10") return VK_F10;
	if (key == "F11") return VK_F11;
	if (key == "F12") return VK_F12;

	if (key == "TAB") return VK_TAB;
	if (key == "SPACE") return VK_SPACE;
	if (key == "RETURN" || key == "ENTER") return VK_RETURN;
	if (key == "ESCAPE" || key == "ESC") return VK_ESCAPE;
	if (key == "TILDE" || key == "`" || key == "~") return VK_OEM_3;
	if (key == "CAPSLOCK" || key == "CAPS") return VK_CAPITAL;
	if (key == "SHIFT") return VK_SHIFT;
	if (key == "CTRL" || key == "CONTROL") return VK_CONTROL;
	if (key == "ALT") return VK_MENU;

	// Single alphanumeric character
	if (key.length() == 1)
	{
		char c = key[0];
		if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
			return (int)c;
	}

	return VK_F1;
}

WORD ParseGamepadButtons(const std::string& comboStrRaw)
{
	std::string combo = ToUpper(Trim(comboStrRaw));
	WORD mask = 0;

	// XInput standard button definitions
	// DPAD
	if (combo.find("DPAD_UP") != std::string::npos || combo.find("UP") != std::string::npos) mask |= 0x0001;
	if (combo.find("DPAD_DOWN") != std::string::npos || combo.find("DOWN") != std::string::npos) mask |= 0x0002;
	if (combo.find("DPAD_LEFT") != std::string::npos || combo.find("LEFT") != std::string::npos) mask |= 0x0004;
	if (combo.find("DPAD_RIGHT") != std::string::npos || combo.find("RIGHT") != std::string::npos) mask |= 0x0008;

	// Center buttons
	if (combo.find("START") != std::string::npos) mask |= 0x0010;
	if (combo.find("SELECT") != std::string::npos || combo.find("BACK") != std::string::npos || combo.find("VIEW") != std::string::npos) mask |= 0x0020;

	// Thumbs
	if (combo.find("L3") != std::string::npos || combo.find("LEFT_THUMB") != std::string::npos) mask |= 0x0040;
	if (combo.find("R3") != std::string::npos || combo.find("RIGHT_THUMB") != std::string::npos) mask |= 0x0080;

	// Bumpers
	if (combo.find("LB") != std::string::npos || combo.find("L1") != std::string::npos) mask |= 0x0100;
	if (combo.find("RB") != std::string::npos || combo.find("R1") != std::string::npos) mask |= 0x0200;

	// Face buttons
	if (combo.find("A") != std::string::npos || combo.find("CROSS") != std::string::npos) mask |= 0x1000;
	if (combo.find("B") != std::string::npos || combo.find("CIRCLE") != std::string::npos) mask |= 0x2000;
	if (combo.find("X") != std::string::npos || combo.find("SQUARE") != std::string::npos) mask |= 0x4000;
	if (combo.find("Y") != std::string::npos || combo.find("TRIANGLE") != std::string::npos) mask |= 0x8000;

	return mask ? mask : (0x0020 | 0x4000); // Default: SELECT + X
}

void LogMessage(const char* fmt, ...)
{
	if (!g_Config.logToFile) return;

	char buffer[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);

	FILE* f = fopen("cutscene_skip.log", "a");
	if (f)
	{
		SYSTEMTIME st;
		GetLocalTime(&st);
		fprintf(f, "[%02d:%02d:%02d.%03d] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buffer);
		fclose(f);
	}

	// Also output to DebugView / debugger if attached
	OutputDebugStringA(buffer);
	OutputDebugStringA("\n");
}

bool LoadConfig(const std::wstring& iniPath)
{
	wchar_t buf[256];

	// SpeedMultiplier
	if (GetPrivateProfileStringW(L"Settings", L"SpeedMultiplier", L"8.0", buf, 256, iniPath.c_str()))
	{
		float val = (float)_wtof(buf);
		if (val >= 1.0f && val <= 64.0f)
			g_Config.speedMultiplier = val;
	}

	// SkipCutscenesAtSpeed
	if (GetPrivateProfileStringW(L"Settings", L"SkipCutscenesAtSpeed", L"true", buf, 256, iniPath.c_str()))
	{
		g_Config.skipCutscenesAtSpeed = (_wcsicmp(buf, L"true") == 0 || _wcsicmp(buf, L"1") == 0);
	}

	// SkipIntro
	if (GetPrivateProfileStringW(L"Settings", L"SkipIntro", L"true", buf, 256, iniPath.c_str()))
	{
		g_Config.skipIntro = (_wcsicmp(buf, L"true") == 0 || _wcsicmp(buf, L"1") == 0);
	}

	// ControlMode
	if (GetPrivateProfileStringW(L"Settings", L"ControlMode", L"Toggle", buf, 256, iniPath.c_str()))
	{
		if (_wcsicmp(buf, L"Hold") == 0)
			g_Config.controlMode = CONTROL_HOLD;
		else
			g_Config.controlMode = CONTROL_TOGGLE;
	}

	// KeyboardHotkey
	if (GetPrivateProfileStringW(L"Settings", L"KeyboardHotkey", L"G", buf, 256, iniPath.c_str()))
	{
		char strBuf[64];
		wcstombs(strBuf, buf, sizeof(strBuf));
		g_Config.keyboardHotkeyName = Trim(strBuf);
		g_Config.keyboardHotkey = ParseVirtualKey(g_Config.keyboardHotkeyName);
	}

	// EnableGamepad
	if (GetPrivateProfileStringW(L"Settings", L"EnableGamepad", L"true", buf, 256, iniPath.c_str()))
	{
		g_Config.enableGamepad = (_wcsicmp(buf, L"true") == 0 || _wcsicmp(buf, L"1") == 0);
	}

	// GamepadCombo
	if (GetPrivateProfileStringW(L"Settings", L"GamepadCombo", L"SELECT+X", buf, 256, iniPath.c_str()))
	{
		char strBuf[64];
		wcstombs(strBuf, buf, sizeof(strBuf));
		g_Config.gamepadComboName = Trim(strBuf);
		g_Config.gamepadButtonMask = ParseGamepadButtons(g_Config.gamepadComboName);
	}

	// ShowOSD (On-Screen Display badge)
	if (GetPrivateProfileStringW(L"Settings", L"ShowOSD", L"true", buf, 256, iniPath.c_str()))
	{
		g_Config.showOSD = (_wcsicmp(buf, L"true") == 0 || _wcsicmp(buf, L"1") == 0);
	}

	// AutoDisableInBattle
	if (GetPrivateProfileStringW(L"Settings", L"AutoDisableInBattle", L"true", buf, 256, iniPath.c_str()))
	{
		g_Config.autoDisableInBattle = (_wcsicmp(buf, L"true") == 0 || _wcsicmp(buf, L"1") == 0);
	}

	// DebugMode (Live Scene & Event Tracer)
	if (GetPrivateProfileStringW(L"Settings", L"DebugMode", L"true", buf, 256, iniPath.c_str()))
	{
		g_Config.debugMode = (_wcsicmp(buf, L"true") == 0 || _wcsicmp(buf, L"1") == 0);
	}

	LogMessage("Config loaded: Speed=%.1fx, SkipCutscenes=%s, SkipIntro=%s, Mode=%s, TurboKey=%s, Gamepad=%s, ShowOSD=%s, AutoBattle=%s, DebugMode=%s",
		g_Config.speedMultiplier,
		g_Config.skipCutscenesAtSpeed ? "true" : "false",
		g_Config.skipIntro ? "true" : "false",
		g_Config.controlMode == CONTROL_TOGGLE ? "Toggle" : "Hold",
		g_Config.keyboardHotkeyName.c_str(),
		g_Config.gamepadComboName.c_str(),
		g_Config.showOSD ? "true" : "false",
		g_Config.autoDisableInBattle ? "true" : "false",
		g_Config.debugMode ? "true" : "false");

	return true;
}
