#include "headers/BitRenderer.hpp"
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>

using json = nlohmann::ordered_json;

static Color ParseColor(const json& j, const Color& def) {
    if (j.is_array() && j.size() == 4)
        return { j[0].get<unsigned char>(), j[1].get<unsigned char>(), j[2].get<unsigned char>(), j[3].get<unsigned char>() };
    return def;
}

static StyleTexture ParseStyleTexture(const json& j) {
    StyleTexture t;
    t.path        = j.value("path",         t.path);
    t.nineSlice   = j.value("nine_slice",   t.nineSlice);
    t.sliceLeft   = j.value("slice_left",   t.sliceLeft);
    t.sliceTop    = j.value("slice_top",    t.sliceTop);
    t.sliceRight  = j.value("slice_right",  t.sliceRight);
    t.sliceBottom = j.value("slice_bottom", t.sliceBottom);
    if (j.contains("tint")) t.tint = ParseColor(j["tint"], t.tint);
    return t;
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

    if (j.contains("history")) {
        auto& h = j["history"];
        s.historyPadding = h.value("padding", s.historyPadding);
        s.historySpacing = h.value("spacing", s.historySpacing);
        s.historySpeakerFontSize = h.value("speaker_font_size", s.historySpeakerFontSize);
        s.historyContentFontSize = h.value("content_font_size", s.historyContentFontSize);
        s.historyHeaderHeight    = h.value("header_height", s.historyHeaderHeight);
        s.historyFooterHeight    = h.value("footer_height", s.historyFooterHeight);
        s.historySidebarWidth    = h.value("sidebar_width", s.historySidebarWidth);
        s.historyEntryGap        = h.value("entry_gap", s.historyEntryGap);
        if (h.contains("bg_color"))      s.historyBg = ParseColor(h["bg_color"], s.historyBg);
        if (h.contains("speaker_color")) s.historySpeakerColor = ParseColor(h["speaker_color"], s.historySpeakerColor);
        if (h.contains("content_color")) s.historyContentColor = ParseColor(h["content_color"], s.historyContentColor);
        if (h.contains("dim_color"))     s.historyDimColor = ParseColor(h["dim_color"], s.historyDimColor);
    }

    if (j.contains("cursor")) {
        auto& c = j["cursor"];
        s.cursorPath = c.value("path", s.cursorPath);
        s.cursorScale = c.value("scale", s.cursorScale);
    }

    // Feature 1: Custom Font
    if (j.contains("font")) s.fontPath = j["font"].get<std::string>();

    // Feature 2: Entity Display
    if (j.contains("entity")) {
        auto& e = j["entity"];
        s.entityScale          = e.value("scale",           s.entityScale);
        s.entityFloatAmplitude = e.value("float_amplitude", s.entityFloatAmplitude);
        s.entityFloatSpeed     = e.value("float_speed",     s.entityFloatSpeed);
        s.entityShadowOpacity  = e.value("shadow_opacity",  s.entityShadowOpacity);
    }

    // Feature 3: Cursor Style
    if (j.contains("cursor")) {
        auto& c = j["cursor"];
        s.cursorShape     = c.value("shape",      s.cursorShape);
        s.cursorSize      = c.value("size",       s.cursorSize);
        s.cursorAnimSpeed = c.value("anim_speed", s.cursorAnimSpeed);
        if (c.contains("color")) s.cursorColor = ParseColor(c["color"], s.cursorColor);
    }

    // Feature 5: Background Clear Color
    if (j.contains("clear_color")) s.clearColor = ParseColor(j["clear_color"], s.clearColor);

    // Texture Overrides — parsed last so they can appear anywhere in the JSON block
    if (j.contains("box_texture"))          s.boxTexture          = ParseStyleTexture(j["box_texture"]);
    if (j.contains("label_texture"))        s.labelTexture        = ParseStyleTexture(j["label_texture"]);
    if (j.contains("choice_texture"))       s.choiceTexture       = ParseStyleTexture(j["choice_texture"]);
    if (j.contains("toast_texture"))        s.toastTexture        = ParseStyleTexture(j["toast_texture"]);
    if (j.contains("cursor_texture"))       s.cursorTexture       = ParseStyleTexture(j["cursor_texture"]);
    if (j.contains("history_bg_texture"))   s.historyBgTexture    = ParseStyleTexture(j["history_bg_texture"]);
    if (j.contains("history_pill_texture")) s.historyPillTexture  = ParseStyleTexture(j["history_pill_texture"]);

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
            auto& stylesRoot = j["styles"];
            for (auto& [name, block] : stylesRoot.items()) {
                json finalBlock = block;
                if (block.contains("extends") && block["extends"].is_string()) {
                    std::string baseName = block["extends"];
                    if (stylesRoot.contains(baseName)) {
                        json merged = stylesRoot[baseName];
                        merged.merge_patch(block);
                        finalBlock = merged;
                    } else {
                        std::cerr << "[StyleManager] Error: style '" << name << "' extends unknown style '" << baseName << "'" << std::endl;
                    }
                }
                m_styleLibrary[name] = ParseStyleBlock(finalBlock);
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

        // Feature 1: Load any new fonts referenced by styles
        for (auto& [name, style] : m_styleLibrary) {
            if (style.fontPath.empty() || m_fontCache.count(style.fontPath)) continue;
            if (FileExists(style.fontPath.c_str())) {
                Font f = LoadFontEx(style.fontPath.c_str(), 96, nullptr, 0);
                if (f.texture.id > 0) {
                    SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
                    m_fontCache[style.fontPath] = f;
                }
            } else {
                std::cerr << "[StyleManager] Font not found: " << style.fontPath << std::endl;
            }
        }

    } catch (const std::exception& ex) {
        std::cerr << "[StyleManager] Failed to parse style.json: " << ex.what() << std::endl;
    }
}

StyleManager::~StyleManager() {
    // Fonts may already be unloaded via Shutdown(); guard against double-free
    Shutdown();
}

void StyleManager::Shutdown() {
    for (auto& [path, font] : m_fontCache) UnloadFont(font);
    m_fontCache.clear();
}

Font StyleManager::GetCurrentFont() const {
    if (!m_activeStyle.fontPath.empty()) {
        auto it = m_fontCache.find(m_activeStyle.fontPath);
        if (it != m_fontCache.end()) return it->second;
    }
    return GetFontDefault();
}

Font StyleManager::GetFont(const std::string& path) {
    if (path.empty()) return GetFontDefault();
    auto it = m_fontCache.find(path);
    if (it != m_fontCache.end()) return it->second;
    
    if (FileExists(path.c_str())) {
        Font f = LoadFont(path.c_str());
        m_fontCache[path] = f;
        return f;
    }
    return GetFontDefault();
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

static float saveToastTimer = 0.0f;
static std::string saveToastMsg;

BitRenderer::BitRenderer(DialogEngine& engine) : m_engine(engine) {
    InitAudioDevice();
    CreateFallbackTexture();
    CreateVignetteTexture();
}

BitRenderer::~BitRenderer() {
    // Unload fonts first — they are GPU resources that must be freed before CloseWindow
    m_styleManager.Shutdown();
    for (auto& [path, tex] : m_textureCache) UnloadTexture(tex);
    for (auto& [path, sfx] : m_sfxCache) UnloadSound(sfx);
    for (auto& [path, mus] : m_musicCache) UnloadMusicStream(mus);
    UnloadTexture(m_fallbackTexture);
    UnloadTexture(m_vignette);
    CloseAudioDevice();
}

void BitRenderer::Draw() {
    if (!m_engine.IsActive()) return;

    // Hot reload check
    m_styleManager.Update();

    auto& style = m_styleManager.GetStyle();

    HandleAudio();

    float intensity = m_engine.GetEffectShake();
    Vector2 shake = { (float)GetRandomValue(-(int)intensity, (int)intensity),
                      (float)GetRandomValue(-(int)intensity, (int)intensity) };

    Camera2D cam = { {0,0}, {shake.x, shake.y}, 0.0f, 1.0f };

    BeginMode2D(cam);
        DrawBackground();
        DrawEntitySprites();
        DrawVFX(); // Screen fade and vignette draw OVER entities
        if (!m_engine.IsUiHidden()) {
            DrawMainBox();
            if (!m_engine.IsTextRevealing()) DrawChoiceBox();
        }
    EndMode2D();
    
    if (m_showHistory) DrawHistory();
    
    if (m_engine.IsDebugOverlayVisible()) DrawDebugOverlay();
    DrawCustomCursor();

    // Toast notification
    if (saveToastTimer > 0.0f) {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        float tx = sw * style.toastNormX - style.toastWidth - style.toastMarginX;
        float ty = sh * style.toastNormY + style.toastMarginY;
        Rectangle toastRect = { tx, ty, style.toastWidth, style.toastHeight };
        // Texture override for toast panel
        DrawStyledRect(toastRect, style.toastTexture, style.toastBg, style.toastBorder, 0.0f, 1.5f, 4);
        Font toastFont = m_styleManager.GetCurrentFont();
        float textY = ty + style.toastHeight / 2.0f - style.toastFontSize / 2.0f;
        DrawTextEx(toastFont, saveToastMsg.c_str(), { tx + 10, textY },
                   (float)style.toastFontSize, 2.0f, style.toastTextColor);
        saveToastTimer -= GetFrameTime();
    }
}

void BitRenderer::HandleAudio() {
    if (!m_engine.IsActive()) return;

    std::string activeBgm = m_engine.GetActiveBgm();
    if (!activeBgm.empty()) {
        std::string req  = activeBgm;
        std::string path = m_engine.GetMusic(req);
        if (path.empty()) path = req;
        if (m_currentMusicPath != path) {
            auto it = m_musicCache.find(path);
            if (it != m_musicCache.end()) {
                if (m_isMusicPlaying) StopMusicStream(m_currentMusic);
                m_currentMusic = it->second;
                if (m_currentMusic.frameCount > 0) {
                    PlayMusicStream(m_currentMusic);
                    m_isMusicPlaying = true;
                } else {
                    m_isMusicPlaying = false;
                }
                m_currentMusicPath = path;
            } else {
                // Not in cache, try loading once
                if (FileExists(path.c_str())) {
                    Music m = LoadMusicStream(path.c_str());
                    if (m.frameCount > 0) {
                        m_musicCache[path] = m;
                        if (m_isMusicPlaying) StopMusicStream(m_currentMusic);
                        m_currentMusic = m;
                        PlayMusicStream(m_currentMusic);
                        m_isMusicPlaying = true;
                    }
                }
                // Mark as handled even if failed to prevent frame-by-frame retries
                m_currentMusicPath = path;
            }
        }
    }
    if (m_isMusicPlaying) UpdateMusicStream(m_currentMusic);

    // Note: per-node SFX is driven by the "play_sfx" event op — see ConsumePendingSFX below.

    // Feature: Event-driven SFX (play_sfx ops)
    for (const auto& pending : m_engine.ConsumePendingSFX()) {
        std::string sfxPath = m_engine.GetSFX(pending);
        if (sfxPath.empty()) sfxPath = pending;
        PlaySFX(sfxPath);
    }
}

void BitRenderer::HandleInput() {
    if (!m_engine.IsActive()) return;
    
    // Feature: Auto-Play toggle
    if (IsKeyPressed(KEY_A)) {
        m_engine.ToggleAutoPlay();
    }

    // Feature: Message History toggle
    if (IsKeyPressed(KEY_H)) {
        m_showHistory = !m_showHistory;
        if (m_showHistory) m_historyScroll = 0; // Reset scroll on open
    }

    // Allow debug overlay toggle even while history is open
    if (IsKeyPressed(KEY_F3)) m_engine.ToggleDebugOverlay();

    if (m_showHistory) {
        m_historyScroll -= GetMouseWheelMove() * 40.0f;
        if (m_historyScroll < 0) m_historyScroll = 0;
        if (IsKeyPressed(KEY_ESCAPE)) m_showHistory = false;
        return; // Block narrative inputs while history is open
    }

    if (m_engine.IsChoiceVisible()) {
        m_debugScroll -= GetMouseWheelMove() * 20.0f;
        if (m_debugScroll < 0) m_debugScroll = 0;
    }
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) { m_engine.Next(); return; }
    if (IsKeyPressed(KEY_F5)) { m_engine.SaveGame(1); saveToastMsg = "QUICK SAVE..."; saveToastTimer = 2.0f; }
    if (IsKeyPressed(KEY_F9)) { if (m_engine.LoadGame(1)) { saveToastMsg = "RELOADING..."; saveToastTimer = 2.0f; } }

    auto& style = m_styleManager.GetStyle();

    if (!m_engine.IsTextRevealing()) {
        auto& opts = m_engine.GetVisibleOptions();
        for (int i = 0; i < (int)opts.size(); ++i) {
            if (IsKeyPressed(KEY_ONE + i)) { m_engine.SelectOption(i); return; }
            int sw = GetScreenWidth(), sh = GetScreenHeight();
            float cw  = style.choiceWidth;
            float itemH = style.optionHeight + style.optionGap;
            float pad = 25.0f;
            float ch  = opts.size() * itemH - style.optionGap + (pad * 2);
            Rectangle r = { sw * style.choiceNormX - cw / 2.0f,
                             sh * style.choiceNormY - ch / 2.0f + style.choiceOffsetY,
                             cw, ch };
            Rectangle oRect = { r.x + 20, r.y + pad + (i * itemH), r.width - 40, (float)style.optionHeight };
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), oRect)) {
                m_engine.SelectOption(i); return;
            }
        }
    }
}


void BitRenderer::DrawBackground() {
    auto& style = m_styleManager.GetStyle();
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), style.clearColor);

    std::string activeBg = m_engine.GetActiveBg();
    std::string prevBg = m_engine.GetPrevBg();
    float fade = m_engine.GetBgFadeAlpha();

    // Draw previous background if we are fading
    if (fade < 1.0f && !prevBg.empty()) {
        std::string path = m_engine.GetBackground(prevBg);
        if (path.empty()) path = prevBg;
        Texture2D tex = GetTexture(path);
        DrawTexturePro(tex, {0,0,(float)tex.width,(float)tex.height},
                       {0,0,(float)GetScreenWidth(),(float)GetScreenHeight()}, {0,0}, 0, WHITE);
    }

    if (!activeBg.empty()) {
        std::string path = m_engine.GetBackground(activeBg);
        if (path.empty()) path = activeBg;
        Texture2D tex = GetTexture(path);
        Color tint = WHITE;
        if (fade < 1.0f) tint.a = (unsigned char)(fade * 255.0f);
        DrawTexturePro(tex, {0,0,(float)tex.width,(float)tex.height},
                       {0,0,(float)GetScreenWidth(),(float)GetScreenHeight()}, {0,0}, 0, tint);
    }
}

void BitRenderer::DrawEntitySprites() {
    auto& style = m_styleManager.GetStyle();
    const auto& activeEntities = m_engine.GetActiveEntities();
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    int i = 0;
    for (const auto& [entityId, state] : activeEntities) {
        const auto* entity = m_engine.GetEntity(entityId);
        if (!entity) continue;

        std::string expr = state.expression;
        std::string path = ""; int frames = 1; float speed = 1.0f;
        float scale = style.entityScale;

        if (entity->sprites.count(expr)) {
            auto& s = entity->sprites.at(expr); path = s.path; frames = s.frames; speed = s.speed;
            scale = s.scale * style.entityScale / 3.0f;
        } else if (entity->sprites.count("idle")) {
            auto& s = entity->sprites.at("idle"); path = s.path; frames = s.frames; speed = s.speed;
            scale = s.scale * style.entityScale / 3.0f;
        }

        Texture2D tex = path.empty() ? m_fallbackTexture : GetTexture(path);
        int finalFrames = frames > 0 ? frames : 1;
        
        // Calculate animation frame
        int currentFrame = (int)(GetTime() * speed) % finalFrames;

        float normX = state.currentNormX;
        float normY = 0.45f;
        
        int fw = tex.width / finalFrames;
        Rectangle src = { (float)(currentFrame * fw), 0, (float)fw, (float)tex.height };

        m_floatOffset = m_engine.GetConfigs().enable_floating
            ? sinf((float)GetTime() * style.entityFloatSpeed + i * 2.0f) * style.entityFloatAmplitude
            : 0.0f;

        Vector2 pos = { (sw * normX) - (fw * scale) / 2.0f,
                        (sh * normY) - (tex.height * scale) / 2.0f + m_floatOffset };

        Color tint = WHITE;
        tint.a = (unsigned char)(state.alpha * 255.0f);

        if (m_engine.GetConfigs().enable_shadows) {
            DrawEllipse((int)pos.x + (int)(fw*scale/2), (int)pos.y + (int)(tex.height*scale),
                        (int)(fw*scale/3), 10, Fade(BLACK, style.entityShadowOpacity * state.alpha));
        }

        DrawTexturePro(tex, src, {pos.x, pos.y, fw * scale, tex.height * scale}, {0,0}, 0, tint);
        i++;
    }
}

void BitRenderer::DrawMainBox() {
    auto* e  = m_engine.GetCurrentEntity();
    auto& style = m_styleManager.GetStyle();
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    float bw = sw * style.boxWidthNorm;
    float bh = (style.boxHeightNorm > 0) ? (sh * style.boxHeightNorm) : style.boxHeight;
    float bx = sw * style.boxNormX - bw / 2.0f;
    float by = sh * style.boxNormY - bh / 2.0f;

    if (style.boxAnchor == "top") { 
        by = style.boxMarginBottom; 
    } else if (style.boxAnchor == "bottom") { 
        by = sh - bh - style.boxMarginBottom; 
    } else if (style.boxAnchor == "left") { 
        bx = style.boxMarginBottom; 
    } else if (style.boxAnchor == "right") { 
        bx = sw - bw - style.boxMarginBottom; 
    }

    Rectangle box = { bx, by, bw, bh };

    // Texture override for dialog box (falls back to rounded rect + border)
    DrawStyledRect(box, style.boxTexture, style.boxBg, style.boxBorder,
                   style.boxRoundness, style.boxBorderThick, 10);

    if (e) {
        Font font = m_styleManager.GetCurrentFont();
        float tw = MeasureTextEx(font, e->name.c_str(), (float)style.labelFontSize, 2).x;
        float lw = tw + style.labelPadding * 2;
        
        float lx = box.x + style.labelOffsetX;
        if (style.labelAlign == "center") lx = box.x + box.width / 2.0f - lw / 2.0f;
        else if (style.labelAlign == "right") lx = box.x + box.width - lw - style.labelOffsetX;
        float ly = box.y + style.labelOffsetY;
        Rectangle labelRect = { lx, ly, lw, (float)style.labelHeight };

        // Texture override for name label
        DrawStyledRect(labelRect, style.labelTexture, style.labelBg, style.labelBorder,
                       0.0f, 1.5f, 4);
        float nameY = ly + style.labelHeight / 2.0f - style.labelFontSize / 2.0f;
        DrawTextEx(font, e->name.c_str(), { lx + style.labelPadding, nameY },
                   (float)style.labelFontSize, 2.0f, style.labelTextColor);
    }

    DrawRichText(m_engine.GetParsedContent(), m_engine.GetRevealedCount(),
                 (int)(box.x + style.boxPadding), (int)(box.y + 40),
                 style.textFontSize, (int)bw - style.boxPadding * 2, style.textColor, style.textLineSpacing);

    // Texture override for cursor sprite (replaces all drawn cursor shapes)
    if (!m_engine.IsTextRevealing()) {
        float anim  = sinf((float)GetTime() * style.cursorAnimSpeed);
        float bR    = box.x + box.width  - 18.0f;
        float bB    = box.y + box.height - 14.0f;
        float sz    = style.cursorSize;

        if (!style.cursorTexture.path.empty()) {
            Texture2D ctex = GetTexture(style.cursorTexture.path);
            if (ctex.id > 0) {
                float scale = (anim * 0.08f + 1.0f);  // gentle breathing pulse
                float tw = ctex.width  * scale;
                float th = ctex.height * scale;
                DrawTexturePro(ctex,
                    { 0, 0, (float)ctex.width, (float)ctex.height },
                    { bR - tw / 2.0f, bB - th / 2.0f, tw, th },
                    { 0, 0 }, 0.0f, style.cursorTexture.tint);
            }
        } else {
            Color col = Fade(style.cursorColor, 0.9f);
            if (style.cursorShape == "dot") {
                DrawCircle((int)bR, (int)bB, sz * (0.8f + anim * 0.2f), col);
            } else if (style.cursorShape == "bar") {
                DrawRectangle((int)(bR - sz * 2.0f), (int)bB,
                              (int)(sz * 2.0f), (int)(sz * 0.45f),
                              Fade(col, (anim + 1.0f) * 0.5f));
            } else {
                float pulse = anim * 3.0f;
                DrawTriangle({ bR - sz * 1.25f, bB + pulse },
                             { bR,              bB - sz * 1.25f + pulse },
                             { bR - sz * 2.5f,  bB - sz * 1.25f + pulse }, col);
            }
        }
    }
}

void BitRenderer::DrawChoiceBox() {
    auto& opts = m_engine.GetVisibleOptions();
    if (opts.empty()) return;

    auto& style = m_styleManager.GetStyle();
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    float cw    = style.choiceWidth;
    float itemH = style.optionHeight + style.optionGap;
    float pad   = 25.0f;
    float ch    = opts.size() * itemH - style.optionGap + (pad * 2);
    float cx    = sw * style.choiceNormX - cw / 2.0f;
    float cy    = sh * style.choiceNormY - ch / 2.0f + style.choiceOffsetY;
    Rectangle r = { cx, cy, cw, ch };

    // Texture override for choice panel
    DrawStyledRect(r, style.choiceTexture, Fade(style.choiceBg, 0.95f), style.choiceBorder,
                   style.choiceRoundness, style.choiceBorderThick, 10);

    Font font = m_styleManager.GetCurrentFont();
    for (int i = 0; i < (int)opts.size(); ++i) {
        Rectangle oRect = { r.x + 20, r.y + pad + (i * itemH), r.width - 40, (float)style.optionHeight };
        Color col = (opts[i].style == "premium") ? style.optionPremium : style.optionColor;
        if (CheckCollisionPointRec(GetMousePosition(), oRect)) {
            DrawRectangleRec(oRect, Fade(col, 0.2f));
            DrawRectangleLinesEx(oRect, 1.0f, col);
            col = style.optionHover;
        }
        // Feature 1: Use custom font for option text
        float textY = oRect.y + style.optionHeight / 2.0f - style.optionFontSize / 2.0f;
        DrawTextEx(font, opts[i].content.c_str(), { oRect.x + 40, textY },
                   (float)style.optionFontSize, 2.0f, col);
        DrawCircle((int)oRect.x + 20, (int)oRect.y + style.optionHeight / 2, 4, col);
    }
}

void BitRenderer::DrawVFX() {
    auto& style = m_styleManager.GetStyle();
    if (m_engine.GetConfigs().enable_vignette)
        DrawTexturePro(m_vignette, {0,0,64,64},
                       {0,0,(float)GetScreenWidth(),(float)GetScreenHeight()},
                       {0,0}, 0, Fade(WHITE, style.vignetteOpacity));
    
    // Feature: Global Screen Fade
    float screenFade = m_engine.GetScreenFadeAlpha();
    if (screenFade > 0.01f) {
        BitColor bc = m_engine.GetScreenFadeColor();
        Color c = { bc.r, bc.g, bc.b, (unsigned char)(screenFade * 255.0f) };
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), c);
    }
}


// Debug overlay — intentionally NOT driven by style

void BitRenderer::DrawDebugOverlay() {

    const auto& vars   = m_engine.GetAllVariables();
    const auto& trace  = m_engine.GetEventTrace();
    const auto& errors = m_engine.GetErrors();
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    
    int gap = 12, pad = 10;
    int cols = 4;
    if (sw < 1100) cols = 2;
    if (sw < 600)  cols = 1;
    
    int rows_count = (6 + cols - 1) / cols;
    int panelW = (sw - 40 - (cols - 1) * gap) / cols;
    if (panelW > 350) panelW = 350;
    
    // Scale height based on rows available
    int panelH = (sh - 60 - (rows_count - 1) * (gap + 20)) / rows_count;
    if (panelH > 380) panelH = 380;
    if (panelH < 150) panelH = 150;

    int totalW = cols * panelW + (cols - 1) * gap;
    int startX = sw - totalW - pad;
    int startY = 20;

    int scrollOffset = (int)m_debugScroll;

    // We now have 5 panels. Draw unified background:
    int fullH = rows_count * panelH + (rows_count - 1) * (gap + 15);
    DrawRectangle(startX - pad, startY - pad, totalW + pad * 2, fullH + pad * 2, Fade(BLACK, 0.85f));

    auto GetPanelRect = [&](int idx) -> Rectangle {
        int r = idx / cols;
        int c = idx % cols;
        return { (float)(startX + c * (panelW + gap)), (float)(startY + r * (panelH + gap + 15)), (float)panelW, (float)panelH };
    };

    // Panel 1: VM Inspector
    Rectangle r1 = GetPanelRect(0);
    int px = (int)r1.x, py = (int)r1.y;
    DrawText("VM INSPECTOR", px, py, 13, SKYBLUE);
    int dy = py + 22;
    DrawText(TextFormat("PC:      %d", m_engine.GetCurrentPC()), px, dy, 10, RAYWHITE); dy += 14;
    DrawText(TextFormat("ACTIVE:  %s", m_engine.IsActive() ? "YES" : "NO"), px, dy, 10, RAYWHITE); dy += 14;
    DrawText(TextFormat("SPEAKER: %s", m_engine.GetCurrentEntity() ? m_engine.GetCurrentEntity()->id.c_str() : "NONE"), px, dy, 10, RAYWHITE); dy += 14;
    DrawText(TextFormat("WAITING: %s", m_engine.IsTextRevealing() ? "YES" : "NO"), px, dy, 10, RAYWHITE); dy += 14;


    // Panel 2: Variable Watcher
    Rectangle r2 = GetPanelRect(1);
    px = (int)r2.x; py = (int)r2.y; dy = py;
    DrawText("VARIABLE WATCHER", px, dy, 13, GREEN); dy += 20;
    for (auto const& [name, val] : vars) {
        if (dy - py + scrollOffset < r2.height - 10 && dy - py + scrollOffset > 0)
            DrawText(TextFormat("%-16s %d", name.c_str(), val), px, dy, 10, LIME);
        dy += 13;
    }

    // Panel 3: Event Trace
    Rectangle r3 = GetPanelRect(2);
    px = (int)r3.x; py = (int)r3.y; dy = py;
    DrawText("EVENT TRACE", px, dy, 13, ORANGE); dy += 20;
    int maxTrace = 12, traceStart = (int)trace.size() > maxTrace ? (int)trace.size() - maxTrace : 0;
    for (int i = traceStart; i < (int)trace.size(); ++i) {
        const auto& t = trace[i];
        if (dy - py + scrollOffset < r3.height - 20 && dy - py + scrollOffset > 0) {
            DrawText(TextFormat("%s: %s", t.op.c_str(), t.var.c_str()), px, dy, 10, ORANGE); 
            DrawText(TextFormat("  %d->%d [%s]", t.old_value, t.new_value, t.node_id.c_str()), px, dy + 12, 8, Fade(ORANGE, 0.7f));
        }
        dy += 25;
    }

    // Panel 4: Cinematic State & Engine Flags
    Rectangle r4 = GetPanelRect(3);
    px = (int)r4.x; py = (int)r4.y; dy = py;
    DrawText("ENGINE & CINEMATIC STATE", px, dy, 13, GOLD); dy += 22;
    
    DrawText(TextFormat("UI Hidden:   %s", m_engine.IsUiHidden() ? "TRUE" : "FALSE"), px, dy, 10, m_engine.IsUiHidden() ? YELLOW : GRAY); dy += 14;
    DrawText(TextFormat("Auto-Play:   %s", m_engine.IsAutoPlaying() ? "ON" : "OFF"), px, dy, 10, m_engine.IsAutoPlaying() ? LIME : GRAY); dy += 14;
    DrawText(TextFormat("Transition:  %s", m_engine.IsTransitioning() ? "ACTIVE" : "FALSE"), px, dy, 10, m_engine.IsTransitioning() ? ORANGE : GRAY); dy += 14;
    std::string waitType = m_engine.GetWaitActionType();
    DrawText(TextFormat("Wait Action: %s", waitType.empty() ? "NONE" : waitType.c_str()), px, dy, 10, !waitType.empty() ? ORANGE : GRAY); dy += 14;
    DrawText(TextFormat("BG Fade:     %.2f", m_engine.GetBgFadeAlpha()), px, dy, 10, RAYWHITE); dy += 18;

    DrawText("Entities:", px, dy, 11, SKYBLUE); dy += 15;
    const auto& entities = m_engine.GetActiveEntities();
    for (const auto& [id, s] : entities) {
        if (dy > r4.y + r4.height - 20) break;
        DrawText(TextFormat("[%s] X:%.2f->%.2f A:%.2f->%.2f", 
                 id.substr(0, 6).c_str(), s.currentNormX, s.targetNormX, s.alpha, s.targetAlpha), 
                 px, dy, 9, (s.moveTimer < s.moveDuration || s.fadeTimer < s.fadeDuration) ? LIME : GRAY);
        dy += 12;
    }
    if (entities.empty()) DrawText("(none)", px, dy, 10, Fade(RAYWHITE, 0.4f)); 

    // Panel 5: Local Scope & Stack
    Rectangle r5 = GetPanelRect(4);
    px = (int)r5.x; py = (int)r5.y; dy = py;
    DrawText("LOCAL SCOPE & STACK", px, dy, 13, VIOLET); dy += 22;
    
    const auto& stack = m_engine.GetCallStack();
    DrawText(TextFormat("Call Depth: %d", (int)stack.size()), px, dy, 10, RAYWHITE); dy += 14;
    for (int i = 0; i < (int)stack.size(); ++i) {
        DrawText(TextFormat("  [%d] @ PC:%d", i, stack[i]), px, dy, 9, Fade(RAYWHITE, 0.7f));
        dy += 12;
    }
    dy += 10;

    DrawText("Active Locals:", px, dy, 11, SKYBLUE); dy += 15;
    const auto& scopes = m_engine.GetLocalScopes();
    if (!scopes.empty()) {
        for (const auto& [name, val] : scopes.back()) {
            DrawText(TextFormat("%-14s %d", name.c_str(), val), px, dy, 10, VIOLET);
            dy += 12;
        }
    } else {
        DrawText("(none)", px, dy, 10, GRAY);
    }

    // Panel 6: Bytecode Inspector
    Rectangle r6 = GetPanelRect(5);
    px = (int)r6.x; py = (int)r6.y; dy = py;
    DrawText("BYTECODE INSPECTOR", px, dy, 13, PURPLE); dy += 20;
    const auto& bytecode = m_engine.GetProject().bytecode;
    int currentPC = m_engine.GetCurrentPC();
    
    int viewRange = (panelH - 30) / 12;
    int startIns = std::max(0, currentPC - viewRange / 2);
    int endIns = std::min((int)bytecode.size(), startIns + viewRange);

    for (int i = startIns; i < endIns; ++i) {
        const auto& ins = bytecode[i];
        Color col = (i == currentPC - 1) ? YELLOW : (i == currentPC) ? GREEN : GRAY;
        if (i == currentPC) DrawRectangle(px - 2, dy, panelW, 11, Fade(GREEN, 0.2f));
        
        std::string opStr = "UNKNOWN";
        switch(ins.op) {
            case BitOp::SAY:    opStr = "SAY"; break;
            case BitOp::TEXT:   opStr = "TEXT"; break;
            case BitOp::CHOICE: opStr = "CHOICE"; break;
            case BitOp::IF:     opStr = "IF"; break;
            case BitOp::IF_REF: opStr = "IF_REF"; break;
            case BitOp::GOTO:   opStr = "GOTO"; break;
            case BitOp::SET:    opStr = "SET"; break;
            case BitOp::SET_REF:opStr = "SET_R"; break;
            case BitOp::ADD:    opStr = "ADD"; break;
            case BitOp::ADD_REF:opStr = "ADD_R"; break;
            case BitOp::SUB:    opStr = "SUB"; break;
            case BitOp::SUB_REF:opStr = "SUB_R"; break;
            case BitOp::MUL:    opStr = "MUL"; break;
            case BitOp::MUL_REF:opStr = "MUL_R"; break;
            case BitOp::DIV:    opStr = "DIV"; break;
            case BitOp::DIV_REF:opStr = "DIV_R"; break;
            case BitOp::EVENT:  opStr = "EVENT"; break;
            case BitOp::BG:     opStr = "BG"; break;
            case BitOp::BGM:    opStr = "BGM"; break;
            case BitOp::LABEL:  opStr = "LABEL"; break;
            case BitOp::HALT:   opStr = "HALT"; break;
            default: break;
        }

        std::string argsStr = "";
        for (const auto& a : ins.args) {
            std::string cleaned = a;
            std::replace(cleaned.begin(), cleaned.end(), '\n', ' ');
            argsStr += " " + (cleaned.size() > 12 ? cleaned.substr(0, 10) + ".." : cleaned);
        }
        
        std::string metaStr = ins.metadata.empty() ? "" : TextFormat(" [+%d]", (int)ins.metadata.size());
        DrawText(TextFormat("%04d %-6s%s%s", i, opStr.c_str(), argsStr.c_str(), metaStr.c_str()), px, dy, 9, col);
        dy += 11;
    }

    // Error log stays at far bottom
    if (!errors.empty()) {
        DrawRectangle(0, sh - 26, sw, 26, Fade(RED, 0.75f));
        DrawText(errors.back().c_str(), 10, sh - 20, 11, WHITE);
        DrawText(TextFormat("(%d total - check debug.log)", (int)errors.size()), sw - 290, sh - 20, 11, Fade(WHITE, 0.8f));
    }
}


int BitRenderer::DrawRichText(const std::vector<RichChar>& content, int limit, int x, int y, int fontSize, int maxWidth, Color defaultColor, int lineSpacing) {
    // Feature 1: Use the active style's custom font
    Font font = m_styleManager.GetCurrentFont();
    int curX = x;
    int curY = y;
    float time = (float)GetTime();
    float spacing = 2.0f;
    int lineOffset = fontSize + lineSpacing;

    float spaceWidth = MeasureTextEx(font, " ", (float)fontSize, spacing).x;

    int i = 0;
    while (i < limit && i < (int)content.size()) {
        const char* charPtr = content[i].ch;
        if (charPtr[0] == ' ' || charPtr[0] == '\n') {
            if (charPtr[0] == '\n') {
                curX = x;
                curY += lineOffset;
            } else {
                curX += spaceWidth + spacing;
            }
            i++;
            continue;
        }

        int wordEnd = i;
        float wordWidth = 0.0f;
        while (wordEnd < limit && wordEnd < (int)content.size() && content[wordEnd].ch[0] != ' ' && content[wordEnd].ch[0] != '\n') {
            wordWidth += MeasureTextEx(font, content[wordEnd].ch, (float)fontSize, spacing).x + spacing;
            wordEnd++;
        }

        if (curX + wordWidth > x + maxWidth && curX > x) {
            curX = x;
            curY += lineOffset;
        }

        for (int j = i; j < wordEnd; j++) {
            const auto& rc = content[j];
            
            // Per-character font override
            Font f = font;
            if (!rc.font.empty()) {
                std::string fontPath = m_engine.GetProject().fonts.count(rc.font) ? m_engine.GetProject().fonts.at(rc.font) : "";
                if (!fontPath.empty()) f = GetFont(fontPath);
            }

            Color c = (rc.color.a == 0 && rc.color.r == 0 && rc.color.g == 0 && rc.color.b == 0) 
                      ? defaultColor 
                      : Color{ rc.color.r, rc.color.g, rc.color.b, rc.color.a };
            
            float ox = 0.0f, oy = 0.0f;
            if (rc.shake) { ox = GetRandomValue(-2, 2); oy = GetRandomValue(-2, 2); }
            if (rc.wave)  { oy += sinf(time * 6.0f + curX * 0.05f) * 4.0f; }

            Vector2 pos = { (float)curX + ox, (float)curY + oy };
            float charW = MeasureTextEx(f, rc.ch, (float)fontSize, spacing).x;
            DrawTextEx(f, rc.ch, pos, (float)fontSize, spacing, c);
            curX += (int)(charW + spacing);
        }
        
        i = wordEnd;
    }
    return curY + lineOffset;
}

Texture2D BitRenderer::GetTexture(const std::string& path) {
    if (path.empty()) return m_fallbackTexture;
    auto it = m_textureCache.find(path);
    if (it != m_textureCache.end()) {
        if (it->second.id == 0) return m_fallbackTexture;
        return it->second;
    }
    if (FileExists(path.c_str())) {
        Texture2D tex = LoadTexture(path.c_str());
        m_textureCache[path] = tex;
        if (tex.id > 0) return tex;
    } else {
        m_textureCache[path] = Texture2D{}; // Mark as failed (all fields zeroed)
    }
    return m_fallbackTexture;
}

void BitRenderer::PreloadAssets() {
    auto& proj = m_engine.GetProject();
    std::cout << "[BitRenderer] Preloading assets into memory..." << std::endl;

    for (auto const& [id, path] : proj.backgrounds) GetTexture(path);
    
    for (auto const& [id, path] : proj.music) {
        if (path.empty() || m_musicCache.count(path)) continue;
        Music m = LoadMusicStream(path.c_str());
        m_musicCache[path] = m; // Even if it fails (frameCount=0)
    }

    for (auto const& [id, path] : proj.sfx) {
        if (path.empty() || m_sfxCache.count(path)) continue;
        Sound s = LoadSound(path.c_str());
        m_sfxCache[path] = s; // Even if it fails (frameCount=0)
    }

    for (auto const& [id, ent] : proj.entities) {
        for (auto const& [s_id, s_def] : ent.sprites) GetTexture(s_def.path);
    }
    for (auto const& [id, path] : proj.fonts) GetFont(path);
}

void BitRenderer::PlaySFX(const std::string& path) {
    // Trust the preloaded cache; avoid redundant filesystem calls at playback time
    auto it = m_sfxCache.find(path);
    if (it != m_sfxCache.end() && it->second.frameCount > 0) {
        PlaySound(it->second);
    }
}

void BitRenderer::CreateFallbackTexture() {
    Image img = GenImageChecked(64, 64, 32, 32, MAGENTA, BLACK);
    m_fallbackTexture = LoadTextureFromImage(img); UnloadImage(img);
}

void BitRenderer::CreateVignetteTexture() {
    Image img = GenImageGradientRadial(64, 64, 0.0f, BLANK, BLACK);
    m_vignette = LoadTextureFromImage(img); UnloadImage(img);
}


// DrawStyledRect: texture-first rendering with solid-rect fallback

void BitRenderer::DrawStyledRect(Rectangle rect, const StyleTexture& stex,
                                 Color fallbackBg, Color fallbackBorder,
                                 float roundness, float borderThick, int segments) {
    if (!stex.path.empty()) {
        Texture2D tex = GetTexture(stex.path);
        if (tex.id > 0) {
            if (stex.nineSlice) {
                NPatchInfo npi = {
                    { 0, 0, (float)tex.width, (float)tex.height },
                    stex.sliceLeft, stex.sliceTop, stex.sliceRight, stex.sliceBottom,
                    NPATCH_NINE_PATCH
                };
                DrawTextureNPatch(tex, npi, rect, { 0.0f, 0.0f }, 0.0f, stex.tint);
            } else {
                DrawTexturePro(tex,
                    { 0, 0, (float)tex.width, (float)tex.height },
                    rect, { 0.0f, 0.0f }, 0.0f, stex.tint);
            }
            return; // Texture provides its own border; skip fallback
        }
    }
    // Fallback: solid rounded rectangle + border
    DrawRectangleRounded(rect, roundness, segments, fallbackBg);
    if (borderThick > 0.0f)
        DrawRectangleRoundedLinesEx(rect, roundness, segments, borderThick, fallbackBorder);
}

void BitRenderer::DrawHistory() {
    UIStyle style = m_styleManager.GetStyle();
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Font font = m_styleManager.GetCurrentFont();

    // Backdrop
    DrawStyledRect({ 0, 0, (float)sw, (float)sh }, style.historyBgTexture,
                   style.historyBg, BLANK, 0.0f, 0.0f, 0);

    // Layout constants
    const int HEADER_H   = style.historyHeaderHeight;
    const int FOOTER_H   = style.historyFooterHeight;
    const int SIDE_W     = style.historySidebarWidth;
    const int CONTENT_X  = SIDE_W + 20;
    const int CONTENT_W  = sw - CONTENT_X - 24;
    const int ENTRY_GAP  = style.historyEntryGap;
    const float SP_SIZE  = style.historySpeakerFontSize;
    const float CT_SIZE  = style.historyContentFontSize;
    const Color DIM      = style.historyDimColor;
    const Color ACCENT   = style.historySpeakerColor;

    const auto& history = m_engine.GetHistory();

    // Measure total height (needed for scroll clamp + progress bar)
    float totalContentH = (float)HEADER_H + 10.0f;
    for (size_t i = 0; i < history.size(); ++i) {
        float charW = CT_SIZE * 0.55f;
        int charsPerLine = std::max(1, (int)(CONTENT_W / charW));
        int chars = (int)history[i].richContent.size();
        int lines = std::max(1, (chars + charsPerLine - 1) / charsPerLine);
        float contentH = lines * (CT_SIZE + 4);
        float pillH = SP_SIZE + 8;
        totalContentH += std::max(contentH, pillH) + ENTRY_GAP + 12;
    }

    float viewH      = (float)(sh - HEADER_H - FOOTER_H);
    float maxScroll  = std::max(0.0f, totalContentH - viewH);
    if (m_historyScroll > maxScroll) m_historyScroll = maxScroll;
    if (m_historyScroll < 0)         m_historyScroll = 0;

    // Clip to content area (between header and footer)
    BeginScissorMode(0, HEADER_H, sw, sh - HEADER_H - FOOTER_H);

    float py = (float)HEADER_H + 10.0f - m_historyScroll;

    for (size_t i = 0; i < history.size(); ++i) {
        const auto& entry = history[i];

        // Only draw if at least partially visible
        if (py < (float)(sh - FOOTER_H) && py + 80 > (float)HEADER_H) {
            
            // Draw Rich Content first to get exact height
            int newY = DrawRichText(entry.richContent, (int)entry.richContent.size(),
                                    CONTENT_X, (int)py, (int)CT_SIZE, CONTENT_W,
                                    style.historyContentColor);
                                    
            float contentH = (float)newY - py;
            float pillH = SP_SIZE + 8;
            float entryH = std::max(contentH, pillH);

            // Vertically center pill relative to content height
            float pillY = py + (entryH - pillH) * 0.5f;

            // Entry number (far left, dimmed)
            DrawText(TextFormat("#%d", (int)(i + 1)), 8, (int)pillY + 2, 9, ColorAlpha(RAYWHITE, 0.25f));

            // Speaker sidebar pill
            float spW = MeasureTextEx(font, entry.speaker.c_str(), SP_SIZE, 1).x + 16;
            
            // Center horizontally within the sidebar
            float spX = ((float)SIDE_W - spW) * 0.5f;
            
            DrawStyledRect({ spX, pillY - 2, spW, pillH }, style.historyPillTexture,
                           ColorAlpha(ACCENT, 0.18f), BLANK, 0.4f, 0.0f, 6);
            DrawTextEx(font, entry.speaker.c_str(), { spX + 8, pillY }, SP_SIZE, 1, ACCENT);

            // Vertical sidebar accent line
            DrawRectangle(SIDE_W - 2, (int)py, 2, (int)entryH, ColorAlpha(ACCENT, 0.25f));

            py += entryH + ENTRY_GAP;

            // Divider between entries
            if (i + 1 < history.size()) {
                DrawLineEx({ (float)SIDE_W, py }, { (float)(sw - 12), py }, 1.0f, DIM);
                py += 12;
            }
        } else {
            // Off-screen estimate
            float charW = CT_SIZE * 0.55f;
            int charsPerLine = std::max(1, (int)(CONTENT_W / charW));
            int chars = (int)entry.richContent.size();
            int lines = std::max(1, (chars + charsPerLine - 1) / charsPerLine);
            float contentH = lines * (CT_SIZE + 4);
            float pillH = SP_SIZE + 8;
            py += std::max(contentH, pillH) + ENTRY_GAP + 12;
        }
    }

    EndScissorMode();

    // Scroll progress bar (right edge)
    if (maxScroll > 0) {
        float barH   = (float)(sh - HEADER_H - FOOTER_H);
        float ratio  = m_historyScroll / maxScroll;
        float thumbH = std::max(30.0f, barH * viewH / totalContentH);
        float thumbY = (float)HEADER_H + ratio * (barH - thumbH);
        DrawRectangle(sw - 6, HEADER_H, 6, (int)barH,     ColorAlpha(WHITE, 0.06f));
        DrawRectangle(sw - 6, (int)thumbY, 6, (int)thumbH, ColorAlpha(ACCENT, 0.6f));
    }

    // Pinned header bar
    DrawRectangleGradientV(0, 0, sw, HEADER_H + 8, ColorAlpha(BLACK, 0.96f), ColorAlpha(BLACK, 0));
    DrawRectangle(0, 0, sw, HEADER_H, ColorAlpha(BLACK, 0.95f));
    DrawLineEx({ 0, (float)HEADER_H }, { (float)sw, (float)HEADER_H }, 1.5f, ColorAlpha(ACCENT, 0.4f));
    DrawTextEx(font, "MESSAGE HISTORY", { 18, 14 }, SP_SIZE + 10, 1, ACCENT);
    int cnt = (int)history.size();
    DrawText(TextFormat("%d entries", cnt), 22, (int)(14 + SP_SIZE + 12), 9, ColorAlpha(RAYWHITE, 0.4f));
    DrawText("[ H ] or [ ESC ] to close  |  Mouse Wheel to scroll",
             sw - 310, HEADER_H / 2 - 5, 10, ColorAlpha(RAYWHITE, 0.4f));

    // Pinned footer bar
    int fy = sh - FOOTER_H;
    DrawRectangle(0, fy, sw, FOOTER_H, ColorAlpha(BLACK, 0.95f));
    DrawLineEx({ 0, (float)fy }, { (float)sw, (float)fy }, 1.0f, ColorAlpha(ACCENT, 0.25f));
    if (history.empty()) {
        DrawText("No messages yet.", 18, fy + 10, 10, ColorAlpha(RAYWHITE, 0.3f));
    } else {
        const auto& last = history.back();
        DrawText(TextFormat("Latest: [%s]  %s", last.speaker.c_str(),
                 last.content.substr(0, 60).c_str()),
                 18, fy + 10, 10, ColorAlpha(RAYWHITE, 0.45f));
    }
}

void BitRenderer::DrawCustomCursor() {
    UIStyle style = m_styleManager.GetStyle();
    if (style.cursorPath.empty()) {
        ShowCursor();
        return;
    }

    if (m_currentCursorPath != style.cursorPath) {
        m_currentCursorPath = style.cursorPath;
        m_customCursor = GetTexture(m_currentCursorPath);
        if (m_customCursor.id != 0) HideCursor();
    }

    if (m_customCursor.id != 0) {
        Vector2 mpos = GetMousePosition();
        float scale = style.cursorScale;
        DrawTextureEx(m_customCursor, {mpos.x, mpos.y}, 0.0f, scale, WHITE);
    }
}
