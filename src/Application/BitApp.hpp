#ifndef BIT_APP_HPP
#define BIT_APP_HPP

#include "raylib.h"
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// BitApp
// Main application entry point and window manager for the BitEngine
// ─────────────────────────────────────────────────────────────────────────────
// Responsibilities:
//   - Command-line argument processing (compile, dry-run, list-scenes, stats)
//   - Configuration loading from res/app.json
//   - Window setup and lifecycle management via Raylib
//   - Orchestration of the runtime engine
// ─────────────────────────────────────────────────────────────────────────────
class BitApp {
public:
    BitApp();
    ~BitApp();

    int ProcessArgs(int argc, char** argv);
    void Run(const std::string& projectPath);

private:
    void LoadConfig(const std::string& path);
    void PrintHelp(const char* argv0);
    int DoCompile(const std::string& src, const std::string& dst);
    int DoDryRun(const std::string& path);
    int DoListScenes(const std::string& path);
    int DoStats(const std::string& path);

    std::string m_title       = "BitEngine Core v0.1";
    int         m_width       = 1280;
    int         m_height      = 720;
    int         m_minWidth    = 800;
    int         m_minHeight   = 600;
    int         m_fps         = 60;
    bool        m_resizable   = true;
    bool        m_fullscreen  = false;
    bool        m_vsync       = true;
    bool        m_borderless  = false;
    bool        m_msaa_4x     = true;
};

#endif
