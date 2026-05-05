#include "headers/BitState.hpp"
#include "headers/BitEngine.hpp"

int BitState::GetVariable(const std::string& name) const {
    auto it = m_variables.find(name);
    return (it != m_variables.end()) ? it->second : 0;
}

void BitState::SetVariable(const std::string& name, int value) {
    m_variables[name] = value;
}

void BitState::AddHistoryEntry(const HistoryEntry& entry) {
    m_history.push_back(entry);
}
