#include "Menu.h"
#include "Features.h" 
#include "Hooking.h"
#include "ItemSpawner.h"
#include "Player.h"
#include "Teleporter.h"
#include "SDKGlobal.h"
#include "imgui_style.h"

SDK::APalPlayerCharacter* g_pLocal = nullptr;

// Static internal state
static int selectedTab = 0;

void Menu::InitTheme() {
    SetupImGuiStyle();
}

// [NEW] Reset Implementation
void Menu::Reset() {
    selectedTab = 0;
}

void Menu::Draw() {
    const char* menuItems[] = { "Player", "Weapons", "Visuals", "Spawner", "Teleporter", "Settings" };

    ImGui::SetNextWindowSize(ImVec2(750, 500), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Palworld Internal [Aiden]", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, 180);

        // SIDEBAR
        ImGui::BeginChild("Sidebar", ImVec2(0, 0), true);
        ColoredSeparatorText("MENU", ImVec4(0.85f, 0.95f, 1.0f, 1.0f));
        ImGui::Spacing();
        for (int i = 0; i < IM_ARRAYSIZE(menuItems); ++i) {
            if (CustomButton(menuItems[i], ImVec2(-1, 40), selectedTab == i)) selectedTab = i;
            ImGui::Spacing();
        }
        ImGui::EndChild();
        ImGui::NextColumn();

        // CONTENT
        ImGui::BeginChild("Content", ImVec2(0, 0), true);

        switch (selectedTab) {
        case 0: // PLAYER
            ColoredSeparatorText("Character Stats", ImVec4(1, 1, 1, 1));

            ImGui::Checkbox("Infinite Stamina", &Features::bInfiniteStamina);
            ImGui::Spacing();

            ImGui::Checkbox("Attack Multiplier", &Player::bAttackMultiplier);
            if (Player::bAttackMultiplier) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(150);
                ImGui::SliderFloat("##AttackMod", &Player::fAttackModifier, 1.0f, 100.0f, "%.1fx");
            }

            ImGui::Checkbox("Weight Adjuster", &Player::bWeightAdjuster);
            if (Player::bWeightAdjuster) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(150);
                ImGui::InputFloat("##WeightMod", &Player::fWeightModifier, 1000.0f, 10000.0f, "%.0f");
            }

            ImGui::Spacing();
            ColoredSeparatorText("World Interaction", ImVec4(0.3f, 1.0f, 0.3f, 1));

            if (ImGui::Button("Reveal Map (Fog of War)", ImVec2(200, 30))) {
                Player::bUnlockMap = true;
            }

            if (ImGui::Button("Unlock All Fast Travel", ImVec2(200, 30))) {
                Player::bUnlockTowers = true;
            }

            if (ImGui::Button("Vacuum Relics (Lifemunks)", ImVec2(200, 30))) {
                Player::bCollectRelics = true;
            }
            break;

        case 1: // WEAPONS
            ColoredSeparatorText("Ammo Management", ImVec4(1, 1, 1, 1));

            if (ImGui::Checkbox("Infinite Ammo (Inventory)", &Features::bInfiniteAmmo)) {
                if (Features::bInfiniteAmmo) Features::bInfiniteMagazine = false;
            }

            if (ImGui::Checkbox("Infinite Magazine (Clip)", &Features::bInfiniteMagazine)) {
                if (Features::bInfiniteMagazine) Features::bInfiniteAmmo = false;
            }

            ImGui::Spacing();
            ColoredSeparatorText("Weapon Properties", ImVec4(1, 1, 1, 1));

            ImGui::Checkbox("Rapid Fire", &Features::bRapidFire);
            ImGui::Checkbox("No Recoil & Spread", &Features::bNoRecoil);
            ImGui::Checkbox("Infinite Durability", &Features::bInfiniteDurability);

            ImGui::Spacing();
            ColoredSeparatorText("Damage", ImVec4(1, 0.3f, 0.3f, 1));

            ImGui::Checkbox("Enable Damage Mod", &Features::bDamageHack);
            if (Features::bDamageHack) {
                ImGui::SliderInt("Multiplier", &Features::DamageMultiplier, 1, 100, "%dx");
            }
            break;

        case 2: // VISUALS
            ColoredSeparatorText("ESP", ImVec4(1, 1, 1, 1));
            ImGui::TextDisabled("Coming Soon...");
            break;

        case 3: // SPAWNER
            ItemSpawner::DrawTab();
            break;

        case 4: // TELEPORTER
        {
            auto pLocal = Hooking::GetLocalPlayerSafe();

            ColoredSeparatorText("Custom Waypoints", ImVec4(0.3f, 1.0f, 1.0f, 1));

            static char wpNameBuffer[64] = "My Base";
            ImGui::InputText("Name", wpNameBuffer, sizeof(wpNameBuffer));

            if (ImGui::Button("Save Current Location", ImVec2(-1, 30))) {
                if (pLocal) Teleporter::AddWaypoint(pLocal, wpNameBuffer);
            }

            ImGui::Spacing();
            ColoredSeparatorText("Saved Locations", ImVec4(1, 1, 1, 1));

            for (int i = 0; i < Teleporter::Waypoints.size(); ++i) {
                ImGui::PushID(i);

                if (ImGui::Button("TP")) {
                    if (pLocal) Teleporter::TeleportTo(pLocal, Teleporter::Waypoints[i].Location);
                }

                ImGui::SameLine();

                if (ImGui::Button("DEL")) {
                    Teleporter::DeleteWaypoint(i);
                }

                ImGui::SameLine();
                ImGui::Text("%s", Teleporter::Waypoints[i].Name.c_str());

                ImGui::PopID();
            }

            ImGui::Spacing();
            ColoredSeparatorText("Boss Teleports", ImVec4(1.0f, 0.5f, 0.5f, 1));
            if (ImGui::Button("Zoe & Grizzbolt")) { if (pLocal) Teleporter::TeleportToBoss(pLocal, 0); }
            ImGui::SameLine();
            if (ImGui::Button("Lily & Lyleen")) { if (pLocal) Teleporter::TeleportToBoss(pLocal, 1); }

        }
        break;

        case 5: // SETTINGS
            ColoredSeparatorText("Config", ImVec4(1, 1, 1, 1));
            ImGui::Text("Version 2.8 (Jarvis)");
            if (ImGui::Button("Unload Cheat")) {
                Hooking::Shutdown();
            }
            break;
        }

        ImGui::EndChild();
        ImGui::Columns(1);
    }
    ImGui::End();
}