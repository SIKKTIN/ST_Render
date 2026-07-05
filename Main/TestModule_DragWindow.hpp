#pragma once

#include "ITestModule.hpp"
#include <imgui.h>

class TestModule_DragWindow : public ITestModule {
public:
    TestModule_DragWindow() : m_showWin1(false), m_showWin2(false) {}

    const char* getName() const override { return "Drag Window"; }
    bool needsRealTimeUpdate() const override { return true; }

    bool renderControls() override {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "Drag Window Test");
        ImGui::Separator();
        ImGui::Text("Every ImGui window is draggable by default.");
        ImGui::Text("Click buttons below to open test windows,");
        ImGui::Text("then drag them anywhere on screen.");
        ImGui::Spacing();
        if (ImGui::Button("Open Window 1")) m_showWin1 = true;
        ImGui::SameLine();
        if (ImGui::Button("Open Window 2")) m_showWin2 = true;
        ImGui::SameLine();
        if (ImGui::Button("Open Both")) { m_showWin1 = true; m_showWin2 = true; }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Note: the Controls panel (this panel) is");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "fixed by design. Only the popup windows above");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "can be dragged freely.");
        return false;
    }

    void renderOverlays() override {
        if (m_showWin1) {
            if (!m_pos1Set) {
                ImGuiIO& io = ImGui::GetIO();
                ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.3f, io.DisplaySize.y * 0.25f), ImGuiCond_Once);
                m_pos1Set = true;
            }
            ImGui::SetNextWindowSize(ImVec2(300, 200));
            ImGui::Begin("Draggable Window 1", &m_showWin1);
            ImGui::Text("Window 1");
            ImGui::Separator();
            ImGui::Text("Drag me by the title bar!");
            ImGui::Spacing();
            static int counter = 0;
            if (ImGui::Button("Click me")) counter++;
            ImGui::Text("Clicks: %d", counter);
            ImGui::End();
        }

        if (m_showWin2) {
            if (!m_pos2Set) {
                ImGuiIO& io = ImGui::GetIO();
                ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.35f), ImGuiCond_Once);
                m_pos2Set = true;
            }
            ImGui::SetNextWindowSize(ImVec2(280, 180));
            ImGui::Begin("Draggable Window 2", &m_showWin2);
            ImGui::Text("Window 2");
            ImGui::Separator();
            ImGui::Text("Drag me too!");
            ImGui::Spacing();
            static float val = 0.5f;
            ImGui::SliderFloat("Value", &val, 0.0f, 1.0f);
            ImGui::End();
        }
    }

    void runConsole(std::string& output) override {
        output = "[Drag Window Test]\n";
        output += "Window 1: " + std::string(m_showWin1 ? "open" : "closed") + "\n";
        output += "Window 2: " + std::string(m_showWin2 ? "open" : "closed") + "\n";
    }

private:
    bool m_showWin1;
    bool m_showWin2;
    ImVec2 m_pos1;
    ImVec2 m_pos2;
    bool m_pos1Set = false;
    bool m_pos2Set = false;
};
