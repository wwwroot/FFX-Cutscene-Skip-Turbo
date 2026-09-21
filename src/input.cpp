#include "input.h"
#include "config.h"
#include "speedhack.h"
#include <atomic>

#pragma pack(push, 1)
typedef struct _XINPUT_GAMEPAD
{
	WORD  wButtons;
	BYTE  bLeftTrigger;
	BYTE  bRightTrigger;
	SHORT sThumbLX;
	SHORT sThumbLY;
	SHORT sThumbRX;
	SHORT sThumbRY;
} XINPUT_GAMEPAD;

typedef struct _XINPUT_STATE
{
	DWORD          dwPacketNumber;
	XINPUT_GAMEPAD Gamepad;
} XINPUT_STATE;
#pragma pack(pop)

typedef DWORD(WINAPI* XInputGetState_t)(DWORD dwUserIndex, XINPUT_STATE* pState);

static std::atomic<bool> g_InputThreadRunning(false);
static HANDLE g_hInputThread = NULL;
static HMODULE g_hXInputDll = NULL;
static XInputGetState_t g_pXInputGetState = nullptr;

static void InitXInput()
{
	const wchar_t* dllNames[] = {
		L"xinput1_4.dll",
		L"xinput1_3.dll",
		L"xinput9_1_0.dll"
	};

	for (const auto& dllName : dllNames)
	{
		g_hXInputDll = LoadLibraryW(dllName);
		if (g_hXInputDll)
		{
			g_pXInputGetState = (XInputGetState_t)GetProcAddress(g_hXInputDll, "XInputGetState");
			if (g_pXInputGetState)
			{
				LogMessage("XInput initialized using %ls", dllName);
				return;
			}
			FreeLibrary(g_hXInputDll);
			g_hXInputDll = NULL;
		}
	}
	LogMessage("XInput DLL not found or failed to load.");
}

static bool IsGameWindowFocused()
{
	HWND foreground = GetForegroundWindow();
	if (!foreground) return false;

	DWORD foregroundPid = 0;
	GetWindowThreadProcessId(foreground, &foregroundPid);
	return (foregroundPid == GetCurrentProcessId());
}

static DWORD WINAPI InputThreadProc(LPVOID lpParam)
{
	LogMessage("Input listener thread started.");
	InitXInput();

	bool wasInputActive = false;

	while (g_InputThreadRunning.load())
	{
		Sleep(15); // ~66 Hz polling rate, <0.01% CPU usage

		if (!IsGameWindowFocused())
		{
			// If we lost focus while in hold mode, turn off speed
			if (g_Config.controlMode == CONTROL_HOLD && IsSpeedActive())
			{
				SetSpeedActive(false);
			}
			wasInputActive = false;
			continue;
		}

		bool keyboardActive = false;
		if (g_Config.keyboardHotkey > 0)
		{
			keyboardActive = (GetAsyncKeyState(g_Config.keyboardHotkey) & 0x8000) != 0;
		}

		bool gamepadActive = false;
		if (g_Config.enableGamepad && g_pXInputGetState)
		{
			for (DWORD i = 0; i < 4; ++i)
			{
				XINPUT_STATE state;
				ZeroMemory(&state, sizeof(XINPUT_STATE));
				if (g_pXInputGetState(i, &state) == 0) // ERROR_SUCCESS
				{
					if (g_Config.gamepadButtonMask != 0 && (state.Gamepad.wButtons & g_Config.gamepadButtonMask) == g_Config.gamepadButtonMask)
					{
						gamepadActive = true;
						break;
					}
				}
			}
		}

		bool isCurrentlyPressed = keyboardActive || gamepadActive;

		static uint8_t s_LastThreadBattleState = 0;
		static uint8_t s_LastThreadBattlePhase = 0;

		if (g_Config.debugMode)
		{
			uint8_t curState = GetBattleState();
			uint8_t curPhase = GetBattlePhase();
			if (curState != s_LastThreadBattleState || curPhase != s_LastThreadBattlePhase)
			{
				LogMessage("[SCENE_TRANSITION] Battle state changed: State=%d -> %d | Phase=%d -> %d | Turbo=%s",
					s_LastThreadBattleState, curState,
					s_LastThreadBattlePhase, curPhase,
					IsSpeedActive() ? "ACTIVE" : "OFF");

				s_LastThreadBattleState = curState;
				s_LastThreadBattlePhase = curPhase;
			}
		}

		// Safety: Auto-disengage and prevent Turbo during combat / scripted battles
		if (g_Config.autoDisableInBattle && IsInBattle())
		{
			if (IsSpeedActive())
			{
				SetSpeedActive(false);
				LogMessage("Auto-disengaged Turbo: Battle/Combat transition active.");
			}
			if (isCurrentlyPressed && !wasInputActive && g_Config.debugMode)
			{
				LogMessage("[USER_INPUT] Turbo blocked: currently in combat/battle (State=%d, Phase=%d)",
					GetBattleState(), GetBattlePhase());
			}
			wasInputActive = isCurrentlyPressed;
			continue;
		}

		if (g_Config.controlMode == CONTROL_TOGGLE)
		{
			// Detect rising edge: press event
			if (isCurrentlyPressed && !wasInputActive)
			{
				if (g_Config.debugMode)
				{
					LogMessage("[USER_INPUT] Hotkey '%s' pressed (Source: %s) -> Toggling Turbo to %s",
						g_Config.keyboardHotkeyName.c_str(),
						keyboardActive ? "Keyboard" : "Gamepad",
						!IsSpeedActive() ? "ACTIVE" : "OFF");
				}
				SetSpeedActive(!IsSpeedActive());
			}
		}
		else if (g_Config.controlMode == CONTROL_HOLD)
		{
			if (isCurrentlyPressed != wasInputActive && g_Config.debugMode)
			{
				LogMessage("[USER_INPUT] Hotkey '%s' hold state changed -> Turbo %s",
					g_Config.keyboardHotkeyName.c_str(),
					isCurrentlyPressed ? "ACTIVE" : "OFF");
			}
			SetSpeedActive(isCurrentlyPressed);
		}

		wasInputActive = isCurrentlyPressed;
	}

	if (g_hXInputDll)
	{
		FreeLibrary(g_hXInputDll);
		g_hXInputDll = NULL;
		g_pXInputGetState = nullptr;
	}

	LogMessage("Input listener thread exiting.");
	return 0;
}

void StartInputThread()
{
	if (g_InputThreadRunning.load()) return;

	g_InputThreadRunning.store(true);
	g_hInputThread = CreateThread(NULL, 0, InputThreadProc, NULL, 0, NULL);
}

void StopInputThread()
{
	if (!g_InputThreadRunning.load()) return;

	g_InputThreadRunning.store(false);
	if (g_hInputThread)
	{
		WaitForSingleObject(g_hInputThread, 1000);
		CloseHandle(g_hInputThread);
		g_hInputThread = NULL;
	}
}
