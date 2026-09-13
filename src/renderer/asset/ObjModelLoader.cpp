#include "renderer/asset/ObjModelLoader.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <filesystem>

namespace ST {
namespace {

struct ObjIndex {
    int position = 0;
    int texCoord = 0;
    int normal = 0;

    bool operator==(const ObjIndex& other) const {
        return position == other.position && texCoord == other.texCoord && normal == other.normal;
    }
};

struct ObjIndexHash {
    size_t operator()(const ObjIndex& value) const {
        size_t h = static_cast<size_t>(value.position);
        h = h * 16777619u ^ static_cast<size_t>(value.texCoord);
        h = h * 16777619u ^ static_cast<size_t>(value.normal);
        return h;
    }
};

int resolveIndex(int index, int count) {
    if (index > 0) return index - 1;
    if (index < 0) return count + index;
    return -1;
}

bool parseFaceIndex(const std::string& token, ObjIndex& result) {
    result = {};
    std::stringstream stream(token);
    std::string component;
    if (!std::getline(stream, component, '/') || component.empty()) return false;
    try {
        result.position = std::stoi(component);
        if (std::getline(stream, component, '/') && !component.empty()) {
            result.texCoord = std::stoi(component);
        }
        if (std::getline(stream, component, '/') && !component.empty()) {
            result.normal = std::stoi(component);
        }
    } catch (...) {
        return false;
    }
    return result.position != 0;
}

std::string trim(std::string value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

void loadMaterialLibrary(const std::filesystem::path& path,
                         std::vector<ModelMaterial>& materials) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    ModelMaterial* current = nullptr;
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream stream(line);
        std::string tag;
        stream >> tag;
        if (tag.empty() || tag[0] == '#') continue;

        if (tag == "newmtl") {
            std::string name;
            stream >> name;
            if (name.empty()) continue;
            materials.push_back(ModelMaterial{});
            materials.back().name = name;
            current = &materials.back();
        } else if (!current) {
            continue;
        } else if (tag == "Ka") {
            stream >> current->ambient.r >> current->ambient.g >> current->ambient.b;
        } else if (tag == "Kd") {
            stream >> current->diffuse.r >> current->diffuse.g >> current->diffuse.b;
        } else if (tag == "Ks") {
            stream >> current->specular.r >> current->specular.g >> current->specular.b;
        } else if (tag == "Ns") {
            stream >> current->shininess;
        } else if (tag == "map_Kd") {
            std::string texturePath;
            std::getline(stream, texturePath);
            current->diffuseTexturePath =
                (path.parent_path() / trim(texturePath)).lexically_normal().string();
        }
    }
}

} // namespace

bool ObjModelLoader::load(const std::string& path, ModelAsset& asset, std::string& error) {
    std::ifstream file(path);
    if (!file.is_open()) {
        error = "unable to open OBJ file: " + path;
        return false;
    }

    std::vector<Vector3> positions;
    std::vector<Vector2> texCoords;
    std::vector<Vector3> normals;
    std::vector<std::string> materialLibraries;
    ModelPart part;
    part.name = "default";
    std::unordered_map<ObjIndex, int, ObjIndexHash> vertexMap;
    std::vector<bool> hasNormal;
    std::vector<Vector3> accumulatedNormals;
    std::vector<Vector3> accumulatedTangents;
    std::string line;
    int lineNumber = 0;

    auto makeVertex = [&](const ObjIndex& source, int& outputIndex) -> bool {
        const int positionIndex = resolveIndex(source.position, static_cast<int>(positions.size()));
        const int texCoordIndex = resolveIndex(source.texCoord, static_cast<int>(texCoords.size()));
        const int normalIndex = resolveIndex(source.normal, static_cast<int>(normals.size()));
        if (positionIndex < 0 || positionIndex >= static_cast<int>(positions.size())) return false;
        if (source.texCoord != 0 && (texCoordIndex < 0 || texCoordIndex >= static_cast<int>(texCoords.size()))) return false;
        if (source.normal != 0 && (normalIndex < 0 || normalIndex >= static_cast<int>(normals.size()))) return false;

        auto existing = vertexMap.find(source);
        if (existing != vertexMap.end()) {
            outputIndex = existing->second;
            return true;
        }

        Vertex vertex;
        vertex.position = positions[positionIndex];
        vertex.texCoord = source.texCoord == 0 ? Vector2::zero() : texCoords[texCoordIndex];
        vertex.normal = source.normal == 0 ? Vector3::zero() : normals[normalIndex];
        vertex.color = Color::white();
        outputIndex = part.mesh.getVertexCount();
        part.mesh.addVertex(vertex);
        vertexMap.emplace(source, outputIndex);
        hasNormal.push_back(source.normal != 0);
        accumulatedNormals.emplace_back(Vector3::zero());
        accumulatedTangents.emplace_back(Vector3::zero());
        return true;
    };

    while (std::getline(file, line)) {
        ++lineNumber;
        std::stringstream stream(line);
        std::string tag;
        stream >> tag;
        if (tag.empty() || tag[0] == '#') continue;

        if (tag == "v") {
            Vector3 value;
            if (!(stream >> value.x >> value.y >> value.z)) {
                error = "invalid vertex at line " + std::to_string(lineNumber);
                return false;
            }
            positions.push_back(value);
        } else if (tag == "vt") {
            Vector2 value;
            if (!(stream >> value.x >> value.y)) {
                error = "invalid texture coordinate at line " + std::to_string(lineNumber);
                return false;
            }
            texCoords.push_back(value);
        } else if (tag == "vn") {
            Vector3 value;
            if (!(stream >> value.x >> value.y >> value.z)) {
                error = "invalid normal at line " + std::to_string(lineNumber);
                return false;
            }
            normals.push_back(value.normalized());
        } else if (tag == "mtllib") {
            std::string library;
            stream >> library;
            if (!library.empty()) materialLibraries.push_back(library);
        } else if (tag == "o" || tag == "g") {
            std::string name;
            stream >> name;
            if (!name.empty()) part.name = name;
        } else if (tag == "usemtl") {
            stream >> part.materialName;
        } else if (tag == "f") {
            std::vector<ObjIndex> face;
            std::string token;
            while (stream >> token) {
                ObjIndex index;
                if (!parseFaceIndex(token, index)) {
                    error = "invalid face index at line " + std::to_string(lineNumber);
                    return false;
                }
                face.push_back(index);
            }
            if (face.size() < 3) {
                error = "face has fewer than three vertices at line " + std::to_string(lineNumber);
                return false;
            }

            int first = -1;
            for (size_t i = 1; i + 1 < face.size(); ++i) {
                int i0 = -1, i1 = -1, i2 = -1;
                if (!makeVertex(face[0], i0) || !makeVertex(face[i], i1) || !makeVertex(face[i + 1], i2)) {
                    error = "face references an invalid vertex at line " + std::to_string(lineNumber);
                    return false;
                }
                if (first < 0) first = i0;
                part.mesh.addTriangle(i0, i1, i2);

                const auto& vertices = part.mesh.getVertices();
                Vector3 faceNormal = (vertices[i1].position - vertices[i0].position)
                    .cross(vertices[i2].position - vertices[i0].position).normalized();
                if (!hasNormal[i0]) accumulatedNormals[i0] += faceNormal;
                if (!hasNormal[i1]) accumulatedNormals[i1] += faceNormal;
                if (!hasNormal[i2]) accumulatedNormals[i2] += faceNormal;

                const Vector3 edge1 = vertices[i1].position - vertices[i0].position;
                const Vector3 edge2 = vertices[i2].position - vertices[i0].position;
                const Vector2 uv1 = vertices[i1].texCoord - vertices[i0].texCoord;
                const Vector2 uv2 = vertices[i2].texCoord - vertices[i0].texCoord;
                const float denominator = uv1.x * uv2.y - uv1.y * uv2.x;
                if (std::fabs(denominator) > 1e-8f) {
                    const float inverse = 1.0f / denominator;
                    const Vector3 tangent = (edge1 * uv2.y - edge2 * uv1.y) * inverse;
                    accumulatedTangents[i0] += tangent;
                    accumulatedTangents[i1] += tangent;
                    accumulatedTangents[i2] += tangent;
                }
            }
        }
    }

    if (part.mesh.getTriangleCount() == 0) {
        error = "OBJ file contains no triangles: " + path;
        return false;
    }

    std::vector<Vertex>& vertices = part.mesh.getVertices();

    // Some repository meshes (notably sphere.obj) provide one identical
    // normal for all three vertices of every triangle. That is a flat-shaded
    // export, even though the editor's default mode is smooth shading. Detect
    // that representation and rebuild normals by averaging faces that share a
    // position so the imported sphere renders smoothly by default.
    bool authoredNormals = !vertices.empty();
    bool authoredFlat = authoredNormals;
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (!hasNormal[i]) {
            authoredNormals = false;
            break;
        }
    }
    if (authoredNormals) {
        const auto& indices = part.mesh.getIndices();
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            const Vector3& n0 = vertices[indices[i + 0]].normal;
            const Vector3& n1 = vertices[indices[i + 1]].normal;
            const Vector3& n2 = vertices[indices[i + 2]].normal;
            if ((n0 - n1).lengthSquared() > 1e-6f ||
                (n0 - n2).lengthSquared() > 1e-6f) {
                authoredFlat = false;
                break;
            }
        }
    }
    if (authoredFlat) {
        std::vector<Vector3> smoothNormals(vertices.size(), Vector3::zero());
        const auto& indices = part.mesh.getIndices();
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            const int i0 = indices[i + 0];
            const int i1 = indices[i + 1];
            const int i2 = indices[i + 2];
            const Vector3 faceNormal = (vertices[i1].position - vertices[i0].position)
                .cross(vertices[i2].position - vertices[i0].position).normalized();
            for (size_t vertexIndex = 0; vertexIndex < vertices.size(); ++vertexIndex) {
                if ((vertices[vertexIndex].position - vertices[i0].position).lengthSquared() < 1e-10f ||
                    (vertices[vertexIndex].position - vertices[i1].position).lengthSquared() < 1e-10f ||
                    (vertices[vertexIndex].position - vertices[i2].position).lengthSquared() < 1e-10f) {
                    smoothNormals[vertexIndex] += faceNormal;
                }
            }
        }
        for (size_t i = 0; i < vertices.size(); ++i) {
            if (smoothNormals[i].lengthSquared() > 1e-8f) {
                vertices[i].normal = smoothNormals[i].normalized();
            }
        }
    }
    for (size_t i = 0; i < vertices.size(); ++i) {
        if (!hasNormal[i]) vertices[i].normal = accumulatedNormals[i].normalized();
        if (vertices[i].normal.lengthSquared() < 1e-8f) vertices[i].normal = Vector3::forward();
        Vector3 tangent = accumulatedTangents[i];
        tangent = tangent - vertices[i].normal * vertices[i].normal.dot(tangent);
        vertices[i].tangent = tangent.lengthSquared() > 1e-8f
            ? tangent.normalized()
            : Vector3(1.0f, 0.0f, 0.0f);
    }

    Vector3 minValue(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    Vector3 maxValue(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
    for (const Vertex& vertex : vertices) {
        minValue.x = std::min(minValue.x, vertex.position.x);
        minValue.y = std::min(minValue.y, vertex.position.y);
        minValue.z = std::min(minValue.z, vertex.position.z);
        maxValue.x = std::max(maxValue.x, vertex.position.x);
        maxValue.y = std::max(maxValue.y, vertex.position.y);
        maxValue.z = std::max(maxValue.z, vertex.position.z);
    }

    asset = {};
    asset.sourcePath = path;
    const std::filesystem::path objPath(path);
    for (const std::string& library : materialLibraries) {
        loadMaterialLibrary(objPath.parent_path() / library, asset.materials);
    }
    for (size_t i = 0; i < asset.materials.size(); ++i) {
        if (asset.materials[i].name == part.materialName) {
            part.materialIndex = static_cast<int>(i);
            break;
        }
    }
    asset.parts.push_back(std::move(part));
    asset.boundsMin = minValue;
    asset.boundsMax = maxValue;
    asset.boundsCenter = (minValue + maxValue) * 0.5f;
    asset.boundsRadius = (maxValue - asset.boundsCenter).length();
    error.clear();
    return true;
}

} // namespace ST
