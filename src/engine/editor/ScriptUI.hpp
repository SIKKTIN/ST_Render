#pragma once

#include <string>
#include <vector>
#include <imgui.h>

namespace ST {

class GameObject;
class Script;

class ScriptUI {
public:
    void setGameObject(GameObject* obj) { m_gameObject = obj; }

    // Returns true if anything changed
    bool renderControls();

private:
    GameObject* m_gameObject = nullptr;
};

}
