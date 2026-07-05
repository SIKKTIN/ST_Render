#include "engine/editor/AudioManager.hpp"
#include <SDL2/SDL.h>
#include <Windows.h>
#include <iostream>

namespace ST {

AudioManager& AudioManager::getInstance() {
    static AudioManager instance;
    return instance;
}

void AudioManager::scanAudioFolder() {
    m_audios.clear();
    m_nameToIndex.clear();

    char* basePath = SDL_GetBasePath();
    if (!basePath) return;

    std::string audioDir(basePath);
    audioDir += "Data/Audio";
    SDL_free(basePath);

    const char* extensions[] = { ".mp3", ".ogg", ".wav" };
    std::string pattern = audioDir + "/*";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        std::cout << "[AudioManager] Audio folder not found: " << audioDir << "\n";
        return;
    }

    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::string fname = findData.cFileName;
            for (const char* ext : extensions) {
                if (fname.size() >= strlen(ext) &&
                    _stricmp(fname.c_str() + fname.size() - strlen(ext), ext) == 0) {
                    AudioInfo info;
                    info.filename = fname;
                    info.fullPath = audioDir + "/" + fname;
                    m_nameToIndex[fname] = (int)m_audios.size();
                    m_audios.push_back(info);
                    std::cout << "[AudioManager] Found: " << fname << "\n";
                    break;
                }
            }
        }
    } while (FindNextFileA(hFind, &findData));
    FindClose(hFind);

    std::cout << "[AudioManager] Done. Found " << m_audios.size() << " audio files.\n";
}

const std::string& AudioManager::getAudioName(int idx) const {
    static std::string empty;
    if (idx >= 0 && idx < (int)m_audios.size()) return m_audios[idx].filename;
    return empty;
}

const std::string& AudioManager::getAudioFullPath(int idx) const {
    static std::string empty;
    if (idx >= 0 && idx < (int)m_audios.size()) return m_audios[idx].fullPath;
    return empty;
}

int AudioManager::findAudio(const std::string& filename) const {
    auto it = m_nameToIndex.find(filename);
    if (it != m_nameToIndex.end()) return it->second;
    return -1;
}

} // namespace ST
