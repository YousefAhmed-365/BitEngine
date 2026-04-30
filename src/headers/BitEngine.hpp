#ifndef BITENGINE_HPP
#define BITENGINE_HPP

#include <string>
#include <vector>
#include <map>
#include <optional>

// --- Logic Structures ---

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
    std::string id, name, type; 
    std::map<std::string, SpriteDef> sprites; 
};

struct VariableDef { std::string id; int initial_value; std::optional<int> min, max; };

struct DialogNode {
    std::string entity_id, content;
    std::optional<std::string> next_id;
    std::vector<DialogOption> options;
    std::vector<Event> events;         
    std::map<std::string, std::string> metadata; 
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
    std::map<std::string, int> variables;
    SaveMetadata meta;
};

struct DialogConfigs {
    std::string start_node = "dialog_start", save_prefix = "save_slot_", mode = "typewriter";
    float reveal_speed = 45.0f;
    bool debug = false, auto_save = false, encrypt_save = false; 
    bool enable_floating = true, enable_shadows = true, enable_vignette = true;
    int max_slots = 5;
    std::vector<std::string> dialog_files;
    std::vector<std::string> entity_files;
    std::vector<std::string> variable_files;
};

struct DialogProject {
    DialogConfigs configs;
    std::map<std::string, Entity> entities;
    std::map<std::string, VariableDef> variables;
    std::map<std::string, DialogNode> nodes;
};

class DialogEngine {
public:
    DialogEngine();
    bool LoadProject(const std::string& configFilePath);
    
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
    std::string GetVisibleContent() const; 
    
    int GetVariable(const std::string& name) const;
    void SetVariable(const std::string& name, int value);
    const std::map<std::string, int>& GetAllVariables() const { return m_variables; }
    
    bool IsActive() const { return m_isActive; }
    const DialogNode* GetCurrentNode() const { return m_currentNode; }
    const Entity* GetCurrentEntity() const;
    const std::vector<DialogOption>& GetVisibleOptions() const { return m_visibleOptions; }
    const DialogConfigs& GetConfigs() const { return m_project.configs; }
    bool IsDebug() const { return m_project.configs.debug; }

    // Narrative Effects State
    float GetEffectShake() const { return m_shakeIntensity; }
    void TriggerShake(float intensity = 5.0f) { m_shakeIntensity = intensity; }

private:
    DialogProject m_project;
    std::map<std::string, int> m_variables;
    std::vector<DialogOption> m_visibleOptions;
    const DialogNode* m_currentNode = nullptr;
    bool m_isActive = false;

    float m_revealedCount = 0.0f;
    std::string m_cachedInterpolatedContent;
    size_t m_cachedTotalChars = 0;
    float m_shakeIntensity = 0.0f;

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
};

#endif
