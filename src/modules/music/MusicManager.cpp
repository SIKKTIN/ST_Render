#include "modules/music/MusicManager.hpp"
#include <iostream>

namespace ST {

MusicManager& MusicManager::get() {
    static MusicManager instance;
    return instance;
}

bool MusicManager::init() {
    if (m_initialized) return true;

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        m_lastError = "Mix_OpenAudio failed: " + std::string(Mix_GetError());
        std::cerr << "[MusicManager] " << m_lastError << std::endl;
        return false;
    }

    m_initialized = true;
    std::cout << "[MusicManager] Initialized" << std::endl;
    return true;
}

void MusicManager::shutdown() {
    if (m_music) {
        Mix_HaltMusic();
        Mix_FreeMusic(m_music);
        m_music = nullptr;
    }
    if (m_initialized) {
        Mix_CloseAudio();
        m_initialized = false;
    }
}

bool MusicManager::loadMusic(const std::string& filePath) {
    if (!m_initialized) {
        if (!init()) return false;
    }

    if (m_music) {
        Mix_HaltMusic();
        Mix_FreeMusic(m_music);
        m_music = nullptr;
    }

    m_music = Mix_LoadMUS(filePath.c_str());
    if (!m_music) {
        m_lastError = "Mix_LoadMUS failed: " + std::string(Mix_GetError());
        std::cerr << "[MusicManager] " << m_lastError << std::endl;
        return false;
    }

    std::cout << "[MusicManager] Loaded: " << filePath << std::endl;
    return true;
}

void MusicManager::play() {
    if (!m_music) {
        std::cerr << "[MusicManager] No music loaded" << std::endl;
        return;
    }
    Mix_PlayMusic(m_music, -1); // loop forever
    std::cout << "[MusicManager] Play" << std::endl;
}

void MusicManager::pause() {
    if (Mix_PausedMusic()) return;
    Mix_PauseMusic();
}

void MusicManager::stop() {
    Mix_HaltMusic();
}

void MusicManager::resume() {
    if (!Mix_PausedMusic()) return;
    Mix_ResumeMusic();
}

bool MusicManager::isPlaying() const {
    return Mix_PlayingMusic() != 0 && !Mix_PausedMusic();
}

bool MusicManager::isPaused() const {
    return Mix_PausedMusic() != 0;
}

void MusicManager::setVolume(int volume) {
    Mix_VolumeMusic(static_cast<int>(volume * 1.28f)); // Mix maps 0-128
}

} // namespace ST
