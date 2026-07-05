#pragma once

#include <SDL2/SDL.h>
#include "ITestModule.hpp"
#include "../Buffer/FrameBuffer.hpp"
#include "../Buffer/DepthBuffer.hpp"
#include "../Pipeline/VertexShader.hpp"
#include "../Pipeline/Rasterizer.hpp"
#include "../Geometry/Vertex.hpp"
#include "../Geometry/Mesh.hpp"
#include "../Math/Matrix4x4.hpp"
#include "../Editor/Scene.hpp"
#include "../Editor/GameObject.hpp"
#include "../Editor/ITransformUI.hpp"
#include "../Editor/Sprite2DUI.hpp"
#include "../Editor/Sprite2D.hpp"
#include "../Editor/MusicPlayerUI.hpp"
#include "../Editor/AudioManager.hpp"
#include "../Editor/CinemachineUI.hpp"
#include "../Data/DataBase/SceneData.hpp"
#include <string>

class TestModule_2D_Scene : public ITestModule {
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
};
