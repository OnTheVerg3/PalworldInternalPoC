#include <Windows.h>
#include <iostream>
#include "SDKGlobal.h"
#include "Hooking.h"

DWORD WINAPI MainThread(LPVOID lpReserved) {
    // 1. Console
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONIN$", "r", stdin);
    SetConsoleTitleA("Palworld Internal [Modular]");

    std::cout << "[Core] Initializing..." << std::endl;

    // 2. SDK
    SDK::Init();

    // 3. Hooks
    Hooking::Init();

    return TRUE;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
    }
    return TRUE;
}