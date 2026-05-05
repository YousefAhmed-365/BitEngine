#ifndef BIT_VM_HPP
#define BIT_VM_HPP

#include "BitOp.hpp"
#include <string>
#include <vector>
#include <unordered_map>

// Forward declaration to avoid circular dependency
class BitRuntime;

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
    BitVM(BitRuntime& engine);
    
    // VM Execution
    void RunVM();
    void ExecuteInstruction(const BitInstruction& ins);
    
    // State Queries/Setters
    int GetPC() const { return m_pc; }
    void SetPC(int pc) { m_pc = pc; }
    
    bool IsWaiting() const { return m_isWaiting; }
    bool IsDelayed() const { return m_isDelayed; }
    void ResetWaiting() { m_isWaiting = false; m_isDelayed = false; m_waitingForActionType = ""; }
    
    // Variable Access
    int GetVariable(const std::string& name) const;
    void SetVariable(const std::string& name, int value);
    
    // Call Stack
    const std::vector<int>& GetCallStack() const { return m_callStack; }
    void SetCallStack(const std::vector<int>& stack) { m_callStack = stack; }
    void ClearStack() { m_callStack.clear(); m_localVariables.clear(); m_localVariables.push_back({}); }
    
    const std::vector<std::unordered_map<std::string, int>>& GetLocalScopes() const { return m_localVariables; }
    void SetLocalScopes(const std::vector<std::unordered_map<std::string, int>>& scopes) { m_localVariables = scopes; }
    
    // Label management
    void BuildLabelIndex();
    int ResolveLabel(const std::string& label) const;
    const std::unordered_map<std::string, int>& GetLabelIndex() const { return m_labelIndex; }

    // Event Wait
    bool IsWaitingForEvent(const std::string& evt) const { return m_isWaiting && m_waitingForEventId == evt; }
    std::string GetWaitActionType() const { return m_waitingForActionType; }

private:
    BitRuntime& m_engine;
    
    int m_pc = 0;
    bool m_isWaiting = false;
    bool m_isDelayed = false;
    std::string m_waitingForActionType = "";
    std::string m_waitingForEventId = "";
    
    std::vector<int> m_callStack;
    std::vector<std::unordered_map<std::string, int>> m_localVariables;
    
    std::unordered_map<std::string, int> m_labelIndex;
};

#endif
