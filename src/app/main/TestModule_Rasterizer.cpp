#include "TestModule_Rasterizer.hpp"
#include <imgui.h>
#include <SDL2/SDL.h>
#include <iostream>

TestModule_Rasterizer::TestModule_Rasterizer()
    : m_frameBuffer(nullptr), m_renderMode(1) {
}

TestModule_Rasterizer::~TestModule_Rasterizer() {
    delete m_frameBuffer;
}

void TestModule_Rasterizer::render(void* canvasTexture, int canvasW, int canvasH) {
    if (!m_frameBuffer || m_frameBuffer->getWidth() != canvasW || m_frameBuffer->getHeight() != canvasH) {
        delete m_frameBuffer;
        m_frameBuffer = new ST::FrameBuffer();
        m_frameBuffer->initialize(canvasW, canvasH);
    }

    m_frameBuffer->clear(ST::Color(0.2f, 0.2f, 0.2f, 1.0f));
    m_rasterizer.setBuffers(m_frameBuffer, nullptr);
    m_rasterizer.setUseRawScreenCoords(true);

    ST::VertexOut v0, v1, v2;

    // 直接设置屏幕坐标，绕过 VertexShader
    // w=1 时，toNDC() 透视除法后坐标不变
    v0.position = ST::Vector4(100, 100, 0, 1);
    v0.color = ST::Color(1, 0, 0, 1);
    v0.worldPosition = ST::Vector3(100, 100, 0);
    v0.normal = ST::Vector3(0, 0, 1);
    v0.texCoord = ST::Vector2(0, 0);

    v1.position = ST::Vector4(300, 100, 0, 1);
    v1.color = ST::Color(0, 1, 0, 1);
    v1.worldPosition = ST::Vector3(300, 100, 0);
    v1.normal = ST::Vector3(0, 0, 1);
    v1.texCoord = ST::Vector2(1, 0);

    v2.position = ST::Vector4(200, 300, 0, 1);
    v2.color = ST::Color(0, 0, 1, 1);
    v2.worldPosition = ST::Vector3(200, 300, 0);
    v2.normal = ST::Vector3(0, 0, 1);
    v2.texCoord = ST::Vector2(0.5f, 1);

    // 使用简单着色器，直接返回顶点颜色
    m_rasterizer.rasterizeTriangle(v0, v1, v2,
        [](const ST::VertexOut& vo) {
            return vo.color;
        });

    // 验证 FrameBuffer 有像素
    auto& pixels = m_frameBuffer->getPixels();
    int nonBlack = 0;
    for (const auto& p : pixels) {
        if (p.r > 0.1f || p.g > 0.1f || p.b > 0.1f) nonBlack++;
    }
    std::cout << "[Rasterizer] Non-black pixels: " << nonBlack << " / " << pixels.size() << std::endl;

    SDL_Renderer* renderer = (SDL_Renderer*)canvasTexture;

    if (m_renderMode == 0) {
        // 模式 0: SDL_RenderDrawPoint
        for (int y = 0; y < canvasH; y++) {
            for (int x = 0; x < canvasW; x++) {
                const auto& col = pixels[y * canvasW + x];
                SDL_SetRenderDrawColor(renderer,
                    (Uint8)(col.r * 255),
                    (Uint8)(col.g * 255),
                    (Uint8)(col.b * 255),
                    255);
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }
    } else {
        // 模式 1: Texture
        std::vector<uint32_t> sdlPixels(canvasW * canvasH);
        for (int i = 0; i < canvasW * canvasH; i++) {
            uint8_t r = (uint8_t)(pixels[i].r * 255);
            uint8_t g = (uint8_t)(pixels[i].g * 255);
            uint8_t b = (uint8_t)(pixels[i].b * 255);
            uint8_t a = (uint8_t)(pixels[i].a * 255);
            sdlPixels[i] = (r) | (g << 8) | (b << 16) | (a << 24);
        }

        SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
            (void*)sdlPixels.data(), canvasW, canvasH,
            32, canvasW * 4,
            0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);

        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_DestroyTexture(texture);
    }
}

bool TestModule_Rasterizer::renderControls() {
    ImGui::Separator();
    ImGui::Text("Rasterizer Controls");
    ImGui::Separator();

    int oldMode = m_renderMode;
    ImGui::RadioButton("RenderDrawPoint", &m_renderMode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Texture", &m_renderMode, 1);

    if (m_renderMode != oldMode) {
        return true;
    }

    ImGui::Spacing();
    ImGui::Text("Current Mode: %s", m_renderMode == 0 ? "RenderDrawPoint" : "Texture");
    return false;
}

void TestModule_Rasterizer::runConsole(std::string& output) {
    output = "[Rasterizer Test] 1.2\n";
    output += "Skip VertexShader, direct rasterizer test\n";
    output += "Draw triangle at screen coords:\n";
    output += "  v0=(100,100) RED\n";
    output += "  v1=(300,100) GREEN\n";
    output += "  v2=(200,300) BLUE\n";
}
