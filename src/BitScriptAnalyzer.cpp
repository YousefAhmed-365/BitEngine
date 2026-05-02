#include "headers/BitScriptAnalyzer.hpp"
#include <iostream>
#include <unordered_set>

std::vector<AnalysisMessage> BitScriptAnalyzer::Analyze(const DialogProject& project) {
    std::vector<AnalysisMessage> messages;
    
    CheckLabels(project, messages);
    CheckEntities(project, messages);
    CheckVariables(project, messages);
    CheckControlFlow(project, messages);
    
    return messages;
}

void BitScriptAnalyzer::CheckLabels(const DialogProject& project, std::vector<AnalysisMessage>& messages) {
    std::unordered_set<std::string> definedLabels;
    std::unordered_set<std::string> referencedLabels;

    for (const auto& ins : project.bytecode) {
        if (ins.op == BitOp::LABEL) {
            if (definedLabels.count(ins.args[0])) {
                messages.push_back({AnalysisMessage::Level::ERROR, "Duplicate scene label: " + ins.args[0], ins.line});
            }
            definedLabels.insert(ins.args[0]);
        }
        if (ins.op == BitOp::GOTO || ins.op == BitOp::CALL) {
            referencedLabels.insert(ins.args[0]);
        }
        if (ins.op == BitOp::CHOICE) {
            referencedLabels.insert(ins.args[1]);
        }
        if (ins.op == BitOp::IF || ins.op == BitOp::IF_REF) {
            referencedLabels.insert(ins.args[3]);
        }
    }

    // Missing labels
    for (const auto& ref : referencedLabels) {
        if (definedLabels.find(ref) == definedLabels.end()) {
            messages.push_back({AnalysisMessage::Level::ERROR, "Reference to non-existent scene: " + ref, -1});
        }
    }

    // Unused labels
    for (const auto& label : definedLabels) {
        if (label != project.configs.start_node && referencedLabels.find(label) == referencedLabels.end()) {
            messages.push_back({AnalysisMessage::Level::WARNING, "Unused scene label: " + label, -1});
        }
    }
}

void BitScriptAnalyzer::CheckEntities(const DialogProject& project, std::vector<AnalysisMessage>& messages) {
    for (const auto& ins : project.bytecode) {
        if (ins.op == BitOp::SAY) {
            std::string id = ins.args[0];
            if (id != "narration" && id != "system" && project.entities.find(id) == project.entities.end()) {
                messages.push_back({AnalysisMessage::Level::ERROR, "Use of undefined character ID: " + id, ins.line});
            }
        }
        if (ins.op == BitOp::EVENT && ins.args[0] == "join") {
            std::string id = ins.metadata["id"];
            if (project.entities.find(id) == project.entities.end()) {
                messages.push_back({AnalysisMessage::Level::ERROR, "Join event for undefined character: " + id, ins.line});
            }
        }
    }
}

void BitScriptAnalyzer::CheckVariables(const DialogProject& project, std::vector<AnalysisMessage>& messages) {
    for (const auto& ins : project.bytecode) {
        if (ins.op == BitOp::SET || ins.op == BitOp::ADD || ins.op == BitOp::SUB || ins.op == BitOp::MUL || ins.op == BitOp::DIV) {
            std::string var = ins.args[0];
            if (project.variables.find(var) == project.variables.end()) {
                messages.push_back({AnalysisMessage::Level::WARNING, "Assignment to undefined variable: " + var, ins.line});
            }
        }
        if (ins.op == BitOp::IF || ins.op == BitOp::IF_REF) {
            std::string var = ins.args[0];
            if (project.variables.find(var) == project.variables.end()) {
                messages.push_back({AnalysisMessage::Level::ERROR, "Conditional check on undefined variable: " + var, ins.line});
            }
        }
    }
}

void BitScriptAnalyzer::CheckControlFlow(const DialogProject& project, std::vector<AnalysisMessage>& messages) {
    (void)messages;
    bool isReachable = true;
    for (size_t i = 0; i < project.bytecode.size(); ++i) {
        const auto& ins = project.bytecode[i];
        
        if (ins.op == BitOp::LABEL) {
            isReachable = true; // New scene resets reachability (simple assumption)
            continue;
        }

        if (!isReachable) {
            // Only warn about significant instructions, skip things like LABELs (already handled)
            if (ins.op != BitOp::LABEL) {
                // messages.push_back({AnalysisMessage::Level::WARNING, "Unreachable code detected", ins.line});
                // Disabled for now as it's too aggressive without better flow analysis
            }
        }

        if (ins.op == BitOp::GOTO || ins.op == BitOp::HALT || ins.op == BitOp::RETURN) {
            isReachable = false;
        }
    }
}
