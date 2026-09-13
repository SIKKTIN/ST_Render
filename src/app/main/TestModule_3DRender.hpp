#pragma once

#include "app/module/IModule.hpp"
#include "renderer/buffer/FrameBuffer.hpp"
#include "renderer/buffer/DepthBuffer.hpp"
#include "renderer/pipeline/VertexShader.hpp"
#include "renderer/pipeline/Rasterizer.hpp"
#include "renderer/pipeline/FragmentShader.hpp"
#include "renderer/shader/ShaderCatalog.hpp"
#include "renderer/shader/ShaderManager.hpp"
#include "renderer/shader/ShaderProgram.hpp"
#include "renderer/asset/ModelAsset.hpp"
#include "renderer/asset/ModelCatalog.hpp"
#include "renderer/asset/TextureCatalog.hpp"
#include "renderer/asset/ObjModelLoader.hpp"
#include "core/texture/ST_Image.hpp"
#include "renderer/geometry/Vertex.hpp"
#include "renderer/geometry/Mesh.hpp"
#include "core/math/Matrix4x4.hpp"
#include "core/math/Vector3.hpp"
#include <SDL2/SDL.h>
#include <memory>
#include <string>
#include <vector>

// Minimal 3D render module.
// Renders a unit cube centered at the origin (edge length 1) using
// the existing software rasterizer pipeline. Right-mouse drag orbits
// the camera around the origin.
class TestModule_3DRender : public IModule {
public:
    TestModule_3DRender();
    ~TestModule_3DRender();

    const char* getName() const override { return "3D Render"; }
    bool needsRealTimeUpdate() const override { return true; }
    double getFrameTimeMs() const { return m_frameTimeMs; }
    double getFps() const { return m_fps; }
    bool isInteractionActive() const { return m_interactionActive; }

    void update(float deltaTime) override;
    void render(void* canvasTexture, int canvasW, int canvasH) override;
    bool renderControls() override;
    void renderCreatePanel() override;
    const std::vector<ST::ShaderEntry>& getShaderEntries() const { return m_shaderCatalog.getEntries(); }
    int getSelectedShaderIndex() const { return m_selectedShaderIndex; }
    const std::string& getShaderError() const { return m_shaderError; }
    bool selectShaderIndex(int index);
    void useBuiltinShader();
    const std::vector<ST::ModelEntry>& getModelEntries() const { return m_modelCatalog.getEntries(); }
    const std::vector<ST::TextureEntry>& getTextureEntries() const { return m_textureCatalog.getEntries(); }
    bool selectMaterialTexture(const std::string& slot, int textureIndex);
    std::string getSelectedMaterialTexturePath(const std::string& slot) const;
    int getSelectedModelIndex() const { return m_selectedModelIndex; }
    const std::string& getModelError() const { return m_modelError; }
    const std::string& getModelTextureStatus() const { return m_modelTextureStatus; }
    bool selectModelIndex(int index);
    const ST::ModelAsset* getActiveModel() const;
    struct SceneObjectInfo {
        int id;
        std::string name;
        int modelIndex;
        std::string modelPath;
        ST::Vector3 position;
        ST::Vector3 rotation;
        ST::Vector3 scale;
        bool visible;
        bool selected;
    };
    std::vector<SceneObjectInfo> getSceneObjectInfos() const;
    int getSelectedSceneObjectIndex() const { return m_selectedSceneObject; }
    bool addModelToScene(int modelIndex) { return createSceneObject(modelIndex); }
    bool selectSceneObjectIndex(int objectIndex);
    bool duplicateSelectedObject();
    bool deleteSelectedObject();
    bool setSceneObjectTransform(int objectIndex,
                                 const ST::Vector3* position,
                                 const ST::Vector3* rotation,
                                 const ST::Vector3* scale);
    const ST::Light& getLight() const { return m_light; }
    bool setLightDirection(const ST::Vector3& direction);
    void setLightIntensity(float intensity);
    struct EditorSettings {
        // 0 = Adaptive, 1 = Preview, 2 = Final.
        int renderQuality = 0;
        bool supersampleEnabled = true;
        bool flatShading = false;
        bool showLightGizmo = true;
        bool showTransformGizmo = true;
        bool showSelectionOutline = true;
        float cameraSpeed = 2.5f;
        float cameraSensitivity = 0.01f;
    };
    EditorSettings getEditorSettings() const;
    void applyEditorSettings(const EditorSettings& settings);
    bool saveScene(const std::string& path, std::string& error) const;
    bool loadScene(const std::string& path, std::string& error);
    const std::string& getSceneWarning() const { return m_sceneWarning; }
    bool isSceneDirty() const { return m_sceneDirty; }
    void markSceneSaved() { m_sceneDirty = false; m_sceneWarning.clear(); }

    void onMouseDown(int button, int x, int y) override;
    void onMouseUp(int button) override;
    void onMouseMove(int x, int y) override;
    void onCanvasMouseDown(int button, int canvasX, int canvasY) override;
    void onCanvasMouseUp(int button, int canvasX, int canvasY) override;
    void onCanvasMouseMove(int canvasX, int canvasY) override;
    void onWheel(float dx, float dy, int canvasX, int canvasY, int canvasW, int canvasH) override;
    void onKeyDown(int keycode) override;

private:
    void rebuildBuffers(int canvasW, int canvasH);
    void drawMesh(const ST::Mesh& mesh,
                  const ST::Matrix4x4& model,
                  const ST::Matrix4x4& view,
                  const ST::Matrix4x4& projection);
    void scanShaderCatalog();
    void loadSelectedShader();
    void pollShaderReload();
    bool renderShaderControls();
    void scanModelCatalog();
    void scanTextureCatalog();
    bool loadSelectedModel();
    bool renderModelControls();
    bool renderSceneObjectControls();
    bool createSceneObject(int modelIndex);
    bool replaceSceneObjectModel(int objectIndex, int modelIndex);
    bool loadDiffuseTextureForObject(int objectIndex, int textureIndex);
    bool loadScalarTextureForObject(int objectIndex, int textureIndex, bool metallic);
    bool loadNormalTextureForObject(int objectIndex, int textureIndex);
    void duplicateSelectedSceneObject();
    void deleteSelectedSceneObject();
    void selectSceneObject(int objectIndex);
    ST::Matrix4x4 buildSceneObjectMatrix(int objectIndex) const;
    void bindSceneObjectMaterial(int objectIndex);
    void drawLightGizmo(SDL_Renderer* renderer,
                        int canvasW, int canvasH,
                        const ST::Matrix4x4& view,
                        const ST::Matrix4x4& projection,
                        const ST::Matrix4x4& model);
    void drawTransformGizmo(SDL_Renderer* renderer,
                            int canvasW, int canvasH,
                            const ST::Matrix4x4& view,
                            const ST::Matrix4x4& projection,
                            const ST::Matrix4x4& model);
    void drawSelectionOutline(SDL_Renderer* renderer,
                              int canvasW, int canvasH,
                              const ST::Matrix4x4& view,
                              const ST::Matrix4x4& projection,
                              const ST::Matrix4x4& model);
    int pickSceneObject(int canvasX, int canvasY) const;
    void focusSelectedSceneObject();
    void syncLightAnglesFromDirection();
    void updateLightDirectionFromAngles();
    void markSceneDirty() { m_sceneDirty = true; }

    ST::FrameBuffer* m_frameBuffer;
    ST::DepthBuffer* m_depthBuffer;
    ST::Rasterizer m_rasterizer;
    ST::VertexShader m_vertexShader;
    ST::FragmentShader m_fragmentShader;
    ST::Material m_material;
    ST::Light m_light;
    ST::Vector3 m_ambientLight;
    ST::Vector3 m_environmentColor = ST::Vector3(0.16f, 0.2f, 0.28f);
    float m_environmentIntensity = 0.35f;
    bool m_toneMappingEnabled = true;
    float m_exposure = 1.0f;
    int m_renderQuality = 0;
    double m_frameTimeMs = 0.0;
    double m_fps = 0.0;
    bool m_environmentMapEnabled = true;
    ST::Image m_environmentTexture;
    std::string m_environmentTexturePath;
    bool m_lightingEnabled;
    bool m_flatShading = false;
    bool m_supersampleEnabled = true;
    bool m_showLightGizmo = true;
    bool m_lightDragActive = false;
    int m_lightGizmoScreenX = 0;
    int m_lightGizmoScreenY = 0;
    float m_lightGizmoHitRadius = 24.0f;
    float m_lightYaw = 0.0f;
    float m_lightPitch = 0.0f;
    enum class TransformTool { Translate, Rotate, Scale };
    TransformTool m_transformTool = TransformTool::Translate;
    bool m_showTransformGizmo = true;
    bool m_showSelectionOutline = true;
    int m_transformGizmoAxis = -1;
    int m_transformGizmoCenterX = 0;
    int m_transformGizmoCenterY = 0;
    int m_transformGizmoEndX[3] = { 0, 0, 0 };
    int m_transformGizmoEndY[3] = { 0, 0, 0 };
    bool m_transformGizmoValid = false;

    struct SceneObject {
        int id = 0;
        std::string name;
        int modelIndex = -1;
        std::string modelPath;
        std::shared_ptr<ST::ModelAsset> model;
        ST::Image diffuseTexture;
        std::string diffuseTexturePath;
        ST::Image roughnessTexture;
        std::string roughnessTexturePath;
        ST::Image metallicTexture;
        std::string metallicTexturePath;
        ST::Image normalTexture;
        std::string normalTexturePath;
        ST::Material material = ST::Material::defaultMaterial();
        std::string textureStatus;
        ST::Vector3 position = ST::Vector3::zero();
        ST::Vector3 rotation = ST::Vector3::zero();
        ST::Vector3 scale = ST::Vector3(1.0f, 1.0f, 1.0f);
        bool visible = true;
    };

    ST::Mesh m_cube;
    std::vector<SceneObject> m_sceneObjects;
    int m_selectedSceneObject = -1;
    int m_nextSceneObjectId = 1;
    int m_addModelIndex = -1;
    bool m_modelLoaded = false;
    bool m_sceneDirty = false;
    std::string m_sceneWarning;
    ST::ModelCatalog m_modelCatalog;
    ST::TextureCatalog m_textureCatalog;
    int m_selectedModelIndex = -1;
    std::string m_modelError;
    std::string m_modelRoot = "Data/Models";
    std::string m_textureRoot = "Data/Textures";
    std::string m_modelTextureStatus;
    ST::ShaderCatalog m_shaderCatalog;
    ST::ShaderManager m_shaderManager;
    std::shared_ptr<ST::IShaderProgram> m_builtinShader;
    std::shared_ptr<ST::IShaderProgram> m_activeShader;
    int m_selectedShaderIndex = -1;
    std::string m_shaderError;
    std::string m_shaderRoot = "Data/Shaders";

    // Fly camera state (UE-style):
    //   - LMB drag: rotate view (yaw/pitch)
    //   - RMB drag also rotates (so you can fly + look simultaneously)
    //   - RMB hold + WASD/QE: move in the camera's local frame
    //   - Wheel: adjust movement speed
    //   - Shift: hold to sprint
    ST::Vector3 m_eye;
    float m_yaw;          // around world Y (radians)
    float m_pitch;        // around camera-right axis (radians)
    float m_moveSpeed;    // WASD units/sec
    float m_moveSpeedMin;
    float m_moveSpeedMax;
    float m_cameraSensitivity = 0.01f;
    bool  m_lmbDown;
    bool  m_rmbDown;
    bool m_interactionActive = false;
    int   m_lastCanvasX;
    int   m_lastCanvasY;

    int m_canvasW;
    int m_canvasH;
    int m_inputCanvasW = 640;
    int m_inputCanvasH = 480;
    // Reused per-draw vertex transform cache. Indexed meshes otherwise
    // transform the same vertex once for every triangle that references it.
    std::vector<ST::VertexOut> m_vertexCache;
    std::vector<uint32_t> m_rgba32Buffer;
    SDL_Renderer* m_sdlRenderer = nullptr;
    SDL_Texture* m_outputTexture = nullptr;
    int m_outputTextureW = 0;
    int m_outputTextureH = 0;
};
