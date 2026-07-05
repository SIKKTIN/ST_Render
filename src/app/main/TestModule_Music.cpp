#include "TestModule_Music.hpp"
#include "modules/music/MusicManager.hpp"
#include <imgui.h>
#include <SDL2/SDL.h>
#include <vector>
#include <string>
#include <cstdio>

TestModule_Music::TestModule_Music()
    : m_musicLoaded(false), m_playing(false), m_paused(false) {
}

TestModule_Music::~TestModule_Music() {
}

void TestModule_Music::render(void* canvasTexture, int canvasW, int canvasH) {
    SDL_Renderer* renderer = (SDL_Renderer*)canvasTexture;
    SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
    SDL_RenderClear(renderer);
}

bool TestModule_Music::renderControls() {
    ImGui::SetNextWindowSize(ImVec2(320, 180));
    ImGui::Begin("Music Player", nullptr, ImGuiWindowFlags_NoResize);

    ImGui::Text("ST Render - Music Test");
    ImGui::Separator();

    ImGui::Spacing();

    if (ImGui::Button("Play", ImVec2(120, 36))) {
        auto& mgr = ST::MusicManager::get();
        if (!m_musicLoaded) {
            std::string path = "Data/Audio/mario.mp3";
            m_musicLoaded = mgr.loadMusic(path);
        }
        if (m_musicLoaded) {
            if (m_paused) {
                mgr.resume();
            } else {
                mgr.play();
            }
            m_playing = true;
            m_paused = false;
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Pause", ImVec2(120, 36))) {
        if (m_playing && !m_paused) {
            ST::MusicManager::get().pause();
            m_paused = true;
        }
    }

    ImGui::Spacing();

    if (m_musicLoaded) {
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f),
            "Status: %s", m_playing ? (m_paused ? "Paused" : "Playing") : "Stopped");
    } else {
        ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f),
            "Status: Not loaded");
    }

    ImGui::End();
    return true;
}

void TestModule_Music::runConsole(std::string& output) {
    if (m_playing && !m_paused) {
        output = "[Music] Playing...\n";
    } else if (m_paused) {
        output = "[Music] Paused.\n";
    } else {
        output = "[Music] Ready. Click Play to start.\n";
    }
}
