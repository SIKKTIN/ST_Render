#include "TestModule_FrameBuffer.hpp"
#include <imgui.h>
#include <SDL2/SDL.h>
#include <iostream>

TestModule_FrameBuffer::TestModule_FrameBuffer()
    : m_frameBuffer(nullptr), m_renderMode(1) {
}

TestModule_FrameBuffer::~TestModule_FrameBuffer() {
    delete m_frameBuffer;
}

void TestModule_FrameBuffer::render(void* canvasTexture, int canvasW, int canvasH) {
    // 初始化 FrameBuffer
    if (!m_frameBuffer || m_frameBuffer->getWidth() != canvasW || m_frameBuffer->getHeight() != canvasH) {
        delete m_frameBuffer;
        m_frameBuffer = new ST::FrameBuffer();
        m_frameBuffer->initialize(canvasW, canvasH);
    }

    // 清空 FrameBuffer 为灰色
    m_frameBuffer->clear(ST::Color(0.2f, 0.2f, 0.2f, 1.0f));

    // 手动在 FrameBuffer 里画几个像素
    m_frameBuffer->setPixel(320, 240, ST::Color(1, 1, 0, 1));
    m_frameBuffer->setPixel(321, 240, ST::Color(1, 1, 0, 1));
    m_frameBuffer->setPixel(322, 240, ST::Color(1, 1, 0, 1));
    m_frameBuffer->setPixel(320, 241, ST::Color(1, 1, 0, 1));
    m_frameBuffer->setPixel(321, 241, ST::Color(1, 1, 0, 1));
    m_frameBuffer->setPixel(322, 241, ST::Color(1, 1, 0, 1));

    m_frameBuffer->setPixel(100, 100, ST::Color(1, 0, 0, 1));
    m_frameBuffer->setPixel(540, 430, ST::Color(0, 1, 0, 1));

    auto& pixels = m_frameBuffer->getPixels();
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

bool TestModule_FrameBuffer::renderControls() {
    ImGui::Separator();
    ImGui::Text("FrameBuffer Controls");
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

void TestModule_FrameBuffer::runConsole(std::string& output) {
    output = "[FrameBuffer Test] 1.1\n";
    output += "Mode: " + std::string(m_renderMode == 0 ? "RenderDrawPoint" : "Texture") + "\n";
    output += "Gray bg with yellow/red/green pixels\n";
}
