#include "BitDebugger.hpp"
#include "BitRuntime.hpp"
#include "UILayout.hpp"
#include <iostream>
#include <iomanip>
#include <ctime>
#include <algorithm>

BitDebugger::BitDebugger() : m_debugTab(0), m_fpsHistoryIdx(0) {
    for (int i = 0; i < 100; ++i) m_fpsHistory[i] = 0;
}

void BitDebugger::Log(BitRuntime* engine, const std::string& msg, const std::string& level) {
    std::string mode = engine ? engine->GetConfigs().debug_mode : "debug_all";
    bool isError = (level == "ERROR");
    
    LogEntry entry = { msg, level, GetTimestamp() };
    m_consoleLogs.push_back(entry);
    if (m_consoleLogs.size() > 200) m_consoleLogs.erase(m_consoleLogs.begin());

    if (mode == "none" && !isError) return;
    std::string tag = "[BitEngine:" + level + "] ";
    if (isError || mode == "debug_overlay" || mode == "debug_all") std::cout << tag << msg << std::endl;
}

void BitDebugger::DrawOverlay(const DebugDrawContext& ctx) {
    if (!ctx.engine) return;

    int sw = GetScreenWidth(), sh = GetScreenHeight();
    
    int overlayW = std::min(1000, sw - 40);
    int overlayH = std::min(700, sh - 80);
    int startX   = (sw - overlayW) / 2;
    int startY   = (sh - overlayH) / 2;
    
    DrawRectangleRec({(float)startX, (float)startY, (float)overlayW, (float)overlayH}, Fade(BLACK, 0.88f));
    DrawRectangleLinesEx({(float)startX, (float)startY, (float)overlayW, (float)overlayH}, 1.0f, Fade(SKYBLUE, 0.3f));
    
    int headerH = 50;
    DrawRectangleGradientV(startX, startY, overlayW, headerH, Fade(SKYBLUE, 0.15f), Fade(BLACK, 0));
    DrawLineEx({(float)startX, (float)startY + headerH}, {(float)startX + overlayW, (float)startY + headerH}, 1.0f, Fade(SKYBLUE, 0.2f));
    
    const char* tabs[] = { "[1] DASH", "[2] EXEC", "[3] STATE", "[4] ASSETS", "[5] LOGS", "[6] SCHED" };
    int tabCount = 6;
    int tabW = overlayW / tabCount;
    for (int i = 0; i < tabCount; ++i) {
        Rectangle tabRect = {(float)(startX + i * tabW), (float)startY, (float)tabW, (float)headerH};
        bool hovered = CheckCollisionPointRec(GetMousePosition(), tabRect);
        
        if (m_debugTab == i) {
            DrawRectangleRec(tabRect, Fade(SKYBLUE, 0.15f));
            DrawRectangle((int)tabRect.x, (int)(tabRect.y + tabRect.height - 3), (int)tabRect.width, 3, SKYBLUE);
        } else if (hovered) {
            DrawRectangleRec(tabRect, Fade(WHITE, 0.05f));
        }
        
        Color tabCol = (m_debugTab == i) ? SKYBLUE : (hovered ? WHITE : GRAY);
        Vector2 textSz = MeasureTextEx(ctx.getFont(""), tabs[i], 12, 1);
        DrawTextEx(ctx.getFont(""), tabs[i], {tabRect.x + (tabW - textSz.x)/2, tabRect.y + (headerH - textSz.y)/2}, 12, 1, tabCol);
        
        if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) m_debugTab = i;
    }

    int footerH = 25;
    DrawRectangle(startX, startY + overlayH - footerH, overlayW, footerH, Fade(BLACK, 0.5f));
    DrawText("F3 to Close | TAB to cycle tabs | ESC to exit", startX + 15, startY + overlayH - 18, 10, Fade(GRAY, 0.8f));
    DrawText(TextFormat("BitEngine v0.3.1 | %dx%d", sw, sh), startX + overlayW - 160, startY + overlayH - 18, 10, Fade(GRAY, 0.8f));

    int contentX = startX + 20;
    int contentY = startY + headerH + 20;
    int contentW = overlayW - 40;
    int contentH = overlayH - headerH - footerH - 40;

    if (m_debugTab == 0) { // DASHBOARD
        int colW = contentW / 2 - 10;
        DrawText("PERFORMANCE MONITOR", contentX, contentY, 14, SKYBLUE);
        
        Rectangle graphRec = {(float)contentX, (float)contentY + 30, (float)colW, 120};
        DrawRectangleRec(graphRec, Fade(BLACK, 0.4f));
        DrawRectangleLinesEx(graphRec, 1, Fade(SKYBLUE, 0.2f));
        
        float maxMs = 33.3f;
        for (int i = 0; i < 100; ++i) {
            int idx = (m_fpsHistoryIdx + i) % 100;
            float val = m_fpsHistory[idx];
            float h = (val / maxMs) * graphRec.height;
            if (h > graphRec.height) h = graphRec.height;
            
            Color barCol = LIME;
            if (val > 16.6f) barCol = GOLD;
            if (val > 33.3f) barCol = ORANGE;
            
            DrawRectangle((int)graphRec.x + i * (colW/100), (int)(graphRec.y + graphRec.height - h), (colW/100) + 1, (int)h, Fade(barCol, 0.6f));
        }
        DrawLineEx({graphRec.x, graphRec.y + graphRec.height/2}, {graphRec.x + graphRec.width, graphRec.y + graphRec.height/2}, 1.0f, Fade(RED, 0.3f));
        DrawText("16.6ms (60 FPS)", (int)graphRec.x + 5, (int)(graphRec.y + graphRec.height/2 - 12), 9, Fade(RED, 0.5f));

        int dy = contentY + 165;
        DrawText(TextFormat("Current FPS:  %d", GetFPS()), contentX, dy, 11, RAYWHITE); dy += 16;
        DrawText(TextFormat("Frame Time:   %.2f ms", GetFrameTime() * 1000.0f), contentX, dy, 11, RAYWHITE); dy += 16;
        DrawText(TextFormat("Draw Calls:   %d (estimate)", 10 + (int)ctx.engine->GetActiveEntities().size() * 3), contentX, dy, 11, RAYWHITE); dy += 25;

        DrawText("QUICK STATS", contentX, dy, 14, SKYBLUE); dy += 25;
        DrawText(TextFormat("Active Speaker: %s", ctx.engine->GetCurrentEntity() ? ctx.engine->GetCurrentEntity()->name.c_str() : "None"), contentX, dy, 11, RAYWHITE); dy += 16;
        DrawText(TextFormat("Wait State:     %s", ctx.engine->GetWaitActionType().c_str()), contentX, dy, 11, RAYWHITE); dy += 16;
        DrawText(TextFormat("Current PC:     %d", ctx.engine->GetCurrentPC()), contentX, dy, 11, RAYWHITE); dy += 16;
        DrawText(TextFormat("Text Reveal:    %d / %d", ctx.engine->GetRevealedCount(), (int)ctx.engine->GetParsedContent().size()), contentX, dy, 11, RAYWHITE);

        int rx = contentX + colW + 20;
        int ry = contentY;
        DrawText("ENGINE HEALTH", rx, ry, 14, SKYBLUE); ry += 30;
        
        bool healthy = !ctx.engine->HasErrors();
        DrawCircle(rx + 10, ry + 7, 6, healthy ? LIME : RED);
        DrawText(healthy ? "SYSTEMS NOMINAL" : "ERRORS DETECTED", rx + 25, ry, 11, healthy ? LIME : RED); ry += 25;
        
        DrawText("Hot Reload Status:", rx, ry, 11, GRAY); ry += 16;
        DrawRectangleRec({(float)rx, (float)ry, 120, 20}, Fade(LIME, 0.15f));
        DrawText("WATCHING FILES", rx + 10, ry + 4, 10, LIME); ry += 35;

        DrawText("Active Background:", rx, ry, 11, GRAY); ry += 16;
        DrawText(ctx.engine->GetActiveBg().empty() ? "(none)" : ctx.engine->GetActiveBg().c_str(), rx, ry, 11, GOLD); ry += 25;
        
        DrawText("Active Audio:", rx, ry, 11, GRAY); ry += 16;
        DrawText(TextFormat("BGM: %s", ctx.engine->GetActiveBgm().empty() ? "NONE" : ctx.engine->GetActiveBgm().c_str()), rx, ry, 11, GREEN);
    } 
    else if (m_debugTab == 1) { // EXECUTION
        int leftW = contentW * 0.6f;
        DrawText("BYTECODE INSPECTOR", contentX, contentY, 14, PURPLE);
        
        Rectangle bcRec = {(float)contentX, (float)contentY + 25, (float)leftW, (float)contentH};
        DrawRectangleRec(bcRec, Fade(BLACK, 0.4f));
        DrawRectangleLinesEx(bcRec, 1, Fade(PURPLE, 0.2f));
        
        const auto& bc = ctx.engine->GetProject().bytecode;
        int curPC = ctx.engine->GetCurrentPC();
        int viewR = (int)(bcRec.height / 14);
        int startI = std::max(0, curPC - viewR / 2);
        int endI = std::min((int)bc.size(), startI + viewR);
        
        int bcy = bcRec.y + 10;
        for (int i = startI; i < endI; ++i) {
            const auto& ins = bc[i];
            bool isCurrent = (i == curPC);
            if (isCurrent) DrawRectangle((int)bcRec.x + 2, bcy - 1, (int)bcRec.width - 4, 13, Fade(GREEN, 0.2f));
            
            Color pcCol = isCurrent ? LIME : GRAY;
            DrawText(TextFormat("%04d", i), (int)bcRec.x + 10, bcy, 10, pcCol);
            
            std::string opStr = "OP_UNK";
            Color opCol = WHITE;
            switch(ins.op) {
                case BitOp::SAY: opStr = "SAY"; opCol = SKYBLUE; break;
                case BitOp::TEXT: opStr = "TEXT"; opCol = RAYWHITE; break;
                case BitOp::CHOICE: opStr = "CHOICE"; opCol = GOLD; break;
                case BitOp::IF: opStr = "IF"; opCol = ORANGE; break;
                case BitOp::GOTO: opStr = "GOTO"; opCol = VIOLET; break;
                case BitOp::SET: opStr = "SET"; opCol = LIME; break;
                case BitOp::ADD: opStr = "ADD"; opCol = LIME; break;
                case BitOp::CALL: opStr = "CALL"; opCol = PINK; break;
                case BitOp::RETURN: opStr = "RET"; opCol = PINK; break;
                default: break;
            }
            DrawText(opStr.c_str(), (int)bcRec.x + 50, bcy, 10, opCol);
            
            std::string args;
            for (const auto& a : ins.args) {
                std::string cl = a; std::replace(cl.begin(), cl.end(), '\n', ' ');
                args += " " + (cl.size() > 20 ? cl.substr(0, 18) + ".." : cl);
            }
            DrawText(args.c_str(), (int)bcRec.x + 110, bcy, 10, Fade(GRAY, 0.8f));
            bcy += 14;
        }

        int rx = contentX + leftW + 20;
        int ry = contentY;
        DrawText("CALL STACK", rx, ry, 14, PINK); ry += 25;
        const auto& stack = ctx.engine->GetCallStack();
        if (stack.empty()) DrawText("(empty)", rx, ry, 10, GRAY);
        for (int i = 0; i < (int)stack.size(); ++i) {
            DrawText(TextFormat("[%d] -> PC %d", i, stack[i]), rx, ry, 10, RAYWHITE); ry += 14;
        }
        
        ry += 15;
        DrawText("EVENT TRACE", rx, ry, 14, ORANGE); ry += 25;
        const auto& trace = ctx.engine->GetEventTrace();
        int maxT = 8, tStart = (int)trace.size() > maxT ? (int)trace.size() - maxT : 0;
        for (int i = tStart; i < (int)trace.size(); ++i) {
            const auto& t = trace[i];
            DrawText(TextFormat("%s: %s", t.op.c_str(), t.var.c_str()), rx, ry, 10, ORANGE);
            DrawText(TextFormat("  %d -> %d", t.old_value, t.new_value), rx, ry + 11, 9, Fade(ORANGE, 0.6f));
            ry += 26;
        }
    }
    else if (m_debugTab == 4) { // LOGS
        DrawText("ENGINE CONSOLE LOGS", contentX, contentY, 14, GOLD);
        
        Rectangle logRec = {(float)contentX, (float)contentY + 25, (float)contentW, (float)contentH};
        DrawRectangleRec(logRec, Fade(BLACK, 0.4f));
        DrawRectangleLinesEx(logRec, 1, Fade(GOLD, 0.2f));
        
        const auto& logs = m_consoleLogs;
        int ly = logRec.y + 10;
        int maxL = (int)(logRec.height / 15);
        int startL = std::max(0, (int)logs.size() - maxL);
        
        for (int i = startL; i < (int)logs.size(); ++i) {
            const auto& l = logs[i];
            Color col = RAYWHITE;
            if (l.level == "ERROR") col = RED;
            else if (l.level == "WARN") col = GOLD;
            
            DrawText(TextFormat("[%s] %s", l.level.c_str(), l.message.c_str()), (int)logRec.x + 10, ly, 10, col);
            ly += 15;
        }
        
        if (logs.empty()) DrawText("(no logs recorded)", (int)logRec.x + 10, (int)logRec.y + 10, 10, GRAY);
    }
    else if (m_debugTab == 2) { // STATE
        int leftW = contentW / 2 - 10;
        DrawText("GLOBAL VARIABLES", contentX, contentY, 14, LIME);
        
        Rectangle varRec = {(float)contentX, (float)contentY + 25, (float)leftW, (float)contentH};
        DrawRectangleRec(varRec, Fade(BLACK, 0.4f));
        DrawRectangleLinesEx(varRec, 1, Fade(LIME, 0.2f));
        
        int vy = varRec.y + 10;
        for (auto& [name, val] : ctx.engine->GetAllVariables()) {
            if (vy > varRec.y + varRec.height - 20) break;
            DrawText(name.c_str(), (int)varRec.x + 10, vy, 11, RAYWHITE);
            DrawText(TextFormat("%d", val), (int)varRec.x + leftW - 40, vy, 11, LIME);
            vy += 16;
        }

        int rx = contentX + leftW + 20;
        DrawText("ACTIVE ENTITIES", rx, contentY, 14, SKYBLUE);
        
        Rectangle entRec = {(float)rx, (float)contentY + 25, (float)leftW, (float)contentH};
        DrawRectangleRec(entRec, Fade(BLACK, 0.4f));
        DrawRectangleLinesEx(entRec, 1, Fade(SKYBLUE, 0.2f));
        
        int ey = entRec.y + 10;
        for (const auto& [id, st] : ctx.engine->GetActiveEntities()) {
            if (ey > entRec.y + entRec.height - 40) break;
            DrawText(TextFormat("ID: %s", id.c_str()), (int)entRec.x + 10, ey, 11, SKYBLUE);
            DrawText(st.visible ? "VISIBLE" : "HIDDEN", (int)entRec.x + leftW - 60, ey, 9, st.visible ? LIME : RED); ey += 14;
            DrawText(TextFormat(" Pos: %.2f | Alpha: %.2f | Expr: %s", st.currentNormX, st.alpha, st.expression.c_str()), (int)entRec.x + 15, ey, 10, GRAY);
            ey += 20;
        }
    }
    else if (m_debugTab == 3) { // ASSETS & UI
        int leftW = contentW / 2 - 10;
        DrawText("UI LAYOUT MANAGER", contentX, contentY, 14, SKYBLUE);
        
        Rectangle uiRec = {(float)contentX, (float)contentY + 25, (float)leftW, (float)contentH};
        DrawRectangleRec(uiRec, Fade(BLACK, 0.4f));
        
        if (ctx.uiManager) {
            auto layers = ctx.uiManager->GetLayerInfo();
            int uy = uiRec.y + 10;
            for (const auto& l : layers) {
                if (uy > uiRec.y + uiRec.height - 30) break;
                Color stateCol = l.active ? (l.visible ? GREEN : ORANGE) : GRAY;
                DrawRectangle((int)uiRec.x + 5, uy, 4, 12, stateCol);
                DrawText(TextFormat("Layer %d: %s", l.layer, l.name.c_str()), (int)uiRec.x + 15, uy, 11, RAYWHITE); uy += 14;
                DrawText(TextFormat("  %s", l.path.c_str()), (int)uiRec.x + 15, uy, 9, GRAY); uy += 18;
            }
        }

        int rx = contentX + leftW + 20;
        DrawText("ASSET CACHE AUDIT", rx, contentY, 14, LIME);
        int ry = contentY + 30;
        
        auto DrawCacheBar = [&](const char* label, int count, int max, Color col) {
            DrawText(TextFormat("%s: %d", label, count), rx, ry, 11, RAYWHITE); ry += 16;
            float barW = leftW - 20;
            DrawRectangle(rx, ry, (int)barW, 6, Fade(BLACK, 0.5f));
            float fill = std::min(1.0f, (float)count / (float)std::max(1, max));
            DrawRectangle(rx, ry, (int)(barW * fill), 6, col);
            ry += 15;
        };
        
        if (ctx.textureCache) DrawCacheBar("Textures", (int)ctx.textureCache->size(), 50, SKYBLUE);
        if (ctx.sfxCache)     DrawCacheBar("SFX Cache", (int)ctx.sfxCache->size(), 20, GOLD);
        if (ctx.musicCache)   DrawCacheBar("Music Streams", (int)ctx.musicCache->size(), 5, PINK);
        
        ry += 10;
        DrawText("CINEMATIC STATE", rx, ry, 14, GOLD); ry += 25;
        DrawText(TextFormat("UI Hidden:  %s", ctx.engine->IsUiHidden() ? "TRUE" : "FALSE"), rx, ry, 11, RAYWHITE); ry += 16;
        DrawText(TextFormat("Auto-Play:  %s", ctx.engine->IsAutoPlaying() ? "ON" : "OFF"), rx, ry, 11, RAYWHITE); ry += 16;
        DrawText(TextFormat("Screen Fade: %.2f", ctx.engine->GetScreenFadeAlpha()), rx, ry, 11, RAYWHITE); ry += 16;
        DrawText(TextFormat("Shake Mag:   %.1f", ctx.engine->GetEffectShake()), rx, ry, 11, RAYWHITE);
    }
    else if (m_debugTab == 5) { // SCHEDULER
        DrawText("ACTIVE TASK GRAPH", contentX, contentY, 14, {180, 140, 255, 255});
        
        if (!ctx.scheduler) {
            DrawText("(scheduler not available)", contentX, contentY + 30, 10, GRAY);
        } else {
            const auto& tasks = ctx.scheduler->GetTasks();
            size_t activeCount = tasks.size();

            // Summary header
            Color summaryCol = activeCount > 0 ? GOLD : LIME;
            DrawText(TextFormat("Active Tasks: %d", (int)activeCount), contentX, contentY + 22, 11, summaryCol);

            // VM Execution Status
            std::string waitType = ctx.engine->GetWaitActionType();
            if (!waitType.empty()) {
                DrawText("VM STATUS:", contentX + 160, contentY + 22, 11, GRAY);
                DrawText(TextFormat("WAITING FOR [%s]", waitType.c_str()), contentX + 230, contentY + 22, 11, PINK);
            } else {
                DrawText("VM STATUS:", contentX + 160, contentY + 22, 11, GRAY);
                DrawText("RUNNING / IDLE", contentX + 230, contentY + 22, 11, LIME);
            }

            if (activeCount == 0) {
                Rectangle idleBox = {(float)contentX, (float)contentY + 50, (float)contentW, 60};
                DrawRectangleRec(idleBox, Fade(BLACK, 0.3f));
                DrawRectangleLinesEx(idleBox, 1, Fade(LIME, 0.2f));
                DrawText("Scheduler Idle — no async tasks running.", contentX + 20, contentY + 70, 11, Fade(LIME, 0.7f));
            } else {
                // Task list area
                Rectangle listRec = {(float)contentX, (float)contentY + 45, (float)contentW, (float)contentH - 10};
                DrawRectangleRec(listRec, Fade(BLACK, 0.35f));
                DrawRectangleLinesEx(listRec, 1, Fade({180, 140, 255, 255}, 0.25f));

                // Column headers
                int hx = listRec.x + 10;
                int hy = listRec.y + 8;
                DrawText("OWNER",    hx,        hy, 10, GRAY);
                DrawText("TAGS",     hx + 160,  hy, 10, GRAY);
                DrawText("PROGRESS", hx + 290,  hy, 10, GRAY);
                DrawLine(listRec.x + 5, hy + 14, listRec.x + listRec.width - 5, hy + 14, Fade(GRAY, 0.3f));

                // Helper: tag name + color
                auto TagInfo = [](uint32_t tags, std::string& label, Color& col) {
                    if      (tags & TAG_MOVE)     { label = "MOVE";  col = SKYBLUE; }
                    else if (tags & TAG_FADE)     { label = "FADE";  col = {180, 140, 255, 255}; }
                    else if (tags & TAG_SHAKE)    { label = "SHAKE"; col = ORANGE; }
                    else if (tags & TAG_UI)       { label = "UI";    col = GOLD; }
                    else if (tags & TAG_AUDIO)    { label = "AUDIO"; col = GREEN; }
                    else if (tags & TAG_DELAY)    { label = "DELAY"; col = GRAY; }
                    else if (tags & TAG_TIMELINE) { label = "TL";    col = PINK; }
                    else                          { label = "MISC";  col = RAYWHITE; }
                    // Secondary tag
                    if ((tags & TAG_MOVE)  && (tags & TAG_FADE))  label = "MOVE+FADE";
                };

                int ty = hy + 20;
                int rowH = 28;
                int maxRows = (int)((listRec.height - 30) / rowH);
                int shown = std::min((int)activeCount, maxRows);

                for (int i = 0; i < shown; ++i) {
                    const auto& task = tasks[i];
                    int rx = listRec.x + 10;
                    int ry = ty + i * rowH;

                    // Alternate row tint
                    if (i % 2 == 0)
                        DrawRectangle(listRec.x + 2, ry - 2, listRec.width - 4, rowH - 2, Fade(WHITE, 0.03f));

                    // Owner label
                    std::string owner = task->GetOwner().empty() ? "(anon)" : task->GetOwner();
                    if (owner.size() > 18) owner = owner.substr(0, 16) + "..";
                    DrawText(owner.c_str(), rx, ry + 4, 10, RAYWHITE);

                    // Tag pill
                    std::string tagLabel; Color tagCol;
                    TagInfo(task->GetTags(), tagLabel, tagCol);
                    Rectangle pill = {(float)(rx + 150), (float)(ry + 2), (float)(tagLabel.size() * 6 + 10), 16};
                    DrawRectangleRec(pill, Fade(tagCol, 0.25f));
                    DrawRectangleLinesEx(pill, 1.0f, Fade(tagCol, 0.6f));
                    DrawText(tagLabel.c_str(), (int)pill.x + 5, (int)pill.y + 3, 9, tagCol);

                    // Progress bar
                    int barX = rx + 280;
                    int barW = contentW - 300;
                    int barY = ry + 6;
                    DrawRectangle(barX, barY, barW, 8, Fade(BLACK, 0.5f));

                    float progress = task->GetProgress();
                    int fillW = (int)(barW * std::max(0.0f, std::min(1.0f, progress)));
                    
                    if (fillW > 0) {
                        DrawRectangleGradientH(barX, barY, fillW, 8,
                            Fade(tagCol, 0.4f), Fade(tagCol, 0.85f));
                    }
                    DrawRectangleLinesEx({(float)barX, (float)barY, (float)barW, 8.0f}, 1, Fade(tagCol, 0.3f));
                }

                if ((int)activeCount > maxRows) {
                    DrawText(TextFormat("+%d more tasks...", (int)activeCount - maxRows),
                        contentX + 10, contentY + contentH - 5, 10, Fade(GRAY, 0.7f));
                }
            }
        }
    }

    if (ctx.engine->HasErrors()) {
        DrawRectangle(startX, startY + overlayH - footerH - 30, overlayW, 30, Fade(RED, 0.8f));
        DrawText(TextFormat("LATEST ERROR: %s", ctx.engine->GetErrors().back().c_str()), startX + 15, startY + overlayH - footerH - 22, 11, WHITE);
    }
}

std::string BitDebugger::GetTimestamp() const {
    time_t now = time(nullptr);
    tm* ltm = localtime(&now);
    char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ltm);
    return std::string(buf);
}
