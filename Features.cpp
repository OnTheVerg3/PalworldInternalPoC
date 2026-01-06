#include "Features.h"
#include "SDKGlobal.h"
#include "Hooking.h"
#include <iostream>
#include <unordered_map>
#include <windows.h> 

extern std::atomic<bool> g_bIsSafe;

namespace Features {
    bool bInfiniteStamina = false;
    bool bInfiniteAmmo = false;
    bool bInfiniteMagazine = false;
    bool bInfiniteDurability = false;

    bool bDamageHack = false;
    int32_t DamageMultiplier = 2;

    bool bRapidFire = false;
    bool bNoRecoil = false;

    struct WeaponBackup {
        int32_t Attack;
        float Recoil;
        float Accuracy;
        bool bSaved = false;
    };

    SDK::APalWeaponBase* g_LastWeapon = nullptr;
    WeaponBackup g_OriginalStats;

    std::unordered_map<std::string, SDK::UFunction*> g_FuncCache;

    void Reset() {
        g_LastWeapon = nullptr;
        g_OriginalStats.bSaved = false;
        g_FuncCache.clear();
        std::cout << "[Features] State and Cache reset." << std::endl;
    }

    SDK::UFunction* GetCachedFunc(const char* name) {
        auto it = g_FuncCache.find(name);
        if (it != g_FuncCache.end()) return it->second;

        if (!SDK::UObject::GObjects || IsGarbagePtr(*(void**)&SDK::UObject::GObjects)) return nullptr;

        SDK::UFunction* fn = SDK::UObject::FindObject<SDK::UFunction>(name);
        if (fn) g_FuncCache[name] = fn;
        return fn;
    }

    void RunLoop_Logic() {
        // [SAFETY] ABSOLUTE KILL SWITCH
        if (!g_bIsSafe) return;

        if (!SDK::UObject::GObjects || IsGarbagePtr(*(void**)&SDK::UObject::GObjects)) return;

        SDK::APalPlayerCharacter* pLocal = Hooking::GetLocalPlayerSafe();
        if (!IsValidObject(pLocal)) return;
        if (!IsValidObject(pLocal->CharacterParameterComponent)) return;

        // --- INFINITE STAMINA ---
        if (bInfiniteStamina) {
            // [FIX] Removed 'static' to prevent stale pointers across world loads
            auto fnResetSP = GetCachedFunc("Function Pal.PalCharacterParameterComponent.ResetSP");
            if (fnResetSP && IsValidObject(fnResetSP)) {
                pLocal->CharacterParameterComponent->ProcessEvent(fnResetSP, nullptr);
            }
        }

        // --- WEAPON LOGIC ---
        if (!IsValidObject(pLocal->ShooterComponent)) return;

        auto pWeapon = pLocal->ShooterComponent->HasWeapon;
        if (!IsValidObject(pWeapon)) {
            g_LastWeapon = nullptr;
            g_OriginalStats.bSaved = false;
            return;
        }

        if (bInfiniteAmmo) pWeapon->IsRequiredBullet = false;
        else pWeapon->IsRequiredBullet = true;

        if (bInfiniteMagazine) pWeapon->IsInfinityMagazine = true;
        else pWeapon->IsInfinityMagazine = false;

        if (IsValidObject(pWeapon->ownWeaponStaticData)) {
            if (g_LastWeapon != pWeapon) {
                if (g_LastWeapon && IsValidObject(g_LastWeapon) && IsValidObject(g_LastWeapon->ownWeaponStaticData) && g_OriginalStats.bSaved) {
                    g_LastWeapon->ownWeaponStaticData->AttackValue = g_OriginalStats.Attack;
                }

                g_OriginalStats.Attack = pWeapon->ownWeaponStaticData->AttackValue;
                g_OriginalStats.bSaved = true;
                g_LastWeapon = pWeapon;
            }

            if (g_OriginalStats.bSaved) {
                if (bDamageHack) pWeapon->ownWeaponStaticData->AttackValue = g_OriginalStats.Attack * DamageMultiplier;
                else pWeapon->ownWeaponStaticData->AttackValue = g_OriginalStats.Attack;
            }
        }
    }

    void RunLoop() {
        __try {
            RunLoop_Logic();
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            g_LastWeapon = nullptr;
        }
    }
}