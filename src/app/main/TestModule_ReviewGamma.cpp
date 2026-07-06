#include "TestModule_ReviewGamma.hpp"

void TestModule_ReviewGamma::render(void* canvasTexture, int canvasW, int canvasH) {
    SDL_Renderer* renderer = (SDL_Renderer*)canvasTexture;
    SDL_SetRenderDrawColor(renderer, 30, 90, 60, 255);
    SDL_RenderClear(renderer);
}

void TestModule_ReviewGamma::runConsole(std::string& output) {
    output = "[Review / Gamma] placeholder - not implemented yet\n";
}
