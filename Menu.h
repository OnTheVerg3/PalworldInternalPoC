#pragma once
#include "imgui.h"

namespace Menu {
    // Styling
    void InitTheme();

    // Main Draw Function
    void Draw();

    // [NEW] Resets Menu State (Tabs)
    void Reset();
}