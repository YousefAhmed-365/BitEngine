#include "headers/BitApp.hpp"
#include "headers/BitEngine.hpp"
#include "headers/BitRenderer.hpp"
#include "json.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

BitApp::BitApp() {
    LoadConfig("res/app.json");

    if (m_resizable) {
        SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    }
    
    InitWindow(m_width, m_height, m_title.c_str());
    SetWindowMinSize(m_minWidth, m_minHeight);
    SetTargetFPS(m_fps);
}

BitApp::~BitApp() {
    CloseWindow();
}

void BitApp::LoadConfig(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "[BitApp] Warning: " << path << " not found, using default app configuration." << std::endl;
        return;
    }
    try {
        json j; f >> j;
        m_title     = j.value("title", m_title);
        m_width     = j.value("width", m_width);
        m_height    = j.value("height", m_height);
        m_minWidth  = j.value("min_width", m_minWidth);
        m_minHeight = j.value("min_height", m_minHeight);
        m_fps       = j.value("fps", m_fps);
        m_resizable = j.value("resizable", m_resizable);
    } catch (const std::exception& e) {
        std::cerr << "[BitApp] Failed to parse " << path << ": " << e.what() << std::endl;
    }
}

void BitApp::Run() {
    DialogEngine dialogSystem;
    BitRenderer uiBridge(dialogSystem);

    bool loaded = false;
    if (FileExists("data.bin")) {
        loaded = dialogSystem.LoadCompiledProject("data.bin");
    } else {
        loaded = dialogSystem.LoadProject("res/configs.json");
    }

    uiBridge.GetStyleManager().LoadStyle("res/style.json");

    if (loaded) {
        dialogSystem.StartDialog();
    } else {
        std::cerr << "CRITICAL ERROR: Failed to load project." << std::endl;
    }

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        dialogSystem.Update(dt);

        int sw = GetScreenWidth();
        int sh = GetScreenHeight();

        // Style cycling
        if (IsKeyPressed(KEY_TAB)) {
            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
                uiBridge.GetStyleManager().PrevStyle();
            else
                uiBridge.GetStyleManager().NextStyle();
        }

        uiBridge.HandleInput();

        BeginDrawing();
        ClearBackground(Color{ 10, 10, 15, 255 });

        // Subtle background grid
        for (int i = 0; i < sw;  i += 80) DrawLine(i, 0, i, sh, Fade(Color{ 80, 80, 120, 255 }, 0.05f));
        for (int i = 0; i < sh; i += 80) DrawLine(0, i, sw, i,  Fade(Color{ 80, 80, 120, 255 }, 0.05f));

        DrawText("BITENGINE v2.0 - MODULAR CORE", 30, 30, 22, Color{ 80, 80, 120, 255 });

        uiBridge.Draw();

        // Style HUD
        {
            std::string styleName = uiBridge.GetStyleManager().GetCurrentStyleName();
            auto names = uiBridge.GetStyleManager().GetStyleNames();
            int total  = (int)names.size();
            int idx    = 0;
            for (int i = 0; i < total; ++i) {
                if (names[i] == styleName) { idx = i; break; }
            }

            std::string label = "STYLE [" + std::to_string(idx + 1) + "/" + std::to_string(total) + "]: " + styleName;
            int tw = MeasureText(label.c_str(), 14);
            DrawRectangle(20, sh - 46, tw + 24, 30, Fade(BLACK, 0.6f));
            DrawRectangleLines(20, sh - 46, tw + 24, 30, Fade(WHITE, 0.15f));
            DrawText(label.c_str(), 32, sh - 38, 14, Color{ 160, 160, 200, 255 });
            DrawText("Tab / Shift+Tab to switch", 32, sh - 18, 11, Fade(Color{ 120, 120, 160, 255 }, 0.8f));
        }

        EndDrawing();
    }
}
