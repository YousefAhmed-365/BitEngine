#ifndef BITENGINE_HPP
#define BITENGINE_HPP

#include "json.hpp"
#include <string>
#include <unordered_map>
#include <map>
#include <optional>
#include <cstring>

// Hardware-agnostic color representation
#define BITENGINE_KEY "BITENGINE_SECRET_KEY_2026"

struct BitColor {
    unsigned char r, g, b, a;
    static BitColor Blank() { return {0, 0, 0, 0}; }
};

enum class BitOp {
    TEXT,       // entity, content (non-blocking)
    SAY,        // entity, content (blocking)
    CHOICE,     // text, target_label
    IF,         // var, op, value, target_label
    IF_REF,     // var, op, ref_var, target_label
    GOTO,       // target_label
    SET,        // var, val
    SET_REF,    // var, ref_var
    ADD,        // var, val
    ADD_REF,    // var, ref_var
    SUB,        // ...
    SUB_REF,
    MUL,
    MUL_REF,
    DIV,
    DIV_REF,
    EVENT,      // op, params_json
    BG,         // id
    BGM,        // id
    LABEL,      // marker (no-op)
    TRANSITION,
    UI_VISIBLE,
    CALL,
    RETURN,
    WAIT_INPUT,
    WAIT_ACTION,
    SET_LOCAL,
    HALT
};

struct BitInstruction {
    BitOp op;
    std::vector<std::string> args;
    nlohmann::json metadata;
    int line = -1;
};

struct RichChar {
    char ch[5]; // UTF-8 character (max 4 bytes + null)
    BitColor color = BitColor::Blank(); 
    float waitBefore = 0.0f;
    float speedMod = 1.0f;
    bool shake = false;
    bool wave = false;
    std::string font = ""; // New: font override per character

    RichChar() { std::memset(ch, 0, 5); }
};

class RichTextParser {
public:
    static std::vector<RichChar> Parse(const std::string& rawText);
    static BitColor StringToColor(const std::string& str);
};

// Condition system: recursive tree
// A ConditionNode is either a leaf {var, op, value} or a group {"and":[...]} / {"or":[...]}.
// The top-level conditions list is implicitly AND-ed.
struct ConditionLeaf { std::string var, op; int value = 0; std::string ref = ""; };
struct ConditionNode {
    bool        isGroup    = false;
    std::string groupLogic = "and";     // "and" | "or"
    std::vector<ConditionNode> children; // populated when isGroup=true
    ConditionLeaf leaf;                  // populated when isGroup=false
};

// Event: op + per-op typed params as JSON
// INSTANT ops: set, add, sub, mul, random, shake, play_sfx, expression, hide, pos, clear
// SYNC    ops: jump, delay  (block narrative until resolved)
// ASYNC   ops: move, fade, fade_screen  (run in background)
struct Event {
    std::string         op;
    nlohmann::json      params;  // fields depend on op — see ProcessEvents
};

struct DialogOption {
    std::string content, next_id, style;
    std::vector<ConditionNode> conditions;
    std::vector<Event>         events;
};

struct SpriteDef {
    std::string path;
    int frames = 1;
    float speed = 5.0f; 
    float scale = 1.0f;
};

struct Entity { 
    std::string id = "", name = "Unknown", type = "char"; 
    float default_pos_x = 0.5f;
    std::unordered_map<std::string, SpriteDef> sprites = {}; 
    std::unordered_map<std::string, nlohmann::json> aliases = {};
};

struct VariableDef { std::string id = ""; int initial_value = 0; std::optional<int> min = std::nullopt, max = std::nullopt; };

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
    float fadeTimer = 0.0f;
    float fadeDuration = 0.0f;
};

struct HistoryEntry {
    std::string speaker;
    std::string content;
    std::vector<RichChar> richContent;
};

// Legacy JSON node structures removed. 
// Narratives now run exclusively via BitScript bytecode.


struct SaveMetadata {
    std::string timestamp;
    std::string node_id;
    std::string entity_name;
    std::string summary;
};

struct SaveData {
    int version = 1;
    int current_pc = 0;
    std::unordered_map<std::string, int> variables;
    std::string active_bg;
    std::string active_bgm;
    std::unordered_map<std::string, ActiveEntityState> active_entities;
    SaveMetadata meta;
};

struct DialogConfigs {
    std::string start_node = "scene_start", save_prefix = "save_slot_", mode = "typewriter";
    std::string debug_mode = "none";
    float reveal_speed = 45.0f;
    float auto_play_delay = 2.0f;
    bool auto_save = false, encrypt_save = false; 
    bool enable_floating = true, enable_shadows = true, enable_vignette = true;
    int max_slots = 5;
};

struct DialogProject {
    DialogConfigs configs;
    std::unordered_map<std::string, Entity> entities;
    std::unordered_map<std::string, VariableDef> variables;
    
    // Asset Registries
    std::unordered_map<std::string, std::string> backgrounds;
    std::unordered_map<std::string, std::string> music;
    std::unordered_map<std::string, std::string> sfx;
    std::unordered_map<std::string, std::string> fonts;
    
    // Bytecode
    std::vector<BitInstruction> bytecode;
};

struct ValidationResult {
    std::vector<std::string> errors;
};

// Event Trace Entry
struct EventTraceEntry {
    std::string node_id;
    std::string op;
    std::string var;
    int old_value;
    int new_value;
};

class DialogEngine {
public:
    DialogEngine();
    bool LoadProject(const std::string& path);
    bool LoadBytecodeFile(const std::string& path);   // Load .bitc VM bytecode
    void CompileProject(const std::string& outputPath);
    bool SaveBytecode(const std::string& path) const; // Export .bitc VM bytecode
    
    void SaveGame(int slot = 0); 
    bool LoadGame(int slot = 0);
    bool HasSave(int slot) const;
    std::optional<SaveMetadata> GetSaveMetadata(int slot) const;
    
    void StartDialog(const std::string& startId = ""); 
    void SelectOption(int index);
    void Next(); 
    
    void Update(float deltaTime);
    void SkipReveal(); 
    bool IsTextRevealing() const;
    const std::vector<RichChar>& GetParsedContent() const { return m_cachedParsedContent; }
    int GetRevealedCount() const { return (int)m_revealedCount; }
    std::string GetVisibleContent() const; // Legacy plain string access if needed
    
    int GetVariable(const std::string& name) const;
    int SafeStoi(const std::string& s) const;
    void SetVariable(const std::string& name, int value);
    const std::unordered_map<std::string, int>& GetAllVariables() const { return m_variables; }
    
    bool IsActive() const { return m_isActive; }
    int GetCurrentPC() const { return m_pc; }

    const Entity* GetCurrentEntity() const;
    const Entity* GetEntity(const std::string& id) const { return m_project.entities.count(id) ? &m_project.entities.at(id) : nullptr; }
    const std::vector<DialogOption>& GetVisibleOptions() const { return m_visibleOptions; }
    const DialogConfigs& GetConfigs() const { return m_project.configs; }
    DialogProject& GetProject() { return m_project; }
    std::string GetDebugMode() const { return m_project.configs.debug_mode; }
    void Log(const std::string& msg, const std::string& level = "INFO") const;
    bool IsDebugOverlayVisible() const { return m_debugOverlayVisible; }
    void ToggleDebugOverlay() { m_debugOverlayVisible = !m_debugOverlayVisible; }
    void SetDebugOverlayVisible(bool visible) { m_debugOverlayVisible = visible; }

    // Global Asset Retrieval
    std::string GetBackground(const std::string& id) const { return m_project.backgrounds.count(id) ? m_project.backgrounds.at(id) : ""; }
    std::string GetMusic(const std::string& id) const { return m_project.music.count(id) ? m_project.music.at(id) : ""; }
    std::string GetSFX(const std::string& id) const { return m_project.sfx.count(id) ? m_project.sfx.at(id) : ""; }

    // Active Persistent States (Retained across nodes and saves)
    const std::string& GetActiveBg() const { return m_activeBg; }
    const std::string& GetPrevBg() const { return m_prevBg; }
    float GetBgFadeAlpha() const { return m_bgFadeAlpha; }
    
    // Screen fade
    float GetScreenFadeAlpha() const { return m_screenFadeAlpha; }
    BitColor GetScreenFadeColor() const { return m_screenFadeColor; }
    
    const std::string& GetActiveBgm() const { return m_activeBgm; }
    
    bool IsUiHidden() const { return m_isUiHidden || m_isTransitioning; }
    bool IsTransitioning() const { return m_isTransitioning; }
    bool IsAutoNext() const { return m_isAutoNext; }
    const std::map<std::string, ActiveEntityState>& GetActiveEntities() const { return m_activeEntities; }

    // Conditional Audio Playback (Event-driven)
    const std::vector<std::string>& ConsumePendingSFX();

    // History
    const std::vector<HistoryEntry>& GetHistory() const { return m_history; }
    void ClearHistory() { m_history.clear(); }

    // Auto-play
    bool IsAutoPlaying() const { return m_isAutoPlaying; }
    void ToggleAutoPlay() { m_isAutoPlaying = !m_isAutoPlaying; }

    bool IsChoiceVisible() const { return !m_visibleOptions.empty(); }

    // Delay State
    bool IsEventDelaying() const { return m_engineDelayTimer > 0.0f; }
    bool IsVisualAnimating() const {
        for (const auto& [id, state] : m_activeEntities) {
            if (state.moveTimer < state.moveDuration || state.fadeTimer < state.fadeDuration) return true;
        }
        if (m_bgFadeAlpha < 1.0f) return true;
        return false;
    }

    // Narrative Effects State
    float GetEffectShake() const { return m_shakeIntensity; }
    void TriggerShake(float intensity = 5.0f) { m_shakeIntensity = intensity; }

    // Validation
    static ValidationResult ValidateProject(const DialogProject& p);

    // Debug Instrumentation
    const std::vector<EventTraceEntry>& GetEventTrace() const { return m_eventTrace; }
    void ClearEventTrace() { m_eventTrace.clear(); }
    bool HasErrors() const { return !m_errors.empty(); }
    const std::vector<std::string>& GetErrors() const { return m_errors; }

    // v0.2 Debug Getters
    const std::vector<int>& GetCallStack() const { return m_callStack; }
    const std::vector<std::unordered_map<std::string, int>>& GetLocalScopes() const { return m_localVariables; }
    std::string GetWaitActionType() const { return m_waitingForActionType; }

private:
    DialogProject m_project;
    bool m_debugOverlayVisible = false;
    std::unordered_map<std::string, int> m_variables;
    std::vector<DialogOption> m_visibleOptions;
    std::string m_currentSpeakerId;
    bool m_isActive = false;

    float m_revealedCount = 0.0f;
    float m_waitTimer = 0.0f;
    std::string m_cachedInterpolatedContent;
    std::vector<RichChar> m_cachedParsedContent;
    size_t m_cachedTotalChars = 0;
    float m_shakeIntensity = 0.0f;

    float m_engineDelayTimer = 0.0f;
    std::string m_pendingJumpId = "";
    std::vector<std::string> m_pendingSFX;

    std::string m_activeBg = "";
    std::string m_prevBg = "";
    float m_bgFadeAlpha = 1.0f; // 1.0 = fully current, 0.0 = fully previous
    float m_bgFadeTimer = 0.0f;
    float m_bgFadeDuration = 0.0f;

    // Screen fade state
    float m_screenFadeAlpha = 0.0f;
    float m_screenFadeTarget = 0.0f;
    float m_screenFadeStart = 0.0f;
    float m_screenFadeTimer = 0.0f;
    float m_screenFadeDuration = 0.0f;
    BitColor m_screenFadeColor = {0,0,0,255};

    // Transition state machine
    bool m_isTransitioning = false;
    int m_transitionState = 0; // 0: Fading in, 1: Fading out, 2: Post-delay
    std::string m_transitionTargetNode = "";
    float m_transitionDurationVal = 0.6f;
    float m_transitionPostDelay = 0.0f;
    float m_transitionTimer = 0.0f;
    
    bool m_hasPendingTransition = false;
    float m_pendingTransitionDuration = 0.6f;
    float m_pendingTransitionPostDelay = 0.0f;

    std::string m_activeBgm = "";
    bool m_isUiHidden = false;
    bool m_isAutoNext = false;
    bool m_isAutoPlaying = false;
    float m_autoPlayTimer = 0.0f;
    float m_inputLockoutTimer = 0.0f;
    std::vector<HistoryEntry> m_history;
    std::map<std::string, ActiveEntityState> m_activeEntities;

    // Debug state
    std::vector<EventTraceEntry> m_eventTrace;
    std::vector<std::string> m_errors;

    // Narrative Stack
    std::vector<int> m_callStack;
    std::vector<std::unordered_map<std::string, int>> m_localVariables;
    std::string m_waitingForActionType = ""; // "sfx", "move", "fade", "all"

    void StartTransition(float duration, float postDelay);
    void RecordError(const std::string& context, const std::string& msg);
    void ProcessEvents(const std::vector<Event>& events);
    bool EvalConditionNode(const ConditionNode& node) const;
    void RefreshVisibleOptions();
    size_t GetUTF8Length(const std::string& s) const;
    std::string InterpolateVariables(const std::string& text) const;
    std::string XORBuffer(const std::string& data) const;
    std::string GetSlotPath(int slot) const;
    std::string GetTimestamp() const;
    float ParsePosition(const std::string& pos) const;
    float ParseXParam(const nlohmann::json& params, const std::string& key = "x") const;

    // VM State
    int m_pc = 0;
    bool m_vmWaiting = false;
    bool m_vmDelayed = false;
    void RunVM();
    void ExecuteInstruction(const BitInstruction& ins);
};

#endif
