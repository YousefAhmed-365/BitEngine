#include "RayDialog.hpp"
#include <vector>
#include <string>
#include <sstream>

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

RayDialog::RayDialog(DialogEngine& engine) : m_engine(engine) {}

void RayDialog::Draw() {
    if (!m_engine.IsActive()) return;
    DrawMainBox();
    if (!m_engine.IsTextRevealing()) DrawChoiceBox();
    if (m_engine.IsDebug()) DrawDebugOverlay();
    
    // Draw Persistence Notification (Toast)
    if (saveToastTimer > 0.0f) {
        DrawRectangle(GetScreenWidth() - 220, 20, 200, 40, Style::BG);
        DrawRectangleLines(GetScreenWidth() - 220, 20, 200, 40, Style::BORDER);
        DrawText(saveToastMsg.c_str(), GetScreenWidth() - 200, 32, 16, Style::TEXT);
        saveToastTimer -= GetFrameTime();
    }
}

void RayDialog::HandleInput() {
    if (!m_engine.IsActive()) return;
    
    // Core Navigation
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) { m_engine.Next(); return; }
    
    // Quick Save/Load System
    if (IsKeyPressed(KEY_F5)) {
        m_engine.SaveGame(1); // Slot 1 for Quick Save
        saveToastMsg = "QUICK SAVE...";
        saveToastTimer = 2.0f;
    }
    if (IsKeyPressed(KEY_F9)) {
        if (m_engine.LoadGame(1)) {
            saveToastMsg = "RELOADING...";
            saveToastTimer = 2.0f;
        }
    }

    if (!m_engine.IsTextRevealing()) {
        auto& options = m_engine.GetVisibleOptions();
        for (int i = 0; i < (int)options.size(); ++i) {
            if (IsKeyPressed(KEY_ONE + i)) { m_engine.SelectOption(i); return; }
            
            // Mouse Interaction (Slot-aware)
            Rectangle r = { (float)GetScreenWidth() - 440, (float)GetScreenHeight() - 260 - ((float)options.size() * 40), 380, 30 };
            r.y += 45 + (i * 35);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), r)) {
                m_engine.SelectOption(i); return;
            }
        }
    }
}

void RayDialog::DrawMainBox() {
    auto* n = m_engine.GetCurrentNode();
    auto* e = m_engine.GetCurrentEntity();
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Rectangle box = { 50, (float)sh - 240, (float)sw - 100, 200 };

    DrawRectangleRounded(box, 0.1f, 10, Style::BG);
    DrawRectangleRoundedLinesEx(box, 0.1f, 10, 2.0f, Style::BORDER);
    DrawRectangleGradientH(box.x + 2, box.y + 2, box.width - 4, 3, Style::BORDER, Fade(Style::BORDER, 0));

    if (e) {
        DrawText(e->name.c_str(), box.x + Style::PADDING, box.y + 30, Style::FONT_NAME, Style::LABEL);
        if (n->metadata.count("expression")) 
            DrawText(TextFormat("[%s]", n->metadata.at("expression").c_str()), box.x + Style::PADDING + MeasureText(e->name.c_str(), Style::FONT_NAME) + 15, box.y + 36, 18, Style::HOVER);
    }

    DrawTextWrapped(m_engine.GetVisibleContent().c_str(), box.x + Style::PADDING, box.y + 75, Style::FONT_MAIN, (int)box.width - 80, Style::TEXT);

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
        if (CheckCollisionPointRec(GetMousePosition(), oRect)) {
            DrawRectangleRounded(oRect, 0.2f, 5, Fade(Style::HOVER, 0.15f));
            col = Style::HOVER;
        }
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
    for (auto const& [name, val] : m_engine.GetAllVariables()) {
        DrawText(TextFormat("> %s: %d", name.c_str(), val), sw - 280, dy, 14, GREEN);
        dy += 18;
    }
    
    // Show Save Slots Status
    dy += 10;
    DrawText("SAVE SLOTS:", sw - 280, dy, 14, Style::BORDER); dy += 18;
    for (int i = 0; i < 3; ++i) {
        bool has = m_engine.HasSave(i);
        DrawText(TextFormat("[%d] %s", i, has ? "OCCUPIED" : "EMPTY"), sw - 280, dy, 12, has ? GREEN : GRAY);
        dy += 14;
    }
}

void RayDialog::DrawTextWrapped(const char* text, int x, int y, int fontSize, int maxWidth, Color color) {
    std::string s(text), l = "", w; std::stringstream ss(s);
    int curY = y;
    while (ss >> w) {
        std::string t = l + (l.empty() ? "" : " ") + w;
        if (MeasureTextEx(GetFontDefault(), t.c_str(), (float)fontSize, 2).x > maxWidth && !l.empty()) {
            DrawTextEx(GetFontDefault(), l.c_str(), { (float)x, (float)curY }, (float)fontSize, 2, color);
            l = w; curY += fontSize + 5;
        } else l = t;
    }
    if (!l.empty()) DrawTextEx(GetFontDefault(), l.c_str(), { (float)x, (float)curY }, (float)fontSize, 2, color);
}
