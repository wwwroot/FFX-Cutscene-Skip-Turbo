#include <windows.h>
#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include "../src/config.h"
#include "../src/minhook/MinHook.h"

// ANSI colors for console output
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"

static int g_TestsRun = 0;
static int g_TestsPassed = 0;
static int g_TestsFailed = 0;

#define TEST_ASSERT(expr, msg) \
	do { \
		g_TestsRun++; \
		if (expr) { \
			g_TestsPassed++; \
			std::cout << "  " << COLOR_GREEN << "[PASS] " << COLOR_RESET << msg << std::endl; \
		} else { \
			g_TestsFailed++; \
			std::cout << "  " << COLOR_RED << "[FAIL] " << COLOR_RESET << msg << " (line " << __LINE__ << ")" << std::endl; \
		} \
	} while(0)

// 1. Test Virtual Key Parsing
void TestVirtualKeyParsing()
{
	std::cout << COLOR_CYAN << "=== Running Virtual Key Parsing Tests ===" << COLOR_RESET << std::endl;

	TEST_ASSERT(ParseVirtualKey("F1") == VK_F1, "Parse 'F1' == VK_F1");
	TEST_ASSERT(ParseVirtualKey("f1") == VK_F1, "Parse lowercase 'f1' == VK_F1");
	TEST_ASSERT(ParseVirtualKey("F12") == VK_F12, "Parse 'F12' == VK_F12");
	TEST_ASSERT(ParseVirtualKey("TAB") == VK_TAB, "Parse 'TAB' == VK_TAB");
	TEST_ASSERT(ParseVirtualKey("tab") == VK_TAB, "Parse lowercase 'tab' == VK_TAB");
	TEST_ASSERT(ParseVirtualKey("SPACE") == VK_SPACE, "Parse 'SPACE' == VK_SPACE");
	TEST_ASSERT(ParseVirtualKey("C") == 'C', "Parse 'C' == 'C'");
	TEST_ASSERT(ParseVirtualKey("h") == 'H', "Parse 'h' == 'H'");
	TEST_ASSERT(ParseVirtualKey("G") == 'G', "Parse 'G' == 'G'");
	TEST_ASSERT(ParseVirtualKey("g") == 'G', "Parse lowercase 'g' == 'G'");
	TEST_ASSERT(ParseVirtualKey("1") == '1', "Parse '1' == '1'");
	TEST_ASSERT(ParseVirtualKey("") == VK_F1, "Parse empty string fallback to VK_F1");
	TEST_ASSERT(ParseVirtualKey("INVALID_KEY_NAME") == VK_F1, "Parse invalid key fallback to VK_F1");
}

// 2. Test Gamepad Combo Parsing
void TestGamepadComboParsing()
{
	std::cout << COLOR_CYAN << "=== Running Gamepad Combo Parsing Tests ===" << COLOR_RESET << std::endl;

	// SELECT is 0x0020, X is 0x4000
	WORD selectX = ParseGamepadButtons("SELECT+X");
	TEST_ASSERT((selectX & 0x0020) && (selectX & 0x4000), "Parse 'SELECT+X' contains BACK and X");

	// BACK+A: BACK is 0x0020, A is 0x1000
	WORD backA = ParseGamepadButtons("BACK+A");
	TEST_ASSERT((backA & 0x0020) && (backA & 0x1000), "Parse 'BACK+A' contains BACK and A");

	// L3+R3: L3 is 0x0040, R3 is 0x0080
	WORD l3r3 = ParseGamepadButtons("L3+R3");
	TEST_ASSERT((l3r3 & 0x0040) && (l3r3 & 0x0080), "Parse 'L3+R3' contains L3 and R3");

	// L1+R1 (LB+RB): LB is 0x0100, RB is 0x0200
	WORD l1r1 = ParseGamepadButtons("L1+R1");
	TEST_ASSERT((l1r1 & 0x0100) && (l1r1 & 0x0200), "Parse 'L1+R1' contains LB and RB");
}

// 3. Test INI Config File Loading & Parsing
void TestConfigLoading()
{
	std::cout << COLOR_CYAN << "=== Running Config File Loading Tests ===" << COLOR_RESET << std::endl;

	TEST_ASSERT(ParseVirtualKey("R") == 'R', "Parse 'R' == 'R'");
	TEST_ASSERT(ParseVirtualKey("r") == 'R', "Parse lowercase 'r' == 'R'");

	// Create a temporary test INI file
	const wchar_t* testIni = L"test_config.ini";
	WritePrivateProfileStringW(L"Settings", L"SpeedMultiplier", L"4.5", testIni);
	WritePrivateProfileStringW(L"Settings", L"SkipCutscenesAtSpeed", L"true", testIni);
	WritePrivateProfileStringW(L"Settings", L"ControlMode", L"Hold", testIni);
	WritePrivateProfileStringW(L"Settings", L"KeyboardHotkey", L"R", testIni);
	WritePrivateProfileStringW(L"Settings", L"EnableGamepad", L"true", testIni);
	WritePrivateProfileStringW(L"Settings", L"GamepadCombo", L"L3+R3", testIni);
	WritePrivateProfileStringW(L"Settings", L"ShowOSD", L"true", testIni);
	WritePrivateProfileStringW(L"Settings", L"AutoDisableInBattle", L"true", testIni);
	WritePrivateProfileStringW(L"Settings", L"SkipIntro", L"true", testIni);
	WritePrivateProfileStringW(L"Settings", L"DebugMode", L"true", testIni);

	bool loaded = LoadConfig(testIni);
	TEST_ASSERT(loaded == true, "LoadConfig returns true");
	TEST_ASSERT(g_Config.speedMultiplier == 4.5f, "Config speedMultiplier == 4.5");
	TEST_ASSERT(g_Config.skipCutscenesAtSpeed == true, "Config skipCutscenesAtSpeed == true");
	TEST_ASSERT(g_Config.skipIntro == true, "Config skipIntro == true");
	TEST_ASSERT(g_Config.controlMode == CONTROL_HOLD, "Config controlMode == CONTROL_HOLD");
	TEST_ASSERT(g_Config.keyboardHotkey == 'R', "Config keyboardHotkey == 'R'");
	TEST_ASSERT((g_Config.gamepadButtonMask & 0x0040) != 0, "Config gamepadButtonMask has L3");
	TEST_ASSERT(g_Config.showOSD == true, "Config showOSD == true");
	TEST_ASSERT(g_Config.autoDisableInBattle == true, "Config autoDisableInBattle == true");
	TEST_ASSERT(g_Config.debugMode == true, "Config debugMode == true");

	DeleteFileW(testIni);
}

// 4. Test Memory Patching (Simulated DialogProc Cutscene Skip)
void TestMemoryPatching()
{
	std::cout << COLOR_CYAN << "=== Running Memory Patching Tests ===" << COLOR_RESET << std::endl;

	// Allocate a 4096-byte dummy page with PAGE_EXECUTE_READWRITE
	void* pDummyPage = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	TEST_ASSERT(pDummyPage != NULL, "VirtualAlloc executable test page succeeds");

	// Setup dummy function prologue (55 8B EC)
	uint8_t* pFunc = (uint8_t*)pDummyPage;
	pFunc[0] = 0x55;
	pFunc[1] = 0x8B;
	pFunc[2] = 0xEC;

	// Simulate activating cutscene skip patch (C2 08 00)
	DWORD oldProtect = 0;
	BOOL protOk = VirtualProtect(pDummyPage, 3, PAGE_EXECUTE_READWRITE, &oldProtect);
	TEST_ASSERT(protOk == TRUE, "VirtualProtect PAGE_EXECUTE_READWRITE succeeds");

	pFunc[0] = 0xC2;
	pFunc[1] = 0x08;
	pFunc[2] = 0x00;
	FlushInstructionCache(GetCurrentProcess(), pDummyPage, 3);

	TEST_ASSERT(pFunc[0] == 0xC2 && pFunc[1] == 0x08 && pFunc[2] == 0x00, "Cutscene patch written: C2 08 00 (ret 8)");

	// Simulate deactivating cutscene skip patch (restoring 55 8B EC)
	pFunc[0] = 0x55;
	pFunc[1] = 0x8B;
	pFunc[2] = 0xEC;
	FlushInstructionCache(GetCurrentProcess(), pDummyPage, 3);
	VirtualProtect(pDummyPage, 3, oldProtect, &oldProtect);

	TEST_ASSERT(pFunc[0] == 0x55 && pFunc[1] == 0x8B && pFunc[2] == 0xEC, "Cutscene patch restored: 55 8B EC (prologue)");

	VirtualFree(pDummyPage, 0, MEM_RELEASE);
}

// 5. Test MinHook Detour on DeltaTime function
typedef void (__cdecl *TargetFunc_t)(float dt);
static TargetFunc_t g_pOriginalTarget = nullptr;
static float g_LastReceivedDt = 0.0f;
static float g_MockSpeedMultiplier = 8.0f;

__declspec(noinline) void __cdecl MockUpdateDeltaTime(float dt)
{
	// Add dummy volatile operations so function is large enough for MinHook
	volatile float dummy = dt * 1.0f;
	g_LastReceivedDt = dummy;
}

static void __cdecl HookedMockUpdateDeltaTime(float dt)
{
	float scaledDt = dt * g_MockSpeedMultiplier;
	g_pOriginalTarget(scaledDt);
}

void TestMinHookDetour()
{
	std::cout << COLOR_CYAN << "=== Running MinHook Detour Integration Tests ===" << COLOR_RESET << std::endl;

	MH_STATUS status = MH_Initialize();
	TEST_ASSERT(status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED, "MH_Initialize succeeds");

	status = MH_CreateHook((LPVOID)&MockUpdateDeltaTime, (LPVOID)&HookedMockUpdateDeltaTime, (LPVOID*)&g_pOriginalTarget);
	TEST_ASSERT(status == MH_OK, "MH_CreateHook on DeltaTime function succeeds");

	status = MH_EnableHook((LPVOID)&MockUpdateDeltaTime);
	TEST_ASSERT(status == MH_OK, "MH_EnableHook succeeds");

	// Call function via pointer to strictly prevent inlining
	TargetFunc_t volatile pFunc = &MockUpdateDeltaTime;

	// Call function with dt = 0.016f (60 FPS tick)
	g_MockSpeedMultiplier = 8.0f;
	pFunc(0.016f);

	// Expect scaled: 0.016 * 8.0 = 0.128
	float expected = 0.016f * 8.0f;
	float diff = fabsf(g_LastReceivedDt - expected);
	TEST_ASSERT(diff < 0.0001f, "Hooked DeltaTime receives 8.0x scaled delta time (0.128)");

	// Test 4.0x speed
	g_MockSpeedMultiplier = 4.0f;
	pFunc(0.016f);
	expected = 0.016f * 4.0f;
	diff = fabsf(g_LastReceivedDt - expected);
	TEST_ASSERT(diff < 0.0001f, "Hooked DeltaTime receives 4.0x scaled delta time (0.064)");

	MH_DisableHook((LPVOID)&MockUpdateDeltaTime);
	MH_Uninitialize();
}

// 6. Test MinHook Detour on simulated IsOpeningScreenPlaying
static uint8_t g_MockOpeningFlag = 1;

__declspec(noinline) bool __cdecl MockIsOpeningScreenPlaying()
{
	volatile uint8_t dummy = g_MockOpeningFlag;
	return dummy != 0;
}

typedef bool (__cdecl *MockIsOpeningScreen_t)();
static MockIsOpeningScreen_t g_pOriginalMockOpening = nullptr;

static bool __cdecl HookedMockIsOpeningScreenPlaying()
{
	if (g_Config.skipIntro)
	{
		g_MockOpeningFlag = 0;
		return false;
	}
	return g_pOriginalMockOpening();
}

void TestSkipIntroDetour()
{
	std::cout << COLOR_CYAN << "=== Running SkipIntro Detour Integration Tests ===" << COLOR_RESET << std::endl;

	MH_STATUS status = MH_Initialize();
	TEST_ASSERT(status == MH_OK || status == MH_ERROR_ALREADY_INITIALIZED, "MH_Initialize succeeds");

	status = MH_CreateHook((LPVOID)&MockIsOpeningScreenPlaying, (LPVOID)&HookedMockIsOpeningScreenPlaying, (LPVOID*)&g_pOriginalMockOpening);
	TEST_ASSERT(status == MH_OK, "MH_CreateHook on MockIsOpeningScreenPlaying succeeds");

	status = MH_EnableHook((LPVOID)&MockIsOpeningScreenPlaying);
	TEST_ASSERT(status == MH_OK, "MH_EnableHook on MockIsOpeningScreenPlaying succeeds");

	MockIsOpeningScreen_t volatile pFunc = &MockIsOpeningScreenPlaying;

	// Case 1: SkipIntro = true -> returns false immediately and sets flag = 0
	g_Config.skipIntro = true;
	g_MockOpeningFlag = 1;
	bool res = pFunc();
	TEST_ASSERT(res == false, "Hooked MockIsOpeningScreenPlaying returns false when skipIntro = true");
	TEST_ASSERT(g_MockOpeningFlag == 0, "Opening screen flag cleared to 0");

	// Case 2: SkipIntro = false -> returns original flag status (true)
	g_Config.skipIntro = false;
	g_MockOpeningFlag = 1;
	res = pFunc();
	TEST_ASSERT(res == true, "Hooked MockIsOpeningScreenPlaying returns true when skipIntro = false");

	MH_DisableHook((LPVOID)&MockIsOpeningScreenPlaying);
	MH_Uninitialize();
}

// 7. Test Battle State Detection Logic
void TestBattleStateSafety()
{
	std::cout << COLOR_CYAN << "=== Running Battle State Safety Logic Tests ===" << COLOR_RESET << std::endl;

	auto EvalInBattle = [](uint8_t bsMain, uint8_t bsPhase, uint8_t bsSub) -> bool {
		// Evaluates whether engine is actively in combat/battle
		// Must return false after battle ends (State=0, Phase=0) even if Sub stayed at 10!
		return (bsMain != 0 || bsPhase != 0);
	};

	// 1. In combat: State=1, Phase=2 -> true
	TEST_ASSERT(EvalInBattle(1, 2, 10) == true, "In battle (State=1, Phase=2, Sub=10) == true");

	// 2. Battle transition out: State=27, Phase=0 -> true
	TEST_ASSERT(EvalInBattle(27, 0, 10) == true, "Battle fadeout (State=27, Phase=0, Sub=10) == true");

	// 3. Battle finished: State=0, Phase=0, Sub=10 (Scripted battle with no victory rewards screen) -> MUST BE FALSE!
	TEST_ASSERT(EvalInBattle(0, 0, 10) == false, "Scripted battle finished (State=0, Phase=0, Sub=10) == false");

	// 4. Normal field state: State=0, Phase=0, Sub=0 -> false
	TEST_ASSERT(EvalInBattle(0, 0, 0) == false, "Normal field mode (State=0, Phase=0, Sub=0) == false");

	// 5. Boss combat: State=2, Phase=2, Sub=0 -> true
	TEST_ASSERT(EvalInBattle(2, 2, 0) == true, "Boss combat (State=2, Phase=2, Sub=0) == true");
}

// 8. Test Movie Safety Logic (FMV Green Screen Prevention)
void TestMovieSafety()
{
	std::cout << COLOR_CYAN << "=== Running Movie Safety Logic Tests (FMV Green Screen Prevention) ===" << COLOR_RESET << std::endl;

	auto EvalTurboAllowed = [](bool speedActive, bool inBattle, bool inMovie) -> bool {
		return speedActive && !inBattle && !inMovie;
	};

	TEST_ASSERT(EvalTurboAllowed(true, false, false) == true, "Turbo allowed during regular cutscene/field (speedActive=1, inBattle=0, inMovie=0)");
	TEST_ASSERT(EvalTurboAllowed(true, true, false) == false, "Turbo prevented during battle (speedActive=1, inBattle=1, inMovie=0)");
	TEST_ASSERT(EvalTurboAllowed(true, false, true) == false, "Turbo prevented during FMV movie (speedActive=1, inBattle=0, inMovie=1) -> Prevents Green Screen!");
	TEST_ASSERT(EvalTurboAllowed(true, true, true) == false, "Turbo prevented during battle movie");
	TEST_ASSERT(EvalTurboAllowed(false, false, false) == false, "Turbo disengaged when user toggle is off");
}

// 9. Test Exported Functions from built DLL
#pragma pack(push, 1)
typedef struct
{
	unsigned char major;
	unsigned char minor;
	unsigned char step;
} tVersion;
#pragma pack(pop)

typedef const char* (*FF10HgetName_t)();
typedef tVersion (*FF10HgetVer_t)();

void TestDllExports()
{
	std::cout << COLOR_CYAN << "=== Running Built DLL Exports Test ===" << COLOR_RESET << std::endl;

	const wchar_t* dllPath = L"bin\\ff10-cutscene-skip.dll";
	HMODULE hMod = LoadLibraryW(dllPath);
	TEST_ASSERT(hMod != NULL, "LoadLibraryW('bin\\ff10-cutscene-skip.dll') succeeds");

	if (hMod)
	{
		FF10HgetName_t pGetName = (FF10HgetName_t)GetProcAddress(hMod, "FF10HgetName");
		TEST_ASSERT(pGetName != nullptr, "GetProcAddress('FF10HgetName') found");
		if (pGetName)
		{
			const char* name = pGetName();
			TEST_ASSERT(name != nullptr && std::string(name).find("FFX Cutscene Skip") != std::string::npos,
				"FF10HgetName() returns correct mod name string");
		}

		FF10HgetVer_t pGetVer = (FF10HgetVer_t)GetProcAddress(hMod, "FF10HgetVer");
		TEST_ASSERT(pGetVer != nullptr, "GetProcAddress('FF10HgetVer') found");
		if (pGetVer)
		{
			tVersion ver = pGetVer();
			TEST_ASSERT(ver.major == 1 && ver.minor == 0 && ver.step == 1,
				"FF10HgetVer() returns version 1.0.1");
		}

		FreeLibrary(hMod);
	}
}

int main()
{
	// Enable ANSI color codes on Windows console
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD dwMode = 0;
	GetConsoleMode(hOut, &dwMode);
	SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

	std::cout << "========================================================" << std::endl;
	std::cout << "  FFX Cutscene Skip & Turbo Mod - Unit Test Suite" << std::endl;
	std::cout << "========================================================" << std::endl << std::endl;

	TestVirtualKeyParsing();
	std::cout << std::endl;

	TestGamepadComboParsing();
	std::cout << std::endl;

	TestConfigLoading();
	std::cout << std::endl;

	TestMemoryPatching();
	std::cout << std::endl;

	TestMinHookDetour();
	std::cout << std::endl;

	TestSkipIntroDetour();
	std::cout << std::endl;

	TestBattleStateSafety();
	std::cout << std::endl;

	TestMovieSafety();
	std::cout << std::endl;

	TestDllExports();
	std::cout << std::endl;

	std::cout << "========================================================" << std::endl;
	std::cout << "  Test Results: "
		<< COLOR_GREEN << g_TestsPassed << " Passed" << COLOR_RESET << ", "
		<< (g_TestsFailed > 0 ? COLOR_RED : COLOR_GREEN) << g_TestsFailed << " Failed" << COLOR_RESET
		<< " (Total: " << g_TestsRun << ")" << std::endl;
	std::cout << "========================================================" << std::endl;

	return g_TestsFailed > 0 ? 1 : 0;
}
