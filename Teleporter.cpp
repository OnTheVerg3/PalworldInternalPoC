#include "Teleporter.h"
#include "Hooking.h"
#include <iostream>
#include <cstring>

namespace Teleporter
{
    std::vector<CustomWaypoint> Waypoints;

    // --- INTERNAL HELPERS ---
    void CallFn(SDK::UObject* obj, const char* fnName, void* params = nullptr) {
        if (!IsValidObject(obj)) return;

        // [FIX] Removed 'static' to find function dynamically every time.
        // This is safe for UI actions and prevents stale pointers.
        auto fn = SDK::UObject::FindObject<SDK::UFunction>(fnName);

        if (fn) obj->ProcessEvent(fn, params);
    }

    // --- MAIN TELEPORT LOGIC ---
    void TeleportTo(SDK::APalPlayerCharacter* pLocal, SDK::FVector Location)
    {
        if (!IsValidObject(pLocal)) return;

        SDK::APlayerController* PC = static_cast<SDK::APlayerController*>(pLocal->Controller);
        if (!IsValidObject(PC) || !IsValidObject(PC->PlayerState)) return;

        SDK::APalPlayerController* PalPC = static_cast<SDK::APalPlayerController*>(PC);
        SDK::APalPlayerState* PalState = static_cast<SDK::APalPlayerState*>(PC->PlayerState);

        // 1. Get Network Transmitter
        if (!IsValidObject(PalPC->Transmitter) || !IsValidObject(PalPC->Transmitter->Player)) {
            std::cout << "[-] Teleport Failed: Transmitter invalid." << std::endl;
            return;
        }
        auto* NetworkPlayer = PalPC->Transmitter->Player;

        // 2. Prepare Data
        SDK::FVector SafeLoc = { Location.X, Location.Y, Location.Z + 100.0f };
        SDK::FQuat Rotation = { 0, 0, 0, 1 };

        // 3. Send "Register Respawn Point" Packet
        struct {
            SDK::FGuid PlayerUId;
            SDK::FVector Location;
            SDK::FQuat Rotation;
        } RegisterParams;

        RegisterParams.PlayerUId = PalState->PlayerUId;
        RegisterParams.Location = SafeLoc;
        RegisterParams.Rotation = Rotation;

        CallFn(NetworkPlayer, "Function Pal.PalNetworkPlayerComponent.RegisterRespawnPoint_ToServer", &RegisterParams);

        // 4. Send "Teleport To Safe Point" Packet
        CallFn(PalPC, "Function Pal.PalPlayerController.TeleportToSafePoint_ToServer", nullptr);

        std::cout << "[Jarvis] Teleport sequence executed." << std::endl;
    }

    void AddWaypoint(SDK::APalPlayerCharacter* pLocal, const char* pName)
    {
        if (!IsValidObject(pLocal)) return;

        CustomWaypoint wp;
        wp.Name = pName;
        wp.Location = pLocal->K2_GetActorLocation();
        wp.Rotation = pLocal->K2_GetActorRotation();

        Waypoints.push_back(wp);
        std::cout << "[Jarvis] Waypoint saved: " << pName << std::endl;
    }

    void DeleteWaypoint(int index)
    {
        if (index >= 0 && index < Waypoints.size()) {
            Waypoints.erase(Waypoints.begin() + index);
        }
    }

    void TeleportToHome(SDK::APalPlayerCharacter* pLocal)
    {
        std::cout << "[Jarvis] Home teleport logic pending SDK integration." << std::endl;
    }

    void TeleportToBoss(SDK::APalPlayerCharacter* pLocal, int bossIndex)
    {
        static const struct { const char* Name; SDK::FVector Loc; } Bosses[] = {
            { "Zoe & Grizzbolt", { 1234.0f, 5678.0f, 0.0f } },
            { "Lily & Lyleen",   { 0.0f, 0.0f, 0.0f } }
        };

        if (bossIndex >= 0 && bossIndex < 2) {
            TeleportTo(pLocal, Bosses[bossIndex].Loc);
        }
    }
}