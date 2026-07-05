#pragma once

#include "GameObject.hpp"

namespace ST {

class MusicPlayer : public GameObject {
public:
    MusicPlayer();
    explicit MusicPlayer(const std::string& name);
    ~MusicPlayer();

    void setAudioPath(const std::string& path) { m_audioPath = path; }
    const std::string& getAudioPath() const { return m_audioPath; }

    void setAudioIndex(int idx) { m_audioIndex = idx; }
    int getAudioIndex() const { return m_audioIndex; }

    void setLoop(bool loop) { m_loop = loop; }
    bool getLoop() const { return m_loop; }

    void setAutoPlay(bool autoPlay) { m_autoPlay = autoPlay; }
    bool getAutoPlay() const { return m_autoPlay; }

    void setVolume(int vol) { m_volume = std::clamp(vol, 0, 100); }
    int getVolume() const { return m_volume; }

private:
    std::string m_audioPath;
    int m_audioIndex = -1;
    bool m_loop = true;
    bool m_autoPlay = false;
    int m_volume = 80;
};

} // namespace ST
