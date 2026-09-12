#include "TestModule_Shader.hpp"

#include <algorithm>
#include <imgui.h>
#include <vector>

namespace {
uint32_t packColor(const ST::Color& color) {
    auto channel = [](float value) {
        return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f);
    };
    return static_cast<uint32_t>(channel(color.r))
        | (static_cast<uint32_t>(channel(color.g)) << 8)
        | (static_cast<uint32_t>(channel(color.b)) << 16)
        | (static_cast<uint32_t>(channel(color.a)) << 24);
}
}

TestModule_Shader::TestModule_Shader()
    : m_renderer(640, 480)
    , m_mesh(ST::Mesh::createTriangle()) {}

TestModule_Shader::~TestModule_Shader() {
    if (m_outputTexture) SDL_DestroyTexture(m_outputTexture);
}

void TestModule_Shader::render(void* canvasTexture, int canvasW, int canvasH) {
    if (canvasW <= 0 || canvasH <= 0) return;

    m_renderer.clear(ST::Color(0.08f, 0.09f, 0.12f, 1.0f));
    m_renderer.render(m_mesh);
    uploadFrame(static_cast<SDL_Renderer*>(canvasTexture));
}

void TestModule_Shader::uploadFrame(SDL_Renderer* renderer) {
    const int width = m_renderer.getWidth();
    const int height = m_renderer.getHeight();
    if (m_sdlRenderer != renderer || !m_outputTexture ||
        m_textureWidth != width || m_textureHeight != height) {
        if (m_outputTexture) SDL_DestroyTexture(m_outputTexture);
        m_sdlRenderer = renderer;
        m_outputTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                            SDL_TEXTUREACCESS_STREAMING,
                                            width, height);
        m_textureWidth = width;
        m_textureHeight = height;
    }
    if (!m_outputTexture) return;

    std::vector<uint32_t> pixels;
    pixels.reserve(static_cast<size_t>(width * height));
    for (const auto& color : m_renderer.getFrameBuffer().getPixels()) {
        pixels.push_back(packColor(color));
    }
    SDL_UpdateTexture(m_outputTexture, nullptr, pixels.data(), width * sizeof(uint32_t));
    SDL_RenderCopy(renderer, m_outputTexture, nullptr, nullptr);
}

bool TestModule_Shader::renderControls() {
    bool changed = m_shaderUI.renderControls(m_renderer);
    if (changed) needsRerender = true;
    return changed;
}

void TestModule_Shader::runConsole(std::string& output) {
    output = "[Shader Test]\n";
    output += "Programmable vertex/fragment stages with script reload.\n";
    if (!m_renderer.getShaderError().empty()) {
        output += "Shader error: " + m_renderer.getShaderError() + "\n";
    }
}
