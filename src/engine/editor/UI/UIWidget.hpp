#pragma once

#include "UIBase.hpp"
#include <functional>

namespace ST {

class UIWidget : public UIBase {
public:
    explicit UIWidget(const std::string& name);

    void setCallback(std::function<void()> cb) { m_callback = std::move(cb); }

    void onMouseDown(float x, float y) override;
    void onMouseUp(float x, float y) override;
    void onMouseMove(float x, float y) override;

    virtual void handleClick() { if (m_callback) m_callback(); }

protected:
    bool m_pressed = false;
    bool m_hovered = false;
    std::function<void()> m_callback;
};

}
