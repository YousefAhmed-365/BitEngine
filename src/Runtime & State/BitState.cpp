#include "BitState.hpp"

int BitState::GetVariable(const std::string& name) const {
    auto it = m_variables.find(name);
    return (it != m_variables.end()) ? it->second : 0;
}

void BitState::SetVariable(const std::string& name, int value) {
    m_variables[name] = value;
}

std::string BitState::GetStringVariable(const std::string& name) const {
    auto it = m_stringVariables.find(name);
    return (it != m_stringVariables.end()) ? it->second : "";
}

void BitState::SetStringVariable(const std::string& name, const std::string& value) {
    m_stringVariables[name] = value;
}

bool BitState::HasStringVariable(const std::string& name) const {
    return m_stringVariables.count(name) > 0;
}
