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
    syncLightAnglesFromDirection();
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

void TestModule_3DRender::useBuiltinShader() {
    m_selectedShaderIndex = -1;
    m_shaderManager.clear();
    m_activeShader = m_builtinShader;
    m_shaderError.clear();
    needsRerender = true;
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
        (m_addModelIndex >= 0 &&
         m_addModelIndex < static_cast<int>(m_modelCatalog.getEntries().size()))
            ? m_modelCatalog.getEntries()[m_addModelIndex].relativePath
            : std::string();

    std::string error;
    if (!m_modelCatalog.scan(m_modelRoot, error)) {
        const std::string fallback = "../../Data/Models";
        if (!m_modelCatalog.scan(fallback, error)) {
            m_selectedModelIndex = -1;
            m_addModelIndex = -1;
            m_modelLoaded = false;
            m_modelError = error;
            return;
        }
        m_modelRoot = fallback;
    }

    m_addModelIndex = m_modelCatalog.findByRelativePath(previousPath);
    if (m_addModelIndex < 0 && !m_modelCatalog.getEntries().empty()) {
        m_addModelIndex = 0;
    }

    // A refresh may reorder catalog entries. Scene objects retain their
    // already loaded assets, while their indices are remapped by path.
    for (auto& object : m_sceneObjects) {
        object.modelIndex = m_modelCatalog.findByRelativePath(object.modelPath);
    }
    m_modelError.clear();
    if (m_sceneObjects.empty() && m_addModelIndex >= 0) {
        createSceneObject(m_addModelIndex);
    } else {
        selectSceneObject(m_selectedSceneObject);
    }
}

bool TestModule_3DRender::loadSelectedModel() {
    const auto& entries = m_modelCatalog.getEntries();
    if (m_selectedModelIndex < 0 || m_selectedModelIndex >= static_cast<int>(entries.size())) {
        return false;
    }

    if (m_selectedSceneObject < 0 ||
        m_selectedSceneObject >= static_cast<int>(m_sceneObjects.size())) {
        return createSceneObject(m_selectedModelIndex);
    }
    return replaceSceneObjectModel(m_selectedSceneObject, m_selectedModelIndex);
}

bool TestModule_3DRender::replaceSceneObjectModel(int objectIndex, int modelIndex) {
    const auto& entries = m_modelCatalog.getEntries();
    if (objectIndex < 0 || objectIndex >= static_cast<int>(m_sceneObjects.size()) ||
        modelIndex < 0 || modelIndex >= static_cast<int>(entries.size())) {
        return false;
    }

    ST::ModelAsset loaded;
    std::string error;
    if (!ST::ObjModelLoader::load(entries[modelIndex].absolutePath, loaded, error)) {
        m_modelError = error;
        return false;
    }

    SceneObject& object = m_sceneObjects[objectIndex];
    object.modelIndex = modelIndex;
    object.modelPath = entries[modelIndex].relativePath;
    object.model = std::make_shared<ST::ModelAsset>(std::move(loaded));
    if (object.name.empty()) {
        object.name = entries[modelIndex].displayName + " " + std::to_string(object.id);
    }
    object.material = ST::Material::defaultMaterial();
    object.diffuseTexture.clear();
    object.textureStatus.clear();
    m_modelError.clear();

    // Each scene object owns its material and texture state. This keeps
    // adding a second model from changing the appearance of the first one.
    if (!object.model->parts.empty()) {
        const ST::ModelPart& part = object.model->parts.front();
        if (part.materialIndex >= 0 &&
            part.materialIndex < static_cast<int>(object.model->materials.size())) {
            const ST::ModelMaterial& material = object.model->materials[part.materialIndex];
            object.material.ambient = material.ambient.rgb;
            object.material.diffuse = material.diffuse.rgb;
            object.material.specular = material.specular.rgb;
            object.material.shininess = std::max(1.0f, material.shininess);
            if (!material.diffuseTexturePath.empty()) {
                if (object.diffuseTexture.load(material.diffuseTexturePath.c_str())) {
                    object.textureStatus = "Diffuse texture: " + material.diffuseTexturePath;
                } else {
                    object.textureStatus = "Diffuse texture missing: " + material.diffuseTexturePath;
                }
            } else {
                object.textureStatus = "Material loaded without diffuse texture";
            }
        } else {
            object.textureStatus = "No MTL material; using default white material";
        }
    }
    selectSceneObject(objectIndex);
    needsRerender = true;
    return true;
}

bool TestModule_3DRender::createSceneObject(int modelIndex) {
    if (modelIndex < 0 || modelIndex >= static_cast<int>(m_modelCatalog.getEntries().size())) {
        return false;
    }
    SceneObject object;
    object.id = m_nextSceneObjectId++;
    // Alternate around the origin so new objects remain in front of the
    // default camera instead of being placed directly underneath it.
    const int existingCount = static_cast<int>(m_sceneObjects.size());
    if (existingCount > 0) {
        const int ring = (existingCount + 1) / 2;
        object.position.x = (existingCount % 2 == 1 ? -1.0f : 1.0f) *
                            static_cast<float>(ring) * 1.8f;
    }
    m_sceneObjects.push_back(std::move(object));
    const int objectIndex = static_cast<int>(m_sceneObjects.size()) - 1;
    if (!replaceSceneObjectModel(objectIndex, modelIndex)) {
        m_sceneObjects.pop_back();
        return false;
    }
    selectSceneObject(objectIndex);
    return true;
}

void TestModule_3DRender::selectSceneObject(int objectIndex) {
    m_transformGizmoAxis = -1;
    if (objectIndex < 0 || objectIndex >= static_cast<int>(m_sceneObjects.size())) {
        m_selectedSceneObject = -1;
        m_selectedModelIndex = m_addModelIndex;
        m_modelTextureStatus.clear();
        m_modelLoaded = !m_sceneObjects.empty();
        return;
    }
    m_selectedSceneObject = objectIndex;
    const SceneObject& object = m_sceneObjects[objectIndex];
    m_selectedModelIndex = object.modelIndex;
    m_addModelIndex = object.modelIndex >= 0 ? object.modelIndex : m_addModelIndex;
    m_modelTextureStatus = object.textureStatus;
    m_modelLoaded = object.model != nullptr;
}

void TestModule_3DRender::duplicateSelectedSceneObject() {
    if (m_selectedSceneObject < 0 ||
        m_selectedSceneObject >= static_cast<int>(m_sceneObjects.size())) return;
    SceneObject copy = m_sceneObjects[m_selectedSceneObject];
    copy.id = m_nextSceneObjectId++;
    copy.name += " Copy";
    copy.position.x += 0.5f;
    copy.position.z += 0.5f;
    m_sceneObjects.push_back(std::move(copy));
    selectSceneObject(static_cast<int>(m_sceneObjects.size()) - 1);
    needsRerender = true;
}

void TestModule_3DRender::deleteSelectedSceneObject() {
    if (m_selectedSceneObject < 0 ||
        m_selectedSceneObject >= static_cast<int>(m_sceneObjects.size())) return;
    const int removed = m_selectedSceneObject;
    m_sceneObjects.erase(m_sceneObjects.begin() + removed);
    if (m_sceneObjects.empty()) {
        selectSceneObject(-1);
    } else {
        selectSceneObject(std::min(removed, static_cast<int>(m_sceneObjects.size()) - 1));
    }
    needsRerender = true;
}

const ST::ModelAsset* TestModule_3DRender::getActiveModel() const {
    if (m_selectedSceneObject < 0 ||
        m_selectedSceneObject >= static_cast<int>(m_sceneObjects.size())) return nullptr;
    return m_sceneObjects[m_selectedSceneObject].model.get();
}

std::vector<TestModule_3DRender::SceneObjectInfo>
TestModule_3DRender::getSceneObjectInfos() const {
    std::vector<SceneObjectInfo> result;
    result.reserve(m_sceneObjects.size());
    for (int i = 0; i < static_cast<int>(m_sceneObjects.size()); ++i) {
        const SceneObject& object = m_sceneObjects[i];
        result.push_back(SceneObjectInfo{
            object.id,
            object.name,
            object.modelIndex,
            object.modelPath,
            object.position,
            object.rotation,
            object.scale,
            object.visible,
            i == m_selectedSceneObject
        });
    }
    return result;
}

bool TestModule_3DRender::selectSceneObjectIndex(int objectIndex) {
    if (objectIndex < 0 || objectIndex >= static_cast<int>(m_sceneObjects.size())) return false;
    selectSceneObject(objectIndex);
    needsRerender = true;
    return true;
}

bool TestModule_3DRender::duplicateSelectedObject() {
    if (m_selectedSceneObject < 0 ||
        m_selectedSceneObject >= static_cast<int>(m_sceneObjects.size())) return false;
    duplicateSelectedSceneObject();
    return true;
}

bool TestModule_3DRender::deleteSelectedObject() {
    if (m_selectedSceneObject < 0 ||
        m_selectedSceneObject >= static_cast<int>(m_sceneObjects.size())) return false;
    deleteSelectedSceneObject();
    return true;
}

bool TestModule_3DRender::setSceneObjectTransform(int objectIndex,
                                                  const ST::Vector3* position,
                                                  const ST::Vector3* rotation,
                                                  const ST::Vector3* scale) {
    if (objectIndex < 0 || objectIndex >= static_cast<int>(m_sceneObjects.size())) return false;
    SceneObject& object = m_sceneObjects[objectIndex];
    if (position) object.position = *position;
    if (rotation) object.rotation = *rotation;
    if (scale) {
        object.scale.x = std::clamp(scale->x, 0.01f, 100.0f);
        object.scale.y = std::clamp(scale->y, 0.01f, 100.0f);
        object.scale.z = std::clamp(scale->z, 0.01f, 100.0f);
    }
    needsRerender = true;
    return true;
}

bool TestModule_3DRender::selectModelIndex(int index) {
    if (index < 0 || index >= static_cast<int>(m_modelCatalog.getEntries().size())) return false;
    m_selectedModelIndex = index;
    m_addModelIndex = index;
    return loadSelectedModel();
}

bool TestModule_3DRender::setLightDirection(const ST::Vector3& direction) {
    if (direction.lengthSquared() <= 1e-8f) return false;
    m_light.direction = direction.normalized();
    syncLightAnglesFromDirection();
    needsRerender = true;
    return true;
}

void TestModule_3DRender::setLightIntensity(float intensity) {
    m_light.intensity = std::clamp(intensity, 0.0f, 5.0f);
    needsRerender = true;
}

ST::Matrix4x4 TestModule_3DRender::buildSceneObjectMatrix(int objectIndex) const {
    if (objectIndex < 0 || objectIndex >= static_cast<int>(m_sceneObjects.size())) {
        return ST::Matrix4x4::identity();
    }
    const SceneObject& object = m_sceneObjects[objectIndex];
    ST::Matrix4x4 normalize = ST::Matrix4x4::identity();
    if (object.model) {
        const float radius = std::max(0.001f, object.model->boundsRadius);
        normalize = ST::Matrix4x4::scale(0.9f / radius) *
                    ST::Matrix4x4::translation(-object.model->boundsCenter);
    }
    const float toRadians = static_cast<float>(M_PI) / 180.0f;
    return ST::Matrix4x4::translation(object.position) *
           ST::Matrix4x4::rotation(object.rotation.y * toRadians,
                                   object.rotation.x * toRadians,
                                   object.rotation.z * toRadians) *
           ST::Matrix4x4::scale(object.scale.x, object.scale.y, object.scale.z) *
           normalize;
}

void TestModule_3DRender::bindSceneObjectMaterial(int objectIndex) {
    if (objectIndex < 0 || objectIndex >= static_cast<int>(m_sceneObjects.size())) return;
    const SceneObject& object = m_sceneObjects[objectIndex];
    m_fragmentShader.setMaterial(object.material);
    if (object.diffuseTexture.isValid()) {
        m_fragmentShader.setTexture(object.diffuseTexture.getPixels(),
                                    object.diffuseTexture.getWidth(),
                                    object.diffuseTexture.getHeight());
    } else {
        m_fragmentShader.setTexture({}, 0, 0);
    }
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
    // Imported meshes normally carry authored normals (or normals generated
    // by the OBJ loader), so use smooth perspective-correct interpolation by
    // default. The procedural fallback cube has shared corners and therefore
    // still needs geometric face normals to preserve its hard edges.
    const bool useFlatShading = m_flatShading || !m_modelLoaded;

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
            auto frag = [this, shader, &shaderContext, faceNormal, cameraInside, useFlatShading](const ST::VertexOut& f) {
                if (shader != m_builtinShader) return shader->fragment(f, shaderContext);
                if (!m_lightingEnabled) return f.color;

                // Smooth mode keeps the perspective-correct interpolated
                // vertex normal produced by Rasterizer. Flat mode replaces it
                // with the geometric face normal. The latter is also required
                // for the procedural cube, whose eight corners are shared.
                ST::VertexOut lit = f;
                if (useFlatShading) {
                    lit.normal = cameraInside ? -faceNormal : faceNormal;
                }
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
    const int qualityScale = (!m_interactionActive && m_supersampleEnabled) ? 2 : 1;
    const int renderW = m_interactionActive
        ? std::max(1, canvasW / 2)
        : canvasW * qualityScale;
    const int renderH = m_interactionActive
        ? std::max(1, canvasH / 2)
        : canvasH * qualityScale;

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

    m_fragmentShader.setViewPosition(eye);
    m_fragmentShader.setAmbient(m_ambientLight);
    m_fragmentShader.clearLights();
    if (m_lightingEnabled) m_fragmentShader.addLight(m_light);

    pollShaderReload();

    m_rasterizer.setUseRawScreenCoords(false);
    ST::Matrix4x4 selectedModelMatrix = ST::Matrix4x4::identity();
    bool renderedSceneObject = false;
    for (int objectIndex = 0; objectIndex < static_cast<int>(m_sceneObjects.size()); ++objectIndex) {
        const SceneObject& object = m_sceneObjects[objectIndex];
        if (!object.visible || !object.model) continue;
        const ST::Matrix4x4 model = buildSceneObjectMatrix(objectIndex);
        bindSceneObjectMaterial(objectIndex);
        for (const auto& part : object.model->parts) {
            drawMesh(part.mesh, model, view, projection);
        }
        if (objectIndex == m_selectedSceneObject) selectedModelMatrix = model;
        renderedSceneObject = true;
    }
    if (!renderedSceneObject) {
        m_fragmentShader.setMaterial(m_material);
        m_fragmentShader.setTexture({}, 0, 0);
        drawMesh(m_cube, ST::Matrix4x4::identity(), view, projection);
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
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
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
    drawTransformGizmo(renderer, canvasW, canvasH, view, projection, selectedModelMatrix);
    drawLightGizmo(renderer, canvasW, canvasH, view, projection, selectedModelMatrix);
}

bool TestModule_3DRender::renderControls() {
    bool changed = false;
    changed |= renderSceneObjectControls();

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
    changed |= ImGui::Checkbox("Show light gizmo", &m_showLightGizmo);
    changed |= ImGui::Checkbox("Flat shading", &m_flatShading);
    changed |= ImGui::Checkbox("2x final supersampling", &m_supersampleEnabled);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Off: interpolate vertex normals (smooth)\nOn: use one geometric normal per triangle");
    }
    changed |= ImGui::ColorEdit3("Light color", &m_light.color.r);
    const ST::Vector3 directionBeforeEdit = m_light.direction;
    changed |= ImGui::DragFloat3("Light direction", &m_light.direction.x, 0.02f, -1.0f, 1.0f);
    if (m_light.direction.lengthSquared() < 1e-8f) {
        m_light.direction = ST::Vector3(0.0f, -1.0f, 0.0f);
    } else {
        m_light.direction.normalize();
    }
    if (m_light.direction != directionBeforeEdit) syncLightAnglesFromDirection();
    changed |= ImGui::SliderFloat("Light intensity", &m_light.intensity, 0.0f, 5.0f);
    ST::Material* editedMaterial = &m_material;
    if (m_selectedSceneObject >= 0 &&
        m_selectedSceneObject < static_cast<int>(m_sceneObjects.size())) {
        editedMaterial = &m_sceneObjects[m_selectedSceneObject].material;
    }
    changed |= ImGui::ColorEdit3("Material diffuse", &editedMaterial->diffuse.x);
    changed |= ImGui::ColorEdit3("Material specular", &editedMaterial->specular.x);
    changed |= ImGui::SliderFloat("Shininess", &editedMaterial->shininess, 1.0f, 256.0f);
    changed |= ImGui::ColorEdit3("Ambient", &m_ambientLight.x);
    if (ImGui::Button("Reset Light")) {
        m_light.direction = ST::Vector3(-0.4f, -1.0f, -0.6f).normalized();
        m_light.color = ST::Color::white();
        m_light.intensity = 1.4f;
        syncLightAnglesFromDirection();
        changed = true;
    }

    changed |= renderShaderControls();
    changed |= renderModelControls();
    if (changed) needsRerender = true;
    return changed;
}

bool TestModule_3DRender::renderSceneObjectControls() {
    bool changed = false;
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Selected Object");
    ImGui::Separator();
    if (m_selectedSceneObject < 0 ||
        m_selectedSceneObject >= static_cast<int>(m_sceneObjects.size())) {
        ImGui::TextDisabled("No scene object selected");
        ImGui::Spacing();
        return false;
    }

    SceneObject& object = m_sceneObjects[m_selectedSceneObject];
    ImGui::Text("%s", object.name.c_str());
    if (ImGui::RadioButton("Move (W)", m_transformTool == TransformTool::Translate)) {
        m_transformTool = TransformTool::Translate;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate (E)", m_transformTool == TransformTool::Rotate)) {
        m_transformTool = TransformTool::Rotate;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale (R)", m_transformTool == TransformTool::Scale)) {
        m_transformTool = TransformTool::Scale;
    }
    changed |= ImGui::Checkbox("Show transform gizmo", &m_showTransformGizmo);
    changed |= ImGui::Checkbox("Visible", &object.visible);
    changed |= ImGui::DragFloat3("Position", &object.position.x, 0.05f);
    changed |= ImGui::DragFloat3("Rotation", &object.rotation.x, 0.5f, -360.0f, 360.0f, "%.1f deg");
    if (ImGui::DragFloat3("Scale", &object.scale.x, 0.01f, 0.01f, 100.0f)) {
        object.scale.x = std::clamp(object.scale.x, 0.01f, 100.0f);
        object.scale.y = std::clamp(object.scale.y, 0.01f, 100.0f);
        object.scale.z = std::clamp(object.scale.z, 0.01f, 100.0f);
        changed = true;
    }
    if (ImGui::Button("Reset Transform")) {
        object.position = ST::Vector3::zero();
        object.rotation = ST::Vector3::zero();
        object.scale = ST::Vector3(1.0f, 1.0f, 1.0f);
        changed = true;
    }
    ImGui::Spacing();
    return changed;
}

void TestModule_3DRender::renderCreatePanel() {
    ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Add Model");
    ImGui::Separator();

    const auto& entries = m_modelCatalog.getEntries();
    if (entries.empty()) {
        ImGui::TextDisabled("No model assets found");
    } else {
        if (m_addModelIndex < 0 || m_addModelIndex >= static_cast<int>(entries.size())) {
            m_addModelIndex = 0;
        }
        const char* preview = entries[m_addModelIndex].displayName.c_str();
        if (ImGui::BeginCombo("Model asset", preview)) {
            for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
                const bool selected = i == m_addModelIndex;
                if (ImGui::Selectable(entries[i].displayName.c_str(), selected)) {
                    m_addModelIndex = i;
                }
                if (selected) ImGui::SetItemDefaultFocus();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", entries[i].relativePath.c_str());
            }
            ImGui::EndCombo();
        }
        if (ImGui::Button("Add to Scene", ImVec2(-1.0f, 0.0f))) {
            createSceneObject(m_addModelIndex);
        }
    }

    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f),
                       "Scene Hierarchy (%d)", static_cast<int>(m_sceneObjects.size()));
    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(m_sceneObjects.size()); ++i) {
        SceneObject& object = m_sceneObjects[i];
        ImGui::PushID(object.id);
        if (ImGui::Checkbox("##visible", &object.visible)) needsRerender = true;
        ImGui::SameLine();
        if (ImGui::Selectable(object.name.c_str(), i == m_selectedSceneObject)) {
            selectSceneObject(i);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", object.modelPath.c_str());
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    if (ImGui::Button("Duplicate") && m_selectedSceneObject >= 0) {
        duplicateSelectedSceneObject();
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete") && m_selectedSceneObject >= 0) {
        deleteSelectedSceneObject();
    }
    ImGui::TextDisabled("Select an object here, then edit its Transform in Controls.");
}

void TestModule_3DRender::syncLightAnglesFromDirection() {
    const ST::Vector3 sourceDirection = (-m_light.direction).normalized();
    m_lightPitch = std::asin(std::clamp(sourceDirection.y, -1.0f, 1.0f));
    m_lightYaw = std::atan2(sourceDirection.x, sourceDirection.z);
}

void TestModule_3DRender::updateLightDirectionFromAngles() {
    const float cp = std::cos(m_lightPitch);
    const ST::Vector3 sourceDirection(
        cp * std::sin(m_lightYaw),
        std::sin(m_lightPitch),
        cp * std::cos(m_lightYaw));
    m_light.direction = -sourceDirection;
}

void TestModule_3DRender::drawTransformGizmo(SDL_Renderer* renderer,
                                             int canvasW, int canvasH,
                                             const ST::Matrix4x4& view,
                                             const ST::Matrix4x4& projection,
                                             const ST::Matrix4x4& model) {
    m_transformGizmoValid = false;
    if (!renderer || !m_showTransformGizmo || m_selectedSceneObject < 0 ||
        m_selectedSceneObject >= static_cast<int>(m_sceneObjects.size())) return;
    const SceneObject& object = m_sceneObjects[m_selectedSceneObject];
    if (!object.visible || !object.model) return;

    auto project = [&](const ST::Vector3& world, int& x, int& y) {
        const ST::Vector4 clip = projection * view * ST::Vector4(world, 1.0f);
        if (!std::isfinite(clip.w) || clip.w <= 1e-6f) return false;
        const float ndcX = clip.x / clip.w;
        const float ndcY = clip.y / clip.w;
        if (!std::isfinite(ndcX) || !std::isfinite(ndcY)) return false;
        x = static_cast<int>((ndcX + 1.0f) * 0.5f * canvasW);
        y = static_cast<int>((1.0f - ndcY) * 0.5f * canvasH);
        return true;
    };

    const ST::Vector3 center =
        (model * ST::Vector4(object.model->boundsCenter, 1.0f)).toVector3();
    const ST::Vector3 axes[3] = {
        ST::Vector3(0.75f, 0.0f, 0.0f),
        ST::Vector3(0.0f, 0.75f, 0.0f),
        ST::Vector3(0.0f, 0.0f, 0.75f)
    };
    if (!project(center, m_transformGizmoCenterX, m_transformGizmoCenterY)) return;
    for (int axis = 0; axis < 3; ++axis) {
        if (!project(center + axes[axis], m_transformGizmoEndX[axis], m_transformGizmoEndY[axis])) {
            return;
        }
    }
    m_transformGizmoValid = true;

    const Uint8 colors[3][3] = {{235, 70, 70}, {80, 220, 95}, {70, 135, 245}};
    for (int axis = 0; axis < 3; ++axis) {
        const bool active = axis == m_transformGizmoAxis;
        SDL_SetRenderDrawColor(renderer,
            active ? 255 : colors[axis][0],
            active ? 225 : colors[axis][1],
            active ? 70 : colors[axis][2], 255);
        SDL_RenderDrawLine(renderer,
            m_transformGizmoCenterX, m_transformGizmoCenterY,
            m_transformGizmoEndX[axis], m_transformGizmoEndY[axis]);
        SDL_Rect handle{
            m_transformGizmoEndX[axis] - (active ? 5 : 4),
            m_transformGizmoEndY[axis] - (active ? 5 : 4),
            active ? 10 : 8,
            active ? 10 : 8
        };
        if (m_transformTool == TransformTool::Translate) {
            SDL_RenderFillRect(renderer, &handle);
        } else {
            SDL_RenderDrawRect(renderer, &handle);
        }
    }
}

void TestModule_3DRender::drawLightGizmo(SDL_Renderer* renderer,
                                        int canvasW, int canvasH,
                                        const ST::Matrix4x4& view,
                                        const ST::Matrix4x4& projection,
                                        const ST::Matrix4x4& model)
{
    if (!renderer || !m_showLightGizmo || !m_lightingEnabled) return;

    const ST::ModelAsset* activeModel = getActiveModel();
    const ST::Vector3 center = activeModel
        ? (model * ST::Vector4(activeModel->boundsCenter, 1.0f)).toVector3()
        : ST::Vector3::zero();
    const ST::Vector3 sourceDirection = (-m_light.direction).normalized();
    const float length = activeModel ? 1.35f : 1.4f;
    const ST::Vector3 endpoint = center + sourceDirection * length;

    auto project = [&](const ST::Vector3& world, int& x, int& y) {
        const ST::Vector4 clip = projection * view * ST::Vector4(world, 1.0f);
        if (!std::isfinite(clip.w) || std::fabs(clip.w) <= 1e-6f) return false;
        const float ndcX = clip.x / clip.w;
        const float ndcY = clip.y / clip.w;
        if (!std::isfinite(ndcX) || !std::isfinite(ndcY)) return false;
        x = static_cast<int>((ndcX + 1.0f) * 0.5f * canvasW);
        y = static_cast<int>((1.0f - ndcY) * 0.5f * canvasH);
        return true;
    };

    int centerX = 0, centerY = 0, endX = 0, endY = 0;
    if (!project(center, centerX, centerY) || !project(endpoint, endX, endY)) return;
    m_lightGizmoScreenX = endX;
    m_lightGizmoScreenY = endY;

    SDL_SetRenderDrawColor(renderer, 255, 190, 45, 255);
    SDL_RenderDrawLine(renderer, centerX, centerY, endX, endY);
    const float dx = static_cast<float>(endX - centerX);
    const float dy = static_cast<float>(endY - centerY);
    const float screenLength = std::sqrt(dx * dx + dy * dy);
    if (screenLength > 1.0f) {
        const float nx = dx / screenLength;
        const float ny = dy / screenLength;
        const float px = -ny;
        const float py = nx;
        const float arrowSize = 10.0f;
        SDL_RenderDrawLine(renderer, endX, endY,
                           static_cast<int>(endX - nx * arrowSize + px * arrowSize * 0.55f),
                           static_cast<int>(endY - ny * arrowSize + py * arrowSize * 0.55f));
        SDL_RenderDrawLine(renderer, endX, endY,
                           static_cast<int>(endX - nx * arrowSize - px * arrowSize * 0.55f),
                           static_cast<int>(endY - ny * arrowSize - py * arrowSize * 0.55f));
    }
    for (int i = 0; i < 16; ++i) {
        const float a0 = static_cast<float>(i) * 2.0f * static_cast<float>(M_PI) / 16.0f;
        const float a1 = static_cast<float>(i + 1) * 2.0f * static_cast<float>(M_PI) / 16.0f;
        SDL_RenderDrawLine(renderer,
                           static_cast<int>(endX + std::cos(a0) * 7.0f),
                           static_cast<int>(endY + std::sin(a0) * 7.0f),
                           static_cast<int>(endX + std::cos(a1) * 7.0f),
                           static_cast<int>(endY + std::sin(a1) * 7.0f));
    }
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

    const ST::ModelAsset* activeModel = getActiveModel();
    if (activeModel) {
        ImGui::Text("Parts: %d  Vertices: %d  Triangles: %d",
                    static_cast<int>(activeModel->parts.size()),
                    activeModel->getVertexCount(), activeModel->getTriangleCount());
        ImGui::Text("Materials: %d", static_cast<int>(activeModel->materials.size()));
        if (!m_modelTextureStatus.empty()) ImGui::TextWrapped("%s", m_modelTextureStatus.c_str());
    } else {
        ImGui::TextDisabled("No scene object selected");
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
            const bool builtinSelected = m_selectedShaderIndex < 0;
            if (ImGui::Selectable("Built-in Blinn-Phong", builtinSelected)) {
                useBuiltinShader();
                changed = true;
            }
            if (builtinSelected) ImGui::SetItemDefaultFocus();
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
    if (button == SDL_BUTTON_LEFT && m_transformGizmoValid && m_showTransformGizmo &&
        m_selectedSceneObject >= 0) {
        float bestDistanceSquared = 10.0f * 10.0f;
        int bestAxis = -1;
        for (int axis = 0; axis < 3; ++axis) {
            const float ax = static_cast<float>(m_transformGizmoCenterX);
            const float ay = static_cast<float>(m_transformGizmoCenterY);
            const float bx = static_cast<float>(m_transformGizmoEndX[axis]);
            const float by = static_cast<float>(m_transformGizmoEndY[axis]);
            const float vx = bx - ax;
            const float vy = by - ay;
            const float lengthSquared = vx * vx + vy * vy;
            if (lengthSquared < 16.0f) continue;
            const float t = std::clamp(
                ((canvasX - ax) * vx + (canvasY - ay) * vy) / lengthSquared,
                0.0f, 1.0f);
            const float px = ax + t * vx;
            const float py = ay + t * vy;
            const float dx = canvasX - px;
            const float dy = canvasY - py;
            const float distanceSquared = dx * dx + dy * dy;
            if (distanceSquared <= bestDistanceSquared) {
                bestDistanceSquared = distanceSquared;
                bestAxis = axis;
            }
        }
        if (bestAxis >= 0) {
            m_transformGizmoAxis = bestAxis;
            m_interactionActive = true;
            m_lastCanvasX = canvasX;
            m_lastCanvasY = canvasY;
            needsRerender = true;
            return;
        }
    }
    if (button == SDL_BUTTON_LEFT && m_showLightGizmo && m_lightingEnabled) {
        const float dx = static_cast<float>(canvasX - m_lightGizmoScreenX);
        const float dy = static_cast<float>(canvasY - m_lightGizmoScreenY);
        if (dx * dx + dy * dy <= m_lightGizmoHitRadius * m_lightGizmoHitRadius) {
            m_lightDragActive = true;
            m_interactionActive = true;
            m_lastCanvasX = canvasX;
            m_lastCanvasY = canvasY;
            needsRerender = true;
            return;
        }
    }
    onMouseDown(button, canvasX, canvasY);
}
void TestModule_3DRender::onCanvasMouseUp(int button, int canvasX, int canvasY) {
    (void)canvasX; (void)canvasY;
    if (m_transformGizmoAxis >= 0 && button == SDL_BUTTON_LEFT) {
        m_transformGizmoAxis = -1;
        m_interactionActive = false;
        needsRerender = true;
        return;
    }
    if (m_lightDragActive && button == SDL_BUTTON_LEFT) {
        m_lightDragActive = false;
        m_interactionActive = false;
        needsRerender = true;
        return;
    }
    onMouseUp(button);
}
void TestModule_3DRender::onCanvasMouseMove(int canvasX, int canvasY) {
    if (m_transformGizmoAxis >= 0 && m_selectedSceneObject >= 0 &&
        m_selectedSceneObject < static_cast<int>(m_sceneObjects.size())) {
        const int axis = m_transformGizmoAxis;
        const float sx = static_cast<float>(m_transformGizmoEndX[axis] - m_transformGizmoCenterX);
        const float sy = static_cast<float>(m_transformGizmoEndY[axis] - m_transformGizmoCenterY);
        const float screenLength = std::sqrt(sx * sx + sy * sy);
        if (screenLength > 1.0f) {
            const float dx = static_cast<float>(canvasX - m_lastCanvasX);
            const float dy = static_cast<float>(canvasY - m_lastCanvasY);
            const float signedPixels = (dx * sx + dy * sy) / screenLength;
            SceneObject& object = m_sceneObjects[m_selectedSceneObject];
            float* position[3] = { &object.position.x, &object.position.y, &object.position.z };
            float* rotation[3] = { &object.rotation.x, &object.rotation.y, &object.rotation.z };
            float* scale[3] = { &object.scale.x, &object.scale.y, &object.scale.z };
            if (m_transformTool == TransformTool::Translate) {
                *position[axis] += signedPixels * 0.012f;
            } else if (m_transformTool == TransformTool::Rotate) {
                *rotation[axis] += signedPixels * 0.8f;
            } else {
                *scale[axis] = std::clamp(*scale[axis] + signedPixels * 0.012f,
                                          0.01f, 100.0f);
            }
        }
        m_lastCanvasX = canvasX;
        m_lastCanvasY = canvasY;
        needsRerender = true;
        return;
    }
    if (m_lightDragActive) {
        const int dx = canvasX - m_lastCanvasX;
        const int dy = canvasY - m_lastCanvasY;
        m_lastCanvasX = canvasX;
        m_lastCanvasY = canvasY;
        m_lightYaw -= static_cast<float>(dx) * 0.012f;
        m_lightPitch = std::clamp(m_lightPitch + static_cast<float>(dy) * 0.012f,
                                  -1.5f, 1.5f);
        updateLightDirectionFromAngles();
        needsRerender = true;
        return;
    }
    onMouseMove(canvasX, canvasY);
}

void TestModule_3DRender::onKeyDown(int keycode) {
    // Do not reinterpret flight keys while RMB is held.
    if (m_rmbDown) return;
    if (keycode == SDLK_w) m_transformTool = TransformTool::Translate;
    if (keycode == SDLK_e) m_transformTool = TransformTool::Rotate;
    if (keycode == SDLK_r) m_transformTool = TransformTool::Scale;
}
