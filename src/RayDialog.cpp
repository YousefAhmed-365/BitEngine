#include "RayDialog.hpp"
#include <vector>
#include <string>
#include <sstream>
#include <iostream>

// UI Styling Constants
namespace Style {
    const Color BG      = { 20, 20, 30, 230 }; 
    const Color BORDER  = { 80, 80, 120, 255 }; 
    const Color LABEL   = { 255, 215, 0, 255 }; 
    const Color TEXT    = { 240, 240, 255, 255 }; 
    const Color OPTION  = { 100, 200, 255, 255 }; 
    const Color PREMIUM = { 255, 100, 255, 255 }; 
    const Color HOVER   = { 255, 100, 150, 255 }; 
    const int FONT_MAIN = 24;
    const int FONT_NAME = 28;
    const int FONT_OPT  = 20;
    const int PADDING   = 40;
}

static float saveToastTimer = 0.0f;
static std::string saveToastMsg = "";

RayDialog::RayDialog(DialogEngine& engine) : m_engine(engine) {
    CreateFallbackTexture();
}

RayDialog::~RayDialog() {
    for (auto& [path, tex] : m_textureCache) {
        if (tex.id > 0) UnloadTexture(tex);
    }
    if (m_fallbackTexture.id > 0) UnloadTexture(m_fallbackTexture);
}

void RayDialog::Draw() {
    if (!m_engine.IsActive()) return;
    DrawEntitySprite();
    DrawMainBox();
    if (!m_engine.IsTextRevealing()) DrawChoiceBox();
    if (m_engine.IsDebug()) DrawDebugOverlay();
    if (saveToastTimer > 0.0f) {
        DrawRectangle(GetScreenWidth() - 220, 20, 200, 40, Style::BG);
        DrawRectangleLines(GetScreenWidth() - 220, 20, 200, 40, Style::BORDER);
        DrawText(saveToastMsg.c_str(), GetScreenWidth() - 200, 32, 16, Style::TEXT);
        saveToastTimer -= GetFrameTime();
    }
}

void RayDialog::HandleInput() {
    if (!m_engine.IsActive()) return;
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) { m_engine.Next(); return; }
    if (IsKeyPressed(KEY_F5)) { m_engine.SaveGame(1); saveToastMsg = "QUICK SAVE..."; saveToastTimer = 2.0f; }
    if (IsKeyPressed(KEY_F9)) { if (m_engine.LoadGame(1)) { saveToastMsg = "RELOADING..."; saveToastTimer = 2.0f; } }
    if (!m_engine.IsTextRevealing()) {
        auto& options = m_engine.GetVisibleOptions();
        for (int i = 0; i < (int)options.size(); ++i) {
            if (IsKeyPressed(KEY_ONE + i)) { m_engine.SelectOption(i); return; }
            Rectangle r = { (float)GetScreenWidth() - 440, (float)GetScreenHeight() - 260 - ((float)options.size() * 40), 380, 30 };
            r.y += 45 + (i * 35);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), r)) { m_engine.SelectOption(i); return; }
        }
    }
}

void RayDialog::DrawEntitySprite() {
    const auto* entity = m_engine.GetCurrentEntity();
    const auto* node = m_engine.GetCurrentNode();
    if (!entity || !node) return;

    std::string expression = node->metadata.count("expression") ? node->metadata.at("expression") : "idle";
    
    // Default values if no sprite is found
    std::string path = "";
    int frames = 1;
    float speed = 1.0f;
    float scale = 3.0f; // Default scale for entities without specific sprite config

    bool found = false;
    if (entity->sprites.count(expression)) {
        const auto& s = entity->sprites.at(expression);
        path = s.path; frames = s.frames; speed = s.speed; scale = s.scale;
        found = true;
    } else if (entity->sprites.count("idle")) {
        const auto& s = entity->sprites.at("idle");
        path = s.path; frames = s.frames; speed = s.speed; scale = s.scale;
        found = true;
    }

    Texture2D tex = GetTexture(path);
    if (tex.id == 0) tex = m_fallbackTexture; // Final safety

    // Animation Logic
    if (m_lastSpritePath != path) {
        m_lastSpritePath = path;
        m_animFrame = 0;
        m_animTimer = 0.0f;
    }
    
    int finalFrames = (frames > 0) ? frames : 1;
    m_animTimer += GetFrameTime();
    if (m_animTimer >= 1.0f / (speed > 0 ? speed : 1.0f)) {
        m_animTimer = 0.0f;
        m_animFrame = (m_animFrame + 1) % finalFrames;
    }
    
    int frameWidth = tex.width / finalFrames;
    Rectangle srcRec = { (float)(m_animFrame * frameWidth), 0.0f, (float)frameWidth, (float)tex.height };
    
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    Vector2 pos = { (float)sw/2.0f - (frameWidth * scale)/2.0f, (float)sh - 240 - (tex.height * scale) - 20 };
    
    DrawTexturePro(tex, srcRec, { pos.x, pos.y, frameWidth * scale, tex.height * scale }, {0,0}, 0, WHITE);
}

void RayDialog::DrawMainBox() {
    auto* n = m_engine.GetCurrentNode();
    auto* e = m_engine.GetCurrentEntity();
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Rectangle box = { 50, (float)sh - 240, (float)sw - 100, 200 };
    DrawRectangleRounded(box, 0.1f, 10, Style::BG);
    DrawRectangleRoundedLinesEx(box, 0.1f, 10, 2.0f, Style::BORDER);
    DrawRectangleGradientH(box.x + 2, box.y + 2, box.width - 4, 3, Style::BORDER, Fade(Style::BORDER, 0));
    if (e) DrawText(e->name.c_str(), box.x + Style::PADDING, box.y + 30, Style::FONT_NAME, Style::LABEL);
    DrawTextWrapped(m_engine.GetVisibleContent(), box.x + Style::PADDING, box.y + 75, Style::FONT_MAIN, (int)box.width - 80, Style::TEXT);
    DrawText("[F5] SAVE | [F9] LOAD", box.x + box.width - 200, box.y + box.height - 35, 14, Fade(Style::BORDER, 0.6f));
    if (m_engine.IsTextRevealing()) DrawText("Typing...", box.x + box.width - 120, box.y + 15, 14, Fade(Style::BORDER, 0.8f));
    else if (m_engine.GetVisibleOptions().empty()) DrawText("[ SPACE TO CONTINUE ]", box.x + Style::PADDING, box.y + box.height - 35, 16, Fade(Style::TEXT, 0.5f));
}

void RayDialog::DrawChoiceBox() {
    auto& opts = m_engine.GetVisibleOptions();
    if (opts.empty()) return;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int cw = 400, ch = (int)opts.size() * 40 + 40;
    Rectangle r = { (float)sw - cw - 50, (float)sh - 260 - ch, (float)cw, (float)ch };
    DrawRectangleRounded(r, 0.1f, 10, Style::BG);
    DrawRectangleRoundedLinesEx(r, 0.1f, 10, 2.0f, Style::BORDER);
    DrawText("CHOOSE ACTION:", r.x + 20, r.y + 15, 18, Style::BORDER);
    for (int i = 0; i < (int)opts.size(); ++i) {
        Rectangle oRect = { r.x + 10, r.y + 45 + (i * 35), r.width - 20, 30 };
        Color col = (opts[i].style == "premium") ? Style::PREMIUM : Style::OPTION;
        if (CheckCollisionPointRec(GetMousePosition(), oRect)) { DrawRectangleRounded(oRect, 0.2f, 5, Fade(Style::HOVER, 0.15f)); col = Style::HOVER; }
        DrawText(opts[i].content.c_str(), oRect.x + 30, oRect.y + 5, Style::FONT_OPT, col);
        DrawText(TextFormat("%d.", i+1), oRect.x + 10, oRect.y + 5, Style::FONT_OPT, Fade(col, 0.5f));
    }
}

void RayDialog::DrawDebugOverlay() {
    int sw = GetScreenWidth();
    DrawRectangle(sw - 320, 60, 300, 230, Fade(BLACK, 0.6f));
    DrawRectangleLines(sw - 320, 60, 300, 230, RED);
    DrawText("DEBUG SYSTEMS", sw - 300, 70, 18, RED);
    int dy = 100;
    for (auto const& [name, val] : m_engine.GetAllVariables()) { DrawText(TextFormat("> %s: %d", name.c_str(), val), sw - 280, dy, 14, GREEN); dy += 18; }
    dy += 10;
    DrawText("SAVE SLOTS:", sw - 280, dy, 14, Style::BORDER); dy += 18;
    for (int i = 0; i < 3; ++i) { bool has = m_engine.HasSave(i); DrawText(TextFormat("[%d] %s", i, has ? "OCCUPIED" : "EMPTY"), sw - 280, dy, 12, has ? GREEN : GRAY); dy += 14; }
}

void RayDialog::DrawTextWrapped(const std::string& text, int x, int y, int fontSize, int maxWidth, Color color) {
    std::string s(text), l = "", w; std::stringstream ss(s);
    int curY = y;
    while (ss >> w) {
        std::string t = l + (l.empty() ? "" : " ") + w;
        if (MeasureTextEx(GetFontDefault(), t.c_str(), (float)fontSize, 2).x > maxWidth && !l.empty()) { DrawTextEx(GetFontDefault(), l.c_str(), { (float)x, (float)curY }, (float)fontSize, 2, color); l = w; curY += fontSize + 5; }
        else l = t;
    }
    if (!l.empty()) DrawTextEx(GetFontDefault(), l.c_str(), { (float)x, (float)curY }, (float)fontSize, 2, color);
}

Texture2D RayDialog::GetTexture(const std::string& path) {
    if (path.empty()) return m_fallbackTexture;
    if (m_textureCache.count(path)) return m_textureCache[path];
    if (FileExists(path.c_str())) {
        Texture2D tex = LoadTexture(path.c_str());
        if (tex.id > 0) { m_textureCache[path] = tex; return tex; }
    }
    return m_fallbackTexture;
}

void RayDialog::CreateFallbackTexture() {
    Image img = GenImageChecked(64, 64, 32, 32, MAGENTA, BLACK);
    m_fallbackTexture = LoadTextureFromImage(img);
    UnloadImage(img);
}
