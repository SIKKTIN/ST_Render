#pragma once

#include "renderer/renderer/Renderer.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

class ShaderEditorUI {
public:
    ShaderEditorUI() {
        setPath("Data/Shaders/basic.stshader");
    }

    bool renderControls(ST::Renderer& renderer) {
        bool changed = false;
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Shader Script");
        ImGui::Separator();

        if (ImGui::InputText("Path", m_path.data(), m_path.size())) {
            changed = true;
        }
        if (ImGui::Button("Load")) {
            loadFromDisk(renderer);
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Save & Reload")) {
            saveToDisk(renderer);
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload")) {
            loadFromDisk(renderer);
            changed = true;
        }

        ImGui::SameLine();
        ImGui::Checkbox("Auto reload", &m_autoReload);
        if (m_autoReload) {
            std::string reloadError;
            if (renderer.reloadShaderIfChanged(reloadError)) {
                readSourceBuffer();
                changed = true;
            } else if (!reloadError.empty()) {
                m_error = reloadError;
            }
        }

        if (!m_error.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Shader error");
            ImGui::TextWrapped("%s", m_error.c_str());
        } else if (m_loaded) {
            ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.45f, 1.0f), "Shader loaded");
        } else {
            ImGui::TextDisabled("No script loaded; built-in shader is active.");
        }

        ImGui::Text("Source");
        if (ImGui::InputTextMultiline("##ShaderSource", m_source.data(), m_source.size(),
                                      ImVec2(-1.0f, 260.0f),
                                      ImGuiInputTextFlags_AllowTabInput)) {
            m_sourceDirty = true;
            changed = true;
        }
        if (m_sourceDirty) {
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "Unsaved changes");
        }
        return changed;
    }

private:
    void setPath(const char* path) {
        std::fill(m_path.begin(), m_path.end(), '\0');
        const size_t length = std::min(std::strlen(path), m_path.size() - 1);
        std::memcpy(m_path.data(), path, length);
        m_path[length] = '\0';
    }

    void readSourceBuffer() {
        std::ifstream file(m_path.data());
        if (!file) return;
        std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        std::fill(m_source.begin(), m_source.end(), '\0');
        size_t copySize = std::min(source.size(), m_source.size() - 1);
        std::memcpy(m_source.data(), source.data(), copySize);
        m_source[copySize] = '\0';
        m_sourceDirty = false;
    }

    void loadFromDisk(ST::Renderer& renderer) {
        std::string error;
        if (renderer.loadShaderFile(m_path.data(), error)) {
            m_error.clear();
            m_loaded = true;
            readSourceBuffer();
        } else {
            m_error = error;
            m_loaded = false;
        }
    }

    void saveToDisk(ST::Renderer& renderer) {
        std::ofstream file(m_path.data(), std::ios::binary | std::ios::trunc);
        if (!file) {
            m_error = std::string("cannot write shader file: ") + m_path.data();
            return;
        }
        file.write(m_source.data(), static_cast<std::streamsize>(std::strlen(m_source.data())));
        file.close();

        std::string error;
        if (renderer.loadShaderFile(m_path.data(), error)) {
            m_error.clear();
            m_loaded = true;
            m_sourceDirty = false;
        } else {
            m_error = error;
        }
    }

    std::vector<char> m_path = std::vector<char>(512, '\0');
    std::vector<char> m_source = std::vector<char>(64 * 1024, '\0');
    std::string m_error;
    bool m_autoReload = true;
    bool m_loaded = false;
    bool m_sourceDirty = false;
};
