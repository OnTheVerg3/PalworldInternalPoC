#include "Hooking.h"
#include "Menu.h"
#include "SDKGlobal.h"
#include "Features.h"
#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "Player.h" 
#include <iostream>
#include <vector>
#include <algorithm>
#include <atomic>
#include <mutex> 
#include <psapi.h> 

#pragma comment(lib, "psapi.lib")

// --- GLOBALS ---
bool Hooking::bFoundValidTraffic = false;
std::vector<void*> g_ActiveHooks;
std::vector<void*> g_HookedObjects;
std::mutex g_HookMutex;

// --- FAST ACCESS CACHE ---
extern SDK::APalPlayerCharacter* g_pLocal;
SDK::UObject* g_pController = nullptr;
SDK::UObject* g_pParam = nullptr;

// Time-Based Latches
ULONGLONG g_TimePlayerDetected = 0;
ULONGLONG g_TimePlayerLost = 0;

std::atomic<bool> g_bIsSafe(false);

// MODULE BOUNDS
uintptr_t g_GameBase = 0;
uintptr_t g_GameSize = 0;

// LATCHES
bool g_bPawnHooked = false;
bool g_bControllerHooked = false;
bool g_bParamHooked = false;

// WATCHDOGS
void* g_LastWorld = nullptr;
void* g_LastLocalPlayer = nullptr;

// DIRECTX
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dContext = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
HWND g_window = nullptr;
bool g_ShowMenu = true;

typedef HRESULT(__stdcall* Present) (IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
Present oPresent;
WNDPROC oWndProc;

typedef void(__thiscall* ProcessEvent_t)(SDK::UObject*, SDK::UFunction*, void*);
ProcessEvent_t oProcessEvent = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// --- MEMORY SAFETY HELPERS ---

void InitModuleBounds() {
    if (g_GameBase != 0) return;
    HMODULE hMod = GetModuleHandle(NULL);
    if (!hMod) return;

    MODULEINFO mi;
    if (GetModuleInformation(GetCurrentProcess(), hMod, &mi, sizeof(mi))) {
        g_GameBase = (uintptr_t)mi.lpBaseOfDll;
        g_GameSize = (uintptr_t)mi.SizeOfImage;
    }
}

bool IsSentinel(void* ptr) {
    return (uintptr_t)ptr == 0xFFFFFFFFFFFFFFFF;
}

bool IsGarbagePtr(void* ptr) {
    uintptr_t addr = (uintptr_t)ptr;
    if (!ptr) return true;
    if (IsSentinel(ptr)) return true;
    if (addr < 0x10000) return true;
    if (addr > 0x7FFFFFFFFFFF) return true;
    if (addr % 8 != 0) return true;
    return false;
}

bool IsValidPtr(void* ptr) {
    if (IsGarbagePtr(ptr)) return false;
    return !IsBadReadPtr(ptr, 8);
}

bool IsValidObject(SDK::UObject* pObj) {
    if (!IsValidPtr(pObj)) return false;
    void* vtable = *reinterpret_cast<void***>(pObj);
    if (IsGarbagePtr(vtable)) return false;
    if (IsBadReadPtr(vtable, 8)) return false;

    if (g_GameBase != 0) {
        uintptr_t vtableAddr = (uintptr_t)vtable;
        if (vtableAddr < g_GameBase || vtableAddr >(g_GameBase + g_GameSize)) {
            return false;
        }
    }
    return true;
}

void GetNameInternal(SDK::UObject* pObject, char* outBuf, size_t size) {
    std::string s = pObject->GetName();
    if (s.length() < size) {
        strcpy_s(outBuf, size, s.c_str());
    }
    else {
        strncpy_s(outBuf, size, s.c_str(), size - 1);
    }
}

void GetNameSafe(SDK::UObject* pObject, char* outBuf, size_t size) {
    memset(outBuf, 0, size);
    if (!IsValidObject(pObject)) return;
    __try {
        GetNameInternal(pObject, outBuf, size);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        memset(outBuf, 0, size);
    }
}

bool RawStrContains(const char* haystack, const char* needle) {
    if (!haystack || !needle) return false;
    size_t hLen = strlen(haystack);
    size_t nLen = strlen(needle);
    if (nLen > hLen) return false;

    for (size_t i = 0; i <= hLen - nLen; ++i) {
        bool match = true;
        for (size_t j = 0; j < nLen; ++j) {
            if (toupper(haystack[i + j]) != toupper(needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

bool IsValidSignature(uintptr_t addr) {
    if (!IsValidPtr((void*)addr)) return false;
    unsigned char* b = (unsigned char*)addr;
    if (b[0] == 0x40 && b[1] == 0x55 && b[2] == 0x56 && b[3] == 0x57) return true;
    if (b[0] == 0x48 && b[1] == 0x89 && b[2] == 0x5C && b[3] == 0x24) return true;
    if (b[0] == 0x4C && b[1] == 0x8B && b[2] == 0xDC) return true;
    return false;
}

// --- SANITIZER HELPER ---
bool FastValidateObject(SDK::UObject* pObject) {
    if (!pObject) return false;
    uintptr_t addr = (uintptr_t)pObject;
    if (addr == 0xFFFFFFFFFFFFFFFF) return false;
    if (addr < 0x10000) return false;
    if (addr % 8 != 0) return false;

    // VTable Dereference (Safely)
    __try {
        void* vtable = *(void**)pObject;
        uintptr_t vtAddr = (uintptr_t)vtable;
        if (vtAddr == 0xFFFFFFFFFFFFFFFF) return false;
        if (vtAddr < 0x10000) return false;
        if (vtAddr % 8 != 0) return false;
    }
    __except (1) { return false; }
    return true;
}

// --- CLEANUP HELPER ---
void PerformWorldExit() {
    std::cout << "[System] World Exit. Resetting Logic (Hooks Persist)." << std::endl;

    // 1. Reset Pointers
    g_pLocal = nullptr;
    g_pController = nullptr;
    g_pParam = nullptr;

    // 2. Clear Logic State (But DO NOT Disable Hooks)
    // This stops the Race Condition. The hooks stay active but in "Pass-Through" mode.
    {
        std::lock_guard<std::mutex> lock(g_HookMutex);
        g_HookedObjects.clear();
        g_bPawnHooked = false;
        g_bControllerHooked = false;
        g_bParamHooked = false;
        g_LastLocalPlayer = nullptr;
    }

    Features::Reset();
    Player::Reset();
}

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN && wParam == VK_INSERT) {
        g_ShowMenu = !g_ShowMenu;
        if (g_pd3dDevice) {
            ImGui::GetIO().MouseDrawCursor = g_ShowMenu;
            if (g_ShowMenu) ClipCursor(nullptr);
        }
        return 0;
    }

    if (g_ShowMenu) {
        if (g_pd3dDevice) {
            ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
        }
        switch (uMsg) {
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
        case WM_MOUSEWHEEL:
        case WM_KEYDOWN: case WM_KEYUP:
        case WM_SYSKEYDOWN: case WM_SYSKEYUP:
        case WM_CHAR:
            return 1;
        }
    }
    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

// --- LOGIC HELPER ---
bool ProcessEvent_Logic(SDK::UObject* pObject, SDK::UFunction* pFunction, const char* name) {

    // [CRITICAL] DETECT SELF-DESTRUCTION
    if (RawStrContains(name, "ReceiveDestroyed") || RawStrContains(name, "EndPlay")) {
        if (pObject == g_pLocal) {
            g_pLocal = nullptr;
            g_pController = nullptr;
            g_pParam = nullptr;
            g_bIsSafe = false;
        }
        return false; // Return FALSE to allow execution to proceed (pass to original)
    }

    if (!Hooking::bFoundValidTraffic) {
        if (RawStrContains(name, "Server") || RawStrContains(name, "Client")) {
            Hooking::bFoundValidTraffic = true;
        }
    }

    if (Features::bInfiniteDurability) {
        if (RawStrContains(name, "UpdateDurability") ||
            RawStrContains(name, "Deterioration") ||
            RawStrContains(name, "Broken")) {
            return true;
        }
    }

    if (Features::bInfiniteAmmo) {
        if (RawStrContains(name, "Consume") && RawStrContains(name, "Bullet")) return true;
        if (RawStrContains(name, "Decrease") && RawStrContains(name, "Bullet")) return true;
        if (RawStrContains(name, "UseBullet")) return true;
        if (RawStrContains(name, "RequestConsumeItem")) return true;
        if (RawStrContains(name, "ConsumeItem")) return true;
    }

    if (Features::bInfiniteMagazine) {
        if (RawStrContains(name, "Reload")) return true;
    }

    return false;
}

// --- THE HOOK CALLBACK ---
void __fastcall hkProcessEvent(SDK::UObject* pObject, SDK::UFunction* pFunction, void* pParams) {
    if (g_GameBase == 0) InitModuleBounds();

    // 1. FAST VALIDATION (Prevent 0xFF Sentinel Crash)
    // If invalid, DROP IT. Passing garbage to engine is fatal.
    if (!FastValidateObject(pObject) || !FastValidateObject(pFunction)) {
        return;
    }

    // 2. UNSAFE MODE (Transition / Menu)
    // Pass everything through safely. No logic.
    if (!g_bIsSafe || !SDK::UWorld::GetWorld()) {
        __try { if (oProcessEvent) oProcessEvent(pObject, pFunction, pParams); }
        __except (1) {}
        return;
    }

    // 3. WHITE-LIST FILTER
    // If pointers are cleared (World Exit), this fails safely and passes through.
    if (pObject != g_pLocal && pObject != g_pController && pObject != g_pParam) {
        __try { if (oProcessEvent) oProcessEvent(pObject, pFunction, pParams); }
        __except (1) {}
        return;
    }

    // 4. SAFE MODE (Gameplay)
    char name[256];
    GetNameSafe(pFunction, name, sizeof(name));

    if (name[0] == '\0') {
        __try { if (oProcessEvent) oProcessEvent(pObject, pFunction, pParams); }
        __except (1) {}
        return;
    }

    bool bBlock = false;
    __try {
        bBlock = ProcessEvent_Logic(pObject, pFunction, name);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        bBlock = false;
    }

    // If logic says block, we return. Otherwise, pass to original.
    if (bBlock) return;

    __try {
        if (oProcessEvent) oProcessEvent(pObject, pFunction, pParams);
    }
    __except (1) {}
}

// --- HOOKING HELPERS ---
bool HookIndex(SDK::UObject* pObject, int index, const char* debugName) {
    if (!IsValidObject(pObject)) return false;
    void** vtable = *reinterpret_cast<void***>(pObject);
    if (!IsValidPtr(vtable)) return false;
    void* pTarget = vtable[index];
    if (!IsValidPtr(pTarget)) return false;

    bool bAlreadyTracked = false;
    {
        std::lock_guard<std::mutex> lock(g_HookMutex);
        for (void* ptr : g_ActiveHooks) {
            if (ptr == pTarget) { bAlreadyTracked = true; break; }
        }
    }

    if (!bAlreadyTracked) {
        MH_STATUS status = MH_CreateHook(pTarget, &hkProcessEvent, (void**)&oProcessEvent);
        if (status == MH_OK) {
            MH_EnableHook(pTarget);
            {
                std::lock_guard<std::mutex> lock(g_HookMutex);
                g_ActiveHooks.push_back(pTarget);
            }
            std::cout << "[+] Hooked " << debugName << std::endl;
        }
        else if (status == MH_ERROR_ALREADY_CREATED) {
            // If already created, ensure enabled (Wake Up)
            MH_EnableHook(pTarget);
            {
                std::lock_guard<std::mutex> lock(g_HookMutex);
                g_ActiveHooks.push_back(pTarget);
            }
        }
        else {
            return false;
        }
    }
    else {
        // Just ensure it's enabled (Wake Up)
        MH_EnableHook(pTarget);
    }

    {
        std::lock_guard<std::mutex> lock(g_HookMutex);
        bool bFound = false;
        for (void* ptr : g_HookedObjects) { if (ptr == pObject) { bFound = true; break; } }
        if (!bFound) g_HookedObjects.push_back(pObject);
    }

    return true;
}

bool ScanAndHook(SDK::UObject* pObject, int startIdx, int endIdx, const char* debugName) {
    if (!IsValidObject(pObject)) return false;
    void** vtable = *reinterpret_cast<void***>(pObject);
    if (!IsValidPtr(vtable)) return false;
    int foundIndex = -1;
    for (int i = startIdx; i <= endIdx; i++) {
        if (i < 10) continue;
        if (IsValidSignature((uintptr_t)vtable[i])) { foundIndex = i; break; }
    }
    if (foundIndex != -1) { return HookIndex(pObject, foundIndex, debugName); }
    return false;
}

// --- ROBUST PLAYER RETRIEVAL ---
SDK::APalPlayerCharacter* Internal_GetLocalPlayer() {
    __try {
        SDK::UWorld* pWorld = SDK::UWorld::GetWorld();
        if (!IsValidObject(pWorld)) return nullptr;
        if (!IsValidObject(pWorld->PersistentLevel)) return nullptr;

        SDK::UGameInstance* pGI = pWorld->OwningGameInstance;
        if (!IsValidObject(pGI)) return nullptr;
        if (pGI->LocalPlayers.Num() <= 0) return nullptr;

        SDK::ULocalPlayer* pLocalPlayer = pGI->LocalPlayers[0];
        if (!IsValidObject(pLocalPlayer)) return nullptr;

        SDK::APlayerController* pPC = pLocalPlayer->PlayerController;
        if (!IsValidObject(pPC)) return nullptr;

        SDK::APawn* pPawn = pPC->Pawn;
        if (IsGarbagePtr(pPawn)) return nullptr;
        if (!IsValidObject(pPawn)) return nullptr;

        char nameBuf[256];
        GetNameSafe(pPawn, nameBuf, sizeof(nameBuf));
        if (!RawStrContains(nameBuf, "Player") && !RawStrContains(nameBuf, "Character")) return nullptr;

        SDK::APalPlayerCharacter* pPalChar = static_cast<SDK::APalPlayerCharacter*>(pPawn);
        if (!IsValidObject(pPalChar->CharacterParameterComponent)) return nullptr;
        if (!IsValidObject(pPC->PlayerState)) return nullptr;

        return pPalChar;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

SDK::APalPlayerCharacter* Hooking::GetLocalPlayerSafe() {
    return Internal_GetLocalPlayer();
}

void AttachPlayerHooks_Logic() {
    SDK::APalPlayerCharacter* pLocal = Internal_GetLocalPlayer();
    if (!IsValidObject(pLocal)) return;

    g_pLocal = pLocal;
    g_pController = pLocal->Controller;
    g_pParam = pLocal->CharacterParameterComponent;

    // If latches are false (reset by exit), we try to hook.
    // HookIndex handles duplicates gracefully now.
    if (g_bPawnHooked && g_bControllerHooked && g_bParamHooked) return;

    if (pLocal != g_LastLocalPlayer) {
        g_LastLocalPlayer = pLocal;
    }

    if (!g_bPawnHooked) {
        if (HookIndex(pLocal, 67, "Pawn")) g_bPawnHooked = true;
    }

    if (!g_bControllerHooked && pLocal->Controller && IsValidObject(pLocal->Controller)) {
        if (ScanAndHook(pLocal->Controller, 10, 175, "Controller")) g_bControllerHooked = true;
    }

    if (!g_bParamHooked && pLocal->CharacterParameterComponent && IsValidObject(pLocal->CharacterParameterComponent)) {
        if (HookIndex(pLocal->CharacterParameterComponent, 76, "ParamComp")) g_bParamHooked = true;
    }
}

// --- SAFE FRAME LOGIC ---
void Present_Logic() {
    ULONGLONG CurrentTime = GetTickCount64();
    SDK::APalPlayerCharacter* pLocal = Internal_GetLocalPlayer();

    if (!pLocal) {
        g_bIsSafe = false;

        g_pLocal = nullptr;
        g_pController = nullptr;
        g_pParam = nullptr;

        if (g_TimePlayerLost == 0) g_TimePlayerLost = CurrentTime;

        if ((CurrentTime - g_TimePlayerLost) > 1000) {
            if (g_LastLocalPlayer != nullptr) {
                PerformWorldExit();
                g_TimePlayerDetected = 0;
            }
        }
        return;
    }

    g_TimePlayerLost = 0;

    if (g_TimePlayerDetected == 0) {
        g_TimePlayerDetected = CurrentTime;
        std::cout << "[System] Player detected. Stabilizing..." << std::endl;
    }
    if ((CurrentTime - g_TimePlayerDetected) < 3000) {
        g_bIsSafe = false;
        return;
    }

    g_bIsSafe = true;
    AttachPlayerHooks_Logic();

    __try { Features::RunLoop(); }
    __except (1) {}
    __try { Player::Update(pLocal); }
    __except (1) {}
}

HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (g_GameBase == 0) InitModuleBounds();

    static bool bInputInitialized = false;
    if (!bInputInitialized) {
        DXGI_SWAP_CHAIN_DESC sd; pSwapChain->GetDesc(&sd);
        g_window = sd.OutputWindow;
        oWndProc = (WNDPROC)SetWindowLongPtr(g_window, GWLP_WNDPROC, (LONG_PTR)WndProc);
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(g_window);
        bInputInitialized = true;
    }

    if (!g_mainRenderTargetView) {
        ID3D11Texture2D* pBackBuffer = nullptr;
        pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice);
        if (g_pd3dDevice) {
            g_pd3dDevice->GetImmediateContext(&g_pd3dContext);
            pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
            if (pBackBuffer) {
                g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
                pBackBuffer->Release();
                ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext);
            }
        }
    }

    __try { Present_Logic(); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}

    __try {
        if (g_mainRenderTargetView) {
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            if (g_ShowMenu) {
                ImGui::GetIO().MouseDrawCursor = true;
                Menu::Draw();
            }
            else {
                ImGui::GetIO().MouseDrawCursor = false;
            }

            ImGui::Render();
            g_pd3dContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}

    return oPresent(pSwapChain, SyncInterval, Flags);
}

void Hooking::AttachPlayerHooks() {
    __try { AttachPlayerHooks_Logic(); }
    __except (1) {}
}

void Hooking::Init() {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "DX11 Dummy", NULL };
    RegisterClassEx(&wc);
    HWND hWnd = CreateWindow("DX11 Dummy", NULL, WS_OVERLAPPEDWINDOW, 100, 100, 300, 300, NULL, NULL, wc.hInstance, NULL);
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    DXGI_SWAP_CHAIN_DESC scd; ZeroMemory(&scd, sizeof(scd));
    scd.BufferCount = 1; scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hWnd; scd.SampleDesc.Count = 1; scd.Windowed = true; scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    ID3D11Device* dev; ID3D11DeviceContext* ctx; IDXGISwapChain* swap;
    if (SUCCEEDED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, levels, 1, D3D11_SDK_VERSION, &scd, &swap, &dev, NULL, &ctx))) {
        DWORD_PTR* vtable = (DWORD_PTR*)swap; vtable = (DWORD_PTR*)vtable[0];
        void* presentAddr = (void*)vtable[8];
        swap->Release(); dev->Release(); ctx->Release(); DestroyWindow(hWnd); UnregisterClass("DX11 Dummy", wc.hInstance);
        if (MH_Initialize() == MH_OK) {
            MH_CreateHook(presentAddr, &hkPresent, (void**)&oPresent);
            MH_EnableHook(MH_ALL_HOOKS);
        }
    }
}
void Hooking::Shutdown() { MH_DisableHook(MH_ALL_HOOKS); MH_Uninitialize(); }
void Hooking::ProbeVTable(int index) {}