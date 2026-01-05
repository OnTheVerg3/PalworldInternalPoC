#pragma once
#include "SDKGlobal.h"

namespace Features {
    // Configuration Variables
    extern bool bInfiniteStamina;
    extern bool bInfiniteAmmo;
    extern bool bInfiniteMagazine;
    extern bool bInfiniteDurability;

    extern bool bDamageHack;
    extern int32_t DamageMultiplier;

    extern bool bRapidFire;
    extern bool bNoRecoil;

    // Logic
    void RunLoop();

    // [NEW] Clears stale pointers (Call this on level change)
    void Reset();
}