#include "UIImage.hpp"
#include <SDL2/SDL.h>

namespace ST {

UIImage::UIImage(const std::string& name) : UIBase(name) {}

void UIImage::setTexture(SDL_Texture* tex) {
    m_texture = tex;
}

void UIImage::render(SDL_Renderer* renderer, float parentX, float parentY) const {
    if (!m_texture) return;
    float wx = parentX + m_position.x;
    float wy = parentY + m_position.y;
    SDL_Rect rect = {
        static_cast<int>(wx), static_cast<int>(wy),
        static_cast<int>(m_width > 0 ? m_width : 64),
        static_cast<int>(m_height > 0 ? m_height : 64)
    };
    SDL_RenderCopy(renderer, m_texture, nullptr, &rect);
}

}
