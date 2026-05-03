#ifndef BIT_RENDERER_HPP
#define BIT_RENDERER_HPP

#include "raylib.h"
#include "BitEngine.hpp"
#include "UILayout.hpp"

#include <unordered_map>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// BitRenderer
// Drives all visual and audio output for BitEngine.
// The UI is fully data-driven through UILayout / UIElement — no hardcoded
// element-specific draw functions exist at the top level.
// ─────────────────────────────────────────────────────────────────────────────
class BitRenderer {
public:
    explicit BitRenderer(DialogEngine& engine);
    virtual ~BitRenderer();

    void Draw();
    void HandleInput();
    void PreloadAssets();

    UILayout& GetLayout() { return m_layout; }

protected:
    // ── Generic element dispatcher ─────────────────────────────────────────
    void DrawElement(UIElement& elem);

    // ── Per-type draw functions ────────────────────────────────────────────
    void DrawGroupElem      (UIElement& elem);
    void DrawPanelElem      (UIElement& elem);
    void DrawTextElem       (UIElement& elem);
    void DrawRichTextElem   (UIElement& elem);
    void DrawCursorElem     (UIElement& elem);
    void DrawChoicesElem    (UIElement& elem);
    void DrawVignetteElem   (UIElement& elem);
    void DrawImageElem      (UIElement& elem);
    void DrawBackgroundElem (UIElement& elem);
    void DrawEntityLayerElem(UIElement& elem);

    // ── Specialised draw helpers (history, debug, cursor) ─────────────────
    void DrawHistory();
    void DrawDebugOverlay();
    void DrawCustomCursor(UIElement* mouseCursorElem);

    // ── Shared rendering utilities ─────────────────────────────────────────
    int DrawRichText(const std::vector<RichChar>& content, int limit,
                     int x, int y, int fontSize, int maxWidth,
                     Color defaultColor = RAYWHITE, int lineSpacing = 6,
                     Font font = { 0 });

    void DrawStyledPanel(Rectangle rect, const UIStyleBlock& style);

    Texture2D GetTexture(const std::string& path);
    Font      GetFont   (const std::string& path);
    void      PlaySFX   (const std::string& path);

    void CreateFallbackTexture();
    void CreateVignetteTexture();

    // ── Audio ───────────────────────────────────────────────────────────────
    void HandleAudio();

    // ── Members ─────────────────────────────────────────────────────────────
    DialogEngine& m_engine;
    UILayout      m_layout;

    std::unordered_map<std::string, Texture2D> m_textureCache;
    std::unordered_map<std::string, Music>     m_musicCache;
    std::unordered_map<std::string, Sound>     m_sfxCache;
    std::string m_currentMusicPath;
    Music       m_currentMusic  = {};
    bool        m_isMusicPlaying = false;

    Texture2D   m_fallbackTexture = {};
    Texture2D   m_vignette        = {};
    Texture2D   m_customCursor    = {};
    std::string m_currentCursorPath;

    bool  m_showHistory   = false;
    float m_historyScroll = 0.0f;
    float m_debugScroll   = 0.0f;
    float m_floatOffset   = 0.0f;

    // Toast state
    float       m_toastTimer = 0.0f;
    std::string m_toastMsg;
};

#endif // BIT_RENDERER_HPP
