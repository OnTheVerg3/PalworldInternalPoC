#pragma once
#include "SDKGlobal.h"

namespace Player
{
    // --- CONFIGURATION ---
    extern bool bAttackMultiplier;
    extern float fAttackModifier;

    extern bool bWeightAdjuster;
    extern float fWeightModifier;

    extern bool bUnlockMap;
    extern bool bUnlockTowers;
    extern bool bCollectRelics;

    // --- INTERFACE ---
    void Update(SDK::APalPlayerCharacter* pLocal);

    // [NEW] Wipes cache on world transition
    void Reset();

    // --- INTERNAL HELPERS ---
    void ProcessAttributes(SDK::APalPlayerCharacter* pLocal);
    void UnlockAllMap(SDK::APalPlayerCharacter* pLocal);
    void UnlockAllTowers(SDK::APalPlayerCharacter* pLocal);
    void TeleportRelicsToPlayer(SDK::APalPlayerCharacter* pLocal);
}