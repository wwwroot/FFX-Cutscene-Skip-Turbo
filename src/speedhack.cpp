#include "speedhack.h"
#include "config.h"
#include "osd.h"
#include "minhook/MinHook.h"
#include <cstdio>
#include <intrin.h>

// Reverse-engineered offsets for FFX.exe (x86 Steam release)
static constexpr uintptr_t OFFSET_SPEEDHACK      = 0x420C00; // UpdateDeltaTime function
static constexpr uintptr_t OFFSET_CUTSCENE_SKIP  = 0x30AEC0; // DialogProc (opcode/text advancing)
static constexpr uintptr_t OFFSET_OPENING_SCREEN = 0x260580; // IsOpeningScreenPlaying (Intro logo skip)
static constexpr uintptr_t OFFSET_OPENING_FLAG   = 0x8CB9C2; // Opening screen active flag (byte)
static constexpr uintptr_t OFFSET_BATTLE_STATE   = 0xD2C9F0; // Battle sub-state (10 = in battle)
static constexpr uintptr_t OFFSET_BATTLE_STATE2  = 0xD2A8E0; // Battle main state (0 = field, >0 = battle)
static constexpr uintptr_t OFFSET_BATTLE_PHASE   = 0xD2A8E4; // Battle phase (0 = none, 1 = init, 2 = combat)

static uintptr_t g_ModuleBase = 0;
static uintptr_t g_SpeedHackAddr = 0;
static uintptr_t g_CutsceneSkipAddr = 0;
static uintptr_t g_OpeningScreenAddr = 0;
static bool g_SpeedActive = false;
static bool g_HookInitialized = false;

// Original function pointer for UpdateDeltaTime
typedef void (__cdecl *UpdateDeltaTime_t)(float dt);
static UpdateDeltaTime_t g_pOriginalUpdateDeltaTime = nullptr;

// Original function pointer for DialogProc
typedef int (__thiscall *DialogProc_t)(void* thisPtr, uint32_t opcode, void* arg2);
static DialogProc_t g_pOriginalDialogProc = nullptr;

// Original function pointer for IsOpeningScreenPlaying
typedef bool (__cdecl *IsOpeningScreenPlaying_t)();
static IsOpeningScreenPlaying_t g_pOriginalIsOpeningScreen = nullptr;

// State inspection helpers
uint8_t GetBattleState()
{
	if (!g_ModuleBase) return 0;
	__try { return *(uint8_t*)(g_ModuleBase + OFFSET_BATTLE_STATE2); }
	__except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

uint8_t GetBattlePhase()
{
	if (!g_ModuleBase) return 0;
	__try { return *(uint8_t*)(g_ModuleBase + OFFSET_BATTLE_PHASE); }
	__except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

uint8_t GetBattleSub()
{
	if (!g_ModuleBase) return 0;
	__try { return *(uint8_t*)(g_ModuleBase + OFFSET_BATTLE_STATE); }
	__except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

bool IsInBattle()
{
	if (!g_ModuleBase) return false;

	__try
	{
		uint8_t bsMain = *(uint8_t*)(g_ModuleBase + OFFSET_BATTLE_STATE2);
		uint8_t bsPhase = *(uint8_t*)(g_ModuleBase + OFFSET_BATTLE_PHASE);

		// Active combat or battle transition
		return (bsMain != 0 || bsPhase != 0);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return false;
	}
}

// Detour function for IsOpeningScreenPlaying (Intro logo skip)
static bool __cdecl HookedIsOpeningScreenPlaying()
{
	if (g_Config.skipIntro)
	{
		if (g_ModuleBase)
		{
			__try
			{
				*(uint8_t*)(g_ModuleBase + OFFSET_OPENING_FLAG) = 0;
			}
			__except (EXCEPTION_EXECUTE_HANDLER) {}
		}
		return false;
	}

	if (g_pOriginalIsOpeningScreen)
	{
		return g_pOriginalIsOpeningScreen();
	}
	return false;
}

// Detour function for UpdateDeltaTime
static void __cdecl HookedUpdateDeltaTime(float dt)
{
	float effectiveDt = g_SpeedActive ? (dt * g_Config.speedMultiplier) : dt;
	g_pOriginalUpdateDeltaTime(effectiveDt);
}

// Detour function for DialogProc with Real-Time Scene & Event Tracer
static int __fastcall HookedDialogProc(void* thisPtr, void* edx_dummy, uint32_t opcode, void* arg2)
{
	void* caller = _ReturnAddress();
	uintptr_t callerRva = (uintptr_t)caller - g_ModuleBase;

	bool turboOn = g_SpeedActive;
	bool inBattle = IsInBattle();
	bool shouldSkip = false;

	// Cutscene Skip behavior: when Turbo is active and skipCutscenesAtSpeed is true
	if (turboOn && g_Config.skipCutscenesAtSpeed)
	{
		shouldSkip = true;
	}

	// Live Scene & Event Tracer
	if (g_Config.debugMode)
	{
		static uint32_t s_LastOpcode = 0;
		static bool s_LastTurbo = false;
		static uint8_t s_LastState = 0;
		static DWORD s_LastLogTick = 0;
		DWORD now = GetTickCount();

		uint8_t curState = GetBattleState();
		uint8_t curPhase = GetBattlePhase();

		// Record if opcode changes, or state changes, or periodic heartbeat
		if (opcode != s_LastOpcode || turboOn != s_LastTurbo || curState != s_LastState || (now - s_LastLogTick > 1500))
		{
			LogMessage("[SCENE_EVENT] Opcode: 0x%08X | Caller: FFX.exe+0x%06X | Action: %s | Turbo: %s | Battle: %s (State=%d, Phase=%d, Sub=%d)",
				opcode,
				(uint32_t)callerRva,
				shouldSkip ? "SKIPPED (Fast-Forward)" : "EXECUTED",
				turboOn ? "ACTIVE" : "OFF",
				inBattle ? "YES" : "NO",
				curState,
				curPhase,
				GetBattleSub());

			s_LastOpcode = opcode;
			s_LastTurbo = turboOn;
			s_LastState = curState;
			s_LastLogTick = now;
		}
	}

	if (shouldSkip)
	{
		// Fast-forward / skip dialogue box immediately
		return 0;
	}

	return g_pOriginalDialogProc(thisPtr, opcode, arg2);
}

bool InitSpeedHack(uintptr_t moduleBase)
{
	g_ModuleBase = moduleBase;
	g_SpeedHackAddr = moduleBase + OFFSET_SPEEDHACK;
	g_CutsceneSkipAddr = moduleBase + OFFSET_CUTSCENE_SKIP;
	g_OpeningScreenAddr = moduleBase + OFFSET_OPENING_SCREEN;

	// Verify original bytes at target addresses
	const uint8_t expectedPrologue[3] = { 0x55, 0x8B, 0xEC };

	if (memcmp((const void*)g_SpeedHackAddr, expectedPrologue, 3) != 0)
	{
		LogMessage("SpeedHack address 0x%p prologue mismatch!", (void*)g_SpeedHackAddr);
		return false;
	}

	if (memcmp((const void*)g_CutsceneSkipAddr, expectedPrologue, 3) != 0)
	{
		LogMessage("CutsceneSkip address 0x%p prologue mismatch!", (void*)g_CutsceneSkipAddr);
		return false;
	}

	// Initialize MinHook
	MH_STATUS status = MH_Initialize();
	if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
	{
		LogMessage("MH_Initialize failed with status %d", status);
		return false;
	}

	// Create hook for UpdateDeltaTime
	status = MH_CreateHook((LPVOID)g_SpeedHackAddr, &HookedUpdateDeltaTime, reinterpret_cast<LPVOID*>(&g_pOriginalUpdateDeltaTime));
	if (status != MH_OK)
	{
		LogMessage("MH_CreateHook on SpeedHack failed with status %d", status);
		return false;
	}

	// Create hook for DialogProc (enables real-time opcode logging + fast-forward)
	status = MH_CreateHook((LPVOID)g_CutsceneSkipAddr, &HookedDialogProc, reinterpret_cast<LPVOID*>(&g_pOriginalDialogProc));
	if (status != MH_OK)
	{
		LogMessage("MH_CreateHook on DialogProc failed with status %d", status);
		return false;
	}

	// Create hook for Opening Screen (Skip Intro Logos / Splash Videos)
	if (*(uint8_t*)g_OpeningScreenAddr == 0xA0)
	{
		status = MH_CreateHook((LPVOID)g_OpeningScreenAddr, &HookedIsOpeningScreenPlaying, reinterpret_cast<LPVOID*>(&g_pOriginalIsOpeningScreen));
		if (status == MH_OK)
		{
			LogMessage("Hooked IsOpeningScreenPlaying @ 0x%p (SkipIntro=%s)",
				(void*)g_OpeningScreenAddr, g_Config.skipIntro ? "ENABLED" : "DISABLED");
		}
		else
		{
			LogMessage("Warning: MH_CreateHook on OpeningScreen returned %d", status);
		}
	}

	// Enable all hooks
	status = MH_EnableHook(MH_ALL_HOOKS);
	if (status != MH_OK)
	{
		LogMessage("MH_EnableHook failed with status %d", status);
		return false;
	}

	g_HookInitialized = true;
	LogMessage("Hooks installed successfully! SpeedHack @ 0x%p, DialogProc Tracer @ 0x%p",
		(void*)g_SpeedHackAddr, (void*)g_CutsceneSkipAddr);
	return true;
}

void ShutdownSpeedHack()
{
	if (g_SpeedActive)
	{
		SetSpeedActive(false);
	}

	if (g_HookInitialized)
	{
		MH_DisableHook(MH_ALL_HOOKS);
		MH_Uninitialize();
		g_HookInitialized = false;
	}

	ShutdownOSD();
}

void SetSpeedActive(bool active)
{
	if (g_SpeedActive == active) return;

	g_SpeedActive = active;

	UpdateOSD(active, g_Config.speedMultiplier);

	LogMessage("Turbo / Cutscene Skip state changed: %s (%.1fx) | InBattle=%s (State=%d, Phase=%d)",
		active ? "ACTIVE" : "INACTIVE",
		active ? g_Config.speedMultiplier : 1.0f,
		IsInBattle() ? "YES" : "NO",
		GetBattleState(),
		GetBattlePhase());
}

bool IsSpeedActive()
{
	return g_SpeedActive;
}
