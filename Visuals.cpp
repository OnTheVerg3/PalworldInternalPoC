#include "Visuals.h"
#include "SDKGlobal.h"
#include "Hooking.h"
#include <string>

namespace Visuals {
    float fFOV = 90.0f;
    float fGamma = 1.0f; // 1.0 is default/neutral here

    void Apply() {
        std::string fovCmd = "fov " + std::to_string((int)fFOV);
        SDK::ExecuteConsoleCommand(fovCmd);
    }

    void Update() {
        // [FIX] Direct Camera Component Modification (Game Thread)
        // This is how the provided 'cheat_state.cpp' did it.
        SDK::APalPlayerCharacter* pLocal = Hooking::GetLocalPlayerSafe();
        if (!SDK::IsValidObject(pLocal) || !SDK::IsValidObject(pLocal->FollowCamera)) return;

        auto* Camera = pLocal->FollowCamera;

        // FOV (Continuous enforcement)
        Camera->WalkFOV = fFOV;
        Camera->SprintFOV = fFOV;
        Camera->AimFOV = fFOV;

        // Brightness (AutoExposureBias)
        // Enabled override and set value
        Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
        Camera->PostProcessBlendWeight = 1.0f;
        Camera->PostProcessSettings.AutoExposureBias = fGamma;
    }
}