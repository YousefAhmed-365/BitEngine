#include "BitDialog.hpp"
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <cmath>

static float saveToastTimer = 0.0f;
static std::string saveToastMsg = "";

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

void BitDialog::Draw() {
    if (!m_engine.IsActive()) return;

    HandleAudio();
    
    float intensity = m_engine.GetEffectShake();
    Vector2 shake = { (float)GetRandomValue(-intensity, intensity), (float)GetRandomValue(-intensity, intensity) };

    BeginMode2D((Camera2D){ {0,0}, {shake.x, shake.y}, 0.0f, 1.0f });
        DrawBackground();
        DrawEntitySprite();
        DrawMainBox();
        if (!m_engine.IsTextRevealing()) DrawChoiceBox();
    EndMode2D();

    DrawVFX();
    if (m_engine.IsDebug()) DrawDebugOverlay();
    
    if (saveToastTimer > 0.0f) {
        DrawRectangle(GetScreenWidth() - 220, 20, 200, 40, m_style.bg);
        DrawRectangleLines(GetScreenWidth() - 220, 20, 200, 40, m_style.border);
        DrawText(saveToastMsg.c_str(), GetScreenWidth() - 200, 32, 16, m_style.text);
        saveToastTimer -= GetFrameTime();
    }
}

void BitDialog::HandleAudio() {
    const auto* node = m_engine.GetCurrentNode();
    if (!node) return;

    if (node->metadata.count("bgm")) {
        std::string path = node->metadata.at("bgm");
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
        std::string path = node->metadata.at("sfx");
        if (lastSFX != path) { PlaySFX(path); lastSFX = path; }
    }
}

void BitDialog::HandleInput() {
    if (!m_engine.IsActive()) return;
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) { m_engine.Next(); return; }
    if (IsKeyPressed(KEY_F5)) { m_engine.SaveGame(1); saveToastMsg = "QUICK SAVE..."; saveToastTimer = 2.0f; }
    if (IsKeyPressed(KEY_F9)) { if (m_engine.LoadGame(1)) { saveToastMsg = "RELOADING..."; saveToastTimer = 2.0f; } }

    if (!m_engine.IsTextRevealing()) {
        auto& options = m_engine.GetVisibleOptions();
        for (int i = 0; i < (int)options.size(); ++i) {
            if (IsKeyPressed(KEY_ONE + i)) { m_engine.SelectOption(i); return; }
            Rectangle r = { (float)GetScreenWidth() - 440, (float)GetScreenHeight() - 260 - ((float)options.size() * 45), 440, 35 };
            r.y += 40 + (i * 45);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), r)) { m_engine.SelectOption(i); return; }
        }
    }
}

void BitDialog::DrawBackground() {
    const auto* node = m_engine.GetCurrentNode();
    if (node && node->metadata.count("bg")) {
        Texture2D bg = GetTexture(node->metadata.at("bg"));
        DrawTexturePro(bg, {0,0,(float)bg.width, (float)bg.height}, {0,0,(float)GetScreenWidth(), (float)GetScreenHeight()}, {0,0}, 0, WHITE);
    }
}

void BitDialog::DrawEntitySprite() {
    const auto* entity = m_engine.GetCurrentEntity();
    const auto* node = m_engine.GetCurrentNode();
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
    
    m_floatOffset = m_engine.GetConfigs().enable_floating ? sin(GetTime() * 2.0f) * 10.0f : 0.0f;
    int fw = tex.width / finalFrames;
    Rectangle src = { (float)(m_animFrame * fw), 0, (float)fw, (float)tex.height };
    Vector2 pos = { (float)GetScreenWidth()/2.0f - (fw * scale)/2.0f, (float)GetScreenHeight() - 240 - (tex.height * scale) - 20 + m_floatOffset };
    
    if (m_engine.GetConfigs().enable_shadows)
        DrawEllipse((int)pos.x + (fw*scale)/2, (int)pos.y + (tex.height*scale), (fw*scale)/3, 10, Fade(BLACK, 0.4f));
    
    DrawTexturePro(tex, src, { pos.x, pos.y, fw * scale, tex.height * scale }, {0,0}, 0, WHITE);
}

void BitDialog::DrawMainBox() {
    auto* e = m_engine.GetCurrentEntity();
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Rectangle box = { 40, (float)sh - 230, (float)sw - 80, 190 };

    DrawRectangleRounded(box, m_style.boxRoundness, 10, m_style.bg);
    DrawRectangleRoundedLinesEx(box, m_style.boxRoundness, 10, m_style.borderThickness, m_style.border);
    
    if (e) {
        float tw = MeasureTextEx(GetFontDefault(), e->name.c_str(), (float)m_style.fontName, 2).x;
        DrawRectangle(box.x + 30, box.y - 20, tw + 40, 40, m_style.bg);
        DrawRectangleLines(box.x + 30, box.y - 20, tw + 40, 40, m_style.border);
        DrawText(e->name.c_str(), box.x + 50, box.y - 12, m_style.fontName, m_style.label);
    }

    DrawTextWrapped(m_engine.GetVisibleContent(), box.x + m_style.padding, box.y + 40, m_style.fontMain, (int)box.width - 80, m_style.text);
}

void BitDialog::DrawChoiceBox() {
    auto& opts = m_engine.GetVisibleOptions();
    if (opts.empty()) return;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int cw = 440, ch = (int)opts.size() * 45 + 50;
    Rectangle r = { (float)sw/2.0f - cw/2.0f, (float)sh/2.0f - ch/2.0f - 100, (float)cw, (float)ch };
    
    DrawRectangleRounded(r, 0.1f, 10, Fade(m_style.bg, 0.95f));
    DrawRectangleRoundedLinesEx(r, 0.1f, 10, 2.0f, m_style.border);

    for (int i = 0; i < (int)opts.size(); ++i) {
        Rectangle oRect = { r.x + 20, r.y + 40 + (i * 45), r.width - 40, 35 };
        Color col = (opts[i].style == "premium") ? m_style.premium : m_style.option;
        if (CheckCollisionPointRec(GetMousePosition(), oRect)) {
            DrawRectangleRec(oRect, Fade(col, 0.2f));
            DrawRectangleLinesEx(oRect, 1.0f, col);
            col = m_style.hover;
        }
        DrawText(opts[i].content.c_str(), oRect.x + 40, oRect.y + 8, m_style.fontOpt, col);
        DrawCircle((int)oRect.x + 20, (int)oRect.y + 17, 4, col);
    }
}

void BitDialog::DrawVFX() {
    if (m_engine.GetConfigs().enable_vignette)
        DrawTexturePro(m_vignette, {0,0,64,64}, {0,0,(float)GetScreenWidth(), (float)GetScreenHeight()}, {0,0}, 0, Fade(BLACK, 0.4f));
}

void BitDialog::DrawDebugOverlay() {
    int sw = GetScreenWidth();
    DrawRectangle(sw - 260, 20, 240, 200, Fade(BLACK, 0.7f));
    DrawText("BITDIALOG ANALYTICS", sw - 240, 35, 16, m_style.border);
    int dy = 65;
    for (auto const& [name, val] : m_engine.GetAllVariables()) { DrawText(TextFormat("> %s: %d", name.c_str(), val), sw - 230, dy, 12, GREEN); dy += 15; }
}

void BitDialog::DrawTextWrapped(const std::string& text, int x, int y, int fontSize, int maxWidth, Color color) {
    std::string s(text), l = "", w; std::stringstream ss(s); int curY = y;
    while (ss >> w) {
        std::string t = l + (l.empty() ? "" : " ") + w;
        if (MeasureTextEx(GetFontDefault(), t.c_str(), (float)fontSize, 2).x > maxWidth && !l.empty()) { 
            DrawTextEx(GetFontDefault(), l.c_str(), { (float)x, (float)curY }, (float)fontSize, 2, color); l = w; curY += fontSize + 5; 
        } else l = t;
    }
    if (!l.empty()) DrawTextEx(GetFontDefault(), l.c_str(), { (float)x, (float)curY }, (float)fontSize, 2, color);
}

Texture2D BitDialog::GetTexture(const std::string& path) {
    if (path.empty()) return m_fallbackTexture;
    if (m_textureCache.count(path)) return m_textureCache[path];
    if (FileExists(path.c_str())) { Texture2D tex = LoadTexture(path.c_str()); if (tex.id > 0) { m_textureCache[path] = tex; return tex; } }
    return m_fallbackTexture;
}

void BitDialog::PlaySFX(const std::string& path) {
    if (!FileExists(path.c_str())) return;
    if (!m_sfxCache.count(path)) m_sfxCache[path] = LoadSound(path.c_str());
    PlaySound(m_sfxCache[path]);
}

void BitDialog::CreateFallbackTexture() {
    Image img = GenImageChecked(64, 64, 32, 32, MAGENTA, BLACK); m_fallbackTexture = LoadTextureFromImage(img); UnloadImage(img);
}

void BitDialog::CreateVignetteTexture() {
    Image img = GenImageGradientRadial(64, 64, 0.0f, WHITE, BLACK); m_vignette = LoadTextureFromImage(img); UnloadImage(img);
}
