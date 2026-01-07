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

    // [FIX] These were missing in your file, causing the build error
    extern bool bTeleportPending;
    extern SDK::FVector TargetLocation;

    void QueueTeleport(SDK::FVector Location);
    void ProcessQueue(); // Called by Hooking.cpp
    void Reset();

    void AddWaypoint(SDK::APalPlayerCharacter* pLocal, const char* pName);
    void DeleteWaypoint(int index);

    void TeleportToHome(SDK::APalPlayerCharacter* pLocal);
    void TeleportToBoss(SDK::APalPlayerCharacter* pLocal, int bossIndex);

    // Internal helper for direct execution if needed
    void TeleportTo(SDK::APalPlayerCharacter* pLocal, SDK::FVector Location);
}