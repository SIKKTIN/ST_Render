#include "renderer/asset/ModelLoader.hpp"

#include "renderer/asset/AssimpModelLoader.hpp"
#include "renderer/asset/ObjModelLoader.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace ST {

bool ModelLoader::load(const std::string& path, ModelAsset& asset, std::string& error) {
    std::string extension = std::filesystem::path(path).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (extension == ".fbx") return AssimpModelLoader::load(path, asset, error);
    if (extension == ".obj") return ObjModelLoader::load(path, asset, error);
    error = "unsupported model format: " + extension;
    return false;
}

} // namespace ST
