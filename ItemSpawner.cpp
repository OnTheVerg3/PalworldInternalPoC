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

// Engine classes
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

// [FIX] Robust Name Creation without crashing
SDK::FName MakeName(const char* NameStr) {
    static SDK::UKismetStringLibrary* Lib = nullptr;
    if (!Lib) {
        Lib = static_cast<SDK::UKismetStringLibrary*>(SDK::UObject::FindObject("Default__KismetStringLibrary"));
        if (!Lib) Lib = static_cast<SDK::UKismetStringLibrary*>(SDK::UObject::FindObject("KismetStringLibrary"));
    }

    if (Lib) return Lib->Conv_StringToName(StdToFString(NameStr));

    // Fallback: This is risky but better than nothing if lib is missing
    // Ideally, we shouldn't reach here.
    std::cout << "[-] StringLib Missing. Cannot create FName for: " << NameStr << std::endl;
    return SDK::FName();
}

SDK::UObject* FindRealInventoryData() {
    if (!SDK::UObject::GObjects) return nullptr;

    // Cache Class
    static SDK::UClass* TargetClass = nullptr;
    if (!TargetClass) TargetClass = SDK::UObject::FindObject<SDK::UClass>("Class Pal.PalPlayerInventoryData");

    if (!TargetClass) return nullptr;

    for (int i = 0; i < SDK::UObject::GObjects->Num(); i++) {
        SDK::UObject* Obj = SDK::UObject::GObjects->GetByIndex(i);
        if (!IsValidObject(Obj)) continue;

        if (Obj->IsA(TargetClass)) {
            // Filter out CDO
            if (Obj->GetName().find("Default__") != std::string::npos) continue;
            return Obj;
        }
    }
    return nullptr;
}

// --- SPAWN METHODS ---

void Spawn_Method1(SDK::UObject* pInventory, const char* ItemID, int32_t Count) {
    if (!pInventory) return;

    SDK::FName ItemName = MakeName(ItemID);
    // SDK::FName comparison operators might not be defined, check index if possible
    // Assuming 0 index is None.

    auto fn = SDK::UObject::FindObject<SDK::UFunction>("Function Pal.PalPlayerInventoryData.AddItem_ServerInternal");
    if (fn) {
        struct { SDK::FName ID; int32_t Num; bool bLog; float Dur; } params;
        params.ID = ItemName;
        params.Num = Count;
        params.bLog = true;
        params.Dur = 0.0f;

        pInventory->ProcessEvent(fn, &params);
        std::cout << "[Jarvis] SP Spawn: " << ItemID << std::endl;
    }
}

void Spawn_Method2(SDK::APlayerController* pController, SDK::UObject* pInventory, const char* ItemID, int32_t Count) {
    if (!pInventory || !pController) return;

    SDK::UPalPlayerInventoryData* InvData = static_cast<SDK::UPalPlayerInventoryData*>(pInventory);
    SDK::APalPlayerController* PalPC = static_cast<SDK::APalPlayerController*>(pController);

    SDK::FName ItemName = MakeName(ItemID);

    // 2. Find Container & Slot
    SDK::UPalItemContainer* Container = nullptr;

    // Try getting container normally
    if (!InvData->TryGetContainerFromStaticItemID(ItemName, &Container) || !Container) {
        // [FIX] Fallback: Grab the first valid container (usually essential/common)
        // This bypasses strict type checking which fails for modded/debug items
        // Note: This requires accessing the InventoryMultiHelper or similar, but for now 
        // we will rely on the user having space in the correct tab.
        std::cout << "[-] Could not auto-resolve container. Item might be invalid category." << std::endl;
        return;
    }

    SDK::UPalItemSlot* Slot = nullptr;
    auto InvType = InvData->GetInventoryTypeFromStaticItemID(ItemName);

    if (!InvData->TryGetEmptySlot(InvType, &Slot) || !Slot) {
        std::cout << "[-] Inventory Full or Type Mismatch." << std::endl;
        return;
    }

    // 3. Add Item (Server Attempt)
    InvData->AddItem_ServerInternal(ItemName, Count, true, 0.0f);

    // 4. Client Override (The Exploit)
    Slot->ItemId.StaticId = ItemName;
    Slot->StackCount = Count;

    // Force Dirty
    static auto fnForceDirty = SDK::UObject::FindObject<SDK::UFunction>("Function Pal.PalItemContainer.ForceMarkSlotDirty_ServerInternal");
    if (fnForceDirty) Container->ProcessEvent(fnForceDirty, nullptr);

    // 5. Request Move (Sync Exploit)
    if (IsValidObject(PalPC->Transmitter) && IsValidObject(PalPC->Transmitter->Item)) {
        SDK::FGuid RequestID = GenerateGuid();
        SDK::FPalItemSlotId SlotId = Slot->GetSlotId();

        SDK::TArray<SDK::FPalItemSlotIdAndNum> Froms;
        Froms.Add({ SlotId, Count });

        PalPC->Transmitter->Item->RequestMove_ToServer(RequestID, SlotId, Froms);
        std::cout << "[Jarvis] MP Sync Sent: " << ItemID << std::endl;
    }

    // Refresh UI
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

    // ... (UI Layout Code Remains Identical to save space, logic is in functions above) ... 

    // Copy-paste the Grid/List logic here if you need full file, 
    // otherwise I will just show the button logic which calls the new functions.

    if (strlen(searchBuffer) > 0) {
        // ... Search Logic ...
        ImGui::BeginChild("SearchResults", ImVec2(0, -60), true);
        std::string filter = searchBuffer;
        for (const auto& cat : Database::Categories) {
            for (const auto& item : cat.Items) {
                std::string itemName = item.Name;
                if (itemName.length() == 0) continue;
                auto it = std::search(itemName.begin(), itemName.end(), filter.begin(), filter.end(), [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); });
                if (it != itemName.end()) {
                    if (ImGui::Selectable(item.Name)) strcpy_s(manualIdBuffer, item.ID);
                }
            }
        }
        ImGui::EndChild();
    }
    else {
        // ... Category/Item Logic ...
        ImGui::Columns(2, "SpawnerColumns", true);
        ImGui::SetColumnWidth(0, 160.0f);
        ImGui::BeginChild("Categories", ImVec2(0, -60));
        for (int i = 0; i < Database::Categories.size(); i++) {
            if (ImGui::Selectable(Database::Categories[i].Name, selectedCategoryIdx == i)) { selectedCategoryIdx = i; selectedItemIdx = -1; }
        }
        ImGui::EndChild();
        ImGui::NextColumn();
        ImGui::BeginChild("Items", ImVec2(0, -60));
        if (selectedCategoryIdx >= 0 && selectedCategoryIdx < Database::Categories.size()) {
            const auto& items = Database::Categories[selectedCategoryIdx].Items;
            for (int i = 0; i < items.size(); i++) {
                if (ImGui::Selectable(items[i].Name, selectedItemIdx == i)) { selectedItemIdx = i; strcpy_s(manualIdBuffer, items[i].ID); }
            }
        }
        ImGui::EndChild();
        ImGui::Columns(1);
    }

    ImGui::Separator();
    ImGui::PushItemWidth(120); ImGui::InputInt("##Amount", &itemQty); if (itemQty < 1) itemQty = 1; ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::PushItemWidth(200); ImGui::InputText("##ManualID", manualIdBuffer, sizeof(manualIdBuffer)); ImGui::PopItemWidth();
    ImGui::SameLine();

    if (CustomButton("SPAWN (SP)", ImVec2(100, 30), false)) {
        Spawn_Method1(FindRealInventoryData(), manualIdBuffer, itemQty);
    }
    ImGui::SameLine();
    if (CustomButton("SPAWN (MP)", ImVec2(100, 30), false)) {
        auto pLocal = SDK::GetLocalPlayer();
        if (pLocal && pLocal->Controller) {
            Spawn_Method2(static_cast<SDK::APlayerController*>(pLocal->Controller), FindRealInventoryData(), manualIdBuffer, itemQty);
        }
    }
}