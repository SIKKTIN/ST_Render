#pragma once

#include "ITestModule.hpp"
#include "engine/editor/Sprite2D.hpp"
#include "engine/editor/GameObject.hpp"
#include "engine/editor/Sprite2DUI.hpp"
#include "renderer/buffer/FrameBuffer.hpp"
#include "renderer/pipeline/Rasterizer.hpp"
#include "renderer/pipeline/VertexShader.hpp"
#include "core/math/Matrix4x4.hpp"
#include <SDL2/SDL.h>
#include <imgui.h>
#include <vector>

enum class SpawnType { GameObject, Sprite2D };

struct SpawnEntry {
    ST::GameObject* obj;
    SpawnType type;
};

class TestModule_Sprite2D : public ITestModule {
public:
    TestModule_Sprite2D();
    ~TestModule_Sprite2D();
    const char* getName() const override { return "Sprite2D"; }
    bool needsRealTimeUpdate() const override { return true; }

    void render(void* canvasTexture, int canvasW, int canvasH) override;
    bool renderControls() override;
    void renderCreatePanel() override;
    void renderOverlays() override;
    void runConsole(std::string& output) override;

    void onMouseMove(int x, int y) override;
    void onMouseDown(int button, int x, int y) override;
    void onMouseUp(int button) override;

private:
    void screenToWorld(int sx, int sy, int cw, int ch, float& wx, float& wy);
    ST::Matrix4x4 buildModelMatrix(const ST::GameObject* obj) const;
    bool isInCanvas(int x, int y, int cw, int ch) const;
    void renderObject(const ST::GameObject* obj, ST::Color fillColor, bool selected);
    bool hitTest(const ST::GameObject* obj, float wx, float wy) const;
    void spawnAt(float wx, float wy);
    void buildThumbnails(SDL_Renderer* renderer);
    void setRenderer(void* renderer) override;

    std::vector<SpawnEntry> m_objects;
    int m_selectedIdx;
    SpawnType m_spawnType;
    ST::FrameBuffer* m_fb;
    ST::Rasterizer m_rasterizer;
    ST::VertexShader m_vs;
    int m_canvasW;
    int m_canvasH;
    float m_zoom;
    float m_panX;
    float m_panY;
    bool m_dragging;
    int m_lastX, m_lastY;
    int m_rbtnDownX, m_rbtnDownY;
    bool m_rbtnDown;
    int m_dragObjIdx;
    float m_dragObjOX, m_dragObjOY;
    ST::Sprite2DUI m_transformUI;
    std::vector<SDL_Texture*> m_thumbnails;
    SDL_Renderer* m_renderer = nullptr;
};
