#pragma once

#include "TransformUI_ImGui.hpp"
#include "engine/editor/Sprite2D.hpp"
#include "engine/editor/MusicPlayer.hpp"
#include "modules/music/MusicManager.hpp"
#include "engine/editor/TextureManager.hpp"
#include "engine/editor/AudioManager.hpp"
#include "engine/editor/Script.hpp"
#include <SDL2/SDL.h>
#include <functional>
#include <vector>
#include <string>

namespace ST {

class ScriptUI;

class Sprite2DUI : public TransformUI_ImGui {
public:
    using TextureCallback = std::function<void(int idx)>;
    using AudioCallback = std::function<void(int idx)>;

    void setTextureCallback(TextureCallback cb) { m_onTextureChanged = std::move(cb); }
    void setAudioCallback(AudioCallback cb) { m_onAudioChanged = std::move(cb); }
    void setThumbnails(SDL_Renderer* renderer, const std::vector<SDL_Texture*>& thumbs) {
        m_renderer = renderer;
        m_thumbnails = &thumbs;
    }

    bool renderControls(GameObject* obj) override {
        if (!obj) return false;

        bool changed = TransformUI_ImGui::renderControls(obj);

        if (auto* sprite = dynamic_cast<Sprite2D*>(obj)) {
            changed = renderSpriteControls(sprite) || changed;
        } else if (auto* mp = dynamic_cast<MusicPlayer*>(obj)) {
            changed = renderMusicPlayerControls(mp) || changed;
        }

        changed = renderScriptSection(obj) || changed;

        return changed;
    }

    void renderAudioBrowserOnTop() {
        if (!m_audioBrowserOpen || !m_browserPlayer) return;

        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f - 240.0f,
                                       io.DisplaySize.y * 0.5f - 200.0f), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(480, 380), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowFocus();
        ImGui::SetNextWindowBgAlpha(0.95f);

        if (ImGui::Begin("Audio Browser", &m_audioBrowserOpen,
                         ImGuiWindowFlags_NoCollapse)) {
            ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "Data/Audio/");
            ImGui::Separator();

            int audioCount = AudioManager::getInstance().getAudioCount();
            if (audioCount == 0) {
                ImGui::TextDisabled("No audio files in Data/Audio/");
            } else {
                const auto& audios = AudioManager::getInstance().getAllAudios();
                ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
                if (ImGui::BeginChild("audio_list", ImVec2(0, 300), true)) {
                    for (int i = 0; i < audioCount; i++) {
                        bool isSelected = (i == m_browserPlayer->getAudioIndex());
                        if (ImGui::Selectable(audios[i].filename.c_str(), isSelected, ImGuiSelectableFlags_None, ImVec2(0, 30))) {
                            m_browserPlayer->setAudioIndex(i);
                            m_browserPlayer->setAudioPath(audios[i].filename);
                            if (m_onAudioChanged) m_onAudioChanged(i);
                            m_audioBrowserOpen = false;
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();
                            ImGui::Text("%s", audios[i].fullPath.c_str());
                            ImGui::EndTooltip();
                        }
                    }
                    ImGui::EndChild();
                }
                ImGui::PopStyleVar();
            }

            ImGui::Separator();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_audioBrowserOpen = false;
            }
            ImGui::End();
        }
    }

    void renderBrowserOnTop() {
        if (!m_browserOpen || !m_browserSprite) return;

        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f - m_browserSize.x * 0.5f,
                                       io.DisplaySize.y * 0.5f - m_browserSize.y * 0.5f), ImGuiCond_Once);
        ImGui::SetNextWindowSize(m_browserSize);
        ImGui::SetNextWindowFocus();

        ImGui::SetNextWindowBgAlpha(0.95f);
        if (ImGui::Begin("Texture Browser", &m_browserOpen,
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "Resource Textures");
            ImGui::Separator();

            int texCount = TextureManager::getInstance().getTextureCount();
            if (texCount == 0) {
                ImGui::TextDisabled("No textures found in Resource/");
            } else {
                const auto& allTex = TextureManager::getInstance().getAllTextures();
                const int thumbSize = 96;
                const int cols = 5;

                for (int i = 0; i < (int)allTex.size(); i++) {
                    if (i % cols != 0) ImGui::SameLine();

                    bool isSelected = (i == m_browserSprite->getTextureIndex());
                    std::string label = "##tex_" + std::to_string(i);
                    ImVec4 tint(isSelected ? 0.3f : 1.0f, isSelected ? 0.7f : 1.0f,
                               isSelected ? 1.0f : 1.0f, 1.0f);

                    bool clicked = false;
                    if (m_thumbnails && m_thumbnails->size() > (size_t)i && (*m_thumbnails)[i]) {
                        clicked = ImGui::ImageButton(label.c_str(),
                            (ImTextureID)(intptr_t)(*m_thumbnails)[i],
                            ImVec2((float)thumbSize, (float)thumbSize),
                            ImVec2(0, 0), ImVec2(1, 1),
                            ImVec4(0, 0, 0, 1), tint);
                    } else {
                        clicked = ImGui::Button(label.c_str(), ImVec2((float)thumbSize, (float)thumbSize));
                    }

                    if (clicked) {
                        m_browserSprite->setTextureIndex(i);
                        if (m_onTextureChanged) m_onTextureChanged(i);
                        m_browserOpen = false;
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("%s", allTex[i].filename.c_str());
                        ImGui::EndTooltip();
                    }
                }
            }

            ImGui::Separator();
            if (ImGui::Button("Close", ImVec2(120, 0))) {
                m_browserOpen = false;
            }
            ImGui::End();
        }
    }

    bool renderMusicPlayerControls(MusicPlayer* player) {
        bool changed = false;

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "MusicPlayer");

        ImGui::Text("Audio: %s",
            player->getAudioIndex() >= 0
                ? AudioManager::getInstance().getAudioName(player->getAudioIndex()).c_str()
                : "(none)");
        if (ImGui::Button("Browse Audio...")) {
            m_browserPlayer = player;
            m_audioBrowserOpen = true;
        }

        ImGui::Spacing();

        const char* playLabel = ST::MusicManager::get().isPlaying() ? "Stop" : "Play";
        if (ImGui::Button(playLabel, ImVec2(100, 30))) {
            auto& mgr = ST::MusicManager::get();
            if (mgr.isPlaying()) {
                mgr.stop();
            } else {
                if (player->getAudioIndex() >= 0) {
                    std::string fullPath = AudioManager::getInstance().getAudioFullPath(player->getAudioIndex());
                    if (mgr.loadMusic(fullPath)) {
                        mgr.setVolume(player->getVolume());
                        mgr.play();
                    }
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Pause", ImVec2(100, 30))) {
            auto& mgr = ST::MusicManager::get();
            if (mgr.isPaused()) {
                mgr.resume();
            } else if (mgr.isPlaying()) {
                mgr.pause();
            }
        }

        ImGui::Spacing();
        ImGui::Text("Volume");
        int vol = player->getVolume();
        if (ImGui::SliderInt("##vol", &vol, 0, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp)) {
            player->setVolume(vol);
            ST::MusicManager::get().setVolume(vol);
            changed = true;
        }

        bool loop = player->getLoop();
        if (ImGui::Checkbox("Loop", &loop)) {
            player->setLoop(loop);
            changed = true;
        }

        bool autoPlay = player->getAutoPlay();
        if (ImGui::Checkbox("Auto Play", &autoPlay)) {
            player->setAutoPlay(autoPlay);
            changed = true;
        }

        return changed;
    }

private:
    bool renderSpriteControls(Sprite2D* sprite) {
        bool changed = false;

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "Sprite2D");

        ImGui::Text("Texture: %s", sprite->hasValidTexture() ? sprite->getTextureName().c_str() : "(none)");
        if (ImGui::Button("Browse Textures...")) {
            m_browserSprite = sprite;
            m_browserOpen = true;
        }

        ImGui::Text("Flip:");
        ImGui::SameLine();
        bool flipX = sprite->getFlipX();
        if (ImGui::Checkbox("X##flip", &flipX)) {
            sprite->setFlipX(flipX);
            changed = true;
        }
        ImGui::SameLine();
        bool flipY = sprite->getFlipY();
        if (ImGui::Checkbox("Y##flip", &flipY)) {
            sprite->setFlipY(flipY);
            changed = true;
        }

        ImGui::Text("Tint");
        float tint[4] = { sprite->tint.r, sprite->tint.g, sprite->tint.b, sprite->tint.a };
        if (ImGui::ColorEdit4("##tint", tint)) {
            sprite->setTint(tint[0], tint[1], tint[2], tint[3]);
            changed = true;
        }

        return changed;
    }

    bool renderScriptSection(GameObject* obj) {
        bool changed = false;

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "Scripts");
        ImGui::Spacing();

        const auto& scripts = obj->getScripts();
        if (scripts.empty()) {
            ImGui::TextDisabled("  No scripts attached");
        } else {
            std::vector<Script*> toRemove;
            for (Script* s : scripts) {
                std::string name = s->getClassName();

                ImGui::PushID((void*)s);
                bool enabled = s->isEnabled();
                if (ImGui::Checkbox("##enabled", &enabled)) {
                    s->setEnabled(enabled);
                    changed = true;
                }
                ImGui::PopID();
                ImGui::SameLine();

                ImGui::TextColored(enabled ? ImVec4(0.9f, 0.9f, 0.9f, 1.0f)
                                           : ImVec4(0.4f, 0.4f, 0.4f, 1.0f),
                                   "  %s", name.c_str());
                ImGui::SameLine();

                if (ImGui::Button("X##del", ImVec2(20, 0))) {
                    toRemove.push_back(s);
                }

                ImGui::Indent();
                s->renderInspector();
                ImGui::Unindent();
            }

            for (Script* s : toRemove) {
                obj->removeScript(s);
                delete s;
                changed = true;
            }
        }

        ImGui::Spacing();

        if (ImGui::Button("Add Script", ImVec2(-1, 0))) {
            ImGui::OpenPopup("script_add_popup");
        }

    if (ImGui::BeginPopup("script_add_popup")) {
        const auto& all = ScriptRegistry::instance().getAllScripts();
        for (const auto& scriptName : all) {
            if (ImGui::Selectable(scriptName.c_str())) {
                Script* ns = ScriptRegistry::instance().create(scriptName);
                if (ns) {
                    obj->addScript(ns);
                    changed = true;
                }
            }
        }
        if (all.empty()) {
            ImGui::TextDisabled("No scripts registered");
        }
        ImGui::EndPopup();
    }

        return changed;
    }

    TextureCallback m_onTextureChanged;
    AudioCallback m_onAudioChanged;
    const std::vector<SDL_Texture*>* m_thumbnails = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    Sprite2D* m_browserSprite = nullptr;
    MusicPlayer* m_browserPlayer = nullptr;
    bool m_browserOpen = false;
    bool m_audioBrowserOpen = false;
    ImVec2 m_browserSize = ImVec2(600, 400);
};

}
