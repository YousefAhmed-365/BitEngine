#include "headers/RichText.hpp"
#include <sstream>
#include <algorithm>

Color RichTextParser::StringToColor(const std::string& str) {
    std::string s = str;
    for (char& c : s) c = toupper(c);
    if (s == "RED") return RED;
    if (s == "BLUE") return BLUE;
    if (s == "GREEN") return GREEN;
    if (s == "YELLOW") return YELLOW;
    if (s == "ORANGE") return ORANGE;
    if (s == "PURPLE") return PURPLE;
    if (s == "PINK") return PINK;
    if (s == "BLACK") return BLACK;
    if (s == "WHITE") return WHITE;
    if (s == "GRAY") return GRAY;
    if (s == "GOLD") return GOLD;
    if (s == "SKYBLUE") return SKYBLUE;
    
    // Hex parsing (#RRGGBB)
    if (s.length() == 7 && s[0] == '#') {
        unsigned int r, g, b;
        if (sscanf(s.c_str() + 1, "%02x%02x%02x", &r, &g, &b) == 3) {
            return Color{ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
        }
    }
    return BLANK;
}

std::vector<RichChar> RichTextParser::Parse(const std::string& rawText) {
    std::vector<RichChar> result;
    Color currentColor = BLANK;
    float currentSpeedMod = 1.0f;
    bool currentShake = false;
    bool currentWave = false;
    float accumulatedWait = 0.0f;

    size_t i = 0;
    while (i < rawText.length()) {
        if (rawText[i] == '[') {
            size_t closeIdx = rawText.find(']', i);
            if (closeIdx != std::string::npos) {
                std::string tag = rawText.substr(i + 1, closeIdx - i - 1);
                
                bool isTag = true;
                if (tag.find("color=") == 0) {
                    currentColor = StringToColor(tag.substr(6));
                } else if (tag == "/color") {
                    currentColor = BLANK;
                } else if (tag.find("speed=") == 0) {
                    try { currentSpeedMod = std::stof(tag.substr(6)); } catch (...) {}
                } else if (tag == "/speed") {
                    currentSpeedMod = 1.0f;
                } else if (tag == "shake") {
                    currentShake = true;
                } else if (tag == "/shake") {
                    currentShake = false;
                } else if (tag == "wave") {
                    currentWave = true;
                } else if (tag == "/wave") {
                    currentWave = false;
                } else if (tag.find("wait=") == 0) {
                    try { accumulatedWait += std::stof(tag.substr(5)); } catch (...) {}
                } else {
                    isTag = false; // Not a recognized tag, treat as raw text
                }

                if (isTag) {
                    i = closeIdx + 1;
                    continue; // Successfully parsed a tag, move to next char
                }
            }
        }

        // Parse UTF-8 character
        unsigned char c = rawText[i];
        size_t charLen = (c <= 127) ? 1 : ((c & 0xE0) == 0xC0) ? 2 : ((c & 0xF0) == 0xE0) ? 3 : 4;
        
        std::string utf8Char = rawText.substr(i, charLen);
        i += charLen;

        RichChar rc;
        rc.ch = utf8Char;
        rc.color = currentColor;
        rc.speedMod = currentSpeedMod;
        rc.shake = currentShake;
        rc.wave = currentWave;
        rc.waitBefore = accumulatedWait;
        
        accumulatedWait = 0.0f; // Reset wait after applying to this character
        result.push_back(rc);
    }
    return result;
}
