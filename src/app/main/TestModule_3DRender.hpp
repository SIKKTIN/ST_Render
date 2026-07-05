#pragma once

#include "app/module/IModule.hpp"
#include "renderer/buffer/FrameBuffer.hpp"
#include "renderer/buffer/DepthBuffer.hpp"
#include "renderer/pipeline/VertexShader.hpp"
#include "renderer/pipeline/Rasterizer.hpp"
#include "renderer/geometry/Vertex.hpp"
#include "renderer/geometry/Mesh.hpp"
#include "core/math/Matrix4x4.hpp"
#include "core/math/Vector3.hpp"
#include <SDL2/SDL.h>

// Minimal 3D render module.
// Renders a unit cube centered at the origin (edge length 1) using
// the existing software rasterizer pipeline. Right-mouse drag orbits
// the camera around the origin.
class TestModule_3DRender : public IModule {
public:
    TestModule_3DRender();
    ~TestModule_3DRender();

    const char* getName() const override { return "3D Render"; }
    bool needsRealTimeUpdate() const override { return true; }

    void update(float deltaTime) override;
    void render(void* canvasTexture, int canvasW, int canvasH) override;
    bool renderControls() override;

    void onCanvasMouseDown(int button, int canvasX, int canvasY) override;
    void onCanvasMouseUp(int button, int canvasX, int canvasY) override;
    void onCanvasMouseMove(int canvasX, int canvasY) override;
    void onWheel(float dx, float dy, int canvasX, int canvasY, int canvasW, int canvasH) override;

private:
    void rebuildBuffers(int canvasW, int canvasH);
    void drawMesh(const ST::Mesh& mesh,
                  const ST::Matrix4x4& model,
                  const ST::Matrix4x4& view,
                  const ST::Matrix4x4& projection);

    ST::FrameBuffer* m_frameBuffer;
    ST::DepthBuffer* m_depthBuffer;
    ST::Rasterizer m_rasterizer;
    ST::VertexShader m_vertexShader;

    ST::Mesh m_cube;

    // Orbit camera state (right-mouse drag rotates around origin)
    float m_orbitYaw;       // radians, around world Y
    float m_orbitPitch;     // radians, around camera-right axis
    float m_orbitDistance;  // distance from origin
    bool  m_rmbDown;
    int   m_lastCanvasX;
    int   m_lastCanvasY;

    int m_canvasW;
    int m_canvasH;
};