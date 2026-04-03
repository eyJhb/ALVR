#include "ALVR-common/packet_types.h"
#include "Logger.h"
#include "bindings.h"
#include <memory>
#include <mutex>

#ifndef __APPLE__
// Workaround symbol clash in openvr.h / openvr_driver.h
namespace alvr_chaperone {
#include <openvr.h>
}
using namespace alvr_chaperone;
#endif

std::mutex chaperone_mutex;

bool isOpenvrInit = false;

void InitOpenvrClient() {
    Debug("InitOpenvrClient: enter");

#ifndef __APPLE__
    std::unique_lock<std::mutex> lock(chaperone_mutex);

    if (isOpenvrInit) {
        Debug("InitOpenvrClient: already initialized, skipping");
        return;
    }

    vr::EVRInitError error;
    // Background needed for VRCompositor()->GetTrackingSpace()
    Debug("InitOpenvrClient: calling vr::VR_Init(VRApplication_Background)");
    vr::VR_Init(&error, vr::VRApplication_Background);
    Debug("InitOpenvrClient: vr::VR_Init returned, error=%d", (int)error);

    if (error != vr::VRInitError_None) {
        Warn("Failed to init OpenVR client! Error: %d", error);
        return;
    }
    isOpenvrInit = true;
    Debug("InitOpenvrClient: success");
#endif
}

void ShutdownOpenvrClient() {
    Debug("ShutdownOpenvrClient");

#ifndef __APPLE__
    std::unique_lock<std::mutex> lock(chaperone_mutex);

    if (!isOpenvrInit) {
        return;
    }

    isOpenvrInit = false;
    vr::VR_Shutdown();
#endif
}

bool IsOpenvrClientReady() { return isOpenvrInit; }

void _SetChaperoneArea(float areaWidth, float areaHeight) {
    Debug("SetChaperoneArea: enter (%.1f x %.1f)", areaWidth, areaHeight);

#ifndef __APPLE__
    std::unique_lock<std::mutex> lock(chaperone_mutex);

    const vr::HmdMatrix34_t MATRIX_IDENTITY
        = { { { 1.0, 0.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0, 0.0 }, { 0.0, 0.0, 1.0, 0.0 } } };

    float perimeterPoints[4][2];

    perimeterPoints[0][0] = -1.0f * areaWidth;
    perimeterPoints[0][1] = -1.0f * areaHeight;
    perimeterPoints[1][0] = -1.0f * areaWidth;
    perimeterPoints[1][1] = 1.0f * areaHeight;
    perimeterPoints[2][0] = 1.0f * areaWidth;
    perimeterPoints[2][1] = 1.0f * areaHeight;
    perimeterPoints[3][0] = 1.0f * areaWidth;
    perimeterPoints[3][1] = -1.0f * areaHeight;

    auto setup = vr::VRChaperoneSetup();

    if (setup != nullptr) {
        Debug("SetChaperoneArea: setting perimeter/pose/size");
        vr::VRChaperoneSetup()->SetWorkingPerimeter(
            reinterpret_cast<vr::HmdVector2_t*>(perimeterPoints), 4
        );
        vr::VRChaperoneSetup()->SetWorkingStandingZeroPoseToRawTrackingPose(&MATRIX_IDENTITY);
        vr::VRChaperoneSetup()->SetWorkingSeatedZeroPoseToRawTrackingPose(&MATRIX_IDENTITY);
        vr::VRChaperoneSetup()->SetWorkingPlayAreaSize(areaWidth, areaHeight);
        Debug("SetChaperoneArea: calling CommitWorkingCopy(EChaperoneConfigFile_Live)");
        vr::VRChaperoneSetup()->CommitWorkingCopy(vr::EChaperoneConfigFile_Live);
        Debug("SetChaperoneArea: CommitWorkingCopy done");
    } else {
        Warn("SetChaperoneArea: VRChaperoneSetup() returned null, skipping");
    }

    auto settings = vr::VRSettings();

    if (settings != nullptr) {
        // Hide SteamVR Chaperone
        Debug("SetChaperoneArea: hiding chaperone bounds");
        vr::VRSettings()->SetFloat(
            vr::k_pch_CollisionBounds_Section, vr::k_pch_CollisionBounds_FadeDistance_Float, 0.0f
        );
    }

    Debug("SetChaperoneArea: done");
#endif
}

#ifdef __linux__
std::unique_ptr<vr::HmdMatrix34_t> GetInvZeroPose() {
    Debug("GetInvZeroPose");

    std::unique_lock<std::mutex> lock(chaperone_mutex);
    if (!isOpenvrInit) {
        return nullptr;
    }
    auto mat = std::make_unique<vr::HmdMatrix34_t>();
    // revert pulls live into working copy
    vr::VRChaperoneSetup()->RevertWorkingCopy();
    auto compositor = vr::VRCompositor();
    if (compositor == nullptr) {
        return nullptr;
    }
    if (compositor->GetTrackingSpace() == vr::TrackingUniverseStanding) {
        vr::VRChaperoneSetup()->GetWorkingStandingZeroPoseToRawTrackingPose(mat.get());
    } else {
        vr::VRChaperoneSetup()->GetWorkingSeatedZeroPoseToRawTrackingPose(mat.get());
    }
    return mat;
}
#endif
