#pragma once

#include "app/module/IModule.hpp"
#include <SDL2/SDL.h>
#include <string>

// Placeholder sub-module for the Review category.
// Real visuals will be filled in later; for now it clears the canvas
// to a distinctive color and prints a tag in the console so the
// review category is observable in the UI.
class TestModule_ReviewAlpha : public IModule {
public:
    const char* getName() const override { return "Alpha"; }
    const char* getCategory() const override { return "Review"; }
    bool hasConsoleOutput() const override { return true; }

    void render(void* canvasTexture, int canvasW, int canvasH) override;
    void runConsole(std::string& output) override;
};
