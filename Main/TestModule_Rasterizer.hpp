#pragma once

#include "ITestModule.hpp"
#include "../Renderer/Renderer.hpp"
#include "../Buffer/FrameBuffer.hpp"
#include "../Pipeline/Rasterizer.hpp"

class TestModule_Rasterizer : public ITestModule {
public:
    TestModule_Rasterizer();
    ~TestModule_Rasterizer();

    const char* getName() const override { return "Rasterizer Test"; }

    void render(void* canvasTexture, int canvasW, int canvasH) override;
    bool renderControls() override;
    void runConsole(std::string& output) override;

private:
    ST::FrameBuffer* m_frameBuffer;
    ST::Rasterizer m_rasterizer;
    int m_renderMode;
};
