#pragma once

#include "app/module/IModule.hpp"
#include <SDL2/SDL.h>
#include <functional>
#include <string>
#include <vector>

// "Rasterizer" review sub-module.
//
// Mirrors the TestModule_ReviewMath architecture: a flat ordered list
// of tests lives on the module, each with a human-readable label and a
// bool-returning callable.  The canvas fills green/red as the verdict,
// and the ImGui panel shows per-test PASS/FAIL rows.
//
// Sitting under the "Review" category, the intent is: if you break the
// rasterizer or depth buffer later, this module turns red first.
class TestModule_ReviewRasterizer : public IModule {
public:
    struct TestEntry {
        const char* label;
        std::function<bool()> run;
    };

    enum class RunState { NeverRun, AllPassed, AnyFailed };

    TestModule_ReviewRasterizer();

    const char* getName()    const override { return "Rasterizer"; }
    const char* getCategory() const override { return "Review"; }
    bool hasConsoleOutput()   const override { return true; }
    bool hasRenderOutput()   const override { return true; }
    bool needsRealTimeUpdate() const override { return false; }

    void render(void* canvasTexture, int canvasW, int canvasH) override;
    void runConsole(std::string& output) override;
    bool renderControls() override;

private:
    std::vector<TestEntry> tests_;
    struct Result { bool passed = false; bool ran = false; };
    std::vector<Result> lastResults_;

    RunState lastState_    = RunState::NeverRun;
    int      lastPassed_   = 0;
    int      lastFailed_   = 0;
    int      lastTotal_    = 0;
    bool     hasNewResult_ = false;

    void runAll();
};
