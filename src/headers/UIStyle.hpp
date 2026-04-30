#ifndef BIT_STYLE_MANAGER_HPP
#define BIT_STYLE_MANAGER_HPP

#include "raylib.h"
#include <map>
#include <vector>
#include <string>

// ============================================================
// Full UI Style — all values are data-driven from style.json
// ============================================================
struct UIStyle {

    // --- Dialog Box ---
    float boxNormX         = 0.5f;
    float boxNormY         = 1.0f;
    float boxWidthNorm     = 0.93f;
    float boxHeight        = 190.0f;
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
};

class StyleManager {
public:
    StyleManager() = default;

    void LoadStyle(const std::string& path);             // Load all named styles from file; enables hot-reload
    void Update();                                       // Hot-reload check (should be called every frame)
    
    void SetStyle(const std::string& name);              // Switch to a named style block
    void NextStyle();                                    // Cycle forward through loaded styles
    void PrevStyle();                                    // Cycle backward through loaded styles
    
    std::string GetCurrentStyleName() const { return m_currentStyleName; }
    std::vector<std::string> GetStyleNames() const { return m_styleOrder; }
    UIStyle& GetStyle() { return m_activeStyle; }

private:
    UIStyle m_activeStyle;
    
    std::map<std::string, UIStyle> m_styleLibrary;
    std::vector<std::string>       m_styleOrder;
    std::string                    m_currentStyleName;
    std::string                    m_stylePath;
    long                           m_styleLastModTime = 0;
    float                          m_hotReloadTimer   = 0.0f;
};

#endif
