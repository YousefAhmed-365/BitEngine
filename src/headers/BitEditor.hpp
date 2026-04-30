#ifndef BIT_TOOL_HPP
#define BIT_TOOL_HPP

#include "raylib.h"
#include "BitEngine.hpp"
#include <string>
#include <vector>
#include <map>

class BitEditor {
public:
    enum class Mode { ProjectHub, ProjectDetail, Graph, ConfigEditor, EntityEditor, VarEditor };

    struct ProjectInfo {
        std::string name, path;
        std::string configFile = "res/configs.json";
    };

    BitEditor(DialogEngine& engine);
    void Update();
    void Draw();

private:
    struct GraphNode {
        std::string id;
        Vector2     pos        = {0, 0};
        bool        dragging   = false;
        Vector2     dragOffset = {0, 0};
    };

    struct InputState {
        int   cursorPos  = 0;
        float backTimer  = 0.f;
        float blinkTimer = 0.f;
    };

    struct Notification {
        std::string text;
        Color       color;
        float       ttl;
    };

    // ── GUI helpers ──
    bool GuiButton   (Rectangle r, const char* text, Color bg, Color fg, bool enabled = true);
    bool GuiInputText(Rectangle r, std::string& text, const char* hint,
                      const std::string& uid, int fontSize = 15, bool multiline = false);
    bool GuiDropdown (Rectangle r, const std::string& current,
                      const std::vector<std::string>& opts, std::string& out,
                      const std::string& uid);
    void GuiSeparator(float x, float y, float w);
    void DrawDropdownOverlay();
    void PushNotif   (const std::string& msg, Color col = {0,210,255,255}, float ttl = 2.5f);

    // ── Node helpers ──
    float       GetNodeHeight    (const DialogNode& dn) const;
    std::string GetNodeType      (const DialogNode& dn) const;
    Color       GetNodeTypeColor (const std::string& type) const;

    // ── Draw modes ──
    void DrawProjectHub();
    void DrawProjectDetail();
    void DrawConfigEditor();
    void DrawEntityEditor();
    void DrawVarEditor();
    void DrawGraphMode();

    // ── Graph sub-draws ──
    void DrawGrid();
    void DrawConnections();
    void DrawNode(GraphNode& gn, DialogNode& dn);
    void DrawNodeInspector();
    void DrawNodeOptions(DialogNode& n, float ix, float& iy, float iw);
    void DrawStatusBar();
    void DrawNotifications();

    // ── Theme ──
    Color Hover(Color c, bool h) const;
    Color Lerp (Color a, Color b, float t) const;

    // ── Logic ──
    void ScanForProjects();
    bool LoadProjectManifest(const std::string& path);
    void LoadProjectFiles();
    void CreateNewProject(const std::string& name);
    void CreateNewDialog (const std::string& name);
    void SaveGlobalConfigs();
    void OpenGraph       (const std::string& filename);
    void CreateNewNode   (Vector2 pos, const std::string& type = "dialog");
    void DeleteNode      (const std::string& id);
    void SaveCurrentGraph();

    // ──────────── State ────────────
    DialogEngine& m_engine;
    Mode          m_mode = Mode::ProjectHub;

    // Hub
    std::vector<ProjectInfo> m_projects;
    ProjectInfo              m_activeProject;
    std::string              m_newProjName = "";

    // Detail sidebar
    std::string              m_projectPath = ".";
    std::vector<std::string> m_dialogFiles;
    std::string              m_newFileNameBuf = "";

    // Graph
    Camera2D                         m_camera             = {{0,0},{0,0},0.f,1.f};
    std::string                      m_currentFile        = "";
    std::map<std::string, GraphNode> m_graphNodes;
    std::string                      m_selectedNode       = "";
    std::string                      m_linkingSource      = "";
    int                              m_linkingSourceOptIdx= -1;  // -1 = main pin
    bool                             m_dirty              = false;

    // Entity editor
    std::string                        m_selectedEntityId = "";
    std::map<std::string, std::string> m_spriteFrameBuf;
    std::map<std::string, std::string> m_entPosBuf;

    // Variable editor — per-variable string buffers (keyed by var id)
    std::map<std::string, std::string> m_varRenameBuf;
    std::map<std::string, std::string> m_varInitBuf;
    std::map<std::string, std::string> m_varMinBuf;
    std::map<std::string, std::string> m_varMaxBuf;

    // GUI input
    std::string  m_activeInputUid = "";
    InputState   m_inputState;

    // Dropdown — deferred overlay so it renders on top of everything
    struct DropdownPending {
        bool        active    = false;
        Rectangle   btnRect   = {};
        Rectangle   panelRect = {};
        std::vector<std::string> opts;
        std::string current;
        std::string uid;
        std::string* outPtr   = nullptr;
    };
    bool            m_dropdownOpen    = false;
    std::string     m_dropdownUid     = "";
    DropdownPending m_dropdownPending;

    // Notifications
    std::vector<Notification> m_notifs;
    float m_animTime = 0.f;

    // Theme colors
    Color m_bgColor, m_panelColor, m_panelLight, m_accentColor,
          m_textColor, m_dimColor, m_errorColor, m_successColor, m_warnColor;
};

#endif
