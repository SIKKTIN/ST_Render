#include "UIButton.hpp"
#include <SDL2/SDL.h>

namespace ST {

UIButton::UIButton(const std::string& name, const std::string& label)
    : UIWidget(name), m_label(label) {}

void UIButton::handleClick() {
    if (m_callback) m_callback();
}

void UIButton::render(SDL_Renderer* renderer, float parentX, float parentY) const {
    float wx = parentX + m_position.x;
    float wy = parentY + m_position.y;
    SDL_Rect rect = {
        static_cast<int>(wx), static_cast<int>(wy),
        static_cast<int>(m_width > 0 ? m_width : 80),
        static_cast<int>(m_height > 0 ? m_height : 28)
    };

    if (m_pressed) {
        SDL_SetRenderDrawColor(renderer, 180, 100, 60, 255);
    } else if (m_hovered) {
        SDL_SetRenderDrawColor(renderer, 80, 140, 220, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 60, 100, 180, 255);
    }
    SDL_RenderFillRect(renderer, &rect);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &rect);
}

}
