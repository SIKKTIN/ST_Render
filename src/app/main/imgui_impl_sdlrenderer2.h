// dear imgui: Renderer Backend for SDL_Renderer for SDL2 (Requires: SDL 2.0.17+)
#pragma once
#ifndef IMGUI_DISABLE
#include "imgui.h"
#include <SDL2/SDL.h>

IMGUI_IMPL_API bool ImGui_ImplSDLRenderer2_Init(SDL_Renderer* renderer);
IMGUI_IMPL_API void ImGui_ImplSDLRenderer2_Shutdown();
IMGUI_IMPL_API void ImGui_ImplSDLRenderer2_NewFrame();
IMGUI_IMPL_API void ImGui_ImplSDLRenderer2_RenderDrawData(ImDrawData* draw_data, SDL_Renderer* renderer);
IMGUI_IMPL_API void ImGui_ImplSDLRenderer2_CreateDeviceObjects();
IMGUI_IMPL_API void ImGui_ImplSDLRenderer2_DestroyDeviceObjects();

struct ImGui_ImplSDLRenderer2_RenderState {
    SDL_Renderer* Renderer;
};

#endif // #ifndef IMGUI_DISABLE
