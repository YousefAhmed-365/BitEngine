#include "UILayout.hpp"

#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <filesystem>

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
    MERGE_OPT(visible);
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

    auto getBool = [&](const char* key, std::optional<bool>& field) {
        if (j.contains(key)) field = j[key].get<bool>();
    };
    getBool("visible", b.visible);

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
        std::cerr << "[UILayout] Not found: " << path << "\n";
        return false;
    }
    try {
        json j; f >> j;
        for (auto& [name, block] : j.items()) {
            m_blocks[name] = ParseBlock(block);
        }
        std::cout << "[UILayout] Loaded " << m_blocks.size()
                  << " style block(s) from " << path << "\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[UILayout] Parse error: " << e.what() << "\n";
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

    elem.id          = j.value("id",           "");
    elem.type        = j.value("type",         "group");
    elem.role        = j.value("role",         "");
    elem.content     = j.value("content",      "");
    elem.bindContent = j.value("bind_content", "");
    elem.bindVisible = j.value("bind_visible", "");
    elem.bindItems   = j.value("bind_items",   "");
    if (j.contains("item_template")) {
        elem.itemTemplateJson = j["item_template"];
    }
    elem.onClick     = j.value("on_click",     "");
    elem.displayContent = elem.content; // initialise from static content

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

    // Initial visible state (can be modified dynamically by app)
    elem.visible = elem.resolvedStyle.visible.value_or(j.value("visible", true));

    // Animations
    if (j.contains("animations") && j["animations"].is_array()) {
        for (const auto& anim : j["animations"]) {
            UIAnimation a;
            a.type   = anim.value("type", "pulse");
            a.target = anim.value("target", "opacity");
            a.speed  = anim.value("speed", 1.0f);
            a.minVal = anim.value("min", 0.0f);
            a.maxVal = anim.value("max", 1.0f);
            elem.animations.push_back(a);
        }
    }

    // Children
    if (j.contains("children") && j["children"].is_array()) {
        for (const auto& child : j["children"])
            elem.children.push_back(ParseElement(child, ss));
    }

    return elem;
}

// ─────────────────────────────────────────────────────────────────────────────
// UILayout — SetVisible / SetContent
// ─────────────────────────────────────────────────────────────────────────────
bool UILayout::SetVisible(const std::string& id, bool vis) {
    UIElement* e = FindById(id);
    if (e) { e->visible = vis; return true; }
    return false;
}

bool UILayout::SetContent(const std::string& id, const std::string& content) {
    UIElement* e = FindById(id);
    if (e) { e->displayContent = content; return true; }
    return false;
}

bool UILayout::Load(const std::string& layoutPath) {
    std::ifstream f(layoutPath);
    if (!f) { std::cerr << "[UILayout] Not found: " << layoutPath << "\n"; return false; }

    try {
        json j; f >> j;

        // Load style sheet first
        if (j.contains("style_sheet")) {
            std::string ssPath = j["style_sheet"].get<std::string>();
            if (!ssPath.empty()) {
                std::filesystem::path dir = std::filesystem::path(layoutPath).parent_path();
                ssPath = (dir / ssPath).string();
            }
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

void UILayout::ResolveElement(UIElement& elem, Rectangle parentRect, int sw, int sh, const UIDataStore* data) {
    // ── Data Bindings ────────────────────────────────────────────────────────
    auto getVal = [&](const std::string& key) -> nlohmann::json {
        if (!data || key.empty()) return nlohmann::json();
        try {
            std::string ptrPath = "/";
            std::string current;
            for (char c : key) {
                if (c == '.' || c == '[') {
                    if (!current.empty()) { ptrPath += current + "/"; current.clear(); }
                } else if (c == ']') {
                    if (!current.empty()) { ptrPath += current + "/"; current.clear(); }
                } else {
                    current += c;
                }
            }
            if (!current.empty()) ptrPath += current;
            else if (ptrPath.size() > 1 && ptrPath.back() == '/') ptrPath.pop_back();

            auto ptr = nlohmann::json::json_pointer(ptrPath);
            if (data->contains(ptr)) return data->at(ptr);
        } catch (...) {}
        if (data->contains(key)) return (*data)[key];
        return nlohmann::json();
    };

    if (data) {
        if (!elem.bindContent.empty()) {
            auto v = getVal(elem.bindContent);
            if (!v.is_null()) {
                if (v.is_string()) elem.displayContent = v.get<std::string>();
                else elem.displayContent = v.dump();
            }
        }
        if (!elem.bindVisible.empty()) {
            auto v = getVal(elem.bindVisible);
            if (!v.is_null()) {
                if (v.is_boolean())    elem.visible = v.get<bool>();
                else if (v.is_number()) elem.visible = (v.get<float>() != 0.0f);
                else if (v.is_string()) {
                    const std::string s = v.get<std::string>();
                    elem.visible = !s.empty() && s != "false" && s != "0";
                }
            }
        }
    }
    if (!elem.bindContent.empty() && elem.displayContent.empty() && data == nullptr)
        elem.displayContent = elem.content;

    // ── Auto Size Resolution ─────────────────────────────────────────────────
    if (elem.sizeWAuto && !elem.displayContent.empty()) {
        Font f = GetFont(elem.resolvedStyle.fontPath);
        int fs = elem.resolvedStyle.fontSize.value_or(20);
        Vector2 sz = MeasureTextEx(f, elem.displayContent.c_str(), (float)fs, 2.0f);
        elem.sizeWPx = sz.x + elem.padLeft + elem.padRight;
    }
    if (elem.sizeHAuto && !elem.bindItems.empty() && data) {
        auto itemsVal = getVal(elem.bindItems);
        if (itemsVal.is_array()) {
            size_t count = itemsVal.size();
            float itemH  = elem.resolvedStyle.optionHeight.value_or(44.0f);
            float gap    = elem.resolvedStyle.optionGap.value_or(10.0f);
            float itemsH = (count > 0) ? (count * itemH + (count - 1) * gap) : 0;
            elem.sizeHPx = elem.padTop + itemsH + elem.padBottom;
        }
    }

    elem.computedRect = ComputeRect(elem, parentRect, sw, sh);
    elem.contentRect  = {
        elem.computedRect.x + elem.padLeft,
        elem.computedRect.y + elem.padTop,
        elem.computedRect.width  - elem.padLeft - elem.padRight,
        elem.computedRect.height - elem.padTop  - elem.padBottom
    };

    // ── Animations ───────────────────────────────────────────────────────────
    for (const auto& anim : elem.animations) {
        float time = (float)GetTime();
        float val = 0.0f;
        if (anim.type == "pulse") {
            val = anim.minVal + (anim.maxVal - anim.minVal) * (sinf(time * anim.speed) * 0.5f + 0.5f);
        } else if (anim.type == "wave") {
            val = anim.minVal + (anim.maxVal - anim.minVal) * sinf(time * anim.speed);
        } else {
            val = anim.maxVal;
        }

        if (anim.target == "offset_x") {
            elem.computedRect.x += val; elem.contentRect.x += val;
        } else if (anim.target == "offset_y") {
            elem.computedRect.y += val; elem.contentRect.y += val;
        } else if (anim.target == "opacity") {
            elem.resolvedStyle.opacity = val;
        } else if (anim.target == "scale") {
            float diffW = elem.computedRect.width  * (val - 1.0f);
            float diffH = elem.computedRect.height * (val - 1.0f);
            elem.computedRect.x -= diffW * 0.5f; elem.computedRect.y -= diffH * 0.5f;
            elem.computedRect.width += diffW;     elem.computedRect.height += diffH;
        }
    }

    // ── Phase 7: Dynamic List Instancing (bind_items) ────────────────────────
    if (data && !elem.bindItems.empty() && !elem.itemTemplateJson.empty()) {
        auto itemsVal = getVal(elem.bindItems);
        if (itemsVal.is_array()) {
            const auto& items = itemsVal;
            size_t count = items.size();
            elem.children.clear();

            float itemH = elem.resolvedStyle.optionHeight.value_or(44.0f);
            float gap   = elem.resolvedStyle.optionGap.value_or(10.0f);
            
            float totalItemsH = (count > 0) ? (count * itemH + (count - 1) * gap) : 0;
            elem.sizeHPx = elem.padTop + totalItemsH + elem.padBottom;

            float yOff  = 0.0f;
            for (size_t idx = 0; idx < count; ++idx) {
                nlohmann::json childJ = elem.itemTemplateJson;
                UIDataStore itemContext = *data;
                const auto& itemData = items[idx];
                if (itemData.is_object()) {
                    for (auto& [ik, iv] : itemData.items()) itemContext["$item." + ik] = iv;
                } else {
                    itemContext["$item"] = itemData;
                }

                UIElement child = ParseElement(childJ, m_styleSheet);
                child.onClick = "select_choice " + std::to_string(idx);
                child.offsetY = yOff;
                if (child.anchorPoint.empty()) child.anchorPoint = "top-left";
                if (child.sizeHPx == 0 && child.sizeHNorm == 0 && !child.sizeHAuto) child.sizeHPx = itemH;

                ResolveElement(child, elem.contentRect, sw, sh, &itemContext);
                elem.children.push_back(std::move(child));
                yOff += itemH + gap;
            }
            return; 
        }
    }

    // ── Generic children ─────────────────────────────────────────────────────
    for (auto& child : elem.children)
        ResolveElement(child, elem.computedRect, sw, sh, data);
}

void UILayout::Resolve(int sw, int sh, const UIDataStore* data) {
    Rectangle screenRect = { 0, 0, (float)sw, (float)sh };
    for (auto& root : m_roots)
        ResolveElement(root, screenRect, sw, sh, data);
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

// ─────────────────────────────────────────────────────────────────────────────
// UIManager implementation
// ─────────────────────────────────────────────────────────────────────────────
std::pair<std::string,std::string> UIManager::SplitScopedId(const std::string& id) {
    auto dot = id.find('.');
    if (dot == std::string::npos) return {"", id};
    return {id.substr(0, dot), id.substr(dot + 1)};
}

UIManager::LayerEntry* UIManager::FindLayer(const std::string& name) {
    for (auto& e : m_layers) if (e.name == name) return &e;
    return nullptr;
}

bool UIManager::Load(const std::string& name, const std::string& path, int layer) {
    // Optimization: If already loaded with the same path, just activate and update layer
    auto* existing = FindLayer(name);
    if (existing && existing->path == path) {
        existing->layer = layer;
        existing->active = true;
        existing->visible = true;
        return true;
    }

    if (existing) {
        existing->layout.Shutdown();
        m_layers.erase(std::remove_if(m_layers.begin(), m_layers.end(),
            [&](const LayerEntry& e) { return e.name == name; }), m_layers.end());
    }
    LayerEntry entry;
    entry.name  = name;
    entry.path  = path;
    entry.layer = layer;
    entry.active = true;
    entry.visible = true;
    if (!entry.layout.Load(path)) {
        std::cerr << "[UILayout] Failed to load: " << path << "\n";
        return false;
    }
    m_layers.push_back(std::move(entry));
    std::cout << "[UILayout] Loaded UI \"" << name << "\" (layer " << layer << ") from " << path << "\n";
    return true;
}

void UIManager::Unload(const std::string& name) {
    m_layers.erase(std::remove_if(m_layers.begin(), m_layers.end(),
        [&](LayerEntry& e) { if (e.name == name) { e.layout.Shutdown(); return true; } return false; }),
        m_layers.end());
    std::cout << "[UILayout] Unloaded UI \"" << name << "\"\n";
}

bool UIManager::IsLoaded(const std::string& name) const {
    for (const auto& e : m_layers) if (e.name == name) return true;
    return false;
}

void UIManager::Shutdown() {
    for (auto& e : m_layers) e.layout.Shutdown();
    m_layers.clear();
}

UIElement* UIManager::FindById(const std::string& scopedId) {
    auto [ns, id] = SplitScopedId(scopedId);
    if (!ns.empty()) {
        auto* layer = FindLayer(ns);
        return layer ? layer->layout.FindById(id) : nullptr;
    }
    for (auto& e : m_layers) {
        UIElement* found = e.layout.FindById(id);
        if (found) return found;
    }
    return nullptr;
}

UIElement* UIManager::FindByRole(const std::string& scopedId) {
    auto [ns, role] = SplitScopedId(scopedId);
    if (!ns.empty()) {
        auto* layer = FindLayer(ns);
        return layer ? layer->layout.FindByRole(role) : nullptr;
    }
    for (auto& e : m_layers) {
        UIElement* found = e.layout.FindByRole(role);
        if (found) return found;
    }
    return nullptr;
}

bool UIManager::SetVisible(const std::string& scopedId, bool visible) {
    auto [ns, id] = SplitScopedId(scopedId);
    if (!ns.empty()) {
        auto* layer = FindLayer(ns);
        return layer ? layer->layout.SetVisible(id, visible) : false;
    }
    bool any = false;
    for (auto& e : m_layers) any |= e.layout.SetVisible(id, visible);
    return any;
}

bool UIManager::SetContent(const std::string& scopedId, const std::string& content) {
    auto [ns, id] = SplitScopedId(scopedId);
    if (!ns.empty()) {
        auto* layer = FindLayer(ns);
        return layer ? layer->layout.SetContent(id, content) : false;
    }
    bool any = false;
    for (auto& e : m_layers) any |= e.layout.SetContent(id, content);
    return any;
}

void UIManager::Resolve(int sw, int sh, const UIDataStore* data) {
    for (auto& e : m_layers) {
        if (e.active) e.layout.Resolve(sw, sh, data);
    }
}

std::vector<UILayout*> UIManager::GetSortedLayers() {
    std::vector<LayerEntry*> sorted;
    sorted.reserve(m_layers.size());
    for (auto& e : m_layers) {
        if (e.active && e.visible) sorted.push_back(&e);
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const LayerEntry* a, const LayerEntry* b) { return a->layer < b->layer; });
    std::vector<UILayout*> out;
    out.reserve(sorted.size());
    for (auto* e : sorted) out.push_back(&e->layout);
    return out;
}

void UIManager::SetActive(const std::string& name, bool active) {
    auto* l = FindLayer(name);
    if (l) l->active = active;
}

void UIManager::SetLayerVisible(const std::string& name, bool visible) {
    auto* l = FindLayer(name);
    if (l) l->visible = visible;
}

bool UIManager::IsActive(const std::string& name) const {
    for (const auto& e : m_layers) if (e.name == name) return e.active;
    return false;
}

bool UIManager::IsLayerVisible(const std::string& name) const {
    for (const auto& e : m_layers) if (e.name == name) return e.visible;
    return false;
}

Font UIManager::GetFont(const std::string& path) {
    // Ask the first layer that has the font
    for (auto& e : m_layers) {
        Font f = e.layout.GetFont(path);
        if (f.texture.id > 0) return f;
    }
    return GetFontDefault();
}
