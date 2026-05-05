#ifndef BIT_GRAPHICS_HPP
#define BIT_GRAPHICS_HPP

#include "raylib.h"
#include "UILayout.hpp"
#include "BitRichText.hpp"
#include <unordered_map>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// BitGraphics: Asset & Rendering Backend
// ─────────────────────────────────────────────────────────────────────────────
// Low-level graphics engine providing texture/font management and drawing.
//
// Key Features:
//   - Asset caching prevents redundant disk I/O
//   - Fallback textures for missing assets (graceful degradation)
//   - Rich text rendering with line wrapping and color tags
//   - Styled panel drawing with borders, rounding, and nine-slice scaling
//   - Vignette overlay for atmospheric effects
// ─────────────────────────────────────────────────────────────────────────────
class BitGraphics {
public:
    BitGraphics();
    ~BitGraphics();
    
    // Initialization
    void Initialize();
    void Shutdown();
    
    // Texture Management
    Texture2D GetTexture(const std::string& path);
    void UnloadTexture(const std::string& path);
    
    // Font Management
    Font GetFont(const std::string& path);
    
    // Drawing Utilities
    int DrawRichText(const std::vector<RichChar>& content, int limit,
                     int x, int y, int fontSize, int maxWidth,
                     Color defaultColor = RAYWHITE, int lineSpacing = 6,
                     Font font = {0});
    
    void DrawStyledPanel(Rectangle rect, const UIStyleBlock& style);
    
    // Special Textures
    Texture2D GetFallbackTexture() const { return m_fallbackTexture; }
    Texture2D GetVignetteTexture() const { return m_vignette; }
    Texture2D GetCustomCursorTexture() const { return m_customCursor; }
    void SetCustomCursorTexture(const std::string& path);

private:
    std::unordered_map<std::string, Texture2D> m_textureCache;
    std::unordered_map<std::string, Font> m_fontCache;
    
    Texture2D m_fallbackTexture = {};
    Texture2D m_vignette = {};
    Texture2D m_customCursor = {};
    
    void CreateFallbackTexture();
    void CreateVignetteTexture();
};

#endif
