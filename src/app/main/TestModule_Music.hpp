#pragma once

#include "app/module/IModule.hpp"
#include <SDL2/SDL.h>
#include <string>

class TestModule_Music : public IModule {
public:
    TestModule_Music();
    ~TestModule_Music();

    const char* getName() const override { return "Music Test"; }
    bool hasRenderOutput() const override { return true; }

    void render(void* canvasTexture, int canvasW, int canvasH) override;
    bool renderControls() override;
    void runConsole(std::string& output) override;

private:
    bool m_musicLoaded = false;
    bool m_playing = false;
    bool m_paused = false;
    std::string m_output;
};
