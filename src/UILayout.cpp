#include "headers/UILayout.hpp"

#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>

using json = nlohmann::json;

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
static Color JParseColor(const json& j, Color def) {
    if (j.is_array() && j.size() == 4)
        return { j[0].get<uint8_t>(), j[1].get<uint8_t>(),
                 j[2].get<uint8_t>(), j[3].get<uint8_t>() };
    return def;
}

static UITexture JParseTexture(const json& j) {
    UITexture t;
    t.path        = j.value("path",         t.path);
    t.nineSlice   = j.value("nine_slice",   t.nineSlice);
    t.sliceLeft   = j.value("slice_left",   t.sliceLeft);
    t.sliceTop    = j.value("slice_top",    t.sliceTop);
    t.sliceRight  = j.value("slice_right",  t.sliceRight);
    t.sliceBottom = j.value("slice_bottom", t.sliceBottom);
    if (j.contains("tint")) t.tint = JParseColor(j["tint"], t.tint);
    return t;
}

// Parse a size field that can be:
//   int/float   → pixel value
//   "50%"       → normalised (0.0–1.0)
//   "auto"      → auto-wrap
static void ParseSizeField(const json& val,
                           float& outPx, float& outNorm, bool& outAuto) {
    outPx = outNorm = 0.0f; outAuto = false;
    if (val.is_number()) {
        outPx = val.get<float>();
    } else if (val.is_string()) {
        std::string s = val.get<std::string>();
        if (s == "auto") { outAuto = true; }
        else if (!s.empty() && s.back() == '%') {
            outNorm = std::stof(s.substr(0, s.size() - 1)) / 100.0f;
        } else {
            try { outPx = std::stof(s); } catch(...) { outAuto = true; }
        }
    }
}

// Parse [top, right, bottom, left] or single value padding
static void ParsePadding(const json& val,
                         float& top, float& right, float& bottom, float& left) {
    top = right = bottom = left = 0.0f;
    if (val.is_number()) {
        top = right = bottom = left = val.get<float>();
    } else if (val.is_array() && val.size() == 4) {
        top    = val[0].get<float>();
        right  = val[1].get<float>();
        bottom = val[2].get<float>();
        left   = val[3].get<float>();
    } else if (val.is_array() && val.size() == 2) {
        top = bottom = val[0].get<float>();
        left = right = val[1].get<float>();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// UIStyleBlock::MergedWith
// ─────────────────────────────────────────────────────────────────────────────
UIStyleBlock UIStyleBlock::MergedWith(const UIStyleBlock& o) const {
    UIStyleBlock r = *this;  // start with base (this)

#define MERGE_OPT(field) if (o.field.has_value()) r.field = o.field
#define MERGE_STR(field) if (!o.field.empty())    r.field = o.field
#define MERGE_TEX(field) if (!o.field.path.empty()) r.field = o.field

    MERGE_OPT(bgColor);
    MERGE_OPT(borderColor);
    MERGE_OPT(borderThick);
    MERGE_OPT(roundness);
    MERGE_TEX(texture);
    MERGE_OPT(textColor);
    MERGE_OPT(fontSize);
    MERGE_OPT(lineSpacing);
    MERGE_STR(fontPath);
    MERGE_OPT(cursorShape);
    MERGE_OPT(cursorColor);
    MERGE_OPT(cursorSize);
    MERGE_OPT(cursorAnimSpeed);
    MERGE_TEX(cursorTexture);
    MERGE_OPT(optionColor);
    MERGE_OPT(optionHover);
    MERGE_OPT(optionPremium);
    MERGE_OPT(optionFontSize);
    MERGE_OPT(optionHeight);
    MERGE_OPT(optionGap);
    MERGE_STR(choiceFontPath);
    MERGE_OPT(opacity);
    MERGE_OPT(entityScale);
    MERGE_OPT(floatAmplitude);
    MERGE_OPT(floatSpeed);
    MERGE_OPT(shadowOpacity);
    MERGE_OPT(clearColor);
    MERGE_STR(mouseCursorPath);
    MERGE_OPT(mouseCursorScale);
    MERGE_OPT(historyPadding);
    MERGE_OPT(historySpacing);
    MERGE_OPT(historySpeakerFontSize);
    MERGE_OPT(historyContentFontSize);
    MERGE_OPT(historyHeaderHeight);
    MERGE_OPT(historyFooterHeight);
    MERGE_OPT(historySidebarWidth);
    MERGE_OPT(historyEntryGap);
    MERGE_OPT(historyBg);
    MERGE_OPT(historySpeakerColor);
    MERGE_OPT(historyContentColor);
    MERGE_OPT(historyDimColor);
    MERGE_TEX(historyBgTexture);
    MERGE_TEX(historyPillTexture);
    MERGE_STR(historyFontPath);

#undef MERGE_OPT
#undef MERGE_STR
#undef MERGE_TEX

    return r;
}

// ─────────────────────────────────────────────────────────────────────────────
// StyleSheet
// ─────────────────────────────────────────────────────────────────────────────
Color StyleSheet::ParseColor(const json& j, Color def) { return JParseColor(j, def); }
UITexture StyleSheet::ParseTexture(const json& j) { return JParseTexture(j); }

UIStyleBlock StyleSheet::ParseBlock(const json& j) {
    UIStyleBlock b;

    auto getColor = [&](const char* key, std::optional<Color>& field) {
        if (j.contains(key)) field = JParseColor(j[key], {255,255,255,255});
    };

    getColor("bg_color",     b.bgColor);
    getColor("border_color", b.borderColor);
    getColor("text_color",   b.textColor);
    getColor("cursor_color", b.cursorColor);
    getColor("option_color", b.optionColor);
    getColor("option_hover", b.optionHover);
    getColor("option_premium", b.optionPremium);
    getColor("clear_color",  b.clearColor);
    getColor("history_bg",           b.historyBg);
    getColor("history_speaker_color",b.historySpeakerColor);
    getColor("history_content_color",b.historyContentColor);
    getColor("history_dim_color",    b.historyDimColor);

    auto getFloat = [&](const char* key, std::optional<float>& field) {
        if (j.contains(key)) field = j[key].get<float>();
    };
    auto getInt = [&](const char* key, std::optional<int>& field) {
        if (j.contains(key)) field = j[key].get<int>();
    };
    auto getStr = [&](const char* key, std::string& field) {
        if (j.contains(key)) field = j[key].get<std::string>();
    };

    getFloat("border_thick",   b.borderThick);
    getFloat("roundness",      b.roundness);
    getInt  ("font_size",      b.fontSize);
    getInt  ("line_spacing",   b.lineSpacing);
    getStr  ("font_path",      b.fontPath);
    getFloat("cursor_size",      b.cursorSize);
    getFloat("cursor_anim_speed",b.cursorAnimSpeed);
    if (j.contains("cursor_shape")) b.cursorShape = j["cursor_shape"].get<std::string>();
    getInt  ("option_font_size", b.optionFontSize);
    getInt  ("option_height",    b.optionHeight);
    getInt  ("option_gap",       b.optionGap);
    getStr  ("choice_font_path", b.choiceFontPath);
    getFloat("opacity",          b.opacity);
    getFloat("entity_scale",     b.entityScale);
    getFloat("float_amplitude",  b.floatAmplitude);
    getFloat("float_speed",      b.floatSpeed);
    getFloat("shadow_opacity",   b.shadowOpacity);
    getStr  ("mouse_cursor_path",b.mouseCursorPath);
    getFloat("mouse_cursor_scale",b.mouseCursorScale);
    getFloat("history_padding",   b.historyPadding);
    getFloat("history_spacing",   b.historySpacing);
    getFloat("history_speaker_font_size", b.historySpeakerFontSize);
    getFloat("history_content_font_size", b.historyContentFontSize);
    getInt  ("history_header_height", b.historyHeaderHeight);
    getInt  ("history_footer_height", b.historyFooterHeight);
    getInt  ("history_sidebar_width", b.historySidebarWidth);
    getInt  ("history_entry_gap",     b.historyEntryGap);
    getStr  ("history_font_path",     b.historyFontPath);

    if (j.contains("texture"))             b.texture        = JParseTexture(j["texture"]);
    if (j.contains("cursor_texture"))      b.cursorTexture  = JParseTexture(j["cursor_texture"]);
    if (j.contains("history_bg_texture"))  b.historyBgTexture   = JParseTexture(j["history_bg_texture"]);
    if (j.contains("history_pill_texture"))b.historyPillTexture = JParseTexture(j["history_pill_texture"]);

    return b;
}

bool StyleSheet::Load(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "[StyleSheet] Not found: " << path << "\n";
        return false;
    }
    try {
        json j; f >> j;
        for (auto& [name, block] : j.items()) {
            m_blocks[name] = ParseBlock(block);
        }
        std::cout << "[StyleSheet] Loaded " << m_blocks.size()
                  << " style block(s) from " << path << "\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[StyleSheet] Parse error: " << e.what() << "\n";
        return false;
    }
}

UIStyleBlock StyleSheet::Get(const std::string& name) const {
    auto it = m_blocks.find(name);
    return (it != m_blocks.end()) ? it->second : UIStyleBlock{};
}

// ─────────────────────────────────────────────────────────────────────────────
// UILayout — Parsing
// ─────────────────────────────────────────────────────────────────────────────
UIStyleBlock UILayout::ParseInlineStyle(const json& j) {
    // Re-use StyleSheet's block parser since the format is identical
    return StyleSheet::ParseBlock(j);   // We call the static-equivalent helper
    // (StyleSheet::ParseBlock is static-equivalent — we just call the free fn)
}

UIElement UILayout::ParseElement(const json& j, const StyleSheet& ss) {
    UIElement elem;

    elem.id      = j.value("id",      "");
    elem.type    = j.value("type",    "group");
    elem.role    = j.value("role",    "");
    elem.visible = j.value("visible", true);
    elem.content = j.value("content", "");

    // Anchor
    elem.anchorTo    = j.value("anchor_to",    "parent");
    elem.anchorPoint = j.value("anchor_point", "fill");

    // Offset
    if (j.contains("offset") && j["offset"].is_array() && j["offset"].size() >= 2) {
        elem.offsetX = j["offset"][0].get<float>();
        elem.offsetY = j["offset"][1].get<float>();
    }

    // Size
    if (j.contains("size") && j["size"].is_object()) {
        auto& sz = j["size"];
        if (sz.contains("w")) ParseSizeField(sz["w"], elem.sizeWPx, elem.sizeWNorm, elem.sizeWAuto);
        if (sz.contains("h")) ParseSizeField(sz["h"], elem.sizeHPx, elem.sizeHNorm, elem.sizeHAuto);
    }

    // Padding
    if (j.contains("padding")) ParsePadding(j["padding"], elem.padTop, elem.padRight, elem.padBottom, elem.padLeft);

    // Style
    elem.styleRef = j.value("style", "");
    UIStyleBlock base = elem.styleRef.empty() ? UIStyleBlock{} : ss.Get(elem.styleRef);
    UIStyleBlock inl;
    if (j.contains("style_overrides") && j["style_overrides"].is_object())
        inl = StyleSheet::ParseBlock(j["style_overrides"]);
    // Also accept top-level style fields directly on the element
    inl = inl.MergedWith(StyleSheet::ParseBlock(j));
    elem.inlineStyle    = inl;
    elem.resolvedStyle  = base.MergedWith(inl);

    // Children
    if (j.contains("children") && j["children"].is_array()) {
        for (const auto& child : j["children"])
            elem.children.push_back(ParseElement(child, ss));
    }

    return elem;
}

bool UILayout::Load(const std::string& layoutPath) {
    std::ifstream f(layoutPath);
    if (!f) { std::cerr << "[UILayout] Not found: " << layoutPath << "\n"; return false; }

    try {
        json j; f >> j;

        // Load style sheet first
        if (j.contains("style_sheet")) {
            std::string ssPath = j["style_sheet"].get<std::string>();
            m_styleSheet.Load(ssPath);
        }

        // Parse elements
        m_roots.clear();
        if (j.contains("elements") && j["elements"].is_array()) {
            for (const auto& e : j["elements"])
                m_roots.push_back(ParseElement(e, m_styleSheet));
        }

        // Pre-load fonts referenced anywhere in the tree
        std::function<void(UIElement&)> preloadFonts = [&](UIElement& elem) {
            const auto& rs = elem.resolvedStyle;
            for (const auto& p : { rs.fontPath, rs.choiceFontPath, rs.historyFontPath,
                                   rs.mouseCursorPath }) {
                (void)p; // just for the loop variable — we preload below
            }
            auto tryLoad = [&](const std::string& p) {
                if (!p.empty() && !m_fontCache.count(p) && FileExists(p.c_str())) {
                    Font font = LoadFontEx(p.c_str(), 96, nullptr, 0);
                    if (font.texture.id > 0) {
                        SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
                        m_fontCache[p] = font;
                    }
                }
            };
            tryLoad(rs.fontPath);
            tryLoad(rs.choiceFontPath);
            tryLoad(rs.historyFontPath);
            for (auto& child : elem.children) preloadFonts(child);
        };
        for (auto& root : m_roots) preloadFonts(root);

        std::cout << "[UILayout] Loaded " << m_roots.size()
                  << " root element(s) from " << layoutPath << "\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[UILayout] Parse error: " << e.what() << "\n";
        return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// UILayout — Layout resolution
// ─────────────────────────────────────────────────────────────────────────────
float UILayout::ResolveSize(float px, float norm, bool autoSize, float parentSize) const {
    if (px > 0.0f)       return px;
    if (norm > 0.0f)     return norm * parentSize;
    if (autoSize)        return parentSize; // fallback — caller may override for auto
    return parentSize;
}

Rectangle UILayout::ComputeRect(const UIElement& elem, Rectangle parent, int sw, int sh) const {
    // Resolve reference frame
    Rectangle ref = parent;
    if (elem.anchorTo == "screen") {
        ref = { 0.0f, 0.0f, (float)sw, (float)sh };
    }
    // (anchoring to a specific element by id is not done here; could be extended)

    // Resolve size
    float w = 0.0f, h = 0.0f;
    if (elem.sizeWPx > 0.0f)       w = elem.sizeWPx;
    else if (elem.sizeWNorm > 0.0f) w = elem.sizeWNorm * ref.width;
    else                            w = ref.width;  // fill / auto defaults to parent w

    if (elem.sizeHPx > 0.0f)       h = elem.sizeHPx;
    else if (elem.sizeHNorm > 0.0f) h = elem.sizeHNorm * ref.height;
    else                            h = ref.height; // fill / auto defaults to parent h

    // Anchor point determines where (x,y) maps to within ref
    float ax = ref.x, ay = ref.y;
    const std::string& ap = elem.anchorPoint;

    if      (ap == "fill")          { ax = ref.x; ay = ref.y; w = ref.width; h = ref.height; }
    else if (ap == "top-left")      { ax = ref.x;                                  ay = ref.y; }
    else if (ap == "top-center")    { ax = ref.x + ref.width * 0.5f - w * 0.5f;   ay = ref.y; }
    else if (ap == "top-right")     { ax = ref.x + ref.width - w;                 ay = ref.y; }
    else if (ap == "center-left")   { ax = ref.x;                                  ay = ref.y + ref.height * 0.5f - h * 0.5f; }
    else if (ap == "center")        { ax = ref.x + ref.width * 0.5f - w * 0.5f;   ay = ref.y + ref.height * 0.5f - h * 0.5f; }
    else if (ap == "center-right")  { ax = ref.x + ref.width - w;                 ay = ref.y + ref.height * 0.5f - h * 0.5f; }
    else if (ap == "bottom-left")   { ax = ref.x;                                  ay = ref.y + ref.height - h; }
    else if (ap == "bottom-center") { ax = ref.x + ref.width * 0.5f - w * 0.5f;   ay = ref.y + ref.height - h; }
    else if (ap == "bottom-right")  { ax = ref.x + ref.width - w;                 ay = ref.y + ref.height - h; }

    return { ax + elem.offsetX, ay + elem.offsetY, w, h };
}

void UILayout::ResolveElement(UIElement& elem, Rectangle parentRect, int sw, int sh) {
    elem.computedRect = ComputeRect(elem, parentRect, sw, sh);
    elem.contentRect  = {
        elem.computedRect.x + elem.padLeft,
        elem.computedRect.y + elem.padTop,
        elem.computedRect.width  - elem.padLeft - elem.padRight,
        elem.computedRect.height - elem.padTop  - elem.padBottom
    };

    for (auto& child : elem.children)
        ResolveElement(child, elem.computedRect, sw, sh);
}

void UILayout::Resolve(int sw, int sh) {
    Rectangle screenRect = { 0, 0, (float)sw, (float)sh };
    for (auto& root : m_roots)
        ResolveElement(root, screenRect, sw, sh);
}

// ─────────────────────────────────────────────────────────────────────────────
// UILayout — Search
// ─────────────────────────────────────────────────────────────────────────────
UIElement* UILayout::FindByRoleInTree(UIElement& elem, const std::string& role) {
    if (elem.role == role) return &elem;
    for (auto& child : elem.children) {
        UIElement* found = FindByRoleInTree(child, role);
        if (found) return found;
    }
    return nullptr;
}

UIElement* UILayout::FindByIdInTree(UIElement& elem, const std::string& id) {
    if (elem.id == id) return &elem;
    for (auto& child : elem.children) {
        UIElement* found = FindByIdInTree(child, id);
        if (found) return found;
    }
    return nullptr;
}

UIElement* UILayout::FindByRole(const std::string& role) {
    for (auto& root : m_roots) {
        UIElement* f = FindByRoleInTree(root, role);
        if (f) return f;
    }
    return nullptr;
}

UIElement* UILayout::FindById(const std::string& id) {
    for (auto& root : m_roots) {
        UIElement* f = FindByIdInTree(root, id);
        if (f) return f;
    }
    return nullptr;
}

std::vector<UIElement*> UILayout::GetRoots() {
    std::vector<UIElement*> out;
    out.reserve(m_roots.size());
    for (auto& r : m_roots) out.push_back(&r);
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// Font cache
// ─────────────────────────────────────────────────────────────────────────────
Font UILayout::GetFont(const std::string& path) {
    if (path.empty()) return m_defaultFont.texture.id ? m_defaultFont : GetFontDefault();
    auto it = m_fontCache.find(path);
    if (it != m_fontCache.end()) return it->second;
    if (FileExists(path.c_str())) {
        Font font = LoadFontEx(path.c_str(), 96, nullptr, 0);
        if (font.texture.id > 0) {
            SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
            m_fontCache[path] = font;
            return font;
        }
    }
    return GetFontDefault();
}

void UILayout::Shutdown() {
    for (auto& [p, font] : m_fontCache) UnloadFont(font);
    m_fontCache.clear();
}
