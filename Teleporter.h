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

    // Simplified Interface
    void AddWaypoint(SDK::APalPlayerCharacter* pLocal, const char* pName);
    void DeleteWaypoint(int index);

    // Direct Teleport Actions
    void TeleportTo(SDK::APalPlayerCharacter* pLocal, SDK::FVector Location);
    void TeleportToHome(SDK::APalPlayerCharacter* pLocal);
    void TeleportToBoss(SDK::APalPlayerCharacter* pLocal, int bossIndex);

    void Reset() {} // No state to reset now
}