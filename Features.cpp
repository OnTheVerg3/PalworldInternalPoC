#include "Features.h"
#include "SDKGlobal.h"
#include "Hooking.h"
#include <iostream>
#include <unordered_map>

namespace Features {
    bool bInfiniteStamina = false;
    bool bInfiniteAmmo = false;
    bool bInfiniteMagazine = false;
    bool bInfiniteDurability = false;
    bool bDamageHack = false;
    int32_t DamageMultiplier = 2;
    // [FIX] Removed RapidFire/NoRecoil globals

    struct WeaponBackup {
        int32_t Attack;
        bool bSaved = false;
    };

    SDK::APalWeaponBase* g_LastWeapon = nullptr;
    WeaponBackup g_OriginalStats;
    std::unordered_map<std::string, SDK::UFunction*> g_FuncCache;

    void Reset() {
        g_LastWeapon = nullptr;
        g_OriginalStats.bSaved = false;
        g_FuncCache.clear();
    }

    SDK::UFunction* GetCachedFunc(const char* name) {
        if (g_FuncCache.count(name)) return g_FuncCache[name];
        auto fn = SDK::UObject::FindObject<SDK::UFunction>(name);
        if (fn) g_FuncCache[name] = fn;
        return fn;
    }

    void RunLoop_Logic() {
        if (!g_bIsSafe) return;
        SDK::APalPlayerCharacter* pLocal = Hooking::GetLocalPlayerSafe();
        if (!IsValidObject(pLocal)) return;

        // Infinite Stamina
        if (bInfiniteStamina && IsValidObject(pLocal->CharacterParameterComponent)) {
            auto fn = GetCachedFunc("Function Pal.PalCharacterParameterComponent.ResetSP");
            if (fn) pLocal->CharacterParameterComponent->ProcessEvent(fn, nullptr);
        }

        // Weapon Features
        if (!IsValidObject(pLocal->ShooterComponent)) return;
        auto pWeapon = pLocal->ShooterComponent->HasWeapon;

        if (IsValidObject(pWeapon)) {
            pWeapon->IsRequiredBullet = !bInfiniteAmmo;
            pWeapon->IsInfinityMagazine = bInfiniteMagazine;

            if (IsValidObject(pWeapon->ownWeaponStaticData)) {
                // Damage Logic
                if (g_LastWeapon != pWeapon) {
                    g_OriginalStats.Attack = pWeapon->ownWeaponStaticData->AttackValue;
                    g_OriginalStats.bSaved = true;
                    g_LastWeapon = pWeapon;
                }

                if (g_OriginalStats.bSaved) {
                    pWeapon->ownWeaponStaticData->AttackValue = bDamageHack ? (g_OriginalStats.Attack * DamageMultiplier) : g_OriginalStats.Attack;
                }
            }
        }
    }

    void RunLoop() {
        __try { RunLoop_Logic(); }
        __except (1) { g_LastWeapon = nullptr; }
    }
}