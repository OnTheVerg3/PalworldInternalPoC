#include "Visuals.h"
#include "SDKGlobal.h"
#include <string>

namespace Visuals {
    float fFOV = 90.0f;
    float fGamma = 2.2f;

    void Apply() {
        // Set FOV
        std::string fovCmd = "fov " + std::to_string((int)fFOV);
        SDK::ExecuteConsoleCommand(fovCmd);

        // Set Gamma (Brightness)
        // r.TonemapperGamma default is usually ~2.2. Lowering it makes it darker, Raising makes it brighter/washed.
        // Actually, 'gamma' command usually works inversely or we use r.Color.Mid
        // Let's use r.TonemapperGamma
        std::string gammaCmd = "r.TonemapperGamma " + std::to_string(fGamma);
        SDK::ExecuteConsoleCommand(gammaCmd);
    }
}