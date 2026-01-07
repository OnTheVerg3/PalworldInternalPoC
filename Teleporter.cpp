#include "Teleporter.h"
#include "Hooking.h"
#include <iostream>
#include <cstring>
#include <string> 

// Need access to D3D resources to free them before heavy teleports
extern ID3D11RenderTargetView* g_mainRenderTargetView;

namespace Teleporter
{
    std::vector<CustomWaypoint> Waypoints;

    void CallFn(SDK::UObject* obj, const char* fnName, void* params = nullptr) {
        if (!IsValidObject(obj)) return;
        auto fn = SDK::UObject::FindObject<SDK::UFunction>(fnName);
        if (fn) obj->ProcessEvent(fn, params);
    }

    void TeleportTo(SDK::APalPlayerCharacter* pLocal, SDK::FVector Location)
    {
        if (!IsValidObject(pLocal)) return;

        SDK::APlayerController* PC = static_cast<SDK::APlayerController*>(pLocal->Controller);
        if (!IsValidObject(PC) || !IsValidObject(PC->PlayerState)) return;

        SDK::APalPlayerController* PalPC = static_cast<SDK::APalPlayerController*>(PC);
        SDK::APalPlayerState* PalState = static_cast<SDK::APalPlayerState*>(PC->PlayerState);

        if (!IsValidObject(PalPC->Transmitter) || !IsValidObject(PalPC->Transmitter->Player)) return;

        // [FIX] Release D3D Resources BEFORE teleporting.
        // TeleportToSafePoint triggers a loading screen/viewport reset.
        // If we hold the BackBuffer, the engine crashes (Error 80004004).
        if (g_mainRenderTargetView) {
            g_mainRenderTargetView->Release();
            g_mainRenderTargetView = nullptr;
        }

        // Coordinates are approx. Scale 100 assumed or raw units.
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

        // Register safe point then teleport to it
        CallFn(PalPC->Transmitter->Player, "Function Pal.PalNetworkPlayerComponent.RegisterRespawnPoint_ToServer", &RegisterParams);
        CallFn(PalPC, "Function Pal.PalPlayerController.TeleportToSafePoint_ToServer", nullptr);

        std::cout << "[Jarvis] Teleporting..." << std::endl;
    }

    void AddWaypoint(SDK::APalPlayerCharacter* pLocal, const char* pName)
    {
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
        std::cout << "[Jarvis] Searching for Base Camp..." << std::endl;

        for (int i = 0; i < SDK::UObject::GObjects->Num(); i++) {
            SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
            if (!IsValidObject(Obj)) continue;

            if (Obj->IsA(SDK::AActor::StaticClass())) {
                std::string ObjName = Obj->GetName();
                // [FIX] Relaxed search string to find ANY base point
                if (ObjName.find("BaseCampPoint") != std::string::npos) {
                    SDK::AActor* BaseActor = static_cast<SDK::AActor*>(Obj);
                    SDK::FVector BaseLoc = BaseActor->K2_GetActorLocation();

                    // Basic sanity check on coords (ignore 0,0,0)
                    if (BaseLoc.X != 0.0f && BaseLoc.Y != 0.0f) {
                        std::cout << "[Jarvis] Found Base: " << ObjName << std::endl;
                        TeleportTo(pLocal, BaseLoc);
                        return;
                    }
                }
            }
        }
        std::cout << "[-] No Base Camp found." << std::endl;
    }

    void TeleportToBoss(SDK::APalPlayerCharacter* pLocal, int bossIndex)
    {
        // [FIX] Real Coordinates (Approximate based on map data)
        static const struct { const char* Name; SDK::FVector Loc; } Bosses[] = {
            { "Zoe & Grizzbolt", { 11200.0f, -43400.0f, 0.0f } },
            { "Lily & Lyleen",   { 18500.0f, 2800.0f, 0.0f } },
            { "Axel & Orserk",   { -58800.0f, -51800.0f, 0.0f } },
            { "Marcus & Faleris", { 56100.0f, 33400.0f, 0.0f } },
            { "Victor & Shadowbeak", { -14900.0f, 44500.0f, 0.0f } }
        };

        if (bossIndex >= 0 && bossIndex < 5) {
            TeleportTo(pLocal, Bosses[bossIndex].Loc);
        }
    }
}