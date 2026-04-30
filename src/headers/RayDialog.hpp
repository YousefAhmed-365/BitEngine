#ifndef RAY_DIALOG_HPP
#define RAY_DIALOG_HPP

#include "raylib.h"
#include "BitEngine.hpp"
#include <map>

// RayDialog - A Raylib-based bridge for BitEngine
class RayDialog {
public:
    RayDialog(DialogEngine& engine);
    ~RayDialog();
    
    void Draw();
    void HandleInput();

private:
    DialogEngine& m_engine;

    // Cache for textures
    std::map<std::string, Texture2D> m_textureCache;
    Texture2D m_fallbackTexture;

    // Animation state
    float m_animTimer = 0.0f;
    int m_animFrame = 0;
    std::string m_lastSpritePath = "";

    // UI Drawing helpers
    void DrawMainBox();
    void DrawChoiceBox();
    void DrawDebugOverlay();
    void DrawEntitySprite();
    
    // Internal helpers
    void DrawTextWrapped(const std::string& text, int x, int y, int fontSize, int maxWidth, Color color);
    Texture2D GetTexture(const std::string& path);
    void CreateFallbackTexture();
};

#endif
