#include "headers/UIStyle.hpp"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

static Color ParseColor(const json& j, const Color& def) {
    if (j.is_array() && j.size() == 4)
        return { j[0].get<unsigned char>(), j[1].get<unsigned char>(), j[2].get<unsigned char>(), j[3].get<unsigned char>() };
    return def;
}

static UIStyle ParseStyleBlock(const json& j) {
    UIStyle s;

    if (j.contains("dialog_box")) {
        auto& b = j["dialog_box"];
        s.boxAnchor       = b.value("anchor",        s.boxAnchor);
        s.boxNormX        = b.value("pos_x",         s.boxNormX);
        s.boxNormY        = b.value("pos_y",         s.boxNormY);
        s.boxWidthNorm    = b.value("width_norm",    s.boxWidthNorm);
        s.boxHeight       = b.value("height",        s.boxHeight);
        s.boxHeightNorm   = b.value("height_norm",   s.boxHeightNorm);
        s.boxMarginBottom = b.value("margin_bottom", s.boxMarginBottom);
        s.boxRoundness    = b.value("roundness",     s.boxRoundness);
        s.boxBorderThick  = b.value("border_thick",  s.boxBorderThick);
        s.boxPadding      = b.value("padding",       s.boxPadding);
        if (b.contains("bg_color"))     s.boxBg     = ParseColor(b["bg_color"],     s.boxBg);
        if (b.contains("border_color")) s.boxBorder = ParseColor(b["border_color"], s.boxBorder);
    }
    if (j.contains("dialog_text")) {
        auto& t = j["dialog_text"];
        s.textFontSize = t.value("font_size", s.textFontSize);
        if (t.contains("color")) s.textColor = ParseColor(t["color"], s.textColor);
    }
    if (j.contains("name_label")) {
        auto& l = j["name_label"];
        s.labelAlign    = l.value("align",     s.labelAlign);
        s.labelOffsetX  = l.value("offset_x",  s.labelOffsetX);
        s.labelOffsetY  = l.value("offset_y",  s.labelOffsetY);
        s.labelPadding  = l.value("padding",   s.labelPadding);
        s.labelHeight   = l.value("height",    s.labelHeight);
        s.labelFontSize = l.value("font_size", s.labelFontSize);
        if (l.contains("bg_color"))    s.labelBg        = ParseColor(l["bg_color"],    s.labelBg);
        if (l.contains("border_color"))s.labelBorder    = ParseColor(l["border_color"],s.labelBorder);
        if (l.contains("text_color"))  s.labelTextColor = ParseColor(l["text_color"],  s.labelTextColor);
    }
    if (j.contains("choice_box")) {
        auto& c = j["choice_box"];
        s.choiceNormX        = c.value("pos_x",            s.choiceNormX);
        s.choiceNormY        = c.value("pos_y",            s.choiceNormY);
        s.choiceOffsetY      = c.value("offset_y",         s.choiceOffsetY);
        s.choiceWidth        = c.value("width",            s.choiceWidth);
        s.choiceRoundness    = c.value("roundness",        s.choiceRoundness);
        s.choiceBorderThick  = c.value("border_thick",     s.choiceBorderThick);
        s.optionFontSize     = c.value("option_font_size", s.optionFontSize);
        s.optionHeight       = c.value("option_height",    s.optionHeight);
        s.optionGap          = c.value("option_gap",       s.optionGap);
        if (c.contains("bg_color"))       s.choiceBg      = ParseColor(c["bg_color"],       s.choiceBg);
        if (c.contains("border_color"))   s.choiceBorder  = ParseColor(c["border_color"],   s.choiceBorder);
        if (c.contains("option_color"))   s.optionColor   = ParseColor(c["option_color"],   s.optionColor);
        if (c.contains("option_hover"))   s.optionHover   = ParseColor(c["option_hover"],   s.optionHover);
        if (c.contains("option_premium")) s.optionPremium = ParseColor(c["option_premium"], s.optionPremium);
    }
    if (j.contains("toast")) {
        auto& t = j["toast"];
        s.toastNormX    = t.value("pos_x",    s.toastNormX);
        s.toastNormY    = t.value("pos_y",    s.toastNormY);
        s.toastMarginX  = t.value("margin_x", s.toastMarginX);
        s.toastMarginY  = t.value("margin_y", s.toastMarginY);
        s.toastWidth    = t.value("width",    s.toastWidth);
        s.toastHeight   = t.value("height",   s.toastHeight);
        s.toastFontSize = t.value("font_size",s.toastFontSize);
        if (t.contains("bg_color"))    s.toastBg        = ParseColor(t["bg_color"],    s.toastBg);
        if (t.contains("border_color"))s.toastBorder    = ParseColor(t["border_color"],s.toastBorder);
        if (t.contains("text_color"))  s.toastTextColor = ParseColor(t["text_color"],  s.toastTextColor);
    }
    if (j.contains("vignette_opacity"))
        s.vignetteOpacity = j["vignette_opacity"].get<float>();

    return s;
}

void StyleManager::LoadStyle(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "[StyleManager] style.json not found: " << path << " — using defaults." << std::endl;
        return;
    }

    m_stylePath = path;
    m_styleLastModTime = GetFileModTime(path.c_str());

    try {
        json j; f >> j;

        std::string prevName = m_currentStyleName;
        m_styleLibrary.clear();
        m_styleOrder.clear();

        if (j.contains("styles") && j["styles"].is_object()) {
            for (auto& [name, block] : j["styles"].items()) {
                m_styleLibrary[name] = ParseStyleBlock(block);
                m_styleOrder.push_back(name);
            }
        } else {
            m_styleLibrary["__default__"] = ParseStyleBlock(j);
            m_styleOrder.push_back("__default__");
        }

        if (!prevName.empty() && m_styleLibrary.count(prevName)) {
            m_activeStyle = m_styleLibrary[prevName];
            m_currentStyleName = prevName;
        } else if (!m_styleOrder.empty()) {
            m_currentStyleName = m_styleOrder[0];
            m_activeStyle = m_styleLibrary[m_currentStyleName];
        }

    } catch (const std::exception& ex) {
        std::cerr << "[StyleManager] Failed to parse style.json: " << ex.what() << std::endl;
    }
}

void StyleManager::Update() {
    m_hotReloadTimer += GetFrameTime();
    if (m_hotReloadTimer >= 0.5f) {
        m_hotReloadTimer = 0.0f;
        if (!m_stylePath.empty()) {
            long newMod = GetFileModTime(m_stylePath.c_str());
            if (newMod != m_styleLastModTime) {
                std::cout << "[StyleManager] style.json changed, hot reloading..." << std::endl;
                LoadStyle(m_stylePath);
            }
        }
    }
}

void StyleManager::SetStyle(const std::string& name) {
    if (!m_styleLibrary.count(name)) {
        std::cerr << "[StyleManager] Style '" << name << "' not found." << std::endl;
        return;
    }
    m_currentStyleName = name;
    m_activeStyle = m_styleLibrary[name];
}

void StyleManager::NextStyle() {
    if (m_styleOrder.empty()) return;
    auto it = std::find(m_styleOrder.begin(), m_styleOrder.end(), m_currentStyleName);
    if (it == m_styleOrder.end() || ++it == m_styleOrder.end()) it = m_styleOrder.begin();
    SetStyle(*it);
}

void StyleManager::PrevStyle() {
    if (m_styleOrder.empty()) return;
    auto it = std::find(m_styleOrder.begin(), m_styleOrder.end(), m_currentStyleName);
    if (it == m_styleOrder.begin()) it = m_styleOrder.end();
    SetStyle(*--it);
}
