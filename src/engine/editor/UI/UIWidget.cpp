#include "UIWidget.hpp"

namespace ST {

UIWidget::UIWidget(const std::string& name) : UIBase(name) {}

void UIWidget::onMouseDown(float x, float y) {
    UIRect r = getWorldRect();
    if (r.contains(x, y)) {
        m_pressed = true;
    }
}

void UIWidget::onMouseUp(float x, float y) {
    if (m_pressed) {
        UIRect r = getWorldRect();
        if (r.contains(x, y)) {
            handleClick();
        }
    }
    m_pressed = false;
}

void UIWidget::onMouseMove(float x, float y) {
    UIRect r = getWorldRect();
    m_hovered = r.contains(x, y);
}

}
