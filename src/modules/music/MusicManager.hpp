#pragma once

#include <string>
#include <SDL2/SDL_mixer.h>

namespace ST {

class MusicManager {
public:
    static MusicManager& get();

    bool init();
    void shutdown();

    bool loadMusic(const std::string& filePath);
    void play();
    void pause();
    void stop();
    void resume();

    bool isPlaying() const;
    bool isPaused() const;

    void setVolume(int volume); // 0-100

    const std::string& getLastError() const { return m_lastError; }

private:
    MusicManager() = default;
    ~MusicManager() = default;

    MusicManager(const MusicManager&) = delete;
    MusicManager& operator=(const MusicManager&) = delete;

    bool m_initialized = false;
    Mix_Music* m_music = nullptr;
    std::string m_lastError;
};

} // namespace ST
