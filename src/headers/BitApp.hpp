#ifndef BIT_APP_HPP
#define BIT_APP_HPP

#include "raylib.h"
#include <string>

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

    std::string m_title       = "BitEngine Core v2.1";
    int         m_width       = 1280;
    int         m_height      = 720;
    int         m_minWidth    = 800;
    int         m_minHeight   = 600;
    int         m_fps         = 60;
    bool        m_resizable   = true;
};

#endif
