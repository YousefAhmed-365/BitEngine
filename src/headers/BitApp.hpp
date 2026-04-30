#ifndef BIT_APP_HPP
#define BIT_APP_HPP

#include "raylib.h"
#include <string>

class BitApp {
public:
    BitApp();
    ~BitApp();

    void Run();

private:
    void LoadConfig(const std::string& path);

    std::string m_title       = "BitEngine";
    int         m_width       = 1280;
    int         m_height      = 720;
    int         m_minWidth    = 800;
    int         m_minHeight   = 600;
    int         m_fps         = 60;
    bool        m_resizable   = true;
};

#endif
