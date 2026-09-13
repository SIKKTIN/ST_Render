#pragma once

#include "renderer/asset/ModelAsset.hpp"
#include <string>

namespace ST {

class AssimpModelLoader {
public:
    static bool load(const std::string& path, ModelAsset& asset, std::string& error);
};

} // namespace ST
