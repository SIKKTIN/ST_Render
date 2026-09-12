#pragma once

#include "TransformUI_ImGui.hpp"
#include "engine/editor/Sprite2D.hpp"
#include "engine/editor/TextureManager.hpp"
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

    void setTextureCallback(TextureCallback cb) { m_onTextureChanged = std::move(cb); }
    void setThumbnails(SDL_Renderer* renderer, const std::vector<SDL_Texture*>& thumbs) {
        m_renderer = renderer;
        m_thumbnails = &thumbs;
    }

    bool renderControls(GameObject* obj) override {
        if (!obj) return false;

        bool changed = TransformUI_ImGui::renderControls(obj);

        if (auto* sprite = dynamic_cast<Sprite2D*>(obj)) {
            changed = renderSpriteControls(sprite) || changed;
        }

        changed = renderScriptSection(obj) || changed;

        return changed;
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
    const std::vector<SDL_Texture*>* m_thumbnails = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    Sprite2D* m_browserSprite = nullptr;
    bool m_browserOpen = false;
    ImVec2 m_browserSize = ImVec2(600, 400);
};

}
