#include "speedhack.h"
#include "config.h"
#include "osd.h"
#include "minhook/MinHook.h"
#include <cstdio>
#include <intrin.h>

// Reverse-engineered offsets for FFX.exe (x86 Steam release)
static constexpr uintptr_t OFFSET_SPEEDHACK      = 0x420C00; // UpdateDeltaTime function
static constexpr uintptr_t OFFSET_CUTSCENE_SKIP  = 0x30AEC0; // Dialog & Voice stream advance
static constexpr uintptr_t OFFSET_OPENING_SCREEN = 0x260580; // IsOpeningScreenPlaying (Intro logo skip)
static constexpr uintptr_t OFFSET_OPENING_FLAG   = 0x8CB9C2; // Opening screen active flag (byte)
static constexpr uintptr_t OFFSET_BATTLE_STATE   = 0xD2C9F0; // Battle sub-state (10 = in battle)
static constexpr uintptr_t OFFSET_BATTLE_STATE2  = 0xD2A8E0; // Battle main state (0 = field, >0 = battle)
static constexpr uintptr_t OFFSET_BATTLE_PHASE   = 0xD2A8E4; // Battle phase (0 = none, 1 = init, 2 = combat)
static constexpr uintptr_t OFFSET_VIDEO_PLAYER   = 0x8DED2C; // Video player instance pointer
static constexpr uintptr_t VIDEO_ACTIVE_OFFSET   = 0x6D0;    // Byte flag (1 = video actively playing)

static uintptr_t g_ModuleBase = 0;
static uintptr_t g_SpeedHackAddr = 0;
static uintptr_t g_CutsceneSkipAddr = 0;
static uintptr_t g_OpeningScreenAddr = 0;
static bool g_SpeedActive = false;
static bool g_HookInitialized = false;

// Original function pointer for UpdateDeltaTime
typedef void (__cdecl *UpdateDeltaTime_t)(float dt);
static UpdateDeltaTime_t g_pOriginalUpdateDeltaTime = nullptr;

// Original function pointer for Dialog & Voice advance
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

bool IsMoviePlaying()
{
	if (!g_ModuleBase) return false;

	__try
	{
		uintptr_t pVideo = *(uintptr_t*)(g_ModuleBase + OFFSET_VIDEO_PLAYER);
		if (pVideo)
		{
			return (*(uint8_t*)(pVideo + VIDEO_ACTIVE_OFFSET) != 0);
		}
		return false;
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
	// Disengage Turbo during combat and FMV movies to prevent audio glitches and green screens
	bool turboAllowed = g_SpeedActive && !IsInBattle() && !IsMoviePlaying();
	float effectiveDt = turboAllowed ? (dt * g_Config.speedMultiplier) : dt;
	g_pOriginalUpdateDeltaTime(effectiveDt);
}

// Detour function for Dialog & Voice stream advance
static int __fastcall HookedDialogProc(void* thisPtr, void* edx_dummy, uint32_t opcode, void* arg2)
{
	bool turboOn = g_SpeedActive;
	bool inBattle = IsInBattle();
	bool inMovie = IsMoviePlaying();

	// ONLY fast-forward dialogue during in-engine cutscenes (NEVER during FMV movies or battles)
	if (turboOn && g_Config.skipCutscenesAtSpeed && !inBattle && !inMovie)
	{
		// Complete voice line immediately to fast-forward text box / dialogue advance
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

	// Create hook for Dialog & Voice stream advance (safely guarded against FMV movies and battles)
	if (memcmp((const void*)g_CutsceneSkipAddr, expectedPrologue, 3) == 0)
	{
		status = MH_CreateHook((LPVOID)g_CutsceneSkipAddr, &HookedDialogProc, reinterpret_cast<LPVOID*>(&g_pOriginalDialogProc));
		if (status == MH_OK)
		{
			LogMessage("Hooked Dialog & Voice Stream Advance @ 0x%p", (void*)g_CutsceneSkipAddr);
		}
		else
		{
			LogMessage("Warning: MH_CreateHook on DialogProc returned %d", status);
		}
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
	LogMessage("Hooks installed successfully! SpeedHack @ 0x%p", (void*)g_SpeedHackAddr);
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
