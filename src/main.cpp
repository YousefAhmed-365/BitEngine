#include "raylib.h"
#include "BitEngine.hpp"
#include "RayDialog.hpp"
#include <iostream>

int main() {
    // 1. Initialize Window
    const int screenWidth = 1280;
    const int screenHeight = 720;
    InitWindow(screenWidth, screenHeight, "BitEngine - Modular Architecture");
    SetTargetFPS(60);

    // 2. Setup Engine and Bridge
    DialogEngine dialogSystem;
    RayDialog uiBridge(dialogSystem);

    // 3. Load Project
    if (dialogSystem.LoadProject("res/configs.json")) {
        dialogSystem.StartDialog();
    } else {
        std::cerr << "CRITICAL ERROR: Failed to load res/configs.json" << std::endl;
    }

    // 4. Main Game Loop
    while (!WindowShouldClose()) {
        // Update Logic
        float dt = GetFrameTime();
        dialogSystem.Update(dt);
        uiBridge.HandleInput();

        // Rendering
        BeginDrawing();
        ClearBackground((Color){ 10, 10, 15, 255 }); 
        
        // Background Deco
        for (int i = 0; i < screenWidth; i += 80) DrawLine(i, 0, i, screenHeight, Fade((Color){ 80, 80, 120, 255 }, 0.05f));
        for (int i = 0; i < screenHeight; i += 80) DrawLine(0, i, screenWidth, i, Fade((Color){ 80, 80, 120, 255 }, 0.05f));
        
        DrawText("BITENGINE v2.0 - MODULAR CORE", 30, 30, 22, (Color){ 80, 80, 120, 255 });
        
        // Draw Dialog UI using the bridge
        uiBridge.Draw();
        
        EndDrawing();
    }

    CloseWindow();
    return 0;
}