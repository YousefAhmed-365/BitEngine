#ifndef BIT_APP_HPP
#define BIT_APP_HPP

#include "raylib.h"
#include <string>

class BitApp {
public:
    BitApp(const std::string& projectPath = "res/configs.json");
    ~BitApp();

    void Run();

private:
    void LoadConfig(const std::string& path);

    std::string m_projectPath;
    std::string m_title       = "BitEngine Core v2.0";
    int         m_width       = 1280;
    int         m_height      = 720;
    int         m_minWidth    = 800;
    int         m_minHeight   = 600;
    int         m_fps         = 60;
    bool        m_resizable   = true;
};

#endif
