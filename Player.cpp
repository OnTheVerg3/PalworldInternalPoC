#include "Player.h"
#include "Hooking.h"
#include <iostream>
#include <vector>
#include <cstring> 
#include <algorithm>
#include <unordered_map>

namespace Player
{
    // --- DEFAULTS ---
    bool bAttackMultiplier = false;
    float fAttackModifier = 2.0f;

    bool bWeightAdjuster = false;
    float fWeightModifier = 50000.0f;

    bool bUnlockMap = false;
    bool bUnlockTowers = false;
    bool bCollectRelics = false;

    // --- STATE ---
    std::vector<SDK::APalLevelObjectObtainable*> g_RelicCache;
    ULONGLONG g_LastCollectTime = 0;
    const ULONGLONG COLLECTION_INTERVAL_MS = 250;

    std::unordered_map<std::string, SDK::UFunction*> g_PlayerFuncCache;

    // --- HELPERS ---
    bool IsClass(SDK::UObject* Obj, const char* ClassName) {
        if (!IsValidObject(Obj)) return false;
        char nameBuf[256];
        GetNameSafe(Obj, nameBuf, sizeof(nameBuf));
        return (strstr(nameBuf, ClassName) != nullptr);
    }

    void CallFn(SDK::UObject* obj, const char* fnName, void* params = nullptr) {
        if (!IsValidObject(obj)) return;
        SDK::UFunction* fn = nullptr;
        auto it = g_PlayerFuncCache.find(fnName);
        if (it != g_PlayerFuncCache.end()) fn = it->second;
        else {
            fn = SDK::UObject::FindObject<SDK::UFunction>(fnName);
            if (fn) g_PlayerFuncCache[fnName] = fn;
        }
        if (fn && IsValidObject(fn)) obj->ProcessEvent(fn, params);
    }

    void Reset() {
        g_RelicCache.clear();
        g_PlayerFuncCache.clear();
        bCollectRelics = false;
        std::cout << "[Player] Cache cleared." << std::endl;
    }

    void Update(SDK::APalPlayerCharacter* pLocal)
    {
        if (!g_bIsSafe || !IsValidObject(pLocal)) return;

        __try {
            ProcessAttributes(pLocal);

            if (bUnlockMap) {
                UnlockAllMap(pLocal);
                bUnlockMap = false;
            }

            if (bUnlockTowers) {
                UnlockAllTowers(pLocal);
                bUnlockTowers = false;
            }

            if (bCollectRelics) TeleportRelicsToPlayer(pLocal);
            else if (!g_RelicCache.empty()) g_RelicCache.clear();
        }
        __except (1) {}
    }

    void ProcessAttributes(SDK::APalPlayerCharacter* pLocal)
    {
        // Attack
        if (bAttackMultiplier && IsValidObject(pLocal->CharacterParameterComponent)) {
            pLocal->CharacterParameterComponent->AttackUp = (int32_t)(50 * fAttackModifier);
        }

        // Weight
        SDK::APlayerController* PC = static_cast<SDK::APlayerController*>(pLocal->Controller);
        if (IsValidObject(PC) && IsValidObject(PC->PlayerState)) {
            SDK::APalPlayerState* pState = static_cast<SDK::APalPlayerState*>(PC->PlayerState);
            if (IsValidObject(pState->InventoryData)) {
                auto inv = pState->InventoryData;

                if (bWeightAdjuster) {
                    inv->MaxInventoryWeight = fWeightModifier;
                }
                else {
                    // [FIX] Reset to a reasonable default if disabled
                    if (inv->MaxInventoryWeight > 5000.0f) inv->MaxInventoryWeight = 500.0f;
                }
            }
        }
    }

    void UnlockAllMap(SDK::APalPlayerCharacter* pLocal)
    {
        if (!SDK::UObject::GObjects || IsGarbagePtr(*(void**)&SDK::UObject::GObjects)) return;

        for (int i = 0; i < SDK::UObject::GObjects->Num(); i++) {
            SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
            if (!IsValidObject(Obj)) continue;

            char nameBuf[256];
            GetNameSafe(Obj, nameBuf, sizeof(nameBuf));
            if (strstr(nameBuf, "PalGameSetting") && !strstr(nameBuf, "Default__")) {
                SDK::UPalGameSetting* pSettings = static_cast<SDK::UPalGameSetting*>(Obj);
                pSettings->worldmapUIMaskClearSize = 99999.0f; // Max out clear radius
            }
        }
        std::cout << "[Jarvis] Map revealed." << std::endl;
    }

    void UnlockAllTowers(SDK::APalPlayerCharacter* pLocal)
    {
        if (!SDK::UObject::GObjects) return;
        std::cout << "[Jarvis] Unlocking Fast Travel..." << std::endl;

        SDK::APalPlayerController* PalPC = static_cast<SDK::APalPlayerController*>(pLocal->Controller);
        if (!IsValidObject(PalPC) || !IsValidObject(PalPC->Transmitter) || !IsValidObject(PalPC->Transmitter->Player)) return;

        for (int i = 0; i < SDK::UObject::GObjects->Num(); i++) {
            SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
            if (!IsValidObject(Obj)) continue;

            if (IsClass(Obj, "PalLevelObjectUnlockableFastTravelPoint")) {
                auto* FT = static_cast<SDK::APalLevelObjectUnlockableFastTravelPoint*>(Obj);
                if (!FT->bUnlocked) {
                    // Send request for every found point
                    PalPC->Transmitter->Player->RequestUnlockFastTravelPoint_ToServer(FT->FastTravelPointID);
                }
            }
        }
    }

    void TeleportRelicsToPlayer(SDK::APalPlayerCharacter* pLocal)
    {
        ULONGLONG CurrentTime = GetTickCount64();
        if (CurrentTime - g_LastCollectTime < COLLECTION_INTERVAL_MS) return;

        SDK::APalPlayerController* PalPC = static_cast<SDK::APalPlayerController*>(pLocal->Controller);
        if (!IsValidObject(PalPC) || !IsValidObject(PalPC->Transmitter) || !IsValidObject(PalPC->Transmitter->Player)) return;

        // Build Cache
        if (g_RelicCache.empty()) {
            if (SDK::UObject::GObjects) {
                for (int i = 0; i < SDK::UObject::GObjects->Num(); i++) {
                    SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
                    if (!IsValidObject(Obj)) continue;

                    if (IsClass(Obj, "PalLevelObjectRelic")) {
                        SDK::APalLevelObjectObtainable* Relic = static_cast<SDK::APalLevelObjectObtainable*>(Obj);
                        if (!Relic->bPickedInClient) g_RelicCache.push_back(Relic);
                    }
                }
            }
            if (g_RelicCache.empty()) { bCollectRelics = false; return; }
        }

        // Collect One
        auto it = g_RelicCache.begin();
        if (it != g_RelicCache.end()) {
            SDK::APalLevelObjectObtainable* Relic = *it;
            if (IsValidObject(Relic) && !Relic->bPickedInClient) {
                PalPC->Transmitter->Player->RequestObtainLevelObject_ToServer(Relic);
            }
            g_RelicCache.erase(it);
            g_LastCollectTime = CurrentTime;
        }
        else {
            bCollectRelics = false;
        }
    }
}