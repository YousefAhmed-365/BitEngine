#ifndef BIT_RICH_TEXT_HPP
#define BIT_RICH_TEXT_HPP

#include <string>
#include <vector>
#include <cstring>

// Hardware-agnostic color representation
struct BitColor {
    unsigned char r, g, b, a;
    static BitColor Blank() { return {0, 0, 0, 0}; }
};

// Rich text character with inline formatting
struct RichChar {
    char ch[5]; // UTF-8 character (max 4 bytes + null)
    BitColor color = BitColor::Blank(); 
    float waitBefore = 0.0f;
    float speedMod = 1.0f;
    bool shake = false;
    bool wave = false;
    std::string font = ""; // font override per character

    RichChar() { std::memset(ch, 0, 5); }
};

/**
 * RichTextParser: Parses rich text markup and generates RichChar stream
 * 
 * Supports:
 * - Color tags: [red], [#FF00FF], etc
 * - Speed modifiers: [speed:0.5]
 * - Effects: [shake], [wave]
 * - Delays: [wait:500ms]
 * - Font overrides: [font:serif]
 */
class RichTextParser {
public:
    static std::vector<RichChar> Parse(const std::string& rawText);
    static BitColor StringToColor(const std::string& str);
};

#endif
