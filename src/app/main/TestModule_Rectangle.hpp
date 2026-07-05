#include "app/module/IModule.hpp"
#include <SDL2/SDL.h>

class TestModule_Rectangle : public IModule {
public:
    const char* getName() const override { return "SDL Rectangle"; }
    bool hasConsoleOutput() const override { return true; }

    void render(void* canvasTexture, int canvasW, int canvasH) override {
        SDL_Renderer* renderer = (SDL_Renderer*)canvasTexture;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);

        SDL_Rect rect;
        rect.x = canvasW / 2 - 100;
        rect.y = canvasH / 2 - 60;
        rect.w = 200;
        rect.h = 120;

        SDL_RenderFillRect(renderer, &rect);
    }

    void runConsole(std::string& output) override {
        output = "[SDL Rectangle] Test passed.\n";
    }
};
