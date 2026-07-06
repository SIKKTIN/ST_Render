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
    // Initial camera matches the previous orbit camera's eye position
    // (yaw=0.6, pitch=0.35, distance=3 around the origin), so the first
    // rendered frame looks identical.
    , m_eye(1.6f, 1.0f, 2.5f)
    , m_yaw(0.6f)
    , m_pitch(0.35f)
    , m_moveSpeed(2.5f)
    , m_moveSpeedMin(0.25f)
    , m_moveSpeedMax(20.0f)
    , m_lmbDown(false)
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

        // Back-face culling in world space. An earlier attempt used a 2D
        // cross of NDC screen-space coordinates, which silently culled the
        // front faces because NDC and screen-Y conventions differ (the
        // rasterizer flips Y in toScreenSpace). Cross of two world-space
        // edges gives the face normal; if it points away from the eye
        // (dot <= 0) the face is back-facing.
        ST::Vector3 faceNormal = (v1.worldPosition - v0.worldPosition)
                                    .cross(v2.worldPosition - v0.worldPosition);
        ST::Vector3 toEye = m_eye - v0.worldPosition;
        if (faceNormal.dot(toEye) <= 0.0f) continue;

        // No per-triangle frustum culling here. Earlier we used
        // vertexShader.isInFrustum on all 3 vertices (kept only if all 3 inside),
        // which silently dropped triangles that straddled the near plane or the
        // edges of the viewport while the camera was close to the cube -- the
        // whole face disappeared even though it should have been partially
        // visible. Without a real Sutherland-Hodgman clipper in the rasterizer
        // the cheapest fix is to skip the coarse reject and let the per-pixel
        // depth test + the rasterizer's screen-bounds clamp handle boundary
        // cases.

        auto frag = [](const ST::VertexOut& f) {
            // Flat color with simple lambert-ish tint based on world-space normal vs camera direction.
            ST::Color base = f.color;
            return base;
        };
        m_rasterizer.rasterizeTriangle(v0, v1, v2, frag);
    }
}

void TestModule_3DRender::update(float deltaTime) {
    // Only sample keyboard for camera movement while the canvas has focus and
    // ImGui isn't asking for text input -- otherwise WASD would bleed into
    // text fields.
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    if (!m_rmbDown) return; // UE editor: WASD only flies while RMB is held

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    float speed = m_moveSpeed * (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT] ? 3.0f : 1.0f);
    float move = speed * deltaTime;
    if (move <= 0.0f) return;

    float cp = std::cos(m_pitch);
    float sp = std::sin(m_pitch);
    float cy = std::cos(m_yaw);
    float sy = std::sin(m_yaw);

    // forward = (target - eye).normalized -- the direction the camera looks.
    // The previous orbit camera used eye = (dist·cp·sy, dist·sp, dist·cp·cy) with
    // target = origin; the equivalent look direction is (-cp·sy, -sp, -cp·cy).
    // We keep the same convention here so initial state matches.
    ST::Vector3 forward(-cp * sy, -sp, -cp * cy);
    ST::Vector3 up_world(0.0f, 1.0f, 0.0f);
    // lookAt uses right = up × (eye - target) = up × (-forward); equivalent
    // is forward × up, so use the same sign convention as the view matrix.
    ST::Vector3 right = forward.cross(up_world).normalized();

    bool moved = false;
    if (keys[SDL_SCANCODE_W]) { m_eye += forward * move; moved = true; }
    if (keys[SDL_SCANCODE_S]) { m_eye -= forward * move; moved = true; }
    if (keys[SDL_SCANCODE_D]) { m_eye += right   * move; moved = true; }
    if (keys[SDL_SCANCODE_A]) { m_eye -= right   * move; moved = true; }
    if (keys[SDL_SCANCODE_E]) { m_eye += up_world * move; moved = true; }
    if (keys[SDL_SCANCODE_Q]) { m_eye -= up_world * move; moved = true; }

    if (moved) needsRerender = true;
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

    // ---- Camera (fly, UE-style) ----
    // m_eye is the world-space camera position; yaw/pitch define the forward
    // direction. We rebuild a target one unit in front of the eye and reuse
    // Matrix4x4::lookAt (no need for a separate lookTo API).
    float cp = std::cos(m_pitch);
    float sp = std::sin(m_pitch);
    float cy = std::cos(m_yaw);
    float sy = std::sin(m_yaw);

    // forward = (target - eye).normalized -- the direction the camera looks.
    // The previous orbit camera used eye = (dist·cp·sy, dist·sp, dist·cp·cy) with
    // target = origin; the equivalent look direction is (-cp·sy, -sp, -cp·cy).
    ST::Vector3 forward(-cp * sy, -sp, -cp * cy);
    ST::Vector3 eye    = m_eye;
    ST::Vector3 target = m_eye + forward;
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
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Camera (Fly, UE-style)");
    ImGui::Separator();

    ImGui::SliderFloat("Yaw",   &m_yaw,   -6.28f, 6.28f);
    ImGui::SliderFloat("Pitch", &m_pitch, -1.5f,  1.5f);

    ImGui::DragFloat3("Eye", &m_eye.x, 0.05f);

    ImGui::SliderFloat("Speed",      &m_moveSpeed, m_moveSpeedMin, m_moveSpeedMax, "%.2f u/s");
    if (ImGui::Button("Reset Camera")) {
        m_eye    = ST::Vector3(1.6f, 1.0f, 2.5f);
        m_yaw    = 0.6f;
        m_pitch  = 0.35f;
        m_moveSpeed = 2.5f;
    }
    needsRerender = true;

    ImGui::Separator();
    ImGui::BulletText("LMB drag in canvas to look.");
    ImGui::BulletText("RMB drag also looks; hold RMB + WASD/QE to fly.");
    ImGui::BulletText("Shift = sprint, Wheel = change speed.");
    return true;
}

void TestModule_3DRender::onMouseDown(int button, int x, int y) {
    if (button == SDL_BUTTON_LEFT) {
        m_lmbDown = true;
        m_lastCanvasX = x;
        m_lastCanvasY = y;
    } else if (button == SDL_BUTTON_RIGHT) {
        m_rmbDown = true;
        m_lastCanvasX = x;
        m_lastCanvasY = y;
    }
}

void TestModule_3DRender::onMouseUp(int button) {
    if (button == SDL_BUTTON_LEFT) {
        m_lmbDown = false;
    } else if (button == SDL_BUTTON_RIGHT) {
        m_rmbDown = false;
    }
}

void TestModule_3DRender::onMouseMove(int x, int y) {
    // Both LMB and RMB dragging rotate the view -- matches UE.
    if (!m_lmbDown && !m_rmbDown) return;
    int dx = x - m_lastCanvasX;
    int dy = y - m_lastCanvasY;
    m_lastCanvasX = x;
    m_lastCanvasY = y;

    const float sens = 0.01f;
    m_yaw -= dx * sens;
    if (m_lmbDown) {
        // LMB keeps the existing pitch orientation.
        m_pitch -= dy * sens;
    } else if (m_rmbDown) {
        // RMB flying: invert vertical so drag-up looks down (UE editor default
        // for RMB-look).
        m_pitch += dy * sens;
    }
    if (m_pitch >  1.5f) m_pitch =  1.5f;
    if (m_pitch < -1.5f) m_pitch = -1.5f;
    needsRerender = true;
}

void TestModule_3DRender::onWheel(float /*dx*/, float dy,
                                  int /*canvasX*/, int /*canvasY*/,
                                  int /*canvasW*/, int /*canvasH*/)
{
    // Wheel controls movement speed (UE editor model). One notch ~ 10% step.
    float factor = (dy > 0) ? 1.1f : 0.9f;
    m_moveSpeed = std::clamp(m_moveSpeed * factor, m_moveSpeedMin, m_moveSpeedMax);
    needsRerender = true;
}

// Canvas-space variants get the same treatment -- the rotate logic only
// cares about deltas, so screen-space (x,y) and canvas-space (cx,cy) are
// interchangeable here.
void TestModule_3DRender::onCanvasMouseDown(int button, int canvasX, int canvasY) {
    onMouseDown(button, canvasX, canvasY);
}
void TestModule_3DRender::onCanvasMouseUp(int button, int canvasX, int canvasY) {
    (void)canvasX; (void)canvasY;
    onMouseUp(button);
}
void TestModule_3DRender::onCanvasMouseMove(int canvasX, int canvasY) {
    onMouseMove(canvasX, canvasY);
}