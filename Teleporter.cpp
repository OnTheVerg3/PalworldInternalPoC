#include "Teleporter.h"
#include "Hooking.h"
#include <iostream>
#include <cstring>
#include <string> 
#include <algorithm> 

extern ID3D11DeviceContext* g_pd3dContext;
extern ID3D11RenderTargetView* g_mainRenderTargetView;
extern ULONGLONG g_TeleportCooldown;

namespace Teleporter
{
    std::vector<CustomWaypoint> Waypoints;
    bool bTeleportPending = false;
    bool bExecuteRPC = false;
    SDK::FVector TargetLocation = { 0, 0, 0 };

    void Reset() {
        bTeleportPending = false;
        bExecuteRPC = false;
        // Keep waypoints, just clear state
    }

    void CallFn(SDK::UObject* obj, const char* fnName, void* params = nullptr) {
        if (!IsValidObject(obj)) return;
        auto fn = SDK::UObject::FindObject<SDK::UFunction>(fnName);
        if (fn) obj->ProcessEvent(fn, params);
    }

    void QueueTeleport(SDK::FVector Location) {
        TargetLocation = Location;
        bTeleportPending = true;
    }

    void ProcessQueue_RenderThread() {
        if (!bTeleportPending) return;
        if (g_pd3dContext) {
            ID3D11RenderTargetView* nullViews[] = { nullptr };
            g_pd3dContext->OMSetRenderTargets(1, nullViews, nullptr);
        }
        if (g_mainRenderTargetView) {
            g_mainRenderTargetView->Release();
            g_mainRenderTargetView = nullptr;
        }
        g_TeleportCooldown = GetTickCount64() + 5000;
        bTeleportPending = false;
        bExecuteRPC = true;
        std::cout << "[Jarvis] D3D Released. Handoff to Game Thread." << std::endl;
    }

    void ProcessQueue_GameThread() {
        if (!bExecuteRPC) return;

        SDK::APalPlayerCharacter* pLocal = Hooking::GetLocalPlayerSafe();
        if (!IsValidObject(pLocal)) { bExecuteRPC = false; return; }

        SDK::APlayerController* PC = static_cast<SDK::APlayerController*>(pLocal->Controller);
        if (!IsValidObject(PC) || !IsValidObject(PC->PlayerState)) { bExecuteRPC = false; return; }

        SDK::APalPlayerController* PalPC = static_cast<SDK::APalPlayerController*>(PC);
        SDK::APalPlayerState* PalState = static_cast<SDK::APalPlayerState*>(PC->PlayerState);

        SDK::FVector SafeLoc = { TargetLocation.X, TargetLocation.Y, TargetLocation.Z + 100.0f };
        SDK::FQuat Rotation = { 0, 0, 0, 1 };

        struct {
            SDK::FGuid PlayerUId;
            SDK::FVector Location;
            SDK::FQuat Rotation;
        } RegisterParams;

        RegisterParams.PlayerUId = PalState->PlayerUId;
        RegisterParams.Location = SafeLoc;
        RegisterParams.Rotation = Rotation;

        if (IsValidObject(PalPC->Transmitter) && IsValidObject(PalPC->Transmitter->Player)) {
            CallFn(PalPC->Transmitter->Player, "Function Pal.PalNetworkPlayerComponent.RegisterRespawnPoint_ToServer", &RegisterParams);
            CallFn(PalPC, "Function Pal.PalPlayerController.TeleportToSafePoint_ToServer", nullptr);
            std::cout << "[Jarvis] Teleport RPC Sent." << std::endl;
        }
        bExecuteRPC = false;
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

    // [FIX] Scan for "BuildObjectBaseCamp" (Palbox) instead of just "BaseCampPoint"
    void TeleportToHome(SDK::APalPlayerCharacter* pLocal) {
        if (!SDK::UObject::GObjects) return;
        std::cout << "[Jarvis] Scanning for Base (Palbox)..." << std::endl;

        for (int i = 0; i < SDK::UObject::GObjects->Num(); i++) {
            SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
            if (!IsValidObject(Obj)) continue;

            if (Obj->IsA(SDK::AActor::StaticClass())) {
                std::string Name = Obj->GetName();
                std::transform(Name.begin(), Name.end(), Name.begin(), ::tolower);

                // Matches "PalBuildObjectBaseCampPoint" or derived classes
                if (Name.find("buildobjectbasecamppoint") != std::string::npos) {
                    SDK::AActor* Actor = static_cast<SDK::AActor*>(Obj);
                    SDK::FVector Loc = Actor->K2_GetActorLocation();

                    // Filter invalid 0,0,0 coordinates
                    if (abs(Loc.X) > 1.0f || abs(Loc.Y) > 1.0f) {
                        std::cout << "[Jarvis] Base Found: " << Name << std::endl;
                        QueueTeleport(Loc);
                        return;
                    }
                }
            }
        }
        std::cout << "[-] No Base Camp found." << std::endl;
    }

    void TeleportToBoss(SDK::APalPlayerCharacter* pLocal, int bossIndex) {
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