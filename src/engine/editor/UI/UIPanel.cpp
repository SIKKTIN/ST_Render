#include "UIPanel.hpp"
#include "UILabel.hpp"
#include "UIButton.hpp"
#include "UIImage.hpp"
#include <SDL2/SDL.h>

namespace ST {

UIPanel::UIPanel(const std::string& name) : UIBase(name) {}

void UIPanel::setColor(float r, float g, float b, float a) {
    m_color[0] = r; m_color[1] = g; m_color[2] = b; m_color[3] = a;
}

void UIPanel::getColor(float& r, float& g, float& b, float& a) const {
    r = m_color[0]; g = m_color[1]; b = m_color[2]; a = m_color[3];
}

void UIPanel::setBorderColor(float r, float g, float b, float a) {
    m_borderColor[0] = r; m_borderColor[1] = g;
    m_borderColor[2] = b; m_borderColor[3] = a;
}

void UIPanel::render(SDL_Renderer* renderer, int canvasX, int canvasY,
                     int canvasW, int canvasH) const {
    // Auto-fill canvas if enabled
    float drawW = m_width;
    float drawH = m_height;
    float drawX = canvasX + m_position.x;
    float drawY = canvasY + m_position.y;
    if (m_fillCanvas) {
        drawW = static_cast<float>(canvasW);
        drawH = static_cast<float>(canvasH);
    }
    if (drawW <= 0.0f || drawH <= 0.0f) return;

    // panel background
    SDL_SetRenderDrawColor(renderer,
        (Uint8)(m_color[0] * 255),
        (Uint8)(m_color[1] * 255),
        (Uint8)(m_color[2] * 255),
        (Uint8)(m_color[3] * 255));
    SDL_Rect bg = {
        static_cast<int>(drawX), static_cast<int>(drawY),
        static_cast<int>(drawW), static_cast<int>(drawH)
    };
    SDL_RenderFillRect(renderer, &bg);

    // border
    SDL_SetRenderDrawColor(renderer,
        (Uint8)(m_borderColor[0] * 255),
        (Uint8)(m_borderColor[1] * 255),
        (Uint8)(m_borderColor[2] * 255),
        (Uint8)(m_borderColor[3] * 255));
    SDL_RenderDrawRect(renderer, &bg);

    // render children with local offset
    for (UIBase* child : m_children) {
        if (child->getWidth() == 0 || child->getHeight() == 0) continue;
        if (auto* lbl = dynamic_cast<UILabel*>(child)) {
            lbl->render(renderer, drawX, drawY);
        } else if (auto* btn = dynamic_cast<UIButton*>(child)) {
            btn->render(renderer, drawX, drawY);
        } else if (auto* img = dynamic_cast<UIImage*>(child)) {
            img->render(renderer, drawX, drawY);
        } else if (auto* panel = dynamic_cast<UIPanel*>(child)) {
            panel->render(renderer,
                static_cast<int>(drawX), static_cast<int>(drawY), canvasW, canvasH);
        }
    }
}

}
