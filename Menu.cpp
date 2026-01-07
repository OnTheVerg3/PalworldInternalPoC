#include "Menu.h"
#include "Features.h" 
#include "Hooking.h"
#include "ItemSpawner.h"
#include "Player.h"
#include "SDKGlobal.h"
#include "imgui_style.h"

static int selectedTab = 0;
static std::vector<Player::PlayerCandidate> g_PlayerList;
static int g_SelectedPlayerIdx = -1;

void Menu::InitTheme() { SetupImGuiStyle(); }
void Menu::Reset() { selectedTab = 0; g_PlayerList.clear(); g_SelectedPlayerIdx = -1; }

void Menu::Draw() {
    // [FIX] Removed Visuals/Teleporter
    const char* menuItems[] = { "Player", "Weapons", "Spawner", "Settings" };

    ImGui::SetNextWindowSize(ImVec2(750, 500), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Palworld Internal [Aiden]", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, 180);

        ImGui::BeginChild("Sidebar", ImVec2(0, 0), true);
        ColoredSeparatorText("MENU", ImVec4(0.85f, 0.95f, 1.0f, 1.0f));
        ImGui::Spacing();
        for (int i = 0; i < IM_ARRAYSIZE(menuItems); ++i) {
            if (CustomButton(menuItems[i], ImVec2(-1, 40), selectedTab == i)) selectedTab = i;
            ImGui::Spacing();
        }
        ImGui::EndChild();
        ImGui::NextColumn();

        ImGui::BeginChild("Content", ImVec2(0, 0), true);

        switch (selectedTab) {
        case 0: // PLAYER
            ColoredSeparatorText("Target Selector", ImVec4(1, 0.5f, 0.0f, 1));
            if (ImGui::Button("Refresh Players", ImVec2(-1, 30))) {
                g_PlayerList = Player::GetPlayerCandidates();
                g_SelectedPlayerIdx = -1;
            }
            if (!g_PlayerList.empty()) {
                std::string preview = (g_SelectedPlayerIdx >= 0) ? g_PlayerList[g_SelectedPlayerIdx].DisplayString : "Select Player...";
                if (ImGui::BeginCombo("##PlayerSelect", preview.c_str())) {
                    for (int i = 0; i < g_PlayerList.size(); i++) {
                        if (ImGui::Selectable(g_PlayerList[i].DisplayString.c_str(), g_SelectedPlayerIdx == i)) {
                            g_SelectedPlayerIdx = i;
                            Hooking::SetManualPlayer(g_PlayerList[i].Ptr);
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            else ImGui::TextDisabled("No players found.");

            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            ColoredSeparatorText("Stats & World", ImVec4(1, 1, 1, 1));
            ImGui::Checkbox("Infinite Stamina", &Features::bInfiniteStamina);
            ImGui::Checkbox("Attack Multiplier", &Player::bAttackMultiplier);
            if (Player::bAttackMultiplier) { ImGui::SameLine(); ImGui::SetNextItemWidth(150); ImGui::SliderFloat("##AttackMod", &Player::fAttackModifier, 1.0f, 100.0f, "%.1fx"); }
            ImGui::Checkbox("Weight Adjuster", &Player::bWeightAdjuster);
            if (Player::bWeightAdjuster) { ImGui::SameLine(); ImGui::SetNextItemWidth(150); ImGui::InputFloat("##WeightMod", &Player::fWeightModifier, 1000.0f, 10000.0f, "%.0f"); }
            ImGui::Spacing();
            if (ImGui::Button("Reveal Map", ImVec2(200, 30))) Player::bUnlockMap = true;
            if (ImGui::Button("Unlock Fast Travel", ImVec2(200, 30))) Player::bUnlockTowers = true;
            if (ImGui::Button("Vacuum Relics", ImVec2(200, 30))) Player::bCollectRelics = true;
            break;

        case 1: // WEAPONS
            ColoredSeparatorText("Combat", ImVec4(1, 1, 1, 1));
            if (ImGui::Checkbox("Infinite Ammo", &Features::bInfiniteAmmo)) Features::bInfiniteMagazine = !Features::bInfiniteAmmo;
            if (ImGui::Checkbox("Infinite Mag", &Features::bInfiniteMagazine)) Features::bInfiniteAmmo = !Features::bInfiniteMagazine;
            ImGui::Checkbox("No Durability Loss", &Features::bInfiniteDurability);
            ImGui::Spacing();
            ImGui::Checkbox("Damage Mod", &Features::bDamageHack);
            if (Features::bDamageHack) { ImGui::SameLine(); ImGui::SliderInt("##DmgMult", &Features::DamageMultiplier, 1, 100, "%dx"); }
            break;

        case 2: // SPAWNER
            ItemSpawner::DrawTab();
            break;

        case 3: // SETTINGS
            ColoredSeparatorText("Config", ImVec4(1, 1, 1, 1));
            ImGui::Text("Version 3.9 (Jarvis)");
            if (ImGui::Button("Unload")) Hooking::Shutdown();
            break;
        }
        ImGui::EndChild();
        ImGui::Columns(1);
    }
    ImGui::End();
}