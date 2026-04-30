#ifndef RAY_DIALOG_HPP
#define RAY_DIALOG_HPP

#include "raylib.h"
#include "BitEngine.hpp"

// RayDialog - A Raylib-based bridge for BitEngine
class RayDialog {
public:
    RayDialog(DialogEngine& engine);
    
    void Draw();
    void HandleInput();

private:
    DialogEngine& m_engine;

    // UI Drawing helpers
    void DrawMainBox();
    void DrawChoiceBox();
    void DrawDebugOverlay();
    
    // Internal wrapping helper
    void DrawTextWrapped(const char* text, int x, int y, int fontSize, int maxWidth, Color color);
};

#endif
