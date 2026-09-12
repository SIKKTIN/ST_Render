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
    , m_material(ST::Material::defaultMaterial())
    , m_light(ST::Light::directional(ST::Vector3(-0.4f, -1.0f, -0.6f), ST::Color::white(), 1.4f))
    , m_ambientLight(0.12f, 0.12f, 0.12f)
    , m_lightingEnabled(true)
{
    m_cube = ST::Mesh::createCube(1.0f); // unit cube, edge length 1, centered at origin
    m_builtinShader = std::make_shared<ST::BuiltinShaderProgram>(m_vertexShader, m_fragmentShader);
    m_activeShader = m_builtinShader;
    scanShaderCatalog();
    if (m_selectedShaderIndex >= 0) loadSelectedShader();
    scanModelCatalog();
}

TestModule_3DRender::~TestModule_3DRender() {
    if (m_outputTexture) {
        SDL_DestroyTexture(m_outputTexture);
        m_outputTexture = nullptr;
    }
    m_sdlRenderer = nullptr;
    m_outputTextureW = 0;
    m_outputTextureH = 0;
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

void TestModule_3DRender::scanShaderCatalog() {
    const std::string previousPath =
        (m_selectedShaderIndex >= 0 &&
         m_selectedShaderIndex < static_cast<int>(m_shaderCatalog.getEntries().size()))
            ? m_shaderCatalog.getEntries()[m_selectedShaderIndex].relativePath
            : std::string();

    std::string error;
    if (!m_shaderCatalog.scan(m_shaderRoot, error)) {
        // When launched from a build/bin directory, the source tree is two
        // levels above the executable. The deployed copy remains the first
        // choice, so this fallback is only for development runs.
        const std::string fallback = "../../Data/Shaders";
        if (!m_shaderCatalog.scan(fallback, error)) {
            m_selectedShaderIndex = -1;
            m_shaderError = error;
            return;
        }
        m_shaderRoot = fallback;
    }

    m_selectedShaderIndex = m_shaderCatalog.findByRelativePath(previousPath);
    if (m_selectedShaderIndex < 0 && !m_shaderCatalog.getEntries().empty()) {
        m_selectedShaderIndex = 0;
    }
    m_shaderError.clear();
}

void TestModule_3DRender::loadSelectedShader() {
    const auto& entries = m_shaderCatalog.getEntries();
    if (m_selectedShaderIndex < 0 || m_selectedShaderIndex >= static_cast<int>(entries.size())) {
        m_shaderManager.clear();
        m_activeShader = m_builtinShader;
        return;
    }

    std::string error;
    if (m_shaderManager.load(entries[m_selectedShaderIndex].absolutePath, error)) {
        m_activeShader = m_shaderManager.getProgram();
        m_shaderError.clear();
    } else {
        // Keep the previous valid program active when the selected script is
        // invalid. This also makes switching back to the built-in path safe.
        m_shaderError = error;
    }
}

bool TestModule_3DRender::selectShaderIndex(int index) {
    if (index < 0 || index >= static_cast<int>(m_shaderCatalog.getEntries().size())) return false;
    m_selectedShaderIndex = index;
    loadSelectedShader();
    needsRerender = true;
    return m_shaderError.empty();
}

void TestModule_3DRender::pollShaderReload() {
    if (!m_shaderManager.hasSourceFile()) return;
    std::string error;
    if (m_shaderManager.reloadIfChanged(error)) {
        m_activeShader = m_shaderManager.getProgram();
        m_shaderError.clear();
        needsRerender = true;
    } else if (!error.empty()) {
        m_shaderError = error;
    }
}

void TestModule_3DRender::scanModelCatalog() {
    const std::string previousPath =
        (m_selectedModelIndex >= 0 &&
         m_selectedModelIndex < static_cast<int>(m_modelCatalog.getEntries().size()))
            ? m_modelCatalog.getEntries()[m_selectedModelIndex].relativePath
            : std::string();

    std::string error;
    if (!m_modelCatalog.scan(m_modelRoot, error)) {
        const std::string fallback = "../../Data/Models";
        if (!m_modelCatalog.scan(fallback, error)) {
            m_selectedModelIndex = -1;
            m_modelLoaded = false;
            m_modelError = error;
            return;
        }
        m_modelRoot = fallback;
    }

    m_selectedModelIndex = m_modelCatalog.findByRelativePath(previousPath);
    if (m_selectedModelIndex < 0 && !m_modelCatalog.getEntries().empty()) {
        m_selectedModelIndex = 0;
    }
    m_modelError.clear();
    if (m_selectedModelIndex >= 0) loadSelectedModel();
}

bool TestModule_3DRender::loadSelectedModel() {
    const auto& entries = m_modelCatalog.getEntries();
    if (m_selectedModelIndex < 0 || m_selectedModelIndex >= static_cast<int>(entries.size())) {
        m_modelLoaded = false;
        m_modelError.clear();
        return true;
    }

    ST::ModelAsset loaded;
    std::string error;
    if (!ST::ObjModelLoader::load(entries[m_selectedModelIndex].absolutePath, loaded, error)) {
        m_modelError = error;
        return false;
    }
    m_activeModel = std::move(loaded);
    m_modelLoaded = true;
    m_modelError.clear();
    needsRerender = true;
    return true;
}

bool TestModule_3DRender::selectModelIndex(int index) {
    if (index < 0 || index >= static_cast<int>(m_modelCatalog.getEntries().size())) return false;
    m_selectedModelIndex = index;
    return loadSelectedModel();
}

void TestModule_3DRender::drawMesh(const ST::Mesh& mesh,
                                   const ST::Matrix4x4& model,
                                   const ST::Matrix4x4& view,
                                   const ST::Matrix4x4& projection)
{
    ST::Uniform u;
    u.modelMatrix = model;
    u.normalMatrix = model.inverse().transpose();
    u.viewMatrix = view;
    u.projectionMatrix = projection;
    m_vertexShader.setUniform(u);

    ST::ShaderContext shaderContext;
    shaderContext.uniforms = u;
    shaderContext.viewPosition = m_eye;
    shaderContext.sampleTexture = [this](const ST::Vector2& uv) {
        return m_fragmentShader.sampleTexture(uv);
    };
    const auto shader = m_activeShader ? m_activeShader : m_builtinShader;

    const auto& verts = mesh.getVertices();
    const auto& idx = mesh.getIndices();

    // Transform each indexed vertex once per draw. The teapot has 3,644
    // vertices but 6,320 triangles; without this cache the vertex shader was
    // invoked up to 18,960 times for one frame instead of 3,644 times.
    m_vertexCache.resize(verts.size());
    for (size_t vertexIndex = 0; vertexIndex < verts.size(); ++vertexIndex) {
        m_vertexCache[vertexIndex] = shader->vertex(verts[vertexIndex], shaderContext);
    }

    // Compute the mesh's world-space bounding sphere once per drawMesh call.
    // We use it to detect "camera inside mesh" and to flip the back-face
    // culling direction so that stepping inside a closed convex mesh still
    // shows the inner walls.
    ST::Vector3 meshCenter(0, 0, 0);
    for (const auto& v : verts) meshCenter = meshCenter + v.position;
    meshCenter = meshCenter * (1.0f / std::max(1, (int)verts.size()));
    ST::Vector3 meshCenterWorld = (model * ST::Vector4(meshCenter, 1.0f)).toVector3();
    float boundingRadius = 0.0f;
    for (const auto& v : verts) {
        float d = (v.position - meshCenter).length();
        if (d > boundingRadius) boundingRadius = d;
    }
    ST::Vector3 cameraToCenter = meshCenterWorld - m_eye;
    bool cameraInside = cameraToCenter.length() < boundingRadius;

    for (int i = 0; i + 2 < (int)idx.size(); i += 3) {
        ST::VertexOut v0 = m_vertexCache[idx[i + 0]];
        ST::VertexOut v1 = m_vertexCache[idx[i + 1]];
        ST::VertexOut v2 = m_vertexCache[idx[i + 2]];

        // ---- Back-face culling, inside-aware ----
        // The outward face normal is (v1 - v0) x (v2 - v0) in world space.
        // For a camera outside the mesh, triangles whose normal points
        // away from the eye (dot <= 0) are back-facing and can be skipped.
        // When the camera is inside the mesh every outward normal points
        // away from the eye, so we flip the test and keep the triangles
        // whose outward normal is pointing most toward the camera. This
        // is the cheapest way to render the "inside view" of a closed
        // convex mesh without doubling the triangle count.
        ST::Vector3 faceNormal = (v1.worldPosition - v0.worldPosition)
                                    .cross(v2.worldPosition - v0.worldPosition);
        if (faceNormal.lengthSquared() <= 1e-12f) continue;
        faceNormal = faceNormal.normalized();
        ST::Vector3 toEye = m_eye - v0.worldPosition;
        float visibilityDot = faceNormal.dot(toEye);
        if (cameraInside) visibilityDot = -visibilityDot;
        if (visibilityDot <= 0.0f) continue;

        // ---- Complete clip-space clipping (Sutherland-Hodgman) ----
        // Do not require any original vertex to be inside the frustum: a
        // triangle can intersect the visible volume with all three vertices
        // outside it (the common case when the camera is inside a cube).
        // Clipping against all six planes handles that case and keeps every
        // rasterized vertex inside the valid perspective-divide domain.
        auto dispatchOne = [&](const ST::VertexOut& a,
                              const ST::VertexOut& b,
                              const ST::VertexOut& c) {
            auto frag = [this, shader, &shaderContext, faceNormal, cameraInside](const ST::VertexOut& f) {
                if (shader != m_builtinShader) return shader->fragment(f, shaderContext);
                if (!m_lightingEnabled) return f.color;

                // createCube() shares its eight corners, so vertex normals
                // cannot represent hard cube edges. Use the geometric face
                // normal for this demo and flip it for an interior view so
                // inner walls receive light from sources inside the cube.
                ST::VertexOut lit = f;
                lit.normal = cameraInside ? -faceNormal : faceNormal;
                return shader->fragment(lit, shaderContext);
            };
            m_rasterizer.rasterizeTriangle(a, b, c, frag);
        };

        ST::VertexOut emitBuf[16];
        int emitCount = 0;
        ST::clipTriangleAgainstFrustum(v0, v1, v2, emitBuf, emitCount);
        if (emitCount < 3) {
            // Triangle fully outside the clip volume -- nothing to draw.
        } else {
            // Convex clipped polygon: triangulate as a fan.
            for (int k = 1; k + 1 < emitCount; ++k) {
                dispatchOne(emitBuf[0], emitBuf[k], emitBuf[k + 1]);
            }
        }
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

    // Dense meshes are fill-rate bound in the software rasterizer. During a
    // camera drag, render a half-resolution preview and let SDL upscale it;
    // mouse release schedules a sharp full-resolution frame.
    const int renderW = m_interactionActive ? std::max(1, canvasW / 2) : canvasW;
    const int renderH = m_interactionActive ? std::max(1, canvasH / 2) : canvasH;

    if (m_frameBuffer == nullptr || m_canvasW != renderW || m_canvasH != renderH) {
        rebuildBuffers(renderW, renderH);
        m_canvasW = renderW;
        m_canvasH = renderH;
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

    float aspect = static_cast<float>(renderW) / static_cast<float>(renderH);
    // The demo deliberately supports flying inside the unit cube.  A 0.1
    // near plane would clip away a wall as soon as the camera gets within
    // ten centimetres of it, so use a smaller near distance for the editor
    // preview while retaining the complete clip-space clipping step above.
    ST::Matrix4x4 projection = ST::Matrix4x4::perspective(
        static_cast<float>(M_PI) / 3.0f, // 60 degrees vertical FOV
        aspect,
        0.001f,
        100.0f
    );

    ST::Matrix4x4 model = ST::Matrix4x4::identity();
    if (m_modelLoaded) {
        const float radius = std::max(0.001f, m_activeModel.boundsRadius);
        // Center and normalize imported assets so arbitrary source units fit
        // the existing editor camera without requiring per-model settings.
        model = ST::Matrix4x4::scale(0.9f / radius) *
                ST::Matrix4x4::translation(-m_activeModel.boundsCenter);
    }

    m_fragmentShader.setViewPosition(eye);
    m_fragmentShader.setMaterial(m_material);
    m_fragmentShader.setAmbient(m_ambientLight);
    m_fragmentShader.clearLights();
    if (m_lightingEnabled) m_fragmentShader.addLight(m_light);

    pollShaderReload();

    m_rasterizer.setUseRawScreenCoords(false);
    if (m_modelLoaded) {
        for (const auto& part : m_activeModel.parts) {
            drawMesh(part.mesh, model, view, projection);
        }
    } else {
        drawMesh(m_cube, model, view, projection);
    }

    // ---- Upload to SDL texture ----
    SDL_Renderer* renderer = static_cast<SDL_Renderer*>(canvasTexture);
    const auto& pixels = m_frameBuffer->getPixels();

    // Keep the upload staging buffer alive between frames. Camera drags can
    // trigger many renders per second, and repeatedly allocating a 640x480
    // pixel array adds avoidable allocator and cache churn.
    m_rgba32Buffer.resize(static_cast<size_t>(renderW) * static_cast<size_t>(renderH));
    for (int i = 0; i < renderW * renderH; ++i) {
        m_rgba32Buffer[static_cast<size_t>(i)] = packRGBA(pixels[i]);
    }

    // Reuse one streaming texture instead of allocating and destroying an SDL
    // texture every frame. The old per-frame allocation caused unnecessary
    // driver/heap churn and could eventually surface as heap corruption.
    if (m_sdlRenderer != renderer || !m_outputTexture ||
        m_outputTextureW != renderW || m_outputTextureH != renderH) {
        if (m_outputTexture) SDL_DestroyTexture(m_outputTexture);
        m_sdlRenderer = renderer;
        m_outputTexture = SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STREAMING,
            renderW, renderH);
        m_outputTextureW = renderW;
        m_outputTextureH = renderH;
    }
    if (!m_outputTexture) return;

    SDL_UpdateTexture(m_outputTexture, nullptr, m_rgba32Buffer.data(), renderW * sizeof(uint32_t));
    SDL_RenderCopy(renderer, m_outputTexture, nullptr, nullptr);
}

bool TestModule_3DRender::renderControls() {
    bool changed = false;
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Camera (Fly, UE-style)");
    ImGui::Separator();

    changed |= ImGui::SliderFloat("Yaw",   &m_yaw,   -6.28f, 6.28f);
    changed |= ImGui::SliderFloat("Pitch", &m_pitch, -1.5f,  1.5f);

    changed |= ImGui::DragFloat3("Eye", &m_eye.x, 0.05f);

    changed |= ImGui::SliderFloat("Speed",      &m_moveSpeed, m_moveSpeedMin, m_moveSpeedMax, "%.2f u/s");
    if (ImGui::Button("Reset Camera")) {
        m_eye    = ST::Vector3(1.6f, 1.0f, 2.5f);
        m_yaw    = 0.6f;
        m_pitch  = 0.35f;
        m_moveSpeed = 2.5f;
        changed = true;
    }

    ImGui::Separator();
    ImGui::BulletText("LMB drag in canvas to look.");
    ImGui::BulletText("RMB drag also looks; hold RMB + WASD/QE to fly.");
    ImGui::BulletText("Shift = sprint, Wheel = change speed.");

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Lighting (Blinn-Phong)");
    ImGui::Separator();
    changed |= ImGui::Checkbox("Enable lighting", &m_lightingEnabled);
    changed |= ImGui::ColorEdit3("Light color", &m_light.color.r);
    changed |= ImGui::DragFloat3("Light direction", &m_light.direction.x, 0.02f, -1.0f, 1.0f);
    if (m_light.direction.lengthSquared() < 1e-8f) {
        m_light.direction = ST::Vector3(0.0f, -1.0f, 0.0f);
    } else {
        m_light.direction.normalize();
    }
    changed |= ImGui::SliderFloat("Light intensity", &m_light.intensity, 0.0f, 5.0f);
    changed |= ImGui::ColorEdit3("Material diffuse", &m_material.diffuse.x);
    changed |= ImGui::ColorEdit3("Material specular", &m_material.specular.x);
    changed |= ImGui::SliderFloat("Shininess", &m_material.shininess, 1.0f, 256.0f);
    changed |= ImGui::ColorEdit3("Ambient", &m_ambientLight.x);

    changed |= renderShaderControls();
    changed |= renderModelControls();
    if (changed) needsRerender = true;
    return changed;
}

bool TestModule_3DRender::renderModelControls() {
    bool changed = false;
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Model");
    ImGui::Separator();

    const auto& entries = m_modelCatalog.getEntries();
    if (entries.empty()) {
        ImGui::TextDisabled("No .obj files found in %s", m_modelRoot.c_str());
    } else {
        const char* preview = (m_selectedModelIndex >= 0 &&
                               m_selectedModelIndex < static_cast<int>(entries.size()))
            ? entries[m_selectedModelIndex].displayName.c_str()
            : "Built-in cube";
        if (ImGui::BeginCombo("Current model", preview)) {
            for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
                const bool selected = i == m_selectedModelIndex;
                if (ImGui::Selectable(entries[i].displayName.c_str(), selected)) {
                    m_selectedModelIndex = i;
                    loadSelectedModel();
                    changed = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", entries[i].relativePath.c_str());
            }
            ImGui::EndCombo();
        }
        if (m_selectedModelIndex >= 0 && m_selectedModelIndex < static_cast<int>(entries.size())) {
            ImGui::TextDisabled("%s", entries[m_selectedModelIndex].relativePath.c_str());
        }
    }

    if (ImGui::Button("Refresh model list")) {
        scanModelCatalog();
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload current model")) {
        loadSelectedModel();
        changed = true;
    }

    if (m_modelLoaded) {
        ImGui::Text("Parts: %d  Vertices: %d  Triangles: %d",
                    static_cast<int>(m_activeModel.parts.size()),
                    m_activeModel.getVertexCount(), m_activeModel.getTriangleCount());
    } else {
        ImGui::TextDisabled("Active: built-in cube");
    }
    if (!m_modelError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Model error");
        ImGui::TextWrapped("%s", m_modelError.c_str());
    }
    return changed;
}

bool TestModule_3DRender::renderShaderControls() {
    bool changed = false;
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Shader");
    ImGui::Separator();

    const auto& entries = m_shaderCatalog.getEntries();
    if (entries.empty()) {
        ImGui::TextDisabled("No .stshader files found in %s", m_shaderRoot.c_str());
    } else {
        const char* preview = (m_selectedShaderIndex >= 0 &&
                               m_selectedShaderIndex < static_cast<int>(entries.size()))
            ? entries[m_selectedShaderIndex].displayName.c_str()
            : "Built-in shader";
        if (ImGui::BeginCombo("Current shader", preview)) {
            for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
                const bool selected = i == m_selectedShaderIndex;
                if (ImGui::Selectable(entries[i].displayName.c_str(), selected)) {
                    m_selectedShaderIndex = i;
                    loadSelectedShader();
                    needsRerender = true;
                    changed = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", entries[i].relativePath.c_str());
                }
            }
            ImGui::EndCombo();
        }

        if (m_selectedShaderIndex >= 0 &&
            m_selectedShaderIndex < static_cast<int>(entries.size())) {
            ImGui::TextDisabled("%s", entries[m_selectedShaderIndex].relativePath.c_str());
        }
    }

    if (ImGui::Button("Refresh shader list")) {
        scanShaderCatalog();
        if (m_selectedShaderIndex >= 0) loadSelectedShader();
        changed = true;
        needsRerender = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload current shader")) {
        loadSelectedShader();
        changed = true;
        needsRerender = true;
    }

    if (m_activeShader == m_builtinShader) {
        ImGui::TextDisabled("Active: built-in Blinn-Phong shader");
    } else if (m_shaderError.empty()) {
        ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "Active: script shader");
    }
    if (!m_shaderError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Shader error");
        ImGui::TextWrapped("%s", m_shaderError.c_str());
    }
    return changed;
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
    if (button == SDL_BUTTON_LEFT || button == SDL_BUTTON_RIGHT) {
        m_interactionActive = true;
        needsRerender = true;
    }
}

void TestModule_3DRender::onMouseUp(int button) {
    if (button == SDL_BUTTON_LEFT) {
        m_lmbDown = false;
    } else if (button == SDL_BUTTON_RIGHT) {
        m_rmbDown = false;
    }
    if (!m_lmbDown && !m_rmbDown) {
        m_interactionActive = false;
        needsRerender = true;
    }
}

void TestModule_3DRender::onMouseMove(int x, int y) {
    // Both LMB and RMB dragging rotate the view -- matches UE.
    if (!m_lmbDown && !m_rmbDown) return;
    m_interactionActive = true;
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
