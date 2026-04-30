#include "BitEngine.hpp"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <regex>
#include <iomanip>
#include <ctime>
#include <sys/stat.h>

using json = nlohmann::json;
static const std::string KEY = "BITENGINE_SECRET_KEY_2026";

// --- DialogParser ---
DialogProject DialogParser::ParseConfig(const std::string& path) {
    DialogProject p;
    std::ifstream f(path); if (!f) return p;
    try {
        json j; f >> j;
        if (j.contains("configs")) {
            auto& c = j["configs"];
            p.configs.start_node = c.value("start_node", "dialog_start");
            p.configs.reveal_speed = c.value("reveal_speed", 30.0f);
            p.configs.debug = c.value("debug", false);
            p.configs.auto_save = c.value("auto_save", false);
            p.configs.encrypt_save = c.value("encrypt_save", false);
            p.configs.save_prefix = c.value("save_prefix", "save_slot_");
            p.configs.max_slots = c.value("max_slots", 5);
            p.configs.mode = c.value("mode", "typewriter");
        }
        if (j.contains("entities")) {
            for (auto& [id, ent] : j["entities"].items()) 
                p.entities[id] = { id, ent.value("name", id), ent.value("type", "char") };
        }
        if (j.contains("variables")) {
            for (auto& [id, v] : j["variables"].items()) {
                VariableDef def = { id, v.value("initial", 0) };
                if (v.contains("min")) def.min = v["min"].get<int>();
                if (v.contains("max")) def.max = v["max"].get<int>();
                p.variables[id] = def;
            }
        }
        if (j.contains("dialogs") && j["dialogs"].is_array())
            for (auto& d : j["dialogs"]) p.configs.dialog_files.push_back(d.get<std::string>());
    } catch (...) {}
    return p;
}

bool DialogParser::LoadDialogFile(const std::string& path, DialogProject& p) {
    std::ifstream f(path); if (!f) return false;
    try {
        json j; f >> j;
        for (auto& [id, n] : j.items()) {
            DialogNode node = { n.value("entity", "unk"), n.value("content", "") };
            if (n.contains("next_id") && !n["next_id"].is_null()) node.next_id = n["next_id"].get<std::string>();
            if (n.contains("metadata")) for (auto& [k, v] : n["metadata"].items()) node.metadata[k] = v.get<std::string>();
            if (n.contains("options")) {
                for (auto& o : n["options"]) {
                    DialogOption opt = { o.value("content", ""), "", o.value("style", "normal") };
                    if (o.contains("next_id") && !o["next_id"].is_null()) opt.next_id = o["next_id"].get<std::string>();
                    if (o.contains("conditions")) {
                        auto& conds = o["conditions"];
                        if (conds.is_array()) for (auto& c : conds) opt.conditions.push_back({ c.value("op", "="), c.value("var", ""), c.value("value", 0) });
                        else if (conds.is_object()) opt.conditions.push_back({ conds.value("op", "="), conds.value("var", ""), conds.value("value", 0) });
                    }
                    if (o.contains("events")) for (auto& e : o["events"]) opt.events.push_back({ e.value("op", "set"), e.value("var", ""), e.value("value", 0) });
                    node.options.push_back(opt);
                }
            }
            if (n.contains("events")) for (auto& e : n["events"]) node.events.push_back({ e.value("op", "set"), e.value("var", ""), e.value("value", 0) });
            p.nodes[id] = node;
        }
    } catch (...) { return false; }
    return true;
}

// --- DialogEngine ---
DialogEngine::DialogEngine() {}

bool DialogEngine::LoadProject(const std::string& path) {
    m_project = DialogParser::ParseConfig(path);
    for (const auto& f : m_project.configs.dialog_files) DialogParser::LoadDialogFile(f, m_project);
    if (m_project.nodes.empty()) return false;
    for (auto const& [id, def] : m_project.variables) m_variables[id] = def.initial_value;
    return true;
}

// Advanced Save Implementation
void DialogEngine::SaveGame(int slot) {
    if (!m_currentNode) return;
    try {
        SaveData sd;
        sd.current_node_id = ""; // To be found (we need current ID)
        // Find current node ID
        for (auto const& [id, node] : m_project.nodes) { if (&node == m_currentNode) { sd.current_node_id = id; break; } }
        
        sd.variables = m_variables;
        sd.meta.timestamp = GetTimestamp();
        sd.meta.node_id = sd.current_node_id;
        const auto* entity = GetCurrentEntity();
        sd.meta.entity_name = entity ? entity->name : "Unknown";
        
        // Dynamic summary
        std::string content = m_cachedInterpolatedContent;
        if (content.length() > 40) content = content.substr(0, 37) + "...";
        sd.meta.summary = content;

        json j;
        j["version"] = sd.version;
        j["node_id"] = sd.current_node_id;
        j["variables"] = sd.variables;
        j["meta"] = { {"time", sd.meta.timestamp}, {"node", sd.meta.node_id}, {"char", sd.meta.entity_name}, {"text", sd.meta.summary} };

        std::string data = j.dump(4);
        if (m_project.configs.encrypt_save) data = XORBuffer(data);
        
        std::ofstream f(GetSlotPath(slot), std::ios::binary);
        if (f) f.write(data.c_str(), data.size());
        
        if (m_project.configs.debug) std::cout << "[DialogEngine] Saved Slot " << slot << " at " << sd.meta.timestamp << std::endl;
    } catch (...) {}
}

bool DialogEngine::LoadGame(int slot) {
    std::ifstream f(GetSlotPath(slot), std::ios::binary | std::ios::ate);
    if (!f) return false;
    std::streamsize sz = f.tellg(); f.seekg(0);
    std::string data(sz, '\0');
    if (f.read(&data[0], sz)) {
        try {
            if (m_project.configs.encrypt_save) data = XORBuffer(data);
            json j = json::parse(data);
            m_variables = j["variables"].get<std::map<std::string, int>>();
            std::string nodeId = j["node_id"].get<std::string>();
            StartDialog(nodeId);
            if (m_project.configs.debug) std::cout << "[DialogEngine] Loaded Slot " << slot << std::endl;
            return true;
        } catch (...) {}
    }
    return false;
}

bool DialogEngine::HasSave(int slot) const {
    struct stat buffer; return (stat(GetSlotPath(slot).c_str(), &buffer) == 0);
}

std::optional<SaveMetadata> DialogEngine::GetSaveMetadata(int slot) const {
    std::ifstream f(GetSlotPath(slot), std::ios::binary | std::ios::ate);
    if (!f) return std::nullopt;
    std::streamsize sz = f.tellg(); f.seekg(0);
    std::string data(sz, '\0');
    if (f.read(&data[0], sz)) {
        try {
            if (m_project.configs.encrypt_save) data = XORBuffer(data);
            json j = json::parse(data);
            auto m = j["meta"];
            return SaveMetadata{ m["time"], m["node"], m["char"], m["text"] };
        } catch (...) {}
    }
    return std::nullopt;
}

void DialogEngine::StartDialog(const std::string& startId) {
    std::string id = startId.empty() ? m_project.configs.start_node : startId;
    if (!m_project.nodes.count(id)) return;
    m_currentNode = &m_project.nodes[id]; m_isActive = true;
    m_cachedInterpolatedContent = InterpolateVariables(m_currentNode->content);
    m_cachedTotalChars = GetUTF8Length(m_cachedInterpolatedContent);
    m_revealedCount = (m_project.configs.mode == "instant") ? (float)m_cachedTotalChars : 0.0f;
    ProcessEvents(m_currentNode->events);
    RefreshVisibleOptions();
    
    // Auto-save on specific nodes if enabled
    if (m_project.configs.auto_save) SaveGame(0); // Slot 0 is auto-save
}

void DialogEngine::SelectOption(int index) {
    if (!m_isActive || IsTextRevealing() || index < 0 || index >= (int)m_visibleOptions.size()) return;
    ProcessEvents(m_visibleOptions[index].events);
    std::string nextId = m_visibleOptions[index].next_id;
    if (nextId.empty() || nextId == "dialog_end") { m_isActive = false; m_currentNode = nullptr; }
    else StartDialog(nextId);
}

void DialogEngine::Next() {
    if (!m_isActive) return;
    if (IsTextRevealing()) { SkipReveal(); return; }
    if (!m_visibleOptions.empty()) return;
    std::string nId = m_currentNode->next_id.value_or("dialog_end");
    if (nId == "dialog_end") { m_isActive = false; m_currentNode = nullptr; }
    else StartDialog(nId);
}

void DialogEngine::Update(float dt) {
    if (!m_isActive || !m_currentNode || m_project.configs.mode == "instant") return;
    if (IsTextRevealing()) { m_revealedCount = std::min((float)m_cachedTotalChars, m_revealedCount + m_project.configs.reveal_speed * dt); }
}

void DialogEngine::SkipReveal() { if (m_currentNode) m_revealedCount = (float)m_cachedTotalChars; }
bool DialogEngine::IsTextRevealing() const { return m_isActive && m_revealedCount < (float)m_cachedTotalChars; }

std::string DialogEngine::GetVisibleContent() const {
    if (!m_currentNode) return "";
    size_t byteIdx = 0, charCnt = 0;
    while (byteIdx < m_cachedInterpolatedContent.length() && charCnt < (size_t)m_revealedCount) {
        unsigned char c = m_cachedInterpolatedContent[byteIdx];
        byteIdx += (c <= 127) ? 1 : ((c & 0xE0) == 0xC0) ? 2 : ((c & 0xF0) == 0xE0) ? 3 : 4;
        charCnt++;
    }
    return m_cachedInterpolatedContent.substr(0, byteIdx);
}

size_t DialogEngine::GetUTF8Length(const std::string& s) const {
    size_t count = 0;
    for (size_t i = 0; i < s.length(); ) {
        unsigned char c = s[i];
        i += (c <= 127) ? 1 : ((c & 0xE0) == 0xC0) ? 2 : ((c & 0xF0) == 0xE0) ? 3 : 4;
        count++;
    }
    return count;
}

std::string DialogEngine::InterpolateVariables(const std::string& text) const {
    std::string res = text; std::regex re("\\{([a-zA-Z0-9_]+)\\}"); std::smatch m;
    while (std::regex_search(res, m, re)) res.replace(m.position(), m.length(), std::to_string(GetVariable(m[1].str())));
    return res;
}

int DialogEngine::GetVariable(const std::string& name) const {
    auto it = m_variables.find(name); return (it != m_variables.end()) ? it->second : 0;
}

void DialogEngine::SetVariable(const std::string& name, int value) {
    if (!m_project.variables.count(name)) return;
    const auto& d = m_project.variables[name];
    int v = value;
    if (d.min) v = std::max(v, *d.min); if (d.max) v = std::min(v, *d.max);
    m_variables[name] = v;
}

void DialogEngine::ProcessEvents(const std::vector<Event>& events) {
    for (const auto& e : events) {
        int cur = GetVariable(e.var);
        if (e.op == "set") SetVariable(e.var, e.value);
        else if (e.op == "add") SetVariable(e.var, cur + e.value);
        else if (e.op == "sub") SetVariable(e.var, cur - e.value);
        else if (e.op == "mul") SetVariable(e.var, cur * e.value);
    }
}

void DialogEngine::RefreshVisibleOptions() {
    m_visibleOptions.clear();
    for (auto& o : m_currentNode->options) {
        bool pass = true;
        for (const auto& c : o.conditions) {
            int v = GetVariable(c.var);
            if (c.op == "=" && v != c.value) pass = false;
            else if (c.op == "!=" && v == c.value) pass = false;
            else if (c.op == ">" && v <= c.value) pass = false;
            else if (c.op == "<" && v >= c.value) pass = false;
            else if (c.op == ">=" && v < c.value) pass = false;
            else if (c.op == "<=" && v > c.value) pass = false;
            if (!pass) break;
        }
        if (pass) m_visibleOptions.push_back(o);
    }
}

const Entity* DialogEngine::GetCurrentEntity() const {
    if (!m_currentNode) return nullptr;
    auto it = m_project.entities.find(m_currentNode->entity_id);
    return (it != m_project.entities.end()) ? &it->second : nullptr;
}

std::string DialogEngine::XORBuffer(const std::string& d) const {
    std::string o = d; for (size_t i = 0; i < d.size(); ++i) o[i] ^= KEY[i % KEY.size()]; return o;
}

std::string DialogEngine::GetSlotPath(int slot) const { return "save/" + m_project.configs.save_prefix + std::to_string(slot) + ".bin"; }

std::string DialogEngine::GetTimestamp() const {
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    std::stringstream ss; ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S"); return ss.str();
}
