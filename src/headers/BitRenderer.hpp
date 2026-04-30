#ifndef BIT_RENDERER_HPP
#define BIT_RENDERER_HPP

#include "raylib.h"
#include "BitEngine.hpp"
#include "UIStyle.hpp"
#include <map>
#include <vector>
#include <string>

class BitRenderer {
public:
    BitRenderer(DialogEngine& engine);
    virtual ~BitRenderer();

    void Draw();
    void HandleInput();

    // Expose StyleManager for external configuration / inputs
    StyleManager& GetStyleManager() { return m_styleManager; }



protected:
    virtual void DrawBackground();
    virtual void DrawMainBox();
    virtual void DrawChoiceBox();
    virtual void DrawEntitySprite();
    virtual void DrawVFX();
    virtual void DrawDebugOverlay();
    virtual void HandleAudio();

    void DrawRichText(const std::vector<RichChar>& content, int limit, int x, int y, int fontSize, int maxWidth, Color defaultColor);
    Texture2D GetTexture(const std::string& path);
    void PlaySFX(const std::string& path);
    void CreateFallbackTexture();
    void CreateVignetteTexture();

    DialogEngine& m_engine;
    StyleManager  m_styleManager;

    std::map<std::string, Texture2D> m_textureCache;
    std::map<std::string, Sound>     m_sfxCache;
    std::string m_currentMusicPath = "";
    Music m_currentMusic;
    bool m_isMusicPlaying = false;

    Texture2D m_fallbackTexture;
    Texture2D m_vignette;

    float m_animTimer   = 0.0f;
    int   m_animFrame   = 0;
    std::string m_lastSpritePath = "";
    float m_floatOffset = 0.0f;
};

#endif
