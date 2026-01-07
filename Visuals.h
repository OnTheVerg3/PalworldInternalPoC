#pragma once

namespace Visuals {
    extern float fFOV;
    extern float fGamma;

    // Apply console vars (FOV)
    void Apply();

    // Called every tick to maintain camera hacks
    void Update();
}