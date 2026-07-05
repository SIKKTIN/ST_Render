#pragma once

#include "engine/editor/GameObject.hpp"

namespace ST {

class ITransformUI {
public:
    virtual ~ITransformUI() = default;
    virtual bool renderControls(GameObject* obj) = 0;
};

}
