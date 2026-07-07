#pragma once

#include "app/module/IModule.hpp"
#include <SDL2/SDL.h>
#include <functional>
#include <string>
#include <vector>

// "Math" review sub-module.
//
// Architecture:
//   - A flat, ordered list of tests lives on the module.  Each test owns
//     a human-readable label and a callable that returns bool when run.
//   - "Run all" executes them in declaration order, recording pass/fail.
//     (User-supplied tests are intentionally not exposed yet -- the hook
//     is the vector of tests; adding more later is a matter of pushing
//     entries, not editing the runner.)
//   - The ImGui panel below the canvas shows the result of the most
//     recent run as a vertical list, with one row per test plus a final
//     summary line.
//   - The canvas viewport fills GREEN when every test passed, RED when
//     any test failed.  A dim stripe + title is painted on top so the
//     meaning of the colour is unambiguous.
//
// Sitting under the "Review" category, the user-facing intent is: if you
// break math / core data later, this module turns red.  When something
// downstream misbehaves, this is the first thing you check.
class TestModule_ReviewMath : public IModule {
public:
    // One test entry.  The id is for ordering; labels are what the user
    // sees.  run() returns true on success.  A test may freely call into
    // ST math primitives, file IO, or anything else -- there is no
    // sandboxing; this is a developer tool.
    struct TestEntry {
        const char* label;
        std::function<bool()> run;
    };

    // Run-state snapshot.  Lives as long as needed to drive both the
    // canvas colour and the on-screen results list.
    enum class RunState { NeverRun, AllPassed, AnyFailed };

    TestModule_ReviewMath();

    const char* getName() const override { return "Math"; }
    const char* getCategory() const override { return "Review"; }
    bool hasConsoleOutput() const override { return true; }
    bool hasRenderOutput() const override { return true; }
    bool needsRealTimeUpdate() const override { return false; }

    void render(void* canvasTexture, int canvasW, int canvasH) override;
    void runConsole(std::string& output) override;
    bool renderControls() override;

private:
    // ---- The default test set ----------------------------------------------
    // Declared in the .cpp (sees all the math headers).  Everything lives
    // here so we don't pollute the header with includes beyond IModule.
    std::vector<TestEntry> tests_;

    // Result of a test, parallel-indexed with tests_.  -1 = not run.
    struct Result { bool passed = false; bool ran = false; };
    std::vector<Result> lastResults_;

    // Roll-up snapshot of the last "Run all".  Mutated only when the user
    // (or auto-run) presses the button.
    RunState  lastState_    = RunState::NeverRun;
    int       lastPassed_   = 0;
    int       lastFailed_   = 0;
    int       lastTotal_    = 0;

    // Re-render the canvas & panels after a state change.
    bool      hasNewResult_ = false;

    // Owns "Run all": actually drives the tests.
    void runAll();
};
