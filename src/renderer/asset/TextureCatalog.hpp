#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ST {

struct TextureEntry {
    std::string displayName;
    std::string relativePath;
    std::string absolutePath;
};

// Repository-backed image catalog used by editor material slots.
class TextureCatalog {
public:
    bool scan(const std::string& rootDirectory, std::string& error);
    const std::vector<TextureEntry>& getEntries() const { return m_entries; }
    int findByRelativePath(const std::string& path) const;

private:
    std::vector<TextureEntry> m_entries;
};

} // namespace ST
