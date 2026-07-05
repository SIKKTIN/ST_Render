#pragma once

#include "Script.hpp"

namespace ST {

class TestScript : public Script {
public:
    void onInit() override;
    void onStart() override;
    void onUpdate(float deltaTime) override;
    void onDestroy() override;
    void onGameStart() override;
    void onGameStop() override;
    void renderInspector() override;
    const char* getClassName() const override { return "TestScript"; }

private:
    std::string m_message = "GameObject started!";
};

} // namespace ST
