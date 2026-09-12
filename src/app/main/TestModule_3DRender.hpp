#pragma once

#include "app/module/IModule.hpp"
#include "renderer/buffer/FrameBuffer.hpp"
#include "renderer/buffer/DepthBuffer.hpp"
#include "renderer/pipeline/VertexShader.hpp"
#include "renderer/pipeline/Rasterizer.hpp"
#include "renderer/pipeline/FragmentShader.hpp"
#include "renderer/shader/ShaderCatalog.hpp"
#include "renderer/shader/ShaderManager.hpp"
#include "renderer/shader/ShaderProgram.hpp"
#include "renderer/geometry/Vertex.hpp"
#include "renderer/geometry/Mesh.hpp"
#include "core/math/Matrix4x4.hpp"
#include "core/math/Vector3.hpp"
#include <SDL2/SDL.h>
#include <memory>
#include <string>
#include <vector>

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
    const std::vector<ST::ShaderEntry>& getShaderEntries() const { return m_shaderCatalog.getEntries(); }
    int getSelectedShaderIndex() const { return m_selectedShaderIndex; }
    const std::string& getShaderError() const { return m_shaderError; }
    bool selectShaderIndex(int index);

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
    void scanShaderCatalog();
    void loadSelectedShader();
    void pollShaderReload();
    bool renderShaderControls();

    ST::FrameBuffer* m_frameBuffer;
    ST::DepthBuffer* m_depthBuffer;
    ST::Rasterizer m_rasterizer;
    ST::VertexShader m_vertexShader;
    ST::FragmentShader m_fragmentShader;
    ST::Material m_material;
    ST::Light m_light;
    ST::Vector3 m_ambientLight;
    bool m_lightingEnabled;

    ST::Mesh m_cube;
    ST::ShaderCatalog m_shaderCatalog;
    ST::ShaderManager m_shaderManager;
    std::shared_ptr<ST::IShaderProgram> m_builtinShader;
    std::shared_ptr<ST::IShaderProgram> m_activeShader;
    int m_selectedShaderIndex = -1;
    std::string m_shaderError;
    std::string m_shaderRoot = "Data/Shaders";

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
    SDL_Renderer* m_sdlRenderer = nullptr;
    SDL_Texture* m_outputTexture = nullptr;
    int m_outputTextureW = 0;
    int m_outputTextureH = 0;
};
