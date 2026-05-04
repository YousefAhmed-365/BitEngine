#include "headers/BitRenderer.hpp"
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

void BitRenderer::DrawDebugOverlay() {
    const auto& vars   = m_engine.GetAllVariables();
    const auto& trace  = m_engine.GetEventTrace();
    const auto& errors = m_engine.GetErrors();
    int sw=GetScreenWidth(), sh=GetScreenHeight();
    int gap=12, pad=10, cols=4;
    if (sw<1100) cols=2;
    if (sw<600)  cols=1;
    int rows_count=(6+cols-1)/cols;
    int panelW=(sw-40-(cols-1)*gap)/cols; if (panelW>350) panelW=350;
    int panelH=(sh-60-(rows_count-1)*(gap+20))/rows_count;
    if (panelH>380) panelH=380;
    if (panelH<150) panelH=150;
    int totalW=cols*panelW+(cols-1)*gap, startX=sw-totalW-pad, startY=20;
    int fullH=rows_count*panelH+(rows_count-1)*(gap+15);
    DrawRectangle(startX-pad,startY-pad,totalW+pad*2,fullH+pad*2,Fade(BLACK,0.85f));
    auto GetPanelRect=[&](int idx)->Rectangle{
        int r=idx/cols, c=idx%cols;
        return{(float)(startX+c*(panelW+gap)),(float)(startY+r*(panelH+gap+15)),(float)panelW,(float)panelH};
    };
    Rectangle r1=GetPanelRect(0); int px=(int)r1.x,py=(int)r1.y,dy=py;
    DrawText("VM INSPECTOR",px,py,13,SKYBLUE); dy+=22;
    DrawText(TextFormat("PC:      %d",m_engine.GetCurrentPC()),px,dy,10,RAYWHITE); dy+=14;
    DrawText(TextFormat("ACTIVE:  %s",m_engine.IsActive()?"YES":"NO"),px,dy,10,RAYWHITE); dy+=14;
    DrawText(TextFormat("SPEAKER: %s",m_engine.GetCurrentEntity()?m_engine.GetCurrentEntity()->id.c_str():"NONE"),px,dy,10,RAYWHITE); dy+=14;
    DrawText(TextFormat("WAITING: %s",m_engine.IsTextRevealing()?"YES":"NO"),px,dy,10,RAYWHITE);
    Rectangle r2=GetPanelRect(1); px=(int)r2.x; py=(int)r2.y; dy=py;
    DrawText("VARIABLE WATCHER",px,dy,13,GREEN); dy+=20;
    for (auto& [name,val]:vars) { DrawText(TextFormat("%-16s %d",name.c_str(),val),px,dy,10,LIME); dy+=13; }
    Rectangle r3=GetPanelRect(2); px=(int)r3.x; py=(int)r3.y; dy=py;
    DrawText("EVENT TRACE",px,dy,13,ORANGE); dy+=20;
    int maxT=12, tStart=(int)trace.size()>maxT?(int)trace.size()-maxT:0;
    for (int i=tStart;i<(int)trace.size();++i) {
        const auto& t=trace[i];
        std::string loc = t.node_id.size() > 10 ? t.node_id.substr(0,8)+".." : t.node_id;
        DrawText(TextFormat("%s: %s", t.op.c_str(), t.var.c_str()), px, dy, 10, ORANGE);
        DrawText(TextFormat("  %d -> %d  [@%s]", t.old_value, t.new_value, loc.c_str()), px, dy+12, 8, Fade(ORANGE, 0.6f));
        dy+=25;
    }
    Rectangle r4=GetPanelRect(3); px=(int)r4.x; py=(int)r4.y; dy=py;
    DrawText("ENGINE & CINEMATIC STATE",px,dy,13,GOLD); dy+=22;
    DrawText(TextFormat("UI Hidden:   %s",m_engine.IsUiHidden()?"TRUE":"FALSE"),px,dy,10,m_engine.IsUiHidden()?YELLOW:GRAY); dy+=14;
    DrawText(TextFormat("Auto-Play:   %s",m_engine.IsAutoPlaying()?"ON":"OFF"),px,dy,10,m_engine.IsAutoPlaying()?LIME:GRAY); dy+=14;
    std::string waitType=m_engine.GetWaitActionType();
    DrawText(TextFormat("Wait Action: %s",waitType.empty()?"NONE":waitType.c_str()),px,dy,10,!waitType.empty()?ORANGE:GRAY); dy+=14;
    DrawText(TextFormat("BG Fade:     %.2f",m_engine.GetBgFadeAlpha()),px,dy,10,RAYWHITE); dy+=18;
    DrawText("Entities:",px,dy,11,SKYBLUE); dy+=15;
    for (const auto& [id,st]:m_engine.GetActiveEntities()) {
        if (dy>r4.y+r4.height-20) break;
        DrawText(TextFormat("[%s] X:%.2f->%.2f A:%.2f",id.substr(0,6).c_str(),st.currentNormX,st.targetNormX,st.alpha),px,dy,9,GRAY); dy+=12;
    }
    Rectangle r5=GetPanelRect(4); px=(int)r5.x; py=(int)r5.y; dy=py;
    DrawText("LOCAL SCOPE & STACK",px,dy,13,VIOLET); dy+=22;
    const auto& stack=m_engine.GetCallStack();
    DrawText(TextFormat("Call Depth: %d",(int)stack.size()),px,dy,10,RAYWHITE); dy+=14;
    for (int i=0;i<(int)stack.size();++i){DrawText(TextFormat("  [%d] @ PC:%d",i,stack[i]),px,dy,9,Fade(RAYWHITE,0.7f));dy+=12;}
    dy+=10; DrawText("Active Locals:",px,dy,11,SKYBLUE); dy+=15;
    const auto& scopes=m_engine.GetLocalScopes();
    if (!scopes.empty()) for (const auto& [n,v]:scopes.back()){DrawText(TextFormat("%-14s %d",n.c_str(),v),px,dy,10,VIOLET);dy+=12;}
    else DrawText("(none)",px,dy,10,GRAY);
    Rectangle r6=GetPanelRect(5); px=(int)r6.x; py=(int)r6.y; dy=py;
    DrawText("BYTECODE INSPECTOR",px,dy,13,PURPLE); dy+=20;
    const auto& bc=m_engine.GetProject().bytecode; int curPC=m_engine.GetCurrentPC();
    int viewR=(panelH-30)/12, startI=std::max(0,curPC-viewR/2), endI=std::min((int)bc.size(),startI+viewR);
    for (int i=startI;i<endI;++i) {
        const auto& ins=bc[i]; Color col=(i==curPC-1)?YELLOW:(i==curPC)?GREEN:GRAY;
        if (i==curPC) DrawRectangle(px-2,dy,panelW,11,Fade(GREEN,0.2f));
        std::string opStr="?";
        switch(ins.op){
            case BitOp::SAY:         opStr="SAY"; break;
            case BitOp::TEXT:        opStr="TEXT"; break;
            case BitOp::CHOICE:      opStr="CHOICE"; break;
            case BitOp::IF:          opStr="IF"; break;
            case BitOp::IF_REF:      opStr="IF_REF"; break;
            case BitOp::GOTO:        opStr="GOTO"; break;
            case BitOp::SET:         opStr="SET"; break;
            case BitOp::SET_REF:     opStr="SET_REF"; break;
            case BitOp::ADD:         opStr="ADD"; break;
            case BitOp::ADD_REF:     opStr="ADD_REF"; break;
            case BitOp::SUB:         opStr="SUB"; break;
            case BitOp::SUB_REF:     opStr="SUB_REF"; break;
            case BitOp::MUL:         opStr="MUL"; break;
            case BitOp::MUL_REF:     opStr="MUL_REF"; break;
            case BitOp::DIV:         opStr="DIV"; break;
            case BitOp::DIV_REF:     opStr="DIV_REF"; break;
            case BitOp::EVENT:       opStr="EVENT"; break;
            case BitOp::BG:          opStr="BG"; break;
            case BitOp::BGM:         opStr="BGM"; break;
            case BitOp::LABEL:       opStr="LABEL"; break;
            case BitOp::TRANSITION:  opStr="TRANS"; break;
            case BitOp::UI_VISIBLE:  opStr="UI_VIS"; break;
            case BitOp::UI_LOAD:     opStr="UI_LOAD"; break;
            case BitOp::UI_UNLOAD:   opStr="UI_UNL"; break;
            case BitOp::UI_SET:      opStr="UI_SET"; break;
            case BitOp::UI_ACTIVATE: opStr="UI_ACT"; break;
            case BitOp::UI_DEACTIVATE: opStr="UI_DEA"; break;
            case BitOp::CALL:        opStr="CALL"; break;
            case BitOp::RETURN:      opStr="RET"; break;
            case BitOp::WAIT_INPUT:  opStr="W_INP"; break;
            case BitOp::WAIT_ACTION: opStr="W_ACT"; break;
            case BitOp::SET_LOCAL:   opStr="L_SET"; break;
            case BitOp::PLAY_TIMELINE: opStr="TLINE"; break;
            case BitOp::WAIT_EVENT:  opStr="W_EVT"; break;
            case BitOp::EMIT:        opStr="EMIT"; break;
            case BitOp::HALT:        opStr="HALT"; break;
            default: opStr="???"; break;
        }
        std::string args;
        for (const auto& a:ins.args){std::string cl=a;std::replace(cl.begin(),cl.end(),'\n',' ');args+=" "+(cl.size()>12?cl.substr(0,10)+"..":cl);}
        DrawText(TextFormat("%04d %-6s%s",i,opStr.c_str(),args.c_str()),px,dy,9,col); dy+=11;
    }
    if (!errors.empty()){DrawRectangle(0,sh-26,sw,26,Fade(RED,0.75f));DrawText(errors.back().c_str(),10,sh-20,11,WHITE);}
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
