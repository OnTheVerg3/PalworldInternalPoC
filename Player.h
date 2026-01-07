#pragma once
#include "SDKGlobal.h"
#include <vector>
#include <string>

namespace Player
{
    struct PlayerCandidate {
        SDK::APalPlayerCharacter* Ptr;
        std::string DisplayString;
    };

    std::vector<PlayerCandidate> GetPlayerCandidates();

    // [FIX] Removed Attack variables
    extern bool bWeightAdjuster;
    extern float fWeightModifier;
    extern bool bUnlockMap;
    extern bool bUnlockTowers;
    extern bool bCollectRelics;

    void Update(SDK::APalPlayerCharacter* pLocal);
    void ProcessAttributes(SDK::APalPlayerCharacter* pLocal);
    void UnlockAllMap(SDK::APalPlayerCharacter* pLocal);
    void UnlockAllTowers(SDK::APalPlayerCharacter* pLocal);
    void TeleportRelicsToPlayer(SDK::APalPlayerCharacter* pLocal);
    void Reset();
}