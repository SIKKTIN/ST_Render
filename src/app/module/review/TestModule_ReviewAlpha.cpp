#include "TestModule_ReviewAlpha.hpp"

void TestModule_ReviewAlpha::render(void* canvasTexture, int canvasW, int canvasH) {
    SDL_Renderer* renderer = (SDL_Renderer*)canvasTexture;
    SDL_SetRenderDrawColor(renderer, 30, 60, 90, 255);
    SDL_RenderClear(renderer);
}

void TestModule_ReviewAlpha::runConsole(std::string& output) {
    output = "[Review / Alpha] placeholder - not implemented yet\n";
}
