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
#include "TestModule_3DRender.hpp"
#include "TestModule_Texture.hpp"
#include "TestModule_Music.hpp"
#include "TestModule_DragWindow.hpp"
#include "TestModule_NetworkTest.hpp"
#include "app/module/review/TestModule_ReviewMath.hpp"
#include "app/module/review/TestModule_ReviewBeta.hpp"
#include "app/module/review/TestModule_ReviewGamma.hpp"
#include "app/module/review/TestModule_ReviewDelta.hpp"
#include "engine/editor/TextureManager.hpp"
#include "engine/editor/AudioManager.hpp"

// ---------------------------------------------------------------------------
// Layout constants (window size, panel widths, canvas size).
// Kept here so event-routing code can use the same values the render uses.
// ---------------------------------------------------------------------------
namespace Layout {
    constexpr int   WINDOW_W          = 1280;
    constexpr int   WINDOW_H          = 720;
    constexpr float MENU_BAR_H        = 24.0f;
    constexpr float LEFT_PANEL_W      = 220.0f;
    constexpr float CREATE_PANEL_W    = 200.0f;
    constexpr float RIGHT_PANEL_W     = 220.0f;
    constexpr float CREATE_PANEL_MIN  = 80.0f;
    constexpr float CREATE_PANEL_MAX  = 400.0f;
    constexpr float RIGHT_PANEL_MIN   = 120.0f;
    constexpr float RIGHT_PANEL_MAX   = 480.0f;
    constexpr float TOP_AREA_H        = 550.0f;     // controls panel height
    constexpr int   CANVAS_W          = 640;        // render-target size
    constexpr int   CANVAS_H          = 480;        // render-target size
    constexpr int   SPLITTER_HALF_W   = 4;          // vertical splitter strip width / 2
    constexpr int   CANVAS_Y0         = 24;         // = (int)MENU_BAR_H; canvas screen Y start
}

int main(int argc, char* argv[]) {
    std::cout << "=== ST Render - Test Manager ===" << std::endl;

    enum class Theme { Dark, Light };

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
        Layout::WINDOW_W, Layout::WINDOW_H,
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

    // Module tree: a flat list of entries; each entry is either a leaf
    // (a real IModule) or a folder-like "group" (e.g. Review).
    //
    // We build this in two stages:
    //   1) Build a single list of all leaf modules.
    //   2) Walk that list and emit a final `entries` list:
    //        - a module whose getCategory() == "" becomes a top-level leaf
    //        - a module whose getCategory() == "Review" (or any category)
    //          becomes a child entry under the matching group
    //      The group itself is emitted once per category, in the order
    //      its first sub-module appears.  Children NEVER appear at the
    //      top level, by construction.
    struct ModuleLeaf {
        IModule* mod;
    };
    struct ModuleGroup {
        std::vector<IModule*> children;
        const char* label = "";
    };
    struct ModuleEntry {
        const char* label = "";
        bool isGroup = false;
        ModuleLeaf  leaf;
        ModuleGroup group;
        // For non-group entries, pointer to the group's slot in `groups`
        // if this entry is a child; nullptr if top-level.
        ModuleGroup* parent = nullptr;
    };

    // 1) Source of truth: every leaf module lives here.
    std::vector<IModule*> allLeaves = {
        new TestModule_FrameBuffer(),
        new TestModule_Rasterizer(),
        new TestModule_3DRender(),
        new TestModule_DragWindow(),
        new TestModule_Texture(),
        new TestModule_Music(),
        new TestModule_NetworkTest(),
        new TestModule_Circle(),
        new TestModule_Rectangle(),
        new TestModule_ReviewMath(),
        new TestModule_ReviewBeta(),
        new TestModule_ReviewGamma(),
        new TestModule_ReviewDelta(),
    };

    // 2) Aggregate by category.  `groups` keeps stable addresses for
    //    each category so we can point entries.parent at them.
    std::vector<ModuleGroup> groups;
    auto findOrCreateGroup = [&](const char* cat) -> ModuleGroup* {
        for (auto& g : groups) if (std::strcmp(g.label, cat) == 0) return &g;
        groups.push_back(ModuleGroup{ {}, cat });
        return &groups.back();
    };

    std::vector<ModuleEntry> entries;
    for (IModule* m : allLeaves) {
        const char* cat = m->getCategory();
        if (cat && cat[0] != '\0') {
            ModuleGroup* g = findOrCreateGroup(cat);
            g->children.push_back(m);
            entries.push_back({ m->getName(), false, { m }, {}, g });
        } else {
            entries.push_back({ m->getName(), false, { m }, {}, nullptr });
        }
    }

    // 3) Re-order so the final layout is:
    //      [top-level leaves] then, per group: [group entry, its children].
    //      This matches what the user sees in the left panel.
    std::vector<ModuleEntry> ordered;
    for (auto& e : entries) {
        if (!e.isGroup && e.parent == nullptr) ordered.push_back(e);
    }
    for (auto& g : groups) {
        ordered.push_back({ g.label, true, {}, { g.children, g.label }, nullptr });
        for (auto& e : entries) {
            if (e.parent == &g) ordered.push_back(e);
        }
    }
    entries = std::move(ordered);

    // Default selection: first leaf module (Frame Buffer).
    int selectedModule = 0;
    auto selectedLeaf = [&]() -> IModule* {
        if (selectedModule < 0 || selectedModule >= (int)entries.size()) return nullptr;
        if (entries[selectedModule].isGroup) return nullptr;
        return entries[selectedModule].leaf.mod;
    };
    auto deliverToLeaf = [&](auto&& fn) {
        if (auto* m = selectedLeaf()) fn(m);
    };
    auto findNextLeaf = [&](int startAfter, int dir) -> int {
        // dir = +1 for next, -1 for prev. Skips groups.
        for (int i = startAfter + dir; i >= 0 && i < (int)entries.size(); i += dir) {
            if (!entries[i].isGroup) return i;
        }
        return startAfter;
    };
    auto firstLeafInGroup = [&](const ModuleGroup& g) -> int {
        const auto& kids = g.children;
        for (size_t i = 0; i < entries.size(); ++i) {
            if (!entries[i].isGroup) {
                for (IModule* k : kids) {
                    if (entries[i].leaf.mod == k) return (int)i;
                }
            }
        }
        return -1;
    };
    std::string consoleOutput;
    float consoleHeight = 120.0f;
    float createObjPanelW = Layout::CREATE_PANEL_W;
    float leftPanelW      = Layout::LEFT_PANEL_W;
    float rightPanelW     = Layout::RIGHT_PANEL_W;
    bool draggingSplitter = false;
    bool draggingCreateSplitter = false;
    bool draggingRightSplitter = false;
    int mouseX = 0, mouseY = 0;

    // Track canvas screen bounds (set during ImGui render, used in event loop)
    int canvasMinX = 0, canvasMinY = 0;
    int canvasMaxX = 0, canvasMaxY = 0;

    SDL_Texture* canvas = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,
        Layout::CANVAS_W, Layout::CANVAS_H
    );

    auto runModule = [&](int index, bool rerender = true) {
        if (index < 0 || index >= (int)entries.size()) return;
        if (entries[index].isGroup) return;
        IModule* m = entries[index].leaf.mod;
        consoleOutput.clear();
        m->runConsole(consoleOutput);
        if (rerender) {
            SDL_SetRenderTarget(renderer, canvas);
            m->render(renderer, Layout::CANVAS_W, Layout::CANVAS_H);
            SDL_SetRenderTarget(renderer, nullptr);
        }
    };
    runModule(selectedModule);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    auto applyTheme = [](Theme t) {
        if (t == Theme::Dark) {
            ImGui::StyleColorsDark();
            ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
            ImGui::GetStyle().Colors[ImGuiCol_WindowBg]  = ImVec4(0.06f, 0.06f, 0.07f, 1.0f);
            ImGui::GetStyle().Colors[ImGuiCol_Text]      = ImVec4(0.90f, 0.90f, 0.92f, 1.0f);
        } else {
            ImGui::StyleColorsLight();
            ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg] = ImVec4(0.88f, 0.88f, 0.90f, 1.0f);
            ImGui::GetStyle().Colors[ImGuiCol_WindowBg]  = ImVec4(0.96f, 0.96f, 0.97f, 1.0f);
            ImGui::GetStyle().Colors[ImGuiCol_Text]      = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
            ImGui::GetStyle().Colors[ImGuiCol_Border]    = ImVec4(0.60f, 0.60f, 0.65f, 1.0f);
            ImGui::GetStyle().Colors[ImGuiCol_Separator] = ImVec4(0.65f, 0.65f, 0.70f, 1.0f);
        }
    };

    Theme currentTheme = Theme::Dark;
    applyTheme(currentTheme);

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // Hand the SDL renderer to every leaf so modules can upload textures.
    for (const auto& e : entries) {
        if (!e.isGroup) e.leaf.mod->setRenderer(renderer);
    }

    bool running = true;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN) {
                deliverToLeaf([&](IModule* m) { m->onKeyDown(event.key.keysym.sym); });
                if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
            }

            bool canvasHandled = false;
            // First, decide whether the mouse is over the canvas this frame.
            // We use the on-screen Image rect (recorded during the last ImGui
            // render) so we know whether a mouse event landed inside the
            // canvas or inside one of the editor panels (Tests / Controls /
            // Create Object / Menu / Console).  Outside-canvas events should
            // never reach the module's camera / scene handlers -- the panels
            // themselves handle their own clicks via ImGui.
            bool inCanvas = false;
            if (canvasMinX != canvasMaxX) {
                if (event.type == SDL_MOUSEMOTION) {
                    inCanvas = (event.motion.x >= canvasMinX && event.motion.x < canvasMaxX &&
                                event.motion.y >= canvasMinY && event.motion.y < canvasMaxY);
                } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
                    inCanvas = (event.button.x >= canvasMinX && event.button.x < canvasMaxX &&
                                event.button.y >= canvasMinY && event.button.y < canvasMaxY);
                }
            }

            // Route canvas-space events to the selected module (generic).
            //
            // Modules that override onCanvasMouseDown/Up/Move get render-target
            // coordinates (Layout::CANVAS_W x CANVAS_H). Modules that don't
            // override fall through to onMouse* below and receive raw screen
            // coordinates instead.
            if (inCanvas) {
                int cx = 0, cy = 0;
                if (event.type == SDL_MOUSEMOTION) {
                    cx = event.motion.x - canvasMinX;
                    cy = event.motion.y - canvasMinY;
                } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
                    cx = event.button.x - canvasMinX;
                    cy = event.button.y - canvasMinY;
                }
                if (event.type == SDL_MOUSEMOTION) {
                    mouseX = event.motion.x;
                    mouseY = event.motion.y;
                } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
                    mouseX = event.button.x;
                    mouseY = event.button.y;
                }
                auto* mod = selectedLeaf();
                if (event.type == SDL_MOUSEMOTION) {
                    int screenW = canvasMaxX - canvasMinX;
                    int screenH = canvasMaxY - canvasMinY;
                    if (screenW > 0 && screenH > 0) {
                        cx = cx * Layout::CANVAS_W / screenW;
                        cy = cy * Layout::CANVAS_H / screenH;
                    }
                    mod->onCanvasMouseMove(cx, cy);
                    canvasHandled = true;
                } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                    int screenW = canvasMaxX - canvasMinX;
                    int screenH = canvasMaxY - canvasMinY;
                    if (screenW > 0 && screenH > 0) {
                        cx = cx * Layout::CANVAS_W / screenW;
                        cy = cy * Layout::CANVAS_H / screenH;
                    }
                    mod->onCanvasMouseDown(event.button.button, cx, cy);
                    canvasHandled = true;
                } else if (event.type == SDL_MOUSEBUTTONUP) {
                    int screenW = canvasMaxX - canvasMinX;
                    int screenH = canvasMaxY - canvasMinY;
                    if (screenW > 0 && screenH > 0) {
                        cx = cx * Layout::CANVAS_W / screenW;
                        cy = cy * Layout::CANVAS_H / screenH;
                    }
                    mod->onCanvasMouseUp(event.button.button, cx, cy);
                    canvasHandled = true;
                }
            }

            // Outside-canvas events are NOT forwarded to module handlers.
            // Editor panels (Tests / Controls / Create Object) and the menu
            // bar own those clicks via ImGui, and forwarding them to modules
            // causes phantom camera rotations and other interactions that
            // should only happen over the canvas itself.

            if (event.type == SDL_MOUSEWHEEL) {
                int wx = event.wheel.mouseX;
                int wy = event.wheel.mouseY;
                mouseX = wx;
                mouseY = wy;

                // Only forward wheel events to the module when the cursor is
                // over the canvas.  Wheels over panels belong to ImGui
                // (scrollbars, sliders, etc.) and must NOT reach the module.
                bool wheelInCanvas = (canvasMaxX > canvasMinX && canvasMaxY > canvasMinY &&
                                      wx >= canvasMinX && wx < canvasMaxX &&
                                      wy >= canvasMinY && wy < canvasMaxY);

                if (wheelInCanvas && selectedModule >= 0 && selectedModule < (int)entries.size() && !entries[selectedModule].isGroup) {
                    // Modules expect render-target coordinates (Layout::CANVAS_W × CANVAS_H).
                    // The on-screen Image rect may differ from the render-target size
                    // (Y differs whenever consoleHeight != 0; X coincides by current
                    // layout math but we still normalize both for safety).
                    int screenW = canvasMaxX - canvasMinX;
                    int screenH = canvasMaxY - canvasMinY;
                    int cx = (wx - canvasMinX) * Layout::CANVAS_W / screenW;
                    int cy = (wy - canvasMinY) * Layout::CANVAS_H / screenH;
                    entries[selectedModule].leaf.mod->onWheel(
                        (float)event.wheel.x, (float)event.wheel.y,
                        cx, cy, Layout::CANVAS_W, Layout::CANVAS_H
                    );
                }
            }
        }

        if (auto* m = selectedLeaf(); m && m->needsRealTimeUpdate()) {
            Uint64 now = SDL_GetPerformanceCounter();
            static Uint64 last = now;
            float dt = (float)(now - last) / SDL_GetPerformanceFrequency();
            last = now;
            m->update(dt);
            SDL_SetRenderTarget(renderer, canvas);
            m->render(renderer, Layout::CANVAS_W, Layout::CANVAS_H);
            SDL_SetRenderTarget(renderer, nullptr);
            runModule(selectedModule, false);
        }

        ImGui_ImplSDL2_NewFrame();
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui::NewFrame();

        float topAreaH = Layout::TOP_AREA_H;

        // Top menu bar (File / Edit / View / Help). Sits above all panels;
        // all panels below are pushed down by the *actual* menu bar height,
        // queried from ImGui after rendering, so they sit flush against the
        // menu bar (no gap) and stay correct if the theme changes the font
        // size or frame padding.
        float menuBarH = Layout::MENU_BAR_H; // refined to true height after EndMainMenuBar()
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                ImGui::MenuItem("Open Scene...", "Ctrl+O", false, false);
                ImGui::MenuItem("Save Scene", "Ctrl+S", false, false);
                ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S", false, false);
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) { running = false; }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
                ImGui::MenuItem("Redo", "Ctrl+Y", false, false);
                ImGui::Separator();
                ImGui::MenuItem("Cut",   "Ctrl+X", false, false);
                ImGui::MenuItem("Copy",  "Ctrl+C", false, false);
                ImGui::MenuItem("Paste", "Ctrl+V", false, false);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Reset Layout")) {
                    leftPanelW      = Layout::LEFT_PANEL_W;
                    rightPanelW     = Layout::RIGHT_PANEL_W;
                    createObjPanelW = Layout::CREATE_PANEL_W;
                    consoleHeight   = 120.0f;
                }
                ImGui::Separator();
                ImGui::TextDisabled("Color Theme");
                if (ImGui::MenuItem("Dark",  nullptr, currentTheme == Theme::Dark))  { currentTheme = Theme::Dark;  applyTheme(currentTheme); }
                if (ImGui::MenuItem("Light", nullptr, currentTheme == Theme::Light)) { currentTheme = Theme::Light; applyTheme(currentTheme); }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                ImGui::MenuItem("Documentation...", nullptr, false, false);
                ImGui::MenuItem("About ST Render...", nullptr, false, false);
                ImGui::EndMenu();
            }
            menuBarH = ImGui::GetFrameHeight(); // actual rendered menu bar height
            ImGui::EndMainMenuBar();
        }

        float windowH = (float)Layout::WINDOW_H - menuBarH;

        // Left panel - Test list
        {
            ImGui::SetNextWindowPos(ImVec2(0, menuBarH));
            ImGui::SetNextWindowSize(ImVec2(leftPanelW, windowH));
            ImGui::Begin("Tests", nullptr,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse);
            ImGui::Text("ST Render");
            ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "Test Manager");
            ImGui::Separator();

            // Render only top-level entries (parent == nullptr). Sub-modules of a
            // group live inside the group's TreeNode and are never shown
            // at the top level.
            for (size_t i = 0; i < entries.size(); ++i) {
                const auto& e = entries[i];
                if (e.parent != nullptr) continue;
                if (!e.isGroup) {
                    if (ImGui::Selectable(e.label, selectedModule == (int)i, 0, ImVec2(180, 0))) {
                        if (selectedModule != (int)i) { selectedModule = (int)i; runModule(selectedModule); }
                    }
                } else {
                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                               ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                               ImGuiTreeNodeFlags_SpanFullWidth;
                    bool open = ImGui::TreeNodeEx((void*)(intptr_t)i, flags, "%s", e.label);
                    if (open) {
                        // Children come straight from the group's children
                        // list. Each child is also an entry (so its index
                        // is a valid `selectedModule`).
                        const auto& kids = e.group.children;
                        for (size_t j = 0; j < entries.size(); ++j) {
                            const auto& c = entries[j];
                            if (c.isGroup) continue;
                            bool isThisGroupChild = false;
                            for (IModule* k : kids) if (c.leaf.mod == k) { isThisGroupChild = true; break; }
                            if (!isThisGroupChild) continue;
                            ImGui::Indent();
                            if (ImGui::Selectable(c.label, selectedModule == (int)j, 0, ImVec2(160, 0))) {
                                if (selectedModule != (int)j) { selectedModule = (int)j; runModule(selectedModule); }
                            }
                            ImGui::Unindent();
                        }
                        ImGui::TreePop();
                    }
                }
            }

            ImGui::End();
        }

        // Right panel - Controls
        {
            ImGui::SetNextWindowPos(ImVec2((float)Layout::WINDOW_W - rightPanelW, menuBarH));
            ImGui::SetNextWindowSize(ImVec2(rightPanelW, topAreaH - menuBarH));
            ImGui::Begin("Controls", nullptr,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse);

            ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "%s",
                               selectedLeaf() ? selectedLeaf()->getName() : entries[selectedModule].label);
            ImGui::Separator();

            ImGui::Separator();

            if (auto* m = selectedLeaf()) m->renderControls();

            ImGui::End();
        }

        // Create Object panel (left of canvas, above console)
        {
            ImGui::SetNextWindowPos(ImVec2(leftPanelW, menuBarH));
            ImGui::SetNextWindowSize(ImVec2(createObjPanelW, windowH));
            ImGui::Begin("Create Object", nullptr,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse);
            if (auto* m = selectedLeaf()) m->renderCreatePanel();
            ImGui::End();
        }

        // Call module overlays (e.g. popup windows) after all panels are closed
        if (auto* m = selectedLeaf()) m->renderOverlays();

        // Canvas + Console (with splitter)
        {
            float canvasW = (float)Layout::WINDOW_W - leftPanelW - createObjPanelW - rightPanelW;
            ImGui::SetNextWindowPos(ImVec2(leftPanelW + createObjPanelW, menuBarH));
            ImGui::SetNextWindowSize(ImVec2(canvasW, windowH));
            ImGui::Begin("Output", nullptr,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse);

            if (selectedModule >= 0 && selectedModule < (int)entries.size()) {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%s",
                                   selectedLeaf() ? selectedLeaf()->getName() : entries[selectedModule].label);
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
                if (auto* m = selectedLeaf()) {
                    m->renderUIOverlay(
                        canvasMinX, canvasMinY, (int)canvasW, (int)thisCanvasH);
                }

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
            ImGui::SetNextWindowPos(ImVec2(leftPanelW + createObjPanelW - Layout::SPLITTER_HALF_W, menuBarH));
            ImGui::SetNextWindowSize(ImVec2(2 * Layout::SPLITTER_HALF_W, windowH));
            ImGui::Begin("##VSplitter", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoBackground);

            ImGui::InvisibleButton("##VSplitterBtn", ImVec2(2.0f * Layout::SPLITTER_HALF_W, windowH));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 wp = ImGui::GetWindowPos();
            ImU32 col = IM_COL32(55, 55, 55, 255);
            if (ImGui::IsItemHovered() || draggingCreateSplitter) col = IM_COL32(80, 130, 255, 255);
            dl->AddRectFilled(ImVec2(wp.x, wp.y), ImVec2(wp.x + 2.0f * Layout::SPLITTER_HALF_W, wp.y + windowH), col);

            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            if (draggingCreateSplitter) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                if (!ImGui::IsMouseDown(0)) {
                    draggingCreateSplitter = false;
                } else {
                    createObjPanelW += ImGui::GetIO().MouseDelta.x;
                    if (createObjPanelW < Layout::CREATE_PANEL_MIN) createObjPanelW = Layout::CREATE_PANEL_MIN;
                    if (createObjPanelW > Layout::CREATE_PANEL_MAX) createObjPanelW = Layout::CREATE_PANEL_MAX;
                }
            } else             if (ImGui::IsItemClicked(0)) {
                draggingCreateSplitter = true;
            }

            ImGui::End();
        }

        // Right vertical splitter (between canvas and Controls panel).
        // Mirrors the create-side splitter: dragging left squeezes the
        // canvas and widens Controls; dragging right narrows Controls.
        // The X axis is the right panel's left edge (= WINDOW_W - rightPanelW).
        {
            float handleX = (float)Layout::WINDOW_W - rightPanelW - Layout::SPLITTER_HALF_W;
            ImGui::SetNextWindowPos(ImVec2(handleX, menuBarH));
            ImGui::SetNextWindowSize(ImVec2(2 * Layout::SPLITTER_HALF_W, windowH));
            ImGui::Begin("##RightVSplitter", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoBackground);

            ImGui::InvisibleButton("##RightVSplitterBtn", ImVec2(2.0f * Layout::SPLITTER_HALF_W, windowH));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 wp = ImGui::GetWindowPos();
            ImU32 col = IM_COL32(55, 55, 55, 255);
            if (ImGui::IsItemHovered() || draggingRightSplitter) col = IM_COL32(80, 130, 255, 255);
            dl->AddRectFilled(ImVec2(wp.x, wp.y), ImVec2(wp.x + 2.0f * Layout::SPLITTER_HALF_W, wp.y + windowH), col);

            if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            if (draggingRightSplitter) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                if (!ImGui::IsMouseDown(0)) {
                    draggingRightSplitter = false;
                } else {
                    // Dragging right (positive dx) shrinks the right panel;
                    // dragging left widens it.
                    rightPanelW -= ImGui::GetIO().MouseDelta.x;
                    if (rightPanelW < Layout::RIGHT_PANEL_MIN) rightPanelW = Layout::RIGHT_PANEL_MIN;
                    if (rightPanelW > Layout::RIGHT_PANEL_MAX) rightPanelW = Layout::RIGHT_PANEL_MAX;
                }
            } else if (ImGui::IsItemClicked(0)) {
                draggingRightSplitter = true;
            }

            ImGui::End();
        }

        ImGui::Render();
        SDL_RenderClear(renderer);
        if (currentTheme == Theme::Light) {
            SDL_SetRenderDrawColor(renderer, 245, 245, 247, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        }
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);

        if (auto* m = selectedLeaf(); m && m->needsRerender) {
            m->needsRerender = false;
            SDL_SetRenderTarget(renderer, canvas);
            m->render(renderer, Layout::CANVAS_W, Layout::CANVAS_H);
            SDL_SetRenderTarget(renderer, nullptr);
        }
    }

    // All leaf modules are owned by `allLeaves`; delete them once at the
    // end. Group entries have no extra resources to free.
    for (IModule* m : allLeaves) delete m;
    SDL_DestroyTexture(canvas);
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
