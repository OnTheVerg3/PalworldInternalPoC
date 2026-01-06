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
    if (addr < 0x10000) return true; // 64KB null partition
    if (addr > 0x7FFFFFFFFFFF) return true; // User mode limit
    if (addr % 8 != 0) return true; // Alignment check
    return false;
}

// [CRITICAL UPDATE] Safe Validation without IsBadReadPtr
// Uses SEH (__try/__except) to safely check pointers.
inline bool IsValidObject(SDK::UObject* pObj) {
    // 1. Pointer Check
    if (IsGarbagePtr(pObj)) return false;

    __try {
        // 2. VTable Dereference
        void** vtablePtr = reinterpret_cast<void**>(pObj);
        void* vtable = *vtablePtr;

        // 3. VTable Garbage Check
        if (IsGarbagePtr(vtable)) return false;

        // 4. MODULE BOUNDS CHECK
        // Valid VTables MUST be in the Game Module (.rdata).
        if (g_GameBase != 0 && g_GameSize != 0) {
            uintptr_t vtAddr = (uintptr_t)vtable;
            if (vtAddr < g_GameBase || vtAddr >(g_GameBase + g_GameSize)) {
                return false; // Zombie/Heap Object -> Drop
            }
        }
    }
    __except (1) {
        return false; // Crashed reading VTable -> Invalid
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