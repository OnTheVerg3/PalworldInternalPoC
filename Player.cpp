#include "Player.h"
#include "Hooking.h"
#include <iostream>
#include <vector>
#include <cstring> 
#include <algorithm>
#include <unordered_map>

namespace Player
{
    // ... (Variables and GetPlayerCandidates same as before) ...
    // ... Keep GetPlayerCandidates implementation from previous response ...
    // Re-pasting standard variables for completeness:
    bool bAttackMultiplier = false;
    float fAttackModifier = 2.0f;
    bool bWeightAdjuster = false;
    float fWeightModifier = 50000.0f;
    bool bUnlockMap = false;
    bool bUnlockTowers = false;
    bool bCollectRelics = false;

    std::vector<SDK::APalLevelObjectObtainable*> g_RelicCache;
    ULONGLONG g_LastCollectTime = 0;
    const ULONGLONG COLLECTION_INTERVAL_MS = 250;
    std::unordered_map<std::string, SDK::UFunction*> g_PlayerFuncCache;

    std::vector<PlayerCandidate> GetPlayerCandidates() {
        std::vector<PlayerCandidate> results;
        if (!SDK::UObject::GObjects) return results;
        for (int i = 0; i < SDK::UObject::GObjects->Num(); i++) {
            SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
            if (!IsValidObject(Obj)) continue;
            if (Obj->IsA(SDK::APalPlayerCharacter::StaticClass())) {
                SDK::APalPlayerCharacter* Char = static_cast<SDK::APalPlayerCharacter*>(Obj);
                if (IsValidObject(Char->Controller)) {
                    SDK::AController* PC = Char->Controller;
                    std::string cName = PC->GetName();
                    std::string dName = "Unknown";
                    if (IsValidObject(PC->PlayerState)) {
                        SDK::FString fName = PC->PlayerState->PlayerNamePrivate;
                        if (fName.IsValid()) {
                            std::wstring w = fName.ToWString();
                            dName = std::string(w.begin(), w.end());
                        }
                    }
                    results.push_back({ Char, cName + " - " + dName });
                }
            }
        }
        return results;
    }

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
    }

    void Update(SDK::APalPlayerCharacter* pLocal)
    {
        if (!g_bIsSafe || !IsValidObject(pLocal)) return;
        __try {
            ProcessAttributes(pLocal);
            if (bUnlockMap) { UnlockAllMap(pLocal); bUnlockMap = false; }
            if (bUnlockTowers) { UnlockAllTowers(pLocal); bUnlockTowers = false; }
            if (bCollectRelics) TeleportRelicsToPlayer(pLocal);
            else if (!g_RelicCache.empty()) g_RelicCache.clear();
        }
        __except (1) {}
    }

    void ProcessAttributes(SDK::APalPlayerCharacter* pLocal)
    {
        if (bAttackMultiplier && IsValidObject(pLocal->CharacterParameterComponent)) {
            pLocal->CharacterParameterComponent->AttackUp = (int32_t)(50 * fAttackModifier);
        }
        SDK::APlayerController* PC = static_cast<SDK::APlayerController*>(pLocal->Controller);
        if (IsValidObject(PC) && IsValidObject(PC->PlayerState)) {
            SDK::APalPlayerState* pState = static_cast<SDK::APalPlayerState*>(PC->PlayerState);
            if (IsValidObject(pState->InventoryData)) {
                auto inv = pState->InventoryData;
                if (bWeightAdjuster) inv->MaxInventoryWeight = fWeightModifier;
            }
        }
    }

    void UnlockAllMap(SDK::APalPlayerCharacter* pLocal)
    {
        if (!SDK::UObject::GObjects) return;
        for (int i = 0; i < SDK::UObject::GObjects->Num(); i++) {
            SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
            if (!IsValidObject(Obj)) continue;
            char nameBuf[256];
            GetNameSafe(Obj, nameBuf, sizeof(nameBuf));
            if (strstr(nameBuf, "PalGameSetting") && !strstr(nameBuf, "Default__")) {
                SDK::UPalGameSetting* pSettings = static_cast<SDK::UPalGameSetting*>(Obj);
                pSettings->worldmapUIMaskClearSize = 99999.0f;
            }
        }
        std::cout << "[Jarvis] Map revealed." << std::endl;
    }

    void UnlockAllTowers(SDK::APalPlayerCharacter* pLocal)
    {
        if (!SDK::UObject::GObjects) return;
        std::cout << "[Jarvis] Unlocking Fast Travel..." << std::endl;
        for (int i = 0; i < SDK::UObject::GObjects->Num(); i++) {
            SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
            if (!IsValidObject(Obj)) continue;

            // Matches cheat_state.cpp logic
            if (IsClass(Obj, "PalLevelObjectUnlockableFastTravelPoint")) {
                auto* FT = static_cast<SDK::APalLevelObjectUnlockableFastTravelPoint*>(Obj);
                if (!FT->bUnlocked) {
                    FT->OnTriggerInteract(pLocal, SDK::EPalInteractiveObjectIndicatorType::UnlockFastTravel);
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