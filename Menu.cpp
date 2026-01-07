#include "Menu.h"
#include "Features.h" 
#include "Hooking.h"
#include "ItemSpawner.h"
#include "Player.h"
#include "Teleporter.h"
#include "Visuals.h"
#include "SDKGlobal.h"
#include "imgui_style.h"

static int selectedTab = 0;

void Menu::InitTheme() { SetupImGuiStyle(); }
void Menu::Reset() { selectedTab = 0; }

void Menu::Draw() {
    const char* menuItems[] = { "Player", "Weapons", "Visuals", "Spawner", "Teleporter", "Settings" };
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
            ColoredSeparatorText("Character Stats", ImVec4(1, 1, 1, 1));
            ImGui::Checkbox("Infinite Stamina", &Features::bInfiniteStamina);
            ImGui::Spacing();
            ImGui::Checkbox("Attack Multiplier", &Player::bAttackMultiplier);
            if (Player::bAttackMultiplier) {
                ImGui::SameLine(); ImGui::SetNextItemWidth(150);
                ImGui::SliderFloat("##AttackMod", &Player::fAttackModifier, 1.0f, 100.0f, "%.1fx");
            }
            ImGui::Checkbox("Weight Adjuster", &Player::bWeightAdjuster);
            if (Player::bWeightAdjuster) {
                ImGui::SameLine(); ImGui::SetNextItemWidth(150);
                ImGui::InputFloat("##WeightMod", &Player::fWeightModifier, 1000.0f, 10000.0f, "%.0f");
            }
            ImGui::Spacing();
            ColoredSeparatorText("World Interaction", ImVec4(0.3f, 1.0f, 0.3f, 1));
            if (ImGui::Button("Reveal Map", ImVec2(200, 30))) Player::bUnlockMap = true;
            if (ImGui::Button("Unlock Fast Travel", ImVec2(200, 30))) Player::bUnlockTowers = true;
            if (ImGui::Button("Vacuum Relics", ImVec2(200, 30))) Player::bCollectRelics = true;
            break;

        case 1: // WEAPONS
            ColoredSeparatorText("Ammo Management", ImVec4(1, 1, 1, 1));
            if (ImGui::Checkbox("Infinite Ammo", &Features::bInfiniteAmmo)) Features::bInfiniteMagazine = !Features::bInfiniteAmmo;
            if (ImGui::Checkbox("Infinite Mag", &Features::bInfiniteMagazine)) Features::bInfiniteAmmo = !Features::bInfiniteMagazine;
            ImGui::Spacing();
            ColoredSeparatorText("Properties", ImVec4(1, 1, 1, 1));
            ImGui::Checkbox("Rapid Fire", &Features::bRapidFire);
            ImGui::Checkbox("No Recoil", &Features::bNoRecoil);
            ImGui::Checkbox("No Durability Loss", &Features::bInfiniteDurability);
            ImGui::Spacing();
            ColoredSeparatorText("Damage", ImVec4(1, 0.3f, 0.3f, 1));
            ImGui::Checkbox("Enable Damage Mod", &Features::bDamageHack);
            if (Features::bDamageHack) ImGui::SliderInt("Multiplier", &Features::DamageMultiplier, 1, 100, "%dx");
            break;

        case 2: // VISUALS
            ColoredSeparatorText("Camera", ImVec4(1, 1, 1, 1));
            if (ImGui::SliderFloat("FOV", &Visuals::fFOV, 60.0f, 140.0f, "%.0f")) Visuals::Apply();
            if (ImGui::SliderFloat("Gamma", &Visuals::fGamma, 0.1f, 5.0f, "%.1f")) Visuals::Apply();
            if (ImGui::Button("Reset", ImVec2(150, 30))) { Visuals::fFOV = 90.0f; Visuals::fGamma = 0.5f; Visuals::Apply(); }
            break;

        case 3: ItemSpawner::DrawTab(); break;

        case 4: // TELEPORTER
        {   // [FIX] Added Scoping Braces to fix C2360
            auto pLocal = Hooking::GetLocalPlayerSafe();
            ColoredSeparatorText("Base & Bosses", ImVec4(0.3f, 1.0f, 0.3f, 1));
            if (ImGui::Button("TP to Base", ImVec2(-1, 30))) if (pLocal) Teleporter::TeleportToHome(pLocal);

            if (ImGui::Button("Tower: Zoe & Grizzbolt")) if (pLocal) Teleporter::TeleportToBoss(pLocal, 0);
            if (ImGui::Button("Tower: Lily & Lyleen")) if (pLocal) Teleporter::TeleportToBoss(pLocal, 1);
            if (ImGui::Button("Tower: Axel & Orserk")) if (pLocal) Teleporter::TeleportToBoss(pLocal, 2);
            if (ImGui::Button("Tower: Marcus & Faleris")) if (pLocal) Teleporter::TeleportToBoss(pLocal, 3);
            if (ImGui::Button("Tower: Victor & Shadowbeak")) if (pLocal) Teleporter::TeleportToBoss(pLocal, 4);

            ImGui::Spacing();
            ColoredSeparatorText("Waypoints", ImVec4(0.3f, 1.0f, 1.0f, 1));
            static char wpName[64] = "My Point";
            ImGui::InputText("Name", wpName, 64);
            if (ImGui::Button("Save Pos", ImVec2(-1, 30))) if (pLocal) Teleporter::AddWaypoint(pLocal, wpName);

            ImGui::Spacing();
            for (int i = 0; i < Teleporter::Waypoints.size(); ++i) {
                ImGui::PushID(i);
                if (ImGui::Button("TP")) if (pLocal) Teleporter::TeleportTo(pLocal, Teleporter::Waypoints[i].Location);
                ImGui::SameLine();
                if (ImGui::Button("DEL")) Teleporter::DeleteWaypoint(i);
                ImGui::SameLine();
                ImGui::Text("%s", Teleporter::Waypoints[i].Name.c_str());
                ImGui::PopID();
            }
        } // [FIX] End Scope
        break;

        case 5: // SETTINGS
            ColoredSeparatorText("Config", ImVec4(1, 1, 1, 1));
            ImGui::Text("Version 3.0 (Jarvis)");
            if (ImGui::Button("Unload")) Hooking::Shutdown();
            break;
        }
        ImGui::EndChild();
        ImGui::Columns(1);
    }
    ImGui::End();
}