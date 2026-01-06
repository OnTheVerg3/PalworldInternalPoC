#include "Player.h"
#include "Hooking.h" 
#include <iostream>
#include <vector>
#include <cstring> 
#include <algorithm>
#include <unordered_map>

// Access external safe flag
extern std::atomic<bool> g_bIsSafe;

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

    // --- STATE MANAGEMENT ---
    std::vector<SDK::APalLevelObjectObtainable*> g_RelicCache;
    ULONGLONG g_LastCollectTime = 0;
    const ULONGLONG COLLECTION_INTERVAL_MS = 250;

    std::unordered_map<std::string, SDK::UFunction*> g_PlayerFuncCache;

    // --- INTERNAL HELPERS ---

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
        if (it != g_PlayerFuncCache.end()) {
            fn = it->second;
        }
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
        std::cout << "[Player] Cache cleared and features reset." << std::endl;
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

            if (bCollectRelics) {
                TeleportRelicsToPlayer(pLocal);
            }
            else {
                if (!g_RelicCache.empty()) g_RelicCache.clear();
            }
        }
        __except (1) {
            // Logic exception (Exit detected)
        }
    }

    void ProcessAttributes(SDK::APalPlayerCharacter* pLocal)
    {
        if (bAttackMultiplier) {
            if (IsValidObject(pLocal->CharacterParameterComponent)) {
                pLocal->CharacterParameterComponent->AttackUp = (int32_t)(50 * fAttackModifier);
            }
        }

        if (bWeightAdjuster) {
            SDK::APlayerController* PC = static_cast<SDK::APlayerController*>(pLocal->Controller);
            if (IsValidObject(PC) && IsValidObject(PC->PlayerState)) {
                SDK::APalPlayerState* pState = static_cast<SDK::APalPlayerState*>(PC->PlayerState);

                if (IsValidObject(pState->InventoryData)) {
                    auto inv = pState->InventoryData;
                    inv->MaxInventoryWeight = fWeightModifier;

                    CallFn(inv, "Function Pal.PalPlayerInventoryData.OnRep_maxInventoryWeight");
                    CallFn(inv, "Function Pal.PalPlayerInventoryData.OnRep_BuffMaxWeight");
                    CallFn(inv, "Function Pal.PalPlayerInventoryData.OnRep_BuffCurrentWeight");

                    struct { bool bUnused; } params = { true };
                    CallFn(inv, "Function Pal.PalPlayerInventoryData.RequestForceMarkAllDirty_ToServer", &params);
                }
            }
        }
    }

    void UnlockAllMap(SDK::APalPlayerCharacter* pLocal)
    {
        if (!SDK::UObject::GObjects || IsGarbagePtr(*(void**)&SDK::UObject::GObjects)) return;
        std::cout << "[Jarvis] Revealing Map..." << std::endl;

        for (int i = 0; i < SDK::UObject::GObjects->Num(); i++) {
            SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
            if (!IsValidObject(Obj)) continue;

            char nameBuf[256];
            GetNameSafe(Obj, nameBuf, sizeof(nameBuf));
            if (strstr(nameBuf, "PalGameSetting") && !strstr(nameBuf, "Default__")) {
                SDK::UPalGameSetting* pSettings = static_cast<SDK::UPalGameSetting*>(Obj);
                pSettings->worldmapUIMaskClearSize = 20000.0f;
            }
        }
    }

    void UnlockAllTowers(SDK::APalPlayerCharacter* pLocal)
    {
        if (!SDK::UObject::GObjects || IsGarbagePtr(*(void**)&SDK::UObject::GObjects)) return;
        std::cout << "[Jarvis] Unlocking Fast Travel..." << std::endl;

        bool bIsAuthority = false;
        if (pLocal->Controller && IsValidObject(pLocal->Controller)) bIsAuthority = pLocal->Controller->HasAuthority();

        for (int i = 0; i < SDK::UObject::GObjects->Num(); i++) {
            SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
            if (!IsValidObject(Obj)) continue;

            if (IsClass(Obj, "PalLevelObjectUnlockableFastTravelPoint")) {
                auto* FT = static_cast<SDK::APalLevelObjectUnlockableFastTravelPoint*>(Obj);
                if (FT->bUnlocked) continue;

                if (bIsAuthority) {
                    FT->OnTriggerInteract(pLocal, SDK::EPalInteractiveObjectIndicatorType::UnlockFastTravel);
                }
                else {
                    SDK::APalPlayerController* PalPC = static_cast<SDK::APalPlayerController*>(pLocal->Controller);
                    if (IsValidObject(PalPC) && IsValidObject(PalPC->Transmitter) && IsValidObject(PalPC->Transmitter->Player)) {
                        PalPC->Transmitter->Player->RequestUnlockFastTravelPoint_ToServer(FT->FastTravelPointID);
                    }
                }
            }
        }
    }

    void TeleportRelicsToPlayer(SDK::APalPlayerCharacter* pLocal)
    {
        ULONGLONG CurrentTime = GetTickCount64();
        if (CurrentTime - g_LastCollectTime < COLLECTION_INTERVAL_MS) return;

        SDK::UWorld* World = SDK::UWorld::GetWorld();
        if (!IsValidObject(World)) return;

        SDK::APalPlayerController* PalPC = static_cast<SDK::APalPlayerController*>(pLocal->Controller);
        if (!IsValidObject(PalPC) || !IsValidObject(PalPC->Transmitter) || !IsValidObject(PalPC->Transmitter->Player)) return;

        if (g_RelicCache.empty()) {
            std::cout << "[Jarvis] Single-Pass Scan initiated..." << std::endl;
            if (SDK::UObject::GObjects && !IsGarbagePtr(*(void**)&SDK::UObject::GObjects)) {
                for (int i = 0; i < SDK::UObject::GObjects->Num(); i++) {
                    SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
                    if (!IsValidObject(Obj)) continue;

                    if (IsClass(Obj, "PalLevelObjectRelic") || IsClass(Obj, "BP_LevelObject_Relic_C")) {
                        SDK::APalLevelObjectObtainable* Relic = static_cast<SDK::APalLevelObjectObtainable*>(Obj);
                        if (!Relic->bPickedInClient) {
                            g_RelicCache.push_back(Relic);
                        }
                    }
                }
            }

            std::cout << "[Jarvis] Scan Complete. Found: " << g_RelicCache.size() << std::endl;

            if (g_RelicCache.empty()) {
                std::cout << "[Jarvis] No relics found. Disabling." << std::endl;
                bCollectRelics = false;
                return;
            }
        }

        auto it = g_RelicCache.begin();
        while (it != g_RelicCache.end()) {
            SDK::APalLevelObjectObtainable* Relic = *it;

            if (!IsValidObject(Relic) || Relic->bPickedInClient) {
                it = g_RelicCache.erase(it);
                continue;
            }

            if (IsValidObject(pLocal->InteractComponent)) {
                pLocal->InteractComponent->SetEnableInteract(true, false);
                PalPC->Transmitter->Player->RequestObtainLevelObject_ToServer(Relic);
                std::cout << "[Jarvis] Collecting Relic (" << g_RelicCache.size() - 1 << " remaining)" << std::endl;
            }

            it = g_RelicCache.erase(it);
            g_LastCollectTime = CurrentTime;
            break;
        }

        if (g_RelicCache.empty()) {
            std::cout << "[Jarvis] Job complete. Disabling vacuum." << std::endl;
            bCollectRelics = false;
        }
    }
}