#include "headers/BitRenderer.hpp"
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <cmath>

using json = nlohmann::ordered_json;

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

static float saveToastTimer = 0.0f;
static std::string saveToastMsg = "";
BitRenderer::BitRenderer(DialogEngine& engine) : m_engine(engine) {
    InitAudioDevice();
    CreateFallbackTexture();
    CreateVignetteTexture();
}

BitRenderer::~BitRenderer() {
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
        DrawVFX(); // Vignette shakes with the camera now for more impact
        DrawEntitySprite();
        DrawMainBox();
        if (!m_engine.IsTextRevealing()) DrawChoiceBox();
    EndMode2D();
    
    if (m_engine.IsDebugOverlayVisible()) DrawDebugOverlay();

    // Toast notification
    if (saveToastTimer > 0.0f) {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        float tx = sw * style.toastNormX - style.toastWidth - style.toastMarginX;
        float ty = sh * style.toastNormY + style.toastMarginY;
        DrawRectangle((int)tx, (int)ty, (int)style.toastWidth, (int)style.toastHeight, style.toastBg);
        DrawRectangleLinesEx({ tx, ty, style.toastWidth, style.toastHeight }, 1.5f, style.toastBorder);
        DrawText(saveToastMsg.c_str(), (int)tx + 10,
                 (int)ty + (int)(style.toastHeight / 2) - style.toastFontSize / 2,
                 style.toastFontSize, style.toastTextColor);
        saveToastTimer -= GetFrameTime();
    }
}

void BitRenderer::HandleAudio() {
    const auto* node = m_engine.GetCurrentNode();
    if (!node) return;

    if (node->metadata.count("bgm")) {
        std::string req  = node->metadata.at("bgm");
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

    if (node->metadata.count("sfx")) {
        std::string req  = node->metadata.at("sfx");
        std::string path = m_engine.GetSFX(req);
        if (path.empty()) path = req;
        
        // Reset lastSFX if the node has changed to allow repetitions
        if (m_lastSFXNodeId != m_engine.GetCurrentNodeId()) {
            m_lastSFX = "";
            m_lastSFXNodeId = m_engine.GetCurrentNodeId();
        }

        if (m_lastSFX != path) { 
            PlaySFX(path); 
            m_lastSFX = path; 
        }
    }
}

void BitRenderer::HandleInput() {
    if (!m_engine.IsActive()) return;
    
    // Debug Scroll
    std::string dmode = m_engine.GetDebugMode();
    if (dmode == "debug_overlay" || dmode == "debug_all") {
        m_debugScroll -= GetMouseWheelMove() * 20.0f;
        if (m_debugScroll < 0) m_debugScroll = 0;
    }

    if (IsKeyPressed(KEY_F3)) m_engine.ToggleDebugOverlay();
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

// ============================================================
void BitRenderer::DrawBackground() {
    const auto* node = m_engine.GetCurrentNode();
    if (node && node->metadata.count("bg")) {
        std::string req  = node->metadata.at("bg");
        std::string path = m_engine.GetBackground(req);
        if (path.empty()) path = req;
        Texture2D bg = GetTexture(path);
        DrawTexturePro(bg, {0,0,(float)bg.width,(float)bg.height},
                       {0,0,(float)GetScreenWidth(),(float)GetScreenHeight()}, {0,0}, 0, WHITE);
    }
}

void BitRenderer::DrawEntitySprite() {
    const auto* entity = m_engine.GetCurrentEntity();
    const auto* node   = m_engine.GetCurrentNode();
    if (!entity || !node) return;

    std::string expr = node->metadata.count("expression") ? node->metadata.at("expression") : "idle";
    std::string path = ""; int frames = 1; float speed = 1.0f; float scale = 3.0f;

    if (entity->sprites.count(expr)) {
        auto& s = entity->sprites.at(expr); path = s.path; frames = s.frames; speed = s.speed; scale = s.scale;
    } else if (entity->sprites.count("idle")) {
        auto& s = entity->sprites.at("idle"); path = s.path; frames = s.frames; speed = s.speed; scale = s.scale;
    }

    Texture2D tex = path.empty() ? m_fallbackTexture : GetTexture(path);
    int finalFrames = frames > 0 ? frames : 1;
    m_animTimer += GetFrameTime();
    if (m_animTimer >= 1.0f / (speed > 0 ? speed : 1.0f)) { m_animTimer = 0.0f; m_animFrame = (m_animFrame + 1) % finalFrames; }

    float normX = entity->default_pos_x;
    if (node->metadata.count("pos")) {
        std::string p = node->metadata.at("pos");
        if      (p == "left")   normX = 0.2f;
        else if (p == "right")  normX = 0.8f;
        else if (p == "center") normX = 0.5f;
    }
    if (node->metadata.count("pos_x")) normX = std::stof(node->metadata.at("pos_x"));
    float normY = node->metadata.count("pos_y") ? std::stof(node->metadata.at("pos_y")) : 0.45f;

    int fw = tex.width / finalFrames;
    Rectangle src = { (float)(m_animFrame * fw), 0, (float)fw, (float)tex.height };
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    m_floatOffset = m_engine.GetConfigs().enable_floating ? (float)sin(GetTime() * 2.0f) * 10.0f : 0.0f;
    Vector2 pos = { (sw * normX) - (fw * scale) / 2.0f,
                    (sh * normY) - (tex.height * scale) / 2.0f + m_floatOffset };

    if (m_engine.GetConfigs().enable_shadows)
        DrawEllipse((int)pos.x + (int)(fw*scale/2), (int)pos.y + (int)(tex.height*scale),
                    (int)(fw*scale/3), 10, Fade(BLACK, 0.4f));

    DrawTexturePro(tex, src, { pos.x, pos.y, fw * scale, tex.height * scale }, {0,0}, 0, WHITE);
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

    DrawRectangleRounded(box, style.boxRoundness, 10, style.boxBg);
    DrawRectangleRoundedLinesEx(box, style.boxRoundness, 10, style.boxBorderThick, style.boxBorder);

    if (e) {
        float tw = MeasureTextEx(GetFontDefault(), e->name.c_str(), (float)style.labelFontSize, 2).x;
        float lw = tw + style.labelPadding * 2;
        
        float lx = box.x + style.labelOffsetX;
        if (style.labelAlign == "center") lx = box.x + box.width / 2.0f - lw / 2.0f;
        else if (style.labelAlign == "right") lx = box.x + box.width - lw - style.labelOffsetX;

        float ly = box.y + style.labelOffsetY;
        
        DrawRectangle((int)lx, (int)ly, (int)lw, style.labelHeight, style.labelBg);
        DrawRectangleLinesEx({ lx, ly, lw, (float)style.labelHeight }, 1.5f, style.labelBorder);
        DrawText(e->name.c_str(), (int)lx + style.labelPadding,
                 (int)ly + style.labelHeight / 2 - style.labelFontSize / 2,
                 style.labelFontSize, style.labelTextColor);
    }

    DrawRichText(m_engine.GetParsedContent(), m_engine.GetRevealedCount(),
                 (int)(box.x + style.boxPadding), (int)(box.y + 40),
                 style.textFontSize, (int)bw - style.boxPadding * 2, style.textColor, style.textLineSpacing);
                 
    // Waiting for input animation (Completion Cursor)
    if (!m_engine.IsTextRevealing()) {
        float anim = sin(GetTime() * 10.0f) * 3.0f;
        Color arrowCol = Fade(style.boxBorder, 0.8f);
        DrawTriangle({ box.x + box.width - 30, box.y + box.height - 20 + anim },
                     { box.x + box.width - 20, box.y + box.height - 30 + anim },
                     { box.x + box.width - 40, box.y + box.height - 30 + anim }, arrowCol);
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

    DrawRectangleRounded(r, style.choiceRoundness, 10, Fade(style.choiceBg, 0.95f));
    DrawRectangleRoundedLinesEx(r, style.choiceRoundness, 10, style.choiceBorderThick, style.choiceBorder);

    for (int i = 0; i < (int)opts.size(); ++i) {
        Rectangle oRect = { r.x + 20, r.y + pad + (i * itemH), r.width - 40, (float)style.optionHeight };
        Color col = (opts[i].style == "premium") ? style.optionPremium : style.optionColor;
        if (CheckCollisionPointRec(GetMousePosition(), oRect)) {
            DrawRectangleRec(oRect, Fade(col, 0.2f));
            DrawRectangleLinesEx(oRect, 1.0f, col);
            col = style.optionHover;
        }
        DrawText(opts[i].content.c_str(), (int)oRect.x + 40,
                 (int)oRect.y + style.optionHeight / 2 - style.optionFontSize / 2,
                 style.optionFontSize, col);
        DrawCircle((int)oRect.x + 20, (int)oRect.y + style.optionHeight / 2, 4, col);
    }
}

void BitRenderer::DrawVFX() {
    auto& style = m_styleManager.GetStyle();
    if (m_engine.GetConfigs().enable_vignette)
        DrawTexturePro(m_vignette, {0,0,64,64},
                       {0,0,(float)GetScreenWidth(),(float)GetScreenHeight()},
                       {0,0}, 0, Fade(WHITE, style.vignetteOpacity));
}

// ============================================================
// Debug overlay — intentionally NOT driven by style
// ============================================================
void BitRenderer::DrawDebugOverlay() {
    const auto* node   = m_engine.GetCurrentNode();
    const auto& vars   = m_engine.GetAllVariables();
    const auto& trace  = m_engine.GetEventTrace();
    const auto& errors = m_engine.GetErrors();
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    
    int gap = 12, pad = 10;
    int cols = 4;
    if (sw < 1100) cols = 2;
    if (sw < 600)  cols = 1;
    
    int rows = (4 + cols - 1) / cols;
    int panelW = (sw - 40 - (cols - 1) * gap) / cols;
    if (panelW > 280) panelW = 280;
    
    // Scale height based on rows available
    int panelH = (sh - 60 - (rows - 1) * (gap + 20)) / rows;
    if (panelH > 380) panelH = 380;
    if (panelH < 150) panelH = 150;

    int totalW = cols * panelW + (cols - 1) * gap;
    int startX = sw - totalW - 20;
    int startY = 20;

    int scrollOffset = (int)m_debugScroll;

    // Draw one unified background for the entire suite to avoid overlapping transparent blacks
    int fullH = rows * panelH + (rows - 1) * (gap + 15);
    DrawRectangle(startX - pad, startY - pad, totalW + pad * 2, fullH + pad * 2, Fade(BLACK, 0.85f));

    auto GetPanelRect = [&](int idx) -> Rectangle {
        int r = idx / cols;
        int c = idx % cols;
        return { (float)(startX + c * (panelW + gap)), (float)(startY + r * (panelH + gap + 15)), (float)panelW, (float)panelH };
    };

    // Panel 1: Node Inspector
    Rectangle r1 = GetPanelRect(0);
    int px = r1.x, py = r1.y;
    DrawText("NODE INSPECTOR", px, py, 13, SKYBLUE);
    int dy = py + 22;
    if (node) {
        DrawText(TextFormat("ID:     %s", m_engine.GetCurrentNodeId().c_str()), px, dy, 10, RAYWHITE); dy += 14;
        DrawText(TextFormat("Entity: %s", node->entity.c_str()),                px, dy, 10, RAYWHITE); dy += 14;
        DrawText(TextFormat("Next:   %s", node->next_id.value_or("(none)").c_str()), px, dy, 10, RAYWHITE); dy += 14;
        DrawText("Metadata:", px, dy, 10, YELLOW); dy += 13;
        for (auto const& [k, v] : node->metadata) {
            if (dy < r1.y + r1.height) {
                DrawText(TextFormat("  %s: %s", k.c_str(), v.c_str()), px, dy, 9, Fade(RAYWHITE, 0.8f));
                dy += 12;
            }
        }
    } else { DrawText("(no active node)", px, dy, 10, Fade(RAYWHITE, 0.5f)); }

    // Panel 2: Variable Watcher
    Rectangle r2 = GetPanelRect(1);
    px = r2.x; py = r2.y; dy = py;
    DrawText("VARIABLE WATCHER", px, dy, 13, GREEN); dy += 20;
    for (auto const& [name, val] : vars) {
        if (dy - py + scrollOffset < r2.height - 10 && dy - py + scrollOffset > 0)
            DrawText(TextFormat("%-16s %d", name.c_str(), val), px, dy, 10, LIME);
        dy += 13;
    }

    // Panel 3: Event Trace
    Rectangle r3 = GetPanelRect(2);
    px = r3.x; py = r3.y; dy = py;
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

    // Panel 4: Asset Monitor
    Rectangle r4 = GetPanelRect(3);
    px = r4.x; py = r4.y; dy = py;
    DrawText("ASSET MONITOR", px, dy, 13, PURPLE); dy += 20;
    
    DrawText("Audio Engine:", px, dy, 11, LIGHTGRAY); dy += 15;
    DrawText(TextFormat(" BGM: %s", m_currentMusicPath.empty() ? "(none)" : GetFileName(m_currentMusicPath.c_str())), px, dy, 10, m_isMusicPlaying ? GOLD : GRAY); dy += 13;
    DrawText(TextFormat(" State: %s", m_isMusicPlaying ? "PLAYING" : "STOPPED"), px, dy, 10, m_isMusicPlaying ? LIME : RED); dy += 18;

    DrawText("Cache Stats:", px, dy, 11, LIGHTGRAY); dy += 15;
    DrawText(TextFormat(" Text: %d  Mus: %d  SFX: %d", (int)m_textureCache.size(), (int)m_musicCache.size(), (int)m_sfxCache.size()), px, dy, 9, RAYWHITE); dy += 20;

    DrawText("Load Failures:", px, dy, 11, RED); dy += 15;
    int errCount = 0;
    for (auto const& [path, mus] : m_musicCache) if (mus.frameCount == 0) {
        if (dy < r4.y + r4.height - 10) DrawText(TextFormat(" ! [MUS] %s", GetFileName(path.c_str())), px, dy, 9, PINK);
        dy += 11; errCount++; if (errCount > 5) break;
    }
    for (auto const& [path, tex] : m_textureCache) if (tex.id == 0) {
        if (dy < r4.y + r4.height - 10) DrawText(TextFormat(" ! [TEX] %s", GetFileName(path.c_str())), px, dy, 9, RED);
        dy += 11; errCount++; if (errCount > 5) break;
    }

    if (errCount == 0) DrawText("(none)", px, dy, 10, Fade(RAYWHITE, 0.4f)); 

    // Error log stays at far bottom
    if (!errors.empty()) {
        DrawRectangle(0, sh - 26, sw, 26, Fade(RED, 0.75f));
        DrawText(errors.back().c_str(), 10, sh - 20, 11, WHITE);
        DrawText(TextFormat("(%d total - check debug.log)", (int)errors.size()), sw - 290, sh - 20, 11, Fade(WHITE, 0.8f));
    }
}

// ============================================================
void BitRenderer::DrawRichText(const std::vector<RichChar>& content, int limit, int x, int y, int fontSize, int maxWidth, Color defaultColor, int lineSpacing) {
    int curX = x;
    int curY = y;
    float time = (float)GetTime();
    float spacing = 2.0f; // Default Raylib letter spacing
    int lineOffset = fontSize + lineSpacing;

    float spaceWidth = MeasureTextEx(GetFontDefault(), " ", (float)fontSize, spacing).x;

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
            wordWidth += MeasureTextEx(GetFontDefault(), content[wordEnd].ch, (float)fontSize, spacing).x + spacing;
            wordEnd++;
        }

        if (curX + wordWidth > x + maxWidth && curX > x) {
            curX = x;
            curY += lineOffset;
        }

        for (int j = i; j < wordEnd; j++) {
            const auto& rc = content[j];
            Color c = (rc.color.a == 0 && rc.color.r == 0 && rc.color.g == 0 && rc.color.b == 0) 
                      ? defaultColor 
                      : Color{ rc.color.r, rc.color.g, rc.color.b, rc.color.a };
            
            float ox = 0.0f;
            float oy = 0.0f;

            if (rc.shake) {
                ox = GetRandomValue(-2, 2);
                oy = GetRandomValue(-2, 2);
            }
            if (rc.wave) {
                oy += sin(time * 6.0f + curX * 0.05f) * 4.0f;
            }

            Vector2 pos = { (float)curX + ox, (float)curY + oy };
            DrawTextEx(GetFontDefault(), rc.ch, pos, (float)fontSize, spacing, c);
            curX += MeasureTextEx(GetFontDefault(), rc.ch, (float)fontSize, spacing).x + spacing;
        }
        
        i = wordEnd;
    }
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
        m_textureCache[path] = {0}; // Failed
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
}

void BitRenderer::PlaySFX(const std::string& path) {
    if (!FileExists(path.c_str())) return;
    if (!m_sfxCache.count(path)) m_sfxCache[path] = LoadSound(path.c_str());
    PlaySound(m_sfxCache[path]);
}

void BitRenderer::CreateFallbackTexture() {
    Image img = GenImageChecked(64, 64, 32, 32, MAGENTA, BLACK);
    m_fallbackTexture = LoadTextureFromImage(img); UnloadImage(img);
}

void BitRenderer::CreateVignetteTexture() {
    Image img = GenImageGradientRadial(64, 64, 0.0f, BLANK, BLACK);
    m_vignette = LoadTextureFromImage(img); UnloadImage(img);
}
