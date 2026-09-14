#include "TestModule_3DRender.hpp"

#include <algorithm>
#include <filesystem>
#include <utility>

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
    const std::vector<ST::ModelRoot> roots = {
        {m_modelRoot, {}},
        {"Data/M1911/source", "M1911/source"}
    };
    if (!m_modelCatalog.scan(roots, error)) {
        const std::string fallback = "../../Data/Models";
        const std::vector<ST::ModelRoot> fallbackRoots = {
            {fallback, {}},
            {"../../Data/M1911/source", "M1911/source"}
        };
        if (!m_modelCatalog.scan(fallbackRoots, error)) {
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
        // Keep the historical sphere fixture as the default startup model.
        // Newly discovered FBX assets must be opt-in so adding a large asset
        // directory cannot make the editor import it during construction.
        for (int i = 0; i < static_cast<int>(m_modelCatalog.getEntries().size()); ++i) {
            const auto& entry = m_modelCatalog.getEntries()[i];
            if (entry.format == "obj" && entry.displayName == "sphere.obj") {
                m_addModelIndex = i;
                break;
            }
        }
        if (m_addModelIndex < 0) {
            for (int i = 0; i < static_cast<int>(m_modelCatalog.getEntries().size()); ++i) {
                if (m_modelCatalog.getEntries()[i].format == "obj") {
                    m_addModelIndex = i;
                    break;
                }
            }
        }
        if (m_addModelIndex < 0) m_addModelIndex = 0;
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

void TestModule_3DRender::scanTextureCatalog() {
    std::string error;
    const std::vector<ST::TextureRoot> roots = {
        {m_textureRoot, {}},
        {"Data/M1911/pbr_textures", "M1911/pbr_textures"}
    };
    if (m_textureCatalog.scan(roots, error)) return;

    const std::string fallback = "../../Data/Textures";
    const std::vector<ST::TextureRoot> fallbackRoots = {
        {fallback, {}},
        {"../../Data/M1911/pbr_textures", "M1911/pbr_textures"}
    };
    if (m_textureCatalog.scan(fallbackRoots, error)) m_textureRoot = fallback;
}

bool TestModule_3DRender::loadDiffuseTextureForObject(int objectIndex, int textureIndex) {
    if (objectIndex < 0 || objectIndex >= static_cast<int>(m_sceneObjects.size())) return false;
    SceneObject& object = m_sceneObjects[objectIndex];
    if (textureIndex < 0) {
        object.diffuseTexture.clear();
        object.diffuseTexturePath.clear();
        object.textureStatus = "Diffuse texture disabled; using material color";
        m_modelTextureStatus = object.textureStatus;
        needsRerender = true;
        return true;
    }

    const auto& entries = m_textureCatalog.getEntries();
    if (textureIndex >= static_cast<int>(entries.size())) return false;
    if (!object.diffuseTexture.load(entries[textureIndex].absolutePath.c_str())) {
        object.textureStatus = "Diffuse texture failed to load: " + entries[textureIndex].relativePath;
        m_modelTextureStatus = object.textureStatus;
        return false;
    }
    object.diffuseTexturePath = entries[textureIndex].relativePath;
    object.textureStatus = "Diffuse texture: " + object.diffuseTexturePath;
    m_modelTextureStatus = object.textureStatus;
    needsRerender = true;
    return true;
}

bool TestModule_3DRender::loadScalarTextureForObject(int objectIndex, int textureIndex, bool metallic) {
    if (objectIndex < 0 || objectIndex >= static_cast<int>(m_sceneObjects.size())) return false;
    SceneObject& object = m_sceneObjects[objectIndex];
    ST::Image& image = metallic ? object.metallicTexture : object.roughnessTexture;
    std::string& path = metallic ? object.metallicTexturePath : object.roughnessTexturePath;
    const char* label = metallic ? "Metallic" : "Roughness";
    if (textureIndex < 0) {
        image.clear();
        path.clear();
        needsRerender = true;
        return true;
    }

    const auto& entries = m_textureCatalog.getEntries();
    if (textureIndex >= static_cast<int>(entries.size()) ||
        !image.load(entries[textureIndex].absolutePath.c_str())) {
        return false;
    }
    path = entries[textureIndex].relativePath;
    object.textureStatus = std::string(label) + " texture: " + path;
    m_modelTextureStatus = object.textureStatus;
    needsRerender = true;
    return true;
}

bool TestModule_3DRender::loadNormalTextureForObject(int objectIndex, int textureIndex) {
    if (objectIndex < 0 || objectIndex >= static_cast<int>(m_sceneObjects.size())) return false;
    SceneObject& object = m_sceneObjects[objectIndex];
    if (textureIndex < 0) {
        object.normalTexture.clear();
        object.normalTexturePath.clear();
        needsRerender = true;
        return true;
    }
    const auto& entries = m_textureCatalog.getEntries();
    if (textureIndex >= static_cast<int>(entries.size()) ||
        !object.normalTexture.load(entries[textureIndex].absolutePath.c_str())) {
        return false;
    }
    object.normalTexturePath = entries[textureIndex].relativePath;
    object.textureStatus = "Normal texture: " + object.normalTexturePath;
    m_modelTextureStatus = object.textureStatus;
    needsRerender = true;
    return true;
}

bool TestModule_3DRender::selectMaterialTexture(const std::string& slot, int textureIndex) {
    if (m_selectedSceneObject < 0 ||
        m_selectedSceneObject >= static_cast<int>(m_sceneObjects.size())) return false;
    bool success = false;
    if (slot == "diffuse" || slot == "baseColor") {
        success = loadDiffuseTextureForObject(m_selectedSceneObject, textureIndex);
    } else if (slot == "roughness") {
        success = loadScalarTextureForObject(m_selectedSceneObject, textureIndex, false);
    } else if (slot == "metallic") {
        success = loadScalarTextureForObject(m_selectedSceneObject, textureIndex, true);
    } else if (slot == "normal") {
        success = loadNormalTextureForObject(m_selectedSceneObject, textureIndex);
    } else {
        return false;
    }
    if (success) {
        markSceneDirty();
        needsRerender = true;
    }
    return success;
}

std::string TestModule_3DRender::getSelectedMaterialTexturePath(const std::string& slot) const {
    if (m_selectedSceneObject < 0 ||
        m_selectedSceneObject >= static_cast<int>(m_sceneObjects.size())) return {};
    const SceneObject& object = m_sceneObjects[m_selectedSceneObject];
    if (slot == "diffuse" || slot == "baseColor") return object.diffuseTexturePath;
    if (slot == "roughness") return object.roughnessTexturePath;
    if (slot == "metallic") return object.metallicTexturePath;
    if (slot == "normal") return object.normalTexturePath;
    return {};
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
    if (!ST::ModelLoader::load(entries[modelIndex].absolutePath, loaded, error)) {
        m_modelError = error;
        return false;
    }

    // The loader creates a new mesh allocation. Clear transformed entries so
    // allocator address reuse cannot make a stale vertex cache look valid.
    m_vertexTransformCaches.clear();
    SceneObject& object = m_sceneObjects[objectIndex];
    const bool modelChanged = object.modelPath != entries[modelIndex].relativePath;
    object.modelIndex = modelIndex;
    object.modelPath = entries[modelIndex].relativePath;
    object.model = std::make_shared<ST::ModelAsset>(std::move(loaded));
    if (object.name.empty() || modelChanged) {
        object.name = entries[modelIndex].displayName + " " + std::to_string(object.id);
    }
    object.material = ST::Material::defaultMaterial();
    object.diffuseTexture.clear();
    object.diffuseTexturePath.clear();
    object.roughnessTexture.clear();
    object.roughnessTexturePath.clear();
    object.metallicTexture.clear();
    object.metallicTexturePath.clear();
    object.normalTexture.clear();
    object.normalTexturePath.clear();
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
                    const int textureIndex = m_textureCatalog.findByRelativePath(material.diffuseTexturePath);
                    object.diffuseTexturePath = textureIndex >= 0
                        ? m_textureCatalog.getEntries()[textureIndex].relativePath
                        : material.diffuseTexturePath;
                    object.textureStatus = "Diffuse texture: " + object.diffuseTexturePath;
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
    if (entries[modelIndex].format == "fbx") {
        try {
            buildPartMaterials(object);
        } catch (const std::exception& exception) {
            object.partMaterials.clear();
            object.textureStatus = std::string("PBR import failed: ") + exception.what();
            m_modelError = object.textureStatus;
        }
    } else {
        object.partMaterials.clear();
    }
    // Imported assets can use very different unit scales. Reframe the camera
    // from the actual model bounds so a newly selected FBX is neither clipped
    // by the near plane nor rendered as a tiny or screen-filling object.
    if (m_selectedSceneObject == objectIndex || entries[modelIndex].format == "fbx") {
        m_selectedSceneObject = objectIndex;
        focusSelectedSceneObject();
    }
    selectSceneObject(objectIndex);
    markSceneDirty();
    needsRerender = true;
    return true;
}

void TestModule_3DRender::buildPartMaterials(SceneObject& object) {
    object.partMaterials.clear();
    if (!object.model || object.model->parts.empty()) return;

    object.partMaterials.resize(object.model->parts.size());
    int boundParts = 0;
    std::vector<std::string> missing;
    std::vector<std::string> names;

    for (size_t partIndex = 0; partIndex < object.model->parts.size(); ++partIndex) {
        const ST::ModelPart& part = object.model->parts[partIndex];
        if (part.materialIndex < 0 ||
            part.materialIndex >= static_cast<int>(object.model->materials.size())) {
            continue;
        }

        const ST::ModelMaterial& source = object.model->materials[part.materialIndex];
        SceneObject::PartMaterial& target = object.partMaterials[partIndex];
        target.material.ambient = source.ambient.rgb;
        target.material.diffuse = source.diffuse.rgb;
        target.material.specular = source.specular.rgb;
        target.material.shininess = std::max(1.0f, source.shininess);
        // A texture represents the channel value for the imported part. Use
        // a neutral factor of one when a map is present; otherwise the
        // ModelMaterial scalar remains the fallback for legacy assets.
        target.material.metallicFactor = source.metallicTexturePath.empty()
            ? std::clamp(source.metallicFactor, 0.0f, 1.0f) : 1.0f;
        target.material.roughness = source.roughnessTexturePath.empty()
            ? std::clamp(source.roughnessFactor, 0.02f, 1.0f) : 1.0f;
        target.material.normalStrength = std::clamp(source.normalStrength, 0.0f, 2.0f);
		// The supplied Grip material contains a shared metallic mask for small
		// fasteners, but the visible grip panel itself is wood/non-metal. Treat
		// the part as a dielectric so blue environment reflections cannot wash
		// out its brown base-color texture.
		const bool isGrip = source.name.find("Grip") != std::string::npos ||
			part.name.find("Grip") != std::string::npos;
		if (isGrip) {
			target.material.metallicFactor = 0.0f;
			target.material.roughness = std::max(target.material.roughness, 0.62f);
			// A small warm fill keeps the non-metal grip readable when the
			// studio panorama has no direct light on the handle.
			target.material.emission = ST::Vector3(0.018f, 0.005f, 0.001f);
		}

        const auto loadTexture = [&](const std::string& path,
                                     ST::Image& image,
                                     std::string& storedPath,
                                     const char* label) {
            if (path.empty()) return false;
            const int textureIndex = m_textureCatalog.findByRelativePath(path);
            const std::string resolvedPath = textureIndex >= 0
                ? m_textureCatalog.getEntries()[textureIndex].absolutePath
                : path;
            // M1911 source maps are 2048x2048. ST_Image stores decoded
            // pixels as float Colors, so cap imported FBX maps at 512 to keep
            // all four maps per part within a predictable software-renderer
            // memory budget while retaining enough detail for the viewport.
            // M1911's FBX UVs match the source texture orientation. Do not
            // apply the generic OBJ/image vertical flip to this atlas.
            if (!image.load(resolvedPath.c_str(), 512, false)) {
                missing.push_back(part.name + " " + label + ": " + path);
                return false;
            }
            storedPath = textureIndex >= 0
                ? m_textureCatalog.getEntries()[textureIndex].relativePath
                : path;
            return true;
        };

        const bool hasDiffuse = loadTexture(source.diffuseTexturePath,
                                            target.diffuseTexture,
                                            target.diffuseTexturePath,
                                            "Base Color");
        const bool hasRoughness = loadTexture(source.roughnessTexturePath,
                                              target.roughnessTexture,
                                              target.roughnessTexturePath,
                                              "Roughness");
        const bool hasMetallic = loadTexture(source.metallicTexturePath,
                                             target.metallicTexture,
                                             target.metallicTexturePath,
                                             "Metallic");
        const bool hasNormal = loadTexture(source.normalTexturePath,
                                           target.normalTexture,
                                           target.normalTexturePath,
                                           "Normal");
        target.bound = hasDiffuse || hasRoughness || hasMetallic || hasNormal;
        if (target.bound) {
            ++boundParts;
            names.push_back(source.name.empty() ? part.name : source.name);
        }
    }

    if (boundParts > 0) {
        object.textureStatus = "PBR parts: " + std::to_string(boundParts) + "/" +
                               std::to_string(object.partMaterials.size());
        if (!names.empty()) {
            object.textureStatus += " (";
            for (size_t i = 0; i < names.size(); ++i) {
                if (i > 0) object.textureStatus += ", ";
                object.textureStatus += names[i];
            }
            object.textureStatus += ")";
        }
    }
    if (!missing.empty()) {
        object.textureStatus += " | Missing: ";
        for (size_t i = 0; i < missing.size(); ++i) {
            if (i > 0) object.textureStatus += "; ";
            object.textureStatus += missing[i];
        }
    }
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
