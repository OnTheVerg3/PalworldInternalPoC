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

    // Flags for Thread Synchronization
    extern bool bTeleportPending;   // Set by UI (Render Thread)
    extern bool bExecuteRPC;        // Set by Render Thread -> Read by Game Thread
    extern SDK::FVector TargetLocation;

    // Called by UI to start the chain
    void QueueTeleport(SDK::FVector Location);

    // Called by Render Thread (hkPresent) to clean D3D
    void ProcessQueue_RenderThread();

    // Called by Game Thread (hkProcessEvent) to fire RPCs
    void ProcessQueue_GameThread();

    void AddWaypoint(SDK::APalPlayerCharacter* pLocal, const char* pName);
    void DeleteWaypoint(int index);

    void TeleportToHome(SDK::APalPlayerCharacter* pLocal);
    void TeleportToBoss(SDK::APalPlayerCharacter* pLocal, int bossIndex);
}