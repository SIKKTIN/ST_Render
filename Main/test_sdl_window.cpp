#include "test_sdl_window.hpp"
#include "../renderer/Renderer.hpp"
#include "../geometry/Mesh.hpp"
#include "../geometry/Triangle.hpp"
#include "../geometry/Vertex.hpp"
#include "../camera/Camera.hpp"
#include "../transform/Transform.hpp"
#include "../buffer/FrameBuffer.hpp"
#include <SDL2/SDL.h>
#include <iostream>

namespace ST {
namespace Test {

void testSDLWindow() {
    constexpr int WIDTH = 640;
    constexpr int HEIGHT = 480;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return;
    }

    SDL_Window* window = SDL_CreateWindow(
        "ST Render - Triangle Test",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return;
    }

    // Create software renderer
    ST::Renderer stRenderer(WIDTH, HEIGHT);
    stRenderer.clear(Color::black());

    // Setup camera
    Camera camera;
    camera.m_eye = Vector3(0, 0, 3);
    camera.m_target = Vector3(0, 0, 0);
    camera.m_aspect = static_cast<float>(WIDTH) / HEIGHT;
    stRenderer.setCamera(camera);

    // Create triangle mesh
    Mesh triangleMesh;
    triangleMesh.addVertex(Vertex(Vector3(-0.5f, -0.4f, -2.0f), Vector3(0, 0, -1), Vector2::zero(), Color::red()));
    triangleMesh.addVertex(Vertex(Vector3( 0.5f, -0.4f, -2.0f), Vector3(0, 0, -1), Vector2::one(), Color::green()));
    triangleMesh.addVertex(Vertex(Vector3( 0.0f,  0.4f, -2.0f), Vector3(0, 0, -1), Vector2(0.5f, 0), Color::blue()));
    triangleMesh.addTriangle(0, 1, 2);

    // Add a light so the triangle is visible
    stRenderer.addLight(
        ST::Light::directional(Vector3(0, 0, 1), Color::white(), 1.0f)
    );

    // Render triangle
    stRenderer.render(triangleMesh);

    // Create SDL texture
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        WIDTH, HEIGHT
    );

    // Convert framebuffer pixels to SDL format (ABGR8888)
    std::vector<uint32_t> pixels(WIDTH * HEIGHT);
    const auto& fbPixels = stRenderer.getFrameBuffer().getPixels();
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        uint8_t r = static_cast<uint8_t>(std::min(255.0f, fbPixels[i].r * 255.0f));
        uint8_t g = static_cast<uint8_t>(std::min(255.0f, fbPixels[i].g * 255.0f));
        uint8_t b = static_cast<uint8_t>(std::min(255.0f, fbPixels[i].b * 255.0f));
        uint8_t a = static_cast<uint8_t>(std::min(255.0f, fbPixels[i].a * 255.0f));
        pixels[i] = (a << 24) | (b << 16) | (g << 8) | r;
    }
    SDL_UpdateTexture(texture, nullptr, pixels.data(), WIDTH * sizeof(uint32_t));

    // Count rendered pixels
    int renderedPixels = 0;
    for (const auto& p : fbPixels) {
        if (p.r > 0 || p.g > 0 || p.b > 0) renderedPixels++;
    }
    std::cout << "Rendered pixels: " << renderedPixels << std::endl;

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = false;
        }

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    std::cout << "SDL Window test completed!" << std::endl;
}

}
}
