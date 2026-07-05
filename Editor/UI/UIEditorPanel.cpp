#include "UIEditorPanel.hpp"
#include <imgui.h>

namespace ST {

UIEditorPanel::UIEditorPanel(const std::string& name) : UIBase(name) {
    m_borderColor[0] = 0.4f; m_borderColor[1] = 0.4f;
    m_borderColor[2] = 0.5f; m_borderColor[3] = 1.0f;
}

void UIEditorPanel::setColor(float r, float g, float b, float a) {
    m_color[0] = r; m_color[1] = g; m_color[2] = b; m_color[3] = a;
}

void UIEditorPanel::getColor(float& r, float& g, float& b, float& a) const {
    r = m_color[0]; g = m_color[1]; b = m_color[2]; a = m_color[3];
}

void UIEditorPanel::setBorderColor(float r, float g, float b, float a) {
    m_borderColor[0] = r; m_borderColor[1] = g;
    m_borderColor[2] = b; m_borderColor[3] = a;
}

void UIEditorPanel::render() const {
    ImGui::SetNextWindowPos(ImVec2(m_position.x, m_position.y));
    ImGui::SetNextWindowSize(ImVec2(m_width, m_height));

    ImGui::PushStyleColor(ImGuiCol_WindowBg,
        ImVec4(m_color[0], m_color[1], m_color[2], m_color[3]));
    ImGui::PushStyleColor(ImGuiCol_Border,
        ImVec4(m_borderColor[0], m_borderColor[1], m_borderColor[2], m_borderColor[3]));

    char id[128];
    snprintf(id, sizeof(id), "%s##%p", m_name.c_str(), this);

    ImGui::Begin(id, nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse);

    for (UIBase* child : m_children) {
        child->render();
    }

    ImGui::End();
    ImGui::PopStyleColor(2);
}

}
