#include "ItemSpawner.h"
#include "ItemDatabase.h"
#include "SDKGlobal.h"
#include "imgui.h"
#include "imgui_style.h" // [Fix] Required for CustomButton
#include <iostream>
#include <string>
#include <algorithm>
#include <vector>

// Access globals from Hooking.cpp if needed
extern std::vector<uintptr_t> g_PatternMatches;
extern int g_CurrentMatchIndex;

// --- HELPER FUNCTIONS ---
SDK::FString StdToFString(const std::string& str) {
    std::wstring wstr(str.begin(), str.end());
    return SDK::FString(wstr.c_str());
}

SDK::UObject* FindRealInventoryData() {
    if (!SDK::UObject::GObjects) return nullptr;

    static SDK::UClass* TargetClass = SDK::UObject::FindObject<SDK::UClass>("Class Pal.PalPlayerInventoryData");
    if (!TargetClass) TargetClass = SDK::UObject::FindObject<SDK::UClass>("PalPlayerInventoryData");
    if (!TargetClass) return nullptr;

    // Quick heuristic scan to find a valid inventory
    for (int i = 0; i < SDK::UObject::GObjects->Num(); i++) {
        SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
        if (!Obj) continue;

        if (Obj->IsA(TargetClass)) {
            std::string name = Obj->GetName();
            if (name.find("Default__") != std::string::npos) continue;

            // Return the first valid non-default inventory found
            return Obj;
        }
    }
    return nullptr;
}

// --- SPAWN METHODS ---

// [Fix] Signature now accepts const char* string, not FName
void Spawn_Method1(SDK::UObject* pInventory, const char* ItemID, int32_t Count) {
    if (!pInventory) { std::cout << "[-] Method 1: Inventory null." << std::endl; return; }

    static auto fn = SDK::UObject::FindObject<SDK::UFunction>("Function Pal.PalPlayerInventoryData.AddItem_ServerInternal");
    if (fn) {
        struct { SDK::FName ID; int32_t Num; bool bLog; float Dur; } params;

        // Convert string to FName internally
        params.ID = SDK::UKismetStringLibrary::GetDefaultObj()->Conv_StringToName(StdToFString(ItemID));
        params.Num = Count;
        params.bLog = true;
        params.Dur = 0.0f;

        pInventory->ProcessEvent(fn, &params);
        std::cout << "[+] Method 1 Executed for: " << ItemID << std::endl;
    }
}

// [Fix] Signature now accepts const char* string, not FName
void Spawn_Method2(SDK::APlayerController* pController, SDK::UObject* pInventory, const char* ItemID, int32_t Count) {
    if (!pInventory || !pController) return;

    // 1. Add Locally
    static auto fnAdd = SDK::UObject::FindObject<SDK::UFunction>("Function Pal.PalPlayerInventoryData.AddItem_ServerInternal");
    if (fnAdd) {
        struct { SDK::FName ID; int32_t Num; bool bLog; float Dur; } params;

        // Convert string to FName internally
        params.ID = SDK::UKismetStringLibrary::GetDefaultObj()->Conv_StringToName(StdToFString(ItemID));
        params.Num = Count;
        params.bLog = true;
        params.Dur = 0.0f;

        pInventory->ProcessEvent(fnAdd, &params);
    }

    // 2. Force Sync
    static auto fnDirty = SDK::UObject::FindObject<SDK::UFunction>("Function Pal.PalPlayerInventoryData.RequestForceMarkAllDirty_ToServer");
    if (fnDirty) {
        struct { bool bUnused; } params = { true };
        pInventory->ProcessEvent(fnDirty, &params);
        std::cout << "[+] Method 2: Sync Sent for: " << ItemID << std::endl;
    }
}

// --- UI STATE ---
static int selectedCategoryIdx = 0;
static int selectedItemIdx = -1;
static char searchBuffer[64] = "";
static char manualIdBuffer[64] = "PalSphere";
static int itemQty = 1;

void ItemSpawner::DrawTab() {
    // 1. SEARCH BAR
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##Search", "Search all items...", searchBuffer, sizeof(searchBuffer));
    bool isSearching = (strlen(searchBuffer) > 0);

    ImGui::Spacing();

    // 2. SPLIT VIEW
    if (isSearching) {
        ImGui::BeginChild("SearchResults", ImVec2(0, -60), true);
        std::string filter = searchBuffer;
        // Simple case-insensitive search could be implemented here
        for (const auto& cat : Database::Categories) {
            for (const auto& item : cat.Items) {
                std::string itemName = item.Name;
                if (itemName.length() == 0) continue;

                // Check substring
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
        // --- Standard View ---
        ImGui::Columns(2, "SpawnerColumns", true);
        ImGui::SetColumnWidth(0, 160.0f);

        // LEFT PANE: Categories
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

        // RIGHT PANE: Items
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

    // 3. CONTROLS
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

    // Spawn Buttons - Now using valid function calls
    if (CustomButton("SPAWN", ImVec2(100, 30), false)) {
        Spawn_Method1(FindRealInventoryData(), manualIdBuffer, itemQty);
    }

    ImGui::SameLine();

    if (CustomButton("FORCE", ImVec2(100, 30), false)) {
        auto pLocal = SDK::GetLocalPlayer();
        if (pLocal && pLocal->Controller) {
            Spawn_Method2(static_cast<SDK::APlayerController*>(pLocal->Controller),
                FindRealInventoryData(), manualIdBuffer, itemQty);
        }
    }
}