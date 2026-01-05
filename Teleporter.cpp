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
        static auto fn = SDK::UObject::FindObject<SDK::UFunction>(fnName);
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
        // Offset Z to prevent falling through floor
        SDK::FVector SafeLoc = { Location.X, Location.Y, Location.Z + 100.0f };
        SDK::FQuat Rotation = { 0, 0, 0, 1 }; // Identity Quaternion

        // 3. Send "Register Respawn Point" Packet
        // Function Signature: void RegisterRespawnPoint_ToServer(FGuid PlayerUId, FVector Location, FQuat Rotation);
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
        // This triggers the server to move us to the point we just registered
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
        // Placeholder: Needs logic to iterate Base Camps or Pal Workers
        // For now, we can just log or implement if you have the BaseCamp SDK structs.
        std::cout << "[Jarvis] Home teleport logic pending SDK integration." << std::endl;
    }

    void TeleportToBoss(SDK::APalPlayerCharacter* pLocal, int bossIndex)
    {
        // Example Boss Coordinates (You can expand this list)
        static const struct { const char* Name; SDK::FVector Loc; } Bosses[] = {
            { "Zoe & Grizzbolt", { 1234.0f, 5678.0f, 0.0f } }, // Replace with real coords
            { "Lily & Lyleen",   { 0.0f, 0.0f, 0.0f } }
        };

        if (bossIndex >= 0 && bossIndex < 2) {
            TeleportTo(pLocal, Bosses[bossIndex].Loc);
        }
    }
}