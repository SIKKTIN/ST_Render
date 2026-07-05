#include "WASDMovement.hpp"
#include "GameObject.hpp"
#include <imgui.h>
#include <cmath>

namespace ST {

void WASDMovement::onInit() {
}

void WASDMovement::onGameStart() {
    m_inputEnabled = true;
}

void WASDMovement::onGameStop() {
    m_inputEnabled = false;
}

void WASDMovement::onUpdate(float deltaTime) {
    if (!m_gameObject || !m_inputEnabled) return;

    const Uint8* keys = SDL_GetKeyboardState(nullptr);

    float dx = 0.0f;
    float dy = 0.0f;

    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])    dy += 1.0f;
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])  dy -= 1.0f;
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])  dx -= 1.0f;
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) dx += 1.0f;

    if (dx != 0.0f || dy != 0.0f) {
        float len = sqrtf(dx * dx + dy * dy);
        if (len > 0.0f) {
            dx /= len;
            dy /= len;
        }
        m_gameObject->position.x += dx * m_speed * deltaTime;
        m_gameObject->position.y += dy * m_speed * deltaTime;
    }
}

void WASDMovement::renderInspector() {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "WASD Movement");
    ImGui::Text("W/A/S/D or Arrow Keys to move");

    ImGui::Text("Speed");
    float speed = m_speed;
    if (ImGui::DragFloat("##spd", &speed, 1.0f, 10.0f, 2000.0f, "%.0f px/s")) {
        m_speed = speed;
    }
}

REGISTER_SCRIPT(WASDMovement);

}
