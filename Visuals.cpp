#include "Visuals.h"
#include "SDKGlobal.h"
#include "Hooking.h"
#include <string>

namespace Visuals {
    float fFOV = 90.0f;
    float fGamma = 1.0f;

    void Apply() {
        // Enforce FOV
        std::string fovCmd = "fov " + std::to_string((int)fFOV);
        SDK::ExecuteConsoleCommand(fovCmd);

        // Enforce Gamma
        std::string gammaCmd = "r.Color.Mid " + std::to_string(fGamma);
        SDK::ExecuteConsoleCommand(gammaCmd);
    }

    void Update() {
        // [FIX] Disabled direct update to prevent 0x338 crash.
        // Use Apply() triggered from UI instead.
    }
}