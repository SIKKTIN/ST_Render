#pragma once

#include "TransformUI_ImGui.hpp"
#include "Cinemachine.hpp"
#include <imgui.h>

namespace ST {

class CinemachineUI : public TransformUI_ImGui {
public:
    bool renderControls(GameObject* obj) override {
        auto* cm = dynamic_cast<Cinemachine*>(obj);
        if (!cm) return false;

        bool changed = false;

        ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "Cinemachine");
        ImGui::Separator();

        // --- Position (camera center in world space) ---
        ImGui::Text("Position (World Center)");
        float pos[2] = { obj->position.x, obj->position.y };
        if (ImGui::DragFloat2("##pos", pos, 1.0f)) {
            obj->position.x = pos[0];
            obj->position.y = pos[1];
            changed = true;
        }

        ImGui::Spacing();

        // --- Viewport Width (height = width / 16:9 automatically) ---
        ImGui::Text("Viewport Width  (16:9 fixed)");
        float width = cm->getViewportWidth();
        if (ImGui::DragFloat("##width", &width, 1.0f, 100.0f, 4000.0f)) {
            width = std::max(100.0f, width);
            cm->setViewportWidth(width);
            changed = true;
        }
        ImGui::Text("Height: %.1f  (%.2f : 1)",
            cm->getViewportHeight(), Cinemachine::ASPECT_RATIO);

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Sorting Order");
        int order = obj->sortingOrder;
        if (ImGui::InputInt("##sorting", &order)) {
            obj->sortingOrder = order;
            changed = true;
        }

        return changed;
    }

};

}
