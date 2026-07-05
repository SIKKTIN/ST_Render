#include "TestModule_3DRender.hpp"
#include <imgui.h>
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
// Map raw RGBA float pixels into the ABGR byte order that
// SDL_PIXELFORMAT_RGBA32 expects on this platform.
inline uint32_t packRGBA(const ST::Color& c) {
    uint8_t r = static_cast<uint8_t>(std::min(255.0f, c.r * 255.0f));
    uint8_t g = static_cast<uint8_t>(std::min(255.0f, c.g * 255.0f));
    uint8_t b = static_cast<uint8_t>(std::min(255.0f, c.b * 255.0f));
    uint8_t a = static_cast<uint8_t>(std::min(255.0f, c.a * 255.0f));
    return (r << 0) | (g << 8) | (b << 16) | (a << 24);
}
}

TestModule_3DRender::TestModule_3DRender()
    : m_frameBuffer(nullptr)
    , m_depthBuffer(nullptr)
    , m_orbitYaw(0.6f)        // look slightly to the right
    , m_orbitPitch(0.35f)     // and slightly down
    , m_orbitDistance(3.0f)
    , m_rmbDown(false)
    , m_lastCanvasX(0)
    , m_lastCanvasY(0)
    , m_canvasW(640)
    , m_canvasH(480)
{
    m_cube = ST::Mesh::createCube(1.0f); // unit cube, edge length 1, centered at origin
}

TestModule_3DRender::~TestModule_3DRender() {
    delete m_frameBuffer;
    delete m_depthBuffer;
}

void TestModule_3DRender::rebuildBuffers(int canvasW, int canvasH) {
    delete m_frameBuffer;
    delete m_depthBuffer;
    m_frameBuffer = new ST::FrameBuffer();
    m_frameBuffer->initialize(canvasW, canvasH);
    m_depthBuffer = new ST::DepthBuffer();
    m_depthBuffer->initialize(canvasW, canvasH);
    m_rasterizer.setBuffers(m_frameBuffer, m_depthBuffer);
}

void TestModule_3DRender::drawMesh(const ST::Mesh& mesh,
                                   const ST::Matrix4x4& model,
                                   const ST::Matrix4x4& view,
                                   const ST::Matrix4x4& projection)
{
    ST::Uniform u;
    u.modelMatrix = model;
    u.viewMatrix = view;
    u.projectionMatrix = projection;
    m_vertexShader.setUniform(u);

    const auto& verts = mesh.getVertices();
    const auto& idx = mesh.getIndices();
    for (int i = 0; i + 2 < (int)idx.size(); i += 3) {
        ST::VertexOut v0 = m_vertexShader.process(verts[idx[i + 0]]);
        ST::VertexOut v1 = m_vertexShader.process(verts[idx[i + 1]]);
        ST::VertexOut v2 = m_vertexShader.process(verts[idx[i + 2]]);

        // Simple back-face culling: skip triangles whose screen-space area is non-positive.
        // (We use raw NDC signs after perspective divide.)
        ST::Vector3 n0 = v0.toNDC();
        ST::Vector3 n1 = v1.toNDC();
        ST::Vector3 n2 = v2.toNDC();
        float cross = (n1.x - n0.x) * (n2.y - n0.y) - (n2.x - n0.x) * (n1.y - n0.y);
        if (cross >= 0.0f) continue; // back-facing or degenerate

        if (!m_vertexShader.isInFrustum(v0)) continue;
        if (!m_vertexShader.isInFrustum(v1)) continue;
        if (!m_vertexShader.isInFrustum(v2)) continue;

        auto frag = [](const ST::VertexOut& f) {
            // Flat color with simple lambert-ish tint based on world-space normal vs camera direction.
            ST::Color base = f.color;
            return base;
        };
        m_rasterizer.rasterizeTriangle(v0, v1, v2, frag);
    }
}

void TestModule_3DRender::update(float /*deltaTime*/) {
    // Cube spins slowly on Y axis so the user can see all sides without dragging.
    // (Kept static -- uncomment the rotation block below to auto-spin the cube.)
}

void TestModule_3DRender::render(void* canvasTexture, int canvasW, int canvasH) {
    if (canvasW == 0 || canvasH == 0) return;

    if (m_frameBuffer == nullptr || m_canvasW != canvasW || m_canvasH != canvasH) {
        rebuildBuffers(canvasW, canvasH);
        m_canvasW = canvasW;
        m_canvasH = canvasH;
    }

    m_frameBuffer->clear(ST::Color(0.08f, 0.09f, 0.12f, 1.0f));
    m_depthBuffer->clear();

    // ---- Camera (orbit) ----
    float cp = std::cos(m_orbitPitch);
    float sp = std::sin(m_orbitPitch);
    float cy = std::cos(m_orbitYaw);
    float sy = std::sin(m_orbitYaw);

    ST::Vector3 eye(
        m_orbitDistance * cp * sy,
        m_orbitDistance * sp,
        m_orbitDistance * cp * cy
    );
    ST::Vector3 target(0.0f, 0.0f, 0.0f);
    ST::Vector3 up(0.0f, 1.0f, 0.0f);
    ST::Matrix4x4 view = ST::Matrix4x4::lookAt(eye, target, up);

    float aspect = static_cast<float>(canvasW) / static_cast<float>(canvasH);
    ST::Matrix4x4 projection = ST::Matrix4x4::perspective(
        static_cast<float>(M_PI) / 3.0f, // 60 degrees vertical FOV
        aspect,
        0.1f,
        100.0f
    );

    ST::Matrix4x4 model = ST::Matrix4x4::identity();

    m_rasterizer.setUseRawScreenCoords(false);
    drawMesh(m_cube, model, view, projection);

    // ---- Upload to SDL texture ----
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(canvasTexture);
    const auto& pixels = m_frameBuffer->getPixels();

    std::vector<uint32_t> rgba32(canvasW * canvasH);
    for (int i = 0; i < canvasW * canvasH; ++i) {
        rgba32[i] = packRGBA(pixels[i]);
    }

    SDL_Texture* tex = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        canvasW, canvasH);
    SDL_UpdateTexture(tex, nullptr, rgba32.data(), canvasW * sizeof(uint32_t));
    SDL_RenderCopy(renderer, tex, nullptr, nullptr);
    SDL_DestroyTexture(tex);
}

bool TestModule_3DRender::renderControls() {
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Camera (Orbit)");
    ImGui::Separator();

    ImGui::SliderFloat("Yaw",   &m_orbitYaw,   -3.14f, 3.14f);
    ImGui::SliderFloat("Pitch", &m_orbitPitch, -1.5f,  1.5f);
    ImGui::SliderFloat("Distance", &m_orbitDistance, 1.5f, 10.0f);

    if (ImGui::Button("Reset Camera")) {
        m_orbitYaw = 0.6f;
        m_orbitPitch = 0.35f;
        m_orbitDistance = 3.0f;
    }
    needsRerender = true;

    ImGui::Separator();
    ImGui::TextWrapped("RMB drag in canvas to orbit. Wheel to zoom.");
    return true;
}

void TestModule_3DRender::onCanvasMouseDown(int button, int canvasX, int canvasY) {
    if (button == SDL_BUTTON_RIGHT) {
        m_rmbDown = true;
        m_lastCanvasX = canvasX;
        m_lastCanvasY = canvasY;
    }
}

void TestModule_3DRender::onCanvasMouseUp(int button, int /*canvasX*/, int /*canvasY*/) {
    if (button == SDL_BUTTON_RIGHT) {
        m_rmbDown = false;
    }
}

void TestModule_3DRender::onCanvasMouseMove(int canvasX, int canvasY) {
    if (!m_rmbDown) return;
    int dx = canvasX - m_lastCanvasX;
    int dy = canvasY - m_lastCanvasY;
    m_lastCanvasX = canvasX;
    m_lastCanvasY = canvasY;

    const float sens = 0.01f;
    m_orbitYaw   -= dx * sens;
    m_orbitPitch -= dy * sens;
    if (m_orbitPitch >  1.5f) m_orbitPitch =  1.5f;
    if (m_orbitPitch < -1.5f) m_orbitPitch = -1.5f;
    needsRerender = true;
}

void TestModule_3DRender::onWheel(float /*dx*/, float dy,
                                  int /*canvasX*/, int /*canvasY*/,
                                  int /*canvasW*/, int /*canvasH*/)
{
    m_orbitDistance *= (dy > 0) ? 0.9f : 1.1f;
    if (m_orbitDistance < 1.2f) m_orbitDistance = 1.2f;
    if (m_orbitDistance > 20.0f) m_orbitDistance = 20.0f;
    needsRerender = true;
}