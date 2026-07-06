#include "TestModule_ReviewBeta.hpp"

void TestModule_ReviewBeta::render(void* canvasTexture, int canvasW, int canvasH) {
    SDL_Renderer* renderer = (SDL_Renderer*)canvasTexture;
    SDL_SetRenderDrawColor(renderer, 90, 30, 60, 255);
    SDL_RenderClear(renderer);
}

void TestModule_ReviewBeta::runConsole(std::string& output) {
    output = "[Review / Beta] placeholder - not implemented yet\n";
}
