#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <vector>
#include "SDKGlobal.h"

// --- MEMORY SAFETY INTERFACE ---
// Global helpers
bool IsValidObject(SDK::UObject* pObj);
bool IsGarbagePtr(void* ptr);
bool IsValidPtr(void* ptr);
void GetNameSafe(SDK::UObject* pObject, char* outBuf, size_t size);

class Hooking
{
public:
    static void Init();
    static void Shutdown();
    static void AttachPlayerHooks();
    static void ProbeVTable(int index);

    // [NEW] Public accessor for the menu/teleporter
    static SDK::APalPlayerCharacter* GetLocalPlayerSafe();

    static bool bFoundValidTraffic;
};