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

    void onMouseDown(int button, int x, int y) override;
    void onMouseUp(int button) override;
    void onMouseMove(int x, int y) override;
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

    // Fly camera state (UE-style):
    //   - LMB drag: rotate view (yaw/pitch)
    //   - RMB drag also rotates (so you can fly + look simultaneously)
    //   - RMB hold + WASD/QE: move in the camera's local frame
    //   - Wheel: adjust movement speed
    //   - Shift: hold to sprint
    ST::Vector3 m_eye;
    float m_yaw;          // around world Y (radians)
    float m_pitch;        // around camera-right axis (radians)
    float m_moveSpeed;    // WASD units/sec
    float m_moveSpeedMin;
    float m_moveSpeedMax;
    bool  m_lmbDown;
    bool  m_rmbDown;
    int   m_lastCanvasX;
    int   m_lastCanvasY;

    int m_canvasW;
    int m_canvasH;
};