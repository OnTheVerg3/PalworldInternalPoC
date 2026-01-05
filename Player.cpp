#include "Player.h"
#include "Hooking.h" 
#include <iostream>
#include <vector>
#include <cstring> 
#include <algorithm>

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
    // [FIX] These are defined exactly ONCE here.
    std::vector<SDK::APalLevelObjectObtainable*> g_RelicCache;
    ULONGLONG g_LastCollectTime = 0;
    const ULONGLONG COLLECTION_INTERVAL_MS = 250;

    // --- INTERNAL HELPERS ---

    bool IsClass(SDK::UObject* Obj, const char* ClassName) {
        if (!IsValidObject(Obj)) return false;
        char nameBuf[256];
        GetNameSafe(Obj, nameBuf, sizeof(nameBuf));
        return (strstr(nameBuf, ClassName) != nullptr);
    }

    void CallFn(SDK::UObject* obj, const char* fnName, void* params = nullptr) {
        if (!IsValidObject(obj)) return;
        static auto fn = SDK::UObject::FindObject<SDK::UFunction>(fnName);
        if (fn) obj->ProcessEvent(fn, params);
    }

    // --- RESET FUNCTION ---
    // Called by Hooking.cpp when world transition detects !pLocal
    void Reset() {
        g_RelicCache.clear();
        bCollectRelics = false;
        std::cout << "[Player] Cache cleared and features reset." << std::endl;
    }

    // --- MAIN LOOP ---
    void Update(SDK::APalPlayerCharacter* pLocal)
    {
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
            // Clear cache if user manually toggles off
            if (!g_RelicCache.empty()) g_RelicCache.clear();
        }
    }

    void ProcessAttributes(SDK::APalPlayerCharacter* pLocal)
    {
        // --- ATTACK ---
        if (bAttackMultiplier) {
            if (IsValidObject(pLocal->CharacterParameterComponent)) {
                // Legacy logic: Base is approx 50
                pLocal->CharacterParameterComponent->AttackUp = (int32_t)(50 * fAttackModifier);
            }
        }

        // --- WEIGHT ---
        if (bWeightAdjuster) {
            SDK::APlayerController* PC = static_cast<SDK::APlayerController*>(pLocal->Controller);
            if (IsValidObject(PC) && IsValidObject(PC->PlayerState)) {
                SDK::APalPlayerState* pState = static_cast<SDK::APalPlayerState*>(PC->PlayerState);

                if (IsValidObject(pState->InventoryData)) {
                    auto inv = pState->InventoryData;
                    inv->MaxInventoryWeight = fWeightModifier;

                    // Force Replication & Sync
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
        if (!SDK::UObject::GObjects) return;
        std::cout << "[Jarvis] Revealing Map..." << std::endl;

        __try {
            for (int i = 0; i < SDK::UObject::GObjects->Num(); i++) {
                SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
                if (!IsValidObject(Obj)) continue;

                char nameBuf[256];
                GetNameSafe(Obj, nameBuf, sizeof(nameBuf));
                // PalGameSetting singleton
                if (strstr(nameBuf, "PalGameSetting") && !strstr(nameBuf, "Default__")) {
                    SDK::UPalGameSetting* pSettings = static_cast<SDK::UPalGameSetting*>(Obj);
                    pSettings->worldmapUIMaskClearSize = 20000.0f;
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    void UnlockAllTowers(SDK::APalPlayerCharacter* pLocal)
    {
        if (!SDK::UObject::GObjects) return;
        std::cout << "[Jarvis] Unlocking Fast Travel..." << std::endl;

        bool bIsAuthority = false;
        if (pLocal->Controller) bIsAuthority = pLocal->Controller->HasAuthority();

        __try {
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
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    void TeleportRelicsToPlayer(SDK::APalPlayerCharacter* pLocal)
    {
        ULONGLONG CurrentTime = GetTickCount64();
        if (CurrentTime - g_LastCollectTime < COLLECTION_INTERVAL_MS) return;

        SDK::UWorld* World = SDK::UWorld::GetWorld();
        if (!IsValidObject(World)) return;

        SDK::APalPlayerController* PalPC = static_cast<SDK::APalPlayerController*>(pLocal->Controller);
        if (!IsValidObject(PalPC) || !IsValidObject(PalPC->Transmitter) || !IsValidObject(PalPC->Transmitter->Player)) return;

        // 1. POPULATE CACHE (One-Pass)
        if (g_RelicCache.empty()) {
            std::cout << "[Jarvis] Single-Pass Scan initiated..." << std::endl;
            __try {
                if (SDK::UObject::GObjects) {
                    for (int i = 0; i < SDK::UObject::GObjects->Num(); i++) {
                        SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
                        if (!IsValidObject(Obj)) continue;

                        if (IsClass(Obj, "PalLevelObjectRelic") || IsClass(Obj, "BP_LevelObject_Relic_C")) {
                            SDK::APalLevelObjectObtainable* Relic = static_cast<SDK::APalLevelObjectObtainable*>(Obj);
                            // Only add valid, unpicked relics
                            if (!Relic->bPickedInClient) {
                                g_RelicCache.push_back(Relic);
                            }
                        }
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                g_RelicCache.clear();
                bCollectRelics = false;
                return;
            }

            std::cout << "[Jarvis] Scan Complete. Found: " << g_RelicCache.size() << std::endl;

            // If nothing found, stop immediately.
            if (g_RelicCache.empty()) {
                std::cout << "[Jarvis] No relics found. Disabling." << std::endl;
                bCollectRelics = false;
                return;
            }
        }

        // 2. PROCESS QUEUE
        auto it = g_RelicCache.begin();
        while (it != g_RelicCache.end()) {
            SDK::APalLevelObjectObtainable* Relic = *it;

            if (!IsValidObject(Relic) || Relic->bPickedInClient) {
                it = g_RelicCache.erase(it); // Remove dead/collected
                continue;
            }

            // Valid Relic Found - Collect it
            if (IsValidObject(pLocal->InteractComponent)) {
                pLocal->InteractComponent->SetEnableInteract(true, false);
                PalPC->Transmitter->Player->RequestObtainLevelObject_ToServer(Relic);
                std::cout << "[Jarvis] Collecting Relic (" << g_RelicCache.size() - 1 << " remaining)" << std::endl;
            }

            it = g_RelicCache.erase(it); // Remove from queue
            g_LastCollectTime = CurrentTime;
            break; // Wait for next tick
        }

        // 3. AUTO-SHUTDOWN
        if (g_RelicCache.empty()) {
            std::cout << "[Jarvis] Job complete. Disabling vacuum." << std::endl;
            bCollectRelics = false;
        }
    }
}