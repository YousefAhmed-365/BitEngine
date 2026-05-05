#ifndef BIT_STATE_HPP
#define BIT_STATE_HPP

#include "json.hpp"
#include <string>
#include <unordered_map>
#include <map>
#include <vector>

struct HistoryEntry;
struct ActiveEntityState;
struct ActiveTimeline;

/**
 * BitState: Runtime state container for the narrative engine
 * 
 * Separates game state from execution logic. Holds:
 * - Variables (global & persistent)
 * - Active UI states
 * - Entity animations
 * - History
 * - Save/load data
 */
class BitState {
public:
    // Variables
    int GetVariable(const std::string& name) const;
    void SetVariable(const std::string& name, int value);
    const std::unordered_map<std::string, int>& GetAllVariables() const { return m_variables; }
    
    // History
    const std::vector<HistoryEntry>& GetHistory() const { return m_history; }
    void AddHistoryEntry(const HistoryEntry& entry);
    void ClearHistory() { m_history.clear(); }
    
    // Active States
    const std::map<std::string, ActiveEntityState>& GetActiveEntities() const { return m_activeEntities; }
    std::map<std::string, ActiveEntityState>& GetMutableActiveEntities() { return m_activeEntities; }
    
    const std::vector<ActiveTimeline>& GetActiveTimelines() const { return m_activeTimelines; }
    std::vector<ActiveTimeline>& GetMutableActiveTimelines() { return m_activeTimelines; }
    
    // Background/Music
    const std::string& GetActiveBg() const { return m_activeBg; }
    void SetActiveBg(const std::string& bg) { m_activeBg = bg; }
    
    const std::string& GetActiveBgm() const { return m_activeBgm; }
    void SetActiveBgm(const std::string& bgm) { m_activeBgm = bgm; }
    
    // Effects
    float GetScreenFadeAlpha() const { return m_screenFadeAlpha; }
    void SetScreenFadeAlpha(float alpha) { m_screenFadeAlpha = alpha; }
    
    float GetShakeIntensity() const { return m_shakeIntensity; }
    void SetShakeIntensity(float intensity) { m_shakeIntensity = intensity; }

private:
    std::unordered_map<std::string, int> m_variables;
    std::vector<HistoryEntry> m_history;
    std::map<std::string, ActiveEntityState> m_activeEntities;
    std::vector<ActiveTimeline> m_activeTimelines;
    
    std::string m_activeBg = "";
    std::string m_activeBgm = "";
    float m_screenFadeAlpha = 0.0f;
    float m_shakeIntensity = 0.0f;
};

#endif
