#include "headers/BitEngine.hpp"
#include "headers/BitScriptInterpreter.hpp"
#include "headers/BitScriptAnalyzer.hpp"
#include "json.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <unordered_set>
#include <ctime>
#include <random>
#include <unistd.h>
#include <filesystem>

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
        return BitScriptInterpreter::LoadScriptFile(configFilePath, m_project);
    }

    if (configFilePath.size() > 5 && configFilePath.substr(configFilePath.size() - 5) == ".bitc") {
        return LoadBytecodeFile(configFilePath);
    }

    RecordError("LoadProject", "Unsupported file type: " + configFilePath + ". Only .bitscript and .bitc are supported.");
    return false;
}

void DialogEngine::CompileProject(const std::string& outputPath) {
    Log("Compiling project to: " + outputPath);
    SaveBytecode(outputPath);
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
        case BitOp::WAIT_INPUT: return "WAIT_INPUT";
        case BitOp::TRANSITION:  return "TRANSITION";
        case BitOp::UI_VISIBLE:  return "UI_VISIBLE";
        case BitOp::CALL:        return "CALL";
        case BitOp::RETURN:      return "RETURN";
        case BitOp::WAIT_ACTION: return "WAIT_ACTION";
        case BitOp::SET_LOCAL:   return "SET_LOCAL";
        case BitOp::PLAY_TIMELINE: return "PLAY_TIMELINE";
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
    if (s=="WAIT_INPUT") return BitOp::WAIT_INPUT;
    if (s=="TRANSITION") return BitOp::TRANSITION;
    if (s=="UI_VISIBLE") return BitOp::UI_VISIBLE;
    if (s=="CALL")       return BitOp::CALL;
    if (s=="RETURN")     return BitOp::RETURN;
    if (s=="WAIT_ACTION") return BitOp::WAIT_ACTION;
    if (s=="SET_LOCAL")  return BitOp::SET_LOCAL;
    if (s=="PLAY_TIMELINE") return BitOp::PLAY_TIMELINE;
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
    root["configs"]["encrypt_save"]  = m_project.configs.encrypt_save;
    root["configs"]["save_prefix"]   = m_project.configs.save_prefix;
    root["configs"]["enable_floating"] = m_project.configs.enable_floating;
    root["configs"]["enable_shadows"]  = m_project.configs.enable_shadows;
    root["configs"]["enable_vignette"] = m_project.configs.enable_vignette;
    root["configs"]["max_slots"]       = m_project.configs.max_slots;

    for (auto& [k,v] : m_project.backgrounds) root["backgrounds"][k] = v;
    for (auto& [k,v] : m_project.music)        root["music"][k]       = v;
    for (auto& [k,v] : m_project.sfx)          root["sfx"][k]         = v;
    for (auto& [k,v] : m_project.fonts)        root["fonts"][k]       = v;
    for (auto& [k,v] : m_project.variables) {
        json vj = { {"id", v.id}, {"initial_value", v.initial_value} };
        if (v.min.has_value()) vj["min"] = v.min.value();
        if (v.max.has_value()) vj["max"] = v.max.value();
        root["variables"][k] = vj;
    }
    for (auto& [k,e] : m_project.entities) {
        json ej = { {"id",e.id},{"name",e.name},{"type",e.type},{"default_pos_x",e.default_pos_x} };
        if (!e.aliases.empty()) ej["aliases"] = e.aliases;
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

    // Timelines
    for (auto& [tid, tl] : m_project.timelines) {
        json tlj;
        tlj["id"] = tl.id;
        json events = json::array();
        for (const auto& ev : tl.events) {
            json evj;
            evj["time"] = ev.time_ms;
            evj["op"] = OpToStr(ev.op);
            evj["args"] = ev.args;
            if (!ev.metadata.empty()) evj["meta"] = ev.metadata;
            events.push_back(evj);
        }
        tlj["events"] = events;
        root["timelines"][tid] = tlj;
    }

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
        m_project.configs.encrypt_save  = c.value("encrypt_save", false);
        m_project.configs.save_prefix   = c.value("save_prefix", "save_slot_");
        m_project.configs.enable_floating = c.value("enable_floating", true);
        m_project.configs.enable_shadows  = c.value("enable_shadows", true);
        m_project.configs.enable_vignette = c.value("enable_vignette", true);
        m_project.configs.max_slots       = c.value("max_slots", 5);
    }
    if (root.contains("backgrounds")) for (auto& [k,v] : root["backgrounds"].items()) m_project.backgrounds[k] = v;
    if (root.contains("music"))        for (auto& [k,v] : root["music"].items())        m_project.music[k]       = v;
    if (root.contains("sfx"))          for (auto& [k,v] : root["sfx"].items())          m_project.sfx[k]         = v;
    if (root.contains("fonts"))        for (auto& [k,v] : root["fonts"].items())        m_project.fonts[k]       = v;
    if (root.contains("variables"))
        for (auto& [k,vj] : root["variables"].items()) {
            VariableDef vd; vd.id = vj.value("id",k); vd.initial_value = vj.value("initial_value",0);
            if (vj.contains("min")) vd.min = vj["min"].get<int>();
            if (vj.contains("max")) vd.max = vj["max"].get<int>();
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
            if (ej.contains("aliases")) e.aliases = ej["aliases"];
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

    if (root.contains("timelines")) {
        m_project.timelines.clear();
        for (auto& [tid, tlj] : root["timelines"].items()) {
            Timeline tl;
            tl.id = tid;
            if (tlj.contains("events")) {
                for (auto& evj : tlj["events"]) {
                    TimelineEvent ev;
                    ev.time_ms = evj.value("time", 0);
                    ev.op = StrToOp(evj.value("op", "HALT"));
                    ev.args = evj.value("args", std::vector<std::string>{});
                    if (evj.contains("meta")) ev.metadata = evj["meta"];
                    tl.events.push_back(ev);
                }
            }
            m_project.timelines[tid] = tl;
        }
    }

    Log("Loaded " + std::to_string(m_project.bytecode.size()) + " bytecode instructions and " + std::to_string(m_project.timelines.size()) + " timelines from " + path);
    return true;
}

// Redundant methods removed.


void DialogEngine::SaveGame(int slot) {
    Log("Saving game to slot " + std::to_string(slot));
    std::string path = GetSlotPath(slot);
    std::string tmpPath = path + ".tmp";
    
    try {
        json j;
        j["version"] = 2; // v0.2 save format
        j["pc"] = (m_vmWaiting && m_pc > 0) ? m_pc - 1 : m_pc;
        j["variables"] = m_variables;
        j["revealed_count"] = m_revealedCount;
        j["screen_fade"] = m_screenFadeAlpha;
        
        // Narrative Stack
        j["stack"] = m_callStack;
        j["locals"] = m_localVariables;
        
        j["active_bg"] = m_activeBg;
        j["active_bgm"] = m_activeBgm;
        j["speaker"] = m_currentSpeakerId;
        j["ui_hidden"] = m_isUiHidden;
        
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
        
        // Metadata for UI
        std::string summary = m_currentSpeakerId.empty() ? "Narrative" : m_currentSpeakerId;
        j["meta"] = { 
            {"time", GetTimestamp()}, 
            {"pc", m_pc}, 
            {"text", "Scene: " + summary + " (PC:" + std::to_string(m_pc) + ")"} 
        };

        std::string data = j.dump(4);
        if (m_project.configs.encrypt_save) data = XORBuffer(data);
        
        {
            // Ensure save directory exists using C++17 filesystem
            std::filesystem::create_directories("save");
            
            std::ofstream f(tmpPath, std::ios::binary);
            if (!f) throw std::runtime_error("Could not open temp save file: " + tmpPath);
            f.write(data.c_str(), data.size());
        }

        // Atomic rename
#ifdef _WIN32
        _unlink(path.c_str());
#else
        unlink(path.c_str());
#endif
        rename(tmpPath.c_str(), path.c_str());
        
        Log("Save successful: " + path);
    } catch (const std::exception& e) {
        RecordError("SaveGame", std::string("Save failed: ") + e.what());
    } catch (...) {
        RecordError("SaveGame", "Save failed: Unknown error");
    }
}

bool DialogEngine::LoadGame(int slot) {
    std::string path = GetSlotPath(slot);
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    
    std::streamsize sz = f.tellg(); 
    f.seekg(0);
    std::string data(sz, '\0');
    
    if (f.read(&data[0], sz)) {
        try {
            if (m_project.configs.encrypt_save) data = XORBuffer(data);
            json j = json::parse(data);
            
            m_pc = j.value("pc", 0);
            if (m_pc < 0) m_pc = 0;
            
            m_variables = j["variables"].get<std::unordered_map<std::string, int>>();
            float savedReveal = j.value("revealed_count", 0.0f);
            m_revealedCount = savedReveal;
            m_screenFadeAlpha = j.value("screen_fade", 0.0f);
            
            // Restore Stack
            m_callStack = j.value("stack", std::vector<int>{});
            m_localVariables = j.value("locals", std::vector<std::unordered_map<std::string, int>>{ {} });
            if (m_localVariables.empty()) m_localVariables.push_back({});

            m_activeBg = j.value("active_bg", "");
            m_activeBgm = j.value("active_bgm", "");
            m_currentSpeakerId = j.value("speaker", "");
            m_isUiHidden = j.value("ui_hidden", false);
            
            m_activeEntities.clear();
            if (j.contains("active_entities")) {
                for (auto& [id, s] : j["active_entities"].items()) {
                    ActiveEntityState state;
                    state.expression = s.value("expression", "idle");
                    state.pos = s.value("pos", "center");
                    state.currentNormX = s.value("normX", 0.5f);
                    state.targetNormX = state.currentNormX;
                    state.alpha = s.value("alpha", 1.0f);
                    state.targetAlpha = state.alpha;
                    m_activeEntities[id] = state;
                }
            }
            
            m_isActive = true;
            m_vmWaiting = false;
            m_vmDelayed = false;
            m_waitingForActionType = "";
            
            Log("Load successful from slot " + std::to_string(slot));
            RunVM();
            
            // Post-RunVM restoration for state-sensitive variables
            m_revealedCount = savedReveal;

            return true;
        } catch (const std::exception& e) {
            RecordError("LoadGame", std::string("Load failed: ") + e.what());
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
            auto m = j["meta"]; return SaveMetadata{ m["time"], "", "", "" };
        } catch (...) {}
    }
    return std::nullopt;
}

void DialogEngine::StartDialog(const std::string& startId) {
    m_isActive = true;
    m_pc = 0;
    m_localVariables.clear();
    m_localVariables.push_back({}); // root scope
    
    std::string id = startId.empty() ? m_project.configs.start_node : startId;
    Log("Starting dialog sequence: " + id);
    
    for (int i = 0; i < (int)m_project.bytecode.size(); ++i) {
        if (m_project.bytecode[i].op == BitOp::LABEL && m_project.bytecode[i].args[0] == id) {
            m_pc = i;
            break;
        }
    }
    
    m_vmWaiting = false;
    m_vmDelayed = false;
    RunVM();
}

void DialogEngine::SelectOption(int index) {
    if (index < 0 || index >= (int)m_visibleOptions.size()) return;
    std::string target = m_visibleOptions[index].next_id;
    m_visibleOptions.clear();
    
    for (int i = 0; i < (int)m_project.bytecode.size(); ++i) {
        if (m_project.bytecode[i].op == BitOp::LABEL && m_project.bytecode[i].args[0] == target) {
            m_pc = i;
            break;
        }
    }
    
    m_vmWaiting = false;
    m_vmDelayed = false;
    RunVM();
}

void DialogEngine::Next() {
    if (!m_isActive || IsEventDelaying() || !m_pendingJumpId.empty() || m_inputLockoutTimer > 0.0f) return;
    
    // Feature: Don't advance if visuals (fades/moves) are still active
    if (!IsTextRevealing() && IsVisualAnimating()) return;

    if (IsTextRevealing()) { SkipReveal(); return; }
    
    m_isAutoNext = false; 
    if (!m_visibleOptions.empty()) return;

    m_vmWaiting = false;
    m_vmDelayed = false;
    RunVM();
}

void DialogEngine::RunVM() {
    while (m_isActive && !m_vmWaiting && m_pc < (int)m_project.bytecode.size()) {
        ExecuteInstruction(m_project.bytecode[m_pc++]);
    }
}

void DialogEngine::Update(float dt) {
    if (!m_isActive) return;

    if (m_inputLockoutTimer > 0.0f) m_inputLockoutTimer -= dt;
    
    // 0. Update Timelines
    for (auto it = m_activeTimelines.begin(); it != m_activeTimelines.end(); ) {
        it->timer += dt * 1000.0f;
        auto tlIt = m_project.timelines.find(it->id);
        if (tlIt == m_project.timelines.end()) {
            it = m_activeTimelines.erase(it);
            continue;
        }
        auto& tl = tlIt->second;
        while (it->nextEventIdx < tl.events.size() && tl.events[it->nextEventIdx].time_ms <= it->timer) {
            auto& ev = tl.events[it->nextEventIdx++];
            ExecuteInstruction({ev.op, ev.args, ev.metadata, -1});
        }
        if (it->nextEventIdx >= tl.events.size()) {
            it->finished = true;
            it = m_activeTimelines.erase(it);
        } else {
            ++it;
        }
    }

    // 1. Screen Fade updates
    if (m_screenFadeDuration <= 0.0f) {
        m_screenFadeAlpha = m_screenFadeTarget;
    } else if (m_screenFadeTimer < m_screenFadeDuration) {
        m_screenFadeTimer += dt;
        float t = std::min(1.0f, m_screenFadeTimer / m_screenFadeDuration);
        m_screenFadeAlpha = m_screenFadeStart + (m_screenFadeTarget - m_screenFadeStart) * t;
        if (m_screenFadeTimer >= m_screenFadeDuration) {
            m_screenFadeAlpha = m_screenFadeTarget;
            m_screenFadeDuration = 0.0f;  // Reset to stop fade
            m_screenFadeTimer = 0.0f;
        }
    }

    // 2. Visual Updates (Must always run for smooth animations)
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

    // 3. Trigger pending transition if animations and delays are done
    // Visual updates check for VM resumption

    // 4. Engine Delay (Pauses narrative, but not visuals)
    if (m_engineDelayTimer > 0.0f) {
        m_engineDelayTimer -= dt;
        if (m_engineDelayTimer <= 0.0f) {
            m_engineDelayTimer = 0.0f;
            if (m_revealedCount < 0.0f) m_revealedCount = 0.0f;
            if (m_vmWaiting && m_vmDelayed && !m_project.bytecode.empty()) {
                m_vmWaiting = false;
                m_vmDelayed = false;
                RunVM();
            }
        }
        return;
    }

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

    if (m_vmWaiting && !m_waitingForActionType.empty()) {
        bool done = false;
        if (m_waitingForActionType == "sfx") done = m_pendingSFX.empty(); 
        else if (m_waitingForActionType == "move") {
            done = true;
            for (auto& [id, s] : m_activeEntities) if (s.moveTimer < s.moveDuration) done = false;
        }
        else if (m_waitingForActionType == "fade") {
            done = true;
            for (auto& [id, s] : m_activeEntities) if (s.fadeTimer < s.fadeDuration) done = false;
            if (m_bgFadeTimer < m_bgFadeDuration) done = false;
            if (m_screenFadeTimer < m_screenFadeDuration) done = false;
        }
        else if (m_waitingForActionType == "all") done = !IsVisualAnimating();
        else if (m_waitingForActionType == "timeline") {
            done = true;
            // If any blocking timeline is still running, we're not done
            for (const auto& atl : m_activeTimelines) if (atl.isBlocking) done = false;
        }

        if (done) {
            m_waitingForActionType = "";
            m_vmWaiting = false;
            RunVM();
        }
    }

    if (!IsTextRevealing() && m_isAutoNext && m_waitTimer <= 0.0f && m_visibleOptions.empty() && !IsEventDelaying() && m_waitingForActionType.empty()) {
        Next();
    }

    // Feature: Auto-Play logic
    if (m_isAutoPlaying && !IsTextRevealing() && m_visibleOptions.empty() && m_waitTimer <= 0.0f) {
        m_autoPlayTimer += dt;
        if (m_autoPlayTimer >= m_project.configs.auto_play_delay) {
            m_autoPlayTimer = 0.0f;
            Next();
        }
    } else {
        m_autoPlayTimer = 0.0f;
    }

}

void DialogEngine::SkipReveal() { 
    m_revealedCount = (float)m_cachedTotalChars; 
    m_waitTimer = 0.0f;
    m_inputLockoutTimer = 0.2f; // Prevent immediate double-skip or advance
}
bool DialogEngine::IsTextRevealing() const { return m_isActive && m_revealedCount < (float)m_cachedTotalChars; }

std::string DialogEngine::GetVisibleContent() const {
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

int DialogEngine::GetVariable(const std::string& name) const { 
    if (!m_localVariables.empty() && m_localVariables.back().count(name)) return m_localVariables.back().at(name);
    auto it = m_variables.find(name); return (it != m_variables.end()) ? it->second : 0; 
}

void DialogEngine::SetVariable(const std::string& name, int value) {
    if (name.substr(0, 5) == "__tmp") {
        m_variables[name] = value;
        return;
    }
    if (!m_localVariables.empty() && m_localVariables.back().count(name)) {
        m_localVariables.back()[name] = value;
        return;
    }
    auto it = m_project.variables.find(name);
    if (it == m_project.variables.end()) {
        RecordError("SetVariable", "Variable '" + name + "' is not declared — ignoring.");
        return;
    }
    const auto& d = it->second;
    int oldVal = m_variables.count(name) ? m_variables[name] : 0;
    int v = value;
    if (d.min) v = std::max(v, *d.min);
    if (d.max) v = std::min(v, *d.max);
    m_variables[name] = v;

    m_eventTrace.push_back({std::to_string(m_pc), "SET", name, oldVal, v});
    if (m_eventTrace.size() > 50) m_eventTrace.erase(m_eventTrace.begin());
}

const std::vector<std::string>& DialogEngine::ConsumePendingSFX() {
    static std::vector<std::string> copy;
    copy = m_pendingSFX;
    m_pendingSFX.clear();
    return copy;
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
        std::ofstream logFile("debug.log", std::ios_base::app);
        if (logFile) {
            auto t = std::time(nullptr);
            logFile << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S")
                    << " | " << tag << msg << "\n";
        }
    }
}

ValidationResult DialogEngine::ValidateProject(const DialogProject& p) {
    ValidationResult results;
    auto messages = BitScriptAnalyzer::Analyze(p);
    for (const auto& msg : messages) {
        std::string prefix = (msg.level == AnalysisMessage::Level::ERROR) ? "[ERROR]" : "[WARN]";
        if (msg.line > 0) {
            results.errors.push_back(prefix + " (Line " + std::to_string(msg.line) + "): " + msg.message);
        } else {
            results.errors.push_back(prefix + " " + msg.message);
        }
    }
    return results;
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
    auto it = m_project.entities.find(m_currentSpeakerId);
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

void DialogEngine::ExecuteInstruction(const BitInstruction& ins) {
    auto& args = ins.args;
    switch (ins.op) {
        case BitOp::SAY: {
            std::string entityId = args[0];
            std::string content = args[1];

            // 1. Process metadata
            bool join = false;
            m_isAutoNext = false; 
            if (!ins.metadata.empty()) {
                if (ins.metadata.contains("join")) join = (ins.metadata["join"].get<std::string>() == "true");
                if (ins.metadata.contains("bg")) {
                    std::string bgId = ins.metadata["bg"];
                    if (m_activeBg != bgId) {
                        m_prevBg = m_activeBg; m_activeBg = bgId;
                        m_bgFadeAlpha = 0.0f; m_bgFadeTimer = 0.0f; m_bgFadeDuration = 0.8f;
                    }
                }
                if (ins.metadata.contains("bgm")) m_activeBgm = ins.metadata["bgm"];
                if (ins.metadata.contains("auto_next")) m_isAutoNext = (ins.metadata["auto_next"].get<std::string>() == "true");
                if (ins.metadata.contains("pre_delay")) m_engineDelayTimer = SafeStoi(ins.metadata["pre_delay"].get<std::string>()) / 1000.0f;
            }

            // 2. Entity visuals
            if (!entityId.empty() && entityId != "system") {
                auto& state = m_activeEntities[entityId];
                state.visible = true; // Auto-join/show when speaking
                
                // Apply Aliases
                if (ins.metadata.contains("alias")) {
                    std::string aliasName = ins.metadata["alias"];
                    const auto* entity = GetEntity(entityId);
                    if (entity && entity->aliases.count(aliasName)) {
                        auto aliasData = entity->aliases.at(aliasName);
                        for (auto& [k, v] : aliasData.items()) {
                            if (!ins.metadata.contains(k)) { // Don't override explicit mods
                                if (k == "sprite") state.expression = v;
                                else if (k == "pos") {
                                    state.pos = v;
                                    state.targetNormX = ParsePosition(state.pos);
                                    state.currentNormX = state.targetNormX;
                                }
                                else if (k == "alpha") {
                                    state.alpha = std::stof(v.get<std::string>());
                                    state.targetAlpha = state.alpha;
                                }
                                else if (k == "shake") {
                                    // Add shake logic if alias supports it (future-proof)
                                }
                            }
                        }
                    }
                }

                if (!ins.metadata.empty()) {
                    if (ins.metadata.contains("sprite")) state.expression = ins.metadata["sprite"];
                    if (ins.metadata.contains("pos")) {
                        state.pos = ins.metadata["pos"];
                        state.targetNormX = ParsePosition(state.pos);
                        state.currentNormX = state.targetNormX;
                    }
                    if (ins.metadata.contains("alpha")) {
                        state.alpha = std::stof(ins.metadata["alpha"].get<std::string>());
                        state.targetAlpha = state.alpha;
                    }
                }
            }

            // 3. Joining logic
            if (!join) {
                if (entityId != "system" && entityId != "narration" && !m_activeEntities.count(entityId)) {
                    m_activeEntities.clear();
                }
            }

            // 4. Content reveal
            m_currentSpeakerId = (entityId == "narration" || entityId == "system") ? "" : entityId;
            m_cachedInterpolatedContent = InterpolateVariables(content);
            m_cachedParsedContent = RichTextParser::Parse(m_cachedInterpolatedContent);
            m_cachedTotalChars = m_cachedParsedContent.size();
            m_revealedCount = (m_project.configs.mode == "instant") ? (float)m_cachedTotalChars : 0.0f;
            m_waitTimer = 0.0f;
            m_vmWaiting = !content.empty();

            // 5. History
            if (!content.empty()) {
                std::string speaker = "SYSTEM";
                if (entityId == "narration") speaker = "";
                else if (m_project.entities.count(entityId)) speaker = m_project.entities.at(entityId).name;
                HistoryEntry entry = { speaker, m_cachedInterpolatedContent, m_cachedParsedContent };
                m_history.push_back(entry);
                if (m_history.size() > 100) m_history.erase(m_history.begin());
            }

            if (m_project.configs.auto_save) SaveGame(0);
            break;
        }
        case BitOp::SET:     SetVariable(args[0], SafeStoi(args[1])); break;
        case BitOp::SET_REF: SetVariable(args[0], GetVariable(args[1])); break;
        case BitOp::ADD:     SetVariable(args[0], GetVariable(args[0]) + SafeStoi(args[1])); break;
        case BitOp::ADD_REF: SetVariable(args[0], GetVariable(args[0]) + GetVariable(args[1])); break;
        case BitOp::SUB:     SetVariable(args[0], GetVariable(args[0]) - SafeStoi(args[1])); break;
        case BitOp::SUB_REF: SetVariable(args[0], GetVariable(args[0]) - GetVariable(args[1])); break;
        case BitOp::MUL:     SetVariable(args[0], GetVariable(args[0]) * SafeStoi(args[1])); break;
        case BitOp::MUL_REF: SetVariable(args[0], GetVariable(args[0]) * GetVariable(args[1])); break;
        case BitOp::DIV: {
            int divisor = SafeStoi(args[1]);
            if (divisor == 0) RecordError("ExecuteInstruction", "Division by zero");
            else SetVariable(args[0], GetVariable(args[0]) / divisor);
            break;
        }
        case BitOp::DIV_REF: {
            int divisor = GetVariable(args[1]);
            if (divisor == 0) RecordError("ExecuteInstruction", "Division by zero");
            else SetVariable(args[0], GetVariable(args[0]) / divisor);
            break;
        }
        case BitOp::GOTO: {
            for (int i = 0; i < (int)m_project.bytecode.size(); ++i) {
                if (m_project.bytecode[i].op == BitOp::LABEL && m_project.bytecode[i].args[0] == args[0]) {
                    m_eventTrace.push_back({std::to_string(m_pc-1), "JUMP", args[0], 0, i});
                    m_pc = i; break;
                }
            }
            break;
        }
        case BitOp::IF:
        case BitOp::IF_REF: {
            int v = GetVariable(args[0]);
            int val = (ins.op == BitOp::IF_REF) ? GetVariable(args[2]) : SafeStoi(args[2]);
            std::string op = args[1];
            bool pass = false;
            if      (op == "==" || op == "=") pass = (v == val);
            else if (op == "!=")              pass = (v != val);
            else if (op == ">")               pass = (v > val);
            else if (op == "<")               pass = (v < val);
            else if (op == ">=")              pass = (v >= val);
            else if (op == "<=")              pass = (v <= val);
            
            m_eventTrace.push_back({std::to_string(m_pc-1), "IF", pass ? "TRUE" : "FALSE", v, val});
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
        case BitOp::TRANSITION:
            break;
        case BitOp::UI_VISIBLE: {
            m_isUiHidden = (args[0] == "hide");
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
        case BitOp::CALL: {
            std::string labelId = args[0];
            int targetPC = -1;
            const BitInstruction* labelIns = nullptr;

            for (int i = 0; i < (int)m_project.bytecode.size(); ++i) {
                if (m_project.bytecode[i].op == BitOp::LABEL && m_project.bytecode[i].args[0] == labelId) {
                    targetPC = i;
                    labelIns = &m_project.bytecode[i];
                    break;
                }
            }

            if (targetPC != -1) {
                m_callStack.push_back(m_pc);
                m_localVariables.push_back({}); // New scope
                
                // Map parameters - resolve variable references in caller's scope
                if (labelIns && labelIns->args.size() > 1) {
                    for (size_t i = 1; i < labelIns->args.size(); ++i) {
                        std::string paramName = labelIns->args[i];
                        int val = 0;
                        if (args.size() > i) {
                            // Try to parse as integer first, if fails treat as variable name
                            try {
                                val = std::stoi(args[i]);
                            } catch (...) {
                                val = GetVariable(args[i]);
                            }
                        }
                        m_localVariables.back()[paramName] = val;
                    }
                }
                m_pc = targetPC;
            }
            break;
        }
        case BitOp::RETURN: {
            if (!m_callStack.empty()) {
                m_pc = m_callStack.back();
                m_callStack.pop_back();
                m_localVariables.pop_back(); // Pop local scope
            }
            break;
        }
        case BitOp::SET_LOCAL: {
            if (!m_localVariables.empty()) {
                m_localVariables.back()[args[0]] = SafeStoi(args[1]);
            }
            break;
        }
        case BitOp::WAIT_ACTION: {
            m_waitingForActionType = args[0];
            m_vmWaiting = true;
            break;
        }
        case BitOp::WAIT_INPUT: {
            m_vmWaiting = true;
            break;
        }
        case BitOp::PLAY_TIMELINE: {
            std::string tid = args[0];
            bool wait = (args[1] == "true");
            if (m_project.timelines.count(tid)) {
                ActiveTimeline atl;
                atl.id = tid;
                atl.isBlocking = wait;
                m_activeTimelines.push_back(atl);
                if (wait) {
                    m_waitingForActionType = "timeline";
                    m_vmWaiting = true;
                }
            }
            break;
        }
        case BitOp::HALT: m_isActive = false; break;
        default: break;
    }
}

void DialogEngine::ProcessEvents(const std::vector<Event>& events) {
    for (const auto& e : events) {
        auto& p = e.params;
        if (e.op == "shake")    { TriggerShake(p.value("intensity", 5.0f)); continue; }
        if (e.op == "play_sfx") { m_pendingSFX.push_back(p.value("id", "")); continue; }
        if (e.op == "clear")    { m_activeEntities.clear(); continue; }
        if (e.op == "expression") {
            std::string target = p.value("target", "");
            if (m_activeEntities.count(target)) m_activeEntities[target].expression = p.value("id", "idle");
            continue;
        }
        if (e.op == "hide") {
            std::string target = p.value("target", "");
            if (m_activeEntities.count(target)) m_activeEntities[target].visible = false;
            continue;
        }
        if (e.op == "leave") {
            m_activeEntities.erase(p.value("target", ""));
            continue;
        }
        if (e.op == "pos") {
            std::string target = p.value("target", "");
            auto& s = m_activeEntities[target];
            float x = ParseXParam(p);
            s.pos = std::to_string(x); s.currentNormX = x; s.targetNormX = x;
            s.visible = true; // Auto-show if modified? Or just keep current? 
            // User said "all character who enter join by default and are active".
            // So let's ensure they are visible.
            continue;
        }
        if (e.op == "jump")  { m_pendingJumpId = p.value("target", ""); continue; }
        if (e.op == "delay") { m_engineDelayTimer = p.value("duration", 0) / 1000.0f; continue; }
        if (e.op == "move") {
            std::string target = p.value("target", "");
            auto& s = m_activeEntities[target];
            float x = ParseXParam(p);
            s.pos = std::to_string(x); s.targetNormX = x; s.startNormX = s.currentNormX;
            s.moveDuration = p.value("duration", 0) / 1000.0f; s.moveTimer = 0.0f;
            s.visible = true;
            continue;
        }
        if (e.op == "fade") {
            std::string target = p.value("target", "");
            int duration = p.value("duration", 0);
            if (target == "bg") {
                std::string bgId = p.value("id", "");
                if (!bgId.empty() && m_activeBg != bgId) {
                    m_prevBg = m_activeBg; m_activeBg = bgId;
                    m_bgFadeAlpha = 0.0f; m_bgFadeTimer = 0.0f; m_bgFadeDuration = std::max(0.01f, duration / 1000.0f);
                }
            } else {
                auto& s = m_activeEntities[target];
                s.targetAlpha = p.value("alpha", 1.0f); s.startAlpha = s.alpha;
                s.fadeDuration = std::max(0.01f, duration / 1000.0f); s.fadeTimer = 0.0f;
                s.visible = true;
            }
            continue;
        }
        if (e.op == "fade_screen") {
            m_screenFadeTarget = p.value("alpha", 0.0f); m_screenFadeStart = m_screenFadeAlpha;
            m_screenFadeDuration = p.value("duration", 0) / 1000.0f; m_screenFadeTimer = 0.0f;
            continue;
        }
        
        std::string var = p.value("var", "");
        if (var.empty()) continue;
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
    }
}
int DialogEngine::SafeStoi(const std::string& s) const {
    if (s.empty()) return 0;
    try {
        return std::stoi(s);
    } catch (...) {
        return 0;
    }
}
