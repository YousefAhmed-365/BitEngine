#include "headers/BitEngine.hpp"
#include "headers/BitJsonInterpreter.hpp"
#include "headers/BitScriptInterpreter.hpp"
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
    std::string currentFont = "";
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
                } else if (tag.find("font=") == 0) {
                    currentFont = tag.substr(5);
                } else if (tag == "/font") {
                    currentFont = "";
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
        rc.font = currentFont;
        rc.waitBefore = accumulatedWait;
        
        accumulatedWait = 0.0f; // Reset wait after applying to this character
        result.push_back(rc);
    }
    return result;
}

//// Implementation moved to BitJsonInterpreter.cpp

// DialogEngine
DialogEngine::DialogEngine() {}

bool DialogEngine::LoadProject(const std::string& configFilePath) {
    Log("Loading project from: " + configFilePath);

    if (configFilePath.size() > 10 && configFilePath.substr(configFilePath.size() - 10) == ".bitscript") {
        if (!BitScriptInterpreter::LoadScriptFile(configFilePath, m_project)) return false;
        for (auto const& [id, def] : m_project.variables) m_variables[id] = def.initial_value;
        return true;
    }

    if (configFilePath.size() > 5 && configFilePath.substr(configFilePath.size() - 5) == ".bitc") {
        return LoadBytecodeFile(configFilePath);
    }

    m_project = BitJsonInterpreter::ParseConfig(configFilePath);
    
    for (const auto& f : m_project.configs.entity_files)
        if (!BitJsonInterpreter::LoadEntitiesFile(f, m_project)) RecordError("LoadProject", "Could not load entities file: " + f);
    for (const auto& f : m_project.configs.variable_files)
        if (!BitJsonInterpreter::LoadVariablesFile(f, m_project)) RecordError("LoadProject", "Could not load variables file: " + f);
    for (const auto& f : m_project.configs.asset_files)
        if (!BitJsonInterpreter::LoadAssetsFile(f, m_project)) RecordError("LoadProject", "Could not load assets file: " + f);
    for (const auto& f : m_project.configs.dialog_files)
        if (!BitJsonInterpreter::LoadDialogFile(f, m_project)) RecordError("LoadProject", "Could not load dialog file: " + f);
    
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

    // Serialize Nodes (store params JSON verbatim — round-trip safe)
    for (auto const& [id, n] : m_project.nodes) {
        json meta;
        meta["bg"]                  = n.metadata.bg;
        meta["bgm"]                 = n.metadata.bgm;
        meta["hide_ui"]             = n.metadata.hide_ui;
        meta["auto_next"]           = n.metadata.auto_next;
        meta["join"]                = n.metadata.join;
        meta["pre_delay"]           = n.metadata.pre_delay;
        meta["alpha"]               = n.metadata.alpha;
        meta["expression"]          = n.metadata.expression;
        meta["pos"]                 = n.metadata.pos;
        meta["transition"]          = n.metadata.transition;
        meta["transition_duration"] = n.metadata.transition_duration;
        meta["transition_delay"]    = n.metadata.transition_delay;
        json nj;
        nj["entity"]   = n.entity;
        nj["content"]  = n.content;
        nj["next_id"]  = n.next_id;
        nj["metadata"] = meta;
        for (auto const& o : n.options) {
            json oj;
            oj["content"] = o.content;
            oj["next_id"] = o.next_id;
            oj["style"]   = o.style;
            // Conditions stored as plain leaf JSON (groups handled recursively below if needed)
            for (auto const& c : o.conditions)
                if (!c.isGroup) oj["conditions"].push_back({{ "var", c.leaf.var }, { "op", c.leaf.op }, { "value", c.leaf.value }, { "ref", c.leaf.ref }});
            // Events: store params verbatim
            for (auto const& e : o.events) oj["events"].push_back(e.params);
            nj["options"].push_back(oj);
        }
        for (auto const& e : n.events) nj["events"].push_back(e.params);
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

// ── BitScript VM Bytecode I/O ─────────────────────────────────────────────────

static const char* BITC_MAGIC = "BITC";
static const int   BITC_VERSION = 1;

static std::string OpToStr(BitOp op) {
    switch(op) {
        case BitOp::TEXT:    return "TEXT";
        case BitOp::SAY:     return "SAY";
        case BitOp::CHOICE:  return "CHOICE";
        case BitOp::IF:      return "IF";
        case BitOp::IF_REF:  return "IF_REF";
        case BitOp::GOTO:    return "GOTO";
        case BitOp::SET:     return "SET";
        case BitOp::SET_REF: return "SET_REF";
        case BitOp::ADD:     return "ADD";
        case BitOp::ADD_REF: return "ADD_REF";
        case BitOp::SUB:     return "SUB";
        case BitOp::SUB_REF: return "SUB_REF";
        case BitOp::MUL:     return "MUL";
        case BitOp::MUL_REF: return "MUL_REF";
        case BitOp::DIV:     return "DIV";
        case BitOp::DIV_REF: return "DIV_REF";
        case BitOp::EVENT:   return "EVENT";
        case BitOp::BG:      return "BG";
        case BitOp::BGM:     return "BGM";
        case BitOp::LABEL:   return "LABEL";
        case BitOp::HALT:    return "HALT";
        default:             return "NOP";
    }
}

static BitOp StrToOp(const std::string& s) {
    if (s=="TEXT")    return BitOp::TEXT;
    if (s=="SAY")     return BitOp::SAY;
    if (s=="CHOICE")  return BitOp::CHOICE;
    if (s=="IF")      return BitOp::IF;
    if (s=="IF_REF")  return BitOp::IF_REF;
    if (s=="GOTO")    return BitOp::GOTO;
    if (s=="SET")     return BitOp::SET;
    if (s=="SET_REF") return BitOp::SET_REF;
    if (s=="ADD")     return BitOp::ADD;
    if (s=="ADD_REF") return BitOp::ADD_REF;
    if (s=="SUB")     return BitOp::SUB;
    if (s=="SUB_REF") return BitOp::SUB_REF;
    if (s=="MUL")     return BitOp::MUL;
    if (s=="MUL_REF") return BitOp::MUL_REF;
    if (s=="DIV")     return BitOp::DIV;
    if (s=="DIV_REF") return BitOp::DIV_REF;
    if (s=="EVENT")   return BitOp::EVENT;
    if (s=="BG")      return BitOp::BG;
    if (s=="BGM")     return BitOp::BGM;
    if (s=="LABEL")   return BitOp::LABEL;
    if (s=="HALT")    return BitOp::HALT;
    return BitOp::HALT;
}

bool DialogEngine::SaveBytecode(const std::string& path) const {
    if (m_project.bytecode.empty()) {
        Log("SaveBytecode: no bytecode to save.", "WARN");
        return false;
    }
    json root;
    root["magic"]   = BITC_MAGIC;
    root["version"] = BITC_VERSION;

    // Project metadata (entities, assets, variables, configs)
    root["configs"]["start_node"]    = m_project.configs.start_node;
    root["configs"]["mode"]          = m_project.configs.mode;
    root["configs"]["reveal_speed"]  = m_project.configs.reveal_speed;
    root["configs"]["auto_play_delay"]= m_project.configs.auto_play_delay;
    root["configs"]["auto_save"]     = m_project.configs.auto_save;

    for (auto& [k,v] : m_project.backgrounds) root["backgrounds"][k] = v;
    for (auto& [k,v] : m_project.music)        root["music"][k]       = v;
    for (auto& [k,v] : m_project.sfx)          root["sfx"][k]         = v;
    for (auto& [k,v] : m_project.fonts)        root["fonts"][k]       = v;
    for (auto& [k,v] : m_project.variables)
        root["variables"][k] = { {"id", v.id}, {"initial_value", v.initial_value} };
    for (auto& [k,e] : m_project.entities) {
        json ej = { {"id",e.id},{"name",e.name},{"type",e.type},{"default_pos_x",e.default_pos_x} };
        for (auto& [sn,sd] : e.sprites)
            ej["sprites"][sn] = { {"path",sd.path},{"frames",sd.frames},{"speed",sd.speed},{"scale",sd.scale} };
        root["entities"][k] = ej;
    }

    // Bytecode instructions
    json bc = json::array();
    for (const auto& ins : m_project.bytecode) {
        json inj;
        inj["op"]   = OpToStr(ins.op);
        inj["args"] = ins.args;
        if (!ins.metadata.empty()) inj["meta"] = ins.metadata;
        bc.push_back(inj);
    }
    root["bytecode"] = bc;

    std::string data = root.dump(2);
    std::ofstream f(path);
    if (!f) { Log("SaveBytecode: failed to open '" + path + "' for writing.", "ERROR"); return false; }
    f << data;
    Log("Saved bytecode to: " + path + " (" + std::to_string(m_project.bytecode.size()) + " instructions)");
    return true;
}

bool DialogEngine::LoadBytecodeFile(const std::string& path) {
    Log("Loading bytecode file: " + path);
    std::ifstream f(path);
    if (!f) { Log("LoadBytecodeFile: file not found: " + path, "ERROR"); return false; }
    json root;
    try { root = json::parse(f); }
    catch (const std::exception& e) { Log(std::string("LoadBytecodeFile: parse error: ") + e.what(), "ERROR"); return false; }

    if (root.value("magic","") != BITC_MAGIC) { Log("LoadBytecodeFile: invalid magic.", "ERROR"); return false; }

    // Restore project metadata
    if (root.contains("configs")) {
        auto& c = root["configs"];
        m_project.configs.start_node    = c.value("start_node","dialog_start");
        m_project.configs.mode          = c.value("mode","typewriter");
        m_project.configs.reveal_speed  = c.value("reveal_speed", 45.0f);
        m_project.configs.auto_play_delay= c.value("auto_play_delay",2.0f);
        m_project.configs.auto_save     = c.value("auto_save",false);
    }
    if (root.contains("backgrounds")) for (auto& [k,v] : root["backgrounds"].items()) m_project.backgrounds[k] = v;
    if (root.contains("music"))        for (auto& [k,v] : root["music"].items())        m_project.music[k]       = v;
    if (root.contains("sfx"))          for (auto& [k,v] : root["sfx"].items())          m_project.sfx[k]         = v;
    if (root.contains("fonts"))        for (auto& [k,v] : root["fonts"].items())        m_project.fonts[k]       = v;
    if (root.contains("variables"))
        for (auto& [k,vj] : root["variables"].items()) {
            VariableDef vd; vd.id = vj.value("id",k); vd.initial_value = vj.value("initial_value",0);
            m_project.variables[k] = vd; m_variables[k] = vd.initial_value;
        }
    if (root.contains("entities"))
        for (auto& [k,ej] : root["entities"].items()) {
            Entity e; e.id = ej.value("id",k); e.name = ej.value("name","Unknown");
            e.type = ej.value("type","char"); e.default_pos_x = ej.value("default_pos_x",0.5f);
            if (ej.contains("sprites"))
                for (auto& [sn,sd] : ej["sprites"].items()) {
                    SpriteDef sdef; sdef.path = sd.value("path","");
                    sdef.frames = sd.value("frames",1); sdef.speed = sd.value("speed",5.0f); sdef.scale = sd.value("scale",1.0f);
                    e.sprites[sn] = sdef;
                }
            m_project.entities[k] = e;
        }

    // Restore bytecode
    if (!root.contains("bytecode")) { Log("LoadBytecodeFile: no bytecode array.", "ERROR"); return false; }
    m_project.bytecode.clear();
    for (auto& inj : root["bytecode"]) {
        BitInstruction ins;
        ins.op   = StrToOp(inj.value("op","HALT"));
        ins.args = inj.value("args", std::vector<std::string>{});
        if (inj.contains("meta")) ins.metadata = inj["meta"];
        m_project.bytecode.push_back(ins);
    }
    Log("Loaded " + std::to_string(m_project.bytecode.size()) + " bytecode instructions from " + path);
    return true;
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
            // Reuse the same parsing helpers from LoadDialogFile logic inline
            auto parseEvent = [](const json& e) -> Event { return { e.value("op", ""), e }; };
            std::function<ConditionNode(const json&)> parseCond = [&](const json& c) -> ConditionNode {
                ConditionNode node;
                if (c.contains("or") && c["or"].is_array()) {
                    node.isGroup = true; node.groupLogic = "or";
                    for (auto& ch : c["or"]) node.children.push_back(parseCond(ch));
                } else if (c.contains("and") && c["and"].is_array()) {
                    node.isGroup = true; node.groupLogic = "and";
                    for (auto& ch : c["and"]) node.children.push_back(parseCond(ch));
                } else {
                    std::string op = c.value("op", "=="); if (op == "=") op = "==";
                    node.leaf = { c.value("var", ""), op, c.value("value", 0) };
                }
                return node;
            };
            for (auto& [id, node] : j["nodes"].items()) {
                DialogNode n;
                n.entity  = node.value("entity", "");
                n.content = node.value("content", "");
                if (node.contains("next_id") && !node["next_id"].is_null())
                    n.next_id = node.value("next_id", "");
                if (node.contains("metadata")) {
                    auto& m = node["metadata"];
                    n.metadata.bg                  = m.value("bg", "");
                    n.metadata.bgm                 = m.value("bgm", "");
                    n.metadata.hide_ui             = m.value("hide_ui", false);
                    n.metadata.auto_next           = m.value("auto_next", false);
                    n.metadata.join                = m.value("join", false);
                    n.metadata.pre_delay           = m.value("pre_delay", 0);
                    n.metadata.alpha               = m.value("alpha", 1.0f);
                    n.metadata.expression          = m.value("expression", "");
                    n.metadata.pos                 = m.value("pos", "");
                    n.metadata.transition          = m.value("transition", "");
                    n.metadata.transition_duration = m.value("transition_duration", 600);
                    n.metadata.transition_delay    = m.value("transition_delay", 0);
                }
                if (node.contains("events"))
                    for (auto& e : node["events"]) n.events.push_back(parseEvent(e));
                if (node.contains("options")) {
                    for (auto& opt : node["options"]) {
                        DialogOption o;
                        o.content = opt.value("content", "");
                        o.next_id = opt.value("next_id", "");
                        o.style   = opt.value("style", "normal");
                        if (opt.contains("conditions"))
                            for (auto& c : opt["conditions"]) o.conditions.push_back(parseCond(c));
                        if (opt.contains("events"))
                            for (auto& e : opt["events"]) o.events.push_back(parseEvent(e));
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
            entities[id] = { 
                {"expression", state.expression}, 
                {"pos", state.pos},
                {"normX", state.currentNormX},
                {"alpha", state.alpha}
            };
        }
        j["active_entities"] = entities;
        j["ui_hidden"] = m_isUiHidden;
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
                for (auto& [id, s] : j["active_entities"].items()) {
                    ActiveEntityState state;
                    state.expression = s.value("expression", "idle");
                    state.pos = s.value("pos", "center");
                    state.currentNormX = s.value("normX", 0.5f);
                    state.targetNormX = state.currentNormX;
                    state.startNormX = state.currentNormX;
                    state.alpha = s.value("alpha", 1.0f);
                    state.targetAlpha = state.alpha;
                    state.startAlpha = state.alpha;
                    m_activeEntities[id] = state;
                }
            }
            if (j.contains("ui_hidden")) m_isUiHidden = j["ui_hidden"].get<bool>();
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
    if (!m_project.bytecode.empty()) {
        m_isActive = true;
        m_pc = 0;
        m_vmWaiting = false;
        // If a label was provided, find it
        if (!startId.empty()) {
            for (int i = 0; i < (int)m_project.bytecode.size(); ++i) {
                if (m_project.bytecode[i].op == BitOp::LABEL && m_project.bytecode[i].args[0] == startId) {
                    m_pc = i; break;
                }
            }
        }
        RunVM();
        return;
    }

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
    
    auto& meta = m_currentNode->metadata;

    // Background
    if (!meta.bg.empty() && m_activeBg != meta.bg) {
        m_prevBg   = m_activeBg;
        m_activeBg = meta.bg;
        m_bgFadeAlpha   = 0.0f;
        m_bgFadeTimer   = 0.0f;
        m_bgFadeDuration = 0.8f;
    }
    if (!meta.bgm.empty()) m_activeBgm = meta.bgm;

    // UI flags (typed booleans, no string coercion)
    m_isUiHidden = meta.hide_ui;
    m_isAutoNext = meta.auto_next;
    if (meta.pre_delay > 0) m_engineDelayTimer = meta.pre_delay / 1000.0f;

    // Scene management: Smart joining
    bool isJoining = meta.join;
    if (!isJoining && !m_currentNode->entity.empty()) {
        if (m_currentNode->entity == "system") isJoining = true;
        else if (m_activeEntities.count(m_currentNode->entity)) isJoining = true;
    }
    if (!isJoining) m_activeEntities.clear();

    // History entry
    if (m_currentNode && !m_currentNode->content.empty()) {
        std::string speakerName = "SYSTEM";
        if (!m_currentNode->entity.empty() && m_currentNode->entity != "system") {
            if (m_project.entities.count(m_currentNode->entity))
                speakerName = m_project.entities.at(m_currentNode->entity).name;
            else speakerName = "???";
        }
        std::string finalContent = InterpolateVariables(m_currentNode->content);
        if (m_history.empty() || m_history.back().content != finalContent) {
            HistoryEntry entry;
            entry.speaker     = speakerName;
            entry.content     = finalContent;
            entry.richContent = RichTextParser::Parse(finalContent);
            m_history.push_back(entry);
            if (m_history.size() > 100) m_history.erase(m_history.begin());
        }
    }

    // Entity initial state from typed metadata
    if (!m_currentNode->entity.empty() && m_currentNode->entity != "system") {
        auto& state = m_activeEntities[m_currentNode->entity];
        if (!meta.expression.empty()) state.expression = meta.expression;
        if (!meta.pos.empty()) {
            state.pos        = meta.pos;
            state.targetNormX = ParsePosition(meta.pos);
            if (state.moveDuration <= 0.0f) state.currentNormX = state.targetNormX;
        }
        // Only apply alpha if the metadata explicitly sets it below 1.0
        // (alpha == 1.0f is the default so we always apply it to allow reset)
        state.alpha      = meta.alpha;
        state.targetAlpha = meta.alpha;
        state.startAlpha  = meta.alpha;
    }

    m_cachedInterpolatedContent = InterpolateVariables(m_currentNode->content);
    m_cachedParsedContent = RichTextParser::Parse(m_cachedInterpolatedContent);
    m_cachedTotalChars = m_cachedParsedContent.size();
    m_revealedCount = (m_project.configs.mode == "instant") ? (float)m_cachedTotalChars : 0.0f;
    m_waitTimer = 0.0f;

    // Reset alpha/pos for new entities if they weren't already active
    for (auto& [id, state] : m_activeEntities) {
        if (state.moveDuration <= 0.0f) {
            state.currentNormX = state.targetNormX;
            state.startNormX = state.targetNormX;
        }
        if (state.fadeDuration <= 0.0f) {
            state.alpha = state.targetAlpha;
            state.startAlpha = state.targetAlpha;
        }
    }

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
    m_visibleOptions.clear();

    if (!m_project.bytecode.empty()) {
        m_vmWaiting = false;
        // Jump to label
        for (int i = 0; i < (int)m_project.bytecode.size(); ++i) {
            if (m_project.bytecode[i].op == BitOp::LABEL && m_project.bytecode[i].args[0] == nextId) {
                m_pc = i; break;
            }
        }
        RunVM();
        return;
    }

    if (nextId.empty() || nextId == "dialog_end") { m_isActive = false; m_currentNode = nullptr; }
    else StartDialog(nextId);
}

void DialogEngine::Next() {
    if (!m_isActive || IsEventDelaying() || !m_pendingJumpId.empty() || m_isTransitioning) return;
    if (IsTextRevealing()) { SkipReveal(); return; }
    
    m_isAutoNext = false; // Consume the auto-advance trigger
    if (!m_visibleOptions.empty()) return;

    // Check for transitions (shared between JSON and VM paths)
    if (m_currentNode && !m_currentNode->metadata.transition.empty() && !m_isTransitioning) {
        const auto& meta = m_currentNode->metadata;
        if (meta.transition == "fade_black") {
            m_isTransitioning      = true;
            m_transitionState      = 0;
            m_transitionTargetNode = !m_project.bytecode.empty() ? "vm_resume"
                                     : m_currentNode->next_id.value_or("dialog_end");
            m_transitionDurationVal  = meta.transition_duration / 1000.0f;
            m_transitionPostDelay    = meta.transition_delay    / 1000.0f;
            m_screenFadeTarget   = 1.0f;
            m_screenFadeStart    = m_screenFadeAlpha;
            m_screenFadeDuration = m_transitionDurationVal;
            m_screenFadeTimer    = 0.0f;
            m_isUiHidden = true;
            return;
        }
    }

    if (!m_project.bytecode.empty()) {
        m_vmWaiting = false;
        RunVM();
        return;
    }

    std::string nId = m_currentNode->next_id.value_or("dialog_end");
    if (nId == "dialog_end") { m_isActive = false; m_currentNode = nullptr; }
    else StartDialog(nId);
}

void DialogEngine::Update(float dt) {
    if (!m_isActive || !m_currentNode || m_project.configs.mode == "instant") return;
    
    // 1. Transition and Screen Fade updates (must always run, even mid-transition)
    if (m_isTransitioning) {
        if (m_transitionState == 0) {
            // Fading to black — wait until fully dark
            if (m_screenFadeAlpha >= 0.98f) {
                m_transitionState = 1;
                m_screenFadeTarget   = 0.0f;
                m_screenFadeStart    = 1.0f;
                m_screenFadeDuration = m_transitionDurationVal;
                m_screenFadeTimer    = 0.0f;
                if (m_transitionTargetNode == "dialog_end") {
                    m_isActive = false; m_currentNode = nullptr; m_isTransitioning = false;
                } else if (m_transitionTargetNode != "vm_resume") {
                    // JSON path: load the next node while the screen is black
                    StartDialog(m_transitionTargetNode);
                }
                // vm_resume: the VM will continue once the fade-out is done
            }
        } else if (m_transitionState == 1) {
            // Fading back in — wait until visible again
            if (m_screenFadeAlpha <= 0.02f) {
                m_screenFadeAlpha = 0.0f;
                if (m_transitionPostDelay > 0.0f) {
                    m_transitionState = 2;
                    m_transitionTimer = 0.0f;
                } else {
                    m_isTransitioning = false;
                    m_isUiHidden = false;
                    if (m_transitionTargetNode == "vm_resume") {
                        m_vmWaiting = false;
                        m_vmDelayed = false;
                        RunVM();
                    }
                }
            }
        } else if (m_transitionState == 2) {
            // Post-delay before revealing UI
            m_transitionTimer += dt;
            if (m_transitionTimer >= m_transitionPostDelay) {
                m_isTransitioning = false;
                m_isUiHidden = false;
                if (m_transitionTargetNode == "vm_resume") {
                    m_vmWaiting = false;
                    m_vmDelayed = false;
                    RunVM();
                }
            }
        }
    }

    if (m_screenFadeTimer < m_screenFadeDuration) {
        m_screenFadeTimer += dt;
        float t = std::min(1.0f, m_screenFadeTimer / m_screenFadeDuration);
        m_screenFadeAlpha = m_screenFadeStart + (m_screenFadeTarget - m_screenFadeStart) * t;
        if (m_screenFadeTimer >= m_screenFadeDuration) m_screenFadeAlpha = m_screenFadeTarget;
    }

    // 2. Pause normal narrative processing while transitioning
    if (m_isTransitioning) return;

    // 3. Normal Narrative Processing
    if (IsTextRevealing()) {
        if (m_waitTimer > 0.0f) {
            m_waitTimer -= dt;
        } else {
            int currentIdx = (int)m_revealedCount;
            if (currentIdx >= 0 && currentIdx < (int)m_cachedParsedContent.size()) {
                if (m_cachedParsedContent[currentIdx].waitBefore > 0.0f) {
                    m_waitTimer = m_cachedParsedContent[currentIdx].waitBefore;
                    m_cachedParsedContent[currentIdx].waitBefore = 0.0f;
                } else {
                    float spd = m_project.configs.reveal_speed * m_cachedParsedContent[currentIdx].speedMod;
                    m_revealedCount += spd * dt;
                    if (m_revealedCount > (float)m_cachedTotalChars) m_revealedCount = (float)m_cachedTotalChars;
                }
            }
        }
    }
    
    if (m_shakeIntensity > 0) m_shakeIntensity = std::max(0.0f, m_shakeIntensity - dt * 20.0f);

    if (!IsTextRevealing() && m_isAutoNext && m_waitTimer <= 0.0f && m_visibleOptions.empty() && !m_isTransitioning && !IsEventDelaying()) {
        Next();
    }

    // Feature: Auto-Play logic
    if (m_isAutoPlaying && !IsTextRevealing() && m_visibleOptions.empty() && !m_isTransitioning && m_waitTimer <= 0.0f) {
        m_autoPlayTimer += dt;
        if (m_autoPlayTimer >= m_project.configs.auto_play_delay) {
            m_autoPlayTimer = 0.0f;
            Next();
        }
    } else {
        m_autoPlayTimer = 0.0f;
    }

    if (m_bgFadeAlpha < 1.0f) {
        m_bgFadeTimer += dt;
        m_bgFadeAlpha = std::min(1.0f, m_bgFadeTimer / m_bgFadeDuration);
    }

    // Entity Interpolation
    for (auto& [id, state] : m_activeEntities) {
        // Movement
        if (state.moveTimer < state.moveDuration) {
            state.moveTimer += dt;
            float t = std::min(1.0f, state.moveTimer / state.moveDuration);
            // Cubic Ease-out
            t = 1.0f - powf(1.0f - t, 3.0f); 
            state.currentNormX = state.startNormX + (state.targetNormX - state.startNormX) * t;
            if (state.moveTimer >= state.moveDuration) state.currentNormX = state.targetNormX;
        }

        // Fading
        if (state.fadeTimer < state.fadeDuration) {
            state.fadeTimer += dt;
            float t = std::min(1.0f, state.fadeTimer / state.fadeDuration);
            state.alpha = state.startAlpha + (state.targetAlpha - state.startAlpha) * t;
            if (state.fadeTimer >= state.fadeDuration) state.alpha = state.targetAlpha;
        }
    }

    if (m_engineDelayTimer > 0.0f) {
        m_engineDelayTimer -= dt;
        if (m_engineDelayTimer <= 0.0f) {
            m_engineDelayTimer = 0.0f;

            // If this was a pre_delay guard for a SAY, start the reveal now
            if (m_revealedCount < 0.0f) {
                m_revealedCount = 0.0f;
                // Don't return — let auto_next fire this frame if applicable
            }

            if (!m_pendingJumpId.empty()) {
                std::string jumpId = m_pendingJumpId;
                m_pendingJumpId = "";
                StartDialog(jumpId);
                return;
            }

            if (m_vmWaiting && m_vmDelayed && !m_project.bytecode.empty()) {
                m_vmWaiting = false;
                m_vmDelayed = false;
                RunVM();
                return;
            }
        } else {
            // Still counting down — block narrative processing
            return;
        }
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
    if (name.substr(0, 5) == "__tmp") {
        m_variables[name] = value;
        return;
    }
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
        "set", "add", "sub", "mul", "random",
        "shake", "play_sfx", "jump", "delay",
        "expression", "hide", "pos", "clear",
        "move", "fade", "fade_screen"
    };
    for (const auto& e : events) {
        if (!VALID_OPS.count(e.op)) {
            RecordError("ProcessEvents", "Unknown op '" + e.op + "' — skipping.");
            continue;
        }
        auto& p = e.params;

        // INSTANT ops
        if (e.op == "shake")    { TriggerShake(p.value("intensity", 5.0f)); continue; }
        if (e.op == "play_sfx") { m_pendingSFX.push_back(p.value("id", "")); continue; }
        if (e.op == "clear")    { m_activeEntities.clear(); continue; }
        if (e.op == "expression") {
            m_activeEntities[p.value("target", "")].expression = p.value("id", "idle");
            continue;
        }
        if (e.op == "hide") { m_activeEntities.erase(p.value("target", "")); continue; }
        if (e.op == "pos") {
            auto& s = m_activeEntities[p.value("target", "")];
            float x = ParseXParam(p);
            s.pos = std::to_string(x); s.currentNormX = x; s.targetNormX = x;
            continue;
        }

        // SYNC ops
        if (e.op == "jump")  { m_pendingJumpId = p.value("target", ""); continue; }
        if (e.op == "delay") { m_engineDelayTimer = p.value("duration", 0) / 1000.0f; continue; }

        // ASYNC ops
        if (e.op == "move") {
            auto& s = m_activeEntities[p.value("target", "")];
            float x = ParseXParam(p);
            s.pos = std::to_string(x); s.targetNormX = x; s.startNormX = s.currentNormX;
            s.moveDuration = p.value("duration", 0) / 1000.0f;
            s.moveTimer    = 0.0f;
            if (s.moveDuration <= 0.0f) s.currentNormX = x;
            continue;
        }
        if (e.op == "fade") {
            std::string target = p.value("target", "");
            int   duration = p.value("duration", 0);
            if (target == "bg") {
                std::string bgId = p.value("id", "");
                if (!bgId.empty() && m_activeBg != bgId) {
                    m_prevBg = m_activeBg; m_activeBg = bgId;
                    m_bgFadeAlpha    = 0.0f; m_bgFadeTimer = 0.0f;
                    m_bgFadeDuration = std::max(0.01f, duration / 1000.0f);
                }
            } else {
                auto& s      = m_activeEntities[target];
                s.targetAlpha = p.value("alpha", 1.0f);
                s.startAlpha  = s.alpha;
                s.fadeDuration = std::max(0.01f, duration / 1000.0f);
                s.fadeTimer    = 0.0f;
            }
            continue;
        }
        if (e.op == "fade_screen") {
            m_screenFadeTarget   = p.value("alpha", 0.0f);
            m_screenFadeStart    = m_screenFadeAlpha;
            m_screenFadeDuration = p.value("duration", 0) / 1000.0f;
            m_screenFadeTimer    = 0.0f;
            if (m_screenFadeDuration <= 0.0f) m_screenFadeAlpha = m_screenFadeTarget;
            continue;
        }

        // Variable ops (INSTANT)
        std::string var = p.value("var", "");
        if (!var.empty() && var.substr(0, 5) != "__tmp" && !m_project.variables.count(var)) {
            RecordError("ProcessEvents", "Op '" + e.op + "' references undeclared variable '" + var + "' — skipping.");
            continue;
        }
        int cur = GetVariable(var), next = cur;
        if      (e.op == "set")    next = p.value("value", 0);
        else if (e.op == "add")    next = cur + p.value("value", 0);
        else if (e.op == "sub")    next = cur - p.value("value", 0);
        else if (e.op == "mul")    next = cur * p.value("value", 1);
        else if (e.op == "random") {
            int lo = p.value("min", 0), hi = p.value("max", 1);
            if (lo > hi) std::swap(lo, hi);
            next = lo + (std::rand() % (hi - lo + 1));
        }
        SetVariable(var, next);
        m_eventTrace.push_back({ m_currentNodeId, e.op, var, cur, GetVariable(var) });
        Log("Event: " + e.op + " '" + var + "' " + std::to_string(cur) + " -> " + std::to_string(GetVariable(var)));
    }
}

void DialogEngine::RecordError(const std::string& context, const std::string& msg) {
    std::string full = "[" + context + "] " + msg;
    m_errors.push_back(full);
    Log(full, "ERROR");
}

void DialogEngine::Log(const std::string& msg, const std::string& level) const {
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
    static const std::unordered_set<std::string> NON_VAR_OPS = {
        "shake", "play_sfx", "jump", "delay", "expression", "hide", "pos",
        "clear", "move", "fade", "fade_screen"
    };

    auto checkEvent = [&](const std::string& nodeId, const std::string& ctx, const Event& e) {
        if (NON_VAR_OPS.count(e.op)) return;
        std::string var = e.params.value("var", "");
        if (!var.empty() && !p.variables.count(var))
            errors.push_back({nodeId, ctx, "Event op '" + e.op + "' references undeclared variable '" + var + "'."});
    };

    // Recursive condition validator
    std::function<void(const std::string&, const ConditionNode&)> checkCond =
        [&](const std::string& nodeId, const ConditionNode& c) {
            if (c.isGroup) { for (auto& ch : c.children) checkCond(nodeId, ch); return; }
            if (!p.variables.count(c.leaf.var))
                errors.push_back({nodeId, "options.conditions", "Condition references undeclared variable '" + c.leaf.var + "'."});
        };

    if (!p.nodes.count(p.configs.start_node))
        errors.push_back({"configs", "start_node", "Start node '" + p.configs.start_node + "' does not exist."});

    for (const auto& [nodeId, node] : p.nodes) {
        if (!node.entity.empty() && node.entity != "system" && !p.entities.count(node.entity))
            errors.push_back({nodeId, "entity", "Entity '" + node.entity + "' is not defined."});
        if (node.next_id && *node.next_id != "dialog_end" && !node.next_id->empty() && !p.nodes.count(*node.next_id))
            errors.push_back({nodeId, "next_id", "next_id '" + *node.next_id + "' does not exist."});
        for (const auto& e : node.events) checkEvent(nodeId, "events", e);
        for (const auto& opt : node.options) {
            if (!opt.next_id.empty() && opt.next_id != "dialog_end" && !p.nodes.count(opt.next_id))
                errors.push_back({nodeId, "options.next_id", "Option next_id '" + opt.next_id + "' does not exist."});
            for (const auto& c : opt.conditions) checkCond(nodeId, c);
            for (const auto& e : opt.events) checkEvent(nodeId, "options.events", e);
        }
    }
    return errors;
}

bool DialogEngine::EvalConditionNode(const ConditionNode& node) const {
    if (!node.isGroup) {
        // Leaf: evaluate single condition
        int v = GetVariable(node.leaf.var);
        const auto& op = node.leaf.op;
        int val = (!node.leaf.ref.empty()) ? GetVariable(node.leaf.ref) : node.leaf.value;
        if (op == "==" || op == "=") return v == val;
        if (op == "!=")              return v != val;
        if (op == ">")               return v >  val;
        if (op == "<")               return v <  val;
        if (op == ">=")              return v >= val;
        if (op == "<=")              return v <= val;
        return false; // unknown op fails
    }
    // Group: evaluate children recursively
    if (node.groupLogic == "or") {
        for (const auto& c : node.children) if (EvalConditionNode(c)) return true;
        return false;
    } else { // "and"
        for (const auto& c : node.children) if (!EvalConditionNode(c)) return false;
        return true;
    }
}

void DialogEngine::RefreshVisibleOptions() {
    m_visibleOptions.clear();
    for (auto& o : m_currentNode->options) {
        bool pass = true;
        for (const auto& cond : o.conditions) {
            if (!EvalConditionNode(cond)) { pass = false; break; }
        }
        if (pass) m_visibleOptions.push_back(o);
    }
}

float DialogEngine::ParseXParam(const nlohmann::json& params, const std::string& key) const {
    if (params.contains(key)) {
        const auto& v = params[key];
        if (v.is_number()) return v.get<float>();
        if (v.is_string()) return ParsePosition(v.get<std::string>());
    }
    return 0.5f;
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

float DialogEngine::ParsePosition(const std::string& pos) const {
    if (pos == "left") return 0.2f;
    if (pos == "right") return 0.8f;
    if (pos == "center") return 0.5f;
    try { return std::stof(pos); } catch(...) { return 0.5f; }
}

void DialogEngine::RunVM() {
    while (m_isActive && !m_vmWaiting && m_pc < (int)m_project.bytecode.size()) {
        ExecuteInstruction(m_project.bytecode[m_pc++]);
    }
}

void DialogEngine::ExecuteInstruction(const BitInstruction& ins) {
    auto& args = ins.args;
    switch (ins.op) {
        case BitOp::SAY: {
            static DialogNode vmNode;
            vmNode.entity  = args[0];
            vmNode.content = args[1];
            vmNode.metadata = NodeMetadata();

            // ── 1. Parse ALL metadata out of the instruction, regardless of entity ────
            if (!ins.metadata.empty()) {
                if (ins.metadata.contains("join"))
                    vmNode.metadata.join = (ins.metadata["join"].get<std::string>() == "true");
                if (ins.metadata.contains("bg")) {
                    vmNode.metadata.bg = ins.metadata["bg"];
                    if (m_activeBg != vmNode.metadata.bg) {
                        m_prevBg = m_activeBg;
                        m_activeBg = vmNode.metadata.bg;
                        m_bgFadeAlpha = 0.0f; m_bgFadeTimer = 0.0f; m_bgFadeDuration = 0.8f;
                    }
                }
                if (ins.metadata.contains("bgm")) {
                    vmNode.metadata.bgm = ins.metadata["bgm"];
                    m_activeBgm = vmNode.metadata.bgm;
                }
                if (ins.metadata.contains("auto_next"))
                    vmNode.metadata.auto_next = (ins.metadata["auto_next"].get<std::string>() == "true");
                if (ins.metadata.contains("hide_ui"))
                    vmNode.metadata.hide_ui = (ins.metadata["hide_ui"].get<std::string>() == "true");
                if (ins.metadata.contains("pre_delay"))
                    vmNode.metadata.pre_delay = std::stoi(ins.metadata["pre_delay"].get<std::string>());
                if (ins.metadata.contains("transition"))
                    vmNode.metadata.transition = ins.metadata["transition"];
                if (ins.metadata.contains("transition_duration"))
                    vmNode.metadata.transition_duration = std::stoi(ins.metadata["transition_duration"].get<std::string>());
                if (ins.metadata.contains("transition_delay"))
                    vmNode.metadata.transition_delay = std::stoi(ins.metadata["transition_delay"].get<std::string>());
            }

            // ── 2. Apply entity-specific visual metadata ──────────────────────────────
            if (!vmNode.entity.empty() && vmNode.entity != "system") {
                bool isNew = (m_activeEntities.find(vmNode.entity) == m_activeEntities.end());
                auto& state = m_activeEntities[vmNode.entity];
                if (isNew) {
                    auto it = m_project.entities.find(vmNode.entity);
                    if (it != m_project.entities.end()) {
                        state.targetNormX  = it->second.default_pos_x;
                        state.currentNormX = state.targetNormX;
                    }
                }
                if (!ins.metadata.empty()) {
                    if (ins.metadata.contains("sprite")) {
                        vmNode.metadata.expression = ins.metadata["sprite"];
                        state.expression = vmNode.metadata.expression;
                    }
                    if (ins.metadata.contains("pos")) {
                        vmNode.metadata.pos  = ins.metadata["pos"];
                        state.pos            = vmNode.metadata.pos;
                        state.targetNormX    = ParsePosition(state.pos);
                        state.currentNormX   = state.targetNormX;
                    }
                    if (ins.metadata.contains("alpha")) {
                        vmNode.metadata.alpha = std::stof(ins.metadata["alpha"].get<std::string>());
                        state.alpha       = vmNode.metadata.alpha;
                        state.targetAlpha = state.alpha;
                        state.startAlpha  = state.alpha;
                    }
                }
            }

            // ── 3. Apply node-level flags to engine state ─────────────────────────────
            m_isAutoNext = vmNode.metadata.auto_next;
            m_isUiHidden = vmNode.metadata.hide_ui;

            // ── 4. Scene management: smart joining ────────────────────────────────────
            {
                bool isJoining = vmNode.metadata.join;
                if (!isJoining) {
                    // system entity always joins (no wipe); already-active entities join too
                    if (vmNode.entity == "system") isJoining = true;
                    else if (m_activeEntities.count(vmNode.entity)) isJoining = true;
                }
                if (!isJoining) {
                    ActiveEntityState saved;
                    bool hasSaved = false;
                    if (m_activeEntities.count(vmNode.entity)) {
                        saved    = m_activeEntities[vmNode.entity];
                        hasSaved = true;
                    }
                    m_activeEntities.clear();
                    if (hasSaved) m_activeEntities[vmNode.entity] = saved;
                }
            }

            m_currentNode   = &vmNode;
            m_currentNodeId = "vm_active";

            // ── 5. History ────────────────────────────────────────────────────────────
            if (!vmNode.content.empty()) {
                std::string speakerName = "SYSTEM";
                if (!vmNode.entity.empty() && vmNode.entity != "system") {
                    if (m_project.entities.count(vmNode.entity))
                        speakerName = m_project.entities.at(vmNode.entity).name;
                    else speakerName = "???";
                }
                std::string finalContent = InterpolateVariables(vmNode.content);
                if (m_history.empty() || m_history.back().content != finalContent) {
                    HistoryEntry entry;
                    entry.speaker     = speakerName;
                    entry.content     = finalContent;
                    entry.richContent = RichTextParser::Parse(finalContent);
                    m_history.push_back(entry);
                    if (m_history.size() > 100) m_history.erase(m_history.begin());
                }
            }

            // ── 6. Set up typewriter reveal ───────────────────────────────────────────
            m_cachedInterpolatedContent = InterpolateVariables(vmNode.content);
            m_cachedParsedContent       = RichTextParser::Parse(m_cachedInterpolatedContent);
            m_cachedTotalChars          = m_cachedParsedContent.size();
            m_waitTimer     = 0.0f;

            // pre_delay: blocks auto_next via IsEventDelaying() but text reveals normally
            if (vmNode.metadata.pre_delay > 0) {
                m_engineDelayTimer = vmNode.metadata.pre_delay / 1000.0f;
                // Do NOT set m_vmDelayed — this is a SAY-level delay, not a standalone delay
            }
            m_revealedCount = 0.0f;

            // Block the VM loop until the player advances (or auto_next fires)
            m_vmWaiting = !vmNode.content.empty();
            break;
        }
        case BitOp::SET:     SetVariable(args[0], std::stoi(args[1])); break;
        case BitOp::SET_REF: SetVariable(args[0], GetVariable(args[1])); break;
        case BitOp::ADD:     SetVariable(args[0], GetVariable(args[0]) + std::stoi(args[1])); break;
        case BitOp::ADD_REF: SetVariable(args[0], GetVariable(args[0]) + GetVariable(args[1])); break;
        case BitOp::SUB:     SetVariable(args[0], GetVariable(args[0]) - std::stoi(args[1])); break;
        case BitOp::SUB_REF: SetVariable(args[0], GetVariable(args[0]) - GetVariable(args[1])); break;
        case BitOp::MUL:     SetVariable(args[0], GetVariable(args[0]) * std::stoi(args[1])); break;
        case BitOp::MUL_REF: SetVariable(args[0], GetVariable(args[0]) * GetVariable(args[1])); break;
        case BitOp::DIV:     SetVariable(args[0], GetVariable(args[0]) / std::stoi(args[1])); break;
        case BitOp::DIV_REF: SetVariable(args[0], GetVariable(args[0]) / GetVariable(args[1])); break;
        case BitOp::GOTO: {
            for (int i = 0; i < (int)m_project.bytecode.size(); ++i) {
                if (m_project.bytecode[i].op == BitOp::LABEL && m_project.bytecode[i].args[0] == args[0]) {
                    m_pc = i; break;
                }
            }
            break;
        }
        case BitOp::IF:
        case BitOp::IF_REF: {
            int v = GetVariable(args[0]);
            int val = (ins.op == BitOp::IF_REF) ? GetVariable(args[2]) : std::stoi(args[2]);
            std::string op = args[1];
            bool pass = false;
            if      (op == "==" || op == "=") pass = (v == val);
            else if (op == "!=")              pass = (v != val);
            else if (op == ">")               pass = (v > val);
            else if (op == "<")               pass = (v < val);
            else if (op == ">=")              pass = (v >= val);
            else if (op == "<=")              pass = (v <= val);
            
            if (pass) {
                for (int i = 0; i < (int)m_project.bytecode.size(); ++i) {
                    if (m_project.bytecode[i].op == BitOp::LABEL && m_project.bytecode[i].args[0] == args[3]) {
                        m_pc = i; break;
                    }
                }
            }
            break;
        }
        case BitOp::CHOICE: {
            DialogOption opt;
            opt.content = args[0];
            opt.next_id = args[1];
            if (!ins.metadata.empty() && ins.metadata.contains("style")) {
                opt.style = ins.metadata["style"].get<std::string>();
            }
            m_visibleOptions.push_back(opt);
            break;
        }
        case BitOp::BG: {
            m_prevBg = m_activeBg;
            m_activeBg = args[0];
            m_bgFadeAlpha = 0.0f;
            m_bgFadeTimer = 0.0f;
            m_bgFadeDuration = 0.8f;
            break;
        }
        case BitOp::BGM: {
            m_activeBgm = args[0];
            break;
        }
        case BitOp::EVENT: {
            std::vector<Event> evs = { {args[0], ins.metadata} };
            ProcessEvents(evs);
            if (args[0] == "delay") {
                m_vmWaiting = true;
                m_vmDelayed = true;
            }
            break;
        }
        case BitOp::HALT: m_isActive = false; break;
        default: break;
    }
}
