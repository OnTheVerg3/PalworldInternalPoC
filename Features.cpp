#include "Features.h"
#include "SDKGlobal.h"
#include "Hooking.h"
#include <iostream>
#include <unordered_map>
#include <windows.h> 

namespace Features {
    bool bInfiniteStamina = false;
    bool bInfiniteAmmo = false;
    bool bInfiniteMagazine = false;
    bool bInfiniteDurability = false;

    bool bDamageHack = false;
    int32_t DamageMultiplier = 2;

    bool bRapidFire = false;
    bool bNoRecoil = false;

    // --- WEAPON STATE TRACKING ---
    struct WeaponBackup {
        int32_t Attack;
        float Recoil;
        float Accuracy;
        bool bSaved = false;
    };

    SDK::APalWeaponBase* g_LastWeapon = nullptr;
    WeaponBackup g_OriginalStats;

    // [NEW] RESET FUNCTION
    void Reset() {
        g_LastWeapon = nullptr;
        g_OriginalStats.bSaved = false;
        std::cout << "[Features] State reset." << std::endl;
    }

    SDK::UFunction* GetCachedFunc(const char* name) {
        static std::unordered_map<std::string, SDK::UFunction*> cache;

        // Return cached if exists
        if (cache.find(name) != cache.end()) {
            return cache[name];
        }

        // [CRITICAL FIX] Verify GObjects before scanning
        // Prevents 0x8 crash during world exit
        if (!SDK::UObject::GObjects || IsGarbagePtr(*(void**)&SDK::UObject::GObjects)) {
            return nullptr;
        }

        SDK::UFunction* fn = SDK::UObject::FindObject<SDK::UFunction>(name);
        if (fn) cache[name] = fn;
        return fn;
    }

    void RunLoop_Logic() {
        // [SAFETY] Double check globals
        if (!SDK::UObject::GObjects || IsGarbagePtr(*(void**)&SDK::UObject::GObjects)) return;

        SDK::APalPlayerCharacter* pLocal = Hooking::GetLocalPlayerSafe();

        if (!pLocal || IsBadReadPtr(pLocal, 8)) return;
        if (!pLocal->CharacterParameterComponent || IsBadReadPtr(pLocal->CharacterParameterComponent, 8)) return;

        // --- INFINITE STAMINA ---
        if (bInfiniteStamina) {
            static auto fnResetSP = GetCachedFunc("Function Pal.PalCharacterParameterComponent.ResetSP");
            if (fnResetSP) {
                pLocal->CharacterParameterComponent->ProcessEvent(fnResetSP, nullptr);
            }
        }

        // --- WEAPON LOGIC ---
        if (pLocal->ShooterComponent && !IsBadReadPtr(pLocal->ShooterComponent, 8) && pLocal->ShooterComponent->HasWeapon) {
            auto pWeapon = pLocal->ShooterComponent->HasWeapon;
            if (IsBadReadPtr(pWeapon, 8)) return;

            // 1. AMMO & MAGAZINE
            if (bInfiniteAmmo) pWeapon->IsRequiredBullet = false;
            else pWeapon->IsRequiredBullet = true;

            if (bInfiniteMagazine) pWeapon->IsInfinityMagazine = true;
            else pWeapon->IsInfinityMagazine = false;

            // 2. STAT MODIFIERS
            if (pWeapon->ownWeaponStaticData && !IsBadReadPtr(pWeapon->ownWeaponStaticData, 8)) {

                if (g_LastWeapon != pWeapon) {
                    if (g_LastWeapon && !IsBadReadPtr(g_LastWeapon, 8) && g_LastWeapon->ownWeaponStaticData && g_OriginalStats.bSaved) {
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
        else {
            g_LastWeapon = nullptr;
            g_OriginalStats.bSaved = false;
        }
    }

    void RunLoop() {
        __try {
            RunLoop_Logic();
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}