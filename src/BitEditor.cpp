#include "headers/BitEditor.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <cmath>

namespace fs = std::filesystem;

// ── Constructor ──────────────────────────────────────────────
BitEditor::BitEditor(DialogEngine& engine) : m_engine(engine) {
    m_bgColor      = {10, 10, 14, 255};
    m_panelColor   = {18, 20, 30, 255};
    m_panelLight   = {28, 32, 46, 255};
    m_accentColor  = {0, 200, 255, 255};
    m_textColor    = {215, 225, 245, 255};
    m_dimColor     = {110, 122, 145, 255};
    m_errorColor   = {255, 65, 65, 255};
    m_successColor = {60, 220, 100, 255};
    m_warnColor    = {255, 185, 0, 255};
    m_camera.zoom   = 1.0f;
    m_camera.offset = {(float)GetScreenWidth()/2, (float)GetScreenHeight()/2};
    ScanForProjects();
}

// ── Theme helpers ─────────────────────────────────────────────
Color BitEditor::Hover(Color c, bool h) const {
    if (!h) return c;
    return {(unsigned char)std::min(255,(int)c.r+35),
            (unsigned char)std::min(255,(int)c.g+35),
            (unsigned char)std::min(255,(int)c.b+35), c.a};
}
Color BitEditor::Lerp(Color a, Color b, float t) const {
    return {(unsigned char)(a.r+(b.r-a.r)*t),(unsigned char)(a.g+(b.g-a.g)*t),
            (unsigned char)(a.b+(b.b-a.b)*t),(unsigned char)(a.a+(b.a-a.a)*t)};
}

// ── Notifications ─────────────────────────────────────────────
void BitEditor::PushNotif(const std::string& msg, Color col, float ttl) {
    m_notifs.push_back({msg, col, ttl});
}

// ── Node helpers ──────────────────────────────────────────────
float BitEditor::GetNodeHeight(const DialogNode& dn) const {
    return 102.f + (float)dn.options.size() * 26.f;
}
std::string BitEditor::GetNodeType(const DialogNode& dn) const {
    return dn.metadata.count("__type") ? dn.metadata.at("__type") : "dialog";
}
Color BitEditor::GetNodeTypeColor(const std::string& type) const {
    if (type == "choice")  return {170, 70, 255, 255};
    if (type == "event")   return {255, 145, 0, 255};
    if (type == "start")   return {60, 220, 100, 255};
    if (type == "end")     return {255, 65, 65, 255};
    if (type == "comment") return {150, 150, 150, 255};
    return m_accentColor; // dialog
}

// ── Project scanning ──────────────────────────────────────────
void BitEditor::ScanForProjects() {
    m_projects.clear();
    try {
        for (const auto& e : fs::directory_iterator(".")) {
            if (!e.is_directory()) continue;
            std::string p = e.path().string() + "/project.json";
            if (!fs::exists(p)) continue;
            try {
                std::ifstream f(p); nlohmann::json j; f >> j;
                m_projects.push_back({
                    j.value("name", e.path().filename().string()),
                    e.path().string(),
                    j.value("config", "res/configs.json")
                });
            } catch(...) {}
        }
    } catch(...) {}
}

bool BitEditor::LoadProjectManifest(const std::string& path) {
    std::string p = path + "/project.json";
    if (!fs::exists(p)) return false;
    try {
        std::ifstream f(p); nlohmann::json j; f >> j;
        m_activeProject = {j.value("name","Project"), path, j.value("config","res/configs.json")};
        m_projectPath = path;
        m_mode = Mode::ProjectDetail;
        m_engine.LoadProject(m_projectPath + "/" + m_activeProject.configFile);
        LoadProjectFiles();
        
        m_varRenameBuf.clear(); m_varInitBuf.clear();
        m_varMinBuf.clear();    m_varMaxBuf.clear();
        m_entPosBuf.clear();    m_spriteFrameBuf.clear(); // Clear entity buffers
        
        // BitEngine legacy parser doesn't read default_pos_x inline. Read it manually:
        try {
            std::ifstream cf(m_projectPath + "/" + m_activeProject.configFile);
            nlohmann::json cj; cf >> cj;
            if (cj.contains("entities")) {
                for (auto& [id, entJ] : cj["entities"].items()) {
                    if (m_engine.GetProject().entities.count(id) && entJ.contains("default_pos_x"))
                        m_engine.GetProject().entities[id].default_pos_x = entJ["default_pos_x"].get<float>();
                }
            }
        } catch(...) {}

        for (auto& [id, val] : m_engine.GetProject().variables) {
            m_varRenameBuf[id] = id;
            m_varInitBuf[id]   = std::to_string(val.initial_value);
            m_varMinBuf[id]    = val.min ? std::to_string(*val.min) : "";
            m_varMaxBuf[id]    = val.max ? std::to_string(*val.max) : "";
        }
        return true;
    } catch(...) { return false; }
}

void BitEditor::LoadProjectFiles() {
    m_dialogFiles.clear();
    for (const auto& f : m_engine.GetProject().configs.dialog_files)
        m_dialogFiles.push_back(f);
}

void BitEditor::CreateNewProject(const std::string& name) {
    if (name.empty()) { PushNotif("Name cannot be empty", m_errorColor); return; }
    std::string path = "./" + name;
    if (fs::exists(path)) { PushNotif("Project already exists", m_errorColor); return; }
    fs::create_directories(path + "/res");
    nlohmann::json pj; pj["name"] = name; pj["config"] = "res/configs.json";
    std::ofstream(path + "/project.json") << pj.dump(4);
    nlohmann::json cj;
    cj["configs"]      = {{"start_node","start"},{"reveal_speed",45.0}};
    cj["entities"]     = nlohmann::json::object();
    cj["variables"]    = nlohmann::json::object();
    cj["dialogs"]      = nlohmann::json::array();
    std::ofstream(path + "/res/configs.json") << cj.dump(4);
    ScanForProjects();
    PushNotif("Created '" + name + "'", m_successColor);
    m_newProjName = "";
}

void BitEditor::CreateNewDialog(const std::string& name) {
    if (name.empty()) { PushNotif("Name cannot be empty", m_errorColor); return; }
    std::string fn = name + (name.find(".json") == std::string::npos ? ".json" : "");
    std::string fp = m_projectPath + "/res/" + fn;
    if (fs::exists(fp)) { PushNotif("File already exists", m_errorColor); return; }
    nlohmann::json dj = nlohmann::json::object();
    std::ofstream(fp) << dj.dump(4);
    auto& df = m_engine.GetProject().configs.dialog_files;
    if (std::find(df.begin(), df.end(), fn) == df.end()) {
        df.push_back(fn); SaveGlobalConfigs(); LoadProjectFiles();
    }
    PushNotif("Created " + fn, m_successColor);
    m_newFileNameBuf = "";
}

void BitEditor::SaveGlobalConfigs() {
    auto& p = m_engine.GetProject();
    nlohmann::json j;
    // Load existing to preserve other configs not edited by BitEditor
    try { std::ifstream f(m_projectPath + "/" + m_activeProject.configFile); if (f) f >> j; } catch(...) {}
    
    if (!j.contains("configs")) j["configs"] = nlohmann::json::object();
    j["configs"]["start_node"] = p.configs.start_node;
    j["configs"]["reveal_speed"] = p.configs.reveal_speed;
    j["configs"]["debug_mode"] = p.configs.debug_mode;

    nlohmann::json ej = nlohmann::json::object();
    for (auto& [id, ent] : p.entities) {
        nlohmann::json entJ;
        entJ["name"] = ent.name; entJ["type"] = ent.type;
        entJ["default_pos_x"] = ent.default_pos_x;
        nlohmann::json sj = nlohmann::json::object();
        for (auto& [sid, sd] : ent.sprites) {
            sj[sid] = {{"path",sd.path},{"frames",sd.frames},{"speed",sd.speed},{"scale",sd.scale}};
        }
        entJ["sprites"] = sj; ej[id] = entJ;
    }
    j["entities"] = ej;
    nlohmann::json vj = nlohmann::json::object();
    for (auto& [id, val] : p.variables) {
        nlohmann::json vo; vo["initial"] = val.initial_value; // BitEngine relies on "initial", not "initial_value"
        if (val.min) vo["min"] = *val.min;
        if (val.max) vo["max"] = *val.max;
        vj[id] = vo;
    }
    j["variables"] = vj;
    nlohmann::json dfj = nlohmann::json::array();
    for (auto& f : p.configs.dialog_files) dfj.push_back(f);
    j["dialogs"] = dfj;
    std::ofstream(m_projectPath + "/" + m_activeProject.configFile) << j.dump(4);
    PushNotif("Configs saved", m_successColor);
}

// ── Graph logic ───────────────────────────────────────────────
void BitEditor::OpenGraph(const std::string& filename) {
    m_currentFile = filename;
    m_graphNodes.clear();
    m_engine.GetProject().nodes.clear();
    m_selectedNode = ""; m_linkingSource = ""; m_linkingSourceOptIdx = -1;
    DialogParser::LoadDialogFile(m_projectPath + "/res/" + filename, m_engine.GetProject());
    int i = 0;
    for (auto& [id, node] : m_engine.GetProject().nodes) {
        GraphNode gn; gn.id = id;
        if (node.metadata.count("__x"))
            gn.pos = {(float)atof(node.metadata.at("__x").c_str()),
                      (float)atof(node.metadata.at("__y").c_str())};
        else gn.pos = {(i%5)*310.f, (i/5)*200.f};
        m_graphNodes[id] = gn; i++;
    }
    m_mode = Mode::Graph; m_camera.target = {0,0}; m_camera.zoom = 1.0f; m_dirty = false;
}

void BitEditor::CreateNewNode(Vector2 pos, const std::string& type) {
    std::string newId = "node_" + std::to_string(m_engine.GetProject().nodes.size());
    while (m_engine.GetProject().nodes.count(newId)) newId += "_";
    DialogNode n; n.entity = "unk"; n.content = "New text...";
    n.metadata["__type"] = type;
    m_engine.GetProject().nodes[newId] = n;
    GraphNode gn; gn.id = newId; gn.pos = pos;
    m_graphNodes[newId] = gn; m_selectedNode = newId; m_dirty = true;
}

void BitEditor::DeleteNode(const std::string& id) {
    m_engine.GetProject().nodes.erase(id);
    m_graphNodes.erase(id);
    for (auto& [nid, n] : m_engine.GetProject().nodes) {
        if (n.next_id && *n.next_id == id) n.next_id = std::nullopt;
        for (auto& opt : n.options) if (opt.next_id == id) opt.next_id = "";
    }
    if (m_selectedNode == id) m_selectedNode = "";
    m_dirty = true;
    PushNotif("Deleted: " + id, m_errorColor);
}

void BitEditor::SaveCurrentGraph() {
    try {
        nlohmann::json j = nlohmann::json::object();
        for (auto& [id, n] : m_engine.GetProject().nodes) {
            nlohmann::json nj;
            nj["entity"]  = n.entity; nj["content"] = n.content;
            nj["next_id"] = n.next_id ? nlohmann::json(*n.next_id) : nlohmann::json(nullptr);
            nj["metadata"] = n.metadata;
            nj["metadata"]["__x"] = std::to_string(m_graphNodes.count(id) ? m_graphNodes[id].pos.x : 0);
            nj["metadata"]["__y"] = std::to_string(m_graphNodes.count(id) ? m_graphNodes[id].pos.y : 0);
            
            nlohmann::json evts = nlohmann::json::array();
            for (auto& e : n.events) evts.push_back({{"op", e.op}, {"var", e.var}, {"value", e.value}});
            if (!evts.empty()) nj["events"] = evts;

            nlohmann::json opts = nlohmann::json::array();
            for (auto& opt : n.options) {
                nlohmann::json oj;
                oj["content"] = opt.content;
                oj["next_id"] = opt.next_id;
                oj["style"] = opt.style;
                nlohmann::json cArr = nlohmann::json::array();
                for (auto& c : opt.conditions) cArr.push_back({{"op", c.op}, {"var", c.var}, {"value", c.value}});
                if (!cArr.empty()) oj["conditions"] = cArr;
                nlohmann::json eArr = nlohmann::json::array();
                for (auto& e : opt.events) eArr.push_back({{"op", e.op}, {"var", e.var}, {"value", e.value}});
                if (!eArr.empty()) oj["events"] = eArr;
                opts.push_back(oj);
            }
            if (!opts.empty()) nj["options"] = opts;
            j[id] = nj;
        }
        std::ofstream(m_projectPath + "/res/" + m_currentFile) << j.dump(4);
        m_dirty = false;
        PushNotif("Graph saved!", m_successColor);
    } catch(...) { PushNotif("Save failed!", m_errorColor); }
}

// ── Update ────────────────────────────────────────────────────
void BitEditor::Update() {
    m_animTime += GetFrameTime();
    for (auto& n : m_notifs) n.ttl -= GetFrameTime();
    m_notifs.erase(std::remove_if(m_notifs.begin(), m_notifs.end(),
        [](const Notification& n){ return n.ttl <= 0; }), m_notifs.end());

    // Close dropdown on Escape
    if (IsKeyPressed(KEY_ESCAPE)) { m_dropdownOpen = false; m_activeInputUid = ""; }

    if (m_mode != Mode::Graph) return;

    // Zoom
    float wheel = GetMouseWheelMove();
    if (wheel != 0 && !m_dropdownOpen) {
        Vector2 mw = GetScreenToWorld2D(GetMousePosition(), m_camera);
        m_camera.offset = GetMousePosition(); m_camera.target = mw;
        m_camera.zoom   = std::clamp(m_camera.zoom + wheel * 0.1f, 0.15f, 4.0f);
    }
    // Pan
    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
        Vector2 d = GetMouseDelta();
        m_camera.target.x -= d.x / m_camera.zoom;
        m_camera.target.y -= d.y / m_camera.zoom;
    }

    Vector2 mouse      = GetMousePosition();
    Vector2 worldMouse = GetScreenToWorld2D(mouse, m_camera);
    bool    hit        = false;

    float inspectorX  = (float)GetScreenWidth() - 390.f;
    bool  overUI      = (!m_selectedNode.empty() && mouse.x >= inspectorX)
                        || mouse.y < 50.f;

    if (!overUI) {
        // ── Option output pins ──
        for (auto& [id, gn] : m_graphNodes) {
            auto& dn = m_engine.GetProject().nodes[id];
            for (int i = 0; i < (int)dn.options.size(); i++) {
                float pinY = gn.pos.y + 102.f + i * 26.f + 13.f;
                if (CheckCollisionPointCircle(worldMouse, {gn.pos.x+280.f, pinY}, 9.f)
                    && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    m_linkingSource = id; m_linkingSourceOptIdx = i;
                    hit = true; break;
                }
            }
            if (hit) break;
        }
        // ── Main output pin ──
        if (!hit) {
            for (auto& [id, gn] : m_graphNodes) {
                if (CheckCollisionPointCircle(worldMouse, {gn.pos.x+280.f, gn.pos.y+60.f}, 9.f)
                    && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    m_linkingSource = id; m_linkingSourceOptIdx = -1;
                    hit = true; break;
                }
            }
        }
        // ── Drag nodes ──
        if (m_linkingSource.empty()) {
            for (auto& [id, gn] : m_graphNodes) {
                float nh = GetNodeHeight(m_engine.GetProject().nodes[id]);
                Rectangle r = {gn.pos.x, gn.pos.y, 280.f, nh};
                if (gn.dragging) {
                    gn.pos.x = worldMouse.x - gn.dragOffset.x;
                    gn.pos.y = worldMouse.y - gn.dragOffset.y;
                    hit = true; m_dirty = true;
                    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) gn.dragging = false;
                } else if (CheckCollisionPointRec(worldMouse, r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    gn.dragging   = true;
                    gn.dragOffset = {worldMouse.x - gn.pos.x, worldMouse.y - gn.pos.y};
                    m_selectedNode = id; hit = true;
                }
            }
        }
        // ── Finish link ──
        if (!m_linkingSource.empty() && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            for (auto& [id, gn] : m_graphNodes) {
                float nh = GetNodeHeight(m_engine.GetProject().nodes[id]);
                if (CheckCollisionPointRec(worldMouse, {gn.pos.x, gn.pos.y, 280.f, nh})
                    && id != m_linkingSource) {
                    auto& src = m_engine.GetProject().nodes[m_linkingSource];
                    if (m_linkingSourceOptIdx == -1) src.next_id = id;
                    else if (m_linkingSourceOptIdx < (int)src.options.size())
                        src.options[m_linkingSourceOptIdx].next_id = id;
                    m_dirty = true; break;
                }
            }
            m_linkingSource = ""; m_linkingSourceOptIdx = -1;
        }
        // Deselect on empty canvas click
        if (!hit && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            m_selectedNode = "";
    }

    // Shortcuts
    if (IsKeyPressed(KEY_A) && m_activeInputUid.empty())
        CreateNewNode(worldMouse);
    if ((IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_S))
        SaveCurrentGraph();
    if (IsKeyPressed(KEY_DELETE) && !m_selectedNode.empty() && m_activeInputUid.empty())
        DeleteNode(m_selectedNode);
}

// ── Draw dispatcher ───────────────────────────────────────────
void BitEditor::Draw() {
    m_dropdownPending.active = false; // reset each frame
    switch (m_mode) {
        case Mode::ProjectHub:    DrawProjectHub();    break;
        case Mode::ProjectDetail: DrawProjectDetail(); break;
        case Mode::ConfigEditor:  DrawConfigEditor();  break;
        case Mode::EntityEditor:  DrawEntityEditor();  break;
        case Mode::VarEditor:     DrawVarEditor();     break;
        case Mode::Graph:         DrawGraphMode();     break;
    }
    DrawNotifications();       // above mode content
    DrawDropdownOverlay();     // LAST — always on top of everything
}

// ── GuiSeparator ─────────────────────────────────────────────
void BitEditor::GuiSeparator(float x, float y, float w) {
    DrawRectangle((int)x, (int)y, (int)w, 1, Fade(m_accentColor, 0.18f));
}

// ── GuiButton ─────────────────────────────────────────────────
bool BitEditor::GuiButton(Rectangle r, const char* text, Color bg, Color fg, bool enabled) {
    if (!enabled) {
        DrawRectangleRounded(r, 0.12f, 8, Fade(bg, 0.25f));
        int tw = MeasureText(text, 14);
        DrawText(text, (int)(r.x + (r.width-tw)/2), (int)(r.y + (r.height-14)/2), 14, Fade(fg,0.3f));
        return false;
    }
    bool hover   = CheckCollisionPointRec(GetMousePosition(), r);
    bool pressed = hover && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    bool clicked = hover && IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
    Color drawBg = pressed ? Lerp(bg, {0,0,0,255}, 0.25f) : (hover ? Hover(bg, true) : bg);
    DrawRectangleRounded(r, 0.12f, 8, drawBg);
    if (hover) DrawRectangleRoundedLinesEx(r, 0.12f, 8, 1.5f, Fade(WHITE, 0.12f));
    int tw = MeasureText(text, 14);
    DrawText(text, (int)(r.x + (r.width-tw)/2), (int)(r.y + (r.height-14)/2), 14, fg);
    return clicked;
}

// ── GuiDropdown (button only — panel is deferred to DrawDropdownOverlay) ──────
bool BitEditor::GuiDropdown(Rectangle r, const std::string& current,
    const std::vector<std::string>& opts, std::string& out, const std::string& uid)
{
    bool open  = (m_dropdownOpen && m_dropdownUid == uid);
    bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    // Draw the selector button
    Color border = open ? m_accentColor : Fade(m_accentColor, 0.3f);
    DrawRectangleRounded(r, 0.1f, 8, hover ? m_panelLight : m_panelColor);
    DrawRectangleRoundedLinesEx(r, 0.1f, 8, 1.5f, border);
    DrawText(current.c_str(), (int)(r.x+10), (int)(r.y+(r.height-14)/2), 14, WHITE);
    DrawText(open ? "v" : ">", (int)(r.x+r.width-18), (int)(r.y+(r.height-14)/2), 14, m_dimColor);
    if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        m_dropdownOpen = !open;
        m_dropdownUid  = uid;
        open = m_dropdownOpen && (m_dropdownUid == uid);
    }
    // Register deferred overlay draw (panel rendered LAST so it's on top)
    if (open) {
        float oh = 30.f;
        Rectangle panelRect = {r.x, r.y + r.height + 3.f, r.width, oh * (float)opts.size()};
        m_dropdownPending = {true, r, panelRect, opts, current, uid, &out};
    }
    return false; // actual selection fires inside DrawDropdownOverlay()
}

// ── GuiInputText ──────────────────────────────────────────────
bool BitEditor::GuiInputText(Rectangle r, std::string& text, const char* hint,
    const std::string& uid, int fontSize, bool multiline)
{
    Vector2 mouse  = GetMousePosition();
    bool wasActive = (m_activeInputUid == uid);
    if (CheckCollisionPointRec(mouse, r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (!wasActive) m_inputState = {};
        m_activeInputUid = uid;
        m_inputState.cursorPos = (int)text.size();
        wasActive = true;
    }
    if (!CheckCollisionPointRec(mouse, r) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && wasActive) {
        m_activeInputUid = ""; wasActive = false;
    }
    bool active = wasActive;
    Color bg     = active ? Color{22,28,44,255} : Color{14,16,24,255};
    Color border = active ? m_accentColor : Fade(m_accentColor, 0.22f);
    DrawRectangleRounded(r, 0.1f, 8, bg);
    DrawRectangleRoundedLinesEx(r, 0.1f, 8, active ? 1.8f : 1.0f, border);
    if (active) {
        float dt = GetFrameTime();
        m_inputState.blinkTimer += dt;
        int key = GetCharPressed();
        while (key > 0) {
            if (key >= 32 && key <= 126) {
                text.insert(text.begin() + m_inputState.cursorPos, (char)key);
                m_inputState.cursorPos++;
            }
            key = GetCharPressed();
        }
        if (multiline && IsKeyPressed(KEY_ENTER)) {
            text.insert(text.begin() + m_inputState.cursorPos, '\n');
            m_inputState.cursorPos++;
        }
        if (IsKeyDown(KEY_BACKSPACE)) {
            m_inputState.backTimer += dt;
            bool trig = IsKeyPressed(KEY_BACKSPACE) ||
                (m_inputState.backTimer > 0.5f &&
                 fmod(m_inputState.backTimer-0.5f, 0.05f) > fmod(m_inputState.backTimer-0.5f-dt, 0.05f));
            if (trig && m_inputState.cursorPos > 0) {
                text.erase(text.begin() + m_inputState.cursorPos - 1);
                m_inputState.cursorPos--;
            }
        } else m_inputState.backTimer = 0;
        if (IsKeyPressed(KEY_LEFT)  && m_inputState.cursorPos > 0) m_inputState.cursorPos--;
        if (IsKeyPressed(KEY_RIGHT) && m_inputState.cursorPos < (int)text.size()) m_inputState.cursorPos++;
        if (IsKeyPressed(KEY_HOME)) m_inputState.cursorPos = 0;
        if (IsKeyPressed(KEY_END))  m_inputState.cursorPos = (int)text.size();
        if (!multiline && IsKeyPressed(KEY_ENTER)) m_activeInputUid = "";
    }
    BeginScissorMode((int)r.x+4, (int)r.y+2, (int)r.width-8, (int)r.height-4);
    // Vertical center for single-line, top-pad for multiline
    int py = multiline ? (int)r.y + 6 : (int)(r.y + (r.height - fontSize) * 0.5f);
    int px = (int)r.x + 8;
    if (text.empty() && !active) {
        DrawText(hint, px, py, fontSize, Fade(m_dimColor, 0.65f));
    } else if (multiline) {
        std::string line; float ly = py;
        for (int i = 0; i <= (int)text.size(); i++) {
            if (i == (int)text.size() || text[i] == '\n') {
                DrawText(line.c_str(), px, (int)ly, fontSize, WHITE);
                line = ""; ly += fontSize + 3;
            } else line += text[i];
        }
    } else {
        int fullW = MeasureText(text.c_str(), fontSize);
        int maxVis = (int)r.width - 16;
        int scroll = std::max(0, fullW - maxVis);
        DrawText(text.c_str(), px - scroll, py, fontSize, WHITE);
    }
    // Cursor blink
    if (active && (int)(m_inputState.blinkTimer * 2) % 2 == 0) {
        std::string before = text.substr(0, m_inputState.cursorPos);
        int cx = px + MeasureText(before.c_str(), fontSize);
        if (!multiline) {
            int fullW = MeasureText(text.c_str(), fontSize);
            cx -= std::max(0, fullW - (int)(r.width - 16));
        }
        DrawRectangle(cx, py, 2, fontSize, m_accentColor);
    }
    EndScissorMode();
    return active;
}

// ── DrawProjectHub ────────────────────────────────────────────
void BitEditor::DrawProjectHub() {
    int W = GetScreenWidth(), H = GetScreenHeight();

    // Background subtle grid
    for (int i = 0; i < W; i += 40)
        DrawLine(i, 0, i, H, Fade(m_accentColor, 0.02f));
    for (int j = 0; j < H; j += 40)
        DrawLine(0, j, W, j, Fade(m_accentColor, 0.02f));

    // Header
    DrawRectangleGradientV(0, 0, W, 90, {14,16,26,255}, m_bgColor);
    DrawRectangle(0, 88, W, 2, Fade(m_accentColor, 0.3f));

    // Title with pseudo-glow
    const char* title = "BITENGINE STUDIO";
    int tsx = 36, tsy = 20, tsz = 36;
    for (int g = 3; g >= 1; g--)
        DrawText(title, tsx, tsy, tsz, Fade(m_accentColor, 0.06f * g));
    DrawText(title, tsx, tsy, tsz, m_accentColor);
    DrawText("Dialog System Editor", tsx+2, tsy+42, 14, m_dimColor);

    // Animated accent dot
    float pulse = 0.6f + 0.4f * sinf(m_animTime * 3.f);
    DrawCircle(W-30, 30, 8, Fade(m_accentColor, pulse));
    DrawCircle(W-30, 30, 4, m_accentColor);

    // Projects label
    int listY = 112;
    DrawText("RECENT PROJECTS", 36, listY, 11, m_dimColor);
    DrawRectangle(36, listY+14, W-72, 1, Fade(m_accentColor, 0.15f));
    listY += 22;

    if (m_projects.empty()) {
        DrawText("No projects found. Create one below.", 36, listY+20, 16, m_dimColor);
    } else {
        float cx = 36, cy = listY, cw = 220.f, ch = 130.f, gap = 16.f;
        int idx = 0;
        for (auto& proj : m_projects) {
            Rectangle card = {cx, cy, cw, ch};
            bool hover = CheckCollisionPointRec(GetMousePosition(), card);
            float glow = hover ? 0.55f : 0.2f;
            // Shadow
            DrawRectangleRounded({cx+4,cy+4,cw,ch}, 0.12f, 8, Fade(BLACK, 0.4f));
            // Card
            DrawRectangleRounded(card, 0.12f, 8,
                hover ? Color{26,30,48,255} : m_panelColor);
            DrawRectangleRoundedLinesEx(card, 0.12f, 8,
                hover ? 2.f : 1.f, Fade(m_accentColor, glow));
            // Color strip top
            DrawRectangle((int)cx, (int)cy, (int)cw, 4, Lerp(m_accentColor,{0,0,0,255},0.3f));
            // Icon placeholder
            DrawRectangle((int)cx+12, (int)cy+14, 34, 28, Fade(m_accentColor, 0.12f));
            DrawText("BS", (int)cx+18, (int)cy+18, 16, Fade(m_accentColor, 0.9f));
            // Project name
            DrawText(proj.name.c_str(), (int)cx+12, (int)cy+52, 17, WHITE);
            // Path
            std::string shortPath = proj.path.size() > 26
                ? "..." + proj.path.substr(proj.path.size()-24) : proj.path;
            DrawText(shortPath.c_str(), (int)cx+12, (int)cy+74, 11, m_dimColor);
            // Open button appears on hover
            if (hover) {
                DrawRectangle((int)cx+10, (int)cy+96, (int)cw-20, 26, Fade(m_accentColor, 0.18f));
                int ow = MeasureText("OPEN  >", 13);
                DrawText("OPEN  >", (int)(cx+cw/2-ow/2), (int)cy+103, 13, m_accentColor);
                if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON))
                    LoadProjectManifest(proj.path);
            }
            cx += cw + gap;
            if (cx + cw > W - 36) { cx = 36; cy += ch + gap; }
            idx++;
        }
    }

    // Bottom bar
    float barH = 68;
    DrawRectangleGradientV(0, H-(int)barH-2, W, 2, Fade(m_bgColor,0), Fade(m_accentColor,0.2f));
    DrawRectangle(0, H-(int)barH, W, (int)barH, Color{14,16,26,255});
    DrawRectangle(0, H-(int)barH, W, 1, Fade(m_accentColor,0.3f));

    float bx = 36, by = H - barH + 12;
    DrawText("PROJECT NAME", (int)bx, (int)by, 10, m_dimColor);
    GuiInputText({bx, by+14, 280, 34}, m_newProjName, "my_project", "hub_newproj");
    if (GuiButton({bx+290, by+14, 150, 34}, "+ CREATE PROJECT", m_successColor, WHITE))
        CreateNewProject(m_newProjName);
    if (GuiButton({bx+452, by+14, 100, 34}, "REFRESH", m_panelLight, m_accentColor))
        ScanForProjects();
}

// ── DrawProjectDetail (sidebar) ───────────────────────────────
void BitEditor::DrawProjectDetail() {
    int H = GetScreenHeight();
    DrawRectangle(0, 0, 308, H, m_panelColor);
    DrawRectangle(308, 0, 2, H, Fade(m_accentColor, 0.2f));

    // Back
    if (GuiButton({8, 8, 88, 30}, "< HUB", m_panelLight, m_textColor))
        m_mode = Mode::ProjectHub;

    DrawText(m_activeProject.name.c_str(), 14, 50, 18, m_accentColor);
    GuiSeparator(14, 72, 280);

    float y = 82;
    auto navBtn = [&](const char* label, Mode target) {
        bool active = (m_mode == target);
        Color bg = active ? Fade(m_accentColor, 0.16f) : m_panelColor;
        Color fg = active ? m_accentColor : m_textColor;
        if (GuiButton({8, y, 292, 34}, label, bg, fg)) m_mode = target;
        if (active) DrawRectangle(0, (int)y, 3, 34, m_accentColor);
        y += 38;
    };
    navBtn("  CONFIG",    Mode::ConfigEditor);
    navBtn("  ENTITIES",  Mode::EntityEditor);
    navBtn("  VARIABLES", Mode::VarEditor);

    GuiSeparator(14, y+2, 280); y += 14;
    DrawText("DIALOG FILES", 14, (int)y, 10, m_dimColor); y += 14;

    // File list with hover
    for (const auto& file : m_dialogFiles) {
        bool hover = CheckCollisionPointRec(GetMousePosition(), {8, y, 292, 28});
        DrawRectangle(8, (int)y, 292, 28, hover ? m_panelLight : Fade(m_panelLight, 0.3f));
        if (hover) DrawRectangle(8, (int)y, 3, 28, m_accentColor);
        DrawText(file.c_str(), 18, (int)y+7, 13, hover ? m_accentColor : m_textColor);
        if (hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) OpenGraph(file);
        y += 30;
    }

    y += 6;
    GuiInputText({8, y, 194, 28}, m_newFileNameBuf, "dialog_name", "detail_newfile", 13);
    if (GuiButton({206, y, 94, 28}, "+ ADD", m_successColor, WHITE))
        CreateNewDialog(m_newFileNameBuf);
    y += 36;

    GuiSeparator(8, y+2, 292); y += 12;
    if (GuiButton({8, y, 292, 32}, "SAVE ALL CONFIGS", {180,100,0,255}, WHITE))
        SaveGlobalConfigs();
}

// ── DrawConfigEditor ──────────────────────────────────────────
void BitEditor::DrawConfigEditor() {
    DrawProjectDetail();
    int  W = GetScreenWidth();
    float x = 322, y = 16;
    DrawText("GLOBAL CONFIG", (int)x, (int)y, 20, WHITE); y += 34;
    GuiSeparator(x, y, W-x-16); y += 18;

    auto& c = m_engine.GetProject().configs;

    DrawText("Start Node ID", (int)x, (int)y, 11, m_dimColor); y += 14;
    GuiInputText({x, y, 380, 34}, c.start_node, "start", "cfg_start"); y += 44;

    DrawText("Text Reveal Speed  (chars/sec)", (int)x, (int)y, 11, m_dimColor); y += 14;
    static std::string s_speed;
    if (m_activeInputUid != "cfg_speed") s_speed = std::to_string((int)c.reveal_speed);
    if (GuiInputText({x, y, 180, 34}, s_speed, "45", "cfg_speed"))
        if (!s_speed.empty()) c.reveal_speed = (float)atof(s_speed.c_str());
    y += 44;

    DrawText("Debug Mode", (int)x, (int)y, 11, m_dimColor); y += 14;
    static const std::vector<std::string> debugOpts = {"none","info","full"};
    if (GuiDropdown({x, y, 220, 34}, c.debug_mode, debugOpts, c.debug_mode, "cfg_debug"))
        {} // value updated in-place
    y += 44;

    if (GuiButton({x, y, 180, 34}, "SAVE CONFIGS", m_successColor, WHITE))
        SaveGlobalConfigs();
}

// ── DrawEntityEditor ──────────────────────────────────────────
void BitEditor::DrawEntityEditor() {
    DrawProjectDetail();
    int W = GetScreenWidth();
    float x = 322, y = 16;
    float cw = W - x - 16;

    DrawText("ENTITY EDITOR", (int)x, (int)y, 20, WHITE); y += 34;
    GuiSeparator(x, y, cw); y += 16;

    auto& entities = m_engine.GetProject().entities;
    std::string toDel;

    for (auto& [id, ent] : entities) {
        float sprH   = (float)ent.sprites.size() * 30.f;
        float cardH  = 52.f + 22.f + sprH + 32.f;
        Rectangle card = {x, y, cw, cardH};
        DrawRectangleRounded(card, 0.07f, 8, m_panelColor);
        DrawRectangleRoundedLinesEx(card, 0.07f, 8, 1.f, Fade(m_accentColor, 0.22f));

        // Header row
        float hy = y + 10;
        DrawText("ID:", (int)x+10, (int)hy+2, 12, m_dimColor);
        DrawText(id.c_str(), (int)x+34, (int)hy+2, 14, m_accentColor);

        float hx = x + 150;
        DrawText("Name:", (int)hx, (int)hy+2, 11, m_dimColor);
        GuiInputText({hx+44, hy, 180, 26}, ent.name, "Character", "ent_nm_"+id, 13);

        hx += 234;
        DrawText("PosX:", (int)hx, (int)hy+2, 11, m_dimColor);
        if (!m_entPosBuf.count(id)) m_entPosBuf[id] = std::to_string(ent.default_pos_x);
        if (GuiInputText({hx+42, hy, 70, 26}, m_entPosBuf[id], "0.5", "ent_px_"+id, 13))
            if (!m_entPosBuf[id].empty()) ent.default_pos_x = (float)atof(m_entPosBuf[id].c_str());

        if (GuiButton({x+cw-84, hy, 74, 26}, "DELETE", m_errorColor, WHITE))
            toDel = id;

        // Sprites
        float sy = y + 46;
        DrawText("SPRITES", (int)x+10, (int)sy+2, 10, m_dimColor);
        sy += 18;
        for (auto& [sid, sdef] : ent.sprites) {
            DrawText(sid.c_str(), (int)x+12, (int)sy+5, 12, m_textColor);
            GuiInputText({x+80, sy, 230, 24}, sdef.path, "path/to/sprite.png", "sp_p_"+id+sid, 12);
            DrawText("fr:", (int)x+320, (int)sy+5, 11, m_dimColor);
            std::string fb_key = id+sid+"_f";
            if (!m_spriteFrameBuf.count(fb_key)) m_spriteFrameBuf[fb_key] = std::to_string(sdef.frames);
            if (GuiInputText({x+344, sy, 44, 24}, m_spriteFrameBuf[fb_key], "1", "sp_f_"+id+sid, 12))
                if (!m_spriteFrameBuf[fb_key].empty()) sdef.frames = atoi(m_spriteFrameBuf[fb_key].c_str());
            sy += 30;
        }
        if (GuiButton({x+12, sy+2, 120, 22}, "+ ADD SPRITE", m_panelLight, m_accentColor))
            ent.sprites["state_" + std::to_string(ent.sprites.size())] = SpriteDef();

        y += cardH + 10;
    }
    if (!toDel.empty()) { entities.erase(toDel); m_selectedEntityId = ""; }

    if (GuiButton({x, y, 180, 34}, "+ NEW ENTITY", m_successColor, WHITE)) {
        Entity e; e.name = "NewChar";
        entities["ent_" + std::to_string((int)GetTime())] = e;
    }
}

// ── DrawVarEditor ─────────────────────────────────────────────
void BitEditor::DrawVarEditor() {
    DrawProjectDetail();
    int W = GetScreenWidth();
    float x = 322, y = 16;
    float cw = W - x - 16;

    DrawText("GLOBAL VARIABLES", (int)x, (int)y, 20, WHITE); y += 34;
    GuiSeparator(x, y, cw); y += 16;

    // Column headers
    DrawText("ID (click to rename)", (int)x+10, (int)y, 10, m_dimColor);
    DrawText("INIT",  (int)x+230, (int)y, 10, m_dimColor);
    DrawText("MIN",   (int)x+320, (int)y, 10, m_dimColor);
    DrawText("MAX",   (int)x+410, (int)y, 10, m_dimColor);
    y += 14;

    auto& vars = m_engine.GetProject().variables;
    std::string toDel;
    bool didRename = false;

    for (auto& [id, val] : vars) {
        if (didRename) break;

        // Seed buffers
        if (!m_varRenameBuf.count(id)) m_varRenameBuf[id] = id;
        if (!m_varInitBuf.count(id))   m_varInitBuf[id]   = std::to_string(val.initial_value);
        if (!m_varMinBuf.count(id))    m_varMinBuf[id]    = val.min ? std::to_string(*val.min) : "";
        if (!m_varMaxBuf.count(id))    m_varMaxBuf[id]    = val.max ? std::to_string(*val.max) : "";

        Rectangle card = {x, y, cw, 48};
        DrawRectangleRounded(card, 0.09f, 8, m_panelColor);
        DrawRectangleRoundedLinesEx(card, 0.09f, 8, 1.f, Fade(m_accentColor, 0.2f));

        // Detect deactivation of rename field
        bool wasRenameActive = (m_activeInputUid == "var_id_"+id);
        GuiInputText({x+8, y+10, 210, 28}, m_varRenameBuf[id], id.c_str(), "var_id_"+id, 14);
        bool nowRenameActive = (m_activeInputUid == "var_id_"+id);

        // On deactivation → rename
        if (wasRenameActive && !nowRenameActive) {
            std::string newId = m_varRenameBuf[id];
            if (!newId.empty() && newId != id && !vars.count(newId)) {
                // Move all data
                vars[newId] = val; vars.erase(id);
                m_varRenameBuf[newId] = newId; m_varRenameBuf.erase(id);
                m_varInitBuf[newId]   = m_varInitBuf[id]; m_varInitBuf.erase(id);
                m_varMinBuf[newId]    = m_varMinBuf[id];  m_varMinBuf.erase(id);
                m_varMaxBuf[newId]    = m_varMaxBuf[id];  m_varMaxBuf.erase(id);
                PushNotif("Renamed to: " + newId, m_successColor);
                didRename = true; break;
            } else if (!newId.empty() && vars.count(newId) && newId != id) {
                PushNotif("Name already taken!", m_errorColor);
                m_varRenameBuf[id] = id; // reset
            }
        }

        if (GuiInputText({x+228, y+10, 80, 28}, m_varInitBuf[id], "0", "var_v_"+id, 14))
            if (!m_varInitBuf[id].empty() && m_varInitBuf[id] != "-")
                val.initial_value = atoi(m_varInitBuf[id].c_str());

        if (GuiInputText({x+318, y+10, 80, 28}, m_varMinBuf[id], "none", "var_mn_"+id, 14))
            val.min = m_varMinBuf[id].empty() ? std::nullopt
                    : std::optional<int>(atoi(m_varMinBuf[id].c_str()));

        if (GuiInputText({x+408, y+10, 80, 28}, m_varMaxBuf[id], "none", "var_mx_"+id, 14))
            val.max = m_varMaxBuf[id].empty() ? std::nullopt
                    : std::optional<int>(atoi(m_varMaxBuf[id].c_str()));

        if (GuiButton({x+cw-74, y+10, 64, 28}, "DEL", m_errorColor, WHITE))
            toDel = id;

        y += 54;
    }
    if (!toDel.empty()) {
        vars.erase(toDel);
        m_varRenameBuf.erase(toDel); m_varInitBuf.erase(toDel);
        m_varMinBuf.erase(toDel);    m_varMaxBuf.erase(toDel);
    }

    if (GuiButton({x, y, 180, 34}, "+ NEW VARIABLE", m_successColor, WHITE)) {
        std::string nid = "var_" + std::to_string((int)GetTime());
        VariableDef v; v.initial_value = 0; vars[nid] = v;
        m_varRenameBuf[nid] = nid; m_varInitBuf[nid] = "0";
        m_varMinBuf[nid] = ""; m_varMaxBuf[nid] = "";
    }
}

// ── DrawGraphMode ─────────────────────────────────────────────
void BitEditor::DrawGraphMode() {
    int W = GetScreenWidth(), H = GetScreenHeight();
    BeginMode2D(m_camera);
    DrawGrid();
    DrawConnections();
    for (auto& [id, gn] : m_graphNodes)
        DrawNode(gn, m_engine.GetProject().nodes[id]);
    // Live-link line
    if (!m_linkingSource.empty() && m_graphNodes.count(m_linkingSource)) {
        auto& src = m_graphNodes[m_linkingSource];
        float srcY = (m_linkingSourceOptIdx < 0)
            ? src.pos.y + 60.f
            : src.pos.y + 102.f + m_linkingSourceOptIdx * 26.f + 13.f;
        Color lc = (m_linkingSourceOptIdx < 0) ? m_accentColor : m_successColor;
        DrawLineBezier({src.pos.x+280.f, srcY}, GetScreenToWorld2D(GetMousePosition(), m_camera),
                       2.5f, lc);
    }
    EndMode2D();

    // Top toolbar
    DrawRectangle(0, 0, W, 46, {10,11,18,240});
    DrawRectangle(0, 46, W, 1, Fade(m_accentColor, 0.25f));

    if (GuiButton({6, 6, 84, 32}, "< BACK", m_panelLight, m_textColor)) {
        if (m_dirty) SaveCurrentGraph();
        m_mode = Mode::ProjectDetail;
    }
    DrawText(m_activeProject.name.c_str(), 98, 6,  11, m_dimColor);
    DrawText(m_currentFile.c_str(),        98, 21, 15, m_accentColor);

    // Node type creation buttons
    static const std::vector<std::pair<std::string,Color>> nodeTypes = {
        {"DIALOG",  {0,200,255,255}}, {"CHOICE",  {170,70,255,255}},
        {"EVENT",   {255,145,0,255}}, {"START",   {60,220,100,255}},
        {"END",     {255,65,65,255}}, {"COMMENT", {130,130,130,255}}
    };
    float bx = (float)W;
    bx -= 104; if (GuiButton({bx, 6, 96, 32}, "SAVE Ctrl+S", m_successColor, WHITE)) SaveCurrentGraph();
    for (int i = (int)nodeTypes.size()-1; i >= 0; i--) {
        bx -= 88;
        std::string label = "+ " + nodeTypes[i].first;
        if (GuiButton({bx, 6, 82, 32}, label.c_str(), Fade(nodeTypes[i].second, 0.25f), nodeTypes[i].second))
            CreateNewNode(GetScreenToWorld2D({(float)W/2,(float)H/2}, m_camera),
                          nodeTypes[i].first == "DIALOG"  ? "dialog"  :
                          nodeTypes[i].first == "CHOICE"  ? "choice"  :
                          nodeTypes[i].first == "EVENT"   ? "event"   :
                          nodeTypes[i].first == "START"   ? "start"   :
                          nodeTypes[i].first == "END"     ? "end"     : "comment");
    }
    if (m_dirty) {
        DrawCircle(W-12, 12, 5, m_warnColor);
        DrawText("unsaved", W-58, 5, 10, m_warnColor);
    }

    if (!m_selectedNode.empty()) DrawNodeInspector();
    DrawStatusBar();
}

// ── DrawGrid ──────────────────────────────────────────────────
void BitEditor::DrawGrid() {
    int step = 60, range = 5000;
    for (int i = -range; i <= range; i += step) {
        bool major = (i % (step*5) == 0);
        Color c = major ? Fade(GRAY, 0.1f) : Fade(GRAY, 0.04f);
        DrawLine(i, -range, i, range, c);
        DrawLine(-range, i, range, i, c);
    }
    DrawLine(-16,0,16,0, Fade(m_accentColor, 0.35f));
    DrawLine(0,-16,0,16, Fade(m_accentColor, 0.35f));
}

// ── DrawNode ──────────────────────────────────────────────────
void BitEditor::DrawNode(GraphNode& gn, DialogNode& dn) {
    float   w  = 280.f;
    float   h  = GetNodeHeight(dn);
    Rectangle r = {gn.pos.x, gn.pos.y, w, h};
    bool    sel   = (gn.id == m_selectedNode);
    std::string type    = GetNodeType(dn);
    Color   typeCol = GetNodeTypeColor(type);

    // Shadow
    DrawRectangleRounded({r.x+5, r.y+5, r.width, r.height}, 0.1f, 8, Fade(BLACK, 0.45f));
    // Body
    DrawRectangleRounded(r, 0.1f, 8, sel ? Color{22,26,42,255} : m_panelColor);
    // Border
    DrawRectangleRoundedLinesEx(r, 0.1f, 8, sel ? 2.2f : 1.2f,
        sel ? typeCol : Fade(typeCol, 0.35f));
    // Header stripe
    DrawRectangle((int)r.x, (int)r.y, (int)r.width, 26, Fade(typeCol, sel ? 0.28f : 0.12f));
    // Type badge
    DrawText(type.c_str(), (int)r.x+8, (int)r.y+6, 12, typeCol);
    // ID
    int idw = MeasureText(gn.id.c_str(), 13);
    DrawText(gn.id.c_str(), (int)(r.x+r.width-idw-10), (int)r.y+6, 13, Fade(WHITE, 0.75f));
    // Entity label
    if (!dn.entity.empty()) {
        int ew = MeasureText(dn.entity.c_str(), 12)+8;
        DrawRectangle((int)r.x+6, (int)r.y+32, ew, 17, Fade(typeCol, 0.15f));
        DrawText(dn.entity.c_str(), (int)r.x+10, (int)r.y+34, 12, typeCol);
    }
    // Content preview (clipped)
    BeginScissorMode((int)r.x+6, (int)r.y+52, (int)r.width-14, 40);
    DrawText(dn.content.substr(0,90).c_str(), (int)r.x+8, (int)r.y+54, 12, m_dimColor);
    EndScissorMode();
    // Main output pin (only if no next via options)
    DrawCircle((int)(r.x+r.width), (int)(r.y+60), 7, m_panelColor);
    DrawCircleLines((int)(r.x+r.width), (int)(r.y+60), 7, typeCol);
    DrawCircle((int)(r.x+r.width), (int)(r.y+60), 3,
        (m_linkingSource == gn.id && m_linkingSourceOptIdx == -1) ? typeCol : Fade(typeCol, 0.5f));
    // Option pins
    for (int i = 0; i < (int)dn.options.size(); i++) {
        float pinY = r.y + 102.f + i * 26.f;
        DrawRectangle((int)r.x+6, (int)pinY+2, (int)r.width-20, 22, Fade(m_successColor, 0.08f));
        // Option text
        std::string preview = dn.options[i].content.substr(0, 26);
        DrawText(preview.c_str(), (int)r.x+10, (int)pinY+5, 11, Fade(m_successColor, 0.85f));
        // Pin circle on right
        DrawCircle((int)(r.x+r.width), (int)(pinY+13), 6, m_panelColor);
        DrawCircleLines((int)(r.x+r.width), (int)(pinY+13), 6, m_successColor);
        DrawCircle((int)(r.x+r.width), (int)(pinY+13), 3,
            (m_linkingSource == gn.id && m_linkingSourceOptIdx == i)
                ? m_successColor : Fade(m_successColor, 0.4f));
    }
}

// ── DrawConnections ───────────────────────────────────────────
void BitEditor::DrawConnections() {
    for (auto const& [id, gn] : m_graphNodes) {
        auto& dn = m_engine.GetProject().nodes[id];
        // Main next_id
        if (dn.next_id && m_graphNodes.count(*dn.next_id)) {
            auto& tgt = m_graphNodes[*dn.next_id];
            Vector2 from = {gn.pos.x+280.f, gn.pos.y+60.f};
            Vector2 to   = {tgt.pos.x,      tgt.pos.y+60.f};
            DrawLineBezier(from, to, 2.2f, Fade(m_accentColor, 0.65f));
            DrawCircle((int)to.x, (int)to.y, 5, m_accentColor);
        }
        // Option next_ids
        for (int i = 0; i < (int)dn.options.size(); i++) {
            const std::string& nid = dn.options[i].next_id;
            if (!nid.empty() && m_graphNodes.count(nid)) {
                float srcY = gn.pos.y + 102.f + i * 26.f + 13.f;
                auto& tgt = m_graphNodes[nid];
                DrawLineBezier({gn.pos.x+280.f, srcY}, {tgt.pos.x, tgt.pos.y+50.f},
                               1.8f, Fade(m_successColor, 0.6f));
                DrawCircle((int)tgt.pos.x, (int)(tgt.pos.y+50), 4, m_successColor);
            }
        }
    }
}

// ── DrawNodeInspector ─────────────────────────────────────────
void BitEditor::DrawNodeInspector() {
    int H = GetScreenHeight();
    float iw = 390, ix = GetScreenWidth() - iw;
    DrawRectangle((int)ix, 48, (int)iw, H-48, Color{14,16,26,252});
    DrawRectangle((int)ix, 48, 1, H-48, Fade(m_accentColor, 0.25f));

    auto& n = m_engine.GetProject().nodes[m_selectedNode];
    float iy = 56;

    DrawText("INSPECTOR", (int)ix+10, (int)iy, 10, m_dimColor); iy += 14;
    std::string type = GetNodeType(n);
    Color tc = GetNodeTypeColor(type);
    DrawRectangle((int)ix+8, (int)iy, (int)iw-16, 28, Fade(tc, 0.15f));
    DrawText(m_selectedNode.c_str(), (int)ix+14, (int)iy+7, 15, tc);
    iy += 34; GuiSeparator(ix+8, iy, iw-16); iy += 10;

    // Node type dropdown
    DrawText("Node Type", (int)ix+10, (int)iy, 10, m_dimColor); iy += 12;
    static const std::vector<std::string> ntypes = {"dialog","choice","event","start","end","comment"};
    if (GuiDropdown({ix+8, iy, iw-16, 30}, type, ntypes, n.metadata["__type"], "insp_type"))
        m_dirty = true;
    iy += 38;

    // Entity
    DrawText("Speaker Entity ID", (int)ix+10, (int)iy, 10, m_dimColor); iy += 12;
    GuiInputText({ix+8, iy, iw-16, 30}, n.entity, "entity_id", "insp_ent_"+m_selectedNode); iy += 38;

    // Content
    DrawText("Dialog Content", (int)ix+10, (int)iy, 10, m_dimColor); iy += 12;
    GuiInputText({ix+8, iy, iw-16, 110}, n.content, "Enter dialog text...",
                 "insp_cnt_"+m_selectedNode, 14, true); iy += 116;

    // Direct next link
    DrawText("Direct next_id  (or use option pins)", (int)ix+10, (int)iy, 10, m_dimColor); iy += 12;
    static std::string s_nextBuf;
    if (m_activeInputUid != "insp_nxt_"+m_selectedNode)
        s_nextBuf = n.next_id ? *n.next_id : "";
    GuiInputText({ix+8, iy, iw-90, 30}, s_nextBuf, "node_id", "insp_nxt_"+m_selectedNode);
    n.next_id = s_nextBuf.empty() ? std::nullopt : std::optional<std::string>(s_nextBuf);
    if (n.next_id && GuiButton({ix+iw-78, iy, 68, 30}, "CLEAR", m_errorColor, WHITE))
        n.next_id = std::nullopt;
    iy += 38;

    GuiSeparator(ix+8, iy, iw-16); iy += 6;
    DrawNodeOptions(n, ix, iy, iw);

    // Footer
    float fy = H - 50;
    DrawRectangle((int)ix, (int)fy-4, (int)iw, 1, Fade(m_accentColor, 0.18f));
    if (GuiButton({ix+8, fy, 120, 36}, "DELETE DEL", m_errorColor, WHITE))
        DeleteNode(m_selectedNode);
    if (GuiButton({ix+iw-128, fy, 120, 36}, "SAVE Ctrl+S", m_successColor, WHITE))
        SaveCurrentGraph();
}

// ── DrawNodeOptions ───────────────────────────────────────────
void BitEditor::DrawNodeOptions(DialogNode& n, float ix, float& iy, float iw) {
    DrawText("DIALOG OPTIONS", (int)ix+10, (int)iy, 10, m_dimColor); iy += 14;
    std::string toDel;
    for (int i = 0; i < (int)n.options.size(); i++) {
        Rectangle card = {ix+6, iy, iw-12, 64};
        DrawRectangleRounded(card, 0.09f, 8, m_panelLight);
        DrawText(TextFormat("#%d", i+1), (int)ix+12, (int)iy+4, 11, m_successColor);
        if (GuiButton({ix+iw-54, iy+4, 44, 18}, "DEL", m_errorColor, WHITE))
            toDel = std::to_string(i);
        DrawText("Text:", (int)ix+10, (int)iy+24, 10, m_dimColor);
        GuiInputText({ix+46, iy+20, iw-60, 22}, n.options[i].content, "Option text",
                     "opt_c_"+m_selectedNode+"_"+std::to_string(i), 12);
        DrawText("Next:", (int)ix+10, (int)iy+46, 10, m_dimColor);
        GuiInputText({ix+46, iy+42, iw-60, 22}, n.options[i].next_id, "node_id",
                     "opt_n_"+m_selectedNode+"_"+std::to_string(i), 12);
        iy += 70;
    }
    if (!toDel.empty()) {
        int idx = atoi(toDel.c_str());
        if (idx >= 0 && idx < (int)n.options.size())
            n.options.erase(n.options.begin()+idx);
        m_dirty = true;
    }
    if (GuiButton({ix+8, iy, iw-16, 26}, "+ ADD OPTION", m_panelLight, m_accentColor)) {
        DialogOption opt; opt.content = "New option"; opt.next_id = "";
        n.options.push_back(opt); m_dirty = true;
    }
    iy += 32;
}

// ── DrawStatusBar ─────────────────────────────────────────────
void BitEditor::DrawStatusBar() {
    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangle(0, H-20, W, 20, {10,11,18,230});
    DrawRectangle(0, H-20, W, 1, Fade(m_accentColor, 0.18f));
    DrawText(TextFormat("Nodes: %d  |  Zoom: %.2fx  |  %s",
        (int)m_graphNodes.size(), m_camera.zoom, m_currentFile.c_str()),
        8, H-16, 11, m_dimColor);
    const char* hints = "A=Add Node | Del=Delete | RMB=Pan | Scroll=Zoom | Ctrl+S=Save";
    DrawText(hints, W - MeasureText(hints,11) - 6, H-16, 11, Fade(m_dimColor, 0.5f));
}

// ── DrawDropdownOverlay ─ always rendered last (on top of all UI) ─────────
void BitEditor::DrawDropdownOverlay() {
    if (!m_dropdownPending.active || !m_dropdownOpen) return;
    auto& p = m_dropdownPending;
    const float oh = 30.f;
    // Shadow
    DrawRectangleRounded({p.panelRect.x+4, p.panelRect.y+4,
                          p.panelRect.width, p.panelRect.height}, 0.1f, 8, Fade(BLACK, 0.5f));
    // Panel background
    DrawRectangleRounded(p.panelRect, 0.1f, 8, Color{18, 20, 32, 255});
    DrawRectangleRoundedLinesEx(p.panelRect, 0.1f, 8, 1.8f, m_accentColor);
    // Option rows
    for (int i = 0; i < (int)p.opts.size(); i++) {
        Rectangle row = {p.panelRect.x+2, p.panelRect.y + i*oh, p.panelRect.width-4, oh};
        bool hov = CheckCollisionPointRec(GetMousePosition(), row);
        if (hov) DrawRectangleRounded(row, 0.08f, 8, m_panelLight);
        bool isCur = (p.opts[i] == p.current);
        DrawText(p.opts[i].c_str(), (int)row.x+12, (int)(row.y + (oh-14)*0.5f), 14,
                 isCur ? m_accentColor : m_textColor);
        if (isCur) DrawCircle((int)(row.x+row.width-14), (int)(row.y+oh*0.5f), 4, m_accentColor);
        if (hov && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            if (p.outPtr) *p.outPtr = p.opts[i];
            m_dropdownOpen = false;
        }
    }
    // Close on outside click
    bool overBtn   = CheckCollisionPointRec(GetMousePosition(), p.btnRect);
    bool overPanel = CheckCollisionPointRec(GetMousePosition(), p.panelRect);
    if (!overBtn && !overPanel && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        m_dropdownOpen = false;
}

// ── DrawNotifications ─────────────────────────────────────────
void BitEditor::DrawNotifications() {
    float nx = GetScreenWidth() - 310.f, ny = 58;
    for (auto& notif : m_notifs) {
        float alpha = std::min(1.0f, notif.ttl * 2.0f);
        DrawRectangleRounded({nx, ny, 296, 36}, 0.15f, 8, Fade({14,16,26,255}, alpha));
        DrawRectangleRoundedLinesEx({nx, ny, 296, 36}, 0.15f, 8, 1.3f, Fade(notif.color, alpha));
        DrawRectangle((int)nx, (int)ny, 4, 36, Fade(notif.color, alpha));
        DrawText(notif.text.c_str(), (int)nx+12, (int)ny+10, 14, Fade(WHITE, alpha));
        ny += 42;
    }
}
