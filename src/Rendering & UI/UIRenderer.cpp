#include "UIRenderer.hpp"
#include "BitRuntime.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// UI Rendering & Input
// ─────────────────────────────────────────────────────────────────────────────
// Alternative UI rendering pipeline. BitRenderer is the active renderer.
// ─────────────────────────────────────────────────────────────────────────────

UIRenderer::UIRenderer(BitRuntime& engine) : m_engine(engine) {}

UIRenderer::~UIRenderer() {
    m_uiManager.Shutdown();
}

void UIRenderer::Draw() {
    // Update data store from engine
    // Draw all elements through m_uiManager
}

void UIRenderer::HandleInput() {
    // Handle mouse/keyboard input for UI elements
}

void UIRenderer::LoadLayout(const std::string& name, const std::string& path, int layer) {
    m_uiManager.Load(name, path, layer);
}

void UIRenderer::UnloadLayout(const std::string& name) {
    m_uiManager.Unload(name);
}

void UIRenderer::DrawElement(UIElement& elem) {}
void UIRenderer::DrawGroupElem(UIElement& elem) {}
void UIRenderer::DrawPanelElem(UIElement& elem) {}
void UIRenderer::DrawTextElem(UIElement& elem) {}
void UIRenderer::DrawRichTextElem(UIElement& elem) {}
void UIRenderer::DrawCursorElem(UIElement& elem) {}
void UIRenderer::DrawButtonElem(UIElement& elem) {}
void UIRenderer::DrawImageElem(UIElement& elem) {}
void UIRenderer::HandleElementInput(UIElement& elem) {}
