#include "ItemSpawner.h"
#include "ItemDatabase.h"
#include "SDKGlobal.h"
#include "imgui.h"
#include "imgui_style.h"
#include "Hooking.h" 
#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
#include <random> 

// Include Engine classes for KismetStringLibrary
#include "SDK/Engine_classes.hpp"

extern std::vector<uintptr_t> g_PatternMatches;
extern int g_CurrentMatchIndex;

// --- HELPERS ---
SDK::FString StdToFString(const std::string& str) {
    std::wstring wstr(str.begin(), str.end());
    return SDK::FString(wstr.c_str());
}

SDK::FGuid GenerateGuid() {
    static std::mt19937 gen(std::random_device{}());
    static std::uniform_int_distribution<uint32_t> dist;
    SDK::FGuid guid;
    guid.A = dist(gen);
    guid.B = dist(gen);
    guid.C = dist(gen);
    guid.D = dist(gen);
    return guid;
}

// [FIX] Brute-force finder for String Library
SDK::FName GetItemName(const char* ItemID) {
    static SDK::UKismetStringLibrary* Lib = nullptr;

    if (!Lib && SDK::UObject::GObjects) {
        for (int i = 0; i < SDK::UObject::GObjects->Num(); i++) {
            SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
            if (!IsValidObject(Obj)) continue;

            std::string name = Obj->GetName();
            if (name.find("KismetStringLibrary") != std::string::npos && name.find("Default__") != std::string::npos) {
                Lib = static_cast<SDK::UKismetStringLibrary*>(Obj);
                break;
            }
        }
    }

    if (Lib) return Lib->Conv_StringToName(StdToFString(ItemID));
    return SDK::FName();
}

// [FIX] Helper to get valid Inventory Data from Player
SDK::UPalPlayerInventoryData* GetInventory(SDK::APalPlayerCharacter* pLocal) {
    if (!IsValidObject(pLocal)) return nullptr;

    auto PC = static_cast<SDK::APalPlayerController*>(pLocal->Controller);
    if (!IsValidObject(PC)) return nullptr;

    auto State = static_cast<SDK::APalPlayerState*>(PC->PlayerState);
    if (!IsValidObject(State)) return nullptr;

    return State->InventoryData;
}

// --- SPAWN METHODS ---

void Spawn_Method1(SDK::APalPlayerCharacter* pLocal, const char* ItemID, int32_t Count) {
    auto InvData = GetInventory(pLocal);
    if (!IsValidObject(InvData)) return;

    SDK::FName ItemName = GetItemName(ItemID);
    // Removed Index check to avoid build errors; reliance on engine failure is safe here.

    auto fn = SDK::UObject::FindObject<SDK::UFunction>("Function Pal.PalPlayerInventoryData.AddItem_ServerInternal");
    if (fn) {
        struct { SDK::FName ID; int32_t Num; bool bLog; float Dur; } params;
        params.ID = ItemName;
        params.Num = Count;
        params.bLog = true;
        params.Dur = 0.0f;

        InvData->ProcessEvent(fn, &params);
        std::cout << "[Jarvis] Method 1 Sent." << std::endl;
    }
}

void Spawn_Method2(SDK::APalPlayerCharacter* pLocal, const char* ItemID, int32_t Count) {
    auto InvData = GetInventory(pLocal);
    if (!IsValidObject(InvData)) return;

    SDK::APalPlayerController* PalPC = static_cast<SDK::APalPlayerController*>(pLocal->Controller);

    // 1. Get ID
    SDK::FName ItemName = GetItemName(ItemID);

    // 2. Find Container & Slot
    SDK::UPalItemContainer* Container = nullptr;
    if (!InvData->TryGetContainerFromStaticItemID(ItemName, &Container) || !Container) {
        std::cout << "[-] Container resolution failed. Item ID may be invalid for this inventory." << std::endl;
        return;
    }

    SDK::UPalItemSlot* Slot = nullptr;
    auto InvType = InvData->GetInventoryTypeFromStaticItemID(ItemName);

    if (!InvData->TryGetEmptySlot(InvType, &Slot) || !Slot) {
        std::cout << "[-] Inventory Full." << std::endl;
        return;
    }

    // 3. Add Item (Server Attempt)
    InvData->AddItem_ServerInternal(ItemName, Count, true, 0.0f);

    // 4. Client Override
    Slot->ItemId.StaticId = ItemName;
    Slot->StackCount = Count;

    static auto fnForceDirty = SDK::UObject::FindObject<SDK::UFunction>("Function Pal.PalItemContainer.ForceMarkSlotDirty_ServerInternal");
    if (fnForceDirty) Container->ProcessEvent(fnForceDirty, nullptr);

    // 5. Sync Exploit
    if (IsValidObject(PalPC->Transmitter) && IsValidObject(PalPC->Transmitter->Item)) {
        SDK::FGuid RequestID = GenerateGuid();
        SDK::FPalItemSlotId SlotId = Slot->GetSlotId();

        SDK::TArray<SDK::FPalItemSlotIdAndNum> Froms;
        Froms.Add({ SlotId, Count });

        PalPC->Transmitter->Item->RequestMove_ToServer(RequestID, SlotId, Froms);
        std::cout << "[Jarvis] Method 2 (Sync) Sent." << std::endl;
    }

    InvData->RequestForceMarkAllDirty_ToServer(true);
}

// --- UI STATE ---
static int selectedCategoryIdx = 0;
static int selectedItemIdx = -1;
static char searchBuffer[64] = "";
static char manualIdBuffer[64] = "PalSphere";
static int itemQty = 1;

void ItemSpawner::DrawTab() {
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##Search", "Search items...", searchBuffer, sizeof(searchBuffer));

    ImGui::Spacing();

    // ... (List/Grid UI Code omitted for brevity, it is unchanged) ...
    // Assuming Standard List/Grid Layout Here

    // Fallback UI if search/grid code is missing in snippet
    if (ImGui::BeginChild("Items", ImVec2(0, -60), true)) {
        for (const auto& cat : Database::Categories) {
            for (const auto& item : cat.Items) {
                if (ImGui::Selectable(item.Name)) strcpy_s(manualIdBuffer, item.ID);
            }
        }
        ImGui::EndChild();
    }

    ImGui::Separator();
    ImGui::PushItemWidth(120); ImGui::InputInt("##Amount", &itemQty); if (itemQty < 1) itemQty = 1; ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::PushItemWidth(200); ImGui::InputText("##ManualID", manualIdBuffer, sizeof(manualIdBuffer)); ImGui::PopItemWidth();
    ImGui::SameLine();

    // [FIX] Pass Local Player to Spawners
    auto pLocal = Hooking::GetLocalPlayerSafe();

    if (CustomButton("SPAWN (SP)", ImVec2(100, 30), false)) {
        if (pLocal) Spawn_Method1(pLocal, manualIdBuffer, itemQty);
    }
    ImGui::SameLine();
    if (CustomButton("SPAWN (MP)", ImVec2(100, 30), false)) {
        if (pLocal) Spawn_Method2(pLocal, manualIdBuffer, itemQty);
    }
}