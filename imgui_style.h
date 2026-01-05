#pragma once
#include "imgui.h"

// Sets up the "Moonlight" Dark Theme
void SetupImGuiStyle();

// Custom styled button for the sidebar
bool CustomButton(const char* label, ImVec2 size = ImVec2(0, 36), bool active = false);

// Stylish separator with text
void ColoredSeparatorText(const char* text, ImVec4 textColor, float thickness = 1.5f, float padding = 6.0f);