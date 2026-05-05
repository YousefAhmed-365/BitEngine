#ifndef BITRUNTIME_HPP
#define BITRUNTIME_HPP

#include "json.hpp"
#include "BitOp.hpp"
#include "BitRichText.hpp"
#include "BitVM.hpp"
#include "BitState.hpp"
#include <string>
#include <unordered_map>
#include <map>
#include <optional>
#include <memory>

#define BITENGINE_KEY "BITENGINE_SECRET_KEY_2026"

struct TimelineEvent {
    int time_ms = 0;
    BitOp op;
    std::vector<std::string> args;
    nlohmann::json metadata;
};

struct Timeline {
    std::string id;
    std::vector<TimelineEvent> events;
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
    nlohmann::json      params;  // fields depend on op
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
    std::string id = "", name = "Unknown"; 
    float default_pos_x = 0.5f;
    std::unordered_map<std::string, SpriteDef> sprites = {}; 
    std::unordered_map<std::string, nlohmann::json> aliases = {};
};

struct VariableDef { std::string id = ""; int initial_value = 0; std::optional<int> min = std::nullopt, max = std::nullopt; };

struct SaveMetadata {
    std::string timestamp;
    std::string node_id;
    std::string entity_name;
    std::string summary;
};

struct DialogConfigs {
    std::string start_node = "scene_start", save_prefix = "save_slot_", mode = "typewriter";
    std::string debug_mode = "none";
    float reveal_speed = 45.0f;
    float auto_play_delay = 2.0f;
    bool auto_save = false, encrypt_save = false; 
    bool enable_floating = true, enable_shadows = true, enable_vignette = true;
    bool strict_assets = false;
    int max_slots = 5;
};

struct BitProject {
    DialogConfigs configs;
    std::unordered_map<std::string, Entity> entities;
    std::unordered_map<std::string, VariableDef> variables;
    
    // Asset Registries
    std::unordered_map<std::string, std::string> backgrounds;
    std::unordered_map<std::string, std::string> music;
    std::unordered_map<std::string, std::string> sfx;
    std::unordered_map<std::string, std::string> fonts;
    std::unordered_map<std::string, UILayoutDef> uiLayouts;
    
    // Bytecode
    std::vector<BitInstruction> bytecode;
    
    // Timelines & Events
    std::unordered_map<std::string, Timeline> timelines;
    std::unordered_map<std::string, std::vector<BitInstruction>> events;

    // Parse errors caught during compilation
    std::vector<std::string> parseErrors;
};

struct ValidationResult {
    std::vector<std::string> errors;
};

// UI command queued by the VM for the renderer to drain each frame
struct UICommand {
    enum class Type { Load, Unload, Set, Activate, Deactivate } type;
    std::string name;     // UI namespace / scoped element id
    std::string arg1;     // path (Load) | property (Set)
    std::string arg2;     // value (Set)
    int         layer = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// BitRuntime: Narrative Engine Core
// ─────────────────────────────────────────────────────────────────────────────
// The central narrative engine orchestrating all screenplay execution, state,
// and interactions between VM, renderer, and game logic.
//
// Key Responsibilities:
//   - Bytecode execution (delegates to BitVM)
//   - State management (BitState holds all persistent/frame data)
//   - Asset loading and registry (backgrounds, music, SFX, fonts)
//   - Save/load game persistence
//   - Game variable tracking
//   - Active entity/timeline management
//   - Event handling (dialogue options, timed events)
//   - Rich text parsing & reveal animation
//   - Debug overlay & introspection
//
// Control Flow:
//   1. LoadProject/CompileProject: Load & parse screenplay
//   2. StartDialog: Initialize execution at a scene node
//   3. Update(deltaTime): Execute one frame (advance VM, animate, reveal text)
//   4. SelectOption: Handle player choice input
//   5. Next/EmitEvent: Advance or trigger branch logic
// ─────────────────────────────────────────────────────────────────────────────
class BitRuntime {
    friend class BitVM;
public:
    BitRuntime();
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
    void EmitEvent(const std::string& evt);
    
    void Update(float deltaTime);
    void SkipReveal(); 
    bool IsTextRevealing() const;
    const std::vector<RichChar>& GetParsedContent() const { return m_cachedParsedContent; }
    int GetRevealedCount() const { return (int)m_revealedCount; }
    float GetScreenFadeAlpha() const { return m_state.ScreenFadeAlpha(); }
    BitColor GetScreenFadeColor() const { return m_screenFadeColor; }
    
    std::string GetVisibleContent() const;
    
    int GetVariable(const std::string& name) const;
    int SafeStoi(const std::string& s) const;
    int ResolveParamInt(const nlohmann::json& params, const std::string& key, int default_val = 0) const;
    float ResolveParamFloat(const nlohmann::json& params, const std::string& key, float default_val = 0.0f) const;
    void SetVariable(const std::string& name, int value);
    const std::unordered_map<std::string, int>& GetAllVariables() const { return m_state.GetVariables(); }
    
    bool IsActive() const { return m_isActive; }
    int GetCurrentPC() const { return m_vm->GetPC(); }

    const Entity* GetCurrentEntity() const;
    const Entity* GetEntity(const std::string& id) const { return m_project.entities.count(id) ? &m_project.entities.at(id) : nullptr; }
    const std::vector<DialogOption>& GetVisibleOptions() const { return m_visibleOptions; }
    const DialogConfigs& GetConfigs() const { return m_project.configs; }
    BitProject& GetProject() { return m_project; }
    std::string GetDebugMode() const { return m_project.configs.debug_mode; }
    void Log(const std::string& msg, const std::string& level = "INFO") const;
    bool IsDebugOverlayVisible() const { return m_debugOverlayVisible; }
    void ToggleDebugOverlay() { m_debugOverlayVisible = !m_debugOverlayVisible; }
    void SetDebugOverlayVisible(bool visible) { m_debugOverlayVisible = visible; }
    std::string GetCurrentLabel() const;

    // Global Asset Retrieval
    std::string GetBackground(const std::string& id) const { return m_project.backgrounds.count(id) ? m_project.backgrounds.at(id) : ""; }
    std::string GetMusic(const std::string& id) const { return m_project.music.count(id) ? m_project.music.at(id) : ""; }
    std::string GetSFX(const std::string& id) const { return m_project.sfx.count(id) ? m_project.sfx.at(id) : ""; }

    // Active Persistent States
    const std::string& GetActiveBg() const { return m_state.GetActiveBg(); }
    const std::string& GetPrevBg() const { return m_state.GetPrevBg(); }
    float GetBgFadeAlpha() const { return m_state.BgFadeAlpha(); }
    const std::string& GetActiveBgm() const { return m_state.GetActiveBgm(); }
    
    bool IsUiHidden() const { return m_state.IsUiHidden(); }
    bool IsTransitioning() const { return false; }
    bool IsAutoNext() const { return m_isAutoNext; }
    const std::map<std::string, ActiveEntityState>& GetActiveEntities() const { return m_state.GetActiveEntities(); }

    // Conditional Audio Playback
    const std::vector<std::string>& ConsumePendingSFX();

    // History
    const std::vector<HistoryEntry>& GetHistory() const { return m_state.History(); }
    void ClearHistory() { m_state.History().clear(); }

    // Auto-play
    bool IsAutoPlaying() const { return m_isAutoPlaying; }
    void ToggleAutoPlay() { m_isAutoPlaying = !m_isAutoPlaying; }

    bool IsInputLocked() const { return m_inputLockoutTimer > 0.0f; }
    bool IsChoiceVisible() const { return !m_visibleOptions.empty(); }

    // Delay State
    bool IsEventDelaying() const { return m_engineDelayTimer > 0.0f; }
    bool IsVisualAnimating() const;

    // Narrative Effects State
    float GetEffectShake() const { return m_state.ShakeIntensity(); }
    void TriggerShake(float intensity = 5.0f) { m_state.ShakeIntensity() = intensity; }

    // Validation
    static ValidationResult ValidateProject(const BitProject& p);

    // Debug Instrumentation
    const std::vector<EventTraceEntry>& GetEventTrace() const { return m_state.EventTrace(); }
    void ClearEventTrace() { m_state.EventTrace().clear(); }
    bool HasErrors() const { return !m_errors.empty(); }
    const std::vector<std::string>& GetErrors() const { return m_errors; }

    // UI Command queue
    std::vector<UICommand> DrainUICommands();

    // System variables (set automatically by the VM, readable by UI bindings)
    // Keys: "var.entity_name", "var.entity_id", "var.dialog", "var.is_revealing", "var.ui_hidden"
    const nlohmann::json& GetSystemVars() const { return m_sysVars; }

    // v0.2 Debug Getters
    const std::vector<int>& GetCallStack() const { return m_vm->GetCallStack(); }
    const std::vector<std::unordered_map<std::string, int>>& GetLocalScopes() const { return m_vm->GetLocalScopes(); }
    std::string GetWaitActionType() const { return m_vm->IsWaiting() ? "waiting" : "running"; }

    // Accessors for VM/State (Internal use)
    BitState& GetState() { return m_state; }
    BitVM& GetVM() { return *m_vm; }

private:
    BitProject m_project;
    BitState m_state;
    std::unique_ptr<BitVM> m_vm;
    
    bool m_debugOverlayVisible = false;
    std::vector<DialogOption> m_visibleOptions;
    std::string m_currentSpeakerId;
    bool m_isActive = false;

    float m_revealedCount = 0.0f;
    float m_waitTimer = 0.0f;
    std::string m_cachedInterpolatedContent;
    std::vector<RichChar> m_cachedParsedContent;
    size_t m_cachedTotalChars = 0;

    float m_engineDelayTimer = 0.0f;
    std::string m_pendingJumpId = "";
    std::vector<std::string> m_pendingSFX;
    std::string m_waitingForEventId = "";

    BitColor m_screenFadeColor = {0,0,0,255};

    bool m_isAutoNext = false;
    bool m_isAutoPlaying = false;
    float m_autoPlayTimer = 0.0f;
    float m_inputLockoutTimer = 0.0f;

    std::vector<std::string> m_errors;
    std::vector<UICommand> m_pendingUICommands;
    nlohmann::json m_sysVars;

    std::string m_projectBasePath;
    std::unordered_map<std::string, std::filesystem::file_time_type> m_fileWatchTimestamps;
    float m_hotReloadTimer = 0.0f;
    void CheckHotReload();

    void UpdateSysVars();
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
};

#endif
