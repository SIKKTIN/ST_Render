#pragma once

#include "renderer/geometry/Mesh.hpp"
#include "core/math/Vector3.hpp"
#include <string>
#include <vector>

namespace ST {

struct ModelPart {
    std::string name;
    std::string materialName;
    Mesh mesh;
};

struct ModelAsset {
    std::string sourcePath;
    std::vector<ModelPart> parts;
    Vector3 boundsMin = Vector3::zero();
    Vector3 boundsMax = Vector3::zero();
    Vector3 boundsCenter = Vector3::zero();
    float boundsRadius = 0.0f;

    int getVertexCount() const {
        int count = 0;
        for (const auto& part : parts) count += part.mesh.getVertexCount();
        return count;
    }

    int getTriangleCount() const {
        int count = 0;
        for (const auto& part : parts) count += part.mesh.getTriangleCount();
        return count;
    }
};

} // namespace ST
