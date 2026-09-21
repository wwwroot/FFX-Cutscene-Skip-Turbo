#pragma once
#include <windows.h>
#include <cstdint>

bool InitSpeedHack(uintptr_t moduleBase);
void ShutdownSpeedHack();
void SetSpeedActive(bool active);
bool IsSpeedActive();
bool IsInBattle();
uint8_t GetBattleState();
uint8_t GetBattlePhase();
uint8_t GetBattleSub();
