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

struct RichChar {
    char ch[5]; // UTF-8 character (max 4 bytes + null)
    BitColor color = BitColor::Blank(); 
    float waitBefore = 0.0f;
    float speedMod = 1.0f;
    bool shake = false;
    bool wave = false;

    RichChar() { std::memset(ch, 0, 5); }
};

class RichTextParser {
public:
    static std::vector<RichChar> Parse(const std::string& rawText);
    static BitColor StringToColor(const std::string& str);
};

struct Condition { std::string op, var; int value; };
struct Event { std::string op, var; int value; };

struct DialogOption {
    std::string content, next_id, style;
    std::vector<Condition> conditions; 
    std::vector<Event> events;         
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
};

struct VariableDef { std::string id = ""; int initial_value = 0; std::optional<int> min = std::nullopt, max = std::nullopt; };

struct DialogNode {
    std::string entity = "", content = "";
    std::optional<std::string> next_id = std::nullopt;
    std::vector<DialogOption> options = {};
    std::vector<Event> events = {};         
    std::unordered_map<std::string, std::string> metadata = {}; 
};

struct SaveMetadata {
    std::string timestamp;
    std::string node_id;
    std::string entity_name;
    std::string summary;
};

struct SaveData {
    int version = 1;
    std::string current_node_id;
    std::unordered_map<std::string, int> variables;
    SaveMetadata meta;
};

struct DialogConfigs {
    std::string start_node = "dialog_start", save_prefix = "save_slot_", mode = "typewriter";
    std::string debug_mode = "none";
    float reveal_speed = 45.0f;
    bool auto_save = false, encrypt_save = false; 
    bool enable_floating = true, enable_shadows = true, enable_vignette = true;
    int max_slots = 5;
    std::vector<std::string> dialog_files;
    std::vector<std::string> entity_files;
    std::vector<std::string> variable_files;
    std::vector<std::string> asset_files;
};

struct DialogProject {
    DialogConfigs configs;
    std::unordered_map<std::string, Entity> entities;
    std::unordered_map<std::string, VariableDef> variables;
    std::unordered_map<std::string, DialogNode> nodes;
    
    // Asset Registries
    std::unordered_map<std::string, std::string> backgrounds;
    std::unordered_map<std::string, std::string> music;
    std::unordered_map<std::string, std::string> sfx;
};

// --- Validation Result ---
struct ValidationError {
    std::string file;
    std::string field;
    std::string message;
};
using ValidationResult = std::vector<ValidationError>;

// --- Event Trace Entry ---
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
    bool LoadProject(const std::string& configFilePath);
    bool LoadCompiledProject(const std::string& binPath);
    void CompileProject(const std::string& outputPath);
    
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
    void SetVariable(const std::string& name, int value);
    const std::unordered_map<std::string, int>& GetAllVariables() const { return m_variables; }
    
    bool IsActive() const { return m_isActive; }
    const DialogNode* GetCurrentNode() const { return m_currentNode; }
    std::string GetCurrentNodeId() const { return m_currentNodeId; }
    const Entity* GetCurrentEntity() const;
    const std::vector<DialogOption>& GetVisibleOptions() const { return m_visibleOptions; }
    const DialogConfigs& GetConfigs() const { return m_project.configs; }
    DialogProject& GetProject() { return m_project; }
    std::string GetDebugMode() const { return m_project.configs.debug_mode; }
    void Log(const std::string& msg, const std::string& level = "INFO");
    bool IsDebugOverlayVisible() const { return m_debugOverlayVisible; }
    void ToggleDebugOverlay() { m_debugOverlayVisible = !m_debugOverlayVisible; }
    void SetDebugOverlayVisible(bool visible) { m_debugOverlayVisible = visible; }

    // Global Asset Retrieval
    std::string GetBackground(const std::string& id) const { return m_project.backgrounds.count(id) ? m_project.backgrounds.at(id) : ""; }
    std::string GetMusic(const std::string& id) const { return m_project.music.count(id) ? m_project.music.at(id) : ""; }
    std::string GetSFX(const std::string& id) const { return m_project.sfx.count(id) ? m_project.sfx.at(id) : ""; }

    // Narrative Effects State
    float GetEffectShake() const { return m_shakeIntensity; }
    void TriggerShake(float intensity = 5.0f) { m_shakeIntensity = intensity; }

    // --- Validation ---
    static ValidationResult ValidateProject(const DialogProject& p);

    // --- Debug Instrumentation ---
    const std::vector<EventTraceEntry>& GetEventTrace() const { return m_eventTrace; }
    void ClearEventTrace() { m_eventTrace.clear(); }
    bool HasErrors() const { return !m_errors.empty(); }
    const std::vector<std::string>& GetErrors() const { return m_errors; }

private:
    DialogProject m_project;
    bool m_debugOverlayVisible = false;
    std::unordered_map<std::string, int> m_variables;
    std::vector<DialogOption> m_visibleOptions;
    const DialogNode* m_currentNode = nullptr;
    std::string m_currentNodeId;
    bool m_isActive = false;

    float m_revealedCount = 0.0f;
    float m_waitTimer = 0.0f;
    std::string m_cachedInterpolatedContent;
    std::vector<RichChar> m_cachedParsedContent;
    size_t m_cachedTotalChars = 0;
    float m_shakeIntensity = 0.0f;

    // Debug state
    std::vector<EventTraceEntry> m_eventTrace;
    std::vector<std::string> m_errors;

    void RecordError(const std::string& context, const std::string& msg);
    void ProcessEvents(const std::vector<Event>& events);
    void RefreshVisibleOptions();
    size_t GetUTF8Length(const std::string& s) const;
    std::string InterpolateVariables(const std::string& text) const;
    std::string XORBuffer(const std::string& data) const;
    std::string GetSlotPath(int slot) const;
    std::string GetTimestamp() const;
};

class DialogParser {
public:
    static DialogProject ParseConfig(const std::string& path);
    static bool LoadDialogFile(const std::string& path, DialogProject& p);
    static bool LoadEntitiesFile(const std::string& path, DialogProject& p);
    static bool LoadVariablesFile(const std::string& path, DialogProject& p);
    static bool LoadAssetsFile(const std::string& path, DialogProject& p);
};

#endif
