#include "renderer/shader/ShaderManager.hpp"

namespace ST {

bool ShaderManager::readTimestamp(std::filesystem::file_time_type& timestamp) const {
    if (m_path.empty()) return false;
    std::error_code ec;
    timestamp = std::filesystem::last_write_time(m_path, ec);
    return !ec;
}

bool ShaderManager::load(const std::string& path, std::string& error) {
    std::string compileError;
    auto candidate = ScriptShaderProgram::fromFile(path, compileError);
    if (!candidate) {
        m_lastError = compileError;
        error = compileError;
        return false;
    }

    std::filesystem::file_time_type timestamp{};
    std::error_code ec;
    timestamp = std::filesystem::last_write_time(path, ec);
    if (ec) {
        error = "shader file disappeared after compilation: " + path;
        m_lastError = error;
        return false;
    }

    m_path = path;
    m_lastWriteTime = timestamp;
    m_program = std::move(candidate);
    m_lastError.clear();
    error.clear();
    return true;
}

bool ShaderManager::reloadIfChanged(std::string& error) {
    error.clear();
    if (m_path.empty()) return false;

    std::filesystem::file_time_type timestamp{};
    if (!readTimestamp(timestamp)) {
        error = "shader file is unavailable: " + m_path;
        m_lastError = error;
        return false;
    }
    if (timestamp == m_lastWriteTime) return false;

    // Keep the timestamp even if compilation fails. This prevents a broken
    // file from being parsed on every frame while still allowing the next
    // edit to trigger another attempt.
    m_lastWriteTime = timestamp;
    std::string compileError;
    auto candidate = ScriptShaderProgram::fromFile(m_path, compileError);
    if (!candidate) {
        m_lastError = compileError;
        error = compileError;
        return false;
    }

    m_program = std::move(candidate);
    m_lastError.clear();
    return true;
}

void ShaderManager::clear() {
    m_path.clear();
    m_lastError.clear();
    m_lastWriteTime = std::filesystem::file_time_type{};
    m_program.reset();
}

} // namespace ST
