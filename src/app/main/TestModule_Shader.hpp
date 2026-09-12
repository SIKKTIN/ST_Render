#pragma once

#include "app/module/IModule.hpp"
#include "renderer/geometry/Mesh.hpp"
#include "renderer/renderer/Renderer.hpp"
#include "ShaderEditorUI.hpp"
#include <SDL2/SDL.h>
#include <string>

class TestModule_Shader : public IModule {
public:
    TestModule_Shader();
    ~TestModule_Shader() override;

    const char* getName() const override { return "Shader Test"; }
    const char* getCategory() const override { return "TestComponent"; }
    void render(void* canvasTexture, int canvasW, int canvasH) override;
    bool renderControls() override;
    void runConsole(std::string& output) override;

private:
    void uploadFrame(SDL_Renderer* renderer);

    ST::Renderer m_renderer;
    ST::Mesh m_mesh;
    ShaderEditorUI m_shaderUI;
    SDL_Texture* m_outputTexture = nullptr;
    SDL_Renderer* m_sdlRenderer = nullptr;
    int m_textureWidth = 0;
    int m_textureHeight = 0;
};
