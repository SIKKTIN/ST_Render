#pragma once

struct SDL_Renderer;
#include "UIWidget.hpp"

namespace ST {

class UILabel : public UIWidget {
public:
    explicit UILabel(const std::string& name, const std::string& text = "");

    void setText(const std::string& text) { m_text = text; }
    const std::string& getText() const { return m_text; }
    void setColor(float r, float g, float b, float a = 1.0f) {
        m_color[0] = r; m_color[1] = g; m_color[2] = b; m_color[3] = a;
    }

    void render(SDL_Renderer* renderer, float parentX, float parentY) const;

private:
    std::string m_text;
    float m_color[4] = { 0.9f, 0.9f, 0.9f, 1.0f };
};

}
