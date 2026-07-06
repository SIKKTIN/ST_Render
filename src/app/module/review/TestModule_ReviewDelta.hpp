#pragma once

#include "app/module/IModule.hpp"
#include <SDL2/SDL.h>
#include <string>

// Placeholder sub-module for the Review category.
class TestModule_ReviewDelta : public IModule {
public:
    const char* getName() const override { return "Delta"; }
    const char* getCategory() const override { return "Review"; }
    bool hasConsoleOutput() const override { return true; }

    void render(void* canvasTexture, int canvasW, int canvasH) override;
    void runConsole(std::string& output) override;
};
