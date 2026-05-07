#include "BitScriptAnalyzer.hpp"
#include <iostream>
#include <unordered_set>
#include <algorithm>

std::vector<AnalysisMessage> BitScriptAnalyzer::Analyze(const BitProject& project) {
    std::vector<AnalysisMessage> messages;
    
    for (const auto& err : project.parseErrors) {
        messages.push_back({AnalysisMessage::Level::ERROR, err, -1});
    }

    CheckConfiguration(project, messages);
    CheckLabels(project, messages);
    CheckEntities(project, messages);
    CheckVariables(project, messages);
    CheckControlFlow(project, messages);
    CheckAssets(project, messages);
    CheckInstructions(project, messages);
    CheckTimelines(project, messages);
    CheckSprites(project, messages);
    CheckEvents(project, messages);
    CheckUIAssets(project, messages);
    CheckUICommands(project, messages);
    
    return messages;
}

void BitScriptAnalyzer::CheckConfiguration(const BitProject& project, std::vector<AnalysisMessage>& messages) {
    // Check if start_node exists
    if (project.configs.start_node.empty()) {
        messages.push_back({AnalysisMessage::Level::ERROR, "Configuration error: start_node is not set", -1});
    } else {
        bool found = false;
        for (const auto& ins : project.bytecode) {
            if (ins.op == BitOp::LABEL && ins.args[0] == project.configs.start_node) {
                found = true;
                break;
            }
        }
        if (!found) {
            messages.push_back({AnalysisMessage::Level::ERROR, "start_node '" + project.configs.start_node + "' does not exist", -1});
        }
    }

    // Check if bytecode is empty
    if (project.bytecode.empty()) {
        messages.push_back({AnalysisMessage::Level::ERROR, "Project has no bytecode instructions", -1});
    }

    // Check reveal_speed
    if (project.configs.reveal_speed <= 0.0f) {
        messages.push_back({AnalysisMessage::Level::WARNING, "reveal_speed is set to " + std::to_string(project.configs.reveal_speed) + ", should be positive", -1});
    }

    // Check max_slots
    if (project.configs.max_slots < 1) {
        messages.push_back({AnalysisMessage::Level::WARNING, "max_slots should be at least 1, got " + std::to_string(project.configs.max_slots), -1});
    }

    // Check for invalid mode
    if (project.configs.mode != "typewriter" && project.configs.mode != "instant") {
        messages.push_back({AnalysisMessage::Level::WARNING, "Unknown reveal mode: " + project.configs.mode + " (expected 'typewriter' or 'instant')", -1});
    }
}

void BitScriptAnalyzer::CheckLabels(const BitProject& project, std::vector<AnalysisMessage>& messages) {
    std::unordered_set<std::string> definedLabels;
    std::unordered_set<std::string> referencedLabels;

    for (const auto& ins : project.bytecode) {
        if (ins.op == BitOp::LABEL) {
            if (!ins.args.empty()) {
                if (definedLabels.count(ins.args[0])) {
                    messages.push_back({AnalysisMessage::Level::ERROR, "Duplicate scene label: " + ins.args[0], ins.line});
                }
                definedLabels.insert(ins.args[0]);
            } else {
                messages.push_back({AnalysisMessage::Level::ERROR, "LABEL instruction with no arguments", ins.line});
            }
        }
        if (ins.op == BitOp::GOTO || ins.op == BitOp::CALL) {
            if (!ins.args.empty()) {
                referencedLabels.insert(ins.args[0]);
            }
        }
        if (ins.op == BitOp::CHOICE) {
            if (ins.args.size() >= 2) {
                referencedLabels.insert(ins.args[1]);
            } else {
                messages.push_back({AnalysisMessage::Level::ERROR, "CHOICE instruction needs at least 2 arguments (text, target)", ins.line});
            }
        }
        if (ins.op == BitOp::IF || ins.op == BitOp::IF_REF) {
            if (ins.args.size() >= 4) {
                referencedLabels.insert(ins.args[3]);
            } else {
                messages.push_back({AnalysisMessage::Level::ERROR, "IF instruction needs 4 arguments (var, op, value, target)", ins.line});
            }
        }
    }

    // Missing labels (referenced but not defined)
    for (const auto& ref : referencedLabels) {
        if (definedLabels.find(ref) == definedLabels.end()) {
            messages.push_back({AnalysisMessage::Level::ERROR, "Reference to non-existent scene: '" + ref + "'", -1});
        }
    }

    // Unused labels
    for (const auto& label : definedLabels) {
        if (label != project.configs.start_node && referencedLabels.find(label) == referencedLabels.end()) {
            messages.push_back({AnalysisMessage::Level::WARNING, "Unused scene label: '" + label + "'", -1});
        }
    }
}

void BitScriptAnalyzer::CheckEntities(const BitProject& project, std::vector<AnalysisMessage>& messages) {
    std::unordered_set<std::string> usedEntities;

    for (const auto& ins : project.bytecode) {
        if (ins.op == BitOp::SAY) {
            if (ins.args.size() < 2) {
                messages.push_back({AnalysisMessage::Level::ERROR, "SAY instruction needs at least 2 arguments (entity, content)", ins.line});
                continue;
            }
            std::string id = ins.args[0];
            if (id != "narration" && id != "system" && project.entities.find(id) == project.entities.end()) {
                messages.push_back({AnalysisMessage::Level::ERROR, "Use of undefined character ID: '" + id + "'", ins.line});
            }
            usedEntities.insert(id);
        }

        if (ins.op == BitOp::EVENT && !ins.args.empty() && ins.args[0] == "join") {
            if (ins.metadata.contains("id")) {
                std::string id = ins.metadata["id"];
                if (project.entities.find(id) == project.entities.end()) {
                    messages.push_back({AnalysisMessage::Level::ERROR, "Join event for undefined character: '" + id + "'", ins.line});
                }
                usedEntities.insert(id);
            }
        }

        if (ins.op == BitOp::EVENT && !ins.args.empty()) {
            std::string op = ins.args[0];
            if ((op == "expression" || op == "hide" || op == "leave" || op == "move" || op == "fade" || op == "pos") && ins.metadata.contains("target")) {
                std::string target = ins.metadata["target"];
                if (target != "bg" && project.entities.find(target) == project.entities.end()) {
                    messages.push_back({AnalysisMessage::Level::ERROR, "EVENT '" + op + "' targets undefined character: '" + target + "'", ins.line});
                }
                if (target != "bg") usedEntities.insert(target);
            }
        }
    }

    // Warn about defined but unused entities
    for (const auto& [id, entity] : project.entities) {
        if (usedEntities.find(id) == usedEntities.end()) {
            messages.push_back({AnalysisMessage::Level::WARNING, "Entity '" + id + "' is defined but never used", -1});
        }
    }
}

void BitScriptAnalyzer::CheckVariables(const BitProject& project, std::vector<AnalysisMessage>& messages) {
    std::unordered_set<std::string> usedVars;
    std::unordered_set<std::string> definedVars;

    for (const auto& [name, def] : project.variables) {
        definedVars.insert(name);
    }

    for (const auto& ins : project.bytecode) {
        // Variable assignments
        if (ins.op == BitOp::SET || ins.op == BitOp::SET_REF) {
            if (!ins.args.empty()) {
                std::string var = ins.args[0];
                usedVars.insert(var);
                if (var.substr(0, 5) != "__tmp" && definedVars.find(var) == definedVars.end()) {
                    messages.push_back({AnalysisMessage::Level::WARNING, "Assignment to undefined variable: '" + var + "'", ins.line});
                }
            }
        }
        if (ins.op == BitOp::ADD || ins.op == BitOp::ADD_REF ||
            ins.op == BitOp::SUB || ins.op == BitOp::SUB_REF ||
            ins.op == BitOp::MUL || ins.op == BitOp::MUL_REF ||
            ins.op == BitOp::DIV || ins.op == BitOp::DIV_REF) {
            if (!ins.args.empty()) {
                std::string var = ins.args[0];
                usedVars.insert(var);
                if (var.substr(0, 5) != "__tmp" && definedVars.find(var) == definedVars.end()) {
                    messages.push_back({AnalysisMessage::Level::WARNING, "Arithmetic on undefined variable: '" + var + "'", ins.line});
                }
                // Check for division by zero with literal values
                if ((ins.op == BitOp::DIV || ins.op == BitOp::DIV_REF) && ins.args.size() >= 2) {
                    if (ins.op == BitOp::DIV) {
                        try {
                            int divisor = std::stoi(ins.args[1]);
                            if (divisor == 0) {
                                messages.push_back({AnalysisMessage::Level::ERROR, "Potential division by zero", ins.line});
                            }
                        } catch (...) {}
                    }
                }
            }
        }

        // Variable reads (in conditions)
        if (ins.op == BitOp::IF || ins.op == BitOp::IF_REF) {
            if (!ins.args.empty()) {
                std::string var = ins.args[0];
                usedVars.insert(var);
                if (var.substr(0, 5) != "__tmp" && definedVars.find(var) == definedVars.end()) {
                    messages.push_back({AnalysisMessage::Level::ERROR, "Conditional check on undefined variable: '" + var + "'", ins.line});
                }
                // Validate operator
                if (ins.args.size() >= 2) {
                    std::string op = ins.args[1];
                    if (op != "==" && op != "=" && op != "!=" && op != ">" && op != "<" && op != ">=" && op != "<=") {
                        messages.push_back({AnalysisMessage::Level::ERROR, "Invalid comparison operator: '" + op + "'", ins.line});
                    }
                }
            }
        }

        if (ins.op == BitOp::SET_LOCAL) {
            if (!ins.args.empty()) {
                std::string var = ins.args[0];
                usedVars.insert(var);
            }
        }
    }

    // Warn about defined but unused variables
    for (const auto& var : definedVars) {
        if (usedVars.find(var) == usedVars.end()) {
            messages.push_back({AnalysisMessage::Level::INFO, "Variable '" + var + "' is defined but never used", -1});
        }
    }
}

void BitScriptAnalyzer::CheckControlFlow(const BitProject& project, std::vector<AnalysisMessage>& messages) {
    bool foundHalt = false;
    int lastLabelLine = -1;

    for (size_t i = 0; i < project.bytecode.size(); ++i) {
        const auto& ins = project.bytecode[i];
        
        if (ins.op == BitOp::LABEL) {
            lastLabelLine = (int)i;
        }

        // Detect obvious infinite loops (GOTO self)
        if (ins.op == BitOp::GOTO && !ins.args.empty()) {
            std::string target = ins.args[0];
            // If GOTO is at line i and jumps to a label at line i, it's infinite
            if (i > 0 && project.bytecode[i-1].op == BitOp::LABEL && !project.bytecode[i-1].args.empty()) {
                if (project.bytecode[i-1].args[0] == target) {
                    messages.push_back({AnalysisMessage::Level::WARNING, "Potential infinite loop: GOTO jumps to preceding label", ins.line});
                }
            }
        }

        if (ins.op == BitOp::HALT) {
            foundHalt = true;
        }
    }

    // Check if bytecode ends properly
    if (!project.bytecode.empty() && project.bytecode.back().op != BitOp::HALT) {
        messages.push_back({AnalysisMessage::Level::WARNING, "Bytecode does not end with HALT instruction", -1});
    }
}

void BitScriptAnalyzer::CheckAssets(const BitProject& project, std::vector<AnalysisMessage>& messages) {
    std::unordered_set<std::string> usedBg, usedMusic, usedSfx, usedFonts;

    for (const auto& ins : project.bytecode) {
        if (ins.op == BitOp::BG) {
            if (!ins.args.empty() && ins.args[0][0] != '@') {
                usedBg.insert(ins.args[0]);
            }
        }
        if (ins.op == BitOp::BGM) {
            if (!ins.args.empty() && ins.args[0][0] != '@') {
                usedMusic.insert(ins.args[0]);
            }
        }
        if (ins.op == BitOp::EVENT && !ins.args.empty() && ins.args[0] == "play_sfx") {
            if (ins.metadata.contains("id") && ins.metadata["id"].get<std::string>()[0] != '@') {
                usedSfx.insert(ins.metadata["id"].get<std::string>());
            }
        }
        if (ins.op == BitOp::EVENT && !ins.args.empty() && ins.args[0] == "fade") {
            if (ins.metadata.contains("id") && ins.metadata["id"].get<std::string>()[0] != '@') {
                usedBg.insert(ins.metadata["id"].get<std::string>());
            }
        }
    }

    // Check referenced assets exist
    auto missingLevel = project.configs.strict_assets ? AnalysisMessage::Level::ERROR : AnalysisMessage::Level::WARNING;
    for (const auto& bg : usedBg) {
        if (project.backgrounds.find(bg) == project.backgrounds.end()) {
            messages.push_back({missingLevel, "Reference to undefined background asset: '" + bg + "'", -1});
        }
    }
    for (const auto& music : usedMusic) {
        if (project.music.find(music) == project.music.end()) {
            messages.push_back({missingLevel, "Reference to undefined music asset: '" + music + "'", -1});
        }
    }
    for (const auto& sfx : usedSfx) {
        if (project.sfx.find(sfx) == project.sfx.end()) {
            messages.push_back({missingLevel, "Reference to undefined SFX asset: '" + sfx + "'", -1});
        }
    }

    // Warn about unused assets
    for (const auto& [id, path] : project.backgrounds) {
        if (usedBg.find(id) == usedBg.end()) {
            messages.push_back({AnalysisMessage::Level::INFO, "Background asset '" + id + "' is defined but never used", -1});
        }
    }
    for (const auto& [id, path] : project.music) {
        if (usedMusic.find(id) == usedMusic.end()) {
            messages.push_back({AnalysisMessage::Level::INFO, "Music asset '" + id + "' is defined but never used", -1});
        }
    }
    for (const auto& [id, path] : project.sfx) {
        if (usedSfx.find(id) == usedSfx.end()) {
            messages.push_back({AnalysisMessage::Level::INFO, "SFX asset '" + id + "' is defined but never used", -1});
        }
    }
}

void BitScriptAnalyzer::CheckInstructions(const BitProject& project, std::vector<AnalysisMessage>& messages) {
    for (const auto& ins : project.bytecode) {
        // Text/SAY with empty content
        if (ins.op == BitOp::TEXT || ins.op == BitOp::SAY) {
            if (ins.args.size() >= 2 && ins.args[1].empty() && ins.metadata.empty()) {
                messages.push_back({AnalysisMessage::Level::WARNING, "SAY/TEXT instruction with empty content", ins.line});
            }
        }

        // CALL instruction validation
        if (ins.op == BitOp::CALL) {
            if (ins.args.empty()) {
                messages.push_back({AnalysisMessage::Level::ERROR, "CALL instruction requires at least a function name", ins.line});
            }
        }

        // RETURN outside of function
        if (ins.op == BitOp::RETURN) {
            // Check if we're inside a function context (heuristic: preceded by CALL or inside a scene with parameters)
            // This is a basic check - ideally would track call stack
        }

        // WAIT_ACTION with invalid type
        if (ins.op == BitOp::WAIT_ACTION) {
            if (!ins.args.empty()) {
                std::string type = ins.args[0];
                if (type != "sfx" && type != "move" && type != "fade" && type != "all" && type != "timeline") {
                    messages.push_back({AnalysisMessage::Level::WARNING, "WAIT_ACTION with potentially invalid type: '" + type + "'", ins.line});
                }
            }
        }

        // EVENT instruction validation
        if (ins.op == BitOp::EVENT) {
            if (ins.args.empty()) {
                messages.push_back({AnalysisMessage::Level::ERROR, "EVENT instruction requires an operation name", ins.line});
                continue;
            }

            std::string op = ins.args[0];
            
            // Validate fade_screen parameters
            if (op == "fade_screen") {
                if (ins.metadata.contains("alpha")) {
                    if (ins.metadata["alpha"].is_string()) {} // Skip validation for variable refs
                    else {
                        try {
                            float alpha = ins.metadata["alpha"].get<float>();
                            if (alpha < 0.0f || alpha > 1.0f) {
                                messages.push_back({AnalysisMessage::Level::ERROR, "fade_screen: alpha must be between 0.0 and 1.0, got " + std::to_string(alpha), ins.line});
                            }
                        } catch (...) {
                            messages.push_back({AnalysisMessage::Level::ERROR, "fade_screen: alpha is not a valid number", ins.line});
                        }
                    }
                }
                if (ins.metadata.contains("duration")) {
                    if (ins.metadata["duration"].is_string()) {} // Skip validation for variable refs
                    else {
                        try {
                            int duration = ins.metadata["duration"].get<int>();
                            if (duration < 0) {
                                messages.push_back({AnalysisMessage::Level::ERROR, op + ": duration must be >= 0, got " + std::to_string(duration), ins.line});
                            }
                        } catch (...) {
                            messages.push_back({AnalysisMessage::Level::ERROR, op + ": duration is not a valid number", ins.line});
                        }
                    }
                }
                if (ins.metadata.contains("pre_delay")) {
                    if (ins.metadata["pre_delay"].is_string()) {}
                    else {
                        try {
                            int pre_delay = ins.metadata["pre_delay"].get<int>();
                            if (pre_delay < 0) {
                                messages.push_back({AnalysisMessage::Level::ERROR, op + ": pre_delay must be >= 0, got " + std::to_string(pre_delay), ins.line});
                            }
                        } catch (...) {
                            messages.push_back({AnalysisMessage::Level::ERROR, op + ": pre_delay is not a valid number", ins.line});
                        }
                    }
                }
            }
            
            // Validate fade parameters
            else if (op == "fade") {
                std::string target = ins.metadata.value("target", "");
                if (target != "bg" && ins.metadata.contains("alpha")) {
                    if (ins.metadata["alpha"].is_string()) {}
                    else {
                        try {
                            float alpha = ins.metadata["alpha"].get<float>();
                            if (alpha < 0.0f || alpha > 1.0f) {
                                messages.push_back({AnalysisMessage::Level::ERROR, "fade: alpha must be between 0.0 and 1.0, got " + std::to_string(alpha), ins.line});
                            }
                        } catch (...) {
                            messages.push_back({AnalysisMessage::Level::ERROR, "fade: alpha is not a valid number", ins.line});
                        }
                    }
                }
                if (ins.metadata.contains("duration")) {
                    if (ins.metadata["duration"].is_string()) {}
                    else {
                        try {
                            int duration = ins.metadata["duration"].get<int>();
                            if (duration < 0) {
                                messages.push_back({AnalysisMessage::Level::ERROR, "fade: duration must be >= 0, got " + std::to_string(duration), ins.line});
                            }
                        } catch (...) {
                            messages.push_back({AnalysisMessage::Level::ERROR, "fade: duration is not a valid number", ins.line});
                        }
                    }
                }
            }
            
            // Validate move parameters
            else if (op == "move") {
                if (ins.metadata.contains("x")) {
                    if (ins.metadata["x"].is_string()) {}
                    else {
                        try {
                            float x = ins.metadata["x"].get<float>();
                            if (x < 0.0f || x > 1.0f) {
                                messages.push_back({AnalysisMessage::Level::ERROR, "move: x position must be between 0.0 and 1.0, got " + std::to_string(x), ins.line});
                            }
                        } catch (...) {
                            messages.push_back({AnalysisMessage::Level::ERROR, "move: x is not a valid number", ins.line});
                        }
                    }
                }
                if (ins.metadata.contains("duration")) {
                    if (ins.metadata["duration"].is_string()) {}
                    else {
                        try {
                            int duration = ins.metadata["duration"].get<int>();
                            if (duration < 0) {
                                messages.push_back({AnalysisMessage::Level::ERROR, "move: duration must be >= 0, got " + std::to_string(duration), ins.line});
                            }
                        } catch (...) {
                            messages.push_back({AnalysisMessage::Level::ERROR, "move: duration is not a valid number", ins.line});
                        }
                    }
                }
            }
            
            // Validate shake parameters
            else if (op == "shake") {
                if (ins.metadata.contains("intensity")) {
                    if (ins.metadata["intensity"].is_string()) {}
                    else {
                        try {
                            float intensity = ins.metadata["intensity"].get<float>();
                            if (intensity < 0.0f) {
                                messages.push_back({AnalysisMessage::Level::WARNING, "shake: intensity should be positive, got " + std::to_string(intensity), ins.line});
                            }
                        } catch (...) {
                            messages.push_back({AnalysisMessage::Level::ERROR, "shake: intensity is not a valid number", ins.line});
                        }
                    }
                }
            }
            
            // Validate delay parameters
            else if (op == "delay") {
                if (ins.metadata.contains("duration")) {
                    if (ins.metadata["duration"].is_string()) {}
                    else {
                        try {
                            if (ins.metadata["duration"].is_number()) {
                                int duration = ins.metadata["duration"].get<int>();
                                if (duration < 0) {
                                    messages.push_back({AnalysisMessage::Level::ERROR, "delay: duration must be >= 0, got " + std::to_string(duration), ins.line});
                                }
                            }
                        } catch (...) {
                            messages.push_back({AnalysisMessage::Level::ERROR, "delay: duration is not a valid number", ins.line});
                        }
                    }
                }
            }
        }

        // Play timeline
        if (ins.op == BitOp::PLAY_TIMELINE) {
            if (ins.args.empty()) {
                messages.push_back({AnalysisMessage::Level::ERROR, "PLAY_TIMELINE requires timeline ID", ins.line});
            } else {
                std::string timelineId = ins.args[0];
                if (!timelineId.empty() && timelineId[0] != '@' && project.timelines.find(timelineId) == project.timelines.end()) {
                    messages.push_back({AnalysisMessage::Level::ERROR, "Reference to undefined timeline: '" + timelineId + "'", ins.line});
                }
            }
        }

        // EMIT — verify target event is declared
        if (ins.op == BitOp::EMIT) {
            if (ins.args.empty()) {
                messages.push_back({AnalysisMessage::Level::ERROR, "EMIT requires an event name", ins.line});
            } else if (project.events.find(ins.args[0]) == project.events.end()) {
                messages.push_back({AnalysisMessage::Level::WARNING,
                    "EMIT references undeclared event block: '" + ins.args[0] + "'", ins.line});
            }
        }

        // WAIT_EVENT — verify event name is non-empty
        if (ins.op == BitOp::WAIT_EVENT) {
            if (ins.args.empty()) {
                messages.push_back({AnalysisMessage::Level::ERROR, "WAIT_EVENT requires an event name", ins.line});
            }
        }

        // UI_LOAD — path and name must be present
        if (ins.op == BitOp::UI_LOAD) {
            if (ins.args.size() < 2) {
                messages.push_back({AnalysisMessage::Level::ERROR, "UI_LOAD requires at least (name, path)", ins.line});
            }
        }

        // UI_UNLOAD — name must be present
        if (ins.op == BitOp::UI_UNLOAD) {
            if (ins.args.empty()) {
                messages.push_back({AnalysisMessage::Level::ERROR, "UI_UNLOAD requires a layout name", ins.line});
            }
        }

        // UI_SET — scoped_id + property + value
        if (ins.op == BitOp::UI_SET) {
            if (ins.args.size() < 3) {
                messages.push_back({AnalysisMessage::Level::ERROR, "UI_SET requires (id, property, value)", ins.line});
            }
        }
    }
}

void BitScriptAnalyzer::CheckTimelines(const BitProject& project, std::vector<AnalysisMessage>& messages) {
    std::unordered_set<std::string> usedTimelines;

    // Collect used timelines
    for (const auto& ins : project.bytecode) {
        if (ins.op == BitOp::PLAY_TIMELINE && !ins.args.empty()) {
            usedTimelines.insert(ins.args[0]);
        }
    }

    // Check for unused timelines
    for (const auto& [id, tl] : project.timelines) {
        if (usedTimelines.find(id) == usedTimelines.end()) {
            messages.push_back({AnalysisMessage::Level::INFO, "Timeline '" + id + "' is defined but never used", -1});
        }

        // Check timeline events
        if (tl.events.empty()) {
            messages.push_back({AnalysisMessage::Level::WARNING, "Timeline '" + id + "' has no events", -1});
        }

        // Verify timeline event operations are valid
        for (const auto& event : tl.events) {
            if (event.time_ms < 0) {
                messages.push_back({AnalysisMessage::Level::ERROR, "Timeline '" + id + "' has event with negative time: " + std::to_string(event.time_ms), -1});
            }
            
            // Validate parameters for timeline events
            if (event.op == BitOp::EVENT && !event.args.empty()) {
                std::string op = event.args[0];
                
                // Validate fade_screen parameters
                if (op == "fade_screen") {
                    if (event.metadata.contains("alpha")) {
                        if (event.metadata["alpha"].is_string()) {}
                        else {
                            try {
                                float alpha = event.metadata["alpha"].get<float>();
                                if (alpha < 0.0f || alpha > 1.0f) {
                                    messages.push_back({AnalysisMessage::Level::ERROR, "Timeline '" + id + "': fade_screen alpha must be between 0.0 and 1.0, got " + std::to_string(alpha), -1});
                                }
                            } catch (...) {
                                messages.push_back({AnalysisMessage::Level::ERROR, "Timeline '" + id + "': fade_screen alpha is not a valid number", -1});
                            }
                        }
                    }
                    if (event.metadata.contains("duration")) {
                        if (event.metadata["duration"].is_string()) {}
                        else {
                            try {
                                int duration = event.metadata["duration"].get<int>();
                                if (duration < 0) {
                                    messages.push_back({AnalysisMessage::Level::ERROR, "Timeline '" + id + "': fade_screen duration must be >= 0, got " + std::to_string(duration), -1});
                                }
                            } catch (...) {
                                messages.push_back({AnalysisMessage::Level::ERROR, "Timeline '" + id + "': fade_screen duration is not a valid number", -1});
                            }
                        }
                    }
                    if (event.metadata.contains("pre_delay")) {
                        try {
                            int pre_delay = event.metadata["pre_delay"].is_number() ? event.metadata["pre_delay"].get<int>() : std::stoi(event.metadata["pre_delay"].get<std::string>());
                            if (pre_delay < 0) {
                                messages.push_back({AnalysisMessage::Level::ERROR, "Timeline '" + id + "': fade_screen pre_delay must be >= 0, got " + std::to_string(pre_delay), -1});
                            }
                        } catch (...) {
                            messages.push_back({AnalysisMessage::Level::ERROR, "Timeline '" + id + "': fade_screen pre_delay is not a valid number", -1});
                        }
                    }
                }
                
                // Validate fade parameters
                else if (op == "fade") {
                    std::string target = event.metadata.value("target", "");
                    if (target != "bg" && event.metadata.contains("alpha")) {
                        if (event.metadata["alpha"].is_string()) {}
                        else {
                            try {
                                float alpha = event.metadata["alpha"].get<float>();
                                if (alpha < 0.0f || alpha > 1.0f) {
                                    messages.push_back({AnalysisMessage::Level::ERROR, "Timeline '" + id + "': fade alpha must be between 0.0 and 1.0, got " + std::to_string(alpha), -1});
                                }
                            } catch (...) {
                                messages.push_back({AnalysisMessage::Level::ERROR, "Timeline '" + id + "': fade alpha is not a valid number", -1});
                            }
                        }
                    }
                    if (event.metadata.contains("duration")) {
                        if (event.metadata["duration"].is_string()) {}
                        else {
                            try {
                                int duration = event.metadata["duration"].get<int>();
                                if (duration < 0) {
                                    messages.push_back({AnalysisMessage::Level::ERROR, "Timeline '" + id + "': fade duration must be >= 0, got " + std::to_string(duration), -1});
                                }
                            } catch (...) {
                                messages.push_back({AnalysisMessage::Level::ERROR, "Timeline '" + id + "': fade duration is not a valid number", -1});
                            }
                        }
                    }
                }
                
                // Validate move parameters
                else if (op == "move") {
                    if (event.metadata.contains("x")) {
                        if (event.metadata["x"].is_string()) {}
                        else {
                            try {
                                float x = event.metadata["x"].get<float>();
                                if (x < 0.0f || x > 1.0f) {
                                    messages.push_back({AnalysisMessage::Level::ERROR, "Timeline '" + id + "': move x position must be between 0.0 and 1.0, got " + std::to_string(x), -1});
                                }
                            } catch (...) {
                                messages.push_back({AnalysisMessage::Level::ERROR, "Timeline '" + id + "': move x is not a valid number", -1});
                            }
                        }
                    }
                    if (event.metadata.contains("duration")) {
                        if (event.metadata["duration"].is_string()) {}
                        else {
                            try {
                                int duration = event.metadata["duration"].get<int>();
                                if (duration < 0) {
                                    messages.push_back({AnalysisMessage::Level::ERROR, "Timeline '" + id + "': move duration must be >= 0, got " + std::to_string(duration), -1});
                                }
                            } catch (...) {
                                messages.push_back({AnalysisMessage::Level::ERROR, "Timeline '" + id + "': move duration is not a valid number", -1});
                            }
                        }
                    }
                }
                
                // Validate shake parameters
                else if (op == "shake") {
                    if (event.metadata.contains("intensity")) {
                        if (event.metadata["intensity"].is_string()) {}
                        else {
                            try {
                                float intensity = event.metadata["intensity"].get<float>();
                                if (intensity < 0.0f) {
                                    messages.push_back({AnalysisMessage::Level::WARNING, "Timeline '" + id + "': shake intensity should be positive, got " + std::to_string(intensity), -1});
                                }
                            } catch (...) {
                                messages.push_back({AnalysisMessage::Level::ERROR, "Timeline '" + id + "': shake intensity is not a valid number", -1});
                            }
                        }
                    }
                }
                
                // Validate delay parameters
                else if (op == "delay") {
                    if (event.metadata.contains("duration")) {
                        if (event.metadata["duration"].is_string()) {}
                        else {
                            try {
                                int duration = event.metadata["duration"].get<int>();
                                if (duration < 0) {
                                    messages.push_back({AnalysisMessage::Level::ERROR, "Timeline '" + id + "': delay duration must be >= 0, got " + std::to_string(duration), -1});
                                }
                            } catch (...) {
                                messages.push_back({AnalysisMessage::Level::ERROR, "Timeline '" + id + "': delay duration is not a valid number", -1});
                            }
                        }
                    }
                }
            }
        }
    }
}

void BitScriptAnalyzer::CheckSprites(const BitProject& project, std::vector<AnalysisMessage>& messages) {
    for (const auto& [entityId, entity] : project.entities) {
        // Check if default position is valid
        if (entity.default_pos_x < 0.0f || entity.default_pos_x > 1.0f) {
            messages.push_back({AnalysisMessage::Level::WARNING, "Entity '" + entityId + "' has invalid default position: " + std::to_string(entity.default_pos_x), -1});
        }

        // Check sprite definitions
        for (const auto& [spriteName, spriteDef] : entity.sprites) {
            if (spriteDef.path.empty()) {
                messages.push_back({AnalysisMessage::Level::ERROR, "Entity '" + entityId + "' sprite '" + spriteName + "' has no path", -1});
            }
            if (spriteDef.frames <= 0) {
                messages.push_back({AnalysisMessage::Level::ERROR, "Entity '" + entityId + "' sprite '" + spriteName + "' has invalid frame count: " + std::to_string(spriteDef.frames), -1});
            }
            if (spriteDef.speed < 0.0f) {
                messages.push_back({AnalysisMessage::Level::WARNING, "Entity '" + entityId + "' sprite '" + spriteName + "' has negative speed", -1});
            }
            if (spriteDef.scale <= 0.0f) {
                messages.push_back({AnalysisMessage::Level::WARNING, "Entity '" + entityId + "' sprite '" + spriteName + "' has invalid scale: " + std::to_string(spriteDef.scale), -1});
            }
        }

        // Warn if entity has no sprites
        if (entity.sprites.empty()) {
            messages.push_back({AnalysisMessage::Level::WARNING, "Entity '" + entityId + "' has no sprite definitions", -1});
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// CheckEvents — validate event blocks declared with `event name { }`
// ─────────────────────────────────────────────────────────────────────────────
void BitScriptAnalyzer::CheckEvents(const BitProject& project, std::vector<AnalysisMessage>& messages) {
    std::unordered_set<std::string> emittedEvents;
    std::unordered_set<std::string> waitedEvents;

    auto scanCode = [&](const std::vector<BitInstruction>& code) {
        for (const auto& ins : code) {
            if (ins.op == BitOp::EMIT && !ins.args.empty())      emittedEvents.insert(ins.args[0]);
            if (ins.op == BitOp::WAIT_EVENT && !ins.args.empty()) waitedEvents.insert(ins.args[0]);
        }
    };

    scanCode(project.bytecode);
    for (const auto& [tid, tl] : project.timelines)
        for (const auto& ev : tl.events)
            if (ev.op == BitOp::EMIT && !ev.args.empty())
                emittedEvents.insert(ev.args[0]);

    for (const auto& [evtId, code] : project.events) {
        if (emittedEvents.find(evtId) == emittedEvents.end())
            messages.push_back({AnalysisMessage::Level::INFO,
                "Event block '" + evtId + "' is declared but never emitted", -1});
        if (code.empty())
            messages.push_back({AnalysisMessage::Level::WARNING,
                "Event block '" + evtId + "' is empty", -1});
    }

    for (const auto& ev : waitedEvents)
        if (project.events.find(ev) == project.events.end())
            messages.push_back({AnalysisMessage::Level::WARNING,
                "WAIT_EVENT waits for event '" + ev + "' which has no event block", -1});
}

// ─────────────────────────────────────────────────────────────────────────────
// CheckUICommands — validate ui_load / ui_set / ui_unload sequences
// ─────────────────────────────────────────────────────────────────────────────
void BitScriptAnalyzer::CheckUIAssets(const BitProject& project, std::vector<AnalysisMessage>& messages) {
    for (const auto& [id, def] : project.uiLayouts) {
        if (def.path.empty()) {
            messages.push_back({AnalysisMessage::Level::ERROR, "UI Asset '" + id + "': path is empty", -1});
        }
        if (def.layer < 0) {
            messages.push_back({AnalysisMessage::Level::WARNING, "UI Asset '" + id + "': layer should be non-negative", -1});
        }
    }
}

void BitScriptAnalyzer::CheckUICommands(const BitProject& project, std::vector<AnalysisMessage>& messages) {
    std::unordered_set<std::string> knownUIs;
    for (const auto& [id, def] : project.uiLayouts) knownUIs.insert(id);

    for (const auto& ins : project.bytecode) {
        if (ins.op == BitOp::UI_LOAD && ins.args.size() >= 1)
            knownUIs.insert(ins.args[0]);

        if (ins.op == BitOp::UI_UNLOAD && !ins.args.empty()) {
            if (knownUIs.find(ins.args[0]) == knownUIs.end()) {
                messages.push_back({AnalysisMessage::Level::WARNING,
                    "UI_UNLOAD for layout '" + ins.args[0] + "' which was not registered in assets or loaded via UI_LOAD", ins.line});
            }
        }

        if (ins.op == BitOp::UI_ACTIVATE && !ins.args.empty()) {
            if (knownUIs.find(ins.args[0]) == knownUIs.end()) {
                messages.push_back({AnalysisMessage::Level::ERROR,
                    "UI_ACTIVATE for unregistered UI asset: '" + ins.args[0] + "'", ins.line});
            }
        }

        if (ins.op == BitOp::UI_DEACTIVATE && !ins.args.empty()) {
            if (knownUIs.find(ins.args[0]) == knownUIs.end()) {
                messages.push_back({AnalysisMessage::Level::WARNING,
                    "UI_DEACTIVATE for unregistered UI asset: '" + ins.args[0] + "'", ins.line});
            }
        }

        if (ins.op == BitOp::UI_SET && ins.args.size() >= 1) {
            std::string scopedId = ins.args[0];
            auto dot = scopedId.find('.');
            if (dot != std::string::npos) {
                std::string uiName = scopedId.substr(0, dot);
                if (knownUIs.find(uiName) == knownUIs.end()) {
                    messages.push_back({AnalysisMessage::Level::WARNING,
                        "UI_SET targets UI '" + uiName + "' which was not registered or loaded", ins.line});
                }
            }
        }
    }
}
