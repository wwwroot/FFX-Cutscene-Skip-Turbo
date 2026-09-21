#include "config.h"
#include "input.h"
#include "module.h"
#include "osd.h"
#include "speedhack.h"
#include <filesystem>
#include <windows.h>

static HMODULE g_hModule = NULL;

extern "C" {
__declspec(dllexport) const char *FF10HgetName() {
  static const char s_ModuleName[] = "FFX Cutscene Skip & Turbo";
  return s_ModuleName;
}

__declspec(dllexport) tVersion FF10HgetVer() { return {1, 0, 0}; }
}

static DWORD WINAPI InitThread(LPVOID lpParam) {
  // Resolve paths
  wchar_t modulePath[MAX_PATH] = {0};
  GetModuleFileNameW(g_hModule, modulePath, MAX_PATH);
  std::filesystem::path dllPath(modulePath);
  std::filesystem::path moduleDir = dllPath.parent_path();

  // Check if config exists in modules/config/ff10-cutscene-skip.ini
  std::filesystem::path configPath =
      moduleDir / L"config" / L"ff10-cutscene-skip.ini";
  if (!std::filesystem::exists(configPath)) {
    // Fallback to same directory
    configPath = moduleDir / L"ff10-cutscene-skip.ini";
  }

  LogMessage("=== Initializing FFX Cutscene Skip & Turbo Mod ===");
  LogMessage("Module path: %ls", dllPath.c_str());
  LogMessage("Config path: %ls", configPath.c_str());

  LoadConfig(configPath);

  // Get base address of FFX.exe
  uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
  LogMessage("Game base address: 0x%p", (void *)base);

  // Wait up to 5 seconds if game is still unpacking / initializing
  for (int i = 0; i < 50; ++i) {
    if (base != 0)
      break;
    Sleep(100);
    base = (uintptr_t)GetModuleHandleA(NULL);
  }

  if (!base) {
    LogMessage("Failed to obtain game module base address!");
    return 1;
  }

  if (!InitSpeedHack(base)) {
    LogMessage("Failed to initialize speed hack / cutscene skip hooks!");
    return 1;
  }

  StartInputThread();
  if (g_Config.showOSD) {
    InitOSD();
  }
  LogMessage("FFX Cutscene Skip & Turbo successfully activated!");
  return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call,
                      LPVOID lpReserved) {
  switch (ul_reason_for_call) {
  case DLL_PROCESS_ATTACH:
    g_hModule = hModule;
    DisableThreadLibraryCalls(hModule);
    CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
    break;

  case DLL_PROCESS_DETACH:
    ShutdownOSD();
    StopInputThread();
    ShutdownSpeedHack();
    break;
  }
  return TRUE;
}
