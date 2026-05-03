#ifndef UI_LAYOUT_HPP
#define UI_LAYOUT_HPP

#include "raylib.h"
#include "json.hpp"

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

// ─────────────────────────────────────────────────────────────────────────────
// StyleTexture  (re-declared here so UILayout.hpp is self-contained)
// ─────────────────────────────────────────────────────────────────────────────
struct UITexture {
    std::string path      = "";
    bool        nineSlice = false;
    int         sliceLeft = 0, sliceTop = 0, sliceRight = 0, sliceBottom = 0;
    Color       tint      = { 255, 255, 255, 255 };
};

// ─────────────────────────────────────────────────────────────────────────────
// UIStyleBlock  — flat bag of optional style properties.
// Every field is optional so that inline overrides on an element can be merged
// cleanly with a referenced style block (only set fields win).
// ─────────────────────────────────────────────────────────────────────────────
struct UIStyleBlock {
    // Panel / box visuals
    std::optional<Color> bgColor;
    std::optional<Color> borderColor;
    std::optional<float> borderThick;
    std::optional<float> roundness;
    UITexture            texture;

    // Text
    std::optional<Color> textColor;
    std::optional<int>   fontSize;
    std::optional<int>   lineSpacing;
    std::string          fontPath;

    // Cursor (dialog wait-cursor)
    std::optional<std::string> cursorShape;
    std::optional<Color>       cursorColor;
    std::optional<float>       cursorSize;
    std::optional<float>       cursorAnimSpeed;
    UITexture                  cursorTexture;

    // Choice list
    std::optional<Color> optionColor;
    std::optional<Color> optionHover;
    std::optional<Color> optionPremium;
    std::optional<int>   optionFontSize;
    std::optional<int>   optionHeight;
    std::optional<int>   optionGap;
    std::string          choiceFontPath;

    // Vignette / screen-overlay
    std::optional<float> opacity;

    // Entity layer
    std::optional<float> entityScale;
    std::optional<float> floatAmplitude;
    std::optional<float> floatSpeed;
    std::optional<float> shadowOpacity;

    // Background / clear color
    std::optional<Color> clearColor;

    // Mouse cursor
    std::string          mouseCursorPath;
    std::optional<float> mouseCursorScale;

    // Conditional visibility: "speaker" | "narration" | "choices" | "ui" | "" (always)
    std::optional<std::string> visibleWhen;

    // History panel
    std::optional<float> historyPadding;
    std::optional<float> historySpacing;
    std::optional<float> historySpeakerFontSize;
    std::optional<float> historyContentFontSize;
    std::optional<int>   historyHeaderHeight;
    std::optional<int>   historyFooterHeight;
    std::optional<int>   historySidebarWidth;
    std::optional<int>   historyEntryGap;
    std::optional<Color> historyBg;
    std::optional<Color> historySpeakerColor;
    std::optional<Color> historyContentColor;
    std::optional<Color> historyDimColor;
    UITexture            historyBgTexture;
    UITexture            historyPillTexture;
    std::string          historyFontPath;

    // Merge another block on top of this one (other wins on set fields)
    UIStyleBlock MergedWith(const UIStyleBlock& other) const;
};

// ─────────────────────────────────────────────────────────────────────────────
// StyleSheet  — loads ui_*_style.json and provides named UIStyleBlocks
// ─────────────────────────────────────────────────────────────────────────────
class StyleSheet {
public:
    bool Load(const std::string& path);
    UIStyleBlock Get(const std::string& name) const;
    static UIStyleBlock ParseBlock(const nlohmann::json& j);

private:
    std::unordered_map<std::string, UIStyleBlock> m_blocks;
    static UITexture    ParseTexture(const nlohmann::json& j);
    static Color        ParseColor(const nlohmann::json& j, Color def = {255,255,255,255});
};

// ─────────────────────────────────────────────────────────────────────────────
// UIElement  — one node in the UI tree
// ─────────────────────────────────────────────────────────────────────────────
struct UIElement {
    // Identity
    std::string id;
    std::string type;   // "group" | "panel" | "text" | "rich_text" | "cursor" |
                        // "choices" | "image" | "vignette" | "background" | "entity_layer"
    std::string role;   // Engine binding: "dialog_text" | "name_label" | "choice_list" |
                        // "dialog_cursor" | "screen_fade" | "toast" | "entity_layer" |
                        // "background" | "history_panel" | "mouse_cursor"
    bool visible = true;

    // ── Layout ──────────────────────────────────────────────────────────────
    std::string anchorTo;    // "screen" | "parent" | element id
    std::string anchorPoint; // "top-left" | "top-center" | "top-right" |
                             // "center-left" | "center" | "center-right" |
                             // "bottom-left" | "bottom-center" | "bottom-right" | "fill"
    float offsetX = 0.0f, offsetY = 0.0f;

    // Size: set exactly one per axis. Pixel wins over norm, norm wins over auto.
    float sizeWPx   = 0.0f, sizeHPx   = 0.0f;   // >0 = use
    float sizeWNorm = 0.0f, sizeHNorm = 0.0f;   // 0.0–1.0 fraction of parent
    bool  sizeWAuto = false, sizeHAuto = false;  // wrap content

    float padTop = 0.0f, padRight = 0.0f, padBottom = 0.0f, padLeft = 0.0f;

    // ── Style ────────────────────────────────────────────────────────────────
    std::string  styleRef;      // Named block from style sheet
    UIStyleBlock inlineStyle;   // Inline overrides declared on the element
    UIStyleBlock resolvedStyle; // = styleSheet[styleRef].MergedWith(inlineStyle) — built at Load()

    // ── Content ──────────────────────────────────────────────────────────────
    std::string content; // Static text (for "text" type); empty = engine-driven

    // ── Children ─────────────────────────────────────────────────────────────
    std::vector<UIElement> children;

    // ── Computed rect (filled by UILayout::Resolve() each frame) ─────────────
    Rectangle computedRect = { 0, 0, 0, 0 };
    // Inner content rect (computedRect minus padding)
    Rectangle contentRect  = { 0, 0, 0, 0 };
};

// ─────────────────────────────────────────────────────────────────────────────
// UILayout  — loads the element tree, resolves rects each frame
// ─────────────────────────────────────────────────────────────────────────────
class UILayout {
public:
    bool Load(const std::string& layoutPath);   // Load ui_*.json (also loads referenced style sheet)
    void Resolve(int screenW, int screenH);     // Recompute all rects — call once per frame before draw
    void Shutdown();

    // Traversal
    UIElement*              FindByRole(const std::string& role);
    UIElement*              FindById(const std::string& id);
    std::vector<UIElement*> GetRoots();         // Top-level elements in declaration order

    // Font cache (shared across the layout)
    Font GetFont(const std::string& path);
    Font GetDefaultFont() const { return m_defaultFont; }

private:
    std::vector<UIElement> m_roots;
    StyleSheet             m_styleSheet;
    std::unordered_map<std::string, Font> m_fontCache;
    Font m_defaultFont = {};

    // Parsing
    UIElement ParseElement(const nlohmann::json& j, const StyleSheet& ss);
    UIStyleBlock ParseInlineStyle(const nlohmann::json& j);

    // Layout resolution helpers
    void      ResolveElement(UIElement& elem, Rectangle parentRect, int sw, int sh);
    Rectangle ComputeRect(const UIElement& elem, Rectangle parentRect, int sw, int sh) const;
    float     ResolveSize(float px, float norm, bool autoSize, float parentSize) const;

    // Tree search helpers
    UIElement* FindByRoleInTree(UIElement& elem, const std::string& role);
    UIElement* FindByIdInTree(UIElement& elem, const std::string& id);
};

#endif // UI_LAYOUT_HPP
