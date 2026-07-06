#pragma once
// Shared helpers for the scene-based test modules.
// TestModule_3DRender #includes this header.

#include "engine/editor/Scene.hpp"
#include "engine/editor/GameObject.hpp"
#include <vector>

// TRS matrix helpers (used by both 2D and 3D render modules).
ST::Matrix4x4 buildModelMatrix3D(const ST::GameObject* obj);
ST::Matrix4x4 buildWorldMatrix3D(const ST::GameObject* obj);

// Render a recursive tree of scene objects.  Clicking an entry sets `selected`
// to that object.  Right-clicking opens a Delete popup.
void renderSceneHierarchyPanel(ST::Scene& scene,
                               ST::GameObject*& selected,
                               bool& needsRerender);
