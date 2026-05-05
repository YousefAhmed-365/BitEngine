#ifndef BIT_RICH_TEXT_HPP
#define BIT_RICH_TEXT_HPP

#include <string>
#include <vector>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
// Rich Text & Markup Processing
// ─────────────────────────────────────────────────────────────────────────────
// Parses markup tags embedded in dialogue text to apply inline formatting.
//
// Supported Markup:
//   - Colors: [red], [#FF00FF], [rgb:255,0,0]
//   - Speed: [speed:0.5] — typewriter speed multiplier
//   - Effects: [shake], [wave] — per-character animation
//   - Timing: [wait:500ms] — pause before revealing character
//   - Fonts: [font:serif] — font override for character range
//
// Usage: RichTextParser::Parse("Hello [red]world[/red]!") → RichChar stream
// ─────────────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────
// RichTextParser: Markup → Formatted Characters
// ─────────────────────────────────────────────────────────────────────────────
// Converts raw text with embedded markup tags into a stream of formatted characters.
// Each character carries color, animation parameters, and timing information.
// ─────────────────────────────────────────────────────────────────────────────
class RichTextParser {
public:
    static std::vector<RichChar> Parse(const std::string& rawText);
    static BitColor StringToColor(const std::string& str);
};

#endif
