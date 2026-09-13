#include "renderer/asset/ModelCatalog.hpp"

#include <algorithm>
#include <cctype>

namespace ST {
namespace {
std::string lowerExtension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension;
}
}

bool ModelCatalog::scan(const std::string& rootDirectory, std::string& error) {
    return scan(std::vector<ModelRoot>{{rootDirectory, {}}}, error);
}

bool ModelCatalog::scan(const std::vector<ModelRoot>& roots, std::string& error) {
    namespace fs = std::filesystem;
    std::vector<ModelEntry> entries;
    bool foundRoot = false;
    for (const ModelRoot& modelRoot : roots) {
        std::error_code ec;
        const fs::path root = fs::absolute(fs::path(modelRoot.directory), ec);
        if (ec || !fs::exists(root, ec) || !fs::is_directory(root, ec)) continue;
        foundRoot = true;

        fs::recursive_directory_iterator iterator(
            root, fs::directory_options::skip_permission_denied, ec);
        fs::recursive_directory_iterator end;
        while (iterator != end) {
            if (ec) {
                ec.clear();
                iterator.increment(ec);
                continue;
            }
            const fs::directory_entry& file = *iterator;
            const std::string extension = lowerExtension(file.path());
            if (file.is_regular_file(ec) && (extension == ".obj" || extension == ".fbx")) {
                fs::path relative = fs::relative(file.path(), root, ec);
                if (!ec) {
                    ModelEntry entry;
                    entry.displayName = file.path().filename().string();
                    entry.relativePath = (fs::path(modelRoot.prefix) / relative).generic_string();
                    entry.absolutePath = fs::absolute(file.path()).string();
                    entry.format = extension.substr(1);
                    entries.push_back(std::move(entry));
                }
            }
            iterator.increment(ec);
        }
    }

    if (!foundRoot) {
        error = "no model directories found";
        m_entries.clear();
        return false;
    }

    std::sort(entries.begin(), entries.end(), [](const ModelEntry& a, const ModelEntry& b) {
        return a.relativePath < b.relativePath;
    });
    m_rootDirectory = roots.empty() ? std::string() : roots.front().directory;
    m_entries = std::move(entries);
    error.clear();
    return true;
}

int ModelCatalog::findByRelativePath(const std::string& path) const {
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].relativePath == path || m_entries[i].absolutePath == path) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace ST
