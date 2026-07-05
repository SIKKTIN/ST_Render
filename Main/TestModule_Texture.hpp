#pragma once

#include <SDL2/SDL.h>
#include <imgui.h>
#include "ITestModule.hpp"
#include "../Buffer/FrameBuffer.hpp"
#include "../Buffer/DepthBuffer.hpp"
#include "../Buffer/Texture2D.hpp"
#include "../Pipeline/VertexShader.hpp"
#include "../Pipeline/Rasterizer.hpp"
#include "../Geometry/Vertex.hpp"
#include "../Geometry/Mesh.hpp"
#include "../Math/Matrix4x4.hpp"
#include "../Editor/GameObject.hpp"
#include "../Editor/Scene.hpp"
#include <vector>
#include <string>

struct ScannedTexture {
    std::string filename;
    std::string fullPath;
};

class TestModule_Texture : public ITestModule {
public:
    TestModule_Texture();
    ~TestModule_Texture();

    const char* getName() const override { return "Texture Test"; }
    bool needsRealTimeUpdate() const override { return true; }

    void onMouseMove(int x, int y) override;
    void onMouseDown(int button, int x, int y) override;
    void onMouseUp(int button) override;
    void onWheel(float dx, float dy, int canvasX, int canvasY, int canvasW, int canvasH) override;

    void render(void* canvasTexture, int canvasW, int canvasH) override;
    bool renderControls() override;
    void renderOverlays() override;
    void runConsole(std::string& output) override;
    void setRenderer(void* renderer) override;

private:
    void rebuildBuffers(int canvasW, int canvasH);
    void scanResourceFolder();
    void loadTextureFromPath(const std::string& path);
    void buildThumbnails(SDL_Renderer* renderer);

    ST::FrameBuffer* m_frameBuffer;
    ST::DepthBuffer* m_depthBuffer;
    ST::Texture2D* m_texture;
    ST::Rasterizer m_rasterizer;
    ST::VertexShader m_vertexShader;
    int m_canvasW;
    int m_canvasH;

    ST::Mesh m_quadMesh;
    ST::Texture2D m_quadTexture;
    ST::Color m_tint;
    float m_zoom;
    float m_rotation;

    std::vector<ScannedTexture> m_scannedTextures;
    std::vector<SDL_Texture*> m_thumbnails;
    SDL_Renderer* m_renderer;
    int m_selectedTextureIdx;
    bool m_showTextureBrowser;
    ImVec2 m_browserSize;
    bool m_browserPosSet;
};
