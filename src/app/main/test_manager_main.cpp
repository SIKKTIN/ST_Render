#include <imgui.h>
#include <SDL2/SDL.h>
#include <iostream>
#include <vector>

#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "app/module/IModule.hpp"
#include "TestModule_Circle.hpp"
#include "TestModule_Rectangle.hpp"
#include "TestModule_FrameBuffer.hpp"
#include "TestModule_Rasterizer.hpp"
#include "TestModule_VertexShader.hpp"
#include "TestModule_2D_Scene.hpp"
#include "TestModule_3DRender.hpp"
#include "TestModule_Texture.hpp"
#include "TestModule_Music.hpp"
#include "TestModule_DragWindow.hpp"
#include "TestModule_Sprite2D.hpp"
#include "TestModule_NetworkTest.hpp"
#include "engine/editor/TextureManager.hpp"
#include "engine/editor/AudioManager.hpp"

int main(int argc, char* argv[]) {
    std::cout << "=== ST Render - Test Manager ===" << std::endl;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    ST::TextureManager::getInstance().scanResourceFolder();
    ST::AudioManager::getInstance().scanAudioFolder();

    SDL_Window* window = SDL_CreateWindow(
        "ST Render - Test Manager",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::vector<IModule*> modules;
    modules.push_back(new TestModule_FrameBuffer());
    modules.push_back(new TestModule_Rasterizer());
    modules.push_back(new TestModule_VertexShader());
    modules.push_back(new TestModule_2D_Scene());
    modules.push_back(new TestModule_3DRender());
    modules.push_back(new TestModule_Sprite2D());
    modules.push_back(new TestModule_DragWindow());
    modules.push_back(new TestModule_Texture());
    modules.push_back(new TestModule_Music());
    modules.push_back(new TestModule_NetworkTest());
    modules.push_back(new TestModule_Circle());
    modules.push_back(new TestModule_Rectangle());

    int selectedModule = 0;
    std::string consoleOutput;
    float consoleHeight = 120.0f;
    float createObjPanelW = 200.0f;
    bool draggingSplitter = false;
    bool draggingCreateSplitter = false;
    int mouseX = 0, mouseY = 0;

    // Track canvas screen bounds (set during ImGui render, used in event loop)
    int canvasMinX = 0, canvasMinY = 0;
    int canvasMaxX = 0, canvasMaxY = 0;

    constexpr int CANVAS_W = 640;
    constexpr int CANVAS_H = 480;
    SDL_Texture* canvas = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,
        CANVAS_W, CANVAS_H
    );

    auto runModule = [&](int index, bool rerender = true) {
        if (index < 0 || index >= (int)modules.size()) return;
        consoleOutput.clear();
        modules[index]->runConsole(consoleOutput);
        if (rerender) {
            SDL_SetRenderTarget(renderer, canvas);
            modules[index]->render(renderer, CANVAS_W, CANVAS_H);
            SDL_SetRenderTarget(renderer, nullptr);
        }
    };
    runModule(selectedModule);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    for (auto* m : modules) {
        m->setRenderer(renderer);
    }

    bool running = true;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN) {
                modules[selectedModule]->onKeyDown(event.key.keysym.sym);
                if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
            }

            bool canvasHandled = false;
            // Route canvas-space events to UI system
            if (canvasMinX != canvasMaxX) {
                int cx = 0, cy = 0;
                bool inCanvas = false;
                if (event.type == SDL_MOUSEMOTION) {
                    cx = event.motion.x - canvasMinX;
                    cy = event.motion.y - canvasMinY;
                    inCanvas = (event.motion.x >= canvasMinX && event.motion.x < canvasMaxX &&
                                event.motion.y >= canvasMinY && event.motion.y < canvasMaxY);
                } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
                    cx = event.button.x - canvasMinX;
                    cy = event.button.y - canvasMinY;
                    inCanvas = (event.button.x >= canvasMinX && event.button.x < canvasMaxX &&
                                event.button.y >= canvasMinY && event.button.y < canvasMaxY);
                }
                if (inCanvas) {
                    if (event.type == SDL_MOUSEMOTION) {
                        mouseX = event.motion.x;
                        mouseY = event.motion.y;
                    } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
                        mouseX = event.button.x;
                        mouseY = event.button.y;
                    }
                    if (auto* s2d = dynamic_cast<TestModule_2D_Scene*>(modules[selectedModule])) {
                        if (event.type == SDL_MOUSEMOTION) {
                            s2d->onCanvasMouseMove(cx, cy);
                            canvasHandled = true;
                        } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                            s2d->onCanvasMouseDown(event.button.button, cx, cy);
                            canvasHandled = true;
                        } else if (event.type == SDL_MOUSEBUTTONUP) {
                            s2d->onCanvasMouseUp(event.button.button, cx, cy);
                            canvasHandled = true;
                        }
                    }
                }
            }

            if (!canvasHandled) {
                if (event.type == SDL_MOUSEMOTION) {
                    mouseX = event.motion.x;
                    mouseY = event.motion.y;
                    modules[selectedModule]->onMouseMove(mouseX, mouseY);
                } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                    modules[selectedModule]->onMouseDown(event.button.button, event.button.x, event.button.y);
                } else if (event.type == SDL_MOUSEBUTTONUP) {
                    modules[selectedModule]->onMouseUp(event.button.button);
                }
            }

            if (event.type == SDL_MOUSEWHEEL) {
                int wx = event.wheel.mouseX;
                int wy = event.wheel.mouseY;
                mouseX = wx;
                mouseY = wy;

                int canvasX0 = 220 + 200; // leftPanelW + createObjPanelW
                int canvasW = 1280 - 220 - 200 - 220;
                int canvasH = CANVAS_H;
                bool inCanvas = (wx >= canvasX0 && wx < canvasX0 + canvasW && wy >= 0 && wy < canvasH);

                if (inCanvas && selectedModule >= 0 && selectedModule < (int)modules.size()) {
                    int cx = wx - canvasX0;
                    int cy = wy;
                    modules[selectedModule]->onWheel(
                        (float)event.wheel.x, (float)event.wheel.y,
                        cx, cy, canvasW, canvasH
                    );
                } else {
                    modules[selectedModule]->onWheel(
                        (float)event.wheel.x, (float)event.wheel.y,
                        mouseX, mouseY, CANVAS_W, canvasH
                    );
                }
            }
        }

        if (modules[selectedModule]->needsRealTimeUpdate()) {
            Uint64 now = SDL_GetPerformanceCounter();
            static Uint64 last = now;
            float dt = (float)(now - last) / SDL_GetPerformanceFrequency();
            last = now;
            modules[selectedModule]->update(dt);
            SDL_SetRenderTarget(renderer, canvas);
            modules[selectedModule]->render(renderer, CANVAS_W, CANVAS_H);
            SDL_SetRenderTarget(renderer, nullptr);
            runModule(selectedModule, false);
        }

        ImGui_ImplSDL2_NewFrame();
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui::NewFrame();

        float leftPanelW = 220.0f;
        float rightPanelW = 220.0f;
        float topAreaH = 550.0f;

        // Left panel - Test list
        {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(leftPanelW, 720));
            ImGui::Begin("Tests", nullptr,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse);
            ImGui::Text("ST Render");
            ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "Test Manager");
            ImGui::Separator();

            for (size_t i = 0; i < modules.size(); ++i) {
                if (ImGui::Selectable(modules[i]->getName(), selectedModule == (int)i, 0, ImVec2(180, 0))) {
                    if (selectedModule != (int)i) { selectedModule = (int)i; runModule(selectedModule); }
                }
            }

            ImGui::End();
        }

        // Right panel - Controls
        {
            ImGui::SetNextWindowPos(ImVec2(1280 - rightPanelW, 0));
            ImGui::SetNextWindowSize(ImVec2(rightPanelW, topAreaH));
            ImGui::Begin("Controls", nullptr,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse);

            ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "%s", modules[selectedModule]->getName());
            ImGui::Separator();

            static int viewportMode_local = 0;
            if (selectedModule >= 0 && selectedModule < (int)modules.size()) {
                IModule* mod = modules[selectedModule];
                if (auto* scene2d = dynamic_cast<TestModule_2D_Scene*>(mod)) {
                    viewportMode_local = scene2d->getViewportMode();
                } else {
                    viewportMode_local = 0;
                }
            }
            if (ImGui::RadioButton("Scene", viewportMode_local == 0)) { viewportMode_local = 0; if (selectedModule >= 0 && selectedModule < (int)modules.size()) { if (auto* s = dynamic_cast<TestModule_2D_Scene*>(modules[selectedModule])) s->setViewportMode(0); } }
            ImGui::SameLine();
            if (ImGui::RadioButton("Game", viewportMode_local == 1)) { viewportMode_local = 1; if (selectedModule >= 0 && selectedModule < (int)modules.size()) { if (auto* s = dynamic_cast<TestModule_2D_Scene*>(modules[selectedModule])) s->setViewportMode(1); } }
            ImGui::Separator();

            modules[selectedModule]->renderControls();

            ImGui::End();
        }

        // Create Object panel (left of canvas, above console)
        {
            ImGui::SetNextWindowPos(ImVec2(leftPanelW, 0));
            ImGui::SetNextWindowSize(ImVec2(createObjPanelW, 720));
            ImGui::Begin("Create Object", nullptr,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse);
            modules[selectedModule]->renderCreatePanel();
            ImGui::End();
        }

        // Call module overlays (e.g. popup windows) after all panels are closed
        modules[selectedModule]->renderOverlays();

        // Canvas + Console (with splitter)
        {
            float canvasW = 1280.0f - leftPanelW - createObjPanelW - rightPanelW;
            ImGui::SetNextWindowPos(ImVec2(leftPanelW + createObjPanelW, 0));
            ImGui::SetNextWindowSize(ImVec2(canvasW, 720));
            ImGui::Begin("Output", nullptr,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse);

            if (selectedModule >= 0 && selectedModule < (int)modules.size()) {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%s", modules[selectedModule]->getName());
                ImGui::Separator();

                float splitterH = 8.0f;
                float minCanvasH = 100.0f;
                float minConsoleH = 50.0f;
                float availH = ImGui::GetContentRegionAvail().y - splitterH;

                if (consoleHeight < minConsoleH) consoleHeight = minConsoleH;
                if (consoleHeight > availH - minCanvasH) consoleHeight = availH - minCanvasH;

                float thisCanvasH = availH - consoleHeight;

                ImGui::Image((void*)(intptr_t)canvas, ImVec2(canvasW, thisCanvasH));

                // Record canvas screen bounds for event routing
                ImVec2 cMin = ImGui::GetItemRectMin();
                ImVec2 cMax = ImGui::GetItemRectMax();
                canvasMinX = (int)cMin.x;
                canvasMinY = (int)cMin.y;
                canvasMaxX = (int)cMax.x;
                canvasMaxY = (int)cMax.y;
                modules[selectedModule]->renderUIOverlay(
                    canvasMinX, canvasMinY, (int)canvasW, (int)thisCanvasH);

                ImGui::Button("##Splitter", ImVec2(-1, splitterH));

                bool splitterHovered = ImGui::IsItemHovered();
                if (splitterHovered) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                }

                if (draggingSplitter) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                    if (!ImGui::IsMouseDown(0)) {
                        draggingSplitter = false;
                    } else {
                        consoleHeight -= ImGui::GetIO().MouseDelta.y;
                    }
                } else if (ImGui::IsItemClicked(0)) {
                    draggingSplitter = true;
                }

                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Console Output:");
                ImGui::BeginChild("Console", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
                ImGui::TextUnformatted(consoleOutput.c_str());
                ImGui::EndChild();
            }

            ImGui::End();
        }

        // Vertical splitter (on top of everything)
        {
            ImGui::SetNextWindowPos(ImVec2(leftPanelW + createObjPanelW - 4, 0));
            ImGui::SetNextWindowSize(ImVec2(8, 720));
            ImGui::Begin("##VSplitter", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoBackground);

            ImGui::InvisibleButton("##VSplitterBtn", ImVec2(8, 720));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 wp = ImGui::GetWindowPos();
            ImU32 col = IM_COL32(55, 55, 55, 255);
            if (ImGui::IsItemHovered() || draggingCreateSplitter) col = IM_COL32(80, 130, 255, 255);
            dl->AddRectFilled(ImVec2(wp.x, wp.y), ImVec2(wp.x + 8, wp.y + 720), col);

            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            if (draggingCreateSplitter) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                if (!ImGui::IsMouseDown(0)) {
                    draggingCreateSplitter = false;
                } else {
                    createObjPanelW += ImGui::GetIO().MouseDelta.x;
                    if (createObjPanelW < 80.0f) createObjPanelW = 80.0f;
                    if (createObjPanelW > 400.0f) createObjPanelW = 400.0f;
                }
            } else if (ImGui::IsItemClicked(0)) {
                draggingCreateSplitter = true;
            }

            ImGui::End();
        }

        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);

        if (modules[selectedModule]->needsRerender) {
            modules[selectedModule]->needsRerender = false;
            SDL_SetRenderTarget(renderer, canvas);
            modules[selectedModule]->render(renderer, CANVAS_W, CANVAS_H);
            SDL_SetRenderTarget(renderer, nullptr);
        }
    }

    for (auto* m : modules) delete m;
    SDL_DestroyTexture(canvas);
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
