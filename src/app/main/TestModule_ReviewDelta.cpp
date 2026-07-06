#include "TestModule_ReviewDelta.hpp"

void TestModule_ReviewDelta::render(void* canvasTexture, int canvasW, int canvasH) {
    SDL_Renderer* renderer = (SDL_Renderer*)canvasTexture;
    SDL_SetRenderDrawColor(renderer, 90, 90, 30, 255);
    SDL_RenderClear(renderer);
}

void TestModule_ReviewDelta::runConsole(std::string& output) {
    output = "[Review / Delta] placeholder - not implemented yet\n";
}
