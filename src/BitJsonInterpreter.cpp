#include "headers/BitJsonInterpreter.hpp"
#include "headers/json.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

DialogProject BitJsonInterpreter::ParseConfig(const std::string& path) {
    DialogProject p;
    std::ifstream f(path); if (!f) return p;
    try {
        json j; f >> j;
        if (j.contains("configs")) {
            auto& c = j["configs"];
            p.configs.start_node = c.value("start_node", "dialog_start");
            p.configs.reveal_speed = c.value("reveal_speed", 30.0f);
            p.configs.auto_play_delay = c.value("auto_play_delay", 2000.0f) / 1000.0f;
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
        
        if (j.contains("entities")) {
            if (j["entities"].is_array()) for (auto& f : j["entities"]) p.configs.entity_files.push_back(f.get<std::string>());
            else if (j["entities"].is_object()) {
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

bool BitJsonInterpreter::LoadEntitiesFile(const std::string& path, DialogProject& p) {
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

bool BitJsonInterpreter::LoadVariablesFile(const std::string& path, DialogProject& p) {
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

bool BitJsonInterpreter::LoadDialogFile(const std::string& path, DialogProject& p) {
    std::ifstream f(path); if (!f) return false;
    try {
        json j; f >> j;
        
        auto parseEvent = [](const json& e) -> Event {
            return { e.value("op", ""), e };
        };

        std::function<ConditionNode(const json&)> parseCondNode = [&](const json& c) -> ConditionNode {
            ConditionNode node;
            if (c.contains("or") && c["or"].is_array()) {
                node.isGroup = true; node.groupLogic = "or";
                for (auto& child : c["or"]) node.children.push_back(parseCondNode(child));
            } else if (c.contains("and") && c["and"].is_array()) {
                node.isGroup = true; node.groupLogic = "and";
                for (auto& child : c["and"]) node.children.push_back(parseCondNode(child));
            } else {
                std::string op = c.value("op", "==");
                if (op == "=") op = "==";
                node.leaf = { c.value("var", ""), op, c.value("value", 0), c.value("ref", "") };
            }
            return node;
        };

        for (auto& [id, n] : j.items()) {
            DialogNode node;
            node.entity  = n.value("entity", "unk");
            node.content = n.value("content", "");
            if (n.contains("next_id") && !n["next_id"].is_null())
                node.next_id = n["next_id"].get<std::string>();

            if (n.contains("metadata")) {
                auto& m = n["metadata"];
                node.metadata.bg         = m.value("bg",         "");
                node.metadata.bgm        = m.value("bgm",        "");
                node.metadata.hide_ui    = m.value("hide_ui",    false);
                node.metadata.auto_next  = m.value("auto_next",  false);
                node.metadata.join       = m.value("join",       false);
                node.metadata.pre_delay  = m.value("pre_delay",  0);
                node.metadata.alpha      = m.value("alpha",      1.0f);
                node.metadata.expression = m.value("expression", "");
                node.metadata.pos        = m.value("pos",        "");
                node.metadata.transition          = m.value("transition",          "");
                node.metadata.transition_duration = m.value("transition_duration", 600);
                node.metadata.transition_delay    = m.value("transition_delay",    0);
            }

            if (n.contains("options")) {
                for (auto& o : n["options"]) {
                    DialogOption opt;
                    opt.content = o.value("content", "");
                    opt.style   = o.value("style",   "normal");
                    if (o.contains("next_id") && !o["next_id"].is_null())
                        opt.next_id = o["next_id"].get<std::string>();
                    if (o.contains("conditions"))
                        for (auto& c : o["conditions"]) opt.conditions.push_back(parseCondNode(c));
                    if (o.contains("events"))
                        for (auto& e : o["events"]) opt.events.push_back(parseEvent(e));
                    node.options.push_back(opt);
                }
            }

            if (n.contains("events"))
                for (auto& e : n["events"]) node.events.push_back(parseEvent(e));

            p.nodes[id] = node;
        }
    } catch (...) { return false; }
    return true;
}

bool BitJsonInterpreter::LoadAssetsFile(const std::string& path, DialogProject& p) {
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
        if (j.contains("fonts")) {
            for (auto& [id, file] : j["fonts"].items()) p.fonts[id] = file.get<std::string>();
        }
    } catch (...) { return false; }
    return true;
}
