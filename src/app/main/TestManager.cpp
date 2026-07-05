#include "TestManager.hpp"
#include <imgui.h>
#include <iostream>

namespace ST {
namespace Test {

TestManager& TestManager::get() {
    static TestManager instance;
    return instance;
}

void TestManager::registerTest(const std::string& name, TestFunc func) {
    m_tests.push_back({ name, func });
}

void TestManager::setCurrentTest(int index) {
    if (index >= 0 && index < static_cast<int>(m_tests.size())) {
        m_currentTest = index;
        std::cout << "=== Running: " << m_tests[index].name << " ===" << std::endl;
    }
}

void TestManager::onImGuiRender() {
    // Left panel - test list
    {
        ImGui::SetNextWindowSize(ImVec2(250, 0));
        ImGui::Begin("Tests");

        ImGui::Text("ST Render Test Manager");
        ImGui::Separator();
        ImGui::Spacing();

        for (int i = 0; i < static_cast<int>(m_tests.size()); ++i) {
            bool isSelected = (m_currentTest == i);
            if (ImGui::Selectable(m_tests[i].name.c_str(), isSelected)) {
                setCurrentTest(i);
            }
        }

        ImGui::End();
    }

    // Right panel - test output
    {
        ImGui::SetNextWindowSize(ImVec2(0, 0)); // fill remaining space
        ImGui::Begin("Output");

        if (m_currentTest >= 0 && m_currentTest < static_cast<int>(m_tests.size())) {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f),
                "[%s]", m_tests[m_currentTest].name.c_str());
            ImGui::Spacing();

            // Run the current test
            if (m_tests[m_currentTest].func) {
                m_tests[m_currentTest].func();
            }
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                "Select a test from the left panel.");
        }

        ImGui::End();
    }
}

} // namespace Test
} // namespace ST
