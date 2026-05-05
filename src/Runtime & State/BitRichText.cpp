#include "BitRichText.hpp"
#include <algorithm>
#include <sstream>
#include <cctype>

BitColor RichTextParser::StringToColor(const std::string& str) {
    std::string s = str;
    for (char& c : s) c = toupper(c);
    if (s == "RED") return {230, 41, 55, 255};
    if (s == "BLUE") return {0, 121, 241, 255};
    if (s == "GREEN") return {0, 228, 48, 255};
    if (s == "YELLOW") return {253, 249, 0, 255};
    if (s == "ORANGE") return {255, 161, 0, 255};
    if (s == "PURPLE") return {200, 122, 255, 255};
    if (s == "PINK") return {255, 109, 194, 255};
    if (s == "BLACK") return {0, 0, 0, 255};
    if (s == "WHITE") return {255, 255, 255, 255};
    if (s == "GRAY") return {130, 130, 130, 255};
    if (s == "GOLD") return {255, 203, 0, 255};
    
    // Hex parsing (#RRGGBB)
    if (s.length() == 7 && s[0] == '#') {
        unsigned int r, g, b;
        if (sscanf(s.c_str() + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
            return BitColor{ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
        }
    }
    return BitColor::Blank();
}

std::vector<RichChar> RichTextParser::Parse(const std::string& rawText) {
    std::vector<RichChar> result;
    result.reserve(rawText.length());
    BitColor currentColor = BitColor::Blank();
    float currentSpeedMod = 1.0f;
    bool currentShake = false;
    bool currentWave = false;
    
    size_t i = 0;
    while (i < rawText.length()) {
        if (rawText[i] == '[') {
            // Parse tag
            size_t end = rawText.find(']', i);
            if (end == std::string::npos) {
                // Treat as regular character
                RichChar rc;
                rc.ch[0] = rawText[i];
                rc.ch[1] = 0;
                rc.color = currentColor;
                rc.speedMod = currentSpeedMod;
                rc.shake = currentShake;
                rc.wave = currentWave;
                result.push_back(rc);
                i++;
                continue;
            }
            
            std::string tag = rawText.substr(i + 1, end - i - 1);
            
            // Parse tag
            if (tag.find("color:") == 0) {
                currentColor = StringToColor(tag.substr(6));
            } else if (tag.find("speed:") == 0) {
                try { currentSpeedMod = std::stof(tag.substr(6)); } catch(...) {}
            } else if (tag == "shake") {
                currentShake = true;
            } else if (tag == "/shake") {
                currentShake = false;
            } else if (tag == "wave") {
                currentWave = true;
            } else if (tag == "/wave") {
                currentWave = false;
            }
            
            i = end + 1;
        } else if (rawText[i] == '\\' && i + 1 < rawText.length()) {
            // Escape sequence
            RichChar rc;
            rc.ch[0] = rawText[i + 1];
            rc.ch[1] = 0;
            rc.color = currentColor;
            rc.speedMod = currentSpeedMod;
            rc.shake = currentShake;
            rc.wave = currentWave;
            result.push_back(rc);
            i += 2;
        } else {
            // Regular character
            RichChar rc;
            rc.ch[0] = rawText[i];
            rc.ch[1] = 0;
            rc.color = currentColor;
            rc.speedMod = currentSpeedMod;
            rc.shake = currentShake;
            rc.wave = currentWave;
            result.push_back(rc);
            i++;
        }
    }
    
    return result;
}
