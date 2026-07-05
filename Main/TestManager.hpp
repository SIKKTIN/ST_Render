#pragma once

#include <functional>
#include <string>
#include <vector>

namespace ST {
namespace Test {

using TestFunc = std::function<void()>;

struct TestItem {
    std::string name;
    TestFunc func;
};

class TestManager {
public:
    static TestManager& get();

    void registerTest(const std::string& name, TestFunc func);
    const std::vector<TestItem>& getTests() const { return m_tests; }
    void setCurrentTest(int index);
    int getCurrentTest() const { return m_currentTest; }
    void onImGuiRender();

private:
    TestManager() = default;

    std::vector<TestItem> m_tests;
    int m_currentTest = -1;
};

} // namespace Test
} // namespace ST
