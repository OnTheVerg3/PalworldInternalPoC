#include "Visuals.h"
#include "SDKGlobal.h"
#include <string>

namespace Visuals {
    float fFOV = 90.0f;
    float fGamma = 0.5f; // Default for r.Color.Mid

    void Apply() {
        // Set FOV
        std::string fovCmd = "fov " + std::to_string((int)fFOV);
        SDK::ExecuteConsoleCommand(fovCmd);

        // Set Gamma (Brightness) using Midtones
        // 0.5 is neutral. Higher = Brighter shadows.
        std::string gammaCmd = "r.Color.Mid " + std::to_string(fGamma);
        SDK::ExecuteConsoleCommand(gammaCmd);
    }
}