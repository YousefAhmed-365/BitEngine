#include "BitGraphics.hpp"
#include <iostream>

BitGraphics::BitGraphics() {}

BitGraphics::~BitGraphics() {
    Shutdown();
}

void BitGraphics::Initialize() {
    CreateFallbackTexture();
    CreateVignetteTexture();
}

void BitGraphics::Shutdown() {
    for (auto& [p, t] : m_textureCache) ::UnloadTexture(t);
    for (auto& [p, f] : m_fontCache) UnloadFont(f);
    ::UnloadTexture(m_fallbackTexture);
    ::UnloadTexture(m_vignette);
    ::UnloadTexture(m_customCursor);
}

Texture2D BitGraphics::GetTexture(const std::string& path) {
    auto it = m_textureCache.find(path);
    if (it != m_textureCache.end()) {
        return it->second;
    }
    Texture2D tx = LoadTexture(path.c_str());
    if (tx.id == 0) {
        return m_fallbackTexture;
    }
    m_textureCache[path] = tx;
    return tx;
}

void BitGraphics::UnloadTexture(const std::string& path) {
    auto it = m_textureCache.find(path);
    if (it != m_textureCache.end()) {
        ::UnloadTexture(it->second);
        m_textureCache.erase(it);
    }
}

Font BitGraphics::GetFont(const std::string& path) {
    auto it = m_fontCache.find(path);
    if (it != m_fontCache.end()) {
        return it->second;
    }
    Font fnt = LoadFont(path.c_str());
    m_fontCache[path] = fnt;
    return fnt;
}

void BitGraphics::SetCustomCursorTexture(const std::string& path) {
    m_customCursor = GetTexture(path);
}

int BitGraphics::DrawRichText(const std::vector<RichChar>& content, int limit,
                              int x, int y, int fontSize, int maxWidth,
                              Color defaultColor, int lineSpacing, Font font) {
    int drawn = 0;
    int currentX = x;
    int currentY = y;
    
    for (int i = 0; i < limit && i < (int)content.size(); ++i) {
        const RichChar& rc = content[i];
        
        Color col = (rc.color.a > 0) ? Color{rc.color.r, rc.color.g, rc.color.b, rc.color.a} : defaultColor;
        
        if (font.baseSize > 0) {
            DrawTextEx(font, rc.ch, {(float)currentX, (float)currentY}, (float)fontSize, 1, col);
        } else {
            DrawText(rc.ch, currentX, currentY, fontSize, col);
        }
        
        currentX += 10;
        drawn++;
    }
    
    return drawn;
}

void BitGraphics::DrawStyledPanel(Rectangle rect, const UIStyleBlock& style) {
    Color bgColor = {200, 200, 200, 255};
    DrawRectangle((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height, bgColor);
}

void BitGraphics::CreateFallbackTexture() {
    Image img = GenImageChecked(64, 64, 8, 8, PINK, BLACK);
    m_fallbackTexture = LoadTextureFromImage(img);
    UnloadImage(img);
}

void BitGraphics::CreateVignetteTexture() {
    Image img = GenImageGradientRadial(256, 256, 0.5f, {0, 0, 0, 0}, {0, 0, 0, 255});
    m_vignette = LoadTextureFromImage(img);
    UnloadImage(img);
}
