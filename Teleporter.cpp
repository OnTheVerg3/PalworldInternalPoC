#include "Teleporter.h"
#include "Hooking.h"
#include <iostream>
#include <cstring>
#include <string> 

namespace Teleporter
{
    std::vector<CustomWaypoint> Waypoints;

    // --- INTERNAL HELPERS ---
    void CallFn(SDK::UObject* obj, const char* fnName, void* params = nullptr) {
        if (!IsValidObject(obj)) return;
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

        if (!IsValidObject(PalPC->Transmitter) || !IsValidObject(PalPC->Transmitter->Player)) {
            std::cout << "[-] Teleport Failed: Transmitter invalid." << std::endl;
            return;
        }
        auto* NetworkPlayer = PalPC->Transmitter->Player;

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

        CallFn(NetworkPlayer, "Function Pal.PalNetworkPlayerComponent.RegisterRespawnPoint_ToServer", &RegisterParams);
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

    // [FIX] Implemented robust Base Teleporter without strict class dependency
    void TeleportToHome(SDK::APalPlayerCharacter* pLocal)
    {
        if (!SDK::UObject::GObjects || IsGarbagePtr(*(void**)&SDK::UObject::GObjects)) return;

        std::cout << "[Jarvis] Scanning for Base Camp..." << std::endl;

        for (int i = 0; i < SDK::UObject::GObjects->Num(); i++) {
            SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
            if (!IsValidObject(Obj)) continue;

            // Check if it's an Actor first
            if (Obj->IsA(SDK::AActor::StaticClass())) {
                std::string ObjName = Obj->GetName();

                // Flexible check for Base Camp Point
                if (ObjName.find("PalLevelObjectBaseCampPoint") != std::string::npos) {

                    // Safe cast to AActor to get location
                    SDK::AActor* BaseActor = static_cast<SDK::AActor*>(Obj);
                    SDK::FVector BaseLoc = BaseActor->K2_GetActorLocation();

                    BaseLoc.Z += 150.0f;

                    std::cout << "[Jarvis] Base found: " << ObjName << ". Teleporting..." << std::endl;
                    TeleportTo(pLocal, BaseLoc);
                    return;
                }
            }
        }
        std::cout << "[-] No Base Camp found in range." << std::endl;
    }

    void TeleportToBoss(SDK::APalPlayerCharacter* pLocal, int bossIndex)
    {
        static const struct { const char* Name; SDK::FVector Loc; } Bosses[] = {
            { "Zoe & Grizzbolt", { 1234.0f, 5678.0f, 0.0f } }, // Needs real coords
            { "Lily & Lyleen",   { 0.0f, 0.0f, 0.0f } }
        };

        if (bossIndex >= 0 && bossIndex < 2) {
            TeleportTo(pLocal, Bosses[bossIndex].Loc);
        }
    }
}