#include "Hooking.h"
#include "Menu.h"
#include "SDKGlobal.h"
#include "Features.h"
#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "Player.h" 
#include "Teleporter.h" 
#include "Visuals.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <atomic>
#include <mutex> 
#include <psapi.h> 

#pragma comment(lib, "psapi.lib")

// --- HOOK DEFINITIONS ---
typedef HRESULT(__stdcall* Present) (IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
Present oPresent;
WNDPROC oWndProc;

// --- GLOBALS ---
VMTHook g_PawnHook;
VMTHook g_ControllerHook;
VMTHook g_ParamHook;

// Recursive to prevent deadlocks in UI callbacks
std::recursive_mutex g_HookMutex;

SDK::APalPlayerCharacter* g_pLocal = nullptr;
SDK::UObject* g_pController = nullptr;
SDK::UObject* g_pParam = nullptr;
std::atomic<bool> g_bIsSafe(false);
std::atomic<bool> g_PendingExit(false);

bool g_ShowMenu = false;
bool g_bHasAutoOpened = false;
void* g_LastLocalPlayer = nullptr;
ULONGLONG g_TimePlayerDetected = 0;
ULONGLONG g_ExitCooldown = 0;

ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dContext = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
HWND g_window = nullptr;

SDK::APalPlayerCharacter* g_ManualPlayer = nullptr;

typedef void(__thiscall* ProcessEvent_t)(SDK::UObject*, SDK::UFunction*, void*);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

uintptr_t g_GameBase = 0;
uintptr_t g_GameSize = 0;

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

__declspec(noinline) void GetNameInternal(SDK::UObject* pObject, char* outBuf, size_t size) {
    std::string s = pObject->GetName();
    if (s.length() < size) strcpy_s(outBuf, size, s.c_str());
    else strncpy_s(outBuf, size, s.c_str(), size - 1);
}

void GetNameSafe(SDK::UObject* pObject, char* outBuf, size_t size) {
    memset(outBuf, 0, size);
    if (!IsValidObject(pObject)) return;
    __try { GetNameInternal(pObject, outBuf, size); }
    __except (1) { memset(outBuf, 0, size); }
}

bool RawStrContains(const char* haystack, const char* needle) {
    if (!haystack || !needle) return false;
    return strstr(haystack, needle) != nullptr;
}

__declspec(noinline) void PerformWorldExit() {
    g_bIsSafe = false;
    g_PendingExit = false;
    g_ExitCooldown = GetTickCount64() + 3000;
    g_TimePlayerDetected = 0;

    // Release RTV if held (safety net)
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }

    g_HookMutex.lock();
    g_PawnHook.Restore();
    g_ControllerHook.Restore();
    g_ParamHook.Restore();
    g_pLocal = nullptr;
    g_pController = nullptr;
    g_pParam = nullptr;
    g_LastLocalPlayer = nullptr;
    g_ManualPlayer = nullptr;
    Features::Reset();
    Player::Reset();
    Menu::Reset();
    Teleporter::Reset();
    g_HookMutex.unlock();

    g_ShowMenu = false;
    g_bHasAutoOpened = false;
    std::cout << "[Jarvis] World Exit Clean." << std::endl;
}

void Hooking::SetManualPlayer(SDK::APalPlayerCharacter* pTarget) {
    if (!IsValidObject(pTarget)) return;
    std::cout << "[Jarvis] Manual Switch: " << pTarget->GetName() << std::endl;

    g_HookMutex.lock();
    g_PawnHook.Restore();
    g_ControllerHook.Restore();
    g_ParamHook.Restore();

    g_pLocal = nullptr;
    g_LastLocalPlayer = nullptr;
    g_ManualPlayer = pTarget;

    Features::Reset();
    Player::Reset();
    g_HookMutex.unlock();
}

__declspec(noinline) void CheckForExit(SDK::UObject* pObject, const char* name) {
    if (RawStrContains(name, "ReceiveDestroyed") || RawStrContains(name, "ReceiveEndPlay") ||
        RawStrContains(name, "ClientReturnToMainMenu") || RawStrContains(name, "ClientTravel")) {
        if (pObject == g_pLocal || pObject == g_pController) {
            if (!g_PendingExit) g_PendingExit = true;
        }
    }
}

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN && wParam == VK_INSERT) {
        g_ShowMenu = !g_ShowMenu;
        if (g_pd3dDevice && g_bIsSafe) ImGui::GetIO().MouseDrawCursor = g_ShowMenu;
    }
    if (g_ShowMenu && g_bIsSafe) {
        if (g_pd3dDevice) ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
        switch (uMsg) {
        case WM_MOUSEMOVE: case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK: case WM_MBUTTONDOWN:
        case WM_MBUTTONUP: case WM_MBUTTONDBLCLK: case WM_XBUTTONDOWN: case WM_XBUTTONUP:
        case WM_XBUTTONDBLCLK: case WM_MOUSEWHEEL: case WM_KEYDOWN: case WM_KEYUP:
        case WM_SYSKEYDOWN: case WM_SYSKEYUP: case WM_CHAR: return 1;
        }
    }
    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

bool ProcessEvent_Logic(SDK::UObject* pObject, SDK::UFunction* pFunction, const char* name) {
    if (Features::bInfiniteDurability && (RawStrContains(name, "Durability") || RawStrContains(name, "Deterioration"))) return true;
    if (Features::bInfiniteAmmo && (RawStrContains(name, "Consume") || RawStrContains(name, "Decrease")) && RawStrContains(name, "Bullet")) return true;
    if (Features::bInfiniteMagazine && RawStrContains(name, "Reload")) return true;
    return false;
}

void __fastcall hkProcessEvent(SDK::UObject* pObject, SDK::UFunction* pFunction, void* pParams) {
    ProcessEvent_t oFunc = nullptr;
    if (pObject == g_pLocal) oFunc = g_PawnHook.GetOriginal<ProcessEvent_t>(67);
    else if (pObject == g_pController) oFunc = g_ControllerHook.GetOriginal<ProcessEvent_t>(67);
    else if (pObject == g_pParam) oFunc = g_ParamHook.GetOriginal<ProcessEvent_t>(67);
    if (!oFunc) return;

    __try {
        if (IsGarbagePtr(pObject) || IsGarbagePtr(pFunction)) return oFunc(pObject, pFunction, pParams);

        char name[256];
        GetNameSafe(pFunction, name, sizeof(name));
        if (name[0] != '\0') {
            CheckForExit(pObject, name);
            if (g_PendingExit || !g_bIsSafe) return oFunc(pObject, pFunction, pParams);
            if (ProcessEvent_Logic(pObject, pFunction, name)) return;
        }
        return oFunc(pObject, pFunction, pParams);
    }
    __except (1) { return oFunc(pObject, pFunction, pParams); }
}

SDK::APalPlayerCharacter* Internal_GetLocalPlayer() {
    __try {
        if (g_ManualPlayer && IsValidObject(g_ManualPlayer)) return g_ManualPlayer;
        if (!SDK::pGWorld || !*SDK::pGWorld) return nullptr;
        SDK::UGameInstance* pGI = (*SDK::pGWorld)->OwningGameInstance;
        if (!IsValidObject(pGI) || pGI->LocalPlayers.Num() <= 0) return nullptr;
        SDK::ULocalPlayer* LP = pGI->LocalPlayers[0];
        if (!IsValidObject(LP) || !LP->PlayerController) return nullptr;
        return static_cast<SDK::APalPlayerCharacter*>(LP->PlayerController->Pawn);
    }
    __except (1) { return nullptr; }
}

void AttachPlayerHooks_Logic() {
    SDK::APalPlayerCharacter* pLocal = Internal_GetLocalPlayer();
    if (!IsValidObject(pLocal)) return;
    if (pLocal != g_LastLocalPlayer) {
        std::cout << "[System] Hooking: " << pLocal->GetName() << std::endl;
        g_HookMutex.lock();
        Features::Reset(); Player::Reset();
        g_pLocal = pLocal;
        g_pController = pLocal->Controller;
        g_pParam = pLocal->CharacterParameterComponent;
        if (g_PawnHook.Init(pLocal)) g_PawnHook.Hook<ProcessEvent_t>(67, &hkProcessEvent);
        if (IsValidObject(g_pController) && g_ControllerHook.Init(g_pController)) g_ControllerHook.Hook<ProcessEvent_t>(67, &hkProcessEvent);
        if (IsValidObject(g_pParam) && g_ParamHook.Init(g_pParam)) g_ParamHook.Hook<ProcessEvent_t>(67, &hkProcessEvent);
        g_LastLocalPlayer = pLocal;
        g_HookMutex.unlock();
    }
}

void Present_Logic() {
    if (g_PendingExit) { PerformWorldExit(); return; }
    if (GetTickCount64() < g_ExitCooldown) { g_bIsSafe = false; return; }
    SDK::APalPlayerCharacter* pLocal = Internal_GetLocalPlayer();
    if (!IsValidObject(pLocal)) { if (g_LastLocalPlayer) PerformWorldExit(); return; }
    if (g_TimePlayerDetected == 0) g_TimePlayerDetected = GetTickCount64();
    if ((GetTickCount64() - g_TimePlayerDetected) < 3000) { g_bIsSafe = false; return; }
    g_bIsSafe = true;
    AttachPlayerHooks_Logic();
    g_HookMutex.lock();
    __try { Features::RunLoop(); }
    __except (1) {}
    __try { Player::Update(pLocal); }
    __except (1) {}
    g_HookMutex.unlock();
}

HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (g_GameBase == 0) InitModuleBounds();

    // 1. Initialize ImGui ONCE
    static bool bInit = false;
    if (!bInit) {
        DXGI_SWAP_CHAIN_DESC sd; pSwapChain->GetDesc(&sd);
        g_window = sd.OutputWindow;
        oWndProc = (WNDPROC)SetWindowLongPtr(g_window, GWLP_WNDPROC, (LONG_PTR)WndProc);

        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pd3dDevice))) {
            g_pd3dDevice->GetImmediateContext(&g_pd3dContext);
            ImGui::CreateContext();
            ImGui_ImplWin32_Init(g_window);
            ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext); // Called exactly once
            Menu::InitTheme();
            bInit = true;
        }
        else {
            return oPresent(pSwapChain, SyncInterval, Flags);
        }
    }

    // 2. Create RTV for this frame
    ID3D11Texture2D* pBackBuffer = nullptr;
    pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);

    if (pBackBuffer) {
        // If creation fails, we skip rendering but don't crash
        if (FAILED(g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView))) {
            pBackBuffer->Release();
            return oPresent(pSwapChain, SyncInterval, Flags);
        }
        pBackBuffer->Release();
    }
    else {
        return oPresent(pSwapChain, SyncInterval, Flags);
    }

    // 3. Logic & Input
    __try { Present_Logic(); }
    __except (1) { g_bIsSafe = false; }

    // 4. Draw
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    g_HookMutex.lock();
    if (g_bIsSafe) {
        if (!g_bHasAutoOpened) { g_ShowMenu = true; g_bHasAutoOpened = true; }
        if (g_ShowMenu) {
            ImGui::GetIO().MouseDrawCursor = true;
            Menu::Draw();
        }
        else {
            ImGui::GetIO().MouseDrawCursor = false;
        }
    }
    g_HookMutex.unlock();

    ImGui::Render();
    g_pd3dContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    // 5. [CRITICAL] Clean Up Immediately
    // Unbind RTV from context
    ID3D11RenderTargetView* nullViews[] = { nullptr };
    g_pd3dContext->OMSetRenderTargets(1, nullViews, nullptr);

    // Release RTV so SwapChain is free to resize/destroy
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }

    return oPresent(pSwapChain, SyncInterval, Flags);
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
            InitModuleBounds();
            MH_CreateHook(presentAddr, &hkPresent, (void**)&oPresent);
            MH_EnableHook(MH_ALL_HOOKS);
        }
    }
}

void Hooking::Shutdown() {
    g_PawnHook.Restore(); g_ControllerHook.Restore(); g_ParamHook.Restore();
    MH_DisableHook(MH_ALL_HOOKS); MH_Uninitialize();
}

void Hooking::AttachPlayerHooks() { AttachPlayerHooks_Logic(); }
SDK::APalPlayerCharacter* Hooking::GetLocalPlayerSafe() { return Internal_GetLocalPlayer(); }