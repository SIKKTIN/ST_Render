#pragma once

#include "ITransformUI.hpp"
#include <imgui.h>
#include <algorithm>

namespace ST {

class TransformUI_ImGui : public ITransformUI {
public:
    bool renderControls(GameObject* obj) override {
        if (!obj) return false;

        bool changed = false;

        ImGui::Text("Selected: %s", obj->name.c_str());
        ImGui::Spacing();

        float pos[3] = {
            obj->position.x,
            obj->position.y,
            obj->position.z
        };
        if (ImGui::DragFloat3("Position", pos, 1.0f)) {
            obj->position.x = pos[0];
            obj->position.y = pos[1];
            obj->position.z = pos[2];
            changed = true;
        }

        float rot[3] = {
            obj->rotation.x * 180.0f / 3.14159f,
            obj->rotation.y * 180.0f / 3.14159f,
            obj->rotation.z * 180.0f / 3.14159f
        };
        if (ImGui::DragFloat3("Rotation", rot, 1.0f, -180.0f, 180.0f)) {
            obj->rotation.x = rot[0] * 3.14159f / 180.0f;
            obj->rotation.y = rot[1] * 3.14159f / 180.0f;
            obj->rotation.z = rot[2] * 3.14159f / 180.0f;
            changed = true;
        }

        float sc[3] = {
            obj->scale.x,
            obj->scale.y,
            obj->scale.z
        };
        if (ImGui::DragFloat3("Scale", sc, 1.0f, 0.1f, 2000.0f)) {
            obj->scale.x = std::max(1.0f, sc[0]);
            obj->scale.y = std::max(1.0f, sc[1]);
            obj->scale.z = std::max(1.0f, sc[2]);
            changed = true;
        }

        ImGui::Spacing();

        ImGui::Text("Rendering");
        int order = obj->sortingOrder;
        if (ImGui::InputInt("Sorting Order", &order)) {
            obj->sortingOrder = order;
            changed = true;
        }
        ImGui::Checkbox("Show AABB", &obj->showAABB);

        ImGui::Spacing();
        ImGui::Separator();

        return changed;
    }
};

}
