#pragma once

#include "app/module/IModule.hpp"
#include "renderer/pipeline/VertexShader.hpp"
#include "renderer/pipeline/Rasterizer.hpp"
#include "renderer/geometry/Mesh.hpp"
#include "renderer/buffer/FrameBuffer.hpp"
#include "renderer/buffer/DepthBuffer.hpp"
#include "core/math/Matrix4x4.hpp"

class TestModule_VertexShader : public IModule {
public:
    TestModule_VertexShader();
    ~TestModule_VertexShader();

    const char* getName() const override { return "VertexShader Test"; }

    void render(void* canvasTexture, int canvasW, int canvasH) override;
    bool renderControls() override;
    void runConsole(std::string& output) override;

private:
    ST::FrameBuffer* m_frameBuffer;
    ST::DepthBuffer* m_depthBuffer;
    ST::Rasterizer m_rasterizer;
    ST::VertexShader m_vertexShader;

    ST::Mesh m_mesh;

    float m_posX;
    float m_posY;
    float m_rotation;
    float m_scaleX;
    float m_scaleY;

    int m_renderMode;
    bool m_showDebugBbox;
    int m_debugBboxMinX, m_debugBboxMaxX, m_debugBboxMinY, m_debugBboxMaxY;

    void initMesh();
    void setupUniforms(int canvasW, int canvasH);
};
