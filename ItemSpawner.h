#pragma once
#include "SDKGlobal.h"

namespace ItemSpawner {
    void DrawTab();

    // [FIX] Now takes the Player Character directly to find the correct Inventory
    void Spawn_Method1(SDK::APalPlayerCharacter* pLocal, const char* ItemID, int32_t Count);
    void Spawn_Method2(SDK::APalPlayerCharacter* pLocal, const char* ItemID, int32_t Count);
}