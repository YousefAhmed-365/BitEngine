#include "BitRenderer.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

// ─────────────────────────────────────────────────────────────────────────────
// DrawGroupElem
// ─────────────────────────────────────────────────────────────────────────────

void BitRenderer::DrawGroupElem(UIElement& elem) {
    if (elem.resolvedStyle.bgColor.has_value())
        DrawStyledPanel(elem.computedRect, elem.resolvedStyle);
}

void BitRenderer::DrawPanelElem(UIElement& elem) {
    if (!elem.displayContent.empty()) {
        const auto& s = elem.resolvedStyle;
        Font f = GetFont(s.fontPath);
        int fs = s.fontSize.value_or(28);
        float tw = MeasureTextEx(f, elem.displayContent.c_str(), (float)fs, 2).x;
        float padL = elem.padLeft, padR = elem.padRight;
        Rectangle r = elem.computedRect;
        r.width = tw + padL + padR;
        elem.computedRect = r;
        DrawStyledPanel(r, s);
        
        // Push rect to child text element if exists
        if (!elem.children.empty() && elem.children[0].type == "text") {
            elem.children[0].computedRect = r;
            elem.children[0].contentRect  = {r.x+padL, r.y, tw, r.height};
        }
        return;
    }
    DrawStyledPanel(elem.computedRect, elem.resolvedStyle);
}

void BitRenderer::DrawTextElem(UIElement& elem) {
    if (elem.displayContent.empty()) return;
    const auto& s = elem.resolvedStyle;
    Font f = GetFont(s.fontPath);
    int fs = s.fontSize.value_or(24);
    Color tc = s.textColor.value_or(Color{255,215,0,255});
    Rectangle r = elem.contentRect;
    float ty = r.y + r.height/2.0f - fs/2.0f;
    DrawTextEx(f, elem.displayContent.c_str(), {r.x, ty}, (float)fs, 2.0f, tc);
}

void BitRenderer::DrawRichTextElem(UIElement& elem) {
    if (!m_engine.IsActive()) return;
    // We bind visibility from the layout logic now, so no need for m_engine.IsUiHidden() here
    // But we still pull parsed content directly from the engine to get typewriter state for now.
    const auto& s = elem.resolvedStyle;
    int fs = s.fontSize.value_or(24);
    int ls = s.lineSpacing.value_or(5);
    Color col = s.textColor.value_or(RAYWHITE);
    Font f = GetFont(s.fontPath);
    Rectangle cr = elem.contentRect;
    DrawRichText(m_engine.GetParsedContent(), m_engine.GetRevealedCount(),
                 (int)cr.x, (int)cr.y, fs, (int)cr.width, col, ls, f);
}

void BitRenderer::DrawCursorElem(UIElement& elem) {
    if (m_engine.IsTextRevealing()) return;
    const auto& s = elem.resolvedStyle;
    float animAlpha = s.opacity.value_or(1.0f); // Driven by UILayout generic animations

    float bR = elem.computedRect.x + elem.computedRect.width;
    float bB = elem.computedRect.y + elem.computedRect.height;
    float sz = s.cursorSize.value_or(8.0f);
    const UITexture& ct = s.cursorTexture;
    
    if (!ct.path.empty()) {
        Texture2D t = GetTexture(ct.path);
        if (t.id > 0) {
            Color tint = ct.tint;
            tint.a = (uint8_t)(tint.a * animAlpha);
            DrawTexturePro(t,{0,0,(float)t.width,(float)t.height},
                {bR-t.width/2,bB-t.height/2,(float)t.width,(float)t.height},{0,0},0,tint);
            return;
        }
    }
    
    std::string shape = s.cursorShape.value_or("triangle");
    Color col = s.cursorColor.value_or(Color{0,210,255,255});
    col.a = (uint8_t)(col.a * animAlpha);

    if (shape == "dot") {
        DrawCircle((int)bR,(int)bB,sz,col);
    } else if (shape == "bar") {
        DrawRectangle((int)(bR-sz*2),(int)bB,(int)(sz*2),(int)(sz*0.45f),col);
    } else { // default triangle
        Vector2 v1 = {bR, bB + sz};
        Vector2 v2 = {bR - sz, bB - sz};
        Vector2 v3 = {bR + sz, bB - sz};
        DrawTriangle(v2, v1, v3, col);
    }
}

void BitRenderer::DrawButtonElem(UIElement& elem) {
    const auto& s = elem.resolvedStyle;
    Rectangle r = elem.computedRect;

    // Hover state
    bool hovered = CheckCollisionPointRec(GetMousePosition(), r);
    Color bg = s.bgColor.value_or(Color{0,0,0,0});
    if (hovered) bg = Fade(bg.a > 0 ? bg : Color{255,255,255,80}, 0.25f);

    DrawStyledPanel(r, s);
    if (hovered) DrawRectangleRec(r, Fade(Color{255,255,255,255}, 0.12f));

    // Label
    if (!elem.displayContent.empty()) {
        int fs = s.optionFontSize.value_or(s.fontSize.value_or(20));
        Color col = hovered
            ? s.optionHover.value_or(WHITE)
            : s.optionColor.value_or(Color{0,200,255,255});
        Font f = GetFont(s.choiceFontPath.empty() ? s.fontPath : s.choiceFontPath);
        float ty = r.y + r.height * 0.5f - fs * 0.5f;
        DrawTextEx(f, elem.displayContent.c_str(), {r.x + 40, ty}, (float)fs, 2.0f, col);
        DrawCircle((int)r.x + 20, (int)(r.y + r.height * 0.5f), 4, col);
    }
}

void BitRenderer::DrawImageElem(UIElement& elem) {
    const UITexture& tex = elem.resolvedStyle.texture;
    if (!tex.path.empty()) {
        Texture2D t = GetTexture(tex.path);
        if (t.id > 0)
            DrawTexturePro(t,{0,0,(float)t.width,(float)t.height},
                elem.computedRect,{0,0},0,tex.tint);
    }
}

void BitRenderer::DrawCustomCursor(UIElement* elem) {
    if (!elem) { ShowCursor(); return; }
    std::string path = elem->resolvedStyle.mouseCursorPath;
    if (path.empty()) { ShowCursor(); return; }
    if (m_currentCursorPath != path) {
        m_currentCursorPath = path;
        m_customCursor = GetTexture(path);
        if (m_customCursor.id != 0) HideCursor();
    }
    if (m_customCursor.id != 0) {
        float sc = elem->resolvedStyle.mouseCursorScale.value_or(1.0f);
        DrawTextureEx(m_customCursor, GetMousePosition(), 0.0f, sc, WHITE);
    }
}

void BitRenderer::DrawHistory() {
    UIElement* hist = m_uiManager.FindByRole("history_panel");
    const UIStyleBlock& s = hist ? hist->resolvedStyle : UIStyleBlock{};
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    Font font = GetFont(s.historyFontPath);
    Color accentCol = s.historySpeakerColor.value_or(SKYBLUE);
    Color contentCol = s.historyContentColor.value_or(RAYWHITE);
    Color dimCol = s.historyDimColor.value_or(Color{245,245,255,30});
    Color bgCol  = s.historyBg.value_or(Color{0,0,0,200});
    int HEADER_H = s.historyHeaderHeight.value_or(56);
    int FOOTER_H = s.historyFooterHeight.value_or(36);
    int SIDE_W   = s.historySidebarWidth.value_or(140);
    int ENTRY_GAP= s.historyEntryGap.value_or(16);
    float SP_SIZE= s.historySpeakerFontSize.value_or(14.0f);
    float CT_SIZE= s.historyContentFontSize.value_or(18.0f);
    int CONTENT_X= SIDE_W+20, CONTENT_W = sw-CONTENT_X-24;

    DrawRectangle(0,0,sw,sh,bgCol);
    const auto& history = m_engine.GetHistory();
    float totalH = (float)HEADER_H+10.0f;
    for (auto& e : history) {
        float cw=CT_SIZE*0.55f; int cpl=std::max(1,(int)(CONTENT_W/cw));
        int lines=std::max(1,((int)e.richContent.size()+cpl-1)/cpl);
        totalH += std::max((float)(lines*(CT_SIZE+4)), SP_SIZE+8) + ENTRY_GAP + 12;
    }
    float viewH = (float)(sh-HEADER_H-FOOTER_H);
    float maxScroll = std::max(0.0f,totalH-viewH);
    if (m_historyScroll > maxScroll) m_historyScroll = maxScroll;
    if (m_historyScroll < 0)        m_historyScroll = 0;
    BeginScissorMode(0,HEADER_H,sw,sh-HEADER_H-FOOTER_H);
    float py = (float)HEADER_H+10.0f-m_historyScroll;
    for (size_t i=0;i<history.size();++i) {
        const auto& entry=history[i];
        if (py < (float)(sh-FOOTER_H) && py+80 > (float)HEADER_H) {
            int newY = DrawRichText(entry.richContent,(int)entry.richContent.size(),
                CONTENT_X,(int)py,(int)CT_SIZE,CONTENT_W,contentCol,6,font);
            float contentH=(float)newY-py, pillH=SP_SIZE+8;
            float entryH=std::max(contentH,pillH);
            float pillY=py+(entryH-pillH)*0.5f;
            DrawText(TextFormat("#%d",(int)(i+1)),8,(int)pillY+2,9,ColorAlpha(RAYWHITE,0.25f));
            float spW=MeasureTextEx(font,entry.speaker.c_str(),SP_SIZE,1).x+16;
            float spX=((float)SIDE_W-spW)*0.5f;
            DrawRectangleRounded({spX,pillY-2,spW,pillH},0.4f,6,ColorAlpha(accentCol,0.18f));
            DrawTextEx(font,entry.speaker.c_str(),{spX+8,pillY},SP_SIZE,1,accentCol);
            DrawRectangle(SIDE_W-2,(int)py,2,(int)entryH,ColorAlpha(accentCol,0.25f));
            py+=entryH+ENTRY_GAP;
            if (i+1<history.size()) { DrawLineEx({(float)SIDE_W,py},{(float)(sw-12),py},1.0f,dimCol); py+=12; }
        } else {
            float cw=CT_SIZE*0.55f; int cpl=std::max(1,(int)(CONTENT_W/cw));
            int lines=std::max(1,((int)entry.richContent.size()+cpl-1)/cpl);
            py+=std::max((float)(lines*(CT_SIZE+4)),SP_SIZE+8)+ENTRY_GAP+12;
        }
    }
    EndScissorMode();
    if (maxScroll>0) {
        float barH=(float)(sh-HEADER_H-FOOTER_H), ratio=m_historyScroll/maxScroll;
        float thumbH=std::max(30.0f,barH*viewH/totalH), thumbY=(float)HEADER_H+ratio*(barH-thumbH);
        DrawRectangle(sw-6,HEADER_H,6,(int)barH,ColorAlpha(WHITE,0.06f));
        DrawRectangle(sw-6,(int)thumbY,6,(int)thumbH,ColorAlpha(accentCol,0.6f));
    }
    DrawRectangleGradientV(0,0,sw,HEADER_H+8,ColorAlpha(BLACK,0.96f),ColorAlpha(BLACK,0));
    DrawRectangle(0,0,sw,HEADER_H,ColorAlpha(BLACK,0.95f));
    DrawLineEx({0,(float)HEADER_H},{(float)sw,(float)HEADER_H},1.5f,ColorAlpha(accentCol,0.4f));
    DrawTextEx(font,"MESSAGE HISTORY",{18,14},SP_SIZE+10,1,accentCol);
    DrawText(TextFormat("%d entries",(int)history.size()),22,(int)(14+SP_SIZE+12),9,ColorAlpha(RAYWHITE,0.4f));
    DrawText("[ H ] or [ ESC ] to close  |  Mouse Wheel to scroll",sw-310,HEADER_H/2-5,10,ColorAlpha(RAYWHITE,0.4f));
    int fy=sh-FOOTER_H;
    DrawRectangle(0,fy,sw,FOOTER_H,ColorAlpha(BLACK,0.95f));
    DrawLineEx({0,(float)fy},{(float)sw,(float)fy},1.0f,ColorAlpha(accentCol,0.25f));
    if (!history.empty()) {
        const auto& last=history.back();
        DrawText(TextFormat("Latest: [%s]  %s",last.speaker.c_str(),last.content.substr(0,60).c_str()),
                 18,fy+10,10,ColorAlpha(RAYWHITE,0.45f));
    }
}



// ─────────────────────────────────────────────────────────────────────────────
// Direct Choice Panel Rendering & Input Handling
// ─────────────────────────────────────────────────────────────────────────────
// Choices use a dedicated rendering pipeline that completely bypasses the UI
// layout system. This ensures:
//   - Zero latency click detection (rendered and clicked in same frame)
//   - Independent of layout resolution complexity
//   - Cached rectangles stay in sync with visual positioning
// See HandleChoiceInput() below for the associated input handler.
// ─────────────────────────────────────────────────────────────────────────────

void BitRenderer::DrawChoicesPanel() {
    auto& opts = m_engine.GetVisibleOptions();
    if (opts.empty() || m_engine.IsTextRevealing() || m_engine.IsUiHidden()) return;

    const UIElement* choiceElem = m_uiManager.FindByRole("choice_list");
    if (!choiceElem) return;
    
    const auto& s = choiceElem->resolvedStyle;
    int optH    = s.optionHeight.value_or(38);
    int optGap  = s.optionGap.value_or(10);
    int optFs   = s.optionFontSize.value_or(20);
    Color optCol  = s.optionColor.value_or(Color{0,180,255,255});
    Color hvrCol  = s.optionHover.value_or(WHITE);
    Color premCol = s.optionPremium.value_or(Color{255,0,255,255});
    Font f = GetFont(s.choiceFontPath.empty() ? s.fontPath : s.choiceFontPath);

    // Calculate dynamically centered panel rectangle
    float pad   = choiceElem->padTop + choiceElem->padBottom;
    float totalH = (float)(opts.size() * (optH + optGap) - optGap) + pad;
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    Rectangle r = {
        sw * 0.5f - choiceElem->computedRect.width * 0.5f,
        sh * 0.5f - totalH * 0.5f - 80.0f,
        choiceElem->computedRect.width,
        totalH
    };

    DrawStyledPanel(r, s);

    // Rebuild choice hit rectangles for this frame (used by HandleChoiceInput)
    m_choiceRects.clear();

    for (int i = 0; i < (int)opts.size(); ++i) {
        Rectangle oRect = {
            r.x + choiceElem->padLeft,
            r.y + choiceElem->padTop + i * (optH + optGap),
            r.width - choiceElem->padLeft - choiceElem->padRight,
            (float)optH
        };
        m_choiceRects.push_back(oRect);

        Color col = (opts[i].style == "premium") ? premCol : optCol;
        if (CheckCollisionPointRec(GetMousePosition(), oRect)) {
            DrawRectangleRec(oRect, Fade(col, 0.2f));
            DrawRectangleLinesEx(oRect, 1.0f, col);
            col = hvrCol;
        }
        float ty = oRect.y + optH * 0.5f - optFs * 0.5f;
        DrawTextEx(f, opts[i].content.c_str(), {oRect.x + 40, ty}, (float)optFs, 2.0f, col);
        DrawCircle((int)oRect.x + 20, (int)(oRect.y + optH * 0.5f), 4, col);
    }
}

void BitRenderer::HandleChoiceInput() {
    auto& opts = m_engine.GetVisibleOptions();
    if (opts.empty() || m_engine.IsTextRevealing() || m_engine.IsUiHidden()) return;

    // Mouse click detection against cached rectangles from DrawChoicesPanel()
    // This runs at the highest priority in the input pipeline to ensure instant response
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        Vector2 mpos = GetMousePosition();
        for (int i = 0; i < (int)m_choiceRects.size(); ++i) {
            if (CheckCollisionPointRec(mpos, m_choiceRects[i])) {
                m_engine.SelectOption(i);
                return;
            }
        }
    }

    // Keyboard selection (number keys 1-9)
    if (!m_engine.IsTextRevealing()) {
        for (int i = 0; i < (int)opts.size() && i < 9; ++i) {
            if (IsKeyPressed(KEY_ONE + i)) {
                m_engine.SelectOption(i);
                return;
            }
        }
    }
}
