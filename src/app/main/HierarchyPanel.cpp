#include "app/main/HierarchyPanel.hpp"
#include <imgui.h>
#include <algorithm>

ST::Matrix4x4 buildModelMatrix3D(const ST::GameObject* obj) {
    ST::Matrix4x4 T = ST::Matrix4x4::translation(obj->position.x, obj->position.y, obj->position.z);
    ST::Matrix4x4 Rx = ST::Matrix4x4::rotationX(obj->rotation.x);
    ST::Matrix4x4 Ry = ST::Matrix4x4::rotationY(obj->rotation.y);
    ST::Matrix4x4 Rz = ST::Matrix4x4::rotationZ(obj->rotation.z);
    ST::Matrix4x4 S  = ST::Matrix4x4::scale(obj->scale.x, obj->scale.y, obj->scale.z);
    return T * Ry * Rx * Rz * S;
}

ST::Matrix4x4 buildWorldMatrix3D(const ST::GameObject* obj) {
    ST::Matrix4x4 local = buildModelMatrix3D(obj);
    if (obj && obj->parent) {
        return buildWorldMatrix3D(obj->parent) * local;
    }
    return local;
}

// ---------------------------------------------------------------------------

static void appendHierarchyEntries(ST::Scene& scene,
                                  const std::vector<ST::GameObject*>& objects,
                                  ST::GameObject*& selected,
                                  bool& needsRerender) {
    for (size_t i = 0; i < objects.size(); ++i) {
        ST::GameObject* obj = objects[i];
        bool hasChildren = !obj->children.empty();

        ImGuiTreeNodeFlags flags = 0;
        if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;
        if (selected == obj) flags |= ImGuiTreeNodeFlags_Selected;

        bool opened = ImGui::TreeNodeEx(obj, flags, "%s", obj->name.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            selected = obj;
            needsRerender = true;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(2)) {
            ImGui::OpenPopup(("obj_popup_" + std::to_string(reinterpret_cast<uintptr_t>(obj))).c_str());
        }
        if (ImGui::BeginPopup(("obj_popup_" + std::to_string(reinterpret_cast<uintptr_t>(obj))).c_str())) {
            selected = obj;
            if (ImGui::MenuItem("Delete")) {
                scene.destroyGameObject(obj);
                selected = nullptr;
                needsRerender = true;
                ImGui::EndPopup();
                if (opened) ImGui::TreePop();
                return;
            }
            ImGui::EndPopup();
        }

        if (opened) {
            std::vector<ST::GameObject*> sortedChildren = obj->children;
            std::stable_sort(sortedChildren.begin(), sortedChildren.end(),
                [](const ST::GameObject* a, const ST::GameObject* b) {
                    return a->sortingOrder < b->sortingOrder;
                });
            appendHierarchyEntries(scene, sortedChildren, selected, needsRerender);
            ImGui::TreePop();
        }
    }
}

void renderSceneHierarchyPanel(ST::Scene& scene,
                              ST::GameObject*& selected,
                              bool& needsRerender) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (selected != nullptr && ImGui::Button("Deselect", ImVec2(avail.x, 0))) {
        selected = nullptr;
        needsRerender = true;
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "Scene Objects");

    std::vector<ST::GameObject*> rootObjects = scene.getRootObjects();
    if (rootObjects.empty()) {
        ImGui::TextDisabled("No objects");
        return;
    }
    std::stable_sort(rootObjects.begin(), rootObjects.end(),
        [](const ST::GameObject* a, const ST::GameObject* b) {
            return a->sortingOrder < b->sortingOrder;
        });

    appendHierarchyEntries(scene, rootObjects, selected, needsRerender);
}
