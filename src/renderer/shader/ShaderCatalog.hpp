#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ST {

struct ShaderEntry {
    std::string displayName;
    std::string relativePath;
    std::string absolutePath;
};

class ShaderCatalog {
public:
    bool scan(const std::string& rootDirectory, std::string& error);

    const std::vector<ShaderEntry>& getEntries() const { return m_entries; }
    int findByRelativePath(const std::string& path) const;
    const std::string& getRootDirectory() const { return m_rootDirectory; }

private:
    std::string m_rootDirectory;
    std::vector<ShaderEntry> m_entries;
};

} // namespace ST
