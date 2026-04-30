#include "headers/BitTool.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    // 1. CLI Mode (Build System Compatibility)
    if (argc > 1) {
        std::cout << "--- BitEngine Project Compiler ---" << std::endl;
        std::string configPath = argv[1];
        std::string outputPath = (argc > 2) ? argv[2] : "data.bin";
        
        DialogEngine engine;
        std::cout << "[BitTool] Loading project from: " << configPath << std::endl;
        if (engine.LoadProject(configPath)) {
            std::cout << "[BitTool] Success! Compiling to: " << outputPath << std::endl;
            engine.CompileProject(outputPath);
            std::cout << "[BitTool] Done." << std::endl;
            return 0;
        } else {
            std::cerr << "[BitTool] ERROR: Failed to load project." << std::endl;
            return 1;
        }
    }

    // 2. Visual Editor Mode (No arguments)
    const int screenWidth = 1280;
    const int screenHeight = 720;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "BitEngine - Visual Dialog Creator");
    SetTargetFPS(60);

    DialogEngine engine;
    engine.LoadProject("res/configs.json");

    BitEditor editor(engine);

    while (!WindowShouldClose()) {
        editor.Update();

        BeginDrawing();
        ClearBackground(Color{ 20, 20, 25, 255 });
        
        editor.Draw();

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
