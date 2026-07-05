#include "TestModule_NetworkTest.hpp"
#include "../Buffer/FrameBuffer.hpp"
#include "../Buffer/DepthBuffer.hpp"
#include "../Pipeline/VertexShader.hpp"
#include "../Pipeline/Rasterizer.hpp"
#include <iostream>
#include <imgui.h>
#include <SDL2/SDL.h>

namespace {
    constexpr int CANVAS_W = 640;
    constexpr int CANVAS_H = 480;
}

TestModule_NetworkTest::TestModule_NetworkTest()
    : m_initialized(false)
    , m_selectedTab(0)
    , m_maxPlayers(4)
    , m_hostStarted(false)
    , m_clientStarted(false)
    , m_selectedRoom(-1)
    , m_statusMessage("Not connected")
    , m_peerConnected(false)
{
    m_frameBuffer.initialize(CANVAS_W, CANVAS_H);
    m_depthBuffer.initialize(CANVAS_W, CANVAS_H);
    m_initialized = true;
}

TestModule_NetworkTest::~TestModule_NetworkTest() {
    ST::NetworkDiscovery::instance().stopHost();
    ST::NetworkDiscovery::instance().stopClient();
}

void TestModule_NetworkTest::render(void* canvasTexture, int canvasW, int canvasH) {
    SDL_Renderer* renderer = reinterpret_cast<SDL_Renderer*>(canvasTexture);
    if (!renderer) return;

    if (canvasW == 0 || canvasH == 0) return;

    if (canvasW != (int)m_frameBuffer.getWidth() || canvasH != (int)m_frameBuffer.getHeight()) {
        m_frameBuffer.initialize(canvasW, canvasH);
        m_depthBuffer.initialize(canvasW, canvasH);
    }

    m_frameBuffer.clear(ST::Color(0.08f, 0.08f, 0.10f, 1.0f));

    // Draw a centered status text as pixels (simple approach)
    renderCanvas(renderer, canvasW, canvasH);
}

void TestModule_NetworkTest::renderCanvas(SDL_Renderer* renderer, int canvasW, int canvasH) {
    const auto& pixels = m_frameBuffer.getPixels();
    std::vector<uint32_t> rgba32(static_cast<size_t>(canvasW) * canvasH);
    for (int i = 0; i < canvasW * canvasH; i++) {
        uint8_t r = static_cast<uint8_t>(std::min(255.0f, pixels[i].r * 255.0f));
        uint8_t g = static_cast<uint8_t>(std::min(255.0f, pixels[i].g * 255.0f));
        uint8_t b = static_cast<uint8_t>(std::min(255.0f, pixels[i].b * 255.0f));
        uint8_t a = static_cast<uint8_t>(std::min(255.0f, pixels[i].a * 255.0f));
        rgba32[i] = (r << 0) | (g << 8) | (b << 16) | (a << 24);
    }

    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC, canvasW, canvasH);
    SDL_UpdateTexture(tex, nullptr, rgba32.data(), canvasW * sizeof(uint32_t));
    SDL_RenderCopy(renderer, tex, nullptr, nullptr);
    SDL_DestroyTexture(tex);
}

bool TestModule_NetworkTest::renderControls() {
    ImGui::SetNextWindowSize(ImVec2(360, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Network Test", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::End();
        return false;
    }

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "=== ST Render Network ===");
    ImGui::Spacing();

    // Tabs: Host / Client
    const char* tabs[] = { "Host", "Client" };
    ImGui::SetNextItemWidth(200);
    ImGui::Combo("##tab", &m_selectedTab, tabs, 2);
    ImGui::Spacing();

    if (m_selectedTab == 0) {
        renderHostPanel();
    } else {
        renderClientPanel();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Status line
    ImGui::Text("Status: ");
    ImGui::SameLine();
    ImColor statusColor(0.6f, 0.9f, 0.6f, 1.0f);
    if (m_peerConnected) {
        ImGui::TextColored(statusColor, "Peer connected!");
    } else if (m_hostStarted || m_clientStarted) {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "%s", m_statusMessage.c_str());
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Idle");
    }

    ImGui::End();
    return false;
}

void TestModule_NetworkTest::renderHostPanel() {
    ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.0f), "--- Host Mode ---");

    if (!m_hostStarted) {
        ImGui::Text("Room Name:");
        ImGui::SetNextItemWidth(220);
        ImGui::InputText("##roomname", m_roomName, sizeof(m_roomName));

        ImGui::Text("Max Players:");
        ImGui::SetNextItemWidth(100);
        ImGui::SliderInt("##maxplayers", &m_maxPlayers, 2, 8);

        ImGui::Spacing();
        if (ImGui::Button("Start Host", ImVec2(200, 0))) {
            bool ok = ST::NetworkDiscovery::instance().startHost(m_roomName, (uint8_t)m_maxPlayers);
            if (ok) {
                m_hostStarted = true;
                m_statusMessage = "Broadcasting room...";
            } else {
                m_statusMessage = "Failed to start host!";
            }
        }
    } else {
        auto& net = ST::NetworkDiscovery::instance();

        ImGui::Text("Room: %s", m_roomName);
        ImGui::Text("Players: %d / %d", net.getCurrentPlayers(), net.getMaxPlayers());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("Connected Players:");
        ImGui::Spacing();

        const auto& clients = net.getClients();
        if (clients.empty()) {
            ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.4f, 1.0f), "  Waiting for players...");
        } else {
            for (int i = 0; i < (int)clients.size(); ++i) {
                const auto& c = clients[i];
                ImGui::Text("  [%d] %s:%d", i + 1, c.ipAddress.c_str(), (int)c.port);
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Note: acceptPeer() and processIncomingPeerMsg() are called inside updateHost()
        // — no need to call them here. Also do NOT call recvFromPeer() on the Host's
        // m_peerSocket; that would steal messages from Client's socket.

        ImGui::Spacing();
        if (!clients.empty() && ImGui::Button("Kick All", ImVec2(200, 0))) {
            net.disconnect();
        }
        if (ImGui::Button("Stop Host", ImVec2(200, 0))) {
            net.stopHost();
            m_hostStarted = false;
            m_peerConnected = false;
            m_statusMessage = "Not connected";
        }
    }
}

void TestModule_NetworkTest::renderClientPanel() {
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.9f, 1.0f), "--- Client Mode ---");

    if (!m_clientStarted) {
        if (ImGui::Button("Start Scanning", ImVec2(200, 0))) {
            bool ok = ST::NetworkDiscovery::instance().startClient();
            if (ok) {
                m_clientStarted = true;
                m_statusMessage = "Scanning...";
            } else {
                m_statusMessage = "Failed to start client!";
            }
        }
    } else {
        if (!ST::NetworkDiscovery::instance().isClient()) {
            ST::NetworkDiscovery::instance().startClient();
        }

        // Refresh
        if (ImGui::Button("Refresh", ImVec2(90, 0))) {
            ST::NetworkDiscovery::instance().clearStaleRooms();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop", ImVec2(90, 0))) {
            ST::NetworkDiscovery::instance().stopClient();
            m_clientStarted = false;
            m_selectedRoom = -1;
            m_peerConnected = false;
            m_statusMessage = "Not connected";
            ImGui::End();
            return;
        }

        ImGui::Spacing();

        const auto& rooms = ST::NetworkDiscovery::instance().getRooms();

        if (rooms.empty()) {
            ImGui::TextDisabled("No rooms found...");
        } else {
            ImGui::Text("Available Rooms (%zu):", rooms.size());
            ImGui::Spacing();

            for (int i = 0; i < (int)rooms.size() && i < MAX_DISPLAY_ROOMS; i++) {
                const auto& r = rooms[i];
                ImGui::PushID(i);
                bool selected = (i == m_selectedRoom);
                std::string label = r.name + "  |  " + r.hostIP;
                if (ImGui::Selectable(label.c_str(), selected, 0, ImVec2(220, 0))) {
                    m_selectedRoom = i;
                }
                ImGui::SameLine();
                ImGui::Text("  [%d/%d]", r.playerCount, r.maxPlayers);
                ImGui::PopID();
            }
        }

        ImGui::Spacing();
        if (m_selectedRoom >= 0 && m_selectedRoom < (int)rooms.size()) {
            const auto& room = rooms[m_selectedRoom];
            std::string label = "Join ";
            label += room.name;
            if (ImGui::Button(label.c_str(), ImVec2(200, 0))) {
                bool ok = ST::NetworkDiscovery::instance().connectToRoom(m_selectedRoom);
                if (ok) {
                    m_peerConnected = true;
                    m_statusMessage = "Connected!";
                    std::string hello = "Hello from client!";
                    ST::NetworkDiscovery::instance().sendToPeer(hello.c_str(), hello.size());
                } else {
                    m_statusMessage = "Connection failed!";
                }
            }
        }

        // Show incoming messages and player count
        if (m_peerConnected) {
            auto& net = ST::NetworkDiscovery::instance();
            ImGui::TextColored(ImVec4(0.6f, 0.9f, 1.0f, 1.0f),
                "Room: %d / %d players", net.getCurrentPlayers(), net.getMaxPlayers());

            ImGui::Spacing();
            if (ImGui::Button("Disconnect", ImVec2(200, 0))) {
                net.disconnect();
                m_peerConnected = false;
                m_statusMessage = "Scanning...";
            }
        }
    }
}

void TestModule_NetworkTest::renderOverlays() {
    // No overlays needed
}

void TestModule_NetworkTest::renderUIOverlay(int canvasX, int canvasY, int canvasW, int canvasH) {
    (void)canvasX; (void)canvasY; (void)canvasW; (void)canvasH;
    // Canvas is driven by the render() method
}

void TestModule_NetworkTest::update(float deltaTime) {
    ST::NetworkDiscovery::instance().update(deltaTime);
}

void TestModule_NetworkTest::setRenderer(void* renderer) {
    m_renderer = reinterpret_cast<SDL_Renderer*>(renderer);
}

void TestModule_NetworkTest::onKeyDown(int keycode) {
    (void)keycode;
    // Future: send key events over network
}
