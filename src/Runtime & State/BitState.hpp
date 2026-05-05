#ifndef BIT_STATE_HPP
#define BIT_STATE_HPP

#include "json.hpp"
#include "BitOp.hpp"
#include "BitRichText.hpp"
#include <string>
#include <unordered_map>
#include <map>
#include <vector>

// Forward declarations and core structures for runtime state
struct ActiveEntityState {
    std::string pos = "center";
    std::string expression = "idle";
    float currentNormX = 0.5f;
    float targetNormX = 0.5f;
    float startNormX = 0.5f;
    float moveTimer = 0.0f;
    float moveDuration = 0.0f;

    float alpha = 1.0f;
    float targetAlpha = 1.0f;
    float startAlpha = 1.0f;
    float fadeDuration = 0.0f;
    float fadeTimer = 0.0f;
    
    bool visible = true;
};

struct ActiveTimeline {
    std::string id;
    float timer = 0.0f;
    size_t nextEventIdx = 0;
    bool finished = false;
    bool isBlocking = false;
};

struct HistoryEntry {
    std::string speaker;
    std::string content;
    std::vector<RichChar> richContent;
};

struct EventTraceEntry {
    std::string node_id;
    std::string op;
    std::string var;
    int old_value;
    int new_value;
};

struct UILayoutDef {
    std::string id = "";
    std::string path = "";
    int layer = 0;
    bool active = true;
    bool visible = true;
};

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
    std::unordered_map<std::string, int>& Variables() { return m_variables; }
    const std::unordered_map<std::string, int>& GetVariables() const { return m_variables; }
    
    // Active Entities
    std::map<std::string, ActiveEntityState>& ActiveEntities() { return m_activeEntities; }
    const std::map<std::string, ActiveEntityState>& GetActiveEntities() const { return m_activeEntities; }
    
    // Timelines
    std::vector<ActiveTimeline>& ActiveTimelines() { return m_activeTimelines; }
    const std::vector<ActiveTimeline>& GetActiveTimelines() const { return m_activeTimelines; }
    
    // Background & Audio
    std::string& ActiveBg() { return m_activeBg; }
    const std::string& GetActiveBg() const { return m_activeBg; }
    
    std::string& PrevBg() { return m_prevBg; }
    const std::string& GetPrevBg() const { return m_prevBg; }
    
    float& BgFadeAlpha() { return m_bgFadeAlpha; }
    float BgFadeAlpha() const { return m_bgFadeAlpha; }
    
    float& BgFadeTimer() { return m_bgFadeTimer; }
    float BgFadeTimer() const { return m_bgFadeTimer; }
    
    float& BgFadeDuration() { return m_bgFadeDuration; }
    float BgFadeDuration() const { return m_bgFadeDuration; }
    
    std::string& ActiveBgm() { return m_activeBgm; }
    const std::string& GetActiveBgm() const { return m_activeBgm; }
    
    // Screen Effects
    float& ScreenFadeAlpha() { return m_screenFadeAlpha; }
    float ScreenFadeAlpha() const { return m_screenFadeAlpha; }
    
    float& ScreenFadeTarget() { return m_screenFadeTarget; }
    float ScreenFadeTarget() const { return m_screenFadeTarget; }
    
    float& ScreenFadeStart() { return m_screenFadeStart; }
    float ScreenFadeStart() const { return m_screenFadeStart; }
    
    float& ScreenFadeTimer() { return m_screenFadeTimer; }
    float ScreenFadeTimer() const { return m_screenFadeTimer; }
    
    float& ScreenFadeDuration() { return m_screenFadeDuration; }
    float ScreenFadeDuration() const { return m_screenFadeDuration; }
    
    float& ShakeIntensity() { return m_shakeIntensity; }
    float ShakeIntensity() const { return m_shakeIntensity; }
    
    // UI State
    bool& IsUiHidden() { return m_isUiHidden; }
    bool IsUiHidden() const { return m_isUiHidden; }
    
    std::unordered_map<std::string, UILayoutDef>& UIStates() { return m_uiStates; }
    const std::unordered_map<std::string, UILayoutDef>& UIStates() const { return m_uiStates; }
    
    // History & Trace
    std::vector<HistoryEntry>& History() { return m_history; }
    const std::vector<HistoryEntry>& History() const { return m_history; }
    
    std::vector<EventTraceEntry>& EventTrace() { return m_eventTrace; }
    const std::vector<EventTraceEntry>& EventTrace() const { return m_eventTrace; }

private:
    std::unordered_map<std::string, int> m_variables;
    std::map<std::string, ActiveEntityState> m_activeEntities;
    std::vector<ActiveTimeline> m_activeTimelines;
    
    std::string m_activeBg = "";
    std::string m_prevBg = "";
    float m_bgFadeAlpha = 1.0f;
    float m_bgFadeTimer = 0.0f;
    float m_bgFadeDuration = 0.0f;
    
    std::string m_activeBgm = "";
    float m_screenFadeAlpha = 0.0f;
    float m_screenFadeTarget = 0.0f;
    float m_screenFadeStart = 0.0f;
    float m_screenFadeTimer = 0.0f;
    float m_screenFadeDuration = 0.0f;
    
    float m_shakeIntensity = 0.0f;
    bool m_isUiHidden = false;
    
    std::vector<HistoryEntry> m_history;
    std::vector<EventTraceEntry> m_eventTrace;
    std::unordered_map<std::string, UILayoutDef> m_uiStates;
};

#endif
