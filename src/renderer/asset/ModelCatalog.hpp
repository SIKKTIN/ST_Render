#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ST {

struct ModelRoot {
    std::string directory;
    std::string prefix;
};

struct ModelEntry {
    std::string displayName;
    std::string relativePath;
    std::string absolutePath;
    std::string format;
};

class ModelCatalog {
public:
    bool scan(const std::string& rootDirectory, std::string& error);
    bool scan(const std::vector<ModelRoot>& roots, std::string& error);
    const std::vector<ModelEntry>& getEntries() const { return m_entries; }
    int findByRelativePath(const std::string& path) const;

private:
    std::string m_rootDirectory;
    std::vector<ModelEntry> m_entries;
};

} // namespace ST
