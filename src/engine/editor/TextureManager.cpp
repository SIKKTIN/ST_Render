#include "engine/editor/TextureManager.hpp"
#include "core/texture/ST_Image.hpp"
#include <SDL2/SDL.h>
#include <Windows.h>
#include <iostream>

namespace ST {

TextureManager& TextureManager::getInstance() {
    static TextureManager instance;
    return instance;
}

void TextureManager::scanResourceFolder() {
    m_textures.clear();
    m_nameToIndex.clear();

    char* basePath = SDL_GetBasePath();
    if (!basePath) {
        OutputDebugStringA("[TextureManager] SDL_GetBasePath returned nullptr\n");
        return;
    }
    std::string resourceDir(basePath);
    resourceDir += "Resource";
    SDL_free(basePath);

    OutputDebugStringA("[TextureManager] Resource dir: ");
    OutputDebugStringA(resourceDir.c_str());
    OutputDebugStringA("\n");

    const char* extensions[] = { ".jpg", ".jpeg", ".png", ".bmp", ".tga" };
    std::string pattern = resourceDir + "/*";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        OutputDebugStringA("[TextureManager] FindFirstFile failed\n");
        return;
    }

    int foundFiles = 0;
    int loadedTextures = 0;
    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            foundFiles++;
            std::string fname = findData.cFileName;
            for (const char* ext : extensions) {
                if (fname.size() >= strlen(ext) &&
                    _stricmp(fname.c_str() + fname.size() - strlen(ext), ext) == 0) {

                    std::string fullPath = resourceDir + "/" + fname;
                    ST::Image img;
                    if (img.load(fullPath.c_str())) {
                        TextureInfo info;
                        info.filename = fname;
                        info.fullPath = fullPath;
                        info.texture.setPixels(img.getPixels(), img.getWidth(), img.getHeight());
                        m_nameToIndex[fname] = (int)m_textures.size();
                        m_textures.push_back(info);
                        loadedTextures++;
                        OutputDebugStringA("[TextureManager] Loaded: ");
                        OutputDebugStringA(fname.c_str());
                        OutputDebugStringA("\n");
                    } else {
                        OutputDebugStringA("[TextureManager] Failed to load image: ");
                        OutputDebugStringA(fullPath.c_str());
                        OutputDebugStringA("\n");
                    }
                    break;
                }
            }
        }
    } while (FindNextFileA(hFind, &findData));
    FindClose(hFind);

    char buf[256];
    //snprintf(buf, sizeof(buf), "[TextureManager] Done. Found %d files, loaded %d textures\n", foundFiles, loadedTextures);
    
    std::cout << "[TextureManager] Done. Found " << foundFiles << " files, loaded " << loadedTextures << "\n";
    OutputDebugStringA(buf);
}

const std::string& TextureManager::getTextureName(int idx) const {
    static std::string empty;
    if (idx >= 0 && idx < (int)m_textures.size()) {
        return m_textures[idx].filename;
    }
    return empty;
}

Texture2D* TextureManager::getTexture(int idx) {
    if (idx >= 0 && idx < (int)m_textures.size()) {
        return &m_textures[idx].texture;
    }
    return nullptr;
}

int TextureManager::findTexture(const std::string& filename) const {
    auto it = m_nameToIndex.find(filename);
    if (it != m_nameToIndex.end()) {
        return it->second;
    }
    return -1;
}

}
