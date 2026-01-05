#pragma once
#include <vector>
#include <string>
#include "SDKGlobal.h"

struct CustomWaypoint {
    std::string Name;
    SDK::FVector Location;
    SDK::FRotator Rotation;
};

namespace Teleporter
{
    extern std::vector<CustomWaypoint> Waypoints;

    // Core Function (The PalGuard Bypass)
    void TeleportTo(SDK::APalPlayerCharacter* pLocal, SDK::FVector Location);

    // Waypoint Management
    void AddWaypoint(SDK::APalPlayerCharacter* pLocal, const char* pName);
    void DeleteWaypoint(int index);

    // Preset Teleports
    void TeleportToHome(SDK::APalPlayerCharacter* pLocal); // Todo: Logic to find base
    void TeleportToBoss(SDK::APalPlayerCharacter* pLocal, int bossIndex);
}