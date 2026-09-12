#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>

#include <nlohmann/json.hpp>

namespace ST {

// Main-thread command bridge used by the local MCP server.
class AppControlBridge {
public:
    using Json = nlohmann::json;
    using CommandHandler = std::function<Json(const Json&)>;

    AppControlBridge();
    ~AppControlBridge();

    bool initialize(std::string* error = nullptr);
    void poll(const CommandHandler& handler);
    void updateState(Json state, bool force = false);
    void shutdown();

    bool isInitialized() const { return m_initialized; }
    const std::filesystem::path& runtimeDirectory() const { return m_runtimeDirectory; }
    const std::filesystem::path& capturesDirectory() const { return m_capturesDirectory; }

private:
    bool writeJsonAtomic(const std::filesystem::path& path,
                         const Json& value,
                         std::string* error = nullptr) const;
    void clearStaleQueue();

    bool m_initialized = false;
    std::filesystem::path m_runtimeDirectory;
    std::filesystem::path m_requestsDirectory;
    std::filesystem::path m_responsesDirectory;
    std::filesystem::path m_capturesDirectory;
    std::filesystem::path m_statePath;
    std::chrono::steady_clock::time_point m_lastStateWrite;
};

} // namespace ST
