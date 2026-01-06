#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <vector>
#include <cstdint>
#include "SDKGlobal.h"
[cite_start]#include "VMTHook.h" // [cite: 4] Using the VMTHook class you uploaded

// --- GLOBAL BOUNDS ---
extern uintptr_t g_GameBase;
extern uintptr_t g_GameSize;

// --- MEMORY SAFETY ---
inline bool IsSentinel(void* ptr) { return (uintptr_t)ptr == 0xFFFFFFFFFFFFFFFF; }
inline bool IsGarbagePtr(void* ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    if (!ptr) return true;
    if (IsSentinel(ptr)) return true;
    if (addr < 0x10000000) return true; // Hardened Threshold
    if (addr > 0x7FFFFFFFFFFF) return true;
    if (addr % 8 != 0) return true;
    return false;
}

inline bool IsValidObject(SDK::UObject* pObj) {
    if (IsGarbagePtr(pObj)) return false;
    __try {
        void** vtablePtr = reinterpret_cast<void**>(pObj);
        void* vtable = *vtablePtr;
        if (IsGarbagePtr(vtable)) return false;
        if (g_GameBase != 0 && g_GameSize != 0) {
            uintptr_t vtAddr = (uintptr_t)vtable;
            if (vtAddr < g_GameBase || vtAddr >(g_GameBase + g_GameSize)) return false;
        }
    }
    __except (1) { return false; }
    return true;
}

void GetNameSafe(SDK::UObject* pObject, char* outBuf, size_t size);

// [NEW] Expose VMT Hooks so we can restore them on exit
extern VMTHook g_PawnHook;
extern VMTHook g_ControllerHook;
extern VMTHook g_ParamHook;

class Hooking
{
public:
    static void Init();
    static void Shutdown();
    static void AttachPlayerHooks();
    static SDK::APalPlayerCharacter* GetLocalPlayerSafe();
};