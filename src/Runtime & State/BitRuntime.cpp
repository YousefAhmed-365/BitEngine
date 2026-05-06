#include "BitRuntime.hpp"
#include "BitScriptInterpreter.hpp"
#include "BitScriptAnalyzer.hpp"
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

BitRuntime::BitRuntime() {
    m_vm = std::make_unique<BitVM>(*this);
}

bool BitRuntime::LoadProject(const std::string& configFilePath) {
    Log("Loading project from: " + configFilePath);

    if (configFilePath.size() > 5 && configFilePath.substr(configFilePath.size() - 5) == ".json") {
        std::filesystem::path basePath = std::filesystem::path(configFilePath).parent_path();
        m_projectBasePath = basePath.string();
        std::ifstream f(configFilePath);
        if (!f) { RecordError("LoadProject", "Could not open project config: " + configFilePath); return false; }
        try {
            json j; f >> j;
            if (j.contains("runtime")) {
                m_project.configs.start_node = j["runtime"].value("start_scene", "init");
                m_project.configs.debug_mode = j["runtime"].value("debug_mode", "none");
                m_project.configs.strict_assets = j["runtime"].value("strict_assets", false);
                
                if (j["runtime"].contains("scripts")) {
                    for (const auto& scriptPath : j["runtime"]["scripts"]) {
                        std::string fullPath = (basePath / scriptPath.get<std::string>()).string();
                        if (!BitScriptInterpreter::LoadScriptFile(fullPath, m_project)) {
                            for (const auto& err : m_project.parseErrors)
                                std::cerr << "[BitScript] " << err << "\n";
                            RecordError("LoadProject", "Failed to parse script: " + fullPath);
                            return false;
                        }
                    }
                } else {
                    RecordError("LoadProject", "project.json must contain 'runtime.scripts' array.");
                    return false;
                }
            } else {
                RecordError("LoadProject", "project.json must contain a 'runtime' object.");
                return false;
            }

            if (j.contains("narrative")) {
                auto& n = j["narrative"];
                m_project.configs.mode = n.value("mode", "typewriter");
                m_project.configs.reveal_speed = n.value("reveal_speed", 45.0f);
                m_project.configs.auto_play_delay = n.value("auto_play_delay", 2.0f);
                m_project.configs.auto_save = n.value("auto_save", false);
                m_project.configs.max_slots = n.value("max_save_slots", 5);
            }

            if (j.contains("directories")) {
                for (auto& [key, val] : j["directories"].items()) {
                    std::string dirPath = (basePath / val.get<std::string>()).string();
                    if (!std::filesystem::exists(dirPath)) {
                        std::filesystem::create_directories(dirPath);
                        Log("Created missing directory: " + dirPath);
                    }
                }
            }

            if (j.contains("ui_layouts")) {
                std::string uiDir = j.contains("directories") ? j["directories"].value("ui", "") : "";
                for (auto& [id, def] : j["ui_layouts"].items()) {
                    UILayoutDef udef;
                    udef.id = id;
                    std::string relPath = def.value("path", "");
                    udef.path = (basePath / uiDir / relPath).string();
                    udef.layer = def.value("layer", 0);
                    udef.active = def.value("active", false);
                    udef.visible = def.value("visible", true);
                    m_project.uiLayouts[id] = udef;
                }
            }

            std::string spritesDir = j.contains("directories") ? j["directories"].value("sprites", "") : "";
            if (!spritesDir.empty()) spritesDir = (basePath / spritesDir).string();

            // Entity loading
            if (j.contains("directories") && j["directories"].contains("entities")) {
                std::string entDir = (basePath / j["directories"]["entities"].get<std::string>()).string();
                if (std::filesystem::exists(entDir)) {
                    for (const auto& entry : std::filesystem::directory_iterator(entDir)) {
                        if (entry.path().extension() == ".json") {
                            m_fileWatchTimestamps[entry.path().string()] = std::filesystem::last_write_time(entry.path());
                            std::ifstream ef(entry.path());
                            if (ef) {
                                try {
                                    json ej; ef >> ej;
                                    Entity e;
                                    e.id = entry.path().stem().string();
                                    e.name = ej.value("name", e.id);
                                    e.default_pos_x = ej.value("default_pos_x", 0.5f);
                                    if (ej.contains("sprites")) {
                                        for (auto& [sname, sdef] : ej["sprites"].items()) {
                                            SpriteDef sd;
                                            sd.path = sdef.value("path", "");
                                            if (!spritesDir.empty() && !sd.path.empty()) {
                                                sd.path = (std::filesystem::path(spritesDir) / sd.path).string();
                                            }
                                            sd.frames = sdef.value("frames", 1);
                                            sd.speed = sdef.value("speed", 5.0f);
                                            sd.scale = sdef.value("scale", 1.0f);
                                            e.sprites[sname] = sd;
                                        }
                                    }
                                    if (ej.contains("aliases")) e.aliases = ej["aliases"];
                                    m_project.entities[e.id] = e;
                                } catch (const std::exception& err) {
                                    Log("Failed to parse entity file " + entry.path().string() + ": " + err.what(), "WARN");
                                }
                            }
                        }
                    }
                } else {
                    Log("Entities directory not found: " + entDir, "WARN");
                }
            }

            // Asset Auto-Discovery
            if (j.contains("directories") && j["directories"].contains("assets")) {
                std::string assetsDir = (basePath / j["directories"]["assets"].get<std::string>()).string();
                if (std::filesystem::exists(assetsDir)) {
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(assetsDir)) {
                        if (entry.is_regular_file()) {
                            std::string ext = entry.path().extension().string();
                            std::string stem = entry.path().stem().string();
                            std::string path = entry.path().string();
                            if (ext == ".png" || ext == ".jpg") m_project.backgrounds[stem] = path;
                            else if (ext == ".mp3" || ext == ".ogg") m_project.music[stem] = path;
                            else if (ext == ".wav") m_project.sfx[stem] = path;
                            else if (ext == ".ttf") m_project.fonts[stem] = path;
                        }
                    }
                } else {
                    Log("Assets directory not found: " + assetsDir, "WARN");
                }
            }

            m_vm->BuildLabelIndex();
            m_state.UIStates().clear();
            for (auto& [id, def] : m_project.uiLayouts) {
                m_state.UIStates()[id] = def;
                if (!def.path.empty() && std::filesystem::exists(def.path)) {
                    m_fileWatchTimestamps[def.path] = std::filesystem::last_write_time(def.path);
                }
                if (def.active) {
                    UICommand cmd; cmd.type = UICommand::Type::Load;
                    cmd.name = id; cmd.arg1 = def.path; cmd.layer = def.layer;
                    m_pendingUICommands.push_back(cmd);
                }
            }
            return true;

        } catch (const std::exception& e) {
            RecordError("LoadProject", std::string("Failed to load project JSON: ") + e.what());
            return false;
        }
    }

    if (configFilePath.size() > 10 && configFilePath.substr(configFilePath.size() - 10) == ".bitscript") {
        bool result = BitScriptInterpreter::LoadScriptFile(configFilePath, m_project);
        if (result) {
            m_vm->BuildLabelIndex();
            m_state.UIStates().clear();
            for (auto& [id, def] : m_project.uiLayouts) {
                m_state.UIStates()[id] = def;
                if (def.active) {
                    UICommand cmd; cmd.type = UICommand::Type::Load;
                    cmd.name = id; cmd.arg1 = def.path; cmd.layer = def.layer;
                    m_pendingUICommands.push_back(cmd);
                }
            }
        }
        return result;
    }

    if (configFilePath.size() > 5 && configFilePath.substr(configFilePath.size() - 5) == ".bitc") {
        return LoadBytecodeFile(configFilePath);
    }

    RecordError("LoadProject", "Unsupported file type: " + configFilePath + ". Expected .json, .bitscript or .bitc.");
    return false;
}

void BitRuntime::CompileProject(const std::string& outputPath) {
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
        case BitOp::UI_LOAD:     return "UI_LOAD";
        case BitOp::UI_UNLOAD:   return "UI_UNLOAD";
        case BitOp::UI_ACTIVATE: return "UI_ACTIVATE";
        case BitOp::UI_DEACTIVATE: return "UI_DEACTIVATE";
        case BitOp::UI_SET:      return "UI_SET";
        case BitOp::EMIT:        return "EMIT";
        case BitOp::WAIT_EVENT:  return "WAIT_EVENT";
        case BitOp::HALT:        return "HALT";
        default:                 return "NOP";
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
    if (s=="UI_LOAD")    return BitOp::UI_LOAD;
    if (s=="UI_UNLOAD")  return BitOp::UI_UNLOAD;
    if (s=="UI_ACTIVATE") return BitOp::UI_ACTIVATE;
    if (s=="UI_DEACTIVATE") return BitOp::UI_DEACTIVATE;
    if (s=="UI_SET")     return BitOp::UI_SET;
    if (s=="EMIT")       return BitOp::EMIT;
    if (s=="WAIT_EVENT") return BitOp::WAIT_EVENT;
    if (s=="HALT")       return BitOp::HALT;
    return BitOp::HALT;
}

bool BitRuntime::SaveBytecode(const std::string& path) const {
    if (m_project.bytecode.empty()) {
        Log("SaveBytecode: no bytecode to save.", "WARN");
        return false;
    }
    json root;
    root["magic"]   = BITC_MAGIC;
    root["version"] = BITC_VERSION;

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
    for (auto& [k,v] : m_project.uiLayouts) {
        root["ui_layouts"][k] = { {"id",v.id}, {"path",v.path}, {"layer",v.layer}, {"active",v.active}, {"visible",v.visible} };
    }
    for (auto& [k,v] : m_project.variables) {
        json vj = { {"id", v.id}, {"initial_value", v.initial_value} };
        if (v.min.has_value()) vj["min"] = v.min.value();
        if (v.max.has_value()) vj["max"] = v.max.value();
        root["variables"][k] = vj;
    }
    for (auto& [k,e] : m_project.entities) {
        json ej = { {"id",e.id},{"name",e.name},{"default_pos_x",e.default_pos_x} };
        if (!e.aliases.empty()) ej["aliases"] = e.aliases;
        for (auto& [sn,sd] : e.sprites)
            ej["sprites"][sn] = { {"path",sd.path},{"frames",sd.frames},{"speed",sd.speed},{"scale",sd.scale} };
        root["entities"][k] = ej;
    }

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

bool BitRuntime::LoadBytecodeFile(const std::string& path) {
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
    if (root.contains("ui_layouts")) {
        for (auto& [k,vj] : root["ui_layouts"].items()) {
            UILayoutDef def;
            def.id = vj.value("id", k);
            def.path = vj.value("path", "");
            def.layer = vj.value("layer", 0);
            def.active = vj.value("active", true);
            def.visible = vj.value("visible", true);
            m_project.uiLayouts[k] = def;
        }
    }
    if (root.contains("variables"))
        for (auto& [k,vj] : root["variables"].items()) {
            VariableDef vd; vd.id = vj.value("id",k); vd.initial_value = vj.value("initial_value",0);
            if (vj.contains("min")) vd.min = vj["min"].get<int>();
            if (vj.contains("max")) vd.max = vj["max"].get<int>();
            m_project.variables[k] = vd; m_state.SetVariable(k, vd.initial_value);
        }
    if (root.contains("entities"))
        for (auto& [k,ej] : root["entities"].items()) {
            Entity e; e.id = ej.value("id",k); e.name = ej.value("name","Unknown");
            e.default_pos_x = ej.value("default_pos_x",0.5f);
            if (ej.contains("sprites"))
                for (auto& [sn,sd] : ej["sprites"].items()) {
                    SpriteDef sdef; sdef.path = sd.value("path","");
                    sdef.frames = sd.value("frames",1); sdef.speed = sd.value("speed",5.0f); sdef.scale = sd.value("scale",1.0f);
                    e.sprites[sn] = sdef;
                }
            if (ej.contains("aliases")) e.aliases = ej["aliases"];
            m_project.entities[k] = e;
        }

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
    m_vm->BuildLabelIndex();
    return true;
}

void BitRuntime::SaveGame(int slot) {
    Log("Saving game to slot " + std::to_string(slot));
    std::string path = GetSlotPath(slot);
    std::string tmpPath = path + ".tmp";
    
    try {
        json j;
        j["version"] = 2;
        j["pc"] = (m_vm->IsWaiting() && m_vm->GetPC() > 0) ? m_vm->GetPC() - 1 : m_vm->GetPC();
        j["variables"] = m_state.GetVariables();
        j["revealed_count"] = m_revealedCount;
        j["screen_fade"] = m_state.ScreenFadeAlpha();
        
        j["stack"] = m_vm->GetCallStack();
        j["locals"] = m_vm->GetLocalScopes();
        
        j["active_bg"] = m_state.GetActiveBg();
        j["active_bgm"] = m_state.GetActiveBgm();
        j["speaker"] = m_currentSpeakerId;
        j["ui_hidden"] = m_state.IsUiHidden();
        
        json entities = json::object();
        for (const auto& [id, st] : m_state.GetActiveEntities()) {
            entities[id] = { 
                {"expression", st.expression}, 
                {"pos", st.pos}, 
                {"normX", st.currentNormX}, 
                {"alpha", st.alpha} 
            };
        }
        j["active_entities"] = entities;
        
        json ui = json::object();
        for (const auto& [id, st] : m_state.UIStates()) {
            ui[id] = { {"path", st.path}, {"layer", st.layer}, {"active", st.active}, {"visible", st.visible} };
        }
        j["ui_states"] = ui;
        
        std::string summary = m_currentSpeakerId.empty() ? "Narrative" : m_currentSpeakerId;
        j["meta"] = { 
            {"time", GetTimestamp()}, 
            {"pc", m_vm->GetPC()}, 
            {"text", "Scene: " + summary + " (PC:" + std::to_string(m_vm->GetPC()) + ")"} 
        };

        std::string data = j.dump(4);
        if (m_project.configs.encrypt_save) data = XORBuffer(data);
        
        std::filesystem::create_directories("save");
        std::ofstream f(tmpPath, std::ios::binary);
        if (!f) throw std::runtime_error("Could not open temp save file: " + tmpPath);
        f.write(data.c_str(), data.size());

#ifdef _WIN32
        _unlink(path.c_str());
#else
        unlink(path.c_str());
#endif
        rename(tmpPath.c_str(), path.c_str());
        
        Log("Save successful: " + path);
    } catch (const std::exception& e) {
        RecordError("SaveGame", std::string("Save failed: ") + e.what());
    }
}

bool BitRuntime::LoadGame(int slot) {
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
            
            int pc = j.value("pc", 0);
            m_vm->SetPC(pc >= 0 ? pc : 0);
            
            auto vars = j["variables"].get<std::unordered_map<std::string, int>>();
            for (auto& [k,v] : vars) m_state.SetVariable(k, v);
            
            float savedReveal = j.value("revealed_count", 0.0f);
            m_revealedCount = savedReveal;
            m_state.ScreenFadeAlpha() = j.value("screen_fade", 0.0f);
            
            m_vm->SetCallStack(j.value("stack", std::vector<int>{}));
            m_vm->SetLocalScopes(j.value("locals", std::vector<std::unordered_map<std::string, int>>{ {} }));

            m_state.ActiveBg() = j.value("active_bg", "");
            m_state.ActiveBgm() = j.value("active_bgm", "");
            m_currentSpeakerId = j.value("speaker", "");
            m_state.IsUiHidden() = j.value("ui_hidden", false);
            
            m_state.ActiveEntities().clear();
            if (j.contains("active_entities")) {
                for (auto& [id, s] : j["active_entities"].items()) {
                    ActiveEntityState st;
                    st.expression = s.value("expression", "idle");
                    st.pos = s.value("pos", "center");
                    st.currentNormX = s.value("normX", 0.5f);
                    st.targetNormX = st.currentNormX;
                    st.alpha = s.value("alpha", 1.0f);
                    st.targetAlpha = st.alpha;
                    m_state.ActiveEntities()[id] = st;
                }
            }

            if (j.contains("ui_states")) {
                m_state.UIStates().clear();
                for (auto& [id, s] : j["ui_states"].items()) {
                    UILayoutDef def;
                    def.id = id; def.path = s.value("path", ""); def.layer = s.value("layer", 0);
                    def.active = s.value("active", true); def.visible = s.value("visible", true);
                    m_state.UIStates()[id] = def;
                    
                    if (def.active) {
                        UICommand cmd;
                        cmd.name = id; cmd.arg1 = def.path; cmd.layer = def.layer; cmd.type = UICommand::Type::Load;
                        m_pendingUICommands.push_back(cmd);
                        if (!def.visible) {
                            UICommand vCmd; vCmd.name = id; vCmd.type = UICommand::Type::Set; vCmd.arg1 = "visible"; vCmd.arg2 = "false";
                            m_pendingUICommands.push_back(vCmd);
                        }
                    }
                }
            }
            
            m_isActive = true;
            m_vm->ResetWaiting();
            
            Log("Load successful from slot " + std::to_string(slot));
            m_vm->RunVM();
            
            m_revealedCount = savedReveal;
            UpdateSysVars();

            return true;
        } catch (const std::exception& e) {
            RecordError("LoadGame", std::string("Load failed: ") + e.what());
        }
    }
    return false;
}

bool BitRuntime::HasSave(int slot) const { 
    std::ifstream f(GetSlotPath(slot)); 
    return f.good(); 
}

std::optional<SaveMetadata> BitRuntime::GetSaveMetadata(int slot) const {
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

void BitRuntime::StartDialog(const std::string& startId) {
    m_isActive = true;
    m_vm->SetPC(0);
    m_vm->ClearStack();
    
    std::string id = startId.empty() ? m_project.configs.start_node : startId;
    Log("Starting dialog sequence: " + id);
    
    int target = m_vm->ResolveLabel(id);
    if (target != -1) m_vm->SetPC(target);
    
    m_vm->ResetWaiting();
    m_vm->RunVM();
}

void BitRuntime::SelectOption(int index) {
    if (index < 0 || index >= (int)m_visibleOptions.size()) return;
    std::string target = m_visibleOptions[index].next_id;
    m_visibleOptions.clear();
    
    int targetPC = m_vm->ResolveLabel(target);
    if (targetPC != -1) m_vm->SetPC(targetPC);
    
    m_vm->ResetWaiting();
    m_vm->RunVM();
}

void BitRuntime::Next() {
    if (!m_isActive || IsEventDelaying() || !m_pendingJumpId.empty() || m_inputLockoutTimer > 0.0f) return;
    if (!IsTextRevealing() && IsVisualAnimating()) return;
    if (IsTextRevealing()) { SkipReveal(); return; }
    
    m_isAutoNext = false; 
    if (!m_visibleOptions.empty()) return;

    m_vm->ResetWaiting();
    m_vm->RunVM();
}

void BitRuntime::EmitEvent(const std::string& evt) {
    if (!m_isActive) return;
    if (m_vm->IsWaitingForEvent(evt)) {
        m_vm->ResetWaiting();
        m_vm->RunVM();
    }
    if (!m_project.events.count(evt)) return;
    int saved_pc = m_vm->GetPC();
    for (const auto& ins : m_project.events.at(evt)) {
        m_vm->ExecuteInstruction(ins);
    }
    m_vm->SetPC(saved_pc);
}

void BitRuntime::Update(float dt) {
    if (!m_isActive) return;
    
    m_hotReloadTimer += dt;
    if (m_hotReloadTimer > 1.0f) {
        m_hotReloadTimer = 0.0f;
        CheckHotReload();
    }
    if (m_inputLockoutTimer > 0.0f) m_inputLockoutTimer -= dt;
    
    for (auto it = m_state.ActiveTimelines().begin(); it != m_state.ActiveTimelines().end(); ) {
        it->timer += dt * 1000.0f;
        auto tlIt = m_project.timelines.find(it->id);
        if (tlIt == m_project.timelines.end()) { it = m_state.ActiveTimelines().erase(it); continue; }
        auto& tl = tlIt->second;
        while (it->nextEventIdx < tl.events.size() && tl.events[it->nextEventIdx].time_ms <= it->timer) {
            auto& ev = tl.events[it->nextEventIdx++];
            m_vm->ExecuteInstruction({ev.op, ev.args, ev.metadata, -1});
        }
        if (it->nextEventIdx >= tl.events.size()) { it->finished = true; it = m_state.ActiveTimelines().erase(it); }
        else { ++it; }
    }

    if (m_state.ScreenFadeDuration() <= 0.0f) {
        m_state.ScreenFadeAlpha() = m_state.ScreenFadeTarget();
    } else if (m_state.ScreenFadeTimer() < m_state.ScreenFadeDuration()) {
        m_state.ScreenFadeTimer() += dt;
        float t = std::min(1.0f, m_state.ScreenFadeTimer() / m_state.ScreenFadeDuration());
        m_state.ScreenFadeAlpha() = m_state.ScreenFadeStart() + (m_state.ScreenFadeTarget() - m_state.ScreenFadeStart()) * t;
        if (m_state.ScreenFadeTimer() >= m_state.ScreenFadeDuration()) {
            m_state.ScreenFadeAlpha() = m_state.ScreenFadeTarget();
            m_state.ScreenFadeDuration() = 0.0f;
            m_state.ScreenFadeTimer() = 0.0f;
        }
    }

    if (m_state.BgFadeAlpha() < 1.0f) {
        m_state.BgFadeTimer() += dt;
        m_state.BgFadeAlpha() = std::min(1.0f, m_state.BgFadeTimer() / m_state.BgFadeDuration());
    }

    for (auto& [id, st] : m_state.ActiveEntities()) {
        if (st.moveTimer < st.moveDuration) {
            st.moveTimer += dt;
            float t = std::min(1.0f, st.moveTimer / st.moveDuration);
            t = 1.0f - powf(1.0f - t, 3.0f); 
            st.currentNormX = st.startNormX + (st.targetNormX - st.startNormX) * t;
            if (st.moveTimer >= st.moveDuration) st.currentNormX = st.targetNormX;
        }
        if (st.fadeTimer < st.fadeDuration) {
            st.fadeTimer += dt;
            float t = std::min(1.0f, st.fadeTimer / st.fadeDuration);
            st.alpha = st.startAlpha + (st.targetAlpha - st.startAlpha) * t;
            if (st.fadeTimer >= st.fadeDuration) st.alpha = st.targetAlpha;
        }
    }

    if (m_engineDelayTimer > 0.0f) {
        m_engineDelayTimer -= dt;
        if (m_engineDelayTimer <= 0.0f) {
            m_engineDelayTimer = 0.0f;
            if (m_revealedCount < 0.0f) m_revealedCount = 0.0f;
            if (m_vm->IsWaiting() && m_vm->IsDelayed() && !m_project.bytecode.empty()) {
                m_vm->ResetWaiting();
                m_vm->RunVM();
            }
        }
        return;
    }

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
    
    if (m_state.ShakeIntensity() > 0) m_state.ShakeIntensity() = std::max(0.0f, m_state.ShakeIntensity() - dt * 20.0f);

    if (m_vm->IsWaiting() && !m_vm->GetWaitActionType().empty()) {
        bool done = false;
        std::string type = m_vm->GetWaitActionType();
        if (type == "sfx") done = m_pendingSFX.empty(); 
        else if (type == "move") {
            done = true;
            for (auto& [id, s] : m_state.ActiveEntities()) if (s.moveTimer < s.moveDuration) done = false;
        }
        else if (type == "fade") {
            done = true;
            for (auto& [id, s] : m_state.ActiveEntities()) if (s.fadeTimer < s.fadeDuration) done = false;
            if (m_state.BgFadeTimer() < m_state.BgFadeDuration()) done = false;
            if (m_state.ScreenFadeTimer() < m_state.ScreenFadeDuration()) done = false;
        }
        else if (type == "all") done = !IsVisualAnimating();
        else if (type == "timeline") {
            done = true;
            for (const auto& atl : m_state.ActiveTimelines()) if (atl.isBlocking) done = false;
        }

        if (done) {
            m_vm->ResetWaiting();
            m_vm->RunVM();
        }
    }

    if (!IsTextRevealing() && m_isAutoNext && m_waitTimer <= 0.0f && m_visibleOptions.empty() && !IsEventDelaying() && m_vm->GetWaitActionType().empty()) {
        Next();
    }

    if (m_isAutoPlaying && !IsTextRevealing() && m_visibleOptions.empty() && m_waitTimer <= 0.0f) {
        m_autoPlayTimer += dt;
        if (m_autoPlayTimer >= m_project.configs.auto_play_delay) {
            m_autoPlayTimer = 0.0f;
            Next();
        }
    } else {
        m_autoPlayTimer = 0.0f;
    }

    UpdateSysVars();
}

void BitRuntime::SkipReveal() { 
    m_revealedCount = (float)m_cachedTotalChars; 
    m_waitTimer = 0.0f;
    m_inputLockoutTimer = 0.2f;
}
bool BitRuntime::IsTextRevealing() const { return m_isActive && m_revealedCount < (float)m_cachedTotalChars; }

std::string BitRuntime::GetVisibleContent() const {
    std::string out = "";
    int limit = (int)m_revealedCount;
    for (int i = 0; i < limit && i < (int)m_cachedParsedContent.size(); ++i) {
        for(int j=0; j<5 && m_cachedParsedContent[i].ch[j]; j++) out += m_cachedParsedContent[i].ch[j];
    }
    return out;
}

size_t BitRuntime::GetUTF8Length(const std::string& s) const {
    size_t count = 0;
    for (size_t i = 0; i < s.length(); ) {
        unsigned char c = s[i]; i += (c <= 127) ? 1 : ((c & 0xE0) == 0xC0) ? 2 : ((c & 0xF0) == 0xE0) ? 3 : 4;
        count++;
    }
    return count;
}

std::string BitRuntime::InterpolateVariables(const std::string& text) const {
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

int BitRuntime::GetVariable(const std::string& name) const { 
    return m_state.GetVariable(name);
}

void BitRuntime::SetVariable(const std::string& name, int value) {
    if (name.substr(0, 5) == "__tmp") { m_state.SetVariable(name, value); return; }
    auto it = m_project.variables.find(name);
    if (it == m_project.variables.end()) { RecordError("SetVariable", "Variable '" + name + "' not declared."); return; }
    const auto& d = it->second;
    int oldVal = m_state.GetVariable(name);
    int v = value;
    if (d.min) v = std::max(v, *d.min);
    if (d.max) v = std::min(v, *d.max);
    m_state.SetVariable(name, v);
    m_state.EventTrace().push_back({GetCurrentLabel(), "SET", name, oldVal, v});
    if (m_state.EventTrace().size() > 50) m_state.EventTrace().erase(m_state.EventTrace().begin());
}

const std::vector<std::string>& BitRuntime::ConsumePendingSFX() {
    static std::vector<std::string> copy;
    copy = m_pendingSFX; m_pendingSFX.clear(); return copy;
}

void BitRuntime::RecordError(const std::string& context, const std::string& msg) {
    std::string full = "[" + context + "] " + msg;
    m_errors.push_back(full); Log(full, "ERROR");
}

void BitRuntime::Log(const std::string& msg, const std::string& level) const {
    std::string mode = m_project.configs.debug_mode;
    bool isError = (level == "ERROR");
    if (mode == "none" && !isError) return;
    std::string tag = "[BitEngine:" + level + "] ";
    if (isError || mode == "debug_overlay" || mode == "debug_all") std::cout << tag << msg << std::endl;
    if (isError || mode == "debug_file" || mode == "debug_all") {
        std::ofstream logFile("debug.log", std::ios_base::app);
        if (logFile) {
            auto t = std::time(nullptr);
            logFile << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << " | " << tag << msg << "\n";
        }
    }
}

ValidationResult BitRuntime::ValidateProject(const BitProject& p) {
    ValidationResult results;
    auto messages = BitScriptAnalyzer::Analyze(p);
    for (const auto& msg : messages) {
        std::string prefix = (msg.level == AnalysisMessage::Level::ERROR) ? "[ERROR]" : "[WARN]";
        results.errors.push_back(prefix + (msg.line > 0 ? " (Line " + std::to_string(msg.line) + "): " : " ") + msg.message);
    }
    return results;
}

float BitRuntime::ParseXParam(const nlohmann::json& params, const std::string& key) const {
    if (params.contains(key)) {
        const auto& v = params[key];
        if (v.is_number()) return v.get<float>();
        if (v.is_string()) return ParsePosition(v.get<std::string>());
    }
    return 0.5f;
}

const Entity* BitRuntime::GetCurrentEntity() const {
    auto it = m_project.entities.find(m_currentSpeakerId);
    return (it != m_project.entities.end()) ? &it->second : nullptr;
}

std::string BitRuntime::XORBuffer(const std::string& d) const { std::string o = d; for (size_t i = 0; i < d.size(); ++i) o[i] ^= KEY[i % KEY.size()]; return o; }
std::string BitRuntime::GetSlotPath(int s) const { return "save/" + m_project.configs.save_prefix + std::to_string(s) + ".bin"; }
std::string BitRuntime::GetTimestamp() const {
    std::time_t t = std::time(nullptr); std::tm tm = *std::localtime(&t);
    std::stringstream ss; ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S"); return ss.str();
}

float BitRuntime::ParsePosition(const std::string& pos) const {
    if (pos == "left") return 0.2f;
    if (pos == "right") return 0.8f;
    if (pos == "center") return 0.5f;
    if (m_state.GetVariables().count(pos)) return (float)m_state.GetVariable(pos);
    try { return std::stof(pos); } catch(...) { return 0.5f; }
}

void BitRuntime::ProcessEvents(const std::vector<Event>& events) {
    for (const auto& e : events) {
        auto& p = e.params;
        if (e.op == "shake")    { TriggerShake(ResolveParamFloat(p, "intensity", 5.0f)); continue; }
        if (e.op == "play_sfx") { m_pendingSFX.push_back(p.value("id", "")); continue; }
        if (e.op == "clear")    { m_state.ActiveEntities().clear(); continue; }
        if (e.op == "expression") {
            std::string target = p.value("target", "");
            if (m_state.ActiveEntities().count(target)) m_state.ActiveEntities()[target].expression = p.value("id", "idle");
            continue;
        }
        if (e.op == "hide") {
            std::string target = p.value("target", "");
            if (m_state.ActiveEntities().count(target)) m_state.ActiveEntities()[target].visible = false;
            continue;
        }
        if (e.op == "leave") { m_state.ActiveEntities().erase(p.value("target", "")); continue; }
        if (e.op == "pos") {
            std::string target = p.value("target", "");
            auto& s = m_state.ActiveEntities()[target];
            float x = ParseXParam(p);
            s.pos = std::to_string(x); s.currentNormX = x; s.targetNormX = x; s.visible = true;
            continue;
        }
        if (e.op == "jump")  { m_pendingJumpId = p.value("target", ""); continue; }
        if (e.op == "delay") { m_engineDelayTimer = ResolveParamInt(p, "duration", 0) / 1000.0f; continue; }
        if (e.op == "move") {
            std::string target = p.value("target", "");
            auto& s = m_state.ActiveEntities()[target];
            float x = ParseXParam(p);
            s.pos = std::to_string(x); s.targetNormX = x; s.startNormX = s.currentNormX;
            s.moveDuration = ResolveParamInt(p, "duration", 0) / 1000.0f; s.moveTimer = 0.0f;
            s.visible = true; continue;
        }
        if (e.op == "fade") {
            std::string target = p.value("target", "");
            int duration = ResolveParamInt(p, "duration", 0);
            if (target == "bg") {
                std::string bgId = p.value("id", "");
                if (!bgId.empty()) {
                    m_state.PrevBg() = m_state.GetActiveBg(); m_state.ActiveBg() = bgId;
                    m_state.BgFadeAlpha() = 0.0f; m_state.BgFadeTimer() = 0.0f; m_state.BgFadeDuration() = std::max(0.01f, duration / 1000.0f);
                }
            } else {
                auto& s = m_state.ActiveEntities()[target];
                s.targetAlpha = ResolveParamFloat(p, "alpha", 1.0f); s.startAlpha = s.alpha;
                s.fadeDuration = std::max(0.01f, duration / 1000.0f); s.fadeTimer = 0.0f;
                s.visible = true;
            }
            continue;
        }
        if (e.op == "fade_screen") {
            float alpha = ResolveParamFloat(p, "alpha", 0.0f);
            alpha = std::max(0.0f, std::min(1.0f, alpha));
            m_state.ScreenFadeTarget() = alpha; m_state.ScreenFadeStart() = m_state.ScreenFadeAlpha();
            int dur = ResolveParamInt(p, "duration", 0);
            m_state.ScreenFadeDuration() = std::max(0.0f, dur / 1000.0f); m_state.ScreenFadeTimer() = 0.0f;
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
            int lo = ResolveParamInt(p, "min", 0), hi = ResolveParamInt(p, "max", 1);
            if (lo > hi) std::swap(lo, hi);
            next = lo + (std::rand() % (hi - lo + 1));
        }
        SetVariable(var, next);
    }
}

int BitRuntime::SafeStoi(const std::string& s) const {
    if (s.empty()) return 0;
    try { return std::stoi(s); } catch (...) { return 0; }
}

int BitRuntime::ResolveParamInt(const nlohmann::json& params, const std::string& key, int default_val) const {
    if (!params.contains(key)) return default_val;
    const auto& v = params[key];
    if (v.is_number()) return v.get<int>();
    if (v.is_string()) {
        std::string s = v.get<std::string>();
        if (m_state.GetVariables().count(s)) return m_state.GetVariable(s);
        try { return std::stoi(s); } catch(...) { return default_val; }
    }
    return default_val;
}

float BitRuntime::ResolveParamFloat(const nlohmann::json& params, const std::string& key, float default_val) const {
    if (!params.contains(key)) return default_val;
    const auto& v = params[key];
    if (v.is_number()) return v.get<float>();
    if (v.is_string()) {
        std::string s = v.get<std::string>();
        if (m_state.GetVariables().count(s)) return (float)m_state.GetVariable(s);
        try { return std::stof(s); } catch(...) { return default_val; }
    }
    return default_val;
}

void BitRuntime::UpdateSysVars() {
    if (!m_currentSpeakerId.empty() && m_project.entities.count(m_currentSpeakerId)) {
        m_sysVars["var.entity_name"] = m_project.entities.at(m_currentSpeakerId).name;
        m_sysVars["var.entity_id"]   = m_currentSpeakerId;
    } else {
        m_sysVars["var.entity_name"] = "";
        m_sysVars["var.entity_id"]   = "";
    }
    m_sysVars["var.dialog"]       = m_cachedInterpolatedContent;
    m_sysVars["var.is_revealing"] = IsTextRevealing()  ? "true" : "false";
    m_sysVars["var.is_waiting_input"] = (!IsTextRevealing() && !m_cachedInterpolatedContent.empty()) ? "true" : "false";
    m_sysVars["var.ui_visible"]   = !m_state.IsUiHidden() ? "true" : "false";
    m_sysVars["var.choices_visible"] = (!m_visibleOptions.empty() && !IsTextRevealing()) ? "true" : "false";
    m_sysVars["var.choices"] = nlohmann::json::array();
    if (!m_visibleOptions.empty() && !IsTextRevealing()) {
        for (size_t i = 0; i < m_visibleOptions.size(); ++i) {
            m_sysVars["var.choices"].push_back({ {"text", m_visibleOptions[i].content}, {"index", i} });
        }
    }
    for (const auto& [k, v] : m_state.GetVariables()) m_sysVars["var." + k] = std::to_string(v);
}

std::vector<UICommand> BitRuntime::DrainUICommands() {
    std::vector<UICommand> out; std::swap(out, m_pendingUICommands); return out;
}

std::string BitRuntime::GetCurrentLabel() const {
    std::string best = "root"; int bestPC = -1;
    for (const auto& [name, pc] : m_vm->GetLabelIndex()) {
        if (pc <= m_vm->GetPC() && pc > bestPC) { best = name; bestPC = pc; }
    }
    return best;
}

bool BitRuntime::IsVisualAnimating() const {
    for (const auto& [id, state] : m_state.GetActiveEntities()) if (state.moveTimer < state.moveDuration || state.fadeTimer < state.fadeDuration) return true;
    if (m_state.BgFadeAlpha() < 1.0f) return true;
    return false;
}

void BitRuntime::CheckHotReload() {
    bool changed = false;
    for (auto& [pathStr, lastTime] : m_fileWatchTimestamps) {
        try {
            if (std::filesystem::exists(pathStr)) {
                auto newTime = std::filesystem::last_write_time(pathStr);
                if (newTime > lastTime) {
                    m_fileWatchTimestamps[pathStr] = newTime;
                    changed = true;
                    Log("Hot-reloading file: " + pathStr);
                    if (pathStr.find("entities") != std::string::npos) {
                        std::ifstream ef(pathStr);
                        if (ef) {
                            json ej; ef >> ej;
                            Entity e;
                            e.id = std::filesystem::path(pathStr).stem().string();
                            e.name = ej.value("name", e.id);
                            e.default_pos_x = ej.value("default_pos_x", 0.5f);
                            if (ej.contains("sprites")) {
                                for (auto& [sname, sdef] : ej["sprites"].items()) {
                                    SpriteDef sd; sd.path = sdef.value("path", "");
                                    if (!sd.path.empty()) sd.path = (std::filesystem::path(m_projectBasePath) / "assets/sprites" / sd.path).string();
                                    sd.frames = sdef.value("frames", 1);
                                    sd.speed = sdef.value("speed", 5.0f);
                                    sd.scale = sdef.value("scale", 1.0f);
                                    e.sprites[sname] = sd;
                                }
                            }
                            if (ej.contains("aliases")) e.aliases = ej["aliases"];
                            m_project.entities[e.id] = e;
                        }
                    } else if (pathStr.find("ui") != std::string::npos) {
                        for (auto& [id, def] : m_project.uiLayouts) {
                            if (def.path == pathStr && def.active) {
                                UICommand cmd; cmd.type = UICommand::Type::Load;
                                cmd.name = id; cmd.arg1 = def.path; cmd.layer = def.layer;
                                m_pendingUICommands.push_back(cmd);
                            }
                        }
                    }
                }
            }
        } catch (...) {}
    }
}
