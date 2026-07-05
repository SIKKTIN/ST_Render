#pragma once

#include "Script.hpp"
#include <SDL2/SDL.h>

namespace ST {

class WASDMovement : public Script {
public:
    void onInit() override;
    void onGameStart() override;
    void onGameStop() override;
    void onUpdate(float deltaTime) override;
    void renderInspector() override;
    const char* getClassName() const override { return "WASDMovement"; }

    void setSpeed(float speed) { m_speed = speed; }
    float getSpeed() const { return m_speed; }

private:
    float m_speed = 200.0f;
    bool m_inputEnabled = false;
};

}
