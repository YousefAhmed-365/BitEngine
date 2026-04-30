#include "raylib.h"
#include "headers/BitEngine.hpp"
#include "headers/BitDialog.hpp"
#include <iostream>

int main() {
    const int screenWidth  = 1280;
    const int screenHeight = 720;
    InitWindow(screenWidth, screenHeight, "BitEngine - Modular Architecture");
    SetTargetFPS(60);

    {
        DialogEngine dialogSystem;
        BitDialog uiBridge(dialogSystem);

        // --- Loading Hierarchy ---
        bool loaded = false;
        if (FileExists("data.bin")) {
            loaded = dialogSystem.LoadCompiledProject("data.bin");
        } else {
            loaded = dialogSystem.LoadProject("res/configs.json");
        }

        // Load UI styles — always from JSON, independent of project data
        uiBridge.LoadStyle("res/style.json");

        if (loaded) {
            dialogSystem.StartDialog();
        } else {
            std::cerr << "CRITICAL ERROR: Failed to load project." << std::endl;
        }

        // --- Main Game Loop ---
        while (!WindowShouldClose()) {
            float dt = GetFrameTime();
            dialogSystem.Update(dt);

            // --- Style cycling ---
            if (IsKeyPressed(KEY_TAB)) {
                if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
                    uiBridge.PrevStyle();
                else
                    uiBridge.NextStyle();
            }

            uiBridge.HandleInput();

            BeginDrawing();
            ClearBackground((Color){ 10, 10, 15, 255 });

            // Subtle grid
            for (int i = 0; i < screenWidth;  i += 80) DrawLine(i, 0, i, screenHeight, Fade((Color){ 80, 80, 120, 255 }, 0.05f));
            for (int i = 0; i < screenHeight; i += 80) DrawLine(0, i, screenWidth, i,  Fade((Color){ 80, 80, 120, 255 }, 0.05f));

            DrawText("BITENGINE v2.0 - MODULAR CORE", 30, 30, 22, (Color){ 80, 80, 120, 255 });

            uiBridge.Draw();

            // --- Style HUD (bottom-left) ---
            {
                std::string styleName = uiBridge.GetCurrentStyleName();
                auto names = uiBridge.GetStyleNames();
                int total  = (int)names.size();
                int idx    = 0;
                for (int i = 0; i < total; ++i) if (names[i] == styleName) { idx = i; break; }

                std::string label = "STYLE [" + std::to_string(idx + 1) + "/" + std::to_string(total) + "]: " + styleName;
                int tw = MeasureText(label.c_str(), 14);
                DrawRectangle(20, screenHeight - 46, tw + 24, 30, Fade(BLACK, 0.6f));
                DrawRectangleLines(20, screenHeight - 46, tw + 24, 30, Fade(WHITE, 0.15f));
                DrawText(label.c_str(), 32, screenHeight - 38, 14, (Color){ 160, 160, 200, 255 });
                DrawText("Tab / Shift+Tab to switch", 32, screenHeight - 18, 11, Fade((Color){ 120, 120, 160, 255 }, 0.8f));
            }

            EndDrawing();
        }
    }

    CloseWindow();
    return 0;
}