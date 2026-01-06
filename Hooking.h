#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <vector>
#include <cstdint>
#include "SDKGlobal.h"

// --- GLOBAL BOUNDS (Defined in Hooking.cpp) ---
extern uintptr_t g_GameBase;
extern uintptr_t g_GameSize;

// --- MEMORY SAFETY INTERFACE (INLINE) ---

inline bool IsSentinel(void* ptr) {
    return (uintptr_t)ptr == 0xFFFFFFFFFFFFFFFF;
}

inline bool IsGarbagePtr(void* ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    if (!ptr) return true;
    if (IsSentinel(ptr)) return true;
    if (addr < 0x10000) return true;
    if (addr > 0x7FFFFFFFFFFF) return true;
    if (addr % 8 != 0) return true;
    return false;
}

inline bool IsValidPtr(void* ptr) {
    if (IsGarbagePtr(ptr)) return false;
    return !IsBadReadPtr(ptr, 8);
}

// [CRITICAL UPDATE] Module-Bound Validation
inline bool IsValidObject(SDK::UObject* pObj) {
    if (!IsValidPtr(pObj)) return false;

    // 1. Read VTable Pointer
    void** vtablePtr = reinterpret_cast<void**>(pObj);
    if (IsBadReadPtr(vtablePtr, 8)) return false;

    void* vtable = *vtablePtr;

    // 2. Garbage Check
    if (IsGarbagePtr(vtable)) return false;
    if (IsBadReadPtr(vtable, 8)) return false;

    // 3. MODULE BOUNDS CHECK (The Crash Fix)
    // VTables MUST be in the executable module (.rdata). 
    // They are never on the Heap (0x00000000...).
    if (g_GameBase != 0 && g_GameSize != 0) {
        uintptr_t vtAddr = (uintptr_t)vtable;
        if (vtAddr < g_GameBase || vtAddr >(g_GameBase + g_GameSize)) {
            return false; // Fake/Zombie Object
        }
    }

    return true;
}

// Heavy functions remain in .cpp
void GetNameSafe(SDK::UObject* pObject, char* outBuf, size_t size);

class Hooking
{
public:
    static void Init();
    static void Shutdown();
    static void AttachPlayerHooks();
    static void ProbeVTable(int index);
    static SDK::APalPlayerCharacter* GetLocalPlayerSafe();

    static bool bFoundValidTraffic;
};