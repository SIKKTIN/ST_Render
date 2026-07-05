#pragma once

#include <string>
#include "ITestModule.hpp"
#include "../Buffer/FrameBuffer.hpp"

class TestModule_FrameBuffer : public ITestModule {
public:
    TestModule_FrameBuffer();
    ~TestModule_FrameBuffer();

    const char* getName() const override { return "FrameBuffer Test"; }

    void render(void* canvasTexture, int canvasW, int canvasH) override;
    bool renderControls() override;
    void runConsole(std::string& output) override;

private:
    ST::FrameBuffer* m_frameBuffer;
    int m_renderMode;
};
