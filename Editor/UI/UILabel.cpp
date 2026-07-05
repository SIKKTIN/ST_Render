#include "UILabel.hpp"
#include <SDL2/SDL.h>

namespace ST {

UILabel::UILabel(const std::string& name, const std::string& text)
    : UIWidget(name), m_text(text) {}

void UILabel::render(SDL_Renderer* renderer, float parentX, float parentY) const {
    if (m_text.empty()) return;
    SDL_SetRenderDrawColor(renderer,
        (Uint8)(m_color[0] * 255),
        (Uint8)(m_color[1] * 255),
        (Uint8)(m_color[2] * 255),
        (Uint8)(m_color[3] * 255));

    float wx = parentX + m_position.x;
    float wy = parentY + m_position.y;
    SDL_Rect rect = { static_cast<int>(wx), static_cast<int>(wy),
                      static_cast<int>(m_text.length() * 9), 16 };
    SDL_RenderFillRect(renderer, &rect);
}

}
