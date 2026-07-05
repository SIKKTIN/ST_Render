#pragma once

struct SDL_Renderer;
struct SDL_Texture;
#include "UIBase.hpp"

namespace ST {

class UIImage : public UIBase {
public:
    explicit UIImage(const std::string& name);

    void setTexture(SDL_Texture* tex);
    SDL_Texture* getTexture() const { return m_texture; }

    void render(SDL_Renderer* renderer, float parentX, float parentY) const;

private:
    SDL_Texture* m_texture = nullptr;
};

}
