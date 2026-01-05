#include "imgui_style.h"
#include "imgui.h"
#include "imgui_internal.h" // Needed for GetContentRegionAvail

void SetupImGuiStyle()
{
    // Moonlight style port
    ImGuiStyle& style = ImGui::GetStyle();

    style.Alpha = 1.0f;
    style.DisabledAlpha = 1.0f;
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.WindowRounding = 5.0f;
    style.WindowBorderSize = 0.0f;
    style.WindowMinSize = ImVec2(20.0f, 20.0f);
    style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_Right;
    style.ChildRounding = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupRounding = 0.0f;
    style.PopupBorderSize = 1.0f;
    style.FramePadding = ImVec2(20.0f, 3.4f);
    style.FrameRounding = 5.0f;
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(4.3f, 5.5f);
    style.ItemInnerSpacing = ImVec2(7.1f, 1.8f);
    style.CellPadding = ImVec2(12.1f, 9.2f);
    style.IndentSpacing = 0.0f;
    style.ColumnsMinSpacing = 4.9f;
    style.ScrollbarSize = 11.6f;
    style.ScrollbarRounding = 10.0f;
    style.GrabMinSize = 3.7f;
    style.GrabRounding = 20.0f;
    style.TabRounding = 6.0f;
    style.TabBorderSize = 0.0f;
    style.TabBarBorderSize = 0.0f;
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

    style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.27f, 0.32f, 0.45f, 1.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.09f, 0.10f, 0.9f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.10f, 0.12f, 0.9f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.09f, 0.10f, 0.9f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.16f, 0.17f, 0.19f, 0.9f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.08f, 0.09f, 0.10f, 0.9f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.05f, 0.07f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.05f, 0.05f, 0.07f, 1.0f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.09f, 0.10f, 1.0f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.11f, 0.12f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.07f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.12f, 0.13f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.16f, 0.17f, 0.19f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.12f, 0.13f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.9f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.12f, 0.13f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.19f, 0.20f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.14f, 0.16f, 0.21f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.11f, 0.11f, 0.11f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.08f, 0.09f, 0.10f, 1.0f);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.13f, 0.15f, 0.19f, 1.0f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.16f, 0.18f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.16f, 0.18f, 0.25f, 1.0f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.97f, 1.0f, 0.50f, 1.0f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.08f, 0.09f, 0.10f, 1.0f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.12f, 0.13f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.12f, 0.13f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.08f, 0.09f, 0.10f, 1.0f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.12f, 0.27f, 0.57f, 1.0f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.52f, 0.60f, 0.70f, 1.0f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.04f, 0.98f, 0.98f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.88f, 0.79f, 0.56f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.96f, 0.96f, 0.96f, 1.0f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.0f);
    style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.50f, 0.51f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.27f, 0.29f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.50f, 0.51f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.20f, 0.18f, 0.55f, 0.50f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.18f, 0.55f, 0.50f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.9f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.95f, 0.6f, 1.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.13f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.14f, 0.15f, 0.17f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.16f, 0.17f, 0.2f, 1.0f);
}

bool CustomButton(const char* label, ImVec2 size, bool active)
{
    if (size.x == 0) size.x = 0; // Auto width
    if (size.y == 0) size.y = 36; // Default height

    ImVec4 baseColor = active ? ImVec4(0.17f, 0.17f, 0.17f, 1.0f) : ImVec4(0.13f, 0.13f, 0.13f, 1.0f);
    ImVec4 hoverColor = active ? ImVec4(0.20f, 0.20f, 0.20f, 1.0f) : ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
    ImVec4 activeColor = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
    ImVec4 textColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    ImVec4 borderColor = active ? ImVec4(1.0f, 0.84f, 0.0f, 0.8f) : ImVec4(0, 0, 0, 0);

    ImGui::PushStyleColor(ImGuiCol_Button, baseColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    ImGui::PushStyleColor(ImGuiCol_Border, borderColor);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, active ? 1.0f : 0.0f);

    bool clicked = ImGui::Button(label, size);

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(5);

    return clicked;
}

void ColoredSeparatorText(const char* text, ImVec4 textColor, float thickness, float padding)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return;

    ImGuiStyle& style = ImGui::GetStyle();
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    float textWidth = ImGui::CalcTextSize(text).x;
    float textHeight = ImGui::GetTextLineHeight();

    ImVec2 textPos = cursorPos;
    textPos.y += -2.0f;

    float lineStartX = textPos.x + textWidth + 8.0f;
    float lineEndX = cursorPos.x + ImGui::GetContentRegionAvail().x;
    float lineY = textPos.y + textHeight * 0.5f;

    ImGui::Dummy(ImVec2(0.0f, textHeight + -2.0f + 2.0f));

    window->DrawList->AddLine(
        ImVec2(lineStartX, lineY),
        ImVec2(lineEndX, lineY),
        ImGui::GetColorU32(ImGuiCol_Separator),
        thickness
    );

    ImGui::SetCursorScreenPos(textPos);
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}