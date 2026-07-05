#pragma once

#include "renderer/buffer/Texture2D.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace ST {

struct TextureInfo {
    std::string filename;
    std::string fullPath;
    Texture2D texture;
};

class TextureManager {
public:
    static TextureManager& getInstance();

    void scanResourceFolder();
    int getTextureCount() const { return (int)m_textures.size(); }
    const std::string& getTextureName(int idx) const;
    Texture2D* getTexture(int idx);
    int findTexture(const std::string& filename) const;

    const std::vector<TextureInfo>& getAllTextures() const { return m_textures; }

private:
    TextureManager() = default;

    std::vector<TextureInfo> m_textures;
    std::unordered_map<std::string, int> m_nameToIndex;
};

}
