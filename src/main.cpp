#include "overlay.h"
#include <iostream>
#include <thread>

// Simple mock structs for game memory offsets
namespace GameState {
    bool espEnabled = true;
    bool boxEsp = true;
    bool skeletonEsp = false;
    bool snaplines = true;
}

int main() {
    std::cout << "[*] Starting oxem.gg ESP..." << std::endl;
    
    // Initialize Overlay
    if (!Overlay::Initialize()) {
        std::cout << "[-] Failed to initialize overlay.\n";
        return 1;
    }

    std::cout << "[+] Overlay initialized successfully!\n";
    std::cout << "[+] Press INSERT to toggle Menu.\n";

    // Main loop
    while (!Overlay::ShouldClose()) {
        Overlay::BeginFrame();

        // 1. Draw ESP if enabled
        if (GameState::espEnabled) {
            // TODO: Read memory here and get W2S locations
            
            // For showcase, drawing simple placeholders
            if (GameState::boxEsp) {
                // Top, Left, Right, Bottom Box lines... 
                // ImGui::GetBackgroundDrawList()->AddRect(...)
            }
            if (GameState::snaplines) {
                // Draw line to bottom of screen
            }
        }

        // 2. Draw Menu
        Overlay::RenderMenu();

        Overlay::EndFrame();
    }

    Overlay::Shutdown();
    return 0;
}
