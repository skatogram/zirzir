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
    extern bool showMenu;
}
