#include "BitDialog.hpp"
#include "json.hpp"
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <cmath>
#include <fstream>

using json = nlohmann::json;

static float saveToastTimer = 0.0f;
static std::string saveToastMsg = "";

// Helper: parse a [r,g,b,a] JSON array into a Raylib Color
static Color ParseColor(const json& j, const Color& def) {
    if (j.is_array() && j.size() == 4)
        return { j[0].get<unsigned char>(), j[1].get<unsigned char>(), j[2].get<unsigned char>(), j[3].get<unsigned char>() };
    return def;
}

// Parse a single style block from JSON into a BitDialogStyle (using defaults as fallback)
static BitDialogStyle ParseStyleBlock(const json& j) {
    BitDialogStyle s; // starts with all defaults

    if (j.contains("dialog_box")) {
        auto& b = j["dialog_box"];
        s.boxNormX        = b.value("pos_x",         s.boxNormX);
        s.boxNormY        = b.value("pos_y",         s.boxNormY);
        s.boxWidthNorm    = b.value("width_norm",    s.boxWidthNorm);
        s.boxHeight       = b.value("height",        s.boxHeight);
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

// ============================================================
BitDialog::BitDialog(DialogEngine& engine) : m_engine(engine) {
    InitAudioDevice();
    CreateFallbackTexture();
    CreateVignetteTexture();
}

BitDialog::~BitDialog() {
    for (auto& [path, tex] : m_textureCache) UnloadTexture(tex);
    for (auto& [path, sfx] : m_sfxCache) UnloadSound(sfx);
    if (m_isMusicPlaying) UnloadMusicStream(m_currentMusic);
    UnloadTexture(m_fallbackTexture);
    UnloadTexture(m_vignette);
    CloseAudioDevice();
}

// ============================================================
// LoadStyle — loads all named style blocks; enables hot reload
// ============================================================
void BitDialog::LoadStyle(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "[BitDialog] style.json not found: " << path << " — using defaults." << std::endl;
        return;
    }

    // Remember for hot reload
    m_stylePath = path;
    m_styleLastModTime = GetFileModTime(path.c_str());

    try {
        json j; f >> j;

        // Clear library but keep current style name so we can re-apply after reload
        std::string prevName = m_currentStyleName;
        m_styleLibrary.clear();
        m_styleOrder.clear();

        if (j.contains("styles") && j["styles"].is_object()) {
            // Multi-style file format
            for (auto& [name, block] : j["styles"].items()) {
                m_styleLibrary[name] = ParseStyleBlock(block);
                m_styleOrder.push_back(name);
            }
        } else {
            // Legacy single-style fallback — stored as "__default__"
            m_styleLibrary["__default__"] = ParseStyleBlock(j);
            m_styleOrder.push_back("__default__");
        }

        // Re-apply previously active style, or first style if new/changed
        if (!prevName.empty() && m_styleLibrary.count(prevName)) {
            m_style = m_styleLibrary[prevName];
            m_currentStyleName = prevName;
        } else if (!m_styleOrder.empty()) {
            m_currentStyleName = m_styleOrder[0];
            m_style = m_styleLibrary[m_currentStyleName];
        }

    } catch (const std::exception& ex) {
        std::cerr << "[BitDialog] Failed to parse style.json: " << ex.what() << std::endl;
    }
}

void BitDialog::SetStyle(const std::string& name) {
    if (!m_styleLibrary.count(name)) {
        std::cerr << "[BitDialog] Style '" << name << "' not found." << std::endl;
        return;
    }
    m_currentStyleName = name;
    m_style = m_styleLibrary[name];
}

void BitDialog::NextStyle() {
    if (m_styleOrder.empty()) return;
    auto it = std::find(m_styleOrder.begin(), m_styleOrder.end(), m_currentStyleName);
    if (it == m_styleOrder.end() || ++it == m_styleOrder.end()) it = m_styleOrder.begin();
    SetStyle(*it);
}

void BitDialog::PrevStyle() {
    if (m_styleOrder.empty()) return;
    auto it = std::find(m_styleOrder.begin(), m_styleOrder.end(), m_currentStyleName);
    if (it == m_styleOrder.begin()) it = m_styleOrder.end();
    SetStyle(*--it);
}

std::vector<std::string> BitDialog::GetStyleNames() const {
    return m_styleOrder;
}

// ============================================================
void BitDialog::Draw() {
    if (!m_engine.IsActive()) return;

    // Hot reload check every 0.5 seconds
    m_hotReloadTimer += GetFrameTime();
    if (m_hotReloadTimer >= 0.5f) {
        m_hotReloadTimer = 0.0f;
        if (!m_stylePath.empty()) {
            long newMod = GetFileModTime(m_stylePath.c_str());
            if (newMod != m_styleLastModTime) {
                std::cout << "[BitDialog] style.json changed, hot reloading..." << std::endl;
                LoadStyle(m_stylePath); // Re-applies current style automatically
            }
        }
    }

    HandleAudio();

    float intensity = m_engine.GetEffectShake();
    Vector2 shake = { (float)GetRandomValue(-(int)intensity, (int)intensity),
                      (float)GetRandomValue(-(int)intensity, (int)intensity) };

    BeginMode2D((Camera2D){ {0,0}, {shake.x, shake.y}, 0.0f, 1.0f });
        DrawBackground();
        DrawEntitySprite();
        DrawMainBox();
        if (!m_engine.IsTextRevealing()) DrawChoiceBox();
    EndMode2D();

    DrawVFX();
    std::string dmode = m_engine.GetDebugMode();
    if (dmode == "debug_overlay" || dmode == "debug_all") DrawDebugOverlay();

    // Toast notification
    if (saveToastTimer > 0.0f) {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        float tx = sw * m_style.toastNormX - m_style.toastWidth - m_style.toastMarginX;
        float ty = sh * m_style.toastNormY + m_style.toastMarginY;
        DrawRectangle((int)tx, (int)ty, (int)m_style.toastWidth, (int)m_style.toastHeight, m_style.toastBg);
        DrawRectangleLinesEx({ tx, ty, m_style.toastWidth, m_style.toastHeight }, 1.5f, m_style.toastBorder);
        DrawText(saveToastMsg.c_str(), (int)tx + 10,
                 (int)ty + (int)(m_style.toastHeight / 2) - m_style.toastFontSize / 2,
                 m_style.toastFontSize, m_style.toastTextColor);
        saveToastTimer -= GetFrameTime();
    }
}

void BitDialog::HandleAudio() {
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

void BitDialog::HandleInput() {
    if (!m_engine.IsActive()) return;
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) { m_engine.Next(); return; }
    if (IsKeyPressed(KEY_F5)) { m_engine.SaveGame(1); saveToastMsg = "QUICK SAVE..."; saveToastTimer = 2.0f; }
    if (IsKeyPressed(KEY_F9)) { if (m_engine.LoadGame(1)) { saveToastMsg = "RELOADING..."; saveToastTimer = 2.0f; } }

    if (!m_engine.IsTextRevealing()) {
        auto& opts = m_engine.GetVisibleOptions();
        for (int i = 0; i < (int)opts.size(); ++i) {
            if (IsKeyPressed(KEY_ONE + i)) { m_engine.SelectOption(i); return; }
            int sw = GetScreenWidth(), sh = GetScreenHeight();
            float cw  = m_style.choiceWidth;
            float itemH = m_style.optionHeight + m_style.optionGap;
            float ch  = opts.size() * itemH + 50;
            Rectangle r = { sw * m_style.choiceNormX - cw / 2.0f,
                             sh * m_style.choiceNormY - ch / 2.0f + m_style.choiceOffsetY,
                             cw, ch };
            Rectangle oRect = { r.x + 20, r.y + 40 + (i * itemH), r.width - 40, (float)m_style.optionHeight };
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), oRect)) {
                m_engine.SelectOption(i); return;
            }
        }
    }
}

// ============================================================
void BitDialog::DrawBackground() {
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

void BitDialog::DrawEntitySprite() {
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

void BitDialog::DrawMainBox() {
    auto* e  = m_engine.GetCurrentEntity();
    int sw   = GetScreenWidth(), sh = GetScreenHeight();
    float bw = sw * m_style.boxWidthNorm;
    float bx = sw * m_style.boxNormX - bw / 2.0f;
    float by = sh - m_style.boxHeight - m_style.boxMarginBottom;
    Rectangle box = { bx, by, bw, m_style.boxHeight };

    DrawRectangleRounded(box, m_style.boxRoundness, 10, m_style.boxBg);
    DrawRectangleRoundedLinesEx(box, m_style.boxRoundness, 10, m_style.boxBorderThick, m_style.boxBorder);

    if (e) {
        float tw = MeasureTextEx(GetFontDefault(), e->name.c_str(), (float)m_style.labelFontSize, 2).x;
        float lx = box.x + m_style.labelOffsetX;
        float ly = box.y + m_style.labelOffsetY;
        float lw = tw + m_style.labelPadding * 2;
        DrawRectangle((int)lx, (int)ly, (int)lw, m_style.labelHeight, m_style.labelBg);
        DrawRectangleLinesEx({ lx, ly, lw, (float)m_style.labelHeight }, 1.5f, m_style.labelBorder);
        DrawText(e->name.c_str(), (int)lx + m_style.labelPadding,
                 (int)ly + m_style.labelHeight / 2 - m_style.labelFontSize / 2,
                 m_style.labelFontSize, m_style.labelTextColor);
    }

    DrawTextWrapped(m_engine.GetVisibleContent(),
                    (int)(box.x + m_style.boxPadding), (int)(box.y + 40),
                    m_style.textFontSize, (int)bw - m_style.boxPadding * 2, m_style.textColor);
}

void BitDialog::DrawChoiceBox() {
    auto& opts = m_engine.GetVisibleOptions();
    if (opts.empty()) return;

    int sw = GetScreenWidth(), sh = GetScreenHeight();
    float cw    = m_style.choiceWidth;
    float itemH = m_style.optionHeight + m_style.optionGap;
    float ch    = opts.size() * itemH + 50;
    float cx    = sw * m_style.choiceNormX - cw / 2.0f;
    float cy    = sh * m_style.choiceNormY - ch / 2.0f + m_style.choiceOffsetY;
    Rectangle r = { cx, cy, cw, ch };

    DrawRectangleRounded(r, m_style.choiceRoundness, 10, Fade(m_style.choiceBg, 0.95f));
    DrawRectangleRoundedLinesEx(r, m_style.choiceRoundness, 10, m_style.choiceBorderThick, m_style.choiceBorder);

    for (int i = 0; i < (int)opts.size(); ++i) {
        Rectangle oRect = { r.x + 20, r.y + 40 + (i * itemH), r.width - 40, (float)m_style.optionHeight };
        Color col = (opts[i].style == "premium") ? m_style.optionPremium : m_style.optionColor;
        if (CheckCollisionPointRec(GetMousePosition(), oRect)) {
            DrawRectangleRec(oRect, Fade(col, 0.2f));
            DrawRectangleLinesEx(oRect, 1.0f, col);
            col = m_style.optionHover;
        }
        DrawText(opts[i].content.c_str(), (int)oRect.x + 40,
                 (int)oRect.y + m_style.optionHeight / 2 - m_style.optionFontSize / 2,
                 m_style.optionFontSize, col);
        DrawCircle((int)oRect.x + 20, (int)oRect.y + m_style.optionHeight / 2, 4, col);
    }
}

void BitDialog::DrawVFX() {
    if (m_engine.GetConfigs().enable_vignette)
        DrawTexturePro(m_vignette, {0,0,64,64},
                       {0,0,(float)GetScreenWidth(),(float)GetScreenHeight()},
                       {0,0}, 0, Fade(WHITE, m_style.vignetteOpacity));
}

// ============================================================
// Debug overlay — intentionally NOT driven by style
// ============================================================
void BitDialog::DrawDebugOverlay() {
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
void BitDialog::DrawTextWrapped(const std::string& text, int x, int y, int fontSize, int maxWidth, Color color) {
    std::string s(text), l = "", w; std::stringstream ss(s); int curY = y;
    while (ss >> w) {
        std::string t = l + (l.empty() ? "" : " ") + w;
        if (MeasureTextEx(GetFontDefault(), t.c_str(), (float)fontSize, 2).x > maxWidth && !l.empty()) {
            DrawTextEx(GetFontDefault(), l.c_str(), {(float)x,(float)curY}, (float)fontSize, 2, color);
            l = w; curY += fontSize + 5;
        } else l = t;
    }
    if (!l.empty()) DrawTextEx(GetFontDefault(), l.c_str(), {(float)x,(float)curY}, (float)fontSize, 2, color);
}

Texture2D BitDialog::GetTexture(const std::string& path) {
    if (path.empty()) return m_fallbackTexture;
    if (m_textureCache.count(path)) return m_textureCache[path];
    if (FileExists(path.c_str())) {
        Texture2D tex = LoadTexture(path.c_str());
        if (tex.id > 0) { m_textureCache[path] = tex; return tex; }
    }
    return m_fallbackTexture;
}

void BitDialog::PlaySFX(const std::string& path) {
    if (!FileExists(path.c_str())) return;
    if (!m_sfxCache.count(path)) m_sfxCache[path] = LoadSound(path.c_str());
    PlaySound(m_sfxCache[path]);
}

void BitDialog::CreateFallbackTexture() {
    Image img = GenImageChecked(64, 64, 32, 32, MAGENTA, BLACK);
    m_fallbackTexture = LoadTextureFromImage(img); UnloadImage(img);
}

void BitDialog::CreateVignetteTexture() {
    Image img = GenImageGradientRadial(64, 64, 0.0f, BLANK, BLACK);
    m_vignette = LoadTextureFromImage(img); UnloadImage(img);
}
