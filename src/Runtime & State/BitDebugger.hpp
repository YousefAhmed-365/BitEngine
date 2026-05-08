#ifndef BITDEBUGGER_HPP
#define BITDEBUGGER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include "raylib.h"
#include "BitOp.hpp"

struct LogEntry {
    std::string message;
    std::string level;
    std::string timestamp;
};

// Forward declarations
class BitRuntime;
class UIManager;

struct DebugDrawContext {
    BitRuntime* engine;
    UIManager* uiManager;
    const std::unordered_map<std::string, Texture2D>* textureCache;
    const std::unordered_map<std::string, Sound>* sfxCache;
    const std::unordered_map<std::string, Music>* musicCache;
    float* fpsHistory;
    int fpsHistoryIdx;
    int* debugTab;
    std::function<Font(const std::string&)> getFont;
};

class BitDebugger {
public:
    BitDebugger();
    void Log(BitRuntime* engine, const std::string& msg, const std::string& level = "INFO");
    void ClearLogs() { m_consoleLogs.clear(); }
    const std::vector<LogEntry>& GetLogs() const { return m_consoleLogs; }
    
    void DrawOverlay(const DebugDrawContext& ctx);
    std::string GetTimestamp() const;
    
    int& GetDebugTab() { return m_debugTab; }
    float* GetFpsHistory() { return m_fpsHistory; }
    int& GetFpsHistoryIdx() { return m_fpsHistoryIdx; }

private:
    std::vector<LogEntry> m_consoleLogs;
    int m_debugTab = 0;
    float m_fpsHistory[100] = {0};
    int m_fpsHistoryIdx = 0;
    
};

#endif
