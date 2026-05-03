#include "headers/BitRenderer.hpp"
#include <iostream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <sstream>

// ─────────────────────────────────────────────────────────────────────────────
// Construction / Destruction
// ─────────────────────────────────────────────────────────────────────────────
BitRenderer::BitRenderer(DialogEngine& engine) : m_engine(engine) {
    InitAudioDevice();
    CreateFallbackTexture();
    CreateVignetteTexture();
}

BitRenderer::~BitRenderer() {
    m_layout.Shutdown();
    for (auto& [p, t] : m_textureCache) UnloadTexture(t);
    for (auto& [p, s] : m_sfxCache)     UnloadSound(s);
    for (auto& [p, m] : m_musicCache)   UnloadMusicStream(m);
    UnloadTexture(m_fallbackTexture);
    UnloadTexture(m_vignette);
    CloseAudioDevice();
}

// ─────────────────────────────────────────────────────────────────────────────
// Main Draw loop
// ─────────────────────────────────────────────────────────────────────────────
void BitRenderer::Draw() {
    if (!m_engine.IsActive()) return;

    // Toggle element visibilities from the application/renderer level based on engine state
    if (auto* e = m_layout.FindById("dialog_root")) {
        e->visible = !m_engine.IsUiHidden();
    }
    if (auto* e = m_layout.FindByRole("name_label")) {
        e->visible = (m_engine.GetCurrentEntity() != nullptr);
    }
    if (auto* e = m_layout.FindByRole("choice_list")) {
        e->visible = !m_engine.GetVisibleOptions().empty() && !m_engine.IsTextRevealing();
    }

    int sw = GetScreenWidth(), sh = GetScreenHeight();
    m_layout.Resolve(sw, sh);
    HandleAudio();

    float intensity = m_engine.GetEffectShake();
    float shakeX = (float)GetRandomValue(-(int)intensity, (int)intensity);
    float shakeY = (float)GetRandomValue(-(int)intensity, (int)intensity);
    Camera2D cam = { {0,0}, {shakeX, shakeY}, 0.0f, 1.0f };

    BeginMode2D(cam);
    for (auto* root : m_layout.GetRoots()) {
        if (root->role == "history_panel" ||
            root->role == "mouse_cursor"  ||
            root->role == "toast") continue;
        DrawElement(*root);
    }
    EndMode2D();

    // History drawn outside camera shake
    UIElement* hist = m_layout.FindByRole("history_panel");
    if (m_showHistory && hist && hist->visible) DrawHistory();

    if (m_engine.IsDebugOverlayVisible()) DrawDebugOverlay();

    UIElement* cur = m_layout.FindByRole("mouse_cursor");
    DrawCustomCursor(cur);

    // Toast
    if (m_toastTimer > 0.0f) {
        UIElement* toast = m_layout.FindByRole("toast");
        if (toast) {
            DrawElement(*toast);
            Font f = GetFont(toast->resolvedStyle.fontPath);
            int fs = toast->resolvedStyle.fontSize.value_or(16);
            Color tc = toast->resolvedStyle.textColor.value_or(RAYWHITE);
            float ty = toast->computedRect.y + toast->computedRect.height / 2.0f - fs / 2.0f;
            DrawTextEx(f, m_toastMsg.c_str(), {toast->computedRect.x + 10, ty}, (float)fs, 2.0f, tc);
        }
        m_toastTimer -= GetFrameTime();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Element dispatcher
// ─────────────────────────────────────────────────────────────────────────────
void BitRenderer::DrawElement(UIElement& elem) {
    if (!elem.visible) return;

    if      (elem.type == "background")    DrawBackgroundElem(elem);
    else if (elem.type == "entity_layer")  DrawEntityLayerElem(elem);
    else if (elem.type == "vignette")      DrawVignetteElem(elem);
    else if (elem.type == "group")         DrawGroupElem(elem);
    else if (elem.type == "panel")         DrawPanelElem(elem);
    else if (elem.type == "text")          DrawTextElem(elem);
    else if (elem.type == "rich_text")     DrawRichTextElem(elem);
    else if (elem.type == "cursor")        DrawCursorElem(elem);
    else if (elem.type == "choices")       DrawChoicesElem(elem);
    else if (elem.type == "image")         DrawImageElem(elem);

    for (auto& child : elem.children) DrawElement(child);
}

// ─────────────────────────────────────────────────────────────────────────────
// Input
// ─────────────────────────────────────────────────────────────────────────────
void BitRenderer::HandleInput() {
    if (!m_engine.IsActive()) return;

    if (IsKeyPressed(KEY_A)) m_engine.ToggleAutoPlay();
    if (IsKeyPressed(KEY_H)) { m_showHistory = !m_showHistory; if (m_showHistory) m_historyScroll = 0; }
    if (IsKeyPressed(KEY_F3)) m_engine.ToggleDebugOverlay();

    if (m_showHistory) {
        m_historyScroll -= GetMouseWheelMove() * 40.0f;
        if (m_historyScroll < 0) m_historyScroll = 0;
        if (IsKeyPressed(KEY_ESCAPE)) m_showHistory = false;
        return;
    }

    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) { m_engine.Next(); return; }
    if (IsKeyPressed(KEY_F5)) { m_engine.SaveGame(1); m_toastMsg = "QUICK SAVE..."; m_toastTimer = 2.0f; }
    if (IsKeyPressed(KEY_F9)) { if (m_engine.LoadGame(1)) { m_toastMsg = "RELOADING..."; m_toastTimer = 2.0f; } }

    if (!m_engine.IsTextRevealing()) {
        UIElement* choiceElem = m_layout.FindByRole("choice_list");
        auto& opts = m_engine.GetVisibleOptions();
        for (int i = 0; i < (int)opts.size(); ++i) {
            if (IsKeyPressed(KEY_ONE + i)) { m_engine.SelectOption(i); return; }
            if (choiceElem) {
                int optH  = choiceElem->resolvedStyle.optionHeight.value_or(38);
                int optGap = choiceElem->resolvedStyle.optionGap.value_or(10);
                float pad  = choiceElem->padTop + choiceElem->padBottom;
                float totalH = (float)(opts.size() * (optH + optGap) - optGap) + pad;
                float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
                float ry = sh * 0.5f - totalH * 0.5f - 80.0f;
                float rx = sw * 0.5f - choiceElem->computedRect.width * 0.5f;
                Rectangle oRect = {
                    rx + choiceElem->padLeft,
                    ry + choiceElem->padTop + i * (optH + optGap),
                    choiceElem->computedRect.width - choiceElem->padLeft - choiceElem->padRight,
                    (float)optH
                };
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
                    CheckCollisionPointRec(GetMousePosition(), oRect)) {
                    m_engine.SelectOption(i); return;
                }
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio
// ─────────────────────────────────────────────────────────────────────────────
void BitRenderer::HandleAudio() {
    if (!m_engine.IsActive()) return;
    std::string activeBgm = m_engine.GetActiveBgm();
    if (!activeBgm.empty()) {
        std::string path = m_engine.GetMusic(activeBgm);
        if (path.empty()) path = activeBgm;
        if (m_currentMusicPath != path) {
            auto it = m_musicCache.find(path);
            if (it != m_musicCache.end()) {
                if (m_isMusicPlaying) StopMusicStream(m_currentMusic);
                m_currentMusic = it->second;
                if (m_currentMusic.frameCount > 0) { PlayMusicStream(m_currentMusic); m_isMusicPlaying = true; }
                else m_isMusicPlaying = false;
                m_currentMusicPath = path;
            } else if (FileExists(path.c_str())) {
                Music m = LoadMusicStream(path.c_str());
                if (m.frameCount > 0) {
                    m_musicCache[path] = m;
                    if (m_isMusicPlaying) StopMusicStream(m_currentMusic);
                    m_currentMusic = m; PlayMusicStream(m_currentMusic); m_isMusicPlaying = true;
                }
                m_currentMusicPath = path;
            }
        }
    }
    if (m_isMusicPlaying) UpdateMusicStream(m_currentMusic);
    for (const auto& sfx : m_engine.ConsumePendingSFX()) {
        std::string p = m_engine.GetSFX(sfx); if (p.empty()) p = sfx; PlaySFX(p);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Preload
// ─────────────────────────────────────────────────────────────────────────────
void BitRenderer::PreloadAssets() {
    auto& proj = m_engine.GetProject();
    std::cout << "[BitRenderer] Preloading assets into memory...\n";
    for (auto& [id, path] : proj.backgrounds) GetTexture(path);
    for (auto& [id, path] : proj.music) {
        if (path.empty() || m_musicCache.count(path)) continue;
        m_musicCache[path] = LoadMusicStream(path.c_str());
    }
    for (auto& [id, path] : proj.sfx) {
        if (path.empty() || m_sfxCache.count(path)) continue;
        m_sfxCache[path] = LoadSound(path.c_str());
    }
    for (auto& [id, ent] : proj.entities)
        for (auto& [sid, sdef] : ent.sprites) GetTexture(sdef.path);
    for (auto& [id, path] : proj.fonts) GetFont(path);
}

// ─────────────────────────────────────────────────────────────────────────────
// Utilities
// ─────────────────────────────────────────────────────────────────────────────
Texture2D BitRenderer::GetTexture(const std::string& path) {
    if (path.empty()) return m_fallbackTexture;
    auto it = m_textureCache.find(path);
    if (it != m_textureCache.end()) return it->second.id ? it->second : m_fallbackTexture;
    if (FileExists(path.c_str())) {
        Texture2D t = LoadTexture(path.c_str());
        m_textureCache[path] = t;
        if (t.id > 0) return t;
    } else { m_textureCache[path] = {}; }
    return m_fallbackTexture;
}

Font BitRenderer::GetFont(const std::string& path) { return m_layout.GetFont(path); }

void BitRenderer::PlaySFX(const std::string& path) {
    auto it = m_sfxCache.find(path);
    if (it != m_sfxCache.end() && it->second.frameCount > 0) PlaySound(it->second);
}

void BitRenderer::CreateFallbackTexture() {
    Image img = GenImageChecked(64, 64, 32, 32, MAGENTA, BLACK);
    m_fallbackTexture = LoadTextureFromImage(img); UnloadImage(img);
}
void BitRenderer::CreateVignetteTexture() {
    Image img = GenImageGradientRadial(64, 64, 0.0f, BLANK, BLACK);
    m_vignette = LoadTextureFromImage(img); UnloadImage(img);
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawStyledPanel — draws background + border for any panel/group element
// ─────────────────────────────────────────────────────────────────────────────
void BitRenderer::DrawStyledPanel(Rectangle rect, const UIStyleBlock& style) {
    const UITexture& tex = style.texture;
    if (!tex.path.empty()) {
        Texture2D t = GetTexture(tex.path);
        if (t.id > 0) {
            if (tex.nineSlice) {
                NPatchInfo npi = { {0,0,(float)t.width,(float)t.height},
                    tex.sliceLeft, tex.sliceTop, tex.sliceRight, tex.sliceBottom,
                    NPATCH_NINE_PATCH };
                DrawTextureNPatch(t, npi, rect, {0,0}, 0.0f, tex.tint);
            } else {
                DrawTexturePro(t, {0,0,(float)t.width,(float)t.height},
                               rect, {0,0}, 0.0f, tex.tint);
            }
            return;
        }
    }
    float rnd    = style.roundness.value_or(0.0f);
    float bthick = style.borderThick.value_or(1.5f);
    Color bg     = style.bgColor.value_or(Color{15,15,25,240});
    Color border = style.borderColor.value_or(Color{0,210,255,255});
    DrawRectangleRounded(rect, rnd, 8, bg);
    if (bthick > 0.0f) DrawRectangleRoundedLinesEx(rect, rnd, 8, bthick, border);
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawRichText
// ─────────────────────────────────────────────────────────────────────────────
int BitRenderer::DrawRichText(const std::vector<RichChar>& content, int limit,
                              int x, int y, int fontSize, int maxWidth,
                              Color defaultColor, int lineSpacing, Font font) {
    if (font.texture.id == 0) font = m_layout.GetDefaultFont().texture.id
                                     ? m_layout.GetDefaultFont() : GetFontDefault();
    int curX = x, curY = y;
    float time = (float)GetTime(), spacing = 2.0f;
    int lineOffset = fontSize + lineSpacing;
    float spaceW = MeasureTextEx(font, " ", (float)fontSize, spacing).x;

    int i = 0;
    while (i < limit && i < (int)content.size()) {
        const char* cp = content[i].ch;
        if (cp[0] == ' ' || cp[0] == '\n') {
            if (cp[0] == '\n') { curX = x; curY += lineOffset; }
            else curX += (int)(spaceW + spacing);
            i++; continue;
        }
        int wordEnd = i; float wordW = 0.0f;
        while (wordEnd < limit && wordEnd < (int)content.size() &&
               content[wordEnd].ch[0] != ' ' && content[wordEnd].ch[0] != '\n') {
            wordW += MeasureTextEx(font, content[wordEnd].ch, (float)fontSize, spacing).x + spacing;
            wordEnd++;
        }
        if (curX + wordW > x + maxWidth && curX > x) { curX = x; curY += lineOffset; }
        for (int j = i; j < wordEnd; j++) {
            const auto& rc = content[j];
            Font f = font;
            if (!rc.font.empty()) {
                std::string fp = m_engine.GetProject().fonts.count(rc.font)
                                 ? m_engine.GetProject().fonts.at(rc.font) : "";
                if (!fp.empty()) f = GetFont(fp);
            }
            Color c = (rc.color.a == 0 && rc.color.r == 0 && rc.color.g == 0 && rc.color.b == 0)
                      ? defaultColor : Color{rc.color.r, rc.color.g, rc.color.b, rc.color.a};
            float ox = 0, oy = 0;
            if (rc.shake) { ox = (float)GetRandomValue(-2,2); oy = (float)GetRandomValue(-2,2); }
            if (rc.wave)  { oy += sinf(time * 6.0f + curX * 0.05f) * 4.0f; }
            float cw = MeasureTextEx(f, rc.ch, (float)fontSize, spacing).x;
            DrawTextEx(f, rc.ch, {(float)curX+ox,(float)curY+oy}, (float)fontSize, spacing, c);
            curX += (int)(cw + spacing);
        }
        i = wordEnd;
    }
    return curY + lineOffset;
}
