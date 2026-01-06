#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <vector>
#include <cstdint> // Required for uintptr_t
#include "SDKGlobal.h"

// --- MEMORY SAFETY INTERFACE (INLINE) ---
// Defined in header to prevent Linker Errors (LNK2001) in Player/Teleporter modules

inline bool IsSentinel(void* ptr) {
    return (uintptr_t)ptr == 0xFFFFFFFFFFFFFFFF;
}

// Lowered threshold to prevent false positives on valid low-mem objects
inline bool IsGarbagePtr(void* ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    if (!ptr) return true;
    if (IsSentinel(ptr)) return true;
    if (addr < 0x10000) return true; // 64KB null partition
    if (addr > 0x7FFFFFFFFFFF) return true;
    if (addr % 8 != 0) return true;
    return false;
}

inline bool IsValidPtr(void* ptr) {
    if (IsGarbagePtr(ptr)) return false;
    return !IsBadReadPtr(ptr, 8);
}

// Check if memory is valid before reading it
// Now inline to resolve external symbol errors
inline bool IsValidObject(SDK::UObject* pObj) {
    if (!IsValidPtr(pObj)) return false;

    // Validate VTable Pointer
    void** vtablePtr = reinterpret_cast<void**>(pObj);
    if (IsBadReadPtr(vtablePtr, 8)) return false;

    void* vtable = *vtablePtr;
    if (IsGarbagePtr(vtable)) return false;
    if (IsBadReadPtr(vtable, 8)) return false;

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

    // Public accessor for the menu/teleporter
    static SDK::APalPlayerCharacter* GetLocalPlayerSafe();

    static bool bFoundValidTraffic;
};