#pragma once

struct SDL_Renderer;
#include "UIBase.hpp"

namespace ST {

class UIPanel : public UIBase {
public:
    explicit UIPanel(const std::string& name);

    void setColor(float r, float g, float b, float a = 1.0f);
    void getColor(float& r, float& g, float& b, float& a) const;
    void setBorderColor(float r, float g, float b, float a = 1.0f);
    void setPadding(float pad) { m_padding = pad; }
    void setSortingOrder(int order) { m_sortingOrder = order; }
    int getSortingOrder() const { return m_sortingOrder; }

    void setFillCanvas(bool fill) { m_fillCanvas = fill; }
    bool isFillCanvas() const { return m_fillCanvas; }

    void render(SDL_Renderer* renderer, int canvasX, int canvasY, int canvasW, int canvasH) const;

private:
    float m_color[4] = { 0.85f, 0.85f, 0.85f, 0.9f };
    float m_borderColor[4] = { 0.6f, 0.6f, 0.6f, 1.0f };
    float m_padding = 8.0f;
    int m_sortingOrder = 0;
    bool m_fillCanvas = true;
};

}
