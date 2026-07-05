#include "Scripts/TestScript.hpp"
#include "GameObject.hpp"
#include "Script.hpp"
#include <iostream>

namespace ST {

void TestScript::onInit() {
}

void TestScript::onStart() {
    std::cout << "[TestScript] " << m_message << std::endl;
}

void TestScript::onUpdate(float deltaTime) {
    (void)deltaTime;
}

void TestScript::onDestroy() {
}

void TestScript::onGameStart() {
}

void TestScript::onGameStop() {
}

void TestScript::renderInspector() {
}

REGISTER_SCRIPT(TestScript);

} // namespace ST
