#pragma once
#include "SDKGlobal.h"

namespace Features {
    extern bool bInfiniteStamina;
    extern bool bInfiniteAmmo;
    extern bool bInfiniteMagazine;
    extern bool bInfiniteDurability;

    extern bool bDamageHack;
    extern int32_t DamageMultiplier;

    // [FIX] Removed unused externs

    void RunLoop();
    void Reset();
}