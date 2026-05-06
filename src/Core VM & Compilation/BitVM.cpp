#include "BitVM.hpp"
#include "BitRuntime.hpp"
#include "BitRichText.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// Virtual Machine: Bytecode Execution
// ─────────────────────────────────────────────────────────────────────────────
// Executes BitEngine bytecode instructions, managing control flow and state.
// ─────────────────────────────────────────────────────────────────────────────

BitVM::BitVM(BitRuntime& engine) : m_engine(engine), m_pc(0), m_isWaiting(false), m_isDelayed(false) {}

void BitVM::BuildLabelIndex() {
    m_labelIndex.clear();
    const auto& bytecode = m_engine.GetProject().bytecode;
    for (int i = 0; i < (int)bytecode.size(); ++i) {
        if (bytecode[i].op == BitOp::LABEL && !bytecode[i].args.empty()) {
            m_labelIndex[bytecode[i].args[0]] = i;
        }
    }
}

int BitVM::ResolveLabel(const std::string& label) const {
    auto it = m_labelIndex.find(label);
    if (it != m_labelIndex.end()) return it->second;
    return -1;
}

// Resolve an argument that may be a dynamic @var reference
std::string BitVM::ResolveAssetArg(const std::string& arg) const {
    if (!arg.empty() && arg[0] == '@') {
        std::string varName = arg.substr(1);
        // Try string locals first, then fall back to int-as-string
        if (!m_localVariables.empty() && m_localVariables.back().count(varName))
            return std::to_string(m_localVariables.back().at(varName));
        return std::to_string(m_engine.GetVariable(varName));
    }
    return arg;
}

int BitVM::GetVariable(const std::string& name) const {
    if (!m_localVariables.empty() && m_localVariables.back().count(name)) return m_localVariables.back().at(name);
    return m_engine.GetVariable(name);
}

void BitVM::SetVariable(const std::string& name, int value) {
    if (!m_localVariables.empty() && m_localVariables.back().count(name)) {
        m_localVariables.back()[name] = value;
    } else {
        m_engine.SetVariable(name, value);
    }
}

void BitVM::RunVM() {
    auto& project = m_engine.GetProject();
    while (m_engine.m_isActive && !m_isWaiting && m_pc < (int)project.bytecode.size()) {
        ExecuteInstruction(project.bytecode[m_pc++]);
    }
}

void BitVM::ExecuteInstruction(const BitInstruction& ins) {
    auto& args = ins.args;
    auto& state = m_engine.GetState();
    
    switch (ins.op) {
        case BitOp::SAY: {
            std::string entityId = args[0];
            std::string content = args[1];

            bool join = false;
            m_engine.m_isAutoNext = false; 
            if (!ins.metadata.empty()) {
                if (ins.metadata.contains("join")) join = (ins.metadata["join"].get<std::string>() == "true");
                if (ins.metadata.contains("bg")) {
                    std::string bgId = ins.metadata["bg"];
                    if (state.GetActiveBg() != bgId) {
                        state.PrevBg() = state.GetActiveBg(); state.ActiveBg() = bgId;
                        state.BgFadeAlpha() = 0.0f; state.BgFadeTimer() = 0.0f; state.BgFadeDuration() = 0.8f;
                    }
                }
                if (ins.metadata.contains("bgm")) state.ActiveBgm() = ins.metadata["bgm"];
                if (ins.metadata.contains("auto_next")) m_engine.m_isAutoNext = (ins.metadata["auto_next"].get<std::string>() == "true");
                if (ins.metadata.contains("pre_delay")) {
                    auto& pd = ins.metadata["pre_delay"];
                    float ms = pd.is_number() ? pd.get<float>() : (float)m_engine.SafeStoi(pd.get<std::string>());
                    m_engine.m_engineDelayTimer = ms / 1000.0f;
                }
            }

            if (!entityId.empty() && entityId != "system") {
                auto& eState = state.ActiveEntities()[entityId];
                eState.visible = true;
                
                if (ins.metadata.contains("alias")) {
                    std::string aliasName = ins.metadata["alias"];
                    const auto* entity = m_engine.GetEntity(entityId);
                    if (entity && entity->aliases.count(aliasName)) {
                        auto aliasData = entity->aliases.at(aliasName);
                        for (auto& [k, v] : aliasData.items()) {
                            if (!ins.metadata.contains(k)) {
                                if (k == "sprite") eState.expression = v;
                                else if (k == "pos") {
                                    eState.pos = v;
                                    eState.targetNormX = m_engine.ParsePosition(eState.pos);
                                    eState.currentNormX = eState.targetNormX;
                                }
                                else if (k == "alpha") {
                                    eState.alpha = std::stof(v.get<std::string>());
                                    eState.targetAlpha = eState.alpha;
                                }
                            }
                        }
                    }
                }

                if (!ins.metadata.empty()) {
                    if (ins.metadata.contains("sprite")) eState.expression = ins.metadata["sprite"];
                    if (ins.metadata.contains("pos")) {
                        eState.pos = ins.metadata["pos"];
                        eState.targetNormX = m_engine.ParsePosition(eState.pos);
                        eState.currentNormX = eState.targetNormX;
                    }
                    if (ins.metadata.contains("alpha")) {
                        eState.alpha = std::stof(ins.metadata["alpha"].get<std::string>());
                        eState.targetAlpha = eState.alpha;
                    }
                }
            }

            if (!join) {
                if (entityId != "system" && entityId != "narration" && !state.ActiveEntities().count(entityId)) {
                    state.ActiveEntities().clear();
                }
            }

            m_engine.m_currentSpeakerId = (entityId == "narration" || entityId == "system") ? "" : entityId;
            m_engine.m_cachedInterpolatedContent = m_engine.InterpolateVariables(content);
            m_engine.m_cachedParsedContent = RichTextParser::Parse(m_engine.m_cachedInterpolatedContent);
            m_engine.m_cachedTotalChars = m_engine.m_cachedParsedContent.size();
            m_engine.m_revealedCount = (m_engine.m_project.configs.mode == "instant") ? (float)m_engine.m_cachedTotalChars : 0.0f;
            m_engine.m_waitTimer = 0.0f;
            m_isWaiting = !content.empty();

            if (!content.empty()) {
                std::string speaker = "SYSTEM";
                if (entityId == "narration") speaker = "";
                else if (m_engine.m_project.entities.count(entityId)) speaker = m_engine.m_project.entities.at(entityId).name;
                HistoryEntry entry = { speaker, m_engine.m_cachedInterpolatedContent, m_engine.m_cachedParsedContent };
                state.History().push_back(entry);
                if (state.History().size() > 100) state.History().erase(state.History().begin());
            }

            if (m_engine.m_project.configs.auto_save) m_engine.SaveGame(0);
            m_engine.UpdateSysVars();
            break;
        }
        case BitOp::SET:     SetVariable(args[0], m_engine.SafeStoi(args[1])); break;
        case BitOp::SET_REF: SetVariable(args[0], GetVariable(args[1])); break;
        case BitOp::ADD:     SetVariable(args[0], GetVariable(args[0]) + m_engine.SafeStoi(args[1])); break;
        case BitOp::ADD_REF: SetVariable(args[0], GetVariable(args[0]) + GetVariable(args[1])); break;
        case BitOp::SUB:     SetVariable(args[0], GetVariable(args[0]) - m_engine.SafeStoi(args[1])); break;
        case BitOp::SUB_REF: SetVariable(args[0], GetVariable(args[0]) - GetVariable(args[1])); break;
        case BitOp::MUL:     SetVariable(args[0], GetVariable(args[0]) * m_engine.SafeStoi(args[1])); break;
        case BitOp::MUL_REF: SetVariable(args[0], GetVariable(args[0]) * GetVariable(args[1])); break;
        case BitOp::DIV: {
            int divisor = m_engine.SafeStoi(args[1]);
            if (divisor == 0) m_engine.RecordError("ExecuteInstruction", "Division by zero");
            else SetVariable(args[0], GetVariable(args[0]) / divisor);
            break;
        }
        case BitOp::DIV_REF: {
            int divisor = GetVariable(args[1]);
            if (divisor == 0) m_engine.RecordError("ExecuteInstruction", "Division by zero");
            else SetVariable(args[0], GetVariable(args[0]) / divisor);
            break;
        }
        case BitOp::GOTO: {
            auto it = m_labelIndex.find(args[0]);
            if (it != m_labelIndex.end()) {
                state.EventTrace().push_back({m_engine.GetCurrentLabel(), "JUMP", args[0], m_pc, it->second});
                m_pc = it->second;
            }
            break;
        }
        case BitOp::IF:
        case BitOp::IF_REF: {
            int v = GetVariable(args[0]);
            int val = (ins.op == BitOp::IF_REF) ? GetVariable(args[2]) : m_engine.SafeStoi(args[2]);
            std::string op = args[1];
            bool pass = false;
            if      (op == "==" || op == "=") pass = (v == val);
            else if (op == "!=")              pass = (v != val);
            else if (op == ">")               pass = (v > val);
            else if (op == "<")               pass = (v < val);
            else if (op == ">=")              pass = (v >= val);
            else if (op == "<=")              pass = (v <= val);
            
            state.EventTrace().push_back({std::to_string(m_pc-1), "IF", pass ? "TRUE" : "FALSE", v, val});
            if (pass) {
                auto it = m_labelIndex.find(args[3]);
                if (it != m_labelIndex.end()) {
                    m_pc = it->second;
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
            m_engine.m_visibleOptions.push_back(opt);
            break;
        }
        case BitOp::BG: {
            std::string id = ResolveAssetArg(args[0]);
            state.PrevBg() = state.GetActiveBg();
            state.ActiveBg() = id;
            state.BgFadeAlpha() = 0.0f;
            state.BgFadeTimer() = 0.0f;
            state.BgFadeDuration() = 0.8f;
            break;
        }
        case BitOp::BGM: {
            std::string id = ResolveAssetArg(args[0]);
            state.ActiveBgm() = id;
            break;
        }
        case BitOp::SFX: {
            std::string id = ResolveAssetArg(args[0]);
            nlohmann::json j; j["op"] = "play_sfx"; j["id"] = id;
            m_engine.ProcessEvents({{"play_sfx", j}});
            break;
        }
        case BitOp::UI_VISIBLE: {
            state.IsUiHidden() = (args[0] == "hide");
            m_engine.UpdateSysVars();
            break;
        }
        case BitOp::UI_LOAD: {
            std::string id = args[0];
            std::string path = args[1];
            int layer = (args.size() > 2) ? std::stoi(args[2]) : 0;
            UILayoutDef def;
            def.id = id; def.path = path; def.layer = layer; def.active = true; def.visible = true;
            state.UIStates()[id] = def;
            UICommand cmd;
            cmd.type = UICommand::Type::Load; cmd.name = id; cmd.arg1 = path; cmd.layer = layer;
            m_engine.m_pendingUICommands.push_back(cmd);
            break;
        }
        case BitOp::UI_UNLOAD: {
            state.UIStates().erase(args[0]);
            UICommand cmd;
            cmd.type = UICommand::Type::Unload; cmd.name = args[0];
            m_engine.m_pendingUICommands.push_back(cmd);
            break;
        }
        case BitOp::UI_ACTIVATE: {
            UICommand cmd;
            cmd.name = args[0];
            if (m_engine.m_project.uiLayouts.count(cmd.name)) {
                cmd.type = UICommand::Type::Load;
                cmd.arg1 = m_engine.m_project.uiLayouts[cmd.name].path;
                cmd.layer = (args.size() > 1) ? std::stoi(args[1]) : m_engine.m_project.uiLayouts[cmd.name].layer;
            } else if (state.UIStates().count(cmd.name)) {
                cmd.type = UICommand::Type::Load;
                cmd.arg1 = state.UIStates()[cmd.name].path;
                cmd.layer = (args.size() > 1) ? std::stoi(args[1]) : state.UIStates()[cmd.name].layer;
            } else {
                if (args.size() > 1) {
                    cmd.type = UICommand::Type::Load;
                    cmd.layer = std::stoi(args[1]);
                } else {
                    cmd.type = UICommand::Type::Activate;
                }
            }
            if (state.UIStates().count(cmd.name)) {
                state.UIStates()[cmd.name].active = true;
                state.UIStates()[cmd.name].visible = true;
            }
            state.EventTrace().push_back({m_engine.GetCurrentLabel(), "UI_ACTIVATE", cmd.name, 0, 0});
            m_engine.m_pendingUICommands.push_back(cmd);
            break;
        }
        case BitOp::UI_DEACTIVATE: {
            UICommand cmd;
            cmd.type = UICommand::Type::Deactivate; cmd.name = args[0];
            if (state.UIStates().count(cmd.name)) state.UIStates()[cmd.name].active = false;
            state.EventTrace().push_back({m_engine.GetCurrentLabel(), "UI_DEACTIVATE", cmd.name, 0, 0});
            m_engine.m_pendingUICommands.push_back(cmd);
            break;
        }
        case BitOp::UI_SET: {
            UICommand cmd;
            cmd.type = UICommand::Type::Set; cmd.name = args[0]; cmd.arg1 = args[1];
            cmd.arg2 = (args.size() > 2) ? args[2] : "";
            if (cmd.arg1 == "visible") {
                if (state.UIStates().count(cmd.name)) state.UIStates()[cmd.name].visible = (cmd.arg2 == "true" || cmd.arg2 == "1");
            }
            m_engine.m_pendingUICommands.push_back(cmd);
            break;
        }
        case BitOp::EVENT: {
            std::vector<Event> evs = { {args[0], ins.metadata} };
            m_engine.ProcessEvents(evs);
            if (args[0] == "delay") {
                m_isWaiting = true; m_isDelayed = true;
            }
            break;
        }
        case BitOp::CALL: {
            std::string labelId = args[0];
            int targetPC = ResolveLabel(labelId);
            if (targetPC != -1) {
                const BitInstruction* labelIns = &m_engine.GetProject().bytecode[targetPC];
                m_callStack.push_back(m_pc);
                m_localVariables.push_back({});
                if (labelIns && labelIns->args.size() > 1) {
                    for (size_t i = 1; i < labelIns->args.size(); ++i) {
                        std::string paramName = labelIns->args[i];
                        int val = 0;
                        if (args.size() > i) {
                            try { val = std::stoi(args[i]); }
                            catch (...) { val = GetVariable(args[i]); }
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
                m_pc = m_callStack.back(); m_callStack.pop_back(); m_localVariables.pop_back();
            }
            break;
        }
        case BitOp::SET_LOCAL: {
            if (!m_localVariables.empty()) m_localVariables.back()[args[0]] = m_engine.SafeStoi(args[1]);
            break;
        }
        case BitOp::WAIT_ACTION: {
            m_waitingForActionType = args[0]; m_isWaiting = true;
            break;
        }
        case BitOp::WAIT_INPUT: {
            m_isWaiting = true;
            break;
        }
        case BitOp::PLAY_TIMELINE: {
            std::string tid = args[0]; bool wait = (args[1] == "true");
            if (m_engine.GetProject().timelines.count(tid)) {
                ActiveTimeline atl; atl.id = tid; atl.isBlocking = wait;
                state.ActiveTimelines().push_back(atl);
                if (wait) { m_waitingForActionType = "timeline"; m_isWaiting = true; }
            }
            break;
        }
        case BitOp::WAIT_EVENT: {
            m_waitingForEventId = args[0]; m_isWaiting = true;
            break;
        }
        case BitOp::EMIT: {
            m_engine.EmitEvent(args[0]);
            break;
        }
        case BitOp::HALT: m_engine.m_isActive = false; break;
        default: break;
    }
}
