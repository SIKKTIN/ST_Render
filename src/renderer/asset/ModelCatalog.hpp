#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ST {

struct ModelEntry {
    std::string displayName;
    std::string relativePath;
    std::string absolutePath;
};

class ModelCatalog {
public:
    bool scan(const std::string& rootDirectory, std::string& error);
    const std::vector<ModelEntry>& getEntries() const { return m_entries; }
    int findByRelativePath(const std::string& path) const;

private:
    std::string m_rootDirectory;
    std::vector<ModelEntry> m_entries;
};

} // namespace ST
