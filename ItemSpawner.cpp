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

// --- HELPERS (Internal) ---
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

SDK::UPalPlayerInventoryData* GetInventory(SDK::APalPlayerCharacter* pLocal) {
    if (!IsValidObject(pLocal)) return nullptr;

    auto PC = static_cast<SDK::APalPlayerController*>(pLocal->Controller);
    if (!IsValidObject(PC)) return nullptr;

    auto State = static_cast<SDK::APalPlayerState*>(PC->PlayerState);
    if (!IsValidObject(State)) return nullptr;

    return State->InventoryData;
}

// --- SPAWN METHODS ---

void ItemSpawner::Spawn_Method1(SDK::APalPlayerCharacter* pLocal, const char* ItemID, int32_t Count) {
    auto InvData = GetInventory(pLocal);
    if (!IsValidObject(InvData)) return;

    SDK::FName ItemName = GetItemName(ItemID);

    auto fn = SDK::UObject::FindObject<SDK::UFunction>("Function Pal.PalPlayerInventoryData.AddItem_ServerInternal");
    if (fn) {
        struct { SDK::FName ID; int32_t Num; bool bLog; float Dur; } params;
        params.ID = ItemName;
        params.Num = Count;
        params.bLog = true;
        params.Dur = 0.0f;

        InvData->ProcessEvent(fn, &params);
        std::cout << "[Jarvis] Method 1 Sent: " << ItemID << std::endl;
    }
}

void ItemSpawner::Spawn_Method2(SDK::APalPlayerCharacter* pLocal, const char* ItemID, int32_t Count) {
    auto InvData = GetInventory(pLocal);
    if (!IsValidObject(InvData)) return;

    SDK::APalPlayerController* PalPC = static_cast<SDK::APalPlayerController*>(pLocal->Controller);

    // 1. Get ID
    SDK::FName ItemName = GetItemName(ItemID);

    // 2. Find Container & Slot
    SDK::UPalItemContainer* Container = nullptr;
    if (!InvData->TryGetContainerFromStaticItemID(ItemName, &Container) || !Container) {
        std::cout << "[-] Container resolution failed." << std::endl;
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

    // 4. Client Override (Clean + Set)
    Slot->StackCount = 0;
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
        std::cout << "[Jarvis] Method 2 (Sync) Sent: " << ItemID << std::endl;
    }

    InvData->RequestForceMarkAllDirty_ToServer(true);
}

// --- PINNING SYSTEM ---
struct PinnedEntry {
    std::string Name;
    std::string ID;
};
static std::vector<PinnedEntry> g_PinnedItems;

void TogglePin(const std::string& name, const std::string& id) {
    auto it = std::find_if(g_PinnedItems.begin(), g_PinnedItems.end(),
        [&](const PinnedEntry& e) { return e.ID == id; });

    if (it != g_PinnedItems.end()) {
        g_PinnedItems.erase(it);
    }
    else {
        g_PinnedItems.push_back({ name, id });
    }
}

bool IsPinned(const std::string& id) {
    return std::any_of(g_PinnedItems.begin(), g_PinnedItems.end(),
        [&](const PinnedEntry& e) { return e.ID == id; });
}

// --- UI STATE ---
static int selectedCategoryIdx = 0;
static int selectedItemIdx = -1;
static char searchBuffer[64] = "";
static char manualIdBuffer[64] = "PalSphere";
static int itemQty = 1;

void ItemSpawner::DrawTab() {
    // Get Local Player First (Needed for direct spawn buttons)
    auto pLocal = Hooking::GetLocalPlayerSafe();

    // --- PINNED ITEMS SECTION ---
    if (!g_PinnedItems.empty()) {
        ColoredSeparatorText("Favorites", ImVec4(1.0f, 0.84f, 0.0f, 1.0f)); // Gold Color
        if (ImGui::BeginChild("PinnedArea", ImVec2(0, 100), true)) {
            for (const auto& item : g_PinnedItems) {
                ImGui::PushID(item.ID.c_str());

                // Spawn Buttons
                if (ImGui::Button("SP")) { if (pLocal) Spawn_Method1(pLocal, item.ID.c_str(), itemQty); }
                ImGui::SameLine();
                if (ImGui::Button("MP")) { if (pLocal) Spawn_Method2(pLocal, item.ID.c_str(), itemQty); }

                ImGui::SameLine();

                // Selectable Name (Click to fill buffer)
                if (ImGui::Selectable(item.Name.c_str(), false, 0, ImVec2(200, 0))) {
                    strcpy_s(manualIdBuffer, item.ID.c_str());
                }

                // Unpin Menu
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Unpin")) TogglePin(item.Name, item.ID);
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        ImGui::Spacing();
    }

    // --- MAIN SEARCH ---
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##Search", "Search items (Right-click to Pin)...", searchBuffer, sizeof(searchBuffer));
    ImGui::Spacing();

    // --- ITEM LIST ---
    if (strlen(searchBuffer) > 0) {
        // Search Results
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
                    bool pinned = IsPinned(item.ID);
                    std::string label = std::string(item.Name) + (pinned ? " *" : "");

                    if (ImGui::Selectable(label.c_str())) {
                        strcpy_s(manualIdBuffer, item.ID);
                    }

                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem(pinned ? "Unpin Item" : "Pin Item")) TogglePin(item.Name, item.ID);
                        ImGui::EndPopup();
                    }
                }
            }
        }
        ImGui::EndChild();
    }
    else {
        // Categories Layout
        ImGui::Columns(2, "SpawnerColumns", true);
        ImGui::SetColumnWidth(0, 160.0f);

        ImGui::BeginChild("Categories", ImVec2(0, -60));
        for (int i = 0; i < Database::Categories.size(); i++) {
            if (ImGui::Selectable(Database::Categories[i].Name, selectedCategoryIdx == i)) {
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
                bool pinned = IsPinned(items[i].ID);
                std::string label = std::string(items[i].Name) + (pinned ? " *" : "");

                if (ImGui::Selectable(label.c_str(), selectedItemIdx == i)) {
                    selectedItemIdx = i;
                    strcpy_s(manualIdBuffer, items[i].ID);
                }

                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem(pinned ? "Unpin Item" : "Pin Item")) TogglePin(items[i].Name, items[i].ID);
                    ImGui::EndPopup();
                }
            }
        }
        ImGui::EndChild();
        ImGui::Columns(1);
    }

    ImGui::Separator();

    // --- CONTROL BAR ---
    ImGui::PushItemWidth(120);
    ImGui::InputInt("##Amount", &itemQty);
    if (itemQty < 1) itemQty = 1;
    ImGui::PopItemWidth();

    ImGui::SameLine();

    ImGui::PushItemWidth(200);
    ImGui::InputText("##ManualID", manualIdBuffer, sizeof(manualIdBuffer));
    ImGui::PopItemWidth();

    ImGui::SameLine();

    if (CustomButton("SPAWN (SP)", ImVec2(100, 30), false)) {
        if (pLocal) ItemSpawner::Spawn_Method1(pLocal, manualIdBuffer, itemQty);
    }
    ImGui::SameLine();
    if (CustomButton("SPAWN (MP)", ImVec2(100, 30), false)) {
        if (pLocal) ItemSpawner::Spawn_Method2(pLocal, manualIdBuffer, itemQty);
    }
}