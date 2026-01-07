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
    extern bool bTeleportPending;
    extern bool bExecuteRPC;
    extern SDK::FVector TargetLocation;

    void QueueTeleport(SDK::FVector Location);
    void ProcessQueue_RenderThread();
    void ProcessQueue_GameThread();

    // [FIX] Reset flags on world exit
    void Reset();

    void AddWaypoint(SDK::APalPlayerCharacter* pLocal, const char* pName);
    void DeleteWaypoint(int index);
    void TeleportToHome(SDK::APalPlayerCharacter* pLocal);
    void TeleportToBoss(SDK::APalPlayerCharacter* pLocal, int bossIndex);
}