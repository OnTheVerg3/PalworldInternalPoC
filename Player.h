#pragma once
#include "SDKGlobal.h"
#include <vector>
#include <string>

namespace Player
{
    // [NEW] Struct for UI List
    struct PlayerCandidate {
        SDK::APalPlayerCharacter* Ptr;
        std::string DisplayString; // "ControllerName - PlayerName"
    };

    // [NEW] Scans for all valid player characters
    std::vector<PlayerCandidate> GetPlayerCandidates();

    extern bool bAttackMultiplier;
    extern float fAttackModifier;
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