#include "Teleporter.h"
#include "Hooking.h"
#include <iostream>
#include <cstring>
#include <string> 
#include <algorithm> 

namespace Teleporter
{
    std::vector<CustomWaypoint> Waypoints;
    bool bTeleportPending = false;
    SDK::FVector TargetLocation = { 0, 0, 0 };

    void Reset() { bTeleportPending = false; }

    void CallFn(SDK::UObject* obj, const char* fnName, void* params = nullptr) {
        if (!IsValidObject(obj)) return;
        auto fn = SDK::UObject::FindObject<SDK::UFunction>(fnName);
        if (fn) obj->ProcessEvent(fn, params);
    }

    // [FIX] UI calls this to safely queue the action
    void QueueTeleport(SDK::FVector Location) {
        TargetLocation = Location;
        bTeleportPending = true;
        std::cout << "[Jarvis] Teleport Queued." << std::endl;
    }

    // [FIX] Executed by Hooking.cpp on the Game Thread
    void ProcessQueue() {
        if (!bTeleportPending) return;

        // Execute the teleport
        SDK::APalPlayerCharacter* pLocal = Hooking::GetLocalPlayerSafe();
        if (pLocal) TeleportTo(pLocal, TargetLocation);

        bTeleportPending = false;
    }

    void TeleportTo(SDK::APalPlayerCharacter* pLocal, SDK::FVector Location)
    {
        if (!IsValidObject(pLocal)) return;

        SDK::APlayerController* PC = static_cast<SDK::APlayerController*>(pLocal->Controller);
        if (!IsValidObject(PC) || !IsValidObject(PC->PlayerState)) return;

        SDK::APalPlayerController* PalPC = static_cast<SDK::APalPlayerController*>(PC);
        SDK::APalPlayerState* PalState = static_cast<SDK::APalPlayerState*>(PC->PlayerState);

        if (!IsValidObject(PalPC->Transmitter) || !IsValidObject(PalPC->Transmitter->Player)) return;

        SDK::FVector SafeLoc = { Location.X, Location.Y, Location.Z + 100.0f };
        SDK::FQuat Rotation = { 0, 0, 0, 1 };

        struct {
            SDK::FGuid PlayerUId;
            SDK::FVector Location;
            SDK::FQuat Rotation;
        } RegisterParams;

        RegisterParams.PlayerUId = PalState->PlayerUId;
        RegisterParams.Location = SafeLoc;
        RegisterParams.Rotation = Rotation;

        CallFn(PalPC->Transmitter->Player, "Function Pal.PalNetworkPlayerComponent.RegisterRespawnPoint_ToServer", &RegisterParams);
        CallFn(PalPC, "Function Pal.PalPlayerController.TeleportToSafePoint_ToServer", nullptr);

        std::cout << "[Jarvis] Teleport Executed." << std::endl;
    }

    void AddWaypoint(SDK::APalPlayerCharacter* pLocal, const char* pName) {
        if (!IsValidObject(pLocal)) return;
        CustomWaypoint wp;
        wp.Name = pName;
        wp.Location = pLocal->K2_GetActorLocation();
        wp.Rotation = pLocal->K2_GetActorRotation();
        Waypoints.push_back(wp);
    }

    void DeleteWaypoint(int index) {
        if (index >= 0 && index < Waypoints.size()) Waypoints.erase(Waypoints.begin() + index);
    }

    void TeleportToHome(SDK::APalPlayerCharacter* pLocal)
    {
        if (!SDK::UObject::GObjects) return;
        std::cout << "[Jarvis] Scanning for Base..." << std::endl;

        for (int i = 0; i < SDK::UObject::GObjects->Num(); i++) {
            SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
            if (!IsValidObject(Obj)) continue;

            if (Obj->IsA(SDK::AActor::StaticClass())) {
                std::string Name = Obj->GetName();
                std::transform(Name.begin(), Name.end(), Name.begin(), ::tolower);

                if (Name.find("basecamppoint") != std::string::npos && Name.find("ui") == std::string::npos) {
                    SDK::AActor* Actor = static_cast<SDK::AActor*>(Obj);
                    SDK::FVector Loc = Actor->K2_GetActorLocation();

                    if (abs(Loc.X) > 1.0f) {
                        std::cout << "[Jarvis] Valid Base Found: " << Name << std::endl;
                        QueueTeleport(Loc);
                        return;
                    }
                }
            }
        }
        std::cout << "[-] No Base Camp found." << std::endl;
    }

    void TeleportToBoss(SDK::APalPlayerCharacter* pLocal, int bossIndex)
    {
        static const struct { const char* Name; SDK::FVector Loc; } Bosses[] = {
            { "Zoe & Grizzbolt", { 11200.0f, -43400.0f, 0.0f } },
            { "Lily & Lyleen",   { 18500.0f, 2800.0f, 0.0f } },
            { "Axel & Orserk",   { -58800.0f, -51800.0f, 0.0f } },
            { "Marcus & Faleris", { 56100.0f, 33400.0f, 0.0f } },
            { "Victor & Shadowbeak", { -14900.0f, 44500.0f, 0.0f } }
        };
        if (bossIndex >= 0 && bossIndex < 5) QueueTeleport(Bosses[bossIndex].Loc);
    }
}