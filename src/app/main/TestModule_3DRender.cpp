#include "TestModule_3DRender.hpp"
#include <imgui.h>
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <limits>
#include <nlohmann/json.hpp>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
constexpr int kEnvironmentMaxDimension = 1024;

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
    scanTextureCatalog();
    m_environmentTexturePath = "environment/studio_small_01.jpg";
    if (!m_environmentTexture.load((m_textureRoot + "/" + m_environmentTexturePath).c_str(),
                                   kEnvironmentMaxDimension)) {
        m_environmentTexture.load((std::string("../../Data/Textures/") + m_environmentTexturePath).c_str(),
                                  kEnvironmentMaxDimension);
    }
    scanModelCatalog();
    // The generated default scene is a clean starting point; only user
    // edits should add the unsaved marker.
    m_sceneDirty = false;
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
    markSceneDirty();
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
    markSceneDirty();
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
    markSceneDirty();
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
    markSceneDirty();
    needsRerender = true;
    return true;
}

void TestModule_3DRender::setLightIntensity(float intensity) {
    m_light.intensity = std::clamp(intensity, 0.0f, 5.0f);
    markSceneDirty();
    needsRerender = true;
}

TestModule_3DRender::EditorSettings TestModule_3DRender::getEditorSettings() const {
    EditorSettings settings;
    settings.renderQuality = m_renderQuality;
    settings.supersampleEnabled = m_supersampleEnabled;
    settings.flatShading = m_flatShading;
    settings.showLightGizmo = m_showLightGizmo;
    settings.showTransformGizmo = m_showTransformGizmo;
    settings.showSelectionOutline = m_showSelectionOutline;
    settings.cameraSpeed = m_moveSpeed;
    settings.cameraSensitivity = m_cameraSensitivity;
    return settings;
}

void TestModule_3DRender::applyEditorSettings(const EditorSettings& settings) {
    m_renderQuality = std::clamp(settings.renderQuality, 0, 2);
    m_supersampleEnabled = settings.supersampleEnabled;
    m_flatShading = settings.flatShading;
    m_showLightGizmo = settings.showLightGizmo;
    m_showTransformGizmo = settings.showTransformGizmo;
    m_showSelectionOutline = settings.showSelectionOutline;
    m_moveSpeed = std::clamp(settings.cameraSpeed, m_moveSpeedMin, m_moveSpeedMax);
    m_cameraSensitivity = std::clamp(settings.cameraSensitivity, 0.001f, 0.1f);
    m_adaptivePreview = false;
    m_frameTimeHistory.clear();
    needsRerender = true;
}

void TestModule_3DRender::recordPerformanceSample(double frameTimeMs) {
    m_frameTimeHistory.push_back(frameTimeMs);
    while (m_frameTimeHistory.size() > 30) m_frameTimeHistory.pop_front();

    if (!m_frameTimeHistory.empty()) {
        double sum = 0.0;
        m_minFrameTimeMs = m_frameTimeHistory.front();
        m_maxFrameTimeMs = m_frameTimeHistory.front();
        for (const double sample : m_frameTimeHistory) {
            sum += sample;
            m_minFrameTimeMs = std::min(m_minFrameTimeMs, sample);
            m_maxFrameTimeMs = std::max(m_maxFrameTimeMs, sample);
        }
        m_averageFrameTimeMs = sum / static_cast<double>(m_frameTimeHistory.size());
    }

    // Adaptive mode makes one controlled quality step when a completed frame
    // exceeds the 30 FPS budget. It does not continuously render in the
    // background: the current frame is finished, then the normal dirty-frame
    // path performs one preview rerender.
    if (m_renderQuality == 0 && !m_interactionActive && !m_adaptivePreview &&
        m_averageFrameTimeMs > 33.0) {
        m_adaptivePreview = true;
        needsRerender = true;
    }
}

bool TestModule_3DRender::saveScene(const std::string& path, std::string& error) const {
    using Json = nlohmann::json;
    auto vectorJson = [](const ST::Vector3& value) {
        return Json{ value.x, value.y, value.z };
    };
    auto colorJson = [](const ST::Color& value) {
        return Json{ value.r, value.g, value.b, value.a };
    };

    Json scene = {
        { "version", 2 },
        { "sceneName", std::filesystem::path(path).stem().string() },
        { "camera", {
            { "eye", vectorJson(m_eye) },
            { "yaw", m_yaw },
            { "pitch", m_pitch },
            { "moveSpeed", m_moveSpeed }
        } },
        { "lighting", {
            { "enabled", m_lightingEnabled },
            { "ambient", vectorJson(m_ambientLight) },
            { "environmentColor", vectorJson(m_environmentColor) },
            { "environmentIntensity", m_environmentIntensity },
            { "toneMapping", m_toneMappingEnabled },
            { "exposure", m_exposure },
            { "environmentMap", m_environmentMapEnabled ? m_environmentTexturePath : std::string() },
            { "direction", vectorJson(m_light.direction) },
            { "color", colorJson(m_light.color) },
            { "intensity", m_light.intensity }
        } },
        { "material", {
            { "ambient", vectorJson(m_material.ambient) },
            { "diffuse", vectorJson(m_material.diffuse) },
            { "specular", vectorJson(m_material.specular) },
            { "shininess", m_material.shininess },
            { "metallic", m_material.metallicFactor },
            { "roughness", m_material.roughness },
            { "normalStrength", m_material.normalStrength },
            { "emission", vectorJson(m_material.emission) }
        } },
        { "shader", (m_selectedShaderIndex >= 0 &&
                      m_selectedShaderIndex < static_cast<int>(m_shaderCatalog.getEntries().size()))
            ? m_shaderCatalog.getEntries()[m_selectedShaderIndex].relativePath
            : std::string() },
        { "selectedObject", m_selectedSceneObject },
        { "objects", Json::array() }
    };

    for (const SceneObject& object : m_sceneObjects) {
        scene["objects"].push_back({
            { "id", object.id },
            { "name", object.name },
            { "model", object.modelPath },
            { "diffuseTexture", object.diffuseTexturePath },
            { "roughnessTexture", object.roughnessTexturePath },
            { "metallicTexture", object.metallicTexturePath },
            { "normalTexture", object.normalTexturePath },
            { "visible", object.visible },
            { "position", vectorJson(object.position) },
            { "rotation", vectorJson(object.rotation) },
            { "scale", vectorJson(object.scale) },
            { "material", {
                { "ambient", vectorJson(object.material.ambient) },
                { "diffuse", vectorJson(object.material.diffuse) },
                { "specular", vectorJson(object.material.specular) },
                { "shininess", object.material.shininess },
                { "metallic", object.material.metallicFactor },
                { "roughness", object.material.roughness },
                { "normalStrength", object.material.normalStrength },
                { "emission", vectorJson(object.material.emission) }
            } }
        });
    }

    try {
        const std::filesystem::path outputPath(path);
        if (!outputPath.parent_path().empty()) {
            std::filesystem::create_directories(outputPath.parent_path());
        }
        std::ofstream output(outputPath);
        if (!output) {
            error = "unable to open scene for writing: " + path;
            return false;
        }
        output << scene.dump(2) << '\n';
        if (!output.good()) {
            error = "unable to write scene: " + path;
            return false;
        }
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
    error.clear();
    return true;
}

bool TestModule_3DRender::loadScene(const std::string& path, std::string& error) {
    using Json = nlohmann::json;
    auto readVector = [](const Json& value, const char* name) {
        if (!value.is_array() || value.size() != 3) {
            throw std::runtime_error(std::string(name) + " must contain three numbers");
        }
        return ST::Vector3(value[0].get<float>(), value[1].get<float>(), value[2].get<float>());
    };
    auto readColor = [](const Json& value, const char* name) {
        if (!value.is_array() || (value.size() != 3 && value.size() != 4)) {
            throw std::runtime_error(std::string(name) + " must contain three or four numbers");
        }
        return ST::Color(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(),
                         value.size() == 4 ? value[3].get<float>() : 1.0f);
    };

    m_sceneWarning.clear();
    try {
        std::ifstream input(path);
        if (!input) {
            error = "scene file not found: " + path;
            return false;
        }
        Json scene;
        input >> scene;
        const int version = scene.value("version", 1);
        if (!scene.is_object() || version < 1 || version > 2) {
            error = "unsupported scene version";
            return false;
        }
        const bool migratedFromV1 = version == 1;
        if (!scene.contains("objects") || !scene["objects"].is_array()) {
            error = "scene objects array is missing";
            return false;
        }

        std::vector<SceneObject> previousObjects = std::move(m_sceneObjects);
        const int previousSelected = m_selectedSceneObject;
        const int previousNextId = m_nextSceneObjectId;
        const int previousModelIndex = m_selectedModelIndex;
        const bool previousModelLoaded = m_modelLoaded;
        const std::string previousTextureStatus = m_modelTextureStatus;
        m_sceneObjects.clear();

        int maxId = 0;
        for (const Json& savedObject : scene["objects"]) {
            const std::string modelPath = savedObject.value("model", std::string());
            const int modelIndex = m_modelCatalog.findByRelativePath(modelPath);
            if (modelIndex < 0) {
                m_sceneObjects = std::move(previousObjects);
                m_selectedSceneObject = previousSelected;
                m_nextSceneObjectId = previousNextId;
                m_selectedModelIndex = previousModelIndex;
                m_modelLoaded = previousModelLoaded;
                m_modelTextureStatus = previousTextureStatus;
                error = "model asset not found: " + modelPath;
                return false;
            }

            SceneObject object;
            object.id = savedObject.value("id", maxId + 1);
            object.name = savedObject.value("name", m_modelCatalog.getEntries()[modelIndex].displayName);
            object.visible = savedObject.value("visible", true);
            if (savedObject.contains("position")) object.position = readVector(savedObject["position"], "position");
            if (savedObject.contains("rotation")) object.rotation = readVector(savedObject["rotation"], "rotation");
            if (savedObject.contains("scale")) object.scale = readVector(savedObject["scale"], "scale");
            object.scale.x = std::clamp(object.scale.x, 0.01f, 100.0f);
            object.scale.y = std::clamp(object.scale.y, 0.01f, 100.0f);
            object.scale.z = std::clamp(object.scale.z, 0.01f, 100.0f);
            object.modelIndex = modelIndex;
            object.modelPath = modelPath;
            m_sceneObjects.push_back(std::move(object));
            const int objectIndex = static_cast<int>(m_sceneObjects.size()) - 1;
            if (!replaceSceneObjectModel(objectIndex, modelIndex)) {
                m_sceneObjects = std::move(previousObjects);
                m_selectedSceneObject = previousSelected;
                m_nextSceneObjectId = previousNextId;
                m_selectedModelIndex = previousModelIndex;
                m_modelLoaded = previousModelLoaded;
                m_modelTextureStatus = previousTextureStatus;
                error = m_modelError.empty() ? "unable to load scene model" : m_modelError;
                return false;
            }
            const bool isPartMaterialModel =
                m_modelCatalog.getEntries()[modelIndex].format == "fbx";
            const std::string importedTextureStatus = m_sceneObjects.back().textureStatus;
            if (savedObject.contains("material")) {
                const Json& material = savedObject["material"];
                if (material.contains("ambient")) m_sceneObjects.back().material.ambient = readVector(material["ambient"], "material ambient");
                if (material.contains("diffuse")) m_sceneObjects.back().material.diffuse = readVector(material["diffuse"], "material diffuse");
                if (material.contains("specular")) m_sceneObjects.back().material.specular = readVector(material["specular"], "material specular");
                m_sceneObjects.back().material.shininess = std::clamp(material.value("shininess", 32.0f), 1.0f, 256.0f);
                m_sceneObjects.back().material.metallicFactor = std::clamp(material.value("metallic", 0.0f), 0.0f, 1.0f);
                m_sceneObjects.back().material.roughness = std::clamp(material.value("roughness", 0.5f), 0.02f, 1.0f);
                m_sceneObjects.back().material.normalStrength = std::clamp(material.value("normalStrength", 1.0f), 0.0f, 2.0f);
                if (material.contains("emission")) m_sceneObjects.back().material.emission = readVector(material["emission"], "material emission");
            }
            if (savedObject.contains("diffuseTexture")) {
                const std::string texturePath = savedObject.value("diffuseTexture", std::string());
                if (texturePath.empty()) {
                    m_sceneObjects.back().diffuseTexture.clear();
                    m_sceneObjects.back().diffuseTexturePath.clear();
                    m_sceneObjects.back().textureStatus = "Diffuse texture disabled; using material color";
                } else {
                    const int textureIndex = m_textureCatalog.findByRelativePath(texturePath);
                    const std::string resolvedPath = textureIndex >= 0
                        ? m_textureCatalog.getEntries()[textureIndex].absolutePath
                        : texturePath;
                    if (!m_sceneObjects.back().diffuseTexture.load(resolvedPath.c_str())) {
                        m_sceneWarning = "Diffuse texture not found: " + texturePath;
                    } else {
                        m_sceneObjects.back().diffuseTexturePath = textureIndex >= 0
                            ? m_textureCatalog.getEntries()[textureIndex].relativePath
                            : texturePath;
                        m_sceneObjects.back().textureStatus = "Diffuse texture: " + m_sceneObjects.back().diffuseTexturePath;
                    }
                }
            }
            auto restoreScalarTexture = [&](const char* key, ST::Image& image, std::string& storedPath) {
                if (!savedObject.contains(key)) return;
                const std::string texturePath = savedObject.value(key, std::string());
                if (texturePath.empty()) {
                    image.clear();
                    storedPath.clear();
                    return;
                }
                const int textureIndex = m_textureCatalog.findByRelativePath(texturePath);
                const std::string resolvedPath = textureIndex >= 0
                    ? m_textureCatalog.getEntries()[textureIndex].absolutePath
                    : texturePath;
                if (image.load(resolvedPath.c_str())) {
                    storedPath = textureIndex >= 0
                        ? m_textureCatalog.getEntries()[textureIndex].relativePath
                        : texturePath;
                } else {
                    m_sceneWarning = std::string(key) + " not found: " + texturePath;
                }
            };
            restoreScalarTexture("roughnessTexture", m_sceneObjects.back().roughnessTexture,
                                 m_sceneObjects.back().roughnessTexturePath);
            restoreScalarTexture("metallicTexture", m_sceneObjects.back().metallicTexture,
                                 m_sceneObjects.back().metallicTexturePath);
            restoreScalarTexture("normalTexture", m_sceneObjects.back().normalTexture,
                                 m_sceneObjects.back().normalTexturePath);
            // Scene files keep object-level texture slots for OBJ backwards
            // compatibility. FBX PBR bindings are rebuilt from their material
            // names and must remain the status shown by the model inspector.
            if (isPartMaterialModel) {
                m_sceneObjects.back().textureStatus = importedTextureStatus;
            }
            maxId = std::max(maxId, m_sceneObjects.back().id);
        }

        m_nextSceneObjectId = maxId + 1;
        if (scene.contains("camera")) {
            const Json& camera = scene["camera"];
            if (camera.contains("eye")) m_eye = readVector(camera["eye"], "camera eye");
            m_yaw = camera.value("yaw", m_yaw);
            m_pitch = std::clamp(camera.value("pitch", m_pitch), -1.5f, 1.5f);
            m_moveSpeed = std::clamp(camera.value("moveSpeed", m_moveSpeed), m_moveSpeedMin, m_moveSpeedMax);
        }
        if (scene.contains("lighting")) {
            const Json& lighting = scene["lighting"];
            m_lightingEnabled = lighting.value("enabled", m_lightingEnabled);
            if (lighting.contains("ambient")) m_ambientLight = readVector(lighting["ambient"], "ambient");
            if (lighting.contains("environmentColor")) m_environmentColor = readVector(lighting["environmentColor"], "environment color");
            m_environmentIntensity = std::clamp(lighting.value("environmentIntensity", m_environmentIntensity), 0.0f, 5.0f);
            m_toneMappingEnabled = lighting.value("toneMapping", m_toneMappingEnabled);
            m_exposure = std::clamp(lighting.value("exposure", m_exposure), 0.0f, 5.0f);
            const std::string environmentMap = lighting.value("environmentMap", m_environmentTexturePath);
            if (!environmentMap.empty()) {
                m_environmentMapEnabled = m_environmentTexture.load(
                    (m_textureRoot + "/" + environmentMap).c_str(), kEnvironmentMaxDimension);
                if (m_environmentMapEnabled) m_environmentTexturePath = environmentMap;
            } else {
                m_environmentMapEnabled = false;
            }
            if (lighting.contains("direction")) setLightDirection(readVector(lighting["direction"], "light direction"));
            if (lighting.contains("color")) m_light.color = readColor(lighting["color"], "light color");
            m_light.intensity = std::clamp(lighting.value("intensity", m_light.intensity), 0.0f, 5.0f);
        }
        if (scene.contains("material")) {
            const Json& material = scene["material"];
            if (material.contains("ambient")) m_material.ambient = readVector(material["ambient"], "material ambient");
            if (material.contains("diffuse")) m_material.diffuse = readVector(material["diffuse"], "material diffuse");
            if (material.contains("specular")) m_material.specular = readVector(material["specular"], "material specular");
            m_material.shininess = std::clamp(material.value("shininess", m_material.shininess), 1.0f, 256.0f);
            m_material.metallicFactor = std::clamp(material.value("metallic", m_material.metallicFactor), 0.0f, 1.0f);
            m_material.roughness = std::clamp(material.value("roughness", m_material.roughness), 0.02f, 1.0f);
            m_material.normalStrength = std::clamp(material.value("normalStrength", m_material.normalStrength), 0.0f, 2.0f);
            if (material.contains("emission")) m_material.emission = readVector(material["emission"], "material emission");
        }

        const std::string shaderPath = scene.value("shader", std::string());
        const int shaderIndex = m_shaderCatalog.findByRelativePath(shaderPath);
        if (shaderIndex >= 0) {
            selectShaderIndex(shaderIndex);
        } else if (!shaderPath.empty()) {
            useBuiltinShader();
            m_sceneWarning = "Shader asset not found: " + shaderPath + ". Using built-in shader.";
        } else {
            useBuiltinShader();
        }

        const int selectedObject = scene.value("selectedObject", -1);
        selectSceneObject(selectedObject >= 0 && selectedObject < static_cast<int>(m_sceneObjects.size())
            ? selectedObject : (m_sceneObjects.empty() ? -1 : 0));
        m_modelError.clear();
        if (migratedFromV1) {
            m_sceneWarning = m_sceneWarning.empty()
                ? "Loaded legacy scene version 1. Save to upgrade it to version 2."
                : m_sceneWarning + " Loaded legacy scene version 1; save to upgrade it to version 2.";
            m_sceneDirty = true;
        } else {
            markSceneSaved();
        }
        needsRerender = true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
    error.clear();
    return true;
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

int TestModule_3DRender::pickSceneObject(int canvasX, int canvasY) const {
    if (m_sceneObjects.empty()) return -1;

    const float width = static_cast<float>(std::max(1, m_inputCanvasW));
    const float height = static_cast<float>(std::max(1, m_inputCanvasH));
    const float aspect = width / height;
    const float tanHalfFov = std::tan(static_cast<float>(M_PI) / 6.0f);
    const float ndcX = (2.0f * (static_cast<float>(canvasX) + 0.5f) / width - 1.0f) *
                       aspect * tanHalfFov;
    const float ndcY = (1.0f - 2.0f * (static_cast<float>(canvasY) + 0.5f) / height) *
                       tanHalfFov;

    const float cp = std::cos(m_pitch);
    const float sp = std::sin(m_pitch);
    const float cy = std::cos(m_yaw);
    const float sy = std::sin(m_yaw);
    const ST::Vector3 forward(-cp * sy, -sp, -cp * cy);
    const ST::Vector3 right = forward.cross(ST::Vector3::up()).normalized();
    const ST::Vector3 cameraUp = right.cross(forward).normalized();
    const ST::Vector3 rayOrigin = m_eye;
    const ST::Vector3 rayDirection =
        (forward + right * ndcX + cameraUp * ndcY).normalized();

    int picked = -1;
    float nearest = std::numeric_limits<float>::max();
    for (int index = 0; index < static_cast<int>(m_sceneObjects.size()); ++index) {
        const SceneObject& object = m_sceneObjects[index];
        if (!object.visible || !object.model) continue;
        const ST::Matrix4x4 objectMatrix = buildSceneObjectMatrix(index);
        const ST::Vector3 center =
            (objectMatrix * ST::Vector4(object.model->boundsCenter, 1.0f)).toVector3();
        const float maxScale = std::max({std::fabs(object.scale.x),
                                         std::fabs(object.scale.y),
                                         std::fabs(object.scale.z)});
        const float radius = std::max(0.05f, 0.9f * maxScale);
        const ST::Vector3 offset = rayOrigin - center;
        const float b = offset.dot(rayDirection);
        const float c = offset.lengthSquared() - radius * radius;
        const float discriminant = b * b - c;
        if (discriminant < 0.0f) continue;
        const float root = std::sqrt(discriminant);
        float distance = -b - root;
        if (distance < 0.0f) distance = -b + root;
        if (distance >= 0.0f && distance < nearest) {
            nearest = distance;
            picked = index;
        }
    }
    return picked;
}

void TestModule_3DRender::focusSelectedSceneObject() {
    if (m_selectedSceneObject < 0 ||
        m_selectedSceneObject >= static_cast<int>(m_sceneObjects.size())) return;
    const SceneObject& object = m_sceneObjects[m_selectedSceneObject];
    if (!object.model || !object.visible) return;
    const ST::Matrix4x4 objectMatrix = buildSceneObjectMatrix(m_selectedSceneObject);
    const ST::Vector3 center =
        (objectMatrix * ST::Vector4(object.model->boundsCenter, 1.0f)).toVector3();
    const float maxScale = std::max({std::fabs(object.scale.x),
                                     std::fabs(object.scale.y),
                                     std::fabs(object.scale.z)});
    // Repository models use mixed authoring units (the M1911 FBX is authored
    // in centimetres while the editor camera uses scene units). Keep the
    // focus distance bounded so a large source-space bounds sphere does not
    // make the visible mesh collapse to a few pixels; users can still zoom
    // farther with the wheel or fly controls.
    const float radius = std::clamp(object.model->boundsRadius * maxScale,
                                    0.05f, 1.0f);
    const float distance = std::max(1.5f, radius * 2.4f);
    const float cp = std::cos(m_pitch);
    const float sp = std::sin(m_pitch);
    const float cy = std::cos(m_yaw);
    const float sy = std::sin(m_yaw);
    const ST::Vector3 forward(-cp * sy, -sp, -cp * cy);
    m_eye = center - forward * distance;
    markSceneDirty();
    needsRerender = true;
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
    if (object.roughnessTexture.isValid()) {
        m_fragmentShader.setRoughnessTexture(object.roughnessTexture.getPixels(),
                                              object.roughnessTexture.getWidth(),
                                              object.roughnessTexture.getHeight());
    } else {
        m_fragmentShader.setRoughnessTexture({}, 0, 0);
    }
    if (object.metallicTexture.isValid()) {
        m_fragmentShader.setMetallicTexture(object.metallicTexture.getPixels(),
                                            object.metallicTexture.getWidth(),
                                            object.metallicTexture.getHeight());
    } else {
        m_fragmentShader.setMetallicTexture({}, 0, 0);
    }
    if (object.normalTexture.isValid()) {
        m_fragmentShader.setNormalTexture(object.normalTexture.getPixels(),
                                          object.normalTexture.getWidth(),
                                          object.normalTexture.getHeight());
    } else {
        m_fragmentShader.setNormalTexture({}, 0, 0);
    }
}

void TestModule_3DRender::bindScenePartMaterial(int objectIndex, int partIndex) {
    if (objectIndex < 0 || objectIndex >= static_cast<int>(m_sceneObjects.size())) return;
    const SceneObject& object = m_sceneObjects[objectIndex];
    if (partIndex < 0 || partIndex >= static_cast<int>(object.partMaterials.size()) ||
        !object.partMaterials[partIndex].bound) {
        bindSceneObjectMaterial(objectIndex);
        return;
    }

    const SceneObject::PartMaterial& part = object.partMaterials[partIndex];
    m_fragmentShader.setMaterial(part.material);
    if (part.diffuseTexture.isValid()) {
        m_fragmentShader.setTexture(part.diffuseTexture.getPixels(),
                                    part.diffuseTexture.getWidth(),
                                    part.diffuseTexture.getHeight());
    } else {
        m_fragmentShader.setTexture({}, 0, 0);
    }
    if (part.roughnessTexture.isValid()) {
        m_fragmentShader.setRoughnessTexture(part.roughnessTexture.getPixels(),
                                             part.roughnessTexture.getWidth(),
                                             part.roughnessTexture.getHeight());
    } else {
        m_fragmentShader.setRoughnessTexture({}, 0, 0);
    }
    if (part.metallicTexture.isValid()) {
        m_fragmentShader.setMetallicTexture(part.metallicTexture.getPixels(),
                                            part.metallicTexture.getWidth(),
                                            part.metallicTexture.getHeight());
    } else {
        m_fragmentShader.setMetallicTexture({}, 0, 0);
    }
    if (part.normalTexture.isValid()) {
        m_fragmentShader.setNormalTexture(part.normalTexture.getPixels(),
                                          part.normalTexture.getWidth(),
                                          part.normalTexture.getHeight());
    } else {
        m_fragmentShader.setNormalTexture({}, 0, 0);
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
    // Imported meshes carry authored/generated vertex normals, so always use
    // smooth perspective-correct interpolation for them. Only the procedural
    // fallback cube needs geometric face normals because its corners are
    // intentionally shared between faces. Do not let hierarchy selection
    // state accidentally force an imported sphere into flat shading.
    const bool useFlatShading = m_flatShading || (&mesh == &m_cube);

    const auto& verts = mesh.getVertices();
    const auto& idx = mesh.getIndices();
    const auto vertexStageStart = std::chrono::steady_clock::now();

    // Transform each indexed vertex once per draw. The teapot has 3,644
    // vertices but 6,320 triangles; without this cache the vertex shader was
    // invoked up to 18,960 times for one frame instead of 3,644 times.
    const std::vector<ST::VertexOut>* transformedVertices = nullptr;
    if (!m_interactionActive && shader == m_builtinShader) {
        for (auto& cache : m_vertexTransformCaches) {
            if (cache.mesh == &mesh && cache.shader == shader.get() &&
                cache.model == model && cache.view == view && cache.projection == projection &&
                cache.vertices.size() == verts.size()) {
                transformedVertices = &cache.vertices;
                ++m_currentCacheHits;
                break;
            }
        }
        if (!transformedVertices) {
            ++m_currentCacheMisses;
            if (m_vertexTransformCaches.size() >= 32) {
                m_vertexTransformCaches.erase(m_vertexTransformCaches.begin());
            }
            auto& cache = m_vertexTransformCaches.emplace_back();
            cache.mesh = &mesh;
            cache.shader = shader.get();
            cache.model = model;
            cache.view = view;
            cache.projection = projection;
            cache.vertices.resize(verts.size());
            for (size_t vertexIndex = 0; vertexIndex < verts.size(); ++vertexIndex) {
                cache.vertices[vertexIndex] = shader->vertex(verts[vertexIndex], shaderContext);
            }
            transformedVertices = &cache.vertices;
        }
    } else {
        ++m_currentCacheMisses;
        m_vertexCache.resize(verts.size());
        for (size_t vertexIndex = 0; vertexIndex < verts.size(); ++vertexIndex) {
            m_vertexCache[vertexIndex] = shader->vertex(verts[vertexIndex], shaderContext);
        }
        transformedVertices = &m_vertexCache;
    }
    m_currentVertexStageMs += std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - vertexStageStart).count();

    // Compute the mesh's world-space bounding sphere once per drawMesh call.
    // We use it to detect "camera inside mesh" and to flip the back-face
    // culling direction so that stepping inside a closed convex mesh still
    // shows the inner walls.
    ST::Vector3 meshCenter(0, 0, 0);
    for (const auto& v : verts) meshCenter = meshCenter + v.position;
    meshCenter = meshCenter * (1.0f / std::max(1, (int)verts.size()));
    ST::Vector3 meshCenterWorld = (model * ST::Vector4(meshCenter, 1.0f)).toVector3();
    float boundingRadius = 0.0f;
    for (const auto& v : *transformedVertices) {
        const float d = (v.worldPosition - meshCenterWorld).length();
        if (d > boundingRadius) boundingRadius = d;
    }
    ST::Vector3 cameraToCenter = meshCenterWorld - m_eye;
    bool cameraInside = cameraToCenter.length() < boundingRadius;

    const auto rasterStageStart = std::chrono::steady_clock::now();
    for (int i = 0; i + 2 < (int)idx.size(); i += 3) {
        ST::VertexOut v0 = (*transformedVertices)[idx[i + 0]];
        ST::VertexOut v1 = (*transformedVertices)[idx[i + 1]];
        ST::VertexOut v2 = (*transformedVertices)[idx[i + 2]];

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
                if (!m_lightingEnabled) return m_fragmentShader.sampleTexture(f.texCoord);

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
    m_currentRasterStageMs += std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - rasterStageStart).count();
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

    if (moved) {
        markSceneDirty();
        needsRerender = true;
    }
}

void TestModule_3DRender::render(void* canvasTexture, int canvasW, int canvasH) {
    const auto frameStart = std::chrono::steady_clock::now();
    if (canvasW == 0 || canvasH == 0) return;
    m_inputCanvasW = canvasW;
    m_inputCanvasH = canvasH;
    m_currentVertexStageMs = 0.0;
    m_currentRasterStageMs = 0.0;
    m_currentUploadStageMs = 0.0;
    m_currentCacheHits = 0;
    m_currentCacheMisses = 0;

    // Dense meshes are fill-rate bound in the software rasterizer. During a
    // camera drag, render a half-resolution preview and let SDL upscale it;
    // mouse release schedules a sharp full-resolution frame.
    // PBR + environment reflection is fill-rate heavy in the software
    // rasterizer. Keep the explicit supersampling option, but adapt it to 1x
    // while the environment map is active so the viewport remains usable.
    const bool heavyPbr = m_environmentMapEnabled && m_environmentTexture.isValid();
    // Adaptive mode controls raster resolution, but must not disable the
    // environment lookup on a settled frame: doing so makes metallic FBX
    // parts look like an unlit grey material after a slow first frame.
    const bool previewQuality = m_renderQuality == 1;
    const bool finalQuality = m_renderQuality == 2;
    const bool reducedQuality = previewQuality || (m_interactionActive && !finalQuality);
    const bool interactionPreview = m_interactionActive && !finalQuality;
    const int qualityScale = finalQuality
        ? (m_supersampleEnabled ? 2 : 1)
        : ((!m_interactionActive && m_supersampleEnabled && !heavyPbr && !previewQuality) ? 2 : 1);
    const int renderW = interactionPreview
        ? std::max(1, canvasW / 2)
        : canvasW * qualityScale;
    const int renderH = interactionPreview
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
    m_fragmentShader.setEnvironment(m_environmentColor, m_environmentIntensity);
    m_fragmentShader.setReducedQuality(reducedQuality);
    // Environment lookup performs trigonometric projection per fragment. Keep
    // interaction responsive by using the cheap constant environment while
    // the camera/gizmo is being dragged; restore reflections on release.
    if (m_environmentMapEnabled && m_environmentTexture.isValid() && !reducedQuality) {
        m_fragmentShader.setEnvironmentTexture(m_environmentTexture.getPixels(),
                                               m_environmentTexture.getWidth(),
                                               m_environmentTexture.getHeight());
    } else {
        m_fragmentShader.setEnvironmentTexture({}, 0, 0);
    }
    m_fragmentShader.setToneMapping(m_toneMappingEnabled, m_exposure);
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
        for (size_t partIndex = 0; partIndex < object.model->parts.size(); ++partIndex) {
            bindScenePartMaterial(objectIndex, static_cast<int>(partIndex));
            const auto& part = object.model->parts[partIndex];
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
    const auto uploadStageStart = std::chrono::steady_clock::now();
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
    m_currentUploadStageMs += std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - uploadStageStart).count();
    drawSelectionOutline(renderer, canvasW, canvasH, view, projection, selectedModelMatrix);
    drawTransformGizmo(renderer, canvasW, canvasH, view, projection, selectedModelMatrix);
    drawLightGizmo(renderer, canvasW, canvasH, view, projection, selectedModelMatrix);

    m_frameTimeMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - frameStart).count();
    m_fps = m_frameTimeMs > 0.001 ? 1000.0 / m_frameTimeMs : 0.0;
    m_lastVertexStageMs = m_currentVertexStageMs;
    m_lastRasterStageMs = m_currentRasterStageMs;
    m_lastUploadStageMs = m_currentUploadStageMs;
    m_lastCacheHits = m_currentCacheHits;
    m_lastCacheMisses = m_currentCacheMisses;
    recordPerformanceSample(m_frameTimeMs);
}

bool TestModule_3DRender::renderControls() {
    bool changed = false;
    ST::Material* editedMaterial = &m_material;
    if (m_selectedSceneObject >= 0 &&
        m_selectedSceneObject < static_cast<int>(m_sceneObjects.size())) {
        editedMaterial = &m_sceneObjects[m_selectedSceneObject].material;
    }
    const ImGuiTreeNodeFlags defaultOpen = ImGuiTreeNodeFlags_DefaultOpen;
    changed |= ImGui::Checkbox("Clean preview", &m_cleanPreview);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Hide selection bounds and editor gizmos without changing their saved visibility settings.");
    }
    if (ImGui::CollapsingHeader("Selected Object", defaultOpen)) {
        changed |= renderSceneObjectControls();
    }

    if (ImGui::CollapsingHeader("Camera (Fly, UE-style)", defaultOpen)) {
        changed |= ImGui::SliderFloat("Yaw",   &m_yaw,   -6.28f, 6.28f);
        changed |= ImGui::SliderFloat("Pitch", &m_pitch, -1.5f,  1.5f);
        changed |= ImGui::DragFloat3("Eye", &m_eye.x, 0.05f);
        changed |= ImGui::SliderFloat("Speed", &m_moveSpeed, m_moveSpeedMin, m_moveSpeedMax, "%.2f u/s");
        if (ImGui::Button("Reset Camera")) {
            m_eye = ST::Vector3(1.6f, 1.0f, 2.5f);
            m_yaw = 0.6f;
            m_pitch = 0.35f;
            m_moveSpeed = 2.5f;
            changed = true;
        }
        ImGui::BulletText("LMB drag in canvas to look.");
        ImGui::BulletText("RMB drag also looks; hold RMB + WASD/QE to fly.");
        ImGui::BulletText("Shift = sprint, Wheel = change speed.");
    }

    if (ImGui::CollapsingHeader("Performance")) {
        const double averageFps = m_averageFrameTimeMs > 0.001
            ? 1000.0 / m_averageFrameTimeMs : 0.0;
        const double minimumFps = m_maxFrameTimeMs > 0.001
            ? 1000.0 / m_maxFrameTimeMs : 0.0;
        const double maximumFps = m_minFrameTimeMs > 0.001
            ? 1000.0 / m_minFrameTimeMs : 0.0;
        const int cacheSamples = m_lastCacheHits + m_lastCacheMisses;
        const double cacheHitRate = cacheSamples > 0
            ? 100.0 * static_cast<double>(m_lastCacheHits) / cacheSamples : 0.0;
        ImGui::Text("Current: %.2f ms (%.1f FPS)", m_frameTimeMs, m_fps);
        ImGui::Text("30-frame average: %.2f ms (%.1f FPS)",
                    m_averageFrameTimeMs, averageFps);
        ImGui::Text("Range: %.1f - %.1f FPS", minimumFps, maximumFps);
        ImGui::Text("Stages: geometry %.2f ms | raster/PBR %.2f ms | upload %.2f ms",
                    m_lastVertexStageMs, m_lastRasterStageMs, m_lastUploadStageMs);
        ImGui::Text("Vertex cache: %d hits / %d misses (%.0f%% hit)",
                    m_lastCacheHits, m_lastCacheMisses, cacheHitRate);
        if (m_renderQuality == 0 && m_adaptivePreview) {
            ImGui::TextDisabled("Adaptive preview active: last frame exceeded 30 FPS budget");
        }
    }

    if (ImGui::CollapsingHeader("Lighting (GGX PBR)", defaultOpen)) {
        changed |= ImGui::Checkbox("Enable lighting", &m_lightingEnabled);
        changed |= ImGui::Checkbox("Show light gizmo", &m_showLightGizmo);
        changed |= ImGui::Checkbox("Flat shading", &m_flatShading);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Off: interpolate vertex normals (smooth)\nOn: use one geometric normal per triangle");
        }
        const char* qualityLabels[] = { "Adaptive", "Preview", "Final" };
        changed |= ImGui::Combo("Render quality", &m_renderQuality,
                                qualityLabels, IM_ARRAYSIZE(qualityLabels));
        changed |= ImGui::Checkbox("2x final supersampling", &m_supersampleEnabled);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Environment-map PBR automatically uses 1x to keep the software renderer responsive.");
        }
        if (m_renderQuality == 1) {
            ImGui::TextDisabled("Preview: 1x, reduced material sampling, no environment reflection");
        } else if (m_renderQuality == 2) {
            ImGui::TextDisabled("Final: full material sampling; supersampling follows the checkbox");
        } else if (m_adaptivePreview) {
            ImGui::TextDisabled("Adaptive preview active after a slow frame");
        } else if (m_environmentMapEnabled && m_environmentTexture.isValid() && m_supersampleEnabled) {
            ImGui::TextDisabled("Adaptive quality: 1x while environment map is enabled");
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
    }

    if (ImGui::CollapsingHeader("Material Inspector", defaultOpen)) {
        const SceneObject* selectedObjectForInspector =
            (m_selectedSceneObject >= 0 &&
             m_selectedSceneObject < static_cast<int>(m_sceneObjects.size()))
                ? &m_sceneObjects[m_selectedSceneObject] : nullptr;
        const bool hasImportedPartMaterials = selectedObjectForInspector &&
            !selectedObjectForInspector->partMaterials.empty();
        if (hasImportedPartMaterials) {
            ImGui::TextDisabled("FBX PBR materials are bound per mesh part");
            ImGui::TextWrapped("%s", selectedObjectForInspector->textureStatus.c_str());
            for (size_t partIndex = 0;
                 partIndex < selectedObjectForInspector->partMaterials.size(); ++partIndex) {
                const auto& part = selectedObjectForInspector->partMaterials[partIndex];
                const auto& meshPart = selectedObjectForInspector->model->parts[partIndex];
                const std::string& label = meshPart.materialName.empty()
                    ? meshPart.name : meshPart.materialName;
                int mapCount = 0;
                mapCount += part.diffuseTexture.isValid() ? 1 : 0;
                mapCount += part.roughnessTexture.isValid() ? 1 : 0;
                mapCount += part.metallicTexture.isValid() ? 1 : 0;
                mapCount += part.normalTexture.isValid() ? 1 : 0;
                ImGui::BulletText("%s: %d/4 PBR maps", label.c_str(), mapCount);
            }
        } else {
            changed |= ImGui::ColorEdit3("Base Color", &editedMaterial->diffuse.x);
            changed |= ImGui::SliderFloat("Metallic", &editedMaterial->metallicFactor, 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("Roughness", &editedMaterial->roughness, 0.02f, 1.0f);
            changed |= ImGui::SliderFloat("Normal strength", &editedMaterial->normalStrength, 0.0f, 2.0f);
            changed |= ImGui::ColorEdit3("Emission", &editedMaterial->emission.x);
        }
        if (!hasImportedPartMaterials && m_selectedSceneObject >= 0 &&
            m_selectedSceneObject < static_cast<int>(m_sceneObjects.size())) {
            SceneObject& selectedObject = m_sceneObjects[m_selectedSceneObject];
            const int selectedTexture = m_textureCatalog.findByRelativePath(selectedObject.diffuseTexturePath);
            const char* texturePreview = selectedTexture >= 0
                ? m_textureCatalog.getEntries()[selectedTexture].displayName.c_str()
                : (selectedObject.diffuseTexturePath.empty() ? "None (material color)" : "External texture");
            if (ImGui::BeginCombo("Base Color texture", texturePreview)) {
                const bool noneSelected = selectedObject.diffuseTexturePath.empty();
                if (ImGui::Selectable("None (material color)", noneSelected)) {
                    loadDiffuseTextureForObject(m_selectedSceneObject, -1);
                    changed = true;
                }
                if (noneSelected) ImGui::SetItemDefaultFocus();
                for (int i = 0; i < static_cast<int>(m_textureCatalog.getEntries().size()); ++i) {
                    const auto& entry = m_textureCatalog.getEntries()[i];
                    const bool isSelected = i == selectedTexture;
                    if (ImGui::Selectable(entry.displayName.c_str(), isSelected)) {
                        if (loadDiffuseTextureForObject(m_selectedSceneObject, i)) changed = true;
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", entry.relativePath.c_str());
                }
                ImGui::EndCombo();
            }
            if (!selectedObject.diffuseTexturePath.empty()) {
                ImGui::TextDisabled("%s", selectedObject.diffuseTexturePath.c_str());
            }
            auto scalarTextureCombo = [&](const char* label, std::string& path, bool metallic) {
                const int selectedMap = m_textureCatalog.findByRelativePath(path);
                const char* preview = selectedMap >= 0
                    ? m_textureCatalog.getEntries()[selectedMap].displayName.c_str()
                    : (path.empty() ? "None (uniform value)" : "External texture");
                if (ImGui::BeginCombo(label, preview)) {
                    const bool noneSelected = path.empty();
                    if (ImGui::Selectable("None (uniform value)", noneSelected)) {
                        if (loadScalarTextureForObject(m_selectedSceneObject, -1, metallic)) changed = true;
                    }
                    if (noneSelected) ImGui::SetItemDefaultFocus();
                    for (int i = 0; i < static_cast<int>(m_textureCatalog.getEntries().size()); ++i) {
                        const auto& entry = m_textureCatalog.getEntries()[i];
                        const bool isSelected = i == selectedMap;
                        if (ImGui::Selectable(entry.displayName.c_str(), isSelected)) {
                            if (loadScalarTextureForObject(m_selectedSceneObject, i, metallic)) changed = true;
                        }
                        if (isSelected) ImGui::SetItemDefaultFocus();
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", entry.relativePath.c_str());
                    }
                    ImGui::EndCombo();
                }
                if (!path.empty()) ImGui::TextDisabled("%s", path.c_str());
            };
            scalarTextureCombo("Roughness texture", selectedObject.roughnessTexturePath, false);
            scalarTextureCombo("Metallic texture", selectedObject.metallicTexturePath, true);
            const int selectedNormal = m_textureCatalog.findByRelativePath(selectedObject.normalTexturePath);
            const char* normalPreview = selectedNormal >= 0
                ? m_textureCatalog.getEntries()[selectedNormal].displayName.c_str()
                : (selectedObject.normalTexturePath.empty() ? "None (flat surface normal)" : "External texture");
            if (ImGui::BeginCombo("Normal texture", normalPreview)) {
                const bool noneSelected = selectedObject.normalTexturePath.empty();
                if (ImGui::Selectable("None (flat surface normal)", noneSelected)) {
                    if (loadNormalTextureForObject(m_selectedSceneObject, -1)) changed = true;
                }
                if (noneSelected) ImGui::SetItemDefaultFocus();
                for (int i = 0; i < static_cast<int>(m_textureCatalog.getEntries().size()); ++i) {
                    const auto& entry = m_textureCatalog.getEntries()[i];
                    const bool isSelected = i == selectedNormal;
                    if (ImGui::Selectable(entry.displayName.c_str(), isSelected)) {
                        if (loadNormalTextureForObject(m_selectedSceneObject, i)) changed = true;
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", entry.relativePath.c_str());
                }
                ImGui::EndCombo();
            }
            if (!selectedObject.normalTexturePath.empty()) {
                ImGui::TextDisabled("%s", selectedObject.normalTexturePath.c_str());
            }
            if (ImGui::Button("Refresh texture list")) {
                scanTextureCatalog();
                changed = true;
            }
        }
        if (hasImportedPartMaterials && ImGui::Button("Refresh texture list")) {
            scanTextureCatalog();
            changed = true;
        }
    }

    if (ImGui::CollapsingHeader("Advanced Lighting")) {
        changed |= ImGui::ColorEdit3("Ambient", &editedMaterial->ambient.x);
        changed |= ImGui::ColorEdit3("Specular", &editedMaterial->specular.x);
        changed |= ImGui::SliderFloat("Legacy shininess", &editedMaterial->shininess, 1.0f, 256.0f);
        changed |= ImGui::ColorEdit3("Ambient light", &m_ambientLight.x);
        changed |= ImGui::ColorEdit3("Environment color", &m_environmentColor.x);
        changed |= ImGui::SliderFloat("Environment intensity", &m_environmentIntensity, 0.0f, 5.0f);
        changed |= ImGui::Checkbox("Environment map", &m_environmentMapEnabled);
        if (m_environmentMapEnabled && m_environmentTexture.isValid()) {
            ImGui::TextDisabled("%s", m_environmentTexturePath.c_str());
        }
        changed |= ImGui::Checkbox("Tone mapping", &m_toneMappingEnabled);
        changed |= ImGui::SliderFloat("Exposure", &m_exposure, 0.0f, 5.0f);
        if (ImGui::Button("Reset Light")) {
            m_light.direction = ST::Vector3(-0.4f, -1.0f, -0.6f).normalized();
            m_light.color = ST::Color::white();
            m_light.intensity = 1.4f;
            syncLightAnglesFromDirection();
            changed = true;
        }
    }

    if (ImGui::CollapsingHeader("Shader")) changed |= renderShaderControls();
    if (ImGui::CollapsingHeader("Model")) changed |= renderModelControls();
    if (!m_sceneWarning.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "Scene warning");
        ImGui::TextWrapped("%s", m_sceneWarning.c_str());
    }
    if (changed) {
        markSceneDirty();
        needsRerender = true;
    }
    return changed;
}

bool TestModule_3DRender::renderSceneObjectControls() {
    bool changed = false;
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

void TestModule_3DRender::drawSelectionOutline(SDL_Renderer* renderer,
                                               int canvasW, int canvasH,
                                               const ST::Matrix4x4& view,
                                               const ST::Matrix4x4& projection,
                                               const ST::Matrix4x4& model) {
    if (!renderer || m_cleanPreview || !m_showSelectionOutline || m_selectedSceneObject < 0 ||
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

    const ST::Vector3& min = object.model->boundsMin;
    const ST::Vector3& max = object.model->boundsMax;
    const ST::Vector3 corners[8] = {
        {min.x, min.y, min.z}, {max.x, min.y, min.z},
        {max.x, max.y, min.z}, {min.x, max.y, min.z},
        {min.x, min.y, max.z}, {max.x, min.y, max.z},
        {max.x, max.y, max.z}, {min.x, max.y, max.z}
    };
    int screen[8][2]{};
    for (int i = 0; i < 8; ++i) {
        if (!project((model * ST::Vector4(corners[i], 1.0f)).toVector3(),
                     screen[i][0], screen[i][1])) return;
    }
    static constexpr int edges[12][2] = {
        {0,1}, {1,2}, {2,3}, {3,0},
        {4,5}, {5,6}, {6,7}, {7,4},
        {0,4}, {1,5}, {2,6}, {3,7}
    };
    SDL_SetRenderDrawColor(renderer, 255, 215, 70, 255);
    for (const auto& edge : edges) {
        SDL_RenderDrawLine(renderer,
                           screen[edge[0]][0], screen[edge[0]][1],
                           screen[edge[1]][0], screen[edge[1]][1]);
    }
}

void TestModule_3DRender::drawTransformGizmo(SDL_Renderer* renderer,
                                             int canvasW, int canvasH,
                                             const ST::Matrix4x4& view,
                                             const ST::Matrix4x4& projection,
                                             const ST::Matrix4x4& model) {
    m_transformGizmoValid = false;
    if (!renderer || m_cleanPreview || !m_showTransformGizmo || m_selectedSceneObject < 0 ||
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
    if (!renderer || m_cleanPreview || !m_showLightGizmo || !m_lightingEnabled) return;

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

    const auto& entries = m_modelCatalog.getEntries();
    if (entries.empty()) {
        ImGui::TextDisabled("No OBJ/FBX model assets found in %s", m_modelRoot.c_str());
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
            ImGui::TextDisabled("%s | format: %s",
                                entries[m_selectedModelIndex].relativePath.c_str(),
                                entries[m_selectedModelIndex].format.c_str());
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
        ImGui::TextDisabled("Active: built-in GGX PBR shader");
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

    const float sens = m_cameraSensitivity;
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
    markSceneDirty();
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
    if (button == SDL_BUTTON_LEFT && !m_cleanPreview && m_transformGizmoValid && m_showTransformGizmo &&
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
    if (button == SDL_BUTTON_LEFT && !m_cleanPreview && m_showLightGizmo && m_lightingEnabled) {
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
    if (button == SDL_BUTTON_LEFT) {
        const int pickedObject = pickSceneObject(canvasX, canvasY);
        if (pickedObject >= 0) {
            selectSceneObject(pickedObject);
        } else {
            // Clicking empty canvas clears the hierarchy selection, while the
            // same press still starts normal camera orbiting below.
            selectSceneObject(-1);
        }
        needsRerender = true;
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
        markSceneDirty();
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
        markSceneDirty();
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
    if (keycode == SDLK_f) focusSelectedSceneObject();
}
