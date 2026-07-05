#pragma once

#include <SDL2/SDL.h>
#include "app/module/IModule.hpp"
#include "renderer/buffer/FrameBuffer.hpp"
#include "renderer/buffer/DepthBuffer.hpp"
#include "renderer/pipeline/VertexShader.hpp"
#include "renderer/pipeline/Rasterizer.hpp"
#include "core/math/Matrix4x4.hpp"
#include "engine/editor/NetWork/NetworkDiscovery.hpp"

class TestModule_NetworkTest : public IModule {
public:
    TestModule_NetworkTest();
    ~TestModule_NetworkTest();

    const char* getName() const override { return "Network Test"; }
    bool hasRenderOutput() const override { return true; }
    bool needsRealTimeUpdate() const override { return true; }

    void render(void* canvasTexture, int canvasW, int canvasH) override;
    bool renderControls() override;
    void renderOverlays() override;
    void renderUIOverlay(int canvasX, int canvasY, int canvasW, int canvasH) override;
    void setRenderer(void* renderer) override;
    void update(float deltaTime) override;

    void onKeyDown(int keycode) override;

private:
    void renderCanvas(SDL_Renderer* renderer, int canvasW, int canvasH);
    void renderHostPanel();
    void renderClientPanel();

    SDL_Renderer* m_renderer = nullptr;
    ST::FrameBuffer m_frameBuffer;
    ST::DepthBuffer m_depthBuffer;
    bool m_initialized = false;

    // UI state
    int m_selectedTab = 0; // 0=Host, 1=Client

    // Host UI
    char m_roomName[64] = "My Room";
    int m_maxPlayers = 4;
    bool m_hostStarted = false;

    // Client UI
    bool m_clientStarted = false;
    int m_selectedRoom = -1;

    // Room list display
    static constexpr int MAX_DISPLAY_ROOMS = 16;

    // Connection status
    std::string m_statusMessage;
    bool m_peerConnected = false;
};
