// dear imgui: Platform Backend for SDL2
#ifndef IMGUI_IMPL_SDL2_NO_SDL_EVENTS
#define SDL_MAIN_HANDLED
#endif
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include <cstring>

struct ImGui_ImplSDL2_Data {
    SDL_Window* Window = nullptr;
    SDL_Renderer* Renderer = nullptr;
};

static ImGui_ImplSDL2_Data* ImGui_ImplSDL2_GetBackendData() {
    return ImGui::GetCurrentContext() ? (ImGui_ImplSDL2_Data*)ImGui::GetIO().BackendPlatformUserData : nullptr;
}

static const char* ImGui_ImplSDL2_GetClipboardText(void*) {
    return SDL_GetClipboardText();
}

static void ImGui_ImplSDL2_SetClipboardText(void*, const char* text) {
    SDL_SetClipboardText(text);
}

bool ImGui_ImplSDL2_InitForSDLRenderer(SDL_Window* window, SDL_Renderer* renderer) {
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendPlatformUserData == nullptr && "Already initialized a platform backend!");

    ImGui_ImplSDL2_Data* bd = new ImGui_ImplSDL2_Data();
    bd->Window = window;
    bd->Renderer = renderer;
    io.BackendPlatformUserData = (void*)bd;
    io.BackendPlatformName = "imgui_impl_sdl2";
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;

    io.SetClipboardTextFn = ImGui_ImplSDL2_SetClipboardText;
    io.GetClipboardTextFn = ImGui_ImplSDL2_GetClipboardText;
    io.ClipboardUserData = nullptr;

    io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
    for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); ++i) io.MouseDown[i] = false;

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    io.DisplaySize = ImVec2((float)w, (float)h);
    io.DeltaTime = 0.016f;

    SDL_StartTextInput();

    return true;
}

void ImGui_ImplSDL2_Shutdown() {
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui_ImplSDL2_Data* bd = ImGui_ImplSDL2_GetBackendData()) {
        delete bd;
    }
    io.BackendPlatformUserData = nullptr;
    io.BackendPlatformName = nullptr;
    io.BackendFlags &= ~ImGuiBackendFlags_HasMouseCursors;
}

void ImGui_ImplSDL2_NewFrame() {
    ImGui_ImplSDL2_Data* bd = ImGui_ImplSDL2_GetBackendData();
    IM_ASSERT(bd != nullptr && "Did you call ImGui_ImplSDL2_InitForSDLRenderer()?");
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(bd->Window != nullptr);

    int w, h;
    SDL_GetWindowSize(bd->Window, &w, &h);
    io.DisplaySize = ImVec2((float)w, (float)h);
    io.DeltaTime = 0.016f; // ~60fps
}

bool ImGui_ImplSDL2_ProcessEvent(const SDL_Event* event) {
    ImGuiIO& io = ImGui::GetIO();

    switch (event->type) {
        case SDL_MOUSEMOTION: {
            int w, h;
            SDL_GetWindowSize(((ImGui_ImplSDL2_Data*)io.BackendPlatformUserData)->Window, &w, &h);
            io.MousePos.x = (float)event->motion.x;
            io.MousePos.y = (float)event->motion.y;
            return true;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            int button = event->button.button - 1;
            if (button >= 0 && button < IM_ARRAYSIZE(io.MouseDown)) {
                io.MouseDown[button] = (event->type == SDL_MOUSEBUTTONDOWN);
            }
            return true;
        }
        case SDL_MOUSEWHEEL: {
            io.MouseWheel += (float)event->wheel.y;
            return true;
        }
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            SDL_Keycode key = event->key.keysym.sym;
            SDL_Scancode sc = event->key.keysym.scancode;
            ImGuiKey imgui_key = ImGuiKey_None;

            if (sc == SDL_SCANCODE_LSHIFT || sc == SDL_SCANCODE_RSHIFT) imgui_key = ImGuiKey_LeftShift;
            else if (sc == SDL_SCANCODE_LCTRL || sc == SDL_SCANCODE_RCTRL) imgui_key = ImGuiKey_LeftCtrl;
            else if (sc == SDL_SCANCODE_LALT || sc == SDL_SCANCODE_RALT) imgui_key = ImGuiKey_LeftAlt;
            else if (sc == SDL_SCANCODE_ESCAPE) imgui_key = ImGuiKey_Escape;
            else if (key >= SDLK_SPACE && key <= SDLK_z) {
                if (key >= 'a' && key <= 'z') imgui_key = (ImGuiKey)(ImGuiKey_A + (key - 'a'));
                else if (key >= '0' && key <= '9') imgui_key = (ImGuiKey)(ImGuiKey_0 + (key - '0'));
                else imgui_key = ImGuiKey_Space;
            }

            if (imgui_key != ImGuiKey_None) {
                if (event->type == SDL_KEYDOWN) io.AddKeyEvent(imgui_key, true);
                else if (event->type == SDL_KEYUP) io.AddKeyEvent(imgui_key, false);
            }
            return true;
        }
        case SDL_TEXTINPUT: {
            io.AddInputCharactersUTF8(event->text.text);
            return true;
        }
        default:
            return false;
    }
}
