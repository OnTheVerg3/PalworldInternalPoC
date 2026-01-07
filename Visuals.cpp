#include "Visuals.h"
#include "SDKGlobal.h"
#include "Hooking.h" // Contains IsValidObject
#include <string>

namespace Visuals {
    float fFOV = 90.0f;
    float fGamma = 1.0f;

    void Apply() {
        // Enforce FOV
        std::string fovCmd = "fov " + std::to_string((int)fFOV);
        SDK::ExecuteConsoleCommand(fovCmd);

        // Enforce Gamma (Midtones)
        // Lower values = Brighter in UE5 r.Color.Mid
        std::string gammaCmd = "r.Color.Mid " + std::to_string(fGamma);
        SDK::ExecuteConsoleCommand(gammaCmd);
    }

    void Update() {
        // Continuous enforcement via Console Commands is safer given the SDK issues
        // We throttle this to avoid spamming the console every tick if needed, 
        // but for now, we just rely on Apply() being called from UI or periodically.

        // If you want continuous enforcement, uncomment this, 
        // but ConsoleCommands are heavy. Better to call Apply() on change.
        // Apply(); 
    }
}