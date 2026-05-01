#include "BitEngine.hpp"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <unordered_set>
#include <ctime>
#include <random>

using json = nlohmann::json;
static const std::string KEY = BITENGINE_KEY;

BitColor RichTextParser::StringToColor(const std::string& str) {
    std::string s = str;
    for (char& c : s) c = toupper(c);
    if (s == "RED") return {230, 41, 55, 255};
    if (s == "BLUE") return {0, 121, 241, 255};
    if (s == "GREEN") return {0, 228, 48, 255};
    if (s == "YELLOW") return {253, 249, 0, 255};
    if (s == "ORANGE") return {255, 161, 0, 255};
    if (s == "PURPLE") return {200, 122, 255, 255};
    if (s == "PINK") return {255, 109, 194, 255};
    if (s == "BLACK") return {0, 0, 0, 255};
    if (s == "WHITE") return {255, 255, 255, 255};
    if (s == "GRAY") return {130, 130, 130, 255};
    if (s == "GOLD") return {255, 203, 0, 255};
    
    // Hex parsing (#RRGGBB)
    if (s.length() == 7 && s[0] == '#') {
        unsigned int r, g, b;
        if (sscanf(s.c_str() + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
            return BitColor{ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
        }
    }
    return BitColor::Blank();
}

std::vector<RichChar> RichTextParser::Parse(const std::string& rawText) {
    std::vector<RichChar> result;
    result.reserve(rawText.length()); // Prevent reallocations
    BitColor currentColor = BitColor::Blank();
    float currentSpeedMod = 1.0f;
    bool currentShake = false;
    bool currentWave = false;
    float accumulatedWait = 0.0f;

    size_t i = 0;
    while (i < rawText.length()) {
        if (rawText[i] == '[') {
            size_t closeIdx = rawText.find(']', i);
            if (closeIdx != std::string::npos) {
                std::string tag = rawText.substr(i + 1, closeIdx - i - 1);
                
                bool isTag = true;
                if (tag.find("color=") == 0) {
                    currentColor = StringToColor(tag.substr(6));
                } else if (tag == "/color") {
                    currentColor = BitColor::Blank();
                } else if (tag.find("speed=") == 0) {
                    try { currentSpeedMod = std::stof(tag.substr(6)); } catch (...) { currentSpeedMod = 1.0f; }
                } else if (tag == "/speed") {
                    currentSpeedMod = 1.0f;
                } else if (tag == "shake") {
                    currentShake = true;
                } else if (tag == "/shake") {
                    currentShake = false;
                } else if (tag == "wave") {
                    currentWave = true;
                } else if (tag == "/wave") {
                    currentWave = false;
                } else if (tag.find("wait=") == 0) {
                    try { accumulatedWait += std::stof(tag.substr(5)); } catch (...) {}
                } else {
                    isTag = false; // Not a recognized tag, treat as raw text
                }

                if (isTag) {
                    i = closeIdx + 1;
                    continue; // Successfully parsed a tag, move to next char
                }
            }
        }

        // Parse UTF-8 character
        unsigned char c = rawText[i];
        size_t charLen = (c <= 127) ? 1 : ((c & 0xE0) == 0xC0) ? 2 : ((c & 0xF0) == 0xE0) ? 3 : 4;
        
        RichChar rc;
        std::memcpy(rc.ch, rawText.data() + i, charLen);
        rc.ch[charLen] = '\0';
        i += charLen;

        rc.color = currentColor;
        rc.speedMod = currentSpeedMod;
        rc.shake = currentShake;
        rc.wave = currentWave;
        rc.waitBefore = accumulatedWait;
        
        accumulatedWait = 0.0f; // Reset wait after applying to this character
        result.push_back(rc);
    }
    return result;
}

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
            p.configs.debug_mode = c.value("debug_mode", "none");
            p.configs.auto_save = c.value("auto_save", false);
            p.configs.encrypt_save = c.value("encrypt_save", false);
            p.configs.enable_floating = c.value("enable_floating", true);
            p.configs.enable_shadows = c.value("enable_shadows", true);
            p.configs.enable_vignette = c.value("enable_vignette", true);
            p.configs.save_prefix = c.value("save_prefix", "save_slot_");
            p.configs.max_slots = c.value("max_slots", 5);
            p.configs.mode = c.value("mode", "typewriter");
        }
        
        // Support for modular file paths
        if (j.contains("entities")) {
            if (j["entities"].is_array()) for (auto& f : j["entities"]) p.configs.entity_files.push_back(f.get<std::string>());
            else if (j["entities"].is_object()) {
                // Legacy inline support
                for (auto& [id, ent] : j["entities"].items()) {
                    Entity entity;
                    entity.id = id;
                    entity.name = ent.value("name", id);
                    entity.type = ent.value("type", "char");
                    if (ent.contains("sprites")) {
                        for (auto& [expr, s] : ent["sprites"].items()) 
                            entity.sprites[expr] = { s.value("path", ""), s.value("frames", 1), s.value("speed", 5.0f), s.value("scale", 1.0f) };
                    }
                    p.entities[id] = entity;
                }
            }
        }
        
        if (j.contains("variables")) {
            if (j["variables"].is_array()) for (auto& f : j["variables"]) p.configs.variable_files.push_back(f.get<std::string>());
            else if (j["variables"].is_object()) {
                // Legacy inline support
                for (auto& [id, v] : j["variables"].items()) {
                    VariableDef def;
                    def.id = id;
                    def.initial_value = v.value("initial", 0);
                    if (v.contains("min")) def.min = v["min"].get<int>();
                    if (v.contains("max")) def.max = v["max"].get<int>();
                    p.variables[id] = def;
                }
            }
        }
        
        if (j.contains("dialogs") && j["dialogs"].is_array())
            for (auto& d : j["dialogs"]) p.configs.dialog_files.push_back(d.get<std::string>());
            
        if (j.contains("assets") && j["assets"].is_array())
            for (auto& a : j["assets"]) p.configs.asset_files.push_back(a.get<std::string>());
            
    } catch (...) {}
    return p;
}

bool DialogParser::LoadEntitiesFile(const std::string& path, DialogProject& p) {
    std::ifstream f(path); if (!f) return false;
    try {
        json j; f >> j;
        auto& root = j.contains("entities") ? j["entities"] : j;
        for (auto& [id, ent] : root.items()) {
            Entity entity;
            entity.id = id;
            entity.name = ent.value("name", id);
            entity.type = ent.value("type", "char");
            
            std::string dpos = ent.value("default_pos", "center");
            if (dpos == "left") entity.default_pos_x = 0.2f;
            else if (dpos == "right") entity.default_pos_x = 0.8f;
            else if (dpos == "center") entity.default_pos_x = 0.5f;
            else try { entity.default_pos_x = std::stof(dpos); } catch(...) { entity.default_pos_x = 0.5f; }

            if (ent.contains("sprites")) {
                for (auto& [expr, s] : ent["sprites"].items()) 
                    entity.sprites[expr] = { s.value("path", ""), s.value("frames", 1), s.value("speed", 5.0f), s.value("scale", 1.0f) };
            }
            p.entities[id] = entity;
        }
    } catch (...) { return false; }
    return true;
}

bool DialogParser::LoadVariablesFile(const std::string& path, DialogProject& p) {
    std::ifstream f(path); if (!f) return false;
    try {
        json j; f >> j;
        auto& root = j.contains("variables") ? j["variables"] : j;
        for (auto& [id, v] : root.items()) {
            VariableDef def;
            def.id = id;
            def.initial_value = v.value("initial", 0);
            if (v.contains("min")) def.min = v["min"].get<int>();
            if (v.contains("max")) def.max = v["max"].get<int>();
            p.variables[id] = def;
        }
    } catch (...) { return false; }
    return true;
}

bool DialogParser::LoadDialogFile(const std::string& path, DialogProject& p) {
    std::ifstream f(path); if (!f) return false;
    try {
        json j; f >> j;
        for (auto& [id, n] : j.items()) {
            DialogNode node;
            node.entity = n.value("entity", "unk");
            node.content = n.value("content", "");
            if (n.contains("next_id") && !n["next_id"].is_null()) node.next_id = n["next_id"].get<std::string>();
            if (n.contains("metadata")) for (auto& [k, v] : n["metadata"].items()) node.metadata[k] = v.get<std::string>();
            if (n.contains("options")) {
                for (auto& o : n["options"]) {
                    DialogOption opt;
                    opt.content = o.value("content", "");
                    opt.style = o.value("style", "normal");
                    if (o.contains("next_id") && !o["next_id"].is_null()) opt.next_id = o["next_id"].get<std::string>();
                    if (o.contains("conditions")) {
                        auto& conds = o["conditions"];
                        if (conds.is_array()) for (auto& c : conds) {
                             std::string op = c.value("op", "==");
                             if (op == "=") op = "==";
                             opt.conditions.push_back({ op, c.value("var", ""), c.value("value", 0) });
                        }
                        else if (conds.is_object()) {
                            std::string op = conds.value("op", "==");
                            if (op == "=") op = "==";
                            opt.conditions.push_back({ op, conds.value("var", ""), conds.value("value", 0) });
                        }
                    }
                    if (o.contains("events")) for (auto& e : o["events"]) opt.events.push_back({ e.value("op", "set"), e.value("var", ""), e.value("value", 0), e.value("value_max", 0), e.value("value_str", "") });
                    node.options.push_back(opt);
                }
            }
            if (n.contains("events")) for (auto& e : n["events"]) node.events.push_back({ e.value("op", "set"), e.value("var", ""), e.value("value", 0), e.value("value_max", 0), e.value("value_str", "") });
            p.nodes[id] = node;
        }
    } catch (...) { return false; }
    return true;
}

bool DialogParser::LoadAssetsFile(const std::string& path, DialogProject& p) {
    std::ifstream f(path); if (!f) return false;
    try {
        json j; f >> j;
        if (j.contains("backgrounds")) {
            for (auto& [id, file] : j["backgrounds"].items()) p.backgrounds[id] = file.get<std::string>();
        }
        if (j.contains("music")) {
            for (auto& [id, file] : j["music"].items()) p.music[id] = file.get<std::string>();
        }
        if (j.contains("sfx")) {
            for (auto& [id, file] : j["sfx"].items()) p.sfx[id] = file.get<std::string>();
        }
    } catch (...) { return false; }
    return true;
}

// --- DialogEngine ---
DialogEngine::DialogEngine() {}

bool DialogEngine::LoadProject(const std::string& configFilePath) {
    Log("Loading project from: " + configFilePath);
    m_project = DialogParser::ParseConfig(configFilePath);
    
    for (const auto& f : m_project.configs.entity_files)
        if (!DialogParser::LoadEntitiesFile(f, m_project)) RecordError("LoadProject", "Could not load entities file: " + f);
    for (const auto& f : m_project.configs.variable_files)
        if (!DialogParser::LoadVariablesFile(f, m_project)) RecordError("LoadProject", "Could not load variables file: " + f);
    for (const auto& f : m_project.configs.asset_files)
        if (!DialogParser::LoadAssetsFile(f, m_project)) RecordError("LoadProject", "Could not load assets file: " + f);
    for (const auto& f : m_project.configs.dialog_files)
        if (!DialogParser::LoadDialogFile(f, m_project)) RecordError("LoadProject", "Could not load dialog file: " + f);
    
    if (m_project.nodes.empty()) {
        RecordError("LoadProject", "No dialog nodes loaded. Check your dialog files.");
        return false;
    }
    for (auto const& [id, def] : m_project.variables) m_variables[id] = def.initial_value;
    
    m_debugOverlayVisible = (m_project.configs.debug_mode == "debug_all");
    
    auto errors = ValidateProject(m_project);
    for (const auto& e : errors) RecordError(e.file + "/" + e.field, e.message);
    if (!errors.empty()) Log("Schema validation found " + std::to_string(errors.size()) + " error(s).", "WARN");
    
    return true;
}

void DialogEngine::CompileProject(const std::string& outputPath) {
    Log("Compiling project to: " + outputPath);
    json j;
    
    // Serialize Configs
    j["configs"] = {
        {"start_node", m_project.configs.start_node},
        {"reveal_speed", m_project.configs.reveal_speed},
        {"debug_mode", m_project.configs.debug_mode},
        {"auto_save", m_project.configs.auto_save},
        {"encrypt_save", m_project.configs.encrypt_save},
        {"enable_floating", m_project.configs.enable_floating},
        {"enable_shadows", m_project.configs.enable_shadows},
        {"enable_vignette", m_project.configs.enable_vignette},
        {"save_prefix", m_project.configs.save_prefix},
        {"max_slots", m_project.configs.max_slots},
        {"mode", m_project.configs.mode}
    };

    // Serialize Entities
    for (auto const& [id, e] : m_project.entities) {
        json ej = { {"name", e.name}, {"type", e.type}, {"default_pos_x", e.default_pos_x} };
        for (auto const& [sid, s] : e.sprites) {
            ej["sprites"][sid] = { {"path", s.path}, {"frames", s.frames}, {"speed", s.speed}, {"scale", s.scale} };
        }
        j["entities"][id] = ej;
    }

    // Serialize Variables
    for (auto const& [id, v] : m_project.variables) {
        j["variables"][id] = { {"initial", v.initial_value}, {"min", v.min}, {"max", v.max} };
    }

    // Serialize Nodes
    for (auto const& [id, n] : m_project.nodes) {
        json nj = { {"entity", n.entity}, {"content", n.content}, {"next_id", n.next_id}, {"metadata", n.metadata} };
        for (auto const& o : n.options) {
            json oj = { {"content", o.content}, {"next_id", o.next_id}, {"style", o.style} };
            for (auto const& c : o.conditions) oj["conditions"].push_back({ {"op", c.op}, {"var", c.var}, {"value", c.value} });
            for (auto const& e : o.events) oj["events"].push_back({ {"op", e.op}, {"var", e.var}, {"value", e.value} });
            nj["options"].push_back(oj);
        }
        for (auto const& e : n.events) nj["events"].push_back({ {"op", e.op}, {"var", e.var}, {"value", e.value} });
        j["nodes"][id] = nj;
    }
    
    // Serialize Assets
    for (auto const& [id, p] : m_project.backgrounds) j["backgrounds"][id] = p;
    for (auto const& [id, p] : m_project.music) j["music"][id] = p;
    for (auto const& [id, p] : m_project.sfx) j["sfx"][id] = p;

    std::string data = j.dump();
    data = XORBuffer(data);
    std::ofstream f(outputPath, std::ios::binary);
    f.write(data.data(), data.size());
}

bool DialogEngine::LoadCompiledProject(const std::string& binPath) {
    Log("Loading compiled project from: " + binPath);
    std::ifstream f(binPath, std::ios::binary | std::ios::ate);
    if (!f) return false;

    size_t size = f.tellg();
    std::string data(size, '\0');
    f.seekg(0); f.read(&data[0], size);
    data = XORBuffer(data);

    try {
        json j = json::parse(data);
        m_project.entities.clear();
        m_project.variables.clear();
        m_project.nodes.clear();

        if (j.contains("configs")) {
            auto& c = j["configs"];
            m_project.configs.start_node = c.value("start_node", "dialog_start");
            m_project.configs.reveal_speed = c.value("reveal_speed", 30.0f);
            m_project.configs.debug_mode = c.value("debug_mode", "none");
            m_project.configs.auto_save = c.value("auto_save", false);
            m_project.configs.encrypt_save = c.value("encrypt_save", false);
            m_project.configs.enable_floating = c.value("enable_floating", true);
            m_project.configs.enable_shadows = c.value("enable_shadows", true);
            m_project.configs.enable_vignette = c.value("enable_vignette", true);
            m_project.configs.save_prefix = c.value("save_prefix", "save_slot_");
            m_project.configs.max_slots = c.value("max_slots", 5);
            m_project.configs.mode = c.value("mode", "typewriter");
        }

        if (j.contains("entities")) {
            for (auto& [id, ent] : j["entities"].items()) {
                Entity entity;
                entity.id = id;
                entity.name = ent.value("name", id);
                entity.type = ent.value("type", "char");
                entity.default_pos_x = ent.value("default_pos_x", 0.5f);
                if (ent.contains("sprites")) {
                    for (auto& [expr, s] : ent["sprites"].items()) 
                        entity.sprites[expr] = { s.value("path", ""), s.value("frames", 1), s.value("speed", 5.0f), s.value("scale", 1.0f) };
                }
                m_project.entities[id] = entity;
            }
        }

        if (j.contains("variables")) {
            for (auto& [id, v] : j["variables"].items()) {
                VariableDef def;
                def.id = id;
                def.initial_value = v.value("initial", 0);
                m_project.variables[id] = def;
            }
        }

        if (j.contains("nodes")) {
            for (auto& [id, node] : j["nodes"].items()) {
                DialogNode n;
                n.entity = node.value("entity", "");
                n.content = node.value("content", "");
                if (node.contains("next_id") && !node["next_id"].is_null()) 
                    n.next_id = node.value("next_id", "");
                
                if (node.contains("metadata")) n.metadata = node["metadata"].get<std::unordered_map<std::string, std::string>>();
                if (node.contains("events")) {
                    for (auto& e : node["events"]) n.events.push_back({ e.value("op", "set"), e.value("var", ""), e.value("value", 0) });
                }
                if (node.contains("options")) {
                    for (auto& opt : node["options"]) {
                        DialogOption o;
                        o.content = opt.value("content", "");
                        o.next_id = opt.value("next_id", "");
                        o.style = opt.value("style", "normal");
                        
                        if (opt.contains("conditions")) {
                            for (auto& c : opt["conditions"]) o.conditions.push_back({ c.value("op", "=="), c.value("var", ""), c.value("value", 0) });
                        }
                        if (opt.contains("events")) {
                            for (auto& e : opt["events"]) o.events.push_back({ e.value("op", "set"), e.value("var", ""), e.value("value", 0) });
                        }
                        n.options.push_back(o);
                    }
                }
                m_project.nodes[id] = n;
            }
        }
        
        if (j.contains("backgrounds")) {
            for (auto& [id, p] : j["backgrounds"].items()) m_project.backgrounds[id] = p.get<std::string>();
        }
        if (j.contains("music")) {
            for (auto& [id, p] : j["music"].items()) m_project.music[id] = p.get<std::string>();
        }
        if (j.contains("sfx")) {
            for (auto& [id, p] : j["sfx"].items()) m_project.sfx[id] = p.get<std::string>();
        }
        
        for (auto const& [name, def] : m_project.variables) m_variables[name] = def.initial_value;

    } catch (...) { return false; }
    return true;
}

void DialogEngine::SaveGame(int slot) {
    Log("Saving game to slot " + std::to_string(slot));
    if (!m_currentNode) return;
    try {
        SaveData sd;
        sd.current_node_id = m_currentNodeId; // ID is already cached — no need to search all nodes
        sd.variables = m_variables;
        sd.meta.timestamp = GetTimestamp();
        sd.meta.node_id = sd.current_node_id;
        const auto* entity = GetCurrentEntity();
        sd.meta.entity_name = entity ? entity->name : "Unknown";
        std::string content = m_cachedInterpolatedContent;
        if (content.length() > 40) content = content.substr(0, 37) + "...";
        sd.meta.summary = content;
        json j;
        j["version"] = sd.version; j["node_id"] = sd.current_node_id; j["variables"] = sd.variables;
        j["active_bg"] = m_activeBg;
        j["active_bgm"] = m_activeBgm;
        json entities = json::object();
        for (const auto& [id, state] : m_activeEntities) {
            entities[id] = { {"expression", state.expression}, {"pos", state.pos} };
        }
        j["active_entities"] = entities;
        j["meta"] = { {"time", sd.meta.timestamp}, {"node", sd.meta.node_id}, {"char", sd.meta.entity_name}, {"text", sd.meta.summary} };
        std::string data = j.dump(4);
        if (m_project.configs.encrypt_save) data = XORBuffer(data);
        std::ofstream f(GetSlotPath(slot), std::ios::binary);
        if (f) f.write(data.c_str(), data.size());
    } catch (...) {}
}

bool DialogEngine::LoadGame(int slot) {
    std::ifstream f(GetSlotPath(slot), std::ios::binary | std::ios::ate);
    if (!f) { RecordError("LoadGame", "Save slot " + std::to_string(slot) + " not found."); return false; }
    std::streamsize sz = f.tellg(); f.seekg(0);
    std::string data(sz, '\0');
    if (f.read(&data[0], sz)) {
        try {
            if (m_project.configs.encrypt_save) data = XORBuffer(data);
            json j = json::parse(data);
            // Deterministic: start from defaults, then apply saved values
            for (auto const& [id, def] : m_project.variables) m_variables[id] = def.initial_value;
            
            if (j.contains("active_bg")) m_activeBg = j["active_bg"].get<std::string>();
            if (j.contains("active_bgm")) m_activeBgm = j["active_bgm"].get<std::string>();
            
            if (j.contains("active_entities") && j["active_entities"].is_object()) {
                m_activeEntities.clear();
                for (auto& [id, state] : j["active_entities"].items()) {
                    m_activeEntities[id] = { state.value("expression", "idle"), state.value("pos", "") };
                }
            }
            if (j.contains("variables") && j["variables"].is_object()) {
                for (auto& [id, val] : j["variables"].items()) {
                    if (m_project.variables.count(id)) m_variables[id] = val.get<int>();
                    else Log("LoadGame: ignoring unknown saved variable '" + id + "'", "WARN");
                }
            }
            std::string nodeId = j.value("node_id", "");
            if (nodeId.empty() || !m_project.nodes.count(nodeId)) {
                RecordError("LoadGame", "Saved node '" + nodeId + "' is invalid — starting from beginning.");
                StartDialog();
            } else {
                StartDialog(nodeId);
            }
            Log("Game loaded from slot " + std::to_string(slot));
            return true;
        } catch (const std::exception& ex) {
            RecordError("LoadGame", "Parse error in slot " + std::to_string(slot) + ": " + ex.what());
        }
    }
    return false;
}

bool DialogEngine::HasSave(int slot) const { 
    std::ifstream f(GetSlotPath(slot)); 
    return f.good(); 
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
            auto m = j["meta"]; return SaveMetadata{ m["time"], m["node"], m["char"], m["text"] };
        } catch (...) {}
    }
    return std::nullopt;
}

void DialogEngine::StartDialog(const std::string& startId) {
    std::string id = startId.empty() ? m_project.configs.start_node : startId;
    Log("Starting dialog sequence: " + id);
    if (!m_project.nodes.count(id)) {
        RecordError("StartDialog", "Node '" + id + "' not found. Dialog halted.");
        m_isActive = false; m_currentNode = nullptr;
        return;
    }
    m_currentNode = &m_project.nodes[id];
    m_currentNodeId = id;
    m_isActive = true;
    
    if (m_currentNode->metadata.count("bg")) m_activeBg = m_currentNode->metadata.at("bg");
    if (m_currentNode->metadata.count("bgm")) m_activeBgm = m_currentNode->metadata.at("bgm");
    
    // Scene management: Clear sprites unless this node is explicitly "joining" the current setup
    bool isJoining = m_currentNode->metadata.count("join") && m_currentNode->metadata.at("join") == "true";
    if (!isJoining) m_activeEntities.clear();
    
    // Auto-show/update speaker for backward compatibility or convenience
    if (!m_currentNode->entity.empty() && m_currentNode->entity != "system") {
        auto& state = m_activeEntities[m_currentNode->entity];
        if (m_currentNode->metadata.count("expression")) state.expression = m_currentNode->metadata.at("expression");
        if (m_currentNode->metadata.count("pos")) state.pos = m_currentNode->metadata.at("pos");
        if (m_currentNode->metadata.count("pos_x")) state.pos = m_currentNode->metadata.at("pos_x");
    }

    m_cachedInterpolatedContent = InterpolateVariables(m_currentNode->content);
    m_cachedParsedContent = RichTextParser::Parse(m_cachedInterpolatedContent);
    m_cachedTotalChars = m_cachedParsedContent.size();
    m_revealedCount = (m_project.configs.mode == "instant") ? (float)m_cachedTotalChars : 0.0f;
    m_waitTimer = 0.0f;
    ProcessEvents(m_currentNode->events);
    RefreshVisibleOptions();
    if (m_project.configs.auto_save) SaveGame(0);
}

void DialogEngine::SelectOption(int index) {
    if (!m_isActive || IsTextRevealing() || IsEventDelaying() || !m_pendingJumpId.empty() || index < 0 || index >= (int)m_visibleOptions.size()) return;
    ProcessEvents(m_visibleOptions[index].events);
    
    // If a jump was triggered during the event processing, we ignore the option's default next_id
    if (!m_pendingJumpId.empty() || IsEventDelaying()) return;

    std::string nextId = m_visibleOptions[index].next_id;
    if (nextId.empty() || nextId == "dialog_end") { m_isActive = false; m_currentNode = nullptr; }
    else StartDialog(nextId);
}

void DialogEngine::Next() {
    if (!m_isActive || IsEventDelaying() || !m_pendingJumpId.empty()) return;
    if (IsTextRevealing()) { SkipReveal(); return; }
    if (!m_visibleOptions.empty()) return;

    // If an event previously triggered a jump/delay, we shouldn't proceed normally
    std::string nId = m_currentNode->next_id.value_or("dialog_end");
    if (nId == "dialog_end") { m_isActive = false; m_currentNode = nullptr; }
    else StartDialog(nId);
}

void DialogEngine::Update(float dt) {
    if (!m_isActive || !m_currentNode || m_project.configs.mode == "instant") return;
    
    if (IsTextRevealing()) {
        if (m_waitTimer > 0.0f) {
            m_waitTimer -= dt;
        } else {
            int currentIdx = (int)m_revealedCount;
            if (currentIdx < (int)m_cachedParsedContent.size()) {
                if (m_cachedParsedContent[currentIdx].waitBefore > 0.0f) {
                    m_waitTimer = m_cachedParsedContent[currentIdx].waitBefore;
                    // Reset to avoid waiting again
                    m_cachedParsedContent[currentIdx].waitBefore = 0.0f;
                } else {
                    float spd = m_project.configs.reveal_speed * m_cachedParsedContent[currentIdx].speedMod;
                    m_revealedCount += spd * dt;
                    if (m_revealedCount > (float)m_cachedTotalChars) m_revealedCount = (float)m_cachedTotalChars;
                }
            }
        }
    }
    
    // Decay shake effect
    if (m_shakeIntensity > 0) m_shakeIntensity = std::max(0.0f, m_shakeIntensity - dt * 20.0f);

    if (m_engineDelayTimer > 0.0f) {
        m_engineDelayTimer -= dt;
        if (m_engineDelayTimer <= 0.0f) {
            m_engineDelayTimer = 0.0f;
            if (!m_pendingJumpId.empty()) {
                std::string jumpId = m_pendingJumpId;
                m_pendingJumpId = "";
                StartDialog(jumpId);
            } else {
                // If it was just a delay with no jump, we just let the engine unpause.
                // We should also clear any option states or force a Next if we had no options.
            }
        }
        return; // Pause the engine processing while delayed
    }

    if (!m_pendingJumpId.empty()) {
        std::string jumpId = m_pendingJumpId;
        m_pendingJumpId = "";
        StartDialog(jumpId);
        return;
    }
}

void DialogEngine::SkipReveal() { 
    if (m_currentNode) {
        m_revealedCount = (float)m_cachedTotalChars; 
        m_waitTimer = 0.0f;
    }
}
bool DialogEngine::IsTextRevealing() const { return m_isActive && m_revealedCount < (float)m_cachedTotalChars; }

std::string DialogEngine::GetVisibleContent() const {
    if (!m_currentNode) return "";
    std::string out = "";
    int limit = (int)m_revealedCount;
    for (int i = 0; i < limit && i < (int)m_cachedParsedContent.size(); ++i) {
        out += m_cachedParsedContent[i].ch;
    }
    return out;
}

size_t DialogEngine::GetUTF8Length(const std::string& s) const {
    size_t count = 0;
    for (size_t i = 0; i < s.length(); ) {
        unsigned char c = s[i]; i += (c <= 127) ? 1 : ((c & 0xE0) == 0xC0) ? 2 : ((c & 0xF0) == 0xE0) ? 3 : 4;
        count++;
    }
    return count;
}

std::string DialogEngine::InterpolateVariables(const std::string& text) const {
    std::string res;
    res.reserve(text.size()); 
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '{') {
            size_t endIdx = text.find('}', i);
            if (endIdx != std::string::npos) {
                std::string varName = text.substr(i + 1, endIdx - i - 1);
                std::string val = std::to_string(GetVariable(varName));
                // Escape brackets in variable values to prevent tag injection
                for (char c : val) {
                    if (c == '[') res += "\\[";
                    else if (c == ']') res += "\\]";
                    else res += c;
                }
                i = endIdx;
                continue;
            }
        }
        res += text[i];
    }
    return res;
}

int DialogEngine::GetVariable(const std::string& name) const { auto it = m_variables.find(name); return (it != m_variables.end()) ? it->second : 0; }

void DialogEngine::SetVariable(const std::string& name, int value) {
    auto it = m_project.variables.find(name);
    if (it == m_project.variables.end()) {
        RecordError("SetVariable", "Variable '" + name + "' is not declared — ignoring.");
        return;
    }
    const auto& d = it->second;
    int v = value;
    if (d.min) v = std::max(v, *d.min);
    if (d.max) v = std::min(v, *d.max);
    m_variables[name] = v;
}

const std::vector<std::string>& DialogEngine::ConsumePendingSFX() {
    static std::vector<std::string> copy;
    copy = m_pendingSFX;
    m_pendingSFX.clear();
    return copy;
}

void DialogEngine::ProcessEvents(const std::vector<Event>& events) {
    static const std::unordered_set<std::string> VALID_OPS = {
        "set", "add", "sub", "mul", "shake", "random", "play_sfx", "jump", "delay", 
        "show_sprite", "hide_sprite", "pos_sprite", "clear_sprites",
        "expression", "pos", "hide" // Unified names
    };
    for (const auto& e : events) {
        if (!VALID_OPS.count(e.op)) { RecordError("ProcessEvents", "Unknown op '" + e.op + "' — skipping."); continue; }
        
        // FX / Flow Events
        if (e.op == "shake") { TriggerShake((float)e.value); continue; }
        if (e.op == "play_sfx") { m_pendingSFX.push_back(e.var); continue; }
        if (e.op == "jump") { m_pendingJumpId = e.var; continue; }
        if (e.op == "delay") { m_engineDelayTimer = (float)e.value / 1000.0f; continue; }
        if (e.op == "clear_sprites") { m_activeEntities.clear(); continue; }
        
        // Sprite Management (Unified + Legacy)
        if (e.op == "show_sprite" || e.op == "expression") {
            m_activeEntities[e.var].expression = e.value_str.empty() ? "idle" : e.value_str;
            continue;
        }
        if (e.op == "hide_sprite" || e.op == "hide") {
            m_activeEntities.erase(e.var);
            continue;
        }
        if (e.op == "pos_sprite" || e.op == "pos") {
            m_activeEntities[e.var].pos = e.value_str;
            continue;
        }

        // Variable Operations
        if (!e.var.empty() && !m_project.variables.count(e.var)) {
            RecordError("ProcessEvents", "Event op '" + e.op + "' references undeclared variable '" + e.var + "' — skipping.");
            continue;
        }
        int cur = GetVariable(e.var);
        int next = cur;
        if      (e.op == "set") next = e.value;
        else if (e.op == "add") next = cur + e.value;
        else if (e.op == "sub") next = cur - e.value;
        else if (e.op == "mul") next = cur * e.value;
        else if (e.op == "random") {
            int minVal = std::min(e.value, e.value_max);
            int maxVal = std::max(e.value, e.value_max);
            next = minVal + (std::rand() % (maxVal - minVal + 1));
        }
        SetVariable(e.var, next);
        m_eventTrace.push_back({ m_currentNodeId, e.op, e.var, cur, GetVariable(e.var) });
        Log("Event: " + e.op + " '" + e.var + "' " + std::to_string(cur) + " -> " + std::to_string(GetVariable(e.var)));
    }
}

void DialogEngine::RecordError(const std::string& context, const std::string& msg) {
    std::string full = "[" + context + "] " + msg;
    m_errors.push_back(full);
    Log(full, "ERROR");
}

void DialogEngine::Log(const std::string& msg, const std::string& level) {
    std::string mode = m_project.configs.debug_mode;
    bool isError = (level == "ERROR");
    if (mode == "none" && !isError) return;

    std::string tag = "[BitEngine:" + level + "] ";

    if (isError || mode == "debug_overlay" || mode == "debug_all") {
        std::cout << tag << msg << std::endl;
    }
    if (isError || mode == "debug_file" || mode == "debug_all") {
        // Open once per call — not a hot path since this only runs in debug modes
        std::ofstream logFile("debug.log", std::ios_base::app);
        if (logFile) {
            auto t = std::time(nullptr);
            logFile << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S")
                    << " | " << tag << msg << "\n";
        }
    }
}

ValidationResult DialogEngine::ValidateProject(const DialogProject& p) {
    ValidationResult errors;
    if (!p.nodes.count(p.configs.start_node))
        errors.push_back({"configs", "start_node", "Start node '" + p.configs.start_node + "' does not exist."});
    for (const auto& [nodeId, node] : p.nodes) {
        if (!node.entity.empty() && node.entity != "system" && !p.entities.count(node.entity))
            errors.push_back({nodeId, "entity", "Entity '" + node.entity + "' is not defined."});
        if (node.next_id && *node.next_id != "dialog_end" && !node.next_id->empty() && !p.nodes.count(*node.next_id))
            errors.push_back({nodeId, "next_id", "next_id '" + *node.next_id + "' does not exist."});
        for (const auto& e : node.events)
            if (e.op != "shake" && e.op != "play_sfx" && e.op != "jump" && e.op != "delay" && 
                e.op != "show_sprite" && e.op != "hide_sprite" && e.op != "pos_sprite" && e.op != "clear_sprites" &&
                e.op != "expression" && e.op != "pos" && e.op != "hide" &&
                !e.var.empty() && !p.variables.count(e.var))
                errors.push_back({nodeId, "events", "Event references undeclared variable '" + e.var + "'."});
        for (const auto& opt : node.options) {
            if (!opt.next_id.empty() && opt.next_id != "dialog_end" && !p.nodes.count(opt.next_id))
                errors.push_back({nodeId, "options.next_id", "Option next_id '" + opt.next_id + "' does not exist."});
            for (const auto& c : opt.conditions)
                if (!p.variables.count(c.var))
                    errors.push_back({nodeId, "options.conditions", "Condition references undeclared variable '" + c.var + "'."});
            for (const auto& e : opt.events)
                if (e.op != "shake" && e.op != "play_sfx" && e.op != "jump" && e.op != "delay" && 
                    e.op != "show_sprite" && e.op != "hide_sprite" && e.op != "pos_sprite" && e.op != "clear_sprites" &&
                    e.op != "expression" && e.op != "pos" && e.op != "hide" &&
                    !e.var.empty() && !p.variables.count(e.var))
                    errors.push_back({nodeId, "options.events", "Option event references undeclared variable '" + e.var + "'."});
        }
    }
    return errors;
}

void DialogEngine::RefreshVisibleOptions() {
    m_visibleOptions.clear();
    for (auto& o : m_currentNode->options) {
        bool pass = true;
        for (const auto& c : o.conditions) {
            int v = GetVariable(c.var);
            if ((c.op == "=" || c.op == "==") && v != c.value) pass = false;
            else if (c.op == "!=" && v == c.value) pass = false;
            else if (c.op == ">" && v <= c.value) pass = false;
            else if (c.op == "<" && v >= c.value) pass = false;
            else if (c.op == ">=" && v < c.value) pass = false;
            else if (c.op == "<=" && v > c.value) pass = false;
            else if (c.op != "==" && c.op != "=" && c.op != "!=" && c.op != ">" && c.op != "<" && c.op != ">=" && c.op != "<=") {
                // If operator is unknown, actually fail the condition rather than passing it.
                pass = false;
            }
            if (!pass) break;
        }
        if (pass) m_visibleOptions.push_back(o);
    }
}

const Entity* DialogEngine::GetCurrentEntity() const {
    if (!m_currentNode) return nullptr;
    auto it = m_project.entities.find(m_currentNode->entity);
    return (it != m_project.entities.end()) ? &it->second : nullptr;
}

std::string DialogEngine::XORBuffer(const std::string& d) const { std::string o = d; for (size_t i = 0; i < d.size(); ++i) o[i] ^= KEY[i % KEY.size()]; return o; }
std::string DialogEngine::GetSlotPath(int s) const { return "save/" + m_project.configs.save_prefix + std::to_string(s) + ".bin"; }
std::string DialogEngine::GetTimestamp() const {
    std::time_t t = std::time(nullptr); std::tm tm = *std::localtime(&t);
    std::stringstream ss; ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S"); return ss.str();
}
