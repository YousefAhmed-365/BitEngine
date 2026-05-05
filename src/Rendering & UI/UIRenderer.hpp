#ifndef UI_RENDERER_HPP
#define UI_RENDERER_HPP

#include "raylib.h"
#include "UILayout.hpp"
#include <unordered_map>
#include <string>
#include <vector>

class BitRuntime;

/**
 * UIRenderer: UI-specific rendering orchestration
 * 
 * Separates UI rendering concerns from general graphics:
 * - Element drawing (panels, buttons, text, etc)
 * - Layout resolution
 * - Interactive state
 * - Data binding
 * 
 * Interfaces with:
 * - BitGraphics (low-level drawing)
 * - UILayout (data-driven layout system)
 * - BitRuntime (runtime state)
 */
class UIRenderer {
public:
    explicit UIRenderer(BitRuntime& engine);
    virtual ~UIRenderer();
    
    // Rendering
    void Draw();
    void HandleInput();
    
    // State Management
    UIManager& GetManager() { return m_uiManager; }
    void LoadLayout(const std::string& name, const std::string& path, int layer);
    void UnloadLayout(const std::string& name);
    
private:
    BitRuntime& m_engine;
    UIManager m_uiManager;
    UIDataStore m_dataStore;
    
    // Per-type drawing
    void DrawElement(UIElement& elem);
    void DrawGroupElem(UIElement& elem);
    void DrawPanelElem(UIElement& elem);
    void DrawTextElem(UIElement& elem);
    void DrawRichTextElem(UIElement& elem);
    void DrawCursorElem(UIElement& elem);
    void DrawButtonElem(UIElement& elem);
    void DrawImageElem(UIElement& elem);
    
    // Input handling
    void HandleElementInput(UIElement& elem);
};

#endif
