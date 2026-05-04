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
    m_uiManager.Shutdown();
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

    // 1. Drain UI commands from the VM (ui_load, ui_unload, ui_set)
    for (auto& cmd : m_engine.DrainUICommands()) {
        switch (cmd.type) {
            case UICommand::Type::Load:
                m_uiManager.Load(cmd.name, cmd.arg1, cmd.layer);
                break;
            case UICommand::Type::Unload:
                m_uiManager.Unload(cmd.name);
                break;
            case UICommand::Type::Set:
                if (cmd.arg1 == "visible")
                    m_uiManager.SetVisible(cmd.name, cmd.arg2 == "true" || cmd.arg2 == "1");
                else if (cmd.arg1 == "content")
                    m_uiManager.SetContent(cmd.name, cmd.arg2);
                break;
        }
    }

    // 2. Populate data store from engine state (system vars + game vars)
    m_dataStore = m_engine.GetSystemVars();

    int sw = GetScreenWidth(), sh = GetScreenHeight();
    m_uiManager.Resolve(sw, sh, &m_dataStore);
    HandleAudio();

    float intensity = m_engine.GetEffectShake();
    float shakeX = (float)GetRandomValue(-(int)intensity, (int)intensity);
    float shakeY = (float)GetRandomValue(-(int)intensity, (int)intensity);
    Camera2D cam = { {0,0}, {shakeX, shakeY}, 0.0f, 1.0f };

    BeginMode2D(cam);
    DrawScene();
    
    for (UILayout* layout : m_uiManager.GetSortedLayers()) {
        for (auto* root : layout->GetRoots()) {
            if (root->role == "history_panel" ||
                root->role == "mouse_cursor"  ||
                root->role == "toast" ||
                root->role == "choice_list") continue;  // Skip choice_list - we render it separately
            DrawElement(*root);
        }
    }
    EndMode2D();

    // History drawn outside camera shake
    UIElement* hist = m_uiManager.FindByRole("history_panel");
    if (m_showHistory && hist && hist->visible) DrawHistory();

    // Choice Panel (direct rendering, bypasses UI layout system)
    DrawChoicesPanel();
    if (m_toastTimer > 0.0f) {
        UIElement* toast = m_uiManager.FindByRole("toast");
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

    if (m_engine.IsDebugOverlayVisible()) DrawDebugOverlay();

    UIElement* cur = m_uiManager.FindByRole("mouse_cursor");
    DrawCustomCursor(cur);
}

// ─────────────────────────────────────────────────────────────────────────────
// Element dispatcher
// ─────────────────────────────────────────────────────────────────────────────
void BitRenderer::DrawElement(UIElement& elem) {
    if (!elem.visible) {
        return;
    }

    if      (elem.type == "group")         DrawGroupElem(elem);
    else if (elem.type == "panel")         DrawPanelElem(elem);
    else if (elem.type == "text")          DrawTextElem(elem);
    else if (elem.type == "rich_text")     DrawRichTextElem(elem);
    else if (elem.type == "cursor")        DrawCursorElem(elem);
    else if (elem.type == "button")        DrawButtonElem(elem);
    else if (elem.type == "image")         DrawImageElem(elem);

    for (auto& child : elem.children) DrawElement(child);
}

// ─────────────────────────────────────────────────────────────────────────────
// Scene Renderer (Backgrounds, Entities, Cinematic Effects)
// ─────────────────────────────────────────────────────────────────────────────
void BitRenderer::DrawScene() {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    // 1. Draw Background
    Color clear = Color{8,8,20,255};
    DrawRectangle(0, 0, sw, sh, clear);

    std::string activeBg = m_engine.GetActiveBg();
    std::string prevBg   = m_engine.GetPrevBg();
    float fade = m_engine.GetBgFadeAlpha();

    if (fade < 1.0f && !prevBg.empty()) {
        std::string p = m_engine.GetBackground(prevBg); if (p.empty()) p = prevBg;
        Texture2D t = GetTexture(p);
        DrawTexturePro(t,{0,0,(float)t.width,(float)t.height},
            {0,0,(float)sw,(float)sh},{0,0},0,WHITE);
    }
    if (!activeBg.empty()) {
        std::string p = m_engine.GetBackground(activeBg); if (p.empty()) p = activeBg;
        Texture2D t = GetTexture(p);
        Color tint = WHITE; if (fade < 1.0f) tint.a = (uint8_t)(fade*255);
        DrawTexturePro(t,{0,0,(float)t.width,(float)t.height},
            {0,0,(float)sw,(float)sh},{0,0},0,tint);
    }

    // 2. Draw Entities
    float escale    = 3.0f; // Default entity scale
    float floatAmp  = 10.0f;
    float floatSpd  = 2.0f;
    float shadowOp  = 0.35f;
    const auto& activeEntities = m_engine.GetActiveEntities();
    int i = 0;
    for (const auto& [entityId, state] : activeEntities) {
        const auto* entity = m_engine.GetEntity(entityId);
        if (!entity || !state.visible) continue;
        std::string expr = state.expression;
        std::string path; int frames=1; float speed=1.0f, scale=escale;
        if (entity->sprites.count(expr)) {
            auto& sp=entity->sprites.at(expr); path=sp.path; frames=sp.frames; speed=sp.speed;
            scale=sp.scale*escale/3.0f;
        } else if (entity->sprites.count("idle")) {
            auto& sp=entity->sprites.at("idle"); path=sp.path; frames=sp.frames; speed=sp.speed;
            scale=sp.scale*escale/3.0f;
        }
        Texture2D tex = path.empty() ? m_fallbackTexture : GetTexture(path);
        int finalFrames = frames>0?frames:1;
        int curFrame = (int)(GetTime()*speed) % finalFrames;
        int fw = tex.width/finalFrames;
        Rectangle src = {(float)(curFrame*fw),0,(float)fw,(float)tex.height};
        float floatOff = m_engine.GetConfigs().enable_floating
            ? sinf((float)GetTime()*floatSpd + i*2.0f)*floatAmp : 0.0f;
        Vector2 pos = {(sw*state.currentNormX)-(fw*scale)/2.0f,
                       (sh*0.45f)-(tex.height*scale)/2.0f+floatOff};
        Color tint = WHITE; tint.a = (uint8_t)(state.alpha*255);
        if (m_engine.GetConfigs().enable_shadows)
            DrawEllipse((int)pos.x+(int)(fw*scale/2),(int)pos.y+(int)(tex.height*scale),
                (int)(fw*scale/3),10,Fade(BLACK,shadowOp*state.alpha));
        DrawTexturePro(tex,src,{pos.x,pos.y,fw*scale,tex.height*scale},{0,0},0,tint);
        i++;
    }

    // 3. Draw Vignette & Screen Fade
    if (m_engine.GetConfigs().enable_vignette) {
        float op = 0.4f;
        DrawTexturePro(m_vignette,{0,0,64,64},
            {0,0,(float)sw,(float)sh},{0,0},0,Fade(WHITE,op));
    }
    float screenFade = m_engine.GetScreenFadeAlpha();
    if (screenFade > 0.001f) {
        BitColor bc = m_engine.GetScreenFadeColor();
        DrawRectangle(0,0,sw,sh, {bc.r,bc.g,bc.b,(uint8_t)(screenFade*255)});
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Input
// ─────────────────────────────────────────────────────────────────────────────
static bool CheckClickRec(UIElement& elem, Vector2 mousePos, DialogEngine& engine) {
    if (!elem.visible) return false;
    for (auto it = elem.children.rbegin(); it != elem.children.rend(); ++it) {
        if (CheckClickRec(*it, mousePos, engine)) return true;
    }
    if (!elem.onClick.empty() && CheckCollisionPointRec(mousePos, elem.computedRect)) {
        const std::string& oc = elem.onClick;
        if (oc.rfind("emit ", 0) == 0) {
            engine.EmitEvent(oc.substr(5));
        } else if (oc.rfind("select_choice ", 0) == 0) {
            try { engine.SelectOption(std::stoi(oc.substr(14))); } catch(...) {}
        }
        return true;
    }
    return false;
}

void BitRenderer::HandleInput() {
    if (!m_engine.IsActive()) return;

    // 1. CHOICE INPUT (highest priority - direct rendering system, instant response)
    //    Choices use a dedicated rendering pipeline (DrawChoicesPanel) that bypasses
    //    the complex UI layout system. This ensures zero-latency click detection.
    HandleChoiceInput();
    if (m_engine.IsInputLocked()) return;

    // 2. UI Click Handling (Always active, bypasses narrative lockout)
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        Vector2 mpos = GetMousePosition();
        auto layers = m_uiManager.GetSortedLayers();
        bool handled = false;
        for (auto it = layers.rbegin(); it != layers.rend() && !handled; ++it) {
            auto roots = (*it)->GetRoots();
            for (int j = (int)roots.size() - 1; j >= 0; --j) {
                if (CheckClickRec(*roots[j], mpos, m_engine)) { handled = true; break; }
            }
        }
        if (handled) return;
    }

    // 3. Global Hotkeys (Always active)
    if (IsKeyPressed(KEY_A)) m_engine.ToggleAutoPlay();
    if (IsKeyPressed(KEY_H)) { m_showHistory = !m_showHistory; if (m_showHistory) m_historyScroll = 0; }
    if (IsKeyPressed(KEY_F3)) m_engine.ToggleDebugOverlay();

    if (m_showHistory) {
        m_historyScroll -= GetMouseWheelMove() * 40.0f;
        if (m_historyScroll < 0) m_historyScroll = 0;
        if (IsKeyPressed(KEY_ESCAPE)) m_showHistory = false;
        return;
    }

    // 4. Narrative Advance Logic (Subject to lockout)
    if (m_engine.IsInputLocked()) return;

    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) { m_engine.Next(); return; }
    if (IsKeyPressed(KEY_F5)) { m_engine.SaveGame(1); m_toastMsg = "QUICK SAVE..."; m_toastTimer = 2.0f; }
    if (IsKeyPressed(KEY_F9)) { if (m_engine.LoadGame(1)) { m_toastMsg = "RELOADING..."; m_toastTimer = 2.0f; } }

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (!m_engine.IsChoiceVisible()) {
            m_engine.Next();
        }
    }

    if (!m_engine.IsTextRevealing()) {
        auto& opts = m_engine.GetVisibleOptions();
        for (int i = 0; i < (int)opts.size(); ++i) {
            if (IsKeyPressed(KEY_ONE + i)) { m_engine.SelectOption(i); return; }
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

Font BitRenderer::GetFont(const std::string& path) { return m_uiManager.GetFont(path); }

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
    if (font.texture.id == 0) font = m_uiManager.GetFont("");
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
