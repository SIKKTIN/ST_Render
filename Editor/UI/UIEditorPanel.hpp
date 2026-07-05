#pragma once

#include "UIBase.hpp"

namespace ST {

class UIEditorPanel : public UIBase {
public:
    explicit UIEditorPanel(const std::string& name);

    void setColor(float r, float g, float b, float a = 1.0f);
    void getColor(float& r, float& g, float& b, float& a) const;
    void setBorderColor(float r, float g, float b, float a = 1.0f);
    void setPadding(float pad) { m_padding = pad; }

    void render() const override;

private:
    float m_color[4] = { 0.18f, 0.18f, 0.18f, 1.0f };
    float m_borderColor[4] = { 0.3f, 0.3f, 0.3f, 1.0f };
    float m_padding = 8.0f;
};

}
