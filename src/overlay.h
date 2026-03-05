#pragma once
#include <windows.h>

namespace Overlay {
    // Initialization and Cleanup
    bool Initialize();
    void Shutdown();

    // Loop logic
    bool ShouldClose();
    void BeginFrame();
    void RenderMenu();
    void EndFrame();
    
    // Window settings
    extern HWND hwnd;
    extern int width;
    extern int height;
}

namespace GameState {
    extern bool espEnabled;
    extern bool boxEsp;
    extern bool skeletonEsp;
    extern bool snaplines;
    extern bool healthEsp;
    extern bool distanceEsp;
    extern bool aimbot;
    extern bool silentAim;
    extern bool ignoreDead;
    extern bool teamCheck;
    extern bool aimPrediction;
    extern bool autoShoot;
    extern bool wallCheck;
    extern float aimSmooth;
    extern float aimFov;
    extern int aimBone;
}
