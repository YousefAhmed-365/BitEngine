#ifndef BIT_DIALOG_HPP
#define BIT_DIALOG_HPP

#include "raylib.h"
#include "BitEngine.hpp"
#include <map>
#include <string>

// Modular Styling System
struct BitDialogStyle {
    Color bg        = { 15, 15, 25, 240 }; 
    Color border    = { 0, 210, 255, 255 }; 
    Color label     = { 255, 215, 0, 255 }; 
    Color text      = { 245, 245, 255, 255 }; 
    Color option    = { 0, 180, 255, 255 }; 
    Color premium   = { 255, 0, 255, 255 }; 
    Color hover     = { 255, 255, 255, 255 }; 
    
    int fontMain = 24;
    int fontName = 28;
    int fontOpt  = 20;
    int padding  = 40;
    
    float boxRoundness = 0.05f;
    float borderThickness = 3.0f;
};

class BitDialog {
public:
    BitDialog(DialogEngine& engine);
    virtual ~BitDialog();
    
    void Draw();
    void HandleInput();
    
    // Customization API
    void SetStyle(const BitDialogStyle& style) { m_style = style; }
    BitDialogStyle& GetStyle() { return m_style; }

protected:
    // Modular drawing components (can be overridden)
    virtual void DrawBackground();
    virtual void DrawMainBox();
    virtual void DrawChoiceBox();
    virtual void DrawEntitySprite();
    virtual void DrawVFX();
    virtual void DrawDebugOverlay();
    virtual void HandleAudio();

    // Internal utilities
    void DrawTextWrapped(const std::string& text, int x, int y, int fontSize, int maxWidth, Color color);
    Texture2D GetTexture(const std::string& path);
    void PlaySFX(const std::string& path);
    void CreateFallbackTexture();
    void CreateVignetteTexture();

    DialogEngine& m_engine;
    BitDialogStyle m_style;

    // Resource Cache
    std::map<std::string, Texture2D> m_textureCache;
    std::map<std::string, Sound> m_sfxCache;
    std::string m_currentMusicPath = "";
    Music m_currentMusic;
    bool m_isMusicPlaying = false;

    Texture2D m_fallbackTexture;
    Texture2D m_vignette;

    // Animation state
    float m_animTimer = 0.0f;
    int m_animFrame = 0;
    std::string m_lastSpritePath = "";
    float m_floatOffset = 0.0f;
};

#endif
