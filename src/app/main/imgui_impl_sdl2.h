// dear imgui: Platform Backend for SDL2
#pragma once
#include <SDL2/SDL.h>
#include "imgui.h"

#ifndef IMGUI_IMPL_SDL2_NO_SDL_EVENTS
#define SDL_MAIN_HANDLED
#endif

IMGUI_IMPL_API bool ImGui_ImplSDL2_InitForSDLRenderer(SDL_Window* window, SDL_Renderer* renderer);
IMGUI_IMPL_API void ImGui_ImplSDL2_Shutdown();
IMGUI_IMPL_API void ImGui_ImplSDL2_NewFrame();
IMGUI_IMPL_API bool ImGui_ImplSDL2_ProcessEvent(const SDL_Event* event);
