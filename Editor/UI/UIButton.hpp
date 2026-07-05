#pragma once

struct SDL_Renderer;
#include "UIWidget.hpp"

namespace ST {

class UIButton : public UIWidget {
public:
    using Callback = std::function<void()>;

    explicit UIButton(const std::string& name, const std::string& label = "");

    void setLabel(const std::string& label) { m_label = label; }
    const std::string& getLabel() const { return m_label; }

    void handleClick() override;

    void render(SDL_Renderer* renderer, float parentX, float parentY) const;

private:
    std::string m_label;
};

}
