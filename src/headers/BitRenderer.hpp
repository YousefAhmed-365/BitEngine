#ifndef BIT_RENDERER_HPP
#define BIT_RENDERER_HPP

#include "raylib.h"
#include "BitEngine.hpp"
#include <unordered_map>
#include <map>
#include <vector>
#include <string>

// ============================================================
// Texture override for any UI element.
// If 'path' is empty the element falls back to its default rendering.
// When 'nineSlice' is true the texture is stretched using Raylib's
// NPatch system so borders don't distort on resize.
// ============================================================
struct StyleTexture {
    std::string path      = "";                       // File path; empty = disabled
    bool        nineSlice = false;                    // Use nine-slice (NPatch) scaling
    int         sliceLeft = 0, sliceTop    = 0,       // Border sizes in pixels
                sliceRight= 0, sliceBottom = 0;
    Color       tint      = { 255, 255, 255, 255 };   // Multiplicative tint
};

// ============================================================
// Full UI Style — all values are data-driven from style.json
// ============================================================
struct UIStyle {

    // --- Dialog Box ---
    float boxNormX         = 0.5f;
    float boxNormY         = 1.0f;
    float boxWidthNorm     = 0.93f;
    float boxHeight        = 190.0f;
    float boxHeightNorm    = 0.0f; // 0 means use fixed boxHeight
    float boxMarginBottom  = 20.0f;
    float boxRoundness     = 0.05f;
    float boxBorderThick   = 3.0f;
    int   boxPadding       = 40;
    std::string boxAnchor  = "bottom";
    Color boxBg            = { 15,  15,  25,  240 };
    Color boxBorder        = {  0, 210, 255, 255 };

    // --- Dialog Text ---
    Color textColor        = { 245, 245, 255, 255 };
    int   textFontSize     = 24;
    int   textLineSpacing  = 5;

    // --- Name Label ---
    float labelOffsetX     = 30.0f;
    float labelOffsetY     = -20.0f;
    int   labelPadding     = 20;
    int   labelHeight      = 40;
    std::string labelAlign = "left";
    Color labelBg          = { 15,  15,  25,  240 };
    Color labelBorder      = {  0, 210, 255, 255 };
    Color labelTextColor   = { 255, 215,   0, 255 };
    int   labelFontSize    = 28;

    // --- Choice Box ---
    float choiceNormX      = 0.5f;
    float choiceNormY      = 0.5f;
    float choiceOffsetY    = -100.0f;
    float choiceWidth      = 440.0f;
    float choiceRoundness  = 0.1f;
    float choiceBorderThick= 2.0f;
    Color choiceBg         = { 15,  15,  25,  242 };
    Color choiceBorder     = {  0, 210, 255, 255 };
    Color optionColor      = {  0, 180, 255, 255 };
    Color optionHover      = { 255, 255, 255, 255 };
    Color optionPremium    = { 255,   0, 255, 255 };
    int   optionFontSize   = 20;
    int   optionHeight     = 35;
    int   optionGap        = 10;

    // --- Toast Notification ---
    float toastNormX       = 1.0f;
    float toastNormY       = 0.0f;
    float toastMarginX     = 20.0f;
    float toastMarginY     = 20.0f;
    float toastWidth       = 200.0f;
    float toastHeight      = 40.0f;
    Color toastBg          = { 15,  15,  25,  240 };
    Color toastBorder      = {  0, 210, 255, 255 };
    Color toastTextColor   = { 245, 245, 255, 255 };
    int   toastFontSize    = 16;

    // --- Vignette ---
    float vignetteOpacity  = 0.4f;

    // --- Font (Feature 1) ---
    // Path to a TTF/OTF font file. Empty = Raylib built-in default font.
    std::string fontPath   = "";

    // --- Entity Display (Feature 2) ---
    float entityScale          = 3.0f;   // Sprite render scale multiplier
    float entityFloatAmplitude = 10.0f;  // Pixels of vertical floating movement
    float entityFloatSpeed     = 2.0f;   // Hz of the floating sine wave
    float entityShadowOpacity  = 0.4f;   // Alpha of the ellipse shadow beneath sprites

    // --- Cursor / Waiting Indicator (Feature 3) ---
    // shape: "triangle" | "dot" | "bar"
    std::string cursorShape     = "triangle";
    Color       cursorColor     = {  0, 210, 255, 255 };
    float       cursorSize      = 8.0f;
    float       cursorAnimSpeed = 10.0f;

    // --- Background Clear Color (Feature 5) ---
    // Drawn as the very first layer of each frame — eliminates ghosting when no bg image is set.
    Color clearColor            = { 18, 18, 30, 255 };

    // --- Texture Overrides ---
    // For each element: if path is non-empty the texture is used instead of the
    // default solid/rounded-rect rendering. Nine-slice is recommended for panels
    // so borders scale correctly at any dialog box size.
    StyleTexture boxTexture;     // Dialog box panel
    StyleTexture labelTexture;   // Character name label
    StyleTexture choiceTexture;  // Choice/options panel
    StyleTexture toastTexture;   // Toast notification panel
    StyleTexture cursorTexture;  // Waiting-for-input cursor sprite
};

class StyleManager {
public:
    StyleManager() = default;
    ~StyleManager();  // Unloads cached fonts

    void LoadStyle(const std::string& path);  // Load all named styles from file; enables hot-reload
    void Update();                            // Hot-reload check (call every frame)
    void Shutdown();                          // Explicitly unload GPU resources (call before CloseWindow)

    void SetStyle(const std::string& name);   // Switch to a named style block
    void NextStyle();                         // Cycle forward through loaded styles
    void PrevStyle();                         // Cycle backward through loaded styles

    std::string GetCurrentStyleName() const { return m_currentStyleName; }
    std::vector<std::string> GetStyleNames() const { return m_styleOrder; }
    UIStyle& GetStyle() { return m_activeStyle; }

    // Returns the loaded Font for the active style, or Raylib's default if none is configured.
    Font GetCurrentFont() const;

private:
    UIStyle m_activeStyle;

    std::unordered_map<std::string, UIStyle> m_styleLibrary;
    std::unordered_map<std::string, Font>    m_fontCache;  // Owns all loaded Font GPU resources
    std::vector<std::string>  m_styleOrder;
    std::string               m_currentStyleName;
    std::string               m_stylePath;
    long                      m_styleLastModTime = 0;
    float                     m_hotReloadTimer   = 0.0f;
};

class BitRenderer {
public:
    BitRenderer(DialogEngine& engine);
    virtual ~BitRenderer();

    void Draw();
    void HandleInput();
    void PreloadAssets();  // Eagerly load all assets from engine registries

    // Expose StyleManager for external configuration / input handling
    StyleManager& GetStyleManager() { return m_styleManager; }

protected:
    virtual void DrawBackground();
    virtual void DrawMainBox();
    virtual void DrawChoiceBox();
    virtual void DrawEntitySprites();
    virtual void DrawVFX();
    virtual void DrawDebugOverlay();
    virtual void HandleAudio();

    void DrawRichText(const std::vector<RichChar>& content, int limit, int x, int y, int fontSize, int maxWidth, Color defaultColor, int lineSpacing);
    Texture2D GetTexture(const std::string& path);
    void PlaySFX(const std::string& path);
    void CreateFallbackTexture();
    void CreateVignetteTexture();
    // Draw a rectangle using a StyleTexture if set, otherwise use solid/rounded-rect fallback.
    // When the texture is active no separate border is drawn (the texture provides it).
    void DrawStyledRect(Rectangle rect, const StyleTexture& stex,
                        Color fallbackBg, Color fallbackBorder,
                        float roundness = 0.0f, float borderThick = 1.5f, int segments = 8);

    DialogEngine& m_engine;
    StyleManager  m_styleManager;

    std::unordered_map<std::string, Texture2D> m_textureCache;
    std::unordered_map<std::string, Music>     m_musicCache;
    std::unordered_map<std::string, Sound>     m_sfxCache;
    std::string m_currentMusicPath = "";
    Music       m_currentMusic;
    bool        m_isMusicPlaying = false;

    Texture2D m_fallbackTexture;
    Texture2D m_vignette;

    float m_animTimer    = 0.0f;
    int   m_animFrame    = 0;
    float m_debugScroll  = 0.0f;
    std::string m_lastSFX       = "";
    std::string m_lastSFXNodeId = "";
    std::string m_lastSpritePath= "";
    float m_floatOffset  = 0.0f;
};

#endif
