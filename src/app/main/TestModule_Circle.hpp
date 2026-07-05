#include "ITestModule.hpp"
#include <SDL2/SDL.h>

class TestModule_Circle : public ITestModule {
public:
    const char* getName() const override { return "SDL Circle"; }
    bool hasConsoleOutput() const override { return true; }

    void render(void* canvasTexture, int canvasW, int canvasH) override {
        SDL_Renderer* renderer = (SDL_Renderer*)canvasTexture;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);

        int centerX = canvasW / 2;
        int centerY = canvasH / 2;
        int radius = 100;

        for (int y = -radius; y <= radius; ++y) {
            for (int x = -radius; x <= radius; ++x) {
                if (x * x + y * y <= radius * radius) {
                    SDL_RenderDrawPoint(renderer, centerX + x, centerY + y);
                }
            }
        }
    }

    void runConsole(std::string& output) override {
        output = "[SDL Circle] Test passed.\n";
    }
};
