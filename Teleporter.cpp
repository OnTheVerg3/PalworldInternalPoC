#include "Teleporter.h"
#include "Hooking.h"
#include <iostream>
#include <cstring>
#include <string> 
#include <algorithm> // For std::transform

// [FIX] Needed to release the D3D context before teleporting to prevent Error 80004004
extern ID3D11DeviceContext* g_pd3dContext;
extern ID3D11RenderTargetView* g_mainRenderTargetView;
extern ULONGLONG g_TeleportCooldown; // [FIX] Access the cooldown timer

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

        // [CRITICAL FIX] Set cooldown and release D3D resources
        g_TeleportCooldown = GetTickCount64() + 5000; // Block UI/RTV for 5 seconds

        if (g_pd3dContext) {
            ID3D11RenderTargetView* nullViews[] = { nullptr };
            g_pd3dContext->OMSetRenderTargets(1, nullViews, nullptr);
        }
        if (g_mainRenderTargetView) {
            g_mainRenderTargetView->Release();
            g_mainRenderTargetView = nullptr;
        }

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

        std::cout << "[Jarvis] Teleport executed. D3D Resources released." << std::endl;
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

    // [FIX] Case-Insensitive Search for Base Camp
    void TeleportToHome(SDK::APalPlayerCharacter* pLocal)
    {
        if (!SDK::UObject::GObjects) return;
        std::cout << "[Jarvis] Scanning for Base Camp..." << std::endl;

        for (int i = 0; i < SDK::UObject::GObjects->Num(); i++) {
            SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
            if (!IsValidObject(Obj)) continue;

            if (Obj->IsA(SDK::AActor::StaticClass())) {
                std::string ObjName = Obj->GetName();

                // Convert to lowercase for safer matching
                std::string LowerName = ObjName;
                std::transform(LowerName.begin(), LowerName.end(), LowerName.begin(), ::tolower);

                // Look for "basecamppoint" inside names like "BP_PalBaseCampPoint_C"
                if (LowerName.find("basecamppoint") != std::string::npos) {
                    SDK::AActor* BaseActor = static_cast<SDK::AActor*>(Obj);
                    SDK::FVector BaseLoc = BaseActor->K2_GetActorLocation();

                    if (BaseLoc.X != 0.0f && BaseLoc.Y != 0.0f) {
                        std::cout << "[Jarvis] Found Base: " << ObjName << std::endl;
                        TeleportTo(pLocal, BaseLoc);
                        return; // Teleport to the first one found
                    }
                }
            }
        }
        std::cout << "[-] No Base Camp found in loaded chunks." << std::endl;
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

        if (bossIndex >= 0 && bossIndex < 5) {
            TeleportTo(pLocal, Bosses[bossIndex].Loc);
        }
    }
}