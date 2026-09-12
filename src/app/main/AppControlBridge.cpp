#include "AppControlBridge.hpp"

#include <SDL2/SDL.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <fstream>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace ST {

namespace {

std::int64_t unixTimeMilliseconds() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

} // namespace

AppControlBridge::AppControlBridge()
    : m_lastStateWrite(std::chrono::steady_clock::time_point::min()) {
}

AppControlBridge::~AppControlBridge() {
    shutdown();
}

bool AppControlBridge::initialize(std::string* error) {
    if (m_initialized) return true;

    char* basePath = SDL_GetBasePath();
    if (!basePath) {
        if (error) *error = "SDL_GetBasePath failed";
        return false;
    }

    m_runtimeDirectory = std::filesystem::path(basePath) / "McpBridge";
    SDL_free(basePath);
    m_requestsDirectory = m_runtimeDirectory / "requests";
    m_responsesDirectory = m_runtimeDirectory / "responses";
    m_capturesDirectory = m_runtimeDirectory / "captures";
    m_statePath = m_runtimeDirectory / "state.json";

    std::error_code ec;
    std::filesystem::create_directories(m_requestsDirectory, ec);
    if (!ec) std::filesystem::create_directories(m_responsesDirectory, ec);
    if (!ec) std::filesystem::create_directories(m_capturesDirectory, ec);
    if (ec) {
        if (error) *error = "Failed to create MCP runtime directories: " + ec.message();
        return false;
    }

    clearStaleQueue();
    m_initialized = true;
    return true;
}

void AppControlBridge::clearStaleQueue() {
    std::error_code ec;
    for (const auto* directory : { &m_requestsDirectory, &m_responsesDirectory }) {
        for (std::filesystem::directory_iterator it(*directory, ec), end;
             !ec && it != end; it.increment(ec)) {
            if (it->is_regular_file() &&
                (it->path().extension() == ".json" || it->path().extension() == ".tmp")) {
                std::filesystem::remove(it->path(), ec);
                ec.clear();
            }
        }
        ec.clear();
    }
}

bool AppControlBridge::writeJsonAtomic(const std::filesystem::path& path,
                                       const Json& value,
                                       std::string* error) const {
    const std::filesystem::path temporary = path.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            if (error) *error = "Unable to open " + temporary.string();
            return false;
        }
        output << value.dump(2);
        if (!output.good()) {
            if (error) *error = "Unable to write " + temporary.string();
            return false;
        }
    }

    std::error_code ec;
    std::filesystem::remove(path, ec);
    ec.clear();
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        if (error) *error = "Unable to publish " + path.string() + ": " + ec.message();
        std::filesystem::remove(temporary, ec);
        return false;
    }
    return true;
}

void AppControlBridge::poll(const CommandHandler& handler) {
    if (!m_initialized || !handler) return;

    std::vector<std::filesystem::path> requests;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(m_requestsDirectory, ec), end;
         !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file() && it->path().extension() == ".json") {
            requests.push_back(it->path());
        }
    }

    for (const auto& requestPath : requests) {
        Json request;
        Json response;
        std::string requestId = requestPath.stem().string();
        try {
            std::ifstream input(requestPath, std::ios::binary);
            if (!input) throw std::runtime_error("Unable to open request file");
            input >> request;
            if (request.contains("id") && request["id"].is_string() &&
                request["id"].get<std::string>() != requestId) {
                throw std::runtime_error("Request id does not match its queue filename");
            }
            response = {
                { "id", requestId },
                { "ok", true },
                { "result", handler(request) }
            };
        } catch (const std::exception& ex) {
            response = {
                { "id", requestId },
                { "ok", false },
                { "error", ex.what() }
            };
        }

        std::filesystem::remove(requestPath, ec);
        ec.clear();
        writeJsonAtomic(m_responsesDirectory / (requestId + ".json"), response);
    }
}

void AppControlBridge::updateState(Json state, bool force) {
    if (!m_initialized) return;

    const auto now = std::chrono::steady_clock::now();
    if (!force && m_lastStateWrite != std::chrono::steady_clock::time_point::min() &&
        now - m_lastStateWrite < std::chrono::milliseconds(500)) {
        return;
    }

    state["bridgeProtocolVersion"] = 1;
    state["pid"] = static_cast<unsigned long>(GetCurrentProcessId());
    state["heartbeatUnixMs"] = unixTimeMilliseconds();
    if (writeJsonAtomic(m_statePath, state)) m_lastStateWrite = now;
}

void AppControlBridge::shutdown() {
    if (!m_initialized) return;
    std::error_code ec;
    std::filesystem::remove(m_statePath, ec);
    m_initialized = false;
}

} // namespace ST
