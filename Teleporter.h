#pragma once
#include "SDKGlobal.h"
#include <vector>
#include <string>

struct CustomWaypoint {
    std::string Name;
    SDK::FVector Location;
    SDK::FRotator Rotation;
};

namespace Teleporter
{
    extern std::vector<CustomWaypoint> Waypoints;

    // [FIX] Flags for Game Thread Execution
    extern bool bTeleportPending;
    extern SDK::FVector TargetLocation;

    // Interface
    void AddWaypoint(SDK::APalPlayerCharacter* pLocal, const char* pName);
    void DeleteWaypoint(int index);

    // UI calls this (Queueing)
    void TeleportTo(SDK::APalPlayerCharacter* pLocal, SDK::FVector Location);

    // Game Thread calls this (Execution)
    void ProcessQueue();

    void TeleportToHome(SDK::APalPlayerCharacter* pLocal);
    void TeleportToBoss(SDK::APalPlayerCharacter* pLocal, int bossIndex);

    // Fixes LNK2005 error
    void Reset();
}