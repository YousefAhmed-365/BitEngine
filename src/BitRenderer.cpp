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
    if (m_isMusicPlaying) UnloadMusicStream(m_currentMusic);
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
    EndMode2D();

    DrawVFX(); // Vignette locked to screen above background

    BeginMode2D(cam);
        DrawEntitySprite();
        DrawMainBox();
        if (!m_engine.IsTextRevealing()) DrawChoiceBox();
    EndMode2D();
    std::string dmode = m_engine.GetDebugMode();
    if (dmode == "debug_overlay" || dmode == "debug_all") DrawDebugOverlay();

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
            if (m_isMusicPlaying) UnloadMusicStream(m_currentMusic);
            m_currentMusic = LoadMusicStream(path.c_str());
            if (m_currentMusic.frameCount > 0) {
                PlayMusicStream(m_currentMusic);
                m_currentMusicPath = path;
                m_isMusicPlaying = true;
            }
        }
    }
    if (m_isMusicPlaying) UpdateMusicStream(m_currentMusic);

    if (node->metadata.count("sfx")) {
        static std::string lastSFX = "";
        std::string req  = node->metadata.at("sfx");
        std::string path = m_engine.GetSFX(req);
        if (path.empty()) path = req;
        if (lastSFX != path) { PlaySFX(path); lastSFX = path; }
    }
}

void BitRenderer::HandleInput() {
    if (!m_engine.IsActive()) return;
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
                 style.textFontSize, (int)bw - style.boxPadding * 2, style.textColor);
                 
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
    int panelW = 240, panelH = 400, pad = 8, gap = 8;
    int totalW = panelW * 3 + gap * 2;
    int startX = sw - totalW - 10, startY = 10;

    DrawRectangle(startX - pad, startY - pad, totalW + pad * 2, panelH + pad * 2, Fade(BLACK, 0.82f));

    // Panel 1: Node Inspector
    int px = startX;
    DrawText("NODE INSPECTOR", px, startY, 13, SKYBLUE);
    DrawLine(px, startY + 17, px + panelW, startY + 17, Fade(SKYBLUE, 0.5f));
    int dy = startY + 24;
    if (node) {
        DrawText(TextFormat("ID:     %s", m_engine.GetCurrentNodeId().c_str()), px, dy, 11, RAYWHITE); dy += 15;
        DrawText(TextFormat("Entity: %s", node->entity.c_str()),                px, dy, 11, RAYWHITE); dy += 15;
        DrawText(TextFormat("Next:   %s", node->next_id.value_or("(none)").c_str()), px, dy, 11, RAYWHITE); dy += 15;
        DrawText(TextFormat("Opts:   %d", (int)node->options.size()),            px, dy, 11, RAYWHITE); dy += 18;
        DrawText("Metadata:", px, dy, 11, YELLOW); dy += 14;
        for (auto const& [k, v] : node->metadata) {
            DrawText(TextFormat("  %s: %s", k.c_str(), v.c_str()), px, dy, 10, Fade(RAYWHITE, 0.8f));
            dy += 13; if (dy > startY + panelH - 10) break;
        }
    } else { DrawText("(no active node)", px, dy, 11, Fade(RAYWHITE, 0.5f)); }

    // Panel 2: Variable Watcher
    px = startX + panelW + gap; dy = startY;
    DrawText("VARIABLE WATCHER", px, dy, 13, GREEN); dy += 18;
    DrawLine(px, dy - 1, px + panelW, dy - 1, Fade(GREEN, 0.5f));
    for (auto const& [name, val] : vars) {
        DrawText(TextFormat("%-18s %d", name.c_str(), val), px, dy, 11, LIME);
        dy += 14; if (dy > startY + panelH - 10) break;
    }

    // Panel 3: Event Trace
    px = startX + (panelW + gap) * 2; dy = startY;
    DrawText("EVENT TRACE", px, dy, 13, ORANGE); dy += 18;
    DrawLine(px, dy - 1, px + panelW, dy - 1, Fade(ORANGE, 0.5f));
    int maxTrace = 16, traceStart = (int)trace.size() > maxTrace ? (int)trace.size() - maxTrace : 0;
    for (int i = traceStart; i < (int)trace.size(); ++i) {
        const auto& t = trace[i];
        std::string arrow = std::to_string(t.old_value) + " -> " + std::to_string(t.new_value);
        DrawText(TextFormat("%s %s", t.op.c_str(), t.var.c_str()), px, dy, 10, ORANGE); dy += 12;
        DrawText(TextFormat("  %s [%s]", arrow.c_str(), t.node_id.c_str()), px, dy, 9, Fade(ORANGE, 0.7f)); dy += 13;
        if (dy > startY + panelH - 10) break;
    }
    if (trace.empty()) DrawText("(no events yet)", px, startY + 24, 11, Fade(RAYWHITE, 0.5f));

    // Errors bar
    if (!errors.empty()) {
        DrawRectangle(0, sh - 26, sw, 26, Fade(RED, 0.75f));
        DrawText(errors.back().c_str(), 10, sh - 20, 11, WHITE);
        DrawText(TextFormat("(%d total - check debug.log)", (int)errors.size()), sw - 290, sh - 20, 11, Fade(WHITE, 0.8f));
    }
}

// ============================================================
void BitRenderer::DrawRichText(const std::vector<RichChar>& content, int limit, int x, int y, int fontSize, int maxWidth, Color defaultColor) {
    int curX = x;
    int curY = y;
    float time = (float)GetTime();
    float spacing = 2.0f; // Default Raylib letter spacing

    int i = 0;
    while (i < limit && i < (int)content.size()) {
        if (content[i].ch == " " || content[i].ch == "\n") {
            if (content[i].ch == "\n") {
                curX = x;
                curY += fontSize + 5;
            } else {
                curX += MeasureTextEx(GetFontDefault(), " ", (float)fontSize, spacing).x + spacing;
            }
            i++;
            continue;
        }

        int wordEnd = i;
        std::string wordRaw = "";
        while (wordEnd < limit && wordEnd < (int)content.size() && content[wordEnd].ch != " " && content[wordEnd].ch != "\n") {
            wordRaw += content[wordEnd].ch;
            wordEnd++;
        }
        float wordWidth = MeasureTextEx(GetFontDefault(), wordRaw.c_str(), (float)fontSize, spacing).x;

        if (curX + wordWidth > x + maxWidth && curX > x) {
            curX = x;
            curY += fontSize + 5;
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
            DrawTextEx(GetFontDefault(), rc.ch.c_str(), pos, (float)fontSize, spacing, c);
            curX += MeasureTextEx(GetFontDefault(), rc.ch.c_str(), (float)fontSize, spacing).x + spacing;
        }
        
        i = wordEnd;
    }
}

Texture2D BitRenderer::GetTexture(const std::string& path) {
    if (path.empty()) return m_fallbackTexture;
    if (m_textureCache.count(path)) return m_textureCache[path];
    if (FileExists(path.c_str())) {
        Texture2D tex = LoadTexture(path.c_str());
        if (tex.id > 0) { m_textureCache[path] = tex; return tex; }
    }
    return m_fallbackTexture;
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
