#ifndef BIT_VM_HPP
#define BIT_VM_HPP

#include "BitOp.hpp"
#include <string>
#include <vector>
#include <unordered_map>

// Forward declaration to avoid circular dependency
class DialogEngine;

/**
 * BitVM: Virtual Machine for executing BitEngine bytecode
 * 
 * Handles:
 * - Instruction execution
 * - Variable management (global and local)
 * - Call stack for subroutines
 * - Program counter (PC) management
 * - State machine (waiting, blocking, etc)
 */
class BitVM {
public:
    BitVM(DialogEngine& engine);
    
    // VM Execution
    void RunVM();
    void ExecuteInstruction(const BitInstruction& ins);
    
    // State Queries
    int GetPC() const { return m_pc; }
    void SetPC(int pc) { m_pc = pc; }
    bool IsWaiting() const { return m_isWaiting; }
    bool IsDelayed() const { return m_isDelayed; }
    
    // Variable Access
    int GetVariable(const std::string& name) const;
    void SetVariable(const std::string& name, int value);
    
    // Call Stack
    const std::vector<int>& GetCallStack() const { return m_callStack; }
    const std::vector<std::unordered_map<std::string, int>>& GetLocalScopes() const { return m_localVariables; }
    
    // Label management
    void BuildLabelIndex();
    int ResolveLabel(const std::string& label) const;

private:
    DialogEngine& m_engine;
    
    int m_pc = 0;
    bool m_isWaiting = false;
    bool m_isDelayed = false;
    std::string m_waitingForActionType = "";
    
    std::vector<int> m_callStack;
    std::vector<std::unordered_map<std::string, int>> m_localVariables;
    
    std::unordered_map<std::string, int> m_labelIndex;  // For O(1) label lookups
};

#endif
