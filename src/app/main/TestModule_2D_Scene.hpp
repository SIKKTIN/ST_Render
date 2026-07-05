#pragma once

#include <SDL2/SDL.h>
#include "app/module/IModule.hpp"
#include "renderer/buffer/FrameBuffer.hpp"
#include "renderer/buffer/DepthBuffer.hpp"
#include "renderer/pipeline/VertexShader.hpp"
#include "renderer/pipeline/Rasterizer.hpp"
#include "renderer/geometry/Vertex.hpp"
#include "renderer/geometry/Mesh.hpp"
#include "core/math/Matrix4x4.hpp"
#include "engine/editor/Scene.hpp"
#include "engine/editor/GameObject.hpp"
#include "engine/editor/ITransformUI.hpp"
#include "engine/editor/Sprite2DUI.hpp"
#include "engine/editor/Sprite2D.hpp"
#include "engine/editor/MusicPlayerUI.hpp"
#include "engine/editor/AudioManager.hpp"
#include "engine/editor/CinemachineUI.hpp"
#include "engine/editor/OrbitCameraController.hpp"
#include "core/data/DataBase/SceneData.hpp"
#include <string>
#include <unordered_map>

class TestModule_2D_Scene : public IModule {
public:
    TestModule_2D_Scene();
    ~TestModule_2D_Scene();

    const char* getName() const override { return "2D Scene"; }
    bool needsRealTimeUpdate() const override { return true; }

    void onMouseMove(int x, int y) override;
    void onMouseDown(int button, int x, int y) override;
    void onMouseUp(int button) override;
    void onWheel(float dx, float dy, int canvasX, int canvasY, int canvasW, int canvasH) override;

    void onCanvasMouseMove(int canvasX, int canvasY) override;
    void onCanvasMouseDown(int button, int canvasX, int canvasY) override;
    void onCanvasMouseUp(int button, int canvasX, int canvasY) override;

    void onKeyDown(int keycode) override;
    void onKeyUp(int keycode) override;
    void update(float deltaTime) override;

    void render(void* canvasTexture, int canvasW, int canvasH) override;
    bool renderControls() override;
    void renderOverlays() override;
    void renderUIOverlay(int canvasX, int canvasY, int canvasW, int canvasH) override;
    void renderCreatePanel() override;
    void runConsole(std::string& output) override;
    void setRenderer(void* renderer) override;

    int getViewportMode() const { return m_viewportMode; }
    void setViewportMode(int mode) { m_viewportMode = mode; }
    bool isGameRunning() const { return m_gameRunning; }

    bool isView3D() const { return m_view3D; }
    void setView3D(bool v);

private:
    void buildThumbnails(SDL_Renderer* renderer);
    void rebuildBuffers(int canvasW, int canvasH);
    void rebuildSSAABuffer(int canvasW, int canvasH);
    void downsample(ST::FrameBuffer* src, ST::FrameBuffer* dst);
    void applyFXAA(ST::FrameBuffer* fb);
    void renderScene(ST::FrameBuffer* fb, int renderW, int renderH);
    void renderGridQuad(ST::FrameBuffer* fb, const ST::Matrix4x4& view, const ST::Matrix4x4& projection, int canvasW, int canvasH);
    void drawLine(ST::FrameBuffer* fb, int x0, int y0, int x1, int y1, ST::Color color);
    void screenToWorld(int sx, int sy, int canvasW, int canvasH, float& wx, float& wy);
    void updateViewFromCamera();
    void updateCinemachine(float deltaTime);
    void syncGameRuntimeState();
    bool isInCanvas(int x, int y, int canvasW, int canvasH) const;
    bool isInCanvasScreen(int sx, int sy, int canvasW, int canvasH) const;
    int toCanvasX(int screenX) const;

    // Picking: screen ray vs object mesh (3D mode).
    // Returns pointer to topmost hit or nullptr.
    ST::GameObject* pickObject3D(int canvasX, int canvasY, int canvasW, int canvasH);

    // Reset the orbit camera to a default 3D view that frames the origin.
    void resetOrbitCamera();

    ST::FrameBuffer* m_frameBuffer;
    ST::FrameBuffer* m_ssaaBuffer;
    ST::DepthBuffer* m_depthBuffer;
    ST::DepthBuffer* m_ssaaDepthBuffer;
    ST::Rasterizer m_rasterizer;
    ST::VertexShader m_vertexShader;
    ST::Scene m_scene;
    ST::Sprite2DUI m_transformUI;
    ST::MusicPlayerUI m_musicPlayerUI;
    ST::CinemachineUI m_cinemachineUI;
    ST::OrbitCameraController m_orbit;
    int m_canvasW;
    int m_canvasH;
    int m_canvasTop;

    float m_zoom;
    float m_panX;
    float m_panY;
    float m_gridSize;
    bool m_dragging;
    int m_lastMouseX, m_lastMouseY;
    bool m_showGrid;

    int m_viewportMode; // 0=Scene, 1=Game
    bool m_gameRunning;
    int m_gameResolutionIndex;

    bool m_ssaaEnabled;
    int m_ssaaLevel;
    int m_aaMode;

    ST::GameObject* m_selected;
    bool m_objDragging;
    int m_objDragStartX, m_objDragStartY;
    float m_objDragStartObjX, m_objDragStartObjY;
    float m_objDragStartWX, m_objDragStartWY;

    std::vector<SDL_Texture*> m_thumbnails;
    SDL_Renderer* m_renderer = nullptr;
    std::string m_lastSavePath;

    // ---- 3D view additions ----
    bool m_view3D = false;                  // when true, render uses Scene.camera + orbit controller
    bool m_orbitDragging = false;           // RMB held -> orbit
    int  m_orbitLastX = 0;
    int  m_orbitLastY = 0;
    std::unordered_map<int, bool> m_keyHeld;// key state, polled in update()
    int  m_createType = 0;
};
