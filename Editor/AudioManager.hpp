#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace ST {

struct AudioInfo {
    std::string filename;
    std::string fullPath;
};

class AudioManager {
public:
    static AudioManager& getInstance();

    void scanAudioFolder();
    int getAudioCount() const { return (int)m_audios.size(); }
    const std::string& getAudioName(int idx) const;
    const std::string& getAudioFullPath(int idx) const;
    int findAudio(const std::string& filename) const;

    const std::vector<AudioInfo>& getAllAudios() const { return m_audios; }

private:
    AudioManager() = default;

    std::vector<AudioInfo> m_audios;
    std::unordered_map<std::string, int> m_nameToIndex;
};

} // namespace ST
