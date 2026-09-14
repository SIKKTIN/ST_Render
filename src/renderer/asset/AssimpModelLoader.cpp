#include "renderer/asset/AssimpModelLoader.hpp"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <limits>

namespace ST {
namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

Vector3 toVector(const aiVector3D& value) {
    return Vector3(value.x, value.y, value.z);
}

Color toColor(const aiColor3D& value) {
    return Color(value.r, value.g, value.b, 1.0f);
}

bool hasPartName(const std::string& materialName, const char* token) {
    return lower(materialName).find(lower(token)) != std::string::npos;
}

std::string m1911PartName(const std::string& materialName) {
    if (hasPartName(materialName, "barrel")) return "Barrel";
    if (hasPartName(materialName, "frame")) return "Frame";
    if (hasPartName(materialName, "grip")) return "Grip";
    if (hasPartName(materialName, "slide")) return "Slide";
    return {};
}

void setM1911TexturePaths(ModelMaterial& material,
                          const std::filesystem::path& modelPath) {
    const std::string partName = m1911PartName(material.name);
    if (partName.empty()) return;

    const std::filesystem::path m1911Root = modelPath.parent_path().parent_path();
    const std::filesystem::path textureRoot =
        m1911Root / "pbr_textures" / "PBR_Textures" / "M1911";
    const std::string prefix = "M1911_low_" + partName + "_";
    const auto assignIfPresent = [&](const char* suffix, std::string& target) {
        const std::filesystem::path candidate = textureRoot / (prefix + suffix + ".png");
        // Keep the stable catalog-relative prefix used by TextureCatalog.
        // The files are intentionally not copied into Data/Textures.
        std::error_code ec;
        const auto relativeToM1911 = std::filesystem::relative(candidate, m1911Root, ec);
        target = ec ? std::string() :
            (std::filesystem::path("M1911") / relativeToM1911).generic_string();
    };

    assignIfPresent("BaseColor", material.diffuseTexturePath);
    assignIfPresent("Roughness", material.roughnessTexturePath);
    assignIfPresent("Metallic", material.metallicTexturePath);
    assignIfPresent("Normal", material.normalTexturePath);
}

void readMaterial(const aiMaterial* source,
                  ModelMaterial& material,
                  const std::filesystem::path& modelPath) {
    aiString name;
    if (source->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) material.name = name.C_Str();

    aiColor3D color;
    if (source->Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS ||
        source->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
        material.diffuse = toColor(color);
    }
    if (source->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS) {
        material.ambient = toColor(color);
    }
    if (source->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) {
        material.specular = toColor(color);
    }

    float value = 0.0f;
    if (source->Get(AI_MATKEY_SHININESS, value) == AI_SUCCESS) {
        material.shininess = std::max(1.0f, value);
    }
    if (source->Get(AI_MATKEY_METALLIC_FACTOR, value) == AI_SUCCESS) {
        material.metallicFactor = std::clamp(value, 0.0f, 1.0f);
    }
    if (source->Get(AI_MATKEY_ROUGHNESS_FACTOR, value) == AI_SUCCESS) {
        material.roughnessFactor = std::clamp(value, 0.02f, 1.0f);
    }

    setM1911TexturePaths(material, modelPath);
}

} // namespace

bool AssimpModelLoader::load(const std::string& path,
                             ModelAsset& asset,
                             std::string& error) {
    Assimp::Importer importer;
    const unsigned int flags = aiProcess_Triangulate |
                               aiProcess_JoinIdenticalVertices |
                               aiProcess_CalcTangentSpace |
                               aiProcess_GenSmoothNormals |
                               aiProcess_PreTransformVertices;
    const aiScene* scene = importer.ReadFile(path, flags);
    if (!scene || !scene->HasMeshes()) {
        error = importer.GetErrorString();
        if (error.empty()) error = "FBX contains no meshes: " + path;
        return false;
    }

    asset = {};
    asset.sourcePath = path;
    const std::filesystem::path modelPath(path);
    asset.materials.resize(scene->mNumMaterials);
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
        readMaterial(scene->mMaterials[i], asset.materials[i], modelPath);
    }

    Vector3 boundsMin(std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max(),
                     std::numeric_limits<float>::max());
    Vector3 boundsMax(std::numeric_limits<float>::lowest(),
                     std::numeric_limits<float>::lowest(),
                     std::numeric_limits<float>::lowest());

    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
        const aiMesh* source = scene->mMeshes[meshIndex];
        if (!source || source->mNumVertices == 0 || source->mNumFaces == 0) continue;

        ModelPart part;
        part.name = source->mName.length > 0
            ? source->mName.C_Str()
            : "mesh_" + std::to_string(meshIndex);
        part.materialIndex = static_cast<int>(source->mMaterialIndex);
        if (part.materialIndex >= 0 &&
            part.materialIndex < static_cast<int>(asset.materials.size())) {
            part.materialName = asset.materials[part.materialIndex].name;
        }

        for (unsigned int vertexIndex = 0; vertexIndex < source->mNumVertices; ++vertexIndex) {
            Vertex vertex;
            vertex.position = toVector(source->mVertices[vertexIndex]);
            vertex.normal = source->HasNormals()
                ? toVector(source->mNormals[vertexIndex]).normalized()
                : Vector3::forward();
            vertex.tangent = source->HasTangentsAndBitangents()
                ? toVector(source->mTangents[vertexIndex]).normalized()
                : Vector3(1.0f, 0.0f, 0.0f);
            if (source->HasTangentsAndBitangents()) {
                const Vector3 bitangent = toVector(source->mBitangents[vertexIndex]).normalized();
                vertex.tangentSign = vertex.normal.cross(vertex.tangent).dot(bitangent) < 0.0f
                    ? -1.0f : 1.0f;
            }
            if (source->HasTextureCoords(0)) {
                // M1911 textures are loaded with their source orientation
                // preserved, so keep the FBX UVs unchanged.
                vertex.texCoord = Vector2(source->mTextureCoords[0][vertexIndex].x,
                                          source->mTextureCoords[0][vertexIndex].y);
            }
            part.mesh.addVertex(vertex);
            boundsMin.x = std::min(boundsMin.x, vertex.position.x);
            boundsMin.y = std::min(boundsMin.y, vertex.position.y);
            boundsMin.z = std::min(boundsMin.z, vertex.position.z);
            boundsMax.x = std::max(boundsMax.x, vertex.position.x);
            boundsMax.y = std::max(boundsMax.y, vertex.position.y);
            boundsMax.z = std::max(boundsMax.z, vertex.position.z);
        }

        for (unsigned int faceIndex = 0; faceIndex < source->mNumFaces; ++faceIndex) {
            const aiFace& face = source->mFaces[faceIndex];
            if (face.mNumIndices != 3) continue;
            part.mesh.addTriangle(static_cast<int>(face.mIndices[0]),
                                  static_cast<int>(face.mIndices[1]),
                                  static_cast<int>(face.mIndices[2]));
        }
        if (part.mesh.getTriangleCount() > 0) asset.parts.push_back(std::move(part));
    }

    if (asset.parts.empty()) {
        error = "FBX contains no triangle meshes: " + path;
        return false;
    }

    asset.boundsMin = boundsMin;
    asset.boundsMax = boundsMax;
    asset.boundsCenter = (boundsMin + boundsMax) * 0.5f;
    asset.boundsRadius = (boundsMax - asset.boundsCenter).length();
    error.clear();
    return true;
}

} // namespace ST
