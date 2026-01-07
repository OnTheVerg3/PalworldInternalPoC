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

    // [FIX] State flags for Deferred Teleportation
    extern bool bTeleportPending;
    extern SDK::FVector TargetLocation;

    // Queues a teleport request (Safe to call from UI)
    void QueueTeleport(SDK::FVector Location);

    // Executes the queued teleport (Call from Hooking::hkPresent start)
    void ProcessQueue();

    void AddWaypoint(SDK::APalPlayerCharacter* pLocal, const char* pName);
    void DeleteWaypoint(int index);

    void TeleportToHome(SDK::APalPlayerCharacter* pLocal);
    void TeleportToBoss(SDK::APalPlayerCharacter* pLocal, int bossIndex);
}