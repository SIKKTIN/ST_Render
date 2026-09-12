#pragma once

#include "renderer/shader/ScriptShaderProgram.hpp"
#include <filesystem>
#include <memory>
#include <string>

namespace ST {

// Owns the currently loaded script shader and performs frame-boundary
// hot-reloads. A failed reload never replaces the last valid program.
class ShaderManager {
public:
    bool load(const std::string& path, std::string& error);
    bool reloadIfChanged(std::string& error);
    void clear();

    bool hasSourceFile() const { return !m_path.empty(); }
    const std::string& getPath() const { return m_path; }
    const std::string& getLastError() const { return m_lastError; }
    const std::shared_ptr<ScriptShaderProgram>& getProgram() const { return m_program; }

private:
    bool readTimestamp(std::filesystem::file_time_type& timestamp) const;

    std::string m_path;
    std::string m_lastError;
    std::filesystem::file_time_type m_lastWriteTime{};
    std::shared_ptr<ScriptShaderProgram> m_program;
};

} // namespace ST
