#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <vector>
#include <cstdint>
#include <atomic> 
#include "SDKGlobal.h"
#include "VMTHook.h" 

// --- GLOBAL BOUNDS ---
extern uintptr_t g_GameBase;
extern uintptr_t g_GameSize;

// --- SHARED GLOBALS ---
// These allow all other files to access the variables defined in Hooking.cpp
extern std::atomic<bool> g_bIsSafe;
extern SDK::APalPlayerCharacter* g_pLocal;

// --- MEMORY SAFETY ---
inline bool IsSentinel(void* ptr) { return (uintptr_t)ptr == 0xFFFFFFFFFFFFFFFF; }
inline bool IsGarbagePtr(void* ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    if (!ptr) return true;
    if (IsSentinel(ptr)) return true;
    if (addr < 0x10000000) return true;
    if (addr > 0x7FFFFFFFFFFF) return true;
    if (addr % 8 != 0) return true;
    return false;
}

inline bool IsValidObject(SDK::UObject* pObj) {
    if (IsGarbagePtr(pObj)) return false;
    __try {
        void** vtablePtr = reinterpret_cast<void**>(pObj);
        void* vtable = *vtablePtr;

        // Use IsGarbagePtr only. VMT Hooks move vtables to Heap, so bounds checks fail.
        if (IsGarbagePtr(vtable)) return false;

        return true;
    }
    __except (1) { return false; }
}

void GetNameSafe(SDK::UObject* pObject, char* outBuf, size_t size);

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