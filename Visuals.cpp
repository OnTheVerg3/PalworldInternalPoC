#include "Visuals.h"
#include "SDKGlobal.h"
#include <string>

namespace Visuals {
    float fFOV = 90.0f;
    float fGamma = 0.5f;

    void Apply() {
        std::string fovCmd = "fov " + std::to_string((int)fFOV);
        SDK::ExecuteConsoleCommand(fovCmd);

        std::string gammaCmd = "r.Color.Mid " + std::to_string(fGamma);
        SDK::ExecuteConsoleCommand(gammaCmd);
    }
}