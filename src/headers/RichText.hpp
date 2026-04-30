#ifndef RICH_TEXT_HPP
#define RICH_TEXT_HPP

#include <string>
#include <vector>
#include "raylib.h"

struct RichChar {
    std::string ch;
    Color color = BLANK; // BLANK usually means style default when rendering
    float waitBefore = 0.0f;
    float speedMod = 1.0f;
    bool shake = false;
    bool wave = false;
};

class RichTextParser {
public:
    // Parses raw text containing tags into a vector of characters with metadata
    static std::vector<RichChar> Parse(const std::string& rawText);

    // Helper to map color strings to Raylib Color
    static Color StringToColor(const std::string& str);
};

#endif
