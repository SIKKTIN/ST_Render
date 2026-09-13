#include <imgui.h>
#include <SDL2/SDL.h>
#include <iostream>
#include <vector>
#include <deque>
#include <cstring>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <iterator>
#include <cstdio>
#include <nlohmann/json.hpp>

#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "AppControlBridge.hpp"
#include "app/module/IModule.hpp"
#include "TestModule_FrameBuffer.hpp"
#include "TestModule_Rasterizer.hpp"
#include "TestModule_3DRender.hpp"
#include "TestModule_Texture.hpp"
#include "TestModule_Shader.hpp"
#include "app/module/review/TestModule_ReviewMath.hpp"
#include "app/module/review/TestModule_ReviewRasterizer.hpp"
#include "engine/editor/TextureManager.hpp"

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
    constexpr int   SPLITTER_HALF_W   = 4;          // vertical splitter strip width / 2
}

struct RenderResolution {
    const char* label;
    int width;
    int height;
};

constexpr RenderResolution kRenderResolutions[] = {
    { "640 x 480 (4:3)", 640, 480 },
    { "800 x 600 (4:3)", 800, 600 },
    { "960 x 540 (16:9)", 960, 540 },
    { "1280 x 720 (16:9)", 1280, 720 }
};

struct RenderSettings {
    int width = 640;
    int height = 480;
};

struct EditorPreferences {
    int resolutionIndex = 0;
    int theme = 0; // 0 = dark, 1 = light
    TestModule_3DRender::EditorSettings render;
    std::vector<std::string> recentScenes;
};

int main(int argc, char* argv[]) {
    std::cout << "=== ST Render - Test Manager ===" << std::endl;

    enum class Theme { Dark, Light };

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    ST::TextureManager::getInstance().scanResourceFolder();

    SDL_Window* window = SDL_CreateWindow(
        "ST Render - Test Manager",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        Layout::WINDOW_W, Layout::WINDOW_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(window, 900, 500);
    int windowWidth = Layout::WINDOW_W;
    int windowHeight = Layout::WINDOW_H;
    RenderSettings renderSettings;
    int currentResolutionIndex = 0;
    int pendingResolutionIndex = 0;
    bool settingsOpen = false;
    EditorPreferences editorPreferences;
    EditorPreferences pendingPreferences;
    const std::filesystem::path editorPreferencesPath = "Data/EditorSettings.json";
    int scenePathDialogMode = 0; // 1 = open, 2 = save as
    char scenePathBuffer[512] = {};

    // Editor preferences are intentionally separate from scene data. A
    // missing or partially invalid file falls back to safe defaults.
    try {
        std::ifstream input(editorPreferencesPath);
        if (input) {
            nlohmann::json saved;
            input >> saved;
            editorPreferences.resolutionIndex = std::clamp(
                saved.value("resolutionIndex", 0), 0,
                static_cast<int>(std::size(kRenderResolutions)) - 1);
            editorPreferences.theme = std::clamp(saved.value("theme", 0), 0, 1);
            const auto& render = saved.value("render", nlohmann::json::object());
            editorPreferences.render.supersampleEnabled = render.value("supersample", true);
            editorPreferences.render.flatShading = render.value("flatShading", false);
            editorPreferences.render.showLightGizmo = render.value("showLightGizmo", true);
            editorPreferences.render.showTransformGizmo = render.value("showTransformGizmo", true);
            editorPreferences.render.showSelectionOutline = render.value("showSelectionOutline", true);
            editorPreferences.render.cameraSpeed = std::clamp(render.value("cameraSpeed", 2.5f), 0.25f, 20.0f);
            editorPreferences.render.cameraSensitivity = std::clamp(render.value("cameraSensitivity", 0.01f), 0.001f, 0.1f);
            if (saved.contains("recentScenes") && saved["recentScenes"].is_array()) {
                for (const auto& recent : saved["recentScenes"]) {
                    if (recent.is_string()) editorPreferences.recentScenes.push_back(recent.get<std::string>());
                    if (editorPreferences.recentScenes.size() >= 8) break;
                }
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "Editor settings ignored: " << error.what() << std::endl;
        editorPreferences = EditorPreferences{};
    }
    currentResolutionIndex = editorPreferences.resolutionIndex;
    pendingResolutionIndex = currentResolutionIndex;
    renderSettings.width = kRenderResolutions[currentResolutionIndex].width;
    renderSettings.height = kRenderResolutions[currentResolutionIndex].height;

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        // Rendering work is CPU-side, so use SDL's software backend as a
        // compatibility fallback when the accelerated driver is unavailable.
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
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
        const char* category = "";
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
        new TestModule_Texture(),
        new TestModule_Shader(),
        new TestModule_ReviewMath(),
        new TestModule_ReviewRasterizer(),
    };

    // 2) Aggregate by category.  A deque keeps group addresses stable while
    // new categories are appended, so entries.parent remains valid.
    std::deque<ModuleGroup> groups;
    auto findOrCreateGroup = [&](const char* cat) -> ModuleGroup* {
        for (auto& g : groups) if (std::strcmp(g.category, cat) == 0) return &g;
        const char* displayName = std::strcmp(cat, "TestComponent") == 0
            ? "Test Components" : cat;
        groups.push_back(ModuleGroup{ {}, cat, displayName });
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
        ordered.push_back({ g.label, true, {}, { g.children, g.category, g.label }, nullptr });
        for (auto& e : entries) {
            if (e.parent == &g) ordered.push_back(e);
        }
    }
    entries = std::move(ordered);

    // Default selection: open the 3D Render module when present, even though
    // modules are grouped and their registration order may change.
    int selectedModule = -1;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (!entries[i].isGroup && std::strcmp(entries[i].leaf.mod->getName(), "3D Render") == 0) {
            selectedModule = static_cast<int>(i);
            break;
        }
    }
    if (selectedModule < 0) {
        for (size_t i = 0; i < entries.size(); ++i) {
            if (!entries[i].isGroup) {
                selectedModule = static_cast<int>(i);
                break;
            }
        }
    }
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
    std::filesystem::path currentScenePath = "Data/Scenes/last.scene.json";
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
    bool canvasMouseCaptured = false;

    SDL_Texture* canvas = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_TARGET,
        renderSettings.width, renderSettings.height
    );
    if (!canvas) {
        std::cerr << "SDL_CreateTexture(canvas) failed: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    auto runModule = [&](int index, bool rerender = true) {
        if (index < 0 || index >= (int)entries.size()) return;
        if (entries[index].isGroup) return;
        IModule* m = entries[index].leaf.mod;
        consoleOutput.clear();
        m->runConsole(consoleOutput);
        if (rerender) {
            SDL_SetRenderTarget(renderer, canvas);
            m->render(renderer, renderSettings.width, renderSettings.height);
            SDL_SetRenderTarget(renderer, nullptr);
        }
    };

    auto recreateCanvas = [&]() {
        SDL_Texture* replacement = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_TARGET,
            renderSettings.width, renderSettings.height);
        if (!replacement) {
            std::cerr << "SDL_CreateTexture(canvas) failed after resolution change: "
                      << SDL_GetError() << std::endl;
            return false;
        }
        SDL_DestroyTexture(canvas);
        canvas = replacement;
        if (auto* m = selectedLeaf()) m->needsRerender = true;
        runModule(selectedModule);
        return true;
    };
    auto saveEditorPreferences = [&]() {
        try {
            std::filesystem::create_directories(editorPreferencesPath.parent_path());
            nlohmann::json saved = {
                { "version", 1 },
                { "resolutionIndex", editorPreferences.resolutionIndex },
                { "theme", editorPreferences.theme },
                { "render", {
                    { "supersample", editorPreferences.render.supersampleEnabled },
                    { "flatShading", editorPreferences.render.flatShading },
                    { "showLightGizmo", editorPreferences.render.showLightGizmo },
                    { "showTransformGizmo", editorPreferences.render.showTransformGizmo },
                    { "showSelectionOutline", editorPreferences.render.showSelectionOutline },
                    { "cameraSpeed", editorPreferences.render.cameraSpeed },
                    { "cameraSensitivity", editorPreferences.render.cameraSensitivity }
                } },
                { "recentScenes", editorPreferences.recentScenes }
            };
            std::ofstream output(editorPreferencesPath);
            output << saved.dump(2) << '\n';
        } catch (const std::exception& error) {
            std::cerr << "Unable to save editor settings: " << error.what() << std::endl;
        }
    };
    auto rememberScenePath = [&](const std::filesystem::path& path) {
        const std::string normalized = path.lexically_normal().generic_string();
        auto& recent = editorPreferences.recentScenes;
        recent.erase(std::remove(recent.begin(), recent.end(), normalized), recent.end());
        recent.insert(recent.begin(), normalized);
        if (recent.size() > 8) recent.resize(8);
        saveEditorPreferences();
    };
    if (auto* render3D = dynamic_cast<TestModule_3DRender*>(selectedLeaf())) {
        render3D->applyEditorSettings(editorPreferences.render);
    }
    runModule(selectedModule);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    auto applyTheme = [](Theme t) {
        // ImGui 1.91+ removed the public InputTextBlinkTime knob, so we
        // can't slow the caret.  Disable blinking entirely so the caret
        // stays solid while a text field has focus.
        ImGui::GetIO().ConfigInputTextCursorBlink = false;

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

    Theme currentTheme = editorPreferences.theme == 1 ? Theme::Light : Theme::Dark;
    int pendingTheme = editorPreferences.theme;
    applyTheme(currentTheme);

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // Hand the SDL renderer to every leaf so modules can upload textures.
    for (const auto& e : entries) {
        if (!e.isGroup) e.leaf.mod->setRenderer(renderer);
    }

    bool running = true;

    ST::AppControlBridge controlBridge;
    std::string controlBridgeError;
    if (!controlBridge.initialize(&controlBridgeError)) {
        std::cerr << "[MCP] Control bridge disabled: " << controlBridgeError << std::endl;
    }

    auto buildControlState = [&]() {
        ST::AppControlBridge::Json modules = ST::AppControlBridge::Json::array();
        for (size_t i = 0; i < entries.size(); ++i) {
            const auto& entry = entries[i];
            if (entry.isGroup) continue;
            modules.push_back({
                { "index", static_cast<int>(i) },
                { "name", entry.label },
                { "category", entry.parent ? entry.parent->label : "" },
                { "selected", selectedModule == static_cast<int>(i) },
                { "realTime", entry.leaf.mod->needsRealTimeUpdate() }
            });
        }

        bool sceneDirty = false;
        if (auto* render3D = dynamic_cast<TestModule_3DRender*>(selectedLeaf())) {
            sceneDirty = render3D->isSceneDirty();
        }
        return ST::AppControlBridge::Json{
            { "application", "ST_Render_Manager" },
            { "running", running },
            { "selectedModule", selectedLeaf() ? selectedLeaf()->getName() : "" },
            { "scene", {
                { "path", currentScenePath.string() },
                { "dirty", sceneDirty }
            } },
            { "canvas", { { "width", renderSettings.width }, { "height", renderSettings.height } } },
            { "canvasBounds", {
                { "minX", canvasMinX }, { "minY", canvasMinY },
                { "maxX", canvasMaxX }, { "maxY", canvasMaxY }
            } },
            { "consoleOutput", consoleOutput },
            { "modules", modules }
        };
    };

    auto saveCanvasBitmap = [&](const std::filesystem::path& outputPath) {
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
            0, renderSettings.width, renderSettings.height, 32, SDL_PIXELFORMAT_RGBA32);
        if (!surface) throw std::runtime_error(SDL_GetError());

        SDL_Texture* previousTarget = SDL_GetRenderTarget(renderer);
        if (SDL_SetRenderTarget(renderer, canvas) != 0) {
            SDL_FreeSurface(surface);
            throw std::runtime_error(SDL_GetError());
        }

        const int readResult = SDL_RenderReadPixels(
            renderer, nullptr, SDL_PIXELFORMAT_RGBA32, surface->pixels, surface->pitch);
        SDL_SetRenderTarget(renderer, previousTarget);
        if (readResult != 0) {
            const std::string message = SDL_GetError();
            SDL_FreeSurface(surface);
            throw std::runtime_error(message);
        }

        const int saveResult = SDL_SaveBMP(surface, outputPath.string().c_str());
        SDL_FreeSurface(surface);
        if (saveResult != 0) throw std::runtime_error(SDL_GetError());
    };

    auto handleControlCommand = [&](const ST::AppControlBridge::Json& request) {
        const std::string command = request.value("command", "");
        const auto params = request.value(
            "params", ST::AppControlBridge::Json::object());

        if (command == "list_modules" || command == "get_status") {
            return buildControlState();
        }

        if (command == "set_resolution") {
            int preset = -1;
            if (params.contains("preset") && params["preset"].is_number_integer()) {
                preset = params["preset"].get<int>();
            }
            if (preset < 0 || preset >= static_cast<int>(std::size(kRenderResolutions))) {
                throw std::runtime_error("Resolution preset was not found");
            }
            renderSettings.width = kRenderResolutions[preset].width;
            renderSettings.height = kRenderResolutions[preset].height;
            currentResolutionIndex = preset;
            pendingResolutionIndex = preset;
            editorPreferences.resolutionIndex = preset;
            if (!recreateCanvas()) throw std::runtime_error("Failed to recreate render canvas");
            saveEditorPreferences();
            controlBridge.updateState(buildControlState(), true);
            return buildControlState();
        }

        if (command == "list_shaders" || command == "select_shader") {
            auto* selected = selectedLeaf();
            auto* render3D = dynamic_cast<TestModule_3DRender*>(selected);
            if (!render3D) throw std::runtime_error("3D Render is not selected");

            if (command == "select_shader") {
                int shaderIndex = -1;
                if (params.contains("index") && params["index"].is_number_integer()) {
                    shaderIndex = params["index"].get<int>();
                }
                if (shaderIndex == -1) {
                    render3D->useBuiltinShader();
                } else if (!render3D->selectShaderIndex(shaderIndex)) {
                    throw std::runtime_error(render3D->getShaderError().empty()
                        ? "Shader index was not found or failed to load"
                        : render3D->getShaderError());
                }
                runModule(selectedModule);
            }

            ST::AppControlBridge::Json shaders = ST::AppControlBridge::Json::array();
            const auto& entries = render3D->getShaderEntries();
            for (size_t i = 0; i < entries.size(); ++i) {
                shaders.push_back({
                    { "index", static_cast<int>(i) },
                    { "name", entries[i].displayName },
                    { "path", entries[i].relativePath },
                    { "selected", static_cast<int>(i) == render3D->getSelectedShaderIndex() }
                });
            }
            return ST::AppControlBridge::Json{
                { "module", "3D Render" },
                { "selectedShader", render3D->getSelectedShaderIndex() },
                { "error", render3D->getShaderError() },
                { "shaders", shaders }
            };
        }

        if (command == "list_models" || command == "select_model") {
            auto* selected = selectedLeaf();
            auto* render3D = dynamic_cast<TestModule_3DRender*>(selected);
            if (!render3D) throw std::runtime_error("3D Render is not selected");

            if (command == "select_model") {
                int modelIndex = -1;
                if (params.contains("index") && params["index"].is_number_integer()) {
                    modelIndex = params["index"].get<int>();
                }
                if (modelIndex < 0 || !render3D->selectModelIndex(modelIndex)) {
                    throw std::runtime_error(render3D->getModelError().empty()
                        ? "Model index was not found or failed to load"
                        : render3D->getModelError());
                }
                runModule(selectedModule);
            }

            ST::AppControlBridge::Json models = ST::AppControlBridge::Json::array();
            const auto& entries = render3D->getModelEntries();
            for (size_t i = 0; i < entries.size(); ++i) {
                models.push_back({
                    { "index", static_cast<int>(i) },
                    { "name", entries[i].displayName },
                    { "path", entries[i].relativePath },
                    { "selected", static_cast<int>(i) == render3D->getSelectedModelIndex() }
                });
            }
            const auto* model = render3D->getActiveModel();
            return ST::AppControlBridge::Json{
                { "module", "3D Render" },
                { "selectedModel", render3D->getSelectedModelIndex() },
                { "error", render3D->getModelError() },
                { "textureStatus", render3D->getModelTextureStatus() },
                { "vertexCount", model ? model->getVertexCount() : 8 },
                { "triangleCount", model ? model->getTriangleCount() : 12 },
                { "partCount", model ? static_cast<int>(model->parts.size()) : 1 },
                { "materialCount", model ? static_cast<int>(model->materials.size()) : 0 },
                { "models", models }
            };
        }

        if (command == "list_textures" || command == "select_texture") {
            auto* render3D = dynamic_cast<TestModule_3DRender*>(selectedLeaf());
            if (!render3D) throw std::runtime_error("3D Render is not selected");
            if (command == "select_texture") {
                const std::string slot = params.value("slot", std::string());
                const int textureIndex = params.value("index", -1);
                if (!render3D->selectMaterialTexture(slot, textureIndex)) {
                    throw std::runtime_error("Texture slot or texture index was not found");
                }
                runModule(selectedModule);
            }
            ST::AppControlBridge::Json textures = ST::AppControlBridge::Json::array();
            const auto& entries = render3D->getTextureEntries();
            for (size_t i = 0; i < entries.size(); ++i) {
                textures.push_back({
                    { "index", static_cast<int>(i) },
                    { "name", entries[i].displayName },
                    { "path", entries[i].relativePath }
                });
            }
            ST::AppControlBridge::Json selectedTextures = {
                { "diffuse", render3D->getSelectedMaterialTexturePath("diffuse") },
                { "roughness", render3D->getSelectedMaterialTexturePath("roughness") },
                { "metallic", render3D->getSelectedMaterialTexturePath("metallic") },
                { "normal", render3D->getSelectedMaterialTexturePath("normal") }
            };
            return ST::AppControlBridge::Json{
                { "module", "3D Render" },
                { "textures", textures },
                { "selectedTextures", selectedTextures },
                { "selectedObject", render3D->getSelectedSceneObjectIndex() }
            };
        }

        if (command == "save_scene" || command == "load_scene") {
            auto* render3D = dynamic_cast<TestModule_3DRender*>(selectedLeaf());
            if (!render3D) throw std::runtime_error("3D Render is not selected");
            if (params.contains("path") && params["path"].is_string()) {
                currentScenePath = params["path"].get<std::string>();
            }
            std::string sceneError;
            const std::string scenePath = currentScenePath.string();
            const bool success = command == "save_scene"
                ? render3D->saveScene(scenePath, sceneError)
                : render3D->loadScene(scenePath, sceneError);
            if (!success) throw std::runtime_error(sceneError);
            render3D->markSceneSaved();
            rememberScenePath(currentScenePath);
            runModule(selectedModule);
            return ST::AppControlBridge::Json{
                { "module", "3D Render" },
                { "path", scenePath },
                { "selectedObject", render3D->getSelectedSceneObjectIndex() },
                { "objectCount", static_cast<int>(render3D->getSceneObjectInfos().size()) },
                { "warning", render3D->getSceneWarning() }
            };
        }

        if (command == "list_scene_objects" || command == "add_scene_object" ||
            command == "select_scene_object" || command == "duplicate_scene_object" ||
            command == "delete_scene_object" || command == "set_scene_object_transform") {
            auto* render3D = dynamic_cast<TestModule_3DRender*>(selectedLeaf());
            if (!render3D) throw std::runtime_error("3D Render is not selected");

            if (command == "add_scene_object") {
                const int modelIndex = params.value("modelIndex", -1);
                if (!render3D->addModelToScene(modelIndex)) {
                    throw std::runtime_error(render3D->getModelError().empty()
                        ? "Model index was not found or failed to load"
                        : render3D->getModelError());
                }
            } else if (command == "select_scene_object") {
                if (!render3D->selectSceneObjectIndex(params.value("index", -1))) {
                    throw std::runtime_error("Scene object index was not found");
                }
            } else if (command == "duplicate_scene_object") {
                if (!render3D->duplicateSelectedObject()) {
                    throw std::runtime_error("No scene object is selected");
                }
            } else if (command == "delete_scene_object") {
                if (!render3D->deleteSelectedObject()) {
                    throw std::runtime_error("No scene object is selected");
                }
            } else if (command == "set_scene_object_transform") {
                const int objectIndex = params.value("index", render3D->getSelectedSceneObjectIndex());
                ST::Vector3 positionValue, rotationValue, scaleValue;
                const ST::Vector3* position = nullptr;
                const ST::Vector3* rotation = nullptr;
                const ST::Vector3* scale = nullptr;
                auto parseVector = [&](const char* key, ST::Vector3& value, const ST::Vector3*& output) {
                    if (!params.contains(key)) return;
                    const auto& input = params[key];
                    if (!input.is_array() || input.size() != 3) {
                        throw std::runtime_error(std::string(key) + " must contain three numbers");
                    }
                    value = ST::Vector3(input[0].get<float>(), input[1].get<float>(), input[2].get<float>());
                    output = &value;
                };
                parseVector("position", positionValue, position);
                parseVector("rotation", rotationValue, rotation);
                parseVector("scale", scaleValue, scale);
                if (!position && !rotation && !scale) {
                    throw std::runtime_error("Provide position, rotation, or scale");
                }
                if (!render3D->setSceneObjectTransform(objectIndex, position, rotation, scale)) {
                    throw std::runtime_error("Scene object index was not found");
                }
            }

            if (command != "list_scene_objects" && command != "select_scene_object") {
                runModule(selectedModule);
            }
            ST::AppControlBridge::Json objects = ST::AppControlBridge::Json::array();
            for (const auto& object : render3D->getSceneObjectInfos()) {
                objects.push_back({
                    { "id", object.id },
                    { "name", object.name },
                    { "modelIndex", object.modelIndex },
                    { "modelPath", object.modelPath },
                    { "position", { object.position.x, object.position.y, object.position.z } },
                    { "rotation", { object.rotation.x, object.rotation.y, object.rotation.z } },
                    { "scale", { object.scale.x, object.scale.y, object.scale.z } },
                    { "visible", object.visible },
                    { "selected", object.selected }
                });
            }
            return ST::AppControlBridge::Json{
                { "module", "3D Render" },
                { "selectedObject", render3D->getSelectedSceneObjectIndex() },
                { "objects", objects }
            };
        }

        if (command == "get_light" || command == "set_light") {
            auto* selected = selectedLeaf();
            auto* render3D = dynamic_cast<TestModule_3DRender*>(selected);
            if (!render3D) throw std::runtime_error("3D Render is not selected");

            if (command == "set_light") {
                if (params.contains("direction") && params["direction"].is_array() &&
                    params["direction"].size() == 3) {
                    const ST::Vector3 direction(
                        params["direction"][0].get<float>(),
                        params["direction"][1].get<float>(),
                        params["direction"][2].get<float>());
                    if (!render3D->setLightDirection(direction)) {
                        throw std::runtime_error("Light direction must be non-zero");
                    }
                }
                if (params.contains("intensity") && params["intensity"].is_number()) {
                    render3D->setLightIntensity(params["intensity"].get<float>());
                }
                runModule(selectedModule);
            }

            const ST::Light& light = render3D->getLight();
            return ST::AppControlBridge::Json{
                { "module", "3D Render" },
                { "enabled", true },
                { "direction", { light.direction.x, light.direction.y, light.direction.z } },
                { "color", { light.color.r, light.color.g, light.color.b, light.color.a } },
                { "intensity", light.intensity }
            };
        }

        if (command == "select_module") {
            int match = -1;
            if (params.contains("index") && params["index"].is_number_integer()) {
                const int candidate = params["index"].get<int>();
                if (candidate >= 0 && candidate < static_cast<int>(entries.size()) &&
                    !entries[candidate].isGroup) {
                    match = candidate;
                }
            } else if (params.contains("name") && params["name"].is_string()) {
                std::string wanted = params["name"].get<std::string>();
                for (char& c : wanted) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                for (size_t i = 0; i < entries.size(); ++i) {
                    if (entries[i].isGroup) continue;
                    std::string candidate = entries[i].label;
                    for (char& c : candidate) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (candidate == wanted) {
                        match = static_cast<int>(i);
                        break;
                    }
                }
            }

            if (match < 0) throw std::runtime_error("Module was not found");
            selectedModule = match;
            runModule(selectedModule);
            controlBridge.updateState(buildControlState(), true);
            return buildControlState();
        }

        if (command == "rerender") {
            runModule(selectedModule);
            controlBridge.updateState(buildControlState(), true);
            return buildControlState();
        }

        if (command == "get_console_output") {
            runModule(selectedModule, false);
            return ST::AppControlBridge::Json{
                { "module", selectedLeaf() ? selectedLeaf()->getName() : "" },
                { "output", consoleOutput }
            };
        }

        if (command == "capture_canvas") {
            runModule(selectedModule);
            std::string requestId = request.value("id", "capture");
            for (char& c : requestId) {
                const unsigned char value = static_cast<unsigned char>(c);
                if (!std::isalnum(value) && c != '-' && c != '_') c = '_';
            }
            const auto outputPath = controlBridge.capturesDirectory() /
                ("canvas-" + requestId + ".bmp");
            saveCanvasBitmap(outputPath);
            return ST::AppControlBridge::Json{
                { "module", selectedLeaf() ? selectedLeaf()->getName() : "" },
                { "path", outputPath.string() },
                { "mimeType", "image/bmp" },
                { "width", renderSettings.width },
                { "height", renderSettings.height }
            };
        }

        if (command == "shutdown") {
            running = false;
            return ST::AppControlBridge::Json{ { "accepted", true } };
        }

        throw std::runtime_error("Unknown control command: " + command);
    };

    controlBridge.updateState(buildControlState(), true);

    auto saveSceneAt = [&](const std::filesystem::path& path) {
        auto* render3D = dynamic_cast<TestModule_3DRender*>(selectedLeaf());
        if (!render3D) return false;
        std::string sceneError;
        const bool saved = render3D->saveScene(path.string(), sceneError);
        consoleOutput = saved ? "Saved scene: " + path.string()
                              : "Save scene failed: " + sceneError;
        if (saved) {
            currentScenePath = path;
            render3D->markSceneSaved();
            rememberScenePath(currentScenePath);
        }
        return saved;
    };
    auto loadSceneAt = [&](const std::filesystem::path& path) {
        auto* render3D = dynamic_cast<TestModule_3DRender*>(selectedLeaf());
        if (!render3D) return false;
        std::string sceneError;
        const bool loaded = render3D->loadScene(path.string(), sceneError);
        consoleOutput = loaded ? "Loaded scene: " + path.string()
                               : "Load scene failed: " + sceneError;
        if (loaded) {
            currentScenePath = path;
            render3D->markSceneSaved();
            rememberScenePath(currentScenePath);
            runModule(selectedModule);
        }
        return loaded;
    };
    auto saveCurrentScene = [&]() { return saveSceneAt(currentScenePath); };
    auto loadCurrentScene = [&]() { return loadSceneAt(currentScenePath); };

    while (running) {
        SDL_GetWindowSize(window, &windowWidth, &windowHeight);
        std::string windowTitle = "ST Render - Test Manager";
        if (auto* render3D = dynamic_cast<TestModule_3DRender*>(selectedLeaf())) {
            windowTitle += " - " + currentScenePath.filename().string();
            if (render3D->isSceneDirty()) windowTitle += " *";
        }
        SDL_SetWindowTitle(window, windowTitle.c_str());
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_WINDOWEVENT &&
                (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                 event.window.event == SDL_WINDOWEVENT_RESIZED ||
                 event.window.event == SDL_WINDOWEVENT_MAXIMIZED ||
                 event.window.event == SDL_WINDOWEVENT_RESTORED)) {
                // SDL/window backends can invalidate or clear render-target
                // contents during a resize/maximize. The canvas has a fixed
                // logical resolution, so simply rerender it for the new
                // presentation surface instead of changing scene resolution.
                if (auto* m = selectedLeaf()) m->needsRerender = true;
            }
            if (event.type == SDL_KEYDOWN) {
                if ((event.key.keysym.mod & KMOD_CTRL) && (event.key.keysym.mod & KMOD_SHIFT) &&
                    event.key.keysym.sym == SDLK_s) {
                    scenePathDialogMode = 2;
                    std::strncpy(scenePathBuffer, currentScenePath.string().c_str(), sizeof(scenePathBuffer) - 1);
                    scenePathBuffer[sizeof(scenePathBuffer) - 1] = '\0';
                    ImGui::OpenPopup("Scene Path");
                } else if ((event.key.keysym.mod & KMOD_CTRL) && event.key.keysym.sym == SDLK_s) {
                    saveCurrentScene();
                } else if ((event.key.keysym.mod & KMOD_CTRL) && event.key.keysym.sym == SDLK_o) {
                    loadCurrentScene();
                }
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
            const bool routeCanvasMotion = inCanvas || canvasMouseCaptured;
            const bool routeCanvasButton = inCanvas || canvasMouseCaptured;
            if (routeCanvasMotion || routeCanvasButton) {
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
                        cx = std::clamp(cx * renderSettings.width / screenW, 0, renderSettings.width - 1);
                        cy = std::clamp(cy * renderSettings.height / screenH, 0, renderSettings.height - 1);
                    }
                    mod->onCanvasMouseMove(cx, cy);
                    canvasHandled = true;
                } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                    int screenW = canvasMaxX - canvasMinX;
                    int screenH = canvasMaxY - canvasMinY;
                    if (screenW > 0 && screenH > 0) {
                        cx = std::clamp(cx * renderSettings.width / screenW, 0, renderSettings.width - 1);
                        cy = std::clamp(cy * renderSettings.height / screenH, 0, renderSettings.height - 1);
                    }
                    mod->onCanvasMouseDown(event.button.button, cx, cy);
                    if (inCanvas && (event.button.button == SDL_BUTTON_LEFT ||
                                     event.button.button == SDL_BUTTON_RIGHT)) {
                        canvasMouseCaptured = true;
                    }
                    canvasHandled = true;
                } else if (event.type == SDL_MOUSEBUTTONUP) {
                    int screenW = canvasMaxX - canvasMinX;
                    int screenH = canvasMaxY - canvasMinY;
                    if (screenW > 0 && screenH > 0) {
                        cx = std::clamp(cx * renderSettings.width / screenW, 0, renderSettings.width - 1);
                        cy = std::clamp(cy * renderSettings.height / screenH, 0, renderSettings.height - 1);
                    }
                    mod->onCanvasMouseUp(event.button.button, cx, cy);
                    if (event.button.button == SDL_BUTTON_LEFT ||
                        event.button.button == SDL_BUTTON_RIGHT) {
                        canvasMouseCaptured = false;
                    }
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
                    int cx = (wx - canvasMinX) * renderSettings.width / screenW;
                    int cy = (wy - canvasMinY) * renderSettings.height / screenH;
                    entries[selectedModule].leaf.mod->onWheel(
                        (float)event.wheel.x, (float)event.wheel.y,
                        cx, cy, renderSettings.width, renderSettings.height
                    );
                }
            }
        }

        controlBridge.poll(handleControlCommand);
        controlBridge.updateState(buildControlState());

        if (auto* m = selectedLeaf(); m && m->needsRealTimeUpdate()) {
            Uint64 now = SDL_GetPerformanceCounter();
            static Uint64 last = now;
            float dt = (float)(now - last) / SDL_GetPerformanceFrequency();
            last = now;
            m->update(dt);
            if (m->needsRerender) {
                m->needsRerender = false;
                SDL_SetRenderTarget(renderer, canvas);
                m->render(renderer, renderSettings.width, renderSettings.height);
                SDL_SetRenderTarget(renderer, nullptr);
                runModule(selectedModule, false);
            }
        }

        ImGui_ImplSDL2_NewFrame();
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui::NewFrame();

        // Top menu bar (File / Edit / View / Help). Sits above all panels;
        // all panels below are pushed down by the *actual* menu bar height,
        // queried from ImGui after rendering, so they sit flush against the
        // menu bar (no gap) and stay correct if the theme changes the font
        // size or frame padding.
        float menuBarH = Layout::MENU_BAR_H; // refined to true height after EndMainMenuBar()
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
                    scenePathDialogMode = 1;
                    std::strncpy(scenePathBuffer, currentScenePath.string().c_str(), sizeof(scenePathBuffer) - 1);
                    scenePathBuffer[sizeof(scenePathBuffer) - 1] = '\0';
                    ImGui::OpenPopup("Scene Path");
                }
                if (ImGui::MenuItem("Save Scene", "Ctrl+S")) saveCurrentScene();
                if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) {
                    scenePathDialogMode = 2;
                    std::strncpy(scenePathBuffer, currentScenePath.string().c_str(), sizeof(scenePathBuffer) - 1);
                    scenePathBuffer[sizeof(scenePathBuffer) - 1] = '\0';
                    ImGui::OpenPopup("Scene Path");
                }
                if (ImGui::BeginMenu("Recent Scenes")) {
                    if (editorPreferences.recentScenes.empty()) {
                        ImGui::TextDisabled("No recent scenes");
                    } else {
                        for (const auto& recent : editorPreferences.recentScenes) {
                            if (ImGui::MenuItem(recent.c_str())) loadSceneAt(recent);
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::TextDisabled("Current: %s", currentScenePath.string().c_str());
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) { running = false; }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Settings...")) {
                    pendingResolutionIndex = currentResolutionIndex;
                    pendingPreferences = editorPreferences;
                    if (auto* render3D = dynamic_cast<TestModule_3DRender*>(selectedLeaf())) {
                        pendingPreferences.render = render3D->getEditorSettings();
                    }
                    pendingTheme = currentTheme == Theme::Light ? 1 : 0;
                    settingsOpen = true;
                }
                ImGui::Separator();
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

        if (settingsOpen) {
            ImGui::SetNextWindowSize(ImVec2(560.0f, 430.0f), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Settings", &settingsOpen)) {
                if (ImGui::BeginTabBar("SettingsTabs")) {
                    if (ImGui::BeginTabItem("Render")) {
                        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Render Target");
                        ImGui::Separator();
                        const char* resolutionLabels[] = {
                            kRenderResolutions[0].label,
                            kRenderResolutions[1].label,
                            kRenderResolutions[2].label,
                            kRenderResolutions[3].label
                        };
                        ImGui::Combo("Resolution", &pendingPreferences.resolutionIndex,
                                     resolutionLabels, IM_ARRAYSIZE(resolutionLabels));
                        ImGui::TextDisabled("Canvas keeps this aspect ratio while panels resize.");
                        ImGui::Checkbox("2x final supersampling", &pendingPreferences.render.supersampleEnabled);
                        ImGui::Checkbox("Flat shading", &pendingPreferences.render.flatShading);
                        ImGui::Checkbox("Show light gizmo", &pendingPreferences.render.showLightGizmo);
                        ImGui::Checkbox("Show transform gizmo", &pendingPreferences.render.showTransformGizmo);
                        ImGui::Checkbox("Show selection outline", &pendingPreferences.render.showSelectionOutline);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Interaction")) {
                        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Camera Input");
                        ImGui::Separator();
                        ImGui::SliderFloat("Camera speed", &pendingPreferences.render.cameraSpeed,
                                           0.25f, 20.0f, "%.2f u/s");
                        ImGui::SliderFloat("Look sensitivity", &pendingPreferences.render.cameraSensitivity,
                                           0.001f, 0.1f, "%.3f");
                        ImGui::TextDisabled("W/E/R switches transform tools; F focuses the selected object.");
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Appearance")) {
                        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Editor Theme");
                        ImGui::Separator();
                        ImGui::RadioButton("Dark", &pendingTheme, 0);
                        ImGui::SameLine();
                        ImGui::RadioButton("Light", &pendingTheme, 1);
                        ImGui::TextDisabled("Theme changes are applied when you press Apply.");
                        ImGui::EndTabItem();
                    }
                    ImGui::EndTabBar();
                }
                ImGui::Separator();
                if (ImGui::Button("Apply")) {
                    const EditorPreferences oldPreferences = editorPreferences;
                    const Theme oldTheme = currentTheme;
                    const int oldWidth = renderSettings.width;
                    const int oldHeight = renderSettings.height;
                    editorPreferences = pendingPreferences;
                    currentTheme = pendingTheme == 1 ? Theme::Light : Theme::Dark;
                    applyTheme(currentTheme);
                    if (auto* render3D = dynamic_cast<TestModule_3DRender*>(selectedLeaf())) {
                        render3D->applyEditorSettings(editorPreferences.render);
                    }
                    const auto& choice = kRenderResolutions[editorPreferences.resolutionIndex];
                    renderSettings.width = choice.width;
                    renderSettings.height = choice.height;
                    if (recreateCanvas()) {
                        currentResolutionIndex = editorPreferences.resolutionIndex;
                        pendingResolutionIndex = currentResolutionIndex;
                        saveEditorPreferences();
                        settingsOpen = false;
                    } else {
                        editorPreferences = oldPreferences;
                        currentTheme = oldTheme;
                        applyTheme(currentTheme);
                        if (auto* render3D = dynamic_cast<TestModule_3DRender*>(selectedLeaf())) {
                            render3D->applyEditorSettings(editorPreferences.render);
                        }
                        renderSettings.width = oldWidth;
                        renderSettings.height = oldHeight;
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Reset defaults")) {
                    pendingPreferences = EditorPreferences{};
                    pendingTheme = 0;
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) settingsOpen = false;
            }
            ImGui::End();
        }

        if (ImGui::BeginPopupModal("Scene Path", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted(scenePathDialogMode == 2 ? "Save scene as" : "Open scene");
            ImGui::InputText("Path", scenePathBuffer, sizeof(scenePathBuffer));
            const bool accepted = ImGui::Button(scenePathDialogMode == 2 ? "Save" : "Open");
            ImGui::SameLine();
            const bool cancelled = ImGui::Button("Cancel");
            if (accepted) {
                const bool success = scenePathDialogMode == 2
                    ? saveSceneAt(std::filesystem::path(scenePathBuffer))
                    : loadSceneAt(std::filesystem::path(scenePathBuffer));
                if (success) {
                    scenePathDialogMode = 0;
                    ImGui::CloseCurrentPopup();
                }
            } else if (cancelled) {
                scenePathDialogMode = 0;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        const float contentH = std::max(100.0f, static_cast<float>(windowHeight) - menuBarH);
        const float contentW = std::max(100.0f, static_cast<float>(windowWidth));

        // Left panel - Test list
        {
            ImGui::SetNextWindowPos(ImVec2(0, menuBarH));
            ImGui::SetNextWindowSize(ImVec2(leftPanelW, contentH));
            ImGui::Begin("Tests", nullptr,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse);
            ImGui::Text("ST Render");
            ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "Test Manager");
            ImGui::Separator();

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 3.0f));
            ImGui::BeginChild("ModuleList", ImVec2(0, 0), false);

            // Render only top-level entries. Group children are rendered inside
            // their TreeNode, with full-width selectable rows.
            for (size_t i = 0; i < entries.size(); ++i) {
                const auto& e = entries[i];
                if (e.parent != nullptr) continue;
                if (!e.isGroup) {
                    const float rowWidth = ImGui::GetContentRegionAvail().x;
                    if (ImGui::Selectable(e.label, selectedModule == static_cast<int>(i),
                                          0, ImVec2(rowWidth, 0))) {
                        if (selectedModule != static_cast<int>(i)) {
                            selectedModule = static_cast<int>(i);
                            runModule(selectedModule);
                        }
                    }
                    continue;
                }

                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                           ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                           ImGuiTreeNodeFlags_SpanFullWidth;
                if (std::strcmp(e.label, "Test Components") == 0) {
                    flags |= ImGuiTreeNodeFlags_DefaultOpen;
                }

                const bool open = ImGui::TreeNodeEx(
                    reinterpret_cast<void*>(static_cast<intptr_t>(i)),
                    flags, "%s (%zu)", e.label, e.group.children.size());

                if (open) {
                    for (IModule* child : e.group.children) {
                        for (size_t j = 0; j < entries.size(); ++j) {
                            const auto& childEntry = entries[j];
                            if (childEntry.isGroup || childEntry.leaf.mod != child) continue;
                            const float rowWidth = ImGui::GetContentRegionAvail().x;
                            if (ImGui::Selectable(childEntry.label,
                                                  selectedModule == static_cast<int>(j),
                                                  0, ImVec2(rowWidth, 0))) {
                                if (selectedModule != static_cast<int>(j)) {
                                    selectedModule = static_cast<int>(j);
                                    runModule(selectedModule);
                                }
                            }
                            break;
                        }
                    }
                    ImGui::TreePop();
                }
            }

            ImGui::EndChild();
            ImGui::PopStyleVar();

            ImGui::End();
        }

        // Right panel - Controls
        {
            ImGui::SetNextWindowPos(ImVec2(contentW - rightPanelW, menuBarH));
            ImGui::SetNextWindowSize(ImVec2(rightPanelW, contentH));
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
            ImGui::SetNextWindowSize(ImVec2(createObjPanelW, contentH));
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
            float outputW = std::max(100.0f, contentW - leftPanelW - createObjPanelW - rightPanelW);
            ImGui::SetNextWindowPos(ImVec2(leftPanelW + createObjPanelW, menuBarH));
            ImGui::SetNextWindowSize(ImVec2(outputW, contentH));
            ImGui::Begin("Output", nullptr,
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse);

            if (selectedModule >= 0 && selectedModule < (int)entries.size()) {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%s",
                                   selectedLeaf() ? selectedLeaf()->getName() : entries[selectedModule].label);
                if (auto* render3D = dynamic_cast<TestModule_3DRender*>(selectedLeaf())) {
                    char renderStatus[96]{};
                    std::snprintf(renderStatus, sizeof(renderStatus),
                                  "%.2f ms  %.1f FPS%s",
                                  render3D->getFrameTimeMs(), render3D->getFps(),
                                  render3D->isInteractionActive() ? "  [interactive]" : "");
                    const float statusWidth = ImGui::CalcTextSize(renderStatus).x;
                    const float rightEdge = ImGui::GetWindowContentRegionMax().x;
                    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), rightEdge - statusWidth));
                    ImGui::TextDisabled("%s", renderStatus);
                }
                ImGui::Separator();

                float splitterH = 8.0f;
                float minCanvasH = 100.0f;
                float minConsoleH = 50.0f;
                float availH = ImGui::GetContentRegionAvail().y - splitterH;

                if (consoleHeight < minConsoleH) consoleHeight = minConsoleH;
                if (consoleHeight > availH - minCanvasH) consoleHeight = availH - minCanvasH;

                float thisCanvasH = availH - consoleHeight;

                // The software render target is deliberately fixed at 640x480
                // (4:3). Fit it inside the resizable Output panel without
                // stretching the model when side panels are dragged.
                const float renderAspect = static_cast<float>(renderSettings.width) /
                                           static_cast<float>(renderSettings.height);
                const float availableW = ImGui::GetContentRegionAvail().x;
                float imageW = availableW;
                float imageH = imageW / renderAspect;
                if (imageH > thisCanvasH) {
                    imageH = thisCanvasH;
                    imageW = imageH * renderAspect;
                }
                const float padX = std::max(0.0f, (availableW - imageW) * 0.5f);
                const float padY = std::max(0.0f, (thisCanvasH - imageH) * 0.5f);
                ImGui::Dummy(ImVec2(0.0f, padY));
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + padX);
                ImGui::Image((void*)(intptr_t)canvas, ImVec2(imageW, imageH));

                // Record canvas screen bounds for event routing
                // immediately after Image: a following Dummy would replace
                // ImGui's last-item rectangle with a zero-width spacer.
                ImVec2 cMin = ImGui::GetItemRectMin();
                ImVec2 cMax = ImGui::GetItemRectMax();
                canvasMinX = (int)cMin.x;
                canvasMinY = (int)cMin.y;
                canvasMaxX = (int)cMax.x;
                canvasMaxY = (int)cMax.y;
                ImGui::Dummy(ImVec2(0.0f, padY));
                if (auto* m = selectedLeaf()) {
                    m->renderUIOverlay(
                        canvasMinX, canvasMinY, (int)imageW, (int)imageH);
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
                // Selection-friendly read-only viewer: wrap consoleOutput in
                // an InputTextMultiline so the user can drag-select and
                // Ctrl+C.  The buffer is fixed-size; if the console text
                // exceeds the cap, only the leading bytes are shown (rare
                // in practice -- none of today's modules come close).
                {
                    constexpr size_t kConsoleBufCap = 64 * 1024;
                    static std::vector<char> consoleBuf;
                    if (consoleBuf.size() != kConsoleBufCap + 1) {
                        consoleBuf.assign(kConsoleBufCap + 1, '\0');
                    }
                    size_t copyLen = std::min(consoleOutput.size(), kConsoleBufCap);
                    if (copyLen > 0) {
                        std::memcpy(consoleBuf.data(), consoleOutput.data(), copyLen);
                    }
                    consoleBuf[copyLen] = '\0';
                    // If truncated, the user sees a visible marker so they
                    // know the displayed text isn't the full thing.
                    if (consoleOutput.size() > kConsoleBufCap) {
                        static const char truncated[] =
                            "\n\n[console output truncated for display; "
                            "use the Copy button below to get the full text]";
                        size_t tlen = sizeof(truncated) - 1;
                        if (copyLen + tlen <= kConsoleBufCap) {
                            std::memcpy(consoleBuf.data() + copyLen, truncated, tlen);
                            consoleBuf[copyLen + tlen] = '\0';
                        }
                    }

                    ImGui::InputTextMultiline(
                        "##ConsoleReadOnly",
                        consoleBuf.data(),
                        consoleBuf.size(),
                        ImVec2(-1, -1),
                        ImGuiInputTextFlags_ReadOnly);

                    if (ImGui::Button("Copy console output")) {
                        ImGui::SetClipboardText(consoleOutput.c_str());
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("(or drag-select + Ctrl+C in the box above)");
                }
                ImGui::EndChild();
            }

            ImGui::End();
        }

        // Vertical splitter (on top of everything)
        {
            ImGui::SetNextWindowPos(ImVec2(leftPanelW + createObjPanelW - Layout::SPLITTER_HALF_W, menuBarH));
            ImGui::SetNextWindowSize(ImVec2(2 * Layout::SPLITTER_HALF_W, contentH));
            ImGui::Begin("##VSplitter", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoBackground);

            ImGui::InvisibleButton("##VSplitterBtn", ImVec2(2.0f * Layout::SPLITTER_HALF_W, contentH));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 wp = ImGui::GetWindowPos();
            ImU32 col = IM_COL32(55, 55, 55, 255);
            if (ImGui::IsItemHovered() || draggingCreateSplitter) col = IM_COL32(80, 130, 255, 255);
            dl->AddRectFilled(ImVec2(wp.x, wp.y), ImVec2(wp.x + 2.0f * Layout::SPLITTER_HALF_W, wp.y + contentH), col);

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
            float handleX = contentW - rightPanelW - Layout::SPLITTER_HALF_W;
            ImGui::SetNextWindowPos(ImVec2(handleX, menuBarH));
            ImGui::SetNextWindowSize(ImVec2(2 * Layout::SPLITTER_HALF_W, contentH));
            ImGui::Begin("##RightVSplitter", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoBackground);

            ImGui::InvisibleButton("##RightVSplitterBtn", ImVec2(2.0f * Layout::SPLITTER_HALF_W, contentH));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 wp = ImGui::GetWindowPos();
            ImU32 col = IM_COL32(55, 55, 55, 255);
            if (ImGui::IsItemHovered() || draggingRightSplitter) col = IM_COL32(80, 130, 255, 255);
            dl->AddRectFilled(ImVec2(wp.x, wp.y), ImVec2(wp.x + 2.0f * Layout::SPLITTER_HALF_W, wp.y + contentH), col);

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
            m->render(renderer, renderSettings.width, renderSettings.height);
            SDL_SetRenderTarget(renderer, nullptr);
        }
    }

    // All leaf modules are owned by `allLeaves`; delete them once at the
    // end. Group entries have no extra resources to free.
    controlBridge.shutdown();
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
