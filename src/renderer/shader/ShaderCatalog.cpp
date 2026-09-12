#include "renderer/shader/ShaderCatalog.hpp"

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

bool ShaderCatalog::scan(const std::string& rootDirectory, std::string& error) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path root = fs::absolute(fs::path(rootDirectory), ec);
    if (ec || !fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        error = "shader directory not found: " + rootDirectory;
        m_entries.clear();
        return false;
    }

    std::vector<ShaderEntry> entries;
    fs::recursive_directory_iterator iterator(root, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;
    while (iterator != end) {
        if (ec) {
            ec.clear();
            iterator.increment(ec);
            continue;
        }

        const fs::directory_entry& file = *iterator;
        if (file.is_regular_file(ec) && lowerExtension(file.path()) == ".stshader") {
            fs::path relative = fs::relative(file.path(), root, ec);
            if (!ec) {
                ShaderEntry entry;
                entry.displayName = file.path().filename().string();
                entry.relativePath = relative.generic_string();
                entry.absolutePath = fs::absolute(file.path()).string();
                entries.push_back(std::move(entry));
            }
        }
        iterator.increment(ec);
    }

    std::sort(entries.begin(), entries.end(), [](const ShaderEntry& a, const ShaderEntry& b) {
        return a.relativePath < b.relativePath;
    });

    m_rootDirectory = root.string();
    m_entries = std::move(entries);
    error.clear();
    return true;
}

int ShaderCatalog::findByRelativePath(const std::string& path) const {
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].relativePath == path || m_entries[i].absolutePath == path) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace ST
