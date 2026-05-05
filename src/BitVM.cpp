#include "headers/BitVM.hpp"
#include "headers/BitEngine.hpp"
#include <iostream>

BitVM::BitVM(DialogEngine& engine) : m_engine(engine), m_pc(0), m_isWaiting(false), m_isDelayed(false) {}

void BitVM::BuildLabelIndex() {
    m_labelIndex.clear();
    const auto& bytecode = m_engine.GetProject().bytecode;
    for (size_t i = 0; i < bytecode.size(); ++i) {
        if (bytecode[i].op == BitOp::LABEL && !bytecode[i].args.empty()) {
            m_labelIndex[bytecode[i].args[0]] = (int)i;
        }
    }
}

int BitVM::ResolveLabel(const std::string& label) const {
    auto it = m_labelIndex.find(label);
    if (it != m_labelIndex.end()) return it->second;
    return -1;
}

int BitVM::GetVariable(const std::string& name) const {
    // Check local scope first
    if (!m_localVariables.empty()) {
        auto& scope = m_localVariables.back();
        auto it = scope.find(name);
        if (it != scope.end()) return it->second;
    }
    // Fall back to engine's global variables
    return m_engine.GetVariable(name);
}

void BitVM::SetVariable(const std::string& name, int value) {
    // Set in local scope if one exists
    if (!m_localVariables.empty()) {
        m_localVariables.back()[name] = value;
    } else {
        m_engine.SetVariable(name, value);
    }
}

void BitVM::RunVM() {
    // Stub - actual execution is in BitEngine
}

void BitVM::ExecuteInstruction(const BitInstruction& ins) {
    // Stub - actual execution is in BitEngine
}
