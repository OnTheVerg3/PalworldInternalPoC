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

SDK::UObject* FindRealInventoryData() {
    if (!SDK::UObject::GObjects || IsGarbagePtr(*(void**)&SDK::UObject::GObjects)) return nullptr;

    SDK::UClass* TargetClass = SDK::UObject::FindObject<SDK::UClass>("Class Pal.PalPlayerInventoryData");
    if (!TargetClass) TargetClass = SDK::UObject::FindObject<SDK::UClass>("PalPlayerInventoryData");
    if (!TargetClass) return nullptr;

    for (int i = 0; i < SDK::UObject::GObjects->Num(); i++) {
        SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
        if (!IsValidObject(Obj)) continue;

        if (Obj->IsA(TargetClass)) {
            std::string name = Obj->GetName();
            if (name.find("Default__") != std::string::npos) continue;
            return Obj;
        }
    }
    return nullptr;
}

// --- SPAWN METHODS ---

void Spawn_Method1(SDK::UObject* pInventory, const char* ItemID, int32_t Count) {
    if (!pInventory) return;
    auto fn = SDK::UObject::FindObject<SDK::UFunction>("Function Pal.PalPlayerInventoryData.AddItem_ServerInternal");
    if (fn) {
        struct { SDK::FName ID; int32_t Num; bool bLog; float Dur; } params;

        static SDK::UObject* lib = SDK::UObject::FindObject<SDK::UObject>("Default__KismetStringLibrary");
        static SDK::UFunction* convFn = SDK::UObject::FindObject<SDK::UFunction>("Function Engine.KismetStringLibrary.Conv_StringToName");

        if (lib && convFn) {
            struct { SDK::FString S; SDK::FName R; } cParams;
            cParams.S = StdToFString(ItemID);
            lib->ProcessEvent(convFn, &cParams);
            params.ID = cParams.R;
        }
        else {
            return;
        }

        params.Num = Count;
        params.bLog = true;
        params.Dur = 0.0f;

        pInventory->ProcessEvent(fn, &params);
        std::cout << "[Jarvis] Spawn Method 1 Executed." << std::endl;
    }
}

void Spawn_Method2(SDK::APlayerController* pController, SDK::UObject* pInventory, const char* ItemID, int32_t Count) {
    if (!pInventory || !pController) return;

    SDK::UPalPlayerInventoryData* InvData = static_cast<SDK::UPalPlayerInventoryData*>(pInventory);
    SDK::APalPlayerController* PalPC = static_cast<SDK::APalPlayerController*>(pController);

    // 1. Get Item Name
    SDK::FName ItemName;
    static SDK::UObject* lib = SDK::UObject::FindObject<SDK::UObject>("Default__KismetStringLibrary");
    static SDK::UFunction* convFn = SDK::UObject::FindObject<SDK::UFunction>("Function Engine.KismetStringLibrary.Conv_StringToName");
    if (lib && convFn) {
        struct { SDK::FString S; SDK::FName R; } cParams;
        cParams.S = StdToFString(ItemID);
        lib->ProcessEvent(convFn, &cParams);
        ItemName = cParams.R;
    }
    else return;

    // 2. Find Container & Slot
    SDK::UPalItemContainer* Container = nullptr;
    if (!InvData->TryGetContainerFromStaticItemID(ItemName, &Container) || !Container) {
        std::cout << "[-] Failed to get container." << std::endl;
        return;
    }

    SDK::UPalItemSlot* Slot = nullptr;
    auto InvType = InvData->GetInventoryTypeFromStaticItemID(ItemName);
    if (!InvData->TryGetEmptySlot(InvType, &Slot) || !Slot) {
        std::cout << "[-] Inventory Full (No Empty Slot)." << std::endl;
        return;
    }

    // 3. Add Item Internal (Server)
    InvData->AddItem_ServerInternal(ItemName, Count, true, 0.0f);

    // 4. Force Local Update (Critical for Sync)
    Slot->ItemId.StaticId = ItemName;
    Slot->StackCount = Count;

    // [FIX] Use ProcessEvent because ForceMarkSlotDirty_ServerInternal is missing from SDK header
    static auto fnForceDirty = SDK::UObject::FindObject<SDK::UFunction>("Function Pal.PalItemContainer.ForceMarkSlotDirty_ServerInternal");
    if (fnForceDirty) {
        Container->ProcessEvent(fnForceDirty, nullptr);
    }
    else {
        // Fallback or log if function not found (unlikely)
        std::cout << "[-] ForceMarkSlotDirty_ServerInternal not found via FindObject." << std::endl;
    }

    // 5. The "Move" Exploit
    if (IsValidObject(PalPC->Transmitter) && IsValidObject(PalPC->Transmitter->Item)) {
        SDK::FGuid RequestID = GenerateGuid();
        SDK::FPalItemSlotId SlotId = Slot->GetSlotId();

        SDK::TArray<SDK::FPalItemSlotIdAndNum> Froms;
        Froms.Add({ SlotId, Count });

        PalPC->Transmitter->Item->RequestMove_ToServer(RequestID, SlotId, Froms);
        std::cout << "[Jarvis] Sync Request Sent (MP Exploit)." << std::endl;
    }

    // 6. Mark All Dirty
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
    ImGui::InputTextWithHint("##Search", "Search all items...", searchBuffer, sizeof(searchBuffer));
    bool isSearching = (strlen(searchBuffer) > 0);

    ImGui::Spacing();

    if (isSearching) {
        ImGui::BeginChild("SearchResults", ImVec2(0, -60), true);
        std::string filter = searchBuffer;
        for (const auto& cat : Database::Categories) {
            for (const auto& item : cat.Items) {
                std::string itemName = item.Name;
                if (itemName.length() == 0) continue;

                auto it = std::search(
                    itemName.begin(), itemName.end(),
                    filter.begin(), filter.end(),
                    [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); }
                );

                if (it != itemName.end()) {
                    if (ImGui::Selectable(item.Name)) {
                        strcpy_s(manualIdBuffer, item.ID);
                    }
                }
            }
        }
        ImGui::EndChild();
    }
    else {
        ImGui::Columns(2, "SpawnerColumns", true);
        ImGui::SetColumnWidth(0, 160.0f);

        ImGui::BeginChild("Categories", ImVec2(0, -60));
        for (int i = 0; i < Database::Categories.size(); i++) {
            bool isSelected = (selectedCategoryIdx == i);
            if (ImGui::Selectable(Database::Categories[i].Name, isSelected)) {
                selectedCategoryIdx = i;
                selectedItemIdx = -1;
            }
        }
        ImGui::EndChild();

        ImGui::NextColumn();

        ImGui::BeginChild("Items", ImVec2(0, -60));
        if (selectedCategoryIdx >= 0 && selectedCategoryIdx < Database::Categories.size()) {
            const auto& items = Database::Categories[selectedCategoryIdx].Items;
            for (int i = 0; i < items.size(); i++) {
                bool isSelected = (selectedItemIdx == i);
                if (ImGui::Selectable(items[i].Name, isSelected)) {
                    selectedItemIdx = i;
                    strcpy_s(manualIdBuffer, items[i].ID);
                }
            }
        }
        ImGui::EndChild();
        ImGui::Columns(1);
    }

    ImGui::Separator();

    ImGui::PushItemWidth(120);
    ImGui::InputInt("##Amount", &itemQty);
    if (itemQty < 1) itemQty = 1;
    ImGui::PopItemWidth();

    ImGui::SameLine();

    ImGui::PushItemWidth(200);
    ImGui::InputText("##ManualID", manualIdBuffer, sizeof(manualIdBuffer));
    ImGui::PopItemWidth();

    ImGui::SameLine();

    if (CustomButton("SPAWN", ImVec2(100, 30), false)) {
        Spawn_Method1(FindRealInventoryData(), manualIdBuffer, itemQty);
    }

    ImGui::SameLine();

    if (CustomButton("MP FORCE", ImVec2(100, 30), false)) {
        auto pLocal = SDK::GetLocalPlayer();
        if (pLocal && pLocal->Controller) {
            Spawn_Method2(static_cast<SDK::APlayerController*>(pLocal->Controller),
                FindRealInventoryData(), manualIdBuffer, itemQty);
        }
    }
}