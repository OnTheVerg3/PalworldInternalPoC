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

        // [SAFETY] Strict check
        if (!SDK::UObject::GObjects || IsGarbagePtr(*(void**)&SDK::UObject::GObjects)) return nullptr;

        SDK::UFunction* fn = SDK::UObject::FindObject<SDK::UFunction>(name);
        if (fn) g_FuncCache[name] = fn;
        return fn;
    }

    void RunLoop_Logic() {
        // [SAFETY] 1. Global Safe Flag
        if (!g_bIsSafe) return;

        // [SAFETY] 2. Global Object Array
        if (!SDK::UObject::GObjects || IsGarbagePtr(*(void**)&SDK::UObject::GObjects)) return;

        // [SAFETY] 3. Local Player & Component Validity
        SDK::APalPlayerCharacter* pLocal = Hooking::GetLocalPlayerSafe();
        if (!IsValidObject(pLocal)) return;
        if (!IsValidObject(pLocal->CharacterParameterComponent)) return;

        // --- INFINITE STAMINA ---
        if (bInfiniteStamina) {
            static auto fnResetSP = GetCachedFunc("Function Pal.PalCharacterParameterComponent.ResetSP");
            if (fnResetSP && IsValidObject(fnResetSP)) {
                pLocal->CharacterParameterComponent->ProcessEvent(fnResetSP, nullptr);
            }
        }

        // --- WEAPON LOGIC ---
        // [SAFETY] Check ShooterComponent
        if (!IsValidObject(pLocal->ShooterComponent)) return;

        // [SAFETY] Check Weapon
        auto pWeapon = pLocal->ShooterComponent->HasWeapon;
        if (!IsValidObject(pWeapon)) {
            g_LastWeapon = nullptr;
            g_OriginalStats.bSaved = false;
            return;
        }

        // 1. AMMO & MAGAZINE
        if (bInfiniteAmmo) pWeapon->IsRequiredBullet = false;
        else pWeapon->IsRequiredBullet = true;

        if (bInfiniteMagazine) pWeapon->IsInfinityMagazine = true;
        else pWeapon->IsInfinityMagazine = false;

        // 2. STAT MODIFIERS
        // [SAFETY] Check Static Data
        if (IsValidObject(pWeapon->ownWeaponStaticData)) {

            if (g_LastWeapon != pWeapon) {
                // Restore previous weapon if valid
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
        // [CRITICAL] Exception Barrier
        // Catches 0xFF / 0x8 crashes if engine destroys objects mid-logic
        __try {
            RunLoop_Logic();
        }
        __except (1) {
            // Logic crashed (likely world exit).
            // Reset state safely.
            g_LastWeapon = nullptr;
        }
    }
}